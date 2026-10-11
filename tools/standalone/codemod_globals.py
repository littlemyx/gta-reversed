#!/usr/bin/env python3
"""codemod_globals.py - rewrite  `[static] [inline] auto& NAME = StaticRef<T>(0xADDR);`  into  `NOTSA_GLOBAL(NAME, 0xADDR, (T), <initialiser>)`
(.notes/DETACH_DATA_PLAN.md 3.3 step 5, slice A2). The macro family lives in source/Base.h; the initialiser is the exe's value of the global, produced by
globals_emit.py from the post-_initterm data image; the alias / owner / member decisions come from .notes/aliases.json (slice A4). In the default build
(address mode) every rewritten line expands to exactly the old declaration (proved with tools/standalone/obj_code_sig.py).

  python3 -I tools/standalone/codemod_globals.py [--apply] [--image-dir DIR] [--tsv FILE] [--aliases FILE] FILE...    (dry run unless --apply; prints a status table)
  python3 -I tools/standalone/codemod_globals.py --check FILE...      list the `StaticRef` sites that are still address-mode and why
  python3 -I tools/standalone/codemod_globals.py --check-converted [--boot-dump FILE] [--out FILE] [--fix [--apply]]
        D-FIX1 gate: every address-mode use (StaticRef / ScopedStaticRef / raw cast / any hex literal) of a CONVERTED global's address OR of any address inside its
        extent, anywhere in source/ (all batches, headers, standalone/), is a STRAY (a detached build reads the old image address -> poisoned/stale data).
        Exit status 1 when strays exist. --fix converts alias sites listed in aliases.json (decl / expr) of the stray's file via the normal path.
  python3 -I tools/standalone/codemod_globals.py --gen-shared [--apply]   (re)generate source/game_sa/DetachedShared.h (aliases.json: globals with owner.scope "shared")

Forms (scope from the row of DETACH_GLOBALS.tsv, header/.cpp from the file name):
  class member / .cpp namespace scope  `static inline NOTSA_GLOBAL(n, a, (T), init);`           (specifiers stay as written)
  header namespace scope               `NOTSA_GLOBAL_HDR(n, a, (T), init);`                  (`static`/`inline` dropped: ONE definition, `inline`)
                                       `NOTSA_GLOBAL_HDR_EXT(...)` when the old text was `inline auto&` without `static` (address mode keeps `inline`)
  function scope                       `NOTSA_GLOBAL_LOCAL(n, a, (T), init);`   (`NOTSA_GLOBAL_LOCAL_NS` when the old local `auto&` had no `static`)
  class member, category c, initialiser > --inline-max chars or a type that mentions the enclosing class, with a sibling <stem>.cpp:
                                        header `static NOTSA_GLOBAL_DECL(Cls, n, a, (T));` + `NOTSA_GLOBAL_DEF(Cls, n, a, (T), init);` appended at the END of the .cpp
  ScopedStaticRef variable             `NOTSA_SCOPED_GLOBAL(v, varA, flagsA, mask, (T), initVal);`   (the macro says `static` itself in both modes; the call-site `static` is dropped; detached: `static T v = initVal;`, the flag word is dropped)
  aliases.json alias declaration       `NOTSA_GLOBAL_ALIAS(n, a, (T), Owner);` ; cast alias `reinterpret_cast<T&>(Owner)`; member `*reinterpret_cast<T*>(reinterpret_cast<uint8*>(&Owner) + off)`
  aliases.json expression alias        `NOTSA_GLOBAL_EXPR(a, (T), Owner)` replaces `StaticRef<T>(a)`
  extern declaration in the sibling header (`extern T& name;` of a namespace-scope .cpp definition)
                                        `NOTSA_GLOBAL_EXTERN(name, (T));`  (address: the old `extern T& name;`, detached: `extern T name;`)
  synthetic owners                     scope "tu": `namespace { NOTSA_GLOBAL_SYNTH(k, a, (T), init); }` after the last #include of the home .cpp; scope "shared": DetachedShared.h
Everything that inserts or removes lines is followed by a `#line N` directive, so __LINE__ (baked into RH_Scoped* / NOTSA_UNREACHABLE sites) never moves.
Skipped (reported, the line stays): category d (objects with constructors: phase C), ctor_audit NONZERO/VPTR, views / virtual / retype / readdress / extents
decisions (fix the declaration first), owners with link/init_in_source/retype/grow, access:"private" aliases, rows with a TODO in the initialiser (string / data /
unresolved code pointers), opaque types, undecided duplicates/conflicts/overlaps, addresses not in the table, and `--skip-addr`. Idempotent: converted lines no longer match.
"""
import argparse
import difflib
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import gen_globals as gg      # noqa: E402  (scanner helpers: strip_source, find_template_end)
import globals_emit as ge     # noqa: E402

REPO = ge.REPO
RE_HEAD = re.compile(r"(?P<pre>(?:\b(?:static|inline|constexpr)\s+)*)\bauto\s*&\s*(?P<name>[A-Za-z_]\w*)\s*=\s*(?P<fn>StaticRef|ScopedStaticRef)\s*<")
RE_TAIL = re.compile(r"\s*\(\s*(?P<addr>0[xX][0-9A-Fa-f]+)\s*\)\s*;")
RE_ANY = re.compile(r"\bStaticRef\s*<")
RE_ADDR = re.compile(r"\s*\(\s*(?P<addr>0[xX][0-9A-Fa-f]+)\s*\)")


def hazard_sets():
    h = {}
    notes = REPO / ".notes"
    for fn, col, why in (("DETACH_duplicates.tsv", ["addr"], "duplicate"), ("DETACH_conflicts.tsv", ["addr"], "type conflict"),
                         ("DETACH_overlaps.tsv", ["outer_addr", "inner_addr"], "overlapping extent")):
        p = notes / fn
        if not p.exists():
            continue
        lines = p.read_text().splitlines()
        hdr = lines[0].split("\t")
        for line in lines[1:]:
            f = line.split("\t")
            for c in col:
                h.setdefault(int(f[hdr.index(c)], 16), why)
    return h


