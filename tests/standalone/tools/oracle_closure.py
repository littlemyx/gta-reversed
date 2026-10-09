#!/usr/bin/env python3
"""Static "is this exe function oracle-able?" analysis for tests/standalone/game_oracle_test.cpp (P2E).

The game oracle maps the exe's .text/.rdata/.data at their ORIGINAL addresses inside the test process (the test is linked with the 9 MB
orig_image_pad.obj placeholder, /BASE:0x400000), so the exe's own machine code runs unrelocated; no bytes of the exe are stored in the repo.
That only works for code that is self-contained. This tool follows the call graph from each root (E8/E9 direct targets, recursively, depth/size
capped), and reports per root:
  * the closure (function entry, size)
  * IMPORT calls  (call/jmp [IAT 0x458000..0x458378) -> kernel32/d3d/etc. are NOT mapped in the test)         => not oracle-able
  * INDIRECT calls through registers / vtables                                                             => not oracle-able (unless whitelisted)
  * global data touched (.data/.bss 0x8A4000..0xC9E000 outside the allow list), with r/w guess             => listed; not oracle-able unless allowed
  * callees that look like CRT (flagged by --crt name list or by touching imports)
usage: python3 -I oracle_closure.py <gta_sa_compact.exe> [--allow 0xADDR[-0xADDR]]... [--depth N] [--maxsize N] <root addr>...
(run with a python that has capstone; exit code 0 always, the verdict is printed)
"""
import sys, struct
from capstone import *
from capstone.x86 import *

args = sys.argv[1:]
exe = open(args[0], 'rb').read(); args = args[1:]
allow = []; depth_cap = 6; size_cap = 0x3000; roots = []
i = 0
while i < len(args):
    a = args[i]
    if a == '--allow':
        s = args[i + 1].split('-'); lo = int(s[0], 0); hi = int(s[1], 0) if len(s) > 1 else lo + 4
        allow.append((lo, hi)); i += 2
    elif a == '--depth': depth_cap = int(args[i + 1], 0); i += 2
    elif a == '--maxsize': size_cap = int(args[i + 1], 0); i += 2
    else: roots.append(int(a, 0)); i += 1

TEXT = (0x401000, 0x858000); RDATA = (0x858000, 0x8A4000); DATA = (0x8A4000, 0xCB0000); IAT = (0x858000, 0x858378)
def raw(va): return va - 0x401000 + 0x400
md = Cs(CS_ARCH_X86, CS_MODE_32); md.detail = True
def allowed(a): return any(lo <= a < hi for lo, hi in allow)

