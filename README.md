# Open source Amiga IDE device driver

## Features
* Autoboot
* Works with Kickstart 1.3 and up
* Supports up to 127GB drives
* Supports ATAPI Devices (CD/DVD-ROM, Zip disk etc)
* SCSI Direct, NSD, TD64 support

## TODO
* Add disk change int support for ATAPI devices
* Implement geometry for LS-120/ZIP drive etc
* Return Sense data for ATAPI commands

## Hardware implementation
* IDECS1 is asserted when A12 is low and the IDE device's base address is decoded
* IDECS2 is asserted as above but when A13 is low rather than A12
* IDECS2 can instead be routed to a second IDE channel as it's IDECS1 and the driver will detect this
* IDE interface is byte-swapped
* Buffered interface
* D15/14 pulled up with a 10K resistor to allow detection of floating bus when no drive present.
* IDE A0/1/2 connected to CPU A9/10/11 providing 512 Byte separation of registers to allow for MOVEM trick

A reference design for this is the [CIDER](https://github.com/LIV2/CIDER) project