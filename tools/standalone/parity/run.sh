#!/bin/bash
# PARITY: record / replay one scenario of the C++ reference build (C:/gta-scratch/parity/FORMAT.md).
# usage: tools/standalone/gpu_locked.sh -o PARITY tools/standalone/parity/run.sh rec  <out-prefix> <mission idx> [frames]   -> <prefix>.input <prefix>.hash (driver ON, generic mode: allmissions.sh's environment)
#        tools/standalone/gpu_locked.sh -o PARITY tools/standalone/parity/run.sh play <out-prefix> <input file> [frames]    -> <prefix>.hash (driver OFF, input file only)
# env: EXE (default build/PortableWin64RR/bin/gta_reversed.exe), DATA (private run dir, default C:/gta-scratch/parity/cpp/d), STEP_MS (20), SEED (12345), WALL (900 s)
set -u
. "$(dirname "$0")/../hostenv.sh"; . "$(dirname "$0")/../own_kill.sh"
REPO="$(hp "$(cd "$(dirname "$0")/../../.." && pwd)")"
MODE="${1:?mode rec|play}"; PFX="$(hp "${2:?out prefix}")"; ARG="${3:?mission idx / input file}"; FRAMES="${4:-9000}"
EXE="$(hp "${EXE:-$REPO/build/PortableWin64RR/bin/gta_reversed.exe}")"
DATA="$(hp "${DATA:-C:/gta-scratch/parity/cpp/d}")"; GAME_SRC="$(hp "${GAME_SRC:-$DEF_GAME_SRC}")"; UD="$DATA.ud"; WALL="${WALL:-900}"
TABLE="$(cygpath -w "${TABLE:-C:/Users/littl/github/gta-reversed/.claude/worktrees/web-ts-port-51e1e0/web/docs/driver-classes.tsv}")" # read only, never copied
mkdir -p "$DATA" "$UD" "$(dirname "$PFX")"
W="$(dirname "$PFX")/cpp/$(basename "$PFX").work"; mkdir -p "$W"
for e in "$GAME_SRC"/*; do
  n="$(basename "$e")"; case "$n" in gta_sa.exe|gta-sa.exe|gta_reversed.exe|logs|standalone*) continue;; esac
  [ -e "$DATA/$n" ] && continue
  if [ -d "$e" ]; then cmd //c mklink //J "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp -R "$e" "$DATA/$n"
  else cmd //c mklink //H "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp "$e" "$DATA/$n"; fi
done
# the boot part is the same as the all-missions driver (New Game, 600 ms wait, game clock 06:30), frame 0 = the rr_start frame
expand() { awk -v d="$REPO/tools/standalone/gameplay_regress/routes" '/^#include / { f = d "/" $2 ".inc"; while ((getline l < f) > 0) print l; close(f); next } { print }' "$1"; }
HEAD="#include header
wait:600
cmd:time:6:30
cmd:rr_start"
if [ "$MODE" = rec ]; then printf '%s\nmission:%s\nwait:3600000\ncmd:quit\n' "$HEAD" "$ARG" > "$W/route.in"
else printf '%s\nwait:3600000\ncmd:quit\n' "$HEAD" > "$W/route.in"; fi
expand "$W/route.in" > "$W/route.txt"
cd "$DATA" || exit 2
own_kill_in_dir gta_reversed "$DATA"
rm -rf "$UD"/* logs standalone*.log; rm -f gta_reversed.exe; cp "$EXE" gta_reversed.exe
rm -f "$PFX.hash" "$W/rc" "$W/res"; [ "$MODE" = rec ] && rm -f "$PFX.input"
(
  export NOTSA_STANDALONE_SKIP_VIDEOS=1 NOTSA_STANDALONE_MUTE=1 NOTSA_STANDALONE_INPUT="$(cygpath -w "$W/route.txt")" NOTSA_STANDALONE_USERDIR="$(cygpath -w "$UD")"
  export NOTSA_STANDALONE_WINDOWED=960x600 NOTSA_STANDALONE_WINDOWED_OFFSCREEN=1 NOTSA_STANDALONE_NOPRESENT=1 NOTSA_STANDALONE_NOVSYNC=1
  export NOTSA_RR=1 NOTSA_RR_NOAUDIO=1 NOTSA_RR_AUDIOSTUBS=1 NOTSA_RR_SEED="${SEED:-12345}" NOTSA_RR_STEP_MS="${STEP_MS:-20}" NOTSA_RR_SCREEN=1280x720 NOTSA_AUDIO=null NOTSA_RR_GAMEPLAY_RAND=1
  export NOTSA_RR_MAXFRAMES="$FRAMES" NOTSA_RR_EXIT=1 NOTSA_RR_TRACE="$(cygpath -w "$W/m.rr.trace")" NOTSA_PARITY_HASH="$(cygpath -w "$PFX.hash")" NOTSA_RR_TAG="parity-$(git -C "$REPO" rev-parse --short HEAD)"
  export NOTSA_SCMTRACE="$(cygpath -w "$W/m.scmtrace")" NOTSA_SCMTRACE_COMMIT="$(git -C "$REPO" rev-parse --short HEAD)"
  if [ "$MODE" = rec ]; then
    export NOTSA_PARITY_REC="$(cygpath -w "$PFX.input")"
    [ -z "${NODRIVER:-}" ] && export NOTSA_MDRIVER_ARGCOUNTS="$(cygpath -w "$REPO/tools/standalone/scmscan/argcounts.tsv")" NOTSA_MDRIVER_TABLE="$TABLE" NOTSA_MDRIVER_RESULT="$(cygpath -w "$W/res")" NOTSA_MDRIVER_MAXFRAMES=30000
  else
    export NOTSA_PARITY_PLAY="$(cygpath -w "$ARG")"
  fi
  ./gta_reversed.exe > "$W/m.out" 2>&1 &
  echo $! > "$W/pid"
  wait $! ; echo $? > "$W/rc"
) &
sleep 2
WPID=$(cat "$W/pid"); t0=$(date +%s); st=ok
while kill -0 "$WPID" 2>/dev/null; do sleep 3; [ $(( $(date +%s) - t0 )) -gt "$WALL" ] && { st=TIMEOUT; break; }; done
sleep 1; own_kill_pid "$WPID"; own_kill_in_dir gta_reversed "$DATA"
cp -f standalone.log "$W/m.log" 2>/dev/null
echo "$MODE done: $st rc=$(cat "$W/rc" 2>/dev/null) wall=$(( $(date +%s) - t0 ))s hash_lines=$(grep -vc '^#' "$PFX.hash" 2>/dev/null)"
