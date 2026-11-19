"""fildscan.py - int-vs-float field diff of hooked Ped / Vehicle / Physical functions against the exe.

For every hooked function (RH_*Install with an address) of the given source files it disassembles the exe (tools/standalone/exedis.py),
tracks the `this` register (ecx and its mov-copies) and lists x87 accesses [this+off]: fild/fist/fistp/fiadd/fimul/ficom... (integer
operand) and fld/fst/fadd/fmul/fcom... dword (float operand). The offset is mapped to a port field through the MSVC class layout
(cl /d1reportSingleClassLayout<Class>, produced by --gen) plus the member declarations of the headers. Mismatches (exe reads the
field as int but the port declares float, or vice versa) are printed as TSV: addr, func, insn, off, field, port type, exe kind.

usage: py -X utf8 -I fildscan.py --repo <worktree> --exe <gta_sa_compact.exe> --layout layout.txt [--gen <build dir with compile_commands.json>] [files...]
"""
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument('--repo', default='.')
ap.add_argument('--exe', required=True)
ap.add_argument('--layout', default='layout.txt')
ap.add_argument('--gen', help='build dir (compile_commands.json); MSVC env must be loaded; (re)generates --layout')
ap.add_argument('--all', action='store_true', help='also print the matching sites')
ap.add_argument('files', nargs='*')
a = ap.parse_args()
repo = Path(a.repo)
SRC = a.files or ['source/game_sa/Entity/Ped/*.cpp', 'source/game_sa/Entity/Vehicle/*.cpp', 'source/game_sa/Entity/Physical.cpp']
CLASSES = 'CPed CPlayerPed CCopPed CCivilianPed CEmergencyPed CPhysical CEntity CVehicle CAutomobile CBike CBmx CBoat CHeli CPlane CTrain CTrailer CMonsterTruck CQuadBike'.split()
HDRS = ['Ped/Ped', 'Ped/PlayerPed', 'Ped/CopPed', 'Ped/CivilianPed', 'Ped/EmergencyPed', 'Physical', 'Entity', 'Vehicle/Vehicle', 'Vehicle/Automobile', 'Vehicle/Bike', 'Vehicle/Bmx', 'Vehicle/Boat', 'Vehicle/Heli', 'Vehicle/Plane', 'Vehicle/Train', 'Vehicle/Trailer', 'Vehicle/MonsterTruck', 'Vehicle/QuadBike']
FILECLS = {'Ped': 'CPed', 'PlayerPed': 'CPlayerPed', 'CopPed': 'CCopPed', 'CivilianPed': 'CCivilianPed', 'EmergencyPed': 'CEmergencyPed', 'Physical': 'CPhysical', 'Vehicle': 'CVehicle', 'Automobile': 'CAutomobile', 'Bike': 'CBike', 'Bmx': 'CBmx', 'Boat': 'CBoat', 'Heli': 'CHeli', 'Plane': 'CPlane', 'Train': 'CTrain', 'Trailer': 'CTrailer', 'MonsterTruck': 'CMonsterTruck', 'QuadBike': 'CQuadBike'}

if a.gen:
    cc = json.load(open(Path(a.gen) / 'compile_commands.json'))
    cmd = next(e['command'] for e in cc if e['file'].replace('\\', '/').endswith('Entity/Ped/Ped.cpp'))
    cmd = re.sub(r'\s/Fo\S+|\s/Fd\S+|\s/FS|\s-c\s.*$|\s/c\s.*$|/Yu\S+|/Fp\S+|/FI\S+|/MP', '', cmd)
    inc = ''.join('#include "%s.h"\n' % Path(h).name for h in HDRS if Path(h).name != 'Entity')
    Path('_lay.cpp').write_text('#include "StdInc.h"\n' + inc)
    txt = ''
    for c in CLASSES:  # cl honours only one /d1reportSingleClassLayout per run
        r = subprocess.run(cmd + ' /Zs /d1reportSingleClassLayout' + c + ' _lay.cpp', capture_output=True, text=True, shell=True)
        txt += r.stdout + r.stderr
    Path(a.layout).write_text(txt)

