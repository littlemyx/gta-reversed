# WIN-TEST - run_all_tests.sh at PC24 and native soaks on the Windows host (2026-10-16/17)

Host: Windows 11, Ryzen 7 7700X, RTX 4080 (the only D3D9 adapter: `Direct3D9 GetAdapterCount() = 1`, adapter 0 = NVIDIA GeForce RTX 4080; the AMD iGPU and the Meta Virtual Monitor are not enumerated), desktop 3840x2160 X8R8G8B8.
Toolchain: see `.notes/reports/WINSETUP_TOOLS.md` (cl 19.51.36260 x86, toolset 14.51.36231, SDK 10.0.26100.0). Repo HEAD during the runs: e54e2cbe..6aa3b0e4. Oracle: repo-root `gta_sa_compact.exe` (sha256 72ae59e4...cecc9b).
Test build: `build/StandaloneRW` (configured and built by run_all_tests.sh, Debug, 715 ninja steps, `-j4` at BelowNormal on the second attempt).

## 1. run_all_tests.sh (PC24; checks / failures / mismatches)
43 `*_test` targets exist at HEAD (the Mac table of 2026-10-10 had 41; `audio_fade_oracle_test` is new, and some counts grew since). **38 green, 5 FAIL** (rw_custom_pipelines, rw_im2d_im3d, rw_platform, rw_skin_pipeline, rw_skin_vs); the device tests are only green when no game runs (see 1b). Non-device oracle tests: all 0 mismatches at PC24, same numbers as the Mac.

| test | Windows | Mac (TESTFIX, 10-10) | note |
|---|---|---|---|
| fixed_oracle | 16/0/0 | 16/0/0 | same |
| game_oracle | 62/0/0 (PC24 strict 926, all NaN-payload-only) | 44/0/0 | more rows since 10-10 |
| gameplay_oracle | 311/0/0 | 311/0/0 | same |
| logic_oracle | 224/0/0, 356 s | 224/0/0 | same |
| physics_oracle | 107/0/0 (PC24 strict 74 = NaN-payload-only, like the Mac) | 107/0/0 | same; 205 s, no crash |
| pool_oracle | 1,900,009/0/0 | same | same |
| review_oracle | 42/0/0 | 42/0/0 | PC24 strict 0 |
| world_oracle | 116/0/0 | 116/0/0 | same |
| x87_oracle | 67/0/0 | 67/0/0 | PC24 strict 0 |
| audio_fade_oracle | 6000/0/0 at PC=53; **PC=24: 499 informational last-bit diffs** | (AUDIO.md: PC53 0, PC24 1) | NEW test, runner had no parser (fixed e042b797). PC24 count 499 vs 1 on the Mac: the test calls PC24 informational (fade thread runs at PC=53), SetVolume sequences equal. Host difference in x87 PC=24 float rounding, not investigated further |
| script_oracle (base, g13_15, g16..g25_26) | 80/47/47/17/51/56/31/75/58/70/58, all 0/0 | same numbers | same |
| rw_atomic_clump, rw_matfx_uvanim, rw_math_stream, rw_rtanim_rtquat, rw_skin_cpu_oracle, rw_skin_hanim, rw_skin_hw_oracle, rw_strip, rw_world_geometry | 274, 151, 139, 4835, 240000, 166, 27800, 46, 166 - all 0/0 | same numbers | same (rw_skin_hw_oracle: 3000 render cases, 0 mismatches on the real GPU as well) |
| rw_engine | 101/0/0 | 93 | green on the real device |
| rw_frame_camera | 118/0/0 | 100 | green |
| rw_matfx_pipeline | 273/0/0 | 273 | green |
| rw_pipeline | 541/0/0 | 541 | green |
| rw_raster_image | 325/0/0 | 325 | green |
| rw_renderstate | 373/0/0 | 373 | green |
| rw_texture | 103/0/0 | 103 | green |
| **rw_custom_pipelines** | 135 / **2 FAIL** | 137/0 | `FAIL nEnv > 100 && Brightness(envOnly) > Brightness(plain)` and `Brightness(both) > Brightness(spec) && > Brightness(envOnly)`: infernus has 64 materials with an Env Map chunk, but "env 14449758 (0 px differ)" - the env-map pipeline changes no pixel vs plain on the RTX 4080 (spec path works: 7719 px differ). Real-GPU / driver difference, not analysed (not a skin-VS issue) |
| **rw_im2d_im3d** | 117 / **1 FAIL** | 117/0 | `FAIL abs((x1-x0) - 5*baseW) <= 4 ...` "tri LTM scale 5: bbox 77,51..227,150, 7600 px": rasteriser bbox of a scaled triangle differs by more than 4 px from the expectation on the real GPU (top-left / precision rule). Not analysed |
| **rw_platform** | 55 / **1 FAIL** | 55/0 | `FAIL NotsaRwPlatform_DeviceResetCount() == r0 + 2 && g_calls == c0 + 2` ("two resets in one showRaster"): on the native d3d9 the second Reset path is not taken the same way as under wined3d. Not analysed |
| **rw_skin_pipeline** | 49 / **3 FAIL** | 52/0 | `FAIL notsa::skinvs::GetVertexShader(k, lay) != nullptr`, then `diff01 > 50`, `coverDiff*100 <= covered` (pose 1: 92 of 933 px differ HW vs CPU route). Same root cause as rw_skin_vs_test (section 3) |
| **rw_skin_vs_test** | 8987 / **2-3 FAIL** | 8987/0 | text differential (5 models, ~2.2 M keys) and NumLightConstants (1.5 M keys) and cache: 0 mismatches. FAIL: (a) `device rejected 400 shaders` for every model, (b) `assembler bytecode: 8987 accepted by only one`. Root cause in section 3 |

