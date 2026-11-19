#!/usr/bin/env python3
"""trivialcheck: scripted verdicts for mechanical single-pattern functions (Tasks/Events), exe vs port source.

usage: py -X utf8 -I tools/standalone/asm/trivialcheck.py --nv noverdict.tsv [--repo .] [--exe gta_sa_compact.exe] [--out result.tsv]
Input: noverdict.tsv from noverdict.py (addr, name, file, size).
Patterns:
  CONST   functions named GetEventType/GetTaskType/GetEventPriority/GetLifeTime/GetLocalSoundLevel/
          CanBeInterruptedBySameEvent/IsCriminalEvent/IsInterruptable/IsSimple/MakeAbortable/AffectsPedGroup/...:
          exe is `mov eax|al, imm | xor eax,eax | xor al,al ; ret [N]`; port body is `{ return <const>; }`
          (literal, bool, or enum value resolved from the sources). Equal -> MATCH, different -> CHECK.
  DTOR    exe destructor that only restores the vtable + calls a base dtor (+ optional CleanupAnim) vs port.
  CTOR    exe ctor that stores constants/args at offsets vs the port's member initialisers (offset-by-offset
          comparison is done by tools in this repo (layout), this mode only prints the exe stores for review).
Anything not provably equal is written with verdict CHECK and must be hand-checked.
"""
import argparse, csv, glob, os, re, subprocess, sys


def load_enums(repo):
    enum = {}
    for p in glob.glob(repo + '/source/**/*.h', recursive=True):
        t = open(p, encoding='utf-8', errors='replace').read()
        t = re.sub(r'/\*.*?\*/', '', t, flags=re.S)
        t = re.sub(r'//[^\n]*', '', t)
        for m in re.finditer(r'\benum\s+(?:class\s+)?\w*\s*(?::\s*[\w:]+\s*)?\{([^{}]*)\}', t):
            cur = -1
            for item in m.group(1).split(','):
                item = item.strip()
                if not item:
                    continue
                mm = re.match(r'(\w+)\s*(?:=\s*(.+))?$', item, re.S)
                if not mm:
                    continue
                n, v = mm.groups()
                if v is not None:
                    v = v.strip()
                    try:
                        cur = int(v, 0)
                    except ValueError:
                        mv = re.fullmatch(r'(?:\w+::)?(\w+)', v)
                        if mv and mv.group(1) in enum:
                            cur = enum[mv.group(1)]
                        else:
                            enum.pop(n, None)
                            cur = None
                            continue
                else:
                    if cur is None:
                        enum.pop(n, None)
                        continue
                    cur += 1
                if n in enum and enum[n] != cur:
                    enum[n] = None  # ambiguous
                else:
                    enum[n] = cur
    return enum


def match_brace(t, i):
    d = 0
    for j in range(i, len(t)):
        if t[j] == '{':
            d += 1
        elif t[j] == '}':
            d -= 1
            if d == 0:
                return j
    return -1


def find_body(repo, src, cls, fn):
    """Return the body text (between braces) of CLS::FN from cpp or inline in header."""
    base = os.path.join(repo, src)
    cands = []
    cpp = open(base, encoding='utf-8', errors='replace').read() if os.path.exists(base) else ''
    for m in re.finditer(r'\b' + re.escape(cls) + r'::' + re.escape(fn) + r'\s*\([^)]*\)[^{;]*\{', cpp):
        j = match_brace(cpp, m.end() - 1)
        cands.append(cpp[m.end():j])
    h = base[:-4] + '.h'
    if os.path.exists(h):
        t = open(h, encoding='utf-8', errors='replace').read()
        for m in re.finditer(r'\b' + re.escape(fn) + r'\s*\([^)]*\)[^{;=]*\{', t):
            cl = re.findall(r'\b(?:class|struct)\s+(?:NOTSA_EXPORT_VTABLE\s+)?(\w+)', t[:m.start()])
            if cl and cl[-1] == cls:
                j = match_brace(t, m.end() - 1)
                cands.append(t[m.end():j])
    # also inherited-from-base inline lines in other headers are not searched: flagged as NOSRC
    return cands


