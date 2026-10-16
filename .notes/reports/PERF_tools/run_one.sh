#!/bin/bash
# run_one.sh orig|ours on|off tag   (env: EXE_O / EXE_R dirs fixed under /tmp/perf)
KIND=$1; LIM=$2; TAG=$3
export PATH="$HOME/tools/Wine Staging.app/Contents/Resources/wine/bin:$HOME/.local/bin:/opt/homebrew/bin:$PATH" WINEPREFIX=/tmp/perf/prefix
export WINEDEBUG=${WD:--all} WINEDLLOVERRIDES=d3d9=n,b
OUT=/tmp/perf/out; mkdir -p $OUT; LOG=$OUT/$TAG.txt
D="/tmp/perf/prefix/drive_c/users/Andrei.Mukhin/Documents/GTA San Andreas User Files"
pkill -f "perf.*gta_" 2>/dev/null; wineserver -k 2>/dev/null; sleep 2
cp /tmp/perf/tool/set_$LIM "$D/gta_sa.set"
if [ $KIND = orig ]; then DIR=/tmp/perf/o; EXE=gta_sa.exe; else DIR=${RDIR:-/tmp/perf/r}; EXE=gta_reversed.exe; fi
rm -f /tmp/perf/out/present_cur.bin /tmp/perf/out/slow_cur.txt; cd $DIR; rm -rf logs standalone*.log frame_*.bmp fprof.log perf.log /tmp/perf/out/exe_perf_cur.log
echo "# $TAG kind=$KIND limiter=$LIM start $(date +%T) load $(sysctl -n vm.loadavg)" > $LOG
ok=0; STARTED=0
for a in 1 2 3 4 5; do
  if [ $KIND = ours ]; then
    env NOTSA_STANDALONE_MUTE=1 NOTSA_STANDALONE_SKIP_VIDEOS=1 NOTSA_STANDALONE_INPUT=/tmp/perf/tool/ours_inj.txt wine $EXE > $OUT/$TAG.wine 2>&1 &
  else wine $EXE > $OUT/$TAG.wine 2>&1 & fi
  for w in $(seq 1 20); do sleep 2; if ps aux | grep -v grep | grep -q "Z:.tmp.perf.*$EXE"; then ok=1; break; fi; done
  if [ $ok = 1 ]; then
    printf 'until:started:50000\n' > /tmp/perf/tool/chk.txt
    if wine /tmp/perf/tool/sk3.exe /tmp/perf/tool/chk.txt $EXE 2>&1 | grep -q "started ok"; then STARTED=1; break; fi
    echo "# attempt $a: process alive but stuck in INITIAL, retry" >> $LOG
  fi
  ok=0
  echo "# attempt $a: not running, retry" >> $LOG; wineserver -k; sleep 2
done
if [ "$STARTED" != 1 ]; then echo "# START FAILED" >> $LOG; pkill -f "perf.*gta_" 2>/dev/null; wineserver -k 2>/dev/null; exit 0; fi
/tmp/perf/tool/cpulog.sh $TAG "Z:.tmp.perf.*$EXE" &
CPL=$!
SCRIPT=${SCRIPT:-/tmp/perf/tool/${PROFILE:-w}_$KIND.txt}
sed "s/@RUN@/$TAG/g" $SCRIPT > /tmp/perf/tool/cur_$TAG.txt
wine /tmp/perf/tool/sk3.exe /tmp/perf/tool/cur_$TAG.txt $EXE 2>&1 | while IFS= read -r l; do echo "$(date +%T) load=$(sysctl -n vm.loadavg | tr -d '{}') | $l" >> $LOG; done
kill $CPL 2>/dev/null
mv /tmp/perf/out/present_cur.bin /tmp/perf/out/$TAG.present.bin 2>/dev/null; mv /tmp/perf/out/slow_cur.txt /tmp/perf/out/$TAG.slow 2>/dev/null
[ -f fprof.log ] && cp fprof.log /tmp/perf/out/$TAG.fprof.log; [ -f perf.log ] && cp perf.log /tmp/perf/out/$TAG.fprof.log; [ -f /tmp/perf/out/exe_perf_cur.log ] && mv /tmp/perf/out/exe_perf_cur.log /tmp/perf/out/$TAG.fprof.log
echo "# end $(date +%T) load $(sysctl -n vm.loadavg)" >> $LOG
pkill -f "perf.*gta_" 2>/dev/null; wineserver -k 2>/dev/null; sleep 2
