#!/usr/bin/env python3
"""ORACLE-W2: generates the exe-oracle rows for task / event constructors and Clone / CloneEditable (the allocator-backed families of wave 2).

Why generated: ~450 ported functions of the form `new T(args)` / `Clone() { return new T(m_a, m_b) }` and the constructors themselves are mechanical, and they are exactly where the playtest bugs came from:
a member the exe's constructor writes and the port's leaves uninitialised (or the reverse). The rows give both implementations the SAME garbage `this` (constructors) / garbage source object
(Clone), the same fake task / event pool, the same deterministic arena, and compare the constructed memory bit by bit (tests/standalone/wave2_oracle_common.inc, Cmp3).

Inputs: sites.json (tools/standalone/oracle_coverage.py --json), the repo (headers are parsed for the constructor wrapper signatures and the pointer members of the task classes).
usage: gen_w2_rows.py <repo> <sites.json> <out_dir> [--skip tools/w2_skip.tsv]
Outputs (in <out_dir> = tests/standalone): wave2_oracle_gen_ctor.inc, wave2_oracle_gen_clone.inc, wave2_oracle_gen_sources.cmake, wave2_oracle_gen_report.txt
A site is taken when: fn is Constructor* / Clone / CloneEditable, the class lives under game_sa/Tasks or game_sa/Events, no other oracle file mentions the address, and every constructor parameter type has a generator.
"""
import json, re, sys, os, glob, collections

RE_ADDR = re.compile(r'0[xX]([0-9A-Fa-f]{6})\b')
NL = chr(10)

def strip_comments(t):
    out = []; i = 0; n = len(t); instr = False
    while i < n:
        c = t[i]
        if instr:
            out.append(c)
            if c == chr(92) and i + 1 < n: out.append(t[i + 1]); i += 2; continue
            if c == '"': instr = False
            i += 1; continue
        if c == '"': instr = True; out.append(c); i += 1; continue
        if t.startswith('//', i):
            j = t.find(NL, i); i = n if j < 0 else j; continue
        if t.startswith('/*', i):
            j = t.find('*/', i + 2); i = n if j < 0 else j + 2; out.append(' '); continue
        out.append(c); i += 1
    return ''.join(out)

def flatten(body, access='public'):   # `access` = the default of the class key: 'private' for `class`, 'public' for `struct`
    """body of a class with nested braces replaced by {} placeholders -> list of (access, statement)"""
    stmts = []; cur = []; depth = 0; i = 0; n = len(body)

    acc_re = re.compile(r'^\s*(public|private|protected)\s*:')
    def flush():
        s = ''.join(cur).strip(); cur.clear()
        return s
    while i < n:
        c = body[i]
        if c == '{':
            j = i; d = 0
            while j < n:
                if body[j] == '{': d += 1
                elif body[j] == '}':
                    d -= 1
                    if d == 0: break
                j += 1
            pre = ''.join(cur).rstrip()
            pre2 = pre
            for _ in range(6):
                pre2 = re.sub(r'\b(const|override|noexcept|final|volatile)\b\s*$', '', pre2).rstrip()
            if pre2.endswith(')') or re.search(r'(class|struct|enum|union)\b[^;]*$', pre):
                cur.append('{}')
                s = flush(); stmts.append(s)
                # a following ';' ends nothing more (class bodies end with ';')
                i = j + 1
                if i < n and body[i] == ';': i += 1
                continue
            cur.append('{}'); i = j + 1; continue
        if c == ';':
            stmts.append(flush()); i += 1; continue
        cur.append(c); i += 1
    rest = flush()
    if rest: stmts.append(rest)
    res = []
    for s in stmts:
        while True:
            m = acc_re.match(s)
            if not m: break
            access = m.group(1); s = s[m.end():].strip()
        if s: res.append((access, s))
    return res

def class_bodies(path):
    """name -> (kind, bases, body) for every class/struct defined in a header"""
    try: t = strip_comments(open(path, encoding='utf-8', errors='replace').read())
    except OSError: return {}
    out = {}
    for m in re.finditer(r'\b(class|struct)\s+((?:NOTSA_EXPORT_VTABLE|[A-Z_]+_API|DllExport)\s+)?(\w+)\s*(final\s*)?(:\s*[^{;]*)?\{', t):
        name = m.group(3)
        j = m.end() - 1; d = 0; k = j
        while k < len(t):
            if t[k] == '{': d += 1
            elif t[k] == '}':
                d -= 1
                if d == 0: break
            k += 1
        bases = []; tmpl = bool(re.search(r'template\s*<[^;{]*>\s*$', t[:m.start()]))
        if m.group(5):
            for b in m.group(5)[1:].split(','):
                b = re.sub(r'\b(public|private|protected|virtual)\b', '', b).strip()
                if '<' in b and not b.strip().startswith('std::'): tmpl = True
                b = re.sub(r'<.*', '', b).strip()
                if b: bases.append(b)
        out[name] = (m.group(1), bases, t[j + 1:k], path, tmpl)
    return out

