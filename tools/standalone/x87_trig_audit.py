"""X87-TRIG: for every hook-registry function (RH_Scoped*Install with an exe address) compare the MATH EVENTS of the exe's code with those of the built port function.

Events (the exe calls the CRT entry points and uses inline x87 instructions):
  exe CALLS : 0x821E70 _CIasin -> ASIN      0x822380 _CIacos -> ACOS      0x822394 / 0x82239D (acos body entry) -> ACOS
              0x822130 _CIpow -> POW        0x8220F0 pow (cdecl) -> POW   0x825DB8.. fmod family -> FMOD
  exe INLINE: fsin SIN, fcos COS, fptan TAN, fpatan ATAN (atan and atan2 share the opcode), fsqrt SQRT, fyl2x LOG, fsincos SINCOS
  port      : calls of x87::asin/acos/sin/... (naked helpers in X87Intrinsics.h), calls of CRT symbols (_sinf, _powf, _fmodf ...), inline fsin/fcos/... instructions,
              sqrtss/sqrtsd (SQRT)
A function whose multiset of events differs from the exe's is a MISMATCH candidate (an asin/acos swap shows as ASIN/ACOS counts exchanged). SQRT differences are informational
(the exe's inline fsqrt vs the port's sqrt are both correctly rounded). CRT calls that are not x87:: helpers are tagged with a trailing '*' (CRT instead of x87 inline).
The exe CRT addresses are proven by tools/standalone/x87_trig_prove.py (unicorn evaluation of each entry point).

usage: py -3 -X utf8 -I tools/standalone/x87_trig_audit.py [--repo .] [--exe gta_sa_compact.exe] [--port build/StandaloneRelease/bin/gta_reversed.exe] [--md out.md] [--all]
  default prints only functions with at least one event in the exe or the port; --all adds the registry functions without events (count only).
"""
import argparse
import collections
import re
import struct
import sys
from pathlib import Path

import capstone

ap = argparse.ArgumentParser()
ap.add_argument('--repo', default='.')
ap.add_argument('--exe', default='gta_sa_compact.exe')
ap.add_argument('--port', default='build/StandaloneRelease/bin/gta_reversed.exe')
ap.add_argument('--md', default=None, help='write a markdown table here')
ap.add_argument('--all', action='store_true')
ap.add_argument('--explained', default=str(Path(__file__).with_name('x87_trig_explained.tsv')), help='TSV: exe address, tag, note - a MISMATCH / REVIEW row listed here is reported as EXPLAINED(tag)')
args = ap.parse_args()
repo = Path(args.repo)


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


md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
exe, port = PE(args.exe), PE(args.port)

EXE_CALLS = {0x821E70: 'ASIN', 0x822380: 'ACOS', 0x822394: 'ACOS', 0x82239D: 'ACOS', 0x822130: 'POW', 0x8220F0: 'POW',
             0x825DB8: 'FMOD', 0x825DC2: 'FMOD', 0x825DC9: 'FMOD', 0x825DDA: 'FMOD'}
INLINE = {'fprem': 'FMOD', 'fsin': 'SIN', 'fcos': 'COS', 'fptan': 'TAN', 'fpatan': 'ATAN', 'fsqrt': 'SQRT', 'fyl2x': 'LOG?', 'fsincos': 'SINCOS'}
PORT_INLINE = dict(INLINE, sqrtss='SQRT', sqrtsd='SQRT')

# ---- registry: exe address -> [(class or '', method, file, line)]
reg = collections.defaultdict(list)
rx = re.compile(r'RH_Scoped(?:VMT|Virtual|Global)?(?:Overloaded)?(?:Named)?Install\(\s*([\w:~]+)\s*,\s*(?:"[^"]*"\s*,\s*)?0x([0-9A-Fa-f]{5,6})')
rxg = re.compile(r'RH_Scoped(?:Named)?Global(?:Overloaded)?Install\(')
rxd = re.compile(r'RH_Scoped(Constructor|Destructor|VMTDestructor)Install\(\s*(?:\w+\s*,\s*)?0x([0-9A-Fa-f]{5,6})')
for p in sorted((repo / 'source').rglob('*')):
    if p.suffix not in ('.cpp', '.h', '.inc'):
        continue
    cur = None
    for ln, line in enumerate(p.read_text(encoding='utf-8', errors='replace').splitlines(), 1):
        m = re.search(r'RH_Scoped(?:Virtual|Named)?Class\(\s*([\w:]+)', line)
        if m:
            cur = m.group(1)
            continue
        if re.search(r'RH_ScopedNamespace(?:Name)?\(', line):
            cur = None
            continue
        m = rxd.search(line)
        if m:
            reg[int(m.group(2), 16)].append((cur or '', 'Constructor' if m.group(1) == 'Constructor' else 'Destructor', str(p.relative_to(repo)).replace('\\', '/'), ln))
            continue
        m = rx.search(line)
        if m:
            isg = bool(rxg.search(line))
            reg[int(m.group(2), 16)].append(('' if isg else (cur or ''), m.group(1).split('::')[-1], str(p.relative_to(repo)).replace('\\', '/'), ln))


