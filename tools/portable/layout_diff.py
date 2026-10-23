#!/usr/bin/env python3
"""layout_diff.py - P-2a gate: compare the record layouts of the FILE structs on the exe target (i686-pc-windows-msvc) and the portable target (arm64-apple-macos).

  layout_diff.py run   [--tsv T] [--out REPORT.md] [--work DIR] [-j N] [--llvm DIR] [--fail-on-new]
  layout_diff.py guard [--work DIR]       list every call site of the typed Save/Load/RwStreamRead templates that the 64-bit scalar guard (FileRecord.hpp) flags

run:
  * rows = the struct names of .notes/reports/MAC-P1-file-structs.tsv (131 rows; '#' lines ignored) + every record whose name ends in `_File` (the P-2 file records).
  * probe TUs = every .cpp named in the TSV evidence column + one synthetic TU that includes every evidence header (after StdInc). Each is compiled twice with
    `-fsyntax-only -Xclang -fdump-record-layouts-complete` :  msvc = clang-cl --target=i686-pc-windows-msvc with the -D/-I set of build/StandaloneRelease (the exe build),
    native = clang++ -target arm64-apple-macos15 with the -D/-I set of build/PortableMac and the flags of tools/standalone/clang_census.py (-fms-extensions -mms-bitfields ...).
  * per struct: sizeof, alignof, and every field path -> byte offset (bit-fields: byte:bit-range), compared field by field. Vptr/vbptr lines are ignored (they show in size/offsets).
  * "known" = the 15 FILE structs of the MAC-P1 failing list (+ StdFrame); any other differing row is a NEW finding and headlined.
Needs the build trees build/StandaloneRelease and build/PortableMac (compile_commands.json) and brew LLVM + ~/tools/msvc (msvc-wine). Cached per TU/mode under --work.
"""
import argparse, concurrent.futures, hashlib, importlib.util, json, os, re, shlex, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SRC = os.path.join(REPO, 'source') + os.sep
DEF_TSV = os.path.join(REPO, '.notes', 'reports', 'MAC-P1-file-structs.tsv')
DEF_OUT = os.path.join(REPO, '.notes', 'reports', 'MAC-P2a-layout.md')
KNOWN = {'CRunningScript', 'tScriptParam', 'CPickup', 'tRadarTrace', 'CWeapon', 'CPedSaveStructure', 'CMatrix', 'CVehicleSaveStructure',
         'CSimpleVariablesSaveStructure', 'CPathNode', 'BreakInfo_t', 'BreakablePlugin', 'ExtraVertColour', 'tReplayCameraBlock', 'rw::HAnimKeyFrame', 'StdFrame'}


ALIASES = {'RwV3d': 'rw::V3d', 'RwV2d': 'rw::V2d', 'RwRGBA': 'rw::RGBA', 'RwTexCoords': 'rw::TexCoords', 'RwMatrix': 'rw::Matrix', 'RpTriangle': 'rw::Triangle',
           'StdFrame': 'rw::HAnimInterpFrame', 'CGangs::Gang': 'CGangInfo', 'ZoneInfoArray': 'CZoneInfo', 'TVertex': 'CVector'}   # TSV names that are typedefs / variables
THIRD_PARTY = {'OggVorbis_File'}   # *_File records of external libraries


def census():
    spec = importlib.util.spec_from_file_location('clang_census', os.path.join(REPO, 'tools', 'standalone', 'clang_census.py'))
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m


def read_tsv(path):
    rows = []
    for ln in open(path, encoding='utf-8', errors='replace').read().split('\n')[1:]:
        if not ln or ln.startswith('#'):
            continue
        c = ln.split('\t')
        if len(c) >= 3:
            rows.append((c[0], c[1], ' '.join(c[2:])))
    return rows


def evidence_files(rows):
    fs = set()
    for _, _, ev in rows:
        for m in re.finditer(r'source/[A-Za-z0-9_/.\-]+\.(?:cpp|hpp|h)\b', ev):
            fs.add(m.group(0))
    return sorted(fs)


# ---- dump parsing -------------------------------------------------------------------------------------------------------------------------------------------
HDR = re.compile(r'^\*\*\* Dumping AST Record Layout')
LINE = re.compile(r'^\s*(\d+(?::\d+-\d+)?)\s\|(\s*)(.*)$')
FOOT = re.compile(r'\[sizeof=(\d+), (?:dsize=\d+, )?align=(\d+)')


