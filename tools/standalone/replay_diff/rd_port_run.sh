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
"$LOCK" --acquire "REPLAY-DIFF port $TAG" $$; HELD=1
# pin bit 128 (audio RNG partition): audio callers of CGeneral::GetRandomNumber are recognised by return address, from the linker map next to the exe (RD-AB-8: without it every audio
# rand() - e.g. the footstep sounds, first at traced frame 42 - drew from the gameplay stream while the original's partition put it in the audio stream = a harness A-B divergence)
if [ -z "${NOTSA_LOCKSTEP_AUDIO_RVA:-}" ] && [ -f "${EXE%.exe}.map" ]; then NOTSA_LOCKSTEP_AUDIO_RVA="$("$PY" -I "$HERE/audio_rva.py" "$(cd "$HERE/../../.." && pwd)" "${EXE%.exe}.map")"; export NOTSA_LOCKSTEP_AUDIO_RVA; fi
[ "$((PIN & 128))" != 0 ] && [ -z "${NOTSA_LOCKSTEP_AUDIO_RVA:-}" ] && echo "rd_port_run: WARNING pin 128 without NOTSA_LOCKSTEP_AUDIO_RVA (no <exe>.map next to the exe): audio rand() calls from CGeneral stay in the gameplay stream"
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
