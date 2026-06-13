# Mounter: AmigaOS Generic Mounter

Generic autoboot / automount RDB parser and mounter.

This is the mounter code from a4091.device.

## 1. Introduction

This Mounter software is a generic, autoboot/automount Rigid Disk Block (RDB)
parser and mounter for AmigaOS. Originally developed by Toni Wilen and extended
by Stefan Reinauer and Matt Harlum. It is used by the `a4091.device` and
'lide.device' drivers. This software is engineered to be a robust and highly
compatible solution for device drivers that need to mount partitions and
filesystems at boot time.

It is designed to be highly portable, 68000-compatible, and capable of
operating in diverse environments, from a Kickstart 1.3 Boot ROM to a modern
AmigaOS system. Its primary function is to scan for storage devices, interpret
their partition maps (RDB, MBR, GPT), load the necessary filesystems, and make
them available to the operating system.

---

## 2. Core Features

The Mounter provides a comprehensive set of features making it a versatile
solution:

* **Broad OS Compatibility**: Full support for Kickstart 1.3 and newer,
  including its specific autoboot mechanisms.
* **CPU Compatibility**: Compatible with the Motorola 68000 processor, ensuring
  it runs on all classic Amiga models.
* **Autoboot Capability**: Can participate in the Amiga's autoconfig boot
  process to mount bootable partitions
* **Full Automount Support**: Automatically finds and mounts all valid
  partitions it discovers.
* **Rich Filesystem Support**:
    * **RDB**: Full support for the Amiga Rigid Disk Block (RDB) standard.
    * **MBR & GPT**: Support for PC-style Master Boot Record and GUID Partition
      Table layouts, enhancing cross-platform compatibility.
    * **CD-ROM**: Capable of booting from CD-ROMs following the ISO 9660
      standard with Amiga-specific "Amiga Boot" or "CDTV" system identifiers.
* **LUN Support**: Can scan for and mount devices on multiple Logical Unit
  Numbers (LUNs).

---

## 3. High-Level Architecture

The Mounter's architecture is centered around a single primary entry point,
`MountDrive()`, which orchestrates the entire mounting process. It operates
by opening a specified device driver, scanning its units, and attempting to
identify and mount partitions.

### 3.1. Key Data Structures

* **`struct MountStruct` (`mounter.h`)**: This is the main input structure
  passed to the `MountDrive` function. It defines the parameters for a mounting
  session.
    * `deviceName`: The name of the device driver to use (e.g., `scsi.device`).
    * `unitNum`: A pointer to an array of unit numbers to scan.
    * `creatorName`: A string to identify the creator of the filesystem entries.
    * `configDev`: A pointer to the `ConfigDev` structure for an autoconfig
      board, essential for autobooting.
    * `luns`, `slowSpinup`, `cdBoot`, `ignoreLast`: Boolean flags to control
      behavior like LUN scanning, spin-up delays, CD booting, and handling of
      the RDB `RDBFF_LAST` flag.
    * `SysBase`: A pointer to the Exec library base.

* **`struct MountData` (`mounter.c`)**: An internal state-management structure
  used during the `MountDrive` execution. It holds pointers to opened
  libraries, the I/O request, device geometry, and state variables for the
  scanning process.

### 3.2. Control Flow

The logical flow of the `MountDrive` function is as follows:

1.  **Initialization**:
    * Allocate a `MountData` structure to hold operational state.
    * Open required system libraries (`expansion.library`, `dos.library`).
    * Create a message port and an I/O request for communicating with the
      device driver.

2.  **Device Scanning Loop**:
    * Iterate through the specified SCSI targets (and LUNs, if enabled).
    * For each unit, attempt to `OpenDevice()`.

3.  **Partition Scheme Identification**:
    * If a device is opened successfully, get its geometry using
      `TD_GETGEOMETRY`.
    * Based on the device type (`DG_DIRECT_ACCESS`, `DG_CDROM`), determine
      the scanning strategy.
    * **For Direct Access Devices**:
        1.  Attempt to find an RDB by scanning the first `RDB_LOCATION_LIMIT`
	    blocks (`ScanRDSK`).
        2.  If no RDB is found, check for a GUID Partition Table (GPT) at
	    block 1 (`ScanMBR`).
        3.  If no GPT is found, check for a Master Boot Record (MBR) at block
	    0 (`ScanMBR`).
    * **For CD-ROM Devices**:
        1.  Check if the unit is ready and contains a data CD (`ScanCDROM`).
        2.  Check the Primary Volume Descriptor (PVD) for "AMIGA BOOT" or
	    "CDTV" identifiers (`CheckPVD`).
        3.  If not an Amiga bootable CD, fall back to scanning for an RDB
	    (`ScanRDSK`).

4.  **Partition Processing**:
    * Call the appropriate parsing function (`ParseRDSK`, `ParseGPT`,
      `ParseMBR`, `ScanCDROM`).
    * These functions iterate through partition entries.

5.  **Filesystem Loading & Mounting**:
    * For each valid partition, `ParsePART` is called.
    * It reads the partition's `DosEnvec` to determine the required filesystem
      `DosType`.
    * It calls `ParseFSHD` to find or load the required filesystem. `ParseFSHD`
      searches `FileSystem.resource` and, if not found or if the version is
      older, loads the filesystem from the disk's FileSystem Header Blocks
      (`FSHD`).
    * The loaded filesystem code is relocated in memory using the `fsrelocate`
      function.
    * A `DeviceNode` is created using `MakeDosNode()` and populated with the
      partition's parameters.
    * The `DeviceNode` is added to the system's mount list using `AddDosNode()`
      or `AddBootNode()` (for bootable partitions).

6.  **Cleanup**:
    * The device is closed.
    * The loop continues to the next unit/target.
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
4.  It processes relocation hunks (`HUNK_RELOC32`) to resolve memory address
    pointers within the loaded code and data, making the filesystem executable
    from its new location in RAM.
5.  Finally, it performs a `CacheClearU()` to ensure instruction caches are
    flushed before the OS attempts to execute the newly loaded code.

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
`expansion.library`'s `MountList`, as `AddBootNode` is not fully featured on KS
1.3.

### 4.4. Autoboot Process

When a bootable partition is found and the mounter is operating in an autoboot
context (i.e., a `configDev` is provided), it uses `AddBootNode()` instead of
`AddDosNode()`. This function links the mountable partition directly to the
autoconfig hardware board, signaling to the OS that this is a candidate for
booting. If no `configDev` is provided, the mounter can create a "fake" one to
enable autobooting from devices that are not on a standard autoconfig chain.

---

## 5. Dependencies

The Mounter relies on the following standard AmigaOS libraries:

* `exec.library` (v34+): For memory management, tasking, ports, and core system
  functions.
* `expansion.library` (v34+): For adding boot nodes and interacting with the
  autoconfig process.
* `dos.library` (v34+): For creating `DeviceNode` structures and adding them
  to the DOS list.

---

## 6. License

Copyright 2021-2022 Toni Wilen

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.  
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.  