### 1b. Host effect found while running them: a running game blocks every D3D test
`rw_*` tests that open a D3D9 device (frame_camera, im2d_im3d, matfx_pipeline, pipeline, platform, raster_image, renderstate, skin_pipeline, texture, engine, custom_pipelines, skin_vs device part) die with `d3ddevice.cpp:1657 CreateDevice() failed` (exit 139, SIGSEGV on the null device that follows; skin_vs: 0x88760868 D3DERR_DEVICELOST) whenever another agent has a `gta_*.exe` game instance running (D-B07V / BUG-QUIT soaks run back to back). While the game runs, the video-mode list also changes (R5G6B5 modes are listed as format 35 instead of 23). With no game present they pass at the first attempt (rw_frame_camera needed 16 attempts: a game kept starting in the gaps). So: **device tests and game runs are mutually exclusive on this PC**; `/c/gta-scratch/wait_nogame.sh` (waits until no `gta*` process exists for 40 s) is what I used; it is not committed (scratch).
The first full run also hit the wall: `run_all_tests.sh` ran ninja without `-j` (18 jobs) and froze the user's desktop; the coordinator killed it. Fix 9ee7bc21 (`-j${GTA_JOBS:-4}` + BelowNormal through `cmd /c start /low`).

## 2. Native soaks (StandaloneRelease at 6aa3b0e4 + ef481d59 Reset fix + e991c642 FxEmitter fix; exe sha256 d43ae60f...; `soak.sh build/StandaloneRelease N`; adapter 0 = RTX 4080; window 800x600 on the 4K desktop; MUTE on, `[mute]` lines present, silent)
The soak.sh RESULT line is FAIL on every run because of the known **BUG-QUIT** (quit menu ENTER -> YES kills the process with a heap-corruption fast fail, see D-B07V.md); errors / exceptions / markers are reported separately:

