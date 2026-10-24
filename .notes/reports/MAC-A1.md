# MAC-A1 report: `GTASA_PORTABLE`, preset `PortableMac`, hook machinery compiled out (slices A-1 + C-0)

Host: Apple Silicon Mac. Branch portable/main. Commit **ffbeba26** (pushed to `fork`). Census: `.notes/reports/MAC-A1-census.md`.

## Result (numbers)
| item | value |
|---|---|
| configure `cmake --preset PortableMac` | OK first time, 3 s; `build/PortableMac/compile_commands.json`: 1,137 game TUs (+ librw) |
| census TUs (portable target) | 1,105 (native arm64 syntax pass, 430 s at -j6) |
| stream A unique diagnostics | **71 -> 0** (same tree, Windows -D set vs portable compdb); zero by message too |
| TUs clean / unique root diagnostics (whole tree) | 404 of 1,105 / 3,643 (C1 snapshot of 2026-10-11: 0 / 4,340; different tree, C6 edits in flight) |
| no-op (OFF) | StandaloneRelease compile commands + link line identical (see below); 8 MSVC TUs code-identical |

## What was compiled out and how (all behind `GTASA_PORTABLE` / `NOTSA_PORTABLE`; OFF = nothing changes)
* `source/CMakeLists.txt`: `option(GTASA_PORTABLE)` (needs STANDALONE=ON, not DUMP_HOOKS_ONLY). ON:
  * `NOTSA_PORTABLE=1` on the game target (target scope, not directory scope).
  * Sources removed from the target: `reversiblehooks/*.cpp` + `ReversibleHook/*.cpp` (RHManager, VMTInfo, HookCategoryItem, Static*Hook, VMT/Virtual hooks), the hook UI
    `toolsmenu/DebugModules/HooksDebugModule/*.cpp`, `dllmain.cpp`, `PluginBase.cpp`. `DataImage.cpp` no longer exists (F3); `ImageLoad/ImageDiag/GlobalsVerify` are
    `#if` options (IMAGE_DIAG / VERIFY_GLOBALS are rejected with PORTABLE).
  * `InjectHooksMain.cpp` is KEPT on purpose: it still carries the non-hook part (script command handler registration via the `X::InjectHooks()` of the command files, logger,
    `CommandLine::PostHooksInjected`); with the macros empty its 1,812 `InjectHooks()` bodies are empty. The census puts 0 unique diagnostics on it.
  * `RH_Scoped*` macros: unchanged files (896), no edit. `NoHookRegistry.h` now also derives `NOTSA_NO_HOOK_REGISTRY` from `NOTSA_PORTABLE` (the existing F2 no-op macros);
    `ReversibleHooks.h` includes the new `reversiblehooks/PortableStub.h` (empty `RHManager` {CreateInstance/DestroyInstance/GetInstance/CheckAll/WriteHooksToFile no-ops},
    `HookInstallOptions::HS`) instead of RHManager.h / HooksUtility.hpp (VirtualProtect, GetModuleHandle) / VMTInfo.h. So WinMain, UIRenderer, CommandLine compile unchanged.
  * `plugin::Call*` / `StaticRef`: nothing to do at macro level: in the detached run configuration `plugin::Call*<addr>` is already a `static_assert` on instantiation and no
    diagnostic of the census mentions it (the 54 + 23 residue sites of A-2 are not instantiated in compiled code).
  * `standalone/Fixups.cpp`: Windows part unchanged (`#if defined(NOTSA_STANDALONE_RUN) && !defined(NOTSA_PORTABLE)` on the same line 3); `#elif` at the end: std-only
    Log / Fatal / InstallDiagnostics / UnreachableHit / PoolBadDelete / LogFpuState (stderr + `standalone.log` in cwd or `NOTSA_STANDALONE_LOG`), `LogGameStateTrace` no-op. The
    Fixups symbols that game code calls therefore stay defined for a later link.
  * `toolsmenu/DebugModules/DebugModules.cpp` (outside the strict zone, needed to drop the hook UI): `#ifndef NOTSA_PORTABLE` around the include and `Add<HooksDebugModule>()`
    (+4 lines) and four blank lines removed so that every later `__LINE__` (NOTSA_LOG_ERR at 190/191/216) is unchanged.
  * Not applied with PORTABLE: exe-oracle / shim unit-test targets of `source/CMakeLists.txt` (they map gta_sa_compact.exe and would pollute the compdb), `ddraw Winmm dxguid
    strmiids d3d9 dsound` link libs, `/SUBSYSTEM:WINDOWS`, `.exe` suffix, `-Wall`/warnings-as-errors. Added: the census flag set
    `-fms-extensions -mms-bitfields -fno-delayed-template-parsing` plus provisional `-ffp-contract=off -fsigned-char -fno-strict-aliasing -fwrapv -w` (C-8 decides the final set).
  * librw: `LIBRW_PLATFORM=NULL` (the only platforms off Windows are NULL / GL3; NULL configures without OpenGL).

