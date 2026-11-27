#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Replace ehide.device in the supported TF4060r5 release ZIP.

Uses only Python's standard library. See tools/README.md for input requirements
and the firmware layout. This program creates an archive; it does not flash it.
"""

import argparse
import copy
from dataclasses import dataclass
import hashlib
import io
from pathlib import Path
import struct
import sys
import zipfile
import zlib


RELEASE = "v0.8.6_2026-11-26_19799ac"
ZIP_SHA256 = "d6e5cbe4e1d45e8c9c3217ebb3d2b58b2732823fb13f017c0a883061ab08b7d6"
FLASH_SHA256 = "248a120aca753da9c3a9f77abcf5aaf10ac6234e3a1aaada016c22e97640cfad"
FLASH_MEMBER = RELEASE + "/tf4060r5_ram_flash.bin"
README_MEMBER = RELEASE + "/README.md"
SPI_OFFSET = 0x21000
FLASH_CAPACITY = 2 * 1024 * 1024
SECTOR = 512
TOC_BEGIN = 0x416C
RELOC_POINTER = 0x56
IDE_INDEX = 2
MARKER = struct.pack(">III", 0xFFFFFFFF, 0, 0)


class PatchError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise PatchError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def u32(data, offset):
    require(0 <= offset <= len(data) - 4, "truncated 32-bit field")
    return struct.unpack_from(">I", data, offset)[0]


def put32(data, offset, value):
    struct.pack_into(">I", data, offset, value)


def with_crc(data):
    return bytes(data) + struct.pack("<I", ~zlib.crc32(data) & 0xFFFFFFFF)


def check_driver(data):
    """Accept the single CODE hunk with RELOC32 emitted by the LIDE build.

    Reject other hunk layouts rather than assuming the SSBL can load them.
    The Resident name is shared by LIDE variants; the caller must supply the
    TF build, not the AIDE, Zorro or PCI build.
    """
    require(32 <= len(data) <= FLASH_CAPACITY and len(data) % 4 == 0,
            "driver has an invalid size")
    require(struct.unpack_from(">5I", data) == (0x3F3, 0, 1, 0, 0),
            "driver must be a single-hunk Amiga executable")
    size = u32(data, 20) * 4
    require(u32(data, 24) == 0x3E9 and u32(data, 28) * 4 == size,
            "driver must contain one CODE hunk with matching allocation size")
    require(32 + size + 4 <= len(data), "truncated CODE hunk")
    body = data[32:32 + size]
    pos = 32 + size
    relocations = set()
    while u32(data, pos) == 0x3EC:
        pos += 4
        while True:
            count = u32(data, pos)
            pos += 4
            if count == 0:
                break
            require(u32(data, pos) == 0, "driver relocation targets another hunk")
            pos += 4
            require(count <= (len(data) - pos) // 4, "truncated driver relocations")
            for _ in range(count):
                site = u32(data, pos)
                pos += 4
                require(site % 2 == 0 and site <= size - 4,
                        "driver relocation lies outside CODE or is unaligned")
                require(site not in relocations, "duplicate driver relocation")
                require(u32(body, site) <= size, "driver relocation target outside CODE")
                relocations.add(site)
    require(u32(data, pos) == 0x3F2 and pos + 4 == len(data),
            "unsupported driver hunk records or trailing bytes")
    ordered = sorted(relocations)
    require(all(b - a >= 4 for a, b in zip(ordered, ordered[1:])),
            "overlapping driver relocations")

    def string_at(offset):
        end = body.find(b"\0", offset)
        require(0 <= offset < size and end >= offset, "invalid Resident string")
        return body[offset:end]

    residents = []
    for at in range(0, size - 25, 2):
        if body[at:at + 2] != b"\x4a\xfc" or u32(body, at + 2) != at:
            continue
        require({at + n for n in (2, 6, 14, 18, 22)} <= relocations,
                "Resident pointers are missing relocations")
        require(at + 26 <= u32(body, at + 6) <= size, "invalid Resident end pointer")
        require(body[at + 10] & 1 and not body[at + 10] & 0x80,
                "driver must use a COLDSTART Resident without AUTOINIT")
        require(body[at + 12] == 3, "Resident is not an Exec device")
        require(u32(body, at + 22) % 2 == 0 and u32(body, at + 22) < size,
                "invalid Resident init pointer")
        require(string_at(u32(body, at + 14)) == b"lide.device",
                "driver Resident must be named lide.device")
        string_at(u32(body, at + 18))
        residents.append(at + 32)
    require(len(residents) == 1, "driver must contain exactly one lide.device Resident")
    return residents[0]


def prepare_driver(data):
    resident = check_driver(data)
    result = bytearray(data)
    # The bundled ODFileSystem has priority 10. Preserve ehide's priority 5
    # so the filesystem is registered before LIDE looks for CD boot support.
    result[resident + 13] = 5
    return bytes(result)


def encode_relocations(sites):
    result = bytearray()
    previous = 0
    for site in sites:
        delta = site - previous
        require(delta > 0, "SSBL relocation sites must increase")
        result.extend(bytes([delta]) if delta < 256 else b"\0" + struct.pack(">I", delta))
        previous = site
    return struct.pack(">I", len(result)) + result


@dataclass(frozen=True)
class Module:
    offset: int
    data: bytes


@dataclass(frozen=True)
class Ssbl:
    data: bytes
    modules: list
    table: int
    relocations: list


def parse_ssbl(part):
    """Parse this release's TOC and vlink -mrel relocation table."""
    require(part[TOC_BEGIN:TOC_BEGIN + 12] == MARKER, "SSBL TOC marker does not match")
    table = u32(part, RELOC_POINTER)
    require(TOC_BEGIN < table <= len(part) - 8, "invalid SSBL relocation table pointer")
    end = table + 4 + u32(part, table)
    require(end + 4 <= len(part), "truncated SSBL relocation table")
    data = part[:end + 4]
    require(zlib.crc32(data) == 0xFFFFFFFF, "SSBL checksum does not match")
    require(not any(part[end + 4:]), "nonzero SSBL partition padding")
    pos = TOC_BEGIN + 12
    modules = []
    while u32(data, pos):
        require(len(modules) < 8, "too many SSBL modules")
        require(pos + 16 <= table, "truncated SSBL module header")
        nxt, size, zero1, zero2 = struct.unpack_from(">4I", data, pos)
        require(size > 0 and size % 4 == 0 and nxt == pos + size + 28 and nxt < table,
                "invalid SSBL module extent")
        require(zero1 == zero2 == 0 and data[nxt - 12:nxt] == MARKER,
                "unexpected SSBL module metadata")
        modules.append(Module(pos, data[pos + 16:nxt - 12]))
        pos = nxt
    require(len(modules) == 8 and data[pos:pos + 12] == struct.pack(">III", 0, 8, 0xDEADBEEF),
            "SSBL TOC terminator does not match")
    require(pos + 24 == table and data[pos + 12:table] == b"\0" * 12,
            "unexpected data after SSBL TOC")
    sites = []
    site = 0
    pos = table + 4
    while pos < end:
        delta = data[pos]
        pos += 1
        if delta == 0:
            require(pos + 4 <= end, "truncated long SSBL relocation delta")
            delta = u32(data, pos)
            pos += 4
        require(delta > 0, "duplicate SSBL relocation")
        site += delta
        require(site % 2 == 0 and site + 4 <= table, "invalid SSBL relocation site")
        require(site < TOC_BEGIN or site in {m.offset for m in modules},
                "SSBL relocation overlaps an embedded module")
        require(u32(data, site) <= table, "SSBL relocation target outside image")
        sites.append(site)
    require(len(sites) == 65 and RELOC_POINTER in sites and
            all(m.offset in sites for m in modules), "unexpected SSBL relocation coverage")
    require(encode_relocations(sites) == data[table:end],
            "noncanonical SSBL relocation encoding")
    return Ssbl(data, modules, table, sites)


