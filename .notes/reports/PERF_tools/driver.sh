#!/bin/bash
cd /tmp/perf/tool
n=${START:-1}
for spec in "$@"; do
  set -- $spec; n=$((n+1)); ./run_one.sh $1 $2 a${n}_$1
  echo "done a${n} $spec $(date +%T)" >> /tmp/perf/out/driver.log
done
echo ALLDONE >> /tmp/perf/out/driver.log
