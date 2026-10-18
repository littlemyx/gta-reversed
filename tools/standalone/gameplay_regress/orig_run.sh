#!/bin/bash
# Original-exe leg of the gameplay regression suite (gameplay_regress.sh -o): runs the ORIGINAL gta_sa.exe (v1.0 US) through the same scenario with inputdrv.exe (SendInput + ReadProcessMemory gates) while an
# out-of-process poller samples the game state. Output: $OUT/<scenario>/orig/ (inputdrv.log, the poller log, game log).
# usage: orig_run.sh <combat|audio|cheats> <out-root>
# env:   GR_ORIG_DIR  directory with the original gta_sa.exe and the full data (default C:/gta-perf/orig; "private data dir" of WIN-PERF.md)
#        GR_INPUTDRV  inputdrv.exe to use (default $GR_PX/inputdrv.exe)
#        GR_PX        directory with d3d9.dll (Present proxy, no vsync pacing) / dsound.dll (mute proxy) / inputdrv.exe (build: tools/standalone/windows/build_tools.sh)
# The original has NO windowed mode and no injector: it runs exclusive fullscreen (the desktop mode is switched for the run), needs the foreground (inputdrv aborts when another window has it) and is silent through
# the dsound proxy. gta_sa.set is written from orig/gta_sa.set.good (video mode 800x600, limiter / radio auto-select off) and the WHOLE Documents\GTA San Andreas User Files dir (the original cannot be redirected) is backed up first and restored afterwards (hostenv.sh docs_backup / docs_restore).
set -u
. "$(dirname "$0")/../hostenv.sh"
SCN="$1"; OUT="$(hp "$2")"
GRD="$(cd "$(dirname "$0")" && pwd)"; GRD="$(hp "$GRD")"
DIR="$(hp "${GR_ORIG_DIR:-C:/gta-perf/orig}")"; PX="$(hp "${GR_PX:-C:/gta-perf/px}")"
INPUTDRV="$(hp "${GR_INPUTDRV:-$PX/inputdrv.exe}")"   # must know the `lctrl` key (fire): rebuild with tools/standalone/windows/build_tools.sh if "unknown key lctrl" shows up in inputdrv.log
od="$OUT/$SCN/orig"; mkdir -p "$od"
for f in "$DIR/gta_sa.exe" "$INPUTDRV" "$PX/d3d9.dll" "$PX/dsound.dll"; do
  [ -f "$f" ] || { echo "orig_run: missing $f (GR_ORIG_DIR / GR_PX; tools: tools/standalone/windows/build_tools.sh)"; exit 2; }
done
case "$SCN" in
  combat) POLL=gr; SCRIPT="$GRD/orig/combat.scn";;
  audio)  POLL=audio; SCRIPT="$GRD/orig/audio.scn";;
  cheats) POLL=cheats; SCRIPT="$GRD/orig/cheats.scn";;
  *) echo "orig_run: no original-exe leg for $SCN"; exit 2;;
esac
awk -v d="$GRD/orig" '/^#include / { f = d "/" $2 ".inc"; while ((getline l < f) > 0) print l; close(f); next } { print }' "$SCRIPT" > "$od/script.txt"
DOC="$DOCS_USERFILES"; SET="$DOC/gta_sa.set"; BK="$od/userfiles_backup"
docs_backup "$BK"
LOCK="$DEF_SCRATCH/gpu.lock"; HELD=0
cleanup() {
  taskkill //F //IM gta_sa.exe >/dev/null 2>&1
  [ -n "${SPID:-}" ] && kill "$SPID" 2>/dev/null
  [ -n "${TPID:-}" ] && kill "$TPID" 2>/dev/null
  docs_restore "$BK"
  [ "$HELD" = 1 ] && "$GRD/../gpu_locked.sh" --release $$; HELD=0
}
trap cleanup EXIT
"$GRD/../gpu_locked.sh" --acquire "GAMEPLAY-REGRESS orig $SCN" $$   # INFRA-FAST: pid-recording lock, same lock dir (dead-holder detection)
HELD=1
taskkill //F //IM gta_sa.exe >/dev/null 2>&1
"$PY" -I - "$SET" "$GRD/orig/gta_sa.set.good" <<'EOPY'
import sys
b = bytearray(open(sys.argv[2], 'rb').read())
b[0xB35] = 0; b[0xB1B] = 0; b[0xB1C] = 0; b[0xB1E] = 0
open(sys.argv[1], 'wb').write(b)
EOPY
cd "$DIR" || exit 2
rm -rf logs dsmute.log; mkdir -p movies
cp "$PX/d3d9.dll" "$PX/dsound.dll" .
export D3D9PX_OUT="$(cygpath -w "$od/px.bin")" D3D9PX_NOVSYNC=1 D3D9PX_CAPFILE="$od/cap" D3D9PX_SHOTFILE="$od/shot.req"
echo 60 > "$od/cap"
./gta_sa.exe > "$od/run.out" 2>&1 &
sleep 5
tasklist | grep -qi gta_sa.exe || { echo "orig_run: gta_sa.exe did not start"; exit 2; }
# every original leg also runs the poller that teleports CJ to the Idlewood street (2100,-1790) (2 s after he stepped off the tutorial BMX), like the routes do with cmd:tp
TPAFTER=2.0; [ "$SCN" = cheats ] && TPAFTER=0   # (the cheat scene stays in the quiet start alley: Idlewood is Ballas turf, CJ gets shot in the original)
"$PY" -X utf8 -I "$GRD/orig_gr_poll.py" "$(cygpath -w "$od/poll.txt")" 1.0 gta_sa.exe 900 $TPAFTER > "$od/poll.out" 2>&1 &
TPID=$!
case "$POLL" in
  gr)     : ;;
  audio)  "$PY" -X utf8 -I "$GRD/../audio_stats_exe.py" "$(cygpath -w "$od/audio_stats.txt")" 1.0 gta_sa.exe 900 > "$od/poll2.out" 2>&1 & ;;
  cheats) "$PY" -X utf8 -I "$GRD/../windows/cheat_probe_exe.py" "$(cygpath -w "$od/orig_probe.log")" 0.5 gta_sa.exe 900 > "$od/poll2.out" 2>&1 & ;;
esac
SPID=$!
"$INPUTDRV" "$(cygpath -w "$od/script.txt")" gta_sa.exe > "$od/inputdrv.log" 2>&1
echo "inputdrv rc=$?" >> "$od/inputdrv.log"
sleep 2
cp -f logs/log.log "$od/gamelog.log" 2>/dev/null
exit 0
