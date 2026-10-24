#!/usr/bin/env python3
"""Host test of rr_diff.py: a valid case (identical traces) and mutations of one synthetic trace; every mutation must be reported at the right frame / subsystem / entity / field.
Run: python3 tools/portable/rr_diff_test.py   (exit 0 = all checks passed)"""
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "rr_diff.py")


def fbits(x):
    return "%08x" % struct.unpack("<I", struct.pack("<f", x))[0]


def frame_line(n, over=None):
    d = dict(gf=str(100 + n), rng="%08x" % (0x1234567 + n), tm="%016x" % (0xA0 + n), ck="%016x" % 0xC0, pop="%016x" % (0xD0 + n % 3),
             pl="%016x/%016x" % (0xE000 + n, 0xE100 + n), cam="%016x/%016x" % (0xF000 + n, 0xF100 + n),
             pd="3:%016x/%016x" % (0x1000 + n, 0x1100 + n), vh="2:%016x/%016x" % (0x2000 + n, 0x2100 + n), ob="5:%016x/%016x" % (0x3000, 0x3100),
             all="%016x/%016x" % (0x9000 + n, 0x9100 + n))
    d.update(over or {})
    return f"F {n} " + " ".join(f"{k}={v}" for k, v in d.items())


def detail_lines(n, ped1_x=1.0):
    return [f"D {n} ped 0 slot=0 model=7 f.px={fbits(2.0)} f.hp={fbits(100.0)}",
            f"D {n} ped 1 slot=1 model=9 f.px={fbits(ped1_x)} f.hp={fbits(100.0)}",
            f"D {n} veh 0 slot=0 model=400 f.px={fbits(5.0)}"]


def write(path, lines, hdr="#RR 1 build=test seed=1 step_ms=20 float=portable"):
    with open(path, "w") as f:
        f.write(hdr + "\n" + "\n".join(lines) + "\n")


def run(a, b, *opts):
    p = subprocess.run([sys.executable, TOOL, a, b, *opts], capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


fails = []


def check(name, cond, info=""):
    print(("ok   " if cond else "FAIL ") + name)
    if not cond:
        fails.append(name)
        print("     " + info.replace("\n", "\n     "))


with tempfile.TemporaryDirectory() as tmp:
    P = lambda n: os.path.join(tmp, n)
    base = [frame_line(n) for n in range(20)]
    write(P("a"), base)
    write(P("b"), base)
    rc, out = run(P("a"), P("b"))
    check("identical traces -> exit 0, IDENTICAL", rc == 0 and "IDENTICAL: 20 frames" in out, out)

    # mutation 1: a ped hash differs from frame 7 on (exact and quantised) => frame 7, pd, behavioural
    m = [frame_line(n, {"pd": "3:%016x/%016x" % (0xBAD, 0xBAD)} if n >= 7 else None) for n in range(20)]
    write(P("m1"), m)
    rc, out = run(P("a"), P("m1"))
    check("pd mutated from frame 7 -> frame 7, pd", rc == 1 and "frame 7 " in out and "pd (peds)" in out and "quantised hashes diverge at the same frame" in out, out)
    check("  ... and the last identical frame is 6", "last identical frame: 6" in out, out)

    # mutation 2: only the exact half of pl differs at frame 3, quantised half equal => rounding drift only
    m = [frame_line(n, {"pl": "%016x/%016x" % (0xDEAD, 0xE100 + n)} if n >= 3 else None) for n in range(20)]
    write(P("m2"), m)
    rc, out = run(P("a"), P("m2"))
    check("exact-only pl difference -> 'rounding-level drift only'", rc == 1 and "frame 3 " in out and "pl (player)" in out and "rounding-level drift only" in out, out)

    # mutation 3: exact differs at frame 2, behavioural (quantised) only at frame 9
    m = []
    for n in range(20):
        over = {}
        if n >= 2: over["cam"] = "%016x/%016x" % (0xCAFE, 0xF100 + n)
        if n >= 9: over["vh"] = "2:%016x/%016x" % (0x77, 0x88)
        m.append(frame_line(n, over))
    write(P("m3"), m)
    rc, out = run(P("a"), P("m3"))
    check("exact at 2, behavioural at 9 -> both reported", rc == 1 and "frame 2 " in out and "first QUANTISED divergence (behavioural, not rounding): frame 9" in out and "vh" in out, out)

    # mutation 4: RNG state differs at frame 11 only
    m = [frame_line(n, {"rng": "deadbeef"} if n == 11 else None) for n in range(20)]
    write(P("m4"), m)
    rc, out = run(P("a"), P("m4"))
    check("RNG only, one frame -> frame 11, rng", rc == 1 and "frame 11 " in out and "rng (RNG state)" in out, out)

    # mutation 5: shorter trace
    write(P("m5"), base[:15])
    rc, out = run(P("a"), P("m5"))
    check("shorter trace -> exit 1 and length reported", rc == 1 and "lengths differ: A has 20, B has 15" in out, out)

    # mutation 6: count of peds differs
    m = [frame_line(n, {"pd": "4:%016x/%016x" % (0x1000 + n, 0x1100 + n)} if n >= 5 else None) for n in range(20)]
    write(P("m6"), m)
    rc, out = run(P("a"), P("m6"))
    check("ped count differs -> reported with counts", rc == 1 and "frame 5 " in out and "count 3 vs 4" in out, out)

    # detail: the entity and the field are named with decoded values
    A = []
    B = []
    for n in range(10):
        A.append(frame_line(n)); B.append(frame_line(n, {"pd": "3:%016x/%016x" % (0x42, 0x43), "all": "1/2"} if n == 4 else None))
        if n == 4:
            A += detail_lines(4, 1.0)
            B += detail_lines(4, 1.0 + 2 ** -20)  # one-ulp-ish difference on ped 1's x
    write(P("da"), A)
    write(P("db"), B)
    rc, out = run(P("da"), P("db"))
    check("detail: names ped[1].f.px with both values and the delta", rc == 1 and "ped[1].f.px" in out and "A=1.0" in out and "delta +9.5367" in out, out)
    check("detail: ped[0] (equal) is not reported", "ped[0]" not in out, out)

    # no detail in the traces -> the hint with the re-run command
    rc, out = run(P("a"), P("m1"))
    check("no detail -> hint with RR_DETAIL=7", "RR_DETAIL=7" in out, out)

    # unreadable
    with open(P("junk"), "w") as f:
        f.write("this is not a trace\n")
    rc, out = run(P("a"), P("junk"))
    check("junk file -> exit 2", rc == 2, out)

print("\n%d failed" % len(fails) if fails else "\nall passed")
sys.exit(1 if fails else 0)
