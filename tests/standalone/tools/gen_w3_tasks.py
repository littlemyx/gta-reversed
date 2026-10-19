#!/usr/bin/env python3
"""ORACLE-W2: generates exe-oracle rows for ordinary task / event member and static functions (MakeAbortable, ProcessPed, CreateSubTask, FinishAnimCB, AffectsPed, ...).

Companion of gen_w2_rows.py (constructors / Clone). Every row runs the port's definition (non-virtual call) and the exe's function on the SAME garbage `this` (entity pointer members point at the
reference-world entities, other pointer members are null, plain bools are 0/1, enums are small), the same arguments, the same fake task / event pools and the same deterministic arena.
Compared after the call: return value, the `this` object, the pools, the arena, the entity reference world and the WHOLE exe data image 0x8A4000..0xCB0000 (every global the function
touched - the port's globals sit at the exe addresses in this test).

usage: gen_w3_tasks.py <repo> <sites.json> <out_dir> [--skip tools/w2_skip.tsv] [--classes A,B]   (writes wave3_oracle_gen_tasks.inc, wave3_oracle_gen_tasks_sources.cmake, wave3_oracle_gen_tasks_report.txt)
A site is taken when: fn is not a constructor / Clone, the exe closure class is A or B (no indirect call, no import), no other oracle file mentions the address and the signature is parsable.
"""
import json, re, sys, os, glob, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_w2_rows as G

NL = chr(10)
QUALS = ('static', 'virtual', 'inline', 'constexpr', 'explicit', 'friend', 'NOTSA_FORCE_INLINE', '[[nodiscard]]', '__stdcall', '__cdecl', '__fastcall', '__thiscall')

def find_decl(index, cls, member):
    """-> (owner class, access, statement) of the unique declaration of `member` in cls or its bases, or None"""
    hits = []
    for c in index.chain(cls):
        kind, bases, body = index.cls[c][:3]
        for acc, st in G.flatten(body, 'private' if kind == 'class' else 'public'):
            m = re.search(r'(?<![\w:~])' + re.escape(member) + r'\s*\(', st)
            if not m: continue
            pre = st[:m.start()]
            if '=' in pre or 'return' in pre or st.startswith(('using', 'typedef', 'friend', 'template')): continue
            hits.append((c, acc, st))
        if hits: break      # most derived declaration wins; several statements of that class = overloads
    return hits

def parse_decl(st, member):
    m = re.match(r'^(?P<pre>.*?)(?<![\w:~])' + re.escape(member) + r'\s*\((?P<params>.*)\)\s*(?P<post>[^()]*?)(?:\{\})?$', st, re.S)
    if not m: return None
    pre = m.group('pre'); post = m.group('post')
    toks = pre.replace('[[nodiscard]]', ' ').split()
    flags = {t for t in toks if t in QUALS}
    ret = ' '.join(t for t in toks if t not in QUALS)
    mt = re.search(r'->\s*([\w:]+(?:\s*[*&])?)', post)
    if ret == 'auto' and mt: ret = mt.group(1).strip()   # trailing return type
    return dict(ret=ret, params=G.split_params(m.group('params')), static='static' in flags, virtual='virtual' in flags or 'override' in post.split(),
                const='const' in post.split(), stdcall='__stdcall' in flags, pure='= 0' in post.replace('=0', '= 0'))

RET_KIND = {'void': 'v', 'bool': 'b', 'float': 'f'}