ENT_KIND = {}
for n in ('CPed', 'CCopPed', 'CPlayerPed', 'CCivilianPed', 'CAnimatedPed', 'CEmergencyPed'): ENT_KIND[n] = 'p'
for n in ('CVehicle', 'CAutomobile', 'CBike', 'CBoat', 'CHeli', 'CPlane', 'CTrain', 'CQuadBike', 'CBmx', 'CMonsterTruck', 'CTrailer', 'CFixedWingAircraft'): ENT_KIND[n] = 'v'
for n in ('CObject',): ENT_KIND[n] = 'o'
for n in ('CEntity', 'CPhysical', 'CBuilding', 'CDummy', 'CDummyObject'): ENT_KIND[n] = 'e'

INT_T = {'int8_t': 'int8_t', 'uint8_t': 'uint8_t', 'int16_t': 'int16_t', 'uint16_t': 'uint16_t', 'PedGroupId': 'int32_t', 'int': 'int32_t', 'int32': 'int32_t', 'int32_t': 'int32_t', 'uint32': 'uint32_t', 'uint32_t': 'uint32_t', 'unsigned': 'uint32_t', 'uint': 'uint32_t', 'int16': 'int16_t', 'uint16': 'uint16_t',
         'int8': 'int8_t', 'uint8': 'uint8_t', 'short': 'int16_t', 'char': 'int8_t', 'DWORD': 'uint32_t', 'size_t': 'uint32_t', 'int64': 'int64_t'}
ENUM_EXTRA = {'AnimationId', 'AssocGroupId', 'eAnimationFlags'}

def split_params(s):
    s = s.strip()
    if not s or s == 'void': return []
    out = []; d = 0; cur = ''
    for c in s:
        if c in '<(': d += 1
        if c in '>)': d -= 1
        if c == ',' and d == 0: out.append(cur.strip()); cur = ''
        else: cur += c
    out.append(cur.strip())
    return out

def norm_param(p):
    """parameter declaration -> type text without name / default (best effort)"""
    p = re.sub(r'=.*$', '', p).strip()
    p = re.sub(r'\s+', ' ', p)
    p = re.sub(r'\b(class|struct)\s+(?=\w)', '', p)    # elaborated type specifiers (`class CPed* ped`)
    m = re.match(r'^(.*?[\w>*&])\s+(\w+)$', p)
    if m and not re.match(r'^(const|unsigned|signed|struct|class)$', m.group(2)):
        t = m.group(1)
        # `unsigned int` / `const CVector` without a name: m.group(1) would be 'unsigned'
        if not t.endswith(('const', 'unsigned', 'signed')): p = t
    return p.strip()

NESTED_ENUMS = {}

