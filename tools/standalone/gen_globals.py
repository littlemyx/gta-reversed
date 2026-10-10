#!/usr/bin/env python3
"""gen_globals.py - dev-time generator for the "detach from gta_sa_compact.exe" plan (.notes/DETACH_DATA_PLAN.md section 3.3, steps 1-3).

NOTHING in the CMake build runs this script. It needs the user's own exe ONCE (stage `classify`); its outputs are reviewed notes
(.notes/DETACH_*) and, later, input of the codemod. Never commit exe bytes (the outputs contain none: only sizes, categories, hashes of
nothing - the literal emitter is step 4 and not part of slice A1).

Stages (each one reads what the previous wrote into --work):
  scan      step 1  scan source/ for every StaticRef<T>(0xADDR) site (comments/strings stripped, scope tracked) -> work/scan.json
  probe     step 2  build the type probe: a COPY of source/ (work/src) whose Base.h turns every `StaticRef<T>(..)` into a call of a
                    function template that instantiates `Out<sizeof(T), alignof(T), traits, elemsize>` (an incomplete type): the
                    compiler (cl /Zs under Wine, real headers, real include chain, real class/namespace/function scope of every
                    site) prints the values in "error C2079 ... uses undefined struct 'notsa_probe::Out<...>'" and the call site
                    in the following "see reference to function template instantiation" note. -> work/probe_raw/*.log, probe.json
  classify  step 3  read the bytes of [addr, addr+sizeof) of the pre- and post-_initterm image of the exe, classify a/b/c/d, emit
                    .notes/DETACH_GLOBALS.tsv, DETACH_objects.tsv, DETACH_duplicates.tsv, DETACH_conflicts.tsv, DETACH_overlaps.tsv ...
  all       scan + probe + classify

Usage:  python3 -I tools/standalone/gen_globals.py <stage> [--exe gta_sa_compact.exe] [--work DIR] [--out .notes] [--jobs N]
        (the python must have `unicorn` for stage classify, e.g. the capvenv of the scratchpad; see PORT_BRIEF.md)

Heuristics are documented next to the code they belong to ("HEURISTIC:") and summarised in .notes/reports/D-A1.md.
"""
import argparse
import collections
import csv
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SRC = REPO / "source"

# ----------------------------------------------------------------------------------------------------------------------------
# step 1: scanner
# ----------------------------------------------------------------------------------------------------------------------------

SRC_SUFFIXES = {".h", ".hpp", ".cpp", ".inl", ".inc", ".c"}
# HEURISTIC: addresses that are only used in files the run build (librw) does not compile (plan 2.2: "Excluded from the run build: 34")
DEAD_PREFIXES = ("source/game_sa/RenderWare/rw/",)
DEAD_FILES = ("source/app/platform/win/WindowedMode.cpp",)


def strip_source(src):
    """Replace comments and the contents of string/char literals by blanks (newlines kept, so line numbers stay valid)."""
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == "/" and i + 1 < n and src[i + 1] == "/":
            j = src.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c == "/" and i + 1 < n and src[i + 1] == "*":
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", src[i:j]))
            i = j
        elif c == '"':
            # raw string R"delim(...)delim"
            if i > 0 and src[i - 1] == "R":
                m = re.match(r'"([^()\\ ]{0,16})\(', src[i:i + 20])
                if m:
                    end = src.find(")" + m.group(1) + '"', i)
                    end = n if end < 0 else end + len(m.group(1)) + 2
                    out.append('"' + re.sub(r"[^\n]", " ", src[i + 1:end - 1]) + '"')
                    i = end
                    continue
            j = i + 1
            while j < n and src[j] != '"' and src[j] != "\n":
                if src[j] == "\\":
                    j += 1
                j += 1
            out.append('"' + " " * (j - i - 1) + '"')
            i = j + 1
        elif c == "'":
            # char literal (but not a digit separator 1'000)
            if i > 0 and (src[i - 1].isalnum() or src[i - 1] == "_"):
                out.append(c)
                i += 1
                continue
            j = i + 1
            while j < n and src[j] != "'" and src[j] != "\n":
                if src[j] == "\\":
                    j += 1
                j += 1
            out.append("'" + " " * (j - i - 1) + "'")
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


# HEURISTIC: `class NOTSA_EXPORT_VTABLE CCamera : public ...` - ALL_CAPS tokens between the keyword and the name are macros/attributes
RE_CLASS = re.compile(r"\b(class|struct|union)\s+(?:(?:[A-Z][A-Z0-9_]+|alignas\s*\([^)]*\)|\[\[[^\]]*\]\])\s+)*([A-Za-z_]\w*)\s*(?:final\s*)?(?::[^{;]*)?$", re.S)
RE_NS = re.compile(r"\bnamespace\s*([A-Za-z_][\w:]*)?\s*$", re.S)
RE_ENUM = re.compile(r"\benum\b")
RE_FUNC = re.compile(r"((?:[A-Za-z_~]\w*(?:<[^<>]*>)?::)*(?:operator\s*\S+|~?[A-Za-z_]\w*))\s*\((?:[^()]|\([^()]*\))*\)\s*(?:const\s*)?(?:noexcept(?:\([^)]*\))?\s*)?(?:override\s*)?(?:->\s*[^{;]+)?\s*$", re.S)
RE_DECL = re.compile(r"(?P<head>(?:\b(?:static|inline|constexpr|const|extern|thread_local)\s+)*)(?P<ty>auto|[\w:<>,\s\*\[\]]+?)\s*&\s*(?P<name>~?[A-Za-z_][\w:]*)\s*(?:\[[^\]]*\]\s*)?=\s*(?:&\s*)?$", re.S)


def norm_ws(t):
    t = re.sub(r"\s+", " ", t.strip())
    t = re.sub(r"\s*([<>,\[\]\*&:()])\s*", r"\1", t)
    return t


def find_template_end(s, i):
    """s[i] is the char after '<' of StaticRef<; returns index after the matching '>' ('>>' safe, parentheses are opaque)."""
    d, p = 1, 0
    n = len(s)
    while i < n:
        c = s[i]
        if c == "(":
            p += 1
        elif c == ")":
            p -= 1
        elif p == 0:
            if c == "<":
                d += 1
            elif c == ">" and s[i - 1] != "-":
                d -= 1
                if d == 0:
                    return i + 1
        i += 1
    return -1


