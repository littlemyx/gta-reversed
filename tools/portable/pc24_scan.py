#!/usr/bin/env python3
"""pc24_scan.py - find (and optionally rewrite) double-typed arithmetic that the exe computed on the x87 stack at precision control PC=24.

Background (.notes/STREAM_F_PLAN.md, RR_PLAN 11.5-11.7): the exe's main thread runs with the x87 at PC=24, so EVERY arithmetic result is rounded to a 24 bit
significand. The reversed source often spells such an expression with (double) casts / double literals / double locals; on arm64/x64 that is real 53-bit
arithmetic and gives different results (e.g. ChooseModel total 8 vs 9). The portable equivalent is IEEE single arithmetic per operation.

Mechanism (source/game_sa/Core/Fp.h): `x87d` = `double` on every non-portable build (the MSVC code bytes do not change) and `float` when NOTSA_PORTABLE is defined.
This tool finds the expression roots and, with --apply, rewrites the clean ones: `(double)` -> `(x87d)`, `double` locals -> `x87d`, exact-float double
literals `1.0` -> `(x87d)1.0`.

usage:
  pc24_scan.py [--cc build/PortableMacRR/compile_commands.json] [--only REGEX]... [--out report.tsv] [--apply] [-j N]
  --only      regex on the TU path (default: every game_sa/*.cpp TU of the compile database)
  --apply     rewrite the AUTO roots in place (source files); only expressions whose every leaf is float/int/exact-float-literal/clean local
  --summary   print per-file counts to stderr

TSV columns: file:line | verdict | ops | expression | leaves | function | oracle | note
 verdict AUTO    every leaf is a float / int / exact-float literal / clean double local: rewrite with x87d (--apply)
         CHECK-D a leaf is a genuine double entity (global / member / param / call result): the exe also rounds the op to float, but the 53 bit operand needs
                 hand treatment (fp::MulD / AddD or an explicit (float) of the operand)
         CHECK-X a leaf is an x87:: transcendental result (64 bit mantissa value rounded by the NEXT op)
         CHECK-L a leaf is a non-float-exact double literal (T3: needs MulD / AddD)
         CHECK-M the root is a math-library call on double (pow / exp / floor ...) - F-7
         SKIP-MACRO part of the expression comes from a macro expansion (cannot be rewritten textually)
 oracle  Y if the function's exe address (the `// 0xADDR` comment above its definition) occurs in tests/standalone/*, else N (? = no address)
"""
import argparse
import bisect
import concurrent.futures as cf
import json
import os
import re
import shlex
import struct
import subprocess
import sys

sys.setrecursionlimit(20000)
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ARITH = {"+", "-", "*", "/"}
PASS_FNS = {"min", "max", "clamp", "sqrt", "sqrtf", "fabs", "fabsf", "abs", "floor", "floorf", "ceil", "ceilf", "trunc", "truncf"}   # exact / correctly rounded ops of the FPU: transparent for the chain
MATH_FNS = {"pow", "exp", "log", "log10", "log2", "floor", "ceil", "fmod", "modf", "sqrt", "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "fabs", "abs",
            "sinf", "cosf", "sqrtf", "powf", "floorf", "ceilf", "fmodf", "atan2f", "ldexp", "frexp", "round", "trunc"}


def qt(t):
    if not t:
        return ""
    return (t.get("desugaredQualType") or t.get("qualType") or "").replace("const ", "").strip()


def is_dbl(t):
    return qt(t) in ("double", "long double")


def is_flt(t):
    return qt(t) == "float"


def float_exact(s):
    try:
        v = float(s)
        return struct.unpack("f", struct.pack("f", v))[0] == v
    except (OverflowError, ValueError):
        return False


class Loc:
    """Replays the JSON dumper's elision of file / line so that every node gets (file, line, begin offset, end offset)."""

    def __init__(self):
        self.file = None
        self.line = 0

    def one(self, d):
        if not isinstance(d, dict):
            return None
        macro = "spellingLoc" in d
        if macro:
            self.one(d["spellingLoc"])
            r = self.one(d["expansionLoc"])
            return (r[0], r[1], r[2], r[3], True)
        if "file" in d:
            self.file = d["file"]
        if "line" in d:
            self.line = d["line"]
        return (self.file, self.line, d.get("offset", 0), d.get("tokLen", 0), False)


