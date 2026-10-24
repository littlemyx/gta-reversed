#!/bin/bash
# Save -> load round trip of the STANDALONE exe on the Windows host (SAVE-LOAD): run 1 starts a new game, ends the intro mission (cmd:endmission: the save menu is only legal outside a mission, the
# mission script's code lives in the unsaved MissionBlock), spawns a car + a cop, sets the money to 777 and saves through the real save menu into slot 1 of a PRIVATE user dir; run 2 starts
# the game again, loads that slot through the main menu (Start game -> Load game -> slot 1 -> Yes) and plays ~3 min (walking / sprinting / jumping). Checks: the save is 202752 bytes, both runs
# leave through cmd:quit without UNREACHABLE / FATAL / exception, the loaded game has money 777, the saved position and mission=0.
# usage: tools/standalone/saveload_roundtrip.sh [build-dir (default build/StandaloneRelease)]      env: OUT=<dir> (default C:/gta-soak-out/saveload-<time>), GAME_SRC, SAVELOAD_DATA (run dir)
# Exit code: 0 = OK, 1 = a check failed, 2 = could not start.  Takes the GPU lock (gpu_locked.sh) for the whole round trip. Never touches the user's Documents\GTA San Andreas User Files.
set -u
. "$(dirname "$0")/hostenv.sh"
[ "$HOST" = windows ] || { echo "saveload_roundtrip: Windows host only"; exit 2; }
if [ -z "${GPU_LOCK_HELD:-}" ]; then exec "$(dirname "$0")/gpu_locked.sh" -o "saveload ${GPU_OWNER:-${USER:-agent}}" "$0" "$@"; fi
REPO="$(hp "$(cd "$(dirname "$0")/../.." && pwd)")"
BDIR="$(hp "${1:-build/StandaloneRelease}")"; case "$BDIR" in /*|[A-Za-z]:*) ;; *) BDIR="$REPO/$BDIR";; esac
EXE="$BDIR/bin/gta_reversed.exe"; [ -f "$EXE" ] || { echo "saveload_roundtrip: missing $EXE"; exit 2; }
GAME_SRC="${GAME_SRC:-$DEF_GAME_SRC}"; DATA="$(hp "${SAVELOAD_DATA:-$DEF_SCRATCH/saveload_data}")"
OUT="$(hp "${OUT:-$DEF_OUTROOT/saveload-$(date +%Y%m%d-%H%M%S)}")"; UD="$(userdir_new saveload none)" || exit 2
mkdir -p "$DATA" "$OUT"
for e in "$GAME_SRC"/*; do n="$(basename "$e")"
  case "$n" in gta_sa.exe|gta_reversed.exe|logs|standalone*) continue;; esac
  [ -e "$DATA/$n" ] && continue
  if [ -d "$e" ]; then cmd //c mklink //J "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp -R "$e" "$DATA/$n"
  else cmd //c mklink //H "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp "$e" "$DATA/$n"; fi
done
export NOTSA_STANDALONE_SCREENSHOT=1000000 NOTSA_STANDALONE_SKIP_VIDEOS=1 NOTSA_STANDALONE_MUTE=1 NOTSA_STANDALONE_NOVSYNC=1 NOTSA_STANDALONE_WINDOWED=960x600 NOTSA_STANDALONE_USERDIR="$UD" NOTSA_STANDALONE_MEMLOG=1
run() { # $1 tag, $2 route, $3 limit seconds
  local o="$OUT/$1"; mkdir -p "$o/shots"; cd "$DATA" || exit 2
  rm -f gta_reversed.exe standalone*.log frame_*.bmp mark_*.bmp; rm -rf logs; cp "$EXE" gta_reversed.exe; cp "$2" "$o/route.txt"
  NOTSA_STANDALONE_INPUT="$o/route.txt" ./gta_reversed.exe > "$o/run.out" 2>&1 &
  local bp=$! wp t0; wp=$(cat /proc/$bp/winpid 2>/dev/null); t0=$(date +%s)
  while kill -0 $bp 2>/dev/null; do sleep 3; [ $(( $(date +%s) - t0 )) -gt "$3" ] && { echo "saveload: $1 time limit"; taskkill //F //T //PID "$wp" >/dev/null 2>&1; break; }; done
  kill -0 $bp 2>/dev/null && taskkill //F //T //PID "$wp" >/dev/null 2>&1
  cp -f standalone.log logs/log.log "$o/" 2>/dev/null; mv mark_*.bmp frame_*.bmp "$o/shots/" 2>/dev/null; echo "saveload: $1 done in $(( $(date +%s) - t0 )) s"
}
HERE="$REPO/tools/standalone/saveload"
run save "$HERE/save.txt" 300; run load "$HERE/load.txt" 360
fail=0; chk() { if [ "$2" = 1 ]; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
echo "saveload: results ($OUT)"
SZ=$(stat -c %s "$UD/GTASAsf1.b" 2>/dev/null || echo 0); chk "save written, 202752 bytes (is $SZ)" $([ "$SZ" = 202752 ] && echo 1 || echo 0)
for r in save load; do
  bad=$(grep -a -c "UNREACHABLE\|FATAL\|Exception Code\|\[critical\]" "$OUT/$r/standalone.log" "$OUT/$r/log.log" 2>/dev/null | awk -F: '{s+=$2} END {print s+0}')
  chk "$r run: no UNREACHABLE/FATAL/exception ($bad lines)" $([ "$bad" = 0 ] && echo 1 || echo 0)
  chk "$r run: left through cmd:quit" $(grep -a -q "quit requested" "$OUT/$r/standalone.log" && echo 1 || echo 0)
done
last=$(grep -a "load_end#.* A " "$OUT/load/standalone.log" | tail -1)
chk "load run: reached the end of the route" $([ -n "$last" ] && echo 1 || echo 0)
t0l=$(grep -a "load_t0#.* A " "$OUT/load/standalone.log" | tail -1)
chk "load run: money=777 after the load" $(echo "$t0l" | grep -q "money=777 " && echo 1 || echo 0)
chk "load run: mission=0 after the load" $(echo "$t0l" | grep -q "mission=0 " && echo 1 || echo 0)
echo "  loaded: $(echo "$t0l" | grep -o 'x=[-0-9]* y=[-0-9]* z=[-0-9]*')   end: $(echo "$last" | grep -o 'x=[-0-9]* y=[-0-9]* z=[-0-9]* .*hp=[0-9]*' | cut -c1-60)"
[ $fail = 0 ] && echo "RESULT OK" || echo "RESULT FAIL"; exit $fail
