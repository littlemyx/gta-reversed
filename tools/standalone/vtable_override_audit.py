"""PT-AUDIO (tools/standalone/vtable_override_audit.py): find virtual overrides the exe has but the port has no address for (the CAECollisionAudioEntity::UpdateParameters class of gap).
For every class with RH_ScopedVirtualClass(Name, vtable, n): override slots = slots whose target differs from the base class's slot (base = first `: public X` in the headers)
plus new slots beyond the base's table; a target is "covered" when 0x<addr> appears anywhere in source/ or tests/. Trivial bodies (ret / mov eax,imm;ret / xor;ret) and scalar deleting dtors are skipped.
"""
import re, glob, struct, os, sys, capstone

# usage: python -I tools/standalone/vtable_override_audit.py [repo=.] [exe=gta_sa_compact.exe] [class-filter]   (needs capstone)
repo = sys.argv[1] if len(sys.argv) > 1 else '.'
exe = sys.argv[2] if len(sys.argv) > 2 else 'gta_sa_compact.exe'
d = open(exe, 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
nsec = struct.unpack_from('<H', d, pe + 6)[0]
osz = struct.unpack_from('<H', d, pe + 20)[0]
base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
so = pe + 24 + osz
secs = []
for i in range(nsec):
    n, vs, va, rs, rp = struct.unpack_from('<8sIIII', d, so + 40 * i)
    secs.append((n, va + base, vs, rp, rs))


def rd(a, n):
    for nm, va, vs, rp, rs in secs:
        if va <= a < va + max(vs, rs):
            return d[rp + a - va:rp + a - va + n]


text = [(va, va + max(vs, rs)) for nm, va, vs, rp, rs in secs if nm.startswith(b'.text')]


def intext(a):
    return any(lo <= a < hi for lo, hi in text)


# repo text
allsrc = []
cls_vt = {}
bases = {}
for p in glob.glob(repo + '/source/**/*.*', recursive=True) + glob.glob(repo + '/tests/**/*.*', recursive=True):
    if not p.endswith(('.cpp', '.h', '.hpp', '.inc')):
        continue
    t = open(p, encoding='utf-8', errors='replace').read()
    allsrc.append(t.lower())
    for m in re.finditer(r'RH_ScopedVirtualClass\(\s*([\w:]+)\s*,\s*(0x[0-9A-Fa-f]+)', t):
        cls_vt[m.group(1)] = int(m.group(2), 16)
    for m in re.finditer(r'\b(?:class|struct)\s+(?:NOTSA_EXPORT_VTABLE\s+)?(\w+)\s*(?:final\s*)?:\s*(?:public|protected|private)\s+([\w:]+)', t):
        bases.setdefault(m.group(1), m.group(2).split('::')[-1])
allsrc = '\n'.join(allsrc)
mentioned = set(int(x, 16) for x in re.findall(r'0x([0-9a-f]{6})\b', allsrc))


def slots(addr):
    out = []
    while len(out) < 300:
        b = rd(addr + 4 * len(out), 4)
        if not b:
            break
        t = struct.unpack('<I', b)[0]
        if not intext(t):
            break
        out.append(t)
    return out


md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def body_kind(t):
    b = rd(t, 24)
    ins = list(md.disasm(b, t))[:4]
    txt = '; '.join(i.mnemonic + ' ' + i.op_str for i in ins)
    if re.match(r'^(ret( 0x[0-9a-f]+| \d+)?|xor (al|eax), (al|eax); ret.*|mov (al|eax), (0x[0-9a-f]+|\d+); ret.*|fld dword ptr \[0x[0-9a-f]+\]; ret.*|mov eax, dword ptr \[ecx \+ 0x[0-9a-f]+\]; ret.*)', txt):
        return 'trivial', txt
    if 'test byte ptr [esp + 8], 1' in txt or 'test byte ptr [esp + 4], 1' in txt:
        return 'dtor', txt
    return 'real', txt


# the exe's vtable of a class (slots until the next non-text dword); overlapping neighbours are cut at the next registered vtable address
vts = sorted(set(cls_vt.values()))
rows = []
for cls, va in sorted(cls_vt.items()):
    sl = slots(va)
    nxt = [v for v in vts if v > va]
    if nxt:
        sl = sl[:max(0, (nxt[0] - va) // 4)]
    b = bases.get(cls)
    bsl = []
    if b in cls_vt:
        bsl = slots(cls_vt[b])
        nb = [v for v in vts if v > cls_vt[b]]
        if nb:
            bsl = bsl[:max(0, (nb[0] - cls_vt[b]) // 4)]
    miss = []
    for i, t in enumerate(sl):
        if i < len(bsl) and bsl[i] == t:
            continue
        if t == 0x82263a or t in mentioned:
            continue
        k, txt = body_kind(t)
        if k != 'real':
            continue
        miss.append((i, t, txt))
    if miss:
        rows.append((cls, b, len(sl), miss))
flt = sys.argv[3] if len(sys.argv) > 3 else ''
tot = 0
for cls, b, n, miss in rows:
    if flt and flt not in cls:
        continue
    print('== %s (base %s, %d slots): %d unmentioned real overrides' % (cls, b, n, len(miss)))
    for i, t, txt in miss[:6]:
        print('   slot %d -> %#x : %s' % (i, t, txt[:90]))
    tot += len(miss)
print('classes', len(rows), 'total', tot)