class Aliases:
    """.notes/aliases.json (slice A4) indexed for the codemod"""

    def __init__(self, path, rows_by_addr):
        self.ok = Path(path).exists()
        self.owner_site, self.alias_decl, self.alias_expr, self.member = {}, {}, {}, {}
        self.addr_manual, self.site_manual, self.synth, self.owner_expr = {}, {}, {}, {}
        self.member_owners = set()
        self.owner_decl_file = {}
        self.shared_header = "source/game_sa/DetachedShared.h"
        if not self.ok:
            return
        d = json.loads(Path(path).read_text())
        self.shared_header = d.get("shared_header", self.shared_header)
        short = lambda n: n.split("::")[-1]
        for a, g in d["globals"].items():
            A = int(a, 16)
            o = g["owner"]
            if "synthetic" in o:
                nm = ("notsa::shared::" if o["scope"] == "shared" else "") + o["synthetic"]
                self.synth[A] = dict(name=o["synthetic"], home=o["home"], scope=o["scope"], type=g["type"], size=g["size"])
                self.owner_expr[A] = nm
            else:
                self.owner_expr[A] = o["name"]
                self.owner_decl_file[A] = o["file"]
                key = (o["file"], A, short(o["name"]))
                if any(k in o for k in ("link", "init_in_source", "retype", "grow")):
                    self.site_manual[key] = "owner needs manual work (%s)" % ", ".join("%s=%s" % (k, o[k]) for k in ("link", "init_in_source", "retype", "grow") if k in o)
                else:
                    self.owner_site[key] = g
            for al in g["aliases"]:
                if al.get("unused"):
                    self.site_manual[(al["file"], A, short(al["name"]))] = "alias is unused: delete the declaration"
                elif al.get("access") == "private":
                    self.site_manual[(al["file"], A, short(al["name"]) if al["kind"] == "decl" else None)] = "alias of a private class static: needs friend/accessor"
                elif al["kind"] == "decl":
                    self.alias_decl[(al["file"], A, short(al["name"]))] = (A, al)
                else:
                    self.alias_expr.setdefault((al["file"], A), []).append((A, al))
        for m in d["members"]:
            A = int(m["addr"], 16)
            self.member_owners.add(int(m["owner"], 16))
            for st in m["sites"]:
                if st["kind"] == "decl":
                    self.member[(st["file"], A, short(st["name"]))] = m
                else:
                    self.site_manual[(st["file"], A, None)] = "member of %s used as an expression" % m["owner"]
        for v in d["views"]:
            self.site_manual[(v["site"]["file"], int(v["addr"], 16), short(v["site"]["name"]))] = "view (own zeroed storage + verifier extent): manual"
        for v in d["virtual"]:
            self.addr_manual[int(v["addr"], 16)] = "virtual base address (rewrite the accessor): manual"
        # retype / readdress decisions are only open while the repo still has the OLD declaration (agent FIXADDR fixed several in source): obsolete ones are ignored
        def txt(f):
            try:
                return (REPO / f).read_text(errors="replace")
            except OSError:
                return ""

        def still_old(f, addr_hex, old_type=None):
            for m in re.finditer(r"StaticRef\s*<([^;]*?)>\s*\(\s*%s\s*\)" % re.escape(addr_hex), txt(f), re.I):
                if old_type is None or re.sub(r"\s+", "", old_type) in re.sub(r"\s+", "", m.group(1)):
                    return True
            return False

        FIXED = {0x96A8B0, 0x96A8B1, 0xBAB378, 0xBAB37C, 0xC17824, 0xB6B98C}   # fixed in source by agent FIXADDR (commits 59803c65 6e9e56a6 37577efa ae300f56)
        obsolete = set()
        for r in d["retype"]:
            if int(r["addr"], 16) not in FIXED or still_old(r["file"], r["addr"], r["frm"]):
                self.addr_manual.setdefault(int(r["addr"], 16), "retype first: %s -> %s" % (r["frm"], r["to"]))
            else:
                obsolete.add(int(r["addr"], 16))
        for r in d["readdress"]:
            if int(r["frm"], 16) not in FIXED or still_old(r["site"]["file"], r["frm"], r["site"]["type"].rstrip("*").strip()):
                self.addr_manual.setdefault(int(r["frm"], 16), "readdress to %s first" % r["to"])
            else:
                obsolete.add(int(r["frm"], 16))
        self.obsolete = obsolete
        for a, e in d["extents"].items():
            if e["kind"] not in ("keep",) and int(a, 16) not in obsolete:
                self.addr_manual.setdefault(int(a, 16), "extent %s: %s B -> %s B (fix the declared type first)" % (e["kind"], e["declared"], e["true_size"]))
        for a, v in d["ctor_audit"].items():
            if v in ("NONZERO", "VPTR"):
                self.addr_manual.setdefault(int(a, 16), "ctor %s: needs zero-init / wrapper (phase C)" % v)


def _owner_file(self, A, rows_by_addr):
    """repo-relative file that declares the owner of address A (None for synthetics: DetachedShared.h is handled separately)"""
    if A in self.synth:
        return None
    f = self.owner_decl_file.get(A)
    if f:
        return f
    r = rows_by_addr.get(A)
    return r["owner"].rsplit(":", 1)[0] if r else None


Aliases.owner_file = _owner_file


def wrap_init(init, indent, width=118):
    if len(init) + indent <= width or not init.startswith("{ "):
        return init
    body = init[2:-2]
    items = ge.split_top(body)
    lines, cur = [], ""
    for it in items:
        piece = it + ","
        if len(cur) + len(piece) + 1 + indent > width and cur:
            lines.append(cur)
            cur = ""
        cur += (" " if cur else "") + piece
    lines.append(cur.rstrip(","))
    pad = " " * (indent + 4)
    return "{\n" + "\n".join(pad + l for l in lines) + "\n" + " " * indent + "}"


