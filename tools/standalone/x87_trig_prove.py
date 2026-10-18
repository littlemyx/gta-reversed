"""X87-TRIG: prove with unicorn what the exe's CRT math entry points compute (evaluates the original code of gta_sa_compact.exe, FPU control word 0x27F = PC24 like the game).

Entries found by scanning every call/jmp target of the exe whose code (<= 3 levels deep) contains x87 transcendental instructions:
  0x821E70 _CIasin   0x822380 _CIacos   0x822394 / 0x82239D acos (cdecl double / body)   0x822130 _CIpow   0x8220F0 pow (cdecl)
(sin / cos / tan / atan2 / sqrt / log are INLINE x87 in the game code: there is no CRT call to prove.)
usage: py -3 -X utf8 -I tools/standalone/x87_trig_prove.py [--exe gta_sa_compact.exe]
"""
import argparse
import math
import struct
import sys

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW

ap = argparse.ArgumentParser()
ap.add_argument('--exe', default='gta_sa_compact.exe')
args = ap.parse_args()
BASE, END = 0x400000, 0xcb1000
STACK, BUF = 0x2000000, 0x7000000
d = open(args.exe, 'rb').read()
# (va, raw offset, size) of .text, _rwcseg, .rdata, .data, _TEXT_HA
secs = [(0x401000, 0x400, 0x456000), (0x857000, 0x456200, 0x600), (0x858000, 0x456800, 0x4B400), (0x8A4000, 0x4A1C00, 0x40000), (0xc9e000, 0x4e1c00, 0x10c00)]
img = bytearray(END - BASE)
for va, raw, sz in secs:
    img[va - BASE:va - BASE + sz] = d[raw:raw + sz]


def run(fn, st=(), stack=()):
    """st: doubles pushed on the x87 stack in order (the last one is ST0); stack: doubles pushed as cdecl arguments (first = leftmost). Returns ST0 as a double."""
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(BASE, END - BASE)
    uc.mem_write(BASE, bytes(img))
    uc.mem_map(STACK, 0x100000)
    uc.mem_map(BUF, 0x10000)
    uc.reg_write(UC_X86_REG_FPCW, 0x027f)
    code = b''
    for i, a in enumerate(st):
        uc.mem_write(BUF + 8 * i, struct.pack('<d', a))
        code += b'\xdd\x05' + struct.pack('<I', BUF + 8 * i)                 # fld qword [..]
    for a in reversed(stack):
        lo, hi = struct.unpack('<II', struct.pack('<d', a))
        code += b'\x68' + struct.pack('<I', hi) + b'\x68' + struct.pack('<I', lo)   # push hi; push lo
    code_at = BUF + 0x1000
    code += b'\xe8' + struct.pack('<I', (fn - (code_at + len(code) + 5)) & 0xFFFFFFFF)
    code += b'\xdd\x1d' + struct.pack('<I', BUF + 0x200) + b'\xf4'          # fstp qword [BUF+0x200]; hlt
    uc.mem_write(code_at, code)
    uc.reg_write(UC_X86_REG_ESP, STACK + 0x80000)
    try:
        uc.emu_start(code_at, code_at + len(code) - 1, count=200000)
    except Exception as e:
        return float('nan') if False else 'exc %s eip=%x' % (e, uc.reg_read(UC_X86_REG_EIP))
    return struct.unpack('<d', uc.mem_read(BUF + 0x200, 8))[0]


CASES = [
    # name, address, call kind, reference function, input tuples
    ('_CIasin', 0x821E70, 'st', lambda x: math.asin(x), [(0.0,), (0.5,), (-0.5,), (0.9,), (1.0,), (-1.0,)]),
    ('_CIacos', 0x822380, 'st', lambda x: math.acos(x), [(0.0,), (0.5,), (-0.5,), (0.9,), (1.0,), (-1.0,)]),
    ('acos (cdecl double, 0x822394 entry)', 0x822394, 'stack', lambda x: math.acos(x), [(0.0,), (0.5,), (-0.5,), (0.9,)]),
    ('_CIpow (x = ST1, y = ST0)', 0x822130, 'st', lambda x, y: math.pow(x, y), [(2.0, 3.0), (0.5, 2.0), (1.5, 0.5), (10.0, 4.0)]),
    ('pow (cdecl double, double)', 0x8220F0, 'stack', lambda x, y: math.pow(x, y), [(2.0, 3.0), (0.5, 2.0), (1.5, 0.5), (10.0, 4.0)]),
]
bad = 0
print('%-38s %-9s %-26s %-22s %-22s %s' % ('entry', 'address', 'input', 'exe result', 'reference', 'verdict'))
for name, addr, kind, ref, ins in CASES:
    for a in ins:
        r = run(addr, st=a) if kind == 'st' else run(addr, stack=a)
        e = ref(*a)
        ok = isinstance(r, float) and abs(r - e) <= 1e-5 * max(1.0, abs(e))      # PC24: ~1e-7 relative
        bad += not ok
        print('%-38s %#-9x %-26s %-22s %-22s %s' % (name, addr, a, r, e, 'OK' if ok else 'MISMATCH'))
print('mismatches:', bad)
sys.exit(1 if bad else 0)
