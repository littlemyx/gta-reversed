#!/bin/bash
# SH2: configure + build the 64-bit SSE RR reference build (PortableWin64 + GTASA_STATEHASH + GTASA_SCMTRACE + GTASA_MDRIVER + GTASA_RR_PORTABLE_MATH, /fp:precise, no x87).
# usage: tools/standalone/statehash/shbuild.sh [build-dir (default build/PortableWin64RR)]      (run it as a background job: a full build is ~15-30 min at -j4 / low priority)
# Output: <build-dir>/bin/gta_reversed.exe.  Needs the conan deps of the PortableWin64 gate (tools/portable/win64/conan_install_win64.sh, cached in ~/.conan2);
# the conan generators dir is copied from the main checkout's build/PortableWin64 (absolute ~/.conan2 paths inside) when the worktree has none.
# Never builds in the main checkout. env: MAIN_CHECKOUT (default C:/Users/littl/github/gta-reversed), JOBS (default 4).
set -u
REPO="$(cd "$(dirname "$0")/../../.." && pwd)"; cd "$REPO" || exit 2
BD="${1:-build/PortableWin64RR}"; JOBS="${JOBS:-4}"; MAIN="${MAIN_CHECKOUT:-C:/Users/littl/github/gta-reversed}"
[ "$(cd "$REPO" && pwd -W 2>/dev/null | tr 'A-Z' 'a-z')" = "$(cd "$MAIN" && pwd -W 2>/dev/null | tr 'A-Z' 'a-z')" ] && { echo "refusing to build in the main checkout"; exit 2; }
export MSYS2_ARG_CONV_EXCL='*'
GEN="build/PortableWin64/generators"
[ -f "$GEN/conan_toolchain.cmake" ] || { mkdir -p build/PortableWin64 && cp -r "$MAIN/build/PortableWin64/generators" build/PortableWin64/ || exit 3; }
(
  export GTA_MSVC_ARCH=x64; source tools/standalone/windows/msvcenv.sh >/dev/null 2>&1 || exit 97
  command -v cl >/dev/null 2>&1 || exit 97
  PY="${GTASA_PYTHON:-python}"
  if [ ! -f "$BD/build.ninja" ] || [ "${RECONF:-0}" = 1 ]; then
    cmake --preset PortableWin64 -B "$BD" -DGTASA_PYTHON="$PY" -DGTASA_STATEHASH=ON -DGTASA_SCMTRACE=ON -DGTASA_MDRIVER=ON -DGTASA_RR_PORTABLE_MATH=ON \
      "-DCMAKE_CXX_FLAGS=/WX- /wd4005 /DWIN32 /D_WINDOWS /EHsc /fp:precise" || exit 4
  fi
  # low priority (BelowNormal; children inherit), output to <build-dir>/build.log, waits for the end
  BDW="$(cygpath -w "$REPO/$BD")"; NJ="$(command -v ninja)"
  powershell -NoProfile -Command "\$p = Start-Process -FilePath '$(cygpath -w "$NJ")' -ArgumentList '-C','$BDW','-k','0','-j$JOBS' -RedirectStandardOutput '$BDW\build.log' -RedirectStandardError '$BDW\build.err' -NoNewWindow -PassThru; \$p.PriorityClass = 'BelowNormal'; \$p.WaitForExit()"
)
echo "shbuild: $( [ -f "$BD/bin/gta_reversed.exe" ] && echo "exe $(ls -l "$BD/bin/gta_reversed.exe" | awk '{print $5, $6, $7, $8}')" || echo "NO EXE" )"
