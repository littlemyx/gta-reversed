#!/bin/bash
# usage: run_orig.sh <script> <tag> <maxsecs> : original exe in C:/gta-perf/orig with inputdrv + orig_probe.py
SCR="$1"; TAG="$2"; LIM="${3:-200}"
PX=C:/gta-perf/px; DIR=C:/gta-perf/orig; OUT=C:/gta-scratch/ptk_out/orig_$TAG; mkdir -p $OUT
PY=/c/Users/littl/AppData/Local/Programs/Python/Python312/python.exe
DOC="$(cygpath -u "$USERPROFILE")/Documents/GTA San Andreas User Files"; SET="$DOC/gta_sa.set"
cleanup() { taskkill //F //IM gta_sa.exe >/dev/null 2>&1; rm -f "$SET"; rm -rf C:/gta-scratch/gpu.lock; }
until mkdir C:/gta-scratch/gpu.lock 2>/dev/null; do sleep 2; done; echo "PT2-CORPSE orig $(date)" > C:/gta-scratch/gpu.lock/owner; trap cleanup EXIT
taskkill //F //IM gta_sa.exe >/dev/null 2>&1
[ -f "$SET" ] && [ ! -e "$SET.soak-bak" ] && cp -p "$SET" "$SET.soak-bak"
"$PY" - "$SET" <<'EOPY'
import sys
b = bytearray(open('C:/gta-scratch/bugquit/gta_sa.set.good', 'rb').read())
b[0xB35] = 0; b[0xB1B] = 0; b[0xB1C] = 0; b[0xB1E] = 0
open(sys.argv[1], 'wb').write(b)
EOPY
cd $DIR; rm -rf logs dsmute.log
cp $PX/d3d9.dll . ; cp $PX/dsound.dll .; mkdir -p movies
export D3D9PX_OUT="C:\gta-perf\runs\ptk_$TAG.bin" D3D9PX_NOVSYNC=1 D3D9PX_SHOTFILE="C:/gta-perf/runs/ptk_$TAG.shotreq"
./gta_sa.exe > $OUT/run.out 2>&1 &
sleep 5
tasklist | grep -qi gta_sa.exe || { echo "orig did not start"; exit 2; }
"$PY" -I C:/gta-scratch/ptk_probe/orig_dead.py gta_sa.exe $LIM $OUT/dead > $OUT/probe.txt 2>&1 &
$PX/inputdrv.exe "$(cygpath -w "$SCR")" gta_sa.exe > $OUT/inputdrv.log 2>&1
echo "inputdrv rc=$?"
sleep 2
cp logs/log.log $OUT/ 2>/dev/null
cleanup; trap - EXIT
