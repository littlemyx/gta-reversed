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
Needs the conan x64 packages below first (see "conan status"). Then, from the main checkout:
```
export GTA_MSVC_ARCH=x64; source tools/standalone/windows/msvcenv.sh        # in a NEW shell (do not mix with an x86-loaded one)
CONAN_EXTRA_ARGS="--no-remote" bash tools/portable/win64/conan_install_win64.sh Release   # builds the 7 x64 packages from the local conan sources, no network
cmake --preset PortableWin64 -DGTASA_PYTHON="<the python from build/StandaloneRelease/CMakeCache.txt GTASA_PYTHON>" --compile-no-warning-as-error
tools/standalone/build_locked.sh cmd //c start //low //b //wait cmake --build build/PortableWin64 -- -k 0 -j4      # first (full) build, in the background
```
After that every gate run builds it incrementally. New .cpp files are globbed at configure time: add `cmake build/PortableWin64 > /dev/null` before the build if a merge adds sources (the Release step does the same).

## Conan status (x64 / windows-x64-msvc.txt + Ninja, Release, static CRT) in ~/.conan2 on this host
`conan graph info --no-remote` (no network): recipes and sources of everything are cached; the x64 BINARIES of seven packages are missing, they would be built locally from the cached sources:

| package | cached source (~/.conan2/p/*/s) | build output (x86 build dir for scale) |
|---|---|---|
| sdl/3.4.14 | 53 MB | ~110 MB |
| imgui/1.92.9b-docking | 13 MB | ~48 MB |
| tracy/cci.20220130 | 21 MB | ~27 MB |
| spdlog/1.17.0 | 1.6 MB | ~22 MB |
| libjpeg-turbo/3.2.0 | 11 MB | small |
| vorbis/1.3.7, ogg/1.3.5 | 5.5 MB, 1 MB | small |

Already cached and usable: cmake 4.2.0 (tool), nasm (tool), nlohmann_json, boost (headers), opengl/system. With `--no-remote` nothing is downloaded. Without it conan would first ask
conan-center (center2.conan.io) for prebuilt x64/msvc-195 binaries of these seven and download any it finds (unlikely for msvc 195). Estimated cost: ~250 MB disk, ~10-20 min CPU.
