#!/usr/bin/env python3
"""cmp.py <a.hash> <b.hash> [--cols tm,ck,...]   (PARITY, see C:/gta-scratch/parity/FORMAT.md)
Compares two per-frame hash files (SH1: `frame randCount tm ck rng pop pl cam pd vh ob all [sc]`, lines starting with # skipped).
Prints: the first divergent frame + the categories that differ in it (in hash order; the first one is the suspect), the first divergence of EVERY category, and the frame counts.
Exit 0 = identical over the common frames and equal length, 1 = divergence or different length, 2 = usage / bad file."""
import sys

BASE = ["frame", "randCount", "tm", "ck", "rng", "pop", "pl", "cam", "pd", "vh", "ob", "all", "sc"]

def load(p):
    rows, hdr = {}, ""
    with open(p, encoding="utf-8") as f:
        for ln in f:
            ln = ln.strip()
            if not ln:
                continue
            if ln.startswith("#"):
                hdr = hdr or ln
                continue
            t = ln.split()
            rows[int(t[0])] = t
    return rows, hdr

def main(a):
    args = [x for x in a if not x.startswith("--")]
    if len(args) != 2:
        print(__doc__); return 2
    ra, ha = load(args[0]); rb, hb = load(args[1])
    fa, fb = sorted(ra), sorted(rb)
    first = None; per = {}
    for fr in sorted(set(fa) & set(fb)):
        x, y = ra[fr], rb[fr]
        n = min(len(x), len(y))
        bad = [BASE[i] for i in range(1, n) if x[i] != y[i]]
        if len(x) != len(y):
            bad.append("columns(%d!=%d)" % (len(x), len(y)))
        for c in bad:
            per.setdefault(c, fr)
        if bad and first is None:
            first = (fr, bad)
    print("frames: a=%d b=%d (common %d)" % (len(fa), len(fb), len(set(fa) & set(fb))))
    if first:
        print("FIRST DIVERGENT FRAME %d: categories %s" % (first[0], ",".join(first[1])))
        x, y = ra[first[0]], rb[first[0]]
        for c in first[1]:
            if c in BASE:
                i = BASE.index(c)
                print("  %-9s a=%s b=%s" % (c, x[i], y[i]))
        print("first divergence per category: " + " ".join("%s@%d" % (c, per[c]) for c in BASE[1:] if c in per))
        print("(verbose rows for that frame: NOTSA_SH_DUMP=%d NOTSA_SH_DUMP_SUBSYS=<category> NOTSA_SH_DUMP_FILE=<f> on both runs, then diff; field order = .notes/STATEHASH_SPEC.md)" % first[0])
        return 1
    if len(fa) != len(fb):
        print("identical over the common frames, LENGTH DIFFERS (a ends at %d, b ends at %d)" % (fa[-1] if fa else -1, fb[-1] if fb else -1))
        return 1
    print("identical (%d frames)" % len(fa))
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
