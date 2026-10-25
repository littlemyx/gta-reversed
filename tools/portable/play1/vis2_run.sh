#!/bin/bash
# MAC-VIS2: one deterministic (NOTSA_RR virtual time) run of a route, native or Win32-under-wine; BMP marks -> $OUT/<tag>/
#   vis2_run.sh native|win32 <tag> <timeout_s> <route.txt>      env: EXE (native exe / Win32 exe), OUT (default /tmp/vis2), SHOTGAMMA (native: 1 = gamma pass in the BMPs, default 0 = like the D3D back buffer)
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../.." && pwd)"
KIND="$1"; TAG="$2"; TMO="$3"; ROUTE="$4"
export OUT="${OUT:-/tmp/vis2}"; mkdir -p "$OUT/shots/$TAG"
while pgrep -f "bin/gta_reversed|wine" >/dev/null; do sleep 5; done
export NOTSA_RR=1 NOTSA_RR_SEED=12345 NOTSA_RR_STEP_MS=20 NOTSA_RR_NOAUDIO=1 NOTSA_STANDALONE_SCREENSHOT=1000000 NOTSA_STANDALONE_HIDPI=0 NOTSA_STANDALONE_WINDOWED=1280x720
if [ "$KIND" = native ]; then
  export NOTSA_LOCK_FILE="$OUT/$TAG.lock" NOTSA_AUDIO=null NOTSA_STANDALONE_NOVSYNC=1 NOTSA_STANDALONE_SHOT_GAMMA="${SHOTGAMMA:-0}"
  export EXE="${EXE:-$REPO/build/PortableMacVis2/bin/gta_reversed}" NOTSA_RR_TRACE="$OUT/$TAG.trace"
  "$REPO/tools/portable/mac_boot_run.sh" "$TAG" "$TMO" "$ROUTE"
  mv "$OUT"/gamedir/mark_*.bmp "$OUT/shots/$TAG/" 2>/dev/null; cp "$OUT/gamedir/standalone.log" "$OUT/$TAG.standalone.log" 2>/dev/null
else
  "$REPO/tools/portable/rr_run_win32.sh" "$TAG" "$TMO" "$ROUTE"
  mv "$OUT/gamedir-$TAG"/mark_*.bmp "$OUT/shots/$TAG/" 2>/dev/null
fi
ls "$OUT/shots/$TAG" | wc -l
