#!/bin/bash
# cpulog.sh TAG : once a second: unix time, cumulative CPU seconds of the game's mac process and of the wineserver
OUT=/tmp/d3f-out/$1.cpu; : > $OUT
while :; do
  g=$(pgrep -f "Z:.tmp.d3f.*(gta_sa|gta_reversed).exe" | head -1); w=$(pgrep -f "wineserver" | head -1)
  t=$(perl -MTime::HiRes=time -e 'printf "%.2f",time')
  cg=$( [ -n "$g" ] && ps -o time= -p $g | tr -d ' ' ); cw=$( [ -n "$w" ] && ps -o time= -p $w | tr -d ' ' )
  echo "$t $cg $cw" >> $OUT; sleep 1
done
