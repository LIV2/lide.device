# TF4060 firmware with LIDE

`replace_tf_ide.py` replaces the embedded `ehide.device` in
`tf4060r5_v0.8.6_2026-11-26_19799ac.zip` with the TF build of LIDE. It needs
Python 3.8 or newer and the original ZIP; no extra Python packages, firmware
SDK or Amiga toolchain are needed to patch an existing driver binary.

Supply `TF-lide.device` from a build with TF4060 support. To build it from
this repository with the Amiga cross toolchain installed:

```sh
make build/TF-lide.device
```

Create a separate firmware archive:

```sh
python3 tools/replace_tf_ide.py \
    tf4060r5_v0.8.6_2026-11-26_19799ac.zip \
    build/TF-lide.device \
    --output tf4060r5_lide.zip
```

The script can be distributed on its own. The second argument can be any
path to the TF driver. Use the HUNK executable, not `lide.rom`, the generic
`lide.device`, or the AIDE/PCI builds. These builds share the same Resident
name, so the script cannot distinguish them by name. The installed Exec
device is called `lide.device`, regardless of the input filename.
RDB partitions use that name automatically. Any explicit `ehide.device`
references in mount files or scripts must be changed to `lide.device`.

The input ZIP must have SHA-256:

```text
d6e5cbe4e1d45e8c9c3217ebb3d2b58b2732823fb13f017c0a883061ab08b7d6
```

Only this exact release is supported. Other versions, modified or repacked
archives, unsupported HUNK layouts, oversized images and existing output
paths are rejected. There is no option to bypass the firmware identity check.

The output keeps the original archive paths. Its
`v0.8.6_2026-11-26_19799ac/tf4060r5_ram_flash.bin` contains LIDE. The README
identifies the custom image and records the input driver and output image
hashes. The source ZIP and driver file are unchanged.

The script creates an image only. TF4060 support is experimental; a successful
patch does not establish physical boot or disk I/O correctness. Keep the
original firmware available for recovery when testing on hardware.

## Image layout

The FPGA bitstream before `0x21000` is preserved. The SPI ROM after it contains
an MBR, an SSBL partition (type `0x68`) and a ROMFS partition (type `0x69`).
LIDE replaces ehide's HUNK file in SSBL module slot 2. Its embedded Resident
priority is set to ehide's priority 5, below the bundled ODFileSystem's
priority 10, so the CD filesystem is available when LIDE initializes.
This changes one byte in the embedded copy of a normal priority-10 TF build.

The SSBL's absolute pointers and their relocation locations are adjusted
for the new module length. The compact vlink relocation table and SSBL CRC
are regenerated, followed by the MBR partition sizes, addresses and disk ID.
The ROMFS, seven other resident modules and both CPLD files retain their
original bytes. The flash image must fit within 2 MiB.

This is specific to the identified SSBL: its PC-relative references stay
before the module table, and references to data moved by the replacement use
the absolute relocation table. A different bootloader needs an implementation
for its layout as well as an accepted hash.

## Checks

```sh
python3 tests/test_replace_tf_ide.py
python3 tests/test_replace_tf_ide.py \
    --firmware tf4060r5_v0.8.6_2026-11-26_19799ac.zip \
    --driver build/TF-lide.device
```

The first command uses generated fixtures. The second also checks the real
release, preservation of the other ZIP entries and modules, and reproduction
of the original SSBL after reversing the replacement.