def argspec(ptype, idx):
    """-> (decl line, port expr, exe type, exe expr) or None. ptype is the normalised parameter type."""
    t = ptype.replace('const ', ' const ').replace(' const', ' const')
    is_const = 'const' in ptype.split()
    base = re.sub(r'\bconst\b', '', ptype).replace(' ', '').rstrip()
    ref = base.endswith('&'); ptr = base.endswith('*')
    core = base.rstrip('&*')
    name = 'a%d' % idx
    if ptr and core.endswith('*'): return None            # pointer to pointer
    if ptr:
        if core in ENT_KIND:
            fn = {'p': 'GPedN', 'v': 'GVehN', 'o': 'GObjN', 'e': 'GEntN'}[ENT_KIND[core]]
            return ('%s* %s = static_cast<%s*>(%s(r));' % (core, name, core, fn), name, '%s*' % core, name)
        if core in ('CVector',):
            return ('CVector %s_v = GV(r); %sCVector* %s = &%s_v;' % (name, 'const ' if is_const else '', name, name), name, 'const CVector*', name)
        if core in ('CVector2D',):
            return ('CVector2D %s_v = GV2(r); %sCVector2D* %s = &%s_v;' % (name, 'const ' if is_const else '', name, name), name, 'const CVector2D*', name)
        if core == 'char':
            if is_const: return ('const char* %s = GStr(r);' % name, name, 'const char*', name)
            return ('char* %s = GStrM(r);' % name, name, 'char*', name)
        if core == 'CTask' or core.startswith('CTask'):
            return ('%s* %s = nullptr;' % (core, name), name, '%s*' % core, name)
        return ('%s%s* %s = nullptr;' % ('const ' if is_const else '', core, name), name, '%s*' % core if not is_const else 'const %s*' % core, name)
    if ref:
        if core == 'CVector':
            return ('CVector %s = GV(r);' % name, name, 'const CVector*', '&' + name)
        if core == 'CVector2D':
            return ('CVector2D %s = GV2(r);' % name, name, 'const CVector2D*', '&' + name)
        return None
    if core == 'bool':   return ('bool %s = GB(r);' % name, name, 'int', 'X(%s)' % name)
    if core == 'float':  return ('float %s = GFl(r);' % name, name, 'float', name)
    if core in INT_T:
        t = INT_T[core]
        gen = 'GU(r)' if t.startswith('u') else 'GI(r)'
        if t in ('uint8_t', 'int8_t'): gen = 'r.below(2)'   # 8-bit parameters are mostly bool flags (the wrapper types them uint8; the real constructor stores a bool, the exe stores the raw byte): 0 / 1 only
        if t in ('uint16_t', 'int16_t'): gen = 'r.below(3000)'
        decl = '%s %s = (%s)%s;' % (core, name, core, gen)
        sub = t in ('uint8_t', 'int8_t', 'uint16_t', 'int16_t')
        return (decl, name, 'int' if sub else core, 'X(%s)' % name if sub else name)
    if core == 'CVector': return ('CVector %s = GV(r);' % name, name, 'CVector', name)
    if core == 'CVector2D': return ('CVector2D %s = GV2(r);' % name, name, 'CVector2D', name)
    if core == 'eAbortPriority':   # 3 values only (LEISURE / URGENT / IMMEDIATE): the exe and the port decode other values differently and no caller passes them
        return ('eAbortPriority %s = (eAbortPriority)r.below(3);' % name, name, 'int', 'X(%s)' % name)
    if core in NESTED_ENUMS:
        q = NESTED_ENUMS[core]
        return ('%s %s = GE<%s>(r);' % (q, name, q), name, 'int', 'X(%s)' % name)
    if re.match(r'^e[A-Z]\w*$', core) or core in ENUM_EXTRA:
        return ('%s %s = GE<%s>(r);' % (core, name, core), name, 'int', 'X(%s)' % name)
    return None

