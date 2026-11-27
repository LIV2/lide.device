#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Generated format fixtures, plus optional firmware archive integration checks."""

import argparse
import contextlib
import hashlib
import io
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import warnings
from unittest import mock
import zipfile
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import replace_tf_ide as patch

FIRMWARE = None
DRIVER = None


def words(*values):
    return struct.pack(">" + "I" * len(values), *values)


def driver_fixture(size=128, name=b"lide.device"):
    name += b"\0"
    body = bytearray(size)
    body[0:2] = b"\x4e\x75"
    body[4:30] = struct.pack(">HII4BIII", 0x4AFC, 4, size, 1, 40, 3, 10, 64, 80, 32)
    body[32:34] = b"\x4e\x75"
    body[64:64 + len(name)] = name
    body[80:90] = b"lide test\0"
    return words(0x3F3, 0, 1, 0, 0, size // 4, 0x3E9, size // 4) + body + words(
        0x3EC, 5, 0, 6, 10, 18, 22, 26, 0, 0x3F2)


def ssbl_fixture(toc_begin=0x400, count=8, ide=2, pointer=0x56,
                 globals_size=12, extras=20):
    result = bytearray(toc_begin) + patch.MARKER
    sites = [pointer] + list(range(0x100, 0x100 + extras * 4, 4))
    for index in range(count):
        at = len(result)
        payload = (driver_fixture(440, b"ehide.device") if index == ide
                   else bytes([index + 1]) * 64)
        result += words(at + len(payload) + 28, len(payload), 0, 0) + payload + patch.MARKER
        sites.append(at)
    result += words(0, count, 0xDEADBEEF) + bytes(globals_size)
    table = len(result)
    patch.put32(result, pointer, table)
    for i, site in enumerate(sites[1:1 + extras]):
        patch.put32(result, site, (table - globals_size, table, 0x40)[i % 3])
    return patch.with_crc(result + patch.encode_relocations(sites))


def romfs_fixture():
    data = bytearray(1024)
    data[:8] = b"-rom1fs-"
    patch.put32(data, 8, len(data))
    checksum = sum(struct.unpack(">128I", data[:512]))
    patch.put32(data, 12, -checksum & 0xFFFFFFFF)
    return bytes(data)


class DriverTests(unittest.TestCase):
    def test_resident_priority(self):
        original = driver_fixture()
        self.assertEqual(patch.check_driver(original), 36)
        installed = patch.prepare_driver(original)
        self.assertEqual([i for i, (a, b) in enumerate(zip(original, installed)) if a != b], [49])
        self.assertEqual(installed[49], 5)
        self.assertEqual(original[49], 10)
        self.assertEqual(patch.prepare_driver(installed), installed)
        self.assertEqual(patch.check_driver(installed), 36)

    def test_truncated_hunk_at_every_boundary(self):
        data = driver_fixture()
        for size in range(0, len(data), 4):
            with self.subTest(size=size), self.assertRaises(patch.PatchError):
                patch.check_driver(data[:size])

    def test_invalid_hunks(self):
        base = driver_fixture()
        edits = {
            "multiple hunks": (8, words(2)),
            "BSS": (24, words(0x3EB)),
            "allocation mismatch": (20, words(33)),
            "missing self relocation": (172, words(0)),
            "duplicate relocation": (176, words(6)),
            "relocation outside code": (172, words(128)),
            "unaligned relocation": (172, words(7)),
            "overlapping relocation": (176, words(8)),
            "other target hunk": (168, words(1)),
            "not coldstart": (46, b"\0"),
            "autoinit": (46, b"\x81"),
            "not device": (48, b"\x09"),
            "invalid name": (96, b"X"),
            "invalid init": (58, words(127)),
        }
        for name, (at, value) in edits.items():
            data = bytearray(base)
            data[at:at + len(value)] = value
            with self.subTest(name=name), self.assertRaises(patch.PatchError):
                patch.check_driver(data)
        with self.assertRaises(patch.PatchError):
            patch.check_driver(base + words(0))


