#!/usr/bin/env python3
"""ORACLE-W1: exe-oracle coverage inventory.

Lists every ported function (RH_Scoped*Install site with its exe address), marks which ones an exe-oracle row already covers (the address appears in tests/standalone/*), classifies the
uncovered ones by how hard they are to run through the oracle (static analysis of the exe function's call closure) and writes the report .notes/ORACLE_COVERAGE.md.

usage: python -I tools/standalone/oracle_coverage.py [--repo .] [--exe gta_sa_compact.exe] [--baseline <commit>] [--out .notes/ORACLE_COVERAGE.md] [--json sites.json]
  needs python with `capstone` (the same one tools/standalone/ uses for the detached build)
  --baseline  commit whose tests/standalone defines the "before" coverage (default: none - only the current tree is reported)

Classification of the exe closure (calls followed up to depth 6 / 0x3000 bytes per function, tests/standalone/tools/oracle_closure.py does the same per root):
  A  pure         no data/bss global, no import, no indirect call: arguments and `this` memory only          -> easy (random objects, compare returns + memory)
  B  state        touches exe globals (pools, timers, camera, world arrays ...) but nothing else               -> medium (both sides share the statics at their exe addresses; snapshot / restore)
  C  virtual      indirect calls through vtables / function pointers, no imports                                -> medium-hard (fake vtables with recording stubs, as script_oracle / gameplay_oracle do)
  S  allocator    ORACLE-W2: the closure reaches only allocator / CRT-lock / TLS imports (HeapAlloc, critical sections ...): tests/standalone/oracle_alloc.h redirects them (exe CRT entry points + IAT stubs) to a deterministic arena shared with the port -> oracle-able like A/B/C
  D  imports      the closure calls another Win32 / D3D / CRT import (file IO, timers, D3D, audio)                -> not oracle-able as is (the IAT is not populated); individual callees can be Patch()ed (rand, malloc ...)
"Covered" is a text search: a 0xAAAAAA literal of the site's address in any tests/standalone file (rows, tables, Patch lines). It over-approximates a little (an address that is only
Patch()ed or only read as data counts) and under-approximates the script-command handlers (covered by opcode through the script oracle tests, not by address).
"""
import argparse, collections, glob, json, os, re, struct, subprocess, sys

BS = chr(92)
SITE = re.compile(r'^\s*(RH_Scoped\w*Install\w*)\s*\(\s*([^,)]+)\s*(?:,\s*("[^"]*"|[^,]*?)\s*)?,\s*(0x[0-9A-Fa-f]+)')
CLS = re.compile(r'^\s*RH_Scoped(?:Virtual)?Class\s*\(\s*([' + 'A-Za-z0-9_:' + r']+)')
NS = re.compile(r'^\s*RH_ScopedNamespace(?:Name)?\s*\(\s*"?([' + 'A-Za-z0-9_:' + r']+)')

def inventory(repo):
    sites = []
    for path in sorted(glob.glob(repo + '/source/**/*.cpp', recursive=True)):
        rel = os.path.relpath(path, repo).replace(BS, '/')
        cur = None; incomment = False
        try: lines = open(path, encoding='utf-8', errors='replace').read().split('\n')
        except OSError: continue
        for i, ln in enumerate(lines):
            s = ln.strip()
            if s.startswith('/*'): incomment = '*/' not in s
            elif incomment:
                if '*/' in s: incomment = False
                continue
            if s.startswith('//') or 'define' in ln: continue
            m = CLS.match(ln) or NS.match(ln)
            if m: cur = m.group(1); continue
            if ln.lstrip().startswith('RH_ScopedCategoryGlobal'): cur = None; continue
            m = SITE.match(ln)
            if m:
                kind = m.group(1)
                if kind == 'RH_ScopedInstallScriptCommand': continue
                fn = m.group(2).strip()
                if kind.startswith('RH_ScopedNamed'): fn = m.group(3).strip('"')
                elif 'Overloaded' in kind and m.group(3): fn = fn + '-' + m.group(3).strip('"')
                sites.append(dict(file=rel, line=i + 1, cls=cur, fn=fn, addr=int(m.group(4), 16), kind=kind, virt='VMT' in kind))
    return sites

ADDR = re.compile(r'0[xX]([0-9A-Fa-f]{6})' + BS + 'b')

