#!/usr/bin/env python3
import argparse
import os
import struct
# Patch the disk by placing the driver at block 2 where the bootblock will be looking for it
# Also mark the blocks used in the bitmap while we're at it

BITMAP_POS = 881 * 512
DEVICE_START_BLOCK = 2

def markBitmapUsed(bitmap: bytearray, start_block: int, num_blocks: int) -> None:
    words = list(struct.unpack(">128I", bitmap))
    for block in range(start_block, start_block + num_blocks):
        bit = block - 2
        word_index = 1 + bit // 32
        bit_index = bit % 32
        words[word_index] &= ~(1 << bit_index) & 0xFFFFFFFF
    struct.pack_into(">128I", bitmap, 0, *words)

def checksumBlock(block: bytearray) -> bytearray:
    checksum = 0
    block[0:4] = b"\x00\x00\x00\x00"

    for long in struct.iter_unpack(">I",block):
        new = (checksum + long[0]) & 0xFFFFFFFF
        checksum = new

    checksum = (-checksum) & 0xFFFFFFFF

    struct.pack_into(">I", block, 0, checksum)
    return block

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--device",required=True)
    parser.add_argument("--adf",required=True)
    args = parser.parse_args()

    with open(args.device,"rb") as fh:
        device = fh.read()

    num_blocks = (len(device) + 511) // 512

    with open(args.adf,"rb+") as fh:
        fh.seek(DEVICE_START_BLOCK*512,os.SEEK_SET)
        fh.write(device)

        fh.seek(BITMAP_POS,os.SEEK_SET)
        bitmap = bytearray(fh.read(512))
        markBitmapUsed(bitmap, DEVICE_START_BLOCK, num_blocks)
        bitmap = checksumBlock(bitmap)
        fh.seek(BITMAP_POS,os.SEEK_SET)
        fh.write(bitmap)