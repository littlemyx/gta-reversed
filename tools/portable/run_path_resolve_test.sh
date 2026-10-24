#!/bin/bash
# MAC-BOOT1 / W-7 host test runner: tests/portable/path_resolve_test.cpp = the REAL source/portable/path_resolve.cpp on a scratch tree. `mutate`: broken resolvers must FAIL.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-${TMPDIR:-/tmp}/path_resolve_test}"; mkdir -p "$OUT"
CLANG="${CLANG:-/opt/homebrew/opt/llvm/bin/clang++}"; [ -x "$CLANG" ] || CLANG=clang++
SDK=(); [ "$(uname)" = Darwin ] && SDK=(-isysroot "$(xcrun --show-sdk-path)")
T="$REPO/tests/portable"; S="$REPO/source/portable"
build() { # $1 = path_resolve.cpp to test (mutants shadow via a copy of the test dir layout), $2 exe
  local d="$OUT/tree_$(basename "$2")"; rm -rf "$d"; mkdir -p "$d/tests/portable" "$d/source/portable"
  cp "$T/path_resolve_test.cpp" "$d/tests/portable/"; cp "$S/path_resolve.h" "$d/source/portable/"; cp "$1" "$d/source/portable/path_resolve.cpp"
  "$CLANG" "${SDK[@]}" -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -w -o "$2" "$d/tests/portable/path_resolve_test.cpp" 2>"$OUT/build.err" || { head -30 "$OUT/build.err"; return 1; }
}
build "$S/path_resolve.cpp" "$OUT/t" || { echo "BUILD FAILED"; exit 1; }
rc=0; ASAN_OPTIONS=detect_leaks=0:symbolize=0 "$OUT/t" || rc=1
[ "${1:-}" = "mutate" ] || exit $rc
[ $rc = 0 ] || { echo "RESULT PATH_RESOLVE FAILED (positive run)"; exit 1; }
bad=0
mut() { # name, old, new
  python3 - "$S/path_resolve.cpp" "$OUT/mut_$1.cpp" "$2" "$3" <<'PY' || { echo "MUTANT $1: pattern not found"; bad=$((bad+1)); return; }
import sys; s=open(sys.argv[1]).read(); assert sys.argv[3] in s, 'pattern'; open(sys.argv[2],'w').write(s.replace(sys.argv[3], sys.argv[4], 1))
PY
  if ! build "$OUT/mut_$1.cpp" "$OUT/m_$1" >/dev/null 2>&1; then echo "MUTANT $1: BUILD ERROR"; bad=$((bad+1))
  elif ASAN_OPTIONS=detect_leaks=0:symbolize=0 "$OUT/m_$1" >"$OUT/m_$1.log" 2>&1; then echo "MUTANT $1: SURVIVED"; bad=$((bad+1)); else echo "MUTANT $1: killed"; fi
}
mut nobackslash "if (c == '\\\\') c = '/';" "if (c == '\\\\') c = '\\\\';"
mut crlf "if (c == '\\r') { cr = true; continue; }" ""
mut ctrlz "if (c == 0x1A) { eof = true; break; }" ""
mut notrim 'comp.resize(n + 1);' ''
mut nolower "c = (char)(c - 'A' + 'a');" "c = c;"
mut nomtime "if (!l.valid || !SameTime(l.mtime, mt)) {" "if (!l.valid) {"
mut trailing "if (norm.back() == '/' && cur.back() != '/') cur += '/';" ""
mut abs "cur = \"/\";" "cur = \"\";"
[ $bad = 0 ] && echo "RESULT PATH_RESOLVE MUTATE OK" || { echo "RESULT PATH_RESOLVE MUTATE FAILED ($bad)"; exit 1; }
