"""cmpscan2.py - HC12: pair EVERY float compare of a port function with the exe x87 compare idioms (strictness + NaN).
usage: py -I cmpscan2.py <hitcount.tsv> <exe> <repo> <out_prefix> [--funcs f1,f2 | --top N] [--min-count N]
Port side: compares (< <= > >= == !=) in the function body (comments stripped) where an operand has a float literal or
looks like a float (hungarian m_f*, declared float/double, .x/.y/.z ...). Exe side: cmpscan.decode-style idioms + the constant
(.rdata float via dword ptr [addr], fldz/fld1, ftst) and the operand order when determinable.
Pairing: a port compare with a literal is matched to the exe idioms using the same constant; OK if some (order, polarity)
reproduces the exact jump set incl. NaN. Classes: EQ (ordered outcome sets differ = strictness) / NAN (only UN differs) / NOPAIR.
Port compares without a literal: PORTVAR rows, next to unconsumed exe idioms (EXEVAR) for hand review.
out: <prefix>.tsv"""
import sys, re, os, csv, struct, subprocess, importlib.util

here = os.path.dirname(os.path.abspath(__file__))
args = sys.argv[1:]
tsv, exe, repo, outp = args[:4]
opts = args[4:]
FUNCS = None
TOP = 40
MINC = 1
i = 0
while i < len(opts):
    if opts[i] == '--funcs':
        FUNCS = opts[i + 1].split(',')
        i += 2
    elif opts[i] == '--top':
        TOP = int(opts[i + 1])
        i += 2
    elif opts[i] == '--min-count':
        MINC = int(opts[i + 1])
        i += 2
    else:
        i += 1
