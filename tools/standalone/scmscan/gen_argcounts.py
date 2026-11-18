#!/usr/bin/env python3
"""gen_argcounts.py: per-opcode SCM argument counts, 0x0000..0x0A4E, from the 1.0 exe (gta_sa_compact.exe) group handlers
(table 0x8A6168, 27 x ProcessCommandsNNN..) cross-checked with a Sanny Builder sa.json (num_params).
exe count of a case = sum of CollectParameters(n) immediates (+1 per ReadTextLabelFromScript / GetPointerToScriptVariable /
ReadArrayInformation call) along the straight/branching path of the case; ReadParametersForNewlyStartedScript = variadic.
Output: argcounts.tsv (opcode<TAB>count<TAB>source exe|db|both|exe!=db ...). Counts: -1 = variadic (type-0 terminated),
-2 = one fixed arg then variadic (START_NEW_SCRIPT, START_NEW_STREAMED_SCRIPT).
usage: gen_argcounts.py --exe gta_sa_compact.exe --db sa.json --out argcounts.tsv --dump exe_counts.json"""
import struct, json, sys, argparse, collections
import capstone

ap = argparse.ArgumentParser()
ap.add_argument('--exe', default='C:/Users/littl/github/gta-reversed/gta_sa_compact.exe')
ap.add_argument('--db', default='C:/gta-scratch/t3/sa.json')
ap.add_argument('--dump', default='C:/gta-scratch/t3/exe_counts.json')
a = ap.parse_args()
d = open(a.exe, 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
nsec = struct.unpack_from('<H', d, pe + 6)[0]; osz = struct.unpack_from('<H', d, pe + 20)[0]
base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
secs = []
for i in range(nsec):
    n, vs, va, rs, rp = struct.unpack_from('<8sIIII', d, pe + 24 + osz + 40 * i)
    secs.append((va + base, max(vs, rs), rp))


def rd(x, n):
    for va, sz, rp in secs:
        if va <= x < va + sz: return d[rp + x - va:rp + x - va + n]
    return b''


COLLECT, PEEK = 0x464080, 0x464250
ONE = {0x463D50: 'text', 0x464790: 'var', 0x463CF0: 'arr'}  # +1 arg each
VAR = 0x464500
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = True


def ins_at(x):
    for i in md.disasm(rd(x, 16), x): return i
    return None


def case_counts(start, lo, hi):
    """DFS over the case body; returns set of totals (or 'V' for variadic) over distinct paths to ret."""
    res = set(); seen = set(); calls = set(); ni = 0
    stack = [(start, 0, None, False)]
    steps = 0
    while stack:
        p, tot, lastpush, var = stack.pop()
        while True:
            steps += 1
            if steps > 4000: res.add('?loop'); break
            key = (p, tot)
            if key in seen: break
            seen.add(key)
            i = ins_at(p)
            if i is None: res.add('?dec'); break
            m = i.mnemonic; ni += 1
            if m == 'push' and i.operands[0].type == capstone.x86.X86_OP_IMM:
                lastpush = i.operands[0].imm
            elif m == 'call' and i.operands[0].type == capstone.x86.X86_OP_IMM:
                t = i.operands[0].imm
                if t == COLLECT:
                    if lastpush is None: res.add('?push'); break
                    tot += lastpush
                elif t in ONE: tot += 1
                elif t == VAR: var = True
                if t not in (COLLECT, PEEK, 0x4859D0): calls.add(t)  # 0x4859D0 = UpdateCompareFlag
            elif m == 'ret': res.add('V%d' % tot if var else tot); break
            elif m == 'jmp':
                if i.operands[0].type == capstone.x86.X86_OP_IMM:
                    t = i.operands[0].imm
                    if lo <= t < hi + 0x2000: p = t; continue
                    res.add('?jmpout'); break  # tail jump out of the group function
                else: res.add('?indirect'); break
            elif m.startswith('j') and i.operands[0].type == capstone.x86.X86_OP_IMM:
                stack.append((i.operands[0].imm, tot, lastpush, var))
            p += i.size
    return res, sorted(calls), ni


def group(fn, g):
    """parse switch: returns ({opcode: target}, default)"""
    p = fn; sub = 0; maxv = None; idx = None; tbl = None; dflt = None; seen = False
    for k in range(40):
        i = ins_at(p)
        m, o = i.mnemonic, i.operands
        if not seen and m in ('add', 'sub') and o[0].type == capstone.x86.X86_OP_REG and o[1].type == capstone.x86.X86_OP_IMM                 and i.reg_name(o[0].reg) in ('eax', 'ecx'):
            v = o[1].imm & 0xffffffff
            sub = v if m == 'sub' else (-v) & 0xffffffff; seen = True  # case index = opcode - sub
        elif not seen and m == 'lea' and o[1].type == capstone.x86.X86_OP_MEM and i.reg_name(o[1].mem.base) in ('eax', 'ecx', 'edx', 'ebx', 'esi', 'edi', 'ebp') and not o[1].mem.index:
            sub = (-o[1].mem.disp) & 0xffffffff; seen = True
        elif m == 'cmp' and maxv is None and o[1].type == capstone.x86.X86_OP_IMM: maxv = o[1].imm
        elif m == 'ja' and dflt is None: dflt = o[0].imm
        elif m == 'movzx' and o[1].type == capstone.x86.X86_OP_MEM and o[1].size == 1: idx = o[1].mem.disp
        elif m == 'jmp' and o[0].type == capstone.x86.X86_OP_MEM and o[0].mem.scale == 4:
            tbl = o[0].mem.disp; break
        p += i.size
    if tbl is None: return None
    out = {}
    for k in range(maxv + 1):
        j = rd(idx + k, 1)[0] if idx else k
        t = struct.unpack('<I', rd(tbl + 4 * j, 4))[0]
        if t != dflt: out[(sub + k) & 0xffff] = t
    return out, dflt


tab = struct.unpack('<27I', rd(0x8A6168, 27 * 4))
res = {}; shapes = {}; extra = {}
for g, fn in enumerate(tab):
    sw = group(fn, g)
    if sw is None: print('group', g, hex(fn), 'NO SWITCH', file=sys.stderr); continue
    sw, dflt = sw
    print('  ops', min(sw), max(sw), file=sys.stderr)
    for op, t in sw.items():
        if op > 0xA4E: continue
        r, cl, ni = case_counts(t, fn, fn + 0x6000)
        res[op] = sorted(map(str, r)); shapes[op] = t; extra[op] = (cl, ni)
    print('group', g, hex(fn), 'cases', len(sw), 'default', hex(dflt), file=sys.stderr)
json.dump({'%04X' % k: [v, hex(shapes[k]), [hex(c) for c in extra[k][0]], extra[k][1]] for k, v in res.items()}, open(a.dump, 'w'))
print(len(res), 'opcodes from exe', file=sys.stderr)

# ---- merge with the Sanny Builder DB -> argcounts.tsv
import os
j = json.load(open(a.db))
db = {}
for c in j['extensions'][0]['commands']:  # 'default' extension = the stock SA opcodes
    o = int(c['id'], 16)
    if o <= 0xA4E:
        n = c['num_params']
        if any(x['type'] == 'arguments' for x in c.get('input', [])): n = -(n - 1) - 1  # fixed args before the vararg tail, then -1-fixed
        db[o] = n
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'argcounts.tsv')
cnt = collections.Counter()
with open(out, 'w') as f:
    f.write('opcode\tcount\tsource\n')
    for o in range(0xA4F):
        ex = res.get(o)
        exn = None
        if ex and len(ex) == 1 and ex[0].isdigit(): exn = int(ex[0])
        elif ex and len(ex) == 1 and ex[0].startswith('V') and ex[0][1:].isdigit(): exn = -(int(ex[0][1:])) - 1
        dbn = db.get(o)
        if exn is not None and dbn is not None and exn == dbn: n, src = dbn, 'both'
        elif dbn is not None: n, src = dbn, 'db'  # exe handler delegates arg reading to helpers / not in a group table, or exe lower
        elif exn is not None: n, src = exn, 'exe'
        else: continue
        if o == 0x5B6: n, src = 0, 'exe'  # SAVE_STRING_TO_DEBUG_FILE: 128 RAW bytes after the opcode (exe case @0x48FAEC), no typed args
        cnt[src] += 1
        f.write('0x%04X\t%d\t%s\n' % (o, n, src))
print('argcounts.tsv', dict(cnt), 'no source:', [hex(o) for o in range(0xA4F) if o not in db and (o not in res or not res[o])], file=sys.stderr)
