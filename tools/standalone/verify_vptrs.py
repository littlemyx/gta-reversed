#!/usr/bin/env python3
"""verify_vptrs.py - phase-C companion of verify_globals.py: checks the vtable pointer dwords that `--allow-rdata-ptr` only masks.

For every converted global of a NOTSA_VERIFY_GLOBALS dump and every aligned dword where our word differs from the pristine original_data.bin and the image word is
the address of a vtable the repo registers (`RH_ScopedVirtualClass(Cls, 0xVTABLE, n)`), our word must be the address of the compiler's primary vtable of THE SAME class
(`??_7Cls@@6B@` in the linker map). A dword that points somewhere else in .rdata (string tables, secondary bases of multiple inheritance) is listed as OTHER, not as a failure.
Also checks (--groups) the self / allocator pointer dwords documented in standalone_deviations.tsv for CPedGroups::ms_groups (C1) against the map.

usage: python3 -I tools/standalone/verify_vptrs.py DUMP [--image-dir build/StandaloneDetached/bin] [--groups]
Exit code 0 = every registered vptr points at its own class's vtable.
"""
import argparse
import json
import os
import re
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def registered_vtables():
    """exe vtable VA -> class name, from the sources (RH_ScopedVirtualClass(Cls, 0xVA, n))"""
    out = {}
    rx = re.compile(r"RH_ScopedVirtualClass\(\s*([\w:]+)\s*,\s*(0x[0-9A-Fa-f]+)")
    for root, _, fs in os.walk(REPO / "source"):
        for f in fs:
            if f.endswith((".h", ".cpp", ".inc")):
                t = (Path(root) / f).read_text(encoding="utf-8", errors="replace")
                for m in rx.finditer(t):
                    out.setdefault(int(m.group(2), 16), m.group(1))
    return out


def map_symbols(path):
    """primary vtables ??_7Cls@@6B@ -> VA, plus every `?name@Cls@@` data symbol we may need -> VA (Rva+Base column)"""
    vt, var = {}, {}
    for line in Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
        p = line.split()
        if len(p) < 3 or not p[0].startswith("000"):
            continue
        n = p[1]
        if n.startswith("??_7") and n.endswith("@@6B@"):
            vt.setdefault(n[4:-5], int(p[2], 16))
        elif n.startswith("?ms_groups@CPedGroups@@") or n.startswith("?ms_StandStillAllocator@CPedGroupDefaultTaskAllocators@@"):
            if "_gReg" not in n:
                var[n.split("@")[0][1:]] = int(p[2], 16)
    return vt, var


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dump")
    ap.add_argument("--image-dir", default=str(REPO / "build/StandaloneDetached/bin"))
    ap.add_argument("--groups", action="store_true")
    a = ap.parse_args()
    d = Path(a.image_dir)
    meta = json.loads((d / "original_data.json").read_text())
    base = meta["data_base"]
    img = (d / "original_data.bin").read_bytes()
    reg = registered_vtables()
    vt, var = map_symbols(d / "gta_reversed.map")
    rows = []
    for line in Path(a.dump).read_text(encoding="utf-8", errors="replace").splitlines():
        f = line.split("\t")
        if line.startswith("#") or len(f) != 5:
            continue
        rows.append((int(f[0], 16), int(f[1]), f[2], bytes.fromhex(f[4])))
    ok = True
    checked = other = 0
    bad = []
    byclass = {}
    for addr, size, name, ours in rows:
        orig = img[addr - base:addr - base + size]
        if ours == orig:
            continue
        for off in range(0, size - size % 4, 4):
            if addr % 4 or ours[off:off + 4] == orig[off:off + 4]:
                continue
            ow, nw = struct.unpack_from("<I", orig, off)[0], struct.unpack_from("<I", ours, off)[0]
            cls = reg.get(ow)
            if cls is None:
                other += 1
                continue
            want = vt.get("@".join(reversed(cls.split("::"))))
            checked += 1
            byclass[cls] = byclass.get(cls, 0) + 1
            if want is None or want != nw:
                bad.append("%s 0x%X+%d: class %s exe vtable 0x%X, ours 0x%X, map primary vtable %s" % (name, addr, off, cls, ow, nw, hex(want) if want else "NOT FOUND"))
    print("vptr dwords checked: %d (%d classes), %d other differing rdata/data dwords not classified, %d wrong" % (checked, len(byclass), other, len(bad)))
    for b in bad[:40]:
        print("FAIL", b)
    ok = not bad
    if a.groups:
        g, sa = var.get("ms_groups"), var.get("ms_StandStillAllocator")
        wrong = 0
        by = {r[0]: r for r in rows}
        if 0xC09920 in by and g and sa:
            ours = by[0xC09920][3]
            for i in range(8):
                for off, want in ((8, g + i * 724), (48, g + i * 724), (700, sa)):
                    if struct.unpack_from("<I", ours, i * 724 + off)[0] != want:
                        wrong += 1
            print("ms_groups self/allocator pointer dwords: 24 checked, %d wrong (ms_groups 0x%X, ms_StandStillAllocator 0x%X)" % (wrong, g, sa))
            ok = ok and wrong == 0
        else:
            print("ms_groups not in the dump or not in the map: skipped")
    print("RESULT: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
