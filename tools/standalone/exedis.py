"""exedis.py - disassemble a function of the original exe (gta_sa_compact.exe) with call targets named from the hook registry of the sources.

usage: python -X utf8 -I tools/standalone/exedis.py ADDR [ADDR...] [--exe gta_sa_compact.exe] [--repo .] [--max 400]
A function ends at the first `ret` / out-of-range `jmp` after the highest forward jump target seen so far. Data refs into .rdata (floats / jump tables) are shown raw.
"""
import argparse
import re
import struct
from pathlib import Path

import capstone

ap = argparse.ArgumentParser()
ap.add_argument('addrs', nargs='+')
ap.add_argument('--exe', default='gta_sa_compact.exe')
ap.add_argument('--repo', default='.')
ap.add_argument('--max', type=int, default=400)
ap.add_argument('--vt', action='store_true', help='treat the addresses as vtables: list slot -> target (+ registry name)')
ap.add_argument('--callers', action='store_true', help='instead of disassembling: list call/jmp sites of the address(es) and 4-byte operands equal to it (vtable stores, pushes)')
a = ap.parse_args()
d = Path(a.exe).read_bytes()
pe = struct.unpack_from('<I', d, 0x3c)[0]
nsec = struct.unpack_from('<H', d, pe + 6)[0]
osz = struct.unpack_from('<H', d, pe + 20)[0]
base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
so = pe + 24 + osz
secs = []
for i in range(nsec):
    n, vs, va, rs, rp = struct.unpack_from('<8sIIII', d, so + 40 * i)
    secs.append((va + base, max(vs, rs), rp))


def rd(x, n):
    for va, sz, rp in secs:
        if va <= x < va + sz:
            return d[rp + x - va:rp + x - va + n]
    return b''


names = {}
rx = re.compile(r'RH_Scoped\w*Install\(\s*([\w:~]+)\s*,\s*(?:"[^"]*"\s*,\s*)?0x([0-9A-Fa-f]{6})')
for p in list((Path(a.repo) / 'source').rglob('*.cpp')) + list((Path(a.repo) / 'source').rglob('*.h')):
    cur = None
    for line in p.read_text(encoding='utf-8', errors='replace').splitlines():
        m = re.search(r'RH_Scoped(?:Virtual)?Class\(\s*([\w:]+)', line)
        if m:
            cur = m.group(1)
            continue
        m = rx.search(line)
        if m:
            names.setdefault(int(m.group(2), 16), (cur or '') + '::' + m.group(1))
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = False
if a.vt:
    for s in a.addrs:
        v = int(s, 16)
        print('; ==== vtable %#x' % v)
        for i in range(400):
            b = rd(v + 4 * i, 4)
            t = struct.unpack('<I', b)[0] if len(b) == 4 else 0
            if not (secs[0][0] <= t < secs[0][0] + secs[0][1]):
                break
            print('  slot %-3d %#x  %s' % (i, t, names.get(t, '')))
    raise SystemExit
if a.callers:
    tva, tsz, trp = secs[0]
    txt = d[trp:trp + tsz]
    reg_sorted = sorted(names)
    import bisect
    def owner(x):
        k = bisect.bisect_right(reg_sorted, x) - 1
        return '%#x %s' % (reg_sorted[k], names[reg_sorted[k]]) if k >= 0 and x - reg_sorted[k] < 0x800 else ''
    for s in a.addrs:
        t = int(s, 16)
        print('; ==== references to %#x %s' % (t, names.get(t, '')))
        for m in re.finditer(b'[' + bytes([0xe8, 0xe9]) + b']', txt):
            i = m.start()
            if i + 5 <= len(txt):
                rel = struct.unpack_from('<i', txt, i + 1)[0]
                if (tva + i + 5 + rel) & 0xffffffff == t:
                    print('  %s %#x   (in %s)' % ('call' if txt[i] == 0xe8 else 'jmp ', tva + i, owner(tva + i)))
        for m in re.finditer(re.escape(struct.pack('<I', t)), txt):
            print('  operand %#x   (in %s)' % (tva + m.start(), owner(tva + m.start())))
    raise SystemExit
for s in a.addrs:
    start = int(s, 16)
    print('; ==== %#x %s' % (start, names.get(start, '')))
    pc = start
    far = start
    cnt = 0
    while cnt < a.max:
        ins = list(md.disasm(rd(pc, 16), pc, 1))
        if not ins:
            print('%08x  ??' % pc)
            break
        i = ins[0]
        op = i.op_str
        extra = ''
        if i.mnemonic in ('call', 'jmp') and re.match(r'^0x[0-9a-f]+$', op):
            t = int(op, 16)
            if t in names:
                extra = '   ; ' + names[t]
        print('%08x  %-6s %s%s' % (pc, i.mnemonic, op, extra))
        if i.mnemonic.startswith('j') and i.mnemonic != 'jmp' or i.mnemonic == 'jmp':
            m = re.match(r'^0x([0-9a-f]+)$', op)
            if m:
                t = int(m.group(1), 16)
                if start <= t < start + 0x4000 and t > far:
                    far = t
        pc += i.size
        cnt += 1
        if i.mnemonic == 'ret' and pc > far:
            break
        if i.mnemonic == 'jmp' and pc > far and re.match(r'^0x', op):
            break
