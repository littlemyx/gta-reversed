#!/usr/bin/env python3
"""calibrate.py - recall / false-positive numbers for asmsig.

usage: py -X utf8 -I tools/standalone/asmsig/calibrate.py --pre-port PRE.exe --post-port POST.exe --picks picks.tsv [--exe EXE] [--out calib.tsv]
  picks.tsv: addr(hex, no 0x)  function  fix_commit  file  ts     (a FIXED row whose fix was reverted in the PRE build)
  PRE  = port built with the fix commits reverted (the deviation present), POST = current port (deviation fixed).
  Recall   = share of picks whose PRE score >= T.   Fix-visible = signals present in PRE and gone in POST (the signal the fix removed).
  FP rate  = share of MATCH rows of .notes/asm_sweep/*.result.tsv (verified equal to the exe) whose POST score >= T.
"""
import argparse, collections, glob, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import asmsig as A  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('--pre-port'); ap.add_argument('--post-port'); ap.add_argument('--picks'); ap.add_argument('--exe', default=A.EXE_DEFAULT)
ap.add_argument('--repo', default='.'); ap.add_argument('--out', default='calib.tsv'); ap.add_argument('--max-match', type=int, default=100000)
a = ap.parse_args()
pre = A.Ctx(a.repo, a.exe, a.pre_port, os.path.splitext(a.pre_port)[0] + '.map')
post = A.Ctx(a.repo, a.exe, a.post_port, os.path.splitext(a.post_port)[0] + '.map')

rows = []
for l in open(a.picks, encoding='utf-8').read().splitlines():
    ad, fn, h, f, ts = l.split('\t')
    ad = int(ad, 16)
    r0, r1 = A.port_sig(pre, ad), A.port_sig(post, ad)
    rows.append((ad, fn, h, r0, r1))
with open(a.out, 'w', encoding='utf-8') as f:
    f.write('addr\tfunction\tfix\tpre_score\tpost_score\tsignals_removed_by_fix\tpre_signals\n')
    for ad, fn, h, r0, r1 in rows:
        if r0[0] is None or r1[0] is None:
            f.write('%x\t%s\t%s\tNO_DATA\t\t%s\t\n' % (ad, fn, h, r0[1] if r0[0] is None else r1[1])); continue
        gone = [s for s in r0[1] if s not in r1[1]]
        f.write('%x\t%s\t%s\t%.1f\t%.1f\t%s\t%s\n' % (ad, fn, h, r0[0], r1[0], ' '.join(gone), ' '.join(r0[1])))
ok = [(r[3][0], r[4][0]) for r in rows if r[3][0] is not None and r[4][0] is not None]
print('picks with data: %d of %d' % (len(ok), len(rows)))

match = []
for p in glob.glob(os.path.join(a.repo, '.notes/asm_sweep/*.result.tsv')):
    for l in open(p, encoding='utf-8', errors='replace').read().splitlines()[1:]:
        c = l.split('\t')
        if len(c) > 2 and c[2] == 'MATCH':
            try:
                match.append(int(c[0], 16))
            except ValueError:
                pass
match = sorted(set(match))[:a.max_match]
ms = []
for ad in match:
    try:
        r = A.port_sig(post, ad)
    except Exception:  # noqa
        continue
    if r[0] is not None:
        ms.append(r[0])
print('MATCH rows with data: %d' % len(ms))
print('T     recall(pre>=T)   fix-visible(pre-post>=T)   FP(MATCH>=T)')
for T in (1, 2, 3, 4, 5, 6, 8, 10):
    rec = sum(1 for x, _ in ok if x >= T)
    vis = sum(1 for x, y in ok if x - y >= T)
    print('%-5g %2d/%d %4.0f%%        %2d/%d %4.0f%%             %5.1f%%' % (T, rec, len(ok), 100.0 * rec / max(1, len(ok)), vis, len(ok), 100.0 * vis / max(1, len(ok)),
                                                                       100.0 * sum(1 for x in ms if x >= T) / max(1, len(ms))))