def last_include_end(text):
    last = 0
    for m in re.finditer(r"(?m)^[ \t]*#[ \t]*include[^\n]*\n", text):
        last = m.end()
    return last


def split_args(s):
    return ge.split_top(s, ",")


def apply_edits(text, edits):
    """edits: (start, end, new). Whenever an edit changes the number of lines, a `#line N` directive restores the numbering of the rest."""
    out = text
    for s, e, n in sorted(edits, reverse=True):
        old = text[s:e]
        delta = n.count("\n") - old.count("\n")
        if delta:
            endline = text.count("\n", 0, e) + 1
            eol = text.find("\n", e)
            rest = text[e:eol] if eol >= 0 else ""
            if not n.endswith("\n") and eol >= 0 and (not rest.strip() or rest.strip().startswith("//")) and "\n" not in rest:
                # keep the original trailing comment on the last line of the replacement; the `#line` goes after that line (no lone comment line)
                out = out[:s] + n + rest + "\n#line %d\n" % (endline + 1) + out[eol + 1:]
                continue
            n += "\n#line %d\n" % endline if not n.endswith("\n") else "#line %d\n" % endline
        out = out[:s] + n + out[e:]
    return out


RE_EXTERN_REF = r"^([ \t]*)extern\s+([^;()\[\]]+?)\s*&\s*%s\s*;"


def rewrite_externs(cpp, externs, args, report):
    """A namespace-scope definition `T& name = StaticRef<T>(A)` is usually declared in the sibling header as `extern T& name;`. In a detached build the definition
    is a real object, so the declaration must not be a reference: `NOTSA_GLOBAL_EXTERN(name, (T));` (Base.h) is `extern T& name` in address mode (same text as before) and
    `extern T name` in detached mode. Single-line, line-count preserving. Looked up in <stem>.h / .hpp next to the .cpp; not found -> reported (the user must find the header)."""
    cands = [cpp.with_suffix(x) for x in (".h", ".hpp")]
    cands = [c for c in cands if c.exists()]
    htexts = {c: c.read_text() for c in cands}
    for name, addr in externs:
        done = False
        for c in cands:
            rx = re.compile(RE_EXTERN_REF % re.escape(name), re.M)
            ms = list(rx.finditer(htexts[c]))
            if len(ms) != 1:
                continue
            m = ms[0]
            htexts[c] = htexts[c][:m.start()] + "%sNOTSA_GLOBAL_EXTERN(%s, (%s));" % (m.group(1), name, m.group(2).strip()) + htexts[c][m.end():]
            report.append((str(c.relative_to(REPO)), name, addr, "EXTERN", "extern decl rewritten"))
            done = True
            break
        if not done:
            has = any(re.search(r"\bextern\b[^;\n]*\b%s\b" % re.escape(name), t) for t in htexts.values())
            if has:
                report.append((str(cpp.relative_to(REPO)), name, addr, "warn", "header declares it `extern` in a form the codemod does not rewrite (array / pointer-to-array / several): fix by hand to NOTSA_GLOBAL_EXTERN"))
    for c, t in htexts.items():
        if args.apply:
            c.write_text(t)
        else:
            old = c.read_text()
            rel = str(c.relative_to(REPO))
            sys.stdout.writelines(difflib.unified_diff(old.splitlines(True), t.splitlines(True), "a/" + rel, "b/" + rel, n=0))


