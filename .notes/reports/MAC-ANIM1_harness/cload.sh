#!/bin/bash
# cload.sh N : N workers compiling in a loop; kill via pkill -f cworker
H=/private/tmp/claude-501/anim1
for i in $(seq 1 $1); do
  ( while true; do for f in $H/cc_*.sh; do bash $f >/dev/null 2>&1; done; done ) &
done
wait
