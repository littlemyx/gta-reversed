#!/bin/bash
cd /tmp/d3f-tool
./logger.sh > /tmp/d3f-out/hostload.log 2>&1 &
LP=$!
n=${START:-0}
waitquiet(){ [ -n "$QUIET" ] || return; local t0=$(date +%s); while :; do l=$(sysctl -n vm.loadavg | awk '{print $2}'); echo "quiet-check load=$l" >> /tmp/d3f-out/driver.log; [ $(echo "$l < 6" | bc) = 1 ] && return; [ $(( $(date +%s) - t0 )) -gt 1200 ] && { echo 'quiet window not found, proceeding' >> /tmp/d3f-out/driver.log; QUIET=; return; }; sleep 30; done; }
SPECS=("$@")
for spec in "${SPECS[@]}"; do
  set -- $spec; n=$((n+1)); waitquiet; ./run_one.sh $1 $2 r${n}_$1_$2
  echo "done r${n} $spec $(date +%T)" >> /tmp/d3f-out/driver.log
done
kill $LP
echo ALLDONE >> /tmp/d3f-out/driver.log
