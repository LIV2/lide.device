SECTIONS {
    rom = .;
    .bootldr 0x00: {
        *(BOOTLDR)
    }
    .device 0x1000 : {
        *(DEVICE)
    }
    .fill : {
	    FILL8(0xFF);
        RESERVE(0x8000-.);
    } =0xffff
}