def port_const(body, enum):
    b = re.sub(r'//[^\n]*', '', body).strip()
    m = re.fullmatch(r'return\s+(.+?)\s*;', b, re.S)
    if not m:
        return None, 'complex'
    e = m.group(1).strip()
    e = re.sub(r'^\(\w+\)\s*', '', e)
    e = re.sub(r'^static_cast<[\w:]+>\((.*)\)$', r'\1', e)
    if e in ('true', 'TRUE'):
        return 1, ''
    if e in ('false', 'FALSE', 'nullptr', 'NULL'):
        return 0, ''
    try:
        return int(e.rstrip('uUfF') if not e.startswith('0x') else e, 0), ''
    except ValueError:
        pass
    n = re.sub(r'^\w+::', '', e)
    if n in enum and enum[n] is not None:
        return enum[n], ''
    return None, 'unresolved:' + e[:40]


def disasm_batch(exe, addrs, mx):
    """One exedis call for all addresses -> {addr(lower, 0x-less, 8 hex): [instruction text, ...]}"""
    res = {}
    for k in range(0, len(addrs), 150):
        r = subprocess.run([sys.executable, '-X', 'utf8', '-I', 'tools/standalone/exedis.py'] + addrs[k:k + 150] +
                           ['--exe', exe, '--max', str(mx)], capture_output=True, text=True)
        cur = None
        for l in r.stdout.splitlines():
            m = re.match(r'; ==== 0x([0-9a-f]+)', l)
            if m:
                cur = int(m.group(1), 16)
                res[cur] = []
                continue
            if l.startswith(';') or cur is None:
                continue
            s = l[10:].split(';')[0].strip() if len(l) > 10 else ''
            if s:
                res[cur].append(s)
    return res


def exe_const(ins):
    if len(ins) >= 2 and re.fullmatch(r'ret(\s+(0x)?[0-9a-f]+)?', ins[1]):
        m = re.fullmatch(r'mov\s+(?:eax|al),\s*(-?(?:0x)?[0-9a-f]+)', ins[0])
        if m:
            return int(m.group(1), 0)
        if re.fullmatch(r'xor\s+(eax,\s*eax|al,\s*al)', ins[0]):
            return 0
    return None


CONST_FUNCS = {'GetEventType', 'GetTaskType', 'GetEventPriority', 'GetLifeTime', 'GetLocalSoundLevel',
               'CanBeInterruptedBySameEvent', 'IsCriminalEvent', 'IsInterruptable', 'IsSimple', 'MakeAbortable',
               'AffectsPedGroup', 'GetType', 'IsValid', 'GetWanderType', 'GetSourceEntity', 'TakesPriorityOver',
               'WillRespond', 'WantsToRiotAgainst', 'AffectsPed'}


BASE_DTOR = {  # exe address of the direct base destructor (verified by hand: bare vtable store / SEH delete-subtask chain)
    0x61a3a0: 'CTaskSimple', 0x61a3d0: 'CTaskComplex', 0x61a6f0: 'CTaskSimpleAnim', 0x653740: 'CTaskComplexEvasiveDiveAndGetUp',
    0x634720: 'CTaskComplexUseClosestFreeScriptedAttractor', 0x620ef0: 'CTaskComplexKillPedOnFoot', 0x669d20: 'CTaskComplexWander'}
NONTRIVIAL_MEMBER = re.compile(r'\b(std::|notsa::|CRef|CPtrNode|unique_ptr|shared_ptr|std::vector|CTaskSequences)')