def process(path, rows_by_addr, hazards, em, al, args, report):
    rel = str(path.relative_to(REPO))
    text = path.read_text()
    stripped = gg.strip_source(text)
    is_header = path.suffix in (".h", ".hpp", ".inl")
    edits, defs = [], []
    externs = []     # (unqualified name, addr) of namespace-scope .cpp definitions that a header may declare `extern T& name;`
    claimed = []     # spans handled as declarations (expression scan skips them)
    shared_used = False
    need_hdr = set()
    for m in RE_HEAD.finditer(stripped):
        i = gg.find_template_end(stripped, m.end())
        if i < 0:
            continue
        ttext = text[m.end():i - 1].strip()
        name, pre, fn = m.group("name"), m.group("pre"), m.group("fn")
        start = m.start()
        indent = len(text[text.rfind("\n", 0, start) + 1:start])
        if fn == "ScopedStaticRef":
            j = stripped.find("(", i)
            depth, k = 0, j
            while k < len(stripped):
                if stripped[k] == "(":
                    depth += 1
                elif stripped[k] == ")":
                    depth -= 1
                    if depth == 0:
                        break
                k += 1
            mt = re.match(r"\s*;", stripped[k + 1:])
            args_ = split_args(text[j + 1:k])
            if not mt or len(args_) < 4:
                report.append((rel, name, "?", "skip", "ScopedStaticRef is not a plain declaration"))
                continue
            end = k + 1 + mt.end()
            claimed.append((start, end))
            new = "%sNOTSA_SCOPED_GLOBAL(%s, %s, %s, %s, (%s), %s);" % (re.sub(r"\bstatic\s+", "", pre), name, args_[0], args_[1], args_[2], ttext, ",".join(text[j + 1:k].split(",")[3:]).strip() if False else ", ".join(args_[3:]))
            edits.append((start, end, new))
            report.append((rel, name, args_[0], "SCOPED", "static %s, flag word %s dropped" % (ttext, args_[1])))
            continue
        mt = RE_TAIL.match(stripped, i)
        if not mt:
            report.append((rel, name, "?", "skip", "StaticRef is not a plain `(0xADDR);` declaration"))
            continue
        addr = int(mt.group("addr"), 16)
        end = mt.end()
        claimed.append((start, end))
        row = rows_by_addr.get(addr)
        key = (rel, addr, name)

        def skip(why):
            report.append((rel, name, "0x%X" % addr, "skip", why))

        if (args.skip_addr and addr in args.skip_addr) or (args.only_addr and addr not in args.only_addr):
            skip("--skip-addr / --only-addr")
            continue
        if key in al.site_manual or (rel, addr, None) in al.site_manual:
            skip(al.site_manual.get(key) or al.site_manual[(rel, addr, None)])
            continue
        if addr in al.addr_manual:
            skip(al.addr_manual[addr])
            continue
        # alias / member declarations
        if key in al.alias_decl:
            A, a = al.alias_decl[key]
            owner = al.owner_expr[A]
            if A in al.synth and al.synth[A]["scope"] == "shared":
                shared_used = True
            expr = "reinterpret_cast<%s&>(%s)" % (ttext, owner) if a.get("cast") else owner
            need_hdr.add(al.owner_file(A, rows_by_addr))
            edits.append((start, end, "%sNOTSA_GLOBAL_ALIAS(%s, %s, (%s), %s);" % (pre, name, mt.group("addr"), ttext, expr)))
            report.append((rel, name, "0x%X" % addr, "ALIAS", "-> " + expr))
            continue
        if key in al.member:
            mm = al.member[key]
            owner = al.owner_expr.get(int(mm["owner"], 16)) or (rows_by_addr[int(mm["owner"], 16)]["name"] if int(mm["owner"], 16) in rows_by_addr else None)
            if not owner or owner.startswith("<"):
                skip("member of unnamed owner %s" % mm["owner"])
                continue
            expr = "*reinterpret_cast<%s*>(reinterpret_cast<uint8*>(&%s) + %d)" % (ttext, owner, mm["offset"])
            need_hdr.add(al.owner_file(int(mm["owner"], 16), rows_by_addr))
            edits.append((start, end, "%sNOTSA_GLOBAL_ALIAS(%s, %s, (%s), %s);" % (pre, name, mt.group("addr"), ttext, expr)))
            report.append((rel, name, "0x%X" % addr, "MEMBER", "-> " + expr))
            continue
        if row is None:
            skip("address not in DETACH_GLOBALS.tsv")
            continue
        if addr in hazards and key not in al.owner_site and addr not in al.member_owners:
            skip("%s: undecided in aliases.json" % hazards[addr])
            continue
        if int(row["n_sites"]) > 1 and key not in al.owner_site and addr not in al.member_owners:
            skip("%s sites, undecided" % row["n_sites"])
            continue
        if row["cat"] == "d":
            skip("category d (object with constructor: phase C)")
            continue
        if row["live_run"] == "0":
            skip("dead in the run build")
            continue
        g = al.owner_site.get(key)
        canon, size = (row["canonical_type"], row["sz"]) if not g else (None, None)
        if g:
            init, notes = em.init_typed(addr, g["type"], g["size"])
        else:
            init, notes = em.init(row)
        if init is None or notes:
            skip("needs manual initialiser: " + "; ".join(notes)[:150])
            continue
        scope, local = row["scope"], row["local_fn"]
        in_class = scope.startswith("c:")
        qname = row["name"]
        if not g and qname.split("::")[-1] != name:
            skip("name mismatch (%s vs %s)" % (qname, name))
            continue
        if in_class and re.search(r"template\s*<[^;{}]*>\s*(?:class|struct)\s+%s\b" % re.escape(qname.rsplit("::", 1)[0].split("::")[-1]), stripped[:start]):
            # B06 finding: a `static inline` member of a CLASS TEMPLATE is one variable PER INSTANTIATION (QuadTreeNode.h: only <void*>::InitPool ran, the other node types saw a null pool)
            skip("static member of a class template: one variable per instantiation; use a shared synthetic (NOTSA_GLOBAL_SYNTH at namespace scope) + NOTSA_GLOBAL_ALIAS in the class")
            continue
        use_def = False
        # a static member whose type mentions the enclosing class (`static std::array<CBulletInfo, 8> aBulletInfos` inside CBulletInfo): in detached mode the
        # in-class `static inline T x{}` needs a complete T, so declare it in the class and define it after the class (DECL + DEF in <stem>.cpp)
        self_typed = in_class and re.search(r"\b%s\b" % re.escape(qname.rsplit("::", 1)[0].split("::")[-1]), ttext) is not None
        if in_class and ("static" in pre) and (len(init) > args.inline_max or row["cat"] == "c" or self_typed):
            cpp = path.with_suffix(".cpp")
            if is_header and cpp.exists():
                use_def = True
        if use_def:
            cls = qname.rsplit("::", 1)[0]
            new = "static NOTSA_GLOBAL_DECL(%s, %s, %s, (%s));" % (cls, name, mt.group("addr"), ttext)
            defs.append((cls, name, "NOTSA_GLOBAL_DEF(%s, %s, %s, (%s), %s);\n" % (cls, name, mt.group("addr"), ttext, wrap_init(init, 0))))
            kind = "DECL+DEF"
        elif local:
            # `static auto&` -> LOCAL; a local `auto&` WITHOUT `static` -> LOCAL_NS (address mode keeps the non-static reference: identical object code)
            new = "%s(%s, %s, (%s), %s);" % ("NOTSA_GLOBAL_LOCAL" if "static" in pre else "NOTSA_GLOBAL_LOCAL_NS", name, mt.group("addr"), ttext, wrap_init(init, indent))
            kind = "LOCAL"
        elif is_header and not in_class:
            # `static inline auto&` -> HDR (internal linkage, address mode keeps `static inline`); `inline auto&` without `static` -> HDR_EXT (external linkage: the
            # compiler treats the reference differently, so address mode must keep the exact specifiers for identical object code)
            new = "%s(%s, %s, (%s), %s);" % ("NOTSA_GLOBAL_HDR" if "static" in pre else "NOTSA_GLOBAL_HDR_EXT", name, mt.group("addr"), ttext, wrap_init(init, indent))
            kind = "HDR"
        else:
            new = "%sNOTSA_GLOBAL(%s, %s, (%s), %s);" % (pre, name, mt.group("addr"), ttext, wrap_init(init, indent))
            kind = "GLOBAL"
            if not is_header and not in_class and "static" not in pre and not local:
                externs.append((name, "0x%X" % addr))
        edits.append((start, end, new))
        report.append((rel, qname, "0x%X" % addr, kind, "cat %s %s B  %s" % (row["cat"], row["size"], init[:60].replace("\n", " "))))
    # expression aliases: StaticRef<T>(A) -> NOTSA_GLOBAL_EXPR(A, (T), Owner)
    for m in RE_ANY.finditer(stripped):
        if any(s <= m.start() < e for s, e in claimed):
            continue
        i = gg.find_template_end(stripped, m.end())
        ma = RE_ADDR.match(stripped, i) if i > 0 else None
        if not ma:
            continue
        addr = int(ma.group("addr"), 16)
        lst = al.alias_expr.get((rel, addr))
        if not lst:
            continue
        if (rel, addr, None) in al.site_manual:
            report.append((rel, "expr", "0x%X" % addr, "skip", al.site_manual[(rel, addr, None)]))
            continue
        A, a = lst[0]
        ttext = text[m.end():i - 1].strip()
        owner = a.get("replace") or al.owner_expr[A]
        if A in al.synth and al.synth[A]["scope"] == "shared":
            shared_used = True
        if a.get("cast"):
            owner = "reinterpret_cast<%s&>(%s)" % (ttext, owner)
        need_hdr.add(al.owner_file(A, rows_by_addr))
        edits.append((m.start(), ma.end(), "NOTSA_GLOBAL_EXPR(%s, (%s), %s)" % (ma.group("addr"), ttext, owner)))
        report.append((rel, "expr", "0x%X" % addr, "EXPR", "-> " + owner))
    # synthetic owners that live in this file (scope tu)
    syn = [(A, s) for A, s in al.synth.items() if s["scope"] == "tu" and s["home"] == rel]
    pos = last_include_end(text)
    ins = ""
    for A, s in syn:
        if re.search(r"NOTSA_GLOBAL_SYNTH\(\s*%s\b" % re.escape(s["name"]), text):
            continue
        init, notes = em.init_typed(A, s["type"], s["size"])
        if init is None or notes:
            report.append((rel, s["name"], "0x%X" % A, "skip", "synthetic needs manual initialiser: " + "; ".join(notes)[:100]))
            continue
        ins += "NOTSA_GLOBAL_SYNTH(%s, 0x%X, (%s), %s);\n" % (s["name"], A, s["type"], wrap_init(init, 0))
        report.append((rel, s["name"], "0x%X" % A, "SYNTH-TU", s["type"]))
    inc = ""
    if shared_used and not re.search(r'#\s*include\s*[<"][^>"]*DetachedShared\.h', text):
        inc = '#include "game_sa/DetachedShared.h"\n'
        report.append((rel, "DetachedShared.h", "", "INCLUDE", "added"))
    for h in sorted(x for x in need_hdr if x and x.endswith((".h", ".hpp")) and x != rel):
        base = h.rsplit("/", 1)[-1]
        if not re.search(r'#\s*include\s*[<"][^>"]*%s[>"]' % re.escape(base), text) and not (is_header and False):
            inc += '#include "%s"\n' % h.replace("source/", "", 1)
            report.append((rel, base, "", "INCLUDE", "owner header added"))
    if ins or inc:
        blk = inc + ("namespace {\n" + ins + "} // namespace\n" if ins else "")
        edits.append((pos, pos, blk))
    if not edits:
        return
    out = apply_edits(text, edits)
    if args.apply:
        path.write_text(out)
    else:
        sys.stdout.writelines(difflib.unified_diff(text.splitlines(True), out.splitlines(True), "a/" + rel, "b/" + rel, n=0))
    if externs:
        rewrite_externs(path, externs, args, report)
    if defs:
        cpp = path.with_suffix(".cpp")
        ctext = cpp.read_text()
        add = ""
        for cls, name, d in defs:
            if re.search(r"NOTSA_GLOBAL_DEF\(\s*%s\s*,\s*%s\b" % (re.escape(cls), re.escape(name)), ctext):
                continue
            add += d
        if add:
            # appended at the END of the file: no line of existing code moves
            new = ctext.rstrip("\n") + "\n\n// Initial values of the globals declared with NOTSA_GLOBAL_DECL in %s (detached mode; ignored in address mode)\n%s" % (path.name, add)
            crel = str(cpp.relative_to(REPO))
            if args.apply:
                cpp.write_text(new)
            else:
                sys.stdout.writelines(difflib.unified_diff(ctext.splitlines(True), new.splitlines(True), "a/" + crel, "b/" + crel, n=0))


