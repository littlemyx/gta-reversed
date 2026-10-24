# MAC-WIN64: first Windows x64 build of the portable game (milestone 5, last target) - 2026-10-24

Branch portable/main, commits a79f1101 and the follow-up (crash-handler diagnostics, WITH_OPENAL switch, this report). Built on the Mac with MSVC 19.51 x64 under msvc-wine/Wine Staging.

## Result
| item | state |
|---|---|
| Compile of all TUs of `gta_reversed` (1129 ninja steps, `-k 0`) | DONE, 0 errors (first full pass: 18 TUs failed, 67 unique diagnostics, see classes) |
| Link | DONE: `build/PortableWin64/bin/gta_reversed.exe`, 19.4 MB, PE32+ x64 GUI (machine 0x8664), + .pdb/.map. Incremental relink ~20 s |
| Cold wall time | conan deps (SDL3, spdlog, imgui, tracy, ogg, vorbis, jpeg-turbo, openal-soft) ~15 min under wine; compile+link of the game ~10 min with -j15 |
| Run under wine (optional) | exe starts, reaches WinMain, SDL3 window created (800x600), script handlers (0 of 2639 missing), CdStream opens models\gta3.img, video mode chosen; then null-function-pointer call at `NotsaRwGetGlInfo+0x20c` (engine_gl3.cpp:152) inside `RwEngineStart`: librw's `gladLoadGLLoader` result is dropped by `rw::Engine::start()`, so under wine's macOS GL the GL3 entry points (glBindFramebuffer...) stay null. Not chased (render under wine is out of scope); on real Windows the same call goes through a real wgl. The new Windows crash filter logged it (see below) |
| Win32 x86 StandaloneRelease unchanged | compile_commands: 1284/1284 TUs identical before/after; build.ninja identical but the RERUN_CMAKE dependency line. cl.exe x86 under wine, `obj_code_sig.py` (code + relocation names, also with `data`): 8/8 identical (PedAttractor, AEUserRadioTrackManager, GroupEventHandler, InteriorManager_c, app_debug, WinMain, AESound, AESoundManager). PortableMac syntax pass of 14 touched TUs OK |

## Toolchain route (msvc-wine, x64)
* `~/tools/msvc` already had `bin/x64` wrappers, `lib/x64`, `kits/10/Lib/*/{um,ucrt}/x64` from the 2026-10-08 install (no new download, no license step needed). `bin/x64/cl` already filtered /Zi -> /Z7.
* `bin/x64/wine-msvc.sh` needed the same `@rsp` patch as x86 (expands response files to `z:` paths; link line > 32 KB): `tools/portable/win64/wine-msvc-rsp.patch`.
* `tools/portable/win64/toolchain-msvc-wine-x64.cmake` (SYSTEM_PROCESSOR AMD64, pointer size 8, lib dirs x64, wrappers bin/x64; root from $MSVC_WINE_ROOT or ~/tools/msvc), `conanprofile-wine.txt` (includes `conan/profiles/windows-x64-msvc.txt`: arch x86_64, msvc 195, static CRT, C++23, sdl libusb off), `conan_install_win64.sh` (scratch-copy approach of the mac/linux scripts).
* Wine-route pitfalls found (all in the wine profile, not needed natively): (1) `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` - CMake's try_compile links an exe and its `mt.exe /notify_update` exits 0x41020001 which wine truncates to 1 = every configure check fails ("No 16 bit type" in ogg); (2) `libjpeg-turbo/*:SIMD=False` - macOS nasm reads the quoted `-I"..."` entries of ninja's rsp literally (jsimdext.inc not found); (3) `CMAKE_NINJA_FORCE_RESPONSE_FILE=ON` as for x86.
* Preset `PortableWin64` (CMakePresets.json): Ninja, Release, `GTASA_PORTABLE=ON` + STANDALONE + STANDALONE_RELEASE, SDL3, toolchain `build/PortableWin64/generators/conan_toolchain.cmake`; no `condition` (usable natively and via wine). Use: `tools/portable/win64/conan_install_win64.sh; cmake --preset PortableWin64; ninja -C build/PortableWin64 -k 0`.