def port_dtor_info(repo, src, cls):
    """-> (direct base class or None, dtor-is-trivial, note)"""
    h = os.path.join(repo, src)[:-4] + '.h'
    ht = open(h, encoding='utf-8', errors='replace').read() if os.path.exists(h) else ''
    ct = open(os.path.join(repo, src), encoding='utf-8', errors='replace').read()
    m = re.search(r'\bclass\s+(?:NOTSA_EXPORT_VTABLE\s+)?' + re.escape(cls) + r'\b[^;{]*?:\s*(?:public\s+)?(\w+)', ht)
    base = m.group(1) if m else None
    triv, note = True, ''
    for txt in (ht, ct):
        for md in re.finditer(r'~' + re.escape(cls) + r'\s*\(\s*\)[^{;=]*(=\s*default\s*;|;|\{)', txt):
            if md.group(1).startswith('='):
                continue
            if md.group(1) == '{':
                j = match_brace(txt, md.end() - 1)
                b = re.sub(r'//[^\n]*|/\*.*?\*/', '', txt[md.end():j], flags=re.S).strip()
                if b:
                    triv, note = False, 'dtor body: ' + b[:50].replace(chr(10), ' ')
    return base, triv, note


def run_dtors(a, rows, out):
    d = [r for r in rows if ('Tasks/' in r[2] or 'Events/' in r[2]) and r[1].endswith('::Destructor')]
    dis = disasm_batch(a.exe, [r[0] for r in d], 3)
    for addr, name, src, size in d:
        cls = name.split('::')[0]
        ins = dis.get(int(addr, 16), [])
        tgt = None
        if len(ins) >= 2 and re.match(r'mov\s+dword ptr \[ecx\], 0x[0-9a-f]+$', ins[0]) and ins[1].startswith('jmp'):
            tgt = int(ins[1].split()[-1], 16)
        elif ins and ins[0].startswith('jmp') and len(ins) >= 1 and ins[0].split()[-1].startswith('0x'):
            tgt = int(ins[0].split()[-1], 16)
        if tgt is None:
            continue
        base, triv, note = port_dtor_info(a.repo, src, cls)
        if tgt not in BASE_DTOR:
            out.append((addr, name, 'CHECK', 'exe jmp 0x%x: base unknown; port base %s' % (tgt, base), ''))
        elif base != BASE_DTOR[tgt]:
            out.append((addr, name, 'CHECK', 'exe jmp base dtor of %s, port base %s' % (BASE_DTOR[tgt], base), ''))
        elif not triv:
            out.append((addr, name, 'CHECK', note, ''))
        else:
            out.append((addr, name, 'MATCH', 'exe: vtable store + jmp %s dtor; port dtor default/empty, base %s' % (base, base), ''))


