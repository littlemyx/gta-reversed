#!/bin/bash
# usage: batch.sh <exe> <prefix> <n> [load_workers]   (env passthrough: NOTSA_MALLOC_POISON etc.)
H=/private/tmp/claude-501/anim1
exe=$1; pre=$2; n=$3; nw=${4:-0}
export NGSCRIPT=ng2.txt
if [ "$nw" -gt 0 ]; then $H/cload.sh $nw & fi
trap 'pkill -f anim1/cc_ ; pkill -f anim1/cload.sh; pkill -f "bash $H/cc_"; true' EXIT
[ "$nw" -gt 0 ] && sleep 15
ok=0; bad=0; invalid=0
for i in $(seq 1 $n); do
  ld=$(uptime | sed 's/.*averages: //')
  r=$($H/run1.sh $exe ${pre}_$i ${TMO:-70} 2>&1 | tail -1)
  f=$H/out/${pre}_$i.out
  lg=$(grep -a -c "Loading the Game" $f); cr=$(grep -a -c "UNHANDLED SIGNAL" $f); sm=$(grep -a -c "ANIM1 SCAN\|RACE1 Rem\|RACE1 Add BAD" $f)
  if [ "$lg" -eq 0 ]; then v=INVALID; invalid=$((invalid+1)); elif [ "$cr" -gt 0 ]; then v=CRASH; bad=$((bad+1)); else v=OK; ok=$((ok+1)); fi
  echo "$pre $i $v load[$ld] $r loading=$lg crash=$cr diag=$sm"
done
echo "SUMMARY $pre ok=$ok crash=$bad invalid=$invalid"
