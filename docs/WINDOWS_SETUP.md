# Windows PC setup (reference work: x86 MSVC build, exe-oracle tests, game soaks, debugging)

Since 2026-10-16 the *reference* work of the standalone port runs on a native x86-64 Windows PC with a GPU (Claude Code runs there, in Git Bash). The Mac keeps
the portability streams (clang-cl / arm64 syntax checks, codemods, porting). Everything below was prepared on the Mac and **not yet executed on Windows** - the
"First session checklist" at the end is how you find out what still needs a fix. Where this file and `docs/STANDALONE.md` disagree about Wine details, this
file wins on Windows (no Wine, no prefixes, no `taskpolicy`).

## 0. Facts to keep (the toolchain is part of the fidelity contract)
| What | Value |
|---|---|
| MSVC toolset | **14.51.36231** (VS 2026 / v145, conan `compiler.version=195`; x87 codegen of the game objects depends on it - do not "just use the newest") |
| Windows SDK | **10.0.26100.0** |
| CMake / Ninja / conan | CMake >= 4.2 (Mac uses 4.4.3), any recent Ninja, conan **2.x** (Mac: 2.33.0) |
| Python | 3.12+ x64. `unicorn` and `capstone` only for the dev tools (the `StandaloneDetached` data-image extractor replays the exe's static initialisers); the run exe builds without them |
| Target | **Win32 (x86)** only. CI uses `windows-2025-vs2026`, `arch=x86` |
| Original exe (OPTIONAL, dev only) | `gta_sa_compact.exe` (1.0 US compact, 5 189 632 bytes, sha256 `72ae59e44c761389e354a50dc6215e964fe771121e2f4b1877273a493ceecc9b`): oracle tests and the `StandaloneDetached` config only; `StandaloneRelease` / `StandaloneRW` configure, build and run without it (clean-clone proof: `.notes/reports/D-G.md`) |

## 1. Prerequisites (install once)
1. **Visual Studio 2026 (18.x) Community, or Build Tools 2026**, workload *Desktop development with C++* **plus** the individual components
   *MSVC Build Tools for x64/x86 (v14.51 ...)* (the exact toolset **14.51**; if the default workload installs a newer 14.5x, tick 14.51 explicitly - both can coexist)
   and *Windows 11 SDK (10.0.26100.0)*. Also tick *C++ CMake tools for Windows* only if you want VS's cmake/ninja instead of installing them separately.
2. **Git for Windows** (provides Git Bash; Claude Code on Windows uses it as its shell). Then, once:
   `git config --global core.autocrlf false` and `git config --global core.longpaths true` (shell scripts must stay LF; `.gitattributes` already forces `eol=lf`).
3. **CMake >= 4.2**, **Ninja** (`winget install Kitware.CMake Ninja-build.Ninja`, or take both from VS; whichever, they must be on PATH in the build shell).
4. **Python 3.12+ (64-bit)** from python.org / `winget install Python.Python.3.12`; turn off the Microsoft Store "python" app-execution alias (Settings > Apps > Advanced app settings),
   then `python -m pip install unicorn capstone conan` (conan can also come from `pipx install conan`).
5. Optional but wanted for debugging: **WinDbg** (Microsoft Store "WinDbg" or *Debugging Tools for Windows* from the SDK installer). Visual Studio's own debugger is already installed.
6. A GPU with a normal D3D9-capable driver (any current GeForce/Radeon/Intel driver). Disk: ~40 GB free (Release 6.4 GB + Debug 3.9 GB + StandaloneRW 6.9 GB on the Mac, plus the conan cache).
7. Short directory names, everything on **one NTFS volume (C:)**: repo `C:\src\gta-reversed`, game data `C:\gta`, soak output `C:\gta-soak-out`. The game copies its cwd into a 128-char
   buffer and the soak builds its data dir from hard links, which cannot cross volumes.

## 2. Clone (private fork) with the librw submodule
```bash
mkdir -p /c/src && cd /c/src
# first clone prompts Git Credential Manager (ships with Git for Windows) - sign in as an account with access to the private repo littlemyx/gta-reversed; `gh auth login` also works
git clone --recurse-submodules -b reverse/interior_c https://github.com/littlemyx/gta-reversed.git
cd gta-reversed
git submodule status      # vendor/librw must be at 6f80bf3... (littlemyx/librw, branch notsa); `git submodule update --init vendor/librw` if it shows a '-'
```
On the Mac the fork is the remote called `fork`; on the clone it is `origin`, so the Windows push is `git push origin reverse/interior_c`. Pull Mac work with `git pull --ff-only`.
`.notes/` is a local scratch dir on the Mac (only `git add -f`-ed files are in the clone: `.notes/symbols.txt`, `PORT_BRIEF.md`, `PHASE2_STATUS.md`, `reports/`, the plans).
`.notes/decomp`, `.notes/asm`, `UNHOOKED*`, `DETACH_GLOBALS.tsv`, ... are NOT in git; porting and `census.py` need them - copy by hand if you ever want them on the PC.

## 3. Files that are never committed (check `.gitignore`: `/gta_sa_compact.exe`, `original_data.*`, `data_pointers.bin`, `standalone*.log`; the run exe itself needs none of the first three since slice F3)
* **Optional: `gta_sa_compact.exe` -> repo root** (`C:\src\gta-reversed\gta_sa_compact.exe`). Copy it from the Mac (`~/github/recomp/gta-reversed/gta_sa_compact.exe`); verify:
  `sha256sum gta_sa_compact.exe` must print the hash from section 0. It is only an *input of dev tools* (oracle tests, the `StandaloneDetached` census/verify config); building and running the exe does not need it.
* **Game data -> `C:\gta`**: a copy of your own GTA San Andreas install folder (Steam: `...\steamapps\common\Grand Theft Auto San Andreas`; copy it, the folder only has to contain `models\gta3.img`,
  `data\`, `anim\`, `audio\`, `txd\`, `text\`, `movies\` ...; its `gta_sa.exe` is never used). `soak.sh` looks for `GAME_SRC` -> `C:\gta` -> the default Steam folder, in this order.
* The game still reads/writes your real `Documents\GTA San Andreas User Files\` (`gta_sa.set` incl. the video mode, saves) - there is no private prefix on Windows. Back that folder up once.

## 4. The build shell (do this every time)
Open the MSVC x86 environment with Git Bash: double-click / run **`tools\standalone\windows\gtash.cmd`** (it finds VS with `vswhere`, runs
`vcvarsall.bat x86 -vcvars_ver=14.51`, `cd`s into the repo and starts Git Bash). **Start Claude Code from that shell** so every child process (cmake, ninja, cl, the scripts) inherits the environment.
Check once:
```bash
cl 2>&1 | head -1        # ...Version 19.51.xxxxx... for x86 (VS 18.11 Build Tools: 19.51.36260, inside the 14.51.36231 toolset folder; check `echo $VCToolsVersion` = 14.51.36231)
cmake --version; ninja --version; conan --version
python -c "import unicorn, capstone; print('ok')"      # dev tools / StandaloneDetached only; the run build needs just a python 3 for -DGTASA_PYTHON (oracle test targets)
```
`GTA_MSVC_VER`, `GTA_GIT_BASH`, `GTA_VSINSTALL` override the launcher's defaults (per-user Git install, other VS edition).

Agents / shells **not started from `gtash.cmd`**: `source tools/standalone/windows/msvcenv.sh` imports the same `vcvarsall.bat x86 -vcvars_ver=14.51` environment into the current Git Bash
(honours `GTA_VSINSTALL` / `GTA_MSVC_VER`, idempotent, dump cached in `C:/gta-scratch/msvcenv-<key>.sh`, `GTA_MSVCENV_FORCE=1` rebuilds it) and adds python / cmake / ninja / conan to PATH when winget put them
somewhere the inherited PATH does not list yet. Source it in every shell that runs cmake/ninja/cl (e.g. at the start of each Bash command: `source tools/standalone/windows/msvcenv.sh && ...`).
The repo may live anywhere (the scripts derive the repo root from their own location); `C:\src\gta-reversed` in this document is only the suggested path.

## 5. Dependencies (conan; Debug and Release are separate installs)
```bash
conan profile detect --force
conan install . --build=missing --profile conan/profiles/windows-msvc-ninja.txt -s build_type=Debug
conan install . --build=missing --profile conan/profiles/windows-msvc-ninja.txt -s build_type=Release
```
`windows-msvc-ninja.txt` = the committed `windows-msvc.txt` (x86, msvc 195, static runtime) + the Ninja generator. Do not pass `-c tools.cmake.cmaketoolchain:user_presets=`: `conanfile.py` writes
`ConanPresets.json`, which `CMakePresets.json` includes (without it every `cmake --preset` fails). Outputs: `build/Debug/generators`, `build/Release/generators`. Expect 10-30 min per install the first time
(sdl3, tracy, imgui, boost headers ... are built from source if there is no binary for msvc 195/x86).
The first install also generates `source/libs/imgui/*` (not committed). Under Wine the profiles add `sdl/*:libusb=False` and compiler paths; **not needed here**.

## 6. Configure and build
Always from the build shell (section 4). Use `tools/standalone/build_locked.sh <command...>` for every build: it takes the shared mutex `C:/gta-scratch/build.lock` (the same `mkdir`/`rmdir` lock the
Mac uses, so parallel agents never run two builds at once), applies a hard timeout (`TIMEOUT_S`, default 2400 s, `timeout` from Git Bash; a timeout also kills leftover `cl.exe`/`link.exe`) and releases the lock on exit.
No background priority on Windows (the PC is not shared with a game being played); lower it in Task Manager if you need to.

| Preset / build | Configure | Build |
|---|---|---|
| **StandaloneRelease** (`build/StandaloneRelease`, optimised, the soak/perf exe) | `cmake --preset StandaloneRelease -DGTASA_PYTHON=$(command -v python) --compile-no-warning-as-error` | `tools/standalone/build_locked.sh ninja -C build/StandaloneRelease gta_reversed.exe` |
| **StandaloneRW** (`build/StandaloneRW`, Debug, main development build + all `*_test` targets) | `cmake --preset StandaloneRW -DGTASA_PYTHON=$(command -v python) --compile-no-warning-as-error` | `tools/standalone/build_locked.sh ninja -C build/StandaloneRW gta_reversed.exe` (tests: `ninja -C build/StandaloneRW <name>_test` or `run_all_tests.sh`) |
| **StandaloneDetached** (`build/StandaloneDetached`, dev only: Release + `GTASA_VERIFY_GLOBALS` + `GTASA_IMAGE_DIAG`; the ONLY preset that needs the original exe: `GTASA_ORACLE_EXE` = `<repo>/gta_sa_compact.exe`, configure fails without it) | `cmake --preset StandaloneDetached -DGTASA_PYTHON=<python with unicorn+capstone> --compile-no-warning-as-error` | `tools/standalone/build_locked.sh ninja -C build/StandaloneDetached gta_reversed.exe` |
| **Debug (DLL / ASI, the classic gta-reversed build)** | `cmake --preset conan-debug` | `cmake --build --preset conan-debug` (or `conan build . --profile conan/profiles/windows-msvc-ninja.txt -s build_type=Debug`); needs a real 1.0 US `gta_sa.exe` + an ASI loader to run, not required for the standalone work |

* **`--compile-no-warning-as-error`** (configure option): the targets carry `COMPILE_WARNING_AS_ERROR`, the Wine flow neutralises it with `/WX-` in `CMAKE_CXX_FLAGS`. The native MSVC 19.51 reports warnings that flow never showed as errors (first seen: C4018 in `WinPs.cpp:473`, C4715 in `WinMain.cpp:240`), so without this flag the build stops at them. The warnings are real; they are only not fatal.
* `GTASA_PYTHON` must be a *native* python path (`$(command -v python)` in Git Bash gives `/c/...`, which CMake accepts). unicorn+capstone are needed in it only for `StandaloneDetached` (if you get "No module named unicorn" at the extract step, pass `-DGTASA_PYTHON=C:/Python312/python.exe`); `StandaloneRelease` / `StandaloneRW` do not run it for the exe.
* The presets set `GTASA_NO_EDIT_AND_CONTINUE=ON` (Wine passes it through the conan profile; without it MSVC would use `/ZI`, which conflicts with `/O2` and produces no linker map). Result: `/Z7`, `/DEBUG`, **`gta_reversed.pdb` and `gta_reversed.map`** next to the exe.
* New `.cpp` files need a re-glob: re-run the configure command. The extractor (`extract_exe_data.py`) runs as a build step only for the dev preset `StandaloneDetached` (`GTASA_IMAGE_DIAG`) and writes `original_data.bin/.json` and `data_pointers.bin` into `<build>/bin`.
* One configure+full build of StandaloneRelease takes ~10-20 min on a normal desktop (the Mac needed ~30 min through Wine); measured on the dev PC (16 cores, -j4 BelowNormal, PCH): 1.5 min Release `gta_reversed.exe`, 3.3 min StandaloneRW + all test targets.
* Hung build: if `build.lock` is older than 20 min and no `cl.exe`/`link.exe` burns CPU, `taskkill //F //IM cl.exe //IM link.exe` and `rmdir C:/gta-scratch/build.lock`.

### Compile cache (opt-in) and fresh worktrees
Agents build in their own git worktrees. `tools/standalone/windows/new_worktree.sh <id> [--cache] [--preset P]` creates `C:/gta-wt/<id>` (branch `<id>`, librw submodule, the untracked conan generators /
`ConanPresets.json` / `source/libs/imgui` copied from the main checkout) and configures the preset in one go (`git worktree remove --force C:/gta-wt/<id> && git branch -D <id>` when done).
The **compile cache** is [sccache](https://github.com/mozilla/sccache), **off by default** (`-DGTASA_COMPILE_CACHE=OFF`: the default configure/build is unchanged):
```bash
tools/standalone/windows/get_sccache.sh                      # once: user-local C:/gta-tools/sccache/sccache.exe, official GitHub release, sha256 verified, no admin
cmake --preset StandaloneRelease -DGTASA_PYTHON=... --compile-no-warning-as-error -DGTASA_COMPILE_CACHE=ON      # or: new_worktree.sh <id> --cache
```
* One disk cache for all worktrees: `C:/gta-tools/sccache-cache` (`GTA_TOOLS` moves the root, a set `SCCACHE_DIR` wins), cap 8 GB. Every source tree gets its own sccache server (port derived from the path,
  `SCCACHE_BASEDIRS` = the tree) so that the absolute paths in the preprocessed text do not make every worktree miss; servers exit after 10 idle minutes; `sccache --show-stats` (set `SCCACHE_SERVER_PORT`
  to the port printed by the configure step) shows hits. A cached object carries the paths of the worktree that compiled it (/Z7 debug info, `__FILE__` in assert strings).
* **PCH trade-off**: sccache cannot cache a compile that uses a precompiled header, so with the cache on `target_precompile_headers` becomes a forced include (`/FI source/StdInc.cpp`) of the same header. Cache HITS skip the
  compiler (a warm worktree is mostly link time), but every MISS parses the heavy headers again: 

  | StandaloneRelease `gta_reversed.exe`, -j4 BelowNormal, 16-core PC | wall |
  |---|---|
  | default PCH build, fresh worktree (1184 steps) | **1.5 min** (87-89 s; a full rebuild after a header edit is the same) |
  | default PCH build of StandaloneRW + all 45 `*_test` targets, fresh worktree (1469 steps) | **3.3 min** |
  | cache ON, empty cache (every TU a miss, 1119 TUs) | **18-36 min** (2 s/TU wall; depends on the load of the PC) |
  | cache ON, other worktree, all hits (measured on 63 TUs: 18 s vs 124 s cold; 0.29 s/TU) | ~5 min for 1119 TUs + link (extrapolated) |

  On this PC the PCH build is **faster than a fully warm cached build**, so the cache does not pay; it is kept only as an opt-in experiment (cheap hits do not exist for the 600-header TUs: sccache has to hash every include).

  Recommendation: keep the default PCH build.
* The forced-include build needs ODR-clean headers (the PCH object used to hold the single copy of non-inline header definitions): `inline` was added to the `notsa::PoolOf<>` specialisations and the deprecated
  `Compress*/Uncompress*Vector` helpers, and `Zone.h`/`Pad.h` include `<rw/...>` with angle brackets (a quoted include is searched in the directory of the compiled .cpp first and found `game_sa/RenderWare/rw/` instead of
  `fakerw/rw/` for the TUs in `game_sa/RenderWare/`). Object code of the default build is unchanged by these edits (`tools/standalone/obj_code_sig.py`: all 1119 objects identical).
  **Not equivalent to the default build**: without the PCH object, header-defined inline functions / globals are emitted into every object (4x more code sections) and the dynamic initialisers of header-defined globals
  come from every TU instead of the PCH object (init order differs); ~1 % of the code sections of the default objects have no byte-identical counterpart (e.g. a `Quaternion` function with a different frame layout).
  So never use a cache-built exe or test as evidence for fidelity / oracle results; the default build stays the reference.
* `/ZI` (Edit&Continue) is not cacheable: the option requires `GTASA_NO_EDIT_AND_CONTINUE=ON` (all Standalone presets).

## 7. Running the exe natively (no Wine)
Manual run: the exe expects only the game data as the cwd (no image files since slice F3).
```bash
# /c/gta-run = a data dir; soak.sh builds one by itself (hard links of the files + junctions of the folders of C:\gta), or make it by hand (copy the folder, minus gta_sa.exe)
cd /c/gta-run
cp /c/src/gta-reversed/build/StandaloneRelease/bin/gta_reversed.exe .
NOTSA_STANDALONE_MUTE=1 NOTSA_STANDALONE_SKIP_VIDEOS=1 ./gta_reversed.exe      # log: standalone.log + logs/log.log
```
A crash/abort raises exception 0xE0AB0001 and prints a symbolised stack into `standalone.log`. Always run **silent** (`NOTSA_STANDALONE_MUTE=1`; verify `[mute]` lines in `standalone.log` on the first run:
the mute patches DirectSound buffers and has only been validated under Wine's DirectSound). Real DirectShow exists on Windows, so videos *could* play; scripts keep skipping them.

### Environment variables (all optional, run build only; details in `docs/STANDALONE.md`)
| Variable | Effect |
|---|---|
| `NOTSA_STANDALONE_MUTE=1` | silent run (DirectSound buffers keep `DSBVOLUME_MIN`); default in `soak.sh`/`census.py`, `=0` opts out |
| `NOTSA_STANDALONE_SKIP_VIDEOS=1` | skip intro videos |
| `NOTSA_STANDALONE_INPUT=<file\|script>` | key injector (`wait:<ms>;key:return;down:w;up:w`, `until:control[:maxms]`, `until:invehicle`, `label:`/`skipif:`, `mark:<name>`) |
| `NOTSA_STANDALONE_SCREENSHOT=<k>`, `_SCREENSHOT_MAX`, `_SCREENSHOT_MS`, `_SCREENSHOT_ARM`, `_SCREENSHOT_BURST` | back-buffer BMP every k-th frame / every N ms / bursts after script marks |
| `NOTSA_STANDALONE_VIDEOMODE=<index>` | force a video-mode index (only if `gta_sa.set` has no valid mode - on Windows the real settings file wins) |
| `NOTSA_STANDALONE_MEMLOG=1` | log CRT heap / committed memory every 100 frames (`soak.sh` sets it; this is where the fps-per-phase numbers come from) |
| `NOTSA_STANDALONE_ALLOCTRACE=<min bytes>` | caller of every 200th allocation >= min |
| `NOTSA_STANDALONE_NOPRESENT=1` | skip `Present` |
| `NOTSA_STANDALONE_NOVSYNC=1` | Present with sync interval 0 even if the game asks for vsync (menu frame limiter / loading screens): boot to the menu in seconds instead of 30-75 s on this PC; game logic and the software frame limiter unchanged; never for fps measurements. Opt-in, passed through by `soak.sh` |
| `NOTSA_STANDALONE_NO_ABORT_TRACE=1` | do not turn `abort()` into a traced exception |
| `NOTSA_STANDALONE_SAMPLER=1` | in-process profiler (SuspendThread of the main thread; harmless on native x86, it only corrupted x87 state under Wine/Rosetta) |
| `NOTSA_AUDIO_TAP=<file>` (`_RAW=1`) | silent DirectSound diagnostics (use a normal `C:\...` path) |
| `NOTSA_VERIFY_GLOBALS=<dump>` (`_EXIT=1`, `_AFTER=<s>`) | detached-globals boot dump (StandaloneDetached) -> `tools/standalone/verify_globals.py` |
| `NOTSA_IMAGE_CENSUS=1` (`_FILE`, `_REARM_MS`), `NOTSA_POISON_CONVERTED=<file>` | image census / poisoning of converted globals (`census.py`) |
| `RW_EXE_ORACLE=<path>` | the original exe for the oracle tests; `run_all_tests.sh` sets it (native Windows path form) only when `GTA_EXE` / `<repo>/gta_sa_compact.exe` exists, otherwise the 23 exe-oracle tests exit 77 and are listed as `SKIP(no_exe)` (not pass, not fail) |
| Script env | `soak.sh`: `GAME_SRC OUT ROUTE SHOT_EVERY SHOT_MAX SAMPLER GTASA_PYTHON`; `run_all_tests.sh`: `SCRATCH SKIP_BUILD ATTEMPTS TIMEOUT TIMEOUT_<test> ASSETS GAME_DIR GTASA_PYTHON NINJA GTA_EXE`; `build_locked.sh`: `SCRATCH TIMEOUT_S LOCK_WARN_MIN`; `gpu_locked.sh`: `SCRATCH GPU_OWNER TIMEOUT_S GPU_LOCK_GRACE_S GPU_LEGACY_STALE_MIN`; `soak.sh`: `SOAK_GPU_LOCK=0` |

## 8. Tests and soaks
```bash
GAME_DIR=/c/gta ATTEMPTS=2 tools/standalone/run_all_tests.sh build/StandaloneRW          # configures build/StandaloneRW if missing, builds every *_test, runs them natively, prints the table
tools/standalone/run_all_tests.sh build/StandaloneRW 'oracle'                              # regex filter on the target names
tools/standalone/soak.sh build/StandaloneRelease 7                                          # smoke = the shortest route (fixed part ~6 min + 60 s idle); report: C:/gta-soak-out/<timestamp>/report.txt
tools/standalone/soak.sh build/StandaloneRelease 10                                         # regression soak
python tools/standalone/census.py run --build build/StandaloneDetached --minutes 10         # needs .notes/DETACH_GLOBALS.tsv (copy from the Mac)
```
* `run_all_tests.sh` and `soak.sh` detect Git Bash (`uname -s` = MINGW*/MSYS*) via `tools/standalone/hostenv.sh` and run the exes directly. Test exes live in `<build>/source/<name>.exe`; the oracle tests map `gta_sa_compact.exe` from the repo root when it is there (otherwise `SKIP(no_exe)`).
* The script ends with `RESULT: OK|FAIL` (soak) / a per-test table (tests, non-zero exit on any failure). Known non-green rows measured under Wine (2026-10-10): `review_oracle_test` (PC24/PC53 strict mismatches), `rw_skin_pipeline_test`
  (page fault), `rw_skin_hw_oracle_test` and `rw_skin_vs_test` (NORESULT parser quirk; vs_test needs `TIMEOUT_rw_skin_vs_test=1500`). On a real GPU/driver these may change in either direction - **record, do not "fix" the numbers away**.
* Per-phase fps and memory are in `report.txt`; screenshots (BMP converted to PNG by PowerShell/System.Drawing) in `<out>/shots`.

### GPU mutex (game runs and device tests are exclusive)
The game is single-instance and the GPU is shared by several agents and by the user's play session, so every game run / device test takes `C:/gta-scratch/gpu.lock`:
`tools/standalone/gpu_locked.sh [-o OWNER] [-t SECONDS] <command...>` (the sibling of `build_locked.sh`; `soak.sh` and `windows/fps_run.sh` re-exec themselves under it, `SOAK_GPU_LOCK=0` opts out).
It records the holder's PID next to the `owner` text; a waiter drops a lock whose holder is gone (killed hard, e.g. by an agent's tool timeout) and a lock directory with neither PID nor owner
older than 60 s; a hand-written lock (owner file only, from the old `mkdir` snippet) is dropped only after 30 min with no `gta_reversed`/`gta_sa`/`*_test` process running; an owner starting with
`USER-PLAY` (`play.cmd`: the user is playing) is **never** removed automatically - agents wait, and must not start builds or heavy work meanwhile. `gpu_locked.sh --status` prints the holder and
whether it looks alive (exit 0 free / 1 held / 3 USER-PLAY / 4 stale), `--reap` drops a stale lock. Nested calls of the same holder (`GPU_LOCK_HELD`) just run.

## 9. Debugging
* **Binaries**: StandaloneRelease/Detached/RW all link with `/DEBUG` (+`/Z7`): `build/<dir>/bin/gta_reversed.pdb` and `gta_reversed.map` (link `/MAP`; `census.py` and the abort tracer symbolise from it). Release objects are `/O2` but with `/OPT:NOREF /OPT:NOICF`, so function addresses stay meaningful.
* **Visual Studio**: `devenv /debugexe C:\gta-run\gta_reversed.exe` (from the build shell), then *Debug > Properties*: working directory `C:\gta-run`, environment `NOTSA_STANDALONE_MUTE=1`.
  Symbols are found through the PDB path embedded in the exe (`build\<dir>\bin`); if you copied the exe elsewhere add that folder to *Tools > Options > Debugging > Symbols*.
* **WinDbg**: `cd /c/gta-run && windbg -y C:\src\gta-reversed\build\StandaloneRelease\bin -srcpath C:\src\gta-reversed gta_reversed.exe` (the debuggee inherits the cwd of the launching shell; the game needs the data dir as cwd).
  Useful: `sxe -c "k;g" 0xe0ab0001` (the port's abort exception), `bp gta_reversed!<symbol>`, `x gta_reversed!CAutomobile::*`, `.symfix; .reload /f`.
* **Image layout**: since slice F3 the exe is an ordinary relocatable image (ASLR on, no data image, no placeholder): a global such as the old `0xB6F5F0` is a named C++ variable (`x gta_reversed!<name>`); nothing is mapped at the original addresses. Only the dev preset
  `StandaloneDetached` (`GTASA_IMAGE_DIAG=ON`) still maps the original image there (`/BASE:0x400000 /FIXED`, placeholder, code range `PAGE_NOACCESS`: an access violation there is a stray original address, the log names it).
  Expect first-chance exceptions only on real bugs - if the debugger stops at start-up, check which exception before ignoring it.
* **FPU**: the main thread runs at PC=24 after `CreateDevice` exactly like the original; do not read `st0..st7` after stepping and expect the exe's values - use the logged `FPU[...]` lines. Native x86 has none of the Wine/Rosetta x87 corruption caveats; suspending the game thread (VS break, WinDbg `~*k`, the sampler) is fine.
* Wine-era workflow remains valid for **comparison**: the same route on the Mac (`soak.sh`) vs Windows is the way to separate port bugs from Wine/host artefacts.

## 10. First session checklist (for the agent; write the result into `.notes/reports/WINSETUP_FIRST.md`)
1. `git status` clean? `git submodule status` (librw 6f80bf3)? `sha256sum gta_sa_compact.exe`? `cl` = 19.51.x for x86 with `$VCToolsVersion` = 14.51.36231 (the folder name; the banner build number is the servicing level of VS, 36260 on 18.11), SDK 10.0.26100.0 (`echo $WindowsSDKVersion`)? If any differs, stop and report - do not proceed with a different toolchain.
2. `conan install` Debug + Release (section 5). Note any recipe that had to be built or failed.
3. `cmake --preset StandaloneRelease ...` + build `gta_reversed.exe` (section 6). Record the wall time; confirm `gta_reversed.pdb`/`.map` exist (no `original_data.bin` is produced by this preset any more).
4. `cmake --preset StandaloneRW ...` + `tools/standalone/run_all_tests.sh build/StandaloneRW` (with `GAME_DIR=/c/gta`). Expected: everything green except the known rows in section 8. Save the table; **any NEW failure is the headline** (host difference or compiler difference vs the Wine-hosted MSVC 14.51 binaries).
5. One soak: `tools/standalone/soak.sh build/StandaloneRelease 7` (shortest route) then `... 10` -> `RESULT: OK`, 0 errors, all route markers reached, no `QUIT NOT EFFECTIVE`. If the start fails (`exe never started`, or `standalone.log`/`logs/log.log` end with `Failed: RwEngineSetVideoMode(-1)`), read `<out>/run.out` and the logs; likely first-run suspects: **more than one monitor/adapter** (fixed 2026-10-16: the original shows a device/mode dialog the standalone exe does not have; it now uses adapter 0 and the default mode search - see the regression note in `docs/STANDALONE.md`; the log shows `N sub systems: no video mode dialog`). A missing `gta_sa.set` is NOT a cause (the default mode search runs; `NOTSA_STANDALONE_VIDEOMODE=<index>` overrides it),  address-space conflict at 0x400000, a DirectSound/DirectInput device difference, antivirus on the unsigned exe.
6. Confirm the run is silent (listen; `grep '\[mute\]' standalone.log`).
7. **Record frame times**: from `report.txt` "fps per phase" (menu / loading / walking / driving / idle) for StandaloneRelease; put them next to the Mac/Wine numbers (menu ~530 fps, gameplay ~29 fps with the limiter) in `.notes/reports/WINSETUP_FIRST.md`. Note whether the in-game frame limiter is on.
8. `git add -f`/commit only the report; push with `git push origin reverse/interior_c`. Report to the orchestrator in <= 15 lines (commits, test table summary, soak result, open items, report path).

Rules that changed for the Windows host are in `.notes/PORT_BRIEF.md` ("Host: Windows"); everything else in the brief (fidelity rules, report format, silent runs) applies unchanged.