def gen_shared(al, em, args):
    lines = ["#pragma once", "", "// GENERATED by tools/standalone/codemod_globals.py --gen-shared from .notes/aliases.json (slice A2/A4): globals that several .cpp files declare at one",
             "// original address. Detached mode: the one definition; address mode: nothing (the alias sites keep their `StaticRef`). Do not edit by hand.",
             "", "namespace notsa::shared {"]
    for A, s in sorted(al.synth.items()):
        if s["scope"] != "shared":
            continue
        init, notes = em.init_typed(A, s["type"], s["size"])
        if init is None or notes:
            lines.append("// 0x%X %s: needs a manual initialiser: %s" % (A, s["name"], "; ".join(notes)))
            continue
        lines.append("NOTSA_GLOBAL_SYNTH(%s, 0x%X, (%s), %s);" % (s["name"], A, s["type"], wrap_init(init, 0)))
    lines += ["} // namespace notsa::shared", ""]
    out = "\n".join(lines)
    p = REPO / al.shared_header
    if args.apply:
        p.write_text(out)
        print("wrote", p)
    else:
        print(out)


def check(path, rows_by_addr, hazards, al, report):
    rel = str(path.relative_to(REPO))
    text = path.read_text()
    stripped = gg.strip_source(text)
    for m in re.finditer(r"\b(?:Scoped)?StaticRef\s*<", stripped):
        i = gg.find_template_end(stripped, m.end())
        ma = RE_ADDR.match(stripped, i) if i > 0 else None
        addr = int(ma.group("addr"), 16) if ma else None
        row = rows_by_addr.get(addr) if addr else None
        why = "expression use" if not ma else hazards.get(addr) or (("category " + row["cat"]) if row else "not in table")
        if addr in al.addr_manual:
            why = al.addr_manual[addr]
        line = stripped.count("\n", 0, m.start()) + 1
        report.append((rel, "line %d" % line, "0x%X" % addr if addr else "?", "left", why))


