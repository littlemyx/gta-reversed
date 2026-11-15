#!/usr/bin/env python3
"""asmsig.py - P4T1 triage tooling: annotate an exe function, and diff the "signature" of the exe's function against the build's code of the same function.

usage (python 3 + capstone, run from the repo root; the exe is the oracle gta_sa_compact.exe, the build is build/StandaloneRelease/bin/gta_reversed.exe + .map):
  asmsig.py annot ADDR [--chunk K] [--exe EXE]         annotated exe disassembly: stack depth tracked, [esp+N]/[ebp+N] -> argK/locN, consts resolved, callees named, jumps -> labels
  asmsig.py diff  ADDR [--port PE --map MAP]            signature of the exe function and of the port function; signals + score
  asmsig.py batch LIST.tsv -o OUT.tsv [--port ..]       LIST has an `addr` column (P4_ORACLE_ONLY.tsv); writes addr, function, score, signals (sorted by score desc)

Signature (identical extraction for both sides, helpers expanded up to depth 2, only small non-registered callees): named callees (hook-registry name on the exe side, MAP symbol on the port side,
only names that exist in the registry are comparable), virtual call slots, read-only float constants (dword/qword operand of x87/SSE ops, value-keyed so float-vs-double literals differ), immediates
(address-like and shifts dropped, +-1 tolerance for the compiler's cmp/jcc rewrites), [reg+disp] member displacements, conditional branch count, instruction count.
Score = weighted number of one-sided signals; AUTO_MATCH (score < T) is triage ONLY - never a verdict.
"""
import argparse, collections, os, re, struct, sys
import capstone
from capstone import x86_const as X

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..'))
import oracle_coverage as oc  # noqa: E402

EXE_DEFAULT = 'gta_sa_compact.exe'
PORT_DEFAULT = 'build/StandaloneRelease/bin/gta_reversed.exe'
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = True
FLOAT_MN = re.compile(r'^(fld|fst|fstp|fadd|fsub|fsubr|fmul|fdiv|fdivr|fcom|fcomp|fadds|fmuls)$|^(mov|add|sub|mul|div|min|max|comi|ucomi|sqrt|cvt\w*)s[sd]$|^(add|sub|mul|div|min|max|and|or|xor|andn)p[sd]$|^(u?comi)s[sd]$')


# x87 control word bits, MSVC rand() LCG, inlined strlen/strcmp magic, plain byte/word masks: appear on one side only for compiler reasons
IGN_IMM = {0, 1, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, 0xC00, 0xF00, 0x343FD, 0x269EC3, 0x7FFF, 0x7EFEFEFF, 0x81010100, 0xFF, 0xFFFF, 0xFFFFFF, 0xFF00, 0xFF0000, 0xFF000000, 0xFFFFFF00, 0xFFFF0000,
           0x8000, 0x10000, 0x20000000, 0x40000000}


class PE:
    def __init__(self, path):
        self.d = open(path, 'rb').read()
        pe = struct.unpack_from('<I', self.d, 0x3c)[0]
        n = struct.unpack_from('<H', self.d, pe + 6)[0]
        osz = struct.unpack_from('<H', self.d, pe + 20)[0]
        self.base = struct.unpack_from('<I', self.d, pe + 24 + 28)[0]
        so = pe + 24 + osz
        self.secs = []
        for i in range(n):
            nm, vs, va, rs, rp, _, _, _, _, ch = struct.unpack_from('<8sIIIIIIHHI', self.d, so + 40 * i)
            self.secs.append((self.base + va, max(vs, rs) if rs else 0, rp, ch, nm.rstrip(b'\0').decode('latin1'), min(vs, rs) if rs else 0))
        self.cache = {}

    def sec(self, a):
        for s in self.secs:
            if s[0] <= a < s[0] + s[1]:
                return s
        return None

    def rd(self, a, k):
        s = self.sec(a)
        if not s or a - s[0] >= s[5]:
            return b''
        return self.d[s[2] + a - s[0]:s[2] + a - s[0] + k]

    def is_exec(self, a):
        s = self.sec(a)
        return bool(s and s[3] & 0x20000000)

    def is_ro(self, a):
        s = self.sec(a)
        return bool(s and not (s[3] & 0x80000000) and not (s[3] & 0x20000000))

    def decode(self, a):
        r = self.cache.get(a)
        if r is None:
            ins = next(md.disasm(self.rd(a, 16), a, 1), None)
            r = self.cache[a] = ins or False
        return r or None


