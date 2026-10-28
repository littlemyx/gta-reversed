"""litdiv_scan.py - find port sites `x / <float literal>` (and `/=`) in hooked functions whose exe counterpart multiplies by a float constant.

MSVC never turns a division into a multiplication, so when the exe has `fmul [~1/d]` and no `fdiv [d]` the original source multiplied by a literal and the
port's division can differ in the last bit. usage:
  py -X utf8 -I tools/standalone/litdiv_scan.py [--exe gta_sa_compact.exe] [--repo .] [--out .notes/asm_sweep/LITDIV.tsv]
(.notes/asm_sweep/LITDIV.tsv has a hand-added last column `status` = FIXED <commit> / ok / open.)
Verdicts: EXE-MULTIPLIES (exe fmul/mulss by ~1/d, no fdiv by d in the function), EXE-DIVIDES (fdiv/fdivr/divss by d, no fmul by 1/d), UNCLEAR (neither,
both, only an fld of the constant, or overloaded name). Column `exe_const` = the matched (or nearest) exe constant as float32 hex.
"""
import argparse
import math
import re
import struct
from collections import Counter
from pathlib import Path

import capstone

ap = argparse.ArgumentParser()
ap.add_argument('--exe', default='gta_sa_compact.exe')
ap.add_argument('--repo', default='.')
ap.add_argument('--out', default='.notes/asm_sweep/LITDIV.tsv')
ap.add_argument('--maxins', type=int, default=8000)
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


md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
MEM = re.compile(r'^(?:xmm\d, )?(dword|qword) ptr \[0x([0-9a-f]+)\]$')
_cache = {}


def exe_consts(start):
    """-> (muls, divs, lds): lists of (value, float32 bits or None, size) for memory-constant operands in the function at start."""
    if start in _cache:
        return _cache[start]
    muls, divs, lds = [], [], []
    pc = start
    far = start
    cnt = 0
    while cnt < a.maxins:
        ins = list(md.disasm(rd(pc, 16), pc, 1))
        if not ins:
            break
        i = ins[0]
        mn, op = i.mnemonic, i.op_str
        m = MEM.match(op) if mn in ('fmul', 'fdiv', 'fdivr', 'fld', 'mulss', 'divss', 'mulsd', 'divsd') else None
        if m:
            sz = 4 if (m.group(1) == 'dword') else 8
            raw = rd(int(m.group(2), 16), sz)
            if len(raw) == sz:
                v = struct.unpack('<f' if sz == 4 else '<d', raw)[0]
                bits = struct.unpack('<I', raw)[0] if sz == 4 else None
                rec = (v, bits, sz)
                if mn in ('fmul', 'mulss', 'mulsd'):
                    muls.append(rec)
                elif mn == 'fld':
                    lds.append(rec)
                else:
                    divs.append(rec)
        if mn.startswith('j'):
            mm = re.match(r'^0x([0-9a-f]+)$', op)
            if mm:
                t = int(mm.group(1), 16)
                if start <= t < start + 0x6000 and t > far:
                    far = t
        pc += i.size
        cnt += 1
        if mn == 'ret' and pc > far:
            break
        if mn == 'jmp' and pc > far and op.startswith('0x'):
            break
    _cache[start] = (muls, divs, lds)
    return _cache[start]


def f32(x):
    try:
        return struct.unpack('<f', struct.pack('<f', x))[0]
    except (OverflowError, struct.error):
        return float('inf')


def bits32(x):
    return struct.unpack('<I', struct.pack('<f', f32(x)))[0]


def close_mul(c, dv):
    """exe constant c ~ 1/dv: 'exact' (== float32 of 1/dv), 'ulp' (<4e-7 rel), 'near' (<1e-3), or None."""
    inv = 1.0 / dv
    if c == 0 or c != c or c in (float('inf'), float('-inf')):
        return None
    if bits32(c) == bits32(inv):
        return 'exact'
    rel = abs(c - inv) / abs(inv)
    if rel < 4e-7:
        return 'ulp'
    if rel < 1e-3:
        return 'near'
    return None