def parse_dump(text, wanted):
    """-> {name: {'size':, 'align':, 'fields': {path: offset}, 'raw': [...]}} for the records whose name matches `wanted(name)`; first occurrence wins."""
    out, cur, stack = {}, None, []
    lines = text.split('\n')
    i, n = 0, len(lines)
    while i < n:
        l = lines[i]
        if HDR.match(l):
            i += 1
            # next line: "  0 | struct Name" (record header, indent 1)
            m = LINE.match(lines[i]) if i < n else None
            name = None
            if m:
                t = m.group(3)
                mm = re.match(r'(?:struct|class|union)\s+(.*)$', t)
                name = mm.group(1).strip() if mm else None
            cur = None
            if name and wanted(name) and name not in out:
                cur = {'name': name, 'size': None, 'align': None, 'fields': {}, 'order': []}
                stack = []
            i += 1
            while i < n and not HDR.match(lines[i]):
                l = lines[i]
                if cur is not None:
                    f = FOOT.search(l)
                    if f:
                        cur['size'], cur['align'] = int(f.group(1)), int(f.group(2))
                    else:
                        m = LINE.match(l)
                        if m:
                            off, ind, txt = m.group(1), len(m.group(2)), m.group(3)
                            if re.match(r'\((?:vtable|vftable|vbtable|vbptr|vfptr)', txt) or 'vtable pointer' in txt or 'vbtable pointer' in txt:
                                pass
                            else:
                                while stack and stack[-1][0] >= ind:
                                    stack.pop()
                                nm = field_name(txt)
                                path = '.'.join([s[1] for s in stack] + [nm])
                                k = path; c = 2
                                while k in cur['fields']:
                                    k = '%s#%d' % (path, c); c += 1
                                cur['fields'][k] = off
                                cur['order'].append(k)
                                stack.append((ind, nm))
                i += 1
            if cur is not None and cur['size'] is not None:
                out[cur['name']] = cur
            continue
        i += 1
    return out


def field_name(txt):
    txt = txt.strip()
    m = re.match(r'\(base\)\s*(?:struct|class|union)?\s*(.*)$', txt)
    if m:
        return '<base %s>' % m.group(1).strip()
    m = re.match(r'\(primary base\)', txt)
    if m:
        return '<primary base>'
    # "type name" ; name is the last identifier, possibly followed by array dims? the dump prints "float [3] name"
    m = re.search(r'([A-Za-z_][A-Za-z0-9_]*)\s*$', txt)
    nm = m.group(1) if m else txt
    return '<elems>' if nm in ('_Elems', '__elems_', '__elems') else nm   # std::array storage member: MSVC STL vs libc++ spelling


# ---- running --------------------------------------------------------------------------------------------------------------------------------------------------
def load_cc(path, game_only=True):
    cc = {}
    for e in json.load(open(path)):
        f = e['file']
        if not f.startswith(SRC) or '/libs/' in f:
            continue
        cmd = shlex.split(e['command'])
        fo = next((a for a in cmd if a.startswith('/Fo')), '')
        if not fo and '-o' in cmd[:-1]:
            fo = cmd[cmd.index('-o') + 1]
        if game_only and 'gta_reversed.dir' not in fo:
            continue
        cc.setdefault(f, cmd)
    return cc


def dump_tu(job):
    mode, cen, llvm, work, f, cmd, tu_real, hxx = job
    key = hashlib.md5((mode + f).encode()).hexdigest()[:8] + '_' + mode + '_' + os.path.basename(f)
    cache = os.path.join(work, key + '.dump')
    if os.path.exists(cache):
        return mode, f, cache, 0
    defs, incs, sysincs = cen.split_cmd(cmd)
    dumpflags = ['-Xclang', '-fdump-record-layouts-complete']
    if mode == 'msvc':
        argv = cen.msvc_flags(llvm, defs, incs, sysincs, []) + ['-fsyntax-only'] + dumpflags + ['/FI' + hxx, '--', tu_real]
    else:
        argv = cen.native_flags(llvm, defs, incs, sysincs, []) + dumpflags + ['-include', hxx, tu_real]
    t0 = time.time()
    p = subprocess.run(argv, capture_output=True, text=True, errors='replace', timeout=1800)
    open(cache, 'w').write(p.stdout)
    open(cache + '.err', 'w').write(p.stderr)
    return mode, f, cache, time.time() - t0