def scan(entry):
    """recursive descent inside one function; returns dict(size, calls, imports, indirect, globals{addr:set('r','w')}, jtab)"""
    seen = {}; work = [entry]; res = dict(calls=set(), imports=set(), indirect=[], globals={}, size=0, lo=entry, hi=entry)
    while work:
        a = work.pop()
        while a not in seen and entry <= a < entry + size_cap and TEXT[0] <= a < TEXT[1]:
            ins = next(md.disasm(exe[raw(a):raw(a) + 16], a), None)
            if ins is None: break
            seen[a] = ins; res['hi'] = max(res['hi'], a + ins.size)
            m = ins.mnemonic
            for op in ins.operands:
                if op.type == X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0 or (op.type == X86_OP_MEM and (op.mem.disp & 0xFFFFFFFF) >= RDATA[0] and op.mem.base == 0):
                    d = op.mem.disp & 0xFFFFFFFF
                    if m in ('call', 'jmp') and IAT[0] <= d < IAT[1]: res['imports'].add(d)
                    elif DATA[0] <= d < DATA[1] and not allowed(d):
                        wr = op.access & CS_AC_WRITE != 0
                        res['globals'].setdefault(d, set()).add('w' if wr else 'r')
                elif op.type == X86_OP_MEM and op.mem.index != 0 and (op.mem.disp & 0xFFFFFFFF) >= DATA[0]:
                    d = op.mem.disp & 0xFFFFFFFF   # indexed global array (or jump table in .text for jmp)
                    if DATA[0] <= d < DATA[1] and not allowed(d):
                        res['globals'].setdefault(d, set()).add('w' if op.access & CS_AC_WRITE else 'r')
            if m == 'call':
                op = ins.operands[0]
                if op.type == X86_OP_IMM: res['calls'].add(op.imm & 0xFFFFFFFF)
                elif op.type == X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0 and IAT[0] <= (op.mem.disp & 0xFFFFFFFF) < IAT[1]: pass
                else: res['indirect'].append(a)
            if m == 'jmp':
                op = ins.operands[0]
                if op.type == X86_OP_IMM:
                    t = op.imm & 0xFFFFFFFF
                    if entry <= t < entry + size_cap: work.append(t)
                    else: res['calls'].add(t)        # tail jump
                elif op.type == X86_OP_MEM and op.mem.index != 0 and op.mem.base == 0:   # jump table
                    t0 = op.mem.disp & 0xFFFFFFFF; k = 0
                    while True:
                        v = struct.unpack_from('<I', exe, raw(t0) + 4 * k)[0] if TEXT[0] <= t0 < TEXT[1] else struct.unpack_from('<I', exe, 0x456800 + t0 - 0x858000 + 4 * k)[0]
                        if not (entry <= v < entry + size_cap): break
                        work.append(v); k += 1
                        if k > 256: break
                elif op.type == X86_OP_MEM and IAT[0] <= (op.mem.disp & 0xFFFFFFFF) < IAT[1]: res['imports'].add(op.mem.disp & 0xFFFFFFFF)
                else: res['indirect'].append(a)
                break
            if m in ('ret',): break
            if m.startswith('j') and ins.operands[0].type == X86_OP_IMM:
                t = ins.operands[0].imm & 0xFFFFFFFF
                if entry <= t < entry + size_cap: work.append(t)
                else: res['calls'].add(t)
            a += ins.size
    res['size'] = res['hi'] - entry
    return res

names = {}
try:
    for ln in open(__file__.rsplit('/tests/standalone', 1)[0] + '/.notes/symbols.txt'):
        p = ln.split(None, 1)
        if len(p) == 2: names[int(p[0], 16)] = p[1].strip()
except Exception: pass
def nm(a): return names.get(a, '')

cache = {}
def closure(root):
    out = {}; work = [(root, 0)]
    while work:
        f, d = work.pop()
        if f in out: continue
        if f not in cache: cache[f] = scan(f)
        out[f] = (cache[f], d)
        if d < depth_cap:
            for c in cache[f]['calls']:
                if TEXT[0] <= c < TEXT[1] and c not in out: work.append((c, d + 1))
    return out

for r in roots:
    cl = closure(r)
    tot = sum(s['size'] for s, d in cl.values())
    bad = {}
    for f, (s, d) in cl.items():
        if s['imports'] or s['indirect'] or s['globals']: bad[f] = s
    print("0x%X %s: %s  (%d fns, %d bytes)" % (r, nm(r), "ORACLE-ABLE" if not bad else "NOT ORACLE-ABLE as is", len(cl), tot))
    if len(cl) > 1: print("    callees: " + ", ".join("0x%X%s" % (f, ('(' + nm(f) + ')') if nm(f) else '') for f in cl if f != r))
    for f, s in sorted(bad.items()):
        print("    in 0x%X%s: %s%s%s" % (f, ('(' + nm(f) + ')') if nm(f) else '',
              ("IMPORTS " + ",".join("0x%X" % i for i in sorted(s['imports'])) + " ") if s['imports'] else '',
              ("INDIRECT x%d " % len(s['indirect'])) if s['indirect'] else '',
              ("GLOBALS " + ",".join("0x%X%s" % (g, ''.join(sorted(rw))) for g, rw in sorted(s['globals'].items()))) if s['globals'] else ''))