def ret_kind(ret):
    r = ret.replace('const ', '').replace(' ', '')
    if r in ('void',): return 'v'
    if r == 'bool': return 'b'
    if r == 'float': return 'f'
    if r.endswith('*'): return 'p'
    if r in ('CVector', 'CColSphere', 'CVector2D', 'RwV3d'): return 'S'
    if r in ('int16', 'uint16', 'short', 'int16_t', 'uint16_t'): return 'h'   # the exe returns AX only: compare the low 16 bits
    if r in ('int8', 'uint8', 'char', 'int8_t', 'uint8_t', 'eWeaponSkill'): return 'c'       # AL only (eWeaponSkill: 0x5E3B60 / 0x5E6580 return `al`, the upper bytes of EAX are garbage)
    if r in G.INT_T or r in ('int32', 'uint32', 'int', 'unsigned', 'uint8', 'uint16', 'int8', 'int16', 'int32_t', 'uint32_t'): return 'i'
    if re.match(r'^(e[A-Z]\w*|AnimationId|AssocGroupId|PedGroupId)$', r): return 'i'
    return None

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    repo, sites_json, out_dir = args[:3]
    skip = {}
    if '--skip' in sys.argv:
        for ln in open(sys.argv[sys.argv.index('--skip') + 1], encoding='utf-8'):
            ln = ln.strip()
            if ln and not ln.startswith('#'):
                k, _, why = ln.partition('	'); skip[k.strip()] = why.strip()
    classes = ('A', 'B', 'C')
    if '--classes' in sys.argv: classes = tuple(sys.argv[sys.argv.index('--classes') + 1].split(','))
    sites = json.load(open(sites_json))
    covered = set()
    for f in glob.glob(os.path.join(repo, 'tests/standalone/*')):
        b = os.path.basename(f)
        if b.startswith('wave2_oracle_gen') or b.startswith('wave3_oracle_gen'): continue
        if f.endswith(('.cpp', '.inc', '.h')):
            for m in G.RE_ADDR.finditer(open(f, encoding='utf-8', errors='replace').read()): covered.add(int(m.group(1), 16))
    index = G.Index(repo)
    rows = []; report = []; srcs = set(); robs = {}; nrob = 0
    base_tus = {'CTask': 'game_sa/Tasks/Task.cpp', 'CTaskSimple': 'game_sa/Tasks/TaskSimple.cpp', 'CTaskComplex': 'game_sa/Tasks/TaskComplex.cpp', 'CEvent': 'game_sa/Events/Event.cpp', 'CEventEditableResponse': 'game_sa/Events/EventEditableResponse.cpp'}
    seen = set()
    for s in sorted(sites, key=lambda s: (s['file'], s['addr'])):
        fn = s['fn']
        if not s['cls'] or s['sub'] != 'game_sa/Tasks/TaskTypes': continue
        if fn in ('CloneEditable',): continue
        if s['addr'] in covered or s['covered'] or s['oc'] not in classes: continue
        cls = s['cls']; key = '%s::%s' % (cls, fn)
        if key in skip: report.append('SKIP %s 0x%X: %s' % (key, s['addr'], skip[key])); continue
        if cls not in index.cls: report.append('NOHEADER %s 0x%X' % (key, s['addr'])); continue
        if any(index.cls[c][4] for c in index.chain(cls)): report.append('TEMPLATE %s 0x%X' % (key, s['addr'])); continue
        if (cls, s['addr']) in seen: continue
        seen.add((cls, s['addr']))
        member = fn.split('-')[0]
        line = open(os.path.join(repo, s['file']), encoding='utf-8', errors='replace').read().split(NL)[s['line'] - 1]
        # explicit signature (overloaded install): `ret(Class::*)(params)` or `ret(*)(params)`
        mo = re.search(r'0x[0-9A-Fa-f]+\s*,\s*(.*?)\s*\(\s*(\w+::)?\*\s*\)\s*\((.*)\)\s*(const)?\s*\)\s*;?\s*$', line)
        if mo:
            d = dict(ret=mo.group(1).strip(), params=G.split_params(mo.group(3)), static=not mo.group(2), virtual=False, const=bool(mo.group(4)), stdcall=False)
            owner = cls
        else:
            hits = find_decl(index, cls, member)
            if len(hits) != 1: report.append('DECL %s 0x%X (%d declarations)' % (key, s['addr'], len(hits))); continue
            owner, acc, st = hits[0]
            d = parse_decl(st, member)
            if d is None: report.append('PARSE %s 0x%X' % (key, s['addr'])); continue
            if acc != 'public' and (d['virtual'] or True) and acc != 'public': pass
            d['access'] = acc
        if member == 'Destructor' and 'auto' in d['ret'].split(): d['ret'] = owner + '*'   # `auto Destructor() { this->T::~T(); return this; }`
        rk = ret_kind(d['ret'])
        is_dtor = (member == 'Destructor')
        if fn.startswith('Constructor') and d['ret'].replace(' ','') in (cls + '*',): rk = 'p'
        if is_dtor: rk = 'v'   # the wrapper returns `this`, the exe's destructor returns whatever EAX held: only the effects are compared
        if rk is None: report.append('RET %s 0x%X "%s"' % (key, s['addr'], d['ret'])); continue
        G.NESTED_ENUMS.clear()
        for c in index.chain(cls):
            for mm in re.finditer(r'enum' + chr(92) + 's+(?:class' + chr(92) + 's+)?(' + chr(92) + 'w+)', index.cls[c][2]): G.NESTED_ENUMS.setdefault(mm.group(1), '%s::%s' % (c, mm.group(1)))
        def qual(t):
            for k2, v2 in G.NESTED_ENUMS.items(): t = re.sub(r'(?<![\w:])' + k2 + r'(?![\w])', v2, t)
            return t
        specs = []; bad = None; ptypes = []
        for i, p in enumerate(d['params']):
            nt = G.norm_param(p)
            if nt in ('void', ''): continue
            sp = argspec_ext(nt, i)
            if sp is None: bad = p; break
            specs.append(sp); ptypes.append(qual(nt))
        if bad is not None: report.append('NOARG %s 0x%X param "%s"' % (key, s['addr'], bad)); continue
        if d['static'] and any(sp[2] == 'void*' for sp in specs): d['static_obj'] = True   # callback-data parameter: the task object is the `this` buffer
        decls = ' '.join(sp[0] for sp in specs).replace('GPedN(r)', 'w3t::TPedN(r)')
        pargs = ', '.join(sp[1] for sp in specs)
        etypes = [sp[2] for sp in specs]; eargs = [sp[3] for sp in specs]
        rtype = {'v': 'void', 'b': 'int', 'f': 'float', 'p': 'void*', 'i': 'int', 'h': 'int', 'c': 'int', 'S': 'void*'}[rk]
        if rk == 'S':   # struct return: hidden pointer as the first stack argument (thiscall: after this), both sides write g_tr
            etypes = ['void*'] + etypes; eargs = ['w3t::g_tr'] + eargs
        if d['static']:
            exe = 'oracle::Fn<%s %s(%s)>(0x%X)(%s)' % (rtype, '__stdcall' if d.get('stdcall') else '__cdecl', ', '.join(etypes), s['addr'], ', '.join(eargs))
            call = '%s::%s(%s)' % (cls, member, pargs)
        else:
            exe = 'oracle::Fn<%s __fastcall(%s)>(0x%X)(%s)' % (rtype, ', '.join(['void*', 'int'] + etypes), s['addr'], ', '.join(['p', '0'] + eargs))
            call = 'static_cast<%s*>(p)->%s::%s(%s)' % (cls, cls, member, pargs)
        # accessibility: private / protected non-virtual members go through the access trick
        use_rob = (not mo) and d.get('access', 'public') != 'public' and not d['virtual']
        if (not mo) and d.get('access', 'public') != 'public' and d['virtual']:
            report.append('PRIVVIRT %s 0x%X' % (key, s['addr'])); continue
        rob = None
        if use_rob:
            nrob += 1; rob = 'W2F%d' % nrob
            if d['static']: mp = '%s (*)(%s)' % (d['ret'], ', '.join(ptypes))
            else: mp = '%s (%s::*)(%s)%s' % (d['ret'], owner, ', '.join(ptypes), ' const' if d['const'] else '')
            robs[rob] = (mp, owner, member)
            if d['static']: call = '(W2Get(%s_T{}))(%s)' % (rob, pargs)
            else: call = '(static_cast<%s*>(p)->*W2Get(%s_T{}))(%s)' % (cls, rob, pargs)
        # entity-pointer members of the object (same rules as the Clone rows)
        ents = []
        if (not d['static']) or d.get('static_obj'):
            for o2, acc2, k, mname, ln, ttxt, ext in index.members(cls):
                r2 = 'W2Off_%s_%s' % (o2, mname)
                ne = {}
                for c in index.chain(o2):
                    for mm in re.finditer(r'enum' + chr(92) + 's+(?:class' + chr(92) + 's+)?(' + chr(92) + 'w+)', index.cls[c][2]): ne.setdefault(mm.group(1), '%s::%s' % (c, mm.group(1)))
                for k2, v2 in ne.items(): ttxt = re.sub(r'(?<![\w:])' + k2 + r'(?![\w])', v2, ttxt)
                G_robs_offsets[r2] = (o2, mname, ttxt, ext)
                if s['oc'] == 'C' and o2 == 'CTaskComplex' and mname == 'm_pSubTask': k = 'T'   # indirect calls go to a real sub task
                for e in range(ln if ln else 0): ents.append((r2, e * (1 if k == 'b' else 4), k))
        if rk == 'S': call = 'w3t::StoreRet(%s)' % call; exe = '(%s, 0)' % exe
        build = None
        if (not d['static']) and not fn.startswith('Constructor') and len(d['params']) <= 3 and not is_dtor:
            cr = find_ctor_args(index, cls)
            if cr is not None: build = cr
        hdr_rel = os.path.relpath(index.header_of(cls), os.path.join(repo, 'source')).replace(chr(92), '/')
        pre = PRE.get(key)
        rows.append(dict(pre=pre, build=build, cls=cls, fn=fn, addr=s['addr'], decls=decls, ctor=fn.startswith('Constructor'), call=call, exe=exe, rk=rk, static=d['static'] and not d.get('static_obj'), static_call=d['static'], ents=ents, hdr=hdr_rel, boolret=(rk == 'b'), sub=(s['oc'] == 'C' and 'CTaskComplex' in index.chain(cls)), dtor=is_dtor or fn.startswith('Constructor')))
        for c in index.chain(cls):
            tu = G.tu_for_cls(index, repo, c) if hasattr(G, 'tu_for_cls') else None
        for c in index.chain(cls):
            h = index.header_of(c)
            cpp = os.path.splitext(h)[0] + '.cpp' if h else None
            if c == cls: srcs.add(s['file'][len('source/'):])
            elif cpp and os.path.exists(cpp): srcs.add(os.path.relpath(cpp, os.path.join(repo, 'source')).replace(chr(92), '/'))
            if c in base_tus: srcs.add(base_tus[c])
    srcs.update(TASK_CALLEE_TUS)
    def w(path, lines): open(os.path.join(out_dir, path), 'w', encoding='utf-8', newline=NL).write(NL.join(lines) + NL)
    L = ['// GENERATED by tests/standalone/tools/gen_w3_tasks.py (%d rows): ped member and static functions, port definition vs exe on identical wired-ped state; do not edit by hand' % len(rows)]
    for h in sorted(set(r['hdr'] for r in rows)): L.append('#include "%s"' % h)
    L.append('')
    for rob, (mp, owner, member) in sorted(robs.items()):
        L.append('struct %s_T { using type = %s; friend type W2Get(%s_T); }; template struct W2Rob<%s_T, &%s::%s>;' % (rob, mp, rob, rob, owner, member))
    have = set()
    cl_inc = os.path.join(out_dir, 'wave3_none.inc')   # the Clone rows (included before this file) already define the member-offset helpers they use
    if os.path.exists(cl_inc):
        have = set(re.findall(r'static size_t (W2Off_\w+)\(', open(cl_inc, encoding='utf-8').read()))
    for rob, (owner, mname, ttxt, ext) in sorted(G_robs_offsets.items()):
        if rob in have: continue
        mp = '%s(%s::*)[%d]' % (ttxt, owner, ext) if ext else '%s %s::*' % (ttxt, owner)
        L.append('struct %s_T { using type = %s; friend type W2Get(%s_T); }; template struct W2Rob<%s_T, &%s::%s>;' % (rob, mp, rob, rob, owner, mname))
        L.append('static size_t %s() { auto pm = W2Get(%s_T{}); uint32_t o = 0; std::memcpy(&o, &pm, 4); return o; }' % (rob, rob))
        L.append('static size_t %s_sz() { return sizeof(W2MemberOf<%s_T::type>::type); }' % (rob, rob))
    L.append('')
    L.append('static void TestW3TaskFuncs() {')
    L.append('    using namespace w2;')
    for r in rows:
        el = ', '.join(('{ %s() + %d, "%s"[0], %s_sz() }' % (mn, e, k, mn)) if k == 'u' else ('{ %s() + %d, "%s"[0] }' % (mn, e, k)) for mn, e, k in r['ents'])
        sz = '0' if r['static'] else 'sizeof(%s)' % r['cls']
        conv = {'v': ('(%s, uint64_t(0))', '(%s, uint64_t(0))'), 'b': ('(uint64_t)(uint8_t)(%s ? 1 : 0)', '(uint64_t)((uint8_t)(%s) ? 1 : 0)'), 'f': ('(uint64_t)FBits(%s)', '(uint64_t)FBits(%s)'),
                'p': ('(uint64_t)(uintptr_t)(%s)', '(uint64_t)(uintptr_t)(%s)'), 'i': ('(uint64_t)(uint32_t)(%s)', '(uint64_t)(uint32_t)(%s)'), 'h': ('(uint64_t)(uint16_t)(%s)', '(uint64_t)(uint16_t)(%s)'), 'c': ('(uint64_t)(uint8_t)(%s)', '(uint64_t)(uint8_t)(%s)'), 'S': ('(uint64_t)(%s)', '(uint64_t)(%s)')}[r['rk']]
        L.append('    Run("%s::%s 0x%X", [&](Rng& r, std::string& d) {' % (r['cls'], r['fn'], r['addr']))
        L.append('        %s' % (r['decls'] + (' ' + r['build'][0] if r.get('build') else '')))
        if r.get('pre'): L.append('        w3t::g_tpost = [](Rng&) { %s };' % r['pre'])
        L.append('        return W3TaskCase(r, d, %s, { %s }, [&](void* p) -> uint64_t { (void)p; return %s; }, [&](void* p) -> uint64_t { (void)p; return %s; }%s);' % (sz, el, conv[0] % r['call'], conv[1] % r['exe'], (', true' if r.get('dtor') else ', false') + ((', [&]() -> bool { return w3t::Build<%s>(%s); }' % (r['cls'], r['build'][1])) if r.get('build') else '')))
        L.append('    }, 60);')
    L.append('}')
    w('wave3_oracle_gen_tasks.inc', L)
    w('wave3_oracle_gen_tasks_sources.cmake', ['# GENERATED by tests/standalone/tools/gen_w3_tasks.py: translation units of the generated function rows', 'set(W3_TASKS_SOURCES'] + ['    %s' % x for x in sorted(srcs)] + [')'])
    w('wave3_oracle_gen_tasks_report.txt', ['%d function rows, %d TUs' % (len(rows), len(srcs))] + report)
    print('%d function rows, %d TUs, %d notes' % (len(rows), len(srcs), len(report)))
    print(dict(collections.Counter(x.split()[0] for x in report)))

