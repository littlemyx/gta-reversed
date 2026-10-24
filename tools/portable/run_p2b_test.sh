#!/bin/bash
# P-2b host tests (not wired in CMake; MAC-W0 owns CMake):
#  (1) tests/portable/p2b_layout_test.cpp : real headers + StdInc, -fsyntax-only (sizeof tScriptParam == 4, SYSTEMTIME == 16, CSimpleVariablesSaveStructure == 0x138, ExtraVertColour == 24 in-memory)
#  (2) the CSimpleVariablesSaveStructure member offsets on arm64 are identical to clang's i686-pc-windows-msvc dump (the x86 exe layout)
#  (3) tests/portable/p2b_prefix_test.cpp : the 10 fixed Save sites emit 4-byte little-endian values; SYSTEMTIME/GetLocalTime
# Compile flags are taken from build/PortableMac/compile_commands.json (needs a configured PortableMac build dir for the -D/-I set).
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COMPDB="${COMPDB:-$REPO/build/PortableMac/compile_commands.json}"
OUT="${OUT:-${TMPDIR:-/tmp}/p2b_test}"; mkdir -p "$OUT"
FLAGS=$(python3 - "$COMPDB" <<'PY'
import json, shlex, sys
cc = json.load(open(sys.argv[1]))
e = next(x for x in cc if x['file'].endswith('source/game_sa/SimpleVariablesSaveStructure.cpp'))
c = shlex.split(e['command']); keep = [c[0]]; i = 1
while i < len(c):
    a = c[i]
    if a in ('-o', '-MF', '-MT', '-MQ'): i += 2; continue
    if a == '-Xclang' and c[i+1] in ('-include-pch', '-include'): i += 4; continue      # drop the PCH (-Xclang -include-pch -Xclang X / -Xclang -include -Xclang X)
    if a in ('-c', '-MD', '-MMD', '-Winvalid-pch') or a.endswith('.cpp'): i += 1; continue
    keep.append(a); i += 1
print(' '.join(shlex.quote(k) for k in keep))
PY
)
STUB="$REPO/tools/standalone/winstub"      # same Windows-header stubs as the native census (clang_census.py)
LFLAGS="$FLAGS -isystem $STUB -include $STUB/winstub_prefix.h -I/opt/homebrew/include -Wno-everything -ferror-limit=0"
fail=0
echo "== (1)+(2) layout on the real headers (portable arm64) and vs i686-pc-windows-msvc"
eval "$LFLAGS -fsyntax-only -Xclang -fdump-record-layouts-complete '$REPO/tests/portable/p2b_layout_test.cpp'" >"$OUT/arm64_dump.txt" 2>"$OUT/arm64_err.txt"
python3 - "$REPO" "$OUT" <<'PY' || fail=1
# The whole engine (StdInc) is parsed, so OTHER slices' known _FILE asserts still fire (that is the census' job). This test requires: no error in the test file itself,
# no error naming one of the P-2b structs, and the arm64 member offsets of CSimpleVariablesSaveStructure == the i686 ones.
import re, subprocess, sys
repo, out = sys.argv[1], sys.argv[2]
err = open(out + '/arm64_err.txt', errors='replace').read()
mine = ('tScriptParam', 'CSimpleVariablesSaveStructure', 'SystemTime', 'ExtraVertColour', 'HeapBlockDesc', 'p2b_layout_test.cpp')
bad = [l for l in err.splitlines() if ' error:' in l and any(m in l for m in mine)]
nerr = len([l for l in err.splitlines() if ' error:' in l])
print('arm64: %d errors in the whole TU (other slices), %d naming the P-2b structs / test file' % (nerr, len(bad)))
for l in bad[:10]: print('  ', l)

