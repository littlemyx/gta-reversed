#!/usr/bin/env python3
"""ftol_scan.py - list (and optionally rewrite) every float -> integer conversion (clang `FloatingToIntegral` cast) of the game TUs, with a risk class.

Background (.notes/reports/MAC-ANIM1.md, MAC-FTOL1.md): the exe converts through `_ftol2` (x87 fistp, truncation, LOW dword): NaN / inf / out-of-range values give a defined
integer. A plain C++ cast is UB there, saturates on arm64 and gives 0x80000000 on x86-64: an out-of-range float used as a sector / grid / index / loop bound crashes the portable
build (a 2^32-iteration loop, an out-of-bounds access). The portable spelling is `NOTSA_FTOL(T, expr)` (Base.h; Win32 keeps the plain `((T)(expr))`, notsa::fp::FtolCast).

Same JSON AST pipeline as pc24_scan.py (and its stale-PCH guard; header-only inline code that lives in the PCH is scanned from ONE extra TU compiled without the PCH).

usage:
  ftol_scan.py [--cc build/PortableMac/compile_commands.json] [--only REGEX]... [--out report.tsv] [--apply CLASS[,CLASS]] [-j N] [--rebuild-pch]
  --only     regex on the TU path (default: source/game_sa, source/app, source/extensions .cpp)
  --apply A  rewrite the cast sites of the given classes (explicit C / static_cast / functional casts and simple implicit conversions) in place, in the sources AND the headers they live in
  --dir RE   with --apply: only sites whose FILE matches RE

TSV: file:line | class | target <- source | sink | function | expression
 class A  the result reaches a loop bound / array index / pool or grid-sector-count-size sink (directly or through a local variable): huge/NaN input => crash risk
 class B  everything else (positions to screen pixels, colours, timers, counters ...)
 class W  already NOTSA_FTOL / fp::Ftol (counted separately, never rewritten)
 sink = what makes it class A (index, loop, call:NAME, member:NAME, return:FN, new[]).
"""
import argparse
import concurrent.futures as cf
import json
import os
import re
import shlex
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pc24_scan as P   # noqa: E402  (reuses annotate / walk / skip_wrappers / srcof / pch_preflight)

REPO = P.REPO
SRC_OK = re.compile(r"/source/(game_sa|app|extensions)/.*\.(cpp|h|hpp|inc|inl)$")
ARITH_OPS = {"+", "-", "*", "/", "%", "<<", ">>", "&", "|", "^"}
CMP_OPS = {"<", ">", "<=", ">=", "==", "!=", "&&", "||"}
PASS_CALLS = {"min", "max", "clamp", "abs", "labs", "floor", "ceil", "Clamp", "Min", "Max", "Abs", "Limit", "Sign", "static_cast"}
SINK_CALL = re.compile(r"(?i)sector|grid|cell|tile|pool|alloc|resize|reserve|^at$|operator\[\]|GetEntry|^New$|^new|Create|AddNode|AddEntry|Zone|Region|Chunk|Block|Bucket|Slot|Index|Idx|"
                       r"memcpy|memset|malloc|^SetSize|^SetCount|FindBlock|GetPoolRef|GetAt|ByIndex|GetNode|GetSquare|Square|Lod|Cluster")
SINK_MEMBER = re.compile(r"(?i)sector|grid|cell|tile|index|idx|count|^m_?num|^num|size|len$|slot|pool|zone|region|chunk|bucket|^m_?n?(row|col)$|square|cluster")
SINK_RET = re.compile(r"(?i)sector|grid|idx|index|cell|tile|count|^num|size|block|zone|slot|area|region|chunk|bucket|node|lod|square|cluster|Id$|^Get.*(X|Y)$|ToInt|ToIdx")
GRID_EXPR = re.compile(r"floor|ceil|3000|Sector|Grid|HalfMap|0\.002f|0\.02f")
INT_NAMES = {"int": "int32", "unsigned int": "uint32", "short": "int16", "unsigned short": "uint16", "signed char": "int8", "unsigned char": "uint8", "char": "int8",
             "long long": "int64", "unsigned long long": "uint64", "long": "int32", "unsigned long": "uint32", "int8_t": "int8", "uint8_t": "uint8",
             "int16_t": "int16", "uint16_t": "uint16", "int32_t": "int32", "uint32_t": "uint32", "int64_t": "int64", "uint64_t": "uint64", "_Bool": "bool"}


