"""dbllit_scan.py - find port comparisons (and std::min/max/clamp) against an unsuffixed decimal literal that is not exactly representable as a float.

`x < 0.12` is a double compare in the port; the exe usually compares against a float constant from .data (0.119999997f) which on x87 PC24 rounds differently at
the boundary. usage:
  py -X utf8 -I tools/standalone/dbllit_scan.py [--exe gta_sa_compact.exe] [--repo .] [--out .notes/asm_sweep/DBLLIT_scan.tsv]
Columns: file line function addr literal nearest_f32 kind hint exe_kind context. exe_kind (hooked functions only): EXE-FLOAT (a dword exe constant == f32(literal)
is read in the function), EXE-DOUBLE (a qword constant == literal), BOTH, UNCLEAR (no matching constant: folded / int operand / inlined callee), UNHOOKED (no hook
address known; triage by hand). `hint` = 1 when the other operand looks float-typed (member f*, .x/.y/.z, Magnitude, ...). Not a type checker: every hit
still needs a look at the exe compare.
"""
import argparse
import re
import struct
from pathlib import Path

import capstone

ap = argparse.ArgumentParser()
ap.add_argument('--exe', default='gta_sa_compact.exe')
ap.add_argument('--repo', default='.')
ap.add_argument('--out', default='.notes/asm_sweep/DBLLIT_scan.tsv')
ap.add_argument('--maxins', type=int, default=12000)
ap.add_argument('--suffixed', action='store_true', help='reverse check: f-suffixed literals in compares where the exe holds only a double constant')
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
MEM = re.compile(r'\b(dword|qword) ptr \[0x([0-9a-f]+)\]$')
FP = ('fld', 'fcom', 'fucom', 'fmul', 'fadd', 'fsub', 'fdiv', 'comis', 'ucomis', 'mulss', 'mulsd', 'addss', 'addsd', 'subss', 'subsd', 'divss', 'divsd',
      'minss', 'maxss', 'movss', 'movsd', 'cvtss', 'cvtsd')
_cache = {}


def exe_consts(start):
    """-> list of (f32 value or None, f64 value or None, size) for FP memory-constant operands (absolute data addresses) in the function at start."""
    if start in _cache:
        return _cache[start]
    res = []
    pc = start
    far = start
    cnt = 0
    while cnt < a.maxins:
        ins = list(md.disasm(rd(pc, 16), pc, 1))
        if not ins:
            break
        i = ins[0]
        mn, op = i.mnemonic, i.op_str
        if mn.startswith(FP):
            m = MEM.search(op)
            if m:
                sz = 4 if m.group(1) == 'dword' else 8
                raw = rd(int(m.group(2), 16), sz)
                if len(raw) == sz:
                    res.append((struct.unpack('<f', raw)[0], None, 4) if sz == 4 else (None, struct.unpack('<d', raw)[0], 8))
        if mn.startswith('j'):
            mm = re.match(r'^0x([0-9a-f]+)$', op)
            if mm:
                t = int(mm.group(1), 16)
                if start <= t < start + 0x8000 and t > far:
                    far = t
        pc += i.size
        cnt += 1
        if mn == 'ret' and pc > far:
            break
        if mn == 'jmp' and pc > far and op.startswith('0x'):
            break
    _cache[start] = res
    return res


def f32(x):
    try:
        return struct.unpack('<f', struct.pack('<f', x))[0]
    except (OverflowError, struct.error):
        return float('inf')


# ---------------- hooks ----------------
rx_hook = re.compile(r'RH_Scoped\w*Install\w*\(\s*([\w:~]+)\s*,\s*(?:"[^"]*"\s*,\s*)?0x([0-9A-Fa-f]{6})')
rx_cls = re.compile(r'RH_Scoped(?:Virtual)?Class\(\s*([\w:]+)')
rx_ns = re.compile(r'RH_ScopedNamespace\(\s*([\w:]+)')
rx_def = re.compile(r'^(?:[\w:<>\*&,~]+\s+)*?(?:(\w+)::)?(~?\w+)\s*\(')
SUF = r'[fF]' if a.suffixed else r''
LIT = r'(?<![\w.])(?:\d+\.\d*(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?|\d+[eE][+-]?\d+)' + SUF + r'(?![\w.])'    # unsuffixed decimal literal (no f/F/l/L suffix, no hex)
rx_lit = re.compile(LIT)
CMP = r'(?<![<>\-=!])(?:<=|>=|==|!=|<|>)(?![<>=])'
CAST = r'(?:\([\w: ]+\)\s*|sq\()*'                                     # (x87d) / (float) / (double) casts and sq( wrappers before the literal
rx_cmp_r = re.compile(r'(' + CMP + r')[\s(]*' + CAST + r'(-?\s*' + LIT + r')')
rx_cmp_l = re.compile(r'(' + LIT + r')\s*\)?\s*(' + CMP + r')')
rx_fn = re.compile(r'\b(?:std::)?(min|max|clamp|Clamp|Min|Max)\s*\(')
HINT = re.compile(r'\.[xyz]\b|->[xyz]\b|\bm_f[A-Z]|\bf[A-Z]\w*|Magnitude|Length|Dist|Speed|Angle|Time|Heading|Rate|Ratio|Radius|Height|Scale|Mass|\bGet\w*\(|\bf\w*\b')


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


