#!/bin/bash
# SCMTRACE-1: one deterministic RR run of the Windows exe (build/StandaloneReleaseRR: -DGTASA_STATEHASH=ON -DGTASA_SCMTRACE=ON) that writes the SCM trace of the injected mission.
# usage: tools/standalone/gpu_locked.sh -o "SCMTRACE <tag>" tools/standalone/scmtrace_run.sh <tag> <route.txt> [limit_s] [outdir]
#   -> <outdir>/<tag>.scmtrace (default outdir C:/gta-scratch/scmtrace/out), <tag>.rr.trace (statehash), <tag>.log
# env: EXE (default build/StandaloneReleaseRR/bin/gta_reversed.exe), RR_SEED (12345), RR_STEP_MS (20), DATA (default C:/gta-scratch/scmtrace/d: SHORT path), GAME_SRC (as soak.sh)
# The route is the injector format (#include header = routes of tools/standalone/gameplay_regress/routes/*.inc). Private user-files dir, silent, windowed 960x600, no Present.
set -u
. "$(dirname "$0")/hostenv.sh"; . "$(dirname "$0")/own_kill.sh"
REPO="$(hp "$(cd "$(dirname "$0")/../.." && pwd)")"
TAG="${1:?tag}"; ROUTE="${2:?route}"; LIMIT="${3:-600}"; OUT="$(hp "${4:-C:/gta-scratch/scmtrace/out}")"
EXE="$(hp "${EXE:-$REPO/build/StandaloneReleaseRR/bin/gta_reversed.exe}")"
DATA="$(hp "${DATA:-C:/gta-scratch/scmtrace/d}")"; GAME_SRC="$(hp "${GAME_SRC:-$DEF_GAME_SRC}")"
UD="$(hp C:/gta-scratch/scmtrace/ud)"
mkdir -p "$OUT" "$DATA" "$UD"
expand() { awk -v d="$REPO/tools/standalone/gameplay_regress/routes" '/^#include / { f = d "/" $2 ".inc"; while ((getline l < f) > 0) print l; close(f); next } { print }' "$1"; }
expand "$ROUTE" > "$OUT/$TAG.route.txt"
for e in "$GAME_SRC"/*; do
  n="$(basename "$e")"; case "$n" in gta_sa.exe|gta-sa.exe|gta_reversed.exe|logs|standalone*) continue;; esac
  [ -e "$DATA/$n" ] && continue
  if [ -d "$e" ]; then cmd //c mklink //J "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp -R "$e" "$DATA/$n"
  else cmd //c mklink //H "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp "$e" "$DATA/$n"; fi
done
cd "$DATA" || exit 2
own_kill_in_dir gta_reversed "$DATA"
rm -rf "$UD"/* logs standalone*.log; rm -f gta_reversed.exe; cp "$EXE" gta_reversed.exe
rm -f "$OUT/$TAG.scmtrace" "$OUT/$TAG.rr.trace"
(
  export NOTSA_STANDALONE_SKIP_VIDEOS=1 NOTSA_STANDALONE_MUTE=1 NOTSA_STANDALONE_INPUT="$OUT/$TAG.route.txt" NOTSA_STANDALONE_USERDIR="$(cygpath -w "$UD")"
  export NOTSA_STANDALONE_WINDOWED=960x600 NOTSA_STANDALONE_WINDOWED_OFFSCREEN=1 NOTSA_STANDALONE_NOPRESENT=1 NOTSA_STANDALONE_NOVSYNC=1
  export NOTSA_RR=1 NOTSA_RR_NOAUDIO=1 NOTSA_RR_SEED="${RR_SEED:-12345}" NOTSA_RR_STEP_MS="${RR_STEP_MS:-20}" NOTSA_RR_SCREEN=1280x720 NOTSA_AUDIO=null
  export NOTSA_RR_TRACE="$(cygpath -w "$OUT/$TAG.rr.trace")" NOTSA_SCMTRACE="$(cygpath -w "$OUT/$TAG.scmtrace")" NOTSA_SCMTRACE_COMMIT="$(git -C "$REPO" rev-parse --short HEAD)"
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
echo "$st $(( $(date +%s) - t0 ))s  scmtrace lines: $(wc -l < "$OUT/$TAG.scmtrace" 2>/dev/null)"
rm -f "$OUT/$TAG.pid"
