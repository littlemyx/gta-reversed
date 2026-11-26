#!/bin/bash
# PAR: the allmissions.sh regression sweep over N game processes at once.
# usage: tools/standalone/mdriver/par_allmissions.sh <outdir> <N> [idx list (default: every idx of $WEIGHTS)]   (env: EXE, WEIGHTS, SRC_DATA_LIST, and everything allmissions.sh reads)
# It takes the GPU lock ONCE for the whole batch (gpu_locked.sh --acquire/--release, EXIT trap), splits the list over N slots (longest first by the frame counts of $WEIGHTS = a
# regression summary: idx name result frame ..., greedy LPT), and runs allmissions.sh once per slot with its own OUT (<outdir>/s<k>), DATA (<outdir>/d<k>) and UD (<outdir>/u<k>).
# Output: <outdir>/summary.tsv (the allmissions.sh row format, idx order of the list) + <outdir>/par.log (per slot wall time). Per-mission artifacts stay in <outdir>/s<k>.
# NOTSA_LOCK_FILE: each slot gets its own single-instance mutex name (the exe refuses a second instance otherwise: instant exit, no result).
# env: WEIGHTS (default C:/gta-scratch/cr60/regression.final.txt), MAXF/WALL/RR_SEED/TABLE/... pass through to allmissions.sh, GPU_OWNER_TXT (lock owner text), NOLOCK=1 (skip the lock: only when the caller holds it)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/../hostenv.sh"; . "$HERE/../own_kill.sh"
OUT="$(hp "${1:?outdir}")"; N="${2:?N}"; shift 2
LIST="${*:-}"; WEIGHTS="${WEIGHTS:-C:/gta-scratch/cr60/regression.final.txt}"
[ -z "$LIST" ] && LIST="$(cut -f1 "$WEIGHTS" | tr '\n' ' ')"
mkdir -p "$OUT"
# greedy LPT: weight = frames of the reference row (non-numeric -> 30000, unknown idx -> 10000)
for i in $(seq 1 "$N"); do echo -n > "$OUT/list.$i"; done
for idx in $LIST; do
  w=$(awk -F'\t' -v i="$idx" '$1==i{print $4; exit}' "$WEIGHTS"); case "${w:-}" in ''|*[!0-9]*) [ -z "${w:-}" ] && w=10000 || w=30000;; esac
  echo "$w $idx"
done | sort -rn | awk -v n="$N" -v o="$OUT" '{ m=1; for(i=2;i<=n;i++) if (load[i]<load[m]) m=i; load[m]+=$1; print $2 >> (o "/list." m) }'
if [ -z "${NOLOCK:-}" ]; then
  "$HERE/../gpu_locked.sh" --acquire "${GPU_OWNER_TXT:-PAR sweep N=$N}" $$ || exit 3
  trap '"$HERE/../gpu_locked.sh" --release $$' EXIT
fi
T0=$(date +%s); PIDS=""
for i in $(seq 1 "$N"); do
  L="$(tr '\n' ' ' < "$OUT/list.$i")"; [ -z "${L// /}" ] && continue
  rm -f "$OUT/s$i/summary.tsv"
  ( t=$(date +%s); NOTSA_LOCK_FILE="par_$$_$i" DATA="$OUT/d$i" UD="$OUT/u$i" "$HERE/allmissions.sh" "$OUT/s$i" $L > "$OUT/slot$i.out" 2>&1; echo "slot $i missions=$(echo $L | wc -w) wall=$(( $(date +%s) - t ))s" >> "$OUT/par.log" ) &
  PIDS="$PIDS $!"
done
trap 'for p in $PIDS; do own_kill_pid $p; done; for i in $(seq 1 "$N"); do own_kill_in_dir gta_reversed "$OUT/d$i"; done; [ -z "${NOLOCK:-}" ] && "$HERE/../gpu_locked.sh" --release $$' EXIT
wait
echo "total wall=$(( $(date +%s) - T0 ))s N=$N" >> "$OUT/par.log"
printf 'idx\tname\tresult\tframe\tthread\tip\topcode\treason\n' > "$OUT/summary.tsv"
for idx in $LIST; do for i in $(seq 1 "$N"); do [ -f "$OUT/s$i/summary.tsv" ] && awk -F'\t' -v x="$idx" '$1==x' "$OUT/s$i/summary.tsv"; done; done >> "$OUT/summary.tsv"
