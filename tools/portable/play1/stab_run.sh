#!/bin/bash
# MAC-STAB1: one real-time run of a route on the native exe with its own lock file (no RR virtual time). usage: stab_run.sh <tag> <timeout_s> <route.txt>   env: EXE, OUT
# Serialises with other GL processes (never two at once). Logs: $OUT/<tag>.out, $OUT/gamedir/standalone.log copied to $OUT/<tag>.standalone.log, marks -> $OUT/shots/<tag>/
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../.." && pwd)"
TAG="$1"; TMO="$2"; ROUTE="$3"
export OUT="${OUT:-/tmp/stab1}"; mkdir -p "$OUT/shots/$TAG"
while pgrep -f "bin/gta_reversed" >/dev/null; do sleep 5; done
# the menu cursor follows the REAL mouse pointer: a pointer over the game window (or one a person moves meanwhile) puts the cursor on "Options" and the scripted Returns never start a game. Park it at (5,5) now and keep it there while the menu part of the route runs.
python3 "$HERE/park_pointer.py" 0 2>/dev/null || true
python3 "$HERE/park_pointer.py" "${PARK_POINTER_SECS:-14}" >/dev/null 2>&1 &
export NOTSA_LOCK_FILE="$OUT/$TAG.lock" NOTSA_AUDIO="${NOTSA_AUDIO:-null}" NOTSA_STANDALONE_NOVSYNC=1 NOTSA_STANDALONE_SCREENSHOT=1000000 NOTSA_STANDALONE_HIDPI=0
export EXE="${EXE:-$REPO/build/PortableMacStab1/bin/gta_reversed}"
case "$ROUTE" in /*) R="$ROUTE";; *) R="$HERE/$ROUTE";; esac
"$REPO/tools/portable/mac_boot_run.sh" "$TAG" "$TMO" "$R"
mv "$OUT"/gamedir/mark_*.bmp "$OUT/shots/$TAG/" 2>/dev/null; cp "$OUT/gamedir/standalone.log" "$OUT/$TAG.standalone.log" 2>/dev/null
rm -f "$OUT/$TAG.wav"
