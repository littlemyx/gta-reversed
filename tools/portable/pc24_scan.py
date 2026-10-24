#!/usr/bin/env python3
"""pc24_scan.py - find (and optionally rewrite) double-typed arithmetic that the exe computed on the x87 stack at precision control PC=24.

Background (.notes/STREAM_F_PLAN.md, RR_PLAN 11.5-11.7): the exe's main thread runs with the x87 at PC=24, so EVERY arithmetic result is rounded to a 24 bit
significand. The reversed source often spells such an expression with (double) casts / double literals / double locals; on arm64/x64 that is real 53-bit
arithmetic and gives different results (e.g. ChooseModel total 8 vs 9). The portable equivalent is IEEE single arithmetic per operation.

Mechanism (source/game_sa/Core/Fp.h): `x87d` = `double` on every non-portable build (the MSVC code bytes do not change) and `float` when NOTSA_PORTABLE is defined.
This tool finds the expression roots and, with --apply, rewrites the clean ones: `(double)` -> `(x87d)`, `double` locals -> `x87d`, exact-float double
literals `1.0` -> `(x87d)1.0`; an operation with a genuine 53 bit operand (double global / call result / x87:: result / non-float literal) is computed in
double and rounded to float: `X87R(a * b)` (Base.h: `((x87d)(...))` portable, plain `(...)` elsewhere: MSVC code bytes unchanged).

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
import shutil
import struct
import subprocess
import sys

sys.setrecursionlimit(20000)
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ARITH = {"+", "-", "*", "/"}
PASS_FNS = {"min", "max", "clamp", "sqrt", "sqrtf", "fabs", "fabsf", "abs", "floor", "floorf", "ceil", "ceilf", "trunc", "truncf"}   # exact / correctly rounded ops of the FPU: transparent for the chain
EXACT_FNS = {"fabs", "fabsf", "abs", "floor", "floorf", "ceil", "ceilf", "trunc", "truncf"}   # no rounding of their own
OVERLOAD_FNS = {"pow", "exp", "log", "log10", "log2", "fmod", "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "hypot", "sinh", "cosh", "tanh", "cbrt", "exp2", "expm1", "log1p", "remainder", "atan2"}   # float / double overloads give different results
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
        """-> (file, line, offset, tokLen, macro, spell) ; spell = None or (spelling file, spelling offset, spelling tokLen)"""
        if not isinstance(d, dict):
            return None
        if "spellingLoc" in d:
            sp = self.one(d["spellingLoc"])
            r = self.one(d["expansionLoc"])
            return (r[0], r[1], r[2], r[3], True, (sp[0], sp[2], sp[3]))
        if "file" in d:
            self.file = d["file"]
        if "line" in d:
            self.line = d["line"]
        return (self.file, self.line, d.get("offset", 0), d.get("tokLen", 0), False, None)


_SRC = {}


def srcof(path):
    if path not in _SRC:
        try:
            _SRC[path] = open(path, "rb").read()
        except OSError:
            _SRC[path] = b""
    return _SRC[path]


def inv_end(path, off, toklen):
    """end offset of the macro invocation whose name starts at `off`: a function-like macro ends at the balancing ')'"""
    s = srcof(path)
    i = off + toklen
    j = i
    while j < len(s) and s[j] in b" \t":
        j += 1
    if j >= len(s) or s[j:j + 1] != b"(":
        return i
    depth = 0
    k = j
    while k < len(s):
        c = s[k:k + 1]
        if c == b"(":
            depth += 1
        elif c == b")":
            depth -= 1
            if depth == 0:
                return k + 1
        k += 1
    return i


def endpoint(r, is_end):
    """-> (offset, body_macro_invocation or None). A token spelled in a macro ARGUMENT lies in the invocation's text: it is edited at its spelling location
    (the macro is transparent). Anything spelled in a macro body can only be edited as the whole invocation (offsets of the invocation start / end)."""
    f, line, off, tl, macro, spell = r
    if not macro:
        return (off + tl if is_end else off), None
    ie = inv_end(f, off, tl)
    if spell and spell[0] == f and off <= spell[1] < ie:
        return (spell[1] + spell[2] if is_end else spell[1]), None
    return (ie if is_end else off), (f, off)


def annotate(n, loc, parent=None):
    """DFS in emission order; stores n['_p']=(file,line,b,e,macro) and n['_par']. macro = the whole node is spelled inside ONE macro body (cannot be edited textually)."""
    n["_par"] = parent
    b = e = None
    for k, v in n.items():
        if k == "loc":
            loc.one(v)
        elif k == "range":
            b = loc.one(v.get("begin"))
            e = loc.one(v.get("end"))
    if b and e:
        bo, bm = endpoint(b, False)
        eo, em_ = endpoint(e, True)
        whole = bm is not None and em_ is not None and bm == em_
        n["_p"] = (b[0], b[1], bo, eo, whole)
        n["_bmacro"] = bm is not None
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


CAST_KINDS = ("ImplicitCastExpr", "CStyleCastExpr", "CXXFunctionalCastExpr", "CXXStaticCastExpr")
CLEAN = ("f", "i", "lit", "node")      # float-valued after the rewrite
FN_DECLS = ("FunctionDecl", "CXXMethodDecl", "CXXConstructorDecl", "CXXDestructorDecl")


class TU:
    def __init__(self, path, root):
        self.path = path
        self.root = root
        self.src = srcof(path)
        self.wide = []
        self.wide_seen = set()
        self.locals = {}      # id -> dict(name, node, defs[], clean)
        self.groups = {}      # DeclStmt id -> [local ids]

    WIDE_INTS = {"int", "unsigned int", "long", "unsigned long", "long long", "unsigned long long", "__int64", "unsigned __int64", "size_t", "ptrdiff_t"}

    def note_wide(self, src):
        """an integer that can exceed 2^24 converted to double inside a chain that becomes float arithmetic: the exe's fild is exact, a float is not"""
        if qt(src.get("type")) not in self.WIDE_INTS:
            return
        if src["kind"] == "IntegerLiteral":
            try:
                if abs(int(src.get("value", "0"))) <= (1 << 24):
                    return
            except ValueError:
                pass
        p = src.get("_p")
        if p and (p[0], p[2]) not in self.wide_seen:
            self.wide_seen.add((p[0], p[2]))
            self.wide.append(src)

    def text(self, n):
        p = n.get("_p")
        return srcof(p[0])[p[2]:p[3]].decode("utf8", "replace") if p else ""

    # ---- operand classification ----------------------------------------------------------------------------------------------------------------------
    # proc(e, em, depth) -> (kind, opedits)
    #   kind: 'f' float-valued float expr, 'i' int, 'lit' exact-float double literal, 'node' arithmetic node (float-valued after the rewrite),
    #         'D' genuine double-valued operand (double global / member / param / call result / dirty local), 'X' x87:: result, 'M' math library result, 'LITD' non-float literal
    #   opedits: edits that make this operand a float when its parent node is "clean" (explicit (double) casts, exact literals)
    #   em: list collecting the edits that are needed regardless (wraps of dirty nodes + edits of nested clean nodes); None = dry run
    def proc(self, e, em, depth=0):
        while True:
            k = e["kind"]
            if k in ("ParenExpr", "ExprWithCleanups", "ConstantExpr", "MaterializeTemporaryExpr"):
                e = e["inner"][0]
                continue
            if k == "ImplicitCastExpr" and e.get("castKind") in ("LValueToRValue", "NoOp", "ToVoid"):
                e = e["inner"][0]
                continue
            if k in ("CStyleCastExpr", "CXXFunctionalCastExpr", "CXXStaticCastExpr") and e.get("castKind") in ("NoOp", "LValueToRValue", "ToVoid"):
                if is_dbl(e.get("type")):      # explicit (double)X where X is already double typed (a promoted float literal, a double expression ...)
                    kind, ed = self.proc(e["inner"][0], em, depth)
                    if kind in CLEAN:
                        ed = ed + self.cast_edit(e)
                    return kind, ed
                e = e["inner"][0]
                continue
            break
        t = e.get("type")
        if k in CAST_KINDS:
            ck = e.get("castKind")
            src = e["inner"][0]
            st = skip_wrappers(src)
            if ck == "FloatingCast":
                if is_flt(src.get("type")) or is_flt(st.get("type")):
                    self.proc(st, em, depth + 1)                 # nested nodes in the float operand
                    ed = []
                    if k != "ImplicitCastExpr" and is_dbl(t):
                        ed = self.cast_edit(e)
                    return "f", ed
                if not is_dbl(t):                               # narrowing (float)(double expr)
                    self.proc(src, em, depth + 1)
                    return "f", []
                return self.proc(src, em, depth)
            if ck == "IntegralToFloating" and em is not None and is_dbl(t):
                self.note_wide(st)
            if ck in ("IntegralToFloating", "FloatingToIntegral", "IntegralCast", "IntegralToBoolean"):
                return "i", []
            return self.proc(src, em, depth)
        if k == "FloatingLiteral":
            if is_flt(t):
                return "f", []
            if float_exact(e.get("value", "x")):
                return "lit", self.lit_edit(e)
            return "LITD", []
        if k in ("IntegerLiteral", "CXXBoolLiteralExpr", "CharacterLiteral"):
            return "i", []
        if k == "UnaryOperator" and e.get("opcode") in ("-", "+"):
            return self.proc(e["inner"][0], em, depth)
        if k == "ConditionalOperator":
            self.proc(e["inner"][0], em, depth + 1)
            a = self.proc(e["inner"][1], em, depth + 1)
            b = self.proc(e["inner"][2], em, depth + 1)
            if a[0] in CLEAN and b[0] in CLEAN:
                return "f", a[1] + b[1]
            return "D", []
        if k == "BinaryOperator" and e.get("opcode") in ARITH:
            if not is_dbl(t):
                self.proc(e["inner"][0], em, depth + 1)
                self.proc(e["inner"][1], em, depth + 1)
                return ("f" if is_flt(t) else "i"), []
            kl, el = self.proc(e["inner"][0], em, depth + 1)
            kr, er = self.proc(e["inner"][1], em, depth + 1)
            self.node(e, kl, kr, el + er, em, depth)
            return "node", []
        if k == "BinaryOperator" and e.get("opcode") in (",", "="):
            return self.proc(e["inner"][1], em, depth)
        if k == "CompoundAssignOperator":
            return "D", []
        if k == "DeclRefExpr":
            rd = e.get("referencedDecl", {})
            if not is_dbl(t):
                return ("f" if is_flt(t) else "i"), []
            if rd.get("id") in self.locals:
                return ("f", []) if self.locals[rd["id"]]["clean"] else ("D", [])
            return "D", []
        if k in ("CallExpr", "CXXOperatorCallExpr", "CXXMemberCallExpr"):
            nm = callee_name(e)
            if not is_dbl(t):
                return ("f" if is_flt(t) else "i"), []
            args = e["inner"][1:]
            if nm in EXACT_FNS and len(args) == 1:
                # fabs / floor / ceil / trunc do not round: a 53 bit operand stays 53 bit (no X87R around it: AcosCrt `ad = fabs(x)` must stay double)
                ka, ea = self.proc(args[0], em, depth + 1)
                if ka not in CLEAN and ka != "node":
                    return "D", []
                self.node(e, ka, "f", ea, em, depth)
                return "node", []
            if nm in PASS_FNS and nm not in ("min", "max", "clamp") and len(args) == 1:
                ka, ea = self.proc(args[0], em, depth + 1)
                self.node(e, ka, "f", ea, em, depth)
                return "node", []
            if nm in ("min", "max", "clamp"):
                ks = [self.proc(a, em, depth + 1) for a in args]
                if all(x[0] in CLEAN for x in ks):
                    return "f", [z for x in ks for z in x[1]]
                return "D", []
            if e["inner"] and "x87" in self.text(e["inner"][0]):
                return "X", []
            if nm in MATH_FNS:
                return "M", []
            return "D", []
        return ("D" if is_dbl(t) else ("f" if is_flt(t) else "i")), []

    def node(self, n, kl, kr, opedits, em, depth):
        """clean node: apply the operand edits; dirty node (a genuine double operand): compute in double, round the result to float: (x87d)(...)"""
        if em is None:
            return
        p = n.get("_p")
        if not p:
            return
        if kl in CLEAN and kr in CLEAN:
            em.extend(opedits)
        else:
            if p[4]:
                em.append(("MACRO", 0, 0, 0, ""))
                return
            if srcof(p[0])[p[2]:p[2] + 5] == b"X87R(" or srcof(p[0])[max(p[2] - 5, 0):p[2]] == b"X87R(":
                em.append(("DONE", 0, 0, 0, ""))
                return                       # already wrapped by an earlier run (the node range is the X87R( ) invocation itself, or sits directly inside it)
            em.append((p[2], p[2], -depth, 0, "X87R("))
            em.append((p[3], p[3], depth, 1, ")"))

    def cast_edit(self, n):
        p = n.get("_p")
        if not p:
            return []
        if n.get("_bmacro"):
            return [("MACRO", 0, 0, 0, "")]
        txt = srcof(p[0])[p[2]:p[3]].decode("utf8", "replace")
        m = re.search(r"\bdouble\b", txt[:40])
        return [(p[2] + m.start(), p[2] + m.end(), 0, 2, "x87d")] if m else []

    def lit_edit(self, n):
        p = n.get("_p")
        if not p:
            return []
        if p[4]:
            return [("MACRO", 0, 0, 0, "")]
        return [(p[2], p[3], 0, 2, "(x87d)" + srcof(p[0])[p[2]:p[3]].decode())]