def is_jcc(i):
    return i.mnemonic[0] == 'j' and i.mnemonic != 'jmp'


def imm_target(i):
    if i.operands and i.operands[0].type == X.X86_OP_IMM:
        return i.operands[0].imm & 0xFFFFFFFF
    return None


def extent(pe, start, maxn=5000):
    """linear walk from start; ends at the first ret / out-of-range tail jmp past the highest forward jump target (jump tables followed)."""
    out, pc, far = [], start, start
    while len(out) < maxn:
        i = pe.decode(pc)
        if i is None:
            break
        out.append(i)
        t = imm_target(i)
        if (is_jcc(i) or i.mnemonic == 'jmp') and t is not None and start <= t < start + 0x6000 and t > far:
            far = t
        if i.mnemonic == 'jmp' and i.operands and i.operands[0].type == X.X86_OP_MEM:
            o = i.operands[0].mem
            if o.index and o.scale == 4 and o.base == 0 and pe.is_ro(o.disp & 0xFFFFFFFF):
                tb = o.disp & 0xFFFFFFFF
                for k in range(300):
                    v = struct.unpack('<I', (pe.rd(tb + 4 * k, 4) or b'\0\0\0\0'))[0]
                    if not (start <= v < start + 0x6000):
                        break
                    far = max(far, v)
        pc += i.size
        if i.mnemonic in ('ret', 'int3') and pc > far:
            break
        if i.mnemonic == 'jmp' and pc > far and (t is None or not (start <= t < pc)):
            break
    return out


def qual(n):
    """MSVC mangled name -> last two qualified components, or None."""
    m = re.match(r'^\?\?([01])((?:[^@?]+@)+)@', n)
    if m:
        sc = m.group(2).split('@')[:-1]
        q = list(reversed(sc)) + ['Constructor' if m.group(1) == '0' else 'Destructor']
        return tuple(q[-2:])
    m = re.match(r'^\?([^@?$]+)((?:@[^@?]+)*)@@', n)
    if m:
        sc = [s for s in m.group(2).split('@') if s]
        return tuple((list(reversed(sc)) + [m.group(1)])[-2:])
    return None


def norm_site(cls, fn, kind=''):
    fn = fn.split('-')[0]
    if 'Global' in kind:        # free function: RH_ScopedNamespaceName is only a label, the mangled name carries no scope
        return (fn,)
    q = ((cls + '::' if cls else '') + fn).split('::')
    return tuple(q[-2:])


INJ = re.compile(r'^\s*(?:static\s+)?void\s+([A-Za-z0-9_:]+)::InjectHooks\s*\(')


def inventory(repo):
    """oracle_coverage.inventory with the class scope fixed: RH_ScopedCategoryGlobal does not reset it, `void X::InjectHooks()` seeds it."""
    sites = []
    for s in oc.inventory(repo):
        sites.append(s)
    byfile = collections.defaultdict(list)
    for s in sites:
        byfile[s['file']].append(s)
    for f, ss in byfile.items():
        if all(s['cls'] for s in ss):
            continue
        cur, idx = None, {s['line']: s for s in ss}
        for k, ln in enumerate(open(os.path.join(repo, f), encoding='utf-8', errors='replace').read().split(chr(10))):
            m = INJ.match(ln) or oc.CLS.match(ln) or oc.NS.match(ln)
            if m:
                cur = m.group(1)
            s = idx.get(k + 1)
            if s and not s['cls'] and 'Global' not in s['kind'] or (s and not s['cls'] and cur and 'Global' in s['kind'] and '::' not in cur and cur[0] != 'C'):
                s['cls'] = cur
    return sites


