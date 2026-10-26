#!/bin/bash
# REPLAY-DIFF probe: one run of OUR exe (gta_reversed.exe) with source/standalone/Lockstep.cpp tracing, same menu/new-game input as the original leg (real-time route, then a quiet wait). Silent, windowed 960x600,
# PRIVATE user-files dir seeded with the same gta_sa.set the original leg uses (gameplay_regress/orig/gta_sa.set.good with the 4 option bytes of orig_run.sh zeroed). The user's Documents files are never touched.
# usage: rd_port_run.sh <tag> <pin-bits> <out-dir> <gta_reversed.exe> [seconds-after-menu=150]
set -u
. "$(dirname "$0")/../hostenv.sh"
TAG="$1"; PIN="$2"; OUT="$(hp "$3")"; EXE="$(hp "$4")"; SECS="${5:-150}"
HERE="$(cd "$(dirname "$0")" && pwd)"; GRD="$(hp "$HERE/../gameplay_regress")"
DATA="$(hp "${RD_PORT_DIR:-C:/gta-scratch/rdd/port}")"; GAME_SRC="$(hp "${GAME_SRC:-$DEF_GAME_SRC}")"
mkdir -p "$OUT" "$DATA"
[ -f "$EXE" ] || { echo "rd_port_run: missing $EXE"; exit 2; }
for e in "$GAME_SRC"/*; do   # Steam assets by links, no original exe
  n="$(basename "$e")"; case "$n" in gta_sa.exe|gta-sa.exe|gta_reversed.exe|logs|standalone*) continue;; esac
  [ -e "$DATA/$n" ] && continue
  if [ -d "$e" ]; then cmd //c mklink //J "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp -R "$e" "$DATA/$n"
  else cmd //c mklink //H "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp "$e" "$DATA/$n"; fi
done
SET="$OUT/gta_sa.set.rd"
"$PY" -I - "$SET" "$GRD/orig/gta_sa.set.good" <<'EOPY'
import sys
b = bytearray(open(sys.argv[2], 'rb').read())
b[0xB35] = 0; b[0xB1B] = 0; b[0xB1C] = 0; b[0xB1E] = 0
open(sys.argv[1], 'wb').write(b)
EOPY
UD="$(userdir_new rd "$SET")"
cat > "$OUT/$TAG.route" <<EOR
until:menu:240000
wait:1500
mouse:472,248
wait:500
key:return
wait:3000
mouse:236,214
wait:500
key:return
wait:3500
key:return
wait:$((SECS*1000))
cmd:quit
EOR
LOCK="$HERE/../gpu_locked.sh"; HELD=0
kill_ours() { [ -n "${WPID:-}" ] || return 0; local wp; wp=$(cat /proc/$WPID/winpid 2>/dev/null); [ -n "$wp" ] && taskkill //F //T //PID "$wp" >/dev/null 2>&1; kill -9 "$WPID" 2>/dev/null; return 0; }   # rules v5: only our own game, by pid
cleanup() { kill_ours; if [ "$HELD" = 1 ]; then "$LOCK" --release $$; HELD=0; fi; }
trap cleanup EXIT
# pin bit 128 (audio RNG partition): audio callers of CGeneral::GetRandomNumber are recognised by return address, from the linker map next to the exe (RD-AB-8: without it every audio
# rand() - e.g. the footstep sounds, first at traced frame 42 - drew from the gameplay stream while the original's partition put it in the audio stream = a harness A-B divergence).
# RD-ENV (2026-10-26): a port exe copied WITHOUT its map only printed a warning and measured equal prefix 42 instead of 98 for a whole session (RD-AB-16: probe*.exe, final.exe, main.exe in a
# scratch dir). Now fatal, before the GPU lock is taken: the exe needs <exe>.map (same link: the map's "Timestamp is" must equal the exe's PE timestamp) or <exe>.audio_rva (the output of
# audio_rva.py, kept for exes whose 40 MB map was not copied), or NOTSA_LOCKSTEP_AUDIO_RVA in the environment; RD_NO_AUDIO_RVA_OK=1 allows running without (partition off, only for pins without 128).
if [ "$((PIN & 128))" != 0 ] && [ -z "${NOTSA_LOCKSTEP_AUDIO_RVA:-}" ]; then
  if [ -f "${EXE%.exe}.map" ]; then
    NOTSA_LOCKSTEP_AUDIO_RVA="$("$PY" -I "$HERE/audio_rva.py" "$(cd "$HERE/../../.." && pwd)" "${EXE%.exe}.map" "$EXE")" || { echo "rd_port_run: FATAL stale/foreign ${EXE%.exe}.map (see above)"; exit 2; }
  elif [ -f "${EXE%.exe}.audio_rva" ]; then NOTSA_LOCKSTEP_AUDIO_RVA="$(cat "${EXE%.exe}.audio_rva")"
  elif [ "${RD_NO_AUDIO_RVA_OK:-0}" != 1 ]; then
    echo "rd_port_run: FATAL pin 128 (audio RNG partition) needs ${EXE%.exe}.map or ${EXE%.exe}.audio_rva next to the exe (copy the map with the exe; without it the port's equal prefix drops 98 -> 42 at the first footstep rand)"; exit 2
  fi
  export NOTSA_LOCKSTEP_AUDIO_RVA
fi
echo "rd_port_run: pin $PIN audio_rva ranges: $(printf '%s' "${NOTSA_LOCKSTEP_AUDIO_RVA:-}" | tr ',' '\n' | grep -c .) (exe $EXE)"
"$LOCK" --acquire "REPLAY-DIFF port $TAG" $$; HELD=1
cd "$DATA" || exit 2
rm -f gta_reversed.exe standalone*.log; rm -rf logs; cp "$EXE" gta_reversed.exe
(
  export NOTSA_STANDALONE_SKIP_VIDEOS=1 NOTSA_STANDALONE_MUTE=1 NOTSA_STANDALONE_INPUT="$OUT/$TAG.route" NOTSA_STANDALONE_USERDIR="$UD" NOTSA_STANDALONE_WINDOWED=960x600 NOTSA_STANDALONE_NOVSYNC=1 NOTSA_STANDALONE_NOPRESENT=1
  export NOTSA_LOCKSTEP_OUT="$(cygpath -w "$OUT/$TAG.trace")" NOTSA_LOCKSTEP_PIN="$PIN" NOTSA_LOCKSTEP_STEP=16 NOTSA_LOCKSTEP_MAXF="${NOTSA_LOCKSTEP_MAXF:-6000}"
  ./gta_reversed.exe > "$OUT/$TAG.run.out" 2>&1 &
  echo $! > "$OUT/$TAG.pid"
)
WPID=$(cat "$OUT/$TAG.pid"); t0=$(date +%s)
while kill -0 "$WPID" 2>/dev/null; do sleep 3; [ $(( $(date +%s) - t0 )) -gt $((SECS+120)) ] && break; done
kill_ours
cp -f standalone.log logs/log.log "$OUT/" 2>/dev/null
echo "rd_port_run $TAG pin=$PIN: $(grep -c '^F ' "$OUT/$TAG.trace" 2>/dev/null) frames traced"