G_robs_offsets = {}
# per-row fixture: exe statics the CRT start-up (not run by the harness) or the function's lazy first-call code would initialise
PRE = {
    'CTaskComplexTrackEntity::CalcMoveRatio': 'volatile uint32_t* s_ = reinterpret_cast<volatile uint32_t*>(0xC18CF4); s_[0] = 0x3FA00000; s_[1] = 0x3E800000; s_[2] = 0x41C80000; s_[3] = 0x3F800000; s_[4] = 0x3D23D70B; s_[5] = 0x1F;',   # lazily initialised function-local statics of the exe (1.25, 0.25, 25.0, 1.0, 0.04 + guard bits)
}
TASK_CALLEE_TUS = ['game_sa/Clock.cpp', 'game_sa/Entity/Ped/PlayerPed.cpp', 'game_sa/Entity/Ped/CopPed.cpp', 'game_sa/Stats.cpp', 'game_sa/WeaponInfo.cpp', 'game_sa/Weapon.cpp', 'game_sa/Wanted.cpp', 'game_sa/Pad.cpp', 'game_sa/PedIntelligence.cpp', 'game_sa/Audio/Entities/AEPedSpeechAudioEntity.cpp']

def find_ctor_args(index, cls):
    """-> (decl text, arg list text) of the first public non-copy constructor of cls whose parameters the generator can produce, or None"""
    kind, bases, body = index.cls[cls][:3]
    for acc, st in G.flatten(body, 'private' if kind == 'class' else 'public'):
        if acc != 'public': continue
        m = re.match(r'^(?:explicit\s+|NOTSA_FORCE_INLINE\s+)?' + re.escape(cls) + r'\s*\((.*)\)\s*(?:noexcept)?\s*(?:=\s*default)?\s*(?:\{\})?$', st, re.S)
        if not m: continue
        params = G.split_params(m.group(1))
        if len(params) == 1 and re.search(r'' + cls + r'\s*(const)?\s*&', params[0]): continue   # copy constructor
        decls = []; names = []; ok = True
        for i, p in enumerate(params):
            nt = G.norm_param(p)
            if nt in ('void', ''): continue
            sp = argspec_ext(nt, 4 + i)
            if sp is None or 'CTask' in nt and not nt.endswith('*'): ok = False; break
            decls.append(sp[0].replace('GPedN(r)', 'w3t::TPedN(r)')); names.append(sp[1])
        if ok and 'Const' not in ' '.join(decls) and '_IGNORED_' not in ' '.join(decls): return ' '.join(decls), ', '.join(names)
    return None

