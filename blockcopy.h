#ifndef _BLOCK_COPY_H
#define _BLOCK_COPY_H
#pragma GCC push_options
#pragma GCC optimize ("-fomit-frame-pointer")
/**
 * ata_read_long_movem
 *
 * Fast copy of a 512-byte sector using movem
 * Adapted from the open source at_apollo_device by Frédéric REQUIN
 * https://github.com/fredrequin/at_apollo_device
 *
 * NOTE!
 * The 68000 does an extra memory access at the end of a movem instruction!
 * Source: https://github.com/prb28/m68k-instructions-documentation/blob/master/instructions/movem.md
 *
 * With the src of end-52 the error reg will be harmlessly read instead.
 *
 * @param source Pointer to drive data port
 * @param destination Pointer to source buffer
*/
static inline void ata_read_long_movem (void *source, void *destination) {

    asm volatile (
    "lea.l   460(%0),%0                 \n\t"
    "offset = 0                         \n\t"
    ".rep 9                             \n\t"
    "movem.l (%0),d0-d7/a1-a4/a6        \n\t"
    "movem.l d0-d7/a1-a4/a6,offset(%1)  \n\t"
    "offset = offset + 52               \n\t"
    ".endr                              \n\t"
    "movem.l 8(%0),d0-d7/a1-a3          \n\t"
    "movem.l d0-d7/a1-a3,offset(%1)     \n\t"
    :
    :"a" (source),"a" (destination)
    :"a1","a2","a3","a4","a6","d0","d1","d2","d3","d4","d5","d6","d7"
    );
}

/**
 * ata_write_long_movem
 *
 * Fast copy of a 512-byte sector using movem
 * Adapted from the open source at_apollo_device by Frédéric REQUIN
 * https://github.com/fredrequin/at_apollo_device
 *
 * @param source Pointer to source buffer
 * @param destination Pointer to drive data port
*/
static inline void ata_write_long_movem (void *source, void *destination) {

    asm volatile (
    ".rep 9                       \n\t"
    "movem.l (%0)+,d0-d7/a1-a4/a6 \n\t"
    "movem.l d0-d7/a1-a4/a6,(%1)  \n\t"
    ".endr                        \n\t"
    "movem.l (%0)+,d0-d7/a1-a3    \n\t"
    "movem.l d0-d7/a1-a3,(%1)     \n\t"
    :
    :"a" (source),"a" (destination)
    :"a1","a2","a3","a4","a6","d0","d1","d2","d3","d4","d5","d6","d7"
    );
}

/**
 * ata_read_long_move
 *
 * Read a sector using move - faster than movem on 68020+
 *
*/
static inline void ata_read_long_move (void *source, void *destination) {
    asm volatile (
        "moveq.l #3,d0          \n\t"
        ".l1:                   \n\t"
        ".rept  32              \n\t"
        "move.l (%0),(%1)+      \n\t"
        ".endr                  \n\t"
        "dbra   d0,.l1"
    :
    :"a" (source), "a" (destination)
    :"d0"
    );
}

/**
 * ata_write_long_move
 *
 * Write a sector using move - faster than movem on 68020+
 *
*/
static inline void ata_write_long_move (void *source, void *destination) {
    asm volatile (
        "moveq.l #3,d0          \n\t"
        ".l2:                   \n\t"
        ".rept  32              \n\t"
        "move.l (%0)+,(%1)      \n\t"
        ".endr                  \n\t"
        "dbra   d0,.l2"
    :
    :"a" (source), "a" (destination)
    :"d0"
    );
}

#pragma GCC pop_options
#endif