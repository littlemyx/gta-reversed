"""VT-OVERRIDES: functions the sources define as a stub (NOTSA_UNREACHABLE / assert(0) / "not reversed" / TODO-only / empty body) although the exe has real code at the address in the comment above them.
For each, the exe is searched for callers (E8/E9 rel32 targets, vtable slots, pushed function pointers); a stub with a caller or a vtable slot is LIVE (the DLL build ran the exe's code through the hook-less original; the standalone runs the stub).
usage: python -X utf8 -I tools/standalone/stub_audit.py [--repo .] [--exe gta_sa_compact.exe]
"""
import argparse, re, struct, sys, collections
from pathlib import Path
import capstone

ap = argparse.ArgumentParser()
ap.add_argument('--repo', default='.'); ap.add_argument('--exe', default='gta_sa_compact.exe')
args = ap.parse_args()
d = Path(args.exe).read_bytes()
pe = struct.unpack_from('<I', d, 0x3c)[0]
n = struct.unpack_from('<H', d, pe + 6)[0]
osz = struct.unpack_from('<H', d, pe + 20)[0]
base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
so = pe + 24 + osz
secs = []
for i in range(n):
    nm, vs, va, rs, rp = struct.unpack_from('<8sIIII', d, so + 40 * i)
    secs.append((nm.rstrip(b'\0').decode(), base + va, max(vs, rs), rp))
txt = next(s for s in secs if s[0] == '.text')
tb = d[txt[3]:txt[3] + txt[2]]


def rd(a, k):
    for nm, va, sz, rp in secs:
        if va <= a < va + sz:
            return d[rp + a - va:rp + a - va + k]
    return b''


# call / jmp rel32 callers and absolute 4-byte references (vtable slots, pushes) to every address
refs = collections.defaultdict(list)
for m in re.finditer(b'[' + bytes([0xe8, 0xe9]) + b']', tb):
    i = m.start()
    if i + 5 <= len(tb):
        t = (txt[1] + i + 5 + struct.unpack_from('<i', tb, i + 1)[0]) & 0xffffffff
        if txt[1] <= t < txt[1] + txt[2]:
            refs[t].append(('call' if tb[i] == 0xe8 else 'jmp', txt[1] + i))
for nm, va, sz, rp in secs:
    if nm in ('.rdata', '.data'):
        for k in range(0, min(sz, len(d) - rp) - 3, 4):
            t = struct.unpack_from('<I', d, rp + k)[0]
            if txt[1] <= t < txt[1] + txt[2]:
                refs[t].append(('slot', va + k))
for m in re.finditer(b'[' + bytes([0x68, 0xb8, 0xb9, 0xba, 0xbb, 0xbe, 0xbf]) + b'](.{4})', tb, re.S):
    t = struct.unpack('<I', m.group(1))[0]
    if txt[1] <= t < txt[1] + txt[2]:
        refs[t].append(('imm', txt[1] + m.start()))
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def trivial(a):
    ins = []
    for i in md.disasm(rd(a, 24), a):
        ins.append(i)
        if i.mnemonic == 'ret' or len(ins) > 6:
            break
    return bool(ins) and ins[-1].mnemonic == 'ret' and len(ins) <= 3 and not any(x.mnemonic in ('call',) for x in ins)


stub_rx = re.compile(r'NOTSA_UNREACHABLE|assert\(\s*(?:0|false)\s*\)|[Nn]ot reversed|[Nn]ot implemented|[Uu]nimplemented|\bTODO\b')
addr_rx = re.compile(r'0x([0-9A-Fa-f]{6})\b')
out = []
for p in sorted((Path(args.repo) / 'source').rglob('*.cpp')):
    lines = p.read_text(encoding='utf-8', errors='replace').splitlines()
    for i, ln in enumerate(lines):
        # function definition start: `<ret> Class::Name(...) {` at column 0
        if re.match(r'^[\w:<>\*&~\s,]+\b[\w~]+::[\w~]+\s*\(', ln) and not ln.rstrip().endswith(';') and ('{' in ln or (i + 1 < len(lines) and lines[i + 1].strip() == '{')):
            # address from the 1-3 preceding comment lines
            addr = None
            for k in range(1, 4):
                if i - k >= 0:
                    m = addr_rx.search(lines[i - k])
                    if m and (lines[i - k].lstrip().startswith(('//', '*', '/*'))):
                        addr = int(m.group(1), 16); break
            if addr is None:
                continue
            # body
            depth, j, body = 0, i, []
            started = False
            while j < len(lines):
                body.append(lines[j])
                depth += lines[j].count('{') - lines[j].count('}')
                if '{' in lines[j]:
                    started = True
                if started and depth <= 0:
                    break
                j += 1
            btxt = '\n'.join(body)
            code = '\n'.join(re.sub(r'//.*', '', ln2) for ln2 in re.sub(r'/[*].*?[*]/', '', btxt, flags=re.S).splitlines())
            inner = code[code.find('{') + 1:code.rfind('}')].strip()
            kind = None
            if stub_rx.search(btxt) and len(inner.splitlines()) <= 6:
                kind = 'STUB'
            elif inner == '':
                kind = 'EMPTY'
            if kind and not trivial(addr) and rd(addr, 1):
                r = refs.get(addr, [])
                out.append((kind, p.as_posix(), i + 1, ln.strip()[:80], addr, len(r), sorted({x[0] for x in r})))
live = [o for o in out if o[5]]
print('%d stubs/empty bodies with a non-trivial exe function at their address; %d have references in the exe (callers / vtable slot / pushed pointer):' % (len(out), len(live)))
for o in sorted(live, key=lambda o: (o[0], o[1])):
    print('%-5s %#x refs=%-3d %-18s %s:%d  %s' % (o[0], o[4], o[5], ','.join(o[6]), o[1], o[2], o[3]))
print('--- without any exe reference (dead code in the exe):')
for o in sorted([o for o in out if not o[5]], key=lambda o: o[1]):
    print('%-5s %#x %s:%d  %s' % (o[0], o[4], o[1], o[2], o[3]))
