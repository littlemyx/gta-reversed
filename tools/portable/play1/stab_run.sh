#!/bin/bash
# MAC-STAB1: one real-time run of a route on the native exe with its own lock file (no RR virtual time). usage: stab_run.sh <tag> <timeout_s> <route.txt>   env: EXE, OUT
# Serialises with other GL processes (never two at once). Logs: $OUT/<tag>.out, $OUT/gamedir/standalone.log copied to $OUT/<tag>.standalone.log, marks -> $OUT/shots/<tag>/
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../.." && pwd)"
TAG="$1"; TMO="$2"; ROUTE="$3"
export OUT="${OUT:-/tmp/stab1}"; mkdir -p "$OUT/shots/$TAG"
while pgrep -f "bin/gta_reversed" >/dev/null; do sleep 5; done
export NOTSA_LOCK_FILE="$OUT/$TAG.lock" NOTSA_AUDIO="${NOTSA_AUDIO:-null}" NOTSA_STANDALONE_NOVSYNC=1 NOTSA_STANDALONE_SCREENSHOT=1000000 NOTSA_STANDALONE_HIDPI=0
export EXE="${EXE:-$REPO/build/PortableMacStab1/bin/gta_reversed}"
case "$ROUTE" in /*) R="$ROUTE";; *) R="$HERE/$ROUTE";; esac
"$REPO/tools/portable/mac_boot_run.sh" "$TAG" "$TMO" "$R"
mv "$OUT"/gamedir/mark_*.bmp "$OUT/shots/$TAG/" 2>/dev/null; cp "$OUT/gamedir/standalone.log" "$OUT/$TAG.standalone.log" 2>/dev/null
rm -f "$OUT/$TAG.wav"
