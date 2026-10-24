#!/bin/bash
# RR-1 acceptance: the native exe run twice from the same script + seed must give identical traces. Usage:
#   tools/portable/rr_selftest.sh [timeout_s=240] [route]        env as tools/portable/rr_run.sh (+ RR_RUNS=2 number of runs, ENVX="VAR=val ..." extra env for every run)
# Runs one after the other (the exe holds a per-run lock file, but the GL window and CPU make parallel runs unreliable). Exit 0 = all traces identical.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
TMO="${1:-240}"; ROUTE="${2:-$HERE/rr_route_walk.txt}"; RUNS="${RR_RUNS:-2}"
export OUT="${OUT:-/tmp/rr}"
rc=0
for i in $(seq 1 "$RUNS"); do
  env ${ENVX:-} "$HERE/rr_run.sh" "st$i" "$TMO" "$ROUTE" | tail -2
done
for i in $(seq 2 "$RUNS"); do
  python3 "$HERE/rr_diff.py" "$OUT/st1.trace" "$OUT/st$i.trace" || rc=1
done
exit $rc
