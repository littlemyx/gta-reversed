#!/usr/bin/env python3
"""MAC-W0a/W0-REV: REPORT-ONLY checker that the WINDOWS view of a source file is unchanged after NOTSA_PORTABLE guards were added.

  w0a_guardlines.py check FILE [--base REV]   verify: Windows view (NOTSA_PORTABLE undefined, guard lines dropped) == `git show REV:FILE`, text only.
  Never writes. Line numbers are NOT compared (decision 2026-10-23: __LINE__-only differences are neutral; `#line` directives were removed).

Guard directives recognised: #ifdef/#ifndef NOTSA_PORTABLE, #if defined(NOTSA_PORTABLE), #if !defined(NOTSA_PORTABLE), their #else/#endif.
All other conditionals are transparent (both branches counted, as in the original). Lines ending with `// W0a` are tool-owned.
"""
import re, subprocess, sys, os

TAG = '// W0a'
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))

def base_lines(path, rev):
    rel = os.path.relpath(os.path.abspath(path), REPO)
    out = subprocess.run(['git', '-C', REPO, 'show', f'{rev}:{rel}'], capture_output=True, text=True)
    if out.returncode != 0:
        return None
    return out.stdout.split('\n')[:-1] if out.stdout.endswith('\n') else out.stdout.split('\n')

IF_P   = re.compile(r'^\s*#\s*(ifdef\s+NOTSA_PORTABLE|if\s+defined\s*\(?\s*NOTSA_PORTABLE\s*\)?)\s*(//.*)?$')
IFN_P  = re.compile(r'^\s*#\s*(ifndef\s+NOTSA_PORTABLE|if\s+!\s*defined\s*\(?\s*NOTSA_PORTABLE\s*\)?)\s*(//.*)?$')
IF_ANY = re.compile(r'^\s*#\s*(if|ifdef|ifndef)\b')
ELSE   = re.compile(r'^\s*#\s*(else|elif\b.*)\s*(//.*)?$')
ENDIF  = re.compile(r'^\s*#\s*endif\b')

def windows_view(lines):
    """-> list of (text, original_number_or_None) for lines that exist in the Windows view; tool lines dropped."""
    out = []
    stack = []  # entries: None (transparent) or dict(active_if: bool, in_else: bool)
    def active():
        return all(e is None or e['act'] for e in stack)
    for idx_, ln in enumerate(lines):
        if ln.rstrip().endswith(TAG):
            continue
        if IF_P.match(ln):
            stack.append({'act': False, 'inv': False}); continue      # `#ifdef NOTSA_PORTABLE`: if-branch inactive on Windows
        if IFN_P.match(ln):
            stack.append({'act': True, 'inv': True}); continue         # `#ifndef`: if-branch active
        if IF_ANY.match(ln):
            stack.append(None)
            if active(): out.append((ln, idx_))
            continue
        if ELSE.match(ln):
            top = stack[-1] if stack else None
            if top is not None:
                top['act'] = not top['act']; continue
            if active(): out.append((ln, idx_))
            continue
        if ENDIF.match(ln):
            top = stack.pop() if stack else None
            if top is not None:
                continue
            if active(): out.append((ln, idx_))
            continue
        if active():
            out.append((ln, idx_))
    return out

def main():
    if len(sys.argv) < 3 or sys.argv[1] != 'check':
        print(__doc__); sys.exit(2)
    path = sys.argv[2]
    rev = 'HEAD'
    if '--base' in sys.argv:
        rev = sys.argv[sys.argv.index('--base') + 1]
    lines = open(path, encoding='utf-8', newline='').read().split('\n')
    if lines and lines[-1] == '': lines = lines[:-1]
    base = base_lines(path, rev)
    if base is None:
        print('NEW FILE (not in', rev, ')'); return
    bview = [t for t, _ in windows_view(base)]
    view = [t for t, _ in windows_view(lines)]
    ok = bview == view
    print(path, 'TEXT', 'OK' if ok else 'DIFF')
    if not ok:
        import difflib
        for d in list(difflib.unified_diff(bview, view, 'base', 'windows-view', lineterm='', n=0))[:30]:
            print(d)
    sys.exit(0 if ok else 1)

main()
