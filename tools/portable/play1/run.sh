#!/bin/bash
# MAC-PLAY1: gameplay smoke routes on the native Mac exe. usage: tools/portable/play1/run.sh <tag> <timeout_s> <route.txt>   (routes in this dir, self-contained; free.inc = the shared header they start with)
#   s1 walk/sprint/jump  s2 car (cmd:roadveh, enter, drive 28 s, exit)  s3 AK + peds + wanted by shooting  s4 wanted 3 / police / busted  s5 18 cheats (gameplay_regress cheats list)
#   s6a save to slot 1 through the save menu (needs the userdir slot empty)  s6c load from the main menu (needs gta-reversed.ini [FastLoader] SaveGameToLoad=-2 in the game dir, else FastLoader loads slot 1 by itself)
#   s7 car + quit (shutdown)  s8 sprint 30 s + camera stat  s9 weapon pickups.  Injector commands used beyond the gameplay_regress ones: portable/play_cmd.cpp (roadveh, poolstat, playerstat, savemenu, pickup).
# Serialises with other agents' game runs (single-instance lock): waits while a gta_reversed process exists. Screenshots (marks) land in $OUT/shots/<tag>/ as BMP.
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../../.." && pwd)"
while pgrep -f "bin/gta_reversed" >/dev/null; do sleep 5; done
export OUT="${OUT:-${TMPDIR:-/tmp}/play1}" NOTSA_STANDALONE_PROBE=1 NOTSA_STANDALONE_SCREENSHOT=1000000 NOTSA_STANDALONE_HIDPI=0
export EXE="${EXE:-$REPO/build/PortableMac/bin/gta_reversed}"
mkdir -p "$OUT/shots/$1"
"$REPO/tools/portable/mac_boot_run.sh" "$1" "$2" "$HERE/$3"
mv "$OUT"/gamedir/mark_*.bmp "$OUT/shots/$1/" 2>/dev/null