# class -> [(offset, name, type-from-layout)]
lay = {}
cur = None
for ln in Path(a.layout).read_text(errors='replace').splitlines():
    m = re.match(r'class (\S+)\tsize\((\d+)\)', ln)
    if m:
        cur = m.group(1)
        lay[cur] = []
        continue
    if cur is None:
        continue
    head = ln.split('|')[0]
    if '.' in head:  # bitfield
        continue
    m = re.match(r'\s*(\d+)\s*\|[ |+\-]*(?:(\S[^|]*?)\s+)?(m_\w+|field_\w+)\s*$', ln)
    if m:
        lay[cur].append((int(m.group(1)), m.group(3), (m.group(2) or '').strip()))
# member declarations: name -> type
decl = {}
for h in HDRS:
    p = repo / 'source/game_sa/Entity' / (h + '.h')
    if not p.exists():
        continue
    for ln in p.read_text(errors='replace').splitlines():
        ln = ln.split('//')[0]
        m = re.match(r'\s*(?:static\s+)?((?:const\s+)?[\w:<>,\*& ]+?)\s+(m_\w+|field_\w+)(?:\[[^\]]*\])?\s*(?:=[^;]*|\{[^;]*)?;', ln)
        if m:
            decl.setdefault(m.group(2), m.group(1).strip())
FLOAT = re.compile(r'^(float|CVector|CVector2D|CRect|CSimpleTransform|CMatrix|CQuaternion|RwV3d|x87d)\b')
INT = re.compile(r'^(u?int(8|16|32)(_t)?|int|unsigned|short|long|bool|eModelID|eStats|e\w+|size_t)\b')


def kind(t):
    t = t.replace('const ', '').strip()
    if '*' in t or '&' in t:
        return 'ptr'
    if FLOAT.match(t):
        return 'float'
    if INT.match(t) or t.startswith('notsa::') or t.startswith('Enum') or t.startswith('?$WEnum'):
        return 'int'
    return 'other'


def field_at(cls, off):
    best = None
    for (o, n, t) in lay.get(cls, []):
        if o <= off and (best is None or o >= best[0]):
            best = (o, n, t)
    return best


funcs = []
for pat in SRC:
    for f in sorted(repo.glob(pat)):
        for ln in f.read_text(errors='replace').splitlines():
            m = re.search(r'RH_\w*Install\(\s*(\w+)\s*,\s*(?:"[^"]*"\s*,\s*)?(0x[0-9A-Fa-f]+)', ln)
            if m:
                funcs.append((m.group(2).lower(), f.stem + '::' + m.group(1)))
seen = set()
funcs = [x for x in funcs if not (x[0] in seen or seen.add(x[0]))]
fmap = dict(funcs)