# reuse cmpscan helpers (it reads sys.argv at import; main is guarded)
sys.argv = ['cmpscan', tsv, exe, repo]
spec = importlib.util.spec_from_file_location('cmpscan', os.path.join(here, 'cmpscan.py'))
cs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cs)
PY = sys.executable
d = open(exe, 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
nsec = struct.unpack_from('<H', d, pe + 6)[0]
osz = struct.unpack_from('<H', d, pe + 20)[0]
base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
so = pe + 24 + osz
secs = []
for k in range(nsec):
    n, vs, va, rs, rp = struct.unpack_from('<8sIIII', d, so + 40 * k)
    secs.append((va + base, max(vs, rs), rp))


def rd(x, n):
    for va, sz, rp in secs:
        if va <= x < va + sz:
            return d[rp + x - va:rp + x - va + n]
    return b''


def fconst(addr, dbl):
    b = rd(addr, 8 if dbl else 4)
    if len(b) < (8 if dbl else 4):
        return None
    return struct.unpack('<d' if dbl else '<f', b)[0]


OUT = cs.OUT
FL = cs.FL
ALL = frozenset(OUT)


def ordered(s):
    return frozenset(x for x in s if x != 'UN')


def swap(s):
    return frozenset({'LT': 'GT', 'GT': 'LT'}.get(x, x) for x in s)


OPSET = {'<': {'LT'}, '<=': {'LT', 'EQ'}, '>': {'GT'}, '>=': {'GT', 'EQ'}, '==': {'EQ'}, '!=': {'LT', 'GT', 'UN'}}


def tset(op):
    return frozenset(OPSET[op])


def jset(jm, mask):
    tk = []
    for kk in OUT:
        c0, c2, c3 = FL[kk]
        r = (c0 + c2 * 4 + c3 * 0x40) & mask
        if cs.jcc_taken(jm, r == 0, cs.par(r), False):
            tk.append(kk)
    return frozenset(tk)


def exe_idioms(L):
    res = []
    for i, (a, t) in enumerate(L):
        mn = t.split(' ')[0]
        if not re.match(r'(fcom|fucom|ficom|ftst)', mn) or mn.startswith(('fcomi', 'fucomi')):
            continue
        op = t.split(' ', 1)[1] if ' ' in t else ''
        consts = []
        pos = None
        dbl = 'qword' in op
        m = re.search(r'ptr \[(0x[0-9a-f]+)\]', op)
        if mn == 'ftst':
            consts.append(0.0)
            pos = 'op'
        elif m and not mn.startswith('ficom'):
            c = fconst(int(m.group(1), 16), dbl)
            if c is not None:
                consts.append(c)
                pos = 'op'
        if pos is None:
            for (pa, pt) in reversed(L[max(0, i - 3):i]):
                if pt.startswith('fldz'):
                    consts.append(0.0); pos = 'st0'; break
                if pt.startswith('fld1'):
                    consts.append(1.0); pos = 'st0'; break
                m2 = re.match(r'fld (dword|qword) ptr \[(0x[0-9a-f]+)\]', pt)
                if m2:
                    c = fconst(int(m2.group(2), 16), m2.group(1) == 'qword')
                    if c is not None:
                        consts.append(c); pos = 'st0'
                    break
                if pt.startswith('fld'):
                    break
        k = i + 1
        while k < min(len(L), i + 6) and not L[k][1].startswith('fnstsw'):
            k += 1
        if k >= min(len(L), i + 6):
            continue
        k += 1
        nxt = L[k:k + 4]
        if not nxt:
            continue
        t0 = nxt[0][1]
        m = re.match(r'test ah, (0x[0-9a-f]+|\d+)', t0)
        j = next((x for x in nxt[1:3] if x[1].startswith('j')), None)
        js = None
        if m and j:
            js = jset(j[1].split(' ')[0], int(m.group(1), 0))
        elif t0 == 'sahf' and j:
            jm = j[1].split(' ')[0]
            tk = []
            for kk in OUT:
                c0, c2, c3 = FL[kk]
                if cs.jcc_taken(jm, bool(c3), bool(c2), bool(c0)):
                    tk.append(kk)
            js = frozenset(tk)
        if js is None:
            continue
        res.append(dict(addr=a, js=js, consts=consts, pos=pos, text='%s %s; %s -> %s' % (mn, op, j[1], cs.name(js))))
    return res


def consistent(js, T, pos):
    orders = []
    if pos in ('op', None):
        orders.append(('st0=var', lambda s: s))
    if pos in ('st0', None):
        orders.append(('st0=K', swap))
    best = None
    for oname, f in orders:
        J = f(js)
        for pol, TT in (('jump=cond', T), ('jump=!cond', ALL - T)):
            if J == TT:
                return 'OK', oname + ' ' + pol
            if ordered(J) == ordered(TT):
                best = best or ('NAN', oname + ' ' + pol)
    return best or ('EQ', '')


FLOATDECL = re.compile(r'\b(?:float|double|CVector|CVector2D)\s+(?:&\s*)?(\w+)')
LIT = r'-?\d+\.\d*(?:[eE][-+]?\d+)?f?|-?\.\d+f?|-?\d+[eE][-+]?\d+f?|-?\d+f'
CMP = re.compile(r'(\w[\w\.\[\]]*|\))\s*(<=|>=|==|!=|<(?![<=])|>(?![>=]))\s*(-?[\w\.]+|\([^()]*\))')
HINT = re.compile(r'(^|\.|::|>)(m_f|f[A-Z]|m_vec|m_v)|\.[xyz]$|^(speed|dist|time|timer|angle|dot|mag|len|height|scale|alpha|ratio)', re.I)


def port_float_cmps(body, line0):
    out = []
    decl = set(FLOATDECL.findall(body))
    isl = lambda x: re.fullmatch(LIT, x.strip('()')) is not None
    isf = lambda x: bool(HINT.search(x)) or x.split('.')[0].split('[')[0] in decl
    for li, s in enumerate(body.split('\n')):
        if not re.search(r'\b(if|while|for|return)\b|\?|&&|\|\||[=(]\s*\w', s):
            continue
        for m in CMP.finditer(s):
            a, op, b = m.group(1), m.group(2), m.group(3)
            lit = None
            var = None
            flip = False
            if isl(b):
                lit, var = b.strip('()'), a
            elif isl(a):
                lit, var, flip = a, b, True
            if lit is None:
                if not (isf(a) or isf(b)):
                    continue
            elif not re.search(r'[.eE]|f$', lit) and not isf(var):
                continue
            if lit is not None and flip:
                op = {'<': '>', '>': '<', '<=': '>=', '>=': '<=', '==': '==', '!=': '!='}[op]
            out.append(dict(line=line0 + li, op=op, lit=(float(lit.rstrip('f')) if lit else None), text=s.strip()[:120]))
    return out


def main():
    rows = [r for r in csv.DictReader(open(tsv, encoding='utf8'), delimiter='\t') if r['exe_addr'] != '-' and int(r['count']) >= MINC]
    by = {}
    for r in rows:
        by.setdefault(r['function'], r)
    names = FUNCS if FUNCS else [r['function'] for r in sorted(by.values(), key=lambda r: -int(r['count']))][:TOP]
    fo = open(outp + '.tsv', 'w', encoding='utf8')
    fo.write('function\texe_addr\tport_file:line\tkind\tport_op\tconst\texe_idiom\tclass\tnote\n')
    for fn in names:
        r = by.get(fn)
        if not r:
            continue
        pb = cs.port_body(fn, r['file'])
        if not pb:
            fo.write('%s\t%s\t-\tNOSRC\t\t\t\t\t\n' % (fn, r['exe_addr']))
            continue
        f, line, body = pb
        rel = os.path.relpath(f, repo).replace('\\', '/')
        pc = port_float_cmps(body, line)
        idi = []
        for a in r['exe_addr'].split('|'):
            out = subprocess.run([PY, '-X', 'utf8', '-I', 'tools/standalone/exedis.py', a, '--exe', exe, '--repo', repo, '--max', '8000'],
                                 capture_output=True, text=True, encoding='utf8', cwd=repo).stdout.splitlines()
            idi += exe_idioms([x for x in (cs.ins(l) for l in out) if x])
        used = set()
        ptr = 0
        WIN = 8
        for p in pc:
            T = tset(p['op'])
            cls = None
            ex = None
            if p['lit'] is not None:
                cands = [x for x in idi if any(abs(c - p['lit']) < 1e-6 * max(1, abs(p['lit'])) for c in x['consts'])]
                res = [(consistent(x['js'], T, x['pos']), x) for x in cands]
                ok = [z for z in res if z[0][0] == 'OK']
                if ok:
                    cls, ex = 'OK', ok[0][1]
                elif res:
                    nn = [z for z in res if z[0][0] == 'NAN']
                    z = nn[0] if nn else res[0]
                    cls, ex = z[0][0], z[1]
            if cls is None or cls == 'OK':
                if ex is not None:
                    ptr = max(ptr, idi.index(ex) + 1)
            if cls is None:
                # sequential alignment on ordered outcome sets (no constant info)
                for kk in range(ptr, min(len(idi), ptr + WIN)):
                    x = idi[kk]
                    c = consistent(x['js'], T, None)
                    if c[0] in ('OK', 'NAN'):
                        cls, ex = 'SEQ-' + c[0], x
                        ptr = kk + 1
                        break
                if cls is None:
                    cls = 'SEQ-NOMATCH'
            if ex is not None:
                used.add(ex['addr'])
            fo.write('%s\t%s\t%s:%d\t%s\t%s\t%s\t%s\t%s\t%s\n' % (fn, r['exe_addr'], rel, p['line'], 'PORTLIT' if p['lit'] is not None else 'PORTVAR', p['op'], ('%g' % p['lit']) if p['lit'] is not None else '', ('%s: %s' % (ex['addr'], ex['text'])) if ex else '', cls, p['text']))
        for x in idi:
            if x['addr'] not in used:
                fo.write('%s\t%s\t-\tEXEVAR\t\t%s\t%s: %s\tREVIEW\t\n' % (fn, r['exe_addr'], ','.join('%g' % c for c in x['consts']), x['addr'], x['text']))
        fo.flush()


main()