def annotate(n, loc, parent=None):
    """DFS in emission order; stores n['_p']=(file,line,b,e,macro) and n['_par']."""
    n["_par"] = parent
    b = e = None
    for k, v in n.items():
        if k == "loc":
            loc.one(v)
        elif k == "range":
            b = loc.one(v.get("begin"))
            e = loc.one(v.get("end"))
    if b and e:
        n["_p"] = (b[0], b[1], b[2], e[2] + e[3], b[4] or e[4])
        n["_bmacro"] = b[4]
    kids = n.get("inner")
    if kids:
        for c in kids:
            annotate(c, loc, n)


def walk(n):
    yield n
    for c in n.get("inner") or ():
        yield from walk(c)


def skip_wrappers(n):
    """strip parens / value-category / no-op casts (never a FloatingCast / IntegralToFloating)"""
    while True:
        k = n["kind"]
        if k == "ParenExpr" or k == "ExprWithCleanups" or k == "ConstantExpr" or k == "MaterializeTemporaryExpr":
            n = n["inner"][0]
        elif k in ("ImplicitCastExpr", "CStyleCastExpr", "CXXFunctionalCastExpr", "CXXStaticCastExpr") and n.get("castKind") in ("LValueToRValue", "NoOp", "ToVoid"):
            n = n["inner"][0]
        else:
            return n


def callee_name(n):
    f = skip_wrappers(n["inner"][0]) if n.get("inner") else None
    while f is not None and f["kind"] in ("ImplicitCastExpr",):
        f = f["inner"][0]
    if f is None:
        return ""
    if f["kind"] == "DeclRefExpr":
        rd = f.get("referencedDecl", {})
        return rd.get("name", "")
    if f["kind"] == "MemberExpr":
        return f.get("name", "")
    return ""


