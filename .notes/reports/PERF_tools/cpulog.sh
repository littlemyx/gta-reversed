#!/bin/bash
# cpulog.sh TAG PATTERN : once a second: epoch + per-thread cumulative cpu (ps -M); raw -M lines, parsed offline
OUT=/tmp/perf/out/$1.cpu; : > $OUT
while :; do
  g=$(pgrep -f "$2" | head -1)
  if [ -n "$g" ]; then
    t=$(perl -MTime::HiRes=time -e 'printf "%.2f",time')
    ps -M -p $g 2>/dev/null | tail -n +2 | awk -v t=$t '{ s=""; for(i=1;i<=NF;i++) if ($i ~ /^[0-9]+:[0-9][0-9]\.[0-9][0-9]$/) s=s" "$i; printf "%s %d%s\n", t, NR, s }' >> $OUT
  fi
  sleep 1
done
