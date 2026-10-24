# MAC-C6 report: Windows scalar types + CRT shim (C-6), libc++ gaps (C-4)

Branch portable/main, pushed to `fork`. Commits: b0080f96 (C-6), fd1ac44f (C-4). Host: Apple Silicon Mac.

## Headers (all new files are under source/portable/, nothing outside `#ifndef _WIN32` for MSVC)
| file | content |
|---|---|
| source/portable/wintypes.h | fixed-width Windows scalars (LONG/ULONG/DWORD/HRESULT = 32 bit, never `long`; *_PTR/SIZE_T/WPARAM pointer-sized), `byte`, BOOL, HANDLE + opaque H* handles, GUID, LARGE_INTEGER, ULARGE_INTEGER, MAX_PATH, S_OK/E_*/SUCCEEDED/FAILED, LOWORD..MAKELONG, ZeroMemory..., `__debugbreak`; `__cdecl/__stdcall/...`, `__declspec`, `__int64` as empty macros only when the compiler lacks `-fms-extensions` (`_MSC_EXTENSIONS` undefined) |
| source/portable/crt.h | `_stricmp/stricmp/_strnicmp/strnicmp`, `_strlwr/_strupr(+_s)`, `strcpy_s/strcat_s/strncpy_s/wcscpy_s/wcscat_s/memcpy_s` (size form + array-template form, CRT destination-size semantics: dst[0]=0, invalid-parameter handler, EINVAL/ERANGE/STRUNCATE, `_TRUNCATE`), `sprintf_s/vsprintf_s`, `fprintf_s/printf_s` (macros), `sscanf_s/fscanf_s` (real: the size argument after %s/%c/%[ is consumed; up to 48 outputs), `strtok_s`, `fopen_s/freopen_s/_wfopen_s`, `_getcwd/_chdir/_wchdir/_wgetcwd`, `_memicmp`, `_strdate_s/_strtime_s`, `_MAX_PATH`, `_set_invalid_parameter_handler`, `Interlocked*` (32/64/8/16 bit) over `__atomic_*` builtins |
| source/portable/stdcompat.h | C-4: `notsa::StackTrace` + `notsa::StackTraceToString` (= std::stacktrace on MSVC / where `__cpp_lib_stacktrace`; empty stand-in otherwise), `notsa::compat::from_chars` (= `using std::from_chars` on MSVC / where `__cpp_lib_to_chars`; otherwise ints -> std, floating point -> strtod family) |
| source/portable/winshim/Windows.h | `#include <Windows.h>` for the ~10 direct includers (fakerw.h, skeleton.h, dllmain.h, debug.hpp ...): forwards to wintypes.h + crt.h. PortableMac must put `source/portable/winshim` on the include path (-I) OR guard those includes. NOT yet wired (CMake is MAC-A1's) |
| source/app/platform/win/winincl.h | `#ifdef _WIN32 <Windows.h>(+mmsystem) #else portable/wintypes.h + crt.h #endif` |
| tools/standalone/winstub/Windows.h | census stub now forwards to the portable headers (one definition of the types for the census) |
| tests/portable/crt_shim_test.cpp | host test of the shims (strcpy_s/strcat_s/strncpy_s/sprintf_s overflow semantics, sscanf_s with sizes incl. 32 outputs, %[ , %*s, interlocked, from_chars): `fails=0`; build line in the file header; not in CMake |
Edited call sites (one line each): Configuration.hpp (include on the former blank line so line numbers do not move; `notsa::compat::from_chars`), SoundManagerDebugModule.hpp (`notsa::StackTrace`), SoundManagerDebugModule.cpp (`#ifdef _WIN32 std::to_string(...) #else notsa::StackTraceToString(...)`), AESoundManager.cpp (`notsa::StackTrace::current()`). No other of the 74 libc++ files touched, no codemod.
Note: commit fd1ac44f also swept in another agent's uncommitted one-liner in AESoundManager.cpp (`static_assert(sizeof(AESoundManager)==36028)` -> `VALIDATE_SIZE(AESoundManager, 36028)`); harmless, mention to its owner.

## Toolchain finding (C-4)
| item | Apple clang 21 / libc++ 21 (Xcode) | Homebrew LLVM 23.1.2 libc++ (/opt/homebrew/opt/llvm) |
|---|---|---|
| `std::views::enumerate` (129 uses) | missing | present |
| `std::ranges::shift_right` (2) | missing | present |
| `<stacktrace>` | missing | missing (no libc++ has it) -> `notsa::StackTrace` |
| `std::from_chars(double)` | "introduced in macOS 26.0" with target macOS 15 | same availability annotation (also with brew libc++ headers) -> `notsa::compat::from_chars` fallback |
**Recommendation for PortableMac (MAC-A1): brew llvm (`/opt/homebrew/opt/llvm/bin/clang++`), always with `-isysroot $(xcrun --show-sdk-path)`** (without it brew clang finds no C headers: FP_NAN/assert.h errors; brew clang already picks brew's libc++ headers), `-std=c++23`, `-fms-extensions` (or rely on the `_MSC_EXTENSIONS` fallback macros), `-I source/portable/winshim`. No `notsa::views::enumerate` was needed. If Apple clang must be supported later, `enumerate` + `shift_right` shims (129 + 2 uses) must be added then.
`utility.hpp` `try_ston<float>` also calls `std::from_chars(floating)` (template, not instantiated in the census): switch it to `notsa::compat::from_chars` when first instantiated natively.

## Census (native, `--mode native`, restricted to 212 TUs = the .cpp files with CRT/libc++/CRT-adjacent diagnostics + up to 12 includers of each affected header; brew clang + -isysroot; same TU list before/after)
| category (unique diagnostics) | before | after |
|---|---|---|
| C: libc++ / standard-library gap | 3 (stacktrace, from_chars) [original Apple-clang full census: 128 in 74 files] | 0 |
| W/C: MSVC CRT name | 305 (original full census: 307) | 8 |
| W: Win32 API / type | 338 | 318 |
| TUs clean | 24 / 212 | 62 / 212 |
| unique diagnostics, all | 2692 | 2198 (the "other" cascades of the missing CRT names disappeared) |
Remaining 8 CRT diagnostics are all in platform / F-stream files: `_set_SSE2_enable`, `_clearfp` (WinMain.cpp), `__cpuid` (WinPs.cpp), `MAX_SYM_NAME` (debug.cpp, dbghelp), `_controlfp`, `_statusfp`, `_set_purecall_handler` (Fixups.cpp). Not shimmed on purpose: x87/SSE control (stream F), dbghelp (W).
"W: Win32 API / type" is NOT 0 outside the platform files: it is Win32 *API* use (CreateFile/threads/semaphores/COM/WMF), by design stream W: CdStreamInfo.cpp 34, Audio/Loaders/AEMFDecoder.cpp 34 + AEWMADecoder.cpp 19 (+.h), AEUserRadioTrackManager 8, AEStreamThread/AESmoothFadeThread 7+7, FileMgr.cpp 7, app_debug.cpp 4, HooksUtility.hpp 10 (A), common.cpp, MenuManager.cpp, LoadingScreen.cpp, SimpleVariablesSaveStructure.h, AEAudioUtility.cpp, AEDataStream.cpp 2, extensions/debug.* 2 (all 1-2 each). The scalar-type part of the category (types/GUID/LARGE_INTEGER) is 0. The full list is in `/private/tmp/claude-501/c6/r3/classified.tsv` (scratch). Wide-string/UTF-16 handling (`WCHAR` is 32-bit here: FileMgr wide paths) is stream W.

## Known semantic deviations
* sscanf_s/fscanf_s: `%s`/`%[` without a width is truncated to size-1 (the CRT fails the conversion). Wide `%ls` passes through unchanged.
* `*_s` violations call the registered `_invalid_parameter_handler`, else print + abort() (like Release CRT).
* fopen_s ignores `,ccs=`. `LONG`/`HRESULT` are `int` (not `long`): code that overloads on `long` vs `int` or prints LONG with `%ld` needs a look under C-7.
* `-Wformat-nonliteral` style warnings in crt.h are suppressed globally by the census (-Wno-everything); the real preset may warn.

## No-op proof (MSVC cl 14.51 x86 under Wine, /O2 /Z7 /arch:IA32, flags from StandaloneRelease compile_commands.json, no PCH; BEFORE = git archive of HEAD, AFTER = HEAD + exactly my 5 edited files + source/portable/; both built from the same path via symlink swap so __FILE__ lengths match; compare = `llvm-objdump -d -r` of all code sections incl. relocation target names; done by a sonnet sub-agent, repo untouched)
| TU / variant | .text before / after | code + relocs identical |
|---|---|---|
| game_sa/FileMgr.cpp (winincl.h) | 48717 / 48717 | yes |
| dllmain.cpp (Configuration.hpp) | 182414 / 182414 | yes |
| game_sa/Audio/Managers/AESoundManager.cpp | 39315 / 39315 | yes |
| same + /DNOTSA_DEBUG (compiles the stacktrace lines) | 43800 / 43800 | yes |
| toolsmenu/.../SoundManagerDebugModule.cpp | 27487 / 27487 | yes |
| same + /DNOTSA_DEBUG | 27487 / 27487 | yes |
| game_sa/CarCtrl.cpp | 134449 / 134449 | yes |
Objects are not byte-identical (debug sections only). Two first-run differences were fixed before landing: (1) a non-inlined `StackTraceToString` wrapper (+204 bytes) -> the MSVC branch now calls `std::to_string` directly; (2) one `__LINE__` immediate in Configuration.hpp moved by an added line -> the include replaces a blank line. crt.h/wintypes.h are `#ifndef _WIN32` and never seen by MSVC; stdcompat.h is seen (included by Configuration.hpp / SoundManagerDebugModule.hpp) and only forwards to std:: on `_WIN32`.

## Open items
* MAC-A1 / PortableMac: -isysroot, brew llvm, `-I source/portable/winshim`; add `tests/portable/crt_shim_test.cpp` to ctest if wanted.
* W: Win32 API users listed above (stream W); `_controlfp/_statusfp/_clearfp/_set_SSE2_enable/__cpuid` -> stream F; `MAX_SYM_NAME`/dbghelp -> compile out debug.cpp stack walker.
* C-7: LONG-as-int overload/format fallout; remaining "other: undeclared identifier" in the restricted set are Win32 API/DirectSound members (W/R).
* Windows-side fidelity agents: new code in `source/` that calls CRT `_s` functions or Windows types needs nothing; new Win32 API calls should be wrapped in `#ifdef _WIN32` only if they are outside platform files.
