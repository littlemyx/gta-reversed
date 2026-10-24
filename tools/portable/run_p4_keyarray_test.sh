#!/bin/bash
# P4-S4 host test runner: tests/portable/p4_keyarray_test.cpp = the REAL Text/KeyArray.cpp on a StdInc stub. `mutate`: pre-fix variants must FAIL.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-${TMPDIR:-/tmp}/p4_keyarray_test}"; mkdir -p "$OUT"
CLANG="${CLANG:-/opt/homebrew/opt/llvm/bin/clang++}"; [ -x "$CLANG" ] || CLANG=clang++
SDK=(); [ "$(uname)" = Darwin ] && SDK=(-isysroot "$(xcrun --show-sdk-path)")
T="$REPO/tests/portable"; G="$REPO/source/game_sa"
build() { # $1 = dir with (optionally mutated) Text/KeyArray.cpp first in the include path, $2 exe
  "$CLANG" "${SDK[@]}" -std=c++23 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -DNOTSA_PORTABLE=1 -w \
     -I"$1" -I"$T/p4_stubs" -I"$G" -I"$G/Core" -I"$G/Text" -o "$2" "$T/p4_keyarray_test.cpp" 2>"$OUT/build.err" || { head -30 "$OUT/build.err"; return 1; }
}
build "$OUT/none" "$OUT/t" || { echo "BUILD FAILED"; exit 1; }
rc=0; ASAN_OPTIONS=detect_leaks=0:symbolize=0 "$OUT/t" || rc=1
[ "${1:-}" = "mutate" ] || { [ $rc = 0 ] && echo "RESULT P4_KEYARRAY OK" || echo "RESULT P4_KEYARRAY FAILED"; exit $rc; }
[ $rc = 0 ] || { echo "RESULT P4_KEYARRAY FAILED (positive run)"; exit 1; }
bad=0; n=0
mut() { # name, old, new  (mutates Text/KeyArray.cpp or .h; the mutated copy shadows the real one via -I order: the test includes "Text/KeyArray.cpp")
  n=$((n+1)); d="$OUT/mut_$1/Text"; mkdir -p "$d"; cp "$G/Text/KeyArray.h" "$d/KeyArray.h"
  python3 - "$G/Text/KeyArray.cpp" "$d/KeyArray.cpp" "$2" "$3" <<'PY' || { echo "MUTANT $1: pattern not found"; bad=$((bad+1)); return; }
import sys; s=open(sys.argv[1]).read(); assert sys.argv[3] in s, 'pattern'; open(sys.argv[2],'w').write(s.replace(sys.argv[3], sys.argv[4], 1))
PY
  if ! build "$OUT/mut_$1" "$OUT/m_$1" >/dev/null 2>&1; then echo "MUTANT $1: BUILD ERROR (not a valid kill)"; bad=$((bad+1))
  elif ASAN_OPTIONS=detect_leaks=0:symbolize=0 "$OUT/m_$1" >"$OUT/m_$1.log" 2>&1; then echo "MUTANT $1: SURVIVED"; bad=$((bad+1)); else echo "MUTANT $1: killed"; fi
}
mut count16   'length / sizeof(CKeyEntry_File)' 'length / sizeof(CKeyEntry)'
mut hashswap  'CKeyEntry{ nullptr, rec.hash, rec.offset }' 'CKeyEntry{ nullptr, rec.offset, rec.hash }'
mut noconsume '*offset += length;
#else' '*offset += length - 1;
#else'
mut updbase   'offset + m_data[i].m_fileOffset' 'offset + 0'
mut nodata    'std::memcpy(&rec, chunk.data() + i * sizeof(CKeyEntry_File), sizeof(rec));' 'std::memcpy(&rec, chunk.data() + (i ? i - 1 : 0) * sizeof(CKeyEntry_File), sizeof(rec));'
echo "mutants: $n, survivors/problems: $bad"
[ $bad = 0 ] && echo "RESULT P4_KEYARRAY MUTATE OK" || { echo "RESULT P4_KEYARRAY MUTATE FAILED"; exit 1; }
