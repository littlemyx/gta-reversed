#!/usr/bin/env python3
"""RR-1: compare two state-hash traces (source/portable/statehash.cpp, format in .notes/RR_PLAN.md section 3).

  rr_diff.py A.trace B.trace [--skip N] [--max-report K] [--quiet]

Finds the FIRST frame where the traces differ and says in which subsystem(s); when both traces carry `D` detail lines for that frame (record with NOTSA_RR_DETAIL=<frame>)
it also names the first diverging entity and field, with the decoded values and the difference. Two levels:
  exact      floats hashed by bit pattern (a rounding-level difference is a divergence)
  quantised  floats rounded to 1/256 before hashing (only a behavioural difference is a divergence)
Exit code: 0 identical (all common frames, same length), 1 diverged / length mismatch, 2 unreadable trace.
"""
import struct
import sys

SUBSYS = ["tm", "ck", "rng", "pop", "pl", "cam", "pd", "vh", "ob"]  # order of the per-frame line = order of the fold into `all`
NAMES = {"tm": "timer", "ck": "game clock / weather", "rng": "RNG state", "pop": "population / traffic / streaming counters", "pl": "player",
         "cam": "camera", "pd": "peds", "vh": "vehicles", "ob": "objects"}


def parse(path):
    hdr, frames, detail = None, {}, {}
    try:
        with open(path) as f:
            for ln, line in enumerate(f, 1):
                line = line.rstrip("\n")
                if not line:
                    continue
                if line.startswith("#RR"):
                    hdr = line
                elif line.startswith("F "):
                    parts = line.split(" ")
                    fr = int(parts[1])
                    kv = {}
                    for p in parts[2:]:
                        k, _, v = p.partition("=")
                        kv[k] = v
                    frames[fr] = kv
                elif line.startswith("D "):
                    parts = line.split(" ")
                    fr = int(parts[1])
                    kind = parts[2]
                    if kind in ("ped", "veh", "obj"):
                        ident, rest = f"{kind}[{parts[3]}]", parts[4:]
                    else:
                        ident, rest = kind, parts[3:]
                    detail.setdefault(fr, {})
                    detail[fr][ident] = {p.partition("=")[0]: p.partition("=")[2] for p in rest}
                else:
                    raise ValueError(f"line {ln}: unrecognised: {line[:60]}")
    except (OSError, ValueError) as e:
        print(f"rr_diff: cannot read {path}: {e}", file=sys.stderr)
        sys.exit(2)
    if hdr is None or not frames:
        print(f"rr_diff: {path}: no header or no frames", file=sys.stderr)
        sys.exit(2)
    return hdr, frames, detail


def split_hash(v):
    """'N:EXACT/QUANT' | 'EXACT/QUANT' | 'EXACT' -> (count or None, exact, quant or None)"""
    cnt = None
    if ":" in v:
        c, _, v = v.partition(":")
        cnt = int(c)
    e, _, q = v.partition("/")
    return cnt, e, (q or None)


def diff_frame(a, b):
    """-> list of (subsystem, exact_differs, quant_differs, detail text)"""
    out = []
    for s in SUBSYS:
        va, vb = a.get(s), b.get(s)
        if va == vb:
            continue
        if va is None or vb is None:
            out.append((s, True, True, "missing in one trace"))
            continue
        ca, ea, qa = split_hash(va)
        cb, eb, qb = split_hash(vb)
        txt = ""
        if ca != cb:
            txt = f"count {ca} vs {cb}"
        out.append((s, ea != eb, (qa != qb) if qa is not None else (ea != eb), txt))
    return out


def fval(field, v):
    """field values: float fields are named f.* (hex bit pattern), the rest are hex ints"""
    n = int(v, 16)
    if field.startswith("f."):
        return struct.unpack("<f", struct.pack("<I", n))[0]
    return n


