#!/bin/bash
# WIN-PERF: one native frame-time run of the ORIGINAL exe or OUR exe through the scenes in fps_scenes.txt, Present log via the d3d9px proxy, input via inputdrv.exe.
# usage: fps_run.sh orig|ours on|off TAG [video-mode-index (default: keep the set file's 4)] [script (default fps_scenes.txt)]
#   limiter on|off = m_bPrefsFrameLimiter in gta_sa.set (byte 0xB35); the video mode index is the int32 at 0xB36 (index into the exe's own mode list; 4 = 800x600 X8R8G8B8 for ours).
# prerequisites: tools built (build_tools.sh -> C:/gta-perf/px, dsmute.dll), data dirs C:/gta-perf/ours and C:/gta-perf/orig (perf_datadir.sh), base set file C:/gta-scratch/bugquit/gta_sa.set.good.
# Takes/releases the GPU lock (C:/gta-scratch/gpu.lock) itself. The original runs SILENT: dsound.dll proxy (dsmute) + SFX/radio volume 0 + empty movies dir. Output: C:/gta-perf/runs/TAG.{bin,log,csv}
# The user's real Documents\GTA San Andreas User Files\gta_sa.set is replaced for the run and DELETED afterwards (soak.sh does the same).
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/../hostenv.sh"
KIND="$1"; LIM="$2"; TAG="$3"; VM="${4:-}"; SCR="${5:-$HERE/fps_scenes.txt}"
[ -n "$TAG" ] || { sed -n 2,8p "$0"; exit 2; }
PY="${GTASA_PYTHON:-/c/Users/littl/AppData/Local/Programs/Python/Python312/python.exe}"
PX=C:/gta-perf/px; RUNS=C:/gta-perf/runs; mkdir -p $RUNS
if [ "$KIND" = ours ]; then DIR=C:/gta-perf/ours; EXE=gta_reversed.exe; else DIR=C:/gta-perf/orig; EXE=gta_sa.exe; fi
DOC="$(cygpath -u "$USERPROFILE")/Documents/GTA San Andreas User Files"; SET="$DOC/gta_sa.set"
cleanup() { taskkill //F //IM $EXE >/dev/null 2>&1; rm -f "$SET"; rm -rf C:/gta-scratch/gpu.lock; }
until mkdir C:/gta-scratch/gpu.lock 2>/dev/null; do sleep 3; done; echo "WIN-PERF fps_run $TAG $(date)" > C:/gta-scratch/gpu.lock/owner; trap cleanup EXIT
taskkill //F //IM $EXE >/dev/null 2>&1
# settings file
[ -f "$SET" ] && [ ! -e "$SET.soak-bak" ] && cp -p "$SET" "$SET.soak-bak"
SFX=64; [ "$KIND" = orig ] && SFX=0
"$PY" - "$LIM" "$VM" "$SFX" "$SET" <<'EOPY'
import sys
lim, vm, sfx, out = sys.argv[1:5]
b = bytearray(open('C:/gta-scratch/bugquit/gta_sa.set.good', 'rb').read())
b[0xB35] = 1 if lim == 'on' else 0
if vm: b[0xB36:0xB3A] = int(vm).to_bytes(4, 'little')
b[0xB1B] = int(sfx); b[0xB1C] = int(sfx)
b[0xB1E] = 0                                  # radio auto-select OFF: the Steam install ships silent stub radio tracks, the ORIGINAL divides by zero (0x4F1464, stream thread) when a car enter starts one      # SFX / radio volume (0 for the original: belt and braces next to the dsound mute proxy)
open(out, 'wb').write(b)
EOPY
cd "$DIR" || exit 2
rm -rf logs standalone*.log frame_*.bmp mark_*.bmp "$RUNS/$TAG".*
cp $PX/d3d9.dll .
if [ "$KIND" = orig ]; then cp $PX/dsound.dll .; rm -f dsmute.log; [ -L movies ] || [ -d movies ] && { cmd //c rmdir "$(cygpath -w "$PWD/movies")" >/dev/null 2>&1; }; mkdir -p movies; fi
export D3D9PX_OUT="C:\\gta-perf\\runs\\$TAG.bin" D3D9PX_CAPFILE="C:/gta-perf/runs/$TAG.cap" D3D9PX_SHOTFILE="C:/gta-perf/runs/$TAG.shotreq" D3D9PX_NOVSYNC=1; echo 60 > "$RUNS/$TAG.cap"
if [ "$KIND" = ours ]; then export NOTSA_STANDALONE_MUTE=1 NOTSA_STANDALONE_SKIP_VIDEOS=1; fi
if [ -n "${CDB:-}" ]; then   # CDB=1: run the exe under cdb (first-chance dz/av break prints the stack to $TAG.cdb, then quits)
  /c/gta-scratch/dbg/cdb.exe -g -G -c 'sxd *; sxe dz; sxe av; g; .echo ---CRASH; r; kv 30; ~*kb 12; .echo ---END; q' ./$EXE > $RUNS/$TAG.cdb 2>&1 &
else
  ./$EXE > $RUNS/$TAG.out 2>&1 &
fi
sleep 5
if ! tasklist | grep -qi "$EXE"; then echo "fps_run: $EXE did not start"; exit 2; fi
sed "s/@RUN@/$TAG/g" "$SCR" > $RUNS/$TAG.script
$PX/inputdrv.exe "$(cygpath -w $RUNS/$TAG.script)" $EXE > $RUNS/$TAG.log 2>&1
echo "fps_run: inputdrv rc=$? ($TAG)"
[ -f dsmute.log ] && cp dsmute.log $RUNS/$TAG.dsmute.log
[ -f "$RUNS/$TAG.cdb" ] && grep -q -- "---CRASH" "$RUNS/$TAG.cdb" && echo "fps_run: the exe crashed under cdb, stack in $RUNS/$TAG.cdb"; cp logs/log.log $RUNS/$TAG.gamelog 2>/dev/null; cp standalone.log $RUNS/$TAG.standalone.log 2>/dev/null
cleanup; trap - EXIT
sleep 1
"$PY" "$HERE/frame_stats.py" $RUNS/$TAG.bin --scenes $RUNS/$TAG.log --csv $RUNS/results.csv --tag "$TAG"