# ---------------------------------------------------------------------------------------------------------------------------------
# --check-converted (D-FIX1)
# ---------------------------------------------------------------------------------------------------------------------------------
DATA_LO, DATA_HI = 0x858000, 0xCB0000
RE_NOTSA_MACRO = re.compile(r"\bNOTSA_(?:SCOPED_)?GLOBAL(?:_[A-Z_]+)?\s*\(")
RE_HEXLIT = re.compile(r"(?<![\w.])0[xX]0*([0-9A-Fa-f]{6})[uUlL]*(?![\w.])")
# macro -> argument positions holding addresses ; owners define a converted global
MACRO_ADDR_ARGS = {"NOTSA_GLOBAL": (1,), "NOTSA_GLOBAL_HDR": (1,), "NOTSA_GLOBAL_HDR_EXT": (1,), "NOTSA_GLOBAL_LOCAL": (1,), "NOTSA_GLOBAL_LOCAL_NS": (1,),
                   "NOTSA_GLOBAL_SYNTH": (1,), "NOTSA_GLOBAL_ALIAS": (1,), "NOTSA_GLOBAL_LOCAL_REF": (1,), "NOTSA_GLOBAL_EXPR": (0,), "NOTSA_GLOBAL_DECL": (2,),
                   "NOTSA_GLOBAL_DEF": (2,), "NOTSA_GLOBAL_VERIFY": (0,), "NOTSA_SCOPED_GLOBAL": (1, 2)}
OWNER_MACROS = {"NOTSA_GLOBAL", "NOTSA_GLOBAL_HDR", "NOTSA_GLOBAL_HDR_EXT", "NOTSA_GLOBAL_LOCAL", "NOTSA_GLOBAL_LOCAL_NS", "NOTSA_GLOBAL_SYNTH", "NOTSA_GLOBAL_DECL",
                "NOTSA_GLOBAL_DEF", "NOTSA_GLOBAL_VERIFY", "NOTSA_SCOPED_GLOBAL"}
LOCAL_MACROS = {"NOTSA_GLOBAL_LOCAL", "NOTSA_GLOBAL_LOCAL_NS", "NOTSA_SCOPED_GLOBAL"}
SKIP_DIRS_INFRA = ("source/standalone/",)


def _macro_spans(stripped, text):
    """-> list of (start, end, macro name, [arg texts]) of every NOTSA_GLOBAL* invocation"""
    out = []
    for m in RE_NOTSA_MACRO.finditer(stripped):
        name = re.match(r"\w+", m.group(0)).group(0)
        if name not in MACRO_ADDR_ARGS and name != "NOTSA_GLOBAL_EXTERN":
            continue
        k, depth = m.end() - 1, 0
        while k < len(stripped):
            if stripped[k] == "(":
                depth += 1
            elif stripped[k] == ")":
                depth -= 1
                if depth == 0:
                    break
            k += 1
        out.append((m.start(), k + 1, name, split_args(text[m.end():k])))
    return out


def _site_kind(stripped, pos):
    pre = stripped[max(0, pos - 90):pos]
    if re.search(r"ScopedStaticRef\s*<[^;]*\(\s*$", pre):
        return "ScopedStaticRef"
    if re.search(r"StaticRef\s*<[^;]*\(\s*$", pre) or re.search(r"StaticRef\s*<[^;]*\(\s*0[xX]\w+\s*[+\-]\s*$", pre):
        return "StaticRef"
    if re.search(r"(reinterpret_cast\s*<[^;]*>\s*\(\s*$)|(\(\s*[\w:\s\*&<>\[\]]+\*\s*\)\s*$)|(\*\s*\(\s*[\w:\s\*&<>,\[\]]+\*\s*\)\s*$)", pre):
        return "raw-cast"
    return "literal"


def _address_only_lines(text):
    """set of 1-based line numbers that exist only in ADDRESS mode: the #else of `#ifdef NOTSA_GLOBALS_DETACHED` / the body of `#ifndef NOTSA_GLOBALS_DETACHED`
    (those keep their StaticRef on purpose: the detached build never compiles them)"""
    out, stack = set(), []      # stack entries: 'det' (branch active only when detached), 'addr' (only when not detached), None (unrelated)
    for n, line in enumerate(text.splitlines(), 1):
        m = re.match(r"\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", line)
        if m:
            d, rest = m.group(1), m.group(2)
            rel = "NOTSA_GLOBALS_DETACHED" in rest
            if d in ("if", "ifdef", "ifndef"):
                neg = d == "ifndef" or re.search(r"!\s*defined|!\s*NOTSA_GLOBALS_DETACHED", rest) is not None
                stack.append((("addr" if neg else "det") if rel else None))
            elif d == "else" and stack:
                stack[-1] = {"det": "addr", "addr": "det", None: None}[stack[-1]]
            elif d == "elif" and stack:
                stack[-1] = None
            elif d == "endif" and stack:
                stack.pop()
        if "addr" in stack:
            out.add(n)
    return out


def _enclosing_headers(stripped, pos):
    """texts of the `{`-headers of every block that encloses pos (outermost first); a header is what stands between the previous `; { }` and the `{`"""
    stack, last = [], 0
    for i in range(pos):
        c = stripped[i]
        if c == "{":
            stack.append(stripped[last:i])
            last = i + 1
        elif c == "}":
            if stack:
                stack.pop()
            last = i + 1
        elif c == ";":
            last = i + 1
    return stack


RE_TMPL = re.compile(r"\btemplate\s*<|\(\s*[^()]*\bauto\b[^()]*\)\s*(?:->[^{]*)?$")