| run | requested | route | markers | errors / exceptions / game [error] | fps (limiter on) | peak committed | frames | RESULT line | notes |
|---|---|---|---|---|---|---|---|---|---|
| soak1 (old exe, before ef481d59) | 3 min | - | 0 | access violation at 0 in `ProxyReset` <- `showRaster` <- `ResetDeviceNow` <- `RwEngineStart` | - | - | - | FAIL | the Reset-slot bug D-B07V fixed in ef481d59; superseded |
| soak1b | 3 min (412 s route: 6 min fixed + 60 s idle) | | 21 of 22 (all up to quit_ask; `quit_sent` missing = BUG-QUIT) | 0 / 0 / 0 | 29.7-30.0 (menu 52-60) | 547 MB (heap 340 MB) | 12000 | FAIL (known BUG-QUIT) | all 43 screenshots rendered, CJ walks, car + Rhino cheat spawn |
| soak2 | 3 min | | 21 of 22 | 0 / 0 / 0 | 29.7-30.0 | 535 MB (heap 342 MB) | 11900 | FAIL (known BUG-QUIT) | same |
| soak3 / 3c / 3e (15 / 15 / 3 min) | | | not started | - | - | - | - | exit 2 (never started) | `window did not appear` x6: another agent's game was running (BUG-QUIT, `C:\gta-quit`); the game has a single-instance mutex (`IsAlreadyRunning`, WinMain.cpp:133) and the second instance returns from WinMain right after `InjectedHooksMain(): Finished` ("atexit handler ran") |
| soak3b, soak3d (15 min route, 900 s, idle 547 s) | 15 min | | 23, but the route misfired (6 gate timeouts: the menu clicks missed, the game stayed in the main menu, `quit_sent` reached with no effect: "QUIT NOT EFFECTIVE") | 0 / 0 / 0 | **12.7 constant, from the first menu frame** (soak1b/2 menu: 52-60) | 408-410 MB | 15500 (both runs, identical) | FAIL | **INVALID as a stability result**: the host display had changed (desktop was 3840x2160 for soak1b/2, 2560x1600 at soak3b/3d/3f, 800x600 whenever another agent's fullscreen game runs; the D3D9 mode list shrank 53 -> 49 modes), and the game ran at 12.7 fps in the menu, so the time-based route (mouse click on START GAME) never reached gameplay (`clocklog day=0`, screenshots show the "Game" menu). Probably the display/adapter routing (e.g. the monitor now on the iGPU, so the RTX 4080 windowed Present is copied across adapters) - not verified; not a port finding |
| soak3f (15 min) | | | killed by me after 5 min | | 12.7 | | | - | same display state as 3b/3d; stopped to free the PC |
**No 15-minute soak with a clean route was obtained**: soak1b and soak2 (3 min, 21 markers, 0 errors, ~30 fps, 12000 frames) are the valid native runs; they stop at the known BUG-QUIT. Re-run `soak.sh build/StandaloneRelease 15` when no other agent runs games and the display is back to its normal mode (the desktop mode is printed in `rw video modes: ... (desktop WxH ...)`; expect 52+ fps in the menu).

Frame limiter: the in-game limiter is on (30 fps cap in gameplay, ~53-60 fps in the menu); fps numbers are therefore the cap, not CPU headroom.

## 3. Game bug found by the tests (not fixed, outside my zone): native Windows d3dcompiler_47 rejects the shader-assembly flag 4
`source/standalone/rw/skin_vs.cpp` assembles the skin vertex shaders with `D3DAssemble` from `d3dcompiler_47.dll` and `kAssembleFlags = 4` (the exe passes 4 = D3DXSHADER_SKIPOPTIMIZATION to its static D3DXAssembleShader). Under Wine (vkd3d) the flag is ignored; on native Windows the call fails with D3DERR_INVALIDCALL (0x8876086C) and **no error blob**, so the port returns a null shader silently (no `skinvs:` line is logged: `errors` is null) and every skin vertex shader is "rejected".
Evidence (scratch program, x86): `D3DAssemble("vs_1_1\nm4x4 oPos, v0, c0\nmov oD0, c4\n", flags)`: flags 0 -> hr 0 (36 bytes), flags 1 -> ok (288 bytes, debug info), flags 2 -> ok (36 bytes), **flags 4 -> 0x8876086C, errors null**; `d3dx9_43!D3DXAssembleShader` behaves identically (the flag is not valid for the current D3DX either). The same flag is used by the assembler section of `tests/standalone/rw_skin_vs_test.cpp` ("smoke: d3dcompiler FAILED (0 bytes)").
Suggested fix (assigned by the orchestrator): pass 0 (or 2) on native Windows - the bytes for flags 0 and 2 are the same 36 bytes; then re-run rw_skin_vs_test (device + assembler bytecode vs the exe's D3DX) and rw_skin_pipeline_test. Consequence now: the skin-vertex-shader route is dead on native Windows (rw_skin_pipeline: HW route differs from the CPU route in pose 1); whether the game falls back to a CPU/other skinning path was not checked (the soak screenshots show rendered cars and the world; peds were not inspected).

## 4. Other findings / fixes in my zone
| commit | what |
|---|---|
| 9ee7bc21 | run_all_tests.sh: `ninja -j${GTA_JOBS:-4}` at BelowNormal on Windows (an unbounded ninja froze the desktop) |
| e042b797 | run_all_tests.sh: parse `audio_fade_oracle_test` summary (PC=53 mismatches; PC=24 informational) |
| 6aa3b0e4, 061d56b9 | soak.sh (Windows): every attempt starts without `Documents\GTA San Andreas User Files\gta_sa.set` (a saved video mode crashed the next start in `ResetDeviceNow`); a pre-existing file is backed up once (`.soak-bak`), the live file is deleted before each attempt and at exit. 6aa3b0e4 restored the moved-aside file at exit - that re-created the game-written file (and with it the saved-video-mode crash for other agents' direct starts), so 061d56b9 never restores. census.py runs soak.sh, so it is covered. The game-written copies were moved to `C:/gta-scratch/gta_sa_set_evidence`. |
| - | Editing run_all_tests.sh while it runs corrupts the running bash (syntax error at the end of the first re-run); I re-ran with SKIP_BUILD=1. Do not edit scripts under a running instance |
| - | A second game instance exits at once after `InjectedHooksMain(): Finished` with "atexit handler ran" (presumably a single-instance check; not verified): soak3's first try died this way because another agent's game was running (`window did not appear` x6) |