class Ctx:
    def __init__(self, repo, exe, port, mapf):
        self.exe, self.port = PE(exe), PE(port) if port else None
        self.sites = inventory(repo)
        self.exe_name = collections.defaultdict(set)    # exe addr -> {qual}
        self.reg_names = set()
        for s in self.sites:
            q = norm_site(s['cls'], s['fn'], s['kind'])
            self.exe_name[s['addr']].add(q)
            self.reg_names.add(q)
        self.va2q, self.q2va, self.va2obj = {}, collections.defaultdict(list), {}
        if mapf and port:
            for line in open(mapf, encoding='utf-8', errors='replace'):
                p = line.split()
                if len(p) >= 4 and p[0].startswith('0001:') and p[3] == 'f':
                    try:
                        va = int(p[2], 16)
                    except ValueError:
                        continue
                    q = qual(p[1])
                    if q:
                        self.va2q.setdefault(va, q)
                        self.q2va[q].append(va)
                    self.va2obj[va] = p[4] if len(p) > 4 else ''
        self.sc = {}

    # ---- signature ----
    def sig(self, side, start, depth=0, seen=None):
        pe = self.exe if side == 'exe' else self.port
        key = (side, start, depth)
        if key in self.sc:
            return self.sc[key]
        seen = seen or set()
        S = dict(call=collections.Counter(), vcall=collections.Counter(), flt=collections.Counter(), imm=collections.Counter(), disp=collections.Counter(), jcc=0, n=0, x=0, small=set(), cflt=collections.Counter(), cimm=collections.Counter())
        ins = extent(pe, start)
        S['n'] = len(ins)
        hi = ins[-1].address + ins[-1].size if ins else start
        for i in ins:
            mn = i.mnemonic
            if mn == 'call' or (mn == 'jmp' and (imm_target(i) is None or not (start <= imm_target(i) < hi))):
                t = imm_target(i)
                if t is None:
                    o = i.operands[0] if i.operands else None
                    if o is not None and o.type == X.X86_OP_MEM and mn == 'call':
                        if o.mem.base and not o.mem.index:
                            S['vcall'][o.mem.disp] += 1
                        else:
                            S['x'] += 1
                    continue
                self.callee(side, pe, t, S, depth, seen | {start})
                continue
            if is_jcc(i):
                S['jcc'] += 1
                continue
            self.consts(pe, i, S)
        self.sc[key] = S
        return S

    def closure(self, side, pe, t, S, depth):
        """constants of the callee body (any callee, <150 insns, depth 1): tolerates the other build having inlined it"""
        if depth >= 1 or not pe.is_exec(t):
            return
        sub = extent(pe, t, 150)
        if sub and len(sub) < 150 and sub[-1].mnemonic in ('ret', 'jmp'):
            C = self.sig(side, t, depth + 1, {t})
            S['cflt'].update(C['flt']); S['cimm'].update(C['imm']); S['cflt'].update(C['cflt']); S['cimm'].update(C['cimm'])

    def callee(self, side, pe, t, S, depth, seen):
        self.closure(side, pe, t, S, depth)
        self._callee(side, pe, t, S, depth, seen)

    def _callee(self, side, pe, t, S, depth, seen):
        if side == 'exe':
            nm = self.exe_name.get(t)
            if nm:
                self.named(side, pe, t, sorted(nm)[0], S, depth, seen)
                return
            small = pe.is_exec(t)
        else:
            q = self.va2q.get(t)
            if q and q in self.reg_names:
                self.named(side, pe, t, q, S, depth, seen)
                return
            obj = self.va2obj.get(t, '')
            small = pe.is_exec(t) and ':' not in obj and obj != ''
        if small and depth < 2 and t not in seen:
            sub = extent(pe, t, 80)
            if len(sub) < 60 and sub and sub[-1].mnemonic in ('ret', 'jmp'):
                C = self.sig(side, t, depth + 1, seen)
                for k in ('call', 'vcall', 'flt', 'imm', 'disp'):
                    S[k].update(C[k])
                S['jcc'] += C['jcc']
                S['n'] += C['n']
                S['small'] |= C['small']
                return
        S['x'] += 1

    def named(self, side, pe, t, q, S, depth, seen):
        S['call'][q] += 1
        if depth < 2 and t not in seen:
            sub = extent(pe, t, 50)
            if len(sub) < 40 and sub and sub[-1].mnemonic in ('ret', 'jmp'):
                S['small'].add(q)
                C = self.sig(side, t, depth + 1, seen)
                for k in ('vcall', 'flt', 'imm', 'disp'):
                    S[k].update(C[k])
                S['jcc'] += C['jcc']
                S['n'] += C['n']
                S['small'] |= C['small']

    def consts(self, pe, i, S):
        mn = i.mnemonic
        shift = mn in ('shl', 'sal', 'shr', 'sar', 'rol', 'ror')
        for o in i.operands:
            if o.type == X.X86_OP_IMM and not shift and mn not in ('ret',):
                v = o.imm & 0xFFFFFFFF
                if v in IGN_IMM or 0x400000 <= v < 0x2000000:
                    continue
                if v >= 0x2000000:                      # float literal materialised as an integer immediate (mov/push 0x437f0000)
                    f = struct.unpack('<f', struct.pack('<I', v))[0]
                    if f == f and 1e-6 < abs(f) < 1e9 and struct.unpack('<f', struct.pack('<f', float('%.8g' % f)))[0] == f:
                        if abs(f) != 1.0:
                            S['flt']['%.9g' % f] += 1
                        continue
                if mn in ('add', 'sub') and i.operands[0].type == X.X86_OP_REG and i.operands[0].reg == X.X86_REG_ESP:
                    continue
                S['imm'][v] += 1
            elif o.type == X.X86_OP_MEM:
                m = o.mem
                if m.base not in (0, X.X86_REG_ESP, X.X86_REG_EBP, X.X86_REG_EIP) and abs(m.disp) >= 4 and m.disp < 0x10000 and mn != 'lea':
                    S['disp'][m.disp & 0xFFFFFFFF] += 1
                if m.base == 0 and m.index == 0:
                    a = m.disp & 0xFFFFFFFF
                    if FLOAT_MN.match(mn) and pe.sec(a) is not None and not pe.is_exec(a):
                        sz = o.size
                        b = pe.rd(a, 8 if sz == 8 else 4)
                        if len(b) == (8 if sz == 8 else 4):
                            v = struct.unpack('<d' if sz == 8 else '<f', b)[0]
                            if v == v and abs(v) != float('inf') and abs(v) != 1.0 and v != 0.0:
                                if not pe.is_ro(a) and float('%.6g' % v) != v and float('%.6g' % v) != struct.unpack('<f', struct.pack('<f', v))[0]:
                                    continue        # .data: only constant-looking initial values (tunables); mutable globals start at 0 / arbitrary
                                S['flt']['%.9g' % v] += 1


