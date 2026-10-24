#!/usr/bin/env python3
"""rw_symbol_audit.py - which symbols of the portable-excluded render files are still referenced by kept code?

Slice R-2 of stream R excludes the D3D9 fixed-function render facade (source/CMakeLists.txt, PORTABLE_EXCLUDED_RENDER_FILES) from the
portable build. Kept translation units may still call into those files (RxPipeline*, RwD3D9*, RwIm2D*/RwIm3D*, D3DResourceSystem::*, ...).
Nothing in the syntax-only census sees that (it is a link-time problem), so this tool lists them:

  1. defined(excluded)  = external-linkage functions / variables defined at namespace scope in the excluded files (brace-level scan,
                          anonymous namespaces and `static` skipped);
  2. defined(kept)      = the same for every kept .cpp (a symbol also defined in a kept file is not missing);
  3. used(kept)         = identifier tokens in kept .cpp/.inc files and in headers outside source/fakerw and source/game_sa/RenderWare/rw
                          (those only declare);
  4. missing            = defined(excluded) & used(kept) - defined(kept);
  5. each missing symbol is checked against the stub file (default source/portable/render/rw_stubs_gl3.cpp): STUBBED / NO STUB.

usage: rw_symbol_audit.py [--tsv OUT.tsv] [--stubs FILE] [--emit-stubs] [--check]
  --check   exit 1 if a missing symbol has no stub (use it as the gate)
  --emit-stubs  print a stub skeleton for the NO STUB symbols (signature copied from the definition; edit by hand)
The kept-file set is the portable compile database (build/PortableMac/compile_commands.json) so it follows the CMake lists.
"""
import argparse, collections, json, os, re, sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SRC = os.path.join(REPO, 'source')

OWNERS = [  # (regex on the symbol, owner slice / disposition)
    (r'^(RwIm2D|RwIm3D)', 'R-4 (Im2D/Im3D GL3 twin)'),
    (r'^(RwD3D9|_rwD3D9|RxD3D9|_rpD3D9|RwShimIsFacade)', 'R-8/R-9 (GL3 pipelines replace the FF API)'),
    (r'^(RxPipeline|RxLockedPipe|RxNodeDefinition|RxHeap)', 'R-8/R-9 (ObjPipeline creation in pipelines_gl3.cpp)'),
    (r'^(RwShimMatFX|RwShimPipeline)', 'R-6 (stock gl3 pipelines wired)'),
    (r'(Skin|skin)', 'R-10 (gl3 skin pipeline)'),
    (r'^D3DResourceSystem::', 'inert (no D3D pools under GL3)'),
]


def strip(src):
    """Remove comments, string/char literals and preprocessor lines (keeps line structure)."""
    out = []
    i, n = 0, len(src)
    bol = True
    while i < n:
        c = src[i]
        if c == '/' and src.startswith('//', i):
            j = src.find('\n', i)
            i = n if j < 0 else j
        elif c == '/' and src.startswith('/*', i):
            j = src.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append('\n' * src.count('\n', i, j))
            i = j
        elif c == '"' or c == "'":
            q = c
            j = i + 1
            while j < n and src[j] != q:
                j += 2 if src[j] == '\\' else 1
            out.append(q + q)
            i = j + 1
        elif c == '#' and bol:
            j = i
            while True:  # continuation lines
                k = src.find('\n', j)
                if k < 0:
                    j = n
                    break
                if src[k - 1] == '\\':
                    j = k + 1
                    continue
                j = k
                break
            out.append('\n' * src.count('\n', i, j))
            i = j
        else:
            out.append(c)
            bol = c == '\n' or (bol and c in ' \t')
            i += 1
    return ''.join(out)


IDENT = re.compile(r'[A-Za-z_]\w*')


