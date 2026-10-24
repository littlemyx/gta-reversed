#!/bin/bash
# Build and run tests/portable/file_records_test.cpp (P-2a). Not wired in CMake (MAC-W0 owns the CMake files) - open item: add_test/one-liner later.
# Compile flags (compiler, -std, -arch, -isysroot, -stdlib, -D set, ms-extension flags) are taken from build/PortableMac/compile_commands.json.
# NOTSA_FILE_SCALAR_STRICT is on globally in PortableMac, so the guard is an error (not a warning). Steps:
#  (1) positive test must compile with 0 guard diagnostics and print fails=0;
#  (2) the 4 former warn cases (FR_WARN=1..4: size_t, ptrdiff_t, pointer, unsigned long) must each FAIL to compile with exactly 1 guard error and no other error;
#  (3) every FR_NEG=1..6 must FAIL to compile with the guard's message.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
COMPDB="${COMPDB:-$REPO/build/PortableMac/compile_commands.json}"
OUT="${OUT:-${TMPDIR:-/tmp}/file_records_test}"
mkdir -p "$OUT"
FLAGS=$(python3 - "$COMPDB" <<'PY'
import json, shlex, sys
cc = json.load(open(sys.argv[1]))
e = next(x for x in cc if x['file'].endswith('source/game_sa/Pickups.cpp'))
c = shlex.split(e['command'])
keep, i = [c[0]], 1
while i < len(c):
    a = c[i]
    if a in ('-arch', '-isysroot'):
        keep += [a, c[i + 1]]; i += 2; continue
    if a.startswith('-D') or a.startswith('-std=') or a.startswith('-stdlib=') or a in ('-fms-extensions', '-mms-bitfields', '-fsigned-char', '-fwrapv', '-fno-delayed-template-parsing'):
        keep.append(a)
    i += 1
print(' '.join(shlex.quote(k) for k in keep))
PY
)
CXX="${FLAGS%% *}"; REST="${FLAGS#* }"
SRC="$REPO/tests/portable/file_records_test.cpp"
INC="-I$REPO/source/game_sa"
fail=0
eval "$CXX $REST $INC -Wno-everything -o '$OUT/t' '$SRC'" || { echo "BUILD FAILED"; exit 1; }
"$OUT/t" || fail=1
# (1b) positive build must carry 0 guard diagnostics (warnings or errors)
n=$(eval "$CXX $REST $INC -Wno-everything -Wuser-defined-warnings -fsyntax-only '$SRC'" 2>&1 | grep -c "64-bit-unsafe")
echo "positive build guard diagnostics: $n (expect 0)"; [ "$n" = 0 ] || fail=1
# (2) former warn cases, now errors under the global NOTSA_FILE_SCALAR_STRICT: each fails with exactly one guard error and nothing else
for k in 1 2 3 4; do
  if eval "$CXX $REST $INC -Wno-everything -DNOTSA_FILE_SCALAR_STRICT -DFR_WARN=$k -fsyntax-only '$SRC'" >"$OUT/warn$k.log" 2>&1; then
    echo "WARN-CASE $k: UNEXPECTEDLY COMPILED"; fail=1
  else
    g=$(grep -c "error: 64-bit-unsafe scalar" "$OUT/warn$k.log"); e=$(grep -c "error:" "$OUT/warn$k.log")
    if [ "$g" = 1 ] && [ "$e" = 1 ]; then echo "WARN-CASE $k: rejected by the guard, 1 error (ok)"
    else echo "WARN-CASE $k: guard errors=$g, total errors=$e (expect 1/1):"; head -8 "$OUT/warn$k.log"; fail=1; fi
  fi
done
# (3) negative cases
for k in 1 2 3 4 5 6; do
  if eval "$CXX $REST $INC -Wno-everything -DNOTSA_FILE_SCALAR_STRICT -DFR_NEG=$k -fsyntax-only '$SRC'" >"$OUT/neg$k.log" 2>&1; then
    echo "NEG $k: UNEXPECTEDLY COMPILED"; fail=1
  elif grep -q "64-bit-unsafe scalar" "$OUT/neg$k.log"; then echo "NEG $k: rejected by the guard (ok)"
  else echo "NEG $k: failed for another reason:"; head -5 "$OUT/neg$k.log"; fail=1; fi
done
[ $fail = 0 ] && echo "ALL OK" || echo "SOME FAILED"
exit $fail
