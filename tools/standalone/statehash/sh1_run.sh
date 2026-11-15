#!/bin/bash
# SH1: one deterministic RR run of the Windows exe (build with -DGTASA_STATEHASH=ON) that writes the per-frame hash file, a verbose dump and the rand log.
# usage: tools/standalone/gpu_locked.sh -o SH1 tools/standalone/statehash/sh1_run.sh <tag> [route] [outdir] [maxframes] [dumpframes] [limit_s]
#   -> <outdir>/<tag>.sh (hash file), <tag>.dump (NOTSA_SH_DUMP rows), <tag>.rand (rand log), <tag>.rr.trace, <tag>.log.   Defaults: route tools/portable/rr_route_walk.txt, outdir C:/gta-scratch/statehash/out,
#   2000 frames, dump frames 1500, limit 900 s. env: EXE (default build/StandaloneRelease/bin/gta_reversed.exe), RR_SEED, DATA (short path with the game data), GAME_SRC (as scmtrace_run.sh).
set -u
. "$(dirname "$0")/../hostenv.sh"; . "$(dirname "$0")/../own_kill.sh"
REPO="$(hp "$(cd "$(dirname "$0")/../../.." && pwd)")"
TAG="${1:?tag}"; ROUTE="${2:-$REPO/tools/portable/rr_route_walk.txt}"; OUT="$(hp "${3:-C:/gta-scratch/statehash/out}")"; MAXF="${4:-2000}"; DUMPF="${5:-1500}"; LIMIT="${6:-900}"
EXE="$(hp "${EXE:-$REPO/build/StandaloneRelease/bin/gta_reversed.exe}")"
DATA="$(hp "${DATA:-C:/gta-scratch/statehash/d}")"; GAME_SRC="$(hp "${GAME_SRC:-$DEF_GAME_SRC}")"
UD="$(hp C:/gta-scratch/statehash/ud)"
mkdir -p "$OUT" "$DATA" "$UD"
cp "$ROUTE" "$OUT/$TAG.route.txt"
for e in "$GAME_SRC"/*; do
  n="$(basename "$e")"; case "$n" in gta_sa.exe|gta-sa.exe|gta_reversed.exe|logs|standalone*) continue;; esac
  [ -e "$DATA/$n" ] && continue
  if [ -d "$e" ]; then cmd //c mklink //J "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp -R "$e" "$DATA/$n"
  else cmd //c mklink //H "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp "$e" "$DATA/$n"; fi
done
cd "$DATA" || exit 2
own_kill_in_dir gta_reversed "$DATA"
rm -rf "$UD"/* logs standalone*.log; rm -f gta_reversed.exe; cp "$EXE" gta_reversed.exe
rm -f "$OUT/$TAG".sh "$OUT/$TAG".dump "$OUT/$TAG".rand "$OUT/$TAG".rr.trace
(
  export NOTSA_STANDALONE_SKIP_VIDEOS=1 NOTSA_STANDALONE_MUTE=1 NOTSA_STANDALONE_INPUT="$OUT/$TAG.route.txt" NOTSA_STANDALONE_USERDIR="$(cygpath -w "$UD")"
  export NOTSA_STANDALONE_WINDOWED=960x600 NOTSA_STANDALONE_NOPRESENT=1 NOTSA_STANDALONE_NOVSYNC=1
  export NOTSA_RR=1 NOTSA_RR_NOAUDIO=1 NOTSA_RR_SEED="${RR_SEED:-12345}" NOTSA_RR_STEP_MS=20 NOTSA_RR_SCREEN=1280x720 NOTSA_AUDIO=null NOTSA_RR_MAXFRAMES="$MAXF" NOTSA_RR_EXIT=1
  export NOTSA_RR_TRACE="$(cygpath -w "$OUT/$TAG.rr.trace")" NOTSA_SH_FILE="$(cygpath -w "$OUT/$TAG.sh")" NOTSA_SH_RANDLOG="$(cygpath -w "$OUT/$TAG.rand")"
  export NOTSA_SH_DUMP="$DUMPF" NOTSA_SH_DUMP_SUBSYS=all NOTSA_SH_DUMP_FILE="$(cygpath -w "$OUT/$TAG.dump")"
  ./gta_reversed.exe > "$OUT/$TAG.out" 2>&1 &
  echo $! > "$OUT/$TAG.pid"
)
WPID=$(cat "$OUT/$TAG.pid"); t0=$(date +%s); st=ok
while kill -0 "$WPID" 2>/dev/null; do
  sleep 3
  [ $(( $(date +%s) - t0 )) -gt "$LIMIT" ] && { st=TIMEOUT; break; }
done
own_kill_pid "$WPID"; own_kill_in_dir gta_reversed "$DATA"
cp -f standalone.log "$OUT/$TAG.log" 2>/dev/null
echo "$st $(( $(date +%s) - t0 ))s  hash lines: $(wc -l < "$OUT/$TAG.sh" 2>/dev/null)  rand rows: $(wc -l < "$OUT/$TAG.rand" 2>/dev/null)"
rm -f "$OUT/$TAG.pid"
