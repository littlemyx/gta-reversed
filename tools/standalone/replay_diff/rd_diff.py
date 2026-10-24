#!/usr/bin/env python3
"""REPLAY-DIFF: compare two lockstep traces (rd_orig.c / source/standalone/Lockstep.cpp output). Frames are aligned by their index among the "F" lines (both sides start at the first frame with a player ped = the arm point).
usage: py -I rd_diff.py A.trace B.trace [--max N] [--quiet]
  prints: frames in A/B, equal-prefix length, per-field first mismatch (in frame order), equal frames overall (full line / without the timer fields), then one machine-readable line
  SUMMARY compared=N prefix=P equal=E first=<field>@<frame>    (first=none when every compared frame is equal)
  --max N: compare only the first N frames of each trace (the acceptance runs use 3000)
F line: F fc tms rng ncalls px py pz hp peds vehs scrhash        S line: S fc <tag> seed  (the tag - return address in the original, file:line in the port - is ignored; fc is relative to the first F line)"""
import sys
F = ['fc', 'tms', 'rng', 'ncalls', 'px', 'py', 'pz', 'hp', 'peds', 'vehs', 'scr']

def load(p):
    fr, cur_s = [], []
    for ln in open(p, errors='replace'):
        t = ln.split()
        if not t:
            continue
        if t[0] == 'S' and len(t) >= 4:
            cur_s.append((int(t[1]), t[3]))
        elif t[0] == 'F' and len(t) == 12:
            fr.append((dict(zip(F, t[1:])), cur_s)); cur_s = []
    return fr

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    mx = None
    if '--max' in sys.argv:
        mx = int(sys.argv[sys.argv.index('--max') + 1]); args.remove(str(mx))
    a, b = load(args[0]), load(args[1])
    n = min(len(a), len(b)) if mx is None else min(len(a), len(b), mx)
    f0s = []
    for fr in (a, b):   # fc is compared relative to the first traced frame (with pin 16 it is absolute-equal too; reported below)
        if fr:
            f0 = int(fr[0][0]['fc']); f0s.append(f0)
            for d, ss in fr:
                d['fc'] = str(int(d['fc']) - f0)
            for i, (d, ss) in enumerate(fr):
                fr[i] = (d, [(s[0] - f0, s[1]) for s in ss])
    print(f'frames: A={len(a)} B={len(b)} compared={n}  first fc A/B={f0s}')
    first = {}
    pref = None
    eq_full = eq_notime = 0
    for i in range(n):
        fa, sa = a[i]; fb, sb = b[i]
        bad = [k for k in F if fa[k] != fb[k]]
        if sa != sb:
            bad.append('srand')
        if bad and pref is None:
            pref = i
        for k in bad:
            first.setdefault(k, i)
        if not bad:
            eq_full += 1
        if not [k for k in bad if k not in ('fc', 'tms')]:
            eq_notime += 1
    print(f'equal prefix: {pref if pref is not None else n} frames')
    print(f'equal frames overall: {eq_full}/{n} (ignoring fc/tms: {eq_notime}/{n})')
    order = sorted(first.items(), key=lambda kv: kv[1])
    for k, i in order:
        fa, sa = a[i]; fb, sb = b[i]
        print(f'  first mismatch {k:7s} at frame {i}: A={fa.get(k, sa[:3])} B={fb.get(k, sb[:3])}')
    fk = f'{order[0][0]}@{order[0][1]}' if order else 'none'
    print(f'SUMMARY compared={n} prefix={pref if pref is not None else n} equal={eq_full} first={fk}')

main()
