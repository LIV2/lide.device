#pragma GCC optimize ("-fomit-frame-pointer")
/**
 * ata_read_fast_long
 * 
 * Fast copy of a 512-byte sector using movem
 * Adapted from the open source at_apollo_device by Frédéric REQUIN
 * https://github.com/fredrequin/at_apollo_device
 * 
 * NOTE! source needs to be 48 bytes before the end of the data port!
 * The 68000 does an extra memory access at the end of a movem instruction!
 * Source: https://github.com/prb28/m68k-instructions-documentation/blob/master/instructions/movem.md
 * 
 * With the src of end-48 the error reg will be harmlessly read instead.
 * 
 * @param source Pointer to drive data port
 * @param destination Pointer to source buffer
*/
static inline void ata_read_fast_long (void *source, void *destination) {
    source += (512 - 48);

    asm volatile ("moveq  #48,d7\n\t"

    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    "add.l   d7,%1\n\t" // 1
    
    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    "add.l   d7,%1\n\t" // 2
    
    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    "add.l   d7,%1\n\t" // 3
    
    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    "add.l   d7,%1\n\t" // 4

    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    "add.l   d7,%1\n\t" // 5
    
    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    "add.l   d7,%1\n\t" // 6

    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"                  
    "add.l   d7,%1\n\t" // 7

    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"                   
    "add.l   d7,%1\n\t" // 8

    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"                   
    "add.l   d7,%1\n\t" // 9

    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    "add.l   d7,%1\n\t" // 10

    "movem.l 16(%0),d0-d6/a1\n\t"
    "movem.l d0-d6/a1,(%1)\n\t"
    :
    :"a" (source),"a" (destination)
    :"a1","a2","a3","a4","a6","d0","d1","d2","d3","d4","d5","d6","d7"
    );
}

/**
 * ata_write_fast_long
 * 
 * Fast copy of a 512-byte sector using movem
 * Adapted from the open source at_apollo_device by Frédéric REQUIN
 * https://github.com/fredrequin/at_apollo_device
 * 
 * @param source Pointer to source buffer
 * @param destination Pointer to drive data port
*/
static inline void ata_write_fast_long (void *source, void *destination) {
    destination += (512 - 48);

    asm volatile (
    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    
    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    
    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    
    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"

    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    
    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"

    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"                  

    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"                   

    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"                   

    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"

    "movem.l (%0)+,d0-d6/a1\n\t"
    "movem.l d0-d6/a1,(%1)\n\t"
    :
    :"a" (source),"a" (destination)
    :"a1","a2","a3","a4","a6","d0","d1","d2","d3","d4","d5","d6","d7"
    );
}

#pragma GCC reset_options