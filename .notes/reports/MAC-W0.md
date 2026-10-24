# MAC-W0: platform layer for the compile gate (SDL3 / OpenAL, Win32-only code out of the portable build)

Branch portable/main. Commits: 3f46d831 + 57b5a8f9 (W0a sub-agent, app/), 79cbd8c5 + 550597e7 (W0, this agent). Sub-reports: MAC-W0a.md (app/), MAC-W0b.md (no-op proof). Syntax pass only, no link.

## 1. What the SDL3 / OpenAL paths covered
| path | state at start | result |
|---|---|---|
| NOTSA_USE_SDL3 (WinMain, WinPs, VideoMode, VideoPlayer, SDLWrapper, Pad, ControllerConfigManager) | already ON in PortableMac; reduces Win32 window/input code but the Win32/D3D9/DDraw/DSound/DirectShow remainder was still compiled in the SDL3 variant (269 W in app/) | W0a guards the rest under NOTSA_PORTABLE, SDL3/POSIX replacements in portable/app_*.cpp |
| USE_OPENAL (forced ON when not WIN32) | only a skeleton OALBase/OALBuffer/OALSource that never included <AL/al.h> and had ownership type errors (`vector<unique_ptr<T>, allocator<T>>`); NOT wired into CAEAudioHardware (whole hardware layer is DirectSound) | headers/link fixed (find_package(OpenAL CONFIG), brew openal-soft); skeleton compiles. Real OpenAL channels = W-3/W-4 (run slice) |

OpenAL route: Homebrew `openal-soft` (keg-only, has OpenALConfig.cmake, headers <AL/al.h>; found via HINTS /opt/homebrew/opt/openal-soft). macOS OpenAL.framework not used (deprecated, no EFX/ALC ext). Conan alternative: add openal-soft + CMAKE_PREFIX_PATH, same find_package. SDL3: conan `sdl/3.4.14` (already in conanfile).

## 2. Census (PortableMac, brew llvm, unique diagnostics), before (A1b/base) -> after
| item | before | after |
|---|---|---|
| TUs clean / total | 845 / 1105 | 945 / 1106 |
| unique diagnostics | 3033 | 1998 |
| W platform (all) | 915 | 3 (+ 29 DirectX-classified W/R, all stream R) |
| W by directory: game_sa/Audio | 527 | 0 (2 "W" are AEAudioUtility x87 asm = F, not W) |
| source/app | 269 | 0 |
| source/oswrapper | 46 | 0 (TU excluded, portable/oswrapper_portable.cpp 0) |
| game_sa root (CdStreamInfo 39, FileMgr 15, ...) | 61 | 0 |
| source/extensions | 6 | 0 |
| source/portable (new, 8 TUs) | - | 0 |
Remaining W (not my zone): standalone/rw/{engine,camera,platform,skin_vs}.cpp (window styles, LoadLibrary: stream R), toolsmenu/UIRenderer.cpp:100 SetFocus. Other remaining Audio diags are P (min/GetRandomNumberInRange int width, 4) and F (AEAudioUtility.cpp:91 x87 asm).
Per-file: .notes/w0/base_table.txt (before), .notes/w0/after_report.txt, .notes/w0/after.tsv.