def argspec_ext(ptype, idx):
    """G.argspec plus the pointer types the function rows need"""
    if re.sub(r'const', '', ptype).replace(' ', '') == 'eDoors': return ('eDoors %s = (eDoors)r.below(6);' % ('a%d' % idx), 'a%d' % idx, 'int', '(int)a%d' % idx)   # the doors enum has 6 values; the game never passes more
    sp = None
    base = re.sub(r'' + chr(92) + 'bconst' + chr(92) + 'b', '', ptype).replace(' ', '')
    name = 'a%d' % idx
    core = base.rstrip('&*')
    if base.endswith('*'):
        if core == 'void': return ('void* %s = static_cast<void*>(w3t::g_tk);' % name, name, 'void*', name)           # callback data pointer: the task itself (the `this` buffer)
        if core == 'CAnimBlendAssociation': return ('CAnimBlendAssociation* %s = reinterpret_cast<CAnimBlendAssociation*>(g_anim);' % name, name, 'CAnimBlendAssociation*', name)
        if core.startswith('CEvent') or core.startswith('CTask') or core in ('CPedGroup', 'CPedGroupMembership'):
            return ('%s* %s = nullptr;' % (core, name), name, 'void*', '(void*)%s' % name)
    if base.endswith('&') and idx < 8:
        if core in ('CVector', 'CVector2D'):   # in/out vector: a fixture buffer compared after the call
            gen = 'GV' if core == 'CVector' else 'GV2'
            return ('%s& %s = *reinterpret_cast<%s*>(w3t::g_ta[%d]); %s = %s(r);' % (core, name, core, idx, name, gen), name, 'void*', '&' + name)
        if core in G.ENT_KIND:
            fn = {'p': 'w3t::TPedR', 'v': 'GVehE', 'o': 'GObjE', 'e': 'GEnt'}[G.ENT_KIND[core]]
            return ('%s& %s = *static_cast<%s*>(%s(r));' % (core, name, core, fn), name, 'void*', '&' + name)
        if core == 'CPointRoute':   # up to 8 entries
            return ('CPointRoute& %s = *reinterpret_cast<CPointRoute*>(w3t::g_ta[%d]); %s.m_NumEntries = r.below(9); for (auto& e_ : %s.m_Entries) e_ = GV(r);' % (name, idx, name, name), name, 'void*', '&' + name)
        if core == 'PedsToAvoidArray':
            return ('std::array<CPed*, 16>& %s = *reinterpret_cast<std::array<CPed*, 16>*>(w3t::g_ta[%d]); for (auto& e_ : %s) e_ = w3t::TPedN(r);' % (name, idx, name), name, 'void*', '&' + name)
        if core in ('uint8', 'uint8_t'):
            return ('uint8& %s = w3t::g_ta[%d][0]; %s = (uint8)r.below(8);' % (name, idx, name), name, 'void*', '&' + name)
        if core in ('CColSphere', 'CNodeAddress', 'CSphere', 'CBoundingBox'):
            return ('%s& %s = *reinterpret_cast<%s*>(w3t::g_ta[%d]);' % (core, name, core, idx), name, 'void*', '&' + name)
    if core == 'RwV3d': return ('RwV3d %s = { GFl(r), GFl(r), GFl(r) };' % name, name, 'RwV3d', name)
    return G.argspec(ptype, idx)

if __name__ == '__main__':
    main()
