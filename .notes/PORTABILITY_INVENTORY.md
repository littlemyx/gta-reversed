# Portability inventory (counts only), 2026-10-10, HEAD 6fafc75a, branch reverse/interior_c
Streams as in `.notes/PORTABILITY_GOAL.md`. Stream D = `.notes/DETACH_DATA_PLAN.md` (38 slices). Counts = non-comment source lines in `source/` excluding `fakerw/` and `libs/`
(grep census, regexes in the label; "files" = files containing at least one hit). Tree: 1,133 .cpp + 1,326 headers, 518,265 lines.
Slice = one sonnet agent (+ one review pass). Heuristics are marked "h".

## A) Fixed addresses that must compile out
| item | uses | files | biggest files | note |
|---|---|---|---|---|
| `StaticRef<>` | 2,811 (2,755 distinct addresses) | 372 | ModelIndices.cpp 164, Cam.cpp 115, TheScripts.h 83, PostEffects.h 82, TimeCycle.h 71, Camera.cpp 70, Hud.h 68 | stream D converts all (20 codemod batches) |
| `ScopedStaticRef` | 3 | 3 | | D batch B20 |
| raw `(T*)0xADDR` / `*(T*)0xADDR` / `reinterpret_cast<T*>(0xADDR)` | 23 | 16 | Pickups.cpp, RenderWare.h, TimeCycle.cpp, Matrix.cpp | D C-slices / F1 |
| `plugin::Call*<0x..>` / CallMethod | 54 | 17 | Collision.cpp 15 (test-only), MemoryMgr.cpp 9, rwtexdict.cpp 8 (excluded), WindowedMode.cpp 8 (excluded) | live in run build ~9 |
| `RH_Scoped*` hook registration lines | 11,275 | 896 | Vehicle.cpp 171, Ped.cpp 152, Camera.cpp 122, CarCtrl.cpp 113, AEVehicleAudioEntity.cpp 109, Automobile.cpp 105 | macro-level compile-out, 0 file edits; 1,812 `InjectHooks()` refs |
| `RH_ScopedVirtualClass` (exe vtable addresses) | 530 | | | compile-out |
| `NOTSA_STANDALONE_RUN` / `NOTSA_STANDALONE` ifdefs | 80 | 32 | WinMain.cpp, Fixups, DataImage | collapse |
| `ORIGINAL_EXE` / `original_data` / `data_pointers` references | 11 | 3 + CMake/presets/scripts/docs (9 items, DETACH 1.1) | | D G1 |
| exe-VA image machinery | DataImage.cpp 202 l, Fixups.cpp 627 l, make_orig_pad.py, `/BASE:0x400000 /FIXED` | | | D F3 (x86-Windows only by construction) |
| `uintptr`/`uintptr_t` address params in hook templates (`PluginBase.h`, `RHManager.h`) | 90 (h) | | | stay compiled only in ASI/oracle mode |
Slices A (beyond D): A-1 `GTASA_PORTABLE` CMake option + macro-level compile-out of `RH_*`/`plugin::Call*`/`InjectHooksMain` (build time and object size, 1,812 InjectHooks bodies stay compiled but unregistered or `#if`-excluded); A-2 `plugin::Call` + raw cast residue (54 + 23 sites); A-3 split `ReversibleHooks`/`toolsmenu` hook UI behind `#if !PORTABLE`; A-4 `dllmain`/`WinMain`/`PluginBase.h` (thiscall member-call trick) split. = 4.