## Design decision: Windows + NOTSA_PORTABLE
Real Windows SDK headers, NOT source/portable/winshim. The portable platform shims were already `#ifndef _WIN32` (wintypes.h, crt.h, winapi_compat.h, app_win32_compat.h), so the guards "!_WIN32 == portable" were mostly right already. What stays shared with mac/linux: SDL3 app layer (app_window/pad/video_player/present), librw GL3 render (portable/render), hook registry out, FileRecord 64-bit guards, x87 lifted branches (no inline asm is compiled on x64: every `__asm` site is `_M_IX86`-guarded or on the portable branch). What is Windows-native instead of the POSIX/OpenAL stand-ins: oswrapper_win.cpp, overlapped CdStream (CdStreamInfo.cpp), Media Foundation/WMA decoders, extensions/debug.cpp (DbgHelp), and **real DirectSound** for the audio hardware layer (`USE_DSOUND` + `dsound.lib`; portable/dsound_al.cpp is written against winshim/dsound.h abstract types that collide with the SDK's, so it is not built on Windows).

## Error classes (first full pass) and fixes
| class | count | fix |
|---|---|---|
| `fakerw/d3d9_lite.h` enums vs real `d3d9types.h` pulled by `StdInc.h` (`<d3d9.h>`, `<dinput.h>`); librw `rwd3d.h` also rejects `<d3d9.h>` (`#error`) and redefines D3DFMT/D3DLOCK | 10 + 7 | StdInc.h: `<d3d9.h>` only when `!NOTSA_PORTABLE`; `<dinput.h>` stays (DIJOYSTATE2 in ControllerConfigManager); `D3DGAMMARAMP` defined in winincl.h (portable+win only) |
| shims that exist only `#ifndef _WIN32`: `notsa::portable::Unsupported` (7 sites), `MainImageRange` (audio_portable.h, 4), `strcasecmp`/`setenv`/`<strings.h>` (4) | 15 | Unsupported + MainImageRange get Windows twins (PE SizeOfImage) included from winincl.h under NOTSA_PORTABLE; app_win32_compat.cpp keeps only Unsupported on Windows; `portable/posix_compat_win.h` (`_stricmp`, `_putenv_s`) |
| LLP64 narrowing in braced init (`size_t`/`ptrdiff_t` -> int32/uint32; MSVC-compiled game code that was fine at 32-bit): PedAttractor:240, GroupEventHandler:336/353, InteriorManager_c:278, AEUserRadioTrackManager:368 | 7 | explicit casts (no-op on x86: obj code identical) |
| POSIX-only TUs/headers in source/portable (app_platform.cpp: flock, signal, mach; path_resolve.cpp: dirent; app_main.cpp `main`; app_debug_posix; link_aliases; statehash; cdstream/oswrapper/decoders/dsound_al) | link-time + preventive | Windows branches in app_platform.cpp (named mutex, GetLogicalDrives CD scan, GlobalMemoryStatusEx, `SetUnhandledExceptionFilter` with x64 RIP/stack-return-address dump), path_resolve.cpp (pass-through: Windows FS is case-insensitive, CRT text mode is native), excluded the rest on WIN32 in source/CMakeLists.txt |
| link: `notsa::debug::LoadSymbols/UnloadSymbols` (debug.cpp excluded for portable) | 2 | debug.cpp kept on Windows; debug.hpp guards `NOTSA_PORTABLE && !_WIN32` |
| CMake: `-fms-extensions -mms-bitfields ...` clang flags; `/arch:IA32` (x86 only); `SUFFIX ""`; winshim include dir; `/SUBSYSTEM:WINDOWS` | - | MSVC branch (`/MP /Zc:preprocessor /w /EHsc /bigobj`), `/arch:IA32` and the `/arch:SSE2` CollisionPrimitives override only when pointer size is 4, `.exe` suffix + subsystem on WIN32, no winshim on WIN32, `Winmm dxguid ole32 ...` libs |
Not hit (checked by the build): inline asm / `__declspec(naked)` (all guarded), intrinsics, `operator new` signatures, NOTSA_PTR_ID32 / FileRecord width guards (`NOTSA_FILE_SCALAR_STRICT` is ON and passes with LLP64 `long`), `static_assert(sizeof)` layout checks.

## Open items
1. GL under wine: `Engine::start` ignores a failed `gladLoadGLLoader`; `NotsaRwGetGlInfo`/LogGlInfo should bail out when the loader failed (also a clean error on real drivers without GL 3.3). Needs a real Windows run to tell wine from code.
2. Audio on Windows = real DirectSound (original code path). The OpenAL path (`portable/dsound_al.cpp`) is mac/linux only; porting it needs the SDK's IDirectSound8 interfaces instead of winshim/dsound.h. openal-soft is built by conan only with `WITH_OPENAL=1` (not linked).
3. `source/portable/statehash.cpp` (RR harness) is excluded on Windows (mach_msg, dsound_al); `GTASA_STATEHASH` is not supported there.
4. Top-level CMakeLists: `CMAKE_GENERATOR_PLATFORM Win32` for non-Ninja MSVC generators would break an x64 VS-generator configure (Ninja only for now).
5. Not run natively on Windows; compiler version 195 in the conan profile = VS 2026 toolset (windows-latest with VS 2022 needs `-s compiler.version=194`).
6. -w hides all warnings in the portable MSVC build (as -w for clang).

## CI notes for a future windows-latest workflow (portable-windows.yml)
* Runner: windows-latest/-2025 with the x64 environment (`ilammy/msvc-dev-cmd` arch x64, or the default x64 shell of a "x64 Native Tools" step), python, `pip install conan cmake ninja`, `conan profile detect`.
* Steps: checkout (librw private: same blocker as LNX1, `tools/portable/ci_fetch_librw.sh` + LIBRW_DEPLOY_KEY) -> `bash tools/portable/win64/conan_install_win64.sh Release` (PROFILE=native picks conan/profiles/windows-x64-msvc.txt; override `CONAN_EXTRA_ARGS="-s compiler.version=194"` for VS 2022) -> `cmake --preset PortableWin64` -> `cmake --build build/PortableWin64 -- -k 0` (log + error count as in portable-linux.yml) -> upload `build/PortableWin64/bin/gta_reversed.exe` + .pdb. Cache `~/.conan2` (SDL3 is the long pole), optionally sccache (`GTASA_COMPILE_CACHE`).
* Existing Windows workflows (x86 ASI/StandaloneRelease) trigger on every push and are untouched.
