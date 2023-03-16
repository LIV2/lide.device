#include <proto/exec.h>
#include <proto/expansion.h>
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/errors.h>
#include <libraries/dos.h>
#include <stdbool.h>
#include <stdio.h>
#include "ata.h"
#include "device.h"
#include <string.h>

UWORD *buf;

int main() {

    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    struct Library *ExpansionBase;
    struct ConfigDev *cd;

    if ((ExpansionBase = OpenLibrary("expansion.library",0)) == NULL) {
        printf("Could not open Expansion.library\n");
        return 1;
    }

    if ((cd = FindConfigDev(NULL,MANUF_ID,DEV_ID)) == NULL) {
        printf("Could not find board\n");
        return 1;
    }

    struct IDEUnit *unit = AllocMem(sizeof(struct IDEUnit),MEMF_ANY);

    unit->cd = cd;
    unit->channel = 0;
    unit->primary = true;
    
    if (ata_init_unit(unit)) {
        printf("Data Port: %06x\n",unit->drive);
        printf("Drive status: %02x\n",*unit->drive->status_command);

        buf = AllocMem(512,MEMF_ANY|MEMF_CLEAR);
        printf("Buffer address: %06X\n", (int)buf);
        ata_identify(unit,buf);
        UWORD cyl, head, sectors = 0;

        cyl     = buf[ata_identify_cylinders];
        head    = buf[ata_identify_heads];
        sectors = buf[ata_identify_sectors];

        char *model_src = (void *)((UWORD *)buf + ata_identify_model);
        char *model = AllocMem(41,MEMF_CLEAR|MEMF_ANY);

        memcpy(model, model_src, 40);

        printf("Model: %s\n",model);

        printf("C: %d H: %d S: %d\n",cyl, head, sectors);
        printf("PIO Mode(s): %d\n",*((UWORD *)buf + ata_identify_pio_modes));
        printf("Capabilities: ");
        if (buf[ata_identify_capabilities] & ata_capability_dma) printf("DMA ");
        if (buf[ata_identify_capabilities] & ata_capability_lba) printf("LBA");
        printf("\n");

    } else {
        printf("No drive found.\n");
    }



    FreeMem(unit,sizeof(struct IDEUnit));
    FreeMem(buf,512);
}