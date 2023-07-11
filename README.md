# Open source Amiga IDE device driver

## Features
* Autoboot
* Works with Kickstart 1.3 and up
* Supports up to 127GB drives
* Supports ATAPI Devices (CD/DVD-ROM, Zip disk etc)
* Boot from ZIP/LS-120 etc
* SCSI Direct, NSD, TD64 support

## TODO
* Add disk change int support for ATAPI devices

## Hardware implementation
* IDECS1 is asserted when A12 is low and the IDE device's base address is decoded
* IDECS2 is asserted as above but when A13 is low rather than A12
* IDECS2 can instead be routed to a second IDE channel as it's IDECS1 and the driver will detect this
* IDE interface is byte-swapped
* Buffered interface
* D15/14 pulled up with a 10K resistor to allow detection of floating bus when no drive present.
* IDE A0/1/2 connected to CPU A9/10/11 providing 512 Byte separation of registers to allow for MOVEM trick

A reference design for this is the [CIDER](https://github.com/LIV2/CIDER) project

## Acknowlegements
This driver uses the movem based fast read / write routines from [Frédéric REQUIN](https://github.com/fredrequin)'s [at_apollo_device](https://github.com/fredrequin/at_apollo_device)  
The release of the open source at_apollo_device inspired me to work on this.  

It is based on [jbilander](https://github.com/jbilander)'s [SimpleDevice](https://github.com/jbilander/SimpleDevice) skeleton.  

Thanks to [Stefan Reinauer](https://github.com/reinauer) and [Chris Hooper](https://github.com/cdhooper) for the [Open Source A4091 Driver](https://github.com/a4091/a4091-software/) which has also been a good source of inspiration for this project.  
Chris' [devtest](https://github.com/cdhooper/amiga_devtest) has also been very helpful for testing the driver.

## Third-party notice
mounter.c and reloc.S are adapted from the [A4091](https://github.com/A4091/a4091-software) open-source driver and is Copyright 2021-2022 Toni Wilen
The fast read/write routines for ATA devices are adapted from [Frédéric REQUIN](https://github.com/fredrequin)'s [at_apollo_device](https://github.com/fredrequin/at_apollo_device)  

## License
All software contained that is not provided by a third-party is covered by a GPL 2.0 Only license  
[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)


lide.device is licensed under the GPL-2.0 only license