def kids(n):
    return n.get("inner") or []


def is_loop_part(n):
    """-> loop kind if `n` sits in the init / condition / increment of a for / while / do (not in its body)"""
    c = n
    p = n["_par"]
    while p is not None:
        k = p.get("kind")
        if k in ("ForStmt", "WhileStmt", "DoStmt", "CXXForRangeStmt"):
            ks = kids(p)
            body = ks[-1] if k != "DoStmt" else ks[0]
            if c is not body:
                return k
        if k in ("CompoundStmt",) or k in P.FN_DECLS:
            return None
        c = p
        p = p["_par"]
    return None


class Ctx:
    def __init__(self, root):
        self.uses = {}      # var id -> [DeclRefExpr nodes]
        self.done = set()
        for n in P.walk(root):
            if n.get("kind") == "DeclRefExpr":
                rd = n.get("referencedDecl") or {}
                if rd.get("kind") == "VarDecl":
                    self.uses.setdefault(rd.get("id"), []).append(n)


def owner_fn(n):
    o = n["_par"]
    while o is not None:
        if o.get("kind") in P.FN_DECLS:
            return o
        o = o["_par"]
    return None


def callee_of(call):
    ks = kids(call)
    if not ks:
        return ""
    if call["kind"] == "CXXOperatorCallExpr":
        f = P.skip_wrappers(ks[0])
        while f.get("kind") == "ImplicitCastExpr":
            f = f["inner"][0]
        rd = f.get("referencedDecl") or {}
        return rd.get("name", "")
    return P.callee_name(call)


def sinks_of(n, ctx, depth=0):
    """Where the value of expression node `n` flows. -> list of sink strings (empty: class B)"""
    out = []
    lp = is_loop_part(n)
    if lp:
        out.append("loop")
    c = n
    p = n["_par"]
    while p is not None:
        k = p.get("kind")
        if k in ("ParenExpr", "ImplicitCastExpr", "CStyleCastExpr", "CXXStaticCastExpr", "CXXFunctionalCastExpr", "ExprWithCleanups", "MaterializeTemporaryExpr", "ConstantExpr",
                 "CXXBindTemporaryExpr", "CXXDefaultArgExpr"):
            c, p = p, p["_par"]
            continue
        if k == "UnaryOperator" and p.get("opcode") in ("+", "-", "~"):
            c, p = p, p["_par"]
            continue
        if k == "BinaryOperator" and p.get("opcode") in ARITH_OPS:
            c, p = p, p["_par"]
            continue
        if k == "ConditionalOperator":
            if c is not kids(p)[0]:
                c, p = p, p["_par"]
                continue
            break
        if k in ("CallExpr", "CXXMemberCallExpr") and callee_of(p) in PASS_CALLS:
            c, p = p, p["_par"]
            continue
        if k == "CXXOperatorCallExpr" and callee_of(p) == "operator[]":
            if c is not kids(p)[1] and c is not kids(p)[0]:
                out.append("index")
            break
        if k == "ArraySubscriptExpr":
            if c is kids(p)[1]:
                out.append("index")
            break
        if k == "VarDecl":
            out += var_sinks(p.get("id"), ctx, depth, p.get("name", "?"))
            break
        if k in ("BinaryOperator", "CompoundAssignOperator") and p.get("opcode") in ("=", "+=", "-=", "*=", "/=", "<<=", ">>=", "|=", "&=") and c is kids(p)[1]:
            lhs = P.skip_wrappers(kids(p)[0])
            if lhs.get("kind") == "DeclRefExpr":
                rd = lhs.get("referencedDecl") or {}
                if rd.get("kind") == "VarDecl":
                    out += var_sinks(rd.get("id"), ctx, depth, rd.get("name", "?"))
            elif lhs.get("kind") == "MemberExpr":
                if SINK_MEMBER.search(lhs.get("name", "")):
                    out.append("member:" + lhs.get("name", ""))
            elif lhs.get("kind") == "ArraySubscriptExpr":
                pass
            break
        if k in ("CallExpr", "CXXMemberCallExpr", "CXXOperatorCallExpr", "CXXConstructExpr"):
            nm = callee_of(p) if k != "CXXConstructExpr" else (p.get("type", {}).get("qualType", ""))
            if c is not (kids(p)[0] if k != "CXXConstructExpr" else None) and SINK_CALL.search(nm or ""):
                out.append("call:" + nm)
            break
        if k == "CXXNewExpr":
            out.append("new[]")
            break
        if k == "ReturnStmt":
            f = owner_fn(p)
            if f is not None and SINK_RET.search(f.get("name", "")):
                out.append("return:" + f.get("name", ""))
            break
        break
    return out


