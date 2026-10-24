# Standalone GTA SA executable (branch `reverse/interior_c`)

## What it is
`gta_reversed.exe` built in "standalone" mode is a **self-contained Win32 executable made from this repository's reversed C++ code**.
It contains no code of the original game executable. Rendering goes through [librw](https://github.com/aap/librw) (fork:
`vendor/librw`) behind a RenderWare-compatible shim (`source/fakerw`, `source/standalone/rw`), not through the original RenderWare.

The run exe needs **nothing of the original exe, neither to build nor to start** (slice F3 of `.notes/DETACH_DATA_PLAN.md`): every global the original keeps at a fixed
address (`0xB6F5F0` ...) is a real C++ variable in this repository (section "Detached globals"), there is no data image, no `original_data.*` / `data_pointers.bin` next to the exe,
no placeholder in the image and no fixed base. Only the game's own assets (your Steam copy's data folder) are read at run time.

The original `gta_sa.exe` (v1.0 US, "compact") is an **optional input of development tools only** (slice G1: no preset of the user-facing build mentions it, a clean clone without it configures, builds and runs; the proof is `.notes/reports/D-G.md`):
* by the **oracle tests**, which map the original code and data at their original addresses and compare it with our port (they link the 9 MB placeholder object
  `tools/standalone/make_orig_pad.py` into their own test exe, `/BASE:0x400000 /FIXED`);
* by the dev-only configuration `StandaloneDetached` (`GTASA_IMAGE_DIAG=ON` + `GTASA_VERIFY_GLOBALS=ON`): `extract_exe_data.py` extracts `original_data.bin/json` at build time,
  `verify_globals.py` / `verify_vptrs.py` compare the detached globals with them, and the census (`census.py`) maps the image at the original addresses to PROVE that the
  run code touches none of it. This config is the only exe that is still linked at `/BASE:0x400000 /FIXED` with the placeholder, and the only CMake configuration that needs the exe path (`-DGTASA_ORACLE_EXE=<gta_sa_compact.exe>`, preset default `<repo>/gta_sa_compact.exe`); it refuses to configure without it. The extract custom command, the `gta_reversed_exe_data` target and the POST_BUILD copy exist only there.

Never commit the original exe (it is git-ignored). Legal note: private use only; you need your own legitimate copy of the game and its data.

Design details (committed snapshots, `git add -f`-ed because `.notes/` is a local scratch dir elsewhere): `.notes/P2A_DESIGN.md` (historic: data image, hooks-as-fixups, traps; removed by slice F3), `.notes/P2B_SHIM_PLAN.md` (RW shim), `.notes/PHASE2_STATUS.md` (history).

> **Native Windows PC (reference work, no Wine):** see `docs/WINDOWS_SETUP.md` (toolchain pins, clone, presets `StandaloneRelease`/`StandaloneRW`/`StandaloneDetached`, scripts, debugger, first-session checklist).
> The scripts `tools/standalone/{soak,run_all_tests,build_locked}.sh` and `census.py` detect the host (`tools/standalone/hostenv.sh`); the Wine-specific text below applies to macOS/Linux only.