def close_div(c, dv):
    if c == 0 or c != c:
        return None
    return 'exact' if abs(c - dv) / abs(dv) < 4e-7 else None


# ---------------- hooks ----------------
rx_hook = re.compile(r'RH_Scoped\w*Install\w*\(\s*([\w:~]+)\s*,\s*(?:"[^"]*"\s*,\s*)?0x([0-9A-Fa-f]{6})')
rx_cls = re.compile(r'RH_Scoped(?:Virtual)?Class\(\s*([\w:]+)')
rx_ns = re.compile(r'RH_ScopedNamespace\(\s*([\w:]+)')
rx_def = re.compile(r'^(?:[\w:<>\*&,~]+\s+)*?(?:(\w+)::)?(~?\w+)\s*\(')
rx_div = re.compile(r'(?<![/*])/(=?)\s*\(?\s*([0-9]*\.[0-9]*(?:[eE][+-]?\d+)?[fF]?|[0-9]+[eE][+-]?\d+[fF]?|[0-9]+[fF])(?![\w.])')


def strip_line(line, st):
    out = []
    i = 0
    n = len(line)
    while i < n:
        if st['blk']:
            j = line.find('*/', i)
            if j < 0:
                return ''.join(out)
            st['blk'] = False
            i = j + 2
            continue
        c = line[i]
        if line.startswith('//', i):
            break
        if line.startswith('/*', i):
            st['blk'] = True
            i += 2
            continue
        if c in '"\'':
            j = i + 1
            while j < n and line[j] != c:
                j += 2 if line[j] == '\\' else 1
            out.append(c + c)
            i = j + 1
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def parse_lit(s):
    try:
        return float(s.rstrip('fF'))
    except ValueError:
        return None


def is_pow2(x):
    if x <= 0:
        return False
    m, e = math.frexp(x)
    return m == 0.5


rows = []
repo = Path(a.repo)
src = repo / 'source' / 'game_sa'
stats = {'files': 0, 'funcs': 0}
for p in sorted(src.rglob('*.cpp')):
    text = p.read_text(encoding='utf-8', errors='replace').splitlines()
    rel = p.relative_to(repo).as_posix()
    hooks = {}
    cur = ''
    for line in text:
        m = rx_cls.search(line) or rx_ns.search(line)
        if m:
            cur = m.group(1)
            continue
        if re.search(r'::InjectHooks\s*\(', line):
            cur = ''
        m = rx_hook.search(line)
        if m:
            hooks.setdefault((cur, m.group(1)), []).append(int(m.group(2), 16))
    if not hooks:
        continue
    stats['files'] += 1
    st = {'blk': False}
    infn = None
    for ln, raw in enumerate(text, 1):
        code = strip_line(raw, st)
        if infn is None:
            if raw and not raw[0].isspace() and raw[0] not in '#}{/' and '(' in code and not code.rstrip().endswith(';'):
                m = rx_def.match(code)
                if m and m.group(2) not in ('if', 'for', 'while', 'switch', 'return', 'static_assert', 'namespace', 'class', 'struct'):
                    cls, name = m.group(1) or '', m.group(2)
                    addrs = hooks.get((cls, name))
                    if addrs is None:
                        for (hc, hn), av in hooks.items():
                            if hn == name and hc.split('::')[-1] == cls:
                                addrs = av
                                break
                    infn = {'cls': cls, 'name': name, 'addrs': addrs}
                    if addrs:
                        stats['funcs'] += 1
        if infn is not None and infn['addrs']:
            for mm in rx_div.finditer(code):
                v = parse_lit(mm.group(2))
                if v is None or v == 0 or is_pow2(abs(v)):
                    continue
                if re.search(r'(?:^|[=(,{?:]|return|constexpr)\s*(?:[0-9]+\.?[0-9]*|\.[0-9]+)(?:[eE][+-]?\d+)?[fF]?\s*$', code[:mm.start()]):
                    continue    # literal / literal: folded by the compiler (the constexpr reciprocal idiom)
                rows.append((rel, ln, (infn['cls'] + '::' if infn['cls'] else '') + infn['name'], infn['addrs'], v, mm.group(0).strip()))
        if infn is not None and raw.startswith('}'):
            infn = None

