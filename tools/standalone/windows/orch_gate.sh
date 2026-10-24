#!/bin/bash
# Orchestrator gate: run_all_tests + Release + soak (new game); refresh C:\gta-play only when the soak is clean.
# usage: gate.sh [regress]   (regress = also run gameplay_regress.sh without render_firststart)
cd /c/Users/littl/github/gta-reversed || exit 2
source tools/standalone/windows/msvcenv.sh >/dev/null 2>&1
echo "HEAD $(git log --oneline -1)"
GTA_JOBS=4 tools/standalone/run_all_tests.sh 2>&1 | grep -E "^summary| (fail|nobuild|noresult|timeout)|hang"
# Re-run CMake first: sources are globbed at configure time, so a new .cpp from a merge (e.g. standalone/Lockstep.cpp) is otherwise not built (LNK2019, 2026-10-24).
cmake build/StandaloneRelease > /dev/null 2>&1 || echo "cmake reconfigure of build/StandaloneRelease FAILED"
tools/standalone/build_locked.sh cmd //c start //low //b //wait ninja -j4 -C build/StandaloneRelease gta_reversed.exe 2>&1 | tail -1
S=C:/gta-scratch/gate_soak/$(date +%m%d-%H%M)
NOTSA_STANDALONE_NOVSYNC=1 OUT=$S tools/standalone/soak.sh build/StandaloneRelease 3 > $S.log 2>&1; rc=$?
echo "soak: $(grep -a -E '^RESULT' $S.log | tail -1) rc=$rc ($S.log)"
if [ "$1" = regress ]; then
  GR_SKIP=render_firststart NOTSA_STANDALONE_NOVSYNC=1 tools/standalone/gameplay_regress.sh > $S.regress.log 2>&1
  echo "regress rc=$? $(grep -a -E 'PASS|FAIL' $S.regress.log | tail -3 | tr '\n' ' ')"
fi
if [ $rc = 0 ]; then
  o=$(cat C:/gta-scratch/gpu.lock/owner 2>/dev/null)
  case "$o" in USER-PLAY*) echo "user playing, skip copy";; *) cp build/StandaloneRelease/bin/gta_reversed.exe /c/gta-play/ && echo "C:\\gta-play refreshed";; esac
else echo "SOAK FAILED - C:\\gta-play NOT refreshed"; fi