def template_scope_owners(scanned):
    """NOTSA_GLOBAL owner definitions (class members, function-local statics) that sit inside a class/function template: ONE variable PER INSTANTIATION (D-B06 finding,
    QuadTreeNode.h). Aliases / externs do not define storage; a template owner must be a namespace-scope NOTSA_GLOBAL_SYNTH + aliases instead."""
    out = []
    for p, (text, stripped, spans) in scanned.items():
        for s, e, name, a in spans:
            if name not in OWNER_MACROS or name in ("NOTSA_GLOBAL_VERIFY",):
                continue
            hs = _enclosing_headers(stripped, s)
            if any(RE_TMPL.search(h) for h in hs):
                out.append((str(p.relative_to(REPO)), stripped.count("\n", 0, s) + 1, name, a[0] if a else "?"))
    return out


def check_converted(args):
    rows = ge.load_rows(args.tsv)
    by = {r["a"]: r for r in rows}
    al_json = json.loads(Path(args.aliases).read_text()) if Path(args.aliases).exists() else {"globals": {}, "extents": {}}
    size_of, owner_of = {}, {}
    for r in rows:
        size_of[r["a"]] = r["sz"]
        owner_of[r["a"]] = r["name"]
    for a, g in al_json["globals"].items():
        size_of[int(a, 16)] = max(size_of.get(int(a, 16), 0), g["size"])
        o = g["owner"]
        owner_of.setdefault(int(a, 16), o.get("name") or o.get("synthetic"))
    for a, e in al_json["extents"].items():
        size_of[int(a, 16)] = max(size_of.get(int(a, 16), 0), e["true_size"])
    for n in al_json.get("new_globals", []):
        size_of.setdefault(int(n["addr"], 16), n["size"])
        owner_of.setdefault(int(n["addr"], 16), n.get("file", "new_global"))
    boot = {}
    if args.boot_dump and Path(args.boot_dump).exists():
        for line in Path(args.boot_dump).read_text().splitlines():
            f = line.split()
            if len(f) >= 2 and f[0].startswith("0x"):
                try:
                    boot[int(f[0], 16)] = int(f[1], 0)
                except ValueError:
                    pass
    files = sorted(p for p in (REPO / "source").rglob("*") if p.suffix in gg.SRC_SUFFIXES)
    files = [p for p in files if p.name != "DetachedShared.h" or True]
    converted = {}       # addr -> dict(size, kinds, files)
    scanned = {}
    for p in files:
        try:
            text = p.read_text(errors="replace")
        except OSError:
            continue
        if "NOTSA_" not in text and "StaticRef" not in text and "0x" not in text and "0X" not in text:
            continue
        stripped = gg.strip_source(text)
        spans = _macro_spans(stripped, text)
        scanned[p] = (text, stripped, spans)
        rel = str(p.relative_to(REPO))
        for s, e, name, a in spans:
            for idx in MACRO_ADDR_ARGS.get(name, ()):
                if idx >= len(a):
                    continue
                m = re.fullmatch(r"0[xX]([0-9A-Fa-f]+)", a[idx].strip())
                if not m:
                    continue
                A = int(m.group(1), 16)
                if not (DATA_LO <= A < DATA_HI):
                    continue
                c = converted.setdefault(A, dict(macros=set(), files=set(), local_only=True, defs=[]))
                if name in OWNER_MACROS and name not in ("NOTSA_GLOBAL_DECL", "NOTSA_GLOBAL_VERIFY") and idx == MACRO_ADDR_ARGS[name][0] and A not in al_json.get("_noop", []):
                    c["defs"].append("%s:%d" % (rel, stripped.count("\n", 0, s) + 1))
                c["macros"].add(name)
                c["files"].add(rel)
                if name not in LOCAL_MACROS and name not in ("NOTSA_GLOBAL_ALIAS", "NOTSA_GLOBAL_EXPR", "NOTSA_GLOBAL_LOCAL_REF"):
                    c["local_only"] = False
    # extents
    ext = []
    for A, c in sorted(converted.items()):
        sz = max(size_of.get(A, 0), boot.get(A, 0)) or 1
        c["size"] = sz
        c["known"] = A in size_of or A in boot
        c["name"] = owner_of.get(A, "?")
        ext.append((A, A + sz, A))
    starts = [e[0] for e in ext]
    import bisect

    def containing(v):
        i = bisect.bisect_right(starts, v) - 1
        res = []
        while i >= 0 and i > bisect.bisect_right(starts, v) - 40:
            if ext[i][0] <= v < ext[i][1]:
                res.append(ext[i][2])
            i -= 1
        return res

    strays, addr_only = [], {}
    for p, (text, stripped, spans) in scanned.items():
        rel = str(p.relative_to(REPO))
        for m in RE_HEXLIT.finditer(stripped):
            v = int(m.group(1), 16)
            if not (DATA_LO <= v < DATA_HI):
                continue
            owners = containing(v)
            if not owners:
                continue
            if any(s <= m.start() < e for s, e, _, _ in spans):
                continue
            line = stripped.count("\n", 0, m.start()) + 1
            if line in addr_only.setdefault(rel, _address_only_lines(text)):
                continue
            kind = _site_kind(stripped, m.start())
            dead = rel.startswith(gg.DEAD_PREFIXES) or rel in gg.DEAD_FILES
            infra = rel.startswith(SKIP_DIRS_INFRA)
            ctx = text.splitlines()[line - 1].strip()[:110]
            O = max(owners)
            strays.append(dict(file=rel, line=line, addr=v, owner=O, off=v - O, kind=kind, dead=dead, infra=infra, ctx=ctx))
    out = ["file\tline\taddr\towner_addr\toffset\towner\tkind\tclass\tcontext"]
    for s in sorted(strays, key=lambda x: (x["infra"], x["dead"], x["file"], x["line"])):
        cls = "dead" if s["dead"] else "infra" if s["infra"] else "STRAY"
        out.append("%s\t%d\t0x%X\t0x%X\t%d\t%s\t%s\t%s\t%s" % (s["file"], s["line"], s["addr"], s["owner"], s["off"], converted[s["owner"]]["name"], s["kind"], cls, s["ctx"]))
    text_out = "\n".join(out) + "\n"
    if args.out:
        Path(args.out).write_text(text_out)
    real = [s for s in strays if not s["dead"] and not s["infra"]]
    infra = [s for s in strays if s["infra"] and not s["dead"]]
    sys.stdout.write(text_out if (real or infra) else "")
    view_addrs = {int(v["addr"], 16) for v in al_json.get("views", [])}   # a "view" deliberately keeps its own storage next to the real owner
    dup = [(A, c["defs"]) for A, c in sorted(converted.items()) if len(c["defs"]) > 1 and A not in view_addrs]
    for A, d in dup:
        print("MULTI-OWNER\t0x%X\t%s\tmore than one defining macro for one address: two objects" % (A, " ".join(d)))
    # an extent that runs into a NOT converted global of the table (under-declared neighbour / over-sized owner): its poison/own storage would overlay live image data
    ovl = []
    for r in rows:
        a0 = r["a"]
        if a0 in converted or r["sz"] <= 0 or r["kind"] != "decl":
            continue
        for O in containing(a0) + containing(a0 + r["sz"] - 1):
            if O in view_addrs or O in converted and converted[O].get("local_only"):
                continue
            ovl.append((a0, r["name"], O, converted[O]["name"], converted[O]["size"]))
    for a0, n0, O, nO, szO in sorted(set(ovl)):
        print("OVERLAP\t0x%X %s\tlies in/crosses the extent of the converted 0x%X %s (%d B)\tconvert both or fix the extent" % (a0, n0, O, nO, szO))
    tmpl = template_scope_owners(scanned)
    for f, ln, mac, nm in tmpl:
        print("TEMPLATE-OWNER\t%s:%d\t%s(%s)\tone variable per instantiation: use a namespace-scope NOTSA_GLOBAL_SYNTH + NOTSA_GLOBAL_ALIAS" % (f, ln, mac, nm))
    unk = [A for A, c in converted.items() if not c["known"]]
    for A in unk:
        print("UNKNOWN-SIZE\t0x%X\t%s\tno row in DETACH_GLOBALS.tsv / aliases.json / boot dump: extent assumed 1 byte (pass --boot-dump)" % (A, " ".join(sorted(converted[A]["files"]))))
    print("\ncheck-converted: %d converted addresses (%d owners that are function-local only), %d strays (+%d in source/standalone, %d dead), %d without a known size"
          % (len(converted), sum(1 for c in converted.values() if c["local_only"]), len(real), len(infra), len(strays) - len(real) - len(infra), len(unk)), file=sys.stderr)
    if args.fix and real:
        import subprocess
        byfile = {}
        for r in real:
            if r.get("addr"):
                byfile.setdefault(r["file"], set()).add(r["addr"])
        for f, addrs in sorted(byfile.items()):
            cmd = [sys.executable, "-I", str(Path(__file__).resolve()), "--only-addr", ",".join("0x%X" % a for a in sorted(addrs)), f] + (["--apply"] if args.apply else [])
            print("fix:", " ".join(cmd[2:]), file=sys.stderr)
            subprocess.run(cmd, check=False)
    return real + [dict(file=f, line=ln) for f, ln, _, _ in tmpl] + [dict(file="?", line=0)] * (len(dup) + len(set(ovl))), infra, converted


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--gen-shared", action="store_true")
    ap.add_argument("--check-converted", action="store_true", help="list address-mode uses of converted globals (strays); exit 1 if any")
    ap.add_argument("--boot-dump", default=None, help="--check-converted: NOTSA_VERIFY_GLOBALS boot dump (`0xADDR size` lines) to take extents from")
    ap.add_argument("--out", default=None, help="--check-converted: write the TSV here")
    ap.add_argument("--fix", action="store_true", help="--check-converted: run the normal conversion (--only-addr <stray>) over the files with strays (dry run unless --apply); sites that are not alias rows of aliases.json are reported 'skip' and need a hand edit")
    ap.add_argument("--tsv", default=None)
    ap.add_argument("--aliases", default=str(REPO / ".notes" / "aliases.json"))
    ap.add_argument("--image-dir", default=None)
    ap.add_argument("--inline-max", type=int, default=400, help="longer initialisers of class members go to <stem>.cpp (DECL+DEF)")
    ap.add_argument("--skip-addr", default="", help="comma separated 0xADDR list to leave alone")
    ap.add_argument("--only-addr", default="", help="comma separated 0xADDR list: convert only these")
    args = ap.parse_args()
    args.skip_addr = {int(x, 16) for x in args.skip_addr.split(",") if x}
    args.only_addr = {int(x, 16) for x in args.only_addr.split(",") if x}
    rows = ge.load_rows(args.tsv)
    by = {r["a"]: r for r in rows}
    hz = hazard_sets()
    al = Aliases(args.aliases, by)
    if not al.ok:
        print("warning: %s not found: duplicates / conflicts / overlaps stay undecided" % args.aliases, file=sys.stderr)
    if args.check_converted:
        real, infra, _ = check_converted(args)
        sys.exit(1 if real else 0)
    report = []
    em = None if args.check else ge.Emit(ge.Img(args.image_dir), ge.FnResolver())
    if args.gen_shared:
        gen_shared(al, em, args)
        return
    for f in args.files:
        p = (REPO / f).resolve() if not Path(f).is_absolute() else Path(f)
        if args.check:
            check(p, by, hz, al, report)
        else:
            process(p, by, hz, em, al, args, report)
    from collections import Counter
    print("\n%-44s %-40s %-9s %-9s %s" % ("file", "global", "addr", "action", "detail"), file=sys.stderr)
    for r in report:
        print("%-44s %-40s %-9s %-9s %s" % r, file=sys.stderr)
    print("\n" + ", ".join("%s: %d" % kv for kv in sorted(Counter(r[3] for r in report).items())), file=sys.stderr)


if __name__ == "__main__":
    main()