def cmd_run(a):
    cen = census()
    work = os.path.abspath(a.work); os.makedirs(work, exist_ok=True)
    rows = read_tsv(a.tsv)
    names = [r[0] for r in rows]
    evid = evidence_files(rows)
    cc_nat = load_cc(os.path.join(REPO, 'build', 'PortableMac', 'compile_commands.json'))
    cc_msv = load_cc(os.path.join(REPO, 'build', 'StandaloneRelease', 'compile_commands.json'))
    hxx = os.path.join(work, 'pch.hxx')
    open(hxx, 'w').write('#ifdef __cplusplus\n#include "%s"\n#endif\n' % (SRC + 'StdInc.cpp'))
    # synthetic header probe
    probe = os.path.join(work, 'probe_headers.cpp')
    heads = [e for e in evid if e.endswith(('.h', '.hpp')) and os.path.exists(os.path.join(REPO, e))]
    open(probe, 'w').write('#include "%s"\n' % (SRC + 'StdInc.cpp') + ''.join('#include "%s"\n' % os.path.join(REPO, h) for h in heads))
    anchor = SRC + 'game_sa/GenericGameStorage.cpp'
    jobs, skipped = [], []
    tus = [os.path.join(REPO, e) for e in evid if e.endswith('.cpp')]
    for mode, cc in (('msvc', cc_msv), ('native', cc_nat)):
        for tu in tus:
            if tu in cc:
                jobs.append((mode, cen, a.llvm, work, tu, cc[tu], tu, hxx))
            else:
                skipped.append((mode, tu))
        jobs.append((mode, cen, a.llvm, work, probe, cc[anchor], probe, hxx))
    print('%d dump jobs (%d TUs + header probe, 2 modes); skipped (not in that compdb): %d' % (len(jobs), len(tus), len(skipped)), flush=True)
    t0 = time.time(); res = []
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        for k, r in enumerate(ex.map(dump_tu, jobs)):
            res.append(r)
            if (k + 1) % 10 == 0:
                print('%d/%d %.0fs' % (k + 1, len(jobs), time.time() - t0), flush=True)
    wanted_set = set(names)
    def wanted(n):
        return (n.endswith('_File') and n not in THIRD_PARTY) or n in wanted_set or n in ALIASES.values() or any(n.endswith('::' + w) for w in wanted_set)
    per = {'msvc': {}, 'native': {}}      # mode -> name -> list[(tu, rec)]
    for mode, f, cache, _ in res:
        recs = parse_dump(open(cache, errors='replace').read(), wanted)
        for nm, r in recs.items():
            per[mode].setdefault(nm, []).append((os.path.relpath(f, REPO), r))
    write_report(a, rows, per, skipped)


def sigof(r):
    return (r['size'], r['align'], tuple((k, r['fields'][k]) for k in r['order']))


def compare(rm, rn):
    d = []
    if rm['size'] != rn['size']:
        d.append('sizeof %d -> %d' % (rm['size'], rn['size']))
    if rm['align'] != rn['align']:
        d.append('alignof %d -> %d' % (rm['align'], rn['align']))
    fm, fn = rm['fields'], rn['fields']
    for k in rm['order']:
        if k not in fn:
            d.append('field %s only on x86 (@%s)' % (k, fm[k]))
        elif fm[k] != fn[k]:
            d.append('%s: %s -> %s%s' % (k, fm[k], fn[k], ' (bit-field)' if ':' in fm[k] + fn[k] else ''))
    for k in rn['order']:
        if k not in fm:
            d.append('field %s only on arm64 (@%s)' % (k, fn[k]))
    return d


