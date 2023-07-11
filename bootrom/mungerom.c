#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// Chop up the bootrom section into nibbles for Z2 nibblewise bootrom  - Kick 1.3 and below don't support DAC_BYTEWIDE :(
// The device driver will be loaded bytewise by reloc
int main () {
  FILE *fh = fopen("obj/bootldr","r");

  char *src,*dst = NULL;

  src = malloc(2048);
  dst = malloc(4096);

  int len = fread(src,1,2048,fh);
  fclose(fh);
  for (int i=0; i<len;i++) {
    char hi = *(src+i) & 0xF0;
    char lo = *(src+i) & 0x0F;
    lo <<= 4;
    dst[i<<1]     = hi;
    dst[(i<<1)+1] = lo;
  }

  fh = fopen("obj/bootnibbles","w");

  fwrite(dst,1,len*2,fh);

  fclose(fh);

  free(src);
  free(dst);
}
