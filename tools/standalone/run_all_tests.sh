#!/bin/bash
# Build and run every *_test target (rw_*_test, game/review/script oracle tests) under Wine; print a summary table.
# usage: tools/standalone/run_all_tests.sh [build-dir (default build/StandaloneRW)] [test-name-filter-regex]
# env:   SCRATCH   scratchpad holding the shared build mutex (build.lock)
#        SKIP_BUILD=1  run only, do not build     ATTEMPTS=6   retries on hang/crash (known wined3d flake)
#        ASSETS=<dir>  dir with infernus.dff/male01.dff/vgsnbuild07.dff (passed to the model-reading rw tests); if it does not exist and
#                      GAME_DIR=<game install dir> is set, the three DFFs are extracted from $GAME_DIR/models/gta3.img (tools/standalone/extract_img.py)
#        GTASA_PYTHON  python with unicorn+capstone (default: python3)   MSVC_WINE_ROOT  msvc-wine dir (default ~/tools/msvc)
#        WINE_BIN      dir of the wine binary to prepend to PATH (optional)   NINJA  ninja path (default: ninja from PATH)
# Exit code: non-zero if any test reports failures/mismatches, never produced a result, or failed to build.
# A fresh build dir is configured exactly like build/StandaloneRW (librw ON, SDL3, Debug via the conan toolchain).
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BDIR="${1:-build/StandaloneRW}"; FILTER="${2:-.}"
case "$BDIR" in /*) ;; *) BDIR="$REPO/$BDIR";; esac
SCRATCH="${SCRATCH:-${TMPDIR:-/tmp}/gta-standalone-scratch}"; mkdir -p "$SCRATCH"
MSVC_WINE_ROOT="${MSVC_WINE_ROOT:-$HOME/tools/msvc}"; GTASA_PYTHON="${GTASA_PYTHON:-python3}"; NINJA="${NINJA:-$(command -v ninja)}"
ATTEMPTS="${ATTEMPTS:-6}"
ASSETS="${ASSETS:-$SCRATCH/assets}"
if [ ! -f "$ASSETS/infernus.dff" ] && [ -f "${GAME_DIR:-/nonexistent}/models/gta3.img" ]; then
  python3 "$REPO/tools/standalone/extract_img.py" "$GAME_DIR/models/gta3.img" "$ASSETS" infernus.dff male01.dff vgsnbuild07.dff
fi
[ -n "${WINE_BIN:-}" ] && PATH="$WINE_BIN:$PATH"
export PATH WINEPREFIX="${WINEPREFIX:-$HOME/.wine-msvc}" WINEDEBUG=-all
EXE="${GTA_EXE:-$REPO/gta_sa_compact.exe}"
export RW_EXE_ORACLE="Z:${EXE//\//\\}"
OUT="$SCRATCH/run_all_tests.$$"; mkdir -p "$OUT"

# conan toolchain: from this checkout, else from the main worktree (a linked worktree has no build/Debug)
TOOLCHAIN="${CONAN_TOOLCHAIN:-$REPO/build/Debug/generators/conan_toolchain.cmake}"
[ -f "$TOOLCHAIN" ] || TOOLCHAIN="$(git -C "$REPO" worktree list --porcelain | sed -n '1s/^worktree //p')/build/Debug/generators/conan_toolchain.cmake"
lock()   { until mkdir "$SCRATCH/build.lock" 2>/dev/null; do sleep 15; done; }
unlock() { rmdir "$SCRATCH/build.lock" 2>/dev/null; }
trap 'unlock_if_held' EXIT
HELD=0; unlock_if_held() { [ "$HELD" = 1 ] && unlock; }

if [ "${SKIP_BUILD:-0}" != 1 ]; then
  lock; HELD=1
  if [ ! -f "$BDIR/build.ninja" ]; then
    mkdir -p "$BDIR"
    cmake -S "$REPO" -B "$BDIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_MAKE_PROGRAM="$NINJA" \
      "-DCMAKE_CXX_FLAGS=/WX- /wd4005 /DWIN32 /D_WINDOWS /Zm1000 /GX" \
      -DGTASA_STANDALONE=ON -DGTASA_RW_LIBRW=ON -DGTASA_RW_LIBRW_OPT_OUT=OFF -DGTASA_USE_SDL3=ON \
      -DGTASA_ORIGINAL_EXE="$EXE" -DGTASA_PYTHON="$GTASA_PYTHON" -DGTASA_ML="$MSVC_WINE_ROOT/bin/x86/ml" \
      -DGTASA_WITH_OPENAL=OFF -DGTASA_WITH_CLEO_COMMANDS=OFF -DGTASA_WITH_SCRIPT_COMMAND_HOOKS=OFF -DGTASA_WITH_LTO=OFF -DGTASA_UNITY_BUILD=OFF \
      > "$OUT/configure.log" 2>&1 || { echo "configure failed, see $OUT/configure.log"; exit 2; }
  fi
fi
TESTS=$(ninja -C "$BDIR" -t targets all 2>/dev/null | sed -n 's/^\([A-Za-z0-9_]*_test\): phony$/\1/p' | sort -u | grep -v "^cmake_" | grep -E "$FILTER")
[ -n "$TESTS" ] || { echo "no *_test targets found in $BDIR"; exit 2; }
BUILD_FAIL=""
if [ "${SKIP_BUILD:-0}" != 1 ]; then
  # shellcheck disable=SC2086
  ninja -C "$BDIR" -k 0 $TESTS > "$OUT/ninja.log" 2>&1 || BUILD_FAIL=1
  unlock; HELD=0
  [ -n "$BUILD_FAIL" ] && echo "ninja reported errors (see $OUT/ninja.log); tests whose exe is missing are marked NOBUILD"
fi

# parse: prints "checks fails mismatches" or nothing when no result line exists in the output
parse() { python3 - "$1" <<'PY'
import re,sys
t=open(sys.argv[1],errors='replace').read()
c=f=m=None
for x in re.finditer(r'(\d+) checks passed, (\d+) failed',t): c,f=int(x[1]),int(x[2]); m=m or 0
x=re.findall(r'(\d+) commands, (\d+) with mismatches',t)
if x: c,m=int(x[-1][0]),int(x[-1][1]); f=f or 0
x=re.findall(r'(\d+) (?:functions|rows), mismatches \(strict / excluding NaN-payload-only(?:, INFO rows excluded)?\): PC24 (\d+) / (\d+), PC53 (\d+) / (\d+)',t)
if x: c,m=int(x[-1][0]),int(x[-1][2]); f=f or 0     # PC24 (game FPU mode) excluding NaN-payload-only; PC53 (CRT default) is informational, the tests' own exit code ignores it too
x=re.findall(r'(\d+) commands, PC24 mismatches \(excluding NaN-payload-only\): (\d+)',t)
if x: c,m=int(x[-1][0]),int(x[-1][1]); f=f or 0
x=re.findall(r'(?m)^TOTAL: (\d+) \w+, (\d+) mismatches',t)               # skin oracle tests (cpu / hw)
if x: c,m=int(x[-1][0]),int(x[-1][1]); f=f or 0
x=re.findall(r'(?m)^[A-Za-z0-9_]+: (OK|FAILED) \((\d+) failures?\)',t)    # rw_skin_vs_test
if x: f=int(x[-1][1]); c=c or 1; m=m or 0
if re.search(r'(?m)^FAILED \(no device\)',t): c=c or 0; f=(f or 0)+1; m=m or 0   # rw_platform_test without a D3D device
x=re.findall(r'(?m)^(PASSED|FAILED) \((\d+) failed\)',t)
if x and c is None: c=len(re.findall(r'(?m)^(?:ok|FAIL)\s',t)); f=int(x[-1][1]); m=0
if c is not None: print(c,f or 0,m or 0)
PY
}

MODEL_TESTS=" rw_custom_pipelines_test rw_matfx_pipeline_test rw_matfx_uvanim_test rw_atomic_clump_test rw_skin_hanim_test rw_skin_pipeline_test rw_pipeline_test rw_rtanim_rtquat_test rw_math_stream_test "
ROWS=""; RC=0
for t in $TESTS; do
  exe="$BDIR/source/$t.exe"
  if [ ! -f "$exe" ]; then ROWS+="$t - - - 0 0 NOBUILD"$'\n'; RC=1; continue; fi
  args=()
  if [[ "$MODEL_TESTS" == *" $t "* ]]; then
    case "$t" in rw_skin_hanim_test|rw_skin_pipeline_test|rw_rtanim_rtquat_test) models="male01";; *) models="infernus male01 vgsnbuild07";; esac   # these two need skinned models
    for f in $models; do [ -f "$ASSETS/$f.dff" ] && args+=("Z:${ASSETS//\//\\}\\$f.dff"); done; fi
  case "$t" in logic_oracle_test) TMO=${TIMEOUT:-3000};; *oracle*) TMO=${TIMEOUT:-900};; rw_skin_vs_test) TMO=${TIMEOUT:-900};; *) TMO=${TIMEOUT:-180};; esac   # vs_test: ~2.2M exe-composer keys (> 180 s under Wine); logic_oracle_test: ~224 functions x >=1000 cases, ~20 min under Wine
  tv="TIMEOUT_$t"; [ -n "${!tv:-}" ] && TMO=${!tv}                                                              # per-test override: TIMEOUT_<test>=<seconds>
  start=$SECONDS; status=NORESULT; res=""; n=0
  while [ $n -lt "$ATTEMPTS" ]; do
    n=$((n+1)); log="$OUT/$t.$n.log"
    ( cd "$BDIR/source" && exec wine "$t.exe" "${args[@]+"${args[@]}"}" ) > "$log" 2>&1 &
    pid=$!; code=""
    for ((s=0; s<TMO; s++)); do sleep 1; kill -0 $pid 2>/dev/null || { wait $pid; code=$?; break; }; done
    if [ -z "$code" ]; then kill -9 $pid 2>/dev/null; echo "  $t attempt $n: hang >${TMO}s"; code=hang; fi
    res=$(parse "$log")
    if [ -z "$res" ] || [ "$code" = hang ]; then pkill -9 -f "$t.exe" 2>/dev/null; pkill -9 winedbg 2>/dev/null; sleep 1; fi
    if [ -n "$res" ] && [ "$code" != hang ]; then break; fi
    res=""; [ "$code" != hang ] && echo "  $t attempt $n: crash/no result (exit $code)"
  done
  if [ -z "$res" ]; then ROWS+="$t - - - $n $((SECONDS-start)) NORESULT"$'\n'; RC=1; continue; fi
  set -- $res
  if [ "$2" -gt 0 ] || [ "$3" -gt 0 ]; then status=FAIL; RC=1
  elif [ "$code" != 0 ]; then status="EXIT$code"; RC=1      # a result line with no failures but a non-zero exit code (77 = exe oracle not available)
  else status=ok; fi
  ROWS+="$t $1 $2 $3 $n $((SECONDS-start)) $status"$'\n'
done
echo; printf '%-30s %8s %9s %10s %8s %7s  %s\n' test checks failures mismatches attempts seconds status
printf '%s' "$ROWS" | while read -r a b c d e f g; do printf '%-30s %8s %9s %10s %8s %7s  %s\n' "$a" "$b" "$c" "$d" "$e" "$f" "$g"; done
echo "logs: $OUT"
exit $RC
