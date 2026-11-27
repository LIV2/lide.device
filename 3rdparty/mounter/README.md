# Mounter: AmigaOS Generic Mounter

Generic autoboot / automount RDB parser and mounter.

This mounter is used by various Amiga disk controller drivers, including
`a4091.device` (A4091/A4092) and `lide.device` (LIDE).

## 1. Introduction

This Mounter software is a generic, autoboot/automount Rigid Disk Block (RDB)
parser and mounter for AmigaOS. It was originally developed by Toni Wilen and
extended by Stefan Reinauer and Matt Harlum. It is meant to be built into
device drivers that need to mount partitions and filesystems at boot time.

It is designed to be highly portable, 68000-compatible, and capable of
operating in diverse environments, from a Kickstart 1.3 Boot ROM to a modern
AmigaOS system. Its primary function is to scan the units of a device driver,
interpret their partition maps (RDB, and optionally MBR/GPT), load the
necessary filesystems, and make the partitions available to the operating
system.

---

## 2. Core Features

* **Broad OS Compatibility**: Full support for Kickstart 1.3 and newer,
  including autoboot on Kickstart 1.3.
* **CPU Compatibility**: Compatible with the Motorola 68000 processor, ensuring
  it runs on all classic Amiga models.
* **Autoboot Capability**: When run from a Boot ROM (before `dos.library` is
  available), bootable partitions are added as boot nodes so the system can
  boot from them.
* **Full Automount Support**: Automatically finds and mounts all partitions
  that are not flagged `PBFF_NOMOUNT`.
* **Partition Formats**:
    * **RDB**: Full support for the Amiga Rigid Disk Block (RDB) standard,
      including loading filesystems stored in the RDB (`FSHD`/`LSEG`).
      All RDB blocks are checksum-verified, honoring `SummedLongs`.
    * **MBR & GPT** (optional, build with `DISKLABELS`): PC-style partition
      tables. Only FAT partitions are mounted (MBR partitions, and GPT
      partitions of type "Microsoft Basic Data"), as `MS0:`..`MS9:`, using a
      `FAT\1` (`0x46415401`, e.g. FAT95) filesystem.
    * **CD-ROM** (optional, `cdBoot`): Booting from ISO 9660 CDs whose system
      identifier is `AMIGA BOOT` or `CDTV`, and from CDs that carry an RDB
      instead of an ISO 9660 volume descriptor.