def pc53_line(p):
    """a source line carrying the marker `pc53` is computed at 53 bits ON PURPOSE (the exe builds its sqrt tables before D3D9 sets PC=24): never touched"""
    S = srcof(p[0])
    a = S.rfind(b"\n", 0, p[2]) + 1
    b = S.find(b"\n", p[2])
    return b"pc53" in S[a:b if b >= 0 else len(S)]


def split_params(sig):
    """'uintptr_t (double &, const float &)' -> ['double &', 'const float &'] ; None when it is not a plain function type"""
    i = sig.find("(")
    if i < 0 or not sig.rstrip().endswith(")"):
        return None
    inner = sig[i + 1:sig.rstrip().rfind(")")]
    out, depth, cur = [], 0, ""
    for ch in inner:
        if ch in "(<[":
            depth += 1
        elif ch in ")>]":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def mutable_ref_use(ref):
    """True when the DeclRefExpr `ref` (an lvalue of a double local) is bound to a non-const reference parameter of a call"""
    child = ref
    par = ref["_par"]
    while par is not None and par["kind"] == "ParenExpr":
        child, par = par, par["_par"]
    if par is None or par["kind"] not in ("CallExpr", "CXXMemberCallExpr", "CXXOperatorCallExpr"):
        return False
    kids = par.get("inner") or []
    if child not in kids[1:]:
        return False
    idx = kids.index(child) - 1
    callee = kids[0]
    while callee["kind"] in ("ImplicitCastExpr", "ParenExpr"):
        callee = callee["inner"][0]
    sig = (callee.get("type") or {}).get("qualType", "")
    params = split_params(sig)
    if params is None or idx >= len(params):
        return True                     # cannot tell: be conservative
    pt = params[idx]
    return "&" in pt and "const" not in pt.replace("&&", "&").split("&")[0]