def write_report(a, rows, per, skipped):
    names = [r[0] for r in rows]
    allnames = list(dict.fromkeys(names))
    # resolve a TSV name to dump names
    def resolve(mode, w):
        w = ALIASES.get(w, w)
        hit = {n: v for n, v in per[mode].items() if n == w or n.endswith('::' + w)}
        return hit
    extra = sorted({n for m in per.values() for n in m if n.endswith('_File') and n not in THIRD_PARTY} - set(allnames))
    lines_tab, diffs, newfind, notfound, ident, internal = [], [], [], [], 0, []
    status = {}
    for w in allnames + extra:
        hm, hn = resolve('msvc', w), resolve('native', w)
        if not hm or not hn:
            status[w] = ('NOT FOUND', 'x86' if not hm else '' + ('' if hn else ' arm64'), [])
            notfound.append((w, 'missing on: ' + ', '.join(m for m, h in (('x86', hm), ('arm64', hn)) if not h)))
            continue
        ds = []
        for nm in sorted(set(hm) | set(hn)):
            if nm not in hm or nm not in hn:
                ds.append('%s: only on %s' % (nm, 'x86' if nm in hm else 'arm64')); continue
            # within-mode consistency: all TUs that dump the record agree
            for mode, h in (('x86', hm), ('arm64', hn)):
                sigs = {sigof(r) for _, r in h[nm]}
                if len(sigs) > 1:
                    internal.append('%s (%s): %d distinct layouts across TUs (%s)' % (nm, mode, len(sigs), ', '.join(sorted({t for t, _ in h[nm]})[:3])))
            dd = compare(hm[nm][0][1], hn[nm][0][1])
            for x in dd:
                ds.append('%s: %s' % (nm, x) if len(set(hm) | set(hn)) > 1 else x)
        if ds:
            known = w in KNOWN
            status[w] = ('DIFF' if known else 'DIFF (NEW)', '', ds)
            (diffs if known else newfind).append((w, ds))
        else:
            status[w] = ('identical', '', [])
            ident += 1
    out = []
    out.append('# MAC-P2a layout diff: i686-pc-windows-msvc (exe build) vs arm64-apple-macos (portable build)\n')
    out.append('Generated by `tools/portable/layout_diff.py run` (%s). Rows: %d TSV names + %d `_File` records. x86 = clang-cl, defs/includes of build/StandaloneRelease; arm64 = clang++ -mms-bitfields, defs/includes of build/PortableMac.\n'
               % (time.strftime('%Y-%m-%d'), len(allnames), len(extra)))
    nd = len(diffs) + len(newfind)
    out.append('## Headline\n')
    out.append('* identical: **%d**, differing: **%d** (known MAC-P1 failing structs: %d, **NEW: %d**), not found in one of the builds: **%d**.' % (ident, nd, len(diffs), len(newfind), len(notfound)))
    if newfind:
        out.append('* **NEW findings (not among the known failing structs): %s**' % ', '.join(w for w, _ in newfind))
    else:
        out.append('* No new differing struct beyond the known list.')
    out.append('')
    if newfind:
        out.append('## NEW differences (extra P-2 rows needed)\n')
        for w, ds in newfind:
            out.append('### %s' % w)
            out += ['* ' + x for x in ds[:60]]
            out.append('')
    for w, ds in newfind:
        if 'HAnimNodeInfo' in w:
            out.append('Note on %s: this is the in-memory RpHAnimHierarchy node record (`RwFrame* pFrame`, fakerw.h:132), not a raw file record; the RW stream holds three int32 per node '
                       '(handled by librw). TSV marked it "doubt / no pointers" - it has one. Needs no P-2 file record, but nothing may dump/read `nodeInfo` raw (RwCompatHAnimNodeInfo reinterpret is in-memory only).' % w)
    out.append('')
    out.append('## Known differing structs (MAC-P1 failing list)\n')
    for w, ds in diffs:
        out.append('### %s' % w)
        out += ['* ' + x for x in ds[:40]] + (['* ... %d more' % (len(ds) - 40)] if len(ds) > 40 else [])
        out.append('')
    out.append('## Not found / not comparable\n')
    out.append('Names that are variables, typedefs of non-records, or defined in TUs not in one of the compile databases:\n')
    out += ['* `%s` - %s' % (w, why) for w, why in notfound]
    out.append('')
    if internal:
        out.append('## Same record, different layouts across TUs of one build (macro/define dependent)\n')
        out += ['* ' + x for x in internal]
        out.append('')
    if skipped:
        out.append('## Probe TUs skipped (not in that mode\'s compdb)\n')
        out += ['* %s: %s' % (m, os.path.relpath(t, REPO)) for m, t in skipped]
        out.append('')
    bf = sorted(w for w in allnames + extra if status[w][0] == 'identical' and any(':' in o for h in (resolve('native', w),) for v in h.values() for _, r in v for o in r['fields'].values()))
    out.append('## Bit-field audit (identical rows that contain bit-fields)\n')
    out.append('arm64 is compiled with `-mms-bitfields` (portable build flag), so the storage-unit rule of MSVC is emulated; every bit-field struct below has the same byte:bit offsets and width as the clang-cl i686 layout: %s.\n' % (', '.join('`%s`' % w for w in bf) or 'none'))
    out.append('Bit-fields inside rows that DIFFER (tRadarTrace, CPickup.m_nFlags, ...) only move by the preceding pointer growth; their position relative to the storage unit start is unchanged.\n')
    out.append('## All rows\n')
    out.append('| struct | kind | status | x86 size | arm64 size |')
    out.append('|---|---|---|---|---|')
    kind = {r[0]: r[1] for r in rows}
    for w in allnames + extra:
        hm, hn = resolve('msvc', w), resolve('native', w)
        sz = lambda h: ('/'.join(sorted({str(r['size']) for v in h.values() for _, r in v}))) if h else '-'
        out.append('| %s | %s | %s | %s | %s |' % (w, kind.get(w, 'record'), status[w][0], sz(hm), sz(hn)))
    open(a.out, 'w').write('\n'.join(out) + '\n')
    print('report %s: identical %d, known-diff %d, NEW %d, not found %d' % (a.out, ident, len(diffs), len(newfind), len(notfound)))
    if a.fail_on_new and (newfind or any(w.endswith('_File') for w, _ in diffs)):
        sys.exit(1)


