#!/bin/bash
# REPLAY-DIFF probe: one run of the ORIGINAL exe (gta_sa_compact.exe, never the Steam gta-sa.exe) with the in-process tracer (rd_orig.c as vorbisFile.dll) through the same menu/new-game input as
# gameplay_regress/orig_run.sh (inputdrv.exe, real-time keys), then a quiet wait of N seconds with NO input, then the game is killed. Output: $OUT/<tag>.trace (+ inputdrv/poll logs).
# usage: rd_orig_run.sh <tag> <pin-bits> <out-dir> [seconds-after-game-start=60]
# Fullscreen exclusive (the original has no windowed mode), silent through the dsound proxy, whole Documents\GTA San Andreas User Files backed up / restored (hostenv.sh docs_backup / docs_restore) as orig_run.sh.
set -u
. "$(dirname "$0")/../hostenv.sh"
TAG="$1"; PIN="$2"; OUT="$(hp "$3")"; SECS="${4:-60}"
HERE="$(cd "$(dirname "$0")" && pwd)"; GRD="$(hp "$HERE/../gameplay_regress")"
DIR="$(hp "${RD_ORIG_DIR:-C:/gta-scratch/rdd/orig}")"; PX="$(hp "${GR_PX:-C:/gta-perf/px}")"; RDX="$(hp "${RD_PX:-C:/gta-scratch/rdd/px}")"
mkdir -p "$OUT"
for f in "$RDX/vorbisFile.dll" "$PX/inputdrv.exe" "$PX/d3d9.dll" "$PX/dsound.dll"; do [ -f "$f" ] || { echo "rd_orig_run: missing $f"; exit 2; }; done
if [ ! -f "$DIR/vorbisFile_real.dll" ]; then   # private data dir: Steam links + the ORIGINAL exe copy; the hard-linked vorbisFile.dll NAME is replaced (never written through)
  bash "$HERE/../windows/perf_datadir.sh" "$DIR" orig >/dev/null || true   # (the worktree has no gta_sa_compact.exe: the dir is prepared by hand then, see REPLAY_DIFF_PLAN.md)
  cp "$DIR/vorbisFile.dll" "$DIR/vorbisFile_real.dll" && rm -f "$DIR/vorbisFile.dll"
fi
cp -f "$RDX/vorbisFile.dll" "$PX/d3d9.dll" "$PX/dsound.dll" "$DIR/"
# input: the header of orig/cheats.scn up to the game start (gstate=9), then a quiet wait
awk -v d="$GRD/orig" '/^#include / { f = d "/" $2 ".inc"; while ((getline l < f) > 0) print l; close(f); next } { print }' "$GRD/orig/cheats.scn" | awk '{print} /^until:gstate=9/ {exit}' > "$OUT/$TAG.script"
echo "wait:$((SECS*1000))" >> "$OUT/$TAG.script"
BK="$OUT/$TAG.userfiles_backup"
HELD=0
cleanup() {   # only what this script holds: the game is killed and the Documents files restored only while WE own gpu.lock (another owner may be running a game)
  if [ "$HELD" = 1 ]; then
    taskkill //F //IM gta_sa.exe >/dev/null 2>&1; taskkill //F //IM inputdrv.exe >/dev/null 2>&1
    docs_restore "$BK"
    "$HERE/../gpu_locked.sh" --release $$; HELD=0
  fi
}
trap cleanup EXIT
"$HERE/../gpu_locked.sh" --acquire "REPLAY-DIFF orig $TAG" $$
HELD=1
docs_backup "$BK"   # after the lock: nobody else is running an original-exe leg that writes the same directory
taskkill //F //IM gta_sa.exe >/dev/null 2>&1
"$PY" -I - "$DOCS_USERFILES/gta_sa.set" "$GRD/orig/gta_sa.set.good" <<'EOPY'
import sys
b = bytearray(open(sys.argv[2], 'rb').read())
b[0xB35] = 0; b[0xB1B] = 0; b[0xB1C] = 0; b[0xB1E] = 0
open(sys.argv[1], 'wb').write(b)
EOPY
cd "$DIR" || exit 2
rm -rf logs; mkdir -p movies
export D3D9PX_OUT="$(cygpath -w "$OUT/$TAG.px.bin")" D3D9PX_NOVSYNC=1 RD_OUT="$(cygpath -w "$OUT/$TAG.trace")" RD_PIN="$PIN" RD_STEP=16 RD_MAXF="${RD_MAXF:-6000}"
./gta_sa.exe > "$OUT/$TAG.run.out" 2>&1 &
sleep 5
tasklist | grep -qi gta_sa.exe || { echo "rd_orig_run: gta_sa.exe did not start"; exit 2; }
"$PX/inputdrv.exe" "$(cygpath -w "$OUT/$TAG.script")" gta_sa.exe > "$OUT/$TAG.inputdrv.log" 2>&1
echo "inputdrv rc=$?" >> "$OUT/$TAG.inputdrv.log"
sleep 1
cp -f logs/log.log "$OUT/$TAG.gamelog.log" 2>/dev/null
echo "rd_orig_run $TAG pin=$PIN: $(grep -c '^F ' "$OUT/$TAG.trace" 2>/dev/null) frames traced"
exit 0