def tol_diff(a, b):
    """values present in counter a but not within +-1 (small values) of any key of b."""
    out = []
    bk = list(b)
    for k in a:
        if k in b:
            continue
        if k < 0x10000 and any(abs(k - j) <= 1 for j in bk):
            continue
        out.append(k)
    return out


W_FLT, W_CALL, W_VCALL, W_IMM_BIG, W_IMM_SMALL, W_DISP = 3.0, 2.5, 1.0, 2.0, 0.4, 0.3


CAP = dict(F=9.0, f=1.5, C=7.5, V=3.0, I=6.0, i=1.2, D=0.9, J=1.5, N=1.0)


def compare(E, P):
    """-> (score, signals). Each signal type contributes at most CAP[type] so a big function with many one-sided constants cannot swamp the ranking."""
    tot = collections.Counter()
    sg = []

    def add(t, w, txt):
        sg.append(txt)
        tot[t] += w
    fe, fp = set(E['flt']), set(P['flt'])
    for o, a, b in (('port', fe - set(P['cflt']), fp), ('exe', fp - set(E['cflt']), fe)):
        for v in sorted(a - b):
            f = float(v)
            weak = f != 0 and (abs(f) in (0.5, 2.0, 4.0, 8.0, 16.0, 0.25, 0.125, 4294967296.0, 2147483648.0, 65536.0, 255.0, 256.0, 3.0517578125e-05) or abs(f) < 1e-9)
            add('f' if weak else 'F', 0.5 if weak else W_FLT, 'F-%s:%s' % (o, v))
    ce, cp = set(E['call']), set(P['call'])
    sm = E['small'] | P['small']
    for c in sorted(ce - cp):
        if c not in sm:
            add('C', W_CALL, 'C-port:' + '::'.join(c))
    for c in sorted(cp - ce):
        if c not in sm:
            add('C', W_CALL, 'C-exe:' + '::'.join(c))
    for o, a, b in (('exe', E['vcall'], P['vcall']), ('port', P['vcall'], E['vcall'])):
        for v in sorted(set(a) - set(b)):
            add('V', W_VCALL, 'V-only-%s:%#x' % (o, v))
    for o, a, b in (('exe', E['imm'], collections.Counter(P['imm']) + P['cimm']), ('port', P['imm'], collections.Counter(E['imm']) + E['cimm'])):
        for v in sorted(tol_diff(a, b)):
            if 0x100 <= v < 0xFFFFFF00:
                add('I', W_IMM_BIG, 'I-only-%s:%#x' % (o, v))
            else:
                add('i', W_IMM_SMALL, 'i-only-%s:%#x' % (o, v))
    for o, a, b in (('exe', E['disp'], P['disp']), ('port', P['disp'], E['disp'])):
        for v in sorted(set(a) - set(b)):
            add('D', W_DISP, 'D-only-%s:%#x' % (o, v))
    d = abs(E['jcc'] - P['jcc'])
    if d > max(4, 0.35 * max(E['jcc'], P['jcc'])):
        add('J', 1.5, 'J:%d/%d' % (E['jcc'], P['jcc']))
    if E['n'] and P['n'] and (P['n'] > 2.5 * E['n'] + 10 or E['n'] > 2.5 * P['n'] + 10):
        add('N', 1.0, 'N:%d/%d' % (E['n'], P['n']))
    return sum(min(v, CAP[t]) for t, v in tot.items()), sg


