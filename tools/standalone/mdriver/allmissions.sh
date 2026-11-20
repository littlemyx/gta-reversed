#!/bin/bash
# AD1: run the generic mission auto-driver (MDRIVER generic mode, table = driver-classes.tsv) over every main.scm mission index, one fresh boot (New Game, no save) per mission.
# usage: tools/standalone/gpu_locked.sh -o AUTODRIVE tools/standalone/mdriver/allmissions.sh [outdir] [idx list, e.g. "24 25" (default: story missions first, then odd jobs / sub-missions)]
# build: tools/standalone/statehash/shbuild.sh -> build/PortableWin64RR/bin/gta_reversed.exe (64-bit SSE RR reference build: STATEHASH + SCMTRACE + MDRIVER + portable math)
# AD4: the dispatch mirrors the TS driver (README in the output dir); env NOTSA_MDRIVER_AD3=1 re-enables the AD3-only warp area / crate pickup.
# env: TABLE (driver-classes.tsv path; default the private TS worktree, read only, never copied into this repo), EXE, DATA (short path with the game data), GAME_SRC, RR_SEED (12345),
#      MAXF (driver frame limit per mission, default 30000), WALL (wall seconds per mission, default 3000), TAG_SUFFIX (appended to the output file names, e.g. "_b" for a determinism rerun)
# outputs in <outdir> (default C:/gta-scratch/scmtrace/out/allmissions): summary.tsv (idx name result frame thread ip opcode reason; PASSED | FAILED (reason = the last text label before M_FAIL) | STOP),
#   <idx>_<thread>.scmtrace.gz  <idx>_<thread>.sh.gz (statehash per-frame file)  <idx>_<thread>.rand.gz (rand log, NOTSA_RR_GAMEPLAY_RAND=1)  <idx>_<thread>.driver (the actions as injector route lines)  <idx>_<thread>.log
# HC_OUT=<dir> (T3H2, with a GTASA_HITCOUNT build as EXE): per-mission function hit dump <dir>/<idx>.hc.txt (NOTSA_HITCOUNT_FILE); the big per-mission artifacts (gz traces/logs) are then not kept.
# A mission already listed in summary.tsv is skipped (delete its row to rerun). Row formats: README in the output dir and C:/gta-scratch/scmtrace/out/full/README.md (section "AD1").
set -u
. "$(dirname "$0")/../hostenv.sh"; . "$(dirname "$0")/../own_kill.sh"
REPO="$(hp "$(cd "$(dirname "$0")/../../.." && pwd)")"
OUT="$(hp "${1:-C:/gta-scratch/scmtrace/out/allmissions}")"; shift || true
LIST="${*:-}"
EXE="$(hp "${EXE:-$REPO/build/PortableWin64RR/bin/gta_reversed.exe}")"
DATA="$(hp "${DATA:-C:/gta-scratch/statehash/d3}")"; GAME_SRC="$(hp "${GAME_SRC:-$DEF_GAME_SRC}")"
TABLE="$(cygpath -w "${TABLE:-C:/Users/littl/github/gta-reversed/.claude/worktrees/web-ts-port-51e1e0/web/docs/driver-classes.tsv}")"
UD="$(hp C:/gta-scratch/statehash/ud3)"; SFX="${TAG_SUFFIX:-}"; WALL="${WALL:-3000}"; MAXF="${MAXF:-30000}"
mkdir -p "$OUT" "$DATA" "$UD"
[ -f "$TABLE" ] || { echo "no table $TABLE"; exit 2; }
if [ -z "$LIST" ]; then
  LIST="2 $(seq -s' ' 10 34) $(seq -s' ' 37 112) 35 36 0 1 $(seq -s' ' 3 9) $(seq -s' ' 113 134)"