def scan_tu(args):
    """returns (rows, edits, path) for one TU"""
    cmd_entry, apply_, oracle_addrs, hdr_pat = args
    hdr_re = re.compile(hdr_pat) if hdr_pat else None
    try:
        a = shlex.split(cmd_entry["command"])
    except ValueError:
        return [], {}, None, "unparsable command"
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
    if not r.stdout or r.returncode != 0:      # a TU the build compiles must parse: errors (stale PCH, missing header ...) make the AST partial -> a silent undercount
        errs = [l for l in r.stderr.decode("utf8", "replace").splitlines() if "error" in l] or r.stderr.decode("utf8", "replace").strip().splitlines() or ["clang produced no AST"]
        return [], {}, None, errs[0][:300]
    root = json.loads(r.stdout)
    annotate(root, Loc())
    path = cmd_entry["file"]
    tu = TU(path, root)
    mainnodes = [n for n in walk(root) if n.get("_p") and (n["_p"][0] == path or (hdr_re and hdr_re.search(n["_p"][0] or "")))]
    fileedits = {}      # file -> [edits]  (headers outside the PCH are edited too; the caller applies them once per file)

    def add_edits(f, es):
        fileedits.setdefault(f, []).extend(es)

    def owner(n):
        o = n["_par"]
        best = None
        while o is not None:
            if o["kind"] in FN_DECLS and o.get("_p"):
                best = o
                break
            o = o["_par"]
        if best is None:
            return "<global>", None
        p = best["_p"]
        head = srcof(p[0])[p[2]:p[2] + 400].decode("utf8", "replace")
        m = re.search(r"([\w:~<>]+::)?" + re.escape(best.get("name", "")) + r"\s*\(", head)
        nm = (m.group(0)[:-1].strip() if m else best.get("name", "?"))
        S = srcof(p[0])
        line_start = S.rfind(b"\n", 0, p[2])
        prev_start = S.rfind(b"\n", 0, max(line_start, 0))
        prev = S[prev_start + 1:line_start].decode("utf8", "replace")
        am = re.search(r"//\s*(0x[0-9A-Fa-f]{5,8})", prev)
        return nm, (am.group(1).upper().replace("0X", "0x") if am else None)

    # ---- double locals: clean = float-valued after the rewrite ------------------------------------------------------------------------------------------
    for n in mainnodes:
        if n["kind"] == "VarDecl" and is_dbl(n.get("type")) and n.get("storageClass") != "extern":
            tu.locals[n["id"]] = {"name": n.get("name"), "node": n, "defs": [], "clean": True}
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
        elif k == "UnaryOperator" and n.get("opcode") in ("&", "++", "--"):
            l = skip_wrappers(n["inner"][0])
            if l["kind"] == "DeclRefExpr" and l.get("referencedDecl", {}).get("id") in tu.locals and n.get("opcode") == "&":
                tu.locals[l["referencedDecl"]["id"]]["clean"] = False
        elif k == "CompoundAssignOperator":
            l = skip_wrappers(n["inner"][0])
            if l["kind"] == "DeclRefExpr" and l.get("referencedDecl", {}).get("id") in tu.locals:
                tu.locals[l["referencedDecl"]["id"]]["defs"].append(n["inner"][1])
        elif k == "DeclStmt":
            ids = [c["id"] for c in n.get("inner", []) if c["kind"] == "VarDecl" and c["id"] in tu.locals]
            if len(ids) > 1:
                tu.groups[n["id"]] = ids
        elif k == "DeclRefExpr" and is_dbl(n.get("type")):
            # a double local bound to a NON-CONST reference parameter (lifted x87 code: AddrOf(x) -> uintptr_t, std::ref, out-params ...): its address escapes, an 8 byte
            # object is expected there -> never retype it (FLOOP2-REV defects 1-3: Group24b sin/cos, AcosCrt out, Cover res)
            rid = n.get("referencedDecl", {}).get("id")
            if rid in tu.locals and mutable_ref_use(n):
                tu.locals[rid]["clean"] = False
                tu.locals[rid]["why"] = "address escapes (non-const reference argument)"
    # textual guard: `qword ptr [name]` (MSVC __asm blocks) and AddrOf(name) (lifted .inc) name the local directly
    names = set()
    for f in {n["_p"][0] for n in mainnodes if n["kind"] == "VarDecl" and n["id"] in tu.locals}:
        t = srcof(f).decode("utf8", "replace")
        names |= set(re.findall(r"qword\s+ptr\s*\[\s*(\w+)", t)) | set(re.findall(r"\bAddrOf\(\s*(\w+)\s*\)", t))
    for lid, L in tu.locals.items():
        if L["name"] in names:
            L["clean"] = False
            L["why"] = "named in __asm qword ptr / AddrOf"
    changed = True
    while changed:
        changed = False
        for lid, L in tu.locals.items():
            if not L["clean"]:
                continue
            for d in L["defs"]:
                if tu.proc(d, None)[0] not in CLEAN:
                    L["clean"] = False
                    changed = True
                    break
        for gid, ids in tu.groups.items():
            if any(not tu.locals[i]["clean"] for i in ids):
                for i in ids:
                    if tu.locals[i]["clean"]:
                        tu.locals[i]["clean"] = False
                        changed = True

    def parent_is_arith(n):
        p = n["_par"]
        while p is not None:
            k = p["kind"]
            if k in ("ParenExpr", "ImplicitCastExpr", "ExprWithCleanups", "ConstantExpr", "MaterializeTemporaryExpr") or (k in CAST_KINDS and p.get("castKind") in ("NoOp", "FloatingCast") and is_dbl(p.get("type"))):
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
            if k == "CompoundAssignOperator" and is_dbl(p.get("computeResultType")):
                return True
            if k == "CallExpr" and callee_name(p) in PASS_FNS and is_dbl(p.get("type")):
                return True
            return False
        return False

    rows = []

    def vname(kind):
        return {"D": "CHECK-D", "X": "CHECK-X", "M": "CHECK-M", "LITD": "CHECK-L"}.get(kind, "AUTO")

    def leafkinds(n):
        """leaf kinds of all double operands in the chain (for the report)"""
        res = set()
        for c in walk(n):
            if c is n or c.get("kind") not in ("DeclRefExpr", "FloatingLiteral", "CallExpr", "MemberExpr", "CXXMemberCallExpr", "ArraySubscriptExpr"):
                continue
            if not is_dbl(c.get("type")):
                continue
            kk = tu.proc(c, None)[0] if c["kind"] != "FloatingLiteral" else ("LITD" if not float_exact(c.get("value", "x")) else "lit")
            if kk in ("D", "X", "M", "LITD"):
                res.add(kk + (":" + (c.get("referencedDecl", {}).get("name") or c.get("name") or "") if c["kind"] == "DeclRefExpr" else ""))
        return res

    for n in mainnodes:
        k = n["kind"]
        isarith = (k == "BinaryOperator" and n.get("opcode") in ARITH and is_dbl(n.get("type"))) or \
                  (k == "CallExpr" and is_dbl(n.get("type")) and callee_name(n) in PASS_FNS and callee_name(n) not in ("min", "max", "clamp") and len(n["inner"]) == 2)
        iscomp = k == "CompoundAssignOperator" and is_dbl(n.get("computeResultType"))
        if not (isarith or iscomp) or parent_is_arith(n) or pc53_line(n["_p"]):
            continue
        em = []
        if iscomp:
            lhs = skip_wrappers(n["inner"][0])
            lclean = lhs["kind"] == "DeclRefExpr" and lhs.get("referencedDecl", {}).get("id") in tu.locals and tu.locals[lhs["referencedDecl"]["id"]]["clean"]
            kr, er = tu.proc(n["inner"][1], em)
            if lclean and kr in CLEAN:
                em.extend(er)
                verdict = "AUTO"
            else:
                verdict = "CHECK-C"
        else:
            kind, oe = tu.proc(n, em)
            verdict = "CHECK-D" if kind == "D" else "AUTO"
        dirtywrap = any(e[4] == "X87R(" for e in em)
        macro = any(e[0] == "MACRO" for e in em)
        lk = leafkinds(n)
        if verdict == "AUTO" and dirtywrap:
            verdict = "WRAP"
        elif verdict == "AUTO" and any(e[0] == "DONE" for e in em):
            verdict = "DONE"          # an earlier run already wrapped this chain
        if macro:
            verdict = "SKIP-MACRO"
        p = n["_p"]
        txt = " ".join(srcof(p[0])[p[2]:p[3]].decode("utf8", "replace").split())
        fn, addr = owner(n)
        orc = "?" if not addr else ("Y" if addr.lower() in oracle_addrs else "N")
        nops = sum(1 for c in walk(n) if c.get("kind") == "BinaryOperator" and c.get("opcode") in ARITH and is_dbl(c.get("type")))
        rows.append((f"{os.path.relpath(p[0], REPO)}:{p[1]}", verdict, nops, txt[:140], ",".join(sorted(lk)), fn, orc, ""))
        if verdict in ("AUTO", "WRAP", "CHECK-C") and apply_:
            add_edits(p[0], [e for e in em if e[0] not in ("MACRO", "DONE")])

    # clean locals: the declared type + the edits of their definitions
    for lid, L in tu.locals.items():
        n = L["node"]
        p = n.get("_p")
        if not p or pc53_line(p):
            continue
        fn, addr = owner(n)
        txt = " ".join(srcof(p[0])[p[2]:p[3]].decode("utf8", "replace").split())
        orc = "?" if not addr else ("Y" if addr.lower() in oracle_addrs else "N")
        verdict = "LOCAL-AUTO" if L["clean"] else "LOCAL-CHECK-D"
        nloc = n.get("loc", {})
        off = nloc.get("offset") if "spellingLoc" not in nloc else None
        if L["clean"] and (p[4] or off is None):
            verdict = "LOCAL-SKIP-MACRO"
        rows.append((f"{os.path.relpath(p[0], REPO)}:{p[1]}", verdict, 0, txt[:140], "double local " + str(L["name"]), fn, orc, L.get("why", "")))
        if L["clean"] and apply_ and verdict == "LOCAL-AUTO":
            head = srcof(p[0])[p[2]:off].decode("utf8", "replace")
            m = re.search(r"\bdouble\b", head)
            if m:
                add_edits(p[0], [(p[2] + m.start(), p[2] + m.end(), 0, 2, "x87d")])
            for d in L["defs"]:
                em = []
                kd, od = tu.proc(d, em)
                if not any(e[0] == "MACRO" for e in em):
                    add_edits(p[0], [e for e in em + (od if kd in CLEAN else []) if e[0] != "DONE"])

    # ---- math calls whose overload CHANGES by the rewrite: pow(double, double) -> powf(float, float) is a different (less precise) function than the CRT's double pow -----
    # the argument is handed over as a double again: X87W(expr) (Base.h: bare tokens on Win32, ((double)(expr)) portable)
    for n in mainnodes:
        if n["kind"] not in ("CallExpr",) or not is_dbl(n.get("type")):
            continue
        nm = callee_name(n)
        if nm not in OVERLOAD_FNS or "x87" in tu.text(n["inner"][0]) or n["_p"][4]:
            continue
        args = n["inner"][1:]
        now, changed = [], []
        for a in args:
            base = skip_wrappers(a)
            if a.get("kind") == "ImplicitCastExpr" and a.get("castKind") == "IntegralToFloating":
                now.append("int")
            elif not is_dbl(a.get("type")):
                now.append("float" if is_flt(a.get("type")) else "int")
            elif base["kind"] == "DeclRefExpr" and base.get("referencedDecl", {}).get("id") in tu.locals and tu.locals[base["referencedDecl"]["id"]]["clean"]:
                now.append("float")
                changed.append(base)
            elif (base["kind"] == "BinaryOperator" and base.get("opcode") in ARITH and is_dbl(base.get("type"))) or \
                 (base["kind"] == "CallExpr" and is_dbl(base.get("type")) and callee_name(base) in PASS_FNS and callee_name(base) not in ("min", "max", "clamp") and len(base["inner"]) == 2):
                em2 = []
                tu.proc(base, em2)
                if any(e[0] == "MACRO" for e in em2):
                    now.append("double")
                else:
                    now.append("float")
                    changed.append(base)
            else:
                now.append("double")
        if changed and "double" not in now and "int" not in now:
            p = n["_p"]
            txt = " ".join(srcof(p[0])[p[2]:p[3]].decode("utf8", "replace").split())
            fn, addr = owner(n)
            orc = "?" if not addr else ("Y" if addr.lower() in oracle_addrs else "N")
            rows.append((f"{os.path.relpath(p[0], REPO)}:{p[1]}", "MATH-OVERLOAD", 0, txt[:140], nm + ": " + ",".join(now), fn, orc, "X87W"))
            if apply_:
                for b in changed:
                    bp = b["_p"]
                    if not bp[4] and srcof(bp[0])[max(bp[2] - 5, 0):bp[2]] != b"X87W(":
                        add_edits(bp[0], [(bp[2], bp[2], -(10 ** 6), 0, "X87W("), (bp[3], bp[3], -(10 ** 6), 1, ")")])

    for src in tu.wide:
        p = src["_p"]
        txt = " ".join(srcof(p[0])[p[2]:p[3]].decode("utf8", "replace").split())
        fn, addr = owner(src)
        orc = "?" if not addr else ("Y" if addr.lower() in oracle_addrs else "N")
        rows.append((f"{os.path.relpath(p[0], REPO)}:{p[1]}", "INT-WIDE", 0, txt[:140], qt(src.get("type")) + " -> x87d (exact in the exe's fild, rounded to 24 bit here when > 2^24)", fn, orc, "review"))
    return rows, fileedits, path, None


