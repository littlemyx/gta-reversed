#!/bin/bash
# GAMEPLAY REGRESSION SUITE of the standalone exe (Windows host, Git Bash). Short scripted scenarios that replay the playtest-1/2 findings (.notes/USER_PLAYTEST_1.md) on OUR build and, where a scenario
# can be observed from outside, on the ORIGINAL exe; gr_check.py extracts comparable metrics and prints PASS / FAIL / KNOWN per check, with a tolerance that is justified by the game's randomness
# (tools/standalone/gameplay_regress/baseline.json, every threshold has a "why"). Exit code: 0 = no FAIL, 1 = at least one FAIL, 2 = could not run.
#
# usage: tools/standalone/gameplay_regress.sh [options] [scenario ...]        (no scenario = all of them; ~18 min unattended, silent, windowed except render_firststart)
#   options:  -l            list the scenarios and exit
#             -o            ALSO run the ORIGINAL exe for the scenarios that have a live reference (combat, audio, cheats) and show its numbers next to the thresholds of baseline.json (the cheat states are compared with that run)
#                           (the original has no windowed mode: fullscreen, switches the desktop mode, needs the foreground; +~12 min; see docs/STANDALONE.md "Gameplay regression suite")
#             -r            record: write the measured values into $OUT/measured.json (does not touch baseline.json)
#             -b <build>    build dir (default build/StandaloneRelease), or -x <gta_reversed.exe> to test one specific exe
#   env:      GR_OUT=<dir>  output dir (default C:/gta-scratch/gr_out/<timestamp>)   GR_DATA=<dir> private game-data dir (default C:/gta-scratch/gr_data, SHORT path)
#             GAME_SRC=<install dir> as in soak.sh   GR_ORIG_DIR / GR_PX  original-exe run dir / tool dir (defaults C:/gta-perf/orig, C:/gta-perf/px; only for -o)
#             GR_NOPRESENT=0  keep Present in the scenarios that do not look at pixels (default 1: NOTSA_STANDALONE_NOPRESENT=1, start-up 3x faster)
#             GR_SKIP="a b"   scenarios to leave out (e.g. render_firststart: the only one that switches the desktop resolution)
# Every run holds the GPU lock (C:/gta-scratch/gpu.lock) for that run only; while its owner is USER-PLAY (the user is playing) nothing starts. Our exe runs with a PRIVATE user-files dir
# (NOTSA_STANDALONE_USERDIR=C:/gta-scratch/userdir/gr-<time>-<pid>, hostenv.sh userdir_new; EMPTY at every start = no gta_sa.set, like soak.sh; USERDIR_SEED=playtest1|<file> seeds it), so the user's Documents
# files are never touched by our runs. Only the ORIGINAL exe (-o, orig_run.sh) cannot be redirected: orig_run.sh backs up and restores the Documents gta_sa.set around its run. A scenario whose set-up failed (a gate timed out, the game died before the first snapshot) is run once more before it is judged.
set -u
. "$(dirname "$0")/hostenv.sh"
[ "$HOST" = windows ] || { echo "gameplay_regress: Windows host only"; exit 2; }
REPO="$(cd "$(dirname "$0")/../.." && pwd)"; REPO="$(hp "$REPO")"
GRD="$REPO/tools/standalone/gameplay_regress"
LIST=0; ORIG=0; REC=0; RECARG=""; BUILD="build/StandaloneRelease"; EXE=""
while getopts "lorb:x:" o; do case $o in l) LIST=1;; o) ORIG=1;; r) REC=1;; b) BUILD="$OPTARG";; x) EXE="$OPTARG";; *) exit 2;; esac; done
shift $((OPTIND-1)); [ "$REC" = 1 ] && RECARG="--record"
# name | route | wall-clock limit (s) | extra env (A=B ..., - = none) | needs Present (pixels) | windowed | live original reference
SCENARIOS="
combat            combat.txt      330 NOTSA_STANDALONE_PROBE=1                         0 1 1
audio             audio.txt       330 NOTSA_AUDIO_STATS=@OUT@/audio_stats.txt           0 1 1
cheats            cheats.txt      330 -                                                 0 1 1
missions          missions.txt    330 -                                                 0 1 0
render_vmode      vmode.txt       200 -                                                 1 1 0
input_mouse       mouse.txt       200 NOTSA_STANDALONE_WINDOWED_FOCUS=1                 1 1 0
render_firststart firststart.txt  150 -                                                 1 0 0
interior_lspd     interior.txt    330 NOTSA_STANDALONE_PROBE=1 0 1 1
arrest0          arrest0.txt     400 -                                                 0 1 0
"
if [ "$LIST" = 1 ]; then echo "$SCENARIOS" | awk 'NF {printf "%-18s route=%-14s limit=%3ss%s%s%s\n", $1, $2, $3, ($5=="1" ? " pixels" : ""), ($6=="0" ? " EXCLUSIVE-FULLSCREEN" : ""), ($7=="1" ? " [live original reference with -o]" : "")}'; exit 0; fi
WANT="$*"; SKIP=" ${GR_SKIP:-} "
OUT="$(hp "${GR_OUT:-$DEF_SCRATCH/gr_out/$(date +%Y%m%d-%H%M%S)}")"; mkdir -p "$OUT"
DATA="$(hp "${GR_DATA:-$DEF_SCRATCH/gr_data}")"
GAME_SRC="$(hp "${GAME_SRC:-$DEF_GAME_SRC}")"
if [ -z "$EXE" ]; then case "$BUILD" in /*|[A-Za-z]:*) ;; *) BUILD="$REPO/$BUILD";; esac; EXE="$BUILD/bin/gta_reversed.exe"; fi
EXE="$(hp "$EXE")"
[ -f "$EXE" ] || { echo "gameplay_regress: missing $EXE (build gta_reversed.exe first or pass -x)"; exit 2; }
[ -d "$GAME_SRC" ] || { echo "gameplay_regress: game install not found: $GAME_SRC (set GAME_SRC)"; exit 2; }
LOCK="$DEF_SCRATCH/gpu.lock"
UD="$(userdir_new gr "${USERDIR_SEED:-none}")"   # private user-files dir of this suite run (re-created before every scenario start)
HELD=0; WPID=""
GPULOCK="$REPO/tools/standalone/gpu_locked.sh"   # INFRA-FAST: pid-recording lock (dead-holder detection), same lock dir
unlock() { [ "$HELD" = 1 ] && "$GPULOCK" --release $$; HELD=0; return 0; }
kill_game() { # our exe by image path under $DATA; the bash job too
  [ -n "${WPID:-}" ] && kill -9 "$WPID" 2>/dev/null
  GR_DATA="$DATA" powershell.exe -NoProfile -Command 'Get-Process gta_reversed -ErrorAction SilentlyContinue | Where-Object { $_.Path -and (($_.Path -replace "\\","/") -like ($env:GR_DATA + "/*")) } | Stop-Process -Force' >/dev/null 2>&1
  sleep 1
}
trap 'kill_game; unlock' EXIT
trap 'exit 130' INT TERM

gpu_lock() { # $1 = owner tag; waits (forever) while anybody, in particular USER-PLAY, holds the lock (gpu_locked.sh: the holder is this script, a killed script is detected as a dead holder)
  "$GPULOCK" --acquire "$1" $$ || return 1
  HELD=1
}

prep_data() { # Steam assets by NTFS links (read-only use), no original exe in the dir (G1)
  mkdir -p "$DATA"
  local e n
  for e in "$GAME_SRC"/*; do
    n="$(basename "$e")"
    case "$n" in gta_sa.exe|gta-sa.exe|gta_reversed.exe|logs|standalone*) continue;; esac
    [ -e "$DATA/$n" ] && continue
    if [ -d "$e" ]; then cmd //c mklink //J "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp -R "$e" "$DATA/$n"
    else cmd //c mklink //H "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp "$e" "$DATA/$n"; fi
  done
}

# `#include NAME` -> routes/NAME.inc ; @OUT@ -> output dir
expand_route() { # $1 = route file $2 = out file $3 = out dir
  awk -v d="$GRD/routes" '/^#include / { f = d "/" $2 ".inc"; while ((getline l < f) > 0) print l; close(f); next } { print }' "$1" | sed "s#@OUT@#$3#g" > "$2"
}

run_ours() { # name route limit env px windowed -> $OUT/<name>/ (logs, route, screenshots, status)
  local name="$1" route="$2" limit="$3" xenv="$4" px="$5" win="$6" od="$OUT/$1"
  rm -rf "$od"; mkdir -p "$od"; expand_route "$GRD/routes/$route" "$od/route.txt" "$od"
  gpu_lock "GAMEPLAY-REGRESS $name"
  prep_data; cd "$DATA" || return 2
  kill_game; userdir_reset "$UD" "${USERDIR_SEED:-none}"; rm -f gta_reversed.exe standalone*.log frame_*.bmp mark_*.bmp burst_*.bmp; rm -rf logs
  cp "$EXE" gta_reversed.exe
  (
    export NOTSA_STANDALONE_SKIP_VIDEOS=1 NOTSA_STANDALONE_MUTE=1 NOTSA_STANDALONE_INPUT="$od/route.txt" NOTSA_STANDALONE_USERDIR="$UD"
    [ "$win" = 1 ] && export NOTSA_STANDALONE_WINDOWED=960x600
    [ "$px" = 1 ] && export NOTSA_STANDALONE_SCREENSHOT=100000 NOTSA_STANDALONE_SCREENSHOT_MAX=60
    [ "$px" = 0 ] && [ "${GR_NOPRESENT:-1}" = 1 ] && export NOTSA_STANDALONE_NOPRESENT=1
    for kv in $xenv; do [ "$kv" = - ] || export "$(echo "$kv" | sed "s#@OUT@#$od#g")"; done
    ./gta_reversed.exe > "$od/run.out" 2>&1 &
    echo $! > "$od/pid"
  )
  WPID=$(cat "$od/pid")
  local side=""
  if grep -q '^mark:sweep' "$od/route.txt"; then
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$GRD/mouse_sweep.ps1")" "$(cygpath -w "$DATA/standalone.log")" "$(cygpath -m "$DATA")/" > "$od/sidecar.txt" 2>&1 &
    side=$!
  fi
  local t0=$(date +%s) st=ok
  while kill -0 "$WPID" 2>/dev/null; do
    sleep 3
    [ $(( $(date +%s) - t0 )) -gt "$limit" ] && { st=TIMEOUT; break; }
  done
  echo "$st $(( $(date +%s) - t0 ))" > "$od/status"
  [ -n "$side" ] && kill "$side" 2>/dev/null
  kill_game
  cp -f standalone.log logs/log.log "$od/" 2>/dev/null
  for b in mark_*.bmp frame_*.bmp burst_*.bmp; do [ -f "$b" ] && mv "$b" "$od/"; done
  cp -f "$UD/gta_sa.set" "$od/" 2>/dev/null
  unlock
  return 0
}

echo "gameplay_regress: exe $EXE ($(stat -c %s "$EXE") bytes, $(date -r "$EXE" '+%F %T')), out $OUT, user-files dir $UD"
: > "$OUT/summary.txt"
TABLE="$OUT/.table"; echo "$SCENARIOS" > "$TABLE"
while read -r name route limit xenv px win live <&3; do
  [ -z "$name" ] && continue
  if [ -n "$WANT" ] && ! echo " $WANT " | grep -q " $name "; then continue; fi
  if echo "$SKIP" | grep -q " $name "; then echo "== $name  (skipped by GR_SKIP)"; continue; fi
  for attempt in 1 2; do
    echo "== $name$([ "$attempt" = 2 ] && echo "  (attempt 2)")"
    run_ours "$name" "$route" "$limit" "$xenv" "$px" "$win" || exit 2
    "$PY" -X utf8 -I "$GRD/gr_check.py" "$name" "$OUT" $RECARG > "$OUT/$name.result" 2>&1; rc=$?
    # rc 3 = the set-up of the scenario failed (a gate timed out / the game died before it started): once more.
    # A crash is never a set-up flake: it stays the verdict (a retry would hide it, ANIM-CRASH 2026-10-23).
    if [ "$rc" = 3 ] && grep -aqs "UNHANDLED EXCEPTION" "$OUT/$name/log.log" "$OUT/$name/standalone.log"; then
      echo "CRASH $name: unhandled exception during set-up (see $OUT/$name/log.log), not retried" >> "$OUT/$name.result"; rc=1; fi
    [ "$rc" = 3 ] && [ "$attempt" = 1 ] && { echo "  set-up failed ($(grep -m1 SETUP "$OUT/$name.result" | cut -c1-110)), retrying once"; rm -rf "$OUT/$name.attempt1"; mv "$OUT/$name" "$OUT/$name.attempt1"; mv "$OUT/$name.result" "$OUT/$name.attempt1.result"; continue; }
    break
  done
  if [ "$ORIG" = 1 ] && [ "$live" = 1 ]; then
    bash "$GRD/orig_run.sh" "$name" "$OUT" || echo "  (original-exe run of $name failed)"
    "$PY" -X utf8 -I "$GRD/gr_check.py" "$name" "$OUT" --live-orig $RECARG > "$OUT/$name.result" 2>&1
  fi
  cat "$OUT/$name.result" | tee -a "$OUT/summary.txt"
done 3< "$TABLE"
"$PY" -X utf8 -I "$GRD/gr_check.py" --summary "$OUT" | tee "$OUT/summary_table.txt"
exit ${PIPESTATUS[0]}