class Index:
    def __init__(self, repo):
        self.repo = repo; self.cls = {}
        for h in glob.glob(repo + '/source/game_sa/**/*.h', recursive=True):
            for n, v in class_bodies(h).items():
                if n not in self.cls or v[0] == 'class': self.cls[n] = v
    def header_of(self, name): return self.cls[name][3] if name in self.cls else None
    def chain(self, name):
        out = []; seen = set()
        def rec(n):
            if n in seen or n not in self.cls: return
            seen.add(n); out.append(n)
            for b in self.cls[n][1]: rec(b)
        rec(name); return out
    def members(self, name):
        """pointer members of the class and its bases: [(owner, access, kind, name, array_len)]"""
        res = []
        for n in self.chain(name):
            kind, bases, body = self.cls[n][:3]
            stmts = flatten(body, 'private' if kind == 'class' else 'public')
            # nested enums declared in a private / protected section cannot be named by the offset trick: members of those types are not listed
            hidden = set()
            for access, st in stmts:
                me = re.match(r'^enum\s+(?:class\s+)?(\w+)', st)
                if me and access != 'public': hidden.add(me.group(1))
            for access, st in stmts:
                if '(' in st or st.startswith(('typedef', 'using', 'static', 'friend', 'enum', 'class', 'struct', 'template')): continue
                if hidden and any(re.search(r'(?<![\w:])' + h + r'(?![\w])', st) for h in hidden): continue
                access = ('public' if kind == 'struct' and False else access)
                m = re.match(r'^(?:notsa::)?EntityRef\s*<\s*(\w+)\s*>\s+(\w+)\s*(?:\{\})?(?:=.*)?$', st)
                if m:
                    res.append((n, access, ENT_KIND.get(m.group(1), 'n'), m.group(2), 1, 'notsa::EntityRef<%s>' % m.group(1), 0)); continue
                m = re.match(r'^(?:mutable\s+)?((?:const\s+)?)((?:volatile\s+)?)([\w:]+)\s*(\*+)\s*(\w+)\s*(?:\[\s*(\w+)\s*\])?\s*(?:=.*|\{\})?$', st)
                if m and ':' not in m.group(5):
                    ln = 1
                    if m.group(6):
                        ln = int(m.group(6)) if m.group(6).isdigit() else 0
                    res.append((n, access, ENT_KIND.get(m.group(3), 'n') if len(m.group(4)) == 1 else 'n', m.group(5), ln, m.group(1) + m.group(2) + m.group(3) + m.group(4), ln if m.group(6) else 0))
                    continue
                m = re.match(r'^(?:mutable\s+)?((?:notsa::)?WEnum[A-Za-z0-9_]*<[^;]*>|e[A-Z]\w*)\s+(\w+)\s*(?:=.*|\{.*\})?$', st)
                if m:
                    res.append((n, access, 'u', m.group(2), 1, m.group(1), 0))
                    continue
                m = re.match(r'^(?:mutable\s+)?(u?int8|u?int8_t)\s+((?:m_)?[bB][A-Z]\w*)\s*(?:=.*|\{.*\})?$', st)   # byte flags named bXxx / m_bXxx: bool-like (the exe copies the raw byte)
                if m:
                    res.append((n, access, 'b', m.group(2), 1, m.group(1), 0))
                    continue
                m = re.match(r'^(?:mutable\s+)?bool\s+(\w+)\s*(?:\[\s*(\w+)\s*\])?\s*(?:=.*|\{.*\})?$', st)
                if m:
                    ln = 1
                    if m.group(2): ln = int(m.group(2)) if m.group(2).isdigit() else 0
                    res.append((n, access, 'b', m.group(1), ln, 'bool', ln if m.group(2) else 0))
        return res

