"""VT-OVERRIDES (tools/standalone/vtable_override_audit.py): compare, slot by slot, the exe's vtables with the port's compiled vtables.

Why: in the DLL build a virtual that the repo never ported kept running the exe's code through the exe vtable. In the standalone there is no exe code: the
slot is whatever the C++ compiler puts there (the base class implementation or an empty body). CAECollisionAudioEntity::UpdateParameters (0x4DA370) was that
class of bug (PT-AUDIO). The first version of this tool only grepped the sources for the exe address; this one reads the real vtables of the BUILT port exe.

Inputs: the original exe (gta_sa_compact.exe), the port exe + its linker map (build/StandaloneRelease/bin/gta_reversed.{exe,map}), the sources (hook registry).
For every exe vtable (all of them, found by scanning .text for 4-byte operands that point at a run of code pointers in .rdata/.data) that can be named
(RH_ScopedVirtualClass registry, else a vote of the registered functions in its slots) and that has a port vtable `??_7Cls@@6B@`:

  MISSING   exe slot differs from the exe's BASE-class slot (own code), the port slot equals the port's BASE-class slot (the port inherits)
  WRONGFN   the exe slot address is registered as Cls2::Name in the hook registry, the port slot holds a function with another name
  EXTRA     the port overrides a slot the exe inherits (informational; the port has its own implementation where the exe has none)
  COUNT     slot count differs between the exe's and the port's vtable
  DTOR      destructors: the exe's scalar deleting destructor calls a function the repo does not mention (informational)

usage: python -X utf8 -I tools/standalone/vtable_override_audit.py [--repo .] [--exe gta_sa_compact.exe] [--port build/StandaloneRelease/bin/gta_reversed.exe] [--md out.txt] [class-filter]
"""
import argparse
import bisect
import collections
import re
import struct
from pathlib import Path

import capstone

ap = argparse.ArgumentParser()
ap.add_argument('--repo', default='.')
ap.add_argument('--exe', default='gta_sa_compact.exe')
ap.add_argument('--port', default='build/StandaloneRelease/bin/gta_reversed.exe')
ap.add_argument('--map', default=None)
ap.add_argument('--md', default=None)
ap.add_argument('filter', nargs='?', default='')
args = ap.parse_args()
repo = Path(args.repo)
portexe = Path(args.port)
mapf = Path(args.map) if args.map else portexe.with_suffix('.map')


class PE:
    def __init__(self, path):
        self.d = Path(path).read_bytes()
        d = self.d
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        nsec = struct.unpack_from('<H', d, pe + 6)[0]
        osz = struct.unpack_from('<H', d, pe + 20)[0]
        self.base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
        so = pe + 24 + osz
        self.secs = []
        for i in range(nsec):
            n, vs, va, rs, rp = struct.unpack_from('<8sIIII', d, so + 40 * i)
            self.secs.append((n.rstrip(b'\0').decode('latin1'), va + self.base, vs, rp, rs))
        self.text = [(va, va + max(vs, rs)) for nm, va, vs, rp, rs in self.secs if nm.startswith('.text')]

    def rd(self, a, n):
        for nm, va, vs, rp, rs in self.secs:
            if va <= a < va + max(vs, rs):
                off = rp + a - va
                return self.d[off:off + n]
        return b''

    def intext(self, a):
        return any(lo <= a < hi for lo, hi in self.text)

    def dw(self, a):
        b = self.rd(a, 4)
        return struct.unpack('<I', b)[0] if len(b) == 4 else None


exe = PE(args.exe)
port = PE(portexe)

# ---- port map: VA -> names, vtable symbols
va2names = collections.defaultdict(list)
vt_port = {}
for line in mapf.read_text(encoding='utf-8', errors='replace').splitlines():
    p = line.split()
    if len(p) < 3 or not re.match(r'^000[0-9a-f]:', p[0]):
        continue
    try:
        va = int(p[2], 16)
    except ValueError:
        continue
    n = p[1]
    va2names[va].append(n)
    if n.startswith('??_7') and n.endswith('@@6B@') and '<' not in n:
        vt_port.setdefault(n[4:-5], va)