def covered_addrs(repo, baseline=None):
    cov = collections.defaultdict(set)
    if baseline:
        names = subprocess.run(['git', '-C', repo, 'ls-tree', '-r', '--name-only', baseline, 'tests/standalone/'], capture_output=True, text=True).stdout.split()
        for n in names:
            if not n.endswith(('.cpp', '.inc', '.h')): continue
            txt = subprocess.run(['git', '-C', repo, 'show', baseline + ':' + n], capture_output=True, text=True, errors='replace').stdout
            for m in ADDR.finditer(txt): cov[int(m.group(1), 16)].add(os.path.basename(n))
    else:
        for f in glob.glob(repo + '/tests/standalone/*'):
            if f.endswith(('.cpp', '.inc', '.h')):
                for m in ADDR.finditer(open(f, encoding='utf-8', errors='replace').read()): cov[int(m.group(1), 16)].add(os.path.basename(f))
    return cov

TEXT = (0x401000, 0x858000); RDATA = (0x858000, 0x8A4000); DATA = (0x8A4000, 0xCB0000); IAT = (0x858000, 0x858378)

def classify(exe_path, addrs):
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    from capstone.x86 import X86_OP_IMM, X86_OP_MEM
    exe = open(exe_path, 'rb').read()
    md = Cs(CS_ARCH_X86, CS_MODE_32); md.detail = True
    raw = lambda va: va - 0x401000 + 0x400
    depth_cap, size_cap = 6, 0x3000
    def scan(entry):
        seen = {}; work = [entry]; res = dict(calls=set(), imports=set(), indirect=0, globals=set(), hi=entry)
        while work:
            a = work.pop()
            while a not in seen and entry <= a < entry + size_cap and TEXT[0] <= a < TEXT[1]:
                ins = next(md.disasm(exe[raw(a):raw(a) + 16], a), None)
                if ins is None: break
                seen[a] = ins; res['hi'] = max(res['hi'], a + ins.size); m = ins.mnemonic
                for op in ins.operands:
                    if op.type == X86_OP_MEM:
                        d = op.mem.disp & 0xFFFFFFFF
                        if op.mem.base == 0 and op.mem.index == 0 or (d >= RDATA[0] and op.mem.base == 0):
                            if m in ('call', 'jmp') and IAT[0] <= d < IAT[1]: res['imports'].add(d)
                            elif DATA[0] <= d < DATA[1]: res['globals'].add(d)
                        elif op.mem.index != 0 and DATA[0] <= d < DATA[1]: res['globals'].add(d)
                if m == 'call':
                    op = ins.operands[0]
                    if op.type == X86_OP_IMM: res['calls'].add(op.imm & 0xFFFFFFFF)
                    elif op.type == X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0 and IAT[0] <= (op.mem.disp & 0xFFFFFFFF) < IAT[1]: pass
                    else: res['indirect'] += 1
                if m == 'jmp':
                    op = ins.operands[0]
                    if op.type == X86_OP_IMM:
                        t = op.imm & 0xFFFFFFFF
                        if entry <= t < entry + size_cap: work.append(t)
                        else: res['calls'].add(t)
                    elif op.type == X86_OP_MEM and op.mem.index != 0 and op.mem.base == 0:
                        t0 = op.mem.disp & 0xFFFFFFFF; k = 0
                        while k <= 256:
                            try: v = struct.unpack_from('<I', exe, raw(t0) + 4 * k)[0] if TEXT[0] <= t0 < TEXT[1] else struct.unpack_from('<I', exe, 0x456800 + t0 - 0x858000 + 4 * k)[0]
                            except struct.error: break
                            if not (entry <= v < entry + size_cap): break
                            work.append(v); k += 1
                    elif op.type == X86_OP_MEM and IAT[0] <= (op.mem.disp & 0xFFFFFFFF) < IAT[1]: res['imports'].add(op.mem.disp & 0xFFFFFFFF)
                    else: res['indirect'] += 1
                    break
                if m == 'ret': break
                if m.startswith('j') and ins.operands[0].type == X86_OP_IMM:
                    t = ins.operands[0].imm & 0xFFFFFFFF
                    if entry <= t < entry + size_cap: work.append(t)
                    else: res['calls'].add(t)
                a += ins.size
        res['size'] = res['hi'] - entry
        return res
    cache = {}
    def closure(root):
        out = {}; work = [(root, 0)]
        while work:
            f, d = work.pop()
            if f in out: continue
            if f not in cache: cache[f] = scan(f)
            out[f] = cache[f]
            if d < depth_cap:
                for c in cache[f]['calls']:
                    if TEXT[0] <= c < TEXT[1] and c not in out: work.append((c, d + 1))
        return out
    res = {}
    for a in sorted(set(addrs)):
        if not (TEXT[0] <= a < TEXT[1]): res[a] = dict(cls='X', own=0, size=0, g=0); continue
        cl = closure(a)
        g = set(); imp = set(); ind = 0; size = 0
        for r in cl.values(): g |= r['globals']; imp |= r['imports']; ind += r['indirect']; size += r['size']
        res[a] = dict(cls='D' if imp else 'C' if ind else 'B' if g else 'A', own=cache[a]['size'], size=size, g=len(g), imp=sorted(imp))
    return res