## Preset / toolchain / conan
* Preset `PortableMac` (CMakePresets.json, condition Darwin): Ninja, `build/PortableMac`, Release (`-O2 -DNDEBUG`), `GTASA_STANDALONE=ON GTASA_PORTABLE=ON GTASA_USE_SDL3=ON`.
* **Compiler: Apple clang 21.0.0 (`/usr/bin/clang++`, Xcode, SDK 27.0)**. Why: what every Mac has, and its libc++ is what the C-4 gap list was measured against (brew LLVM 23 has a
  different libc++ and would hide/shift those gaps). The census tool still defaults to brew llvm 23.1.2 as front end (`--llvm`), as before.
* Conan route: `conan/profiles/macos-arm64.txt` (Macos, armv8, apple-clang 21, c++23) + `tools/portable/conan_install_mac.sh`: runs `conan install --build=missing` from a
  **scratch copy** of conanfile.py and copies only the generators to `build/PortableMac/generators` (the toolchain file). `ConanPresets.json` (Windows presets) and
  `source/libs/imgui` are never rewritten (verified: git status clean for both). All deps built from source for arm64 in ~10 min (sdl3, spdlog, tracy, imgui, ogg, vorbis,
  libjpeg-turbo + nasm; boost header-only). Release only; a Debug profile is a one-liner if wanted.
* Not built: this slice produces compile_commands.json; a full build/link of PortableMac is expected to fail on streams P/W/R/F/C (3,643 diagnostics).

## No-op proof (the Win32 x86 MSVC build stays bit-for-bit)
(a) `cmake --preset StandaloneRelease` configured twice under wine/msvc-wine (HEAD worktree vs working tree): **1,281 of 1,281 compile commands identical** (paths normalised);
the working tree has 3 more TUs only because the HEAD worktree reached `source/libs/imgui` through a symlink that GLOB_RECURSE does not follow (artefact, not a diff);
`build.ninja` differs only by those 3 objects; the link statement and LINK_LIBRARIES / LINK_FLAGS are identical.
(b) MSVC `cl.exe` under wine, flags of StandaloneRelease minus the PCH, HEAD worktree vs working tree, `tools/standalone/obj_code_sig.py` (code bytes + relocation names, strings normalised):
| TU | why | code identical |
|---|---|---|
| standalone/Fixups.cpp | edited (line 3 + new #elif) | yes |
| toolsmenu/DebugModules/DebugModules.cpp | edited (guards + blank lines, __LINE__ sensitive) | yes |
| app/platform/win/WinMain.cpp | uses RHManager + hook macros | yes |
| game_sa/Door.cpp | hook registration file | yes |
| game_sa/Scripts/Commands/UtilityCommands.cpp | REGISTER_COMMAND_HANDLER_BEGIN | yes |
| game_sa/Cheat.cpp | uses NOTSA_NO_HOOK_REGISTRY | yes |
| game_sa/Pools.cpp | pools / Pool.h | yes |
| InjectHooksMain.cpp | 1,812 InjectHooks calls | yes |
8 / 8 (the shared headers changed are NoHookRegistry.h and ReversibleHooks.h, both in the PCH closure of every TU; their new content is under `#ifdef NOTSA_PORTABLE`).

## Open items
* **R**: compdb has both `RW_NULL` (librw NULL platform) and the game's `RW_D3D9` + `USE_D3D9`; `USE_D3D9` is still hard-coded in source/CMakeLists.txt. The render backend choice (GL3 / Metal path) belongs to R; 1,262 unique diagnostics (standalone/rw 925, Pipelines 209, RenderWare 71).
* **W-1**: `LoadConfigurations()` (gta-reversed.ini) lived in dllmain.cpp and is not called in the portable tree (the Windows run build gets configs via other paths: check); `notsa::GetDLLHandle` is gone (no callers). `imgui_impl_dx9.cpp` still globbed from source/libs/imgui. `WinMain.cpp` creates the stub `RHManager` (harmless).
* **C-1**: `_ReturnAddress()` in Pool.h (Fixups::PoolBadDelete caller) and other MSVC intrinsics; **C-6/C-7**: 936 W diagnostics (app 267, Audio 544, oswrapper 45).
* **P-1**: 927 unique size/offset asserts (+ 358 errors in the StdInc.h closure: 293 asserts, 16 operator new size_t) are what keeps "clean" TUs from compiling.
* A-2/A-4: PluginBase.h `plugin::Call*` stays as the static_assert guard; `__thiscall` casts there are ignored by clang on arm64.
* Census front end (brew LLVM 23) vs preset compiler (Apple clang 21) differ: libc++ gap counts may shift; census numbers include C6's uncommitted edits.
* Shared-checkout note: nothing of MAC-C6's files was touched or committed by this slice.