class SsblTests(unittest.TestCase):
    def setUp(self):
        self.data = ssbl_fixture()
        self.ssbl = patch.parse_ssbl(self.data)

    def test_noop_is_exact(self):
        self.assertEqual(patch.replace_module(self.ssbl, self.ssbl.modules[2].data), self.data)

    def test_growth_shrink_and_runtime_addresses(self):
        for size in (4, 224, 228, 512, 4096, 65536):
            with self.subTest(size=size):
                replacement = bytes([0xAB]) * size
                result = patch.replace_module(self.ssbl, replacement)
                after = patch.parse_ssbl(result)
                self.assertEqual(after.modules[2].data, replacement)
                self.assertEqual(patch.replace_module(after, self.ssbl.modules[2].data, 2), self.data)
                self.assertEqual(zlib.crc32(result), 0xFFFFFFFF)
                # Independently check relocation-site and target movement as
                # seen by the SSBL after it adds its arbitrary load address.
                boundary = self.ssbl.modules[2].offset + 16 + 512
                delta = size - 512
                for base in (0, 0x100000, 0x8000000):
                    for old_site, new_site in zip(self.ssbl.relocations, after.relocations):
                        self.assertEqual(new_site, old_site + (delta if old_site >= boundary else 0))
                        target = patch.u32(self.data, old_site)
                        expected = base + target + (delta if target >= boundary else 0)
                        self.assertEqual(base + patch.u32(result, new_site), expected)
                for i in (0, 1, 3, 4, 5, 6, 7):
                    self.assertEqual(after.modules[i].data, self.ssbl.modules[i].data)

    def test_bad_crc_and_padding(self):
        damaged = bytearray(self.data)
        damaged[2] ^= 1
        for data in (damaged, self.data[:-1], self.data + b"\x01"):
            with self.assertRaises(patch.PatchError):
                patch.parse_ssbl(data)

    def test_crc_ending_in_zero(self):
        body = bytearray(self.data[:-4])
        for value in range(65536):
            struct.pack_into(">H", body, 0, value)
            data = patch.with_crc(body)
            if data[-1] == 0:
                break
        else:
            self.fail("could not generate a zero-ended CRC fixture")
        for part in (data, data.ljust((len(data) + 511) // 512 * 512, b"\0")):
            self.assertEqual(patch.parse_ssbl(part).data, data)

    def test_relocation_into_replacement_refused(self):
        damaged = bytearray(self.data[:-4])
        patch.put32(damaged, 0x100, self.ssbl.modules[2].offset + 16)
        parsed = patch.parse_ssbl(patch.with_crc(damaged))
        with self.assertRaisesRegex(patch.PatchError, "into the replaced driver"):
            patch.replace_module(parsed, b"\0" * 4)

    def test_truncated_long_delta_with_valid_crc(self):
        data = self.data[:self.ssbl.table] + words(1) + b"\0"
        with self.assertRaisesRegex(patch.PatchError, "truncated long"):
            patch.parse_ssbl(patch.with_crc(data))

    def test_spirom_rebuild_and_capacity(self):
        romfs = romfs_fixture()
        for offset in (0, 0x200, 0x21000, 0x30000):
            with self.subTest(offset=offset):
                prefix = b"\xA5" * offset
                original = prefix + patch.make_spirom(self.data, romfs)
                result = patch.patch_flash(original, driver_fixture(4096))
                new_offset, after, new_romfs = patch.split_flash(result)
                self.assertEqual(new_offset, offset)
                self.assertEqual(result[:offset], prefix)
                self.assertEqual(new_romfs, romfs)
                self.assertEqual(after.modules[2].data, patch.prepare_driver(driver_fixture(4096)))
        with mock.patch.object(patch, "FLASH_CAPACITY", len(original)):
            with self.assertRaisesRegex(patch.PatchError, "flash capacity"):
                patch.patch_flash(original, driver_fixture(65536))

    def test_variable_layouts(self):
        for count, ide, toc, pointer, globals_size, extras in (
                (1, 0, 0x200, 0x20, 0, 0),
                (3, 2, 0x500, 0x60, 32, 7),
                (6, 3, 0x1000, 0x70, 20, 10),
                (9, 8, 0x600, 0x90, 12, 25)):
            with self.subTest(count=count, ide=ide, toc=toc, pointer=pointer):
                data = ssbl_fixture(toc, count, ide, pointer, globals_size, extras)
                before = patch.parse_ssbl(data)
                self.assertEqual(patch.ide_index(before), ide)
                installed = patch.prepare_driver(driver_fixture(4096))
                after = patch.parse_ssbl(patch.replace_module(before, installed))
                self.assertEqual(patch.ide_index(after), ide)
                self.assertEqual(after.modules[ide].data, installed)
                self.assertEqual(patch.replace_module(after, before.modules[ide].data), data)
                for i in range(count):
                    if i != ide:
                        self.assertEqual(before.modules[i].data, after.modules[i].data)

    def test_relocations_and_nonzero_data_after_toc(self):
        before = patch.parse_ssbl(ssbl_fixture(globals_size=32))
        body = bytearray(before.data[:before.table])
        site = before.table - 12
        patch.put32(body, site, before.table - 28)
        body[before.table - 4:before.table] = b"tail"
        data = patch.with_crc(body + patch.encode_relocations(before.relocations + [site]))
        parsed = patch.parse_ssbl(data)
        replacement = driver_fixture(4096)
        after = patch.parse_ssbl(patch.replace_module(parsed, replacement))
        delta = len(replacement) - len(parsed.modules[2].data)
        self.assertEqual(patch.u32(after.data, site + delta), before.table - 28 + delta)
        self.assertEqual(after.data[after.table - 4:after.table], b"tail")
        self.assertEqual(patch.replace_module(after, parsed.modules[2].data), data)

    def test_corrupted_spirom(self):
        source = patch.make_spirom(self.data, romfs_fixture())
        for offset in (440, 450, 454, 466, 478, 510, len(source) - 1):
            damaged = bytearray(source)
            damaged[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(patch.PatchError):
                patch.patch_flash(damaged, driver_fixture())

    def test_missing_ambiguous_and_false_residents(self):
        name = self.ssbl.modules[2].offset + 16 + 96
        data = bytearray(self.data[:-4])
        data[name:name + 5] = b"other"
        with self.assertRaisesRegex(patch.PatchError, "exactly one"):
            patch.ide_index(patch.parse_ssbl(patch.with_crc(data)))
        # A plain name string in another module must not select that module.
        at = self.ssbl.modules[0].offset + 16
        data[at:at + 13] = b"ehide.device\0"
        with self.assertRaisesRegex(patch.PatchError, "exactly one"):
            patch.ide_index(patch.parse_ssbl(patch.with_crc(data)))
        duplicate = patch.replace_module(self.ssbl, driver_fixture(), 0)
        with self.assertRaisesRegex(patch.PatchError, "exactly one"):
            patch.patch_flash(patch.make_spirom(duplicate, romfs_fixture()), driver_fixture())

    def test_inconsistent_metadata(self):
        toc_end = self.ssbl.modules[-1].offset + len(self.ssbl.modules[-1].data) + 28
        for at, value in ((toc_end + 4, words(7)), (self.ssbl.modules[2].offset, words(0)),
                          (self.ssbl.modules[0].offset + 8, words(1))):
            damaged = bytearray(self.data[:-4])
            damaged[at:at + len(value)] = value
            with self.subTest(at=at), self.assertRaises(patch.PatchError):
                patch.parse_ssbl(patch.with_crc(damaged))
        damaged = bytearray(self.data[:-4])
        for site in self.ssbl.relocations:
            if patch.u32(damaged, site) == self.ssbl.table:
                patch.put32(damaged, site, 0)
        with self.assertRaisesRegex(patch.PatchError, "coverage"):
            patch.parse_ssbl(patch.with_crc(damaged))
        # Relocations into headers/data may not be inferred or omitted.
        sites = [s for s in self.ssbl.relocations if s != self.ssbl.modules[0].offset]
        bad = patch.with_crc(self.data[:self.ssbl.table] + patch.encode_relocations(sites))
        with self.assertRaisesRegex(patch.PatchError, "coverage"):
            patch.parse_ssbl(bad)


class ArchiveTests(unittest.TestCase):
    def archive(self, directory="custom/build/", readme=True, compression=zipfile.ZIP_STORED):
        flash = b"\xA5" * 0x21000 + patch.make_spirom(ssbl_fixture(), romfs_fixture())
        output = io.BytesIO()
        with zipfile.ZipFile(output, "w", compression=compression) as archive:
            archive.comment = b"custom ZIP comment"
            archive.writestr(directory + "tf4060r5_ram_flash.bin", flash)
            archive.writestr(directory + "tf4060r5_bus_prod.jed", b"CPLD contents")
            archive.writestr(directory + "reports/build.txt", b"build report")
            if readme:
                archive.writestr(directory + "README.md",
                                 "Image: " + hashlib.sha1(flash).hexdigest()[:8])
        return output.getvalue()

    def test_repacked_arbitrary_paths_and_optional_readme(self):
        for directory in ("", "future-release/", "nested/custom/build/"):
            for readme in (False, True):
                for compression in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED):
                    with self.subTest(directory=directory, readme=readme, compression=compression):
                        source = self.archive(directory, readme, compression)
                        result = patch.patch_archive(source, driver_fixture())
                        with zipfile.ZipFile(io.BytesIO(source)) as before, \
                                zipfile.ZipFile(io.BytesIO(result)) as after:
                            self.assertEqual(before.comment, after.comment)
                            for entry in before.infolist():
                                self.assertEqual(entry.compress_type,
                                                 after.getinfo(entry.filename).compress_type)
                                if not entry.filename.endswith((".bin", "README.md")):
                                    self.assertEqual(before.read(entry), after.read(entry.filename))
                            member = patch.flash_member(after)
                            _, ssbl, _ = patch.split_flash(after.read(member))
                            self.assertEqual(ssbl.modules[2].data, patch.prepare_driver(driver_fixture()))
                            notes = after.read(directory + "README.md")
                            self.assertIn(patch.sha256(source).encode(), notes)
                            self.assertIn(patch.sha256(after.read(member)).encode(), notes)
                            if readme:
                                self.assertIn(hashlib.sha1(after.read(member)).hexdigest()[:8].encode(), notes)

    def test_driver_can_be_updated(self):
        first = patch.patch_archive(self.archive(), driver_fixture())
        second = patch.patch_archive(first, driver_fixture(4096))
        third = patch.patch_archive(second, driver_fixture(4096))
        with zipfile.ZipFile(io.BytesIO(second)) as a, zipfile.ZipFile(io.BytesIO(third)) as b:
            self.assertEqual(a.read(patch.flash_member(a)), b.read(patch.flash_member(b)))
            _, ssbl, _ = patch.split_flash(a.read(patch.flash_member(a)))
            self.assertEqual(ssbl.modules[2].data, patch.prepare_driver(driver_fixture(4096)))

    def test_duplicate_zip_names_rejected(self):
        source = io.BytesIO(self.archive())
        with zipfile.ZipFile(source, "a") as z:
            with warnings.catch_warnings():
                warnings.simplefilter("ignore", UserWarning)
                z.writestr("custom/build/tf4060r5_ram_flash.bin", b"duplicate")
        with self.assertRaisesRegex(patch.PatchError, "duplicate"):
            patch.patch_archive(source.getvalue(), driver_fixture())

    def test_unstructured_readme_is_preserved(self):
        source = io.BytesIO(self.archive(readme=False))
        readme = b"Local firmware notes: \xff\xfe, no checksum provided.\n"
        with zipfile.ZipFile(source, "a") as z:
            z.writestr("custom/build/README.md", readme)
        result = patch.patch_archive(source.getvalue(), driver_fixture())
        with zipfile.ZipFile(io.BytesIO(result)) as z:
            self.assertTrue(z.read("custom/build/README.md").endswith(readme))

    def test_multiple_or_wrong_board_images_rejected(self):
        for names in (("tf1260_ram_flash.bin",),
                      ("one/tf4060r5_ram_flash.bin", "two/tf4060r5_ram_flash.bin")):
            output = io.BytesIO()
            with zipfile.ZipFile(output, "w") as z:
                for name in names:
                    z.writestr(name, b"not read")
            with self.assertRaisesRegex(patch.PatchError, "exactly one TF4060"):
                patch.patch_archive(output.getvalue(), driver_fixture())



class CliTests(unittest.TestCase):
    def test_reject_zip_without_creating_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "input.zip"
            driver = root / "TF-lide.device"
            output = root / "output.zip"
            source.write_bytes(b"unsupported zip")
            driver.write_bytes(driver_fixture())
            with contextlib.redirect_stderr(io.StringIO()):
                status = patch.main([str(source), str(driver), "-o", str(output)])
            self.assertEqual(status, 1)
            self.assertFalse(output.exists())
            self.assertEqual(source.read_bytes(), b"unsupported zip")

    def test_existing_output_and_input_alias_are_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "keep.zip"
            path.write_bytes(b"keep these bytes")
            for output in (path, path.parent / "alias.zip"):
                if output != path:
                    output.symlink_to(path)
                with contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(patch.main([str(path), str(path), "-o", str(output)]), 1)
                self.assertEqual(path.read_bytes(), b"keep these bytes")


class ReleaseTests(unittest.TestCase):
    def test_firmware_archive(self):
        if FIRMWARE is None:
            self.skipTest("pass --firmware and --driver for a firmware archive")
        original_data = FIRMWARE.read_bytes()
        driver = DRIVER.read_bytes()
        patched_data = patch.patch_archive(original_data, driver)
        with zipfile.ZipFile(io.BytesIO(original_data)) as original, \
                zipfile.ZipFile(io.BytesIO(patched_data)) as patched:
            member = patch.flash_member(original)
            readme_member = str(Path(member).with_name("README.md"))
            self.assertIsNone(patched.testzip())
            for name in original.namelist():
                if name not in (member, readme_member):
                    self.assertEqual(patched.read(name), original.read(name))
            before = original.read(member)
            after = patched.read(member)
            offset, old_ssbl, old_romfs = patch.split_flash(before)
            new_offset, new_ssbl, new_romfs = patch.split_flash(after)
            self.assertEqual(offset, new_offset)
            self.assertEqual(before[:offset], after[:offset])
            self.assertEqual(old_romfs, new_romfs)
            index = patch.ide_index(old_ssbl)
            self.assertEqual(new_ssbl.modules[index].data, patch.prepare_driver(driver))
            self.assertEqual(patch.replace_module(new_ssbl, old_ssbl.modules[index].data), old_ssbl.data)
            for i in range(len(old_ssbl.modules)):
                if i != index:
                    self.assertEqual(old_ssbl.modules[i].data, new_ssbl.modules[i].data)
            readme = patched.read(readme_member)
            self.assertIn(patch.sha256(after).encode(), readme)
        self.assertEqual(FIRMWARE.read_bytes(), original_data)
        self.assertEqual(DRIVER.read_bytes(), driver)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--firmware", type=Path)
    parser.add_argument("--driver", type=Path)
    options, rest = parser.parse_known_args()
    if bool(options.firmware) != bool(options.driver):
        parser.error("--firmware and --driver must be supplied together")
    FIRMWARE, DRIVER = options.firmware, options.driver
    unittest.main(argv=[sys.argv[0]] + rest)
