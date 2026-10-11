# Stream C plan: compilers (slice C1 result), 2026-10-11, branch reverse/interior_c

Goal of C1: a compiler-generated inventory of what blocks building the game with clang, in two passes, without changing game behaviour.
Pass 1 = clang-cl `--target=i686-pc-windows-msvc` (same ABI, same MSVC STL / Windows SDK headers, same conan packages as the MSVC build).
Pass 2 = native `arm64-apple-macos15`, `-fsyntax-only`, Windows headers replaced by stubs, only to COUNT and classify (nothing fixed there).
Agent report: `.notes/reports/C1.md`. Raw data: `.notes/stream_c/*.tsv`. Tools: `tools/standalone/clang_census.py`, `clang_census_report.py`, `clang-cl/`, `winstub/`.

## 1. Result in six lines
* The tree is much closer to clang than the heuristic inventory assumed. With the MSVC-compatible driver **1,064 of 1,115 TUs compiled as they were** once the PCH closure (StdInc.h) parsed; the PCH closure itself had 30 errors / 8 root causes that blocked every TU.
* After 5 behaviour-neutral fix commits: clang-cl i686 **syntax pass 1,114 of 1,115 TUs clean** (also with `-fno-delayed-template-parsing`), and the real `/O2` build with the `StandaloneClangCL` CMake preset yields **1,117 of 1,118 objects**. The one failure is `AEAudioUtility.cpp` (`__declspec(naked)` on a member function = inline asm, listed, not ported).
* MSVC object code is unchanged: 24 sampled TUs compiled by `cl.exe` (wine) before/after the fixes, no PCH, same flags as `build/StandaloneRelease`: code bytes identical in all 24; relocation targets identical except the mangled names of types that moved (`HookInstallOptions`, `HookFilter::Cutoffs`, the unnamed `WakeSegmentPartColors` element type).
* Native arm64 (pass 2): **0 of 1,115 TUs clean; 4,340 unique root diagnostics** (file,line,message; 62,099 raw). By owner stream: W 1,294, R 1,220, P 920, F 211, A 157, C 132 (4 compiler issues + 128 libc++ gaps), 406 unclassified (cascades).
* **797 of the 813 size/offset asserts that fail on arm64 get BIGGER** (pointer / `size_t` / `long` growth; the other 14 are artefacts of stubbed Windows types): 812 unique failures in 524 files = ~94% of all `VALIDATE_SIZE` / `VALIDATE_OFFSET` / `static_assert(sizeof/offsetof)`. The P-1 `VALIDATE_SIZE_X86` split is mandatory and large (section 5). `-mms-bitfields` (MSVC bit-field layout for clang on arm64) gives the SAME failure set as without it: bit-field layout is not an extra problem.
* Rest of stream C: a compile gate in the dev loop (cheap), the libc++ gaps (`views::enumerate`, `ranges::shift_right`, `<stacktrace>`, `from_chars(double)`), a Windows-type / CRT-name shim header, the SSE skin kernels and an evaluation-order / FP-contraction audit (section 7).

## 2. Reproduce
```
brew install llvm lld                                   # clang-cl 23.1.2, lld-link, llvm-lib (installed on this Mac: /opt/homebrew/opt/llvm)
tools/standalone/clang-cl/make_generators.sh           # build/ClangCL/generators = build/Release/generators + the clang-cl user toolchain (no second conan install: that rewrites ConanPresets.json and source/libs/imgui under running builds)
cmake --preset StandaloneClangCL -DGTASA_PYTHON=<py>   # configure ~2 s; toolchain tools/standalone/clang-cl/toolchain-clang-cl.cmake
taskpolicy -b nice -n 15 ninja -k 0 -j5 -C build/StandaloneClangCL gta_reversed   # ~60 min on the loaded Mac; expected: exactly 1 FAILED (AEAudioUtility.cpp.obj)
# census: no CMake needed, follows build/StandaloneRelease/compile_commands.json (TU list + -D set)
python3 tools/standalone/clang_census.py run --mode msvc   --out DIR -j 8 [--extra=-fno-delayed-template-parsing]   # 1-5 min (own PCH)
python3 tools/standalone/clang_census.py run --mode native --out DIR -j 8                                          # 22-40 min
python3 tools/standalone/clang_census_report.py --out DIR --mode msvc|native --top 20 [--tsv DIR/classified.tsv]
```
Caveats. Editing a PCH header while a build runs makes clang say "file ... has been modified since the precompiled header": re-run ninja. The preset adds `-Wno-error=c++11-narrowing` only because vendor/librw (`d3d/xbox.cpp`) narrows constants; the game sources are kept strict by the census. `ConanPresets.json` and `build/Release/generators` are never touched; `conanprofile-clangcl-release.txt` parses (`conan profile show`) but was NOT used for an install (`make_generators.sh` is the verified route). The clang-cl objects are a COMPILE GATE, not a fidelity reference: `NOTSA_CLANG_SSE2_*` (below) and clang's own x87/SSE choices are not exe-equivalent. Native census flags: `-target arm64-apple-macos15 -std=c++23 -fsyntax-only -fms-extensions -mms-bitfields -fno-delayed-template-parsing -Wno-everything -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast -Werror=pointer-integer-compare -Werror=int-conversion`, one PCH over StdInc.h built with `-fallow-pch-with-compiler-errors`.

## 3. Pass 1: clang-cl i686-pc-windows-msvc