def var_sinks(vid, ctx, depth, name):
    if vid is None or depth > 4 or vid in ctx.done:
        return []
    ctx.done.add(vid)
    out = []
    for u in ctx.uses.get(vid, []):
        for s in sinks_of(u, ctx, depth + 1):
            out.append(("%s>%s" % (name, s)) if not s.startswith("loop") else "loop(%s)" % name)
    ctx.done.discard(vid)
    return out


def tyname(t):
    q = P.qt(t)
    return q


def ctype(q, desugared):
    """spelling for the macro's T"""
    if desugared in INT_NAMES:
        return INT_NAMES[desugared]
    return None


def scan_tu(args):
    ent, nopch, apply_classes = args
    try:
        a = shlex.split(ent["command"])
    except ValueError:
        return [], None, "unparsable command"
    out = []
    i = 0
    while i < len(a):
        if a[i] in ("-o", "-c"):
            i += 2
            continue
        out.append(a[i])
        i += 1
    if nopch:      # parse the PCH's headers textually: their inline functions become visible
        s = " ".join(out)
        m = re.search(r"-Xclang -include-pch -Xclang (\S+)", s)
        out2 = []
        j = 0
        while j < len(out):
            if out[j] == "-include-pch" or (out[j] == "-Xclang" and j + 1 < len(out) and out[j + 1] == "-include-pch"):
                j += 4 if out[j] == "-Xclang" else 2
                continue
            out2.append(out[j])
            j += 1
        out = out2
    out += ["-fsyntax-only", "-Xclang", "-ast-dump=json", ent["file"]]
    r = subprocess.run(out, capture_output=True, cwd=ent["directory"])
    if not r.stdout or r.returncode != 0:
        errs = [l for l in r.stderr.decode("utf8", "replace").splitlines() if "error" in l] or ["clang produced no AST"]
        return [], None, errs[0][:300]
    root = json.loads(r.stdout)
    P.annotate(root, P.Loc())
    path = ent["file"]
    ctx = Ctx(root)
    rows = []
    edits = []
    for n in P.walk(root):
        if n.get("kind") not in ("ImplicitCastExpr", "CStyleCastExpr", "CXXStaticCastExpr", "CXXFunctionalCastExpr"):
            continue
        if n.get("castKind") != "FloatingToIntegral" or not n.get("_p"):
            continue
        site = n
        par = n["_par"]
        if n.get("kind") == "ImplicitCastExpr" and par is not None and par.get("kind") in ("CStyleCastExpr", "CXXStaticCastExpr", "CXXFunctionalCastExpr") \
                and par.get("castKind") in ("NoOp", "FloatingToIntegral") and par.get("_p"):
            site = par      # `(int)f`: clang spells the conversion as an implicit cast under a NoOp explicit cast; the explicit node carries the text
        f, line, b, e, whole = site["_p"]
        if not f or not SRC_OK.search(f) or re.search(r"/(Fp|X87\w*|x87\w*)\.h$", f):
            continue
        if nopch and f == path:
            continue
        # skip dependent / template-instantiation duplicates of the same location: the caller dedups by (file, b)
        tgt = P.qt(n.get("type"))
        tgt_d = (n.get("type") or {}).get("desugaredQualType") or (n.get("type") or {}).get("qualType", "")
        child = kids(n)[0]
        src = P.qt(child.get("type"))
        S = P.srcof(f)
        explicit = site.get("kind") != "ImplicitCastExpr"
        text = S[b:e].decode("utf8", "replace") if explicit and e > b else ""
        text = " ".join(text.split())[:140]
        fn = owner_fn(site)
        fname = fn.get("name", "?") if fn else "<global>"
        # wrapped already?
        wrapped = bool(re.search(r"NOTSA_FTOL|fp::Ftol|FtolCast", S[max(0, b - 40):b + 1].decode("utf8", "replace"))) or "fp::Ftol" in text
        ss = sinks_of(site, ctx)
        ss = list(dict.fromkeys(ss))
        if not ss and explicit and GRID_EXPR.search(text):
            ss = ["grid-expr"]      # a floored / ceiled / +-3000 offset conversion is a map-grid coordinate (sector, radar tile, zone, water block): its users are not always traced
        cls = "W" if wrapped else ("A" if ss else "B")
        macro = (not explicit and False)
        row = ("%s:%d" % (os.path.relpath(f, REPO), line), cls, "%s <- %s" % (tgt_d, src), ",".join(ss[:3]), fname, text if explicit else "(implicit)")
        rows.append((f, b, row))
        if cls != "W" and cls in apply_classes:
            edits.append((f, site, cls))
    return rows, edits_pack(edits, root), None


