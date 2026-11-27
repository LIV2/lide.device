#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Generated format fixtures, plus optional original-release integration checks."""

import argparse
import contextlib
import hashlib
import io
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock
import zipfile
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import replace_tf_ide as patch

FIRMWARE = None
DRIVER = None


def words(*values):
    return struct.pack(">" + "I" * len(values), *values)


def driver_fixture(size=128):
    body = bytearray(size)
    body[0:2] = b"\x4e\x75"
    body[4:30] = struct.pack(">HII4BIII", 0x4AFC, 4, size, 1, 40, 3, 10, 64, 80, 32)
    body[32:34] = b"\x4e\x75"
    body[64:76] = b"lide.device\0"
    body[80:90] = b"lide test\0"
    return words(0x3F3, 0, 1, 0, 0, size // 4, 0x3E9, size // 4) + body + words(
        0x3EC, 5, 0, 6, 10, 18, 22, 26, 0, 0x3F2)


def ssbl_fixture():
    # Keep the release's table layout, with generated code and module contents.
    result = bytearray(patch.TOC_BEGIN) + patch.MARKER
    sites = [patch.RELOC_POINTER] + list(range(0x80, 0x80 + 56 * 4, 4))
    for index in range(8):
        at = len(result)
        payload = bytes([index + 1]) * (512 if index == patch.IDE_INDEX else 64)
        result += words(at + len(payload) + 28, len(payload), 0, 0) + payload + patch.MARKER
        sites.append(at)
    result += words(0, 8, 0xDEADBEEF) + bytes(12)
    table = len(result)
    patch.put32(result, patch.RELOC_POINTER, table)
    for i, site in enumerate(sites[1:57]):
        patch.put32(result, site, (table - 12, table - 8, table - 4, table, 0x40)[i % 5])
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
                self.assertEqual(patch.replace_module(after, self.ssbl.modules[2].data), self.data)
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

    def test_relocation_into_replacement_refused(self):
        damaged = bytearray(self.data[:-4])
        patch.put32(damaged, 0x80, self.ssbl.modules[2].offset + 16)
        parsed = patch.parse_ssbl(patch.with_crc(damaged))
        with self.assertRaisesRegex(patch.PatchError, "into the replaced driver"):
            patch.replace_module(parsed, b"\0" * 4)

    def test_truncated_long_delta_with_valid_crc(self):
        data = self.data[:self.ssbl.table] + words(1) + b"\0"
        with self.assertRaisesRegex(patch.PatchError, "truncated long"):
            patch.parse_ssbl(patch.with_crc(data))

    def test_spirom_rebuild_and_capacity(self):
        romfs = romfs_fixture()
        prefix = b"\xA5" * patch.SPI_OFFSET
        original = prefix + patch.make_spirom(self.data, romfs)
        with mock.patch.object(patch, "FLASH_SHA256", patch.sha256(original)):
            result = patch.patch_flash(original, driver_fixture(4096))
            after, new_romfs = patch.split_flash(result)
            self.assertEqual(result[:patch.SPI_OFFSET], prefix)
            self.assertEqual(new_romfs, romfs)
            self.assertEqual(after.modules[2].data, patch.prepare_driver(driver_fixture(4096)))
            with mock.patch.object(patch, "FLASH_CAPACITY", len(original)):
                with self.assertRaisesRegex(patch.PatchError, "flash capacity"):
                    patch.patch_flash(original, driver_fixture(65536))
        with self.assertRaisesRegex(patch.PatchError, "unsupported firmware"):
            patch.patch_flash(original, driver_fixture())


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
    def test_original_release(self):
        if FIRMWARE is None:
            self.skipTest("pass --firmware and --driver for the real release")
        original_data = FIRMWARE.read_bytes()
        driver = DRIVER.read_bytes()
        patched_data = patch.patch_archive(original_data, driver)
        with zipfile.ZipFile(io.BytesIO(original_data)) as original, \
                zipfile.ZipFile(io.BytesIO(patched_data)) as patched:
            self.assertIsNone(patched.testzip())
            self.assertEqual(patched.namelist(), original.namelist())
            for name in original.namelist():
                if name not in (patch.FLASH_MEMBER, patch.README_MEMBER):
                    self.assertEqual(patched.read(name), original.read(name))
            before = original.read(patch.FLASH_MEMBER)
            after = patched.read(patch.FLASH_MEMBER)
            self.assertEqual(before[:patch.SPI_OFFSET], after[:patch.SPI_OFFSET])
            old_ssbl, old_romfs = patch.split_flash(before)
            new_ssbl, new_romfs = patch.split_flash(after)
            self.assertEqual(old_romfs, new_romfs)
            self.assertIn(b"ehide.device\0", old_ssbl.modules[2].data)
            self.assertEqual(new_ssbl.modules[2].data, patch.prepare_driver(driver))
            self.assertNotIn(b"ehide.device\0", new_ssbl.data)
            self.assertEqual(patch.replace_module(new_ssbl, old_ssbl.modules[2].data), old_ssbl.data)
            for i in (0, 1, 3, 4, 5, 6, 7):
                self.assertEqual(old_ssbl.modules[i].data, new_ssbl.modules[i].data)
            readme = patched.read(patch.README_MEMBER)
            self.assertIn(patch.sha256(after).encode(), readme)
            self.assertIn(hashlib.sha1(after).hexdigest()[:8].encode(), readme)
        self.assertEqual(FIRMWARE.read_bytes(), original_data)
        self.assertEqual(DRIVER.read_bytes(), driver)
        # The previously patched archive cannot accidentally be patched again.
        with self.assertRaisesRegex(patch.PatchError, "unsupported ZIP"):
            patch.patch_archive(patched_data, driver)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--firmware", type=Path)
    parser.add_argument("--driver", type=Path)
    options, rest = parser.parse_known_args()
    if bool(options.firmware) != bool(options.driver):
        parser.error("--firmware and --driver must be supplied together")
    FIRMWARE, DRIVER = options.firmware, options.driver
    unittest.main(argv=[sys.argv[0]] + rest)
