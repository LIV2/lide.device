# TF4060 firmware with LIDE

`replace_tf_ide.py` replaces the embedded IDE driver in a TF4060 firmware
ZIP with the TF build of LIDE. It supports firmware versions that use the
TF4060 SPI ROM and SSBL module format, including repacked archives and
images already containing LIDE. It needs Python 3.8 or newer; no extra
Python packages, firmware SDK or Amiga toolchain are needed to patch an
existing driver binary.

Supply `TF-lide.device` from a build with TF4060 support. To build it from
this repository with the Amiga cross toolchain installed:

```sh
make build/TF-lide.device
```

Create a separate firmware archive:

```sh
python3 tools/replace_tf_ide.py \
    tf4060-firmware.zip \
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

The ZIP must contain exactly one `tf4060*_ram_flash.bin` image. The script
finds the SPI ROM partition table, SSBL module chain and relocation table
from their contents, then selects the `ehide.device` or `lide.device`
Resident. A previously converted image can therefore receive a newer driver.

The output keeps the original archive paths. The RAM flash image contains
LIDE, and its adjacent README identifies the custom image and records the
input archive, driver and output image hashes. Existing flash checksums in
the README are updated; a README is added if needed. Other archive members
and the source ZIP and driver file are unchanged.

Malformed or ambiguous images, unsupported bootloader/HUNK layouts,
oversized images and existing output paths are rejected.

The script creates an image only. TF4060 support is experimental; a successful
patch does not establish physical boot or disk I/O correctness. Keep the
original firmware available for recovery when testing on hardware.

## Image layout

The FPGA bitstream before the SPI ROM is preserved. The SPI ROM contains
an MBR, an SSBL partition (type `0x68`) and a ROMFS partition (type `0x69`).
Its offset is discovered rather than assumed. The SSBL module count,
IDE module position and relocation table location may vary between builds.
LIDE's embedded Resident priority is set to 5, below the bundled
ODFileSystem's priority 10, so the CD filesystem is available when LIDE
initializes. This changes one byte in the embedded copy of a normal
priority-10 TF build.

The SSBL's absolute pointers and their relocation locations are adjusted
for the new module length. The compact vlink relocation table and SSBL CRC
are regenerated, followed by the MBR partition sizes, addresses and disk ID.
The ROMFS, other resident modules and CPLD files retain their original
bytes. The flash image must fit within 2 MiB.

The supported SSBL layout places position-independent code before the
module table and records references to the table and following data as
absolute relocations. Firmware with a different bootloader format needs
corresponding parser support; a version number alone cannot establish
compatibility.

## Checks

```sh
python3 tests/test_replace_tf_ide.py
python3 tests/test_replace_tf_ide.py \
    --firmware tf4060-firmware.zip \
    --driver build/TF-lide.device
```

The first command uses generated fixtures with varying module layouts,
relocation tables and ZIP packaging. The second also checks the supplied
firmware, preservation of other ZIP entries and modules, and reproduction
of the original SSBL after reversing the replacement.