## P) 64-bit risks
| item | count | files | biggest | note |
|---|---|---|---|---|
| `VALIDATE_SIZE(` | 804 (795 distinct types) | 636 | | all assume 4-byte pointers |
| `VALIDATE_OFFSET(` | 21 | 8 | | |
| `static_assert(sizeof...` | 38 | 30 | | |
| size-validated types with direct pointer members (h) | 334 | | game_sa/Tasks 101, game_sa root 69, Events 48, Audio 36, Entity 17, Animation 10, Core 9, Fx 9 | in-memory only: macro -> `VALIDATE_SIZE_X86` (guard by `sizeof(void*)==4`) |
| size-validated polymorphic types (own `virtual`) (h) | 43 (+ all derived, vptr = 8 B) | | | |
| size-validated types that are ALSO file/stream layouts | ~40 (h) | ~15 | ColHelpers.h (15 asserts: FileHeader, V1..V3 Header), 2dEffect.h/.cpp, BreakablePlugin, AnimManager.cpp (26 `RwStreamRead`), PathFind.cpp (11), Directory.h, ColAccel.h, Text.h, ReplayBlockType.h, IplDef.h | must become fixed-width (`uint32` handles/offsets, no pointers) |
| raw struct reads: `CFileMgr::Read`/`fread` | 69 | 33 | ColAccel.cpp 9, Text.cpp 7, FileMgr.cpp 6, ControllerConfigManager.cpp 6, Replay.cpp 5, AEUserRadioTrackManager.cpp 5 | |
| `RwStreamRead(...sizeof)` | 93 | 19 | AnimManager.cpp 26, 2dEffect.cpp 12, PathFind.cpp 11, BreakablePlugin.cpp 10 | |
| raw struct read/memcpy with `sizeof` | 66 (h) | 10 files with direct struct reads: Streaming.cpp, Directory.cpp, C_PcSave.cpp, BreakablePlugin.cpp, AnimManager.cpp, CustomBuildingDNPipeline.cpp, 2dEffect.cpp, Text.cpp, MissionTextOffsets.cpp, Text/Data.cpp | |
| save-game structs (work-buffer save/load) | ~440 refs (h) | 40 files use `CGenericGameStorage` | TheScripts.cpp 64, AERadioTrackManager.cpp 64, GenericGameStorage.cpp 63, Restart.cpp 36, Garages.cpp 22, Pools.cpp 20, TheZones.cpp 18, Stats.cpp 16 | save format decision: keep the 32-bit file layout |
| script: `tScriptParam` union (`void* pParam`, `char* szParam`, VALIDATE_SIZE 4) | 4 uses (`pParam`) | 3 | | + script handles = pool refs (`GetRef/GetAtRef` 161 uses, int32 OK); script space is a byte array (`ScriptSpace`, SCRIPT_SPACE_SIZE) read with `reinterpret_cast<T&>`; `CRunningScript` IP/stack are pointers |
| pointer<->int casts (`(int32)&x`, `(DWORD)this`, `reinterpret_cast<u?int32>`) | 24 + 31 + 3 | 13 + 17 + 2 | Entity.cpp 11, Object.cpp 9, TrafficLights.cpp 2, TaskComplexStareAtPed.cpp 2 | |
| `pad[...]` members (layout padding assumptions) | 33 | 18 | | |
| `sizeof(void*)` conscious code | 2 | | | |
Slices P: P-1 `VALIDATE_SIZE_X86` split + conditional (mechanical, 334 pointer types: Tasks 101 / root 69 / Events 48 / Audio 36 / rest 80) = 5; P-2 file-layout types to fixed-width + `ReadStruct` helpers (col, 2dfx, breakable, anim, path, directory, text, replay, IPL, audio banks, img) = 4; P-3 save-game layout (40 files) = 3; P-4 script params/handles/IP + pointer<->int casts (58) = 2; P-5 pools/`CStore`/`CLink` `uint32` handle audit + `alignas` (84 `alignas`) = 1. Total 15.

