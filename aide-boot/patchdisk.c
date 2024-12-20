#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>

#define bitmap_pos (881 * 512)

void checksumBlock(uint32_t *block, uint32_t size) {
	block[0] = 0;
	uint32_t checksum = 0;
	uint32_t new = 0;
	for (int i=0; i<size; i++) {
		new = checksum + ntohl(block[i]);
		if (new < checksum)
			checksum++;
		
		checksum = new;
	}
	checksum = -checksum;
	printf("%08x\n",checksum);
	block[0] = ntohl(checksum);
}

// Patch the FFS Bitmap to show the blocks occupied by LIDE as being used
int main () {
	FILE *fh = fopen("aide-boot.adf","rb+");

	if (!fh) {
		printf("Error opening file\n");
		return 1;
	}

	if (fseek(fh,bitmap_pos,SEEK_SET))
			return 1;
	
	uint32_t *bitmap = malloc(512);
	if (!bitmap) {
		printf("Failed to allocate memory.\n");
		return 1;
	}

	fread(bitmap,1,512,fh);

	for (int i=0; i<=64/32; i++) {
		bitmap[i] = 0;
	}
	checksumBlock(bitmap,512/4);
	fseek(fh,bitmap_pos,SEEK_SET);
	fwrite(bitmap,1,512,fh);

	if (fh) fclose(fh);
	if (bitmap) free(bitmap);
}