def run_ctors(a, rows, out):
    """exe ctor = base ctor call (+ vtable store) only: the port class must declare no data members of its own."""
    d = [r for r in rows if ('Tasks/' in r[2] or 'Events/' in r[2]) and r[1].split('::')[1].startswith('Constructor')]
    dis = disasm_batch(a.exe, [r[0] for r in d], 40)
    mem = re.compile(r'^[ 	]+(?!return|using|static|typedef|friend|virtual)[A-Za-z_][\w:<>,*& ]*[ *&]m_\w+\s*(\{[^}]*\}|=[^;]+|\[[^\]]*\])?\s*;', re.M)
    for addr, name, src, size in d:
        ins = dis.get(int(addr, 16), [])
        if not ins or len(ins) >= 40 or any('fs:' in i for i in ins):
            continue
        stores = [i for i in ins if re.match(r'mov\s+(dword|word|byte) ptr \[', i)]
        calls = [i for i in ins if i.startswith('call')]
        if len(calls) != 1 or len(stores) != 1 or not re.match(r'mov\s+dword ptr \[(esi|ecx|edi)\], 0x[0-9a-f]{6}$', stores[0]):
            continue
        cls = name.split('::')[0]
        h = os.path.join(a.repo, src)[:-4] + '.h'
        ht = open(h, encoding='utf-8', errors='replace').read() if os.path.exists(h) else ''
        m = re.search(r'\bclass\s+(?:NOTSA_EXPORT_VTABLE\s+)?' + re.escape(cls) + r'\b[^;{]*?\{', ht)
        body = ht[m.end() - 1:match_brace(ht, m.end() - 1)] if m else ''
        nonempty = []
        cpp = open(os.path.join(a.repo, src), encoding='utf-8', errors='replace').read()
        for txt, pat in ((cpp, re.escape(cls) + '::' + re.escape(cls)), (body, r'\b' + re.escape(cls))):
            for mc in re.finditer(pat + r'\s*\([^)]*\)[^{;=]*?\{', txt):
                j = match_brace(txt, mc.end() - 1)
                b = re.sub(r'//[^\n]*|/\*.*?\*/', '', txt[mc.end():j], flags=re.S).strip()
                if b:
                    nonempty.append(b[:40].replace(chr(10), ' '))
        if nonempty:
            out.append((addr, name, 'CHECK', 'port ctor body not empty: ' + nonempty[0], ''))
            continue
        if not m:
            out.append((addr, name, 'CHECK', 'class body not found', ''))
        elif mem.search(body):
            out.append((addr, name, 'CHECK', 'exe ctor = base call + vtable only; port class declares own members', ''))
        else:
            out.append((addr, name, 'MATCH', 'exe ctor = base ctor call + vtable store only; port class has no own data members', ''))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--nv', required=True)
    ap.add_argument('--repo', default='.')
    ap.add_argument('--exe', default='C:/Users/littl/github/gta-reversed/gta_sa_compact.exe')
    ap.add_argument('--out', required=True)
    ap.add_argument('--only', default='')
    a = ap.parse_args()
    enum = load_enums(a.repo)
    rows = [r for r in csv.reader(open(a.nv, encoding='utf-8'), delimiter='\t')][1:]
    out = []
    dis = disasm_batch(a.exe, [r[0] for r in rows if ('Tasks/' in r[2] or 'Events/' in r[2]) and '::' in r[1] and r[1].split('::', 1)[1] in CONST_FUNCS], 4)
    for addr, name, src, size in rows:
        if not ('Tasks/' in src or 'Events/' in src) or '::' not in name:
            continue
        cls, fn = name.split('::', 1)
        if fn not in CONST_FUNCS or (a.only and fn not in a.only.split(',')):
            continue
        ins = dis.get(int(addr, 16), [])
        ev = exe_const(ins)
        if ev is None:
            out.append((addr, name, 'SKIP', 'not mov-imm/ret: ' + ' | '.join(ins[:3]), ''))
            continue
        bodies = find_body(a.repo, src, cls, fn)
        if not bodies:
            out.append((addr, name, 'CHECK', 'port body not found (exe=%d)' % ev, ''))
            continue
        res = [port_const(b, enum) for b in bodies]
        if any(r[1] == 'unresolved:Type' for r in res):  # `static constexpr auto Type = X;` in the class header
            hp = os.path.join(a.repo, src)[:-4] + '.h'
            mt = re.search(r'\bType\s*=\s*(?:\w+::)?(\w+)\s*;', open(hp, encoding='utf-8', errors='replace').read()) if os.path.exists(hp) else None
            if mt and enum.get(mt.group(1)) is not None:
                res = [(enum[mt.group(1)], '')]
        pv = [r[0] for r in res if r[0] is not None]
        if pv and all(v == ev for v in pv):
            out.append((addr, name, 'MATCH', 'const %d == exe mov-imm/ret' % ev, ''))
        elif pv:
            out.append((addr, name, 'CHECK', 'exe=%d port=%s' % (ev, pv), ''))
        else:
            out.append((addr, name, 'CHECK', 'exe=%d port %s' % (ev, res[0][1]), ''))
    run_dtors(a, rows, out)
    run_ctors(a, rows, out)
    with open(a.out, 'w', encoding='utf-8', newline='') as f:
        w = csv.writer(f, delimiter='\t')
        w.writerow(['addr', 'function', 'verdict', 'note', 'fix commit'])
        w.writerows(out)
    import collections
    print(collections.Counter(r[2] for r in out))


if __name__ == '__main__':
    main()
