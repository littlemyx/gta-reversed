#!/usr/bin/env python3
"""initorder_link.py - relink the detached exe with a different OBJECT ORDER (no recompile) to prove that the static-initialisation end state does not depend on the link order.

The detached build keeps the exe's data as real C++ globals. Their dynamic initialisers (ctors) run in the order of the objects on the link line (MSVC puts each object's
.CRT$XCU slot in link order), so a hidden cross-TU dependency ("ctor of A reads / overwrites object B") would make the post-init state depend on how CMake happens to
list the sources. `.notes/reports/D-B20.md` documents the audited dependencies; this tool is the regression test for them:

    python -X utf8 -I tools/standalone/initorder_link.py --build build/StandaloneDetached --mode reverse --out C:/gta-scratch/b20/io_rev
    python -X utf8 -I tools/standalone/initorder_link.py --build build/StandaloneDetached --mode shuffle --seed 7 --out C:/gta-scratch/b20/io_s7

writes <out>/gta_reversed.exe (+ .map, + the original_data.* / data_pointers.bin the exe wants next to it). Then take the boot dump of that exe exactly like the normal
gate (`NOTSA_VERIFY_GLOBALS=<dump> NOTSA_VERIFY_GLOBALS_EXIT=1`, run it in a game directory) and check it with
    verify_globals.py <dump> --image-dir <out> --allow-rdata-ptr      (0 unexplained)
    verify_vptrs.py   <dump> --image-dir <out>                         (0 wrong)
`--compare REF_DUMP DUMP` additionally diffs two dumps byte for byte except code-pointer dwords (they legitimately move with the layout) and prints the differing globals.

Modes: reverse (every pair of objects flips: any A-before-B dependency of the normal order is violated), shuffle (seeded random), rotate (N objects moved to the end),
none (same order as CMake, control run: must give a dump identical to the normal build). `orig_image_pad.obj` (the data-image placeholder) always stays first.
Needs the MSVC x86 environment (source tools/standalone/windows/msvcenv.sh) and an up to date build dir (all objects built, bin/gta_reversed.exe linked once).
"""
import argparse
import random
import re
import shutil
import subprocess
import sys
from pathlib import Path


def parse_link(build: Path):
    text = (build / "build.ninja").read_text(encoding="utf8", errors="replace")
    lines = text.split("\n")
    start = next(i for i, l in enumerate(lines) if re.match(r"build bin[\\/]gta_reversed\.exe:", l))
    head = lines[start].replace("$:", ":")
    ins = head.split(":", 1)[1]          # after "build bin\gta_reversed.exe:" -> "RULE in1 in2 | implicit || order"
    ins = ins.split(" | ")[0].split(" || ")[0].split()[1:]   # drop the rule name
    objs = [x for x in ins if x.endswith(".obj")]
    libs_in = [x for x in ins if x.endswith(".lib")]
    var = {}
    for l in lines[start + 1:start + 30]:
        if not l.startswith("  "):
            break
        k, _, v = l.strip().partition(" = ")
        var[k] = v.replace("$:", ":")
    return objs, libs_in, var


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default="build/StandaloneDetached")
    ap.add_argument("--mode", choices=["reverse", "shuffle", "rotate", "none"], default="reverse")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--rotate", type=int, default=100, help="rotate: move the first N objects (after the pad) to the end")
    ap.add_argument("--out")
    ap.add_argument("--debug", action="store_true", help="keep /DEBUG (pdb); default off (faster)")
    ap.add_argument("--compare", nargs=2, metavar=("REF_DUMP", "DUMP"))
    a = ap.parse_args()

    if a.compare:
        return compare(*a.compare)

    if not a.out:
        ap.error("--out is required")
    build = Path(a.build).resolve()
    out = Path(a.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    objs, _libs_in, var = parse_link(build)
    pad = [o for o in objs if o.endswith("orig_image_pad.obj")]
    rest = [o for o in objs if o not in pad]
    if a.mode == "reverse":
        rest.reverse()
    elif a.mode == "shuffle":
        random.Random(a.seed).shuffle(rest)
    elif a.mode == "rotate":
        rest = rest[a.rotate:] + rest[:a.rotate]
    order = pad + rest
    libs = var["LINK_LIBRARIES"]
    rsp = out / "link.rsp"
    rsp.write_text("\n".join(order) + "\n" + libs + "\n", encoding="utf8")
    flags = ["/INCREMENTAL:NO", "/subsystem:windows", "/MAP:" + str(out / "gta_reversed.map"), "/OPT:NOREF", "/OPT:NOICF",
             "/BASE:0x400000", "/DYNAMICBASE:NO", "/FIXED", "/MERGE:_TEXT=.text", "-machine:X86", "/version:0.0"]
    flags.append("/DEBUG" if a.debug else "/DEBUG:NONE")
    if a.debug:
        flags.append("/pdb:" + str(out / "gta_reversed.pdb"))
    cmd = ["link.exe", "/nologo", "@" + str(rsp), "/out:" + str(out / "gta_reversed.exe"), "/implib:" + str(out / "gta_reversed.lib")] + flags
    print("objects: %d (%s), first after pad: %s" % (len(order), a.mode, rest[0]))
    r = subprocess.run(cmd, cwd=str(build))
    if r.returncode != 0:
        return r.returncode
    for f in ("original_data.bin", "original_data.json", "data_pointers.bin"):
        shutil.copy2(build / "bin" / f, out / f)
    print("linked", out / "gta_reversed.exe")
    return 0


def load(path):
    d = {}
    for l in Path(path).read_text().splitlines():
        if not l or l.startswith("#"):
            continue
        f = l.split("\t")
        if len(f) == 5:
            d[int(f[0], 16)] = (int(f[1]), f[2], bytes.fromhex(f[4]))
    return d


def compare(ref, dump):
    A, B = load(ref), load(dump)
    only_a = sorted(set(A) - set(B))
    only_b = sorted(set(B) - set(A))
    bad = 0
    masked = [0]
    for addr in sorted(set(A) & set(B)):
        (sa, na, ba), (sb, nb, bb) = A[addr], B[addr]
        if sa != sb:
            print("SIZE  0x%X %s %d vs %d" % (addr, na, sa, sb)); bad += 1; continue
        if ba == bb:
            continue
        diff = []
        for o in range(0, sa - 3, 4):
            wa, wb = ba[o:o + 4], bb[o:o + 4]
            if wa != wb:
                va, vb = int.from_bytes(wa, "little"), int.from_bytes(wb, "little")
                # pointers into the exe image (code, vtables, other globals: our objects live in the linked sections, which move with the object order) are
                # layout-dependent by definition; everything else must be bit-identical
                if 0x400000 <= va < 0x8000000 and 0x400000 <= vb < 0x8000000:
                    masked[0] += 1
                    continue
                diff.append(o)
        if any(ba[o:o + 1] != bb[o:o + 1] for o in range(sa - (sa % 4), sa)):
            diff.append(sa - (sa % 4))
        if diff:
            print("DIFF  0x%X %s size %d first offsets %s" % (addr, na, sa, [hex(o) for o in diff[:6]])); bad += 1
    print("compare: %d globals ref, %d dump, %d only in ref, %d only in dump, %d differing (%d pointer-like dwords masked)" % (len(A), len(B), len(only_a), len(only_b), bad, masked[0]))
    return 1 if bad or only_a or only_b else 0


if __name__ == "__main__":
    sys.exit(main())