# ---------------- classify ----------------
out_rows = []
cnt = {}
fn_div = Counter()
for r in rows:
    fn_div[(r[0], r[2], f32(r[4]))] += 1
for rel, ln, fn, addrs, v, txt in rows:
    ms, ds, ls = [], [], []
    for ad in addrs:
        m_, d_, l_ = exe_consts(ad)
        ms += m_
        ds += d_
        ls += l_
    mul_hits = [(c, b, close_mul(c, v)) for c, b, sz in ms if close_mul(c, v) in ('exact', 'ulp')]
    near_hits = [(c, b) for c, b, sz in ms if close_mul(c, v) == 'near']
    div_hits = [(c, b) for c, b, sz in ds if close_div(c, v)]
    ld_hits = [(c, b) for c, b, sz in ls if close_mul(c, v) in ('exact', 'ulp')]
    ldd_hits = [(c, b) for c, b, sz in ls if close_div(c, v)]
    note = 'overloaded/multiple addrs ' + ','.join('%#x' % x for x in addrs) if len(addrs) > 1 else ''
    const = ''

    def hx(b, c):
        return ('0x%08x' % b) if b is not None else 'dbl:%r' % c

    if mul_hits and not div_hits:
        verdict = 'EXE-MULTIPLIES' if len(addrs) == 1 else 'UNCLEAR'
        c, b, q = mul_hits[0]
        const = hx(b, c)
        note += ' ' + ('const=f32(1/d)' if q == 'exact' else 'const within 4e-7 of 1/d (%r)' % c)
    elif div_hits and not mul_hits:
        verdict = 'EXE-DIVIDES'
        const = hx(div_hits[0][1], div_hits[0][0])
    elif mul_hits and div_hits:
        verdict = 'UNCLEAR'
        note += ' exe has both fmul 1/d and fdiv d'
        const = hx(mul_hits[0][1], mul_hits[0][0])
    else:
        verdict = 'UNCLEAR'
        if ld_hits:
            note += ' only fld of 1/d const'
            const = hx(ld_hits[0][1], ld_hits[0][0])
        elif ldd_hits:
            note += ' only fld of d const'
            const = hx(ldd_hits[0][1], ldd_hits[0][0])
        elif near_hits:
            note += ' nearest fmul const %r (1/d=%r)' % (near_hits[0][0], 1.0 / v)
            const = hx(near_hits[0][1], near_hits[0][0])
        else:
            note += ' no matching exe constant (folded / inlined callee / int-typed / var divisor)'
    note = note.strip() + ' | exe fmul(1/d)=%d fdiv(d)=%d port_sites_same_d=%d' % (len(mul_hits), len(div_hits), fn_div[(rel, fn, f32(v))])
    cnt[verdict] = cnt.get(verdict, 0) + 1
    out_rows.append((rel, ln, fn, '0x%06X' % addrs[0], repr(v) + ('f' if txt.lower().rstrip('=').endswith('f') else ''), verdict, const, note))

order = {'EXE-MULTIPLIES': 0, 'EXE-DIVIDES': 1, 'UNCLEAR': 2}
out_rows.sort(key=lambda r: (order[r[5]], r[0], r[1]))
out = Path(a.out)
out.parent.mkdir(parents=True, exist_ok=True)
with out.open('w', encoding='utf-8', newline='\n') as f:
    f.write('file\tline\tfunction\taddr\tdivisor\tverdict\texe_const\tnote\n')
    for r in out_rows:
        f.write('\t'.join(str(x) for x in r) + '\n')
print('files with hooks: %(files)d, hooked functions with a body: %(funcs)d, sites: ' % stats + str(len(out_rows)))
for k in order:
    print('%-15s %d' % (k, cnt.get(k, 0)))