def scan_text(relpath, text):
    """Return (sites, raw_casts) of one file."""
    s = strip_source(text)
    sites, raws = [], []
    # scope stack of dicts {kind, name, tmpl}; kind in class/namespace/enum/block/extern
    stack = []
    stmt_start = 0
    paren = 0
    i, n = 0, len(s)
    # positions of StaticRef<... matches, processed in order while walking
    matches = [m for m in re.finditer(r"\b(Scoped)?StaticRef\s*<", s)]
    mi = 0
    line = 1
    last_nl = 0
    nl_positions = [m.start() for m in re.finditer(r"\n", s)]
    import bisect

    def line_of(pos):
        return bisect.bisect_left(nl_positions, pos) + 1

    while i < n:
        c = s[i]
        # handle StaticRef at this position
        while mi < len(matches) and matches[mi].start() <= i:
            m = matches[mi]
            mi += 1
            if m.start() < i:
                continue
            e = find_template_end(s, m.end())
            if e < 0:
                continue
            ty = s[m.end():e - 1]
            tail = s[e:e + 200]
            ma = re.match(r"\s*\(\s*(0x[0-9A-Fa-f]+|\d+)\s*([^,)]?)", tail)
            addr_txt = ma.group(1) if ma else None
            addr_extra = ma.group(2) if ma else ""
            pre = s[stmt_start:m.start()]
            kind, name, declty = "expr", "", ""
            md = RE_DECL.search(pre) if pre.strip() else None
            if m.group(1):
                kind = "scoped"
            elif md and not re.search(r"\breturn\b", pre):
                kind, name, declty = "decl", md.group("name"), md.group("ty").strip()
            elif re.search(r"\breturn\s*&?\s*$", pre):
                kind = "return"
            sites.append(dict(
                file=relpath, line=line_of(m.start()), type=norm_ws(ty), type_raw=re.sub(r"\s+", " ", ty.strip()),
                addr_txt=addr_txt, addr_trail=addr_extra, kind=kind, name=name, declty=norm_ws(declty),
                static=bool(re.search(r"\bstatic\b", md.group("head"))) if md else False,
                inline=bool(re.search(r"\binline\b", md.group("head"))) if md else False,
                const_decl=bool(re.search(r"\bconst(expr)?\b", md.group("head"))) if md else False,
                scope=[dict(x) for x in stack],
            ))
        if c == "(":
            paren += 1
        elif c == ")":
            paren -= 1 if paren else 0
        elif c == ";" and paren <= 0:
            stmt_start = i + 1
            paren = 0
        elif c == "{":
            hdr = s[stmt_start:i]
            hdr_s = hdr.strip()
            ent = dict(kind="block", name="", tmpl=False)
            # strip a leading access label / template header noise
            mcl = RE_CLASS.search(hdr_s)
            mns = RE_NS.search(hdr_s)
            if hdr_s.endswith('"C"') or re.search(r'extern\s+"\s*C\s*"$', hdr_s) or re.search(r'extern\s+"\s*"$', hdr_s):
                ent = dict(kind="extern", name="", tmpl=False)
            elif mns and not RE_ENUM.search(hdr_s):
                ent = dict(kind="namespace", name=(mns.group(1) or ""), tmpl=False)
            elif RE_ENUM.search(hdr_s) and not re.search(r"\(", hdr_s):
                ent = dict(kind="enum", name="", tmpl=False)
            elif mcl and "(" not in hdr_s.split(mcl.group(1))[-1].split(":")[0]:
                ent = dict(kind="class", name=mcl.group(2), tmpl=bool(re.search(r"\btemplate\s*<", hdr_s)), ckind=mcl.group(1))
            else:
                mf = RE_FUNC.search(hdr_s) if hdr_s.endswith(")") or re.search(r"\)\s*(const|noexcept|override|->[^;]*)?\s*$", hdr_s) else None
                ent = dict(kind="block", name=(mf.group(1) if mf else ""), tmpl=bool(re.search(r"\btemplate\s*<", hdr_s)))
            stack.append(ent)
            stmt_start = i + 1
            paren = 0
        elif c == "}":
            if stack:
                stack.pop()
            stmt_start = i + 1
            paren = 0
        i += 1
    # raw address casts: `(T*)0xADDR`, `*(T**)0xADDR`, `reinterpret_cast<T*>(0xADDR)`, `((R(__cdecl*)(..))0xADDR)` (function pointers: kind=code)
    lines = s.split("\n")
    for m in re.finditer(r"(?:reinterpret_cast\s*<[^;<>]*(?:<[^<>]*>)?[^;<>]*>\s*\(\s*|\(\s*[A-Za-z_][^();]*?\*+\s*\)\s*|\(\s*[A-Za-z_][^();]*?\(\s*__\w+\s*\*+\s*\)\s*\([^()]*\)\s*\)\s*)(0x[0-9A-Fa-f]{6,8})\b", s):
        a_ = int(m.group(1), 16)
        if 0x401000 <= a_ < 0xCB1000:
            ln = line_of(m.start())
            raws.append(dict(file=relpath, line=ln, addr=a_, kind="data" if a_ >= 0x858000 else "code", text=re.sub(r"\s+", " ", text.split("\n")[ln - 1].strip())[:160]))
    return sites, raws


def qualified_scope(site):
    """-> (chain, local, func) chain = [('namespace'|'class', name, tmpl)], local = inside a function body, func = innermost function name."""
    chain, local, func = [], False, ""
    for e in site["scope"]:
        if e["kind"] in ("namespace", "class"):
            if not local:
                if e["kind"] == "namespace" and "::" in e["name"]:
                    for part in e["name"].split("::"):
                        chain.append(("namespace", part, False))
                elif e["kind"] == "namespace" and e["name"] == "":
                    chain.append(("namespace", "", False))
                else:
                    chain.append((e["kind"], e["name"], e.get("tmpl", False)))
            else:
                pass  # local class inside a function: ignored for the name
        elif e["kind"] == "block":
            if not local:
                local = True
                func = e["name"]
    return chain, local, func


def scan_tree(root):
    root = Path(root)
    sites, raws = [], []
    for p in sorted(root.rglob("*")):
        if p.suffix not in SRC_SUFFIXES or not p.is_file():
            continue
        rel = "source/" + p.relative_to(root).as_posix()
        if rel.endswith(".orig") or "/.git/" in rel:
            continue
        text = p.read_text(errors="replace")
        if "StaticRef" not in text and "0x" not in text:
            continue
        s, r = scan_text(rel, text)
        sites += s
        raws += r
    return sites, raws


def stage_scan(args):
    work = Path(args.work)
    work.mkdir(parents=True, exist_ok=True)
    # snapshot first: other agents edit source/ concurrently, scan and probe must see the same bytes (line numbers are the join key)
    snap = work / "snap"
    if snap.exists():
        shutil.rmtree(snap)
    shutil.copytree(Path(args.src or SRC), snap, ignore=shutil.ignore_patterns("*.orig", "*.bak"))
    root = snap
    sites, raws = scan_tree(root)
    for st in sites:
        chain, local, func = qualified_scope(st)
        st["chain"] = [list(c) for c in chain]
        st["local"] = local
        st["func"] = func
        st.pop("scope")
    import hashlib
    h = hashlib.sha1()
    for f in sorted(root.rglob("*")):
        if f.is_file() and f.suffix in SRC_SUFFIXES:
            h.update(f.relative_to(root).as_posix().encode() + b"\0" + f.read_bytes() + b"\0")
    (work / "scan.json").write_text(json.dumps(dict(sites=sites, raws=raws, src=str(root), snap_hash=h.hexdigest())))
    lit = [s for s in sites if s["addr_txt"] and s["addr_txt"].startswith("0x") and s["kind"] != "scoped" and s["file"] != "source/Base.h"]
    print(f"scan: {len(sites)} StaticRef matches, {len(lit)} hex-literal sites (non-scoped, excl. Base.h), {len({int(s['addr_txt'], 16) for s in lit})} distinct addresses, {len(raws)} raw casts")
    return sites, raws



# ----------------------------------------------------------------------------------------------------------------------------
# step 2: type probe (compile real TUs of a patched COPY of source/ with cl under Wine; every StaticRef<T> site drops a record
# {file, line, sizeof(T), alignof(T), trait bits, sizeof(elem), __FUNCSIG__} into a custom COFF section of the .obj)
# ----------------------------------------------------------------------------------------------------------------------------

PROBE_PATCH = r"""
// ---- gen_globals.py probe patch (work copy only) ----
#include <type_traits>
#pragma section("probe$m", read)
namespace notsa_probe {
template<size_t N> struct Str { char s[N]{}; constexpr Str(const char (&a)[N]) { for (size_t i = 0; i < N; i++) s[i] = a[i]; } };
template<size_t N> Str(const char (&)[N]) -> Str<N>;
template<typename T> constexpr unsigned traits() {
    unsigned f = 0;
    if constexpr (std::is_polymorphic_v<T>) f |= 1;
    if constexpr (std::is_trivially_default_constructible_v<T>) f |= 2;
    if constexpr (std::is_default_constructible_v<T>) f |= 4;
    if constexpr (std::is_aggregate_v<T>) f |= 8;
    if constexpr (std::is_pointer_v<T>) f |= 16;
    if constexpr (std::is_trivially_destructible_v<T>) f |= 32;
    if constexpr (std::is_trivially_copyable_v<T>) f |= 64;
    if constexpr (std::is_array_v<T>) f |= 128;
    if constexpr (std::is_class_v<T> || std::is_union_v<T>) f |= 256;
    if constexpr (std::is_const_v<T>) f |= 512;
    if constexpr (std::is_enum_v<T>) f |= 1024;
    if constexpr (std::is_pointer_v<std::remove_all_extents_t<T>>) f |= 2048;
    if constexpr (std::is_floating_point_v<std::remove_all_extents_t<T>>) f |= 4096;
    if constexpr (std::is_abstract_v<T>) f |= 8192;
    return f;
}
template<size_t NF, size_t NS> struct Rec {
    unsigned magic, id, line, size, align, flags, elem;
    char file[NF];
    char sig[NS];
    constexpr Rec(unsigned id_, unsigned line_, unsigned size_, unsigned align_, unsigned flags_, unsigned elem_, const char (&f)[NF], const char (&s)[NS])
        : magic(0x424F5250u), id(id_), line(line_), size(size_), align(align_), flags(flags_), elem(elem_), file{}, sig{} {
        for (size_t i = 0; i < NF; i++) file[i] = f[i];
        for (size_t i = 0; i < NS; i++) sig[i] = s[i];
    }
};
template<typename T> constexpr auto sigstr() { return Str(__FUNCSIG__); }
template<Str F, int Line, int Id, typename T> struct Holder {
    __declspec(allocate("probe$m")) static inline const Rec<sizeof(F.s), sizeof(decltype(sigstr<T>())::s)> v{
        (unsigned)Id, (unsigned)Line, (unsigned)sizeof(T), (unsigned)alignof(T), traits<T>(), (unsigned)sizeof(std::remove_all_extents_t<T>), F.s, sigstr<T>().s};
};
inline volatile const void* g_keep;
template<Str F, int Line, int Id> struct Sr {
    template<typename T> static T& f(uintptr addr) {
        g_keep = &Holder<F, Line, Id, T>::v;
        return *reinterpret_cast<T*>(addr);
    }
};
}
#define StaticRef ::notsa_probe::Sr<notsa_probe::Str(__FILE__), __LINE__, __COUNTER__>::f
// ---- end probe patch ----
"""