def port_sig(ctx, addr):
    """best port candidate for the exe function at addr (min score) -> (score, signals, E, P) or (None, reason)."""
    names = ctx.exe_name.get(addr)
    if not names:
        return None, 'no-site'
    E = ctx.sig('exe', addr)
    best = None
    for q in sorted(names):
        for va in ctx.q2va.get(q, []):
            P = ctx.sig('port', va)
            r = compare(E, P)
            if best is None or r[0] < best[0]:
                best = (r[0], r[1], E, P)
    if best is None:
        return None, 'no-port-symbol'
    return best


def fmt_sig(S):
    return 'calls=%s vcalls=%s flt=%s imm=%s disp=%s jcc=%d n=%d x=%d' % (
        ['::'.join(k) + ('x%d' % v if v > 1 else '') for k, v in sorted(S['call'].items())],
        sorted('%#x' % k for k in S['vcall']), sorted(S['flt']), sorted('%#x' % k for k in S['imm']),
        sorted('%#x' % k for k in S['disp']), S['jcc'], S['n'], S['x'])


# ---------------- annotator ----------------
def annot(ctx, addr, chunk=None):
    pe = ctx.exe
    ins = extent(pe, addr)
    targets = {imm_target(i) for i in ins if (is_jcc(i) or i.mnemonic == 'jmp') and imm_target(i) is not None}
    labels = {a: 'L%d' % k for k, a in enumerate(sorted(a for a in targets if any(j.address == a for j in ins)))}
    retn = {}

    def callee_ret(t):
        if t not in retn:
            retn[t] = 0
            for j in extent(pe, t, 120):
                if j.mnemonic == 'ret' and j.operands:
                    retn[t] = j.operands[0].imm
                    break
        return retn[t]
    D, ebp_at, lines = 0, None, []
    unk = False
    for i in ins:
        mn, op = i.mnemonic, i.op_str
        note = []
        regs = {X.X86_REG_ESP}
        for o in i.operands:
            if o.type == X.X86_OP_MEM:
                m = o.mem
                if m.base == X.X86_REG_ESP and not m.index and not unk:
                    x = m.disp - D
                    note.append(('arg%d' % ((x - 4) // 4)) if x >= 4 else ('loc%X' % -x) if x < 0 else 'retaddr')
                elif m.base == X.X86_REG_EBP and not m.index and ebp_at is not None:
                    x = m.disp - ebp_at
                    note.append(('arg%d' % ((x - 4) // 4)) if x >= 4 else ('loc%X' % -x))
                elif m.base == 0 and not m.index:
                    a = m.disp & 0xFFFFFFFF
                    if pe.is_ro(a) and FLOAT_MN.match(mn):
                        b = pe.rd(a, 8 if o.size == 8 else 4)
                        if len(b) >= 4:
                            note.append('const=%r' % (struct.unpack('<d' if o.size == 8 else '<f', b)[0] if len(b) == o.size else b.hex()))
                    elif pe.is_ro(a) and o.size == 4 and mn in ('mov', 'cmp', 'add', 'sub', 'push'):
                        b = pe.rd(a, 4)
                        if len(b) == 4:
                            note.append('i=%d f=%.6g' % (struct.unpack('<i', b)[0], struct.unpack('<f', b)[0]))
                    else:
                        note.append('g_%X' % a)
        t = imm_target(i)
        if mn == 'call' and t is not None:
            nm = ctx.exe_name.get(t)
            note.append(('->' + '::'.join(sorted(nm)[0])) if nm else '->sub_%X' % t)
            D -= callee_ret(t)
        elif (is_jcc(i) or mn == 'jmp') and t is not None:
            note.append('->' + labels.get(t, '%X' % t))
        # stack depth bookkeeping
        if mn == 'push':
            D += 4
        elif mn == 'pop':
            D -= 4
        elif mn == 'pushal' or mn == 'pushad':
            D += 32
        elif mn in ('sub', 'add') and i.operands and i.operands[0].type == X.X86_OP_REG and i.operands[0].reg == X.X86_REG_ESP:
            if i.operands[1].type == X.X86_OP_IMM:
                D += i.operands[1].imm if mn == 'sub' else -i.operands[1].imm
            else:
                unk = True
        elif mn == 'mov' and op == 'ebp, esp':
            ebp_at = -D
        elif mn == 'leave':
            D = -(ebp_at or 0) - 4 if ebp_at is not None else D
        lab = (labels[i.address] + ':') if i.address in labels else ''
        lines.append('%-4s%X  %-6s %s%s%s' % (lab, i.address, mn, op, ('   ; ' + ' '.join(note)) if note else '', ('   [D=%d]' % D) if mn in ('push', 'pop', 'call') and False else ''))
    # chunks of basic blocks
    CH, chunks, cur = 150, [], []
    for ln, i in zip(lines, ins):
        cur.append(ln)
        if len(cur) >= 100 and (i.mnemonic in ('ret', 'jmp') or is_jcc(i)) and len(lines) > CH:
            chunks.append(cur); cur = []
    if cur:
        chunks.append(cur)
    print('; ==== %#x %s  (%d insns, %d chunk(s); stack depth %s)' % (addr, ' / '.join('::'.join(q) for q in sorted(ctx.exe_name.get(addr, []))), len(ins), len(chunks), 'unknown at some point' if unk else 'tracked'))
    for k, c in enumerate(chunks):
        if chunk is not None and k != chunk:
            continue
        if len(chunks) > 1:
            print('; ---- chunk %d/%d' % (k, len(chunks) - 1))
        print('\n'.join(c))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['annot', 'diff', 'batch'])
    ap.add_argument('arg')
    ap.add_argument('--exe', default=EXE_DEFAULT)
    ap.add_argument('--port', default=PORT_DEFAULT)
    ap.add_argument('--map', default=None)
    ap.add_argument('--repo', default='.')
    ap.add_argument('--chunk', type=int)
    ap.add_argument('-o', '--out')
    ap.add_argument('--threshold', type=float, default=2.0, help='AUTO_MATCH below this score')
    a = ap.parse_args()
    mp = a.map or os.path.splitext(a.port)[0] + '.map'
    ctx = Ctx(a.repo, a.exe, a.port if a.cmd != 'annot' else None, mp if a.cmd != 'annot' else None)
    if a.cmd == 'annot':
        annot(ctx, int(a.arg, 16), a.chunk)
    elif a.cmd == 'diff':
        addr = int(a.arg, 16)
        r = port_sig(ctx, addr)
        if r[0] is None:
            print('no result:', r[1]); return
        print('exe :', fmt_sig(r[2])); print('port:', fmt_sig(r[3]))
        print('score %.1f  %s  %s' % (r[0], 'AUTO_MATCH' if r[0] < a.threshold else 'REVIEW', ' '.join(r[1])))
    else:
        rows = [l.rstrip('\n').split('\t') for l in open(a.arg, encoding='utf-8')][1:]
        out = []
        for k, r in enumerate(rows):
            addr = int(r[0], 16)
            try:
                res = port_sig(ctx, addr)
            except Exception as e:  # noqa
                res = (None, 'error:%s' % e)
            if res[0] is None:
                out.append((r[0], r[1], '', 'NO_DATA', res[1]))
            else:
                out.append((r[0], r[1], '%.1f' % res[0], 'AUTO_MATCH' if res[0] < a.threshold else 'REVIEW', ' '.join(res[1])))
            if k % 500 == 0:
                print(k, len(rows), file=sys.stderr)
        out.sort(key=lambda x: -float(x[2] or -1))
        with open(a.out, 'w', encoding='utf-8') as f:
            f.write('addr\tfunction\tscore\ttriage\tsignals\n')
            for o in out:
                f.write('\t'.join(o) + '\n')
        c = collections.Counter(o[3] for o in out)
        print(dict(c))


if __name__ == '__main__':
    main()