def edits_pack(edits, root):
    """-> list of (file, begin, end, replacement)"""
    res = []
    for f, n, cls in edits:
        S = P.srcof(f)
        fl, line, b, e, whole = n["_p"]
        if whole:
            continue            # spelled in a macro body
        child = kids(n)[0]
        tgt = n.get("type") or {}
        desug = tgt.get("desugaredQualType") or tgt.get("qualType", "")
        qual = tgt.get("qualType", "")
        if n.get("kind") == "ImplicitCastExpr":
            T = INT_NAMES.get(desug)
            if T is None or T == "bool" or not child.get("_p"):
                continue
            cb, ce = child["_p"][2], child["_p"][3]
            if child["_p"][0] != f or ce <= cb or P.skip_wrappers(child) is None:
                continue
            res.append((f, b, e, "NOTSA_FTOL(%s, %s)" % (T, S[b:e].decode())))
            continue
        # explicit
        cp = child.get("_p")
        if not cp or cp[0] != f:
            continue
        cb, ce = cp[2], cp[3]
        if not (b <= cb < ce <= e):
            continue
        # the cast type spelling as written
        head = S[b:cb].decode("utf8", "replace")
        if n.get("kind") == "CStyleCastExpr":
            m = re.match(r"\(\s*([A-Za-z_][\w:\s]*?)\s*\)\s*$", head)
        elif n.get("kind") == "CXXStaticCastExpr":
            m = re.match(r"static_cast\s*<\s*([A-Za-z_][\w:\s]*?)\s*>\s*\(\s*$", head)
        else:
            m = re.match(r"([A-Za-z_][\w:]*)\s*\(\s*$", head)
        if not m:
            continue
        T = m.group(1).strip()
        # the closing paren of a static_cast / functional cast sits between ce and e
        tail = S[ce:e].decode("utf8", "replace")
        if n.get("kind") != "CStyleCastExpr":
            if not re.match(r"^\s*\)\s*$", tail):
                continue
        else:
            if tail.strip():
                continue
        if T in ("int", "unsigned", "unsigned int", "short", "unsigned short", "char", "unsigned char", "long", "unsigned long"):
            T = INT_NAMES.get("unsigned int" if T == "unsigned" else T, T)
        inner = S[cb:ce].decode("utf8", "replace")
        res.append((f, b, e, "NOTSA_FTOL(%s, %s)" % (T, inner)))
    return res