## 3. What was done
* CMake (source/CMakeLists.txt): `source/portable/*.cpp` globbed ONLY with GTASA_PORTABLE=ON (Windows lists never see them); PORTABLE_EXCLUDED_FILES += oswrapper/oswrapper_win.cpp, game_sa/CdStreamInfo.cpp, Audio/Loaders/AEMFDecoder.cpp, AEWMADecoder.cpp, + PORTABLE_EXCLUDED_APP_FILES (W0a); portable defines USE_DSOUND (DirectSound code path, types from shim) next to USE_OPENAL; OpenAL::OpenAL linked.
* portable/winapi_compat.h: straight equivalents: Sleep, GetTickCount(64), QPC, RECT/POINT/SIZE/SYSTEMTIME/OVERLAPPED, LocalAlloc/Free, CRITICAL_SECTION funcs, MultiByteToWideChar/WideCharToMultiByte (UTF-8 <-> UTF-32), thread API, CoInitialize (no-op: no COM objects exist).
* portable/oswrapper_portable.cpp: full oswrapper.h over std::thread/mutex/cv/stdio (recursive mutex, counting semaphore with max, suspended start gate); Win32 CreateThread/Resume/Wait/Close; SHGetFolderPathW(CSIDL_PERSONAL) = $HOME/Documents.
* portable/cdstream_portable.cpp: CdStream over pread + streaming thread (same globals/contract; no overlapped mode; clean thread stop at shutdown).
* winshim/{dsound,objidl,mmsystem,mfidl,mfreadwrite,wmsdk,wmsdkidl,ShlObj,direct}.h: DirectSound/COM/WMF as abstract TYPES; DirectSoundCreate(8) logs + returns DSERR_NODRIVER, so CAEAudioHardware::Initialise returns false (audio off).
* portable/audio_decoders_portable.cpp: CAEMFDecoder/CAEWMADecoder replacements, InitLibrary/Initialise log + fail.
* portable/audio_portable.h: MainImageRange() (Mach-O segments) for CAESound::IsInExeImage / audio stats.
* `#if` edits (Windows view token-identical, __LINE__ kept by tools/portable/w0a_guardlines.py): AESound.cpp, AESoundManager.cpp, AEUserRadioTrackManager.cpp (ResolveShortcut, path overloads), FileMgr.cpp (createDirectory, '/' separators for Gallery/User Tracks/User Files); DirectSound tap/mute debug tools: `#ifdef NOTSA_STANDALONE_RUN` -> `#if defined(NOTSA_STANDALONE_RUN) && !defined(NOTSA_PORTABLE)` at 5 sites (line count unchanged).

## 4. Stubs / honest gaps (W-n items; W-A1..A13 in MAC-W0a.md)
| id | where | behaviour |
|---|---|---|
| W-B1 | oswrapper_portable.cpp:8,54; winapi_compat.h:115 | thread priorities ignored (logged once), SetThreadPriority returns TRUE |
| W-B2 | oswrapper_portable.cpp:272 | SuspendThread: no portable suspend, logs, returns (DWORD)-1 (CAEStreamThread::Pause ineffective) |
| W-B3 | oswrapper_portable.cpp:258 | CreateThread leaves *threadId = 0 |
| W-B4 | winshim/dsound.h:183 | DirectSound unavailable: DirectSoundCreate(8) fail, audio hardware disabled until OpenAL layer (W-3/W-4) |
| W-B5 | audio_decoders_portable.cpp:5 | user-track decoders (MF/WMA) fail: user tracks unplayable |
| W-B6 | oswrapper_portable.cpp:307 | TerminateThread logs, returns FALSE (user-track scan thread keeps running) |
| W-B7 | AEUserRadioTrackManager.cpp:395 | .lnk shortcut resolution unsupported (returns empty) |
| W-B8 | AESoundManager.cpp:519 | audio-stats vptr RVA read has no SEH guard |
| W-B9 | cdstream_portable.cpp header | no overlapped I/O mode; paths '\\'->'/' only for CdStreamOpen; case/data-root handling = W-7 |
| W-B10 | oswrapper OS_TimeMS | keeps Win32 formula counter/frequency (= seconds, upstream quirk); psTimer uses it |
Not done: W-6 file-record threads beyond audio/CdStream, DirectInput (skeleton.h LPDIRECTINPUT8: R/skeleton), toolsmenu SetFocus, standalone/rw window styles (R). `<intrin.h>` in Pool.h, `winshim` has no io.h/malloc.h/intrin.h (census winstub supplies empty ones): needed for a real link build.

## 5. No-op proof (MAC-W0b.md, sonnet sub-agent, wine msvc)
| check | result |
|---|---|
| StandaloneRelease compile commands BEFORE/AFTER (four commits reverted in BEFORE, 42 paths) | 4670/4670 identical (1284 TUs); build.ninja byte-identical after path normalisation |
| cl.exe under wine, obj_code_sig code + reloc names (+ --data), 11 TUs: AESmoothFadeThread, AEStreamThread, AEAudioHardware, AESound, AESoundManager, AEUserRadioTrackManager, FileMgr, WinPs, WinMain, WinPlatform, app_debug | 11/11 identical; .debug$T byte-identical; .debug$S differs 64-97 bytes (source checksums only) |
| preprocessed -E | identical token stream, only `#line` re-syncs |

## CONTINUE
OpenAL hardware layer (W-3/W-4) replacing the DirectSound classes; W-6 thread/stream beyond; real link needs winshim intrin/io/malloc and SetFocus; standalone/rw window code (stream R).
