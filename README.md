# lide.device - An open source IDE device driver for the Amiga

## Table of Contents
* [Features](#features)
* [Supported hardware](#supported-hardware)
* [Downloads](#downloads)
    * [lide-update.adf](#lide-updateadf)
    * [AIDE-boot.adf](#aide-bootadf)
    * [ROM images](#rom-images)
* [Boot from CDROM](#boot-from-cdrom)
    * [Loading BootCDFileSystem from Board ROM](#loading-bootcdfilesystem-from-board-rom)
* [Large drive (>4GB) support](#large-drive-4gb-support)
* [Hardware Implementation](#hardware-implementation)
* [Building / Development](#building--development)
* [Acknowledgements](#acknowledgements)
* [Third-party notice](#third-party-notice)
* [License](#license)

## Features
* Autoboot
* Works with Kickstart 1.3 and up
* [Supports up to 2TB drives*](#large-drive-4gb-support)
* Supports ATAPI Devices (CD/DVD-ROM, Zip disk etc)
* Boot from ZIP/LS-120 etc
* [Boot from CD-ROM*](#boot-from-cd-rom)
* SCSI Direct, NSD, TD64 support

# Supported hardware
lide.device supports the following devices:
* [CIDER](https://github.com/LIV2/CIDER)
* [RIPPLE](https://github.com/LIV2/RIPPLE-IDE)
* [N2630](https://github.com/jasonsbeer/Amiga-N2630)
* [SF2000](https://github.com/jbilander/SF2000)
* [SF500](https://github.com/jbilander/SF500)
* [Dicke Olga](https://www.amibay.com/threads/dicke-olga-030-cpu-64-128mb-accelerator-card-for-a2000-amiga-official-host.2443695/)
* [68030-TK2](https://gitlab.com/MHeinrichs/68030-tk2)
* [68EC020-TK](https://gitlab.com/MHeinrichs/68EC020-TK)
* [Zorro-LAN-IDE](https://gitlab.com/MHeinrichs/Zorro-LAN-IDE)
* [Zorro-LAN-IDE-SATA](https://gitlab.com/Hamag/zorrolanidesata)
* [CDTV-RAM-IDE](https://gitlab.com/MHeinrichs/CDTV-RAM-IDE)
* [AIDE](https://gitlab.com/MHeinrichs/AIDE)
* AT-Bus 2008 and clones

This list is not exhaustive, lide may also support other devices that implement the same IDE interface as Matzes TKs  
Let me know if there's something that should be added to this list!

# Downloads
ROM downloads are available under [releases](https://github.com/LIV2/lide.device/releases)

## lide-update.adf
`lide-update-<version>.adf` is a boot disk that can update the IDE rom on the following boards:
* [CIDER](https://github.com/LIV2/CIDER)
* [RIPPLE](https://github.com/LIV2/RIPPLE-IDE)
* [RIDE](https://github.com/LIV2/RIDE)
* [Dicke Olga](https://www.amibay.com/threads/dicke-olga-030-cpu-64-128mb-accelerator-card-for-a2000-amiga-official-host.2443695/)
* [68EC020-TK](https://gitlab.com/MHeinrichs/68EC020-TK)
* [Zorro-LAN-IDE-SATA](https://gitlab.com/Hamag/zorrolanidesata)

## AIDE-boot.adf
`aide-boot-<version>.adf` is a boot disk for the AIDE interface by Matze.  
This interface has no boot rom so the driver must first be loaded from disk or added to a custom kickstart.  
lide-update.lha contains the AIDE-lide.device file which can be used with `LoadModule` or added to a custom ROM.

## ROM images
Releases contain the following builds
|Filename|Boards supported|Notes|
|--------|----------------|-----|
|lide.rom|[CIDER](https://github.com/LIV2/CIDER)<br/>[RIPPLE](https://github.com/LIV2/RIPPLE-IDE)<br/>[RIDE](https://github.com/LIV2/RIDE)|Update by booting the latest lide-update adf under [releases](https://github.com/LIV2/lide.device/releases)|
|lide-N2630-high.rom<br>lide-N2630-low.rom|[N2630](https://github.com/jasonsbeer/Amiga-N2630)|Follow the N2630 documentation to combine LIDE with the N2630 ROM|
|lide-atbus.rom|AT-Bus 2008 and clones<br>[Dicke Olga](https://www.amibay.com/threads/dicke-olga-030-cpu-64-128mb-accelerator-card-for-a2000-amiga-official-host.2443695/)<br>[68030-TK2](https://gitlab.com/MHeinrichs/68030-tk2)<br>[68EC020-TK](https://gitlab.com/MHeinrichs/68EC020-TK)<br>[CDTV-RAM-IDE](https://gitlab.com/MHeinrichs/CDTV-RAM-IDE)<br>[Zorro-LAN-IDE](https://gitlab.com/MHeinrichs/Zorro-LAN-IDE)|ROM must be repeated as many times as needed to completely fill the flash i.e:<br>xxF010: 4x<br>xxF020: 8x<br>xxF040: 16x|

## Boot from CDROM
lide.device supports booting from CD-ROM but requires a CD Filesystem to be loaded for this to work.

There are a few options:
1. `CDFileSystem` from AmigaOS 3.2.2 loaded with LoadModule
2. `BootCDFileSystem` from OS 4 either:
    * Added to a custom Kickstart ROM **OR**
    * Added to the IDE/Accelerator ROM (subject to board support)

### Loading BootCDFileSystem from Board ROM
The following boards support loading BootCDFileSystem from ROM:
* RIPPLE
* RIDE
* 68EC020-TK
* 68030-TK2

`BootCDFileSystem` must be flashed to the second bank of the ROM.  
This can be achieved on RIPPLE and 68EC020-TK boards using `lideflash` i.e  
`lideflash -C BootCDFileSystem`

## Large drive (>4GB) support
For drives larger than 4GB it is required to use a Filesystem that supports TD64, NSD or SCSI-Direct  
The default FFS in OS 3.1 does **not** support these, and use of this above the 4GB boundary will result in data corruption!

There are several options for larger drive support
* [PFS3](https://aminet.net/package/disk/misc/pfs3aio)
* SFS
* FFS from AmigaOS 3.2, 3.9 etc
* [FFSTD64](https://aminet.net/package/disk/misc/ffstd64)

Also make sure to use the "Quick Format" option when formatting such partitions

## Hardware implementation
* IDECS1 is asserted when A12 is low and the IDE device's base address is decoded
* IDECS2 is asserted as above but when A13 is low rather than A12
* IDECS2 can instead be routed to a second IDE channel as it's IDECS1 and the driver will detect this
* IDE interface is byte-swapped
* Buffered interface
* D15/14 pulled up with a 10K resistor to allow detection of floating bus when no drive present.
* IDE A0/1/2 connected to CPU A9/10/11 providing 512 Byte separation of registers to allow for MOVEM trick
* Configure as a 128K board to permit loading CDFS from ROM **(Optional)**
* Hardware support of programming the flash **(Optional)**

A reference design for this is the [RIPPLE](https://github.com/LIV2/RIPPLE-IDE) project

## Building / Development
Building this code will require the following
* [Bebbo GCC](https://github.com/bebbo/amiga-gcc)
* [Amitools](https://github.com/cnvogelg/amitools)
* [VBCC m68k-amigaos target](http://phoenix.owl.de/vbcc/2022-05-22/vbcc_target_m68k-amigaos.lha)

The easiest way to get a working build environment is to use Docker
You can build inside docker as follows:
```  
docker run --rm -it -v ${PWD}:${PWD} -w ${PWD} stefanreinauer/amiga-gcc:latest make clean all
```

If you are using VS Code you can install the "Dev containers" extension which will allow you to develop with the environment ready to go.

## Acknowledgements
This driver uses the movem based fast read / write routines from [Frédéric REQUIN](https://github.com/fredrequin)'s [at_apollo_device](https://github.com/fredrequin/at_apollo_device)  
The release of the open source at_apollo_device inspired me to work on this.  

It is based on [jbilander](https://github.com/jbilander)'s [SimpleDevice](https://github.com/jbilander/SimpleDevice) skeleton.  

Thanks to [Stefan Reinauer](https://github.com/reinauer) and [Chris Hooper](https://github.com/cdhooper) for the [Open Source A4091 Driver](https://github.com/a4091/a4091-software/) which has also been a good source of inspiration for this project.  
Chris' [devtest](https://github.com/cdhooper/amiga_devtest) has also been very helpful for testing the driver.  
Thanks to [Olaf Barthel](https://github.com/obarthel) for providing code and valuable advice to improve the driver.  
Thanks to [MHeinrichs](https://gitlab.com/MHeinrichs) for contributing improvements to the driver.

## Third-party notice
reloc.S is adapted from the [A4091](https://github.com/A4091/a4091-software) open-source driver and is Copyright Stefan Reinauer  
mounter.c adapted from the [A4091](https://github.com/A4091/a4091-software) open-source driver and is Copyright 2021-2022 Toni Wilen  
The fast read/write routines for ATA devices are adapted from [Frédéric REQUIN](https://github.com/fredrequin)'s [at_apollo_device](https://github.com/fredrequin/at_apollo_device)  

## License
All software contained that is not provided by a third-party is covered by a GPL 2.0 Only license  
[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)


lide.device is licensed under the GPL-2.0 only license