## Prerequisites
* **MSVC x86 toolchain.** Windows: VS 2022/2026 with the x86 C++ tools. macOS/Linux: [msvc-wine](https://github.com/mstorsjo/msvc-wine) plus Wine
  (tested: Wine Staging 11.x on macOS, `WINEPREFIX=~/.wine-msvc`, MSVC 14.51, SDK 10.0.26100, `cl` at `~/tools/msvc/bin/x86/cl`).
  Wine toolchain files (committed, portable): `tools/standalone/msvc-wine/{toolchain-msvc-wine.cmake,no-wx.cmake,conanprofile-wine-debug.txt,conanprofile-wine-release.txt}`;
  point them at your msvc-wine install with `export MSVC_WINE_ROOT=<dir>` (default `~/tools/msvc`; `cl` must be `$MSVC_WINE_ROOT/bin/x86/cl`).
* CMake >= 4.2, Ninja, conan 2 (e.g. `pipx install conan`), git (with submodules: `git submodule update --init vendor/librw`).
* Python 3 (any; passed as `-DGTASA_PYTHON=<python>`): the run exe itself builds without it, the oracle `*_test` targets use it for the placeholder object. `unicorn` and `capstone` (`pip install unicorn capstone`) are needed only by dev tools: the `StandaloneDetached` extractor (replays the exe's 1667 static initialisers under Unicorn), `lift_x87.py`, the case generators.
* The game's data directory (a legitimate install).
* **Optional, dev only:** `gta_sa_compact.exe` (v1.0 US compact) in the repository root, for the exe-oracle tests and the `StandaloneDetached` config. Not needed to build, run or test the non-oracle part.

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
PY=<python 3>                        # unicorn+capstone only for the dev tools (venv: python3 -m venv ~/gta-venv && ~/gta-venv/bin/pip install unicorn capstone)
COMMON=(-G Ninja -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" -DGTASA_STANDALONE=ON -DGTASA_USE_SDL3=ON -DGTASA_UNITY_BUILD=OFF -DGTASA_WITH_LTO=OFF
  -DGTASA_WITH_OPENAL=OFF -DGTASA_WITH_CLEO_COMMANDS=OFF -DGTASA_WITH_SCRIPT_COMMAND_HOOKS=OFF
  -DGTASA_PYTHON="$PY"
  -DCMAKE_C_COMPILER="$MSVC/bin/x86/cl" -DCMAKE_CXX_COMPILER="$MSVC/bin/x86/cl"
  "-DCMAKE_C_FLAGS=/DWIN32 /D_WINDOWS /Zm1000" "-DCMAKE_CXX_FLAGS=/WX- /wd4005 /DWIN32 /D_WINDOWS /Zm1000 /GX")
```
(Use the array exactly as `"${COMMON[@]}"` below - the flags contain spaces.)
1. **`build/Standalone`** - original RenderWare (opt-out of librw), Debug:
   `cmake -S . -B build/Standalone -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=$PWD/build/Debug/generators/conan_toolchain.cmake "${COMMON[@]}" -DGTASA_RW_LIBRW=OFF -DGTASA_RW_LIBRW_OPT_OUT=ON`
   (RW functions are reached by exe address and trap; kept for comparison, it stops at the first RW call.)
2. **`build/StandaloneRW`** - librw, Debug (the main development build):
   `cmake -S . -B build/StandaloneRW -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=$PWD/build/Debug/generators/conan_toolchain.cmake "${COMMON[@]}" -DGTASA_RW_LIBRW=ON -DGTASA_RW_LIBRW_OPT_OUT=OFF`
3. **`StandaloneRelease`** preset - librw, `/O2 /Ob2 /DNDEBUG -MT /Z7 /arch:IA32 /fp:precise`, ~17 MB exe (Debug ~50 MB; was 33 MB before the 9.1 MB placeholder and the hook/fixup machinery went), bit-exact with Debug:
   `cmake --preset StandaloneRelease -DGTASA_PYTHON="$PY"` (+ under msvc-wine `-DCMAKE_C_COMPILER=... -DCMAKE_CXX_COMPILER=...` and the two `CMAKE_*_FLAGS` entries above, as separate quoted arguments)

Build: `ninja -C build/StandaloneRW gta_reversed.exe`. No exe is needed and no extraction step runs for these presets (only `StandaloneDetached`, `GTASA_IMAGE_DIAG=ON` + `GTASA_ORACLE_EXE`, runs the extractor and
writes `original_data.bin/json` and `data_pointers.bin` next to its exe in `<build>/bin`; they are tied to the 1.0 US compact exe). New `.cpp` files need a CMake re-glob (re-run cmake).
Under Wine, one build at a time (cl via Wine is memory hungry); `ninja` may be interrupted by Wine (exit 130) - just re-run.

## Run
Copy `gta_reversed.exe` (alone: no image files) into the **game data directory** and run it from there
(`wine gta_reversed.exe` on macOS/Linux). Use a **short path** (e.g. `C:\gta` or `/tmp/d3h`): the original `CFileMgr::Initialise` copies the
cwd into a 128-char buffer. The log is `standalone.log` (plus `logs/log.log`); a crash/abort raises exception 0xE0AB0001 and prints a symbolised stack.

Environment variables (all optional, run build only):
* `NOTSA_STANDALONE_SKIP_VIDEOS=1` - skip the intro videos (the original also waits for a key in PLAYING_LOGO when DirectShow fails). Native Windows plays them (DirectShow, Logo.mpg 15 s + GTAtitles.mpg 88 s, then the menu, same timeline as the original exe);
  the end-of-movie notification reaches the game through an SDL message hook (`VideoPlayer.cpp`), `.notes/reports/BUG-VIDEO.md`. Under `NOTSA_STANDALONE_MUTE=1` the graph's audio renderer is muted too (`IBasicAudio`, -100 dB, logged as `[mute] ... DirectShow`).
* `NOTSA_STANDALONE_INPUT=<file|script>` - key injector: `wait:<ms>;key:return;down:w;up:w` (newline = `;`); `until:control[:maxms]` / `until:invehicle[:maxms]` (also `onfoot`, `nomission` = no mission running) hold the script clock until the player has control (ped exists, no menu/cutscene/fade) / is in a vehicle (default max 60000 ms, logs `[until] ... ok|TIMEOUT after N ms`), so later steps are relative to the game being ready (`label:NAME` + `skipif:COND:NAME` jump forward to the label when COND holds, e.g. skip a retry); `mission:N` starts main.scm mission N like the debug menu's Missions Starter (ids: toolsmenu MissionDebugModule; a running mission is failed first), `kill:0` / `arrest:0` put the player ped into PEDSTATE_DEAD / PEDSTATE_ARRESTED so CGameLogic runs the wasted / busted flow (mission-failure repros, PT-MISSION); `probe:NAME` logs one `[probe] NAME wl=.. hp=.. weap=[..] veh=[..] ...` line of game state (wanted level, health, cheat flags, weapons, every vehicle model ...; the same fields `tools/standalone/windows/cheat_probe_exe.py` samples from the ORIGINAL exe - VT-OVERRIDES cheat comparison, `tools/standalone/gen_cheats_route.py` generates the scene for both exes); e.g. `wait:16000;key:return;wait:2500;key:return;wait:2500;key:return` starts a new game.
  `vmode:<idx>` applies video mode `<idx>` the way the menu's resolution screen does (`m_nPrefsVideoMode`, `SetVideoMode`, `CPostEffects::DoScreenModeDependentInitializations`; logs `[vmode] N at T ms`), e.g. `vmode:15;wait:10000;vmode:23` for a live mode-change regression run (PLAYTEST-1 U2: the post-effects front buffer stayed one mode behind).
  `aa:<n>` does what accepting the menu's Anti Aliasing option does (`m_nPrefsAntialiasing` = n, `RwD3D9ChangeMultiSamplingLevels`, `SetVideoMode`, `SaveSettings`; n = 1 off, 2.. the option values "1".., max 4) and logs `[aa] n at T ms: max=.. sel=.. live_type=.. dev=ok|LOST` (the multisample type of the LIVE swap chain; PLAYTEST-1 U17). `cmd:hour:<H>` sets the game clock to H:00 (the heat haze post effect needs H >= 10). `cmd:endmission` stops the running mission script like MISSION_HAS_FINISHED + TERMINATE_THIS_SCRIPT (mission cleanup, `OnAMission` = 0; the save menu is only legal outside a mission: the mission script's code is not part of a save), `cmd:savemenu` opens the save-game menu like ACTIVATE_SAVE_MENU. `tools/standalone/saveload_roundtrip.sh` = save -> load round trip with both (SAVE-LOAD). With a multisampled back buffer the `NOTSA_STANDALONE_SCREENSHOT` dumps resolve it first (StretchRect), otherwise the driver hands out sample 0 only.
* `NOTSA_STANDALONE_SCREENSHOT=<k>` - write `frame_N.bmp` (back buffer) every k-th frame; `..._SCREENSHOT_MAX=<n>` caps the count (default 30).
  `..._SCREENSHOT_ARM=<mark>` starts the every-k-th-frame cadence only after the input script's `mark:<mark>` (frame-exact bursts without burning `_MAX` in the menu/loading);
  `..._SCREENSHOT_BURST=<n>` makes a script line `mark:burst_<x>` save the NEXT n Presents as `burst_<x>_00.bmp` .. (consecutive frames, outside the cadence and `_MAX`); every other `mark:<x>` writes one `mark_<x>.bmp`
  (all of these need `NOTSA_STANDALONE_SCREENSHOT` to be set, a huge k such as 100000 turns the cadence off).
* `NOTSA_STANDALONE_VIDEOMODE=<index>` - force a video-mode index (default: the native-resolution / 800x600x32 search, else the smallest 32 bit exclusive mode with width >= 800; the list is logged). An invalid index is logged and ignored. NOTE: it is only consulted when the settings file has no usable mode (`CheckDefaultVideoModeSupported`); a saved `gta_sa.set` (`m_nDisplayVideoMode`, dword at byte 0xB36 of the 2900-byte file in `<Documents>/GTA San Andreas User Files/`) wins (mode 1 = 960x600 R5G6B5, 16 bit, no stencil shadows, is what the settings of a normal run contain; 35 = 1920x1200 X8R8G8B8 on this Mac). A missing `gta_sa.set` is fine (fresh prefix / first run on a new PC): the default search runs, see the regression note under "Status / known issues".
* `NOTSA_STANDALONE_WINDOWED=<W>x<H>` (e.g. `1280x720`; `1`/`on` = 960x600; `0`/`off`/empty = off, the default) - run in a WINDOW instead of exclusive fullscreen, so an automated run never switches the desktop
  resolution of the PC the user works on (and never changes the adapter's mode list for later runs). Run build only (`NOTSA_STANDALONE_RUN`, engine.cpp `WindowedEmu`), no effect on the game binary.
  The device is created windowed (`D3DPRESENT_PARAMETERS.Windowed = TRUE`, back buffer = window client area = W x H, desktop format), the window has a caption and no sizing border, is centred on the primary
  monitor, is not topmost and is shown WITHOUT taking the focus (`NOTSA_STANDALONE_WINDOWED_FOCUS=1` activates it as usual, e.g. for a `SendInput` driver). The game keeps seeing an ordinary exclusive video mode: a
  synthetic `W x H x 32` mode is appended at the END of the mode list (the indices of the real modes, which `gta_sa.set` stores, do not move; the logged list ends with it) and `psSelectDevice` selects it, overriding
  the saved / default / searched mode (the saved index in `gta_sa.set` is not touched, but the file is still written by a normal quit as usual); `RwEngineGetVideoModeInfo` reports every exclusive mode as exclusive, so
  `fullScreen`, `RsGlobal.maximumWidth/Height` (= W/H), the HUD scale and the menu mode list behave as in an exclusive W x H run. Inside the device layer every exclusive mode is a windowed one: a live mode change
  from the menu (`RwD3D9ChangeVideoMode`) resizes the window instead of switching the display. `standalone.log` has `NOTSA_STANDALONE_WINDOWED:` / `windowed: window client WxH` lines.
  Frame limiter / vsync: unchanged logic (`PresentationInterval` as in the exclusive run: librw starts with IMMEDIATE, `RwCameraShowRaster` flags/`Reset` switch it with the menu vsync option; the game's own
  frame limiter is software and works the same). In a window Present is composed by DWM, so there is no tearing and a vsync request is paced by the compositor (the refresh rate of the monitor the window is on),
  not by an exclusive flip chain; the frame-time distribution therefore differs slightly from the exclusive run. Consequence for MEASUREMENTS: compare ours vs the original exe in the SAME mode - the original has no
  windowed switch, so WIN_FPS / `fps_run.sh` / `startup_probe.py` runs of both exes are exclusive fullscreen (do not export `NOTSA_STANDALONE_WINDOWED` in the shell that starts them); if a windowed number of ours is
  quoted, say so. `soak.sh` and `census.py` set `960x600` by default on the Windows host (the size `soak_route.txt`'s mouse coordinates assume; `NOTSA_STANDALONE_WINDOWED=0` or empty opts out; not set on the Wine hosts).
  Listed in `tools/standalone/standalone_deviations.tsv` and `STANDALONE_FIDELITY.md`.
* `NOTSA_STANDALONE_MEMLOG=1` - log CRT heap / committed memory every 100 frames (leak hunting).
* `NOTSA_STANDALONE_ALLOCTRACE=<min bytes>` - log the caller of every 200th allocation >= min bytes.
* `NOTSA_STANDALONE_NOPRESENT=1` - skip `Present` (diagnostics).
* `NOTSA_STANDALONE_NOVSYNC=1` - (run build only, off by default) present with sync interval 0: `RwCameraShowRaster` (camera.cpp) clears `rwRASTERFLIPWAITVSYNC` before `showRaster`, so the menu "frame limiter" option and the loading-screen vsync request no longer
* `NOTSA_STANDALONE_USERDIR=<path>` - (run build only, unset = the exe's behaviour) the user-files directory (`InitUserDirectories`, FileMgr.cpp: gta_sa.set, saves, Gallery, User Tracks) is `<path>` instead of `DocumentsGTA San Andreas User Files` (created if missing; takes priority over the ini `SaveDirectoryPath`). The agent scripts set it to a private per-run dir `C:/gta-scratch/userdir/<run>` (`hostenv.sh userdir_new`: empty = first start, `USERDIR_SEED=playtest1|<file>` seeds a gta_sa.set); `play.cmd` clears it, so the user's play session always uses Documents. Runs of the ORIGINAL exe cannot be redirected: `orig_run.sh` / `fps_run.sh orig` / `startup_probe.py` back up and restore the whole Documents dir (`docs_backup` / `docs_restore`).
  make `IDirect3DDevice9::Present` wait for the vertical blank. On the dev PC a vsync-locked Present costs ~78 ms/frame, i.e. 30-75 s from start to the menu; without it the boot takes a few seconds. **Not affected**: all game logic and timing (`CTimer`, the game's software frame limiter (WinMain.cpp GAME_STATE_IDLE: one frame per 1000/RsGlobal.frameLimit ms when the option is on), `m_bPrefsFrameLimiter` and the saved settings stay as they are), the device-reset path (`ResetDeviceNow` flips the interval itself). **Affected**: only display pacing - the frame rate shown (no vsync cap), possible tearing in exclusive fullscreen; in `NOTSA_STANDALONE_WINDOWED` mode DWM still composes. Never use it for frame-time/fps measurements (WIN_FPS, `fps_run.sh`) unless the report states the mode; `soak.sh` passes it through (opt-in), `play.cmd` clears it. `standalone.log` has a `NOTSA_STANDALONE_NOVSYNC:` line when active. Listed in `tools/standalone/standalone_deviations.tsv` and `STANDALONE_FIDELITY.md`.
* `NOTSA_STANDALONE_NO_ABORT_TRACE=1` - do not turn `abort()` into a traced exception.
* `NOTSA_STANDALONE_MUTE=1` - silent run: the whole audio pipeline (DirectSound init, sound banks, streaming, channels, fades) runs unchanged, but every DirectSound
  secondary buffer the hardware layer creates is attached to a patched COM vtable (`source/game_sa/Audio/Hardware/AEDirectSoundMute.h`): `SetVolume` records the volume
  the game asked for and sets `DSBVOLUME_MIN` on the real buffer, `GetVolume` returns the recorded value, so the fade/volume logic that reads volumes back behaves as in the
  original and the menu volume sliders cannot undo the mute. `standalone.log` gets `[mute]` lines (first attach, the count of swallowed `SetVolume` calls at 1 / 1000 / 100000 / 1000000, and each +5 dB step of the loudest volume the game requested).
  `soak.sh` and `census.py` set it by default (`NOTSA_STANDALONE_MUTE=0` opts out). Run-build-only deviation, listed in `tools/standalone/standalone_deviations.tsv` and `STANDALONE_FIDELITY.md`.

* `NOTSA_AUDIO_TAP=<file>` - silent audio diagnostics (`source/game_sa/Audio/Hardware/AEAudioTap.h`, usable together with the mute): every DirectSound secondary buffer is attached to a copy of its vtable that logs
  Lock/Unlock (offsets, cursors at the write, hash, first/last frames), GetCurrentPosition slot tracking, Play/Stop, SetVolume (thread, fade-entry range), SetFrequency, plus the main-loop frame time and the stream-thread
  iteration time. `!KIND` lines are anomalies (stale half replayed = underrun, write under the play cursor, seam jump, click, fade value out of range, rejected SetVolume/SetFrequency, stream-thread stall), each tagged with the
  time since the last >50 ms frame; `[tap-summary]` blocks every 30 s and at exit. `NOTSA_AUDIO_TAP_RAW=1` logs every call. Use a Windows path under Wine (`Z:\tmp\tap.log`).

* `NOTSA_AUDIO_STATS=<file>` (`NOTSA_AUDIO_STATS_EVERY=<frames>`, default 60) - silent MIXER statistics (`source/game_sa/Audio/Hardware/AEAudioStats.h`, `notsa::audio_stats::Service` at the end of `CAESoundManager::Service`): one line per N frames with the collision-list fill (`coll=used(one loop n= add=)`), active / physically playing sounds (`act`, `phys`, peak since the last line), hardware channels in use, `want` (audible sounds without a hardware channel), looped / stop-requested / uncancellable sounds, age of the oldest sound, DirectSound buffers alive, requests / refused requests (the 300-sound table was full), `PlaySound` outcomes (no buffer, bad channel, `SetAudioBuffer` / `CreateSoundBuffer` / `Lock` failures with the last HRESULT), failed `IDirectSoundBuffer::Play` (needs `NOTSA_AUDIO_TAP` too), the pause flags and the fader / master scaling factors. A refused request also dumps a breakdown of the active sounds (bank, sound, event, owner vtable RVA). `tools/standalone/audio_stats_exe.py <file>` samples the same quantities from a RUNNING original `gta_sa.exe` (ReadProcessMemory), so a scripted scene can be compared (`tools/standalone/audio_dense_route.txt` for ours, `tools/standalone/windows/audio_scenes.txt` for the exe via inputdrv).

Silent runs: never let a test/soak/measurement run play audio on the host. Use `NOTSA_STANDALONE_MUTE=1` (above; the default in `soak.sh`/`census.py`).
Do NOT disable the Wine audio driver (`HKCU\Software\Wine\Drivers` `Audio` = empty) as a second net: with no driver `DirectSoundCreate` fails (`GetDefaultAudioEndpoint` 80070490,
"User explicitly chose no driver") and the game then hangs at 0% CPU right after `CPU vendor:` in `logs/log.log` and never reaches the menu (verified under Wine Staging 11.18, 2026-10-10).
A run of an exe WITHOUT the mute (an older build, or the original `gta_sa.exe`) has to be made silent by other means (e.g. the host volume).

Wine quirks: about half of the starts die in wined3d `Direct3DCreate9` (`nested exception on signal stack`) - simply retry; there is no
800x600 mode (the list starts at 960x600, hence the fallback above); no DirectShow, so videos cannot play; set the registry value
`HKCU\Software\Wine\WineDbg\ShowCrashDialog=0` (DWORD) to avoid the modal crash dialog. Run each experiment in a fresh copy of the exe.

## Tests
`tools/standalone/run_all_tests.sh [build-dir] [regex]` (on Windows: natively, no Wine) configures (if needed) a Debug librw build, builds every `*_test` target, runs them under Wine
with a retry loop for the wined3d flake and prints a table; exit code is non-zero on any failure/mismatch. The original exe is optional (`GTA_EXE=<path>`, default `<repo>/gta_sa_compact.exe`): when the file is absent the exe-oracle tests (23 targets that exit 77 without `RW_EXE_ORACLE`) are listed as `SKIP(no_exe)`, are not counted as passes and do not affect the exit code (the last line says `N tests: X ok, Y SKIP(no exe), Z other`). Set `ASSETS=<dir>` (infernus.dff, male01.dff,
vgsnbuild07.dff) or `GAME_DIR=<game install>` (the script then extracts them from `models/gta3.img`) for the model-reading tests and `RW_EXE_ORACLE=<path to the original exe>` (set by the script from `GTA_EXE` / the repo-root exe when the file exists, Windows path form; an inherited value is ignored).
Known non-green rows on a clean clone (2026-10-10, verified by a from-scratch build): `review_oracle_test` (8 PC24 / 5191 PC53 strict mismatches, see PHASE2_STATUS),
`rw_skin_pipeline_test` (page fault, pixel-test skeleton in progress); `rw_skin_hw_oracle_test` and `rw_skin_vs_test` print `0 mismatches` lines the script's parser does not
recognise (shown as NORESULT; `rw_skin_vs_test` also needs `TIMEOUT=1500`, it runs several minutes). `ATTEMPTS`/`TIMEOUT` env vars override the retry count / per-test seconds.
Environment for the script: `GTASA_PYTHON` (python with unicorn+capstone), `MSVC_WINE_ROOT`, `WINE_BIN` (dir of `wine`), `WINEPREFIX` (default `~/.wine-msvc`), `SCRATCH` (build mutex dir, default `$TMPDIR/gta-standalone-scratch`).

The **oracle idea**: `tests/standalone/game_oracle.h` maps the original exe's code over the address pad at its original VAs, so a test can call the
original function and our port with the same randomised inputs (incl. NaN/0/denormals, under PC=24 and PC=53) and compare the results bit by bit.
`rw_*_test` (shim vs exe, with a real D3D9 device and pixel read-back), `game_oracle_test`, `review_oracle_test`, `script_oracle_*_test` (script
opcode handlers vs the exe group processors) all use it. Unicorn-generated case lists (`rw_quat_cases.inc`, `rw_strip_cases.inc`) cover RW maths and the tristrip generator.

## Gameplay regression suite (`tools/standalone/gameplay_regress.sh`)
Soaks and per-function oracles did not find the 14 gameplay bugs of the user's first playtest (`.notes/USER_PLAYTEST_1.md`: idle police, no weapon drops, dead audio, squares, mouse capture, missions not reset ...). The suite replays them as
short scripted scenarios on OUR build (and, where the scenario can be observed from outside, on the ORIGINAL exe), extracts comparable numbers and prints `PASS | FAIL | KNOWN | SKIP` per check with a tolerance that is justified by the game's randomness.
**Run it before declaring a gameplay-affecting change done** (tasks / AI / audio / pools / scripts / input / camera / render path), next to `run_all_tests.sh` and the soak: `tools/standalone/gameplay_regress.sh` (~17 min, silent, windowed, no arguments needed; `-l` lists the scenarios,
`gameplay_regress.sh combat audio` runs some). Exit code 0 = no FAIL, 1 = a FAIL, 2 = could not run. Windows host only (the same Git Bash environment as `soak.sh`).

| scenario | route | what it replays (playtest item) | checks (`<scenario>.<check>`) |
|---|---|---|---|
| `combat` | `routes/combat.txt` | wanted level -> police in the open street (Idlewood); killed armed cop; player hit; car occupants (U5 U6 U7) | `chase.police_arrive`, `chase.police_engage` (any sign within 70 s: cop on foot / pursuit-arrest-kill task / player hurt / busted), `chase.no_zombie_cops` (after a lethal damage event no cop stays at 0 hp without dying), `foot.cop_dies`, `foot.weapon_pickup` (a dead armed cop on foot drops a pickup), `hit.damage_event`, `chase.player_shot_at` (real bullets hurt CJ), `car.driver_dies` (a script-spawned police car with its cop driver: `CTaskSimpleCarDrive::MakeAbortable` must let the event through) |
| `audio` | `routes/audio.txt` | a Rancher driven into five parked cars, then 25 s of idling; mixer statistics (`NOTSA_AUDIO_STATS`) (U12 U13) | `coll_list_max`, `coll_list_released` (one-shot collision entries are freed: `CAECollisionAudioEntity::UpdateParameters` 0x4DA370), `active_sounds`, `channels`, `starvation` (no refused request / list-full / sound without channel), `no_stuck_loops` |
| `cheats` | `routes/cheats.txt` (generated by `gen_cheats_scenario.py`) | 18 typed cheats incl. the toggles that were swapped (Funhouse, Ninja, VillagePeople, BeachParty, Dusk) | `state_matches_original`: wanted / armour / money / cheat flags / weapons / weather / time scale / vehicle models added after every cheat = the original's (`baseline.json` "cheats") |
| `missions` | `routes/missions.txt` | opening mission; Return mounts / dismounts / re-mounts a bike and enters a car; `arrest:0` during the mission (U8 U10 U11) | `first_mission_started`, `bike.enter_return`, `bike.exit_return`, `bike.reenter_return`, `car.enter_return`, `arrest.game_goes_on` (busted flow ends, control back), `arrest.mission_cleaned_up` (mission flag 1 -> 0: `ResetIP`) |
| `render_vmode` | `routes/vmode.txt` | live video-mode change at game hour 10 (heat haze) (U2) | `vm_N.front_covers_camera` (post-effects front buffer >= camera raster, else `StretchRect` fails with `D3DERR_INVALIDCALL` every frame), `vm_N.camera_is_mode` |
| `input_mouse` | `routes/mouse.txt` + `mouse_sweep.ps1` | REAL mouse (`mouse_event`) sweeps, the camera must keep turning (U3) | `relative_mode_on`, `sweep1_rotates` (>= 360 deg: 1253 deg with relative mode, 150 deg without), `sweep2_not_stalled`, `past_360` |
| `render_firststart` | `routes/firststart.txt` | first start without `gta_sa.set`, EXCLUSIVE fullscreen (the only scenario that switches the desktop mode; `GR_SKIP=render_firststart`) (U1) | `window_is_mode_size`, `default_mode_800x600` |
| `interior_lspd` | `routes/interior.txt` | 3 stars, teleport to the LSPD entrance (U15 U16; repro of PT2-INTERIOR) | `entered`, `cops_present`, `cops_on_floor`, `cops_react` |
| `arrest0` | `routes/arrest0.txt` | `CGameLogic::Update` busted / wasted fee (exe 0x442AD0; ARREST0): arrested at 0 stars (fee switch falls through to 100), money <= 0 stays untouched (busted and wasted), 1000 -> 900 | `busted_fee_100`, `busted_negative_money_kept`, `wasted_fee_100`, `wasted_negative_money_kept` (`cmd:money:N` sets the money, snapshots carry `money=`) |

Every scenario also gets `no_crash` (no UNREACHABLE / exception / assert lines in `standalone.log` / `log.log`) and `ran_to_end` (the route finished and left the game loop by itself: `cmd:quit`).
**How a run works**: the exe is copied into a private data dir (`C:/gta-scratch/gr_data`, links to the Steam assets, no original exe), started with `NOTSA_STANDALONE_MUTE=1`, `NOTSA_STANDALONE_WINDOWED=960x600` (not for `render_firststart`) and the route as `NOTSA_STANDALONE_INPUT`;
`NOTSA_STANDALONE_NOPRESENT=1` is set for the scenarios that do not look at pixels (start-up in seconds instead of ~30 s, the loading screens are frame-count loops). Each run holds the GPU lock (`C:/gta-scratch/gpu.lock`) for that run only and waits
while somebody else (in particular `USER-PLAY`) holds it; the user's `gta_sa.set` is saved before the suite and put back afterwards. A scenario whose set-up failed (a gate timed out, the game died before the first snapshot) is run once more before it is judged. Results: `$GR_OUT/<scenario>/`
(`standalone.log`, `route.txt`, `audio_stats.txt`, screenshots of the pixel scenarios), `$GR_OUT/<scenario>.result` and `$GR_OUT/summary_table.txt`.
**Determinism**: the same start state every time (new game, intro skipped, CJ at the same spot with control; `until:` gates instead of fixed waits), and the injector places what it needs (`cmd:spawncop`, `cmd:spawnveh`, `cmd:hitplayer`, `cmd:shootlast|shootdrv`, `cmd:time`) so that the checks do not depend on what the AI happens to do. Only the police chase and the audio scene
keep the game's randomness; their tolerances come from the original exe (`baseline.json`: each threshold has a `why`, `orig` = what the original showed): *any one* sign of engagement within 70 s (the original shows cop cars idling at 25-50 m for 40 s in one run and busting CJ within 25 s in another),
collision-list / channel / sound counts must stay inside the original's observed range with a margin (a leak grows without bound, the margin is far below it).
**Original exe**: `-o` also runs `combat`, `audio` and `cheats` on the original `gta_sa.exe` (v1.0 US, `GR_ORIG_DIR`, default `C:/gta-perf/orig`; tools in `GR_PX`, default `C:/gta-perf/px`, built by `windows/build_tools.sh`) through `inputdrv.exe` (SendInput + memory gates) with out-of-process pollers (`orig_gr_poll.py`,
`audio_stats_exe.py`, `windows/cheat_probe_exe.py`) and prints its numbers next to the thresholds (the cheat states are compared with THAT run instead of `baseline.json`; +~12 min; exclusive fullscreen, the desktop mode is switched, the game needs the foreground, silent through the dsound proxy). `record_baseline.py` refreshes the `orig` numbers of `baseline.json` from such a run.
**KNOWN**: `known.tsv` lists checks that fail because a playtest item is still open (`<scenario>.<check><TAB>reason`); they are shown as KNOWN and do not fail the suite. Delete the line when the item is fixed.
**New injector items** (run build only, `GameplayRegress.cpp` + `SDLWrapper.cpp`): `cmd:snap:NAME` logs `[gr] NAME#n A|B|C|D ...` (player vitals / task chain / mission flags / relative-mouse mode / accumulated camera heading; cops, pickups, law cars; the last spawned car, driver and cop; video mode, rasters, window client size),
`cmd:spawncop[:MODEL[:DIST]]`, `cmd:spawnveh:MODEL[:DRIVER[:DIST]]`, `cmd:shootlast`, `cmd:shootdrv`, `cmd:copattack`, `cmd:hitplayer[:WEAPON[:DAMAGE]]`, `cmd:time:H[:M]`, `cmd:quit` (normal shutdown), `until:cutscene` / `until:nocutscene`, `vmode:WxH` (the first video mode of that size, host independent).
**Adding a scenario for a new finding**: write `routes/<name>.txt` (start with `#include header`, end with `cmd:quit`), add a row to the table in `gameplay_regress.sh`, a `chk_<name>` in `gr_check.py` and thresholds with a `why` in `baseline.json`; if the original exe can show the same thing, add `orig/<name>.scn` + a poller and set `live original reference` = 1.


## Detached globals (`.notes/DETACH_DATA_PLAN.md`; the run build is always detached since F3)
The run exe runs without the original exe's data image. Globals the original keeps at fixed addresses are declared with the `NOTSA_GLOBAL*` macros (`source/Base.h`):
in the run exe (every `GTASA_STANDALONE=ON` preset) they are real C++ variables initialised with the exe's values (`NOTSA_DETACHED_GLOBALS`, always defined for the exe target);
in the ASI/DLL build and in the oracle tests they expand to the old `auto& x = StaticRef<T>(0xADDR)`. `NOTSA_ORACLE_ADDRESS_GLOBALS` forces address mode
(the oracle tests that copy the exe target's definitions set it). In the run exe `StaticRef<T>()`, `plugin::Call*<addr>` and every `RH_Scoped*` hook macro are compile errors / no-ops
(`NOTSA_NO_HOOK_REGISTRY`, `reversiblehooks/NoHookRegistry.h`): there is no hook registry, no exported vtable, no exe address anywhere in its object code.
The exe is a **normal relocatable image** (default base, `/DYNAMICBASE`, ASLR on); `standalone.log` starts with `image base 0x... (ASLR on, ...)`. Tools (dev-time, they need the exe and its extracted image once, the build never runs them):
`tools/standalone/gen_globals.py` (inventory `.notes/DETACH_GLOBALS.tsv`), `globals_emit.py` (initialiser literals from the image), `codemod_globals.py` (rewrites
the declarations; dry run by default, `--check` lists what is left), `obj_code_sig.py` (fingerprint .obj code to prove that the default build did not change).
Verification: build the dev preset `StandaloneDetached` (`-DGTASA_VERIFY_GLOBALS=ON -DGTASA_IMAGE_DIAG=ON`, needs `-DGTASA_ORACLE_EXE=<gta_sa_compact.exe>`, preset default `<repo>/gta_sa_compact.exe`; `original_data.bin` next to the exe), run `NOTSA_VERIFY_GLOBALS=<dump> NOTSA_VERIFY_GLOBALS_EXIT=1 gta_reversed.exe` (or without
`_EXIT` to keep playing; `NOTSA_VERIFY_GLOBALS_AFTER=<s>` adds a late snapshot `<dump>.late`) and compare with `python3 -I tools/standalone/verify_globals.py <dump>`.

## Fidelity rules (behaviour, not byte, identity)
* **x87**: the game and shim are compiled `/arch:IA32 /fp:precise` (no SSE in game objects; verified with `dumpbin /disasm`); WinMain calls
  `_set_SSE2_enable(0)`. Creating the D3D9 device drops the FPU precision control to **PC=24** exactly as in the exe (logged as `FPU[...]`).
* **`_ftol2`**: MSVC `/arch:IA32` would emit the saturating `__ftol2_sse`; `source/standalone/FtolExe.cpp` overrides `_ftol2*` with the exe's
  semantics (low dword of `fistp qword`; NaN -> 0).
* **`ExeRecip(N)`** (`ExeRecip.h`, consteval): the exe multiplies by a float reciprocal constant; the compiler would fold `x / N` or `1.f/N` differently.
  `common.h` (SCREEN_STRETCH, lerp, deg/rad) follows the exe's constants.
* **All bugfixes off**: `FIX_BUGS` is not defined in the run build (`notsa::IsFixBugs()` false; `#error` if both `NOTSA_STANDALONE_RUN` and `FIX_BUGS` are set) and every `notsa::bugfixes::*` switch defaults to OFF (`reversiblebugfixes/ReversibleBugFix.hpp`; the two kept exceptions are listed in `docs/STANDALONE_FIDELITY.md`). Also `NDEBUG` (no asserts; the original has none); `NOTSA_DEBUG` and the ImGui UI are off.
* RW parts are checked against the exe asm (table-based `RwSqrt`, minimax `RwSin`, `acos` polynomial, matrix kernel 0x7F12F0 order, tristrip generator 1:1).
Deliberate deviations are listed in `docs/STANDALONE_FIDELITY.md`.

## Source layout
* `source/standalone/` - `Fixups.*` (name is historic: `standalone.log` `Log`/`Fatal`, process diagnostics: atexit/purecall/terminate/abort/CRT report hooks, sampler; F3 removed the
  hook-fixups, the trap stubs and the redirect handler), `ImageLoad.cpp` + `ImageDiag.*` (DEV ONLY, `GTASA_IMAGE_DIAG`: maps the original data image for the census / poison),
  `GlobalsVerify*` (dev only, `GTASA_VERIFY_GLOBALS`), `FtolExe.cpp`, `GameRand.h` (the game's MSVC LCG `rand`).
* `source/fakerw/` - RenderWare-compatible headers over librw (`rwapi.h` generated by `tools/standalone/gen_fakerw_api.py`, `rwenums.h`, `rwaccessors.h`,
  `rwextra.h`, and one header per RW module the game includes; empty stubs for the unused ones).
* `source/standalone/rw/` - the shim implementation:
  `engine/platform/camera/renderstate/rwd3d_ff` (device, video modes, render-state mapper, FF D3D9 API, restore callback),
  `frame/clump/atomic/world/light/geometry(+_strip)/material` (scene graph, mesh grouping, tristrip),
  `raster/image/texture/texdict/rwtexdict` (rasters, BMP/PNG, TXD streaming, PAL8 expansion),
  `pipeline.cpp` + `pipeline_matfx.cpp` + `matfx/uvanim` (the exe's fixed-function pipelines on librw), `skin/hanim/rtanim/rtquat` (animation),
  `im2d_im3d.cpp` (the exe's 2D/3D immediate driver), `math/rwmath_*/rwtrig/rwpixel/camera_sync*` (bit-exact RW maths), `stream.cpp`, `plugins.cpp`, `rwglobals.cpp`.
* `tools/standalone/` - `extract_exe_data.py`, `initterm_delta.py` (dev: image for the census / verifiers), `make_orig_pad.py` (placeholder of the oracle tests and of the census exe), `gen_fakerw_*.py`, `lift_x87.py` (x87 -> C), `quat_unicorn.py`/`strip_unicorn.py` (case generators), `run_all_tests.sh`.
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
* **Regression note: `Failed: RwEngineSetVideoMode(-1)` right after `GcurSelSS=0` (fixed 2026-10-16, WinPs.cpp `psSelectDevice`).** NOT caused by a missing `gta_sa.set`. Cause: the host reports MORE THAN ONE display adapter
  (Wine on macOS with an external monitor attached, a Windows PC with two monitors), so `MultipleSubSystems` is true and the original code shows the "select device / video mode" dialog (`IDD_DIALOG1`, `DialogFunc` 0x745E50).
  The standalone exe has no such dialog resource, `DialogBoxParam` fails with -1 (non-zero, i.e. "success" to `!CreateVidModeSelectDialog`), the video-mode block (`!MultipleSubSystems` only) was skipped and `GcurSelVM` stayed -1.
  It looked like a settings problem because it appeared around the time soak.sh stopped symlinking the prefix's Documents, but the symptom does not depend on the file (it also failed with a valid `gta_sa.set` of mode 1): the Wine prefix cloned from `~/.wine-msvc` lists two adapters (`DISPLAY\APPA061` built-in + `DISPLAY\DEL4279` external monitor in its `system.reg`), the run log says `2 sub systems`.
  Fix (`NOTSA_STANDALONE_RUN` only): skip the dialog, keep the default sub system (adapter 0) and run the normal video-mode selection (saved mode if valid, else the default search). The log line `UseDefaultVM=.. MultipleSubSystems=.. IsVMNotSelected=.. DisplayVideoMode=..` after `GcurSelSS=` shows which branch ran; `N sub systems: no video mode dialog ...` appears on multi-adapter hosts.
  If the symptom comes back, check `MultipleSubSystems` first (more than one `DISPLAY\...` entry in the prefix `system.reg`).
