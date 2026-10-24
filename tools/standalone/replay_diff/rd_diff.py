#!/usr/bin/env python3
"""REPLAY-DIFF: compare two lockstep traces (rd_orig.c / source/standalone/Lockstep.cpp output). Frames are aligned by their index among the "F" lines (both sides start at the first frame with a player ped).
usage: py -I rd_diff.py A.trace B.trace [--skip-fc]      prints: frames in A/B, equal-prefix length, per-field first mismatch, equal frames overall (full line / without the timer fields)
F line: F fc tms rng ncalls px py pz hp peds vehs scrhash        S line: S fc ra arg  (ra is ignored)"""
import sys
F = ['fc', 'tms', 'rng', 'ncalls', 'px', 'py', 'pz', 'hp', 'peds', 'vehs', 'scr']

def load(p):
    fr, cur_s = [], []
    for ln in open(p, errors='replace'):
        t = ln.split()
        if not t:
            continue
        if t[0] == 'S' and len(t) >= 4:
            cur_s.append((t[1], t[3]))
        elif t[0] == 'F' and len(t) == 12:
            fr.append((dict(zip(F, t[1:])), cur_s)); cur_s = []
    return fr

def main():
    a, b = load(sys.argv[1]), load(sys.argv[2])
    n = min(len(a), len(b))
    for fr in (a, b):   # fc is compared relative to the first traced frame (the menu/loading length before the game starts differs per run; the plan pins it)
        if fr:
            f0 = int(fr[0][0]['fc'])
            for d, _ in fr:
                d['fc'] = str(int(d['fc']) - f0)
    print(f'frames: A={len(a)} B={len(b)} compared={n}')
    first = {}
    pref = None
    eq_full = eq_notime = 0
    for i in range(n):
        fa, sa = a[i]; fb, sb = b[i]
        bad = [k for k in F if fa[k] != fb[k]]
        if sa != sb:
            bad.append('srand')
        if not bad and pref is None:
            pass
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
    for k, i in sorted(first.items(), key=lambda kv: kv[1]):
        fa, sa = a[i]; fb, sb = b[i]
        print(f'  first mismatch {k:7s} at frame {i}: A={fa.get(k, sa[:3])} B={fb.get(k, sb[:3])}')

main()