## F) x87-specific
| item | count | files | biggest | note |
|---|---|---|---|---|
| `x87::sin/cos/tan/atan2/asin/acos/atan/sqrt` calls | 685 (sin 265, cos 229, atan2 115, asin 39, tan 15, acos 13, sqrt 8, atan 1) | 110 | Cam.cpp 142, Automobile.cpp 38, Camera.cpp 23, Bike.cpp 18, Vehicle.cpp 17, Birds.cpp 16, Radar.cpp 15, TaskSimplePlayerOnFoot.cpp 14 | `X87Intrinsics.h` (naked fsin/fcos/fpatan/fyl2x/fprem wrappers; has an `std::` fallback branch, lines 164-175, NOT bit-exact) |
| `X87SinP1Half`, `X87DotProduct` | 3 + 2 | | | |
| `ExeRecip*` constants | 254 | 100 | Automobile.cpp 23, ReplayBlockType.cpp 12, AEVehicleAudioEntity.cpp 10, Fx.cpp 8 | already portable (constants) |
| `Ftol`/`_ftol2` (float->int exe semantics: NaN/inf -> 0, wrap) | 118 | 67 | FtolExe.cpp 16 (naked), Radar.cpp 15, Group05_08.cpp 11, PathFind.cpp 9, Renderer.cpp 8, Heli.cpp 8 | portable helper with the same truncate/low-dword semantics |
| inline asm / `__declspec(naked)` | 66 asm lines in 17 files / 22 naked in 5 | | WindowedMode 21 (excluded), X87Intrinsics 12, Matrix.cpp 10, FtolExe 7, Radar.cpp 5, Group13_15.cpp 4, PathFind.cpp 3, AEAudioUtility 1 | live: ~35 lines in 6 files |
| `(double)` casts in game code | 5,028 | 189 | | x87 PC=24 rounds every op to float; `double` intermediates are NOT rounded on SSE2/arm64 -> per-op float rounding needed for record/replay equality |
| `double` declarations | 2,108 | | | |
| libm calls `std::sin/cos/sqrt/pow/...` | 625 (+20 `sinf`-style) | | | arm64 libm differs from x87 libm in last bits |
| fp environment setup | `_controlfp/_control87/_PC_24` 5 in 3 files, `_set_SSE2_enable` 1, `_clearfp` 1, `/arch:IA32 /fp:precise` CMake | | Physical.cpp 3, FtolExe.cpp 6, WinMain.cpp 2 | |
| `rand()`/`srand`/`GetRandomNumber` | 221 / 793 refs | | GameRand.h (MSVC LCG already own code) | portable already |
| lifted x87 code (RW) | camera_sync_lifted.inc 126,915 B, pipeline_skin_cpu_lifted.inc 89,672 B, pipeline_skin_lifted.inc 13,433 B | 3 | | uses MSVC x87 semantics via `rwmath_exact.h`; check portability |
Slices F: F-1 software x87 `fsin/fcos/fptan` bit-exact (265 + 229 + 15 uses) + differential test vs exe oracle = 2; F-2 `fpatan/fyl2x/fprem/asin/acos` exact + sqrt/rounding modes (115 + 39 + 13 + 8) = 1; F-3 `Ftol` portable helper (118/67 files) = 1; F-4 inline asm -> C++ (Matrix, Radar, Group13_15, PathFind, AEAudioUtility, FtolExe) = 1; F-5 per-op float rounding: replace `double` intermediates by `float` with identical op order, 189 files / 5,028 casts in ~25-file batches = 8; F-6 libm substitution (625 calls, correct-rounded or x87-matching sin/cos table) with record/replay hash check = 2. Total 15.

