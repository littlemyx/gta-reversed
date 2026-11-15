#!/bin/bash
# SH2: RYDER_1 reference run on the 64-bit SSE RR build (tools/standalone/statehash/shbuild.sh -> build/PortableWin64RR), NOTSA_RR_GAMEPLAY_RAND=1.
#   record:  driver ON  (NOTSA_MDRIVER=tools/standalone/mdriver/ryder1.steps) -> <out>/<tag>.driver (injector route lines = the INPUT recording)
#   replay:  driver OFF, input = base route + the recorded driver lines
# usage: tools/standalone/gpu_locked.sh -o SH2 tools/standalone/statehash/sh2_run.sh record|replay <tag> [outdir] [limit_s] [dumpframes]
# outputs (<out>/<tag>.*): .sh (per-frame hash file) .rand (rand log) .load (load-order log) .scmtrace .rr.trace .dump (verbose dump at <dumpframes>, default 1000,5000,15000) .log .out .route.txt
# env: EXE (default build/PortableWin64RR/bin/gta_reversed.exe), RR_SEED (12345), DATA (SHORT path with the game data), GAME_SRC, STEPS, MAXF (record frames limit, default unlimited), PORTABLE_ENV (extra env words)
set -u
. "$(dirname "$0")/../hostenv.sh"; . "$(dirname "$0")/../own_kill.sh"
REPO="$(hp "$(cd "$(dirname "$0")/../../.." && pwd)")"
MODE="${1:?record|replay}"; TAG="${2:?tag}"; OUT="$(hp "${3:-C:/gta-scratch/statehash/ryder1}")"; LIMIT="${4:-7200}"; DUMPF="${5:-1000,5000,15000}"
EXE="$(hp "${EXE:-$REPO/build/PortableWin64RR/bin/gta_reversed.exe}")"
DATA="$(hp "${DATA:-C:/gta-scratch/statehash/d2}")"; GAME_SRC="$(hp "${GAME_SRC:-$DEF_GAME_SRC}")"
STEPS="$(cygpath -w "${STEPS:-$REPO/tools/standalone/mdriver/ryder1.steps}")"
UD="$(hp C:/gta-scratch/statehash/ud2)"
mkdir -p "$OUT" "$DATA" "$UD"
HEAD="# SH2 base route: New Game, a fixed 600 ms wait (no extra spawns: they shift the pool handles), game clock 06:30, rr_start, injected start of mission index 24 (GUNS1 = RYDER_1)
#include header
wait:600
cmd:time:6:30
cmd:rr_start
mission:24"
expand() { awk -v d="$REPO/tools/standalone/gameplay_regress/routes" '/^#include / { f = d "/" $2 ".inc"; while ((getline l < f) > 0) print l; close(f); next } { print }' "$1"; }
RT="$OUT/$TAG.route.in"
if [ "$MODE" = record ]; then printf '%s\nwait:3600000\ncmd:quit\n' "$HEAD" > "$RT"; else printf '%s\n' "$HEAD" > "$RT"; cat "$OUT/${RECORD_TAG:-record}.driver" >> "$RT"; fi
expand "$RT" > "$OUT/$TAG.route.txt"; rm -f "$RT"
for e in "$GAME_SRC"/*; do
  n="$(basename "$e")"; case "$n" in gta_sa.exe|gta-sa.exe|gta_reversed.exe|logs|standalone*) continue;; esac
  [ -e "$DATA/$n" ] && continue
  if [ -d "$e" ]; then cmd //c mklink //J "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp -R "$e" "$DATA/$n"
  else cmd //c mklink //H "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp "$e" "$DATA/$n"; fi
done
cd "$DATA" || exit 2
own_kill_in_dir gta_reversed "$DATA"
rm -rf "$UD"/* logs standalone*.log; rm -f gta_reversed.exe; cp "$EXE" gta_reversed.exe
for x in sh rand load scmtrace rr.trace dump log out driver; do rm -f "$OUT/$TAG.$x"; done
(
  export NOTSA_STANDALONE_SKIP_VIDEOS=1 NOTSA_STANDALONE_MUTE=1 NOTSA_STANDALONE_INPUT="$(cygpath -w "$OUT/$TAG.route.txt")" NOTSA_STANDALONE_USERDIR="$(cygpath -w "$UD")"
  export NOTSA_STANDALONE_WINDOWED=960x600 NOTSA_STANDALONE_NOPRESENT=1 NOTSA_STANDALONE_NOVSYNC=1
  export NOTSA_RR=1 NOTSA_RR_NOAUDIO=1 NOTSA_RR_SEED="${RR_SEED:-12345}" NOTSA_RR_STEP_MS=20 NOTSA_RR_SCREEN=1280x720 NOTSA_AUDIO=null NOTSA_RR_GAMEPLAY_RAND=1
  [ -n "${MAXF:-}" ] && export NOTSA_RR_MAXFRAMES="$MAXF" NOTSA_RR_EXIT=1
  export NOTSA_RR_TRACE="$(cygpath -w "$OUT/$TAG.rr.trace")" NOTSA_SH_FILE="$(cygpath -w "$OUT/$TAG.sh")" NOTSA_SH_RANDLOG="$(cygpath -w "$OUT/$TAG.rand")" NOTSA_SH_LOADLOG="$(cygpath -w "$OUT/$TAG.load")"
  export NOTSA_SCMTRACE="$(cygpath -w "$OUT/$TAG.scmtrace")" NOTSA_SCMTRACE_COMMIT="$(git -C "$REPO" rev-parse --short HEAD)"
  export NOTSA_SH_DUMP="$DUMPF" NOTSA_SH_DUMP_SUBSYS=all NOTSA_SH_DUMP_FILE="$(cygpath -w "$OUT/$TAG.dump")"
  [ "$MODE" = record ] && export NOTSA_MDRIVER="$STEPS" NOTSA_MDRIVER_OUT="$(cygpath -w "$OUT/$TAG.driver")"
  ./gta_reversed.exe > "$OUT/$TAG.out" 2>&1 &
  echo $! > "$OUT/$TAG.pid"
  wait $! ; echo $? > "$OUT/$TAG.rc"
) &
sleep 2; true
WPID=$(cat "$OUT/$TAG.pid"); t0=$(date +%s); st=ok
while kill -0 "$WPID" 2>/dev/null; do
  sleep 3
  [ $(( $(date +%s) - t0 )) -gt "$LIMIT" ] && { st=TIMEOUT; break; }
done
sleep 1; own_kill_pid "$WPID"; own_kill_in_dir gta_reversed "$DATA"
cp -f standalone.log "$OUT/$TAG.log" 2>/dev/null; cp -f logs/log.log "$OUT/$TAG.glog" 2>/dev/null
echo "$st $(( $(date +%s) - t0 ))s rc=$(cat "$OUT/$TAG.rc" 2>/dev/null)  frames: $(grep -vc '^#' "$OUT/$TAG.sh" 2>/dev/null)  rand rows: $(wc -l < "$OUT/$TAG.rand" 2>/dev/null)  load rows: $(wc -l < "$OUT/$TAG.load" 2>/dev/null)"
rm -f "$OUT/$TAG.pid"
