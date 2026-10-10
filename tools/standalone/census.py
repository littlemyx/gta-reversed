#!/usr/bin/env python3
"""Image census (slice A3 of .notes/DETACH_DATA_PLAN.md): which pages of the ORIGINAL data image (0x858000..0xCB0000, 1,112 pages) does the detached
standalone exe still touch, and which function touches them?

  census.py run    [--build build/StandaloneDetached] [--minutes 10] [--rearm-ms 15000] [--data /tmp/d3c] [--keep]
        one tools/standalone/soak.sh run (own data dir + Wine prefix, deleted afterwards) with NOTSA_VERIFY_GLOBALS (the list of converted globals),
        NOTSA_IMAGE_CENSUS=1 and NOTSA_IMAGE_CENSUS_REARM_MS (ImageDiag.cpp), then `report`. The build must be a detached + verify build (preset StandaloneDetached).
  census.py report --raw image_census.tsv --converted verify_dump.tsv [--map gta_reversed.map] [--out .notes/DETACH_census.tsv]

Every logged touch (address, IP, two return-address candidates) is classified by the address:
  STRAY_CONVERTED  inside a global that is already converted (NOTSA_VERIFY_GLOBALS dump): something still reads/writes the OLD address (a bug of the conversion)
  UNCLAIMED        in no DETACH_GLOBALS.tsv row, no converted global and no vtable: bytes of the image nobody declared (string/float constants, EH tables,
                   CRT/D3DX/RW tables, ...): they have to be accounted for before the exe can go
  PENDING          inside a not yet converted global of DETACH_GLOBALS.tsv (expected while Phase B/C is running)
  VTABLE           inside a vtable run of data_pointers.bin (+ the RTTI locator dword before it): explained, only counted
The report lists everything except VTABLE, one row per (class, owner or page, touching function, access). Touching function = the linker-map symbol that contains the IP.
"""
import argparse, bisect, os, re, shutil, struct, subprocess, sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
IMG_LO, IMG_HI = 0x858000, 0xCB0000


def load_map(path):
    pat = re.compile(r"^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8}) f")
    syms = []
    if not Path(path).exists():
        return [], []
    with open(path, errors="replace") as f:
        for line in f:
            m = pat.match(line)
            if m:
                syms.append((int(m.group(2), 16), m.group(1)))
    syms.sort()
    return [a for a, _ in syms], [n for _, n in syms]


def demangle(n):
    m = re.match(r"^\?\?([01])([^@]+)@@", n)
    if m:
        return ("~" if m.group(1) == "1" else "") + m.group(2)
    m = re.match(r"^\?([^@?]+)((?:@[^@]+)*)@@", n)
    if m:
        return "::".join(reversed([p for p in m.group(2).split("@") if p])) + ("::" if m.group(2) else "") + m.group(1)
    return n


class Sym:
    def __init__(self, map_path):
        self.addrs, self.names = load_map(map_path)

    def __call__(self, ip):
        if ip == 0:
            return ""
        i = bisect.bisect_right(self.addrs, ip) - 1
        if i < 0 or ip - self.addrs[i] > 0x20000:
            return "0x%08X" % ip
        return demangle(self.names[i])[:140] + ("+0x%X" % (ip - self.addrs[i]) if ip != self.addrs[i] else "")


def load_ranges_tsv(path, addr_col, size_col, name_col, skip_comment=True):
    out = []
    for line in open(path, errors="replace"):
        if line.startswith("#") or line.startswith("addr\t"):
            continue
        p = line.rstrip("\n").split("\t")
        try:
            out.append((int(p[addr_col], 16), int(p[size_col]), p[name_col]))
        except (ValueError, IndexError):
            pass
    return out


class Intervals:
    def __init__(self, items):
        self.items = sorted(items)
        self.starts = [a for a, _, _ in self.items]

    def find(self, addr):
        # items may overlap (aliases): scan back a few entries
        i = bisect.bisect_right(self.starts, addr) - 1
        for j in range(i, max(i - 6, -1), -1):
            a, s, n = self.items[j]
            if a <= addr < a + max(s, 1):
                return a, s, n
        return None


