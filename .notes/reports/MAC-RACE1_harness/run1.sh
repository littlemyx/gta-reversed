#!/bin/bash
# usage: run1.sh <exe> <tag> [timeout]  -> prints exit + crash indicator
R=/Users/Andrei.Mukhin/github/recomp/gta-reversed
rm -f /private/tmp/claude-501/race1/out/userdir/gta_sa.set; export OUT=/private/tmp/claude-501/race1/out NOTSA_LOCK_FILE=/tmp/race1_gta.lock EXE="$1" NOTSA_STANDALONE_HIDPI=0
mkdir -p $OUT
/private/tmp/claude-501/race1/boot.sh "$2" "${3:-60}" /private/tmp/claude-501/race1/${NGSCRIPT:-ng.txt}
