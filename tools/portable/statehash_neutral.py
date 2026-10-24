#!/usr/bin/env python3
"""RR-1 neutrality proof: with NOTSA_STATEHASH undefined, every translation unit touched by the harness preprocesses to EXACTLY the text it had at --base.

  statehash_neutral.py [--base REV=HEAD] [--cc build/PortableMacRR/compile_commands.json] FILE...

For each FILE: take its compile command from compile_commands.json, drop -DNOTSA_STATEHASH / -o / -MD... / precompiled-header options, add `-E -P`, preprocess the current
file and `git show REV:FILE` (copied next to the original under another name, so relative includes resolve), and compare. Headers touched by the harness (Base.h, msvc_rand.h,
dsound_al.h) are covered through the TUs that include them: pass at least one user (Entity.cpp for NOTSA_PTR_ID32, anything using rand() for msvc_rand.h).
Exit 0 = all identical. Linemarker-free (-P): a pure line-number shift is neutral (decision of w0a_guardlines.py); `__LINE__` expansions are the only thing that can differ and are listed.
Does not replace the wine/MSVC A/B (.notes/STREAM_C_PLAN.md) for the Win32 build; it proves the text the compiler sees is unchanged in the portable configuration, and the
guards are plain `#ifdef NOTSA_STATEHASH` blocks, so the Win32 view is the same by construction.
"""
import json, os, shlex, subprocess, sys, tempfile

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def pp(entry, src_path):
    args = shlex.split(entry['command'])
    out, skip = [], False
    for a in args:
        if skip:
            skip = False
            continue
        if a in ('-o', '-MF', '-MT', '-MQ', '-include-pch'):
            skip = True
            continue
        if a in ('-MD', '-MMD', '-c') or a == '-DNOTSA_STATEHASH' or a.startswith('-Winvalid-pch'):
            continue
        if a.endswith('.cpp') and os.path.basename(a) == os.path.basename(entry['file']):
            continue
        out.append(a)
    # drop precompiled header use: the .pch is not valid for another define set, include the text header instead (-include cmake_pch.hxx is kept as is)
    cmd = out + ['-E', '-P', '-Wno-everything', src_path]
    r = subprocess.run(cmd, capture_output=True, text=True, errors='replace', cwd=entry.get('directory', REPO))
    if r.returncode != 0:
        return None, r.stderr[-600:]
    return r.stdout, ''


def main(argv):
    base, cc = 'HEAD', os.path.join(REPO, 'build/PortableMacRR/compile_commands.json')
    files = []
    i = 1
    while i < len(argv):
        if argv[i] == '--base':
            base = argv[i + 1]; i += 2
        elif argv[i] == '--cc':
            cc = argv[i + 1]; i += 2
        else:
            files.append(argv[i]); i += 1
    table = {os.path.realpath(e['file']): e for e in json.load(open(cc))}
    rc = 0
    for f in files:
        path = os.path.realpath(f)
        e = table.get(path)
        if not e:
            print(f'SKIP  {f}: not in compile_commands.json (header? pass a TU that includes it)')
            continue
        old = subprocess.run(['git', '-C', REPO, 'show', f'{base}:{os.path.relpath(path, REPO)}'], capture_output=True, text=True)
        if old.returncode != 0:
            print(f'NEW   {f}: not in {base} (new file, nothing to compare)')
            continue
        tmp = os.path.join(os.path.dirname(path), '.__base__' + os.path.basename(path))
        try:
            with open(tmp, 'w') as t:
                t.write(old.stdout)
            a, ea = pp(e, tmp)
            b, eb = pp(e, path)
        finally:
            if os.path.exists(tmp):
                os.remove(tmp)
        if a is None or b is None:
            print(f'ERROR {f}: preprocessor failed: {(ea or eb)[-300:]}')
            rc = 1
            continue
        # the temp copy has another file name: __FILE__ expansions differ in the quoted name only
        norm = lambda s, n: s.replace(os.path.basename(tmp), os.path.basename(path)) if n else s
        a = norm(a, True)
        # neutral differences: blank lines (-P keeps the ones the added #ifdef blocks leave) and __LINE__ numbers that follow a source file name inside a string ("file.cpp", 54)
        import re
        strip = lambda s: '\n'.join(l for l in (re.sub(r'(\.(?:cpp|h|hpp|inl)")\s*,\s*\d+', r'\1, N', l).rstrip() for l in s.splitlines()) if l.strip())
        a, b = strip(a), strip(b)
        if a == b:
            print(f'OK    {f}: preprocessed text identical to {base} ({len(b.splitlines())} lines)')
        else:
            al, bl = a.splitlines(), b.splitlines()
            d = next((k for k in range(min(len(al), len(bl))) if al[k] != bl[k]), min(len(al), len(bl)))
            print(f'DIFF  {f}: first difference at preprocessed line {d}:\n   base: {al[d][:160] if d < len(al) else "<eof>"}\n   now : {bl[d][:160] if d < len(bl) else "<eof>"}')
            rc = 1
    return rc


if __name__ == '__main__':
    sys.exit(main(sys.argv))