### 3.1 Initial state (before any fix)
PCH closure (StdInc.h): 30 diagnostics, 8 root causes (clang rejects, MSVC accepts):

| root cause | diagnostics | header |
|---|---|---|
| nested type with default member initializers used as `= {}` default argument / `static inline` member inside the enclosing class ("default member initializer needed within definition of enclosing class") | RHManager 5, AEVehicleAudioEntity 2, WaterLevel 3, Collision 1 | RHManager.h, AEVehicleAudioEntity.h, WaterLevel.h, Collision.h |
| non-dependent call on an incomplete type inside a template (`GetPtrNodeDoubleLinkPool()->IsObjectValid`) | 2 | Core/PtrListDoubleLink.h, PtrListSingleLink.h |
| friend `swap` naming a member that does not exist (`m_FirstFreeSlot`, never instantiated) | 2 | Core/Pool.h |
| `auto` return type used before its definition inside the class (`GetType()` in `GetIsType*`) | 7 | Entity/Entity.h |
| `using Base::CPtrList;` (injected-class-name of a dependent base) | 4 (+2 cascade static_asserts in Sector.h, RepeatSector.h) | Core/PtrListDoubleLink.h |
| class named in a nested-name-specifier while incomplete (`CAnimBlendAssociation::FromLink`), `GetNode` indexing `std::span<incomplete>` | 2 | Animation/AnimBlendAssociation.h |

With the PCH closure fixed, TU level (clang-cl, syntax only, MS-compatible, delayed template parsing as in MSVC):

| item | count |
|---|---|
| TUs | 1115 |
| TUs clean | 1064 |
| TUs with errors | 51 |
| errors inside the shared PCH closure (StdInc.h), counted once | 0 |
| TUs failed without parsed diagnostics (crash/timeout) | 0 |
| raw diagnostics (every re-parse counted) | 83 |
| unique diagnostics (file,line,message) | 29 |

| category | unique | raw | files | TUs reaching it |
|---|---|---|---|---|
| other | 11 | 11 | 5 | 5 |
| P: pointer/int width (cast/narrowing) | 5 | 5 | 2 | 2 |
| C: nested DMI inside enclosing class | 5 | 32 | 3 | 15 |
| other: conversion/overload | 5 | 5 | 1 | 1 |
| other: undeclared/unknown identifier | 1 | 27 | 1 | 27 |
| C: two-phase lookup / template / incomplete type | 1 | 2 | 1 | 2 |
| F/C: inline asm / naked | 1 | 1 | 1 | 1 |

All files with a diagnostic (14):

