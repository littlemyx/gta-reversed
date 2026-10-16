#!/bin/bash
# bv.sh NAME flags... : build bench variant and run
N=$1; shift
export PATH="$HOME/tools/Wine Staging.app/Contents/Resources/wine/bin:$HOME/.local/bin:/opt/homebrew/bin:$PATH" WINEDEBUG=-all
WINEPREFIX=$HOME/.wine-msvc taskpolicy -b nice -n 15 python3 /tmp/perf/tool/bench.py /tmp/perf/bench/out/$N "$@" 2>&1 | grep -E "^\('|fatal error|link [1-9]" | grep -v "', 0, \[\])"
cd /tmp/perf/bench/out/$N && WINEPREFIX=/tmp/perf/prefix BENCH=1 RW_EXE_ORACLE=/Users/Andrei.Mukhin/github/recomp/gta-reversed/gta_sa_compact.exe wine bench.exe 2>&1 | tail -9 | sed "s/^/[$N] /"