def default_access(index, owner):
    return 'struct' if index.cls[owner][0] == 'struct' else 'class'

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    repo, sites_json, out_dir = args[:3]
    skip = {}; refdiff = {}
    if '--skip' in sys.argv:
        for ln in open(sys.argv[sys.argv.index('--skip') + 1], encoding='utf-8'):
            ln = ln.strip()
            if ln and not ln.startswith('#'):
                k, _, why = ln.partition('\t')
                if k.startswith('refdiff:'): refdiff[k[8:].strip()] = why.strip()   # keep the row, but do not compare the entity reference world (a documented exe reference quirk)
                else: skip[k.strip()] = why.strip()
    sites = json.load(open(sites_json))
    covered = set()
    for f in glob.glob(os.path.join(repo, 'tests/standalone/*')):
        b = os.path.basename(f)
        if b.startswith('wave2_oracle_gen'): continue
        if f.endswith(('.cpp', '.inc', '.h')):
            for m in RE_ADDR.finditer(open(f, encoding='utf-8', errors='replace').read()): covered.add(int(m.group(1), 16))
    index = Index(repo)
    ctor_rows = []; clone_rows = []; report = []; srcs = set()
    base_tus = {'CTask': 'game_sa/Tasks/Task.cpp', 'CTaskSimple': 'game_sa/Tasks/TaskSimple.cpp', 'CTaskComplex': 'game_sa/Tasks/TaskComplex.cpp', 'CEvent': 'game_sa/Events/Event.cpp',
                'CEventEditableResponse': 'game_sa/Events/EventEditableResponse.cpp'}
    def tu_for(cls_name):
        h = index.header_of(cls_name)
        if not h: return None
        c = os.path.splitext(h)[0] + '.cpp'
        return os.path.relpath(c, os.path.join(repo, 'source')).replace(chr(92), '/') if os.path.exists(c) else None
    seen_rows = set(); robs = {}
    for s in sorted(sites, key=lambda s: (s['file'], s['addr'])):
        fn = s['fn']
        if not s['cls'] or not (s['sub'].startswith('game_sa/Tasks') or s['sub'].startswith('game_sa/Events')): continue
        is_ctor = fn.startswith('Constructor'); is_clone = fn in ('Clone', 'CloneEditable')
        if not (is_ctor or is_clone) or s['addr'] in covered: continue
        cls = s['cls']; key = '%s::%s' % (cls, fn)
        if key in skip: report.append('SKIP %s 0x%X: %s' % (key, s['addr'], skip[key])); continue
        if cls not in index.cls:
            report.append('NOHEADER %s 0x%X' % (key, s['addr'])); continue
        if any(index.cls[c][4] for c in index.chain(cls)):
            report.append('TEMPLATE %s 0x%X (class or base is a template)' % (key, s['addr'])); continue
        if (cls, s['addr']) in seen_rows: continue
        seen_rows.add((cls, s['addr']))
        hdr_rel = os.path.relpath(index.header_of(cls), os.path.join(repo, 'source')).replace(chr(92), '/')
        kind, bases, body = index.cls[cls][:3]
        if is_ctor:
            # parameter list: from the install line (overloaded: explicit pointer-to-member type), else the Constructor wrapper in the class
            line = open(os.path.join(repo, s['file']), encoding='utf-8', errors='replace').read().split(NL)[s['line'] - 1]
            member = fn.split('-')[0]
            NESTED_ENUMS.clear()
            for c in index.chain(cls):
                for mm in re.finditer(r'\benum\s+(?:class\s+)?(\w+)\b', index.cls[c][2]): NESTED_ENUMS.setdefault(mm.group(1), '%s::%s' % (c, mm.group(1)))
            def qual(t):
                for k, v in NESTED_ENUMS.items(): t = re.sub(r'(?<![\w:])' + k + r'\b', v, t)
                return t
            m = re.search(r'0x[0-9A-Fa-f]+\s*,\s*(.*?)\s*\(\s*\w+::\*\s*\)\s*\((.*)\)\s*\)\s*;?\s*$', line)
            if m: ret = m.group(1); params = split_params(m.group(2))
            else:
                wrap = None; ret = None
                for acc, st in flatten(body, 'private' if kind == 'class' else 'public'):
                    mm = re.match(r'^([\w:<>\s*&]*?)\b' + re.escape(member) + r'\s*\((.*)\)\s*(\{\})?$', st, re.S)
                    if mm and ('*' in mm.group(1) or 'auto' in mm.group(1)): wrap = mm.group(2); ret = mm.group(1).strip(); break
                if wrap is None: report.append('NOWRAPPER %s 0x%X' % (key, s['addr'])); continue
                params = split_params(wrap)
            if ret in (None, '', 'auto') or 'auto' in ret.split(): ret = cls + '*'
            specs = []; bad = None; ptypes = []
            for i, p in enumerate(params):
                nt = norm_param(p)
                sp = argspec(nt, i)
                if sp is None: bad = p; break
                specs.append(sp); ptypes.append(qual(nt))
            if bad is not None: report.append('NOARG %s 0x%X param "%s"' % (key, s['addr'], bad)); continue
            decls = ' '.join(sp[0] for sp in specs)
            pargs = ', '.join(sp[1] for sp in specs)
            etypes = ', '.join(['void*', 'int'] + [sp[2] for sp in specs])
            eargs = ', '.join(['p', '0'] + [sp[3] for sp in specs])
            ctor_rows.append((cls, fn, s['addr'], decls, pargs, etypes, eargs, hdr_rel, member, qual(ret), ', '.join(ptypes)))
        else:
            ents = []
            for owner, acc, k, mname, ln, ttxt, ext in index.members(cls):
                rob = 'W2Off_%s_%s' % (owner, mname)
                ne = {}
                for c in index.chain(owner):
                    for mm in re.finditer(r'\benum\s+(?:class\s+)?(\w+)\b', index.cls[c][2]): ne.setdefault(mm.group(1), '%s::%s' % (c, mm.group(1)))
                for k2, v2 in ne.items(): ttxt = re.sub(r'(?<![\w:])' + k2 + r'\b', v2, ttxt)
                robs[rob] = (owner, mname, ttxt, ext)
                for e in range(ln if ln else 0): ents.append((rob, e * (1 if k == 'b' else 4), k))
            clone_rows.append((cls, fn, s['addr'], ents, hdr_rel))
        for c in index.chain(cls):
            tu = tu_for(c) if c != cls else s['file'][len('source/'):]
            if tu: srcs.add(tu)
        for c in index.chain(cls):
            if c in base_tus: srcs.add(base_tus[c])
    def w(path, lines): open(os.path.join(out_dir, path), 'w', encoding='utf-8', newline=NL).write(NL.join(lines) + NL)
    # ---- ctor rows
    L = ['// GENERATED by tests/standalone/tools/gen_w2_rows.py (%d rows): task / event constructors, port `new (buf) T(args)` vs the exe constructor on identical garbage; do not edit by hand' % len(ctor_rows)]
    for h in sorted(set(r[7] for r in ctor_rows)): L.append('#include "%s"' % h)
    L.append('')
    for n, (cls, fn, addr, decls, pargs, etypes, eargs, h, member, ret, ptypes) in enumerate(ctor_rows):
        L.append('struct W2C%d_T { using type = %s (%s::*)(%s); friend type W2Get(W2C%d_T); }; template struct W2Rob<W2C%d_T, &%s::%s>;' % (n, ret, cls, ptypes, n, n, cls, member))
    L.append('')
    L.append('static void TestW2Ctors() {')
    L.append('    using namespace w2;')
    for n, (cls, fn, addr, decls, pargs, etypes, eargs, h, member, ret, ptypes) in enumerate(ctor_rows):
        L.append('    Run("%s::%s 0x%X", [&](Rng& r, std::string& d) {' % (cls, fn, addr))
        L.append('        %s' % decls)
        L.append('        return CtorCase(r, d, sizeof(%s), [&](void* p) { (static_cast<%s*>(p)->*W2Get(W2C%d_T{}))(%s); }, [&](void* p) { oracle::Fn<void* __fastcall(%s)>(0x%X)(%s); }%s);' % (cls, cls, n, pargs, etypes, addr, eargs, ', false' if '%s::%s' % (cls, fn) in refdiff else ''))
        L.append('    }, 150);')
    L.append('}')
    w('wave2_oracle_gen_ctor.inc', L)
    # ---- clone rows
    L = ['// GENERATED by tests/standalone/tools/gen_w2_rows.py (%d rows): task / event Clone / CloneEditable, port definition (non-virtual call) vs exe on a garbage source object; do not edit by hand' % len(clone_rows)]
    for h in sorted(set(r[4] for r in clone_rows)): L.append('#include "%s"' % h)
    L.append('')
    L.append('// offsets of the (possibly private / protected) pointer members, via the explicit-instantiation access trick (no layout assumptions, no #define private public)')
    for rob, (owner, mname, ttxt, ext) in sorted(robs.items()):
        mp = '%s(%s::*)[%d]' % (ttxt, owner, ext) if ext else '%s %s::*' % (ttxt, owner)
        L.append('struct %s_T { using type = %s; friend type W2Get(%s_T); }; template struct W2Rob<%s_T, &%s::%s>;' % (rob, mp, rob, rob, owner, mname))
        L.append('static size_t %s() { auto pm = W2Get(%s_T{}); uint32_t o = 0; std::memcpy(&o, &pm, 4); return o; }' % (rob, rob))
        L.append('static size_t %s_sz() { return sizeof(W2MemberOf<%s_T::type>::type); }' % (rob, rob))
    L.append('')
    L.append('static void TestW2Clones() {')
    L.append('    using namespace w2;')
    for cls, fn, addr, ents, h in clone_rows:
        el = ', '.join(('{ %s() + %d, "%s"[0], %s_sz() }' % (mn, e, k, mn)) if k == 'u' else ('{ %s() + %d, "%s"[0] }' % (mn, e, k)) for mn, e, k in ents)
        L.append('    Run("%s::%s 0x%X", [&](Rng& r, std::string& d) {' % (cls, fn, addr))
        L.append('        return CloneCase<%s>(r, d, 0x%X, { %s }, [](%s* t) { return t->%s::%s(); }%s);' % (cls, addr, el, cls, cls, fn, ', false' if '%s::%s' % (cls, fn) in refdiff else ''))
        L.append('    }, 120);')
    L.append('}')
    w('wave2_oracle_gen_clone.inc', L)
    w('wave2_oracle_gen_sources.cmake', ['# GENERATED by tests/standalone/tools/gen_w2_rows.py: translation units of the generated wave-2 rows', 'set(W2_GEN_SOURCES'] + ['    %s' % x for x in sorted(srcs)] + [')'])
    w('wave2_oracle_gen_report.txt', ['%d ctor rows, %d clone rows, %d TUs' % (len(ctor_rows), len(clone_rows), len(srcs))] + report)
    print('%d ctor rows, %d clone rows, %d TUs, %d notes' % (len(ctor_rows), len(clone_rows), len(srcs), len(report)))
    cnt = collections.Counter(r.split()[0] for r in report)
    print(dict(cnt))

if __name__ == '__main__':
    main()