# ---- function scanner (shared heuristic: end at the first ret / far jmp after the farthest forward branch)
_starts_sorted = []


def _next_start(pc):
    global _starts_sorted
    if len(_starts_sorted) != len(EXE_STARTS):
        _starts_sorted = sorted(EXE_STARTS)
    import bisect as _b
    k = _b.bisect_right(_starts_sorted, pc)
    return _starts_sorted[k] if k < len(_starts_sorted) else 1 << 60


EXE_STARTS = set()        # filled before the first exe scan: registry addresses + all call targets of the game range


def scan(pe, t, limit=20000):
    far, pc, out = t, t, []
    term_far = None     # the farthest forward branch seen up to the last ret / jmp (branches decoded from a jump table after it are garbage)
    n = 0
    while n < limit:
        ins = list(md.disasm(pe.rd(pc, 16), pc, 1))
        if not ins:
            break
        i = ins[0]
        n += 1
        if pe is exe and term_far is not None and pc != t and (pc in EXE_STARTS or _next_start(pc) < pc + i.size) and pc >= term_far:
            break        # reached the next known function (no forward branch of this function reaches beyond it)
        m = re.match(r'^0x([0-9a-f]+)$', i.op_str)
        tgt = int(m.group(1), 16) if m else None
        out.append((pc, i.mnemonic, tgt))
        if tgt is not None and i.mnemonic != 'call' and (i.mnemonic == 'jmp' or i.mnemonic.startswith('j')):
            if t <= tgt < t + 0x6000 and tgt > far and not (pe is exe and i.mnemonic == 'jmp' and tgt in EXE_STARTS):   # a jmp to another function's entry is a tail call
                far = tgt
        pc += i.size
        if (i.mnemonic == 'ret' or (i.mnemonic == 'jmp' and tgt is not None and not (t <= tgt < pc))) and pc > far:
            term_far = far       # a closing ret / tail jump: no branch of the function reaches beyond it
        if (i.mnemonic == 'ret' or (i.mnemonic == 'jmp' and tgt is not None and not (t <= tgt < pc))) and pc > far:
            # a ret in the middle of a function (early return) is followed by more code: stop only at padding or at the next known function start
            nb = pe.rd(pc, 1)
            if pe is not exe or pc in EXE_STARTS or nb in (bytes([204]), bytes([144]), b''):
                break
    return out


ASINSEQ = ['fld1', 'fadd', 'fld1', 'fsub', 'fmulp', 'fsqrt']


def patan_kind(prev):
    """the CRT's asin / acos body inlined as x87 asm: fld1 fadd fld1 fsub fmulp fsqrt [fxch] fpatan -> ASIN (no fxch) / ACOS (fxch), else a plain ATAN"""
    if prev[-1:] == ['fxch'] and prev[-7:-1] == ASINSEQ:
        return 'ACOS'
    if prev[-6:] == ASINSEQ:
        return 'ASIN'
    return 'ATAN'


EXE_WRAPPER_SITES = set()   # the call instructions inside the wrappers
EXE_WRAPPERS = {}      # tiny functions (<= 8 instructions, ret) that call a CRT entry: `fld dword [esp+4]; call _CIacos; ret` (the exe's acosf / powf ... COMDATs)