port_syms_sorted = sorted(va2names)
vt_port_dtor = {}
for va, ns in va2names.items():
    for n in ns:
        m = re.match(r'^\?\?1(\w+)@@[UQ]AE@XZ$', n)
        if m:
            vt_port_dtor.setdefault(m.group(1), []).append((n, va))

# ---- sources: registry (address -> Class::Method), registered vtables, bases
reg_vt = {}
reg_fn = {}     # exe address -> set((class, method))
bases = {}
mentioned = set()
inst_rx = re.compile(r'RH_Scoped(?:VMT|Virtual)?(?:Overloaded)?Install\(\s*([\w:~]+)\s*,\s*(?:"[^"]*"\s*,\s*)?0x([0-9A-Fa-f]{6})')
for p in list((repo / 'source').rglob('*')) + list((repo / 'tests').rglob('*')):
    if p.suffix not in ('.cpp', '.h', '.hpp', '.inc'):
        continue
    t = p.read_text(encoding='utf-8', errors='replace')
    for m in re.finditer(r'0x([0-9a-fA-F]{6})\b', t):
        mentioned.add(int(m.group(1), 16))
    cur = None
    for line in t.splitlines():
        m = re.search(r'RH_ScopedVirtualClass\(\s*([\w:]+)\s*,\s*(0x[0-9A-Fa-f]+)', line)
        if m:
            cur = m.group(1)
            reg_vt[cur] = int(m.group(2), 16)
            continue
        m = re.search(r'RH_ScopedClass\(\s*([\w:]+)', line)
        if m:
            cur = m.group(1)
            continue
        m = inst_rx.search(line)
        if m and cur:
            reg_fn.setdefault(int(m.group(2), 16), set()).add((cur, m.group(1)))
    for m in re.finditer(r'\b(?:class|struct)\s+(?:NOTSA_EXPORT_VTABLE\s+)?(\w+)\s*(?:final\s*)?:\s*(?:public|protected|private)\s+([\w:]+)', t):
        bases.setdefault(m.group(1), m.group(2).split('::')[-1])

# ---- every exe vtable: 4-byte operands in .text that point at a code pointer in .rdata/.data
dsecs = [(va, va + max(vs, rs)) for nm, va, vs, rp, rs in exe.secs if nm in ('.rdata', '.data')]
txt = None
for nm, va, vs, rp, rs in exe.secs:
    if nm == '.text':
        txt = exe.d[rp:rp + rs]
lo_d = min(a for a, _ in dsecs)
hi_d = max(b for _, b in dsecs)
cand = set()
# vptr stores: mov dword ptr [reg], imm32 / [reg+d8], imm32 / [reg+d32], imm32 (primary and secondary vptrs); opcode C7 /0
for m in re.finditer(rb'\xc7(?:[\x00-\x03\x06\x07]|[\x40-\x43\x45-\x47].|[\x80-\x83\x85-\x87].{4})(.{4})', txt, re.S):
    v = struct.unpack('<I', m.group(1))[0]
    if lo_d <= v < hi_d and v % 4 == 0:
        cand.add(v)


def is_code_ptr(v):
    return v is not None and exe.intext(v)


starts = {a for a in cand if is_code_ptr(exe.dw(a))}
for v in reg_vt.values():
    if is_code_ptr(exe.dw(v)):
        starts.add(v)
starts = sorted(starts)


def exe_slots(a):
    k = bisect.bisect_right(starts, a)
    lim = starts[k] if k < len(starts) else a + 4 * 400
    out = []
    while a + 4 * len(out) < lim:
        v = exe.dw(a + 4 * len(out))
        if not is_code_ptr(v):
            break
        out.append(v)
    return out


exe_vt = {a: exe_slots(a) for a in starts}
vt_by_addr = {a: n for n, a in reg_vt.items()}
name_of = dict(vt_by_addr)
unnamed = []
for a in starts:
    if a in name_of:
        continue
    c = collections.Counter()
    for t in exe_vt[a]:
        for cls, _ in reg_fn.get(t, ()):
            c[cls] += 1
    if c:
        name_of[a] = c.most_common(1)[0][0]
    else:
        unnamed.append(a)