def detail_diff(da, db, subs, limit):
    """first diverging entities of the given subsystems (detail dict: ident -> {field: value})"""
    lines = []
    want = {"pd": "ped[", "vh": "veh[", "ob": "obj[", "pl": "player", "cam": "camera", "tm": "timer", "ck": "clock", "rng": "rng", "pop": "pop"}
    for s in subs:
        pre = want[s]
        for ident in sorted({k for k in list(da) + list(db) if k.startswith(pre)}, key=lambda k: (len(k), k)):
            ea, eb = da.get(ident), db.get(ident)
            if ea is None or eb is None:
                lines.append(f"    {ident}: present only in {'A' if eb is None else 'B'}")
            else:
                for f in ea:
                    if ea[f] != eb.get(f):
                        if eb.get(f) is None:
                            lines.append(f"    {ident}.{f}: missing in B")
                        else:
                            xa, xb = fval(f, ea[f]), fval(f, eb[f])
                            d = f"  (delta {xb - xa:+.9g})" if isinstance(xa, float) else ""
                            lines.append(f"    {ident}.{f}: A={xa!r} B={xb!r}{d}")
                        if "--first-field" in sys.argv:
                            break  # first field of this entity only (default: every differing field of the entity)
            if len(lines) >= limit:
                return lines
    return lines


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    opts = {a.split("=")[0]: (a.split("=", 1)[1] if "=" in a else True) for a in argv[1:] if a.startswith("--")}
    if len(args) != 2:
        print(__doc__)
        return 2
    quiet = "--quiet" in opts
    limit = int(opts.get("--max-report", 6))
    ha, fa, da = parse(args[0])
    hb, fb, db = parse(args[1])
    common = sorted(set(fa) & set(fb))
    first_exact = first_quant = None
    for fr in common:
        d = diff_frame(fa[fr], fb[fr])
        if d and first_exact is None:
            first_exact = (fr, d)
        if any(x[2] for x in d) and first_quant is None:
            first_quant = (fr, d)
        if first_exact and first_quant:
            break
    rc = 0
    if not quiet:
        print(f"A: {args[0]}: {len(fa)} frames  {ha}")
        print(f"B: {args[1]}: {len(fb)} frames  {hb}")
    if first_exact is None:
        if len(fa) != len(fb):
            print(f"IDENTICAL on {len(common)} common frames, but lengths differ: A has {len(fa)}, B has {len(fb)}")
            return 1
        print(f"IDENTICAL: {len(common)} frames")
        return 0
    rc = 1
    fr, d = first_exact
    print(f"FIRST DIVERGENCE (exact): frame {fr}  subsystems: " + ", ".join(f"{s} ({NAMES[s]}){' ' + t if t else ''}" for s, _, _, t in d))
    if first_quant is None:
        print("quantised hashes (floats rounded to 1/256) never diverge: rounding-level drift only")
    elif first_quant[0] != fr:
        print(f"first QUANTISED divergence (behavioural, not rounding): frame {first_quant[0]}  subsystems: " + ", ".join(s for s, _, q, _ in first_quant[1] if q))
    else:
        print("the quantised hashes diverge at the same frame: behavioural difference, not float rounding")
    # entity / field level
    for label, (frm, dd) in (("exact", first_exact), ("quantised", first_quant)) if first_quant and first_quant[0] != fr else (("exact", first_exact),):
        have = frm in da and frm in db
        if have:
            lines = detail_diff(da[frm], db[frm], [s for s, e, q, _ in dd if (e if label == "exact" else q)], limit)
            print(f"  detail at frame {frm} ({label}):" if lines else f"  detail at frame {frm}: present, no field differs (hash covers a field the detail omits?)")
            for l in lines:
                print(l)
        else:
            print(f"  no per-entity detail for frame {frm} in {'A' if frm not in da else ''}{' and ' if frm not in da and frm not in db else ''}{'B' if frm not in db else ''}: re-run both with NOTSA_RR_DETAIL={frm} (RR_DETAIL={frm} for tools/portable/rr_run.sh)")
    last_ok = max((f for f in common if f < fr), default=None)
    print(f"last identical frame: {last_ok}; differing frames after the first: " + str(sum(1 for f in common if f > fr and diff_frame(fa[f], fb[f]))) + f" of {len(common) - common.index(fr) - 1}")
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
