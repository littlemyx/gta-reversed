#!/usr/bin/env python3
"""MAC-W0a: keep the WINDOWS view of a source file bit-identical (tokens AND __LINE__) after NOTSA_PORTABLE guards were added.

  w0a_guardlines.py fix   FILE [--base REV]   re-insert `#line N // W0a` directives so every Windows-view line keeps its original number
  w0a_guardlines.py check FILE [--base REV]   verify: Windows view (NOTSA_PORTABLE undefined) == `git show REV:FILE` line by line, with the same line numbers

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

def fix(lines, targets):
    """insert #line directives; returns new list"""
    res = []
    stack = []
    cur = 1           # line number the compiler assigns to the next output line
    kept = 0          # Windows-view lines emitted so far (without tool lines)
    def active():
        return all(e is None or e['act'] for e in stack)
    src = [l for l in lines if not l.rstrip().endswith(TAG)]
    # Windows-view line counting: transparent directives count as lines (they exist in the original)
    for ln in src:
        guard = False
        if IF_P.match(ln):
            stack.append({'act': False}); guard = True
        elif IFN_P.match(ln):
            stack.append({'act': True}); guard = True
        elif IF_ANY.match(ln):
            stack.append(None)
        elif ELSE.match(ln):
            if stack and stack[-1] is not None:
                stack[-1]['act'] = not stack[-1]['act']; guard = True
        elif ENDIF.match(ln):
            if stack and stack[-1] is not None:
                stack.pop(); guard = True
            else:
                if stack: stack.pop()
        if guard:
            res.append(ln); cur += 1
            continue
        if active():
            want = targets[kept]; kept += 1
            # a #line must not be inserted before a preprocessor-continued line; fine for plain lines
            if cur != want:
                res.append(f'#line {want} {TAG}'); cur = want
            res.append(ln); cur += 1
            # NOTE: transparent #if/#else/#endif lines are in `src` and counted by kept as in the original
        else:
            res.append(ln); cur += 1
    return res

def main():
    mode, path = sys.argv[1], sys.argv[2]
    rev = 'HEAD'
    if '--base' in sys.argv:
        rev = sys.argv[sys.argv.index('--base') + 1]
    lines = open(path, encoding='utf-8', newline='').read().split('\n')
    trailing = lines[-1] == ''
    if trailing: lines = lines[:-1]
    crlf = any(l.endswith('\r') for l in lines[:5])
    if mode == 'fix':
        base = base_lines(path, rev)
        if base is None:
            print('new file: nothing to do'); return
        bview = windows_view(base)
        nview = windows_view(lines)
        if [t for t, _ in bview] != [t for t, _ in nview]:
            print('Windows view differs from', rev, '- run check'); sys.exit(1)
        new = fix(lines, [i + 1 for _, i in bview])
        open(path, 'w', encoding='utf-8', newline='').write('\n'.join(new) + ('\n' if trailing else ''))
        print('fixed', path, 'line directives:', sum(1 for l in new if l.rstrip().endswith(TAG)))
    elif mode == 'check':
        base = base_lines(path, rev)
        if base is None:
            print('NEW FILE (not in', rev, ')'); return
        bview = windows_view(base)
        nview = windows_view(lines)
        view = [t for t, _ in nview]
        ok = [t for t, _ in bview] == view
        targets = [i + 1 for _, i in bview]
        # compare numbering: simulate compiler numbering over `lines`
        num_ok = True
        if ok:
            stack = []; cur = 1; idx = 0
            def active():
                return all(e is None or e['act'] for e in stack)
            for ln in lines:
                m = re.match(r'^#line (\d+) // W0a$', ln.rstrip('\r'))
                if m:
                    cur = int(m.group(1)); continue   # directive itself: next line gets this number
                guard = False
                if IF_P.match(ln): stack.append({'act': False}); guard = True
                elif IFN_P.match(ln): stack.append({'act': True}); guard = True
                elif IF_ANY.match(ln): stack.append(None)
                elif ELSE.match(ln):
                    if stack and stack[-1] is not None: stack[-1]['act'] = not stack[-1]['act']; guard = True
                elif ENDIF.match(ln):
                    if stack and stack[-1] is not None: stack.pop(); guard = True
                    elif stack: stack.pop()
                if not guard and active():
                    if cur != targets[idx]:
                        num_ok = False; print('LINE MISMATCH at view line', idx, 'compiler number', cur, 'want', targets[idx], ':', ln[:80]); break
                    idx += 1
                cur += 1
        print(path, 'TEXT', 'OK' if ok else 'DIFF', 'LINENO', 'OK' if num_ok else 'BAD')
        if not ok:
            import difflib
            for d in list(difflib.unified_diff([t for t, _ in bview], view, 'base', 'windows-view', lineterm='', n=0))[:30]:
                print(d)
        sys.exit(0 if ok and num_ok else 1)

main()
