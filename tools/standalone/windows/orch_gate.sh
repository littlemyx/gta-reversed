#!/bin/bash
# Orchestrator gate: run_all_tests + Release + soak (new game) + PortableWin64 compile; refresh C:\gta-play only when all of them are clean.
# usage: gate.sh [regress]   (regress = also run gameplay_regress.sh without render_firststart)
cd /c/Users/littl/github/gta-reversed || exit 2
source tools/standalone/windows/msvcenv.sh >/dev/null 2>&1
echo "HEAD $(git log --oneline -1)"
# GPULOCK-PRIO: reserve the GPU lock queue (C:/gta-scratch/gpu.prio) so sweep agents' gameplay_regress/soak runs stop queueing in front of the gate's run_all_tests/soak/regress.
# Children inherit GPU_PRIO_OWNER and acquire normally; the reservation is dropped on exit (stale = dead pid or > 6 h is ignored by the waiters). See the gpu_locked.sh header.
if tools/standalone/gpu_locked.sh --prio-reserve "orch_gate" $$; then export GPU_PRIO_OWNER=$$; trap 'tools/standalone/gpu_locked.sh --prio-release $$' EXIT; fi
GTA_JOBS=4 tools/standalone/run_all_tests.sh > /c/gta-scratch/gate_soak/last_run_all_tests.txt 2>&1; trc=$?
# Statuses are upper case (FAIL, EXITn, NORESULT, NOBUILD, TIMEOUT): print every non-ok row, not only the summary
grep -a -E "^summary|^[a-z0-9_]+ .* (FAIL|EXIT[0-9]+|NORESULT|NOBUILD|TIMEOUT)$|hang" /c/gta-scratch/gate_soak/last_run_all_tests.txt
# Re-run CMake first: sources are globbed at configure time, so a new .cpp from a merge (e.g. standalone/Lockstep.cpp) is otherwise not built (LNK2019, 2026-10-24).
cmake --compile-no-warning-as-error build/StandaloneRelease > /dev/null 2>&1 || echo "cmake reconfigure of build/StandaloneRelease FAILED"
tools/standalone/build_locked.sh cmd //c start //low //b //wait ninja -j4 -C build/StandaloneRelease gta_reversed.exe 2>&1 | tail -1
S=C:/gta-scratch/gate_soak/$(date +%m%d-%H%M)
NOTSA_STANDALONE_NOVSYNC=1 OUT=$S tools/standalone/soak.sh build/StandaloneRelease 3 > $S.log 2>&1; rc=$?
echo "soak: $(grep -a -E '^RESULT' $S.log | tail -1) rc=$rc ($S.log)"
if [ "$1" = regress ]; then
  GR_SKIP=render_firststart NOTSA_STANDALONE_NOVSYNC=1 tools/standalone/gameplay_regress.sh > $S.regress.log 2>&1
  grc=$?; echo "regress rc=$grc $(grep -a -E 'PASS|FAIL' $S.regress.log | tail -3 | tr '\n' ' ')"
fi
# Portable gate (.notes/PORTABLE_GATE.md): the x64 NOTSA_PORTABLE configuration must still compile and link (catches Win32-only code outside guards, 32-bit pointer assumptions, inline asm).
# Incremental, x64 MSVC environment in a subshell (GTA_MSVC_ARCH=x64), same low priority/-j4/build mutex as the Release build. One-time setup: .notes/reports/PORTABLE-GATE.md
prc=0
if [ -f build/PortableWin64/build.ninja ]; then
  PL=$S.portable.log
  ( GTA_MSVC_ARCH=x64 source tools/standalone/windows/msvcenv.sh >/dev/null 2>&1 || exit 97
    command -v cl >/dev/null 2>&1 || exit 97
    TIMEOUT_S=3600 tools/standalone/build_locked.sh cmd //c start //low //b //wait cmake --build build/PortableWin64 -- -k 0 -j4 ) > $PL 2>&1; prc=$?
  # `cmd /c start /wait` does not carry ninja's exit code (same for the Release build above): judge by the log and by the exe
  [ $prc != 0 ] || ! grep -a -q -E '^FAILED:|ninja: build stopped|ninja: error' $PL || prc=95
  [ $prc != 0 ] || [ -f build/PortableWin64/bin/gta_reversed.exe ] || prc=96
  echo "portable: prc=$prc $(grep -a -c -E ' error [A-Z]*[0-9]+|: error:|FAILED:' $PL) error lines ($PL)"
  [ $prc = 0 ] || grep -a -E ' error [A-Z]*[0-9]+|: error:|FAILED:' $PL | sort -u | head -15
else echo "PORTABLE SKIP (not configured: build/PortableWin64 missing, see .notes/reports/PORTABLE-GATE.md)"; fi
if [ $rc = 0 ] && [ $trc = 0 ] && [ "${grc:-0}" = 0 ] && [ "$prc" = 0 ]; then
  o=$(cat C:/gta-scratch/gpu.lock/owner 2>/dev/null)
  case "$o" in USER-PLAY*) echo "user playing, skip copy";; *) cp build/StandaloneRelease/bin/gta_reversed.exe /c/gta-play/ && echo "C:\\gta-play refreshed";; esac
else
  f=""; [ $trc = 0 ] || f="$f tests(trc=$trc)"; [ $rc = 0 ] || f="$f soak(rc=$rc)"; [ "${grc:-0}" = 0 ] || f="$f regress(grc=$grc)"; [ "$prc" = 0 ] || f="$f portable(prc=$prc)"
  echo "GATE FAILED:$f - C:\\gta-play NOT refreshed"
fi