TRAIT_BITS = [("poly", 1), ("triv_def", 2), ("def_ctor", 4), ("aggregate", 8), ("pointer", 16), ("triv_dtor", 32), ("triv_copy", 64),
              ("array", 128), ("class", 256), ("const", 512), ("enum", 1024), ("ptr_elem", 2048), ("float_elem", 4096), ("abstract", 8192)]


def read_probe_records(path):
    """Records of the custom `probe$m` COFF sections of an .obj (regular or /bigobj)."""
    d = Path(path).read_bytes()
    m, ns = struct.unpack_from("<HH", d, 0)
    if m == 0 and ns == 0xFFFF:  # bigobj
        ns, sp, nsym = struct.unpack_from("<III", d, 44)
        base, symsz = 56, 20
    else:
        _, ns, _, sp, nsym, osz, _ = struct.unpack_from("<HHIIIHH", d, 0)
        base, symsz = 20 + osz, 18
    strtab = sp + nsym * symsz
    recs = []
    for i in range(ns):
        o = base + 40 * i
        name = d[o:o + 8].rstrip(b"\0").decode("latin1")
        if name.startswith("/"):
            off = int(name[1:])
            e = d.index(b"\0", strtab + off)
            name = d[strtab + off:e].decode("latin1")
        _, _, rs, rp = struct.unpack_from("<IIII", d, o + 8)
        if not name.startswith("probe"):
            continue
        b = d[rp:rp + rs]
        off = 0
        while off + 28 <= len(b):   # internal-linkage records are not COMDATs: several records share one section (4-byte aligned)
            magic, rid, line, size, align, flags, elem = struct.unpack_from("<7I", b, off)
            if magic != 0x424F5250:
                break
            e1 = b.index(b"\0", off + 28)
            file = b[off + 28:e1].decode("latin1")
            e2 = b.index(b"\0", e1 + 1)
            sig = b[e1 + 1:e2].decode("latin1")
            recs.append(dict(id=rid, line=line, size=size, align=align, flags=flags, elem=elem, file=file, sig=sig))
            off = b.find(b"PROB", e2 + 1)
            if off < 0:
                break
    return recs


def sig_type(sig):
    """__FUNCSIG__ of sigstr<T>() -> T"""
    m = re.search(r"sigstr<(.*)>\(void\)\s*$", sig)
    return m.group(1) if m else sig


def make_clflags(work, build_dir, alt=False):
    """cl flags of the run build (taken from the configured build dir's compile_commands.json, entry game_sa/Timer.cpp).
    alt=True: the 'DLL-like' config without librw / SDL3 / standalone (needed for the few sites inside `#ifndef NOTSA_RW_LIBRW` etc.)"""
    cc = Path(build_dir) / "compile_commands.json"
    if not cc.exists():
        raise SystemExit(f"{cc} missing: configure the StandaloneRelease preset once (flags are taken from there)")
    import shlex
    ents = json.loads(cc.read_text())
    e = next(x for x in ents if x["file"].endswith("game_sa/Timer.cpp"))
    rs = str(SRC)
    out = []
    drop_alt = {"-DNOTSA_RW_LIBRW", "-DNOTSA_STANDALONE", "-DNOTSA_STANDALONE_RUN", "-DNOTSA_USE_SDL3"}
    for t in shlex.split(e["command"]):
        if t.startswith(("/Yu", "/Fp", "/FI", "/Fo", "/Fd", "/MP", "/Z7", "/FS", "-c")) or t in ("/O2", "/Ob2", "/wd4996", "/W3", "/arch:IA32"):
            continue
        if t.endswith("Timer.cpp"):
            continue
        if alt and (t in drop_alt or t.endswith(("/fakerw", "/vendor/librw"))):
            continue
        out.append(t.replace(rs, str(Path(work) / "src")))
    out += ["/W0", "/Od", "/bigobj", "/DNOTSA_PROBE", "/c"]
    if alt:
        out += ["-D_DX9_SDK_INSTALLED"]
    return out


class BuildLock:
    def __init__(self, path):
        self.path = path

    def __enter__(self):
        if not self.path:
            return
        t0 = time.time()
        while True:
            try:
                os.mkdir(self.path)
                return
            except FileExistsError:
                if time.time() - t0 > 3 * 3600:
                    raise SystemExit("build lock wait > 3 h")
                time.sleep(15)

    def __exit__(self, *a):
        if self.path:
            try:
                os.rmdir(self.path)
            except OSError:
                pass


def run_cl(flags, extra, log, timeout=1200):
    env = dict(os.environ)
    home = str(Path.home())
    env["PATH"] = f"{home}/tools/Wine Staging.app/Contents/Resources/wine/bin:{home}/.local/bin:/opt/homebrew/bin:" + env["PATH"]
    env["WINEPREFIX"] = f"{home}/.wine-msvc"
    env["WINEDEBUG"] = "-all"
    with open(log, "wb") as lf:
        try:
            p = subprocess.run(flags + extra, stdout=lf, stderr=subprocess.STDOUT, env=env, timeout=timeout)
            return p.returncode
        except subprocess.TimeoutExpired:
            return -999


def probe_pass(work, name, flags, owners, jobs):
    """Build a PCH from the patched StdInc and compile one TU per owner file (.cpp itself / synthetic `#include header` TU)."""
    from concurrent.futures import ThreadPoolExecutor
    work = Path(work)
    wsrc = work / "src"
    pch = work / ("pch" + name)
    pch.mkdir(exist_ok=True)
    (pch / "pch.hxx").write_text(f'#include "{wsrc}/StdInc.cpp"\n')
    (pch / "pch.cxx").write_text('#include "pch.hxx"\n')
    pf = [f"/Yc{pch}/pch.hxx", f"/Fp{pch}/pch.pch", f"/FI{pch}/pch.hxx", f"/Fo{pch}/pch.obj"]
    rc = run_cl(flags, pf + [str(pch / "pch.cxx")], pch / "pch.log")
    if rc != 0:
        raise SystemExit(f"PCH build failed rc={rc}, see {pch / 'pch.log'}")
    raw = work / ("probe_raw" + name)
    if raw.exists():
        shutil.rmtree(raw)
    raw.mkdir()
    tus = []
    for k, f in enumerate(owners):
        rel = f[len("source/"):]
        if f.endswith((".cpp", ".c")):
            tus.append((f, str(wsrc / rel)))
        else:
            tu = raw / f"hdr_{k}.cpp"
            tu.write_text(f'#include "{wsrc / rel}"\n')
            tus.append((f, str(tu)))
    print(f"probe[{name or 'main'}]: {len(owners)} owner files -> {len(tus)} TUs, {jobs} jobs")
    use = [f"/Yu{pch}/pch.hxx", f"/Fp{pch}/pch.pch", f"/FI{pch}/pch.hxx"]

    def one(item):
        k, (owner, tu) = item
        obj = raw / f"{k}.obj"
        log = raw / f"{k}.log"
        rc = run_cl(flags, use + [f"/Fo{obj}", tu], log)
        recs = read_probe_records(obj) if obj.exists() else []
        if obj.exists():
            obj.unlink()
        return dict(owner=owner, tu=tu, rc=rc, n=len(recs), recs=recs, log=str(log), cfg=name or "main")

    res = [dict(owner="(pch)", tu=str(pch / "pch.cxx"), rc=0, n=0, recs=read_probe_records(pch / "pch.obj"), log=str(pch / "pch.log"), cfg=name or "main")]
    t0 = time.time()
    with ThreadPoolExecutor(max_workers=jobs) as ex:
        for k, r in enumerate(ex.map(one, list(enumerate(tus)))):
            res.append(r)
            if (k + 1) % 50 == 0:
                print(f"  {k + 1}/{len(tus)} TUs, {time.time() - t0:.0f}s")
    return res


