SECTIONS {
    rom = .;
    .bootldr 0x00: {
        *(BOOTLDR);
        RESERVE(0x800-.);
    } = 0xFFFF
    .reserved 0x800: {
        FILL8(0xFF);
        RESERVE(0x1000-.);
    }
    .device 0x1000 : {
        *(DEVICE)
    }
    .fill : {
        RESERVE(0x8000-4-.);
        LONG(0x4C494445);
    }
}
