"""cmpscan.py - HC11 compare-strictness / NaN scan.
For every function of a hitcount tsv (exe_addr, function, count, file.obj, ...) with an exe address:
 (a) port sites of std::min/max/clamp/Clamp/fminf/fmaxf (+ fmin/fmax) in the function body (source/**.cpp),
 (b) exe x87/SSE compare idioms decoded: fcom*/fucom* + fnstsw + test ah,M / sahf + jcc, or fcomi + jcc ->
     'jump taken when  st0 <op> operand' incl. NaN (UN) behaviour.
usage: py -I cmpscan.py <hitcount.tsv> <exe> <repo> [mincount] [maxfuncs] > out.tsv
cols: count, function, exe_addr, port_file:line, port_sites (min/max/clamp exprs), exe_idioms (addr: cmp => cond incl. +NaN)"""
import subprocess, re, sys, csv, os, glob
tsv, exe, repo = sys.argv[1:4]
mn = int(sys.argv[4]) if len(sys.argv) > 4 else 1
mx = int(sys.argv[5]) if len(sys.argv) > 5 else 10**9
PY = sys.executable
SITES_ONLY = os.environ.get('CMPSCAN_SITES_ONLY') == '1'
skip = re.compile(r'Audio|Hud|HUD|Font|Sprite|Render|Draw|Shadow|Corona|Fx|Particle|Skidmark|Vector::|notsa::|Scene|Text|Menu|Widget|Radar|Billboard|Timecycle|Sky|Clouds|PostEffects', re.I)
OUT = ['LT', 'EQ', 'GT', 'UN']
FL = {'LT': (1, 0, 0), 'EQ': (0, 0, 1), 'GT': (0, 0, 0), 'UN': (1, 1, 1)}  # C0,C2,C3


def par(x):
    return bin(x & 0xff).count('1') % 2 == 0


def jcc_taken(j, zf, pf, cf):
    return {'je': zf, 'jz': zf, 'jne': not zf, 'jnz': not zf, 'jp': pf, 'jpe': pf, 'jnp': not pf, 'jpo': not pf,
            'ja': not cf and not zf, 'jae': not cf, 'jb': cf, 'jbe': cf or zf, 'jnb': not cf, 'jnbe': not cf and not zf,
            'jc': cf, 'jnc': not cf, 'jna': cf or zf, 'jnae': cf}.get(j)


def name(s):
    s = frozenset(s)
    tab = {frozenset(['LT']): '<', frozenset(['LT', 'EQ']): '<=', frozenset(['GT']): '>', frozenset(['GT', 'EQ']): '>=',
           frozenset(['EQ']): '==', frozenset(['LT', 'GT']): '!=', frozenset(['LT', 'GT', 'EQ']): 'any-ordered',
           frozenset(): 'never'}
    b = frozenset(x for x in s if x != 'UN')
    n = tab.get(b, '|'.join(sorted(b)))
    return n + ('+NaN' if 'UN' in s else '')


def ins(l):
    m = re.match(r'([0-9a-f]{8})\s+(.*)', l)
    return (m.group(1), re.sub(r'\s+', ' ', m.group(2).split(';')[0].strip())) if m else None


def decode(L):
    res = []
    for i, (a, t) in enumerate(L):
        mnem = t.split(' ')[0]
        if not re.match(r'(fcom|fucom|ficom)', mnem):
            continue
        op = t.split(' ', 1)[1] if ' ' in t else 'st(1)'
        ctx = [x[1] for x in L[max(0, i - 4):i] if x[1].startswith(('fld', 'fmul', 'fsub', 'fadd', 'fdiv', 'fabs', 'fchs'))][-2:]
        cmpdesc = '[%s] %s %s' % (' ; '.join(ctx), mnem, op)
        if mnem.startswith(('fcomi', 'fucomi')):
            j = next((x for x in L[i + 1:i + 4] if x[1].startswith('j')), None)
            if j:
                jm = j[1].split(' ')[0]
                tab = {'LT': (0, 0, 1), 'EQ': (1, 0, 0), 'GT': (0, 0, 0), 'UN': (1, 1, 1)}  # zf,pf,cf
                tk = [k for k in OUT if jcc_taken(jm, *tab[k])]
                res.append((a, cmpdesc, 'jump(%s) if st0 %s op' % (jm, name(tk))))
            continue
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
        if m and j:
            mask = int(m.group(1), 0)
            jm = j[1].split(' ')[0]
            tk = []
            for kk in OUT:
                c0, c2, c3 = FL[kk]
                r = (c0 * 1 + c2 * 4 + c3 * 0x40) & mask
                if jcc_taken(jm, r == 0, par(r), False):
                    tk.append(kk)
            res.append((a, cmpdesc, 'test ah,%#x; %s -> taken if st0 %s op' % (mask, jm, name(tk))))
        elif t0 == 'sahf' and j:
            jm = j[1].split(' ')[0]
            tk = []
            for kk in OUT:
                c0, c2, c3 = FL[kk]
                if jcc_taken(jm, bool(c3), bool(c2), bool(c0)):
                    tk.append(kk)
            res.append((a, cmpdesc, 'sahf; %s -> taken if st0 %s op' % (jm, name(tk))))
        else:
            res.append((a, cmpdesc, '?? ' + t0 + ' ' + (nxt[1][1] if len(nxt) > 1 else '')))
    for i, (a, t) in enumerate(L):
        if re.match(r'(comiss|ucomiss|comisd|ucomisd) ', t):
            j = next((x for x in L[i + 1:i + 3] if x[1].startswith('j')), None)
            if j:
                jm = j[1].split(' ')[0]
                tab = {'LT': (0, 0, 1), 'EQ': (1, 0, 0), 'GT': (0, 0, 0), 'UN': (1, 1, 1)}
                tk = [k for k in OUT if jcc_taken(jm, *tab[k])]
                res.append((a, t, 'jump(%s) if a %s b' % (jm, name(tk))))
    return res