exe_vt_of = {}
for a in sorted(name_of, key=lambda a: a not in vt_by_addr):   # registered vtables win over voted ones
    exe_vt_of.setdefault(name_of[a], a)

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def body_bytes(t):
    ins = []
    for i in md.disasm(exe.rd(t, 48), t):
        ins.append(i)
        if i.mnemonic in ('ret', 'jmp') or len(ins) > 12:
            break
    if not ins or not (ins[-1].mnemonic == 'ret' or (ins[-1].mnemonic == 'jmp' and not ins[-1].op_str.startswith('0x'))):   # ret, or an indirect tail jump (forwarders)
        return None
    return b''.join(bytes(i.bytes) for i in ins)


def dis(t, n=4):
    return '; '.join(i.mnemonic + ' ' + i.op_str for i in list(md.disasm(exe.rd(t, 32), t))[:n])


def leaf_sig(pe, t, maxins=8):
    """normalised text of a small leaf function (<= maxins instructions, ends in ret, no call/jmp), absolute data operands replaced by the value they point at; None otherwise"""
    out = []
    for i in md.disasm(pe.rd(t, 64), t):
        if i.mnemonic in ('call', 'jmp') or i.mnemonic.startswith('j'):
            return None
        op = i.op_str
        m = re.search(r'(dword|qword) ptr \[(0x[0-9a-f]+)\]', op)
        if m:
            v = pe.rd(int(m.group(2), 16), 8 if m.group(1) == 'qword' else 4)
            op = op[:m.start()] + m.group(1) + ' ptr [data:' + v.hex() + ']' + op[m.end():]
        out.append(i.mnemonic + ' ' + op if i.mnemonic != 'ret' else 'ret')
        if i.mnemonic == 'ret':
            return '; '.join(out)
        if len(out) >= maxins:
            return None
    return None


def func_ins(pe, t, limit=400):
    """instructions of the function at t: stop at the first ret / tail jmp beyond the highest forward branch target"""
    out, pc, far = [], t, t
    while len(out) < limit:
        ins = list(md.disasm(pe.rd(pc, 16), pc, 1))
        if not ins:
            break
        i = ins[0]
        out.append(i)
        m = re.match(r'^0x([0-9a-f]+)$', i.op_str)
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


def indirect_calls(pe, t):
    return sum(1 for i in func_ins(pe, t) if i.mnemonic == 'call' and not i.op_str.startswith('0x'))


def call_targets(pe, t):
    r = []
    for i in func_ins(pe, t):
        if i.mnemonic == 'call':
            m = re.match(r'^0x([0-9a-f]+)$', i.op_str)
            r.append(int(m.group(1), 16) if m else -1)
    return r


def body_kind(t):
    b = body_bytes(t)
    if b is not None and len(b) <= 8:
        return 'trivial'
    return 'real'


def is_dtor(t):
    s = dis(t, 6)
    return 'test byte ptr [esp + 8], 1' in s or 'test byte ptr [esp + 4], 1' in s


def resolve(t):
    """follow `jmp rel32` thunks (MSVC emits them for overrides that only forward to the base implementation)"""
    for _ in range(4):
        b = exe.rd(t, 5)
        if len(b) == 5 and b[0] == 0xe9:
            t = (t + 5 + struct.unpack('<i', b[1:])[0]) & 0xffffffff
        else:
            break
    return t


def same_exe_fn(a, b):
    a, b = resolve(a), resolve(b)
    if a == b:
        return True
    na, nb = body_bytes(a), body_bytes(b)
    return na is not None and na == nb and len(na) <= 24


def demangle_short(n):
    if n.startswith('??_G') or n.startswith('??_E'):
        return 'scalar-deleting-dtor'
    if n.startswith('??1'):
        return 'dtor'
    m = re.match(r'^\?(\w+?)@([\w@]+?)@@', n)
    if m:
        return m.group(2).split('@')[0] + '::' + m.group(1)
    return n


def port_slots(cls):
    a = vt_port.get(cls)
    if a is None:
        return None
    k = bisect.bisect_right(port_syms_sorted, a)
    lim = port_syms_sorted[k] if k < len(port_syms_sorted) else a + 1600
    out = []
    while a + 4 * len(out) < lim:
        v = port.dw(a + 4 * len(out))
        if v is None or not port.intext(v):
            break
        out.append(v)
    return out


def pname(va):
    ns = [demangle_short(n) for n in va2names.get(va, [])]
    return ' | '.join(dict.fromkeys(ns)) if ns else '?%x' % va