IFOP = ('fild', 'fist', 'fistp', 'fiadd', 'fisub', 'fisubr', 'fimul', 'fidiv', 'fidivr', 'ficom', 'ficomp')
FFOP = ('fld', 'fst', 'fstp', 'fadd', 'fsub', 'fsubr', 'fmul', 'fdiv', 'fdivr', 'fcom', 'fcomp')
rows = []
psites = []
nsite = 0
exedis = repo / 'tools/standalone/exedis.py'
CH = 40
for i in range(0, len(funcs), CH):
    chunk = funcs[i:i + CH]
    out = subprocess.run([sys.executable, '-X', 'utf8', '-I', str(exedis), *[c[0] for c in chunk], '--exe', a.exe, '--repo', str(repo), '--max', '1500'], capture_output=True, text=True, encoding='utf8').stdout
    for b in re.split(r'^; ==== ', out, flags=re.M)[1:]:
        lines = b.splitlines()
        hm = re.match(r'(0x[0-9a-f]+)\s*(\S*)', lines[0])
        addr = hm.group(1)
        name = fmap.get(addr, hm.group(2) or '?')
        cls = FILECLS.get(name.split('::')[0])
        if cls not in lay:
            continue
        al = {'ecx'}
        esp = 0  # bytes pushed since entry (params: [esp + pos], pos = off + esp; arg k is at pos 4 + 4*k)
        for ln in lines[1:]:
            m = re.match(r'[0-9a-f]+\s+(\w+)\s+(.*)$', ln)
            if not m:
                continue
            op, rest = m.group(1), m.group(2).strip()
            if op == 'push':
                esp += 4
            elif op == 'pop':
                esp -= 4
            elif op in ('sub', 'add') and rest.startswith('esp, '):
                try:
                    esp += int(rest[5:], 16) * (1 if op == 'sub' else -1)
                except ValueError:
                    pass
            pm = re.search(r'(\w+) ptr \[esp(?: \+ (0x[0-9a-f]+))?\]', rest)
            if pm and op in IFOP + FFOP and (op in IFOP or pm.group(1) == 'dword'):
                pos = int(pm.group(2) or '0', 16) + esp
                if pos >= 4:
                    psites.append((addr, name, op, (pos - 4) // 4, 'int' if op in IFOP else 'float'))
            mm = re.match(r'(\w+), (\w+)$', rest)
            if op == 'mov' and mm and mm.group(2) in al and mm.group(1) != 'esp':
                al.add(mm.group(1))
                continue
            if op == 'call':
                al.discard('ecx')
            if op in ('mov', 'lea', 'xor', 'pop', 'movzx', 'movsx', 'imul', 'sub', 'add', 'and', 'or'):
                dst = rest.split(',')[0].strip()
                al.discard(dst)
            if op in IFOP + FFOP:
                om = re.search(r'(\w+) ptr \[(\w+)(?: \+ (0x[0-9a-f]+))?\]', rest)
                if om and om.group(2) in al:
                    sz, off = om.group(1), int(om.group(3) or '0', 16)
                    ek = 'int' if op in IFOP else 'float'
                    if ek == 'float' and sz != 'dword':
                        continue
                    f = field_at(cls, off)
                    if not f:
                        continue
                    t = decl.get(f[1], f[2]) or '?'
                    pk = kind(t)
                    nsite += 1
                    bad = pk in ('float', 'int') and pk != ek
                    if bad or a.all:
                        rows.append((addr, name, ('MISMATCH ' if bad else 'ok ') + op, hex(off), f[1] + ('' if f[0] == off else '+%d' % (off - f[0])), t, ek))
# parameter sites: exe reads arg k with an int (fild) / float (fld) instruction -> compare with the port definition's k-th parameter
cache = {}
for (addr, name, op, k, ek) in psites:
    stem, fn = name.split('::')
    src = next((f for pat in SRC for f in repo.glob(pat) if f.stem == stem), None)
    if src is None:
        continue
    if src not in cache:
        cache[src] = src.read_text(errors='replace')
    m = re.search(r'::' + re.escape(fn) + r'\(([^)]*)\)\s*(?:const\s*)?\{', cache[src])
    if not m:
        continue
    prm = [x.strip() for x in re.split(r',(?![^<]*>)', m.group(1)) if x.strip()]
    if k >= len(prm):
        continue
    t = re.sub(r'\s*=.*$', '', prm[k])
    t = re.sub(r'\s+\w+$', '', t).strip()
    pk = kind(t)
    bad = pk in ('float', 'int') and pk != ek
    if bad or a.all:
        rows.append((addr, name, ('MISMATCH ' if bad else 'ok ') + op, 'arg%d' % k, prm[k], t, ek))
for r in rows:
    print('\t'.join(r))
print('# sites=%d functions=%d mismatches=%d' % (nsite, len(funcs), sum(1 for r in rows if r[2].startswith('MISMATCH'))), file=sys.stderr)
