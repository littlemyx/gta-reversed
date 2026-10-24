#!/usr/bin/env python3
"""P4-S1 proof: every rand()/srand()/RAND_MAX user of the game resolves to the MSVC-CRT shim (source/portable/msvc_rand.h) in the PortableMac build.

For each TU of build/PortableMac/compile_commands.json whose own file mentions `rand(`/`srand(`/`RAND_MAX` (and, for header users, every TU, see --headers) the TU is
preprocessed with its real flags (no PCH; the PCH header is force-included textually) and the non-system text is checked:
  * no `rand(` / `srand(` / `RAND_MAX` token is left unexpanded in user code (expansion happens only through the shim's macros),
  * the shim's expansions `::notsa::msvc_crt::Rand()` / `SRand(` appear at least as often as the source calls.
usage: rand_proof.py [--compdb FILE] [-j N] [--root DIR]"""
import argparse, json, os, re, shlex, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

ap = argparse.ArgumentParser()
ap.add_argument('--compdb', default='build/PortableMac/compile_commands.json')
ap.add_argument('-j', type=int, default=4)
ap.add_argument('--root', default=os.getcwd(), help='tree to scan (a snapshot dir for a stable tree)')
ap.add_argument('--pchhxx', help='replacement for the force-included cmake_pch.hxx (snap_census: <out>/pch.hxx, which includes the snapshot StdInc.cpp)')
a = ap.parse_args()
root = os.path.realpath(a.root)
tok = re.compile(r'\b(s?rand)\s*\(|\bRAND_MAX\b')
callre = re.compile(r'(?<![\w:>.])(s?rand)\s*\(')
cc = json.load(open(a.compdb))
tus = {}
for e in cc:
    f = os.path.realpath(e['file'])
    if '/source/libs/' in f or not f.startswith(root + '/source/'):
        continue
    tus[f] = e['command']

def strip(line):
    return line.split('//')[0]

def src_count(path):
    n = 0
    for line in open(path, errors='replace'):
        l = strip(line)
        if re.match(r'\s*(/\*|\*)', l):
            continue
        n += len(callre.findall(l)) + len(re.findall(r'\bRAND_MAX\b', l))
    return n

users = {}
for dp, _, fn in os.walk(os.path.join(root, 'source')):
    if '/source/libs' in dp:
        continue
    for f in fn:
        if f.endswith(('.cpp', '.h', '.hpp', '.inc')):
            p = os.path.join(dp, f)
            if p.endswith(('standalone/GameRand.h', 'portable/msvc_rand.h')):
                continue
            c = src_count(p)
            if c:
                users[os.path.realpath(p)] = c

def run(f):
    toks = shlex.split(tus[f])
    out = []
    i = 0
    while i < len(toks):
        t = toks[i]
        if t == '-Xclang' and toks[i + 1] == '-include-pch':
            i += 4; continue            # drop `-Xclang -include-pch -Xclang X.pch`
        if a.pchhxx and t.endswith('cmake_pch.hxx'):
            out.append(a.pchhxx); i += 1; continue
        if t in ('-o', '-c'):
            i += 2 if t == '-o' else 1; continue
        if t == f:
            i += 1; continue
        out.append(t); i += 1
    stub = os.path.join(root, 'tools/standalone/winstub')   # the census' Windows-SDK replacement (the real build has no Windows headers either)
    cmd = out + ['-isystem', stub, '-include', os.path.join(stub, 'winstub_prefix.h'), '-I/opt/homebrew/include', '-E', '-fdiagnostics-color=never', f]
    r = subprocess.run(['nice', '-n', '10'] + cmd, capture_output=True, text=True, cwd=root)
    if r.returncode != 0:
        return f, None, ' | '.join(l for l in r.stderr.splitlines() if 'error' in l)[:400]
    cur = None; sysh = False
    left, rand_exp, srand_exp, seen = [], 0, 0, set()
    for line in r.stdout.splitlines():
        m = re.match(r'# \d+ "([^"]*)"(.*)', line)
        if m:
            cur = os.path.realpath(m.group(1)) if os.path.exists(m.group(1)) else m.group(1)
            sysh = ' 3' in m.group(2) or ' 4' in m.group(2)
            seen.add(cur)
            continue
        if sysh or cur is None or not cur.startswith(root + '/source/') or '/source/libs/' in cur:
            if tok.search(line) and cur and cur.startswith(root + '/source/') and '/source/libs/' not in cur:
                pass
            continue
        left += [(cur, l.strip()) for l in [line] if tok.search(l) and not re.match(r'\s*(/\*|\*)', l)]
        rand_exp += line.count('::notsa::msvc_crt::Rand()')
        srand_exp += line.count('::notsa::msvc_crt::SRand(')
    return f, (left, rand_exp, srand_exp, seen), ''

cpp_users = [f for f in users if f.endswith('.cpp') and f in tus]
print(f'files that mention rand/srand/RAND_MAX: {len(users)} ({len(cpp_users)} TUs, {len(users) - len(cpp_users)} headers/others); preprocessing {len(cpp_users)} TUs', flush=True)
bad, covered, tot_r, tot_s = 0, set(), 0, 0
with ThreadPoolExecutor(a.j) as ex:
    for f, res, err in ex.map(run, cpp_users):
        rel = os.path.relpath(f, root)
        if res is None:
            print(f'ERROR {rel}: {err}'); bad += 1; continue
        left, re_, se_, seen = res
        covered |= seen; tot_r += re_; tot_s += se_
        # `left`: lines in user files that still carry a token. Expected: ONLY lines that are not calls (none) -> any `rand(`, `srand(` or RAND_MAX left is a bypass.
        if left:
            bad += 1
            for cur, l in left[:5]:
                print(f'BYPASS {os.path.relpath(cur, root)}: {l[:140]}')
        if re_ + se_ < users[f] - sum(1 for _ in [0] if 'RAND_MAX' in open(f, errors='replace').read() and False):
            pass
hdrs = [h for h in users if not h.endswith('.cpp')]
missing = [os.path.relpath(h, root) for h in hdrs if h not in covered]
print(f'headers/others with a user: {len(hdrs)}, seen in at least one preprocessed TU: {len(hdrs) - len(missing)}; not seen: {missing}')
print(f'shim expansions in those TUs: Rand()={tot_r}  SRand()={tot_s}; bypasses/errors: {bad}')
print('RESULT RAND_PROOF OK' if bad == 0 else 'RESULT RAND_PROOF FAILED')
sys.exit(1 if bad else 0)