def apply_edits(path, edits):
    """edits: (begin, end, depth_key, phase, text). Applied from the end of the file; for equal offsets the order is chosen so that nested wraps nest properly."""
    src = open(path, "rb").read()
    seen = set()
    uniq = []
    for e in edits:
        if e not in seen:
            seen.add(e)
            uniq.append(e)
    # replacement edits (b<e) must not overlap each other: drop a replacement that lies inside another one (keep the outer)
    reps = [e for e in uniq if e[0] != e[1]]
    ins = [e for e in uniq if e[0] == e[1]]
    keep = []
    for e in sorted(reps, key=lambda x: (x[0], -x[1])):
        if keep and e[0] < keep[-1][1]:
            continue
        keep.append(e)
    allx = keep + ins
    # application order (descending position): at one offset apply: closes (phase 1) of shallower nodes first ... see keys
    def key(e):
        b, en, d, ph, txt = e
        # larger b first; at equal b: replacements (en>b) first, then closing insertions (phase 1: apply outer (small depth) first so inner ends up left of it),
        # then opening insertions (phase 0: apply inner (large depth) first so outer ends up left)
        if en > b:
            return (b, 2, 0)
        if ph == 1:
            return (b, 1, -d)
        return (b, 0, d)
    n = 0
    for e in sorted(allx, key=key, reverse=True):
        b, en, d, ph, txt = e
        src = src[:b] + txt.encode() + src[en:]
        n += 1
    open(path, "wb").write(src)
    return n