class TU:
    def __init__(self, path, root):
        self.path = path
        self.root = root
        self.src = open(path, "rb").read()
        self.locals = {}      # id -> dict(name, node, defs[], clean)
        self.groups = {}      # DeclStmt id -> [local ids]

    # ---- leaf classification --------------------------------------------------------------------------------------------------------------------------
    def leaves(self, e, out, ctx):
        """Collect the leaves of the double expression e into out (list of (kind, text)); returns the list of arithmetic nodes."""
        e0 = e
        e = skip_wrappers(e)
        k = e["kind"]
        t = e.get("type")
        if k in ("ImplicitCastExpr", "CStyleCastExpr", "CXXFunctionalCastExpr", "CXXStaticCastExpr"):
            ck = e.get("castKind")
            src = e["inner"][0]
            if ck == "FloatingCast":
                st = skip_wrappers(src)
                if is_flt(src.get("type")) or is_flt(st.get("type")):
                    out.append(("f", ""))
                    ctx["floatsrc"].append(st)
                    return
                if not is_dbl(t):
                    out.append(("f", ""))            # narrowing (float)(double chain): its own root
                    return
                return self.leaves(src, out, ctx)
            if ck == "IntegralToFloating":
                out.append(("i", ""))
                return
            if ck in ("FloatingToIntegral", "IntegralCast", "IntegralToBoolean"):
                out.append(("i", ""))
                return
            return self.leaves(src, out, ctx)
        if k == "FloatingLiteral":
            if is_flt(t):
                out.append(("f", ""))
            elif float_exact(e.get("value", "x")):
                out.append(("lit", e.get("value")))
            else:
                out.append(("LIT", e.get("value")))
            return
        if k == "IntegerLiteral" or k == "CXXBoolLiteralExpr" or k == "CharacterLiteral":
            out.append(("i", ""))
            return
        if k == "UnaryOperator" and e.get("opcode") in ("-", "+"):
            return self.leaves(e["inner"][0], out, ctx)
        if k == "ConditionalOperator":
            for c in e["inner"][1:]:
                self.leaves(c, out, ctx)
            return
        if k == "BinaryOperator":
            op = e.get("opcode")
            if op in ARITH:
                if is_dbl(t):
                    ctx["nodes"].append(e)
                    self.leaves(e["inner"][0], out, ctx)
                    self.leaves(e["inner"][1], out, ctx)
                else:
                    out.append(("f" if is_flt(t) else "i", ""))
                return
            if op == ",":
                return self.leaves(e["inner"][1], out, ctx)
            if op == "=":
                return self.leaves(e["inner"][1], out, ctx)
        if k == "CompoundAssignOperator" and is_dbl(e.get("computeResultType")):
            ctx["nodes"].append(e)
            self.leaves(e["inner"][1], out, ctx)
            lhs = skip_wrappers(e["inner"][0])
            self.leaves(lhs, out, ctx)
            return
        if k == "DeclRefExpr":
            rd = e.get("referencedDecl", {})
            if not is_dbl(t):
                out.append(("f" if is_flt(t) else "i", ""))
                return
            if rd.get("id") in self.locals:
                out.append(("DL", rd["id"]))
                ctx["locals"].add(rd["id"])
            else:
                out.append(("D", rd.get("name", "?")))
            return
        if k == "CallExpr" or k == "CXXOperatorCallExpr" or k == "CXXMemberCallExpr":
            nm = callee_name(e)
            if not is_dbl(t):
                out.append(("f" if is_flt(t) else "i", ""))
                return
            if nm in PASS_FNS and len(e["inner"]) >= 2:
                ctx["nodes"].append(e)
                for c in e["inner"][1:]:
                    self.leaves(c, out, ctx)
                return
            if nm in MATH_FNS:
                ctx["math"].append(e)
                out.append(("M", nm))
                return
            if e["inner"] and "x87" in self.text(e["inner"][0]):
                out.append(("X", nm))
                return
            out.append(("D", nm + "()"))
            return
        if not is_dbl(t):
            out.append(("f" if is_flt(t) else "i", ""))
            return
        out.append(("D", k))

    def text(self, n):
        p = n.get("_p")
        if not p:
            return ""
        return self.src[p[2]:p[3]].decode("utf8", "replace")


