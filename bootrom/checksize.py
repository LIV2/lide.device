#!/usr/bin/env python3
import sys
import io
import argparse
import shutil
import subprocess
import struct

# First 4096 bytes are DiagArea/BootRom + IDE Registers, minus another 4 for the ROM trailer
DEV_MAXSIZE = 28668
ZX0_HEADER = 0x5a583001
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
RESET='\033[0m'

def ok(message: str) -> None:
    print(f"{GREEN}{message}{RESET}")

def error(message: str) -> None:
    print(f"{RED}{message}{RESET}")

def warn(message: str) -> None:
    print(f"{YELLOW}{message}{RESET}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-i" ,"--infile", required=True)
    parser.add_argument("-o" ,"--outfile", required=True)
    parser.add_argument("-s" ,"--size", type=int, default=DEV_MAXSIZE)
    parser.add_argument("-f" ,"--force-compress", action='store_true', default=False)
    args = parser.parse_args()

    infile = args.infile
    outfile = args.outfile
    maxSize = args.size
    force   = args.force_compress

    with open(infile,"rb") as fh:
        fh.seek(0,io.SEEK_END)
        size = fh.tell()
        fh.seek(0,io.SEEK_SET)

    if not force:
        if size <= maxSize:
            shutil.copy2(infile,outfile)
            sys.exit(0)
        else:
            warn(f"{infile} larger than {maxSize} bytes, compressing...")

    if not shutil.which("salvador"):
        error("'salvador' not found in PATH")
        sys.exit(1)

    compressed = subprocess.run(["salvador", infile, '/dev/stdout'], stdout=subprocess.PIPE).stdout
    compressedSize = len(compressed)

    if compressedSize > maxSize:
        error(f"{infile} too big even after compression: {compressedSize} > {maxSize}")
        sys.exit(1)

    with open(outfile,"wb") as fh:
        fh.write(struct.pack(">III",ZX0_HEADER, size, compressedSize))
        fh.write(compressed)
