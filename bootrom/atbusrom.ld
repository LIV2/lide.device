SECTIONS {
    rom = .;
    .bootldr 0x00: {
        *(BOOTLDR);
        RESERVE(0x800-.);
    }
    .device : {
        *(DEVICE)
    }
    .fill : {
        RESERVE(0x4000-4-.);
        LONG(0x4C494445);
    }
}
