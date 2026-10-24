#!/usr/bin/env python3
"""Case-insensitivity shim for case-sensitive file systems (Linux). MAC-LNX1.

The reversed sources were written on Windows: `#include <Audio/entities/AEAudioEntity.h>` names a directory `Entities`, `#include "Tasks/TaskTypes/..."` a file with other letter case.
Instead of editing hundreds of MSVC-compiled files, this script scans every `#include` under the source roots, resolves it the way the compiler does (directory of the including
file for "..." includes, then the -I directories), and for each include that exists only with a different letter case creates a symlink  <shim>/<include text as written> -> <real file>.
The shim directory is added as the LAST -I directory of the portable Linux build, so correctly-spelled includes never go through it.

usage: gen_case_shim.py <repo> <shim-dir> <include-dir>...        (include dirs: absolute, in compiler order)
Exit 0; prints the number of links. Idempotent (the shim dir is rebuilt).
"""
import os, re, shutil, sys

repo, shim = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
incdirs = [os.path.abspath(d) for d in sys.argv[3:]]
INC = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.M)
ci_cache = {}

def listdir_ci(d):
    m = ci_cache.get(d)
    if m is None:
        try:
            m = {}
            for n in os.listdir(d):
                m.setdefault(n.lower(), n)
        except OSError:
            m = {}
        ci_cache[d] = m
    return m

def resolve_ci(base, rel):
    """case-insensitive resolution of rel below base; returns the real path or None"""
    cur = base
    for part in rel.replace('\\', '/').split('/'):
        if part in ('', '.'):
            continue
        if part == '..':
            cur = os.path.dirname(cur); continue
        real = listdir_ci(cur).get(part.lower())
        if real is None:
            return None
        cur = os.path.join(cur, real)
    return cur if os.path.isfile(cur) else None

shutil.rmtree(shim, ignore_errors=True)
links = {}
roots = [os.path.join(repo, 'source')]
for root in roots:
    for dp, dns, fns in os.walk(root):
        if os.path.abspath(dp).startswith(shim):
            continue
        for fn in fns:
            if not fn.endswith(('.h', '.hpp', '.cpp', '.c', '.inc', '.hxx')):
                continue
            path = os.path.join(dp, fn)
            try:
                text = open(path, errors='replace').read()
            except OSError:
                continue
            for kind, inc in INC.findall(text):
                inc = inc.replace('\\', '/')
                if inc in links:
                    continue
                dirs = ([dp] if kind == '"' else []) + incdirs
                if any(os.path.isfile(os.path.join(d, inc)) for d in dirs):
                    continue                                   # spelled correctly (or not ours)
                for d in dirs:
                    real = resolve_ci(d, inc)
                    if real:
                        links[inc] = real
                        break
for inc, real in sorted(links.items()):
    dst = os.path.join(shim, inc)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    os.symlink(real, dst)
os.makedirs(shim, exist_ok=True)
print(f'case shim: {len(links)} includes -> {shim}')
for inc, real in sorted(links.items())[:40]:
    print(f'  {inc} -> {os.path.relpath(real, repo)}')
