# MAC-A1b census: PortableMac on brew llvm, librw GL3 (no D3D9 defines), native arm64 syntax pass

Command: `cmake --preset PortableMac` (brew LLVM 23.1.2, -isysroot MacOSX27.0.sdk via CMAKE_OSX_SYSROOT=macosx, winshim on -I, `-DRW_GL3 -DLIBRW_SDL3`, no `RW_D3D9/RW_NULL/USE_D3D9`), then
`nice -n 15 python3 tools/standalone/clang_census.py run --mode native --compdb build/PortableMac/compile_commands.json --out DIR -j 6` (1105 TUs, 68 s wall: a lot faster than A1's 430 s because the PCH
closure now has 83 instead of 358 errors) and `clang_census_report.py --out DIR --mode native --top 25`.
Tree: HEAD 828725d2 + the UNCOMMITTED work of MAC-P1 in source/game_sa (static_assert -> VALIDATE_* codemod): the P-stream drop below is P1's, not A1b's. Full per-diagnostic TSV: `.notes/reports/MAC-A1b-census-classified.tsv`.

## Before (MAC-A1, 2026-10-23 morning, Apple-clang preset, RW_NULL+RW_D3D9, no P1 edits) -> after (this run)
| item | before | after |
|---|---|---|
| TUs clean / TUs | 404 / 1105 | 845 / 1105 (260 with errors) |
| unique diagnostics | 3643 | 3033 |
| PCH (StdInc.h) closure errors | 358 | 83 |
| R render | 1262 | 1315 (+53: fakerw.h itself now fails against GL3 librw: rw::d3d::Im3DVertex, sizeof(EngineOpenParams) 8 vs 32, ILP32 asserts; D3D9-only game code is now honest R) |
| P 64-bit | 1027 | 159 (P1's codemod) |
| W platform | 936 | 915 |
| F float/x87/SIMD | 208 | 208 (172 are xmmintrin/emmintrin.h builtins: x86 intrinsics on arm64) |
| unclassified/cascade | 195 | 421 |
| C compiler | 15 | 15 |
| A | 0 | 0 |

Build decision: 260 failing TUs (>= 50) -> `ninja -k 0` build skipped as per the brief.

## Totals (native)

| item | count |
|---|---|
| TUs | 1105 |
| TUs clean | 845 |
| TUs with errors | 260 |
| errors inside the shared PCH closure (StdInc.h), counted once | 83 |
| TUs failed without parsed diagnostics (crash/timeout) | 0 |
| raw diagnostics (every re-parse counted) | 4384 |
| unique diagnostics (file,line,message) | 3033 |

## By category (unique | raw)

| category | unique | raw | files | TUs reaching it |
|---|---|---|---|---|
| W/R: DirectX | 1126 | 1577 | 44 | 73 |
| other: undeclared/unknown identifier | 526 | 766 | 47 | 121 |
| W: Win32 API / type | 375 | 418 | 41 | 41 |
| other | 361 | 569 | 51 | 50 |
| F/C: SIMD / intrinsic / asm | 166 | 331 | 4 | 2 |
| P: pointer/int width (cast/narrowing) | 101 | 131 | 32 | 47 |
| other: conversion/overload | 100 | 103 | 35 | 35 |
| C: syntax / extension | 95 | 191 | 23 | 23 |
| other: undeclared/unknown type | 59 | 117 | 28 | 44 |
| F/C: inline asm / naked | 36 | 36 | 13 | 13 |
| P: 64-bit layout (sizeof/offsetof static_assert) | 34 | 61 | 26 | 36 |
| P: 64-bit (size_t in operator new/delete) | 20 | 20 | 16 | 3 |
| P: integer width / mixed-type overload | 15 | 43 | 13 | 37 |
| C: two-phase lookup / template / incomplete type | 5 | 5 | 2 | 2 |
| W/C: MSVC CRT name | 5 | 5 | 3 | 3 |
| C: nested DMI inside enclosing class | 5 | 5 | 2 | 1 |
| other static_assert | 4 | 6 | 2 | 4 |

## By owner stream (unique diagnostics; stream chosen by file area first, then category)

| stream | unique | files | TUs reaching it |
|---|---|---|---|
| R render | 1315 | 33 | 81 |
| W platform | 915 | 44 | 57 |
| unclassified (cascade / other) | 421 | 71 | 141 |
| F float / x87 / SIMD | 208 | 16 | 15 |
| P 64-bit | 159 | 76 | 90 |
| C compiler | 15 | 6 | 3 |

## By directory (TUs of the directory | clean TUs | unique root diagnostics located in it | by stream letter; u = unclassified)

| directory | TUs | TUs clean | unique diagnostics | A | W | R | P | F | C | u |
|---|---|---|---|---|---|---|---|---|---|---|
| source/standalone | 40 | 26 | 1005 | 0 | 0 | 998 | 0 | 7 | 0 | 0 |
| source/game_sa/Audio | 48 | 14 | 601 | 0 | 527 | 9 | 4 | 1 | 1 | 59 |
| source/game_sa (root) | 251 | 181 | 404 | 0 | 61 | 18 | 50 | 12 | 13 | 250 |
| source/app | 19 | 7 | 269 | 0 | 269 | 0 | 0 | 0 | 0 | 0 |
| source/game_sa/Pipelines | 7 | 0 | 206 | 0 | 0 | 206 | 0 | 0 | 0 | 0 |
| (outside source/) | 0 | 0 | 176 | 0 | 0 | 0 | 0 | 172 | 0 | 4 |
| source/game_sa/Entity | 30 | 15 | 120 | 0 | 0 | 0 | 49 | 2 | 0 | 69 |
| source/game_sa/RenderWare | 3 | 0 | 69 | 0 | 0 | 69 | 0 | 0 | 0 | 0 |
| source/oswrapper | 1 | 0 | 46 | 0 | 46 | 0 | 0 | 0 | 0 | 0 |
| source/game_sa/Scripts | 68 | 3 | 45 | 0 | 1 | 0 | 23 | 5 | 0 | 16 |
| source/extensions | 3 | 1 | 19 | 0 | 6 | 0 | 1 | 0 | 0 | 12 |
| source/fakerw | 0 | 0 | 15 | 0 | 0 | 15 | 0 | 0 | 0 | 0 |
| source/game_sa/Core | 15 | 9 | 12 | 0 | 0 | 0 | 3 | 9 | 0 | 0 |
| source/game_sa/Events | 89 | 84 | 8 | 0 | 0 | 0 | 8 | 0 | 0 | 0 |
| source/. | 0 | 0 | 8 | 0 | 4 | 0 | 0 | 0 | 0 | 4 |
| source/game_sa/Models | 12 | 9 | 7 | 0 | 0 | 0 | 5 | 0 | 0 | 2 |
| source/game_sa/Fx | 56 | 53 | 6 | 0 | 0 | 0 | 2 | 0 | 0 | 4 |
| source/toolsmenu | 41 | 36 | 4 | 0 | 1 | 0 | 2 | 0 | 0 | 1 |
| source/game_sa/Tasks | 333 | 329 | 4 | 0 | 0 | 0 | 4 | 0 | 0 | 0 |
| source/game_sa/Collision | 19 | 16 | 3 | 0 | 0 | 0 | 3 | 0 | 0 | 0 |
| source/game_sa/Plugins | 7 | 5 | 2 | 0 | 0 | 0 | 2 | 0 | 0 | 0 |
| source/game_sa/Animation | 9 | 9 | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| source/game_sa/Enums | 1 | 1 | 1 | 0 | 0 | 0 | 0 | 0 | 1 | 0 |
| source/game_sa/Attractors | 12 | 11 | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| source/game_sa/Text | 4 | 3 | 1 | 0 | 0 | 0 | 1 | 0 | 0 | 0 |
| source/game_sa/Plant | 4 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| source/game_sa/Ragdoll | 4 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| source (root) | 3 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| source/game_sa/DecisionMakers | 5 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| source/game_sa/Conversations | 4 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| source/game_sa/Interior | 7 | 7 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| source/game_sa/Frontend | 6 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| source/game_sa/Memory | 4 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

## Top 25 files by unique diagnostics (file the diagnostic points to)

| file | unique | main categories |
|---|---|---|
| source/standalone/rw/pipeline_matfx.cpp | 351 | W/R: DirectX 342, other: undeclared/unknown identifi 7, other 2 |
| source/standalone/rw/engine.cpp | 184 | W/R: DirectX 97, other: undeclared/unknown identifi 52, W: Win32 API / type 27 |
| source/standalone/rw/pipeline.cpp | 130 | W/R: DirectX 127, other: undeclared/unknown identifi 3 |
| source/game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline.cpp | 97 | W/R: DirectX 96, C: syntax / extension 1 |
| source/game_sa/Audio/Loaders/AEMFDecoder.cpp | 97 | other: undeclared/unknown identifi 57, W: Win32 API / type 34, other 4 |
| /opt/homebrew/Cellar/llvm/23.1.2/lib/clang/23/include/emmintrin.h | 84 | F/C: SIMD / intrinsic / asm 83, other 1 |
| source/game_sa/Audio/Hardware/AEStreamingChannel.cpp | 81 | other: undeclared/unknown identifi 64, W/R: DirectX 7, other 6 |
| source/game_sa/Audio/Hardware/AEAudioTap.h | 80 | other 24, C: syntax / extension 21, W/R: DirectX 15 |
| source/standalone/rw/rwd3d_ff.cpp | 72 | W/R: DirectX 53, other 14, C: syntax / extension 3 |
| source/app/platform/win/WinPs.cpp | 68 | W: Win32 API / type 26, W/R: DirectX 18, other: undeclared/unknown identifi 16 |
| source/app/platform/win/WinPlatform.cpp | 66 | W: Win32 API / type 59, W/R: DirectX 2, other: undeclared/unknown identifi 2 |
| /opt/homebrew/Cellar/llvm/23.1.2/lib/clang/23/include/xmmintrin.h | 63 | F/C: SIMD / intrinsic / asm 60, other: conversion/overload 2, other 1 |
| source/game_sa/Audio/Hardware/AEStaticChannel.cpp | 61 | other: undeclared/unknown identifi 50, other 6, W/R: DirectX 5 |
| source/standalone/rw/camera.cpp | 61 | other: undeclared/unknown identifi 28, W/R: DirectX 28, other: undeclared/unknown type 2 |
| source/standalone/rw/im2d_im3d.cpp | 57 | W/R: DirectX 51, other 4, P: 64-bit layout (sizeof/offsetof  1 |
| source/standalone/rw/renderstate.cpp | 54 | W/R: DirectX 52, other: undeclared/unknown identifi 2 |
| source/app/platform/win/WinMain.cpp | 51 | W: Win32 API / type 23, other: undeclared/unknown identifi 20, other: undeclared/unknown type 3 |
| source/game_sa/Entity/Vehicle/Boat.cpp | 49 | other 49 |
| source/game_sa/WaterLevel.cpp | 47 | other 38, C: syntax / extension 5, other: conversion/overload 4 |
| source/oswrapper/oswrapper_win.cpp | 46 | W: Win32 API / type 42, other 2, other: undeclared/unknown identifi 2 |
| source/game_sa/Clouds.cpp | 44 | other 44 |
| source/game_sa/RenderWare/D3DResourceSystem.cpp | 44 | W/R: DirectX 22, C: syntax / extension 13, other: undeclared/unknown identifi 9 |
| source/game_sa/Pipelines/CustomBuilding/CustomBuildingDNPipeline.cpp | 44 | W/R: DirectX 43, P: pointer/int width (cast/narrowi 1 |
| source/game_sa/Pipelines/CustomBuilding/CustomBuildingPipeline.cpp | 43 | W/R: DirectX 43 |
| source/game_sa/CdStreamInfo.cpp | 39 | W: Win32 API / type 34, other: undeclared/unknown identifi 4, other: undeclared/unknown type 1 |

## Top 25 TUs by diagnostics (headers re-counted per TU)

| TU | diagnostics |
|---|---|
| source/standalone/rw/pipeline_matfx.cpp | 351 |
| source/standalone/rw/pipeline_skin_cpu.cpp | 184 |
| source/standalone/rw/engine.cpp | 184 |
| source/standalone/rw/pipeline_skin.cpp | 180 |
| source/game_sa/Audio/Hardware/AEStreamingChannel.cpp | 180 |
| source/game_sa/Audio/Hardware/AEStaticChannel.cpp | 156 |
| source/standalone/rw/pipeline.cpp | 130 |
| source/game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline.cpp | 118 |
| source/game_sa/Audio/Hardware/AEAudioHardware.cpp | 118 |
| source/game_sa/Audio/Loaders/AEMFDecoder.cpp | 102 |
| source/game_sa/Audio/AEStreamThread.cpp | 98 |
| source/game_sa/Audio/AESmoothFadeThread.cpp | 95 |
| <PCH StdInc.h closure> | 83 |
| source/standalone/rw/rwd3d_ff.cpp | 72 |
| source/app/platform/win/WinPs.cpp | 68 |
| source/app/platform/win/WinPlatform.cpp | 68 |
| source/game_sa/Pipelines/CustomBuilding/CustomBuildingDNPipeline.cpp | 66 |
| source/game_sa/Pipelines/CustomBuilding/CustomBuildingPipeline.cpp | 65 |
| source/app/platform/win/WinMain.cpp | 63 |
| source/standalone/rw/camera.cpp | 61 |
| source/standalone/rw/im2d_im3d.cpp | 57 |
| source/standalone/rw/renderstate.cpp | 54 |
| source/game_sa/Audio/Hardware/AEAudioChannel.cpp | 53 |
| source/game_sa/Entity/Vehicle/Boat.cpp | 51 |
| source/app/platform/win/VideoPlayer/VideoPlayer.cpp | 50 |

## Most frequent messages (unique, normalised) in the biggest categories

### W/R: DirectX
- 913 x use of undeclared identifier '…'
- 156 x unknown type name '…'
- 51 x use of undeclared identifier '…'; did you mean '…'?
- 6 x unknown type name '…'; did you mean '…'?

### other: undeclared/unknown identifier
- 439 x use of undeclared identifier '…'
- 57 x no member named '…' in namespace '…'
- 18 x use of undeclared identifier '…'; did you mean '…'?
- 8 x no member named '…' in '…'
- 3 x no member named '…' in namespace '…'; did you mean '…'?
- 1 x no member named '…' in the global namespace

### W: Win32 API / type
- 301 x use of undeclared identifier '…'
- 43 x unknown type name '…'
- 25 x use of undeclared identifier '…'; did you mean '…'?
- 2 x no member named '…' in the global namespace; did you mean simply '…'?
- 2 x no type named '…' in the global namespace
- 1 x no type named '…' in the global namespace; did you mean '…'?

### other
- 161 x member reference base type '…' (aka '…') is not a structure or union
- 85 x member reference base type '…' is not a structure or union
- 14 x cannot initialize object parameter of type '…' with an expression of type '…'
- 14 x value of type '…' is not implicitly convertible to '…'
- 14 x duplicate member '…'
- 12 x type-id cannot have a name

### F/C: SIMD / intrinsic / asm
- 162 x use of undeclared identifier '…'
- 4 x use of undeclared identifier '…'; did you mean '…'?

### P: pointer/int width (cast/narrowing)
- 70 x cast to smaller integer type '…' from '…' [-Werror,-Wpointer-to-int-cast]
- 10 x cast to '…' from smaller integer type '…' (aka '…') [-Werror,-Wint-to-pointer-cast]
- 9 x cast to '…' from smaller integer type '…' (aka '…') [-Werror,-Wint-to-void-pointer-cast]
- 9 x cast to smaller integer type '…' from '…' [-Werror,-Wvoid-pointer-to-int-cast]
- 3 x cast to '…' from smaller integer type '…' [-Werror,-Wint-to-void-pointer-cast]

### other: conversion/overload
- 86 x no matching function for call to '…'
- 4 x no matching function for call to object of type '…'
- 2 x cannot initialize a parameter of type '…' with an rvalue of type '…' (vector of 4 '…' values)
- 2 x cannot initialize return object of type '…' with an rvalue of type '…'
- 2 x incompatible pointer types assigning to '…' from '…'
- 1 x cannot initialize a parameter of type '…' with an rvalue of type '…' (vector of 8 '…' values)

### C: syntax / extension
- 30 x expected expression
- 21 x expected '…'
- 16 x expected '…' at end of declaration list
- 12 x expected '…' after top level declarator
- 5 x statement requires expression of integer type ('…' invalid)
- 2 x expected '…' at end of declaration