def scan_tu(args):
    """returns (rows, edits) for one TU"""
    cmd_entry, apply_, oracle_addrs = args
    try:
        a = shlex.split(cmd_entry["command"])
    except ValueError:
        return [], {}
    out = []
    i = 0
    while i < len(a):
        if a[i] in ("-o", "-c"):
            i += 2
            continue
        out.append(a[i])
        i += 1
    out += ["-fsyntax-only", "-Xclang", "-ast-dump=json", cmd_entry["file"]]
    r = subprocess.run(out, capture_output=True, cwd=cmd_entry["directory"])
    if not r.stdout:
        return [], {}
    root = json.loads(r.stdout)
    annotate(root, Loc())
    path = cmd_entry["file"]
    tu = TU(path, root)
    mainnodes = [n for n in walk(root) if n.get("_p") and n["_p"][0] == path]

    # ---- double locals ---------------------------------------------------------------------------------------------------------------------------------
    funcs = []   # (begin, end, name)
    for n in mainnodes:
        k = n["kind"]
        if k in ("FunctionDecl", "CXXMethodDecl", "CXXConstructorDecl", "CXXDestructorDecl") and "inner" in n and any(c["kind"] == "CompoundStmt" for c in n["inner"]):
            funcs.append(n)
    def owner(n):
        o = n["_par"]
        best = None
        while o is not None:
            if o["kind"] in ("FunctionDecl", "CXXMethodDecl", "CXXConstructorDecl", "CXXDestructorDecl") and o.get("_p"):
                best = o
                break
            o = o["_par"]
        if best is None:
            return "<global>", None
        p = best["_p"]
        head = tu.src[p[2]:p[2] + 400].decode("utf8", "replace")
        m = re.search(r"([\w:~<>]+::)?" + re.escape(best.get("name", "")) + r"\s*\(", head)
        nm = (m.group(0)[:-1].strip() if m else best.get("name", "?"))
        # exe address comment on the line above
        line_start = tu.src.rfind(b"\n", 0, p[2])
        prev_start = tu.src.rfind(b"\n", 0, max(line_start, 0))
        prev = tu.src[prev_start + 1:line_start].decode("utf8", "replace")
        am = re.search(r"//\s*(0x[0-9A-Fa-f]{5,8})", prev)
        return nm, (am.group(1).upper().replace("0X", "0x") if am else None)

    for n in mainnodes:
        if n["kind"] == "VarDecl" and is_dbl(n.get("type")) and not n.get("storageClass") == "extern":
            par = n["_par"]
            tu.locals[n["id"]] = {"name": n.get("name"), "node": n, "defs": [], "clean": True, "param": False}
        if n["kind"] == "ParmVarDecl" and is_dbl(n.get("type")):
            pass  # genuine double entity (D)
    # definitions: init + assignments + compound assigns
    for lid, L in tu.locals.items():
        n = L["node"]
        if n.get("inner"):
            L["defs"].append(n["inner"][-1])
    for n in mainnodes:
        k = n["kind"]
        if k == "BinaryOperator" and n.get("opcode") == "=":
            l = skip_wrappers(n["inner"][0])
            if l["kind"] == "DeclRefExpr" and l.get("referencedDecl", {}).get("id") in tu.locals:
                tu.locals[l["referencedDecl"]["id"]]["defs"].append(n["inner"][1])
        elif k == "UnaryOperator" and n.get("opcode") == "&":
            l = skip_wrappers(n["inner"][0])
            if l["kind"] == "DeclRefExpr" and l.get("referencedDecl", {}).get("id") in tu.locals:
                tu.locals[l["referencedDecl"]["id"]]["clean"] = False
        elif k == "CompoundAssignOperator":
            l = skip_wrappers(n["inner"][0])
            if l["kind"] == "DeclRefExpr" and l.get("referencedDecl", {}).get("id") in tu.locals:
                tu.locals[l["referencedDecl"]["id"]]["defs"].append(n["inner"][1])
        elif k == "DeclStmt":
            ids = [c["id"] for c in n.get("inner", []) if c["kind"] == "VarDecl" and c["id"] in tu.locals]
            if len(ids) > 1:
                tu.groups[n["id"]] = ids
        # reference-bound (T& r = local) / call by reference are conservative: detected through DeclRefExpr under a non-const reference parameter is skipped

    def clean_expr(e):
        ctx = {"nodes": [], "locals": set(), "math": [], "floatsrc": []}
        lv = []
        tu.leaves(e, lv, ctx)
        return all(t in ("f", "i", "lit") or (t == "DL" and tu.locals[v]["clean"]) for t, v in lv)

    changed = True
    while changed:
        changed = False
        for lid, L in tu.locals.items():
            if not L["clean"]:
                continue
            ok = bool(L["defs"]) or True
            for d in L["defs"]:
                if not clean_expr(d):
                    ok = False
                    break
            if not ok:
                L["clean"] = False
                changed = True
        for gid, ids in tu.groups.items():
            if any(not tu.locals[i]["clean"] for i in ids):
                for i in ids:
                    if tu.locals[i]["clean"]:
                        tu.locals[i]["clean"] = False
                        changed = True

    # ---- roots -----------------------------------------------------------------------------------------------------------------------------------------
    def parent_is_arith(n):
        p = n["_par"]
        while p is not None:
            k = p["kind"]
            if k in ("ParenExpr", "ImplicitCastExpr", "ExprWithCleanups", "ConstantExpr", "MaterializeTemporaryExpr") or (k in ("CStyleCastExpr", "CXXFunctionalCastExpr", "CXXStaticCastExpr") and p.get("castKind") in ("NoOp", "FloatingCast") and is_dbl(p.get("type"))):
                p = p["_par"]
                continue
            if k == "UnaryOperator" and p.get("opcode") in ("-", "+"):
                p = p["_par"]
                continue
            if k == "ConditionalOperator" and p["inner"][0] is not n:
                p = p["_par"]
                continue
            if k == "BinaryOperator" and p.get("opcode") in ARITH and is_dbl(p.get("type")):
                return True
            if k == "CompoundAssignOperator" and is_dbl(p.get("computeResultType")) and p["inner"][1] is not None:
                return True
            if k == "CallExpr" and callee_name(p) in PASS_FNS and is_dbl(p.get("type")):
                return True
            return False
        return False

    def verdict0(kinds, dirtyloc):
        if "M" in kinds:
            return "CHECK-M"
        if "X" in kinds:
            return "CHECK-X"
        if "LIT" in kinds:
            return "CHECK-L"
        if "D" in kinds or dirtyloc:
            return "CHECK-D"
        return "AUTO"

    rows = []
    edits = {}   # (b,e) -> replacement; keyed by offsets
    lineof = {}

    def add_edit(b, e, s):
        edits[(b, e)] = s

    def collect_edits(e, seen):
        """edits for the double expression e: (double) casts, double literals; local ids are handled by the caller. Returns (edits, hit_macro)"""
        res = []
        macro = False
        for n in walk(e):
            k = n["kind"]
            if k in ("CStyleCastExpr", "CXXFunctionalCastExpr", "CXXStaticCastExpr") and is_dbl(n.get("type")) and n.get("_p"):
                p = n["_p"]
                bl = n.get("_bmacro")
                txt = tu.src[p[2]:p[3]].decode("utf8", "replace")
                m = re.search(r"\bdouble\b", txt[:40])
                if m and bl:
                    macro = True
                elif m:
                    res.append(((p[2] + m.start(), p[2] + m.end()), "x87d"))
            elif k == "FloatingLiteral" and is_dbl(n.get("type")) and n.get("_p"):
                p = n["_p"]
                if p[4]:
                    macro = True
                    continue
                res.append(((p[2], p[3]), "(x87d)" + tu.src[p[2]:p[3]].decode()))
        return res, macro

    for n in mainnodes:
        k = n["kind"]
        isnode = (k == "BinaryOperator" and n.get("opcode") in ARITH and is_dbl(n.get("type"))) or \
                 (k == "CompoundAssignOperator" and is_dbl(n.get("computeResultType"))) or \
                 (k == "CallExpr" and is_dbl(n.get("type")) and callee_name(n) in PASS_FNS and len(n["inner"]) >= 2)
        if not isnode or parent_is_arith(n):
            continue
        ctx = {"nodes": [], "locals": set(), "math": [], "floatsrc": []}
        lv = []
        tu.leaves(n, lv, ctx)
        kinds = {t for t, v in lv}
        dirtyloc = [v for t, v in lv if t == "DL" and not tu.locals[v]["clean"]]
        ed, macro = collect_edits(n, None) if verdict0(kinds, dirtyloc) == "AUTO" else ([], False)
        verdict = verdict0(kinds, dirtyloc)
        if verdict == "AUTO" and macro:
            verdict = "SKIP-MACRO"
        p = n["_p"]
        txt = " ".join(tu.src[p[2]:p[3]].decode("utf8", "replace").split())
        fn, addr = owner(n)
        orc = "?" if not addr else ("Y" if addr.lower() in oracle_addrs else "N")
        lvs = ",".join(sorted({(t if t not in ("D", "DL", "X", "M", "LIT") else t + ":" + str(v if t != "DL" else tu.locals[v]["name"])) for t, v in lv}))
        nops = len(ctx["nodes"]) + len(ctx["math"])
        rows.append((f"{os.path.relpath(path, REPO)}:{p[1]}", verdict, nops, txt[:140], lvs, fn, orc, ""))
        if verdict == "AUTO" and apply_:
            for k_, v_ in ed:
                add_edit(k_[0], k_[1], v_)

    # clean locals: type token + their initialiser / assignment edits
    for lid, L in tu.locals.items():
        n = L["node"]
        p = n.get("_p")
        if not p:
            continue
        ctxloc = n["_par"]
        verdict = "AUTO" if L["clean"] else "CHECK-D"
        fn, addr = owner(n)
        txt = " ".join(tu.src[p[2]:p[3]].decode("utf8", "replace").split())
        orc = "?" if not addr else ("Y" if addr.lower() in oracle_addrs else "N")
        if p[4]:
            verdict = "SKIP-MACRO" if verdict == "AUTO" else verdict
        rows.append((f"{os.path.relpath(path, REPO)}:{p[1]}", "LOCAL-" + verdict, 0, txt[:140], "double local " + str(L["name"]), fn, orc, ""))
        if L["clean"] and apply_ and not p[4]:
            head = tu.src[p[2]:n["loc"]["offset"] if "offset" in n.get("loc", {}) else p[3]].decode("utf8", "replace")
            nloc = n.get("loc", {})
            off = nloc.get("offset") if "spellingLoc" not in nloc else None
            if off is not None:
                head = tu.src[p[2]:off].decode("utf8", "replace")
                m = re.search(r"\bdouble\b", head)
                if m:
                    add_edit(p[2] + m.start(), p[2] + m.end(), "x87d")
            for d in L["defs"]:
                ed2, mac2 = collect_edits(d, None)
                if not mac2:
                    for k_, v_ in ed2:
                        add_edit(k_[0], k_[1], v_)
    return rows, edits, path