def join_key(f, line):
    return f"{f}:{line}"


def hex_sites(scan):
    return [s for s in scan["sites"] if s["file"] != "source/Base.h" and s["kind"] != "scoped" and s["addr_txt"] and s["addr_txt"].startswith("0x")]


def stage_probe(args):
    work = Path(args.work)
    scan = json.loads((work / "scan.json").read_text())
    sites = [s for s in scan["sites"] if s["file"] != "source/Base.h"]
    src_root = Path(scan["src"])
    # 1. patched copy of the scanned tree (src_root = the snapshot taken by `scan`)
    wsrc = work / "src"
    if wsrc.exists():
        shutil.rmtree(wsrc)
    shutil.copytree(src_root, wsrc, ignore=shutil.ignore_patterns("*.orig", "*.bak"))
    base = wsrc / "Base.h"
    t = base.read_text()
    marker = "    return var;\n}\n"
    i = t.index(marker, t.index("ScopedStaticRef")) + len(marker)
    base.write_text(t[:i] + PROBE_PATCH + t[i:])
    owners = sorted({s["file"] for s in sites})
    with BuildLock(args.lock):
        res = probe_pass(work, "", make_clflags(work, args.build_dir), owners, args.jobs)
        (work / "probe_raw.json").write_text(json.dumps(res))
        merged = merge_probe(work, quiet=True)
        # 2. sites that the run configuration does not compile (#ifndef NOTSA_RW_LIBRW, #ifndef NOTSA_USE_SDL3, WindowedMode.cpp, rw/*.h): second pass, DLL-like flags
        miss = sorted({s["file"] for s in hex_sites(scan) if join_key(s["file"], s["line"]) not in merged})
        if miss:
            res2 = probe_pass(work, "2", make_clflags(work, args.build_dir, alt=True), miss, args.jobs)
            (work / "probe_raw2.json").write_text(json.dumps(res2))
    merge_probe(work)


def _norm_probe_file(f, wprefix):
    f = f.replace("\\", "/")
    if f[1:3] == ":/":
        f = f[2:]
    f = os.path.normpath(f)
    if f.startswith(wprefix):
        f = "source/" + f[len(wprefix):]
    return f


def merge_probe(work, quiet=False):
    """probe_raw*.json -> probe.json {sites: {"file:line": [records]}, failed: [...]}. Pass 2 (alt config) only fills keys pass 1 lacks."""
    work = Path(work)
    wprefix = str(work / "src").rstrip("/") + "/"
    merged, failed = {}, []
    for fn, cfg in (("probe_raw.json", "run"), ("probe_raw2.json", "alt")):
        pth = work / fn
        if not pth.exists():
            continue
        res = json.loads(pth.read_text())
        seen = set()
        part = {}
        for r in res:
            if r["rc"] != 0:
                failed.append(dict(owner=r["owner"], tu=r["tu"], rc=r["rc"], log=r["log"], cfg=cfg))
            for rc_ in r["recs"]:
                f = _norm_probe_file(rc_["file"], wprefix)
                ty = sig_type(rc_["sig"])
                k2 = (f, rc_["line"], ty)
                if k2 in seen:
                    continue
                seen.add(k2)
                part.setdefault(join_key(f, rc_["line"]), []).append(dict(type=ty, size=rc_["size"], align=rc_["align"], flags=rc_["flags"], elem=rc_["elem"], tu=r["owner"], id=rc_["id"], cfg=cfg))
        for k, v in part.items():
            merged.setdefault(k, v)
    snap_hash = json.loads((work / "scan.json").read_text()).get("snap_hash")
    (work / "probe.json").write_text(json.dumps(dict(sites=merged, failed=failed, traits=TRAIT_BITS, snap_hash=snap_hash), indent=0))
    if not quiet:
        print(f"probe: {sum(len(v) for v in merged.values())} records for {len(merged)} (file,line) keys; {len(failed)} TU compile failures (pass 1 failures of dead-in-run files are expected)")
    return merged


# ----------------------------------------------------------------------------------------------------------------------------
# step 3: join scanner + probe + exe image -> tables
# ----------------------------------------------------------------------------------------------------------------------------

IMG_BASE_NAMES = (".rdata", ".data", "_TEXT_HA", "_rwdseg", ".text", "_rwcseg")


class Image:
    """Pre- and post-_initterm data image of the exe (layout of original_data.bin: VA data_base.. , gaps zero)."""

    def __init__(self, work, exe):
        work = Path(work)
        for sub, extra in (("img_post", []), ("img_pre", ["--no-initterm"])):
            d = work / sub
            if not (d / "original_data.bin").exists():
                d.mkdir(parents=True, exist_ok=True)
                r = subprocess.run([sys.executable, "-I", str(REPO / "tools/standalone/extract_exe_data.py")] + extra + [str(exe), str(d)], capture_output=True, text=True)
                if r.returncode != 0:
                    raise SystemExit("extract_exe_data.py failed (needs unicorn for the post image):\n" + r.stdout[-500:] + r.stderr[-2000:])
        meta = json.loads((work / "img_post" / "original_data.json").read_text())
        self.meta = meta
        self.base = meta["data_base"]
        self.end = meta["data_end"]
        self.code_lo, self.code_hi = meta["code_lo"], meta["code_hi"]
        self.post = (work / "img_post" / "original_data.bin").read_bytes()
        self.pre = (work / "img_pre" / "original_data.bin").read_bytes()
        self.secs = meta["sections"]
        self.ptrs = {}   # addr -> (value, 'V'|'C')
        for line in (work / "img_post" / "data_pointers.txt").read_text().splitlines():
            a, v, c, _ = line.split()
            if c in ("V", "C"):
                self.ptrs[int(a, 16)] = (int(v, 16), c)

    def section(self, a):
        for s in self.secs:
            if s["va"] <= a < s["va"] + s["virt_size"]:
                if s["name"] == ".data":
                    return ".data" if a < s["va"] + s["raw_size"] else ".bss"
                return s["name"]
        return "?"

    def has(self, a, n):
        return self.base <= a and a + n <= self.end

    def get(self, img, a, n):
        return img[a - self.base:a - self.base + n]

    def dword(self, a, img=None):
        img = img or self.post
        return struct.unpack_from("<I", img, a - self.base)[0]

    def ptr_scan(self, a, size, canon=""):
        """pointer-like dwords in [a, a+size): (code, str, data, vtbl, [(off, kind, value)]).
        HEURISTIC: code = accepted by extract_exe_data's classifier (data_pointers.txt V/C); vtbl = points at the start of a code-pointer run
        in .rdata; str = points at a printable-ASCII NUL-terminated string (>= 2 chars) in .rdata/.data; data = any other value inside the image."""
        pc = ps = pd = pv = 0
        lst = []
        if a % 4 or small_elem(canon):
            return 0, 0, 0, 0, []
        for off in range(0, size - size % 4, 4):
            v = self.dword(a + off)
            if v == 0:
                continue
            if (a + off) in self.ptrs:
                pc += 1
                lst.append((off, "code", v))
            elif self.base <= v < self.end:
                sec = self.section(v)
                if sec == ".rdata" and v in self.ptrs:
                    pv += 1
                    lst.append((off, "vtbl", v))
                elif self.cstring_at(v) is not None and sec in (".rdata", ".data"):
                    ps += 1
                    lst.append((off, "str", v))
                else:
                    pd += 1
                    lst.append((off, "data", v))
        return pc, ps, pd, pv, lst

    def cstring_at(self, v):
        """printable-ASCII NUL-terminated string (>= 2 chars) at VA v inside the image -> str or None"""
        if not (self.base <= v < self.end):
            return None
        o = v - self.base
        e = self.post.find(b"\0", o, o + 256)
        if e < 0 or e - o < 2:
            return None
        s = self.post[o:e]
        if all(32 <= c < 127 or c in (9, 10, 13) for c in s):
            return s.decode("ascii")
        return None