def pch_preflight(cc, rebuild):
    """The AST dump of every TU is parsed against the precompiled header. A PCH older than a header in its closure (Base.h, Vector.h ...) makes clang fail on EVERY TU with
    a 'file has been modified since the precompiled header was built' error and the scan used to report 0 rows, which looked like "idempotent" (FLOOP2-REV). Ask ninja (the
    PCH has a depfile): refuse to run, or with --rebuild-pch rebuild it first."""
    m = None
    for e in cc:
        m = re.search(r"-include-pch\s+-Xclang\s+(\S+)", e["command"]) or re.search(r"-include-pch\s+(\S+)", e["command"])
        if m:
            break
    if not m:
        return
    pch = m.group(1)
    bdir = e["directory"]
    ninja = shutil.which("ninja") or "ninja"
    rel = os.path.relpath(pch, bdir)
    r = subprocess.run([ninja, "-C", bdir, "-n", rel], capture_output=True, text=True)
    if "no work to do" in (r.stdout + r.stderr):
        return
    if r.returncode != 0 and not r.stdout.strip():
        sys.stderr.write("pc24_scan: cannot ask ninja about the PCH (%s): %s\n" % (rel, r.stderr.strip()[:300]))
        sys.exit(3)
    if not rebuild:
        sys.stderr.write("pc24_scan: the PCH %s is OLDER than a header in its closure (ninja would rebuild it): every TU would fail to parse. Rebuild it (`ninja -C %s %s`) or pass --rebuild-pch.\n" % (rel, bdir, rel))
        sys.exit(3)
    sys.stderr.write("pc24_scan: rebuilding the stale PCH %s ...\n" % rel)
    if subprocess.run([ninja, "-C", bdir, rel]).returncode != 0:
        sys.exit(3)