def scan_defs(path):
    """-> {qualified name: (line, signature)} for external-linkage definitions at namespace scope."""
    text = strip(open(path, errors='replace').read())
    defs = {}
    depth_ns = []  # stack of namespace names ('' for anonymous)
    i, n, line = 0, len(text), 1
    head_start, head_line = 0, 1
    while i < n:
        c = text[i]
        if c == '\n':
            line += 1
        if c == ';':
            head = text[head_start:i].strip()
            if '' not in depth_ns:
                _variable(head, head_line, depth_ns, defs)
            head_start, head_line = i + 1, line
        elif c == '{':
            head = ' '.join(text[head_start:i].split())
            head_line += text[head_start:i][:len(text[head_start:i]) - len(text[head_start:i].lstrip())].count('\n')
            if re.match(r'^(inline\s+)?namespace\b', head):
                m = re.match(r'^(?:inline\s+)?namespace\s*([\w:]*)', head)
                depth_ns.append(m.group(1))
                head_start, head_line = i + 1, line
                i += 1
                continue
            if re.match(r'^extern\s*""$', head):
                depth_ns.append('\0')  # transparent
                head_start, head_line = i + 1, line
                i += 1
                continue
            # a body: skip to the matching brace
            j, d, ln = i, 0, line
            while j < n:
                if text[j] == '\n':
                    ln += 1
                if text[j] == '{':
                    d += 1
                elif text[j] == '}':
                    d -= 1
                    if d == 0:
                        break
                j += 1
            if '' not in depth_ns:
                _function_or_type(head, head_line, text, i, j, depth_ns, defs)
            i, line = j, ln
            # after a class/struct/enum body or a lambda the statement continues up to ';'
            k = i + 1
            rest = re.match(r'\s*([\w\s,\[\]\*&]*?)\s*(;|=|\()', text[k:k + 200])
            if rest and rest.group(2) == ';':
                head_start = k  # the `;` handler sees `head` = variable names after the body (ignored)
                head_line = line
                head_text_is_after_body = True
            else:
                head_start, head_line = i + 1, line
        elif c == '}':
            if depth_ns:
                depth_ns.pop()
            head_start, head_line = i + 1, line
        i += 1
    return defs


def _qual(depth_ns, name):
    ns = [x for x in depth_ns if x and x != '\0']
    return '::'.join(ns + [name]) if ns else name


def _function_or_type(head, head_line, text, b, e, depth_ns, defs):
    if not head or head.startswith(('static ', 'template')):
        return
    if re.match(r'^(typedef\s+)?(struct|class|union|enum)\b', head) and '(' not in head:
        return
    p = head.find('(')
    if p < 0:
        return
    pre = head[:p]
    if '=' in pre:
        return
    m = re.search(r'((?:[A-Za-z_]\w*::)*~?[A-Za-z_]\w*)\s*$', pre)
    if not m:
        return
    name = m.group(1)
    if name in ('if', 'for', 'while', 'switch', 'sizeof', 'operator'):
        return
    if re.search(r'\bstatic\b', pre):
        return
    sig = head  # declarator text without the body
    defs[_qual(depth_ns, name)] = (head_line, sig)


def _variable(head, head_line, depth_ns, defs):
    if not head or re.match(r'^(typedef|using|static_assert|extern|template|friend|struct|class|enum|union)\b', head):
        return
    if re.search(r'\bstatic\b|\bconstexpr\b|\bconst\b(?!.*\*)', head.split('=')[0]) and 'extern' not in head:
        return  # internal linkage
    pre = head.split('=')[0]
    if '(' in pre:
        return
    m = re.search(r'([A-Za-z_]\w*)\s*(\[[^\]]*\])*\s*$', pre.strip())
    if m and ' ' in pre.strip():
        defs[_qual(depth_ns, m.group(1))] = (head_line, head)


_IFN = re.compile(r'^\s*#\s*(ifndef\s+NOTSA_PORTABLE|if\s+!\s*defined\s*\(?\s*NOTSA_PORTABLE\s*\)?)\b')
_IFP = re.compile(r'^\s*#\s*(ifdef\s+NOTSA_PORTABLE|if\s+defined\s*\(?\s*NOTSA_PORTABLE\s*\)?)\b')
_IFA = re.compile(r'^\s*#\s*(if|ifdef|ifndef)\b')


def portable_view(text):
    """The text the PORTABLE compile sees: NOTSA_PORTABLE guard branches resolved (other conditionals transparent)."""
    out, stack = [], []   # stack entries: None (transparent) or [active]
    for ln in text.split('\n'):
        if _IFN.match(ln):
            stack.append([False]); out.append(''); continue
        if _IFP.match(ln):
            stack.append([True]); out.append(''); continue
        if _IFA.match(ln):
            stack.append(None)
        elif re.match(r'^\s*#\s*else\b', ln) and stack and stack[-1] is not None:
            stack[-1][0] = not stack[-1][0]; out.append(''); continue
        elif re.match(r'^\s*#\s*endif\b', ln) and stack:
            top = stack.pop()
            if top is not None:
                out.append(''); continue
        out.append(ln if all(e is None or e[0] for e in stack) else '')
    return '\n'.join(out)


def tokens(path):
    t = strip(portable_view(open(path, errors='replace').read()))
    return set(re.findall(r'[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*', t))