SMALL_SCALARS = ("bool", "char", "signed char", "unsigned char", "short", "unsigned short")


def small_elem(canon):
    """True if the innermost element of an (array of) scalar type is narrower than a pointer: such storage cannot hold pointer slots, so the
    4-byte-aligned dword scan would only produce false hits (review D-A4: std::array<short,14> gBeatTrackLookup was counted as 7 data pointers)."""
    t = canon.strip()
    while True:
        m = re.match(r"^(?:class |struct )?std::array<(.*),\s*\d+>$", t) or re.match(r"^(.*?)\s*\[[^\]]*\]$", t)
        if not m:
            break
        t = m.group(1).strip()
    return t in SMALL_SCALARS


def parse_audit(path):
    """INITTERM_AUDIT.md: section 5 (178 non-float initialisers) + section 6 (float stores).
    -> list of dict(kind, fn, ctor, lo, hi, what) ; float stores as dict(kind='float', dest, fn)"""
    txt = Path(path).read_text()
    rows, floats = [], []
    s5 = txt.index("## 5.")
    s6 = txt.index("## 6.")
    for line in txt[s5:s6].splitlines():
        if not line.startswith("| ") or line.startswith("| #") or line.startswith("|---"):
            continue
        cols = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cols) < 9 or not cols[0].isdigit():
            continue
        what, net = cols[4], cols[6]
        ctor = None
        obj_lo = obj_hi = None
        m = re.search(r"ctor 0x([0-9A-Fa-f]+)", what)
        if m:
            ctor = int(m.group(1), 16)
        m = re.search(r"this=0x([0-9A-Fa-f]+)", what)
        if m:
            obj_lo = int(m.group(1), 16)
        m = re.search(r"(\d+) x 0x([0-9A-Fa-f]+) @0x([0-9A-Fa-f]+)", what)
        if m:
            obj_lo = int(m.group(3), 16)
            obj_hi = obj_lo + int(m.group(1)) * int(m.group(2), 16)
        lo = hi = None
        m = re.search(r"0x([0-9A-Fa-f]+)\.\.0x([0-9A-Fa-f]+)", net)
        if m:
            lo, hi = int(m.group(1), 16), int(m.group(2), 16) + 1
        rows.append(dict(idx=int(cols[0]), fn=int(cols[2], 16), ctor=ctor, obj_lo=obj_lo, obj_hi=obj_hi, lo=lo, hi=hi, what=what, net=net, cl=cols[7], action=cols[8]))
    in_block = False
    for line in txt[s6:].splitlines():
        if line.startswith("```"):
            in_block = not in_block
            continue
        if in_block:
            m = re.match(r"([0-9A-Fa-f]{6,8}) ([0-9A-Fa-f]{8}) ([0-9A-Fa-f]{6,8})\s*$", line)
            if m:
                floats.append(dict(dest=int(m.group(1), 16), bits=int(m.group(2), 16), fn=int(m.group(3), 16)))
    return rows, floats


def qual_name(site):
    """qualified C++ name of the declared variable (best effort)"""
    name = site["name"]
    if not name:
        return ""
    parts = []
    for kind, nm, tm in site["chain"]:
        if nm:
            parts.append(nm)
        else:
            parts.append("(anon)")
    if site["local"]:
        fn = site["func"]
        pre = "::".join(p for p in parts if p != "(anon)")
        return (f"{fn}()::" if fn else (pre + "::<fn>()::" if pre else "<fn>()::")) + name
    if "::" in name:
        # `uint16& CStats::m_X = ...` : qualifier is a class of the enclosing namespaces
        ns = [p for k, p, _ in [tuple(c) for c in site["chain"]] if k == "namespace" and p]
        return "::".join(ns + [name])
    return "::".join([p for p in parts if p != "(anon)"] + [name])


def owner_rank(s):
    return (0 if s["kind"] == "decl" and not s["local"] else 1 if s["kind"] == "decl" else 2 if s["kind"] == "return" else 3,
            0 if s["file"].endswith((".h", ".hpp")) else 1, s["file"], s["line"])


def is_dead_file(f):
    return f.startswith(DEAD_PREFIXES) or f in DEAD_FILES


def row_delta(img, r):
    a, n = r["addr"], r["size"]
    return sum(1 for x, y in zip(img.get(img.post, a, n), img.get(img.pre, a, n)) if x != y) if img.has(a, n) else 0


def ctor_run_map(rows, audit_rows, audit_floats, img):
    """addr -> reason for every row whose own constructor / initialiser ran in _initterm (see HEURISTICS above)."""
    out = {}
    for r in rows:
        a, size = r["addr"], r["size"]
        why = []
        for x in audit_rows:
            if x["cl"] in ("b", "n") and x["obj_lo"] is not None and a <= x["obj_lo"] < a + size:
                why.append(f"audit#{x['idx']}:{x['cl']}")
            elif x["cl"] == "b" and x["obj_lo"] is None and x["lo"] is not None and x["lo"] < a + size and x["hi"] > a and row_delta(img, r) > 0:
                why.append(f"fill#{x['idx']}")   # ctor-less fill loop that really changed bytes of this extent
        for f in audit_floats:
            if a <= f["dest"] < a + size:
                why.append("float-init")
                break
        if why:
            out[a] = ",".join(why[:4])
    return out


