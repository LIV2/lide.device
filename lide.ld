ASSERT(SIZEOF(.bss) == 0, "Global variables found but BSS segment must be empty!")
ASSERT(SIZEOF(.data) == 0, "Global variables found but DATA segment must be empty")
