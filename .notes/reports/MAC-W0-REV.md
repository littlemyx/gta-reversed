# MAC-W0-REV: review of MAC-W0 + removal of the `#line // W0a` hack

Commits (portable/main, pushed to fork): 7203bc53 (remove directives), 5f6ce85d (psGrabScreen/VRAM fix), + cdstream fix commit. Sub-report (wine A/B, sonnet sub-agent): MAC-W0-REV-AB.md.

## 1. `#line` removal
108 `#line N // W0a` lines removed from 9 files (app_debug.cpp, WinMain.cpp, WinPlatform.cpp, WinPs.cpp, debug.hpp, AESound.cpp, AESoundManager.cpp, AEUserRadioTrackManager.cpp, FileMgr.cpp). Pre-existing `#line` (Base.h, app_game.cpp, platform.cpp, dllmain.cpp, FastLoader.hpp, 3dMarker.cpp ...) untouched.
tools/portable/w0a_guardlines.py: `fix` removed, `check` is report-only (text compare of the Windows view vs base, no numbering). All 9 files: TEXT OK vs 3f46d831~1.

## 2. Review (every W0 hunk in MSVC-compiled files)
(a) Windows path unchanged: yes in all hunks (checker + manual read of the full diff). Hunks with `#if defined(NOTSA_STANDALONE_RUN) && !defined(NOTSA_PORTABLE)` (AESmoothFadeThread x2, AEStreamThread, AEAudioHardware, AEAudioTap.h, AEDirectSoundMute.h): the Windows expression is the old one (NOTSA_PORTABLE undefined) - ok.
(b) guards balanced, no code moved: ok. Cosmetic only: AEUserRadioTrackManager.cpp:280 `        #ifdef NOTSA_PORTABLE` is indented (left).
(c) portable replacements:
| where | kind | verdict |
|---|---|---|
| OALBase.h/.cpp, source/CMakeLists.txt OpenAL block | NOT guarded by NOTSA_PORTABLE (HeapPtrArray = vector<T*>, trashCan owning, `#include <AL/al.h>`) | only compiled with GTASA_WITH_OPENAL (OFF in every Windows preset, no consumer); no-op proof covers it (0 diffs). Listed: turning WITH_OPENAL on for Windows would now need AL/al.h. |
| WinPs.cpp psGrabScreen portable | returned nullptr; JPegCompressScreen (RsGrabScreen) dereferences it unchecked => crash on photo/screenshot | FIXED 5f6ce85d: portable/app_platform.cpp GrabScreenBlank() returns an opaque black image of camera size, logged (W-A11) |
| WinPs.cpp VRAM 0 | VideoMode.cpp:53 `VRAM.Avail - w*h*d/8 <= 45MB` only passed through unsigned wrap (every mode accepted by accident) | FIXED 5f6ce85d: VRAM = RAM.TotalPhys (unified memory), explicit |
| WinPs.cpp CheckDirectSound -> TRUE | sound-device assumed (logged W-A12); audio hardware init still fails honestly later (DirectSoundCreate8 -> NODRIVER) | doubtful but logged; ok until OpenAL layer |
| WinPs.cpp "sound card" MessageBox uses an English literal, not GXT | cosmetic | listed |
| WinMain.cpp psInitialize/NOTSA_WinMain portable: SystemParametersInfo/SetErrorMode/UpdateWindow/CoInitialize dropped, anisotropy assumed (W-A4), FRONTEND_IDLE minimised test via SDL flags | logged or equivalent | ok |
| AESoundManager.cpp SafeVptrRva portable | unguarded read (W-B8), 64-bit vptr minus image start truncated to uint32 | stats-only, logged in report; listed |
| AEUserRadioTrackManager ResolveShortcut portable | logs error, returns empty (callers treat empty path as unplayable/skip) | honest |
| FileMgr createDirectory portable | create_directory with error ignored = CreateDirectoryW semantics (callers ignore) | ok |
| AESound::IsInExeImage portable | Mach-O segment range (cached static in MainImageRange) | ok |
Portable platform layer (source/portable/*.cpp) skim:
| file:line | finding | status |
|---|---|---|
| cdstream_portable.cpp CdStreamThread (shutdown) | `pthread_exit` inside a std::thread: OS thread gate `finished` never set, shared_ptr leaked; WaitForSingleObject(INFINITE) on that handle would hang | FIXED: thread leaves via a private exception caught in the launch lambda |
| cdstream_portable.cpp CdStreamInit/Open | Win32 CdStreamInit leaves the GTA3.IMG probe handle in slot 0 (RemoveImages loop runs to gOpenStreamCount=0), so the exe's first CStreaming image gets FileID 1; portable gives FileID 0. Consumers only use CdStreamHandleToFileID consistently, no truthiness test of the handle (grep: Streaming.h/.cpp, StreamingInfo.cpp, AEBankLoader, AEMP3BankLoader) | harmless, listed |
| cdstream_portable.cpp | failed CdStreamOpen leaves slot free (Win32 stores INVALID_HANDLE_VALUE); path '/' conversion only; case-insensitivity (W-7) missing | listed (known W-B9) |
| oswrapper_portable.cpp OS_MutexRelease | unlock of a recursive_mutex not owned by the caller is UB (Win32 ReleaseMutex just fails) | listed, only the magic check protects |
| oswrapper_portable.cpp WaitForSingleObject/OS_ThreadWait | two concurrent joiners race on `joinable()/join()` | listed, single waiter in the game |
| oswrapper_portable.cpp CsMutex | every Enter/Leave takes a global std::mutex for lazy init: contention only, correct | listed |
| SuspendThread -> (DWORD)-1 | CAEStreamThread::Pause cannot pause the thread (W-B2): shared state is then touched while the thread runs | listed, audio hardware disabled anyway |
| winapi_compat.h, audio_decoders_portable.cpp, app_platform.cpp (flock single instance, crash handler uses non-async-signal-safe calls, GetBestRefreshRate) | no bug found | ok |

## 3. No-op proof (details: MAC-W0-REV-AB.md)
| check | result |
|---|---|
| StandaloneRelease A (8fcde553) vs B (7203bc53): TUs / compile commands / build.ninja | 1284/1284, 4670/4670 identical, build.ninja identical modulo paths |
| wine cl.exe, 11 TUs of MAC-W0b: code + relocs + symbols + section sizes | 7 TUs strictly identical; 4 TUs (AEUserRadioTrackManager 3, WinMain 5, WinPs 16, app_debug 25) differ ONLY in 49 `__LINE__` immediates (each verified A->B line mapping), 0 OTHER |
| preprocessed (`cl /P`, minus #line/blank) | identical except literal line numbers + P-2a/b header block (FileRecord.hpp, tScriptParam) |
| .debug$T | -40 B in all 11 (P-2b removed `void* pParam; char* szParam` members of tScriptParam; size 4 unchanged) |
| COMDAT order | `std::nullopt` / `_Digit_pairs<char>` .rdata COMDAT emitted earlier in AEStreamThread + AEUserRadioTrackManager (P-2 header include order); contents identical |
My later edits (5f6ce85d WinPs.cpp lines inside `#ifdef NOTSA_PORTABLE`, same line count; cdstream/app_platform = portable-only files) do not change the Windows view (checker TEXT OK, no line shifts), so the A/B stands for HEAD.

## 4. Portable census on touched TUs
18 TUs (11 W0 TUs + 7 source/portable/*): 17 clean; AEAudioHardware.cpp 2 diagnostics (P: `min` int width, pre-existing from MAC-W0). W (platform) = 0 on these, no regression vs MAC-W0 (W 3 elsewhere: standalone/rw, UIRenderer).

## CONTINUE
Nothing blocking. Optional: restore AEUserRadioTrackManager.cpp:280 indent; WITH_OPENAL on Windows would need AL headers.