def _find_wrappers():
    sweep = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    sweep.skipdata = True
    text = exe.rd(0x401000, 0x456000)
    ins = list(sweep.disasm(text, 0x401000))
    for k, i in enumerate(ins):
        if i.mnemonic == 'call' and i.op_str.startswith('0x') and int(i.op_str, 16) in EXE_CALLS and k + 1 < len(ins) and ins[k + 1].mnemonic == 'ret':
            # walk back to the start of the wrapper (at most 6 instructions, no other call / ret before it)
            j = k
            while j > 0 and k - j < 6 and ins[j - 1].mnemonic not in ('ret', 'int3', 'nop', 'call', 'jmp') and not ins[j - 1].mnemonic.startswith('j'):
                j -= 1
            EXE_WRAPPERS[ins[j].address] = EXE_CALLS[int(i.op_str, 16)]
            EXE_WRAPPER_SITES.add(i.address)


_find_wrappers()
_sw = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
_sw.skipdata = True
_ins = [(i.address, i.mnemonic, i.op_str) for i in _sw.disasm(exe.rd(0x401000, 0x456000), 0x401000)]
ALLCALLS = [(a_, int(o_, 16)) for a_, m_, o_ in _ins if m_ == 'call' and o_.startswith('0x')]
for k_, (a_, m_, o_) in enumerate(_ins[:-1]):          # a function start also follows `ret` / `jmp` + nop / int3 padding
    if m_ in ('ret', 'jmp') and _ins[k_ + 1][1] in ('nop', 'int3'):
        j_ = k_ + 1
        while j_ < len(_ins) and _ins[j_][1] in ('nop', 'int3'):
            j_ += 1
        if j_ < len(_ins):
            EXE_STARTS.add(_ins[j_][0])
EXE_STARTS |= {t for _, t in ALLCALLS if 0x401000 <= t < 0x748000} | set(reg)
# virtual methods are only reached through vtables: every dword in the data sections that points just after a ret / int3 / nop of the code range
for _va, _sz, _rp in exe.secs[2:4]:
    _blk = exe.d[_rp:_rp + _sz]
    for _o in range(0, len(_blk) - 3, 4):
        _v = struct.unpack_from('<I', _blk, _o)[0]
        if 0x401000 < _v < 0x748000:
            _pb = exe.rd(_v - 3, 3)
            if _pb[2:3] in (bytes([0xC3]), bytes([0xCC]), bytes([0x90])) or _pb[0:1] == bytes([0xC2]):
                EXE_STARTS.add(_v)
EXE_GAME_END = 0x7F0000          # above: RenderWare (librw in the port) and the CRT
DEPTH = 2                        # unregistered callees (the exe's / the port's private subroutines and inlined helpers) are expanded this deep


def exe_own(a):
    ev = collections.Counter()
    callees = []
    prev = []
    for pc, mn, tgt in scan(exe, a):
        if mn == 'call' and tgt in EXE_CALLS:
            ev[EXE_CALLS[tgt]] += 1
        elif mn == 'call' and tgt in EXE_WRAPPERS:
            ev[EXE_WRAPPERS[tgt]] += 1
        elif mn == 'fyl2x':
            # the base constant is loaded a few instructions before: fldlg2 = log10, fldln2 = ln, fld1 = log2
            k = 'LOG?'
            for q in reversed(prev[-6:]):
                if q == 'fldlg2':
                    k = 'LOG10'; break
                if q == 'fldln2':
                    k = 'LN'; break
                if q == 'fld1':
                    k = 'LOG2'; break
            ev[k] += 1
        elif mn == 'fpatan':
            k = patan_kind(prev)
            ev[k] += 1
            if k != 'ATAN':
                ev['SQRT'] -= 1
        elif mn in INLINE:
            ev[INLINE[mn]] += 1
        elif mn == 'call' and tgt is not None and 0x401000 <= tgt < EXE_GAME_END:
            callees.append(tgt)
        prev.append(mn)
    return ev, callees


_exe_own = {}


_exe_memo = {}
_port_memo = {}


def exe_events(a, depth=0, maxdepth=DEPTH, all_=False):
    key = (a, depth, maxdepth, all_)
    if key in _exe_memo:
        return collections.Counter(_exe_memo[key])
    if a not in _exe_own:
        _exe_own[a] = exe_own(a)
    ev, callees = _exe_own[a]
    ev = collections.Counter(ev)
    if depth < maxdepth:
        for c in callees:
            if (all_ or c not in reg) and c != a:
                ev += exe_events(c, depth + 1, maxdepth, all_)
    _exe_memo[key] = collections.Counter(ev)
    return ev


