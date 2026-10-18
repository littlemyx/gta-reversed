"""HEADING-AUDIT: for every hook-registry function (RH_Scoped*Install with an exe address) in the chosen source directories compare the HEADING / ANGLE events of the exe
function with those found in the port function's source text.

Events (exe: call targets and the inline fpatan; port: textual helper uses):
  LIM     CGeneral::LimitRadianAngle                 exe call 0x53CB50      port LimitRadianAngle(
  LIMDEG  CGeneral::LimitAngle                       exe call 0x53CB00      port LimitAngle(
  GRABP   CGeneral::GetRadianAngleBetweenPoints      exe call 0x53CBE0      port GetRadianAngleBetweenPoints[Ext](
  ATANXY  CGeneral::GetATanOfXY                      exe call 0x53CC70      port GetATanOfXY[Ext](
  NODEH   CGeneral::GetNodeHeadingFromVector         exe call 0x53CDC0      port GetNodeHeadingFromVector( / .NodeHeading(
  GAP     CGeneral::GetAngleBetweenPoints            exe call 0x53CEA0      port GetAngleBetweenPoints[Ext](
  GETHDG  CPlaceable::GetHeading                     exe call 0x441DB0      port GetHeading(
  FPH     FindPlayerHeading                          exe call 0x56E450      port FindPlayerHeading(
  ATAN2   inline fpatan (atan2 / atan)               exe fpatan             port .Heading( / x87::atan2( / std::atan2( / atan2f( / std::atan(
A CVector::Heading(true) counts as ATAN2 + LIM.  Port Heading() on a CVector/CVector2D is the inline `x87::atan2(-x, y)`.

usage: py -3 -X utf8 -I tools/standalone/heading_audit.py [--repo .] [--exe gta_sa_compact.exe] [--dirs source/game_sa/Tasks source/game_sa/Events ...] [--md out.md] [--all]
"""
import argparse
import collections
import re
import struct
from pathlib import Path

import capstone

ap = argparse.ArgumentParser()
ap.add_argument('--repo', default='.')
ap.add_argument('--exe', default='gta_sa_compact.exe')
ap.add_argument('--dirs', nargs='*', default=['source/game_sa/Tasks', 'source/game_sa/Events'])
ap.add_argument('--files', nargs='*', default=['source/game_sa/World.cpp', 'source/game_sa/Entity/Vehicle/Trailer.cpp'])
ap.add_argument('--md', default=None)
ap.add_argument('--explained', default=str(Path(__file__).with_name('heading_audit_explained.tsv')), help='TSV: exe address, tag, note - a DIFF / NOBODY row listed here is reported as EXPLAINED(tag)')
ap.add_argument('--all', action='store_true', help='also list OK rows')
args = ap.parse_args()
repo = Path(args.repo)

CALLS = {0x53CB50: 'LIM', 0x53CB00: 'LIMDEG', 0x53CBE0: 'GRABP', 0x53CC70: 'ATANXY', 0x53CDC0: 'NODEH', 0x53CEA0: 'GAP', 0x441DB0: 'GETHDG', 0x56E450: 'FPH'}


class PE:
    def __init__(self, path):
        self.d = Path(path).read_bytes()
        pe = struct.unpack_from('<I', self.d, 0x3c)[0]
        n = struct.unpack_from('<H', self.d, pe + 6)[0]
        osz = struct.unpack_from('<H', self.d, pe + 20)[0]
        self.base = struct.unpack_from('<I', self.d, pe + 24 + 28)[0]
        so = pe + 24 + osz
        self.secs = [(self.base + va, max(vs, rs), rp) for nm, vs, va, rs, rp in (struct.unpack_from('<8sIIII', self.d, so + 40 * i) for i in range(n))]

    def rd(self, a, k):
        for va, sz, rp in self.secs:
            if va <= a < va + sz:
                return self.d[rp + a - va:rp + a - va + k]
        return b''


exe = PE(args.exe)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

# ---------------------------------------------------------------- registry
rx = re.compile(r'RH_Scoped\w*Install\(\s*([\w:~<>]+)\s*,\s*(?:"[^"]*"\s*,\s*)?0x([0-9A-Fa-f]{5,6})')
srcs = []
for d in args.dirs:
    srcs += [p for p in sorted((repo / d).rglob('*')) if p.suffix in ('.cpp', '.h', '.inc')]
