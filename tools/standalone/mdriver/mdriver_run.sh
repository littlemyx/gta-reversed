#!/bin/bash
# MDRIVER-1: record a driven mission run, then replay it twice WITHOUT the driver from the recorded input.
# usage: tools/standalone/gpu_locked.sh -o "MDRIVER" tools/standalone/mdriver/mdriver_run.sh <steps file> [outdir] [limit_s]
# build: cmake --preset StandaloneRelease -B build/StandaloneReleaseRR -DGTASA_STATEHASH=ON -DGTASA_SCMTRACE=ON -DGTASA_MDRIVER=ON
# outputs in <outdir> (default C:/gta-scratch/scmtrace/out/full): record.scmtrace record.rr.trace driver_record.txt (route lines `at:<ms>` + `cmd:...`), replay1/2.*, replay_route.txt
set -u
D="$(cd "$(dirname "$0")" && pwd)"; STEPS="$(cygpath -w "$(cd "$(dirname "${1:?steps file}")" && pwd)/$(basename "$1")")"; OUT="${2:-C:/gta-scratch/scmtrace/out/full}"; LIMIT="${3:-1800}"
RUN="$D/../scmtrace_run.sh"; mkdir -p "$OUT"
HEAD="# MDRIVER-1 base route: New Game, 6 standing peds + a police car (pins the population), rr_start, injected start of mission index 24 (GUNS1 = RYDER_1)
#include header
cmd:spawnmany:6:7:0
cmd:spawnveh:596:1:14
wait:600
cmd:rr_start
mission:24"
printf '%s\nwait:3600000\ncmd:quit\n' "$HEAD" > "$OUT/driver_route.txt"
rm -f "$OUT/driver_record.txt"
NOTSA_MDRIVER="$STEPS" NOTSA_MDRIVER_OUT="$(cygpath -w "$OUT/driver_record.txt")" "$RUN" record "$OUT/driver_route.txt" "$LIMIT" "$OUT"
printf '%s\n' "$HEAD" > "$OUT/replay_route.txt"; cat "$OUT/driver_record.txt" >> "$OUT/replay_route.txt"
for r in replay1 replay2; do "$RUN" $r "$OUT/replay_route.txt" "$LIMIT" "$OUT"; done
for e in scmtrace rr.trace; do md5sum "$OUT"/record.$e "$OUT"/replay1.$e "$OUT"/replay2.$e; done
