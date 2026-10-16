#!/bin/bash
# run_one.sh orig|ours on|off tag
KIND=$1; LIM=$2; TAG=$3
. /tmp/d3f-env.sh
export WINEDEBUG=${WD:--all} WINEDLLOVERRIDES=d3d9=n,b
OUT=/tmp/d3f-out; mkdir -p $OUT; LOG=$OUT/$TAG.txt
D="/tmp/d3f-prefix/drive_c/users/Andrei.Mukhin/Documents/GTA San Andreas User Files"
pkill -f "d3f.*gta_" 2>/dev/null; wineserver -k 2>/dev/null; sleep 2
cp /tmp/d3f-tool/set_$LIM "$D/gta_sa.set"
if [ $KIND = orig ]; then DIR=/tmp/d3fo; EXE=gta_sa.exe; else DIR=/tmp/d3f; EXE=gta_reversed.exe; fi
rm -f /tmp/d3f-out/present_cur.bin; cd $DIR; rm -rf logs standalone*.log frame_*.bmp
echo "# $TAG kind=$KIND limiter=$LIM start $(date +%T) load $(sysctl -n vm.loadavg)" > $LOG
ok=0
for a in 1 2 3 4 5; do
  if [ $KIND = ours ]; then
    EXTRA=""; [ -n "$SHOTS" ] && EXTRA="NOTSA_STANDALONE_SCREENSHOT=$SHOTS NOTSA_STANDALONE_SCREENSHOT_MAX=80"
    env NOTSA_STANDALONE_SKIP_VIDEOS=1 $EXTRA NOTSA_STANDALONE_INPUT=/tmp/d3f-tool/ours_inj.txt wine $EXE > $OUT/$TAG.wine 2>&1 &
  else wine $EXE > $OUT/$TAG.wine 2>&1 & fi
  for w in $(seq 1 20); do sleep 2; if ps aux | grep -v grep | grep -q "Z:.tmp.d3f.*$EXE"; then ok=1; break; fi; done
  if [ $ok = 1 ]; then
    printf 'until:started:50000\n' > /tmp/d3f-tool/chk.txt
    if wine /tmp/d3f-tool/sk3.exe /tmp/d3f-tool/chk.txt $EXE 2>&1 | grep -q "started ok"; then break; fi
    echo "# attempt $a: process alive but stuck in INITIAL, retry" >> $LOG
  fi
  ok=0
  echo "# attempt $a: not running, retry" >> $LOG; wineserver -k; sleep 2
done
/tmp/d3f-tool/cpulog.sh $TAG &
CPL=$!
SCRIPT=${SCRIPT:-/tmp/d3f-tool/${PROFILE:-ft}_$KIND.txt}
sed "s/@RUN@/$TAG/g" $SCRIPT > /tmp/d3f-tool/cur_$TAG.txt
wine /tmp/d3f-tool/sk3.exe /tmp/d3f-tool/cur_$TAG.txt $EXE 2>&1 | while IFS= read -r l; do echo "$(date +%T) load=$(sysctl -n vm.loadavg | tr -d '{}') cpu=$(ps -A -o %cpu= | awk '{s+=$1} END {printf "%d",s/18}')% | $l" >> $LOG; done
kill $CPL 2>/dev/null
mv /tmp/d3f-out/present_cur.bin /tmp/d3f-out/$TAG.present.bin 2>/dev/null
echo "# end $(date +%T) load $(sysctl -n vm.loadavg)" >> $LOG
pkill -f "d3f.*gta_" 2>/dev/null; wineserver -k 2>/dev/null; sleep 2