def parse_excluded():
    cm = open(os.path.join(SRC, 'CMakeLists.txt')).read()
    m = re.search(r'set\(PORTABLE_EXCLUDED_RENDER_FILES(.*?)\)', cm, re.S)
    return [os.path.join(SRC, x) for x in m.group(1).split() if x.endswith('.cpp')]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tsv')
    ap.add_argument('--stubs', default=os.path.join(SRC, 'portable', 'render', 'rw_stubs_gl3.cpp'))
    ap.add_argument('--emit-stubs', action='store_true')
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--compdb', default=os.path.join(REPO, 'build', 'PortableMac', 'compile_commands.json'))
    a = ap.parse_args()

    excl = parse_excluded()
    edefs = {}
    for f in excl:
        for k, (ln, sig) in scan_defs(f).items():
            edefs.setdefault(k, (os.path.relpath(f, REPO) + ':%d' % ln, sig))
    kept = sorted({e['file'] for e in json.load(open(a.compdb)) if e['file'].startswith(SRC + '/')})
    kept = [k for k in kept if k not in excl]
    kdefs = set()
    used = collections.defaultdict(set)
    scan_files = list(kept)
    for root, _, fs in os.walk(SRC):
        if root.startswith((os.path.join(SRC, 'fakerw'), os.path.join(SRC, 'game_sa', 'RenderWare', 'rw'))):
            continue
        for fn in fs:
            if fn.endswith(('.h', '.hpp', '.inc')):
                p = os.path.join(root, fn)
                if p not in excl:
                    scan_files.append(p)
    for f in scan_files:
        if f == a.stubs:
            continue
        for k in scan_defs(f) if f.endswith('.cpp') else ():
            kdefs.add(k)
        toks = tokens(f)
        short = {t.rsplit('::', 1)[-1] for t in toks}
        for s in edefs:
            if s in toks or (('::' not in s) and s in short):
                used[s].add(os.path.relpath(f, SRC))
    # a symbol that a kept file defines (other than the stubs file) is not missing
    kdefs_short = {k.rsplit('::', 1)[-1] for k in kdefs}
    stubs_text = strip(open(a.stubs).read()) if os.path.exists(a.stubs) else ''
    stub_defs = set(scan_defs(a.stubs)) if os.path.exists(a.stubs) else set()
    rows = []
    for s, (loc, sig) in sorted(edefs.items()):
        if s not in used or s in kdefs:
            continue
        # users that are only the defining excluded files' own headers are not interesting; keep all
        stubbed = s in stub_defs
        owner = next((o for rx, o in OWNERS if re.search(rx, s)), '?')
        if all(u.startswith('standalone/rw/') for u in used[s]):
            owner = 'R-3a/R-3b (only standalone/rw callers: engine/camera/raster/renderstate are guarded in those slices) ' + owner.split(' ')[0]
        rows.append((s, loc, len(used[s]), ','.join(sorted(used[s]))[:160], 'STUBBED' if stubbed else 'NO STUB', owner, sig))
    rows.sort(key=lambda r: (r[4] != 'NO STUB', r[5], r[0]))
    out = sys.stdout
    print('# excluded-file symbols referenced by kept code: %d (%d stubbed, %d without stub)' % (
        len(rows), sum(r[4] == 'STUBBED' for r in rows), sum(r[4] == 'NO STUB' for r in rows)))
    if a.tsv:
        with open(a.tsv, 'w') as fh:
            fh.write('symbol\tdefined_in\tn_users\tusers\tstatus\towner\tsignature\n')
            for r in rows:
                fh.write('\t'.join(str(x) for x in r) + '\n')
        print('tsv:', a.tsv)
    for r in rows:
        if r[4] == 'NO STUB' or not a.tsv:
            print('%-8s %-52s %s [%s] users=%d' % (r[4], r[0], r[1], r[5], r[2]))
    if a.emit_stubs:
        for r in rows:
            if r[4] == 'NO STUB':
                print('// ' + r[0] + ' (' + r[1] + ', users ' + str(r[2]) + ')\n' + r[6] + ' { /* TODO */ }')
    missing = {r[0] for r in rows}
    stale = sorted(x for x in stub_defs if x in edefs and x not in missing)
    if stale:
        print('# stubs for excluded-file symbols that no kept code references any more (can go): ' + ', '.join(stale))
    if a.check and any(r[4] == 'NO STUB' for r in rows):
        sys.exit(1)


if __name__ == '__main__':
    main()