def import_names(exe_path):
    """IAT slot VA -> 'dll!name' (from the PE import directory of the exe)"""
    exe = open(exe_path, 'rb').read()
    pe = struct.unpack_from('<I', exe, 0x3C)[0]
    nsec = struct.unpack_from('<H', exe, pe + 6)[0]; opt = struct.unpack_from('<H', exe, pe + 20)[0]
    secs = [struct.unpack_from('<8sIIII', exe, pe + 24 + opt + 40 * i) for i in range(nsec)]
    def off(rva):
        for n, vs, va, rs, ro in secs:
            if va <= rva < va + max(vs, rs): return ro + rva - va
        return None
    imp_rva = struct.unpack_from('<I', exe, pe + 24 + 96 + 8)[0]
    names = {}
    o = off(imp_rva)
    while o is not None:
        oft, _, _, name, ft = struct.unpack_from('<IIIII', exe, o)
        if not name: break
        dll = exe[off(name):exe.index(b'\0', off(name))].decode().lower()
        k = 0
        while True:
            th = struct.unpack_from('<I', exe, off(oft or ft) + 4 * k)[0]
            if not th: break
            fn = '#%d' % (th & 0xFFFF) if th & 0x80000000 else exe[off(th) + 2:exe.index(b'\0', off(th) + 2)].decode()
            names[0x400000 + ft + 4 * k] = dll + '!' + fn
            k += 1
        o += 20
    return names

FAMILY = [('d3d / d3dx / graphics', ('d3d9', 'd3dx', 'ddraw', 'gdi32')), ('audio (DirectSound / OpenAL / winmm audio)', ('dsound', 'openal', 'wmvcore', 'winmm!wave', 'winmm!mixer')),
          ('input (DirectInput / user32 input)', ('dinput',)), ('video (DirectShow / COM)', ('ole32', 'oleaut32', 'quartz', 'strmiids')),
          ('file IO', ('createfile', 'readfile', 'writefile', 'setfilepointer', 'findfirst', 'findnext', 'getfilesize', 'getfileattributes', 'deletefile', 'copyfile', 'createdirectory')),
          ('timers / sleep', ('timegettime', 'queryperformance', 'gettickcount', 'sleep', 'getsystemtime', 'getlocaltime')),
          ('heap / virtual memory', ('heap', 'virtualalloc', 'virtualfree', 'globalalloc', 'localalloc')),
          ('threads / locks / TLS (CRT)', ('criticalsection', 'tls', 'fls', 'interlocked', 'createthread', 'waitfor', 'getlasterror', 'setlasterror', 'getcurrentthread', 'getcurrentprocess')),
          ('window / UI (user32)', ('user32',))]

def family(names):
    """first matching family of a function's imports (priority = list order)"""
    low = [n.lower() for n in names]
    for fam, keys in FAMILY:
        if any(k in n for n in low for k in keys): return fam
    return 'other kernel32 / CRT services'

def subsystem(f):
    p = f.split('/')[1:]
    if p[0] == 'game_sa':
        if len(p) > 3 and p[1] in ('Entity', 'Audio', 'Tasks', 'Fx', 'Plugins'): return 'game_sa/%s/%s' % (p[1], p[2])
        if len(p) > 2: return 'game_sa/%s' % p[1]
        return 'game_sa (root files)'
    return '/'.join(p[:2]) if len(p) > 2 else p[0]

