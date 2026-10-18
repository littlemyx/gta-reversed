"""VT-OVERRIDES: for every function the hook registry knows (RH_ScopedInstall / VMTInstall ... with an exe address) compare how often the EXE's code calls a watched callee
(CEntity::RegisterReference / CleanUpOldReference, CAnimBlendAssociation::SetFinishCallback / SetDeleteCallback, CAnimManager::Add/RemoveAnimBlockRef, ...) with how often the built port's function calls it.
A function that releases fewer references / callbacks than the exe's keeps dangling pointers the exe nulls (use-after-free reads / writes in the standalone, where no exe code runs instead).
usage: python -X utf8 -I tools/standalone/callee_audit.py [--repo .] [--exe gta_sa_compact.exe] [--port build/StandaloneRelease/bin/gta_reversed.exe]
"""
import argparse, bisect, collections, re, struct, sys
from pathlib import Path
import capstone

ap = argparse.ArgumentParser()
ap.add_argument('--repo', default='.'); ap.add_argument('--exe', default='gta_sa_compact.exe'); ap.add_argument('--port', default='build/StandaloneRelease/bin/gta_reversed.exe')
ap.add_argument('--watch', default='CEntity::RegisterReference,CEntity::CleanUpOldReference,CAnimBlendAssociation::SetFinishCallback,CAnimBlendAssociation::SetDeleteCallback,CAnimManager::AddAnimBlockRef,CAnimManager::RemoveAnimBlockRef,CAnimManager::RemoveFromAnimGroup')
args = ap.parse_args()
repo = Path(args.repo)
sys.path.insert(0, str(Path(__file__).parent))


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


exe, port = PE(args.exe), PE(args.port)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

# registry: exe address -> (class, method)
reg = collections.defaultdict(set)
rx = re.compile(r'RH_Scoped(?:VMT|Virtual)?(?:Overloaded)?(?:Named)?Install\(\s*([\w:~]+)\s*,\s*(?:"[^"]*"\s*,\s*)?0x([0-9A-Fa-f]{6})')
dx = re.compile(r'RH_ScopedInstall\(\s*(Constructor|Destructor)\s*,\s*0x([0-9A-Fa-f]{6})')
for p in (repo / 'source').rglob('*'):
    if p.suffix not in ('.cpp', '.h', '.inc'):
        continue
    cur = None
    for line in p.read_text(encoding='utf-8', errors='replace').splitlines():
        m = re.search(r'RH_Scoped(?:Virtual)?Class\(\s*([\w:]+)', line)
        if m:
            cur = m.group(1); continue
        m = rx.search(line)
        if m and cur:
            reg[int(m.group(2), 16)].add((cur, m.group(1)))

# port map
va2names = collections.defaultdict(list)
name2va = collections.defaultdict(list)
mp = Path(args.port).with_suffix('.map')
for line in mp.read_text(encoding='utf-8', errors='replace').splitlines():
    q = line.split()
    if len(q) >= 3 and re.match(r'^000[0-9a-f]:', q[0]):
        try:
            va = int(q[2], 16)
        except ValueError:
            continue
        va2names[va].append(q[1]); name2va[q[1]].append(va)


def short(n):
    m = re.match(r'^\?(\w+?)@([\w@]+?)@@', n)
    if n.startswith('??0'):
        m2 = re.match(r'^\?\?0(\w+)@@', n); return (m2.group(1) + '::Constructor') if m2 else None
    if n.startswith('??1'):
        m2 = re.match(r'^\?\?1(\w+)@@', n); return (m2.group(1) + '::Destructor') if m2 else None
    return (m.group(2).split('@')[0] + '::' + m.group(1)) if m else None


port_by_short = collections.defaultdict(set)
for n, vas in name2va.items():
    sh = short(n)
    if sh:
        port_by_short[sh].update(vas)


def calls(pe, t, limit=600):
    out, pc, far = [], t, t
    while len(out) < limit:
        ins = list(md.disasm(pe.rd(pc, 16), pc, 1))
        if not ins:
            break
        i = ins[0]
        m = re.match(r'^0x([0-9a-f]+)$', i.op_str)
        if i.mnemonic == 'call' and m:
            out.append(int(m.group(1), 16))
        if m and (i.mnemonic == 'jmp' or i.mnemonic.startswith('j')):
            tt = int(m.group(1), 16)
            if t <= tt < t + 0x4000 and tt > far:
                far = tt
        pc += i.size
        if i.mnemonic == 'ret' and pc > far:
            break
        if i.mnemonic == 'jmp' and m and pc > far:
            break
    return out


watch = set(args.watch.split(','))
_small = {}


def small(t):
    if t not in _small:
        n = 0
        pc = t
        while n < 60:
            ins = list(md.disasm(port.rd(pc, 16), pc, 1))
            if not ins:
                break
            pc += ins[0].size
            n += 1
            if ins[0].mnemonic == 'ret':
                break
        _small[t] = n < 60 and any(True for _ in [0])
    return _small[t]

watch_exe = {a for a, s in reg.items() for c, mth in s if (c + '::' + mth) in watch}
rows = []
for a, names in sorted(reg.items()):
    cm = sorted(names)[0]
    cname = cm[0] + '::' + cm[1]
    if cm[1] in ('Constructor', 'Destructor'):
        pass
    ec = collections.Counter()
    for t in calls(exe, a):
        for c, mth in reg.get(t, ()):
            if (c + '::' + mth) in watch:
                ec[c + '::' + mth] += 1
    if not ec:
        continue
    cand = port_by_short.get(cname)
    if not cand:
        rows.append((cname, a, 'NO PORT SYMBOL', dict(ec)))
        continue
    best = None
    for v in cand:
        pcnt = collections.Counter()

        def walk(fn, depth):
            for t in calls(port, fn):
                hit = False
                for n in va2names.get(t, []):
                    sh = short(n)
                    if sh in watch:
                        pcnt[sh] += 1
                        hit = True
                if not hit and depth < 2 and small(t):      # a small helper (SafeRegisterRef, CEntityRef::..., lambdas): count what it calls
                    walk(t, depth + 1)
        walk(v, 0)
        score = sum(min(pcnt[k], ec[k]) for k in ec)
        if best is None or score > best[0]:
            best = (score, pcnt)
    pcnt = best[1]
    if any(pcnt[k] < ec[k] for k in ec):
        rows.append((cname, a, 'exe %s | port %s' % (dict(ec), dict(pcnt)), None))
for cname, a, txt, _ in rows:
    print('%-55s %#x  %s' % (cname, a, txt))
print(len(rows), 'functions with fewer watched calls in the port than in the exe')