def stage_classify(args):
    work, out = Path(args.work), Path(args.out)
    scan = json.loads((work / "scan.json").read_text())
    probe = json.loads((work / "probe.json").read_text())
    if probe.get("snap_hash") != scan.get("snap_hash"):
        raise SystemExit("probe.json was produced from another source snapshot than scan.json: rerun `scan` + `probe` (line numbers are the join key)")
    img = Image(work, args.exe)
    audit_rows, audit_floats = parse_audit(REPO / ".notes" / "INITTERM_AUDIT.md")
    sites = hex_sites(scan)
    # attach probe records to sites: key file:line, several sites on one line pair up by textual order = counter id order
    by_line = collections.defaultdict(list)
    for s in sites:
        by_line[join_key(s["file"], s["line"])].append(s)
    for k, lst in by_line.items():
        recs = sorted(probe["sites"].get(k, []), key=lambda r: r["id"])
        # records of the same line with identical type are a single template instantiation: broadcast
        for i, s in enumerate(lst):
            s["probe"] = recs[i] if i < len(recs) else (recs[-1] if recs and len(recs) == 1 else None)
    unprobed = [s for s in sites if not s.get("probe")]
    # ---- group by address
    groups = collections.defaultdict(list)
    for s in sites:
        s["addr"] = int(s["addr_txt"], 16)
        groups[s["addr"]].append(s)
    rows = []
    for addr in sorted(groups):
        ss = sorted(groups[addr], key=owner_rank)
        o = ss[0]
        named = next((x for x in ss if x["name"]), None)
        probed = [x for x in ss if x.get("probe")]
        po = o.get("probe") or (probed[0]["probe"] if probed else None)
        size = max((x["probe"]["size"] for x in probed), default=0)
        rows.append(dict(addr=addr, sites=ss, owner=o, named=named, probe=po, size=size, psize=sorted({x["probe"]["size"] for x in probed})))
    # ---- classify
    ctor_runs = ctor_run_map(rows, audit_rows, audit_floats, img)
    for r in rows:
        a, size = r["addr"], r["size"]
        r["section"] = img.section(a)
        r["in_image"] = img.has(a, size) if size else False
        flags = []
        if not r["in_image"]:
            flags.append("outside_image")
        post = img.get(img.post, a, size) if r["in_image"] else b""
        pre = img.get(img.pre, a, size) if r["in_image"] else b""
        r["nz"] = sum(1 for c in post if c)
        r["delta"] = sum(1 for x, y in zip(post, pre) if x != y)
        r["delta_nz"] = sum(1 for x, y in zip(post, pre) if x != y and x)
        pc, ps, pd, pv, ptr_list = img.ptr_scan(a, size, r["probe"]["type"] if r["probe"] else "") if r["in_image"] else (0, 0, 0, 0, [])
        r["ptr_code"], r["ptr_str"], r["ptr_data"], r["ptr_vtbl"] = pc, ps, pd, pv
        r["ptrs"] = ptr_list
        fl = r["probe"]["flags"] if r["probe"] else 0
        r["poly"] = bool(fl & 1)
        r["traits"] = ",".join(n for n, b in TRAIT_BITS if fl & b)
        # category (HEURISTICS, see report): d = a constructor of THIS object ran in _initterm (audit rows of class b/n whose object start lies in
        # the extent, float-init destinations, ctor-less fill loops) or the type is polymorphic with image bytes changed; a = all-zero after
        # _initterm; c = pointer-like dwords in the declared extent; b = everything else (constants / tables).
        own = ctor_runs.get(r["addr"])
        if own:
            cat, r["d_why"] = "d", own
        elif r["poly"] and r["delta"]:
            cat, r["d_why"] = "d", "poly+delta"
        elif r["delta"] and (r["probe"] and (r["probe"]["flags"] & 256)) and r["size"] >= 64:
            cat, r["d_why"] = "d", "class-victim"   # AERadioTrackManager: CMenuManager's ctor writes into it
        elif r["nz"] == 0:
            cat, r["d_why"] = "a", ""
        elif pc or ps or pd:
            cat, r["d_why"] = "c", ""
        else:
            cat, r["d_why"] = "b", ""
        r["cat"] = cat
        r["flags"] = flags
    # HEURISTIC "tail" columns: the repo often declares only the first element / a prefix of a table. Look at the gap between the end of the
    # declared extent and the next declared global (<= 8 KB): pointer-like dwords, _initterm delta bytes and non-zero bytes there hint at an
    # under-declared extent (A4/B batches must look at those rows).
    srt = sorted(rows, key=lambda r: r["addr"])
    for i, r in enumerate(srt):
        end = r["addr"] + r["size"]
        nxt = srt[i + 1]["addr"] if i + 1 < len(srt) else end
        gap = max(0, min(nxt, end + 8192) - end)
        r["gap"] = nxt - end if nxt > end else 0
        r["tail_ptr"] = r["tail_delta"] = r["tail_nz"] = 0
        if gap and r["in_image"] and img.has(end, gap) and r["section"] == img.section(end):
            pc, ps, pd, pv, _ = img.ptr_scan(end + (-end) % 4, gap - (-end) % 4)
            r["tail_ptr"] = pc + ps + pd
            r["tail_nz"] = sum(1 for c in img.get(img.post, end, gap) if c)
            r["tail_delta"] = sum(1 for x, y in zip(img.get(img.post, end, gap), img.get(img.pre, end, gap)) if x != y)
    # cat_plan: the category the plan's census (est-capped extents) would give. Two differences to `cat`:
    #  (1) initterm "victims" (a scalar nobody constructs, but another object's ctor writes a non-zero value into it) count as zero-init (a)
    #      because the exe FILE holds zeros there; `cat` says b (the value must be emitted);
    #  (2) HEURISTIC tail promotion: a non-scalar (class/struct/array) b row whose declared extent is a prefix of a table (<= 1100 B gap up to
    #      the next declared global) and whose tail holds pointer-like dwords is counted as c (the census scanned est-capped extents).
    for r in srt:
        cp = r["cat"]
        if cp == "b" and r["delta"] and not r["poly"]:
            cp = "a"
        elif cp == "b" and r["probe"] and (r["probe"]["flags"] & (256 | 128)) and 0 < r["gap"] <= 1100 and r["tail_ptr"] > 0 and r["section"] != ".rdata":
            cp = "c"
        r["cat_plan"] = cp
    write_tables(args, scan, sites, rows, unprobed, img, audit_rows, audit_floats)


