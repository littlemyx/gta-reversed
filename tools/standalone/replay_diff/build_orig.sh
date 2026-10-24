#!/bin/bash
# builds the REPLAY-DIFF original-exe tracer (vorbisFile.dll proxy) into $1 (default C:/gta-perf/rd). Needs the MSVC x86 env (source tools/standalone/windows/msvcenv.sh).
D="$(cd "$(dirname "$0")" && pwd)"; OUT="${1:-C:/gta-perf/rd}"; mkdir -p "$OUT"
cd "$D" && cl //nologo //O2 //LD //Fe:"$(cygpath -w "$OUT/vorbisFile.dll")" //Fo:"$(cygpath -w "$OUT/")" rd_orig.c //link //DEF:rd_orig.def