def classify_unreg(cls, i, et, pt):
    """both sides have their own function, but the exe address is not mentioned in the sources: auto-compare leaf bodies, flag stubs, list the rest for review"""
    es, ps_ = leaf_sig(exe, resolve(et)), leaf_sig(port, pt)
    if es is not None and ps_ is not None:
        if es != ps_:
            rows.append(('LEAFDIFF', cls, i, et, 'port %s: %s   exe: %s' % (pname(pt), ps_, es)))
        return                                    # equal leaf bodies: verified
    if es is None and ps_ is not None:
        rows.append(('STUB?', cls, i, et, 'port %s is a leaf (%s); exe: %s' % (pname(pt), ps_, dis(et, 6))))
        return
    rows.append(('UNREG', cls, i, et, 'port %s; exe: %s' % (pname(pt), dis(et))))


rows = []        # (kind, cls, slot, exe_addr, detail)
checked = 0
for cls in sorted(exe_vt_of):
    if args.filter and args.filter not in cls:
        continue
    es = exe_vt[exe_vt_of[cls]]
    ps = port_slots(cls)
    if ps is None:
        continue
    checked += 1
    b = bases.get(cls)
    ebs = exe_vt[exe_vt_of[b]] if b in exe_vt_of else None
    pbs = port_slots(b) if b else None
    if len(es) != len(ps):
        rows.append(('COUNT', cls, -1, 0, 'exe %d slots, port %d' % (len(es), len(ps))))
    for i in range(min(len(es), len(ps))):
        et, pt = es[i], ps[i]
        pn = va2names.get(pt, [])
        pure = any('_purecall' in n for n in pn)
        if et == 0x82263a:
            continue   # pure in the exe too
        if is_dtor(et):
            tgt = None
            for ins in list(md.disasm(exe.rd(et, 40), et))[:8]:
                if ins.mnemonic == 'call':
                    tgt = int(ins.op_str, 16)
                    break
            if tgt is not None:
                ecalls = [c for c in call_targets(exe, tgt)]
                pdt = [v for n, v in vt_port_dtor.get(cls, [])]
                pcalls = call_targets(port, pdt[0]) if pdt else None
                ne, npc = len(ecalls), (len(pcalls) if pcalls is not None else -1)
                note = 'exe dtor %#x: %d calls (%s)%s; port dtor: %s' % (tgt, ne, ' '.join('%x' % c for c in ecalls[:8]), '' if tgt in mentioned else ' [address never mentioned]', ('%d calls' % npc) if pdt else 'no symbol')
                # calls with a registry name (CEntity::CleanUpOldReference, CAnimBlendAssociation::SetFinishCallback ...): the port's destructor has to call the same function (by symbol name)
                missing = []
                if pcalls is not None:
                    pnames = set()
                    for c in pcalls:
                        for n in va2names.get(c, []):
                            pnames.add(demangle_short(n))
                    for c in ecalls:
                        for rc, rm in reg_fn.get(c, ()):
                            if rm in ('Constructor', 'Destructor') or rm.endswith('_Reversed'):
                                continue
                            if (rc + '::' + rm) not in pnames and (rc + '::' + rm) not in [x for x in pnames]:
                                missing.append(rc + '::' + rm)
                if pdt and indirect_calls(exe, tgt) > indirect_calls(port, pdt[0]):
                    missing.append('(%d virtual calls in the exe dtor, %d in the port dtor)' % (indirect_calls(exe, tgt), indirect_calls(port, pdt[0])))
                if pdt and missing:
                    rows.append(('DTOR', cls, i, tgt, note + ' ; exe calls not found in the port dtor: ' + ' '.join(missing)))
                elif not pdt and ne > 0:
                    rows.append(('DTOR', cls, i, tgt, note + ' ; no port dtor symbol'))
            continue
        flagged = False
        if ebs is not None and pbs is not None and i < len(ebs) and i < len(pbs):
            exe_ov = not same_exe_fn(et, ebs[i])
            port_ov = pt != pbs[i] and not (set(pn) & set(va2names.get(pbs[i], [])))
            if exe_ov and not port_ov:
                rows.append(('MISSING', cls, i, et, '%s  (port slot = %s)' % (dis(et), pname(pt))))
                flagged = True
            elif port_ov and not exe_ov and not pure:
                es_, ps2 = leaf_sig(exe, resolve(et)), leaf_sig(port, pt)
                if es_ is not None and ps2 is not None and es_ == ps2:
                    pass            # same tiny body: harmless
                else:
                    rows.append(('EXTRA', cls, i, et, 'port %s: %s ; exe inherits: %s' % (pname(pt), ps2 or 'non-leaf', es_ or dis(et, 5))))
            elif port_ov and exe_ov and resolve(et) not in mentioned and et not in mentioned:
                classify_unreg(cls, i, et, pt)
        elif ebs is None and not pure and b is None and resolve(et) not in mentioned and et not in mentioned:
            classify_unreg(cls, i, et, pt)
        elif pure:
            rows.append(('MISSING', cls, i, et, '%s  (port slot = pure virtual)' % dis(et)))
            flagged = True
        if flagged:
            continue
        for rc, rm in reg_fn.get(et, ()):
            if rm in ('Constructor', 'Destructor'):
                continue
            short = rm.split('::')[-1]
            short = short[:-9] if short.endswith('_Reversed') else short
            if not any(('?' + short + '@') in n or n.endswith('::' + short) for n in va2names.get(pt, [])) and not pure:
                rows.append(('WRONGFN', cls, i, et, 'registry %s::%s, port slot = %s' % (rc, rm, pname(pt))))
            break