| file | unique | main categories |
|---|---|---|
| source/game_sa/Door.cpp | 6 | other 6 |
| source/toolsmenu/DebugModules/CloudsDebugModule.cpp | 5 | other: conversion/overload 5 |
| source/game_sa/Sprite.cpp | 4 | P: pointer/int width (cast/narrowi 4 |
| source/game_sa/Clouds.h | 2 | C: nested DMI inside enclosing cla 2 |
| source/toolsmenu/DebugModules/HooksDebugModule/HookFilter.h | 2 | C: nested DMI inside enclosing cla 2 |
| source/game_sa/Entity/Object/Object.cpp | 2 | other 2 |
| source/game_sa/Tasks/TaskTypes/SeekEntity/TaskComplexSeekEntity.h | 1 | other: undeclared/unknown identifi 1 |
| source/game_sa/Tasks/Allocators/TaskAllocatorKillOnFoot.cpp | 1 | P: pointer/int width (cast/narrowi 1 |
| source/game_sa/Replay.cpp | 1 | other 1 |
| source/toolsmenu/DebugModules/HooksDebugModule/HooksDebugModule.h | 1 | C: nested DMI inside enclosing cla 1 |
| source/./extensions/File.hpp | 1 | C: two-phase lookup / template / i 1 |
| source/game_sa/Audio/Entities/AEWeaponAudioEntity.cpp | 1 | other 1 |
| source/game_sa/Audio/AEAudioUtility.cpp | 1 | F/C: inline asm / naked 1 |
| source/game_sa/Tasks/TaskTypes/TaskComplexStuckInAir.cpp | 1 | other 1 |

Top TUs (headers re-counted per TU):

| TU | diagnostics |
|---|---|
| source/toolsmenu/DebugModules/CloudsDebugModule.cpp | 7 |
| source/game_sa/Door.cpp | 6 |
| source/game_sa/Sprite.cpp | 4 |
| source/toolsmenu/DebugModules/DebugModules.cpp | 3 |
| source/toolsmenu/DebugModules/HooksDebugModule/HooksDebugModule.cpp | 3 |
| source/InjectHooksMain.cpp | 3 |
| source/app/app_game.cpp | 2 |
| source/toolsmenu/DebugModules/HooksDebugModule/RListFilterer.cpp | 2 |
| source/game_sa/Mirrors.cpp | 2 |
| source/toolsmenu/DebugModules/HooksDebugModule/HookFilter.cpp | 2 |
| source/game_sa/Clouds.cpp | 2 |
| source/game_sa/Entity/Object/Object.cpp | 2 |
| source/app/app.cpp | 2 |
| source/game_sa/Birds.cpp | 2 |
| source/toolsmenu/DebugModules/HooksDebugModule/RListBuilder.cpp | 2 |
| source/game_sa/PostEffects.cpp | 2 |
| source/game_sa/PointLights.cpp | 2 |
| source/game_sa/Game.cpp | 2 |
| source/game_sa/Tasks/TaskTypes/TaskComplexSeekEntityAiming.cpp | 1 |
| source/game_sa/Tasks/TaskTypes/TaskComplexFollowLeaderInFormation.cpp | 1 |

### 3.2 Fixes (this slice; all behaviour neutral; one commit per kind)

| kind | sites | commit | notes |
|---|---|---|---|
| nested types with DMI inside their enclosing class | 16 diagnostics, 6 headers | 0bc515d1 | `HookInstallOptions` and `HookFilter::Cutoffs` to namespace scope (nested alias keeps the old names); `static inline` debug statics of CClouds / CWaterLevel / CCollision / CAEVehicleAudioEntity become `static` + `inline` out-of-class definitions at the end of the header (line numbers inside the headers unchanged) |
| two-phase lookup, completeness, constructor calls | 20 | 12b3c2d8 | explicit return types (`File::CloseFile`, `CEntity::GetType`), dependent name for the incomplete class, complete pool types, `std::construct_at(this, ...)` instead of `this->T::T(...)` inside a class template, `using Base::Base`, CPool swap member name, `CAnimBlendAssociation::GetNode` defined inline after `CAnimBlendNode`, `CAEWeaponAudioEntity::Constructor` (finding 1) |
| `RH_ScopedGlobalInstall` used for member functions (`&Member`, MSVC only) | 9 | 5dd628be | CDoor 6, CObject::Save/Load, CTaskComplexStuckInAir::CreateSubTask -> `RH_ScopedInstall` (same pointer, same names). The other 573 `RH_ScopedGlobalInstall` uses (103 files) are real free functions and compile |
| list-init narrowing, jump over initialisation | 6 | ea7a94f1 | `static_cast` in CSprite (4) and KillOnFoot (1); braces for one `case` of `CReplay::ProcessReplayCamera`, placed on the existing lines |
| SSE intrinsics under `/arch:IA32` | 2 TUs (+2 cores) | e805cf80 | clang rejects `_mm_*` in functions without the sse2 target: `NOTSA_CLANG_SSE2_BEGIN/END` (rwmath_exact.h) = `clang attribute push(target("sse2"))` on clang, empty for MSVC |
| toolchain, preset, census tools, Windows header stubs | - | 36e7e72c | |

MSVC A/B (cl.exe under wine, flags of `build/StandaloneRelease`, no PCH, worktree of the commit before C1 vs working tree; fingerprint = code section bytes + relocation names, string-literal and anonymous-namespace names normalised for the different root path): Door, Object, TaskComplexStuckInAir, Replay, Sprite, TaskAllocatorKillOnFoot, AEWeaponAudioEntity, AEVehicleAudioEntity, WaterLevel, Collision, Clouds, CloudsDebugModule, HookFilter, TaskComplexSeekEntityAiming, TaskComplexFollowLeaderInFormation, AnimBlendAssociation, TaskSimpleRunNamedAnim, Entity, World, Automobile, Pools, Cover, pipeline_skin, pipeline_skin_cpu: code identical in all 24. One real regression was caught by this check and fixed: closing a `case` brace on its own line in Replay.cpp shifted a `__LINE__` immediate (now `break; }`); likewise the WaterLevel.h edit keeps its 3 lines.

### 3.3 State after the fixes
* Syntax pass, MS-compatible and with `-fno-delayed-template-parsing` (identical result):

| item | count |
|---|---|
| TUs | 1115 |
| TUs clean | 1114 |
| TUs with errors | 1 |
| errors inside the shared PCH closure (StdInc.h), counted once | 0 |
| TUs failed without parsed diagnostics (crash/timeout) | 0 |
| raw diagnostics (every re-parse counted) | 1 |
| unique diagnostics (file,line,message) | 1 |

  Remaining: `game_sa/Audio/AEAudioUtility.cpp:90 'naked' attribute only applies to non-member functions`.
* Real build (`StandaloneClangCL`, /O2, x87 `/arch:IA32` preset flags): 1,117 of 1,118 objects, same single failure. The unguarded MS inline `__asm` blocks (Matrix.cpp, Quaternion.cpp, Radar.cpp, PathFind.cpp, Cover.cpp, EntryExit.cpp, Group13_15.cpp, Maths.cpp, Placeable.cpp, FtolExe.cpp ...) **are accepted by clang-cl** (equivalence with the exe is untested and not needed for a gate). X87Intrinsics.h, IdleCam.cpp (`X87SinP1Half`), BoneNode.cpp and Fp.h are already guarded `defined(_MSC_VER) && !defined(__clang__) && defined(_M_IX86)`: under clang-cl they take the `std::` fallback, i.e. the gate does NOT see their asm and a clang-cl build has different x87 numerics there. Link not attempted (the missing object blocks it; `lld-link` is installed; the conan packages are the MSVC-ABI libraries).
* Inline asm / naked, non-comment lines, 66 `__asm` + 22 `__declspec(naked)` in 17 files (file: asm lines / naked): WindowedMode.cpp 14/1 (excluded with librw), X87Intrinsics.h 12/11, FtolExe.cpp 7/7, Matrix.cpp 7/0, Radar.cpp 5/2, Group13_15.cpp 4/0, Quaternion.cpp 2/0, Cover.cpp 2/0, EntryExit.cpp 2/0, PathFind.cpp 2/0, AEAudioUtility.cpp 1/1, Placeable.cpp 1, Maths.cpp 1, IdleCam.cpp 1, BoneNode.cpp 1, Group24b.cpp 1, rwplcore.h 3 (RW original, excluded with librw). Only AEAudioUtility fails with clang-cl; on arm64 13 files fail with "unsupported architecture for MS-style inline assembly" (section 4).
  `CAEAudioUtility::AudioLog10` cannot get a forwarding wrapper: callers rely on the UNROUNDED ST0 result (comment in the file), a float-returning thunk would round once more. It needs the F-4 treatment (software x87), not a compiler workaround.

## 4. Pass 2: native arm64-apple-macos15, `-fsyntax-only`
Setup: same TU list and `-D` set as the Windows run build, so `USE_D3D9` / `USE_DSOUND` / `NOTSA_STANDALONE_RUN` are ON: the W/R counts are the uses the portable configuration must REPLACE or compile out, not what remains after stream A/W work. Windows SDK headers replaced by `tools/standalone/winstub` (scalar typedefs only; every API / struct stays undeclared on purpose; 70 files, the empty ones are placeholders so that `#include` does not abort the TU; `<stacktrace>` is an empty stub = a libc++ gap, section 6). 341 errors inside the PCH closure (the StdInc.h include tree) are counted ONCE as the pseudo TU `<PCH StdInc.h closure>`: "TUs reaching it" is an under-count for PCH-level causes (almost all P asserts). Unique = distinct (file,line,message). Cascades (an undeclared Windows type makes the next ten lines fail) inflate W/R/"unclassified"; the unique numbers are a ranking, not a work estimate.

| item | count |
|---|---|
| TUs | 1115 |
| TUs clean | 0 |
| TUs with errors | 1115 |
| errors inside the shared PCH closure (StdInc.h), counted once | 341 |
| TUs failed without parsed diagnostics (crash/timeout) | 0 |
| raw diagnostics (every re-parse counted) | 62099 |
| unique diagnostics (file,line,message) | 4340 |

| category | unique | raw | files | TUs reaching it |
|---|---|---|---|---|
| W/R: DirectX | 1123 | 1798 | 45 | 74 |
| P: 64-bit layout (sizeof/offsetof static_assert) | 812 | 29998 | 524 | 1116 |
| other: undeclared/unknown identifier | 598 | 847 | 79 | 152 |
| W: Win32 API / type | 481 | 527 | 47 | 46 |
| W/C: MSVC CRT name | 307 | 317 | 103 | 107 |
| other: conversion/overload | 208 | 26908 | 79 | 1115 |
| F/C: SIMD / intrinsic / asm | 170 | 339 | 4 | 2 |
| other | 146 | 317 | 39 | 68 |
| C: libc++ / standard-library gap | 134 | 142 | 78 | 75 |
| P: pointer/int width (cast/narrowing) | 130 | 395 | 37 | 166 |
| C: syntax / extension | 84 | 175 | 22 | 24 |
| other: undeclared/unknown type | 59 | 119 | 23 | 43 |
| F/C: inline asm / naked | 36 | 112 | 13 | 49 |
| P: 64-bit (size_t in operator new/delete) | 20 | 20 | 16 | 3 |
| C: two-phase lookup / template / incomplete type | 16 | 40 | 5 | 16 |
| P: integer width / mixed-type overload | 15 | 44 | 13 | 38 |
| other static_assert | 1 | 1 | 1 | 1 |

| stream | unique | files | TUs reaching it |
|---|---|---|---|
| W platform | 1294 | 142 | 149 |
| R render | 1220 | 39 | 88 |
| P 64-bit | 920 | 541 | 602 |
| unclassified (cascade / other) | 406 | 104 | 174 |
| F float / x87 / SIMD | 211 | 16 | 51 |
| A fixed addresses / hooks | 157 | 9 | 1116 |
| C compiler / standard library | 128 | 74 | 71 |
| C compiler | 4 | 4 | 3 |

Top 20 files by unique diagnostics:

| file | unique | main categories |
|---|---|---|
| source/standalone/rw/pipeline_matfx.cpp | 350 | W/R: DirectX 342, other: undeclared/unknown identifi 6, other 2 |
| source/standalone/rw/pipeline.cpp | 127 | W/R: DirectX 127 |
| source/game_sa/Audio/Loaders/AEMFDecoder.cpp | 114 | other: undeclared/unknown identifi 57, W: Win32 API / type 42, C: two-phase lookup / template / i 8 |
| source/standalone/rw/engine.cpp | 105 | W/R: DirectX 80, W: Win32 API / type 13, other: undeclared/unknown identifi 9 |
| source/game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline.cpp | 97 | W/R: DirectX 96, C: syntax / extension 1 |
| source/standalone/Fixups.cpp | 90 | W: Win32 API / type 41, other: undeclared/unknown identifi 23, W/C: MSVC CRT name 12 |
| <clang>/include/emmintrin.h | 86 | F/C: SIMD / intrinsic / asm 85, other 1 |
| source/game_sa/Audio/Hardware/AEAudioTap.h | 83 | other 28, C: syntax / extension 21, W/R: DirectX 15 |
| source/game_sa/Audio/Hardware/AEStreamingChannel.cpp | 76 | other: undeclared/unknown identifi 59, W/R: DirectX 7, other 6 |
| source/standalone/rw/rwd3d_ff.cpp | 67 | W/R: DirectX 53, other 9, C: syntax / extension 3 |
| source/app/platform/win/WinPs.cpp | 67 | W: Win32 API / type 26, W/R: DirectX 18, other: undeclared/unknown identifi 14 |
| source/app/platform/win/WinPlatform.cpp | 66 | W: Win32 API / type 59, W/R: DirectX 2, other: undeclared/unknown identifi 2 |
| <clang>/include/xmmintrin.h | 64 | F/C: SIMD / intrinsic / asm 62, other 1, other: conversion/overload 1 |
| source/standalone/rw/im2d_im3d.cpp | 54 | W/R: DirectX 51, other 2, other: undeclared/unknown identifi 1 |
| source/standalone/ImageDiag.cpp | 53 | W: Win32 API / type 27, other: undeclared/unknown identifi 11, P: pointer/int width (cast/narrowi 6 |
| source/standalone/rw/renderstate.cpp | 53 | W/R: DirectX 52, other: undeclared/unknown identifi 1 |
| source/oswrapper/oswrapper_win.cpp | 53 | W: Win32 API / type 43, W/C: MSVC CRT name 6, other 2 |
| source/app/platform/win/WinMain.cpp | 52 | W: Win32 API / type 23, other: undeclared/unknown identifi 20, W/C: MSVC CRT name 3 |
| source/game_sa/Audio/Hardware/AEStaticChannel.cpp | 51 | other: undeclared/unknown identifi 40, other 6, W/R: DirectX 5 |
| source/game_sa/RenderWare/D3DResourceSystem.cpp | 48 | W/R: DirectX 24, C: syntax / extension 13, other: undeclared/unknown identifi 9 |

Top 20 TUs (the PCH closure row is the shared StdInc.h tree):

| TU | diagnostics |
|---|---|
| source/standalone/rw/pipeline_matfx.cpp | 366 |
| <PCH StdInc.h closure> | 341 |
| source/InjectHooksMain.cpp | 321 |
| source/standalone/rw/pipeline_skin_cpu.cpp | 201 |
| source/standalone/rw/pipeline_skin.cpp | 198 |
| source/game_sa/Audio/Hardware/AEStreamingChannel.cpp | 188 |
| source/game_sa/Audio/Hardware/AEStaticChannel.cpp | 158 |
| source/standalone/rw/pipeline.cpp | 143 |
| source/game_sa/Audio/Hardware/AEAudioHardware.cpp | 141 |
| source/game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline.cpp | 125 |
| source/game_sa/Audio/Loaders/AEMFDecoder.cpp | 123 |
| source/standalone/rw/engine.cpp | 121 |
| source/game_sa/Audio/AEStreamThread.cpp | 114 |
| source/game_sa/Audio/AESmoothFadeThread.cpp | 110 |
| source/game_sa/Events/EventHandler.cpp | 96 |
| source/standalone/Fixups.cpp | 92 |
| source/app/platform/win/WinMain.cpp | 76 |
| source/game_sa/Pipelines/CustomBuilding/CustomBuildingDNPipeline.cpp | 72 |
| source/app/platform/win/WinPlatform.cpp | 72 |
| source/game_sa/Pipelines/CustomBuilding/CustomBuildingPipeline.cpp | 71 |

(`Audio/Hardware/AEAudioTap.h` and the Audio/Hardware files belong to the running AUDIO agent; their numbers are a snapshot of the working tree.)
Full list: `.notes/stream_c/native_arm64_unique_diagnostics.tsv` (category, file, line, message; 4,340 rows); pass 1: `.notes/stream_c/clangcl_i686_initial_diagnostics.tsv`.

## 5. Real numbers for streams P / W / F / R / A (replace the heuristic ones of PORTABILITY_INVENTORY.md)
Source: native census above (unique = distinct file,line,message; "files" = files the diagnostic points to). Heuristic column = `.notes/PORTABILITY_INVENTORY.md` (counts of text patterns).

### P: 64-bit
| item | inventory (h) | census (real) |
|---|---|---|
| size / offset asserts that FAIL on arm64 | 804 `VALIDATE_SIZE` + 21 `VALIDATE_OFFSET` + 38 `static_assert(sizeof..)`, "334 pointer types (h)" | **812 unique failures in 524 files**: 510 `VALIDATE_SIZE`, 275 `static_assert(offsetof(..) == 0x..)`, 27 other `sizeof` asserts; 797 of 813 with a size note grow (x1.1-x2.0), the other 14 are stub artefacts. Not a single one flips with `-mms-bitfields` |
| ...by directory (unique failures) | Tasks 101 / root 69 / Events 48 / Audio 36 / Entity 17 ... (types) | Scripts 218 (almost all `static_assert(offsetof(CVehicle / CPed / ..., m_x) == 0x..)` in the ported `Group*.cpp` command files: exe-layout constants of in-memory classes), Tasks 174, game_sa root 140, Events 86, Audio 52, Entity 43, Core 14, Fx 14, Models 12, Animation 10, Interior 9, Collision 7, fakerw 6, Plugins 6, Plant 5, Ragdoll 4, Pipelines 3, other 8 |
| pointer <-> int casts | 24 + 31 + 3 sites in 13 + 17 + 2 files | **130 unique sites in 37 files** (77 pointer -> smaller int, 34 int -> pointer, 19 `void*` variants): Entity.cpp 13, Object.cpp 9, Vehicle.cpp 9, Fixups.cpp 9, DataImage.cpp 8, VisibilityPlugins.cpp 7, AttractorScanner.cpp 6, ImageDiag.cpp 6, MemoryMgr.cpp 4, VehicleModelInfo.cpp 4, Pickups.cpp 4, RwHelper.cpp 4 (the `(uint32)this + id` effect-id hashes are the typical case) |
| `operator new` / `delete` with a 32-bit size parameter | not counted | 20 sites in 16 files (PedAttractor.h, ColModel.h, ColStore.h, EntryInfoNode.h, Building.h, Ped.h/.cpp, Vehicle.h/.cpp, Event.h, FxSphere.h, FxSystemBP.h, VehicleModelInfo.h, NodeRoute.h, PedGeometryAnalyser.h, PedIntelligence.h): must take `size_t` |
| mixed-width overloads (`std::min(uint32, size_t)`, `GetRandomNumberInRange(.., size_t)`) | not counted | 15 sites in 13 files |
| `long` / `DWORD` assumed 32 bit | 235 textual uses in 43 files | the compiler sees only `fakerw.h:278 static_assert(sizeof(long) == 4)` plus size growth above; the textual uses are silent (format strings, save structs): audit by P-2 |
Reading: P-1 is ~510 types + ~275 offset asserts, not 334 types; the file-layout types of P-2 appear in the same list (`2dEffect.h`, `ColModel.h`, `PathFind.cpp` ...). The Scripts offset asserts need their own rule (`VALIDATE_OFFSET_X86` or compile-out on non-x86), they are 27% of the failures.

### W: platform
| item | inventory (h) | census (real) |
|---|---|---|
| Win32 API / type uses (undeclared with the stub header) | `HWND/HINSTANCE/...` 80 in 31 files | **481 unique in 45 files**: WinPlatform.cpp 59, oswrapper_win.cpp 43, AEMFDecoder.cpp 42, Fixups.cpp 41, CdStreamInfo.cpp 29, DataImage.cpp 27, ImageDiag.cpp 27, WinPs.cpp 26, WinMain.cpp 23 |
| MSVC CRT names | 242 names + 69 paths (362 uses in 111 files by text) | **307 unique in 103 files**, dominated by FIVE names: `sscanf_s` 99, `strcpy_s` 76, `_stricmp` 47, `sprintf_s` 46, `strcat_s` 8 (then `fopen_s` 4, `_getcwd` 3, `_chdir` 3); FileLoader.cpp 44, FileMgr.cpp 18, CutsceneMgr.cpp 15, Streaming.cpp 15, Hud.cpp 10. One shim header removes ~300 diagnostics (slice C-6) |
| DirectSound / EAX / MF / DirectShow (Audio, Video) | 229 hits in 35 files | AEMFDecoder.cpp 114, AEStreamingChannel.cpp 76, AEStaticChannel.cpp 51, AEAudioHardware.cpp, AEAudioTap.h 83 (AUDIO agent), VideoPlayer.cpp 28 "expected expression" (DirectShow types) |
| Windows headers that do not exist off Windows (fatal for the TU) | 14 `windows.h` includes | `objidl.h`, `debugapi.h`, `minwinbase.h`, `wmsdkidl.h`, `ddraw.h`, `ShObjIdl.h` (stubbed), plus the empty stubs for d3d9 / dinput / dsound / mmsystem / mf* / crtdbg / intrin / xtl ... |

### F: float / x87 / SIMD
| item | inventory (h) | census (real) |
|---|---|---|
| inline `__asm` / `__declspec(naked)` | 66 lines in 17 files / 22 naked in 5 | exact list in 3.3; arm64: **36 unique diagnostics in 13 files** ("unsupported architecture for MS-style inline assembly"): Matrix.cpp 7, FtolExe.cpp 7, Radar.cpp 5, Group13_15.cpp 4, Quaternion.cpp 2, Cover.cpp 2, EntryExit.cpp 2, PathFind.cpp 2, AEAudioUtility.cpp, Pool.h, Placeable.cpp, Maths.cpp, Group24b.cpp (X87Intrinsics.h, IdleCam.cpp, BoneNode.cpp, Fp.h are already guarded `_MSC_VER && !__clang__ && _M_IX86` with a `std::` fallback, which is NOT bit-exact: F-1/F-2 replace it); clang-cl i686: 1 failing file |
| SSE kernels (`_mm_*`) | 69 uses / 7 files | **170 unique diagnostics, all in 2 TUs** (`standalone/rw/pipeline_skin.cpp`, `pipeline_skin_cpu.cpp`; emmintrin.h 86, xmmintrin.h 64, mmintrin.h 25: arm64 `<emmintrin.h>` refuses to compile); also the only clang-cl-specific SSE problem (fixed with the target pragma) |
| compiler flags that change results (not visible as errors) | `/arch:IA32 /fp:precise` | clang native defaults differ: `-ffp-contract=on` (FMA fusion on arm64!), `char` signedness (Apple arm64 signed, Linux arm64 unsigned), strict aliasing, signed overflow; see slice C-8 |

### R: render (D3D9 outside librw)
| item | inventory (h) | census (real) |
|---|---|---|
| D3D9 / RW-D3D uses | 497 refs in 45 files; shim 18,417 lines | **1,123 unique DirectX diagnostics in 45 files** (cascade-inflated): standalone/rw/pipeline_matfx.cpp 342, pipeline.cpp 127, CustomCarEnvMapPipeline.cpp 96, engine.cpp 80, rwd3d_ff.cpp 53, im2d_im3d.cpp 51, renderstate.cpp 52, D3DResourceSystem.cpp 24, CustomBuilding*Pipeline.cpp ~85, camera.cpp 22, raster.cpp 16. 39 files own 1,220 unique diagnostics = the whole R workload is 39 files |

### A: fixed addresses / hooks
| item | inventory (h) | census (real) |
|---|---|---|
| hook machinery | 11,275 `RH_Scoped*` lines, 896 files; 2,811 `StaticRef` | the macros themselves compile on arm64 without errors; the 157 unique diagnostics sit in **9 files**: standalone/Fixups.cpp 90, standalone/DataImage.cpp 41, reversiblehooks/HooksUtility.hpp 13 (`VirtualProtect`, `GetLastError`, `SYSTEM_INFO`), RHManager.cpp 4, StaticOneWayHook.cpp 3, VMTInfo.cpp 2, PluginBase.cpp 1. So A-1 (macro-level compile-out of `RH_*` / `plugin::Call*`) can be verified by this census going to 0 on these 9 files without touching the 896 hook-registration files |

## 6. Findings (not fixed here; evidence in the commits)
1. **`CAEWeaponAudioEntity::Constructor` (0x5DE990) constructs a TEMPORARY.** The source says `CAEWeaponAudioEntity::CAEWeaponAudioEntity();` (no `this->`); MSVC reads this as a functional-style temporary (85 bytes of code vs 12 for `this->T::T()`), so the hooked constructor leaves `*this` unconstructed. clang rejects the form. Kept behaviour (`CAEWeaponAudioEntity{};` with a `// BUG:` comment) because C1 is behaviour neutral; the fix (`this->CAEWeaponAudioEntity::CAEWeaponAudioEntity();`) belongs under `notsa::IsFixBugs()` or the audio batch. All other ctor wrappers in the tree use `this->`.
2. `RH_ScopedGlobalInstall` on member functions (9 sites, section 3.2) only worked because MSVC accepts `&Member` inside a static member function.
3. `CPool` friend `swap` referenced `m_FirstFreeSlot` (does not exist; never instantiated).
4. `fakerw.h` compares enumerators of different enum types in `static_assert` (lines 315-340): an ERROR in C++26 mode (`-std=c++2c`, i.e. `/std:c++latest` with clang, GCC 15 `-std=c++26`). Fine with CMake's `-std=c++23`; cast to `int` before any c++26 switch.
5. libc++ gaps (Apple libc++ 21 / macOS 15 SDK): `std::views::enumerate` (129 uses), `std::ranges::shift_right` (2), `<stacktrace>` (SoundManagerDebugModule.hpp: 3 TUs), `std::from_chars(double)` (Configuration.hpp: macOS 26 deployment target only) = 128 diagnostics in 74 files. The MSVC STL has all of them.
6. `eCarWheel.h` forward-declares an `enum` without a fixed underlying type (ISO C++ forbids), `Weapon.h`, `Formation.cpp`, `dllmain.cpp` (`__arg`): the 4 genuine "compiler" issues of the native pass (the rest of "C: syntax" is Windows-type cascade).
7. `vendor/librw` (submodule, not ours) narrows constants in `d3d/xbox.cpp` and uses `-Wall -Wextra` style warnings: only matters for a clang build of librw (preset downgrades the error).
8. The clang-cl build emits ~8,200 warnings (mostly `-Wmicrosoft-*`, shadowing in librw); no census yet for warnings that indicate real bugs (`-Wunsequenced`, `-Wuninitialized`, `-Wsometimes-uninitialized`, `-Wreturn-type`): slice C-3.

## 7. Plan: the rest of stream C (sliced; one sonnet agent + review pass each; sizes from the census)
Order: C-2 and C-6 can start now and are independent of D/A; C-4/C-5 are small; C-7 needs P-1; C-8/C-3 are audits whose output feeds P/F.

| slice | content | evidence / size | acceptance | depends |
|---|---|---|---|---|
| C-2 clang gate in the dev loop | script `tools/standalone/clang_gate.sh`: incremental `ninja -k 0` on `build/StandaloneClangCL` for the TUs changed since a ref + the msvc-mode syntax census over the changed files (1-5 min); add the line to PORT_BRIEF ("every batch must pass the clang-cl gate") and keep the known failure list (`AEAudioUtility.cpp`) in the script. Keeps the tree at 1,114/1,115 while ~3,000 functions are still being ported | tree is clean today; typical regressions are the 6 kinds of 3.2 | gate on the 3 latest commits of the other agents passes or produces a fix list | none |
| C-3 clang warnings audit (clang-cl i686) | build with the warnings that find real bugs (`-Wunsequenced -Wuninitialized -Wsometimes-uninitialized -Wreturn-type -Wmissing-braces -Wtautological-compare -Wshift-count-overflow -Wint-in-bool-context -Wdangling`) and triage ~200 expected hits: real bugs go to the owners as `// BUG:` items (never silent fixes) | 8,217 warnings in the first build, 0 triaged | list of confirmed bugs with MSVC-vs-clang behaviour statement | C-2 |
| C-4 libc++ / library gaps | `notsa::views::enumerate`, `notsa::ranges::shift_right` (+ `<stacktrace>` guard in SoundManagerDebugModule, `from_chars(double)` fallback): selected by `__cpp_lib_*` so MSVC still uses `std::`; MSVC code identical | 128 diagnostics / 74 files (129 `enumerate` uses) | native census category "C: libc++" = 0; MSVC A/B identical on 5 TUs | none |
| C-5 SSE kernels portable | `pipeline_skin_core.h` / `pipeline_skin_cpu_core.h` / `pipeline_skin.cpp`: `<emmintrin.h>` only on x86; on arm64 use an `sse2neon`-style header or scalar float4 code that keeps the asm operand order (bit-exact against the exe SSE path: `rw_skin_cpu_oracle_test` stays the judge); drop `NOTSA_CLANG_SSE2_*` when the x86 path is the only user | 170 diagnostics, 2 TUs (+ lifted x87 `.inc` files stay `double`) | oracle test identical on x86; arm64 syntax pass of the 2 TUs clean | coordinate with R/F (SHADOW agent works in the shim) |
| C-6 Windows scalar types + CRT-name shim | `source/portable/wintypes.h` (DWORD, BOOL, HANDLE, `byte`, SIZE_T ... as fixed-width) and `source/portable/crt.h` (`sscanf_s`, `strcpy_s`, `sprintf_s`, `strcat_s`, `fopen_s`, `_stricmp`, `_strnicmp`, `_strlwr/_strupr`, `_getcwd/_chdir`, `MAX_PATH`, `InterlockedExchange*` on `std::atomic`), included from `winincl.h` on non-Windows; `strcpy_s`-family keep the destination-size semantics (the originals are the CRT's); no behaviour change on Windows | 307 CRT + ~60 type diagnostics, ~100 files; 5 names = 276 of them | native census W/C = 0 and `W: Win32 API / type` outside the 14 platform files = 0 | none (feeds W-7) |
| C-7 compile-fix passes by tree on the native target | after P-1/P-2 land: iterate `clang_census.py --mode native --only <dir>` per tree (game_sa root, Entity/Tasks/Events, Audio, Scripts, Models/Collision/Fx, app/standalone) until each directory is clean | 4,340 unique now, 406 unclassified cascades; expect < 500 after P-1 + C-6 + C-4 | per-directory clean list in the report | P-1, C-4, C-6 |
| C-8 portable build flags + evaluation-order audit | decide and document the flag set of the portable build: `-ffp-contract=off` (clang defaults to fusing on arm64), `-fsigned-char`, `-fno-strict-aliasing`, `-fwrapv`, `-mms-bitfields` (checked: identical failure set), `-fno-delayed-template-parsing`; audit evaluation-order dependencies: MSVC x86 evaluates call arguments right-to-left, clang/gcc left-to-right: grep + AST matcher for calls whose arguments contain >= 2 side-effecting calls (`GetRandomNumber*`, `rand`, `Read<>`), `i++` in arguments; unit test = record/replay hash on the first soak route | not visible as compile errors; the single highest-risk class for record/replay equality besides x87 | list of sites + fixes behind `NOTSA_PORTABLE` (explicit temporaries in the MSVC order) | none (feeds F/P) |
| C-9 clang-cl exe: link + boot | make `AudioLog10` linkable under clang-cl (module-level asm label or F-4 software x87), link with `lld-link`, run to the menu under wine (ABI check of thiscall hooks, MS asm via clang) and optionally use it as a second compiler in the soak: any divergence from the MSVC exe is a UB/compiler-sensitivity finding | 1 failing object, 3 libs | boots to the menu, soak hash equal or findings listed | C-2 |

Not planned (nothing to do): `__cdecl` (905 uses), `__stdcall` (50), `__thiscall` (15), `__fastcall` (1), `__forceinline`, `__declspec(other)`: produce NO errors on arm64 with `-fms-extensions` (ignored calling-convention keywords); they matter only for the hook machinery (A) and for the `plugin::CallMethod` thiscall trick. `#pragma pack` (10 uses) / `#pragma comment` (17): no errors; pack matters for file layouts (P-2). SEH `__try` (2 uses, `standalone/Fixups.cpp:164`, Windows-only run-build code): accepted by clang-cl, arm64-apple needs it compiled out with the rest of Fixups (A).

## 8. Limits of this census (read before quoting numbers)
* Native pass: only what the front end sees. Linking, `-O2` codegen, inline asm semantics and library behaviour are not covered. Windows APIs are counted as "undeclared identifier", so one missing type produces many diagnostics (cascades): use unique counts per FILE, not per diagnostic, to size work (W 142 files, R 39, P 541, F 16, A 9, C 74).
* The PCH-closure errors are counted once (they were not re-reported per TU): P asserts reachable only through a TU's own headers are in the per-TU logs, those in StdInc.h's tree are in the 341 PCH errors.
* The native defines are the Windows run build's (see section 4), so "W/R" overstates what stays after compile-outs. The -Werror pointer casts were enabled after the first native run (runs 5/6 include them; run 6 adds `-mms-bitfields`; identical failure set).
* Running agents were active (AUDIO, PERF, SHADOW, QUIT): Audio/Hardware files, Cheat.cpp and standalone/rw/camera.cpp, engine.cpp were dirty during the runs. Their numbers are a snapshot; C1 touched none of their files except the 4 SSE-pragma lines in `standalone/rw/pipeline_skin*.{h,cpp}` and `rwmath_exact.h` (all clean in git before the edit).