def exact_f32(v):
    return f32(v) == v


def lit_val(s):
    try:
        return float(s.replace(' ', '').rstrip('fF'))
    except ValueError:
        return None


def paren_span(code, i):
    """code[i] is just after '(' of a call; return text up to the matching ')'."""
    depth = 1
    j = i
    while j < len(code) and depth:
        if code[j] == '(':
            depth += 1
        elif code[j] == ')':
            depth -= 1
        j += 1
    return code[i:j - 1] if depth == 0 else code[i:]


def other_operand(code, pos, left):
    """crude: the operand text on the side opposite the literal."""
    if left:    # literal is on the left of the operator; the other operand follows
        seg = code[pos:]
        return re.split(r'&&|\|\||[;?]|\)\s*\{|,', seg)[0].strip()
    seg = code[:pos]
    parts = re.split(r'&&|\|\||[;?{]|\bif\s*\(|\bwhile\s*\(|\breturn\b|,|=(?!=)', seg)
    return parts[-1].strip()


rows = []
repo = Path(a.repo)
src = repo / 'source' / 'game_sa'
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
                    infn = {'cls': cls, 'name': name, 'addrs': addrs or []}
        if infn is not None and rx_lit.search(code) and not code.lstrip().startswith('#'):
            fname = (infn['cls'] + '::' if infn['cls'] else '') + infn['name']
            found = []
            for m in rx_cmp_r.finditer(code):
                found.append((m.group(2), 'cmp', other_operand(code, m.start(), False)))
            for m in rx_cmp_l.finditer(code):
                found.append((m.group(1), 'cmp', other_operand(code, m.end(), True)))
            for m in rx_fn.finditer(code):
                args = paren_span(code, m.end())
                for lm in rx_lit.finditer(args):
                    found.append((lm.group(0), m.group(1), args))
            seen = set()
            for lit, kind, oth in found:
                v = lit_val(lit)
                if v is None or exact_f32(v) or (lit, kind, oth) in seen:
                    continue
                seen.add((lit, kind, oth))
                hint = 1 if HINT.search(oth) else 0
                rows.append((rel, ln, fname, infn['addrs'], lit.replace(' ', ''), v, kind, hint, code.strip()[:140]))
        if infn is not None and raw.startswith('}'):
            infn = None

out_rows = []
cnt = {}
for rel, ln, fn, addrs, lit, v, kind, hint, ctx in rows:
    if not addrs:
        ek = 'UNHOOKED'
        addr = ''
    else:
        cs = []
        for ad in addrs:
            cs += exe_consts(ad)
        fl = any(c[0] is not None and c[0] == f32(v) for c in cs)
        db = any(c[1] is not None and abs(c[1] - v) <= 1e-12 * abs(v) for c in cs)
        ek = 'BOTH' if fl and db else 'EXE-FLOAT' if fl else 'EXE-DOUBLE' if db else 'UNCLEAR'
        addr = '0x%06X' % addrs[0] + ('+' if len(addrs) > 1 else '')
    cnt[ek] = cnt.get(ek, 0) + 1
    out_rows.append((rel, ln, fn, addr, lit, '%.9g' % f32(v), kind, hint, ek, ctx))
if a.suffixed:
    out_rows = [r for r in out_rows if r[8] in ('EXE-DOUBLE', 'BOTH')]
out_rows.sort(key=lambda r: (r[8], r[0], r[1]))
out = Path(a.out)
out.parent.mkdir(parents=True, exist_ok=True)
with out.open('w', encoding='utf-8', newline='\n') as f:
    f.write('file\tline\tfunction\taddr\tliteral\tnearest_f32\tkind\thint\texe_kind\tcontext\n')
    for r in out_rows:
        f.write('\t'.join(str(x) for x in r) + '\n')
print('suffixed mode (f literal, exe double only) ' if a.suffixed else '', end='')
print('hits: %d' % len(out_rows) + ' | ' + ' '.join('%s=%d' % kv for kv in sorted(cnt.items())))
