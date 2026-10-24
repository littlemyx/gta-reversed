#!/bin/bash
# P4-S3 host test runner: tests/portable/p4_restore_test.cpp on the tRestoreEntry text extracted from the REAL source/game_sa/Models/VehicleModelInfo.h (NOTSA_PORTABLE branch). `mutate`: variants must FAIL.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-${TMPDIR:-/tmp}/p4_restore_test}"; mkdir -p "$OUT"
CLANG="${CLANG:-/opt/homebrew/opt/llvm/bin/clang++}"; [ -x "$CLANG" ] || CLANG=clang++
SDK=(); [ "$(uname)" = Darwin ] && SDK=(-isysroot "$(xcrun --show-sdk-path)")
T="$REPO/tests/portable"; H="$REPO/source/game_sa/Models/VehicleModelInfo.h"
extract() { python3 - "$H" "$1" <<'PY'
import re, sys
s = open(sys.argv[1]).read()
m = re.search(r'#ifdef NOTSA_PORTABLE\n(?://[^\n]*\n)*struct tRestoreEntry \{.*?\n\};', s, re.S)
assert m, 'portable tRestoreEntry not found'
body = m.group(0).split('\n', 1)[1]
open(sys.argv[2], 'w').write('#include <cstring>\n' + body + '\n')
PY
}
build() { "$CLANG" "${SDK[@]}" -std=c++23 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -w -I"$1" -o "$2" "$T/p4_restore_test.cpp" 2>"$OUT/build.err" || { head -20 "$OUT/build.err"; return 1; }; }
mkdir -p "$OUT/base"; extract "$OUT/base/restore_entry.inc" || exit 1
build "$OUT/base" "$OUT/t" || { echo "BUILD FAILED"; exit 1; }
rc=0; ASAN_OPTIONS=detect_leaks=0:symbolize=0 "$OUT/t" || rc=1
[ "${1:-}" = "mutate" ] || { [ $rc = 0 ] && echo "RESULT P4_RESTORE OK" || echo "RESULT P4_RESTORE FAILED"; exit $rc; }
[ $rc = 0 ] || { echo "RESULT P4_RESTORE FAILED (positive run)"; exit 1; }
bad=0; n=0
mut() { n=$((n+1)); d="$OUT/mut_$1"; mkdir -p "$d"
  python3 - "$OUT/base/restore_entry.inc" "$d/restore_entry.inc" "$2" "$3" <<'PY' || { echo "MUTANT $1: pattern not found"; bad=$((bad+1)); return; }
import sys; s=open(sys.argv[1]).read(); assert sys.argv[3] in s, 'pattern'; open(sys.argv[2],'w').write(s.replace(sys.argv[3], sys.argv[4], 1))
PY
  if ! build "$d" "$OUT/m_$1" >/dev/null 2>&1; then echo "MUTANT $1: BUILD ERROR"; bad=$((bad+1))
  elif ASAN_OPTIONS=detect_leaks=0:symbolize=0 "$OUT/m_$1" >"$OUT/m_$1.log" 2>&1; then echo "MUTANT $1: SURVIVED"; bad=$((bad+1)); else echo "MUTANT $1: killed"; fi; }
mut restore4   'std::memcpy(m_pAddress, &m_nValue, m_nSize);' 'std::memcpy(m_pAddress, &m_nValue, 4);'
mut save8      'std::memcpy(&m_nValue, address, size);' 'std::memcpy(&m_nValue, address, 8);'
mut sizewrong  'm_nSize    = (uint8)size;' 'm_nSize    = 4;'
mut noaddr     'm_pAddress = address;' 'm_pAddress = nullptr;'
echo "mutants: $n, survivors/problems: $bad"
[ $bad = 0 ] && echo "RESULT P4_RESTORE MUTATE OK" || { echo "RESULT P4_RESTORE MUTATE FAILED"; exit 1; }
