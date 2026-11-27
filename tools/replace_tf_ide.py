#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Replace the embedded IDE driver in a TF4060 firmware ZIP.

Uses only Python's standard library. See tools/README.md for input requirements
and the firmware layout. This program creates an archive; it does not flash it.
"""

import argparse
import copy
from dataclasses import dataclass
import hashlib
import io
from pathlib import Path, PurePosixPath
import struct
import sys
import zipfile
import zlib


FLASH_CAPACITY = 2 * 1024 * 1024
SECTOR = 512
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
    # The bundled ODFileSystem has priority 10. Use device priority 5
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


def find_toc(data):
    """Find a complete module chain with a matching terminator count."""
    candidates = {}
    begin = data.find(MARKER)
    while begin >= 0:
        if begin % 2 == 0:
            pos = begin + len(MARKER)
            modules = []
            while pos + 16 <= len(data):
                nxt, size, zero1, zero2 = struct.unpack_from(">4I", data, pos)
                if nxt == 0:
                    if modules and zero1 == 0xDEADBEEF:
                        # Every module trailer also looks like a TOC start.
                        # Keep the longest chain to each terminator so a bad
                        # count cannot turn a suffix into a valid table.
                        candidates.setdefault(pos, (begin, pos + 12, modules, size))
                    break
                if (not size or size % 4 or nxt != pos + size + 28 or
                        nxt + 12 > len(data) or zero1 or zero2 or
                        data[nxt - 12:nxt] != MARKER):
                    break
                modules.append(Module(pos, data[pos + 16:nxt - 12]))
                pos = nxt
        begin = data.find(MARKER, begin + 2)
    require(len(candidates) == 1, "SSBL needs one complete module table")
    begin, end, modules, count = next(iter(candidates.values()))
    require(count == len(modules), "SSBL module count does not match its chain")
    return begin, end, modules


def parse_relocations(data, table, toc_begin, modules):
    end = table + 4 + u32(data, table)
    require(end == len(data) - 4, "SSBL relocation table length does not match")
    sites = []
    site = 0
    pos = table + 4
    headers = {m.offset for m in modules}
    toc_end = modules[-1].offset + len(modules[-1].data) + 28
    while pos < end:
        delta = data[pos]
        pos += 1
        if delta == 0:
            require(pos + 4 <= end, "truncated long SSBL relocation delta")
            delta = u32(data, pos)
            pos += 4
        require(delta > 0, "duplicate SSBL relocation")
        site += delta
        require(site % 2 == 0 and site + 4 <= table and
                (not sites or site - sites[-1] >= 4), "invalid SSBL relocation site")
        require(site + 4 <= toc_begin or site in headers or site >= toc_end + 12,
                "SSBL relocation overlaps an embedded module")
        require(u32(data, site) <= table, "SSBL relocation target outside image")
        sites.append(site)
    require(headers <= set(sites) and
            any(site < toc_begin and u32(data, site) == table for site in sites),
            "incomplete SSBL relocation coverage")
    require(encode_relocations(sites) == data[table:end],
            "noncanonical SSBL relocation encoding")
    return sites


def parse_ssbl(part):
    """Discover the TOC, exact CRC extent and vlink -mrel table."""
    require(len(part) <= FLASH_CAPACITY, "SSBL exceeds flash capacity")
    toc_begin, toc_end, modules = find_toc(part)
    # Zero partition padding and trailing zero CRC bytes are indistinguishable.
    # Use each candidate's recorded length to locate the complete CRC word.
    nonzero_end = len(part.rstrip(b"\0"))
    candidates = []
    error = "SSBL needs one vlink relocation table"
    for table in range(toc_end, len(part) - 7, 2):
        end = table + 8 + u32(part, table)
        if not nonzero_end <= end <= len(part):
            continue
        data = part[:end]
        try:
            sites = parse_relocations(data, table, toc_begin, modules)
            require(zlib.crc32(data) == 0xFFFFFFFF, "SSBL checksum does not match")
        except PatchError as exc:
            error = str(exc)
            continue
        candidates.append(Ssbl(data, modules, table, sites))
    require(len(candidates) == 1, error if not candidates else
            "SSBL has ambiguous vlink relocation tables")
    return candidates[0]


def ide_index(ssbl):
    """Select the device by its Resident, independent of module order."""
    matches = []
    for index, module in enumerate(ssbl.modules):
        data = module.data
        # The IDE modules use a single CODE hunk. Confirm Resident identity
        # through its self pointer and name pointer, not a string search.
        if len(data) < 36 or struct.unpack_from(">5I", data) != (0x3F3, 0, 1, 0, 0):
            continue
        if u32(data, 24) != 0x3E9:
            continue
        size = u32(data, 28) * 4
        if 32 + size > len(data):
            continue
        body = data[32:32 + size]
        for at in range(0, size - 25, 2):
            if (body[at:at + 2] != b"\x4a\xfc" or
                    u32(body, at + 2) != at or body[at + 12] != 3):
                continue
            name = u32(body, at + 14)
            if any(body[name:name + len(n)] == n for n in
                   (b"ehide.device\0", b"lide.device\0")):
                matches.append(index)
    require(len(matches) == 1, "SSBL needs exactly one ehide.device or lide.device module")
    return matches[0]


def replace_module(ssbl, replacement, index=None):
    """Rebase absolute addresses and relocation sites around a TOC payload.

    The SSBL format places position-independent code before the TOC and
    records absolute references to the TOC and following data in -mrel.
    """
    if index is None:
        index = ide_index(ssbl)
    require(len(replacement) > 0 and len(replacement) % 4 == 0,
            "replacement must be longword aligned")
    module = ssbl.modules[index]
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
    require(parsed.modules[index].data == replacement, "driver replacement mismatch")
    require(all(a.data == b.data for i, (a, b) in
                enumerate(zip(ssbl.modules, parsed.modules)) if i != index),
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


def split_spirom(spirom):
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


def split_flash(flash):
    """Locate a sector-aligned SPI ROM behind the FPGA bitstream."""
    require(len(flash) <= FLASH_CAPACITY, "input exceeds the 2 MiB flash capacity")
    offsets = [at for at in range(0, len(flash) - SECTOR + 1, SECTOR)
               if flash[at + 510:at + 512] == b"\x55\xaa" and
               flash[at + 450] == 0x68 and flash[at + 466] == 0x69]
    require(len(offsets) == 1, "flash image needs one SPI ROM partition table")
    offset = offsets[0]
    ssbl, romfs = split_spirom(flash[offset:])
    return offset, ssbl, romfs


def patch_flash(flash, driver):
    driver = prepare_driver(driver)
    offset, ssbl, romfs = split_flash(flash)
    index = ide_index(ssbl)
    require(replace_module(ssbl, ssbl.modules[index].data) == ssbl.data,
            "SSBL does not reproduce byte-for-byte")
    updated = replace_module(ssbl, driver)
    result = flash[:offset] + make_spirom(updated, romfs)
    require(len(result) <= FLASH_CAPACITY, "replacement exceeds the 2 MiB flash capacity")
    new_offset, new_ssbl, new_romfs = split_flash(result)
    require(new_offset == offset and new_romfs == romfs and new_ssbl.data == updated,
            "rebuilt SPI ROM mismatch")
    require(replace_module(new_ssbl, ssbl.modules[index].data) == ssbl.data,
            "SSBL replacement does not reverse to the original")
    return result


def flash_member(archive):
    candidates = [n for n in archive.namelist()
                  if PurePosixPath(n).name.startswith("tf4060") and n.endswith("_ram_flash.bin")]
    require(len(candidates) == 1, "ZIP needs exactly one TF4060 *_ram_flash.bin image")
    return candidates[0]


def patch_archive(archive_data, driver):
    with zipfile.ZipFile(io.BytesIO(archive_data)) as original:
        names = original.namelist()
        require(len(names) == len(set(names)), "ZIP contains duplicate member names")
        require(sum(e.file_size for e in original.infolist()) <= 64 * 1024 * 1024,
                "uncompressed ZIP is too large")
        member = flash_member(original)
        source = original.read(member)
        flash = patch_flash(source, driver)
        readme_member = str(PurePosixPath(member).with_name("README.md"))
        readme = original.read(readme_member) if readme_member in names else b""
        for heading in (b"# TF4060 firmware with TF-lide.device\n",
                        b"# TF4060r5 with TF-lide.device\n"):
            if readme.startswith(heading) and b"\n---\n\n" in readme:
                readme = readme.split(b"\n---\n\n", 1)[1]
                break
        # Release notes are optional. Update checksums where provided without
        # depending on a particular release's text or encoding.
        for digest in (hashlib.sha1, hashlib.sha256):
            old, new = digest(source).hexdigest(), digest(flash).hexdigest()
            readme = readme.replace(old.encode(), new.encode())
            readme = readme.replace(old[:8].encode(), new[:8].encode())
        note = ("# TF4060 firmware with TF-lide.device\n\n"
                "Custom image: the SSBL IDE module is TF-lide.device.\n"
                "The installed Exec device name is `lide.device`.\n"
                "Its Resident priority is set to 5 so the bundled CD filesystem starts first.\n"
                "The FPGA bitstream, other resident modules, ROMFS and CPLD files are unchanged.\n\n"
                f"- Input ZIP SHA-256: `{sha256(archive_data)}`\n"
                f"- Supplied driver SHA-256: `{sha256(driver)}`\n"
                f"- Embedded driver SHA-256: `{sha256(prepare_driver(driver))}`\n"
                f"- Patched flash SHA-256: `{sha256(flash)}`\n\n"
                "The original release notes below describe the base firmware.\n\n---\n\n")
        changes = {member: flash, readme_member: note.encode("utf-8") + readme}
        output = io.BytesIO()
        with zipfile.ZipFile(output, "w") as patched:
            patched.comment = original.comment
            for entry in original.infolist():
                patched.writestr(copy.copy(entry), changes.get(entry.filename, original.read(entry)))
            if readme_member not in names:
                patched.writestr(readme_member, changes[readme_member])
        result = output.getvalue()
        with zipfile.ZipFile(io.BytesIO(result)) as patched:
            require(patched.testzip() is None, "output ZIP checksum failure")
            expected_names = names + ([] if readme_member in names else [readme_member])
            require(patched.namelist() == expected_names, "output ZIP member list changed")
            for name in names:
                expected = changes.get(name, original.read(name))
                require(patched.read(name) == expected, "output ZIP member mismatch: " + name)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path, help="TF4060 firmware ZIP containing a RAM flash image")
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
