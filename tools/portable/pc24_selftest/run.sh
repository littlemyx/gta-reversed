#!/bin/bash
# self-test of tools/portable/pc24_scan.py: scan + apply the fixture in a scratch copy, assert the expected rows / rewrites, then assert idempotency and that both flavours compile.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../.." && pwd)"
CXX="${CXX:-/opt/homebrew/opt/llvm/bin/clang++}"
SDK="${SDKROOT:-$(xcrun --show-sdk-path 2>/dev/null || true)}"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
cp "$HERE/fixture.cpp" "$T/fixture.cpp"
FL="-std=c++23 -w -DNOTSA_PORTABLE=1 ${SDK:+-isysroot $SDK}"
python3 - "$T" "$CXX" "$FL" <<'PY'
import json, sys
T, cxx, fl = sys.argv[1:4]
json.dump([{"directory": T, "file": T + "/fixture.cpp", "command": f"{cxx} {fl} -c {T}/fixture.cpp -o {T}/fixture.o"}], open(T + "/cc.json", "w"))
PY
SCAN="python3 $REPO/tools/portable/pc24_scan.py --cc $T/cc.json --only fixture\\.cpp\$ -j 1 --no-pch-check"
$SCAN --out "$T/before.tsv" 2>/dev/null
fail=0
want() { if ! grep -qE "$1" "$T/$2"; then echo "FAIL: $3 (pattern: $1 in $2)"; fail=1; fi; }
want "LOCAL-CHECK-D.*double local esc[[:space:]].*address escapes" before.tsv "esc bound to a non-const reference stays double"
want "LOCAL-CHECK-D.*double local viaAddr.*(address escapes|AddrOf)" before.tsv "AddrOf(local) stays double"
want "LOCAL-AUTO.*double local loc[[:space:]]" before.tsv "loc (const-ref use only) is retyped"
want "MATH-OVERLOAD.*log10" before.tsv "log10 overload change reported"
want "INT-WIDE.*int" before.tsv "wide int reported"
$SCAN --apply --out "$T/after.tsv" 2>/dev/null
grep -q "X87W(" "$T/fixture.cpp" || { echo "FAIL: log10 argument not wrapped in X87W"; fail=1; }
grep -q "std::log10(X87W(" "$T/fixture.cpp" || { echo "FAIL: X87W is not directly inside log10("; fail=1; }
grep -q "double esc" "$T/fixture.cpp" || { echo "FAIL: esc was retyped"; fail=1; }
grep -q "double viaAddr" "$T/fixture.cpp" || { echo "FAIL: viaAddr was retyped"; fail=1; }
grep -q "x87d loc" "$T/fixture.cpp" || { echo "FAIL: loc was not retyped"; fail=1; }
grep -q "(x87d)ARGM(a, b)" "$T/fixture.cpp" || { echo "FAIL: cast of a macro expansion not rewritten"; fail=1; }
grep -q "const double ad = std::fabs(dglob)" "$T/fixture.cpp" || { echo "FAIL: fabs of a double global was rewritten/wrapped"; fail=1; }
grep -q "const double t = (double)a \* 3.0; // pc53" "$T/fixture.cpp" || { echo "FAIL: pc53 line touched"; fail=1; }
# idempotency: a second apply changes nothing
cp "$T/fixture.cpp" "$T/once.cpp"
$SCAN --apply --out "$T/again.tsv" 2>/dev/null
cmp -s "$T/once.cpp" "$T/fixture.cpp" || { echo "FAIL: second --apply edited the file again"; diff "$T/once.cpp" "$T/fixture.cpp" | head; fail=1; }
# both flavours compile
$CXX $FL -fsyntax-only "$T/fixture.cpp" || { echo "FAIL: portable flavour does not compile"; fail=1; }
$CXX -std=c++23 -w ${SDK:+-isysroot $SDK} -fsyntax-only "$T/fixture.cpp" || { echo "FAIL: Win32 flavour does not compile"; fail=1; }
[ $fail = 0 ] && echo "pc24_scan self-test: OK" || { echo "pc24_scan self-test: FAILED"; exit 1; }