* **Filesystem Loading**: Filesystems can come from the RDB, from
  `FileSystem.resource`, or from the driver itself via the `LoadFileSys`
  callback (e.g. a filesystem stored in the controller's ROM).
* **LUN Support**: Can scan for and mount devices on multiple Logical Unit
  Numbers (LUNs).
* **RDB Flags**: Honors `RDBFF_LAST` (stop scanning further units, unless
  `ignoreLast` is set) and `RDBFF_LASTLUN` (stop scanning further LUNs).

---

## 3. High-Level Architecture

The Mounter's architecture is centered around a single primary entry point,
`MountDrive()`, which orchestrates the entire mounting process. It opens the
given device driver for each unit, and attempts to identify and mount
partitions.

### 3.1. Key Data Structures

* **`struct MountStruct` (`mounter.h`)**: This is the main input structure
  passed to the `MountDrive` function. It defines the parameters for a mounting
  session.
    * `deviceName`: The name of the device driver to use (e.g., `scsi.device`).
    * `unitNum`: Currently ignored. `MountDrive()` always scans targets 0-7
      (and their LUNs, if enabled). The field is kept so the structure layout
      does not change.
    * `creatorName`: A string to identify the creator of the filesystem
      entries added to `FileSystem.resource`. May be `NULL`.
    * `configDev`: A pointer to the `ConfigDev` structure of the autoconfig
      board. It is passed to `AddBootNode()` for bootable partitions.
    * `SysBase`: A pointer to the Exec library base.
    * `LoadFileSys`: An optional callback that lets the driver provide a
      filesystem (for example from its ROM). It is called with the wanted
      DosType(s) and should add the filesystem to `FileSystem.resource`.
      Set to `NULL` if not used.
    * `LoadFileSysCtx`: Context pointer passed to the `LoadFileSys` function.
    * `luns`, `slowSpinup`, `cdBoot`, `ignoreLast`: Boolean flags to control
      LUN scanning, spin-up delays, CD booting, and whether scanning continues
      to later device units after an RDB with `RDBFF_LAST` is found.
    * `hostId`: The host controller's own SCSI ID, which is skipped during the
      scan. Set to 255 for non-SCSI controllers.

* **`struct MountData` (`mounter.c`)**: An internal state-management structure
  used during the `MountDrive` execution. It holds pointers to opened
  libraries, the I/O request, the block size, and state variables for the
  scanning process.

### 3.2. Control Flow

The logical flow of the `MountDrive` function is as follows:

1.  **Initialization**:
    * Open `expansion.library` (required) and `dos.library` (optional).
      Whether `dos.library` could be opened decides whether partitions are
      added as boot nodes (Boot ROM mode) or mounted directly.
    * Allocate a `MountData` structure to hold operational state.
    * Create a message port and an I/O request for communicating with the
      device driver.

2.  **Device Scanning Loop**:
    * Iterate through SCSI targets 0-7, skipping `hostId`, and through LUNs
      if `luns` is set.
    * For each unit, attempt to `OpenDevice()`.

3.  **Partition Scheme Identification**:
    * If a device is opened successfully, get its geometry using
      `TD_GETGEOMETRY`.
    * Based on the device type, determine the scanning strategy.
    * **For Direct Access Devices** (`DG_DIRECT_ACCESS`):
        1.  Attempt to find an RDB by scanning the first `RDB_LOCATION_LIMIT`
	    blocks (`ScanRDSK`).
        2.  With `DISKLABELS` only: if no RDB is found, check for a GUID
	    Partition Table (GPT) at block 1, then for a Master Boot Record
	    (MBR) at block 0 (`ScanMBR`).
    * **For CD-ROM Devices** (`DG_CDROM`, `DG_WORM`, `DG_OPTICAL_DISK`),
      only if `cdBoot` is set (`ScanCDROM`):
        1.  Check that a disc is inserted and that track 1 is a data track.
        2.  Read the ISO 9660 Primary Volume Descriptor (`CheckPVD`).
        3.  If there is no PVD, scan for an RDB (`ScanRDSK`).
        4.  If there is a PVD with system identifier `AMIGA BOOT` or `CDTV`,
	    mount the CD as `CD0:` (bootable, priority 2) using a `CD01` or
	    `CDVD` filesystem. CDs with any other system identifier are
	    ignored.

4.  **Partition Processing**:
    * Call the appropriate parsing function (`ParseRDSK`, `ParseGPT`,
      `ParseMBR`).
    * These functions iterate through partition entries.

5.  **Filesystem Loading & Mounting** (RDB):
    * For each partition without `PBFF_NOMOUNT`, `ParsePART` is called.
    * It reads the partition's `DosEnvec` to determine the required filesystem
      `DosType`.
    * It calls `ParseFSHD` to find or load the required filesystem.
      `ParseFSHD` walks the RDB's FileSystem Header Blocks (`FSHD`). If one
      matches the `DosType` and is newer than the version in
      `FileSystem.resource` (or there is none there), the filesystem is
      loaded from its `LSEG` blocks. Otherwise the `LoadFileSys` callback is
      called (if set) and the filesystem is taken from `FileSystem.resource`.
    * The loaded filesystem code is relocated in memory using the `fsrelocate`
      function.
    * Duplicate DOS device names get a `.1`, `.2`, ... suffix.
    * A `DeviceNode` is created using `MakeDosNode()` and populated with the
      partition's parameters and the filesystem's patch flags.
    * If `dos.library` is not available yet (Boot ROM mode) and the partition
      is bootable, it is added with `AddBootNode()`. Otherwise it is mounted
      with `AddDosNode()`.

6.  **Cleanup**:
    * The drive motor is turned off and the device is closed.
    * The loop continues to the next LUN/target, unless `RDBFF_LASTLUN` /
      `RDBFF_LAST` stops it.
    * After the loop, all allocated resources (I/O requests, ports, library
      bases) are freed.

---

## 4. Core Functionality Details

### 4.1. Filesystem Relocation (`fsrelocate`)

A critical function is `fsrelocate`, which loads Amiga HUNK-formatted
filesystem binaries from disk into memory.
1.  It starts by reading a `HUNK_HEADER`.
2.  It pre-allocates memory for all hunks (`HUNK_CODE`, `HUNK_DATA`,
    `HUNK_BSS`) defined in the header.
3.  It reads each hunk's data from the LSEG (Load Segment) blocks on disk.
4.  It processes relocation hunks (`HUNK_RELOC32`, `HUNK_RELOC32SHORT`) to
    resolve memory address pointers within the loaded code and data, making
    the filesystem executable from its new location in RAM.
5.  Finally, on Kickstart 2.0 and newer, it calls `CacheClearU()` to ensure
    instruction caches are flushed before the OS attempts to execute the newly
    loaded code.

### 4.2. Filesystem Resource Management (`FSHDProcess`, `FSHDAdd`)

The mounter interacts carefully with the central `FileSystem.resource`.
* `FSHDProcess` is responsible for checking if a required filesystem
  (identified by `DosType`) is already present in the resource list.
* If a filesystem is not present, or if the version on disk is newer than the
  one in memory, it allocates a new `FileSysEntry`.
* After a filesystem is successfully loaded and relocated by `fsrelocate`,
  `FSHDAdd` adds the new `FileSysEntry` to `FileSystem.resource`,
  making it available for all subsequent mounting operations
  system-wide.
* If `FileSystem.resource` does not exist (common on KS 1.3), it is created.

### 4.3. Kickstart 1.3 Compatibility

To maintain compatibility with Kickstart 1.3, which lacks certain OS functions,
the mounter includes its own implementations:
* `W_CreateMsgPort` / `W_DeleteMsgPort`
* `W_CreateIORequest` / `W_DeleteIORequest`

These functions use `AllocMem()` to create the necessary structures and manage
signals manually, mimicking the behavior of their modern counterparts. For
autobooting, it manually creates and inserts a `BootNode` into the
`expansion.library`'s `MountList`, as `AddBootNode()` does not exist on KS
1.3. When mounting without autoboot on KS 1.3, it starts the filesystem
process with `DeviceProc()`, since `ADNF_STARTPROC` is not supported there.

### 4.4. Autoboot Process

When a bootable partition is found and `dos.library` is not available yet
(i.e. the mounter runs from a Boot ROM during autoconfig), it uses
`AddBootNode()` instead of `AddDosNode()`. This links the partition to the
autoconfig board given in `configDev`, signaling to the OS that this is a
candidate for booting. When `dos.library` is available, partitions are
mounted right away and are not boot candidates.

When built with `NO_CONFIGDEV`, and no `configDev` is provided, the mounter
creates a "fake" `ConfigDev` to enable autobooting from devices that are not
on a standard autoconfig chain. This requires the driver to provide the
`entrypoint` and `bootblock` symbols.

---

## 5. Build-Time Options

* `A4091`: Build for `a4091.device`. Uses the driver's own geometry function,
  enables `printf` output, and waits for drives that are not ready yet.
* `DISKLABELS`: Enable MBR and GPT support. Requires `legacy.h`.
* `NO_CONFIGDEV`: Enable the fake `ConfigDev` described above.
* `DEBUG_MOUNTER`: In the `a4091.device` build, send the mounter's debug
  output to the serial port.

---

## 6. Dependencies

The Mounter relies on the following standard AmigaOS libraries:

* `exec.library` (v34+): For memory management, tasking, ports, and core system
  functions.
* `expansion.library` (v34+): For adding boot nodes and interacting with the
  autoconfig process.
* `dos.library` (v34+, optional): For starting filesystem processes on
  Kickstart 1.3. Its absence selects Boot ROM (autoboot) mode.

---

## 7. License

Copyright 2021-2022 Toni Wilen\
Copyright 2022-2026 Stefan Reinauer\
Copyright 2023-2026 Matt Harlum

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.  
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.  
