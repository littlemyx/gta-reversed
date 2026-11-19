#!/usr/bin/env python3
"""trivialcheck: scripted verdicts for mechanical single-pattern functions (Tasks/Events), exe vs port source.

usage: py -X utf8 tools/standalone/asm/trivialcheck.py --nv noverdict.tsv [--repo .] [--exe gta_sa_compact.exe] [--out result.tsv]
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
import argparse, collections, csv, glob, json, os, re, subprocess, sys


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


# ---------------------------------------------------------------------------------------------------------------------
# P4R27 --scan mode: every hooked function of source/ whose exe body is a one-liner, vs the port one-liner.
#   NOP      exe `ret [N]`                                  port body empty
#   THISRET  exe `mov eax,ecx ; ret [N]`                    port `return this;`
#   CONST    exe `mov eax|al, imm | xor ; ret [N]`          port `return <const>;`
#   FCONST   exe `fld dword [abs] ; ret [N]`                port `return <float literal>;` (float read from the exe image)
#   GETTER   exe `mov eax|al|ax,[ecx+off] / movzx / movsx / fld [ecx+off] / lea eax,[ecx+off] ; ret [N]`   port `return m_x;` (`&m_x`)
#   SETTER   exe `mov eax|al|ax,[esp+4] ; mov [ecx+off],eax|al|ax ; ret 4`   port `m_x = arg;`
#   SETCONST exe `mov dword|word|byte [ecx+off], imm ; ret [N]`                port `m_x = <const>;`
# Member offset / width / float-ness of the port member are proven by a generated translation unit of static_asserts that is
# compiled with the build's own flags (/Zs; the MSVC environment must be loaded). Not provably equal -> CHECK (hand check).
# ---------------------------------------------------------------------------------------------------------------------
import struct

CAT_RENDER = re.compile(r'/(Fx|RenderWare|Pipelines|Shadows?|Coronas?|Rendering|Particle)|Corona|Shadow|Sprite|Font|Skidmarks|Water|Weather|Clouds|RwHelper|RenderBuffer|Light|Occlusion|Pipeline')


def category(src):
    if '/Audio/' in src:
        return 'audio'
    if CAT_RENDER.search(src.replace('game_sa/', '/', 1)):
        return 'render'
    return 'logic'


class Exe:
    def __init__(self, path):
        d = open(path, 'rb').read()
        self.d = d
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        nsec = struct.unpack_from('<H', d, pe + 6)[0]
        osz = struct.unpack_from('<H', d, pe + 20)[0]
        base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
        so = pe + 24 + osz
        self.secs = []
        for i in range(nsec):
            n, vs, va, rs, rp = struct.unpack_from('<8sIIII', d, so + 40 * i)
            self.secs.append((va + base, max(vs, rs), rp))

    def rd(self, x, n):
        for va, sz, rp in self.secs:
            if va <= x < va + sz:
                return self.d[rp + x - va:rp + x - va + n]
        return b''


NUM = r'(-?(?:0x[0-9a-f]+|\d+))'
RET = r'ret(?:\s+(?:0x)?[0-9a-f]+)?'
THIS = r'\[ecx(?:\s*\+\s*' + NUM + r')?\]'
SZ = {'dword': 4, 'byte': 1, 'word': 2, 'eax': 4, 'al': 1, 'ax': 2}


def exe_pattern(ins, exe):
    """-> (kind, dict) or None"""
    if not ins:
        return None
    n = len(ins)
    if re.fullmatch(RET, ins[0]):
        return 'NOP', {}
    if n >= 2 and re.fullmatch(RET, ins[1]):
        c = exe_const(ins)
        if c is not None:
            return 'CONST', {'v': c}
        m = re.fullmatch(r'fld\s+dword ptr \[(0x[0-9a-f]+)\]', ins[0])
        if m:
            b = exe.rd(int(m.group(1), 16), 4)
            if len(b) == 4:
                return 'FCONST', {'v': struct.unpack('<f', b)[0]}
        m = re.fullmatch(r'mov\s+(eax|al|ax),\s*(dword|byte|word) ptr ' + THIS, ins[0])
        if m:
            return 'GETTER', {'off': int(m.group(3) or '0', 0), 'sz': SZ[m.group(2)], 'fl': 0}
        m = re.fullmatch(r'mov(?:zx|sx)\s+eax,\s*(byte|word) ptr ' + THIS, ins[0])
        if m:
            return 'GETTER', {'off': int(m.group(2) or '0', 0), 'sz': SZ[m.group(1)], 'fl': 0}
        m = re.fullmatch(r'fld\s+dword ptr ' + THIS, ins[0])
        if m:
            return 'GETTER', {'off': int(m.group(1) or '0', 0), 'sz': 4, 'fl': 1}
        m = re.fullmatch(r'lea\s+eax,\s*\[ecx(?:\s*\+\s*' + NUM + r')?\]', ins[0])
        if m:
            return 'GETTER', {'off': int(m.group(1) or '0', 0), 'sz': 0, 'fl': 0, 'addr': 1}
        if re.fullmatch(r'mov\s+eax,\s*ecx', ins[0]):
            return 'THISRET', {}
        m = re.fullmatch(r'mov\s+(dword|word|byte) ptr ' + THIS + r',\s*' + NUM, ins[0])
        if m:
            return 'SETCONST', {'off': int(m.group(2) or '0', 0), 'sz': SZ[m.group(1)], 'v': int(m.group(3), 0)}
    if n >= 3 and re.fullmatch(r'ret\s+(0x)?[0-9a-f]+', ins[2]):
        m1 = re.fullmatch(r'mov\s+(eax|al|ax),\s*(?:dword|byte|word) ptr \[esp \+ 4\]', ins[0])
        m2 = re.fullmatch(r'mov\s+(?:dword|byte|word) ptr ' + THIS + r',\s*(eax|al|ax)', ins[1])
        if m1 and m2 and m1.group(1) == m2.group(2):
            return 'SETTER', {'off': int(m2.group(1) or '0', 0), 'sz': SZ[m1.group(1)]}
    return None


def find_fns(repo, src, cls, fn):
    """-> (cls, [(params text, body text)]) for CLS::FN (cpp) or inline FN of class CLS (header next to the cpp).
    cls may be '' (hook name without class): the class is taken from the unique `X::FN(` definition."""
    base = os.path.join(repo, src)
    res = []
    cpp = open(base, encoding='utf-8', errors='replace').read() if os.path.exists(base) else ''
    if cls:
        pat = r'\b(' + re.escape(cls) + r')::' + re.escape(fn) + r'\s*\(([^()]*)\)[^{;()]*\{'
    else:
        pat = r'\b(\w+)::' + re.escape(fn) + r'\s*\(([^()]*)\)[^{;()]*\{'
    found = set()
    for m in re.finditer(pat, cpp):
        found.add(m.group(1))
        res.append((m.group(1), m.group(2), cpp[m.end():match_brace(cpp, m.end() - 1)]))
    if not cls and not res:  # genuinely free function
        for m in re.finditer(r'(?<![\w:.>])' + re.escape(fn) + r'\s*\(([^()]*)\)[^{;()=]*\{', cpp):
            res.append(('', m.group(1), cpp[m.end():match_brace(cpp, m.end() - 1)]))
    h = base[:-4] + '.h'
    if os.path.exists(h) and (cls or not res):
        t = open(h, encoding='utf-8', errors='replace').read()
        for m in re.finditer(r'\b' + re.escape(fn) + r'\s*\(([^()]*)\)[^{;=()]*\{', t):
            cl = re.findall(r'\b(?:class|struct)\s+(?:NOTSA_EXPORT_VTABLE\s+)?(\w+)\s*(?:final\s*)?(?::[^;{]*)?\{', t[:m.start()])
            if cl and (not cls or cl[-1] == cls):
                res.append((cl[-1], m.group(1), t[m.end():match_brace(t, m.end() - 1)]))
    return res


def clean(b):
    b = re.sub(r'/\*.*?\*/', '', b, flags=re.S)
    b = re.sub(r'//[^\n]*', '', b)
    b = re.sub(r'\bZoneScoped(?:N\([^)]*\))?\s*;', '', b)  # profiler zone: no-op in the run build
    b = re.sub(r'#if\w*[^\n]*\n\s*#endif', '', b)
    return b.strip()


def strip_casts(e):
    e = e.strip()
    while True:
        m = re.fullmatch(r'(?:static_cast|reinterpret_cast|const_cast)<[^()]*>\((.*)\)', e, re.S)
        if m:
            e = m.group(1).strip()
            continue
        m = re.fullmatch(r'\((?:const\s+)?[\w:]+\s*\**\)\s*(.+)', e, re.S)
        if m:
            e = m.group(1).strip()
            continue
        break
    return re.sub(r'^this->', '', e)


def parse_param(params):
    """'float factor' -> ('float', 'factor') or None"""
    m = re.fullmatch(r'((?:const\s+)?[\w:]+(?:<[^<>]*>)?\s*[*&]?)\s*(\w+)', params.strip())
    return (m.group(1).strip(), m.group(2)) if m else None


def run_scan(a, rows):
    exe = Exe(a.exe)
    enum = load_enums(a.repo)
    enum.update({'E_NOTIMPL': 0x80004001, 'E_FAIL': 0x80004005, 'E_NOINTERFACE': 0x80004002, 'S_OK': 0, 'S_FALSE': 1, 'rpATOMIC': 1, 'rpCLUMP': 2})
    cand = [r for r in rows if int(r[3], 16) <= 0x30 and '::Constructor' not in r[1] and '::Destructor' not in r[1]]
    dis = disasm_batch(a.exe, [r[0] for r in cand], 5)
    out, skip = [], collections.Counter()
    pend = []  # members to prove by static_assert
    for addr, name, src, size in cand:
        ins = dis.get(int(addr, 16), [])
        pat = exe_pattern(ins, exe)
        if pat is None:
            continue
        kind, info = pat
        cat = category(src)
        cls, fn = name.rsplit('::', 1) if '::' in name else ('', name)
        fns = find_fns(a.repo, src, cls, fn)
        if not fns:
            skip[kind + ':nobody'] += 1
            continue
        if not cls and len({c for c, _, _ in fns}) == 1:
            cls = fns[0][0]
            name = cls + '::' + fn if cls else name
        bodies = [(p, clean(b)) for _, p, b in fns]
        asm = '%s +0x%x' % (addr, 2 * len(ins))
        if kind == 'NOP':
            if all(b == '' or b == 'return;' for _, b in bodies):
                out.append([addr, name, 'MATCH', asm, 'exe bare ret; port body empty', kind, cat])
            else:
                out.append([addr, name, 'CHECK', asm, 'exe bare ret; port body: ' + bodies[0][1][:60].replace('\n', ' '), kind, cat])
            continue
        if kind == 'THISRET':
            if all(re.fullmatch(r'return\s+\*?this\s*;', b) for _, b in bodies):
                out.append([addr, name, 'MATCH', asm, 'exe mov eax,ecx; ret; port returns this', kind, cat])
            else:
                skip['THISRET:complex'] += 1
            continue
        if kind in ('CONST', 'FCONST'):
            pv = []
            for _, b in bodies:
                mm = re.fullmatch(r'return\s+(.+?)\s*;', b, re.S)
                if not mm:
                    pv.append(None)
                    continue
                if kind == 'FCONST':
                    mf = re.fullmatch(r'(-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)f?', strip_casts(mm.group(1)))
                    pv.append(struct.unpack('<f', struct.pack('<f', float(mf.group(1))))[0] if mf else None)
                else:
                    pv.append(port_const(b, enum)[0])
            if None in pv:
                skip[kind + ':port-nonconst'] += 1
                if a.verbose:
                    print('  nonconst', name, [b[:50] for _, b in bodies])
            elif all((v == info['v']) if kind == 'FCONST' else ((v & 0xFFFFFFFF) == (info['v'] & 0xFFFFFFFF)) for v in pv):
                out.append([addr, name, 'MATCH', asm, 'const %r == exe' % (info['v'],), kind, cat])
            else:
                out.append([addr, name, 'CHECK', asm, 'exe=%r port=%r' % (info['v'], pv), kind, cat])
            continue
        if len(bodies) != 1 or not cls:
            skip[kind + ':overload/free'] += 1
            continue
        params, b = bodies[0]
        if kind == 'GETTER':
            mm = re.fullmatch(r'return\s+(&?)\s*(.+?)\s*;', b, re.S)
            e = strip_casts(mm.group(2)) if mm else ''
            if not mm or not re.fullmatch(r'\w+', e) or (mm.group(1) and not info.get('addr')):
                skip['GETTER:complex'] += 1
                continue
            pend.append((addr, name, kind, cat, asm, cls, e, info, None, src))
        elif kind == 'SETTER':
            pp = parse_param(params)
            mm = re.fullmatch(r'(?:this->)?(\w+)\s*=\s*(.+?)\s*;', b, re.S)
            if not (pp and mm) or strip_casts(mm.group(2)) != pp[1]:
                skip['SETTER:complex'] += 1
                continue
            pend.append((addr, name, kind, cat, asm, cls, mm.group(1), info, pp[0], src))
        elif kind == 'SETCONST':
            mm = re.fullmatch(r'(?:this->)?(\w+)\s*=\s*(.+?)\s*;', b, re.S)
            v = port_const('return %s;' % strip_casts(mm.group(2)), enum)[0] if mm else None
            if v is None:
                skip['SETCONST:complex'] += 1
            elif (v & (2 ** (8 * info['sz']) - 1)) != (info['v'] & (2 ** (8 * info['sz']) - 1)):
                out.append([addr, name, 'CHECK', asm, 'exe stores %d port %d' % (info['v'], v), kind, cat])
            else:
                pend.append((addr, name, kind, cat, asm, cls, mm.group(1), info, None, src))
    # ---- compile the static_assert TU (one assertion line per pending member; chunks, because MSVC stops after 100 errors)
    cc = json.load(open(os.path.abspath(os.path.join(a.repo, a.cc))))
    ent = next(e for e in cc if e['file'].replace(chr(92), '/').endswith('Entity/Ped/Ped.cpp'))
    cmd0 = re.sub(r'\s/Fo\S+|\s/Fd\S+|\s/FS|\s-c\s.*$|\s/c\s.*$|/Y[cu]\S*|/Fp\S+|/FI\S+|/MP', '', ent['command'])
    cmd0 += ' /D_ALLOW_KEYWORD_MACROS /Zs /nologo "%s"' % os.path.abspath(os.path.join(a.repo, a.tu))
    errs = {}

    def build_line(p):
        addr, name, kind, cat, asm, cls, mem, info, ptype, src = p
        s = 'static_assert(offsetof(%s, %s) == %d, "OFF");' % (cls, mem, info['off'])
        M = 'decltype(%s::%s)' % (cls, mem)
        if not info.get('addr'):
            s += ' static_assert(sizeof(%s) == %d, "SIZE");' % (M, info['sz'])
            if kind == 'GETTER':
                s += ' static_assert(std::is_floating_point_v<%s> == %s, "KIND");' % (M, 'true' if info['fl'] else 'false')
            if kind == 'SETTER' and ptype:
                pt = re.sub(r'^const\s+', '', ptype).rstrip('&').strip()
                s += ' static_assert(sizeof(%s) == %d && std::is_floating_point_v<%s> == std::is_floating_point_v<%s>, "PARAM");' % (pt, info['sz'], pt, M)
        return s

    def check(idx):
        hdrs = sorted({os.path.abspath(os.path.join(a.repo, pend[i][9]))[:-4] + '.h' for i in idx})
        tu = ['#define private public', '#define protected public', '#include "StdInc.h"'] + ['#include "%s"' % h.replace(chr(92), '/') for h in hdrs if os.path.exists(h)] + ['#undef private', '#undef protected']
        ln0 = len(tu)
        open(os.path.join(a.repo, a.tu), 'w').write(chr(10).join(tu + [build_line(pend[i]) for i in idx]) + chr(10))
        r = subprocess.run(cmd0, capture_output=True, text=True, shell=True, cwd=ent.get('directory'))
        txt = r.stdout + r.stderr
        if 'C1003' in txt and len(idx) > 1:
            h = len(idx) // 2
            check(idx[:h])
            check(idx[h:])
            return
        if 'C1003' in txt or re.search(r'fatal error C1\d\d\d', txt) and 'C1003' not in txt:
            raise SystemExit('compile failed: ' + txt[:1500])
        for l in txt.splitlines():
            m = re.search(r'\((\d+)\)\s*:\s*error\s+(C\d+)\s*:\s*(.*)', l)
            if m and os.path.basename(a.tu) in l:
                errs.setdefault(idx[int(m.group(1)) - ln0 - 1], []).append(m.group(2) + ' ' + m.group(3)[:80])

    for k in range(0, len(pend), 60):
        check(list(range(k, min(k + 60, len(pend)))))
    for i, (addr, name, kind, cat, asm, cls, mem, info, ptype, src) in enumerate(pend):
        e = errs.get(i)
        if not e:
            w = 'addr' if info.get('addr') else 'sz%d%s' % (info['sz'], ' float' if info.get('fl') else '')
            out.append([addr, name, 'MATCH', asm, 'exe %s [ecx+0x%x] %s == port %s::%s (offset/width/float-ness by static_assert)' % (kind.lower(), info['off'], w, cls, mem), kind, cat])
        else:
            out.append([addr, name, 'CHECK', asm, 'exe off 0x%x sz%d vs port %s::%s: %s' % (info['off'], info['sz'], cls, mem, '; '.join(sorted(set(e)))[:150]), kind, cat])
    return out, skip


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--nv', required=True)
    ap.add_argument('--repo', default='.')
    ap.add_argument('--exe', default='C:/Users/littl/github/gta-reversed/gta_sa_compact.exe')
    ap.add_argument('--out', required=True)
    ap.add_argument('--only', default='')
    ap.add_argument('--scan', action='store_true', help='P4R27: one-liner patterns over all of source/ (needs MSVC env + --cc)')
    ap.add_argument('--cc', default='build/StandaloneRelease/compile_commands.json')
    ap.add_argument('--verbose', action='store_true')
    ap.add_argument('--tu', default='_p4r27_scan.cpp')
    a = ap.parse_args()
    enum = load_enums(a.repo)
    rows = [r for r in csv.reader(open(a.nv, encoding='utf-8'), delimiter='\t')][1:]
    if a.scan:
        out, skip = run_scan(a, rows)
        with open(a.out, 'w', encoding='utf-8', newline='') as f:
            w = csv.writer(f, delimiter='\t')
            w.writerow(['addr', 'function', 'verdict', 'asm range', 'note', 'fix commit', 'pattern', 'category'])
            w.writerows([r[:5] + [''] + r[5:] for r in out])
        print(sorted(collections.Counter((r[5], r[2]) for r in out).items()))
        print('unscripted:', dict(skip))
        return
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
    print(collections.Counter(r[2] for r in out))


if __name__ == '__main__':
    main()
