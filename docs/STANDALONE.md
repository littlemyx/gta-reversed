# Standalone GTA SA executable (branch `reverse/interior_c`)

## What it is
`gta_reversed.exe` built in "standalone" mode is a **self-contained Win32 executable made from this repository's reversed C++ code**.
It contains no code of the original game executable. Rendering goes through [librw](https://github.com/aap/librw) (fork:
`vendor/librw`) behind a RenderWare-compatible shim (`source/fakerw`, `source/standalone/rw`), not through the original RenderWare.

The original `gta_sa.exe` (v1.0 US, "compact") is used only as an **input**:
* once at build time, to extract the initial `.rdata/.data` image (`original_data.bin/json`, `data_pointers.bin`), which the new exe maps at
  the original virtual addresses so that reversed code reading `0xB6F5F0`-style globals keeps working;
* by the **oracle tests**, which map the original code and compare it with our port.

Never commit the original exe (it is git-ignored). Legal note: private use only; you need your own legitimate copy of the game and its data.

Design details (committed snapshots, `git add -f`-ed because `.notes/` is a local scratch dir elsewhere): `.notes/P2A_DESIGN.md` (data image, hooks-as-fixups, traps), `.notes/P2B_SHIM_PLAN.md` (RW shim), `.notes/PHASE2_STATUS.md` (history).