# ---- port map
va2names = collections.defaultdict(list)
obj_of = {}
code_vas = []
for line in Path(args.port).with_suffix('.map').read_text(encoding='utf-8', errors='replace').splitlines():
    q = line.split()
    if len(q) >= 3 and re.match(r'^0001:[0-9a-f]+$', q[0]):
        try:
            va = int(q[2], 16)
        except ValueError:
            continue
        va2names[va].append(q[1])
        code_vas.append(va)
        obj_of[va] = q[-1]
code_vas = sorted(set(code_vas))
name2vas = collections.defaultdict(list)
for va, ns in va2names.items():
    for n in ns:
        name2vas[n].append(va)

KIND_BY_NAME = [
    (re.compile(r'^\?(sin|cos|tan|atan|atan2|asin|acos|log2|log10|ln|sqrt|pow)@x87@@|^\?(Pow)@powdetail@x87@@'), None),
    (re.compile(r'^_{1,2}(?:CI)?(sin|cos|tan|asin|acos|atan|atan2|pow|fmod|log10|log|exp|sqrt)f?(?:_default)?$|^__libm_sse2_(sin|cos|tan|asin|acos|atan|atan2|pow|fmod|log10|log|exp|sqrt)_precise$'), None),
]
NAME2KIND = {'sin': 'SIN', 'cos': 'COS', 'tan': 'TAN', 'atan': 'ATAN', 'atan2': 'ATAN', 'asin': 'ASIN', 'acos': 'ACOS', 'log2': 'LOG2', 'log10': 'LOG10', 'ln': 'LN',
             'log': 'LN', 'sqrt': 'SQRT', 'pow': 'POW', 'fmod': 'FMOD', 'exp': 'EXP'}


def port_call_kind(t):
    for n in va2names.get(t, ()):
        m = KIND_BY_NAME[0][0].match(n)
        if m:
            return NAME2KIND[(m.group(1) or m.group(2)).lower()]
        m = KIND_BY_NAME[1][0].match(n)
        if m:
            return NAME2KIND[m.group(1) or m.group(2)] + '*'
    return None


reg_port_vas = set()           # filled after the registry is matched


def port_own(va):
    import bisect
    k = bisect.bisect_right(code_vas, va)
    end = code_vas[k] if k < len(code_vas) else va + 0x4000
    ev = collections.Counter()
    callees = []
    prev = []
    pc = va
    while pc < end:
        ins = list(md.disasm(port.rd(pc, 16), pc, 1))
        if not ins:
            break
        i = ins[0]
        m = re.match(r'^0x([0-9a-f]+)$', i.op_str)
        if i.mnemonic in ('call', 'jmp') and m:
            t = int(m.group(1), 16)
            kk = port_call_kind(t)
            if kk:
                ev[kk] += 1
            elif i.mnemonic == 'call' and t in va2names and ':' not in obj_of.get(t, ':') and not any(n.startswith('_') for n in va2names[t]):
                callees.append(t)
        elif i.mnemonic == 'fyl2x':
            k = 'LOG?'
            for q in reversed(prev[-6:]):
                if q == 'fldlg2':
                    k = 'LOG10'; break
                if q == 'fldln2':
                    k = 'LN'; break
                if q == 'fld1':
                    k = 'LOG2'; break
            ev[k] += 1
        elif i.mnemonic == 'fpatan':
            k = patan_kind(prev)
            ev[k] += 1
            if k != 'ATAN':
                ev['SQRT'] -= 1
        elif i.mnemonic in PORT_INLINE:
            ev[PORT_INLINE[i.mnemonic]] += 1
        prev.append(i.mnemonic)
        pc += i.size
    return ev, callees


_port_own = {}


def port_events(va, depth=0, maxdepth=DEPTH, all_=False):
    key = (va, depth, maxdepth, all_)
    if key in _port_memo:
        return collections.Counter(_port_memo[key])
    if va not in _port_own:
        _port_own[va] = port_own(va)
    ev, callees = _port_own[va]
    ev = collections.Counter(ev)
    if depth < maxdepth:
        for c in callees:
            if (all_ or c not in reg_port_vas) and c != va:
                ev += port_events(c, depth + 1, maxdepth, all_)
    _port_memo[key] = collections.Counter(ev)
    return ev


