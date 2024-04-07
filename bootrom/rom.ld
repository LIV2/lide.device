SECTIONS {
    rom = .;
    .header 0x00: {
      LONG(0x4C495632);
    } = 0xFFFF
    .bootldr 0x04: {
        *(BOOTLDR);
        RESERVE(0x1000-.);
    }
    .device 0x1000 : {
        *(DEVICE)
    }
    .fill : {
        RESERVE(0x8000-4-.);
        LONG(0x4C494445);
    } =0xffff
}
