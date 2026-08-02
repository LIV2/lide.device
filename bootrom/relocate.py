#!/usr/bin/env python3
from amitools.binfmt.Relocate import Relocate
from amitools.binfmt.BinFmt import BinFmt
import argparse

"""
Take a hunk and create a relocated binary
This allows us to build a kickstart module for amigapci-lide.device
"""

parser = argparse.ArgumentParser()
parser.add_argument("base",help="Base address for relocation")
parser.add_argument("infile",help="Input file")
parser.add_argument("outfile",help="Output file")
args = parser.parse_args()

base = int(args.base,16)

bf = BinFmt()

bin_img = bf.load_image(path=args.infile)
if bin_img:
    reloc = Relocate(bin_img=bin_img)
    data = reloc.relocate_one_block(base_addr=base)
    with open(args.outfile,"wb") as f:
        f.write(data)