fi
NAMES="$REPO/source/toolsmenu/DebugModules/Script/MissionDebugModule.cpp"
mname() { sed -n "s/^ *{ $1, \"\(.*\)\" },\{0,1\} *\$/\1/p" "$NAMES" | head -1; }
HEAD="# AD1 base route: New Game, a fixed 600 ms wait, game clock 06:30, rr_start, injected start of the mission index (same as the SH2 RYDER_1 route)
#include header
wait:600
cmd:time:6:30
cmd:rr_start"
expand() { awk -v d="$REPO/tools/standalone/gameplay_regress/routes" '/^#include / { f = d "/" $2 ".inc"; while ((getline l < f) > 0) print l; close(f); next } { print }' "$1"; }
for e in "$GAME_SRC"/*; do
  n="$(basename "$e")"; case "$n" in gta_sa.exe|gta-sa.exe|gta_reversed.exe|logs|standalone*) continue;; esac
  [ -e "$DATA/$n" ] && continue
  if [ -d "$e" ]; then cmd //c mklink //J "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp -R "$e" "$DATA/$n"
  else cmd //c mklink //H "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp "$e" "$DATA/$n"; fi
done
[ -f "$OUT/summary.tsv" ] || printf 'idx\tname\tresult\tframe\tthread\tip\topcode\treason\n' > "$OUT/summary.tsv"
cd "$DATA" || exit 2
for IDX in $LIST; do
  awk -F'\t' -v i="$IDX" '$1==i{f=1} END{exit !f}' "$OUT/summary.tsv" && { echo "skip $IDX (done)"; continue; }
  MN="$(mname "$IDX")"; W="$OUT/w$IDX"; mkdir -p "$W"
  # AD4 launch context (TS launch.ts): 36/48 CESAR1 in a car (492), 132 MTBIKER (510), 133 STUNT (481): CJ at the wheel; 71 DSKOOL: area 4
  LAUNCH="# no launch context"; case "$IDX" in 36|48) LAUNCH=$'cmd:mdlaunch:492\nwait:400';; 132) LAUNCH=$'cmd:mdlaunch:510\nwait:400';; 133) LAUNCH=$'cmd:mdlaunch:481\nwait:400';; 71) LAUNCH=$'cmd:mdarea:4\nwait:100';; esac
  printf '%s\n%s\nmission:%s\nwait:3600000\ncmd:quit\n' "$HEAD" "$LAUNCH" "$IDX" > "$W/route.in"; expand "$W/route.in" > "$W/route.txt"
  own_kill_in_dir gta_reversed "$DATA"
  rm -rf "$UD"/* logs standalone*.log; rm -f gta_reversed.exe; cp "$EXE" gta_reversed.exe
  rm -f "$W"/res "$W"/*.rc
  (
    export NOTSA_STANDALONE_SKIP_VIDEOS=1 NOTSA_STANDALONE_MUTE=1 NOTSA_STANDALONE_INPUT="$(cygpath -w "$W/route.txt")" NOTSA_STANDALONE_USERDIR="$(cygpath -w "$UD")"
    # CR16: mission 46 (TRUTH1): TS stalls after 600 frames of 33.33 ms = 20 s; 600 RR frames (20 ms) = 12 s landed after the patrol van left its locate (01AD false -> 8x vehwarp). 1000 frames = the same 20 s; the action sequence then equals the TS run. (Not global: 89 needs the old 600.)
    case "$IDX" in 46) export NOTSA_MDRIVER_STALL="${NOTSA_MDRIVER_STALL:-1000}";; esac
    export NOTSA_STANDALONE_WINDOWED=960x600 NOTSA_STANDALONE_NOPRESENT=1 NOTSA_STANDALONE_NOVSYNC=1
    export NOTSA_RR=1 NOTSA_RR_NOAUDIO=1 NOTSA_RR_AUDIOSTUBS=1 NOTSA_RR_SEED="${RR_SEED:-12345}" NOTSA_RR_STEP_MS=20 NOTSA_RR_SCREEN=1280x720 NOTSA_AUDIO=null NOTSA_RR_GAMEPLAY_RAND=1
    export NOTSA_RR_TRACE="$(cygpath -w "$W/m.rr.trace")" NOTSA_SH_FILE="$(cygpath -w "$W/m.sh")" NOTSA_SH_RANDLOG="$(cygpath -w "$W/m.rand")"
    export NOTSA_SCMTRACE="$(cygpath -w "$W/m.scmtrace")" NOTSA_SCMTRACE_COMMIT="$(git -C "$REPO" rev-parse --short HEAD)"
    export NOTSA_MDRIVER_ARGCOUNTS="$(cygpath -w "$REPO/tools/standalone/scmscan/argcounts.tsv")" NOTSA_MDRIVER_TABLE="$TABLE" NOTSA_MDRIVER_OUT="$(cygpath -w "$W/m.driver")" NOTSA_MDRIVER_RESULT="$(cygpath -w "$W/res")" NOTSA_MDRIVER_MAXFRAMES="$MAXF"
    [ -n "${HC_OUT:-}" ] && export NOTSA_HITCOUNT_FILE="$(cygpath -w "$W/hc.txt")"
    ./gta_reversed.exe > "$W/m.out" 2>&1 &
    echo $! > "$W/pid"
    wait $! ; echo $? > "$W/rc"
  ) &
  sleep 2
  WPID=$(cat "$W/pid"); t0=$(date +%s); st=ok
  while kill -0 "$WPID" 2>/dev/null; do
    sleep 3
    [ $(( $(date +%s) - t0 )) -gt "$WALL" ] && { st=TIMEOUT; break; }
  done
  sleep 1; own_kill_pid "$WPID"; own_kill_in_dir gta_reversed "$DATA"
  cp -f standalone.log "$W/m.log" 2>/dev/null
  RC="$(cat "$W/rc" 2>/dev/null)"
  if [ -s "$W/res" ]; then IFS=$'\t' read -r RES FR TH IP OP RSN < "$W/res"
  else RES=STOP; FR=-; TH="${MN:-?}"; IP=-; OP=-; RSN="$([ "$st" = TIMEOUT ] && echo wall-timeout || echo "no-result rc=$RC")"; fi
  TH="$(printf '%s' "$TH" | tr -c 'A-Za-z0-9_\n' '_')"
  B="$OUT/${IDX}_${TH}${SFX}"
  if [ -n "${HC_OUT:-}" ]; then mkdir -p "$HC_OUT"; [ -f "$W/hc.txt" ] && cp "$W/hc.txt" "$HC_OUT/$IDX.hc.txt"; else
  for x in scmtrace sh rand rr.trace; do [ -f "$W/m.$x" ] && { gzip -c "$W/m.$x" > "$B.$x.gz" 2>/dev/null; }; done
  rm -f "$B.driver.gz"; [ -f "$W/m.driver" ] && cp "$W/m.driver" "$B.driver"; rm -f "$B.log.gz"; [ -f "$W/m.log" ] && cp "$W/m.log" "$B.log"
  fi
  [ -z "$SFX" ] && printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$IDX" "${MN:-?}" "$RES" "$FR" "$TH" "$IP" "$OP" "$RSN" >> "$OUT/summary.tsv"
  echo "[$IDX] ${MN:-?}: $RES frame=$FR $RSN ($(( $(date +%s) - t0 ))s)"
  rm -rf "$W"
done
