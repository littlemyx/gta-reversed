# PORTABLE-GATE (Windows host): PortableWin64 compile step in orch_gate.sh

Request: .notes/PORTABLE_GATE.md section 1 (Mac orchestrator). Agent PORTABLE-GATE, 2026-10-27.

## What landed
- `tools/standalone/windows/msvcenv.sh`: `GTA_MSVC_ARCH=x64 source tools/standalone/windows/msvcenv.sh` gives the x64 MSVC environment (cl for x64, toolset 14.51 / VS 2026 BuildTools,
  conan `compiler.version=195` = the base profile, so no `CONAN_EXTRA_ARGS` is needed). Default stays x86. The vcvars dump now starts with INCLUDE/LIB/LIBPATH/EXTERNAL_INCLUDE cleared, so an
  x86 shell can load x64 in a subshell without x86 library dirs leaking in (verified: x64 cache built from an x86 shell has 0 x86 lib dirs). Separate cache file per arch; the x86 cache key is unchanged.
- `tools/standalone/windows/orch_gate.sh`: after soak/regress (so it does not disturb the soak timing) an incremental `cmake --build build/PortableWin64 -- -k 0 -j4` in an x64 subshell, via
  `build_locked.sh ... start /low` (same mutex/priority as the Release build, TIMEOUT_S=3600). Log: `C:/gta-scratch/gate_soak/<stamp>.portable.log`. Result `prc`:
  0 ok, 95 = ninja `FAILED:`/`build stopped` in the log (`start /wait` loses ninja's exit code, as for the Release build), 96 = no `bin/gta_reversed.exe`, 97 = no x64 environment, else build_locked's rc (124 timeout).
  `prc` is part of the C:\gta-play refresh condition. Not configured (`build/PortableWin64/build.ninja` missing) -> prints `PORTABLE SKIP (not configured ...)`, prc=0.
  The final else-message is now `GATE FAILED: tests(trc=..) soak(rc=..) regress(grc=..) portable(prc=..) - C:\gta-play NOT refreshed` naming only the steps that failed.
- Tested with a throw-away x64 ninja project: skip path, ok path, compile error path (prc=95, error lines printed), missing exe path (prc=96).

## One-time setup for the main checkout (orchestrator; NOT done by the agent)
Then, from the main checkout (the conan x64 packages are already in ~/.conan2, so step 2 only regenerates build/PortableWin64/generators in ~2 min):
```
export GTA_MSVC_ARCH=x64; source tools/standalone/windows/msvcenv.sh        # in a NEW shell (do not mix with an x86-loaded one)
CONAN_EXTRA_ARGS="--no-remote" bash tools/portable/win64/conan_install_win64.sh Release   # builds the 7 x64 packages from the local conan sources, no network
cmake --preset PortableWin64 -DGTASA_PYTHON="<the python from build/StandaloneRelease/CMakeCache.txt GTASA_PYTHON>" --compile-no-warning-as-error
tools/standalone/build_locked.sh cmd //c start //low //b //wait cmake --build build/PortableWin64 -- -k 0 -j4      # first (full) build, in the background
```
After that every gate run builds it incrementally. New .cpp files are globbed at configure time: add `cmake build/PortableWin64 > /dev/null` before the build if a merge adds sources (the Release step does the same).

## Result of the first run (2026-10-27, worktree, -j4 low priority)
- conan: the 7 x64 packages were built locally from cached sources with `--no-remote` (zero downloads), ~2 min. ~/.conan2 keeps them; every worktree/checkout reuses them.
- PortableWin64 full build (1177 steps): ~2.5 min (cl runs fast here), then ~2.5 min for the rebuild after the fixes; link OK, `build/PortableWin64/bin/gta_reversed.exe` 19.6 MB.
- First build: 3 errors (all fixed, one commit each):
  | file | error | origin | fix |
  |---|---|---|---|
  | game_sa/Entity/Vehicle/Automobile.cpp:2702 | `static_assert(offsetof(CAutomobile, m_swingingChassis) == 0x70C)` (x86 layout) | interior_c 74099207c ASM-S16 | `VALIDATE_OFFSET` (a no-op on portable) |
  | game_sa/Pools.cpp:524 | task allocator pool slot too small: CTaskAllocatorPlayerCommandAttack is 56 on Win64 (MSVC ABI keeps the base tail padding; LP64 is 48) | portable/main f1ed8739 STAB2 | slot 56 under `_WIN32` in Pools/TaskAllocatorPool.h |
  | game_sa/Entity/Placeable.cpp:11 | `<execinfo.h>` missing on Windows | portable/main f1ed8739 STAB2 | `_WIN32`: empty `backtrace` stubs |
  x86 StandaloneRelease: the three touched TUs (+ pch) recompile clean.
- Gate step verified in the worktree against the real build (same block as orch_gate.sh): no-op incremental prc=0; injected `#error` prc=95 with the error lines printed; restored prc=0.