# why a subsystem is (mostly) not oracle-able, shown next to its counts
WHY = {
    'game_sa/Audio': 'DirectSound / OpenAL / stream threads (imports); the pure maths parts are in world_oracle_audio.inc',
    'game_sa/Audio/Entities': 'audio entities drive sound hardware + vehicle/ped state through vtables; only the arithmetic helpers are testable',
    'game_sa/Audio/Hardware': 'DirectSound buffers, threads, file IO',
    'game_sa/Audio/Loaders': 'file decoders (vorbis/wma/wav): file IO + codecs',
    'game_sa/Audio/Managers': 'streaming / radio / ambience managers: file IO, threads',
    'game_sa/Pipelines': 'RenderWare pipeline callbacks: D3D9 device calls (covered by the rw_* device tests instead)',
    'game_sa/RenderWare': 'RW plugin glue, D3D',
    'game_sa/Plugins': 'RW plugin streams (RwStream* IO)',
    'game_sa/Fx': 'particle rendering / texture IO (the key-curve maths is covered)',
    'game_sa/Text': 'GXT file IO, fonts',
    'game_sa/Streaming': 'CdStream file reads, threads',
    'app/platform': 'window / input / video: Win32 + DirectShow',
    'game_sa/Animation': 'RW anim blend + file loading; blend maths partly covered',
}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--repo', default='.'); ap.add_argument('--exe', default=None); ap.add_argument('--baseline', default=None)
    ap.add_argument('--out', default=None); ap.add_argument('--json', default=None)
    a = ap.parse_args()
    repo = os.path.abspath(a.repo); exe = a.exe or os.path.join(repo, 'gta_sa_compact.exe')
    sites = inventory(repo)
    now = covered_addrs(repo); before = covered_addrs(repo, a.baseline) if a.baseline else None
    cl = classify(exe, [s['addr'] for s in sites])
    iat = import_names(exe)
    for s in sites:
        s['covered'] = s['addr'] in now; s['coveredBefore'] = (s['addr'] in before) if before is not None else None
        s['oc'] = cl[s['addr']]['cls']; s['own'] = cl[s['addr']]['own']; s['sub'] = subsystem(s['file'])
        s['why'] = family([iat.get(i, '?') for i in cl[s['addr']]['imp']]) if s['oc'] == 'D' else ''
        s['softD'] = s['oc'] == 'D' and all(family([iat.get(i, '?')]) in ('heap / virtual memory', 'threads / locks / TLS (CRT)') for i in cl[s['addr']]['imp'])   # only allocator / CRT-lock imports: Patch()-able
    for s in sites:
        if s['softD']: s['oc'] = 'S'      # ORACLE-W2: allocator / lock imports only -> oracle_alloc.h
    if a.json: json.dump(sites, open(a.json, 'w'))
    if a.out: write_report(a.out, sites, before is not None)
    tot = len(sites); c = sum(1 for s in sites if s['covered'])
    print('%d ported functions, %d covered now%s' % (tot, c, (', %d before' % sum(1 for s in sites if s['coveredBefore'])) if before is not None else ''))

