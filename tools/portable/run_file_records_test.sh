#!/bin/bash
# Build and run tests/portable/file_records_test.cpp (P-2a). Not wired in CMake (MAC-W0 owns the CMake files) - open item: add_test/one-liner later.
# Compile flags (compiler, -std, -arch, -isysroot, -stdlib, -D set, ms-extension flags) are taken from build/PortableMac/compile_commands.json.
# Steps: (1) positive test must print fails=0; (2) warning mode: FR_WARN_CASES must produce exactly 4 deprecated-guard warnings on LP64;
#        (3) every FR_NEG=1..6 (NOTSA_FILE_SCALAR_STRICT) must FAIL to compile with the guard's static_assert message.
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
# (2) warning mode
n=$(eval "$CXX $REST $INC -Wno-everything -Wuser-defined-warnings -DFR_WARN_CASES -o '$OUT/tw' '$SRC'" 2>&1 | grep -c "warning: 64-bit-unsafe")
echo "warn-mode guard warnings: $n (expect 4)"; [ "$n" = 4 ] || fail=1
"$OUT/tw" || fail=1
# (3) strict mode negative cases
for k in 1 2 3 4 5 6; do
  if eval "$CXX $REST $INC -Wno-everything -DFR_NEG=$k -fsyntax-only '$SRC'" >"$OUT/neg$k.log" 2>&1; then
    echo "NEG $k: UNEXPECTEDLY COMPILED"; fail=1
  elif grep -q "64-bit-unsafe scalar" "$OUT/neg$k.log"; then echo "NEG $k: rejected by the guard (ok)"
  else echo "NEG $k: failed for another reason:"; head -5 "$OUT/neg$k.log"; fail=1; fi
done
[ $fail = 0 ] && echo "ALL OK" || echo "SOME FAILED"
exit $fail