# ---- guard offender list -----------------------------------------------------------------------------------------------------------------------------------
def cmd_guard(a):
    work = os.path.abspath(a.work); os.makedirs(work, exist_ok=True)
    pat = re.compile(r'SaveDataToWorkBuffer|LoadDataFromWorkBuffer|RwStreamRead<')
    users = []
    for root, _, fs in os.walk(os.path.join(REPO, 'source')):
        for f in fs:
            if f.endswith(('.cpp', '.h', '.hpp')) and 'FileRecord' not in f and f != 'GenericGameStorage.h':
                p = os.path.join(root, f)
                try:
                    if pat.search(open(p, errors='replace').read()):
                        users.append(os.path.relpath(p, os.path.join(REPO, 'source')))
                except OSError:
                    pass
    only = '(' + '|'.join(re.escape(u) for u in sorted(users) if u.endswith('.cpp')) + ')$'
    out = os.path.join(work, 'guard')
    subprocess.run(['rm', '-rf', out])
    cmd = [sys.executable, os.path.join(REPO, 'tools', 'standalone', 'clang_census.py'), 'run', '--mode', 'native', '--compdb',
           os.path.join(REPO, 'build', 'PortableMac', 'compile_commands.json'), '--out', out, '-j', str(a.jobs), '--only', only, '--extra=-Wuser-defined-warnings']
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
    hits = set()
    for f in os.listdir(os.path.join(out, 'logs')):
        for l in open(os.path.join(out, 'logs', f), errors='replace'):
            m = re.match(r'(/[^:]+):(\d+):(\d+): warning: 64-bit-unsafe scalar', l)
            if m:
                hits.add((os.path.relpath(m.group(1), REPO), int(m.group(2))))
    for p, ln in sorted(hits):
        print('%s:%d' % (p, ln))
    print('%d offender call sites in %d user TUs' % (len(hits), len([u for u in users if u.endswith('.cpp')])))


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest='cmd', required=True)
    r = sp.add_parser('run')
    r.add_argument('--tsv', default=DEF_TSV); r.add_argument('--out', default=DEF_OUT)
    r.add_argument('--work', default='/private/tmp/layout_diff_work'); r.add_argument('--llvm', default='/opt/homebrew/opt/llvm')
    r.add_argument('-j', '--jobs', type=int, default=5); r.add_argument('--fail-on-new', action='store_true')
    g = sp.add_parser('guard')
    g.add_argument('--work', default='/private/tmp/layout_diff_work'); g.add_argument('-j', '--jobs', type=int, default=5)
    a = ap.parse_args()
    {'run': cmd_run, 'guard': cmd_guard}[a.cmd](a)


if __name__ == '__main__':
    main()