srcs += [repo / f for f in args.files]
reg = []   # (addr, cls, method, file, line)
for p in srcs:
    cur = None
    for ln, line in enumerate(p.read_text(encoding='utf-8', errors='replace').splitlines(), 1):
        m = re.search(r'RH_Scoped(?:Virtual)?Class\(\s*([\w:]+)', line)
        if m:
            cur = m.group(1)
            continue
        m = re.search(r'RH_ScopedNamespace(?:Name)?\(\s*(\w+)', line)
        if m:
            cur = m.group(1)
            continue
        if re.search(r'RH_ScopedCategoryGlobal\(', line):
            cur = None
        m = rx.search(line)
        if m:
            isg = 'GlobalInstall' in line or 'GlobalOverloaded' in line
            reg.append((int(m.group(2), 16), '' if isg else (cur or ''), m.group(1).split('::')[-1], str(p.relative_to(repo)).replace('\\', '/'), ln))

REG_ADDRS = {r[0] for r in reg}


def scan_exe(t, strict=False, limit=6000):
    far, pc, out, n = t, t, [], 0
    while n < limit:
        ins = list(md.disasm(exe.rd(pc, 16), pc, 1))
        if not ins:
            break
        i = ins[0]
        n += 1
        m = re.match(r'^0x([0-9a-f]+)$', i.op_str)
        tgt = int(m.group(1), 16) if m else None
        out.append((pc, i.mnemonic, tgt))
        if tgt is not None and i.mnemonic != 'call' and (i.mnemonic == 'jmp' or i.mnemonic.startswith('j')):
            if t <= tgt < t + 0x8000 and tgt > far:
                far = tgt
        pc += i.size
        if i.mnemonic == 'ret' or (i.mnemonic == 'jmp' and tgt is not None and not (t <= tgt < pc)):
            if pc > far:
                nb = exe.rd(pc, 1)
                if strict or pc in REG_ADDRS or nb in (b'\xcc', b'\x90', b''):
                    break
    return out


def exe_events(t, strict=False):
    ev = collections.Counter()
    for pc, mn, tgt in scan_exe(t, strict):
        if mn in ('call', 'jmp') and tgt in CALLS and tgt != t:
            ev[CALLS[tgt]] += 1
        elif mn == 'fpatan':
            ev['ATAN2'] += 1
    return ev


def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), s, flags=re.S)
    return re.sub(r'//[^\n]*', '', s)


filetext = {}


def text(p):
    if p not in filetext:
        s = strip_comments((repo / p).read_text(encoding='utf-8', errors='replace'))
        out_, depth_skip, stack = [], 0, []      # keep the first branch of #if / #ifdef, drop the #else / #elif branches (both define the same braces)
        for ln_ in s.split(chr(10)):
            t_ = ln_.strip()
            if t_.startswith('#if'):
                stack.append(False)
            elif t_.startswith('#el') and stack:
                stack[-1] = True
            elif t_.startswith('#endif') and stack:
                stack.pop()
            elif not any(stack):
                out_.append(ln_)
                continue
            out_.append('')                      # keep line structure
        filetext[p] = chr(10).join(out_)
    return filetext[p]


def brace_body(s, i):
    depth, j = 0, i
    while j < len(s):
        c = s[j]
        if c == '"':                          # skip string literals (a '{' inside one)
            j += 1
            while j < len(s) and s[j] != '"' and s[j] != chr(10):
                j += 2 if s[j] == chr(92) else 1
            j += 1
            continue
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return s[i:j + 1]
        j += 1
    return s[i:]


def bodies(cls, method):
    names = [method]
    c = cls.split('::')[-1]
    if method == 'Constructor':
        names = [c]
    elif method == 'Destructor':
        names = ['~' + c]
    res = []
    for p in srcs:
        rp = str(p.relative_to(repo)).replace('\\', '/')
        s = text(rp)
        for nm in names:
            pat = (re.escape(cls) + r'\s*::\s*' if cls else r'(?<![\w:])') + re.escape(nm) + r'\s*\('
            for m in re.finditer(pat, s):
                j, depth = m.end() - 1, 0
                while j < len(s):
                    ch = s[j]
                    if ch == '(':
                        depth += 1
                    elif ch == ')':
                        depth -= 1
                        if depth == 0:
                            break
                    j += 1
                k = j + 1
                # skip qualifiers / trailing return / initialiser list up to '{' or ';'
                d2 = 0
                while k < len(s):
                    ch = s[k]
                    if ch == '(':
                        d2 += 1
                    elif ch == ')':
                        d2 -= 1
                    elif d2 == 0 and ch == ';':
                        break
                    elif d2 == 0 and ch == '{':
                        # initialiser brace (preceded by identifier/'>' inside an init list) vs body brace: body brace is preceded by ')' / '}' / const / override / whitespace after those
                        prev = s[:k].rstrip()
                        if re.search(r'[\w>]$', prev) and re.search(r':[^;{}]*$', s[j:k]) and not re.search(r'\b(?:const|override|noexcept|final)$', prev):
                            d2 += 100   # an init-list brace: skip the braces
                            k = s.index('}', k)
                            d2 -= 100
                            k += 1
                            continue
                        break
                    k += 1
                if k < len(s) and s[k] == '{':
                    res.append((rp, brace_body(s, k)))
    return res


