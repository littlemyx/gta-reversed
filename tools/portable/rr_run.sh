#!/bin/bash
# RR-1: one deterministic record run of the native exe (build with -DGTASA_STATEHASH=ON). Usage:
#   tools/portable/rr_run.sh <tag> <timeout_s> [route] [frames]       -> $OUT/<tag>.trace (+ $OUT/<tag>.out, $OUT/gamedir/standalone.log)
# env: EXE (default build/PortableMacRR/bin/gta_reversed), OUT (default /tmp/rr), RR_SEED (12345), RR_STEP_MS (20), RR_DETAIL (A-B, per-entity D lines for those recorded frames),
#      RR_MAXFRAMES (stop recording after N frames and exit), RR_TAG (build name for the trace header).
# Pins what the harness can pin (see .notes/RR_PLAN.md): NOTSA_RR=1 (virtual clock, synchronous CdStream, fixed seed, virtual script clock), NOTSA_AUDIO=null, no videos, NOVSYNC.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
TAG="${1:?tag}"; TMO="${2:-240}"; ROUTE="${3:-$REPO/tools/portable/rr_route_walk.txt}"
export OUT="${OUT:-/tmp/rr}"; mkdir -p "$OUT"
export EXE="${EXE:-$REPO/build/PortableMacRR/bin/gta_reversed}"
export NOTSA_RR=1 NOTSA_RR_SEED="${RR_SEED:-12345}" NOTSA_RR_STEP_MS="${RR_STEP_MS:-20}" NOTSA_RR_TRACE="$OUT/$TAG.trace" NOTSA_RR_TAG="${RR_TAG:-portable-$(git -C "$REPO" rev-parse --short HEAD)}"
[ -n "${RR_DETAIL:-}" ] && export NOTSA_RR_DETAIL="$RR_DETAIL"
[ -n "${RR_MAXFRAMES:-}" ] && export NOTSA_RR_MAXFRAMES="$RR_MAXFRAMES" NOTSA_RR_EXIT=1
export NOTSA_LOCK_FILE="$OUT/$TAG.lock" NOTSA_AUDIO=null NOTSA_STANDALONE_NOVSYNC=1
rm -f "$OUT/$TAG.trace"
"$REPO/tools/portable/mac_boot_run.sh" "$TAG" "$TMO" "$ROUTE"
echo "trace: $OUT/$TAG.trace ($(grep -c '^F ' "$OUT/$TAG.trace" 2>/dev/null || echo 0) frames)"