def apply_edits(path, edits):
    src = open(path, "rb").read()
    items = sorted(edits.items(), key=lambda kv: kv[0][0], reverse=True)
    last = None
    n = 0
    for (b, e), s in items:
        if last is not None and e > last:
            continue          # overlapping (nested) edit: skip the outer-most duplicate
        src = src[:b] + s.encode() + src[e:]
        last = b
        n += 1
    open(path, "wb").write(src)
    return n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc", default=os.path.join(REPO, "build/PortableMacRR/compile_commands.json"))
    ap.add_argument("--only", action="append", default=[])
    ap.add_argument("--out")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--summary", action="store_true")
    ap.add_argument("-j", type=int, default=8)
    ap.add_argument("--tests", default=os.path.join(REPO, "tests/standalone"))
    a = ap.parse_args()
    cc = json.load(open(a.cc))
    pats = [re.compile(p) for p in (a.only or [r"/source/game_sa/.*\.cpp$"])]
    ents = [e for e in cc if any(p.search(e["file"]) for p in pats)]
    oracle = set()
    for dp, _, fs in os.walk(a.tests):
        for f in fs:
            if f.endswith((".cpp", ".inc", ".h")):
                oracle |= {m.lower() for m in re.findall(r"0x[0-9A-Fa-f]{5,8}", open(os.path.join(dp, f), errors="replace").read())}
    allrows = []
    nedits = 0
    with cf.ProcessPoolExecutor(a.j) as ex:
        for rows, edits, path in ex.map(scan_tu, [(e, a.apply, oracle) for e in ents]):
            allrows += rows
            if a.apply and edits:
                nedits += apply_edits(path, edits)
    w = open(a.out, "w") if a.out else sys.stdout
    w.write("file:line\tverdict\tops\texpression\tleaves\tfunction\toracle\tnote\n")
    for r in allrows:
        w.write("\t".join(str(x) for x in r) + "\n")
    if a.summary or a.out:
        from collections import Counter
        c = Counter((r[0].split(":")[0], r[1]) for r in allrows)
        per = {}
        for (f, v), n in c.items():
            per.setdefault(f, {})[v] = n
        for f, d in sorted(per.items(), key=lambda kv: -sum(kv[1].values()))[:60]:
            sys.stderr.write(f"{f}: " + " ".join(f"{k}={v}" for k, v in sorted(d.items())) + "\n")
    if a.apply:
        sys.stderr.write(f"applied {nedits} edits\n")


if __name__ == "__main__":
    main()
