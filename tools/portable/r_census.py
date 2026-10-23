#!/usr/bin/env python3
"""r_census.py - count the stream-R diagnostics (render zones) in a native census TSV.

usage: r_census.py TSV [TSV2 ...] [--files] [--top N] [--zone-detail]
The TSV is the --tsv output of tools/standalone/clang_census_report.py (columns: category, file, line, msg). Several TSVs are
shown side by side (first = baseline) so a slice reports before/after in one table.

R-owned zones (STREAM_R_PLAN.md section 1): everything below source/standalone/rw, source/game_sa/Pipelines,
source/game_sa/RenderWare/D3D*, source/fakerw, source/game_sa/RenderWare/rw/skeleton.h, Gamma.cpp, Radar.cpp.
Rows are counted by the file they are located in (a diagnostic in a system header, e.g. emmintrin.h, is not R).
"""
import argparse, collections, csv, re, sys

ZONES = [
    ('standalone/rw', re.compile(r'(^|/)source/standalone/rw/')),
    ('game_sa/Pipelines', re.compile(r'(^|/)source/game_sa/Pipelines/')),
    ('game_sa/RenderWare/D3D*', re.compile(r'(^|/)source/game_sa/RenderWare/D3D')),
    ('fakerw', re.compile(r'(^|/)source/fakerw/')),
    ('skeleton.h', re.compile(r'(^|/)source/game_sa/RenderWare/rw/skeleton\.h')),
    ('Gamma/Radar', re.compile(r'(^|/)source/game_sa/(Gamma|Radar)\.cpp')),
]


def load(path):
    with open(path, newline='') as fh:
        rows = list(csv.reader(fh, delimiter='\t'))[1:]
    return [r for r in rows if len(r) >= 4]


def zone_of(f):
    for name, rx in ZONES:
        if rx.search(f):
            return name
    return None


def stats(rows):
    per_file = collections.Counter()
    per_zone = collections.Counter()
    total = len(rows)
    uniq = set()
    for cat, f, line, msg in (r[:4] for r in rows):
        z = zone_of(f)
        if z:
            per_zone[z] += 1
            per_file[re.sub(r'^.*?source/', '', f)] += 1
    return total, per_zone, per_file


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('tsv', nargs='+')
    ap.add_argument('--files', action='store_true', help='per-file table of the R zones')
    ap.add_argument('--top', type=int, default=40)
    a = ap.parse_args()
    S = [stats(load(p)) for p in a.tsv]
    hdr = ['item'] + [p.rsplit('/', 1)[-1] for p in a.tsv]
    print('| ' + ' | '.join(hdr) + ' |'); print('|' + '---|' * len(hdr))
    print('| total | ' + ' | '.join(str(s[0]) for s in S) + ' |')
    for name, _ in ZONES:
        print('| ' + name + ' | ' + ' | '.join(str(s[1].get(name, 0)) for s in S) + ' |')
    print('| **R** | ' + ' | '.join('**%d**' % sum(s[1].values()) for s in S) + ' |')
    if a.files:
        files = sorted({f for s in S for f in s[2]}, key=lambda f: -S[-1][2].get(f, 0) - S[0][2].get(f, 0))
        print(); print('| file | ' + ' | '.join(hdr[1:]) + ' |'); print('|' + '---|' * len(hdr))
        for f in files[:a.top]:
            print('| ' + f + ' | ' + ' | '.join(str(s[2].get(f, 0)) for s in S) + ' |')


if __name__ == '__main__':
    main()