def vtable_runs(data_pointers_bin):
    runs, prev, start = [], None, None
    if not Path(data_pointers_bin).exists():
        return []
    d = open(data_pointers_bin, "rb").read()
    for i in range(0, len(d), 8):
        a, k = struct.unpack_from("<II", d, i)
        if k != 1:
            continue
        if prev is not None and a == prev + 4:
            prev = a
            continue
        if start is not None:
            runs.append((start - 4, prev + 4 - (start - 4), "vtable"))  # -4: the RTTI complete-object locator dword precedes the vtable
        start = prev = a
    if start is not None:
        runs.append((start - 4, prev + 4 - (start - 4), "vtable"))
    return runs


def report(a):
    sym = Sym(a.map)
    conv = Intervals(load_ranges_tsv(a.converted, 0, 1, 2)) if a.converted and Path(a.converted).exists() else Intervals([])
    pend = Intervals(load_ranges_tsv(a.globals, 0, 4, 6))
    vt = Intervals(vtable_runs(a.pointers))
    rows = defaultdict(lambda: {"n": 0, "addr": 0, "tick": 1 << 62, "ips": set()})
    pages_all, pages_by_class, total = set(), defaultdict(set), 0
    vt_events = 0
    for line in open(a.raw, errors="replace"):
        if line.startswith("#") or not line.strip():
            continue
        p = line.strip().split("\t")
        if len(p) < 8:
            continue
        page, addr, acc, tick, tid, ip, c1, c2 = int(p[0]), int(p[1], 16), p[2], int(p[3]), p[4], int(p[5], 16), int(p[6], 16), int(p[7], 16)
        total += 1
        pages_all.add(page)
        owner, cls = "", None
        if (r := conv.find(addr)):
            cls, owner = "STRAY_CONVERTED", "%s (0x%08X+%d)" % (r[2], r[0], addr - r[0])
        elif (r := pend.find(addr)):
            cls, owner = "PENDING", "%s (0x%08X+%d)" % (r[2], r[0], addr - r[0])
        elif (r := vt.find(addr)):
            cls = "VTABLE"
        else:
            cls, owner = "UNCLAIMED", "page %d" % page
        pages_by_class[cls].add(page)
        if cls == "VTABLE":
            vt_events += 1
            continue
        fn = sym(ip)
        key = (cls, owner.split(" (")[0] if cls != "UNCLAIMED" else owner, re.sub(r"\+0x[0-9A-F]+$", "", fn), acc)
        e = rows[key]
        e["n"] += 1
        if not e["addr"] or addr < e["addr"]:
            e["addr"] = addr
        e["tick"] = min(e["tick"], tick)
        e.setdefault("c1", set()).add(re.sub(r"\+0x[0-9A-F]+$", "", sym(c1)))
        e.setdefault("c2", set()).add(re.sub(r"\+0x[0-9A-F]+$", "", sym(c2)))
        e["ip"] = ip
        e["page"] = page
    order = {"STRAY_CONVERTED": 0, "UNCLAIMED": 1, "PENDING": 2}
    out = sorted(rows.items(), key=lambda kv: (order[kv[0][0]], kv[0][1], kv[0][2]))
    n_pages = (IMG_HI - IMG_LO) // 0x1000
    with open(a.out, "w") as f:
        f.write("# image census (tools/standalone/census.py): raw '%s', %d logged touches (distinct address+IP pairs; a page is logged once per re-arm round)\n" % (a.raw, total))
        f.write("# pages touched: %d of %d | by class: %s | VTABLE touches (explained, not listed): %d on %d pages\n" % (
            len(pages_all), n_pages, ", ".join("%s %d pages" % (c, len(pages_by_class[c])) for c in ("STRAY_CONVERTED", "UNCLAIMED", "PENDING", "VTABLE")), vt_events, len(pages_by_class["VTABLE"])))
        f.write("# class\towner\tfunction\tacc\tfirst_addr\tpage\ttouches\tfirst_tick\tcallers (return-address candidates)\n")
        for (cls, owner, fn, acc), e in out:
            callers = sorted(x for x in (e.get("c1", set()) | e.get("c2", set())) if x)
            f.write("%s\t%s\t%s\t%s\t0x%08X\t%d\t%d\t%d\t%s\n" % (cls, owner, fn, acc, e["addr"], e["page"], e["n"], e["tick"], "; ".join(callers[:4])))
    print("census: %d touches, %d/%d pages, %d rows -> %s" % (total, len(pages_all), n_pages, len(out), a.out))
    for c in ("STRAY_CONVERTED", "UNCLAIMED", "PENDING", "VTABLE"):
        print("  %-16s %4d pages" % (c, len(pages_by_class[c])))


