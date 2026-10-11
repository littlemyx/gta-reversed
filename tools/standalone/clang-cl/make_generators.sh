#!/usr/bin/env bash
# Derive build/ClangCL/generators (conan toolchain + CMakeConfigDeps of the clang-cl configuration) from an existing msvc-wine `conan install` (build/Release/generators)
# without running conan again (a second `conan install` rewrites ConanPresets.json and source/libs/imgui under the feet of running builds).
# The dependency packages are the same prebuilt MSVC-ABI static libraries; only the user toolchain / compiler lines of conan_toolchain.cmake change.
# usage: tools/standalone/clang-cl/make_generators.sh [src-generators-dir] [dst-generators-dir]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
SRC="${1:-$ROOT/build/Release/generators}"
DST="${2:-$ROOT/build/ClangCL/generators}"
LLVM="${LLVM_ROOT:-/opt/homebrew/opt/llvm}"
rm -rf "$DST"; mkdir -p "$(dirname "$DST")"; cp -R "$SRC" "$DST"
python3 - "$DST/conan_toolchain.cmake" "$HERE/toolchain-clang-cl.cmake" "$LLVM" <<'PY'
import re, sys
p, tc, llvm = sys.argv[1:4]
s = open(p).read()
s = re.sub(r'include\("[^"]*toolchain-msvc-wine\.cmake"\)', 'include("%s")' % tc, s)
s = re.sub(r'message\(STATUS "Conan toolchain: Including user_toolchain: [^"]*"\)', 'message(STATUS "Conan toolchain: Including user_toolchain: %s")' % tc, s)
s = re.sub(r'set\(CMAKE_(C|CXX)_COMPILER "[^"]*/cl"\)', lambda m: 'set(CMAKE_%s_COMPILER "%s/bin/clang-cl")' % (m.group(1), llvm), s)
open(p, 'w').write(s)
PY
grep -n "clang-cl" "$DST/conan_toolchain.cmake" | head
