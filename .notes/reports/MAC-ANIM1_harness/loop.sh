#!/bin/bash
# usage: loop.sh <exe> <prefix> <n> <nyes>
H=/private/tmp/claude-501/anim1
exe=$1; pre=$2; n=$3; ny=$4
pids=()
if [ "$LOADKIND" = cc ]; then $H/cload.sh $ny & pids+=($!); else for i in $(seq 1 $ny); do yes > /dev/null & pids+=($!); done; fi
trap "pkill -f cload.sh; pkill -f race1/cc_; kill ${pids[*]} 2>/dev/null" EXIT
sleep 20
for i in $(seq 1 $n); do
  ld=$(uptime | sed 's/.*averages: //')
  rm -f $H/out/userdir/gta_sa.set; r=$($H/run1.sh $exe ${pre}_$i ${TMO:-60} 2>&1 | tail -1)
  c=$(grep -c -i "RemoveAndAdd\|SIGSEGV\|crash" $H/out/${pre}_$i.out)
  echo "$pre $i load[$ld] $r crashmatch=$c"
done