order = {'MISSING': 0, 'STUB?': 1, 'LEAFDIFF': 2, 'WRONGFN': 3, 'UNREG': 4, 'COUNT': 5, 'EXTRA': 6, 'DTOR': 7}
# triaged rows (tools/standalone/vtable_override_audit_explained.tsv): shown at the end as EXPLAINED, not counted
expl = []
tsv = Path(__file__).with_name('vtable_override_audit_explained.tsv')
if tsv.exists():
    for ln in tsv.read_text(encoding='utf-8').splitlines():
        if ln.strip() and not ln.startswith('#'):
            f = ln.split('	')
            if len(f) == 5:
                expl.append(f)
kept, explained = [], []
for r in rows:
    why = None
    for k, c, sl, rx, reason in expl:
        if k == r[0] and re.fullmatch(c, r[1]) and (sl == '*' or sl == str(r[2]) or re.fullmatch(sl, str(r[2]))) and re.search(rx, r[4]):
            why = reason
            break
    (explained if why else kept).append(r + (why,) if why else r)
rows = kept
rows.sort(key=lambda r: (order[r[0]], r[1], r[2]))
print('exe vtables found %d (named %d, unnamed %d); compared with a port vtable: %d' % (len(starts), len(name_of), len(unnamed), checked))
print('UNEXPLAINED:', dict(collections.Counter(r[0] for r in rows)), '  EXPLAINED:', len(explained))
out = ['%-8s %-40s slot %-3d %#08x  %s' % (k, c, i, a, det) for k, c, i, a, det in rows]
print('\n'.join(out))
if explained:
    print('--- explained (tools/standalone/vtable_override_audit_explained.tsv):')
    for r in sorted(explained, key=lambda r: (r[0], r[1], r[2])):
        print('%-8s %-40s slot %-3d %s' % (r[0], r[1], r[2], r[5]))
if args.md:
    Path(args.md).write_text('\n'.join(out) + '\n', encoding='utf-8')

# ---- coverage report: vtables that could not be compared
if not args.filter:
    print('--- exe vtables with no name (no registered function in any slot):')
    for a in unnamed:
        sl = exe_vt[a]
        if sl[0] >= 0x770000:
            continue    # CRT / STL / C++ runtime vtables
        print('   %#x  %d slots  slot0 %#x %s' % (a, len(sl), sl[0], dis(sl[0], 3)))
        print('        slots: ' + ' '.join('%x%s' % (t, '' if (t in mentioned or t == 0x82263a) else '!') for t in sl) + '    (! = address never mentioned in the sources)')
    print('--- named exe vtables without a port vtable symbol (??_7Cls@@6B@):')
    for n in sorted(exe_vt_of):
        if n not in vt_port:
            print('   %s  vtable %#x (%d slots)%s' % (n, exe_vt_of[n], len(exe_vt[exe_vt_of[n]]), '' if exe_vt_of[n] in vt_by_addr else ' [voted name]'))
