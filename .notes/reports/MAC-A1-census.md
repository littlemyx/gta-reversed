# MAC-A1 census (C-0, portable mode): `build/PortableMac` TU list, native arm64 syntax pass

Command (Apple clang 21 builds the preset; the census front end is brew LLVM 23.1.2, `-target arm64-apple-macos15 -std=c++23 -fsyntax-only -fms-extensions -mms-bitfields
-fno-delayed-template-parsing`, winstub Windows headers, one PCH over StdInc.h, `-Werror=pointer-to-int-cast` family):

    cmake --preset PortableMac            # after tools/portable/conan_install_mac.sh
    python3 tools/standalone/clang_census.py run --mode native --compdb build/PortableMac/compile_commands.json --out DIR -j 6   # 430 s, 1105 TUs
    python3 tools/standalone/clang_census_report.py --out DIR --mode native --top 25 --tsv DIR/classified.tsv

Tree: HEAD ffbeba26 + the uncommitted work of MAC-C6 (winincl.h, winstub/Windows.h, Configuration.hpp, AESoundManager.cpp, SoundManagerDebugModule.*) = a snapshot, not a clean commit.
The TU list is the 1,105 game TUs of the portable target (the 1,133/1,115 of the Windows run build minus the hook registry (RHManager, VMTInfo, HookCategoryItem, ReversibleHook/*),
the hook UI (HooksDebugModule/*), dllmain, PluginBase; Image*/GlobalsVerify are empty `#if`s in both). "TUs clean" = no diagnostic of its own; the 358 errors inside the StdInc.h
closure are counted once as the pseudo TU, as in the C1 census (so "clean" does not mean "builds": every TU still includes the PCH closure, P-1 fixes it).

## Acceptance for A: stream A diagnostics = 0
| | unique diagnostics, stream A | files |
|---|---|---|
| before (same tree, Windows run-build -D set, A-zone TUs only: reversiblehooks, Fixups, PluginBase, InjectHooksMain, dllmain, HooksDebugModule, UIRenderer, CommandLine, WinMain, DebugModules; 24 TUs) | **71** (Fixups.cpp 56, HooksUtility.hpp 11, StaticOneWayHook.cpp 3, PluginBase.cpp 1; dllmain.cpp 8 more, classified W) | 4 (+dllmain) |
| after (portable compdb, whole target) | **0** (also 0 by message: no ReversibleHooks / RHManager / HookInstallOptions / plugin::Call / StaticRef / kCallByAddressForbidden in any diagnostic) | 0 |

The older snapshot (STREAM_C_PLAN section 4, tree of 2026-10-11): 157 in 9 files, DataImage.cpp has since been deleted (F3).

## Totals (native)

| item | count |
|---|---|
| TUs | 1105 |
| TUs clean | 404 |
| TUs with errors | 701 |
| errors inside the shared PCH closure (StdInc.h), counted once | 358 |
| TUs failed without parsed diagnostics (crash/timeout) | 0 |
| raw diagnostics (every re-parse counted) | 7764 |
| unique diagnostics (file,line,message) | 3643 |

## By category (unique | raw)

| category | unique | raw | files | TUs reaching it |
|---|---|---|---|---|
| W/R: DirectX | 1138 | 1708 | 45 | 75 |
| P: 64-bit layout (sizeof/offsetof static_assert) | 927 | 3637 | 575 | 601 |
| other: undeclared/unknown identifier | 454 | 726 | 49 | 126 |
| W: Win32 API / type | 374 | 417 | 41 | 41 |
| F/C: SIMD / intrinsic / asm | 166 | 331 | 4 | 2 |
| other | 145 | 268 | 30 | 51 |
| other: conversion/overload | 102 | 105 | 36 | 36 |
| P: pointer/int width (cast/narrowing) | 101 | 131 | 32 | 47 |
| C: syntax / extension | 87 | 180 | 21 | 22 |
| other: undeclared/unknown type | 57 | 115 | 26 | 43 |
| F/C: inline asm / naked | 36 | 36 | 13 | 13 |
| P: 64-bit (size_t in operator new/delete) | 20 | 20 | 16 | 3 |
| P: integer width / mixed-type overload | 15 | 43 | 13 | 37 |
| C: two-phase lookup / template / incomplete type | 7 | 31 | 3 | 14 |
| W/C: MSVC CRT name | 5 | 5 | 3 | 3 |
| C: nested DMI inside enclosing class | 5 | 5 | 2 | 1 |
| other static_assert | 4 | 6 | 2 | 4 |

## By owner stream (unique diagnostics; stream chosen by file area first, then category)

| stream | unique | files | TUs reaching it |
|---|---|---|---|
| R render | 1262 | 39 | 88 |
| P 64-bit | 1027 | 588 | 610 |
| W platform | 936 | 50 | 64 |
| F float / x87 / SIMD | 208 | 16 | 15 |
| unclassified (cascade / other) | 195 | 53 | 123 |
| C compiler | 15 | 6 | 3 |

## By directory (TUs of the directory | clean TUs | unique root diagnostics located in it | by stream letter; u = unclassified)

| directory | TUs | TUs clean | unique diagnostics | A | W | R | P | F | C | u |
|---|---|---|---|---|---|---|---|---|---|---|
| source/standalone | 40 | 24 | 932 | 0 | 0 | 925 | 0 | 7 | 0 | 0 |
| source/game_sa/Audio | 48 | 8 | 652 | 0 | 544 | 9 | 48 | 1 | 1 | 49 |
| source/game_sa (root) | 251 | 139 | 436 | 0 | 68 | 21 | 224 | 12 | 13 | 98 |
| source/app | 19 | 6 | 267 | 0 | 267 | 0 | 0 | 0 | 0 | 0 |
| source/game_sa/Scripts | 68 | 2 | 257 | 0 | 1 | 0 | 235 | 5 | 0 | 16 |
| source/game_sa/Pipelines | 7 | 0 | 209 | 0 | 0 | 209 | 0 | 0 | 0 | 0 |
| (outside source/) | 0 | 0 | 191 | 0 | 0 | 15 | 0 | 172 | 0 | 4 |
| source/game_sa/Tasks | 333 | 69 | 191 | 0 | 0 | 0 | 191 | 0 | 0 | 0 |
| source/game_sa/Entity | 30 | 10 | 106 | 0 | 0 | 0 | 95 | 2 | 0 | 9 |
| source/game_sa/Events | 89 | 36 | 94 | 0 | 0 | 0 | 94 | 0 | 0 | 0 |
| source/game_sa/RenderWare | 3 | 0 | 71 | 0 | 0 | 71 | 0 | 0 | 0 | 0 |
| source/oswrapper | 1 | 0 | 45 | 0 | 45 | 0 | 0 | 0 | 0 | 0 |
| source/game_sa/Models | 12 | 8 | 29 | 0 | 0 | 0 | 27 | 0 | 0 | 2 |
| source/game_sa/Core | 15 | 7 | 28 | 0 | 0 | 0 | 19 | 9 | 0 | 0 |
| source/game_sa/Collision | 19 | 13 | 23 | 0 | 0 | 0 | 23 | 0 | 0 | 0 |
| source/extensions | 3 | 1 | 19 | 0 | 6 | 0 | 1 | 0 | 0 | 12 |
| source/game_sa/Fx | 56 | 13 | 18 | 0 | 0 | 0 | 18 | 0 | 0 | 0 |
| source/game_sa/Interior | 7 | 0 | 13 | 0 | 0 | 0 | 13 | 0 | 0 | 0 |
| source/fakerw | 0 | 0 | 12 | 0 | 0 | 12 | 0 | 0 | 0 | 0 |
| source/game_sa/Animation | 9 | 9 | 11 | 0 | 0 | 0 | 11 | 0 | 0 | 0 |
| source/. | 0 | 0 | 8 | 0 | 4 | 0 | 0 | 0 | 0 | 4 |
| source/game_sa/Plant | 4 | 0 | 6 | 0 | 0 | 0 | 6 | 0 | 0 | 0 |
| source/game_sa/Ragdoll | 4 | 0 | 6 | 0 | 0 | 0 | 6 | 0 | 0 | 0 |
| source/game_sa/Plugins | 7 | 4 | 6 | 0 | 0 | 0 | 6 | 0 | 0 | 0 |
| source/toolsmenu | 41 | 29 | 4 | 0 | 1 | 0 | 2 | 0 | 0 | 1 |
| source/game_sa/Text | 4 | 3 | 3 | 0 | 0 | 0 | 3 | 0 | 0 | 0 |
| source/game_sa/Attractors | 12 | 10 | 2 | 0 | 0 | 0 | 2 | 0 | 0 | 0 |
| source/game_sa/Memory | 4 | 3 | 2 | 0 | 0 | 0 | 2 | 0 | 0 | 0 |
| source/game_sa/Conversations | 4 | 0 | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| source/game_sa/Enums | 1 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 0 |
| source/game_sa/DecisionMakers | 5 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| source/game_sa/Frontend | 6 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| source (root) | 3 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

## Top 25 files by unique diagnostics (file the diagnostic points to)

| file | unique | main categories |
|---|---|---|
| source/standalone/rw/pipeline_matfx.cpp | 350 | W/R: DirectX 342, other: undeclared/unknown identifi 6, other 2 |
| source/standalone/rw/engine.cpp | 139 | W/R: DirectX 98, W: Win32 API / type 27, other: undeclared/unknown identifi 13 |
| source/standalone/rw/pipeline.cpp | 127 | W/R: DirectX 127 |
| source/game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline.cpp | 97 | W/R: DirectX 96, C: syntax / extension 1 |
| source/game_sa/Audio/Loaders/AEMFDecoder.cpp | 97 | other: undeclared/unknown identifi 57, W: Win32 API / type 34, other 4 |
| /opt/homebrew/Cellar/llvm/23.1.2/lib/clang/23/include/emmintrin.h | 84 | F/C: SIMD / intrinsic / asm 83, other 1 |
| source/game_sa/Audio/Hardware/AEStreamingChannel.cpp | 81 | other: undeclared/unknown identifi 64, W/R: DirectX 7, other 6 |
| source/game_sa/Audio/Hardware/AEAudioTap.h | 80 | other 24, C: syntax / extension 21, W/R: DirectX 15 |
| source/standalone/rw/rwd3d_ff.cpp | 72 | W/R: DirectX 53, other 14, C: syntax / extension 3 |
| source/app/platform/win/WinPs.cpp | 66 | W: Win32 API / type 26, W/R: DirectX 18, other: undeclared/unknown identifi 14 |
| source/app/platform/win/WinPlatform.cpp | 66 | W: Win32 API / type 59, W/R: DirectX 2, other: undeclared/unknown identifi 2 |
| /opt/homebrew/Cellar/llvm/23.1.2/lib/clang/23/include/xmmintrin.h | 63 | F/C: SIMD / intrinsic / asm 60, other: conversion/overload 2, other 1 |
| source/game_sa/Audio/Hardware/AEStaticChannel.cpp | 61 | other: undeclared/unknown identifi 50, other 6, W/R: DirectX 5 |
| source/standalone/rw/im2d_im3d.cpp | 54 | W/R: DirectX 51, other 2, other: undeclared/unknown identifi 1 |
| source/standalone/rw/renderstate.cpp | 53 | W/R: DirectX 52, other: undeclared/unknown identifi 1 |
| source/app/platform/win/WinMain.cpp | 51 | W: Win32 API / type 23, other: undeclared/unknown identifi 20, other: undeclared/unknown type 3 |
| source/standalone/rw/camera.cpp | 48 | W/R: DirectX 28, other: undeclared/unknown identifi 15, other: undeclared/unknown type 2 |
| source/oswrapper/oswrapper_win.cpp | 45 | W: Win32 API / type 41, other 2, other: undeclared/unknown identifi 2 |
| source/game_sa/RenderWare/D3DResourceSystem.cpp | 44 | W/R: DirectX 22, C: syntax / extension 13, other: undeclared/unknown identifi 9 |
| source/game_sa/Pipelines/CustomBuilding/CustomBuildingDNPipeline.cpp | 44 | W/R: DirectX 43, P: pointer/int width (cast/narrowi 1 |
| source/game_sa/Pipelines/CustomBuilding/CustomBuildingPipeline.cpp | 43 | W/R: DirectX 43 |
| source/game_sa/CdStreamInfo.cpp | 39 | W: Win32 API / type 34, other: undeclared/unknown identifi 4, other: undeclared/unknown type 1 |
| source/game_sa/Audio/Loaders/AEWMADecoder.cpp | 38 | W: Win32 API / type 19, other: undeclared/unknown identifi 6, other: undeclared/unknown type 6 |
| source/game_sa/CarCtrl.cpp | 37 | P: 64-bit layout (sizeof/offsetof  36, other: conversion/overload 1 |
| source/game_sa/Audio/Hardware/AEAudioChannel.cpp | 36 | other: undeclared/unknown identifi 30, W/R: DirectX 2, other: undeclared/unknown type 2 |

## Top 25 TUs by diagnostics (headers re-counted per TU)

| TU | diagnostics |
|---|---|
| source/standalone/rw/pipeline_matfx.cpp | 364 |
| <PCH StdInc.h closure> | 358 |
| source/InjectHooksMain.cpp | 339 |
| source/standalone/rw/pipeline_skin_cpu.cpp | 196 |
| source/standalone/rw/pipeline_skin.cpp | 193 |
| source/game_sa/Audio/Hardware/AEStreamingChannel.cpp | 187 |
| source/game_sa/Audio/Hardware/AEStaticChannel.cpp | 162 |
| source/standalone/rw/engine.cpp | 153 |
| source/standalone/rw/pipeline.cpp | 141 |
| source/game_sa/Audio/Hardware/AEAudioHardware.cpp | 134 |
| source/game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline.cpp | 121 |
| source/game_sa/Audio/AEStreamThread.cpp | 108 |
| source/game_sa/Audio/Loaders/AEMFDecoder.cpp | 104 |
| source/game_sa/Audio/AESmoothFadeThread.cpp | 101 |
| source/game_sa/Events/EventHandler.cpp | 94 |
| source/standalone/rw/rwd3d_ff.cpp | 72 |
| source/game_sa/Pipelines/CustomBuilding/CustomBuildingDNPipeline.cpp | 69 |
| source/standalone/rw/im2d_im3d.cpp | 68 |
| source/game_sa/Pipelines/CustomBuilding/CustomBuildingPipeline.cpp | 68 |
| source/app/platform/win/WinPlatform.cpp | 68 |
| source/app/platform/win/WinPs.cpp | 67 |
| source/standalone/rw/renderstate.cpp | 67 |
| source/game_sa/Audio/Hardware/AEAudioChannel.cpp | 66 |
| source/app/platform/win/WinMain.cpp | 65 |
| source/standalone/rw/camera.cpp | 62 |

## Most frequent messages (unique, normalised) in the biggest categories

### W/R: DirectX
- 934 x use of undeclared identifier '…'
- 164 x unknown type name '…'
- 33 x use of undeclared identifier '…'; did you mean '…'?
- 7 x unknown type name '…'; did you mean '…'?

### P: 64-bit layout (sizeof/offsetof static_assert)
- 396 x static assertion failed due to requirement '…'
- 2 x static assertion failed due to requirement '…': Invalid structure size of CTaskComplexCarDrive
- 2 x static assertion failed due to requirement '…': Invalid structure size of CAESoundManager
- 2 x static assertion failed due to requirement '…': Invalid structure size of CAEAudioHardware
- 2 x static assertion failed due to requirement '…': Invalid structure size of tScriptBrainWaitEntity
- 1 x static assertion failed due to requirement '…': Invalid structure size of CEventAcquaintancePedHate

### other: undeclared/unknown identifier
- 380 x use of undeclared identifier '…'
- 43 x no member named '…' in namespace '…'
- 19 x use of undeclared identifier '…'; did you mean '…'?
- 10 x no member named '…' in '…'
- 1 x no member named '…' in namespace '…'; did you mean '…'?
- 1 x no member named '…' in the global namespace

### W: Win32 API / type
- 302 x use of undeclared identifier '…'
- 43 x unknown type name '…'
- 23 x use of undeclared identifier '…'; did you mean '…'?
- 2 x no member named '…' in the global namespace; did you mean simply '…'?
- 2 x no type named '…' in the global namespace
- 1 x no type named '…' in the global namespace; did you mean '…'?

### F/C: SIMD / intrinsic / asm
- 162 x use of undeclared identifier '…'
- 4 x use of undeclared identifier '…'; did you mean '…'?

### other
- 24 x member reference base type '…' is not a structure or union
- 19 x cannot initialize object parameter of type '…' with an expression of type '…'
- 14 x value of type '…' is not implicitly convertible to '…'
- 14 x duplicate member '…'
- 12 x type-id cannot have a name
- 11 x out-of-line definition of '…' does not match any declaration in '…'

### other: conversion/overload
- 87 x no matching function for call to '…'
- 4 x no matching function for call to object of type '…'
- 2 x cannot initialize a parameter of type '…' with an rvalue of type '…' (vector of 4 '…' values)
- 2 x cannot initialize return object of type '…' with an rvalue of type '…'
- 2 x incompatible pointer types assigning to '…' from '…'
- 1 x cannot initialize a parameter of type '…' with an rvalue of type '…' (vector of 8 '…' values)

### P: pointer/int width (cast/narrowing)
- 70 x cast to smaller integer type '…' from '…' [-Werror,-Wpointer-to-int-cast]
- 10 x cast to '…' from smaller integer type '…' (aka '…') [-Werror,-Wint-to-pointer-cast]
- 9 x cast to '…' from smaller integer type '…' (aka '…') [-Werror,-Wint-to-void-pointer-cast]
- 9 x cast to smaller integer type '…' from '…' [-Werror,-Wvoid-pointer-to-int-cast]
- 3 x cast to '…' from smaller integer type '…' [-Werror,-Wint-to-void-pointer-cast]

