#!/usr/bin/env python3
"""verify_globals.py - compare the detached globals of a standalone run with the original exe's data image (slice A2 of .notes/DETACH_DATA_PLAN.md, 3.3 step 6).

The exe (built with -DGTASA_DETACHED_GLOBALS=ON -DGTASA_VERIFY_GLOBALS=ON) writes, right before the game starts (source/standalone/GlobalsVerify.cpp):
    NOTSA_VERIFY_GLOBALS=<dump> [NOTSA_VERIFY_GLOBALS_EXIT=1] gta_reversed.exe
one line per converted global:   addr \t size \t name \t file:line \t <our bytes, hex>
and this script compares "our bytes" with  original_data.bin  (the post-_initterm image the build extracted from the user's exe).

Rules per differing 32-bit word (everything else must be bit-identical):
  * code pointer: the image word is a code address (in [code_lo, code_hi)) and our word is the address of a function that the linker map (--map, default
    <image-dir>/gta_reversed.map) names exactly like the repo's own hook for that original address (`RH_Scoped*Install(Fn, 0xADDR)` under `RH_ScopedClass(Cls)`, or a
    `0xADDR, Cls::Fn` table: the same resolver the emitter uses, globals_emit.FnResolver): `Cls::Fn` <-> `?Fn@Cls@@...`. An unported function has no such name and
    shows up as a mismatch. Counted as "code ptr ok".
  * `--allow-rdata-ptr`: the image word points into .rdata (vtable / string table pointers of objects): masked, counted separately (phase C checks them by class).
  * documented deviations: --deviations FILE (default tools/standalone/standalone_deviations.tsv): lines `addr<TAB>offset<TAB>length<TAB>reason`
    (length 0 = the whole global), e.g. rand()-derived bytes.
Cross checks: every dumped address must be a row of .notes/DETACH_GLOBALS.tsv with the same size; `--cover SUBSTR[,SUBSTR]` lists the rows owned by files
matching a substring that are NOT in the dump (not converted yet / skipped by the codemod: reasons are in the codemod's table).
Exit code 0 only with 0 unexplained mismatches and 0 cross-check failures.

usage: python3 -I tools/standalone/verify_globals.py DUMP [--image-dir build/StandaloneRelease/bin] [--tsv ..] [--deviations ..] [--allow-rdata-ptr] [--cover Timer.h,Hud.h]
"""
import argparse
import json
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def load_dump(path):
    out = []
    for n, line in enumerate(Path(path).read_text().splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        f = line.split("\t")
        if len(f) != 5:
            raise SystemExit("%s:%d: expected 5 tab separated fields, got %d" % (path, n, len(f)))
        out.append(dict(addr=int(f[0], 16), size=int(f[1]), name=f[2], loc=f[3], ours=bytes.fromhex(f[4])))
    return out


def load_deviations(path):
    dev = {}
    p = Path(path)
    if not p.exists():
        return dev
    for line in p.read_text().splitlines():
        if not line.strip() or line.startswith("#") or line.startswith("addr"):
            continue
        f = line.split("\t")
        dev.setdefault(int(f[0], 16), []).append((int(f[1]), int(f[2]), f[3] if len(f) > 3 else ""))
    return dev


_resolver = None
_fnames = {}


def load_map_names(map_path, words):
    """address -> mangled symbol for the given addresses (one pass over the linker map; function symbols only)"""
    import re
    want = {"%08x" % w for w in words}
    if not want or not Path(map_path).exists():
        return {}
    pat = re.compile(r"^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8}) f")
    out = {}
    with open(map_path, errors="replace") as f:
        for line in f:
            m = pat.match(line)
            if m and m.group(2) in want:
                out.setdefault(int(m.group(2), 16), m.group(1))
    return out


def expected_prefix(orig_addr):
    """`?Fn@Cls@@` for the function the repo hooks at orig_addr (None if the repo names none)"""
    global _resolver
    if _resolver is None:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import globals_emit
        _resolver = globals_emit.FnResolver()
    r, _ = _resolver.resolve(orig_addr)
    if not r:
        return None
    parts = r.lstrip("&").split("::")
    return "?" + "@".join(reversed(parts)) + "@@"


def code_ptr_ok(orig_word, our_word):
    pre = expected_prefix(orig_word)
    name = _fnames.get(our_word)
    return bool(pre and name and name.startswith(pre))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dump")
    ap.add_argument("--image-dir", default=str(REPO / "build" / "StandaloneRelease" / "bin"))
    ap.add_argument("--tsv", default=str(REPO / ".notes" / "DETACH_GLOBALS.tsv"))
    ap.add_argument("--deviations", default=str(REPO / "tools" / "standalone" / "standalone_deviations.tsv"))
    ap.add_argument("--map", default=None, help="linker map of the exe (default <image-dir>/gta_reversed.map)")
    ap.add_argument("--allow-rdata-ptr", action="store_true")
    ap.add_argument("--cover", default="")
    ap.add_argument("-v", "--verbose", action="store_true")
    a = ap.parse_args()

    meta = json.loads((Path(a.image_dir) / "original_data.json").read_text())
    img = (Path(a.image_dir) / "original_data.bin").read_bytes()
    base, end = meta["data_base"], meta["data_end"]
    code_lo, code_hi, rd_lo, rd_hi = meta["code_lo"], meta["code_hi"], meta["rdata_lo"], meta["rdata_hi"]
    rows = {}
    tsvp = Path(a.tsv)
    if tsvp.exists():
        lines = tsvp.read_text().splitlines()
        hdr = lines[0].split("\t")
        for l in lines[1:]:
            f = l.split("\t")
            rows[int(f[0], 16)] = dict(zip(hdr, f))
    raw_casts = set()
    rc = REPO / ".notes" / "DETACH_raw_casts.tsv"      # raw `(T*)0xADDR` data casts: not rows of the table, but legitimate converted globals
    if rc.exists():
        for l in rc.read_text().splitlines()[1:]:
            f = l.split("\t")
            if len(f) > 3 and f[3] == "data" and f[2].startswith("0x"):
                raw_casts.add(int(f[2], 16))
    # .notes/aliases.json decisions: `new_globals` (addresses that are not rows of the table: re-addressed sites, lazy-init variables, tables found by the A4 review)
    # are legitimate converted globals with their own size; `extents` (grow/extend/shrink) give the true byte size of a row whose declaration is under-/over-sized.
    new_globals, extent_size = {}, {}
    ap = REPO / ".notes" / "aliases.json"
    if ap.exists():
        aj = json.loads(ap.read_text())
        for n in aj.get("new_globals", []):
            new_globals[int(n["addr"], 16)] = int(n["size"])
        for k, v in aj.get("extents", {}).items():
            if v.get("kind") in ("grow", "extend", "shrink") and "true_size" in v:
                extent_size[int(k, 16)] = int(v["true_size"])
    dev = load_deviations(a.deviations)
    dump = load_dump(a.dump)
    # our words that are candidates for code pointers: named through the linker map in one pass
    cand = set()
    for g in dump:
        if g["addr"] % 4 == 0 and len(g["ours"]) == g["size"] and g["addr"] >= base and g["addr"] + g["size"] <= end:
            o = img[g["addr"] - base:g["addr"] - base + g["size"]]
            for off in range(0, g["size"] - g["size"] % 4, 4):
                ow = struct.unpack_from("<I", o, off)[0]
                nw = struct.unpack_from("<I", g["ours"], off)[0]
                if nw != ow and code_lo <= ow < code_hi:
                    cand.add(nw)
    _fnames.update(load_map_names(a.map or str(Path(a.image_dir) / "gta_reversed.map"), cand))

    tot_bytes = ident = codeok = masked = devs = 0
    bad, xfail = [], []
    seen = set()
    for g in dump:
        addr, size = g["addr"], g["size"]
        if addr in seen:
            xfail.append("%s 0x%X: dumped twice" % (g["name"], addr))
        seen.add(addr)
        if len(g["ours"]) != size:
            xfail.append("%s 0x%X: dump line has %d/%d bytes for size %d" % (g["name"], addr, len(g["ours"]), len(g["ours"]), size))
            continue
        row = rows.get(addr)
        if rows and row is None and addr in raw_casts:
            pass
        elif rows and row is None and addr in new_globals:
            if new_globals[addr] != size:
                xfail.append("%s 0x%X: sizeof %d != aliases.json new_globals size %d" % (g["name"], addr, size, new_globals[addr]))
        elif rows and row is None:
            xfail.append("%s 0x%X: address is not a row of the table (a base+offset or unlisted address?)" % (g["name"], addr))
        elif row is not None and addr in extent_size and extent_size[addr] == size:
            pass     # aliases.json extents: the declared type was wrong, the decided true size is what the global has now
        elif row is not None and int(row["size"]) != size:
            xfail.append("%s 0x%X: sizeof %d != table size %s" % (g["name"], addr, size, row["size"]))
        if not (base <= addr and addr + size <= end):
            xfail.append("%s 0x%X: outside the data image" % (g["name"], addr))
            continue
        orig = img[addr - base:addr - base + size]
        tot_bytes += size
        ours = g["ours"]
        if ours == orig:
            ident += 1
            continue
        diffs = []     # byte offsets that remain unexplained
        explained_dwords = set()
        for off in range(0, size - size % 4, 4):
            if addr % 4 or ours[off:off + 4] == orig[off:off + 4]:
                continue
            ow, nw = (struct.unpack_from("<I", x, off)[0] for x in (orig, ours))
            if code_lo <= ow < code_hi and code_ptr_ok(ow, nw):
                codeok += 1
                explained_dwords.add(off)
            elif a.allow_rdata_ptr and rd_lo <= ow < rd_hi:
                masked += 1
                explained_dwords.add(off)
        for off in range(size):
            if ours[off] == orig[off] or (off - off % 4) in explained_dwords:
                continue
            if any((l == 0 or o <= off < o + l) for o, l, _ in dev.get(addr, [])):
                devs += 1
                continue
            diffs.append(off)
        if diffs:
            bad.append((g, diffs, orig))
    print("%d globals, %d bytes: %d identical, %d code-pointer dwords naming the repo's replacement function, %d masked (rdata ptr), %d deviation bytes, %d globals with unexplained differences"
          % (len(dump), tot_bytes, ident, codeok, masked, devs, len(bad)))
    for g, diffs, orig in bad:
        print("MISMATCH %s 0x%X (%s) size %d: %d bytes differ, first at +0x%X: ours %s image %s" % (
            g["name"], g["addr"], g["loc"], g["size"], len(diffs), diffs[0], g["ours"][diffs[0]:diffs[0] + 8].hex(), orig[diffs[0]:diffs[0] + 8].hex()))
    for x in xfail:
        print("CROSSCHECK", x)
    if a.cover:
        subs = [s for s in a.cover.split(",") if s]
        miss = [r for ad, r in rows.items() if ad not in seen and any(s in r["owner"] for s in subs)]
        print("cover: %d rows owned by %s, %d in the dump, %d not converted:" % (
            sum(1 for r in rows.values() if any(s in r["owner"] for s in subs)), "/".join(subs), sum(1 for ad in seen if ad in rows and any(s in rows[ad]["owner"] for s in subs)), len(miss)))
        for r in miss:
            print("   not converted: %s %s cat %s (%s B)" % (r["addr"], r["name"], r["cat"], r["size"]))
    ok = not bad and not xfail
    print("RESULT: %s (%d mismatching globals, %d cross-check failures)" % ("PASS" if ok else "FAIL", len(bad), len(xfail)))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
