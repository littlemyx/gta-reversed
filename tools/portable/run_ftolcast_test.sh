#!/bin/bash
# MAC-FTOL1 host test: tests/portable/ftolcast_test.cpp = notsa::fp::FtolCast<T> (NOTSA_FTOL) vs a model of the exe's _ftol2 (low dword / low bits, 0x80000000.. for NaN / out of range).
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-${TMPDIR:-/tmp}/ftolcast_test}"; mkdir -p "$OUT"
CLANG="${CLANG:-/opt/homebrew/opt/llvm/bin/clang++}"; [ -x "$CLANG" ] || CLANG=clang++
SDK=(); [ "$(uname)" = Darwin ] && SDK=(-isysroot "$(xcrun --show-sdk-path)")
"$CLANG" "${SDK[@]}" -std=c++23 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -ffp-contract=off -w -o "$OUT/t" "$REPO/tests/portable/ftolcast_test.cpp" 2>"$OUT/build.err" || { head -30 "$OUT/build.err"; echo "BUILD FAILED"; exit 1; }
ASAN_OPTIONS=detect_leaks=0:symbolize=0 "$OUT/t"