by_meth = collections.defaultdict(list)
for n, vas in name2vas.items():
    if n.startswith('??0') or n.startswith('??1'):
        m = re.match(r'^\?\?([01])(\w+)@@', n)
        if m:
            by_meth[('ctor' if m.group(1) == '0' else 'dtor', m.group(2))].append((n, vas))
    elif n.startswith('?'):
        m = re.match(r'^\?(\w+)@', n)
        if m:
            by_meth[m.group(1)].append((n, vas))


def mangled_candidates(cls, meth):
    out = set()
    if meth in ('Constructor', 'Destructor'):
        for n, vas in by_meth.get(('ctor' if meth == 'Constructor' else 'dtor', cls.split('::')[-1]), ()):
            out.update(vas)
        return out
    pats = [r'^\?%s@%s@@' % (re.escape(meth), re.escape(cls.split('::')[-1]))] if cls else []
    for n, vas in by_meth.get(meth, ()):
        if any(re.match(p, n) for p in pats):
            out.update(vas)
    if not out:       # global functions / functions registered under a class scope that is not their real owner / file-local (anonymous namespace) helpers: any scope
        for n, vas in by_meth.get(meth, ()):
            if re.match(r'^\?%s@[\w?$@]*@@[A-Z]' % re.escape(meth), n) and not n.startswith('??'):
                out.update(vas)
    return out


def delta(ee, pev):
    e, q = norm(ee), norm(pev)
    return ' '.join('%s:%d/%d' % (k, e[k], q[k]) for k in sorted(set(e) | set(q)) if e[k] != q[k])


def fmt(c):
    return ' '.join('%s%s' % (k, ('x%d' % v) if v != 1 else '') for k, v in sorted(c.items())) or '-'


def norm(c):
    """port counts with '*' (CRT) folded into the base kind, SQRT dropped (informational)"""
    o = collections.Counter()
    for k, v in c.items():
        k = k.rstrip('*')
        if k == 'SQRT':
            continue
        if k == 'SINCOS':
            o['SIN'] += v; o['COS'] += v
        else:
            o[k] += v
    return o


EXPLAINED = {}
if Path(args.explained).exists():
    for ln_ in Path(args.explained).read_text(encoding='utf-8').splitlines():
        q_ = ln_.split('	')
        if len(q_) >= 3 and q_[0].startswith('0x'):
            EXPLAINED[int(q_[0], 16)] = (q_[1], q_[2])
rows = []
nonev = 0
cand_of = {}
for a in sorted(reg):
    c = set()
    for cls, meth, f, ln in reg[a]:
        c |= mangled_candidates(cls, meth)
    cand_of[a] = c
    reg_port_vas |= c
KCRIT = {'ASIN', 'ACOS', 'POW', 'FMOD', 'EXP', 'LOG10', 'LN', 'LOG2', 'LOG?'}   # the exe's CRT calls / log bases: must match exactly (in the best variant)
KTRIG = {'SIN', 'COS', 'TAN', 'ATAN'}


def only(c, ks):
    return collections.Counter({k: v for k, v in norm(c).items() if k in ks})


def dist(x, y, ks=None):
    if ks is None:
        return sum(((norm(x) - norm(y)) + (norm(y) - norm(x))).values())
    return sum(((only(x, ks) - only(y, ks)) + (only(y, ks) - only(x, ks))).values())