def write_tables(args, scan, sites, rows, unprobed, img, audit_rows, audit_floats):
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    # duplicates / conflicts / overlaps
    dups, confs = [], []
    for r in rows:
        ss = r["sites"]
        r["n_sites"] = len(ss)
        if len(ss) > 1:
            dups.append(r)
        types = collections.OrderedDict()
        for s in ss:
            types.setdefault(s["type"], []).append(s)
        r["conflict"] = ""
        if len(types) > 1:
            canon = {s["probe"]["type"] if s.get("probe") else "?" for s in ss}
            sizes = {s["probe"]["size"] if s.get("probe") else -1 for s in ss}
            if len(sizes) > 1:
                kind = "size"
            elif len(canon) > 1:
                kind = "type"
            else:
                kind = "spelling"
            r["conflict"] = kind
            confs.append((r, kind, types, canon, sizes))
    # overlaps (extent of the max-size type)
    ov = []
    srt = sorted(rows, key=lambda r: r["addr"])
    for i, r in enumerate(srt):
        end = r["addr"] + r["size"]
        j = i + 1
        while j < len(srt) and srt[j]["addr"] < end:
            q = srt[j]
            inner_end = q["addr"] + q["size"]
            rel = "contained" if inner_end <= end else "partial"
            ov.append((r, q, rel))
            r.setdefault("ov", []).append(f"+{q['addr'] - r['addr']:#x}:{q['addr']:#X}")
            q.setdefault("ov_in", []).append(f"{r['addr']:#X}")
            j += 1
    # TSV
    cols = ["addr", "section", "cat", "cat_plan", "size", "align", "name", "type", "canonical_type", "traits", "nz_bytes", "delta_bytes", "ptr_code", "ptr_str", "ptr_data", "ptr_vtbl",
            "owner", "kind", "n_sites", "other_sites", "decl", "scope", "local_fn", "live_run", "probe_cfg", "est", "gap_to_next", "tail_nz", "tail_ptr", "tail_delta", "notes"]
    lines = ["\t".join(cols)]
    for r in rows:
        o, n = r["owner"], r["named"]
        po = r["probe"]
        notes = []
        if r["conflict"]:
            notes.append(f"conflict:{r['conflict']}")
        if r["n_sites"] > 1:
            notes.append("dup")
        if r.get("ov"):
            notes.append("overlaps:" + ",".join(r["ov"]))
        if r.get("ov_in"):
            notes.append("inside:" + ",".join(r["ov_in"]))
        if len(r["psize"]) > 1:
            notes.append("sizes:" + "/".join(map(str, r["psize"])))
        if not r["in_image"]:
            notes.append("outside_image")
        if r["named"] is None:
            notes.append("no_decl_name")
        if r["delta"] and r["cat"] == "d" and r["size"] <= 4 and not r["poly"]:
            notes.append("initterm_scalar")
        if r["poly"]:
            notes.append("polymorphic")
        live = any(not is_dead_file(s["file"]) for s in r["sites"])
        cfgs = sorted({s["probe"]["cfg"] for s in r["sites"] if s.get("probe")})
        nm = qual_name(n) if n else f"<{Path(o['file']).name}:{o['line']}>"
        scope = "/".join((k[0] + ":" + (nm_ or "(anon)")) for k, nm_, _ in [tuple(c) for c in o["chain"]])
        d = ("static " if (n and n["static"]) else "") + ("inline " if (n and n["inline"]) else "") + ("const " if (n and n["const_decl"]) else "")
        row = [f"0x{r['addr']:06X}", r["section"], r["cat"], r["cat_plan"], r["size"], po["align"] if po else "", nm, o["type"], po["type"] if po else "", r["traits"], r["nz"], r["delta"],
               r["ptr_code"], r["ptr_str"], r["ptr_data"], r["ptr_vtbl"], f"{o['file']}:{o['line']}", o["kind"], r["n_sites"],
               ";".join(f"{s['file']}:{s['line']}" for s in r["sites"] if s is not o), d.strip(), scope, (o["func"] or "<lambda>") if o["local"] else "", int(live),
               "+".join(cfgs), 0 if po else 1, r["gap"], r["tail_nz"], r["tail_ptr"], r["tail_delta"], " ".join(notes)]
        lines.append("\t".join(str(x).replace("\t", " ").replace("\n", " ") for x in row))
    (out / "DETACH_GLOBALS.tsv").write_text("\n".join(lines) + "\n")
    # duplicates
    L = ["addr\tn\tname\ttypes\tsites"]
    for r in dups:
        L.append("\t".join([f"0x{r['addr']:06X}", str(r["n_sites"]), qual_name(r["named"]) if r["named"] else "", "|".join(sorted({s["type"] for s in r["sites"]})),
                            ";".join(f"{s['file']}:{s['line']}[{s['kind']}{':' + s['name'] if s['name'] else ''}]" for s in r["sites"])]))
    (out / "DETACH_duplicates.tsv").write_text("\n".join(L) + "\n")
    L = ["addr\tclass\tsizes\tcanonical_types\tspellings (site list)"]
    for r, kind, types, canon, sizes in confs:
        L.append("\t".join([f"0x{r['addr']:06X}", kind, "/".join(map(str, sorted(sizes))), "|".join(sorted(canon)),
                            " || ".join(f"{t} @ " + ";".join(f"{s['file']}:{s['line']}" for s in v) for t, v in types.items())]))
    (out / "DETACH_conflicts.tsv").write_text("\n".join(L) + "\n")
    L = ["outer_addr\touter_name\touter_size\tinner_addr\tinner_name\tinner_size\toffset\trelation\touter_type\tinner_type"]
    for r, q, rel in ov:
        L.append("\t".join([f"0x{r['addr']:06X}", qual_name(r["named"]) if r["named"] else "", str(r["size"]), f"0x{q['addr']:06X}", qual_name(q["named"]) if q["named"] else "",
                            str(q["size"]), f"{q['addr'] - r['addr']:#x}", rel, r["owner"]["type"], q["owner"]["type"]]))
    (out / "DETACH_overlaps.tsv").write_text("\n".join(L) + "\n")
    # objects (category d)
    write_objects(out, rows, img, audit_rows, audit_floats)
    # probe.json (types)
    types = {}
    for r in rows:
        for s in r["sites"]:
            if s.get("probe"):
                types.setdefault(s["probe"]["type"], dict(size=s["probe"]["size"], align=s["probe"]["align"], flags=s["probe"]["flags"], elem=s["probe"]["elem"], spellings=set()))["spellings"].add(s["type"])
    pj = {t: dict(v, spellings=sorted(v["spellings"]), traits=[n for n, b in TRAIT_BITS if v["flags"] & b]) for t, v in sorted(types.items())}
    (out / "DETACH_probe.json").write_text(json.dumps(dict(trait_bits=dict(TRAIT_BITS), types=pj), indent=1))
    # extra sites (not in the 2,755 rows)
    ex = ["file\tline\tkind\ttype\taddr_expr\tnote"]
    for s in scan["sites"]:
        if s["file"] == "source/Base.h":
            continue
        if s["kind"] == "scoped":
            ex.append(f"{s['file']}\t{s['line']}\tScopedStaticRef\t{s['type']}\t{s['addr_txt']}\tvariable address; the flags word (2nd arg) is a second global")
        elif not (s["addr_txt"] or "").startswith("0x"):
            ex.append(f"{s['file']}\t{s['line']}\tdecimal/other\t{s['type']}\t{s['addr_txt']}\tnot a 0x literal ({int(s['addr_txt']):#x})" if (s["addr_txt"] or "").isdigit() else f"{s['file']}\t{s['line']}\tother\t{s['type']}\t{s['addr_txt']}\t")
        elif s["addr_trail"] in ("+", "-", "*"):
            ex.append(f"{s['file']}\t{s['line']}\tbase+expr\t{s['type']}\t{s['addr_txt']}{s['addr_trail']}...\taddress is a base plus computed offset (indexed access); the row is the base only")
    (out / "DETACH_extra_sites.tsv").write_text("\n".join(ex) + "\n")
    raws = ["file\tline\taddr\tkind\ttext"]
    seenraw = set()
    for rr in scan["raws"]:
        raws.append(f"{rr['file']}\t{rr['line']}\t0x{rr['addr']:X}\t{rr['kind']}\t{rr['text']}")
    (out / "DETACH_raw_casts.tsv").write_text("\n".join(raws) + "\n")
    # summary
    cnt = collections.Counter(r["cat"] for r in rows)
    secs = collections.Counter(r["section"] for r in rows)
    summary = dict(rows=len(rows), sites=len(sites), est=sum(1 for r in rows if not r["probe"]), cat=dict(cnt), cat_plan=dict(collections.Counter(r["cat_plan"] for r in rows)), sections=dict(secs), duplicates=len(dups),
                   dup_extra_sites=sum(r["n_sites"] - 1 for r in dups), conflicts=len(confs), conflict_kinds=dict(collections.Counter(k for _, k, *_ in confs)),
                   overlaps=len(ov), unprobed=len(unprobed), dead_in_run_addrs=sum(1 for r in rows if all(is_dead_file(s["file"]) for s in r["sites"])),
                   alt_cfg_rows=sum(1 for r in rows if any(s.get("probe") and s["probe"]["cfg"] == "alt" for s in r["sites"])))
    (Path(args.work) / "summary.json").write_text(json.dumps(summary, indent=1))
    print(json.dumps(summary))


def write_objects(out, rows, img, audit_rows, audit_floats):
    # float initialisers by destination
    fl = {f["dest"]: f for f in audit_floats}
    L = ["addr\tname\ttype\tsize\tsection\tctor_va\tctor_source\trepo_ctor\tdelta_bytes\tdelta_nz_bytes\tnz_bytes\tvptr_dwords\tptr_other\tinitterm_ranges\tpolymorphic\towner\tnotes"]
    n = 0
    for r in rows:
        if r["cat"] != "d":
            continue
        a, size = r["addr"], r["size"]
        # HEURISTIC attribution of the constructor: (1) audit rows whose object start (this= / array start) lies inside the extent;
        # (2) _initterm float stores into the extent; (3) ctor-less fill loops whose net range overlaps; foreign = a ctor of ANOTHER object whose
        # net delta range overlaps this extent (CMenuManager writes 52 B into other globals)
        own = [x for x in audit_rows if x["obj_lo"] is not None and a <= x["obj_lo"] < a + size and x["ctor"] is not None]
        fhits = [fl[d_] for d_ in fl if a <= d_ < a + size]
        fills = [x for x in audit_rows if x["obj_lo"] is None and x["lo"] is not None and x["lo"] < a + size and x["hi"] > a]
        foreign = [x for x in audit_rows if x["obj_lo"] is not None and not (a <= x["obj_lo"] < a + size) and x["lo"] is not None and x["lo"] < a + size and x["hi"] > a and x["ctor"] is not None]
        ctors = sorted({x["ctor"] for x in own})
        src = "audit"
        if not ctors and fhits:
            ctors, src = sorted({f["fn"] for f in fhits}), "float-init"
        if not ctors and fills:
            ctors, src = sorted({x["fn"] for x in fills}), "audit-fill-loop"
        if not ctors and foreign:
            ctors, src = sorted({x["ctor"] for x in foreign}), "foreign-ctor-write"
        if not ctors:
            src = "none-found"
        reuse = sorted({m_.group(1) for x in own for m_ in re.finditer(r"repo has reversed ctor `([^`]+)`", x["action"])})
        fl_ = r["probe"]["flags"] if r["probe"] else 0
        triv = bool(fl_ & 2)
        repo_ctor = ("reversed:" + ",".join(reuse)) if reuse else ("default ctor non-trivial (trait)" if (fl_ & 4 and not triv) else ("none (trivial / aggregate)" if triv else ("no default ctor" if not (fl_ & 4) else "?")))
        nr = 0
        if r["in_image"]:
            pre, post = img.get(img.pre, a, size), img.get(img.post, a, size)
            in_run = False
            for x, y in zip(pre, post):
                if x != y and not in_run:
                    nr += 1
                in_run = x != y
        o = r["owner"]
        notes = []
        if len(r["psize"]) > 1:
            notes.append("sizes:" + "/".join(map(str, r["psize"])))
        if r["size"] <= 4 and not r["poly"]:
            notes.append("scalar written by an _initterm float/int initialiser")
        L.append("\t".join(str(x) for x in [f"0x{a:06X}", qual_name(r["named"]) if r["named"] else f"<{Path(o['file']).name}:{o['line']}>", o["type"], size, r["section"],
                                              ",".join(f"0x{c:06X}" for c in ctors), src, repo_ctor, r["delta"], r["delta_nz"], r["nz"], r["ptr_vtbl"], r["ptr_code"] + r["ptr_str"] + r["ptr_data"], nr,
                                              int(r["poly"]), f"{o['file']}:{o['line']}", " ".join(notes)]))
        n += 1
    (out / "DETACH_objects.tsv").write_text("\n".join(L) + "\n")
    print(f"objects: {n} rows")