def apply_edits(path, es):
    S = open(path, "rb").read()
    es = sorted(set(es), key=lambda t: (t[1], -t[2]))
    # drop edits nested inside another edit of the same file (the outer rewrite carries the inner text; a second pass picks the inner one up)
    keep = []
    last_end = -1
    for ed in es:
        if ed[1] < last_end:
            continue
        keep.append(ed)
        last_end = ed[2]
    for f, b, e, rep in reversed(keep):
        S = S[:b] + rep.encode() + S[e:]
    open(path, "wb").write(S)
    return len(keep)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default=os.path.join(REPO, "build/PortableMac/compile_commands.json"))
    ap.add_argument("--only", action="append", default=[])
    ap.add_argument("--out")
    ap.add_argument("--apply", default="")
    ap.add_argument("--dir", default="")
    ap.add_argument("--rebuild-pch", action="store_true")
    ap.add_argument("--no-pch-check", action="store_true")
    ap.add_argument("--no-headers", action="store_true", help="skip the PCH-less pass over the PCH's headers")
    ap.add_argument("-j", type=int, default=12)
    a = ap.parse_args()
    cc = json.load(open(a.cc))
    if not a.no_pch_check:
        P.pch_preflight(cc, a.rebuild_pch)
    pats = [re.compile(p) for p in (a.only or [r"/source/(game_sa|app|extensions)/.*\.cpp$"])]
    ents = [e for e in cc if any(p.search(e["file"]) for p in pats)]
    classes = set(a.apply.split(",")) if a.apply else set()
    jobs = [(e, False, classes) for e in ents]
    if not a.no_headers:
        for e in cc:
            if e["file"].endswith("/source/game_sa/World.cpp"):
                jobs.append((e, True, classes))
                break
    rows = {}
    byfile = {}
    failed = []
    with cf.ProcessPoolExecutor(a.j) as ex:
        for job, (rs, eds, err) in zip(jobs, ex.map(scan_tu, jobs)):
            if err:
                failed.append((job[0]["file"], err))
                continue
            for f, b, row in rs:
                rows.setdefault((f, b), row)
            for (f, b, e, rep) in (eds or []):
                if a.dir and not re.search(a.dir, f):
                    continue
                byfile.setdefault(f, set()).add((f, b, e, rep))
    if failed:
        sys.stderr.write("ftol_scan: %d of %d TUs produced no AST; first: %s -> %s\n" % (len(failed), len(jobs), os.path.relpath(failed[0][0], REPO), failed[0][1]))
        if len(failed) * 50 > len(jobs):
            sys.stderr.write("ftol_scan: more than 2%% of the TUs failed: the report is NOT meaningful (stale PCH?) - no edits applied\n")
            sys.exit(4)
    n = 0
    if classes:
        for f, es in byfile.items():
            n += apply_edits(f, list(es))
        sys.stderr.write("applied %d edits in %d files\n" % (n, len(byfile)))
    w = open(a.out, "w") if a.out else sys.stdout
    w.write("file:line\tclass\ttarget <- source\tsink\tfunction\texpression\n")
    from collections import Counter
    cnt = Counter()
    perdir = {}
    for (f, b), row in sorted(rows.items(), key=lambda kv: (kv[0][0], kv[0][1])):
        w.write("\t".join(row) + "\n")
        cnt[row[1]] += 1
        d = "/".join(row[0].split(":")[0].split("/")[:3])
        perdir.setdefault(d, Counter())[row[1]] += 1
    sys.stderr.write("TOTAL: %s\n" % dict(cnt))
    for d, c in sorted(perdir.items()):
        sys.stderr.write("  %-40s %s\n" % (d, dict(c)))


if __name__ == "__main__":
    main()