## W) Win32 / DirectX users per subsystem
| subsystem | hits | files | biggest files (hits) | portable counterpart present |
|---|---|---|---|---|
| Window/main loop/input (app/platform/win: WinMain 613 l, WndProc 297, WinPs 640, WinInput 437, VideoMode*, WindowedMode 1,900 (excluded w/ librw)) | `HWND/HINSTANCE/HDC/MessageBox/WndProc` 80 | 31 | VideoModeSelectDialog.cpp 11, CdStreamInfo.h 9, WndProc.cpp 7, WinPs.cpp 7, WinMain.cpp 7 | `SDLWrapper.cpp` 316 l (SDL3) partial, `oswrapper` (win only, 6 hits) |
| `#include <windows.h>`-family | 14 | 14 | | `winincl.h` |
| DirectSound / EAX (+ DSBUFFER) | 229 | 35 | AEAudioChannel.cpp 57, AEStreamingChannel.cpp 53, AEStaticChannel.cpp 40, AEAudioHardware.cpp 28, AEAudioChannel.h 9 | `Audio/OpenAL/` 6 files (OALBase/Buffer/Source), `USE_OPENAL` in 17 files |
| Audio decoders (Media Foundation/WMA/MP3) | `__stdcall` 47 in 24 files (AEMFDecoder.cpp 6, AEPedSpeechAudioEntity 8) | | AEMFDecoder.cpp, AEWMADecoder.cpp, AEMP3*Loader | none |
| DirectInput | 24 | 5 | WinInput.cpp 10, Pad.cpp 2 | SDL3 gamepad/keyboard/mouse in SDLWrapper (partial) |
| DirectShow video (VideoPlayer) | 14 | 4 | VideoPlayer.cpp 12, WndProc.cpp 7, WinMain.cpp 6 | none |
| registry | 16 | 1 | rwplcore.h (excluded w/ librw) | n/a |
| threads / sync (CreateThread, CRITICAL_SECTION, events, mutex, Interlocked) | 33 | 12 | CdStreamInfo.cpp 7, MemoryMgr.cpp 6, Fixups.cpp 4, oswrapper_win.cpp 4, AEStreamThread.cpp 4, AESmoothFadeThread.cpp 2 | `std::thread`/`std::mutex` partly |
| file system (FindFirstFile, GetModuleFileName, CreateFile, GetCurrentDirectory) | 11 | 5 | FileMgr.cpp 6 | |
| backslash path literals | 69 | | | case-insensitive lookup needed on Linux/mac |
| MSVC CRT names (`_stricmp/stricmp/strcpy_s/_strlwr/sprintf/_mkdir/...`) | 242 | | | |
| time (`GetTickCount/QueryPerformance/timeGetTime`) | 15 | 7 | | |
| `LoadLibrary/GetProcAddress` | 12 | 6 | | |
| D3D9 outside the RW layer | 497 (all D3D9 refs) | 45 (30 outside rw/ and WindowedMode) | see R | |
Slices W: W-1 window/main loop/events on SDL3 (WinMain/WndProc/WinPs/VideoMode) = 3; W-2 input (WinInput/Pad -> SDL) = 1; W-3 audio hardware layer on OpenAL (4 files, ~180 hits) = 3; W-4 decoders (MF/WMA/MP3/streams) portable = 2; W-5 video playback (DirectShow -> ffmpeg/libvlc-less decoder or skip) = 2; W-6 threads/CdStream/async IO = 2; W-7 file system, paths, user files, CRT names (69 + 242 + 11) = 3; W-8 time/misc/LoadLibrary = 1. Total 17.

## R) D3D9-only render code outside librw
| item | count | files | note |
|---|---|---|---|
| game custom pipelines (`game_sa/Pipelines`: CustomBuilding x3 .cpp, CustomCarEnvMap x4 .cpp) | 1,898 lines | 12 | D3D refs: CustomCarEnvMapPipeline.cpp 83, CustomBuildingDNPipeline.cpp 49, CustomBuildingPipeline.cpp 46; 4 pipelines: building DN, car env-map, specular, env-atm |
| shim over librw D3D9 backend (`source/standalone/rw`, 18,417 lines) | D3D refs: engine.cpp 43, pipeline.cpp 35, pipeline_matfx.cpp 31, renderstate.cpp 29, skin_vs.cpp 21, camera.cpp 19, rwd3d_ff.cpp 22, raster.cpp 22, pipeline_skin.cpp 20 | 30 files | |
| vertex shader code | `skin_vs.cpp` (11 shader refs; HW skin VS composed from exe keys, ~2.2 M keys tested) , pipeline_skin.cpp 3, pipeline_skin_core.h 3 | 6 | librw has `src/gl` (gl3pipe, gl3skin, gl3matfx, gl3shader) as the replacement |
| RW calls into D3D9 state (`_rwD3D9*`, `RwD3D9*`, RxD3D9) | 958 | 41 | rwcore.h 372 (excluded w/ librw), WindowedMode 147 (excluded), pipeline.cpp 123 |
| `GetD3DDevice`/vertex declarations in game code | 8 | 5 | AEAudioHardware 5 (device ref), Gamma.cpp 3 (gamma ramp) |
| pixel formats / DXT / palette in game or shim | not counted | | raster.cpp, texdict.cpp |
Slices R: R-1 librw GL3/GLES backend in CMake + shim `engine/platform/camera/raster/renderstate` swap (D3D9 refs 43+35+29+22+22+19) = 3; R-2 skin pipeline (HW VS -> GL3 skin, CPU skin fallback exists) = 2; R-3 matfx + env map + specular pipelines = 3; R-4 building day/night pipeline + CustomBuilding = 2; R-5 Im2D/Im3D, textures (DXT/palette), render-state coverage, gamma = 2; R-6 pixel-diff harness vs D3D9 reference screenshots = 1. Total 13.

