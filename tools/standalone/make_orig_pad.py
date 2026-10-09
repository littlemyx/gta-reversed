#!/usr/bin/env python3
"""Write the x86 COFF object with the original-image placeholder (S2 of .notes/P2A_REVIEW.md).

Usage: python3 -I make_orig_pad.py <out.obj> [size = 0x8B0000]

One section named exactly `.text` (CODE|EXECUTE|READ, 16 byte aligned) with `size` zero bytes and the external symbol `_notsa_orig_pad` at its start.
It is linked FIRST (first object on the link line): link.exe lays the plain `.text` sections out in object order before `.text$*`, so
`_notsa_orig_pad` ends up at the very start of .text == 0x401000 (the original code start). That makes the placeholder cover the original
data range [0x858000, 0xCB0000) inside the main image (only the main image is mapped before the process heap / NLS sections), and the whole
original code range [0x401000, 0x858000) then contains NO code of ours: DataImage makes it PAGE_NOACCESS, so a raw call to an original
address faults with that address. DataImage::Load fails loudly if the symbol is not at the code start.
Why not ml/cl: both always emit `.text$mn` (or a data-flagged section), which link.exe orders after other code; the name must be plain `.text`.
"""
import struct
import sys

size = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x8B0000
out = sys.argv[1]

IMAGE_FILE_MACHINE_I386 = 0x14C
SCN_CODE_X_R_ALIGN16 = 0x00000020 | 0x20000000 | 0x40000000 | 0x00500000  # CNT_CODE | MEM_EXECUTE | MEM_READ | ALIGN_16BYTES

name = b"_notsa_orig_pad"
strtab = struct.pack("<I", 4 + len(name) + 1) + name + b"\0"
sec_hdr_off = 20
raw_off = sec_hdr_off + 40
sym_off = raw_off + size
symbols = b""
# @feat.00 (value 1: SafeSEH-compatible object), absolute, static
symbols += b"@feat.00" + struct.pack("<IhHBB", 1, -1, 0, 3, 0)
# _notsa_orig_pad: long name -> string table offset 4
symbols += struct.pack("<II", 0, 4) + struct.pack("<IhHBB", 0, 1, 0, 2, 0)
hdr = struct.pack("<HHIIIHH", IMAGE_FILE_MACHINE_I386, 1, 0, sym_off, 2, 0, 0)
sec = b".text\0\0\0" + struct.pack("<IIIIIIHHI", 0, 0, size, raw_off, 0, 0, 0, 0, SCN_CODE_X_R_ALIGN16)
with open(out, "wb") as f:
    f.write(hdr + sec)
    f.write(bytes(size))
    f.write(symbols + strtab)