def records(text, name):
    """-fdump-record-layouts-complete: {member: offset} of the top-level members of the first record called `name`, and its sizeof."""
    lines = text.splitlines(); res = {}; size = None; inside = False
    for l in lines:
        if not inside:
            if re.match(r'\s*0 \| (class|struct|union) %s$' % re.escape(name), l): inside = True
            continue
        m = re.match(r'\s*(\d+)(?::\d+-\d+)? \| {1,3}(?! )(?:[\w:<>_ ,*&]+?) (\w+)(\[\d+\])?$', l)   # exactly one level of nesting (2 spaces after the bar)
        if m: res[m.group(2)] = int(m.group(1))
        sz = re.search(r'\[sizeof=(\d+)', l)
        if sz: size = int(sz.group(1)); break
    return res, size

a64, a64size = records(open(out + '/arm64_dump.txt', errors='replace').read(), 'CSimpleVariablesSaveStructure')
src = '''
typedef unsigned char uint8; typedef signed char int8; typedef unsigned short uint16; typedef short int16; typedef unsigned int uint32; typedef int int32; typedef unsigned char GxtChar;
struct CVector { float x, y, z; };
enum eWeatherType : short { W_ };
enum class eWantedLevel : unsigned { WL_ };
struct SaveSystemTime { unsigned short a,b,c,d,e,f,g,h; };
'''
h = open(repo + '/source/game_sa/SimpleVariablesSaveStructure.h').read()
h = re.sub(r'#(ifdef NOTSA_PORTABLE|else|endif)[^\n]*\n', '', h)
h = re.sub(r'#include[^\n]*\n|#pragma once|class CVector;', '', h)
h = re.sub(r'using SaveSystemTime[^\n]*\n', '', h)
h = re.sub(r'VALIDATE_SIZE_FILE\([^\n]*\n', '', h)
open(out + '/ref_i686.cpp', 'w').write(src + h)
r = subprocess.run([__import__('os').environ.get('CLANG', '/opt/homebrew/opt/llvm/bin/clang++'), '--target=i686-pc-windows-msvc', '-fms-extensions', '-std=c++23', '-fsyntax-only', '-Xclang', '-fdump-record-layouts-complete', out + '/ref_i686.cpp'], capture_output=True, text=True)
x86, x86size = records(r.stdout, 'CSimpleVariablesSaveStructure')
diff = [(k, x86.get(k), v) for k, v in a64.items() if x86.get(k) != v]
print('CSimpleVariablesSaveStructure: arm64 sizeof=%s i686 sizeof=%s; %d members compared, %d mismatches' % (a64size, x86size, len(a64), len(diff)))
for d in diff: print('  MISMATCH', d)
ok = not bad and a64size == 0x138 and x86size == 0x138 and len(a64) >= 30 and not diff
print('layout OK' if ok else 'layout FAILED')
sys.exit(0 if ok else 1)
PY
echo "== (3) prefix / systemtime host test"
BASE=$(python3 - "$COMPDB" <<'PY'
import json, shlex, sys
cc = json.load(open(sys.argv[1]))
e = next(x for x in cc if x['file'].endswith('source/game_sa/Pickups.cpp'))
c = shlex.split(e['command']); keep = [c[0]]; i = 1
while i < len(c):
    a = c[i]
    if a in ('-arch', '-isysroot'): keep += [a, c[i+1]]; i += 2; continue
    if a.startswith('-D') or a.startswith('-std=') or a.startswith('-stdlib=') or a in ('-fms-extensions', '-mms-bitfields', '-fsigned-char', '-fwrapv', '-fno-delayed-template-parsing'): keep.append(a)
    i += 1
print(' '.join(shlex.quote(k) for k in keep))
PY
)
eval "$BASE -I$REPO/source -I$REPO/source/game_sa -Wno-everything -Wuser-defined-warnings -DNOTSA_FILE_SCALAR_STRICT -DREPO_DIR=$REPO -o '$OUT/prefix' '$REPO/tests/portable/p2b_prefix_test.cpp'" || { echo "BUILD FAILED"; exit 1; }
"$OUT/prefix" || fail=1
[ $fail = 0 ] && echo "ALL OK" || echo "SOME FAILED"
exit $fail
