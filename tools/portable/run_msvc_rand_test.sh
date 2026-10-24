#!/bin/bash
# P4-S1 host test runner: tests/portable/msvc_rand_test.cpp (source/portable/msvc_rand.h vs the cl.exe/MSVC-CRT golden tests/portable/msvc_rand_golden.inc).
#   usage: run_msvc_rand_test.sh [mutate]      env: OUT (work dir), CLANG, REGEN=1 (rebuild the golden with cl.exe under Wine first; needs ~/tools/msvc + Wine Staging)
#  - positive run (ASan+UBSan) must print ALL OK
#  - scan: no game source takes the address of / qualifies rand/srand (`&rand`, `std::rand`, `::rand`, `.rand(`...), which the function-like macros could not handle
#  - `mutate`: every source mutant of msvc_rand.h (global state, no 15-bit mask, wrong constant, host RAND_MAX, srand ignored, thread seed != 1, main seed 1) must FAIL the test
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-${TMPDIR:-/tmp}/msvc_rand_test}"; mkdir -p "$OUT"
CLANG="${CLANG:-/opt/homebrew/opt/llvm/bin/clang++}"; [ -x "$CLANG" ] || CLANG=clang++
SDK=(); [ "$(uname)" = Darwin ] && SDK=(-isysroot "$(xcrun --show-sdk-path)")
T="$REPO/tests/portable"
if [ -n "${REGEN:-}" ]; then
  ( export PATH="$HOME/tools/Wine Staging.app/Contents/Resources/wine/bin:$HOME/tools/msvc/bin/x86:$PATH" WINEPREFIX="${WINEPREFIX:-$HOME/.wine-msvc}" WINEDEBUG=-all
    cd "$OUT" && cl /nologo /O2 /MT /EHsc /Fe:gen.exe /Fo:gen.obj "$T/msvc_rand_gen.cpp" >/dev/null && wine ./gen.exe > "$T/msvc_rand_golden.inc" ) || { echo "REGEN FAILED"; exit 1; }
fi
build() { # $1 = include dir that provides portable/msvc_rand.h, $2 = exe
  "$CLANG" "${SDK[@]}" -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -DNOTSA_PORTABLE=1 -Wall -Wno-unused-function \
      -I"$1" -I"$REPO/source" -I"$T" -o "$2" "$T/msvc_rand_test.cpp" 2>"$OUT/build.err" || { head -30 "$OUT/build.err"; return 1; }
}
build "$REPO/source" "$OUT/t" || { echo "BUILD FAILED"; exit 1; }
rc=0
"$OUT/t" || rc=1
# ---- scan of the game sources for spellings the function-like macros cannot cover
hits=$(python3 - "$REPO" <<'PY'
import os, re, sys
repo = sys.argv[1]
roots = ['source/game_sa', 'source/extensions', 'source/toolsmenu', 'source/app', 'source/standalone', 'source/reversiblehooks']
files = [os.path.join(repo, 'source/StdInc.h')]
for r in roots:
    for dp, _, fn in os.walk(os.path.join(repo, r)):
        files += [os.path.join(dp, f) for f in fn if f.endswith(('.cpp', '.h', '.hpp', '.inc'))]
# not a call: `std::rand`, `::rand`, `&rand`, `.rand(`, `->rand(`, bare `rand` / `srand` as a value (function pointer), a declaration `int rand(`
bad = re.compile(r'(std::|::|&|\.|->)\s*s?rand\b|(?<![\w])s?rand\b(?!\s*\()|(?<![\w])(int|void|unsigned)\s+s?rand\s*\(')
for f in files:
    if f.endswith('standalone/GameRand.h') or 'msvc_rand' in f: continue
    for i, line in enumerate(open(f, errors='replace'), 1):
        code = line.split('//')[0]
        if bad.search(code) and not re.search(r'GetRandom|random_device|operand|brand|^\s*/?\*|\* rand\)|rand == -1', code):   # allow: comments, VehicleModelInfo.cpp LOCAL variable named `rand` (function-like macro does not touch it)
            print(f'{os.path.relpath(f, repo)}:{i}: {line.strip()}')
PY
)
echo "scan hits (expect none that are real uses):"; echo "${hits:-  none}"
[ -z "$hits" ] || rc=1
[ "${1:-}" = "mutate" ] || { [ $rc = 0 ] && echo "RESULT MSVC_RAND OK" || echo "RESULT MSVC_RAND FAILED"; exit $rc; }
[ $rc = 0 ] || { echo "RESULT MSVC_RAND FAILED (positive run)"; exit 1; }
# ---- mutants
H="$REPO/source/portable/msvc_rand.h"; bad=0; n=0
mut() { # name, python replace old, new
  n=$((n+1)); d="$OUT/mut_$1/portable"; mkdir -p "$d"
  python3 - "$H" "$d/msvc_rand.h" "$2" "$3" <<'PY' || { echo "MUTANT $1: pattern not found"; bad=$((bad+1)); return; }
import sys; s=open(sys.argv[1]).read()
assert sys.argv[3] in s, 'pattern'
open(sys.argv[2],'w').write(s.replace(sys.argv[3], sys.argv[4], 1))
PY
  if ! build "$OUT/mut_$1" "$OUT/m_$1" >"$OUT/m_$1.build" 2>&1; then
    if grep -q "static assertion failed" "$OUT/m_$1.build"; then echo "MUTANT $1: killed (static_assert)"; else echo "MUTANT $1: BUILD ERROR (not a valid kill)"; bad=$((bad+1)); fi
  elif "$OUT/m_$1" >"$OUT/m_$1.log" 2>&1; then echo "MUTANT $1: SURVIVED"; bad=$((bad+1)); else echo "MUTANT $1: killed ($(grep -c '^FAIL' "$OUT/m_$1.log") failing checks)"; fi
}
mut nomask      '& 0x7FFF);
}
inline void SRand' '& 0xFFFF);
}
inline void SRand'
mut const1      '214013u + 2531011u' '214013u + 2531012u'
mut const2      'holdrand * 214013u' 'holdrand * 214014u'
mut shift       '(h >> 16)' '(h >> 15)'
mut hostmax     '#define RAND_MAX     0x7FFF' '#define RAND_MAX     0x7FFFFFFF'
mut srandnop    'HoldRand() = seed;' '(void)seed;'
mut threadseed  ': 1u; // _initptd: 1' ': 2u;'
mut globalstate 'thread_local uint32_t holdrand' 'static uint32_t holdrand'
mut mainseed    '? MainThreadInitialSeed() : 1u' '? 1u : 1u'
mut srand16     'SRand((unsigned)(seed))' 'SRand((unsigned short)(seed))'
echo "mutants: $n, survivors/problems: $bad"
[ $bad = 0 ] && echo "RESULT MSVC_RAND MUTATE OK" || { echo "RESULT MSVC_RAND MUTATE FAILED"; exit 1; }