def replace_module(ssbl, replacement):
    """Rebase addresses and relocation *sites* after replacing TOC entry 2.

    The firmware hash is checked by patch_flash before this is used. In this
    particular SSBL all PC-relative references stay before TOC_BEGIN; moved
    globals and TOC pointers use the recorded absolute relocations.
    """
    require(len(replacement) > 0 and len(replacement) % 4 == 0,
            "replacement must be longword aligned")
    module = ssbl.modules[IDE_INDEX]
    start = module.offset + 16
    end = start + len(module.data)
    delta = len(replacement) - len(module.data)

    def move(offset):
        require(not start <= offset < end, "SSBL pointer refers into the replaced driver")
        return offset + delta if offset >= end else offset

    body = bytearray(ssbl.data[:start] + replacement + ssbl.data[end:ssbl.table])
    sites = []
    for site in ssbl.relocations:
        new_site = move(site)
        put32(body, new_site, move(u32(ssbl.data, site)))
        sites.append(new_site)
    put32(body, module.offset + 4, len(replacement))
    result = with_crc(body + encode_relocations(sites))
    parsed = parse_ssbl(result)
    require(parsed.modules[IDE_INDEX].data == replacement, "driver replacement mismatch")
    require(all(a.data == b.data for i, (a, b) in
                enumerate(zip(ssbl.modules, parsed.modules)) if i != IDE_INDEX),
            "another SSBL module changed")
    return result


