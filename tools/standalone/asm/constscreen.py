"""Screen exe float/double constants against port literals, per function.

For every row of a hitcount tsv (exe_addr, function, count, file.obj, verdict) with an exe address,
disassemble the exe function, collect the .rdata constants it loads (fld/fmul/fadd/fsub/fcomp [mem]: dword = float,
qword = double) and compare them with the literals in the port function body:
  float literals (1.5f), bit_cast<float>(0x...), double literals (1.5), and named constants (constexpr/const/#define of a simple
  numeric expression defined in the same .cpp or in any header under --src and source/*.h), whose identifiers appear in the body.
Prints functions where an exe constant is within a few ulp (float, --ulp) or a small relative error (double, --drel) of,
but not equal to, a port literal. A port float literal counts as its widened double for the double screen.
Exe constants with no port counterpart at all are only counted: --missing FILE writes "addr<TAB>func<TAB>n_float<TAB>n_double".
Usage: py -X utf8 -I constscreen.py <tsv> [--exe EXE] [--src source/game_sa] [--ulp 8] [--drel 1e-6] [--skip regex] [--missing FILE]
"""
import re, struct, sys, glob, os, argparse, math
import capstone

ap = argparse.ArgumentParser()
ap.add_argument('tsv')
ap.add_argument('--exe', default='gta_sa_compact.exe')
ap.add_argument('--src', default='source/game_sa')
ap.add_argument('--ulp', type=int, default=8)
ap.add_argument('--drel', type=float, default=1e-6)
ap.add_argument('--missing', default='')
ap.add_argument('--follow', type=int, default=2, help='follow calls in the port body into the callees port bodies (levels; split helpers)')
ap.add_argument('--detail', default='', help='write per-constant missing list: addr func kind constaddr value')
ap.add_argument('--skip', default=r'Render|HUD|Font|Sprite|Audio|AE|Draw|Shadow|Coronas|Fx|Particle')
a = ap.parse_args()
d = open(a.exe, 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
n = struct.unpack_from('<H', d, pe + 6)[0]; so = struct.unpack_from('<H', d, pe + 20)[0]
base = struct.unpack_from('<I', d, pe + 52)[0]
secs = []
for i in range(n):
    o = pe + 24 + so + i * 40
    vs, va, rs, ra = struct.unpack_from('<IIII', d, o + 8)
    secs.append((va + base, max(vs, rs), ra))
def rd(x, k):
    for va, sz, ra in secs:
        if va <= x < va + sz: return d[ra + x - va: ra + x - va + k]
    return b''
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
FOP = r'f(?:ld|mul|add|sub|subr|div|divr|comp|com|ucom)'

def exe_consts(start, maxn=3000):
    """-> (set of dword-float const addrs, set of qword-double const addrs)"""
    pc = start; far = start; outf = set(); outd = set()
    for _ in range(maxn):
        ins = list(md.disasm(rd(pc, 16), pc, 1))
        if not ins: break
        i = ins[0]
        m = re.match(r'^' + FOP + r'\s+(dword|qword) ptr \[(0x[0-9a-f]+)\]$', i.mnemonic + ' ' + i.op_str)
        if m and int(m.group(2), 16) >= base + 0x450000:
            (outf if m.group(1) == 'dword' else outd).add(int(m.group(2), 16))
        if i.mnemonic[0] == 'j':
            mm = re.match(r'^0x([0-9a-f]+)$', i.op_str)
            if mm:
                t = int(mm.group(1), 16)
                if start <= t < start + 0x6000 and t > far: far = t
        pc += i.size
        if i.mnemonic == 'ret' and pc > far: break
        if i.mnemonic == 'jmp' and pc > far and i.op_str.startswith('0x'): break
    return outf, outd

# ---- named constants: name -> set of values (a name may be defined differently in different files) ----
PI = 3.14159265358979323846
def f32(x): return struct.unpack('<f', struct.pack('<f', x))[0]
def eval_const(e, env):
    e = e.strip()
    e = re.sub(r'std::numbers::pi_v<float>|std::numbers::pi_v<double>|std::numbers::pi\b|\bM_PI\b', repr(PI), e)
    e = re.sub(r'(?:std::)?bit_cast<float>\(\s*(0[xX][0-9A-Fa-f]+)[uU]?\s*\)', lambda m: repr(struct.unpack('<f', struct.pack('<I', int(m.group(1), 16)))[0]), e)
    e = re.sub(r'\((?:float|double|int|x87d|int32|float32)\)', '', e)
    e = re.sub(r'\b(?:static_cast<\w+>|float|double)\s*\(', '(', e)
    e = re.sub(r'(?<![\w.])(\d+\.?\d*(?:[eE][-+]?\d+)?|\.\d+)[fFuUlL]*\b', r'\1', e)
    for k in sorted(env, key=len, reverse=True):
        if re.search(r'\b' + re.escape(k) + r'\b', e): e = re.sub(r'\b' + re.escape(k) + r'\b', '(' + repr(env[k]) + ')', e)
    if not re.fullmatch(r'[-+*/().\d eE]+', e): return None
    try: return float(eval(e, {'__builtins__': {}}))
    except Exception: return None
DEFRE = [re.compile(r'\b(?:constexpr|const)\s+(?:static\s+)?(?:inline\s+)?(?:float|double|auto|x87d|int|int32|float32)\s+(\w+)\s*(?:=|\{)\s*([^;{}]+?)\s*\}?\s*;'),
         re.compile(r'^[ \t]*#define\s+(\w+)\s+([-+*/().\w ]+?)[ \t]*$', re.M)]
def defs_of(text):
    out = []
    for r in DEFRE:
        for m in r.finditer(text): out.append((m.group(1), m.group(2)))
    return out
def resolve(defs):
    env = {}; multi = {}
    for _ in range(3):
        for k, e in defs:
            v = eval_const(e, env)
            if v is not None: env[k] = v; multi.setdefault(k, set()).add(v)
    return multi
hdr_defs = []
for pth in glob.glob(a.src + '/**/*.h', recursive=True) + glob.glob('source/*.h'):
    try: hdr_defs += defs_of(open(pth, encoding='utf8', errors='ignore').read())
    except Exception: pass
HDR_NAMED = resolve(hdr_defs)
# trivial constexpr helpers `T Name(T p) { return p * K; }` / `return K * p;` / `p / K`: a call to Name(...) contributes K
HELPERS = {}
for pth in glob.glob(a.src + '/**/*.h', recursive=True) + glob.glob('source/*.h'):
    t = open(pth, encoding='utf8', errors='ignore').read()
    for m in re.finditer(r'(?:constexpr|inline)\s+(?:float|double|x87d)\s+(\w+)\s*\(\s*(?:const\s+)?(?:float|double)\s+(\w+)\s*\)\s*\{\s*return\s+([^;]+);', t):
        k = re.sub(r'' + m.group(2) + r'', ' ', m.group(3)).replace('*', ' ').replace('/', ' ').split()
        if len(k) == 1 and k[0] in HDR_NAMED: HELPERS[m.group(1)] = HDR_NAMED[k[0]]
print('header named constants:', len(HDR_NAMED), file=sys.stderr)
file_named = {}
def named_for(path):
    if path not in file_named:
        m = resolve(defs_of(open(path, encoding='utf8', errors='ignore').read()))
        for k, v in HDR_NAMED.items(): m.setdefault(k, set()).update(v)
        file_named[path] = m
    return file_named[path]

_CC = {}
def collect(body, bpath):
    k = (hash(body), bpath)
    if k not in _CC: _CC[k] = _collect(body, bpath)
    return _CC[k]
def _collect(body, bpath):
    flits = set(); dlits = set()
    for m in re.finditer(r'(?<![\w.])(\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?f\b', body):
        try: v = float(m.group(0)[:-1]); flits.add(fbits(v)); dlits.add(f32(v))
        except Exception: pass
    for m in re.finditer(r'bit_cast<float>\((0x[0-9A-Fa-f]+)', body):
        b = int(m.group(1), 16); flits.add(b); dlits.add(struct.unpack('<f', struct.pack('<I', b))[0])
    for m in re.finditer(r'(?<![\w.])(\d+\.\d+(?:[eE][-+]?\d+)?|\d+[eE][-+]?\d+|\.\d+)(?![\w.])', body):
        try: v = float(m.group(0)); dlits.add(v); flits.add(fbits(v))   # a double literal narrowed to float is a float literal too
        except Exception: pass
    for hn, vals in HELPERS.items():
        if re.search(r'(?<![\w.:])' + re.escape(hn) + r'\s*\(', body):
            for v in vals: flits.add(fbits(v)); dlits.add(v); dlits.add(f32(v))
    for k, vals in named_for(bpath).items():
        if re.search(r'(?<![\w.:])' + re.escape(k) + r'\b', body):
            for v in vals:
                try: flits.add(fbits(v)); dlits.add(v); dlits.add(f32(v))
                except Exception: pass
    return flits, dlits

src = {}
for p in glob.glob(a.src + '/**/*.cpp', recursive=True):
    src.setdefault(os.path.basename(p) + '.obj', []).append(p)
FUNCS = {}
if a.follow:
    HEAD = re.compile(r'^[A-Za-z_][^;{}#=\n]*?\b(\w+)\s*\(')
    for ps in src.values():
        for pth in ps:
            t = open(pth, encoding='utf8', errors='ignore').read()
            for m in re.finditer(r'^[A-Za-z_][^\n;{}#=]*?\b(\w+)\s*\([^\n;{}]*(?:\n[^\n;{}]*)?\)[^\n;{}]*\n?\{', t, re.M):
                if m.group(1) in ('if', 'for', 'while', 'switch', 'return'): continue
                e = t.find('\n}', m.end())
                if e < 0: continue
                FUNCS.setdefault(m.group(1), []).append((re.sub(r'//[^\n]*|/\*.*?\*/', '', t[m.start():e + 2], flags=re.S), pth))
    print('indexed callee names:', len(FUNCS), file=sys.stderr)
fbits = lambda x: struct.unpack('<I', struct.pack('<f', x))[0]
rows = []
for l in open(a.tsv, encoding='utf8').read().splitlines()[1:]:
    c = l.split('\t')
    if len(c) >= 5 and c[0] != '-' and not re.search(a.skip, c[1] + c[3]): rows.append((int(c[2]), c[0], c[1], c[3], c[4]))
rows.sort(reverse=True)
nos = 0; scr = 0; miss = []; det = []
for cnt, addr, fn, obj, verdict in rows:
    body = None; bpath = None
    for p in src.get(obj, []):
        t = open(p, encoding='utf8', errors='ignore').read()
        m = re.search(r'\n[^\n;{}]*\b' + re.escape(fn) + r'\s*\([^;{]*\)[^;{]*\{.*?\n\}', t, re.S)
        if m: body = m.group(0); bpath = p; break
    if not body: nos += 1; continue
    body = re.sub(r'//[^\n]*|/\*.*?\*/', '', body, flags=re.S)   # literals quoted in comments are not code
    flits, dlits = collect(body, bpath)
    if a.follow:
        seen = {fn.split('::')[-1]}; frontier = [body]
        for _lvl in range(a.follow):
            nxt = []
            for b in frontier:
                for cm in re.finditer(r'(?<![\w])(\w+)\s*\(', b):
                    nm = cm.group(1)
                    if nm in seen or nm not in FUNCS or len(FUNCS[nm]) > 6: continue
                    seen.add(nm)
                    for cb, cp in FUNCS[nm]:
                        f2, d2 = collect(cb, cp); flits |= f2; dlits |= d2; nxt.append(cb)
            frontier = nxt
    flits |= {x ^ 0x80000000 for x in flits}; dlits |= {-x for x in dlits}   # a negative exe constant matches the port's `-lit`
    try: cf, cd = exe_consts(int(addr, 16))
    except Exception: continue
    scr += 1
    fl = []; nmf = 0; nmd = 0
    for ca in sorted(cf):
        b = rd(ca, 4)
        if len(b) < 4: continue
        v = struct.unpack('<I', b)[0]
        if v in flits or v == 0: continue
        near = [x for x in flits if x != v and abs(x - v) <= a.ulp]
        if near: fl.append('F %#x exe=%#x port~%s' % (ca, v, [hex(x) for x in near]))
        else: nmf += 1; det.append((addr, fn, 'F', ca, struct.unpack('<f', b)[0]))
    for ca in sorted(cd):
        b = rd(ca, 8)
        if len(b) < 8: continue
        v = struct.unpack('<d', b)[0]
        if v == 0 or not math.isfinite(v) or v in dlits: continue
        near = [x for x in dlits if x != 0 and x != v and abs(x - v) <= a.drel * abs(v)]
        if near: fl.append('D %#x exe=%r port~%s' % (ca, v, near))
        else: nmd += 1; det.append((addr, fn, 'D', ca, v))
    if fl: print(cnt, addr, fn, verdict, fl)
    if nmf or nmd: miss.append((addr, fn, nmf, nmd))
if a.missing:
    with open(a.missing, 'w', encoding='utf8') as o:
        for r in miss: o.write('%s\t%s\t%d\t%d\n' % r)
if a.detail:
    with open(a.detail, 'w', encoding='utf8') as o:
        for r in det: o.write('%s\t%s\t%s\t%#x\t%r\n' % r)
print('screened', scr, 'nosrc', nos, 'functions with exe consts lacking a port counterpart:', len(miss), file=sys.stderr)