def strip_cm(s):
    s = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), s, flags=re.S)
    return re.sub(r'//[^\n]*', '', s)


src_idx = {}
for f in glob.glob(os.path.join(repo, 'source', '**', '*.cpp'), recursive=True):
    src_idx.setdefault(os.path.basename(f), []).append(f)
_cache = {}


def srctext(f):
    if f not in _cache:
        _cache[f] = strip_cm(open(f, encoding='utf8', errors='replace').read())
    return _cache[f]


SITE = re.compile(r'\b(std::min|std::max|std::clamp|std::fmin|std::fmax|fminf|fmaxf|fmin|fmax|Clamp|clamp|std::ranges::clamp|min|max|Min|Max|Limit)\s*\(')


def port_body(func, objname):
    cpp = objname.replace('.obj', '')
    if '::' not in func:
        return None
    cands = src_idx.get(cpp, [])
    short = func.split('::')[-1]
    cls = func.rsplit('::', 1)[0].split('::')[-1]
    pat = re.compile(r'^[\w:<>\*&\s,]*\b' + re.escape(cls) + r'::' + re.escape(short) + r'\s*\(', re.M)
    for f in cands + [x for v in src_idx.values() for x in v if x not in cands]:
        t = srctext(f)
        for m in pat.finditer(t):
            p = t.find('{', m.end())
            semi = t.find(';', m.end())
            if p < 0 or (0 <= semi < p):
                continue
            d = 0
            q = p
            while q < len(t):
                if t[q] == '{':
                    d += 1
                elif t[q] == '}':
                    d -= 1
                    if d == 0:
                        break
                q += 1
            return f, t.count('\n', 0, p) + 1, t[p:q + 1]
    return None


if __name__ == '__main__':
    rows = [r for r in csv.DictReader(open(tsv, encoding='utf8'), delimiter='\t') if r['exe_addr'] != '-' and int(r['count']) >= mn]
    rows.sort(key=lambda r: -int(r['count']))
    n = 0
    print('count\tfunction\texe_addr\tport_file:line\tport_sites\texe_idioms')
    seen = set()
    for r in rows:
        if skip.search(r['function']) or r['function'] in seen:
            continue
        seen.add(r['function'])
        if n >= mx:
            break
        n += 1
        pb = port_body(r['function'], r['file'])
        sites = []
        loc = '-'
        if pb:
            f, line, body = pb
            loc = '%s:%d' % (os.path.relpath(f, repo).replace(chr(92), '/'), line)
            for i, ln in enumerate(body.split(chr(10))):
                if SITE.search(ln):
                    sites.append('L%d: %s' % (line + i, ln.strip()[:140]))
        idi = []
        if sites or not SITES_ONLY:  # CMPSCAN_SITES_ONLY=1: disassemble only functions with min/max/clamp sites (fast)
            for a in r['exe_addr'].split('|'):
                out = subprocess.run([PY, '-X', 'utf8', '-I', 'tools/standalone/exedis.py', a, '--exe', exe, '--repo', repo, '--max', '6000'],
                                     capture_output=True, text=True, encoding='utf8', cwd=repo).stdout.splitlines()
                L2 = [x for x in (ins(l) for l in out) if x]
                for (ad, c, d) in decode(L2):
                    idi.append('%s: %s => %s' % (ad, c, d))
        print('\t'.join([r['count'], r['function'], r['exe_addr'], loc, ' || '.join(sites) or '-', ' || '.join(idi) or '-']), flush=True)