def chs(lba):
    cylinder, remainder = divmod(lba, 255 * 63)
    head, sector = divmod(remainder, 63)
    require(cylinder <= 1023, "CHS address exceeds supported flash size")
    return bytes((head, sector + 1 | (cylinder >> 2 & 0xC0), cylinder & 0xFF))


def make_spirom(ssbl, romfs):
    n1 = (len(ssbl) + SECTOR - 1) // SECTOR
    n2 = (len(romfs) + SECTOR - 1) // SECTOR
    mbr = bytearray(SECTOR)
    struct.pack_into("<I", mbr, 440, zlib.crc32(ssbl + romfs))
    for index, (kind, lba, count) in enumerate(((0x68, 1, n1), (0x69, 1 + n1, n2))):
        offset = 446 + 16 * index
        mbr[offset:offset + 16] = (b"\0" + chs(lba) + bytes([kind]) +
                                   chs(lba + count - 1) + struct.pack("<II", lba, count))
    mbr[510:512] = b"\x55\xaa"
    return bytes(mbr) + ssbl.ljust(n1 * SECTOR, b"\0") + romfs.ljust(n2 * SECTOR, b"\0")


def split_flash(flash):
    spirom = flash[SPI_OFFSET:]
    require(len(spirom) >= SECTOR and spirom[510:512] == b"\x55\xaa", "invalid SPI ROM MBR")
    parts = []
    next_lba = 1
    for index, kind in enumerate((0x68, 0x69)):
        entry = spirom[446 + 16 * index:462 + 16 * index]
        lba, count = struct.unpack_from("<II", entry, 8)
        require(entry[4] == kind and lba == next_lba and count > 0 and
                (lba + count) * SECTOR <= len(spirom), "invalid SPI ROM partition")
        parts.append(spirom[lba * SECTOR:(lba + count) * SECTOR])
        next_lba = lba + count
    require(next_lba * SECTOR == len(spirom), "unexpected bytes after SPI ROM partitions")
    ssbl = parse_ssbl(parts[0])
    romfs = parts[1]
    require(romfs[:8] == b"-rom1fs-", "ROMFS magic does not match")
    size = u32(romfs, 8)
    require(16 <= size <= len(romfs) and size % 4 == 0, "invalid ROMFS size")
    header = romfs[:min(size, SECTOR)]
    require(sum(struct.unpack(">" + "I" * (len(header) // 4), header)) & 0xFFFFFFFF == 0,
            "ROMFS header checksum does not match")
    require(make_spirom(ssbl.data, romfs) == spirom, "SPI ROM metadata does not match its contents")
    return ssbl, romfs


def patch_flash(flash, driver):
    require(sha256(flash) == FLASH_SHA256, "unsupported firmware image SHA-256")
    driver = prepare_driver(driver)
    ssbl, romfs = split_flash(flash)
    require(replace_module(ssbl, ssbl.modules[IDE_INDEX].data) == ssbl.data,
            "SSBL does not reproduce byte-for-byte")
    updated = replace_module(ssbl, driver)
    result = flash[:SPI_OFFSET] + make_spirom(updated, romfs)
    require(len(result) <= FLASH_CAPACITY, "replacement exceeds the 2 MiB flash capacity")
    new_ssbl, new_romfs = split_flash(result)
    require(new_romfs == romfs and new_ssbl.data == updated, "rebuilt SPI ROM mismatch")
    require(replace_module(new_ssbl, ssbl.modules[IDE_INDEX].data) == ssbl.data,
            "SSBL replacement does not reverse to the original")
    return result


def patch_archive(archive_data, driver):
    require(sha256(archive_data) == ZIP_SHA256,
            "unsupported ZIP: use the unmodified tf4060r5_" + RELEASE + ".zip")
    with zipfile.ZipFile(io.BytesIO(archive_data)) as original:
        flash = patch_flash(original.read(FLASH_MEMBER), driver)
        readme = original.read(README_MEMBER).decode("utf-8")
        old_hash = hashlib.sha1(original.read(FLASH_MEMBER)).hexdigest()[:8]
        new_hash = hashlib.sha1(flash).hexdigest()[:8]
        require(readme.count(old_hash) == 1, "original README flash checksum not found")
        readme = readme.replace(old_hash, new_hash)
        note = ("# TF4060r5 with TF-lide.device\n\n"
                "Custom image: TF-lide.device replaces ehide.device in the SSBL.\n"
                "The installed Exec device name is `lide.device`.\n"
                "Its Resident priority is set to 5 so the bundled CD filesystem starts first.\n"
                "The FPGA bitstream, other resident modules, ROMFS and CPLD files are unchanged.\n\n"
                f"- Original ZIP SHA-256: `{ZIP_SHA256}`\n"
                f"- Supplied driver SHA-256: `{sha256(driver)}`\n"
                f"- Embedded driver SHA-256: `{sha256(prepare_driver(driver))}`\n"
                f"- Patched flash SHA-256: `{sha256(flash)}`\n\n"
                "This custom image requires TF4060 hardware boot testing.\n"
                "The original release notes below describe the base firmware.\n\n---\n\n")
        changes = {FLASH_MEMBER: flash, README_MEMBER: (note + readme).encode("utf-8")}
        output = io.BytesIO()
        with zipfile.ZipFile(output, "w") as patched:
            patched.comment = original.comment
            for entry in original.infolist():
                patched.writestr(copy.copy(entry), changes.get(entry.filename, original.read(entry)))
        result = output.getvalue()
        with zipfile.ZipFile(io.BytesIO(result)) as patched:
            require(patched.testzip() is None, "output ZIP checksum failure")
            require(patched.namelist() == original.namelist(), "output ZIP member list changed")
            for name in original.namelist():
                expected = changes.get(name, original.read(name))
                require(patched.read(name) == expected, "output ZIP member mismatch: " + name)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path, help="unmodified TF4060r5 release ZIP")
    parser.add_argument("driver", type=Path, help="TF-lide.device Amiga HUNK executable")
    parser.add_argument("-o", "--output", required=True, type=Path, help="new output ZIP (must not exist)")
    args = parser.parse_args(argv)
    try:
        require(not args.output.exists(), "output already exists; choose a new filename")
        require(args.firmware.stat().st_size <= 16 * 1024 * 1024, "input ZIP is too large")
        require(args.driver.stat().st_size <= FLASH_CAPACITY, "driver is too large")
        driver = args.driver.read_bytes()
        result = patch_archive(args.firmware.read_bytes(), driver)
        # Build and check everything in memory before creating the output.
        # Exclusive creation also protects inputs and existing files from races.
        with args.output.open("xb") as output:
            output.write(result)
        print(f"Created {args.output}")
        print(f"ZIP SHA-256: {sha256(result)}")
        print(f"Driver SHA-256: {sha256(driver)}")
        return 0
    except (OSError, PatchError, zipfile.BadZipFile) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
