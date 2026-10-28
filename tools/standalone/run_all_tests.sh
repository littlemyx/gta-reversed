#!/bin/bash
# Build and run every *_test target (rw_*_test, game/review/script oracle tests) under Wine (macOS/Linux) or directly (native Windows, Git Bash); print a summary table.
# usage: tools/standalone/run_all_tests.sh [build-dir (default build/StandaloneRW)] [test-name-filter-regex]
# env:   SCRATCH   scratchpad holding the shared build mutex (build.lock)
#        SKIP_BUILD=1  run only, do not build   BUILD_ONLY=1  build only, do not run     ATTEMPTS=6   retries on hang/crash (known wined3d flake)
#        ASSETS=<dir>  dir with infernus.dff/male01.dff/vgsnbuild07.dff (passed to the model-reading rw tests); if it does not exist and
#                      GAME_DIR=<game install dir> is set, the three DFFs are extracted from $GAME_DIR/models/gta3.img (tools/standalone/extract_img.py)
#        GTASA_PYTHON  python with unicorn+capstone (default: python3, or python)   MSVC_WINE_ROOT  msvc-wine dir (default $TOOLS_DIR/msvc, TOOLS_DIR default ~/tools)  [Mac/Linux]
#        WINE_BIN      dir of the wine binary to prepend to PATH (optional) [Mac/Linux]   NINJA  ninja path (default: ninja from PATH)
#        Windows host: run it from a shell with the MSVC x86 environment (tools/standalone/windows/gtash.cmd or vcvarsall x86): `cl` must be on PATH. No Wine/WINEPREFIX;
#        SCRATCH defaults to C:/gta-scratch; RW_EXE_ORACLE is the native Windows path of the exe. See docs/WINDOWS_SETUP.md.
#        GTA_JOBS=4    Windows only: ninja -j for the test build (default 4; the PC is shared with the user, a higher value freezes the desktop)
#        GTA_EXE=<path>  the original gta_sa.exe (1.0 US compact) for the exe-oracle tests; default <repo>/gta_sa_compact.exe. It is OPTIONAL (slice G1): the build, the run exe and the
#                      non-oracle tests never need it. A RW_EXE_ORACLE inherited from the environment is ignored. When the file does not exist the exe-oracle tests (they exit 77
#                      "oracle not available") are reported as SKIP(no_exe), never as ok, and do not affect the exit code.
# Exit code: non-zero if any test reports failures/mismatches, never produced a result, or failed to build. SKIP rows (no exe) do not count as failures.
# A fresh build dir is configured exactly like build/StandaloneRW (librw ON, SDL3, Debug via the conan toolchain).
set -u
. "$(dirname "$0")/hostenv.sh"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BDIR="$(hp "${1:-build/StandaloneRW}")"; FILTER="${2:-.}"
case "$BDIR" in /*|[A-Za-z]:*) ;; *) BDIR="$REPO/$BDIR";; esac
SCRATCH="$(hp "${SCRATCH:-$DEF_SCRATCH}")"; mkdir -p "$SCRATCH"
MSVC_WINE_ROOT="${MSVC_WINE_ROOT:-$TOOLS_DIR/msvc}"; GTASA_PYTHON="$PY"; NINJA="${NINJA:-$(command -v ninja)}"
if [ "$HOST" = windows ]; then
  REPO="$(hp "$REPO")"; BDIR="$(hp "$BDIR")"; NINJA="$(hp "$NINJA")"
  if [ "${SKIP_BUILD:-0}" != 1 ] && ! command -v cl >/dev/null 2>&1; then
    echo "run_all_tests: cl.exe not on PATH - start this from the MSVC x86 environment (tools/standalone/windows/gtash.cmd, or: source tools/standalone/windows/msvcenv.sh), see docs/WINDOWS_SETUP.md"; exit 2
  fi
fi
ATTEMPTS="${ATTEMPTS:-6}"
ASSETS="${ASSETS:-$SCRATCH/assets}"
if [ ! -f "$ASSETS/infernus.dff" ] && [ -f "${GAME_DIR:-/nonexistent}/models/gta3.img" ]; then
  "$PY" "$REPO/tools/standalone/extract_img.py" "$GAME_DIR/models/gta3.img" "$ASSETS" infernus.dff male01.dff vgsnbuild07.dff
fi
if [ "$HOST" != windows ]; then
  [ -n "${WINE_BIN:-}" ] && PATH="$WINE_BIN:$PATH"
  export PATH WINEPREFIX="${WINEPREFIX:-$HOME/.wine-msvc}" WINEDEBUG=-all
fi
EXE="$(hp "${GTA_EXE:-$REPO/gta_sa_compact.exe}")"
unset RW_EXE_ORACLE            # only GTA_EXE / <repo>/gta_sa_compact.exe count, never a stray variable of the calling shell
if [ -f "$EXE" ]; then export RW_EXE_ORACLE="$(wp "$EXE")"; EXE_PRESENT=1; echo "run_all_tests: exe oracle ON  ($EXE)"
else EXE_PRESENT=0; echo "run_all_tests: exe oracle OFF (no $EXE): the exe-oracle tests are reported as SKIP(no_exe)"; fi
OUT="$SCRATCH/run_all_tests.$$"; mkdir -p "$OUT"

# conan toolchain: from this checkout, else from the main worktree (a linked worktree has no build/Debug)
TOOLCHAIN="${CONAN_TOOLCHAIN:-$REPO/build/Debug/generators/conan_toolchain.cmake}"
[ -f "$TOOLCHAIN" ] || TOOLCHAIN="$(git -C "$REPO" worktree list --porcelain | sed -n '1s/^worktree //p')/build/Debug/generators/conan_toolchain.cmake"
lock()   { until mkdir "$SCRATCH/build.lock" 2>/dev/null; do sleep 15; done; }
unlock() { rmdir "$SCRATCH/build.lock" 2>/dev/null; }
trap 'unlock_if_held' EXIT
HELD=0; unlock_if_held() { [ "${gpuheld:-0}" = 1 ] && "$REPO/tools/standalone/gpu_locked.sh" --release $$; [ "$HELD" = 1 ] && unlock; }

if [ "${SKIP_BUILD:-0}" != 1 ]; then
  lock; HELD=1
  if [ ! -f "$BDIR/build.ninja" ]; then
    mkdir -p "$BDIR"
    if [ "$HOST" = windows ]; then HOSTARGS=(-DGTASA_NO_EDIT_AND_CONTINUE=ON --compile-no-warning-as-error)   # /Z7 + /MAP like the Wine toolchain (the /ZI default breaks the optimised presets and gives no map); no -WX: the Wine flow builds with /WX- and the native cl reports warnings (C4018/C4715) the port does not fix
    else HOSTARGS=("-DCMAKE_CXX_FLAGS=/WX- /wd4005 /DWIN32 /D_WINDOWS /Zm1000 /GX" "-DGTASA_ML=$MSVC_WINE_ROOT/bin/x86/ml"); fi
    cmake -S "$REPO" -B "$BDIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_TOOLCHAIN_FILE="$(hp "$TOOLCHAIN")" -DCMAKE_MAKE_PROGRAM="$NINJA" "${HOSTARGS[@]}" \
      -DGTASA_STANDALONE=ON -DGTASA_RW_LIBRW=ON -DGTASA_RW_LIBRW_OPT_OUT=OFF -DGTASA_USE_SDL3=ON \
      -DGTASA_PYTHON="$(hp "$GTASA_PYTHON")" \
      -DGTASA_WITH_OPENAL=OFF -DGTASA_WITH_CLEO_COMMANDS=OFF -DGTASA_WITH_SCRIPT_COMMAND_HOOKS=OFF -DGTASA_WITH_LTO=OFF -DGTASA_UNITY_BUILD=OFF \
      > "$OUT/configure.log" 2>&1 || { echo "configure failed, see $OUT/configure.log"; exit 2; }
  fi
fi
# Regenerate build.ninja first: a CMakeLists change (e.g. a new *_test target landed from another worktree) is otherwise only picked up
# by the first real build below, after the target list was already taken from the stale file.
[ "${SKIP_BUILD:-0}" = 1 ] || ninja -C "$BDIR" build.ninja >/dev/null 2>&1
TESTS=$(ninja -C "$BDIR" -t targets all 2>/dev/null | sed -n 's/^\([A-Za-z0-9_]*_test\): phony$/\1/p' | sort -u | grep -v "^cmake_" | grep -E "$FILTER")
[ -n "$TESTS" ] || { echo "no *_test targets found in $BDIR"; exit 2; }
BUILD_FAIL=""
if [ "${SKIP_BUILD:-0}" != 1 ]; then
  # shellcheck disable=SC2086
  if [ "$HOST" = windows ]; then
    # shared interactive PC: never an unbounded ninja (default = cores+2 jobs froze the desktop); -j${GTA_JOBS:-4} at BelowNormal ("start /low"; cl/link children inherit it)
    cmd //c start "" //low //b //wait ninja -C "$BDIR" -j "${GTA_JOBS:-4}" -k 0 $TESTS > "$OUT/ninja.log" 2>&1 || BUILD_FAIL=1
  else
    ninja -C "$BDIR" -k 0 $TESTS > "$OUT/ninja.log" 2>&1 || BUILD_FAIL=1
  fi
  unlock; HELD=0
  # `cmd /c start /wait` does not hand ninja's exit code back (2026-10-27: a test whose compile FAILED still ran its stale exe and reported ok), so the log is the source of truth:
  # a FAILED: step, "ninja: build stopped" or "ninja: error" = build failure. Every test whose target is not up to date afterwards (its own FAILED step, or blocked by a failed
  # dependency: -k 0 prints no FAILED line for those) is NOBUILD and never runs its old exe.
  if grep -a -q -E '^FAILED:|^ninja: build stopped|^ninja: error' "$OUT/ninja.log"; then BUILD_FAIL=1; fi
  NOBUILD_SET=" "
  if [ -n "$BUILD_FAIL" ]; then
    for t in $TESTS; do
      if grep -a -E '^FAILED:' "$OUT/ninja.log" | grep -a -q -E "[/\\\\ ]$t\.(dir|exe)"; then NOBUILD_SET+="$t "
      elif ! ninja -C "$BDIR" -n "$t" 2>&1 | grep -a -q 'no work to do'; then NOBUILD_SET+="$t "; fi
    done
    echo "ninja reported errors (see $OUT/ninja.log); NOBUILD (target not up to date, stale exe not run):$NOBUILD_SET"
  fi
  # BUILD_ONLY=1: stop after the build (so the run stage can be done later under the GPU lock with SKIP_BUILD=1)
  [ "${BUILD_ONLY:-0}" = 1 ] && { echo "run_all_tests: build only, $(echo $TESTS | wc -w) test targets, ninja ${BUILD_FAIL:+FAILED}${BUILD_FAIL:-ok}"; [ -n "$BUILD_FAIL" ] && exit 1; exit 0; }
fi

# parse: prints "checks fails mismatches" or nothing when no result line exists in the output
parse() { "$PY" - "$1" <<'PY'
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
x=re.findall(r'(?m)^pool_oracle_test: .*?(\d+) checks, .*: (\d+) mismatches',t)                  # pool_oracle_test
if x: c,m=int(x[-1][0]),int(x[-1][1]); f=f or 0
x=re.findall(r'(\d+) Service calls .*?: (\d+) mismatches at PC=53',t)                           # audio_fade_oracle_test (the PC=24 count is informational)
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
  if [ ! -f "$exe" ] || [[ "${NOBUILD_SET:- }" == *" $t "* ]]; then ROWS+="$t - - - 0 0 NOBUILD"$'\n'; RC=1; continue; fi
  args=()
  if [[ "$MODEL_TESTS" == *" $t "* ]]; then
    case "$t" in rw_skin_hanim_test|rw_skin_pipeline_test|rw_rtanim_rtquat_test) models="male01";; *) models="infernus male01 vgsnbuild07";; esac   # these two need skinned models
    for f in $models; do [ -f "$ASSETS/$f.dff" ] && args+=("$(wp "$ASSETS")\\$f.dff"); done; fi
  case "$t" in logic_oracle_test) TMO=${TIMEOUT:-3000};; wave2_oracle_test) TMO=${TIMEOUT:-2400};; *oracle*) TMO=${TIMEOUT:-900};; rw_skin_vs_test) TMO=${TIMEOUT:-900};; *) TMO=${TIMEOUT:-180};; esac   # vs_test: ~2.2M exe-composer keys (> 180 s under Wine); logic_oracle_test: ~224 functions x >=1000 cases, ~20 min under Wine
  tv="TIMEOUT_$t"; [ -n "${!tv:-}" ] && TMO=${!tv}                                                              # per-test override: TIMEOUT_<test>=<seconds>
  # rw_* tests open a real D3D9/GL device: on the Windows host they share the GPU with game runs (soaks, gameplay_regress, agents) and
  # fail with NORESULT while a game holds it (2026-10-26 gate: 5 rw_* NORESULT, all ok when re-run alone) -> take gpu.lock around them
  gpuheld=0; if [ "$HOST" = windows ] && [[ "$t" == rw_* ]]; then "$REPO/tools/standalone/gpu_locked.sh" --acquire "run_all_tests $t" $$ && gpuheld=1; fi
  start=$SECONDS; status=NORESULT; res=""; n=0
  while [ $n -lt "$ATTEMPTS" ]; do
    n=$((n+1)); log="$OUT/$t.$n.log"
    if [ "$HOST" = windows ]; then ( cd "$BDIR/source" && exec "./$t.exe" "${args[@]+"${args[@]}"}" ) > "$log" 2>&1 &
    else ( cd "$BDIR/source" && exec wine "$t.exe" "${args[@]+"${args[@]}"}" ) > "$log" 2>&1 & fi
    pid=$!; code=""; wp=""
    for ((s=0; s<TMO; s++)); do sleep 1; [ -z "$wp" ] && wp=$(cat /proc/$pid/winpid 2>/dev/null); kill -0 $pid 2>/dev/null || { wait $pid; code=$?; break; }; done
    if [ -z "$code" ]; then kill -9 $pid 2>/dev/null; echo "  $t attempt $n: hang >${TMO}s"; code=hang; fi
    if [ "$code" = 77 ]; then res=SKIP; break; fi          # exit 77 = "exe oracle not available" (RW_EXE_ORACLE unset): not a result, not a failure, no retry
    res=$(parse "$log")
    if [ -z "$res" ] || [ "$code" = hang ]; then
      # Windows: kill only our own instance (by its Windows pid) - killing by image name also killed agents' runs of the same test.
      if [ "$HOST" = windows ]; then [ -n "$wp" ] && taskkill //F //T //PID "$wp" >/dev/null 2>&1
      else pkill -9 -f "$t.exe" 2>/dev/null; pkill -9 winedbg 2>/dev/null; fi
      sleep 1
    fi
    if [ -n "$res" ] && [ "$code" != hang ]; then break; fi
    res=""; [ "$code" != hang ] && echo "  $t attempt $n: crash/no result (exit $code)"
  done
  [ "$gpuheld" = 1 ] && { "$REPO/tools/standalone/gpu_locked.sh" --release $$; gpuheld=0; }
  if [ "$res" = SKIP ]; then
    if [ "$EXE_PRESENT" = 1 ]; then ROWS+="$t - - - $n $((SECONDS-start)) EXIT77"$'\n'; RC=1     # the exe was given but the test still reports "not available"
    else ROWS+="$t - - - $n $((SECONDS-start)) SKIP(no_exe)"$'\n'; fi
    continue
  fi
  if [ -z "$res" ]; then ROWS+="$t - - - $n $((SECONDS-start)) NORESULT"$'\n'; RC=1; continue; fi
  set -- $res
  if [ "$2" -gt 0 ] || [ "$3" -gt 0 ]; then status=FAIL; RC=1
  elif [ "$code" != 0 ]; then status="EXIT$code"; RC=1      # a result line with no failures but a non-zero exit code (77 = exe oracle not available)
  else status=ok; fi
  ROWS+="$t $1 $2 $3 $n $((SECONDS-start)) $status"$'\n'
done
echo; printf '%-30s %8s %9s %10s %8s %7s  %s\n' test checks failures mismatches attempts seconds status
printf '%s' "$ROWS" | while read -r a b c d e f g; do printf '%-30s %8s %9s %10s %8s %7s  %s\n' "$a" "$b" "$c" "$d" "$e" "$f" "$g"; done
NOK=$(printf '%s' "$ROWS" | grep -c ' ok$'); NSKIP=$(printf '%s' "$ROWS" | grep -c ' SKIP(no_exe)$'); NALL=$(printf '%s' "$ROWS" | grep -c .)
echo "summary: $NALL tests: $NOK ok, $NSKIP SKIP(no exe), $((NALL-NOK-NSKIP)) other (fail/nobuild/noresult)"
echo "logs: $OUT"
exit $RC