## Prerequisites
* **MSVC x86 toolchain.** Windows: VS 2022/2026 with the x86 C++ tools. macOS/Linux: [msvc-wine](https://github.com/mstorsjo/msvc-wine) plus Wine
  (tested: Wine Staging 11.x on macOS, `WINEPREFIX=~/.wine-msvc`, MSVC 14.51, SDK 10.0.26100, `cl` at `~/tools/msvc/bin/x86/cl`).
  Wine toolchain files (committed, portable): `tools/standalone/msvc-wine/{toolchain-msvc-wine.cmake,no-wx.cmake,conanprofile-wine-debug.txt,conanprofile-wine-release.txt}`;
  point them at your msvc-wine install with `export MSVC_WINE_ROOT=<dir>` (default `~/tools/msvc`; `cl` must be `$MSVC_WINE_ROOT/bin/x86/cl`).
* CMake >= 4.2, Ninja, conan 2 (e.g. `pipx install conan`), git (with submodules: `git submodule update --init vendor/librw`).
* Python 3 with `unicorn` and `capstone` (`pip install unicorn capstone`); passed as `-DGTASA_PYTHON=<python>`. The extractor replays the
  exe's 1667 static initialisers under Unicorn and classifies code pointers.
* The game's data directory (a legitimate install).
* `gta_sa_compact.exe` (v1.0 US compact) in the repository root.

## Build
Dependencies (Debug and Release are separate installs). On Windows use the repo's `conan/profiles/windows-msvc.txt` as is. Under msvc-wine
use the committed profiles (they include it and add the tweaks below; Wine on PATH, `WINEPREFIX` set up for msvc-wine):
```
export MSVC_WINE_ROOT=$HOME/tools/msvc     # your msvc-wine install
conan install . --build=missing --profile tools/standalone/msvc-wine/conanprofile-wine-debug.txt -s build_type=Debug
conan install . --build=missing --profile tools/standalone/msvc-wine/conanprofile-wine-release.txt -s build_type=Release
```
(Do not pass `-c tools.cmake.cmaketoolchain:user_presets=`: `conanfile.py` writes `ConanPresets.json`, which `CMakePresets.json` includes. The first install also generates `source/libs/imgui/*` - ImGui bindings copied from the conan package by `conanfile.py`; they are not committed.)
Known msvc-wine profile tweaks (already in those profiles; see `tools/standalone/msvc-wine/conanprofile-wine-debug.txt`): `sdl/*:libusb=False` (needs MSBuild), toolchain +
`compiler_executables` pointing at msvc-wine `cl`, `/WX- /wd4005`, extra variables `CMAKE_NINJA_FORCE_RESPONSE_FILE=ON`,
`CMAKE_PROJECT_INCLUDE=tools/standalone/msvc-wine/no-wx.cmake`. For **Release** additionally: `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`,
`libjpeg-turbo/*` extra variables `HAVE_BUILTIN_CTZL=0`, and no response files for nasm. Outputs: `build/Debug/generators`, `build/Release/generators`.

Common flags used below (bash; shown with the msvc-wine paths - adapt `MSVC`/`PY`; run from the repo root, after `export MSVC_WINE_ROOT=...` as above):
```
MSVC=$MSVC_WINE_ROOT                 # msvc-wine install (Windows: omit the compiler/flags lines, cl is found by CMake)
PY=<python with unicorn+capstone>    # e.g. a venv: python3 -m venv ~/gta-venv && ~/gta-venv/bin/pip install unicorn capstone
COMMON=(-G Ninja -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" -DGTASA_STANDALONE=ON -DGTASA_USE_SDL3=ON -DGTASA_UNITY_BUILD=OFF -DGTASA_WITH_LTO=OFF
  -DGTASA_WITH_OPENAL=OFF -DGTASA_WITH_CLEO_COMMANDS=OFF -DGTASA_WITH_SCRIPT_COMMAND_HOOKS=OFF
  -DGTASA_ORIGINAL_EXE="$PWD/gta_sa_compact.exe" -DGTASA_PYTHON="$PY"
  -DCMAKE_C_COMPILER="$MSVC/bin/x86/cl" -DCMAKE_CXX_COMPILER="$MSVC/bin/x86/cl"
  "-DCMAKE_C_FLAGS=/DWIN32 /D_WINDOWS /Zm1000" "-DCMAKE_CXX_FLAGS=/WX- /wd4005 /DWIN32 /D_WINDOWS /Zm1000 /GX")
```
(Use the array exactly as `"${COMMON[@]}"` below - the flags contain spaces.)
1. **`build/Standalone`** - original RenderWare (opt-out of librw), Debug:
   `cmake -S . -B build/Standalone -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=$PWD/build/Debug/generators/conan_toolchain.cmake "${COMMON[@]}" -DGTASA_RW_LIBRW=OFF -DGTASA_RW_LIBRW_OPT_OUT=ON`
   (RW functions are reached by exe address and trap; kept for comparison, it stops at the first RW call.)
2. **`build/StandaloneRW`** - librw, Debug (the main development build):
   `cmake -S . -B build/StandaloneRW -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=$PWD/build/Debug/generators/conan_toolchain.cmake "${COMMON[@]}" -DGTASA_RW_LIBRW=ON -DGTASA_RW_LIBRW_OPT_OUT=OFF`
3. **`StandaloneRelease`** preset - librw, `/O2 /Ob2 /DNDEBUG -MT /Z7 /arch:IA32 /fp:precise`, ~32 MB exe (Debug 61 MB), bit-exact with Debug:
   `cmake --preset StandaloneRelease -DGTASA_PYTHON="$PY"` (+ under msvc-wine `-DCMAKE_C_COMPILER=... -DCMAKE_CXX_COMPILER=...` and the two `CMAKE_*_FLAGS` entries above, as separate quoted arguments)

Build: `ninja -C build/StandaloneRW gta_reversed.exe`. The extractor runs as a build step and writes `original_data.bin`, `original_data.json`
and `data_pointers.bin` next to the exe in `<build>/bin` (they are tied to the 1.0 US compact exe). New `.cpp` files need a CMake re-glob (re-run cmake).
Under Wine, one build at a time (cl via Wine is memory hungry); `ninja` may be interrupted by Wine (exit 130) - just re-run.

## Run
Copy `gta_reversed.exe`, `original_data.bin`, `original_data.json`, `data_pointers.bin` into the **game data directory** and run it from there
(`wine gta_reversed.exe` on macOS/Linux). Use a **short path** (e.g. `C:\gta` or `/tmp/d3h`): the original `CFileMgr::Initialise` copies the
cwd into a 128-char buffer. The log is `standalone.log` (plus `logs/log.log`); a crash/abort raises exception 0xE0AB0001 and prints a symbolised stack.

Environment variables (all optional, run build only):
* `NOTSA_STANDALONE_SKIP_VIDEOS=1` - skip the intro videos (the original also waits for a key in PLAYING_LOGO when DirectShow fails).
* `NOTSA_STANDALONE_INPUT=<file|script>` - key injector: `wait:<ms>;key:return;down:w;up:w` (newline = `;`); `until:control[:maxms]` / `until:invehicle[:maxms]` hold the script clock until the player has control (ped exists, no menu/cutscene/fade) / is in a vehicle (default max 60000 ms, logs `[until] ... ok|TIMEOUT after N ms`), so later steps are relative to the game being ready (`label:NAME` + `skipif:COND:NAME` jump forward to the label when COND holds, e.g. skip a retry); e.g. `wait:16000;key:return;wait:2500;key:return;wait:2500;key:return` starts a new game.
* `NOTSA_STANDALONE_SCREENSHOT=<k>` - write `frame_N.bmp` (back buffer) every k-th frame; `..._SCREENSHOT_MAX=<n>` caps the count (default 30).
* `NOTSA_STANDALONE_VIDEOMODE=<index>` - force a video-mode index (default: first mode with width >= 800; the list is logged).
* `NOTSA_STANDALONE_MEMLOG=1` - log CRT heap / committed memory every 100 frames (leak hunting).
* `NOTSA_STANDALONE_ALLOCTRACE=<min bytes>` - log the caller of every 200th allocation >= min bytes.
* `NOTSA_STANDALONE_NOPRESENT=1` - skip `Present` (diagnostics).
* `NOTSA_STANDALONE_NO_ABORT_TRACE=1` - do not turn `abort()` into a traced exception.
* `NOTSA_STANDALONE_MUTE=1` - silent run: the whole audio pipeline (DirectSound init, sound banks, streaming, channels, fades) runs unchanged, but every DirectSound
  secondary buffer the hardware layer creates is attached to a patched COM vtable (`source/game_sa/Audio/Hardware/AEDirectSoundMute.h`): `SetVolume` records the volume
  the game asked for and sets `DSBVOLUME_MIN` on the real buffer, `GetVolume` returns the recorded value, so the fade/volume logic that reads volumes back behaves as in the
  original and the menu volume sliders cannot undo the mute. `standalone.log` gets `[mute]` lines (first attach, the count of swallowed `SetVolume` calls at 1 / 1000 / 100000 / 1000000, and each +5 dB step of the loudest volume the game requested).
  `soak.sh` and `census.py` set it by default (`NOTSA_STANDALONE_MUTE=0` opts out). Run-build-only deviation, listed in `tools/standalone/standalone_deviations.tsv` and `STANDALONE_FIDELITY.md`.

Silent runs: never let a test/soak/measurement run play audio on the host. Use `NOTSA_STANDALONE_MUTE=1` (above; the default in `soak.sh`/`census.py`).
Do NOT disable the Wine audio driver (`HKCU\Software\Wine\Drivers` `Audio` = empty) as a second net: with no driver `DirectSoundCreate` fails (`GetDefaultAudioEndpoint` 80070490,
"User explicitly chose no driver") and the game then hangs at 0% CPU right after `CPU vendor:` in `logs/log.log` and never reaches the menu (verified under Wine Staging 11.18, 2026-10-10).
A run of an exe WITHOUT the mute (an older build, or the original `gta_sa.exe`) has to be made silent by other means (e.g. the host volume).

Wine quirks: about half of the starts die in wined3d `Direct3DCreate9` (`nested exception on signal stack`) - simply retry; there is no
800x600 mode (the list starts at 960x600, hence the fallback above); no DirectShow, so videos cannot play; set the registry value
`HKCU\Software\Wine\WineDbg\ShowCrashDialog=0` (DWORD) to avoid the modal crash dialog. Run each experiment in a fresh copy of the exe.

## Tests
`tools/standalone/run_all_tests.sh [build-dir] [regex]` configures (if needed) a Debug librw build, builds every `*_test` target, runs them under Wine
with a retry loop for the wined3d flake and prints a table; exit code is non-zero on any failure/mismatch. Set `ASSETS=<dir>` (infernus.dff, male01.dff,
vgsnbuild07.dff) or `GAME_DIR=<game install>` (the script then extracts them from `models/gta3.img`) for the model-reading tests and `RW_EXE_ORACLE=<path to the original exe>` (done by the script, Windows path form).
Known non-green rows on a clean clone (2026-10-10, verified by a from-scratch build): `review_oracle_test` (8 PC24 / 5191 PC53 strict mismatches, see PHASE2_STATUS),
`rw_skin_pipeline_test` (page fault, pixel-test skeleton in progress); `rw_skin_hw_oracle_test` and `rw_skin_vs_test` print `0 mismatches` lines the script's parser does not
recognise (shown as NORESULT; `rw_skin_vs_test` also needs `TIMEOUT=1500`, it runs several minutes). `ATTEMPTS`/`TIMEOUT` env vars override the retry count / per-test seconds.
Environment for the script: `GTASA_PYTHON` (python with unicorn+capstone), `MSVC_WINE_ROOT`, `WINE_BIN` (dir of `wine`), `WINEPREFIX` (default `~/.wine-msvc`), `SCRATCH` (build mutex dir, default `$TMPDIR/gta-standalone-scratch`).

The **oracle idea**: `tests/standalone/game_oracle.h` maps the original exe's code over the address pad at its original VAs, so a test can call the
original function and our port with the same randomised inputs (incl. NaN/0/denormals, under PC=24 and PC=53) and compare the results bit by bit.
`rw_*_test` (shim vs exe, with a real D3D9 device and pixel read-back), `game_oracle_test`, `review_oracle_test`, `script_oracle_*_test` (script
opcode handlers vs the exe group processors) all use it. Unicorn-generated case lists (`rw_quat_cases.inc`, `rw_strip_cases.inc`) cover RW maths and the tristrip generator.

## Detached globals (work in progress, `.notes/DETACH_DATA_PLAN.md`)
Goal: run without the original exe's data image. Globals the original keeps at fixed addresses are declared with the `NOTSA_GLOBAL*` macros (`source/Base.h`):
by default (and in the ASI/DLL build and the oracle tests) they expand to the old `auto& x = StaticRef<T>(0xADDR)`; with `-DGTASA_DETACHED_GLOBALS=ON`
(preset `StandaloneDetached`, run build only) they are real C++ variables initialised with the exe's values. `NOTSA_ORACLE_ADDRESS_GLOBALS` forces address mode
(the oracle tests that copy the exe target's definitions set it). Tools (dev-time, they need the exe and its extracted image once, the build never runs them):
`tools/standalone/gen_globals.py` (inventory `.notes/DETACH_GLOBALS.tsv`), `globals_emit.py` (initialiser literals from the image), `codemod_globals.py` (rewrites
the declarations; dry run by default, `--check` lists what is left), `obj_code_sig.py` (fingerprint .obj code to prove that the default build did not change).
Verification: build with `-DGTASA_VERIFY_GLOBALS=ON` (set by the preset), run `NOTSA_VERIFY_GLOBALS=<dump> NOTSA_VERIFY_GLOBALS_EXIT=1 gta_reversed.exe` (or without
`_EXIT` to keep playing; `NOTSA_VERIFY_GLOBALS_AFTER=<s>` adds a late snapshot `<dump>.late`) and compare with `python3 -I tools/standalone/verify_globals.py <dump>`.

## Fidelity rules (behaviour, not byte, identity)
* **x87**: the game and shim are compiled `/arch:IA32 /fp:precise` (no SSE in game objects; verified with `dumpbin /disasm`); WinMain calls
  `_set_SSE2_enable(0)`. Creating the D3D9 device drops the FPU precision control to **PC=24** exactly as in the exe (logged as `FPU[...]`).
* **`_ftol2`**: MSVC `/arch:IA32` would emit the saturating `__ftol2_sse`; `source/standalone/FtolExe.cpp` overrides `_ftol2*` with the exe's
  semantics (low dword of `fistp qword`; NaN -> 0).
* **`ExeRecip(N)`** (`ExeRecip.h`, consteval): the exe multiplies by a float reciprocal constant; the compiler would fold `x / N` or `1.f/N` differently.
  `common.h` (SCREEN_STRETCH, lerp, deg/rad) follows the exe's constants.
* **`FIX_BUGS` off** in the run build (`notsa::IsFixBugs()` false) and `NDEBUG` (no asserts; the original has none); `NOTSA_DEBUG` and the ImGui UI are off.
* RW parts are checked against the exe asm (table-based `RwSqrt`, minimax `RwSin`, `acos` polynomial, matrix kernel 0x7F12F0 order, tristrip generator 1:1).
Deliberate deviations are listed in `docs/STANDALONE_FIDELITY.md`.

## Source layout
* `source/standalone/` - `DataImage.*` (maps the data image at the original VAs via the in-exe pad), `Fixups.*` (hooks become pointer fixups in the data image;
  trap stubs for unported addresses; CRT replacement, diagnostics), `FtolExe.cpp`, `GameRand.h` (the game's MSVC LCG `rand`).
* `source/fakerw/` - RenderWare-compatible headers over librw (`rwapi.h` generated by `tools/standalone/gen_fakerw_api.py`, `rwenums.h`, `rwaccessors.h`,
  `rwextra.h`, and one header per RW module the game includes; empty stubs for the unused ones).
* `source/standalone/rw/` - the shim implementation:
  `engine/platform/camera/renderstate/rwd3d_ff` (device, video modes, render-state mapper, FF D3D9 API, restore callback),
  `frame/clump/atomic/world/light/geometry(+_strip)/material` (scene graph, mesh grouping, tristrip),
  `raster/image/texture/texdict/rwtexdict` (rasters, BMP/PNG, TXD streaming, PAL8 expansion),
  `pipeline.cpp` + `pipeline_matfx.cpp` + `matfx/uvanim` (the exe's fixed-function pipelines on librw), `skin/hanim/rtanim/rtquat` (animation),
  `im2d_im3d.cpp` (the exe's 2D/3D immediate driver), `math/rwmath_*/rwtrig/rwpixel/camera_sync*` (bit-exact RW maths), `stream.cpp`, `plugins.cpp`, `rwglobals.cpp`.
* `tools/standalone/` - `extract_exe_data.py`, `initterm_delta.py`, `make_orig_pad.py`, `gen_fakerw_*.py`, `lift_x87.py` (x87 -> C), `quat_unicorn.py`/`strip_unicorn.py` (case generators), `run_all_tests.sh`.
* `tests/standalone/` - all `*_test` targets and the oracle header.

## librw fork
`vendor/librw` -> `https://github.com/littlemyx/librw` branch `notsa` (base aap/librw 18532c2). Patches:
* `bac3df2` uvanim: exact parameter interpolation, add-time keeps the overshoot, per-clone UV matrices, unsigned channel check.
* `5c73376` d3d9: anisotropy > 1 forces MAG/MIN anisotropic + MIP linear (exe `RwD3D9SetTexture`).
* `0540724` skin: `sortLikeSA` after the skin stream read; `AnimInterpolator::create` clears callbacks.
* `a6215f2` uvanim/matfx review fixes (case-sensitive dictionary lookup, identity stand-in anim, used channels only, addRef order).
* `8f8bc5a` frame LTM sync / `Matrix::mult_` in the exe's x87 order; per-frame hierarchy flags (sibling bug).
* `6f80bf3` d3d9: `resetTextureStageSamplers` / `getTextureStageSamplers` (sampler cache agrees with the device).

## Status / known issues (2026-10-10)
* Both librw builds link with 0 unresolved symbols. The exe boots to the main menu (~4-5 s under Wine), then NEW GAME -> loading -> intro cutscene ->
  **gameplay renders** (Los Santos, HUD, radar; ~29 fps with the frame limiter, ~530 fps in the menu, ~300 MB, no leak). Zero RW traps hit so far.
All 2640 script commands are ported.
* Open: (1) `CStreaming::ConvertBufferToObject` -> `RwStreamClose` on a stale stream after ~130 s (suspect the heap-stream decision in `_rwStreamInitialize`);
  (2) `CIdleCam` -> `CInterestingEvents::InvalidateNonVisibleEvents` reads a freed entity after ~60 s of idle; (3) `CEntryExit` pool (400) allocation failures logged once in game.
* Not done: skin pipeline fidelity (exe vs_1_1 / CPU skinning; `.notes/P2B_SKIN_PIPELINE.md`, in progress), `CCurves::CalcCurvePoint` and `CWeapon::FireM16_1stPerson`
  oracle mismatches, repo-wide `_ftol` oracle audit (P2E-2), audio and video playback untested/unsupported under Wine.
* Current truth is in `.notes/PHASE2_STATUS.md` (rows S5, P2E-2).