HDR_SKIP = re.compile(r"(X87|Fp\.h|x87_lifted|X87Lift|X87Soft)")


def census_headers():
    """Header-only code (inline functions / templates in game_sa headers) lives in the PCH and is invisible to the AST dump: list its `double` mentions textually
    (FLOOP3: AnimBlendNode.h was a whole header of PC=53 spellings and caused the first divergences of the Win32 trace). Rows: HEADER-DOUBLE, review by hand."""
    rows = []
    for root in ("source/game_sa", "source/extensions", "source/standalone/rw"):
        for dp, _, fs in os.walk(os.path.join(REPO, root)):
            for f in sorted(fs):
                if not f.endswith((".h", ".hpp")) or HDR_SKIP.search(os.path.join(dp, f)):
                    continue
                path = os.path.join(dp, f)
                for i, line in enumerate(open(path, errors="replace"), 1):
                    code = line.split("//")[0]
                    if re.search(r"\bdouble\b", code) and "x87d" not in code.replace("(double)", ""):
                        rows.append((os.path.relpath(path, REPO) + ":" + str(i), "HEADER-DOUBLE", 0, " ".join(code.split())[:140], "", "", "?", "census"))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--census-headers", action="store_true", help="textual census of `double` in game_sa / extensions / standalone-rw headers (PCH-hidden inline code); no AST")
    ap.add_argument("--cc", default=os.path.join(REPO, "build/PortableMacRR/compile_commands.json"))
    ap.add_argument("--only", action="append", default=[])
    ap.add_argument("--exclude", action="append", default=[], help="regex on the TU path: skip")
    ap.add_argument("--headers", default="", help="regex on header paths that are NOT in the PCH (e.g. 'source/standalone/rw/.*\\.h$'): their double arithmetic is scanned / rewritten too")
    ap.add_argument("--out")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--summary", action="store_true")
    ap.add_argument("--rebuild-pch", action="store_true", help="rebuild a stale PCH instead of refusing to run")
    ap.add_argument("--no-pch-check", action="store_true")
    ap.add_argument("-j", type=int, default=8)
    ap.add_argument("--tests", default=os.path.join(REPO, "tests/standalone"))
    a = ap.parse_args()
    if a.census_headers:
        w = open(a.out, "w") if a.out else sys.stdout
        for r in census_headers():
            w.write("\t".join(str(x) for x in r) + "\n")
        return
    cc = json.load(open(a.cc))
    if not a.no_pch_check:
        pch_preflight(cc, a.rebuild_pch)
    pats = [re.compile(p) for p in (a.only or [r"/source/game_sa/.*\.cpp$"])]
    ents = [e for e in cc if any(p.search(e["file"]) for p in pats) and not any(re.search(x, e["file"]) for x in a.exclude)]
    oracle = set()
    for dp, _, fs in os.walk(a.tests):
        for f in fs:
            if f.endswith((".cpp", ".inc", ".h")):
                oracle |= {m.lower() for m in re.findall(r"0x[0-9A-Fa-f]{5,8}", open(os.path.join(dp, f), errors="replace").read())}
    allrows = []
    failed = []
    byfile = {}
    with cf.ProcessPoolExecutor(a.j) as ex:
        for ent, (rows, fedits, path, err) in zip(ents, ex.map(scan_tu, [(e, a.apply, oracle, a.headers) for e in ents])):
            if err:
                failed.append((ent["file"], err))
                continue
            allrows += rows
            for f, es in fedits.items():
                byfile.setdefault(f, set()).update(es)
    if failed:
        sys.stderr.write("pc24_scan: %d of %d TUs produced no AST (clang failed); first: %s -> %s\n" % (len(failed), len(ents), os.path.relpath(failed[0][0], REPO), failed[0][1]))
        if len(failed) * 50 > len(ents):
            sys.stderr.write("pc24_scan: more than 2% of the TUs failed: the report is NOT meaningful (stale PCH? broken build tree?) - no edits applied\n")
            sys.exit(4)
    nedits = 0
    if a.apply:
        for f, es in byfile.items():
            if es:
                nedits += apply_edits(f, list(es))
    w = open(a.out, "w") if a.out else sys.stdout
    w.write("file:line\tverdict\tops\texpression\tleaves\tfunction\toracle\tnote\n")
    seen = set()
    for r in allrows:
        key = tuple(str(x) for x in r)
        if key in seen:      # a header scanned from several TUs
            continue
        seen.add(key)
        w.write("\t".join(key) + "\n")
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
    if failed:
        sys.exit(5 if len(failed) else 0)


if __name__ == "__main__":
    main()
