// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */

/**
 * ata_xfer_long_move16
 * 
 * Transfer a sector using move16 - (68040+ only!)
 * 
*/
void ata_xfer_long_move16 (void *source, void *destination) {
    asm volatile (
        "move.l #3,d0           \n\t"
        ".l3:                   \n\t"
        ".rept  32              \n\t"
        "move16 (%0)+,(%1)+      \n\t"
        ".endr                  \n\t"
        "dbra   d0,.l3"
    :
    :"a" (source), "a" (destination)
    :"d0"
    );
}
