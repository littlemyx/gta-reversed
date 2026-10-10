#!/usr/bin/env python3
"""globals_emit.py - step 4 of .notes/DETACH_DATA_PLAN.md 3.3, the literal emitter (slice A2). Also the library of codemod_globals.py.

Given a row of .notes/DETACH_GLOBALS.tsv (written by gen_globals.py) and the post-_initterm data image of the exe it produces the C++ initialiser of the
global as source text. Nothing here is run by the build; the output is reviewed and committed like any other source.

Type model: the compiler's own spelling of the type (`canonical_type` column, from the probe) is parsed into scalars / enums / pointers / C arrays /
std::array; everything else (aggregate classes, unions, ...) is `opaque` and is NOT emitted (the codemod reports it for manual treatment; `--bytes` of the
CLI prints the raw bytes for it).

Literal rules (all bit exact):
  float/double    shortest decimal that round-trips (`1.5f`, `0.0f`); -0.0, NaN, inf, denormals via `std::bit_cast<float>(0x...u)`
  int/uint        decimal; unsigned >= 0x10000 in hex (`0xDE4B237Du`); int32 hashes with the top bit set as `(int32)0xDE4B237Du`
  bool            `false` / `true` (any other byte value: `std::bit_cast<bool>((uint8)N)`)
  enum            `static_cast<E>(N)`
  pointer         0 -> `nullptr`; code pointer -> `&Qualified::Function` (resolved from the RH_Scoped*Install lines and `0xADDR, Qual::Fn` tables of the
                  sources); string pointer / data pointer -> `nullptr /*TODO(0xV)*/` and the global is reported as NEEDS-MANUAL (never silently wrong)
  array           `{}` when all zero, else `{ e0, e1, ... }` (trailing zero elements dropped), nested for multi-dimensional, `char` text as "string"
Usage:  python3 -I tools/standalone/globals_emit.py show <0xADDR>... [--image-dir build/StandaloneRelease/bin]
        python3 -I tools/standalone/globals_emit.py report <file-substring> [--image-dir ...]     (all rows whose owner file contains the substring)
"""
import argparse
import csv
import json
import math
import re
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))

# ---------------------------------------------------------------------------------------------------------------------------------
# TSV + image
# ---------------------------------------------------------------------------------------------------------------------------------


