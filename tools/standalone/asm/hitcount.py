#!/usr/bin/env python3
"""hitcount: resolve a GTASA_HITCOUNT dump (rva count rows) to functions, exe addresses (hook registry) and asm verdicts.

usage: py -I tools/standalone/asm/hitcount.py --dump <hitcount.txt> --map <gta_reversed.map> [--repo .] [--out C:/gta-scratch/statehash/hitcount/ryder1.tsv] [--top 50]
Output columns: exe_addr (hook registry address of the function, '-' when it is not hooked or the name is not found; ambiguous overloads: a|b),
  function (undecorated name, no arguments), count, file (object file from the map), verdict (best OK verdict in .notes/asm_sweep/*.tsv of any of the exe addresses, else NONE),
  other (non-OK verdict text if there is one). Sorted by count. OK verdict = the noverdict.py list (MATCH FIXED INTENTIONAL SUPERSEDED DEVIATION_FIXED CONFIRMED RESOLVED TRIVIAL).
Name resolution: .map symbol -> UnDecorateSymbolName(NAME_ONLY) (dbghelp via ctypes) -> 'Class::fn' -> RH_Scoped*Install sites (tools/standalone/oracle_coverage.inventory).
"""
import argparse, bisect, collections, csv, ctypes, glob, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..'))
import oracle_coverage as oc

OK = re.compile(r'^(MATCH|FIXED|INTENTIONAL|SUPERSEDED|DEVIATION[_ ]?\(?FIXED|CONFIRMED|RESOLVED|TRIVIAL)', re.I)
SYM = re.compile(r'^\s*([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+(\S+)\s+([0-9a-fA-F]{16})\s+(f\s+)?(i\s+)?(\S+)')


def undname():
    dh = ctypes.WinDLL('dbghelp')
    f = dh.UnDecorateSymbolName
    f.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32, ctypes.c_uint32]
    f.restype = ctypes.c_uint32
    buf = ctypes.create_string_buffer(4096)

    def un(s):
        if not s.startswith('?'):
            return s
        n = f(s.encode(), buf, 4096, 0x1000)  # UNDNAME_NAME_ONLY
        return buf.value.decode(errors='replace') if n else s
    return un


def read_map(path):
    base = None; syms = []
    for ln in open(path, encoding='utf-8', errors='replace'):
        if base is None and 'Preferred load address is' in ln:
            base = int(ln.split()[-1], 16)
            continue
        m = SYM.match(ln)
        if m and m.group(1) == '0001' and m.group(5):
            syms.append((int(m.group(4), 16), m.group(3), m.group(7)))
    syms.sort()
    return base, syms


def verdicts(repo):
    ok = {}; other = {}
    rank = {}
    for p in glob.glob(repo + '/.notes/asm_sweep/*.tsv'):
        with open(p, encoding='utf-8', errors='replace', newline='') as f:
            rd = csv.reader(f, delimiter='\t')
            hdr = next(rd, None)
            if not hdr: continue
            h = [c.strip().lower() for c in hdr]
            if 'verdict' not in h: continue
            ia = h.index('addr') if 'addr' in h else (h.index('exe_addr') if 'exe_addr' in h else None)
            if ia is None: continue
            iv = h.index('verdict')
            for r in rd:
                if len(r) <= max(ia, iv): continue
                m = re.match(r'(?:0x)?([0-9A-Fa-f]{5,8})$', r[ia].strip())
                if not m: continue
                a = int(m.group(1), 16); v = r[iv].strip()
                if OK.match(v):
                    w = v.split()[0].upper()
                    # FIXED outranks MATCH (the function was found wrong at some point), keep any other OK word otherwise
                    if a not in ok or (w.startswith('FIXED') and not ok[a].startswith('FIXED')): ok[a] = w
                elif v:
                    other[a] = v.split()[0].upper()
    return ok, other


def inventory(repo):
    """oracle_coverage.inventory without its reset of the class at RH_ScopedCategoryGlobal (RH_ScopedClass(X) is followed by it in most files)."""
    out = []
    for path in sorted(glob.glob(repo + '/source/**/*.cpp', recursive=True)):
        cur = None
        for ln in open(path, encoding='utf-8', errors='replace'):
            if ln.lstrip().startswith('//') or 'define' in ln: continue
            m = oc.CLS.match(ln) or oc.NS.match(ln)
            if m: cur = m.group(1); continue
            m = oc.SITE.match(ln)
            if not m or m.group(1) == 'RH_ScopedInstallScriptCommand': continue
            fn = m.group(2).strip()
            if m.group(1).startswith('RH_ScopedNamed'): fn = m.group(3).strip('"')
            out.append(dict(cls=cur, fn=fn, addr=int(m.group(4), 16)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dump', required=True); ap.add_argument('--map', required=True)
    ap.add_argument('--repo', default='.'); ap.add_argument('--out', default='C:/gta-scratch/statehash/hitcount/ryder1.tsv')
    ap.add_argument('--top', type=int, default=50)
    a = ap.parse_args()
    base, syms = read_map(a.map)
    addrs = [s[0] for s in syms]
    un = undname()
    by = collections.defaultdict(set)
    for s in inventory(a.repo):
        fn = s['fn'].split('-')[0]
        by[(s['cls'] + '::' if s['cls'] else '') + fn].add(s['addr'])
        by[fn].add(s['addr']) if not s['cls'] else None
    ok, other = verdicts(a.repo)
    agg = collections.OrderedDict()
    unres = 0
    for ln in open(a.dump):
        if ln.startswith('#'): continue
        rva, cnt = ln.split(); va = base + int(rva, 16); cnt = int(cnt)
        i = bisect.bisect_right(addrs, va) - 1
        if i < 0: unres += 1; continue
        _, dec, obj = syms[i]
        agg.setdefault(dec, [obj, 0]); agg[dec][1] += cnt
    rows = []
    for dec, (obj, cnt) in agg.items():
        name = un(dec)
        cand = by.get(name) or by.get('::'.join(name.split('::')[-2:]))
        ea = sorted(cand) if cand else []
        v = next((ok[x] for x in ea if x in ok), 'NONE')
        o = next((other[x] for x in ea if x in other), '')
        rows.append(('|'.join('0x%X' % x for x in ea) or '-', name, cnt, obj, v, o))
    rows.sort(key=lambda r: -r[2])
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(['exe_addr', 'function', 'count', 'file', 'verdict', 'other'])
        w.writerows(rows)
    hooked = [r for r in rows if r[0] != '-']
    nov = [r for r in hooked if r[4] == 'NONE']
    print('functions executed (distinct symbols): %d, unresolved rows: %d' % (len(rows), unres))
    print('hooked (exe addr known): %d, hooked with verdict: %d, hooked NONE: %d, unhooked: %d' % (len(hooked), len(hooked) - len(nov), len(nov), len(rows) - len(hooked)))
    print('top %d hooked NONE by count:' % a.top)
    for r in nov[:a.top]:
        print('%s\t%s\t%d' % (r[0], r[1], r[2]))


if __name__ == '__main__':
    main()
