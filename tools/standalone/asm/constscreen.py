"""Screen exe float constants against port float literals, per function.

For every row of a hitcount tsv (exe_addr, function, count, file.obj, verdict) with an exe address,
disassemble the exe function, collect the .rdata float constants it loads (fld/fmul/fadd/fsub/fcomp [mem])
and compare them with the float literals / bit_cast<float>(0x...) in the port function body.
Prints functions where an exe constant is within a few ulp of (but not equal to) a port literal.
Usage: py -X utf8 -I constscreen.py <tsv> [--exe EXE] [--src source/game_sa] [--ulp 8] [--skip regex]
"""
import re, struct, sys, glob, os, argparse
import capstone

ap = argparse.ArgumentParser()
ap.add_argument('tsv')
ap.add_argument('--exe', default='gta_sa_compact.exe')
ap.add_argument('--src', default='source/game_sa')
ap.add_argument('--ulp', type=int, default=8)
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

def exe_consts(start, maxn=3000):
    pc = start; far = start; out = set()
    for _ in range(maxn):
        ins = list(md.disasm(rd(pc, 16), pc, 1))
        if not ins: break
        i = ins[0]
        m = re.match(r'^f(?:ld|mul|add|sub|subr|div|divr|comp|com|ucom)\s+dword ptr \[(0x[0-9a-f]+)\]$', i.mnemonic + ' ' + i.op_str)
        if m and int(m.group(1), 16) >= base + 0x450000: out.add(int(m.group(1), 16))
        if i.mnemonic[0] == 'j':
            mm = re.match(r'^0x([0-9a-f]+)$', i.op_str)
            if mm:
                t = int(mm.group(1), 16)
                if start <= t < start + 0x6000 and t > far: far = t
        pc += i.size
        if i.mnemonic == 'ret' and pc > far: break
        if i.mnemonic == 'jmp' and pc > far and i.op_str.startswith('0x'): break
    return out

src = {}
for p in glob.glob(a.src + '/**/*.cpp', recursive=True):
    src.setdefault(os.path.basename(p) + '.obj', []).append(p)
bits = lambda x: struct.unpack('<I', struct.pack('<f', x))[0]
rows = []
for l in open(a.tsv, encoding='utf8').read().splitlines()[1:]:
    c = l.split('\t')
    if len(c) >= 5 and c[0] != '-' and not re.search(a.skip, c[1] + c[3]): rows.append((int(c[2]), c[0], c[1], c[3], c[4]))
rows.sort(reverse=True)
nos = 0; scr = 0
for cnt, addr, fn, obj, verdict in rows:
    body = None
    for p in src.get(obj, []):
        t = open(p, encoding='utf8', errors='ignore').read()
        m = re.search(r'\n[^\n;{}]*\b' + re.escape(fn) + r'\s*\([^;{]*\)[^;{]*\{.*?\n\}', t, re.S)
        if m: body = m.group(0); break
    if not body: nos += 1; continue
    lits = set()
    for m in re.finditer(r'(?<![\w.])(\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?f\b', body):
        try: lits.add(bits(float(m.group(0)[:-1])))
        except Exception: pass
    for m in re.finditer(r'bit_cast<float>\((0x[0-9A-Fa-f]+)', body): lits.add(int(m.group(1), 16))
    try: cs = exe_consts(int(addr, 16))
    except Exception: continue
    scr += 1
    fl = []
    for ca in sorted(cs):
        b = rd(ca, 4)
        if len(b) < 4: continue
        v = struct.unpack('<I', b)[0]
        if v in lits or v == 0: continue
        near = [x for x in lits if x != v and abs(x - v) <= a.ulp]
        if near: fl.append('%#x exe=%#x port~%s' % (ca, v, [hex(x) for x in near]))
    if fl: print(cnt, addr, fn, verdict, fl)
print('screened', scr, 'nosrc', nos, file=sys.stderr)