def load_rows(tsv=None):
    tsv = Path(tsv or REPO / ".notes" / "DETACH_GLOBALS.tsv")
    with open(tsv, newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    for r in rows:
        r["a"] = int(r["addr"], 16)
        r["sz"] = int(r["size"])
    return rows


class Img:
    """post-_initterm image (original_data.bin/json of the build) + the extractor's code-pointer classification (data_pointers.txt)"""

    def __init__(self, image_dir=None):
        d = Path(image_dir or REPO / "build" / "StandaloneRelease" / "bin")
        meta = json.loads((d / "original_data.json").read_text())
        self.base = meta["data_base"]
        self.end = meta["data_end"]
        self.code_lo, self.code_hi = meta["code_lo"], meta["code_hi"]
        self.data = (d / "original_data.bin").read_bytes()
        self.secs = meta["sections"]
        self.codeptr = {}
        dp = d.parent / "standalone_data" / "data_pointers.txt"
        if not dp.exists():
            dp = d / "data_pointers.txt"
        if dp.exists():
            for line in dp.read_text().splitlines():
                p = line.split()
                if len(p) >= 3 and p[2] in ("V", "C"):
                    self.codeptr[int(p[0], 16)] = int(p[1], 16)

    def bytes(self, a, n):
        if not (self.base <= a and a + n <= self.end):
            raise ValueError("0x%X+%d outside the image" % (a, n))
        return self.data[a - self.base:a - self.base + n]

    def is_code_ptr(self, a, v):
        return a in self.codeptr and self.codeptr[a] == v or (self.code_lo <= v < self.code_hi and v % 1 == 0 and a in self.codeptr)


# ---------------------------------------------------------------------------------------------------------------------------------
# type model
# ---------------------------------------------------------------------------------------------------------------------------------

SCALARS = {
    "bool": ("bool", 1, False), "char": ("char", 1, True), "signed char": ("int8", 1, True), "unsigned char": ("uint8", 1, False),
    "short": ("int16", 2, True), "unsigned short": ("uint16", 2, False), "int": ("int32", 4, True), "unsigned int": ("uint32", 4, False),
    "long": ("int32", 4, True), "unsigned long": ("uint32", 4, False), "__int64": ("int64", 8, True), "unsigned __int64": ("uint64", 8, False),
    "float": ("float", 4, True), "double": ("double", 8, True), "wchar_t": ("wchar_t", 2, False), "char16_t": ("char16_t", 2, False),
}


class T:
    def __init__(self, kind, size, **kw):
        self.kind, self.size = kind, size   # kind: scalar | enum | ptr | carr | stdarr | opaque
        self.__dict__.update(kw)

    def __repr__(self):
        return "T(%s,%d)" % (self.kind, self.size)


def split_top(s, sep=","):
    out, depth, cur = [], 0, ""
    for c in s:
        if c in "<([":
            depth += 1
        elif c in ">)]":
            depth -= 1
        if c == sep and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += c
    out.append(cur)
    return [x.strip() for x in out]


TYPEDEFS = {"uint8": "unsigned char", "int8": "signed char", "uint16": "unsigned short", "int16": "short", "uint32": "unsigned int", "int32": "int",
            "uint64": "unsigned __int64", "int64": "__int64", "RwInt32": "int", "RwUInt32": "unsigned int"}


def parse_type(canon, total):
    """canonical (cl) spelling (or the repo's own simple spelling: uint32, std::array<int32,4>, ...) + total byte size -> T"""
    canon = re.sub(r"\b(%s)\b" % "|".join(TYPEDEFS), lambda m: TYPEDEFS[m.group(1)], canon)
    c = re.sub(r"\b(class|struct|union)\s+", "", canon.strip())
    c = re.sub(r"\s*const\s*$", "", c) if c.endswith("* const") else c
    # function-pointer arrays:  ret(__cdecl *[92])(args)
    m = re.match(r"^(.*?)\((?:__cdecl|__stdcall|__thiscall)?\s*\*((?:\[\d+\])+)\)\((.*)\)$", c)
    if m:
        dims = [int(x) for x in re.findall(r"\[(\d+)\]", m.group(2))]
        t = T("ptr", 4, fn=True)
        for n in reversed(dims):
            t = T("carr", n * t.size, elem=t, n=n)
        return t
    if re.match(r"^.*\((?:__cdecl|__stdcall|__thiscall)? ?\*\)\(.*\)$", c):
        return T("ptr", 4, fn=True)
    m = re.match(r"^std::array<(.*),\s*(\d+)>$", c)
    if m:
        n = int(m.group(2))
        e = parse_type(m.group(1), total // n if total else 0)
        return T("stdarr", e.size * n, elem=e, n=n)
    m = re.match(r"^(.*?)((?:\s*\[\d+\])+)$", c)
    if m:
        dims = [int(x) for x in re.findall(r"\[(\d+)\]", m.group(2))]
        cnt = 1
        for n in dims:
            cnt *= n
        e = parse_type(m.group(1), total // cnt if total else 0)
        t = e
        for n in reversed(dims):
            t = T("carr", n * t.size, elem=t, n=n)
        return t
    if c.endswith("*") or c.endswith("* const"):
        return T("ptr", 4, fn=False, pointee=re.sub(r"\s*\*\s*(const)?\s*$", "", c))
    if c in SCALARS:
        nm, sz, sg = SCALARS[c]
        return T("scalar", sz, name=nm, signed=sg, float_=nm in ("float", "double"))
    m = re.match(r"^enum\s+(.*)$", canon.strip().replace("class ", "").replace("struct ", ""))
    if canon.strip().startswith("enum "):
        return T("enum", total, name=canon.strip()[5:].strip().replace("class ", ""))
    m = re.match(r"^(?:notsa::)?WEnum<\s*(?:enum\s+)?([\w:]+)\s*,\s*([\w ]+?)\s*>$", c)
    if m and m.group(2) in SCALARS and SCALARS[m.group(2)][1] == total:
        return T("wenum", total, name=m.group(1), signed=SCALARS[m.group(2)][2])
    return T("opaque", total, name=c)


# ---------------------------------------------------------------------------------------------------------------------------------
# function resolver:  exe address -> `&Qualified::Function`
# ---------------------------------------------------------------------------------------------------------------------------------

RE_INSTALL = re.compile(r"RH_Scoped(?:Virtual)?(?:Global)?(?:Overloaded)?(?:Named)?(?:Install|VMTInstall)\s*\(\s*([^,]+?)\s*,\s*(?:\"[^\"]*\"\s*,\s*)?(0x[0-9A-Fa-f]+)")
RE_CLASS = re.compile(r"RH_ScopedClass\s*\(\s*([\w:]+)\s*\)")
RE_PAIR = re.compile(r"(0x[0-9A-Fa-f]+)\s*,\s*&?\s*((?:[A-Za-z_]\w*::)+[A-Za-z_]\w*)\b(?!\s*\()")


class FnResolver:
    def __init__(self, srcdir=None):
        self.byaddr = {}   # addr -> set of qualified names
        self.overloaded = set()
        src = Path(srcdir or REPO / "source")
        for p in src.rglob("*"):
            if p.suffix not in (".cpp", ".h", ".inc", ".hpp"):
                continue
            try:
                txt = p.read_text(errors="replace")
            except OSError:
                continue
            if "RH_Scoped" in txt:
                cls = None
                for line in txt.splitlines():
                    mc = RE_CLASS.search(line)
                    if mc:
                        cls = mc.group(1)
                    mi = RE_INSTALL.search(line)
                    if mi and "Overloaded" in line:
                        self.overloaded.add(int(mi.group(2), 16))
                    if mi and cls and "Global" not in line.split("(")[0]:
                        name = mi.group(1).strip()
                        if re.fullmatch(r"\w+", name):
                            self.byaddr.setdefault(int(mi.group(2), 16), set()).add("%s::%s" % (cls, name))
            for mp in RE_PAIR.finditer(txt):
                self.byaddr.setdefault(int(mp.group(1), 16), set()).add(mp.group(2))

    def resolve(self, addr):
        c = self.byaddr.get(addr)
        if not c:
            return None, "no RH install / table entry names 0x%X" % addr
        if len(c) > 1:
            return None, "0x%X: ambiguous %s" % (addr, sorted(c))
        if addr in self.overloaded:
            return None, "0x%X: overloaded function, needs a cast" % addr
        return "&" + next(iter(c)), None


# ---------------------------------------------------------------------------------------------------------------------------------
# literals
# ---------------------------------------------------------------------------------------------------------------------------------


def fmt_float(bits, double=False):
    fmt, ifmt, sfx, ibits = ("<d", "<Q", "", 64) if double else ("<f", "<I", "f", 32)
    raw = struct.pack(ifmt, bits)
    v = struct.unpack(fmt, raw)[0]
    if math.isnan(v) or math.isinf(v) or (v == 0 and bits != 0) or (v != 0 and abs(v) < (2.2250738585072014e-308 if double else 1.17549435e-38)):
        t = "double" if double else "float"
        return "std::bit_cast<%s>(0x%0*X%s)" % (t, ibits // 4, bits, "ull" if double else "u")
    for prec in range(1, 18 if double else 10):
        s = "%.*g" % (prec, v)
        if struct.pack(fmt, float(s)) == raw:
            break
    if "e" in s:
        from decimal import Decimal
        d = Decimal(s)
        if -5 <= d.adjusted() <= 9:
            s = format(d, "f")
    if "e" not in s and "." not in s:
        s += ".0"
    if "e" in s and "." not in s.split("e")[0]:
        a, b = s.split("e")
        s = a + ".0e" + b
    return s + sfx


def fmt_int(v, size, signed):
    """decimal for small magnitudes, hex for |v| >= 0x10000 (hashes, masks); negative big int32/int64 as a cast of the unsigned pattern"""
    sv = (v - (1 << (size * 8)) if v >= 1 << (size * 8 - 1) else v) if signed else v
    if abs(sv) < 0x10000:
        return str(sv)
    if signed and sv < 0:
        return "(int32)0x%08XU" % v if size == 4 else "(int64)0x%016XULL" % v
    return "0x%0*X%s" % (size * 2, v, "ULL" if size == 8 else ("U" if not signed else ""))


class Emit:
    def __init__(self, img, resolver):
        self.img, self.res = img, resolver

    # returns (text, notes[]) ; text None when opaque
    def scalar(self, t, raw, notes):
        v = int.from_bytes(raw, "little")
        if t.kind == "enum":
            return "static_cast<%s>(%s)" % (t.name, fmt_int(v, t.size, False))
        if t.kind == "wenum":
            # notsa::WEnum<E, Store> (ModelIndex = WEnumU16<eModelID>): implicit from E, stored as `Store`; the value is the stored integer
            return "static_cast<%s>(%s)" % (t.name, fmt_int(v, t.size, t.signed))
        if t.name == "bool":
            return "false" if v == 0 else "true" if v == 1 else "std::bit_cast<bool>((uint8)%d)" % v
        if t.name == "float":
            return fmt_float(v)
        if t.name == "double":
            return fmt_float(v, True)
        if t.name in ("wchar_t", "char16_t"):
            return fmt_int(v, t.size, False)
        return fmt_int(v, t.size, t.signed)

    def ptr(self, t, raw, addr, notes):
        v = int.from_bytes(raw, "little")
        if v == 0:
            return "nullptr"
        if t.fn or (self.img.code_lo <= v < self.img.code_hi):
            s, why = self.res.resolve(v)
            if s:
                return s
            notes.append("0x%X: %s" % (addr, why))
            return "nullptr /*TODO(0x%X)*/" % v
        notes.append("0x%X: pointer to 0x%X (string/data pointer) needs a manual initialiser" % (addr, v))
        return "nullptr /*TODO(0x%X)*/" % v

    def value(self, t, addr, notes):
        raw = self.img.bytes(addr, t.size)
        if t.kind in ("scalar", "enum", "wenum"):
            return self.scalar(t, raw, notes)
        if t.kind == "ptr":
            return self.ptr(t, raw, addr, notes)
        if t.kind in ("carr", "stdarr"):
            return self.array(t, addr, notes)
        if t.kind == "opaque" and t.size in (1, 2, 4, 8) and t.name:
            # small trivially copyable wrapper / enum / handle: exact bits (compile error, not silent, if the type is not trivially copyable)
            v = int.from_bytes(raw, "little")
            lit = {1: "(uint8)%d", 2: "(uint16)0x%X", 4: "0x%XU", 8: "0x%XULL"}[t.size] % v
            return "std::bit_cast<%s>(%s)" % (t.name, lit)
        return None

    def array(self, t, addr, notes):
        e = t.elem
        raw = self.img.bytes(addr, t.size)
        if not any(raw):
            return "{}"
        # char text
        if e.kind == "scalar" and e.name in ("char",) and all(32 <= b < 127 or b == 0 for b in raw):
            z = raw.rstrip(b"\0")
            if len(z) >= 2 and b"\0" not in z:
                return '{ "%s" }' % z.decode("ascii").replace("\\", "\\\\").replace('"', '\\"')
        items = []
        for i in range(t.n):
            ea = addr + i * e.size
            if not any(self.img.bytes(ea, e.size)):
                items.append(None)
                continue
            items.append(self.value(e, ea, notes))
            if items[-1] is None:
                return None
        while items and items[-1] is None:
            items.pop()
        zero = "{}" if e.kind in ("carr", "stdarr") else None
        out = []
        for it in items:
            if it is None:
                it = zero or self.zero_of(e)
            out.append(it)
        return "{ " + ", ".join(out) + " }"

    def zero_of(self, e):
        if e.kind == "ptr":
            return "nullptr"
        if e.kind in ("enum", "wenum"):
            return "static_cast<%s>(0)" % e.name
        if e.kind == "scalar":
            return "false" if e.name == "bool" else "0.0f" if e.name == "float" else "0.0" if e.name == "double" else "0"
        return "{}"

    def init(self, row):
        """-> (init_text or None, notes[]) ; init_text is what follows the declarator (`{}`, `{ ... }`, `= value`)"""
        return self.init_typed(row["a"], row["canonical_type"], row["sz"])

    def init_typed(self, addr, canon, size):
        """same for an explicit type spelling (aliases.json decides the type of synthetic owners)"""
        notes = []
        t = parse_type(canon, size)
        if t.size != size and t.kind != "opaque":
            return None, ["type model size %d != size %d (%s)" % (t.size, size, canon)]
        if not any(self.img.bytes(addr, size)):
            return "{}", notes     # all zero: value-initialisation (A4 ctor_audit decides which classes need more than that)
        if t.kind == "opaque" and t.size not in (1, 2, 4, 8):
            return None, ["opaque type '%s' (%d B): manual initialiser" % (t.name, t.size)]
        raw = self.img.bytes(addr, t.size)
        v = self.value(t, addr, notes)
        if v is None:
            return None, ["cannot emit"]
        if t.kind in ("carr", "stdarr"):
            return v, notes
        return "{ %s }" % v, notes


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["show", "report"])
    ap.add_argument("args", nargs="+")
    ap.add_argument("--image-dir", default=None)
    ap.add_argument("--tsv", default=None)
    a = ap.parse_args()
    rows = load_rows(a.tsv)
    by = {r["a"]: r for r in rows}
    em = Emit(Img(a.image_dir), FnResolver())
    sel = [by[int(x, 16)] for x in a.args] if a.cmd == "show" else [r for r in rows if any(s in r["owner"] for s in a.args)]
    bad = 0
    for r in sel:
        init, notes = em.init(r)
        print("%s %-40s %-6s %s %s" % (r["addr"], r["name"], r["cat"], r["canonical_type"], (init if init is None or len(init) < 200 else init[:200] + "...")))
        for n in notes:
            print("     NOTE", n)
        bad += init is None or bool(notes)
    print("%d rows, %d need manual attention" % (len(sel), bad))


if __name__ == "__main__":
    main()