EV_PORT = [
    ('LIM', r'\bLimitRadianAngle\s*\('),
    ('LIMDEG', r'\bLimitAngle\s*\('),
    ('GRABP', r'\bGetRadianAngleBetweenPoints(?:Ext)?\s*\('),
    ('ATANXY', r'\bGetATanOfXY(?:Ext)?\s*\('),
    ('NODEH', r'\bGetNodeHeadingFromVector\s*\(|\.NodeHeading\s*\(|->NodeHeading\s*\('),
    ('GAP', r'\bGetAngleBetweenPoints(?:Ext)?\s*\('),
    ('GETHDG', r'\bGetHeading(?:Ext)?\s*\('),
    ('FPH', r'\bFindPlayerHeading\s*\('),
    ('ATAN2', r'(?:\.|->)Heading\s*\(|\bx87::atan2\s*\(|\bstd::atan2f?\s*\(|\batan2f?\s*\(|\bx87::atan\s*\(|\bstd::atan\s*\('),
]


def port_events(body):
    ev = collections.Counter()
    for k, rxp in EV_PORT:
        n = len(re.findall(rxp, body))
        if n:
            ev[k] += n
    ev['LIM'] += len(re.findall(r'(?:\.|->)Heading\s*\(\s*true\s*\)', body))
    return ev


def fmt(c):
    return ' '.join('%s%s' % (k, '' if v == 1 else 'x%d' % v) for k, v in sorted(c.items())) or '-'


EXPLAINED = {}
if Path(args.explained).exists():
    for l_ in Path(args.explained).read_text(encoding='utf-8').splitlines():
        c_ = l_.split(chr(9))
        if len(c_) >= 3 and c_[0].startswith('0x'):
            EXPLAINED[int(c_[0], 16)] = (c_[1], c_[2])

rows = []
for addr, cls, method, f, ln in sorted(reg):
    ee = exe_events(addr)           # loose end detection: continues past a ret that is not followed by padding / a hooked function
    es = exe_events(addr, True)     # strict: the first ret / out-of-range jmp after the farthest forward branch (exedis.py rule)
    bs = bodies(cls, method)
    pe_ = collections.Counter()
    where = []
    for rp, b in bs:
        pe_ += port_events(b)
        where.append(rp)
    if not ee and not pe_:
        continue
    st = 'OK' if (ee == pe_ or es == pe_) else 'DIFF'
    if not bs:
        st = 'NOBODY'
    if st != 'OK' and addr in EXPLAINED:
        st = 'EXPLAINED(%s)' % EXPLAINED[addr][0]
    rows.append((addr, cls, method, f, ln, ee, pe_, st, len(bs), sorted(set(where)), es))

out = ['| exe addr | function | exe events | port events | status | n bodies | port file |', '|---|---|---|---|---|---|---|']
cnt = collections.Counter(r[7] for r in rows)
for addr, cls, method, f, ln, ee, pe_, st, nb, wh, es in rows:
    if st == 'OK' and not args.all:
        continue
    out.append('| 0x%X | %s::%s | %s | %s | %s | %d | %s |' % (addr, cls, method, fmt(ee) + ('' if ee == es else ' [strict ' + fmt(es) + ']'), fmt(pe_), st, nb, ', '.join(w.split('/')[-1] for w in wh) or f.split('/')[-1]))
out.append('')
out.append('rows with events: %d, %s' % (len(rows), dict(cnt)))
res = '\n'.join(out)
print(res)
if args.md:
    Path(args.md).write_text(res + '\n', encoding='utf-8')