## C) MSVC-only constructs
| item | count | files | biggest | note |
|---|---|---|---|---|
| `__thiscall` / `__fastcall` | 6 / 1 | 6 / 1 | FxManager.cpp 6, PluginBase.h 4 | `plugin::CallMethod` trick (A-4) |
| `__stdcall` | 47 | 24 | AEMFDecoder.cpp 6, Cam.cpp 4, AEPedSpeechAudioEntity 8 | callback typedefs |
| `__cdecl` | 904 | 28 | | harmless on x64/arm64 (ignored warning), noise |
| `__declspec(...)` | 30 | 10 | naked 22 in 5 | |
| inline `__asm` / naked | 66 / 22 | 17 / 5 | see F | |
| SEH `__try/__except` | 2 | 1 | | |
| `#pragma pack/comment/warning/section` | 34 | 13 | rwplcore.h 6, ReplayBlockType.h 4, DataImage/pipeline.cpp | pragma pack on file-layout structs matters (P) |
| `__forceinline` / `__debugbreak` / `_ReturnAddress` / `<intrin.h>` / `_mm_*` | 1 / 69 total intrin | 7 | pipeline_skin_core.h 37, pipeline_skin_cpu_core.h 23 (SSE kernels), Base.h 3 | SSE kernels need NEON/portable path |
| MSVC CRT names + `\\` paths | 242 + 69 | | | W-7 |
| C++23/20 library features (`std::format`, ranges, `<print>`...) | 221 | | | needs libc++ 17+/GCC 13+ |
| `/Zc:preprocessor`, `/arch:IA32`, `/MT`, `/Z7`, conan/vcpkg toolchain | CMake + conanfile | | | CMake: clang/gcc flags |
| `windows.h` types (DWORD/BOOL/HRESULT/GUID) in game headers | via `winincl.h` 14 includes | | | portable shim header |
Slices C: C-0 first clang (mac arm64) compile census of 1,133 TUs, error table by directory = 1; C-1 calling conventions / declspec / pragma / intrinsics (~100 sites, 30 files) = 1; C-2 Windows type shim header + CRT-name shim (`_stricmp` ...) = 1; C-3 SSE kernels in `standalone/rw/pipeline_skin_*` portable/NEON = 1; C-4..C-9 compile-fix passes by source tree (game_sa root, Entity/Tasks/Events, Audio, Scripts, Models/Collision/Fx, app/standalone) = 6. Total 10.

## Summary (slice counts, one sonnet agent each)
| stream | slices | depends on |
|---|---|---|
| D detach data image | 38 (28 if codemod batches merge in pairs) | - |
| A no fixed addresses | 4 | D phase A-B |
| P 64-bit | 15 | independent, start now (P-1/P-2 before first arm64 run) |
| F float / x87 | 15 | needs reference record/replay harness (outside this list); F-5 is the long pole (189 files) |
| W platform | 17 | A-1 |
| R render | 13 | W-1 |
| C compilers | 10 | C-0 first, after A-1 |
| total | 112 |  |
Critical path to a first native macOS arm64 run (menu only): D (A,B,F,G) -> A-1 -> C-0..C-9 -> W-1, W-2, W-7 -> R-1; gameplay fidelity: + P, F.
