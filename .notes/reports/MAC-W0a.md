# MAC-W0a: app/ + extensions/debug W diagnostics (portable macOS arm64 compile gate)

Branch portable/main, commit **3f46d831** (pushed to fork) + a report/tag commit. Census = `tools/standalone/clang_census.py --mode native` on `build/PortableMac/compile_commands.json`.

## Before -> after (unique diagnostics located in the file)
| file | before | after |
|---|---|---|
| app/app_debug.cpp | 15 | 0 |
| app/platform/GameplayRegress.cpp | 2 | 0 |
| app/platform/SDLWrapper.cpp | 8 | 0 (parent's GetTickCount64) |
| app/platform/win/VideoMode.cpp | 1 | 0 |
| app/platform/win/VideoModeSelectDialog.cpp | 22 | excluded TU |
| app/platform/win/VideoPlayer/VideoPlayer.cpp | 36 | excluded TU |
| app/platform/win/WinMain.cpp | 51 | 0 |
| app/platform/win/WinPlatform.cpp | 66 | 0 |
| app/platform/win/WinPs.cpp | 68 | 0 |
| extensions/debug.cpp / debug.hpp | 12 / 6 | excluded TU / 0 |
| extensions/Configs/FastLoader.hpp (VK_CONTROL) | 2 | 0 |
| game_sa/Gamma.h (D3DGAMMARAMP), ControllerConfigManager.h (DIJOYSTATE2) | 2 + 2 | 0 (PODs in my header) |
| new TUs portable/app_{main,platform,debug_posix,video_player,win32_compat}.cpp | - | 0 |
| **all of the above** | **~287** | **0** |

Full census at the end (tree incl. the parent's and other agents' uncommitted work): 1106 TUs, **906 clean** (base 845), unique 2590 (base 3027). No diagnostic mentions a symbol I introduced (VK_*, GetKeyState, RECT users, D3DGAMMARAMP, DIJOYSTATE2, __argc ...).
Not mine, still open near my zone: `extensions/Shapes/AngledRect.cpp:43` (pointer->uint32 cast, ILP32/P stream: `reinterpret_cast<uint32>(this)`), `game_sa/Plugins/BreakablePlugin/BreakablePlugin.h` (2 sizeof asserts), `game_sa/Core/Pool.h:17 #include <intrin.h>` (unguarded; resolves only through tools/standalone/winstub, a real build without it fails at StdInc), Audio/Hardware/AEDirectSoundMute.h (14, only reached by the excluded VideoPlayer.cpp).

## Excluded TUs (PORTABLE_EXCLUDED_APP_FILES, source/CMakeLists.txt line 163 - UNCOMMITTED, parent's file)
`set(PORTABLE_EXCLUDED_APP_FILES app/platform/win/VideoPlayer/VideoPlayer.cpp app/platform/win/VideoModeSelectDialog.cpp extensions/debug.cpp)`
Replacements: portable/app_video_player.cpp (VideoPlayer::* and CreateVidModeSelectDialog), portable/app_debug_posix.cpp (notsa::debug::GetFunctionInfoAtAddress / LoadSymbols / UnloadSymbols via dladdr + __cxa_demangle).

## Other uncommitted dependency (parent's file)
`source/app/platform/win/winincl.h` line 52 `#include "portable/app_win32_compat.h"` (right after the parent's winapi_compat.h include, inside the `#else` of `#ifdef _WIN32`: nothing on Windows). The committed files need it AND the CMake exclusion/glob above (the Windows builds must not compile source/portable/*.cpp): please commit both with/after 3f46d831.

## New portable-only files (source/portable/)
* app_win32_compat.{h,cpp}: VK_*, KF_EXTENDED, MAPVK_*, SW_*, LANG_*, PRIMARYLANGID, PODs D3DGAMMARAMP / DIJOYSTATE2, `__argc/__argv/__wargv`; functions with a straight SDL3 equivalent: GetKeyState / GetAsyncKeyState (SDL keyboard state, VK->scancode), GetClientRect (HWND == SDL_Window*), MapVirtualKey (logged stub). `notsa::portable::Unsupported(what)` = log once to stderr.
* app_platform.{h,cpp}: AcquireSingleInstance (flock), FindDvdGamePath (/Volumes/GTA_SAN_ANDREAS/), GetLangIds (SDL_GetPreferredLocales -> Win32 primary language id), QueryPhysMemory (sysconf + mach host_statistics64), GetNativeResolution / GetBestRefreshRate (SDL display modes), InstallCrashHandler (sigaction SEGV/BUS/ILL/FPE/ABRT/TRAP + backtrace through the game logger).
* app_main.cpp: `main(argc, argv)` sets __argc/__argv and calls the game's `WinMain` (NOTSA_STANDALONE); there was no process entry on non-Windows.
* app_video_player.cpp, app_debug_posix.cpp (see above).
Convention: on portable `PSGLOBAL(window)` is the `SDL_Window*` cast to HWND (WinMain.cpp), because SDL_PROP_WINDOW_WIN32_HWND_POINTER is null on macOS; `PSGLOBAL(sdlWindow)` is the same pointer.

## SDL3 paths that existed and were reused (not rewritten)
WinMain.cpp `NOTSA_USE_SDL3` (SDL_Init / SDL_CreateWindow / RaiseWindow, SDLWrapper::ProcessEvents), WinPs.cpp (`new psGlobalType`, SDL_SetWindowSize, SDL_WarpMouseInWindow in psMouseSetPos), VideoMode.cpp, SDLWrapper.cpp/.hpp. The Win32 window procedure / WinInput code stays compiled only `#ifndef NOTSA_USE_SDL3` (WndProc.cpp, WinInput.cpp were already clean).

## W-A items (stubs and assumptions; file:line at 3f46d831)
| id | where | what is missing / how it behaves | later slice |
|---|---|---|---|
| W-A1 | portable/app_video_player.cpp:10 | no movie playback; Play() logs once and returns, game waits in PLAYING_LOGO/INTRO for the skip input | movie player (SDL3 + libav/AVFoundation) |
| W-A2 | portable/app_video_player.cpp:25 | CreateVidModeSelectDialog logs, returns FALSE (not reached in NOTSA_STANDALONE_RUN psSelectDevice) | none needed |
| W-A3 | app/app_debug.cpp:192 (+ app_platform.cpp InstallCrashHandler) | crash dump is signal based: no register dump / raw stack scan; symbols via dladdr | optional |
| W-A4 | app/platform/win/WinMain.cpp:285 | anisotropy support assumed true (logged) instead of the D3DCAPS9 query | GL3 caps query (R) |
| W-A5 | WinMain.cpp standalone-run block (`_set_SSE2_enable` dropped, `_clearfp` -> feclearexcept) | x87-libm emulation switch has no arm64 meaning | stream F |
| W-A6 | portable/app_win32_compat.cpp:91 | MapVirtualKey returns 0 (logged); only reachable from the Win32 WndProc key path | none |
| W-A7 | portable/app_main.cpp:9 | `__wargv` stays null | none |
| W-A8 | app/platform/win/WinPlatform.cpp:22 | gamma ramp reset skipped (logged), flag cleared | gamma on GL3 (R) |
| W-A9 | app/platform/win/WinPs.cpp:251,254 | VRAM total/avail = 0 (logged); virtual memory figures = physical | GL3 / Metal query |
| W-A10 | WinPs.cpp:162,176 | no cpuid (gCpuVendor keeps "GTAReversed!"), OSVer = WIN_VISTA_OR_LATER | none |
| W-A11 | WinPs.cpp:340 | psGrabScreen returns nullptr (logged): no front-buffer read-back | GL3 glReadPixels (R) |
| W-A12 | WinPs.cpp:73 | CheckDirectSound returns TRUE (logged): sound device assumed | OpenAL device probe (audio slice) |
| W-A13 | portable/app_debug_posix.cpp:13 | no source line in symbolised frames | optional |

Dropped as Windows-only without replacement: SetProcessDPIAware, SPI_SETFOREGROUNDLOCKTIMEOUT / power-off / low-power / sticky-keys toggles, SetErrorMode, UpdateWindow, CoInitialize, the OS-version / DirectX / Win95 checks. Replaced by SDL3: screensaver (SDL_Disable/EnableScreenSaver), cursor (SDL_Show/HideCursor), message boxes (SDL_ShowSimpleMessageBox), window title, always-on-top, native resolution, refresh rate, minimized state.

## No-op proof for the Windows builds
* The commit is insertions only (13 files, +909 / -0): no existing line was edited or removed.
* Every added block is `#ifdef/#ifndef NOTSA_PORTABLE` (or in portable-only files). To keep `__LINE__` (ZoneScoped, SPDLOG, assert) and debug line tables identical, `#line N // W0a` directives restore the original numbering after each guard (tools/portable/w0a_guardlines.py `fix`). `check` (run per file, all OK) compares the Windows view (NOTSA_PORTABLE undefined) with `git show <base>:file`, text AND numbering: debug.hpp, app_debug.cpp, WinMain.cpp, WinPlatform.cpp, WinPs.cpp (re-verify: `w0a_guardlines.py check FILE --base 3f46d831~1`).
* Wine/MSVC A/B (parent's): suggested TUs WinPs.cpp, WinMain.cpp, WinPlatform.cpp, app_debug.cpp, and any TU including extensions/debug.hpp. Expected: identical object code.
* The CMake variable line and the winincl.h include sit in Windows-inactive branches.

## Open / continue
* None blocking in my zone. Parent: commit CMakeLists.txt (exclusion line 163 + portable-only glob) and winincl.h with/after 3f46d831.
* The census is syntax only: link-level gaps are untested (`WinMain` vs `main` entry, symbols the excluded TUs defined for other TUs).
* tools/portable/w0a_guardlines.py is generic: any later stream editing Windows-hot files can use it to keep line numbers.