# ----------------------------------------------------------------------------------------------------------------------------
# slice A4: --check = every duplicate / conflict / overlap / under-declared extent of the CURRENT source tree is decided in aliases.json
# ----------------------------------------------------------------------------------------------------------------------------

def live_sites():
    """hex-literal StaticRef sites of the current source tree (no snapshot), enriched like stage_scan does."""
    sites, _ = scan_tree(SRC)
    for st in sites:
        chain, local, func = qualified_scope(st)
        st["chain"] = [list(c) for c in chain]
        st["local"] = local
        st["func"] = func
        st.pop("scope", None)
    return [s for s in sites if s["file"] != "source/Base.h" and s["kind"] != "scoped" and s["addr_txt"] and s["addr_txt"].startswith("0x")]


def site_key(s, addr=None):
    """identity of a site that survives line drift: (file, address, kind, qualified name, enclosing function)"""
    return (s["file"], int(s["addr_txt"], 16) if addr is None else addr, s["kind"], qual_name(s) if s["name"] else "", s["func"])


def listed_key(r, addr):
    return (r["file"], addr, r["kind"], r["name"], r["func"])


def stage_check(args):
    """`--check`: 0 unresolved or exit status 1. Unresolved = (1) a live site of an address that has several sites (duplicate / conflict) or that
    aliases.json treats specially and that aliases.json does not list; (2) two effective extents that intersect after applying aliases.json
    (members / views / virtual rows removed, size overrides, new globals added); (3) a row with non-zero bytes behind its extent (tail_nz) without a
    tail_audit verdict, or an EXTEND verdict without an `extents` override; (4) a category-a row with a non-trivial default ctor without a ctor_audit
    verdict; (5) an address in the source that DETACH_GLOBALS.tsv does not know. Stale entries (listed but no longer in the source) are only warnings:
    after the codemod converted a site it disappears from the scan."""
    out = Path(args.out)
    J = json.loads(Path(args.aliases or out / "aliases.json").read_text())
    rows = {}
    for r in csv.DictReader(open(out / "DETACH_GLOBALS.tsv"), delimiter="\t"):
        rows[int(r["addr"], 16)] = r
    live = live_sites()
    by_addr = collections.defaultdict(list)
    for s in live:
        by_addr[int(s["addr_txt"], 16)].append(s)
    bad, warn = [], []
    H = lambda a: "0x%X" % a
    # ---- listed sites
    listed = collections.Counter()
    special = set()          # addresses with a decision
    def add(r, a):
        listed[listed_key(r, a)] += 1
    for ak, g in J["globals"].items():
        a = int(ak, 16)
        special.add(a)
        if "file" in g["owner"]:
            add(g["owner"], a)
        for r in g["aliases"]:
            add(r, a)
    for m in J["members"]:
        special.add(int(m["addr"], 16))
        for r in m["sites"]:
            add(r, int(m["addr"], 16))
    for v in J["views"]:
        special.add(int(v["addr"], 16))
        add(v["site"], int(v["addr"], 16))
    for v in J["virtual"]:
        special.add(int(v["addr"], 16))
        for r in v["sites"]:
            add(r, int(v["addr"], 16))
    for v in J["readdress"]:
        special.add(int(v["frm"], 16))
        add(v["site"], int(v["frm"], 16))
    have = collections.Counter(site_key(s) for s in live)
    for a, ss in sorted(by_addr.items()):
        if len(ss) < 2 and a not in special:
            continue
        for s in ss:
            k = site_key(s)
            if listed[k] <= 0:
                bad.append(f"unlisted site {H(a)} {s['file']}:{s['line']} [{s['kind']} {k[3] or k[4]}]")
            else:
                listed[k] -= 1
    for k, n in listed.items():
        if n > 0:
            warn.append(f"stale entry (no longer in source): {H(k[1])} {k[0]} [{k[2]} {k[3] or k[4]}]")
    for a in by_addr:
        if a not in rows:
            bad.append(f"address {H(a)} is not a row of DETACH_GLOBALS.tsv (rerun scan/probe/classify)")
    # ---- effective extents
    ext = {}
    for a, r in rows.items():
        ext[a] = (a, int(r["size"]), r["name"])
    for m in J["members"]:
        if int(m["addr"], 16) != int(m["owner"], 16):
            ext.pop(int(m["addr"], 16), None)
    for v in J["virtual"]:
        ext.pop(int(v["addr"], 16), None)
    for v in J["views"]:
        ext.pop(int(v["addr"], 16), None)
        ext[int(v["verify_addr"], 16)] = (int(v["verify_addr"], 16), v["verify_size"], "view of " + v["site"]["name"])
    for ak, g in J["globals"].items():
        a = int(ak, 16)
        if a in ext:
            ext[a] = (a, g["size"], ext[a][2])
    for ak, e in J.get("extents", {}).items():
        a = int(ak, 16)
        if a in ext:
            ext[a] = (a, e["true_size"], ext[a][2])
    for n in J["new_globals"]:
        ext[int(n["addr"], 16)] = (int(n["addr"], 16), n["size"], "new " + n["type"])
    prev = None   # (end, addr, size, name) of the extent that reaches furthest
    for a, sz, nm in sorted(ext.values()):
        if prev is not None and a < prev[0]:
            bad.append(f"overlap: {H(a)}+{sz} ({nm}) intersects {H(prev[1])}+{prev[2]} ({prev[3]})")
        if prev is None or a + sz > prev[0]:
            prev = (a + sz, a, sz, nm)
    # ---- tails / ctors
    ta = J.get("tail_audit", {})
    for a, r in sorted(rows.items()):
        if int(r["tail_nz"] or 0) > 0:
            v = ta.get(H(a))
            if v is None:
                bad.append(f"tail_nz>0 without tail_audit verdict: {H(a)} {r['name']}")
            elif v.startswith("EXTEND") and H(a) not in J.get("extents", {}):
                bad.append(f"tail_audit EXTEND without extents entry: {H(a)} {r['name']}")
    ca = J.get("ctor_audit", {})
    for a, r in sorted(rows.items()):
        if r["cat"] == "a" and "triv_def" not in r["traits"].split(","):
            if H(a) not in ca:
                bad.append(f"non-trivial default ctor on a zero row without ctor_audit verdict: {H(a)} {r['name']}")
    for w in warn[:20]:
        print("warning:", w)
    for b in bad:
        print("UNRESOLVED:", b)
    print(f"check: {len(live)} live sites, {len(by_addr)} addresses, {len(special)} decided addresses, {len(ext)} effective extents; "
          f"{len(bad)} unresolved, {len(warn)} stale")
    if bad:
        sys.exit(1)

# ----------------------------------------------------------------------------------------------------------------------------
# main
# ----------------------------------------------------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("stage", nargs="?", choices=["scan", "probe", "merge", "classify", "all", "check"])
    ap.add_argument("--check", action="store_true", help="same as the stage `check` (slice A4): every duplicate / conflict / overlap / tail of the current source is decided in aliases.json")
    ap.add_argument("--aliases", default=None, help="aliases.json for the check (default: <out>/aliases.json)")
    ap.add_argument("--exe", default=str(REPO / "gta_sa_compact.exe"))
    ap.add_argument("--work", default=str(REPO / "build" / "detach_work"))
    ap.add_argument("--out", default=str(REPO / ".notes"))
    ap.add_argument("--src", default=None, help="source tree to scan (default: <repo>/source)")
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--build-dir", default=str(REPO / "build" / "StandaloneRelease"), help="configured build dir whose compile_commands.json gives the cl flags")
    ap.add_argument("--lock", default=None, help="build mutex directory (mkdir lock) held while cl runs")
    args = ap.parse_args()
    if args.check:
        args.stage = "check"
    if args.stage is None:
        ap.error("stage required")
    if args.stage == "check":
        stage_check(args)
        return
    if args.stage in ("scan", "all"):
        stage_scan(args)
    if args.stage in ("probe", "all"):
        stage_probe(args)
    if args.stage == "merge":
        merge_probe(args.work)
    if args.stage in ("classify", "all"):
        stage_classify(args)


if __name__ == "__main__":
    main()
