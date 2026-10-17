#!/usr/bin/env python3
"""preprocessed_scan.py - F1 sweep of the DETACHED run build for anything that still names a fixed exe address in ACTIVE code (after the preprocessor:
#if / macros resolved), which a grep of the sources cannot tell. For every PCH translation unit of the build dir (ninja -t commands) it runs
cl /E with the TU's own flags and keeps the lines that come from source/ or vendor/ and contain
  - StaticRef< / ScopedStaticRef< / plugin::Call* / CallMethod / CallAndReturn / ResolveCallTarget          (HIT = CALL/STATICREF)
  - a hex literal in the exe image range 0x401000..0xCB0000 (or, with MODE=dec, a decimal one)             (HIT = HEX)
(the notsa::globals::Reg verify records are blanked first). Output: one tsv per TU in OUT, then print the union with --agg.
  env: BUILD (default build/StandaloneDetached), OUT (default <tmp>/preprocessed_scan), JOBS (default 2), ONLY=<obj substring>, MODE=hex|dec
  usage (Git Bash, repo root, after `source tools/standalone/windows/msvcenv.sh`):  python -X utf8 -I tools/standalone/preprocessed_scan.py [--agg]
Remaining hits are constants (flag masks, colours), template DEFINITIONS (Base.h ScopedStaticRef, PluginBase.h) and the image machinery (F3) - see .notes/reports/D-F1.md.
"""
import re, subprocess, sys, os, shlex, json
from concurrent.futures import ThreadPoolExecutor

import tempfile
BUILD = os.environ.get('BUILD', 'build/StandaloneDetached')
OUT = os.environ.get('OUT', tempfile.gettempdir() + '/preprocessed_scan')
MODE = os.environ.get('MODE', 'hex')
os.makedirs(OUT, exist_ok=True)
JOBS = int(os.environ.get('JOBS', '2'))
LIMIT = int(os.environ.get('LIMIT', '100000'))
ONLY = os.environ.get('ONLY')

cmds = subprocess.run(['ninja', '-t', 'commands', 'gta_reversed.exe'], cwd=BUILD, capture_output=True, text=True).stdout.splitlines()
tus = []
for c in cmds:
    m = re.search(r'/Fo(source\\CMakeFiles\\gta_reversed\.dir\\\S+?\.obj)', c)
    if m and ' /Yu' in c:
        tus.append((m.group(1), c))
print('TUs with PCH use:', len(tus), file=sys.stderr)

HEX = re.compile(r'\b0[xX]([0-9A-Fa-f]{5,8})\b') if MODE == 'hex' else re.compile(r'(?<![\w."])(\d{7,8})(?![\w.])')
PAT = re.compile(r'ZZZZNEVER') if MODE == 'dec' else re.compile(r'StaticRef\s*<|ScopedStaticRef\s*<|plugin\s*::\s*Call|CallMethod|CallAndReturn|ResolveCallTarget|NOTSA_PLUGIN_ADDR')
REG = re.compile(r'(?:static )?(?:inline )?const ::notsa::globals::Reg\s+\w+\s*\{[^{}]*\}')
LINE = re.compile(r'^#line (\d+) "(.*)"')

def run(item):
    obj, cmd = item
    name = obj.replace('\\', '_')
    outp = f'{OUT}/{name}.tsv'
    if os.path.exists(outp):
        return obj, 0
    cmd = re.sub(r' /Yu\S+', '', cmd)
    cmd = re.sub(r' /Fp\S+', '', cmd)
    cmd = re.sub(r' /showIncludes', '', cmd)
    cmd = re.sub(r' /Fo\S+', '', cmd)
    cmd = re.sub(r' /Fd\S*', '', cmd)
    cmd = re.sub(r' /MP\d*', '', cmd)
    cmd = re.sub(r' /Z7', '', cmd)
    cmd = cmd.replace('/nologo', '/nologo /E', 1)
    p = subprocess.Popen(cmd, cwd=BUILD, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, encoding='utf-8', errors='replace', shell=True)
    seen = set()
    cur = None; ln = 0
    rows = []
    for line in p.stdout:
        m = LINE.match(line)
        if m:
            ln = int(m.group(1)); cur = m.group(2).replace('\\\\', '/').replace('\\', '/'); continue
        ln += 1
        if cur is None:
            continue
        lc = cur.lower()
        if '/gta-reversed/source/' not in lc and '/gta-reversed/vendor/' not in lc:
            continue
        line = REG.sub('', line)
        hit = None
        if PAT.search(line):
            hit = 'CALL/STATICREF'
        else:
            for h in HEX.finditer(line):
                v = int(h.group(1), 16 if MODE == 'hex' else 10)
                if 0x401000 <= v < 0xCB0000:
                    hit = 'HEX'; break
        if hit:
            key = (cur, ln, line.strip())
            if key in seen: continue
            seen.add(key)
            rows.append((hit, cur, ln, line.strip()[:300]))
    p.wait()
    with open(outp, 'w', encoding='utf-8') as f:
        for r in rows:
            f.write('\t'.join(map(str, r)) + '\n')
    return obj, len(rows)

if ONLY:
    tus = [t for t in tus if ONLY in t[0]]
tus = tus[:LIMIT]
with ThreadPoolExecutor(JOBS) as ex:
    for i, (obj, n) in enumerate(ex.map(run, tus)):
        print(i, obj, n, flush=True)

if '--agg' in sys.argv:
    import glob
    rows = {}
    for f in glob.glob(OUT + '/*.tsv'):
        for l in open(f, encoding='utf-8'):
            k = l.rstrip(chr(10)).split(chr(9))
            if len(k) >= 4:
                rows[(k[1], k[2], k[3])] = rows.get((k[1], k[2], k[3]), 0) + 1
    for (f, ln, t), n in sorted(rows.items()):
        print(chr(9).join([f, ln, str(n), t[:220]]))