def run(a):
    build = Path(a.build) if os.path.isabs(a.build) else REPO / a.build
    data, prefix = Path(a.data), Path(a.data + "-prefix")
    out = Path(a.data + "-out") / "census"
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, PREFIX=str(prefix), OUT=str(out),
               NOTSA_VERIFY_GLOBALS="verify_dump.tsv", NOTSA_IMAGE_CENSUS="1", NOTSA_IMAGE_CENSUS_REARM_MS=str(a.rearm_ms), NOTSA_IMAGE_CENSUS_FILE="image_census.tsv")
    if a.poison:
        env["NOTSA_POISON_CONVERTED"] = a.poison
    for f in ("image_census.tsv", "verify_dump.tsv"):
        (data / f).unlink(missing_ok=True)
    # bash reads a script while it runs: another agent committing soak.sh in the middle of a 10-minute run breaks it. Run a private snapshot next to it (REPO is derived from its location).
    snap = REPO / "tools/standalone" / (".census_soak_%d.sh" % os.getpid())
    shutil.copy(REPO / "tools/standalone/soak.sh", snap)
    try:
        rc = subprocess.call(["bash", str(snap), str(build), str(a.minutes), str(data)], env=env)
    finally:
        snap.unlink(missing_ok=True)
    print("census: soak exit %d" % rc)
    for f in ("image_census.tsv", "verify_dump.tsv"):
        if (data / f).exists():
            shutil.copy(data / f, out / f)
    ok = (out / "image_census.tsv").exists() and (out / "verify_dump.tsv").exists()
    if ok:
        ns = argparse.Namespace(raw=str(out / "image_census.tsv"), converted=str(out / "verify_dump.tsv"), map=str(build / "bin/gta_reversed.map"),
                                globals=a.globals, pointers=str(build / "bin/data_pointers.bin"), out=a.out)
        report(ns)
    if not a.keep:
        # the data dir only holds symlinks into the game install: unlink them, never recurse through them
        if data.exists():
            for e in data.iterdir():
                if e.is_symlink() or e.is_file():
                    e.unlink()
                else:
                    shutil.rmtree(e, ignore_errors=True)
            data.rmdir()
        shutil.rmtree(prefix, ignore_errors=True)
    print("census: raw files kept in %s (soak report.txt there)" % out)
    return 0 if ok else 2


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("--build", default="build/StandaloneDetached")
    r.add_argument("--minutes", type=int, default=10)
    r.add_argument("--rearm-ms", type=int, default=15000)
    r.add_argument("--data", default="/tmp/d3c")
    r.add_argument("--poison", default=None, help="also NOTSA_POISON_CONVERTED=<file> (usually not: the census should not change behaviour)")
    r.add_argument("--keep", action="store_true")
    r.add_argument("--globals", default=str(REPO / ".notes/DETACH_GLOBALS.tsv"))
    r.add_argument("--out", default=str(REPO / ".notes/DETACH_census.tsv"))
    p = sub.add_parser("report")
    p.add_argument("--raw", required=True)
    p.add_argument("--converted", required=True)
    p.add_argument("--map", default=str(REPO / "build/StandaloneDetached/bin/gta_reversed.map"))
    p.add_argument("--globals", default=str(REPO / ".notes/DETACH_GLOBALS.tsv"))
    p.add_argument("--pointers", default=str(REPO / "build/StandaloneDetached/bin/data_pointers.bin"))
    p.add_argument("--out", default=str(REPO / ".notes/DETACH_census.tsv"))
    a = ap.parse_args()
    sys.exit(run(a) if a.cmd == "run" else report(a) or 0)


if __name__ == "__main__":
    main()
