#!/usr/bin/env python3
"""noverdict: every hooked function (RH_Scoped*Install sites) with no verdict row in .notes/asm_sweep/*.tsv.

usage: py -I tools/standalone/asm/noverdict.py [--repo .] [--exe gta_sa_compact.exe] [--out noverdict.tsv] [--min-size N]
Covered = any row (addr + verdict columns) whose verdict starts with MATCH / FIXED / INTENTIONAL / SUPERSEDED /
DEVIATION_FIXED / DEVIATION (fixed) / CONFIRMED / RESOLVED / TRIVIAL.  AUTO_MATCH (P4T1 triage) is NOT a verdict.
Output columns: addr, name, file, size (estimate: min(next hooked address, first int3 after a ret)).
Prints counts per top-level source directory (source/<dir>/<sub>) to stdout.
"""
import argparse, collections, csv, glob, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..'))
import oracle_coverage as oc

OK = re.compile(r'^(MATCH|FIXED|INTENTIONAL|SUPERSEDED|DEVIATION[_ ]?\(?FIXED|CONFIRMED|RESOLVED|TRIVIAL)', re.I)


def covered(repo):
    cov = set()
    for p in glob.glob(repo + '/.notes/asm_sweep/*.tsv'):
        with open(p, encoding='utf-8', errors='replace', newline='') as f:
            rd = csv.reader(f, delimiter='\t')
            hdr = next(rd, None)
            if not hdr:
                continue
            h = [c.strip().lower() for c in hdr]
            if 'verdict' not in h:
                continue
            ia = h.index('addr') if 'addr' in h else (h.index('exe_addr') if 'exe_addr' in h else None)
            if ia is None:
                continue
            iv = h.index('verdict')
            for r in rd:
                if len(r) <= max(ia, iv) or not OK.match(r[iv].strip()):
                    continue
                m = re.match(r'(?:0x)?([0-9A-Fa-f]{5,8})$', r[ia].strip())
                if m:
                    cov.add(int(m.group(1), 16))
    return cov


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--repo', default='.')
    ap.add_argument('--exe', default='C:/Users/littl/github/gta-reversed/gta_sa_compact.exe')
    ap.add_argument('--out', default='noverdict.tsv')
    ap.add_argument('--min-size', type=int, default=0)
    a = ap.parse_args()
    exe = open(a.exe, 'rb').read()
    off = lambda va: va - 0x401000 + 0x400
    sites = oc.inventory(a.repo)
    addrs = sorted({s['addr'] for s in sites if 0x401000 <= s['addr'] < 0x858000})
    nxt = {x: (addrs[i + 1] if i + 1 < len(addrs) else 0x858000) for i, x in enumerate(addrs)}

    def size(va):
        lim = nxt[va]
        i = off(va) + 1
        end = off(lim)
        while i < end:
            if exe[i] == 0xCC and (exe[i - 1] == 0xC3 or exe[i - 3] == 0xC2):
                return i - off(va)
            i += 1
        return lim - va

    cov = covered(a.repo)
    rows = []; seen = set(); tot = collections.Counter(); nov = collections.Counter()
    for s in sites:
        d = '/'.join(s['file'].split('/')[:3])
        tot[d] += 1
        if s['addr'] in cov:
            continue
        key = (s['addr'], s['fn'])
        if key in seen:
            continue
        seen.add(key)
        sz = size(s['addr']) if 0x401000 <= s['addr'] < 0x858000 else 0
        if sz < a.min_size:
            continue
        nov[d] += 1
        rows.append((s['addr'], (s['cls'] + '::' if s['cls'] else '') + s['fn'], s['file'], sz))
    rows.sort()
    with open(a.out, 'w', newline='') as f:
        w = csv.writer(f, delimiter='\t', lineterminator='\n')
        w.writerow(['addr', 'name', 'file', 'size'])
        for r in rows:
            w.writerow(['0x%X' % r[0], r[1], r[2], '0x%X' % r[3]])
    print('sites %d, no verdict %d (rows, unique addr+name)' % (len(sites), len(rows)))
    for d, n in sorted(nov.items(), key=lambda x: -x[1]):
        print('%-40s %5d / %5d' % (d, n, tot[d]))


if __name__ == '__main__':
    main()