for a in sorted(reg):
    names = reg[a]
    cands = cand_of[a]
    exs = [(exe_events(a, 0, 0), 'own'), (exe_events(a), 'unreg-callees'), (exe_events(a, 0, 3, True), 'all-callees d3')]
    pxs = lambda v: ((port_events(v, 0, 0), 'own'), (port_events(v), 'unreg-callees'), (port_events(v, 0, 3, True), 'all-callees d3'))
    best = None
    for v in cands:
        for (pe_, pn) in pxs(v):
            for (ee_, en) in exs:
                # lexicographic: critical kinds first, then every kind, then the amount of expansion
                key = (dist(pe_, ee_, KCRIT), dist(pe_, ee_), (en != 'own') + (pn != 'own') + (en == 'all-callees d3') + (pn == 'all-callees d3'))
                if best is None or key < best[0]:
                    best = (key, v, pe_, ee_, en, pn)
    if best:
        ee, pev = best[3], best[2]
        how = 'own' if best[4] == 'own' and best[5] == 'own' else 'exe:%s port:%s' % (best[4], best[5])
        # explained by helpers: every critical kind of one side's OWN code is present in the other side's whole call tree, and every exe sin / cos / tan / atan is in the port's tree
        pall = max(pxs(best[1]), key=lambda t: sum(norm(t[0]).values()))[0]
        eown, eall = exs[0][0], exs[2][0]
        pown = pxs(best[1])[0][0]
        crit_sub = all(only(eown, KCRIT)[k] <= only(pall, KCRIT)[k] for k in KCRIT) and all(only(pown, KCRIT)[k] <= only(eall, KCRIT)[k] for k in KCRIT)
        trig_sub = all(only(eown, KTRIG)[k] <= only(pall, KTRIG)[k] for k in KTRIG)
        best_key = best[0]
    else:
        ee, pev, how, trig_sub, crit_sub, best_key = exs[0][0], collections.Counter(), '', False, False, (9, 9, 9)
    if not ee and not any(norm(pev).values()):
        nonev += 1
        if not args.all:
            continue
    nm = ', '.join(sorted({(c + '::' if c else '') + m for c, m, f, ln in names}))
    src = names[0][2] + ':' + str(names[0][3])
    if not cands:
        verdict = 'NOPORT'
    elif best_key[0] > 0 and not (crit_sub and trig_sub):
        verdict = 'MISMATCH'          # a critical kind differs (asin / acos / pow / fmod / exp / log base) and no helper explains it
    elif best_key[1] == 0:
        verdict = 'OK' if (ee.get('SQRT', 0) == pev.get('SQRT', 0) + pev.get('SQRT*', 0)) else 'OK(sqrt differs)'
    elif crit_sub and trig_sub:
        verdict = 'OK(helper)'        # the exe's inlined helpers (or the port's) account for the difference
    else:
        verdict = 'REVIEW'            # sin / cos / tan / atan counts differ
    if verdict in ('MISMATCH', 'REVIEW'):
        how += ' | ' + delta(ee, pev)
        if a in EXPLAINED:
            verdict = 'EXPLAINED(%s)' % EXPLAINED[a][0]
    rows.append((a, nm, src, ee, pev, verdict, how))

# ---- exe call sites of the CRT entries (directly or through a tiny wrapper) -> containing function
sites = []
text = exe.rd(0x401000, 0x456000)
sweep = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
sweep.skipdata = True
import bisect
allcalls = ALLCALLS
starts = sorted(set(reg) | EXE_STARTS)
for addr, t in allcalls:
    kind = EXE_CALLS.get(t) or EXE_WRAPPERS.get(t)
    if not kind or addr in EXE_WRAPPER_SITES:
        continue
    k = bisect.bisect_right(starts, addr) - 1
    sites.append((addr, kind, t, starts[k] if k >= 0 else None))

cnt = collections.Counter(r[5] for r in rows)
print('registry functions: %d; with math events: %d (%s); without: %d' % (len(reg), len(rows), ', '.join('%s %d' % kv for kv in sorted(cnt.items())), nonev))
print('%-9s %-44s %-8s %-26s %-26s %s' % ('exe', 'function', 'verdict', 'exe events', 'port events', 'registry file:line'))
for a, nm, src, ee, pev, verdict, how in rows:
    print('%-9s %-44s %-8s %-26s %-26s %-16s %s' % ('%#x' % a, nm[:44], verdict, fmt(ee), fmt(pev), how, src))
print()
print('CRT entry call sites in the exe: %d (RenderWare / librw range >= %#x: %d)' % (len(sites), EXE_GAME_END, sum(1 for x in sites if x[0] >= 0x748000)))
byrow = {r[0]: r for r in rows}
for addr, kind, t, owner in sites:
    r = byrow.get(owner)
    where = ('RW' if addr >= 0x748000 else ('%s [%s]' % (r[1], r[5]) if r else '(fn %#x not in the table: unregistered or beyond the heuristic range)' % (owner or 0)))
    print('site %#x %-5s -> %#x   owner %#x %s' % (addr, kind, t, owner or 0, where))
if args.md:
    with open(args.md, 'w', encoding='utf-8') as f:
        f.write('| exe fn | function | verdict | exe events | port events | registry file:line |\n|---|---|---|---|---|---|\n')
        for a, nm, src, ee, pev, verdict, how in rows:
            f.write('| %#x | %s | %s | %s | %s | %s |\n' % (a, nm, verdict, fmt(ee), fmt(pev), src))