def write_report(path, sites, has_before):
    by = collections.defaultdict(lambda: collections.Counter())
    for s in sites:
        b = by[s['sub']]; b['total'] += 1
        if s['covered']: b['cov'] += 1
        else: b[s['oc']] += 1
        if has_before and s['coveredBefore']: b['covBefore'] += 1
    tot = collections.Counter()
    for v in by.values(): tot.update(v)
    L = []
    L.append('# Exe-oracle coverage map')
    L.append('')
    L.append('Generated by `tools/standalone/oracle_coverage.py` (inventory of every `RH_Scoped*Install` site + static analysis of the original exe). Re-run it after adding oracle rows.')
    L.append('')
    L.append('* **%d** ported functions (hook sites with an exe address); **%d** covered by an exe-oracle row (address literal in `tests/standalone/*`)%s.' % (tot['total'], tot['cov'], (', %d at the start of ORACLE-W1' % tot['covBefore']) if has_before else ''))
    L.append('* The uncovered rest is classified by the exe function\'s call closure: **A** pure (arguments + `this` memory only), **B** touches exe globals only, **C** indirect (vtable) calls, **S** reaches only allocator / CRT-lock / TLS imports (oracle-able since ORACLE-W2: `tests/standalone/oracle_alloc.h`), **D** reaches another Win32/D3D/CRT import.')
    L.append('  A is easy (random objects, bit-exact compare), B medium (both sides share the statics at the exe addresses, snapshot/restore), C medium-hard (fake vtables with recording stubs), S like A/B/C once the exe\'s CRT heap / locks are redirected to the shared deterministic arena (`oracle::alloc::InstallExe`, rows via `w2::BothA`), D not oracle-able as is (the import table is not populated; `oracle::Patch` can replace single callees such as rand / malloc).')
    L.append('* Script command handlers are covered by opcode in the `script_oracle_*` tests, not by address: their sites appear as uncovered here when the handler itself is not listed in a table with its address.')
    L.append('')
    L.append('## Per subsystem')
    L.append('')
    L.append('| subsystem | ported | covered | uncovered A | B | C | S | D | note |')
    L.append('|---|---:|---:|---:|---:|---:|---:|---:|---|')
    for k, v in sorted(by.items(), key=lambda kv: -kv[1]['total']):
        L.append('| %s | %d | %d | %d | %d | %d | %d | %d | %s |' % (k, v['total'], v['cov'], v['A'], v['B'], v['C'], v['S'], v['D'], WHY.get(k, '')))
    L.append('| **total** | **%d** | **%d** | **%d** | **%d** | **%d** | **%d** | **%d** | |' % (tot['total'], tot['cov'], tot['A'], tot['B'], tot['C'], tot['S'], tot['D']))
    L.append('')
    L.append('## Why class D is not oracle-able (first import family reached by the closure, per subsystem)')
    L.append('')
    fams = collections.defaultdict(collections.Counter)
    for s in sites:
        if s['oc'] == 'D' and not s['covered']: fams[s['sub']][s['why']] += 1
    L.append('| subsystem | class D | reasons (count) |')
    L.append('|---|---:|---|')
    for k, v in sorted(fams.items(), key=lambda kv: -sum(kv[1].values()))[:25]:
        L.append('| %s | %d | %s |' % (k, sum(v.values()), '; '.join('%s %d' % (f, n) for f, n in v.most_common(4))))
    allf = collections.Counter()
    for v in fams.values(): allf.update(v)
    soft = sum(1 for s in sites if s['softD'] and not s['covered'])
    L.append('')
    L.append('**Class S (%d uncovered functions)** are the ones whose closure reached only allocator / CRT-lock imports (HeapAlloc / HeapFree / critical sections / TLS / last-error); they used to be class D. `tests/standalone/oracle_alloc.h` patches the exe\'s CRT entry points (malloc 0x824257, realloc 0x824269, calloc 0x824416, free 0x82413F, operator new 0x82119A / 0x821195; operator delete / delete[] are `jmp free`) and the kernel32 IAT slots (heap, critical sections, TLS/FLS, last-error, interlocked) to one deterministic bump arena that the port\'s operator new / CMemoryMgr use as well, so both sides get identical addresses and identical uninitialised bytes. Task / event objects come from the fake `CTaskPool` / `CEventPool` (`w2::PoolFx`) installed at the exe pool globals.' % soft)
    L.append('')
    L.append('Other imports (still class D): ' + '; '.join('%s %d' % (f, n) for f, n in allf.most_common()) + '.')
    L.append('')
    L.append('## Next candidates (uncovered, class A, B or S, 24+ bytes), by file')
    L.append('')
    files = collections.defaultdict(list)
    for s in sites:
        if not s['covered'] and s['oc'] in 'ABS' and s['own'] >= 24: files[s['file']].append(s)
    L.append('| file | A/B/S functions (>= 24 bytes) | examples |')
    L.append('|---|---:|---|')
    for f, ss in sorted(files.items(), key=lambda kv: -len(kv[1]))[:60]:
        ex = ', '.join('%s::%s' % (s['cls'] or '', s['fn']) for s in sorted(ss, key=lambda s: -s['own'])[:3])
        L.append('| %s | %d | %s |' % (f[len('source/'):], len(ss), ex))
    L.append('')
    open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(L) + '\n')

if __name__ == '__main__':
    main()
