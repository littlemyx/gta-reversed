# MAC-F4a report: inline asm / naked functions out of the portable build (compile gate), via the soft x87

Branch portable/main. Brief `.notes/briefs/MAC-F4a.md`. Host Apple Silicon; reference machine for every "x87" statement = Wine + Rosetta 2 (as F1_soft.md).

## What was built
| piece | file | note |
|---|---|---|
| x87 register stack + 32 bit register file | `source/game_sa/Core/X87Lift.h` | `Fpu` (8 slots of `soft::F80`, add/sub/mul/div/sqrt rounded at the PC (24), fsin/fcos/fpatan/fyl2x unrounded 64 bit results, fcom* + fnstsw) and `Cpu` (eax..edi/esp as uintptr_t, ZF/PF/CF, private 1 KB stack at virtual 0x10000, pointer ARGUMENTS pass as handles h*256). Integer-only: identical on every host. |
| asm -> C++ lifter | `tools/portable/x87lift.py`, `x87lift_sites.py` | Intel-syntax MSVC `__asm` subset (fld/fst/fild/arith/fsin/fcos/fpatan/fyl2x/fsqrt/fcom*/fnstsw + mov/movzx/lea/push/pop/add/sub/and/xor/shl/shr/cmp/test/jcc/call/ret). An unknown instruction aborts, never mistranslates. 20 sites lifted automatically into `source/game_sa/Core/x87_lifted/<site>.inc` (GENERATED header line each). |
| hand routines on the same machine | `source/game_sa/Core/X87Sites.h` | Radar SetRadarAngle*, AudioLog10, IdleCam SinP1Half, BoneNode AtanDeg, PostEffects ExeRasterLog2, AEPed JetPackSinSpeeds, AEVehicle BikeLeanTerm |
| `x87::*` wired to the soft x87 | `source/game_sa/Core/X87Intrinsics.h` (new `#elif defined(NOTSA_PORTABLE)`) | sin cos tan atan2 sqrt atan asin acos log2 log10 ln sincos: `ToDoubleRoundToOdd(soft::...)`, asin/acos = the CRT sequence at PC24 incl. the |x|=1 / NaN / domain cases; tan with |x|>=2^63 returns the real indefinite like the asm helper (empty stack). |
| ftol family | `source/standalone/FtolExe.cpp` (`#ifdef NOTSA_PORTABLE` branch) | `notsa::fp::exe::Ftol2/Ftoi2/Ftoui2/Ftoul2(double)`; the naked `_ftol2*` symbols stay in the `#else` (MSVC) |
| differential test | `tests/portable/f4a_lift_test.cpp`, `tools/portable/run_f4a_test.sh` | see below |

MSVC x86 code is untouched: every site is `#ifdef NOTSA_PORTABLE / #include ".inc" / #else <original asm> / #endif` (or `#ifndef NOTSA_PORTABLE` around single `_asm push/pop edx` lines, or an extra `#elif defined(NOTSA_PORTABLE)` before an existing libm `#else`).

## Differential test (the evidence)
Both sides are the SAME generated site functions (`x87lift_sites.py harness`): the verbatim asm block (MSVC x86 `cl /arch:IA32`, run under Wine/Rosetta, PC set per site with `_controlfp`) writes golden outputs; the portable build (clang arm64, the real committed `.inc` files / X87Sites.h / FtolExe.cpp) must reproduce them bit for bit. Inputs: deterministic integer RNG (bits -> floats, no FP in the generator): game-like magnitudes, wide exponents, +-0, denormals, inf, NaN, +-1, k*pi/2-ish angles, huge angles up to 2^22 (and doubles to 2^30), degenerate matrices.
`tools/portable/run_f4a_test.sh [N] [seed]` (golden build + run under wine, host build + compare; `MUTATE=1` adds 5 mutants of lifted `.inc` that must all be killed: 5/5 killed).

Run: N = 20000 cases per row, seed 7, 43 rows.
| row | cases | bit-exact | NaN-payload only | hard | 1-ulp float words |
|---|---|---|---|---|---|
| Matrix_SetRotate | 20000 | 20000 | 0 | 0 | 0 |
| Matrix_RotateX | 20000 | 20000 | 0 | 0 | 0 |
| Matrix_RotateY | 20000 | 20000 | 0 | 0 | 0 |
| Matrix_RotateZ | 20000 | 20000 | 0 | 0 | 0 |
| Matrix_Rotate | 20000 | 20000 | 0 | 0 | 0 |
| Matrix_ConvertToEulerAngles | 20000 | 20000 | 0 | 0 | 0 |
| Matrix_ConvertFromEulerAngles | 20000 | 20000 | 0 | 0 | 0 |
| Quaternion_Slerp | 20000 | 20000 | 0 | 0 | 0 |
| Quaternion_CalcThetaFromQuats | 20000 | 20000 | 0 | 0 | 0 |
| Cover_FindDirFromVector | 20000 | 20000 | 0 | 0 | 0 |
| Cover_FindVectorFromDir | 20000 | 20000 | 0 | 0 | 0 |
| Placeable_GetRoll | 20000 | 20000 | 0 | 0 | 0 |
| Entity_GetIsTouching | 20000 | 20000 | 0 | 0 | 0 |
| Maths_SinTableEntry | 20000 | 20000 | 0 | 0 | 0 |
| PathFind_Heading | 20000 | 20000 | 0 | 0 | 0 |
| Group13_DivideAssign | 20000 | 20000 | 0 | 0 | 0 |
| Group13_AttractorDir | 20000 | 20000 | 0 | 0 | 0 |
| Group13_AcosCrt | 20000 | 20000 | 0 | 0 | 0 |
| Group13_Fsqrt | 20000 | 20000 | 0 | 0 | 0 |
| Group24b_SinCos | 20000 | 19999 | 0 | 1 | 0 |
| x87::sin | 20000 | 20000 | 0 | 0 | 0 |
| x87::cos | 20000 | 20000 | 0 | 0 | 0 |
| x87::tan | 20000 | 20000 | 0 | 0 | 0 |
| x87::atan2 | 20000 | 20000 | 0 | 0 | 0 |
| x87::sqrt | 20000 | 20000 | 0 | 0 | 0 |
| x87::atan | 20000 | 20000 | 0 | 0 | 0 |
| x87::asin | 20000 | 20000 | 0 | 0 | 0 |
| x87::acos | 20000 | 20000 | 0 | 0 | 0 |
| x87::log2 | 20000 | 20000 | 0 | 0 | 0 |
| x87::log10 | 20000 | 20000 | 0 | 0 | 0 |
| x87::ln | 20000 | 20000 | 0 | 0 | 0 |
| Radar_SetRadarAngleAtan2 | 20000 | 20000 | 0 | 0 | 0 |
| Radar_SetRadarAngle | 20000 | 20000 | 0 | 0 | 0 |
| AudioLog10 | 20000 | 20000 | 0 | 0 | 0 |
| AudioLog10_x20 (F-4 TODO) | 20000 | 15436 | 0 | 4564 | 4564 |
| AudioLog10_x10 (F-4 TODO) | 20000 | 15346 | 0 | 4654 | 4654 |
| ftol2 | 20000 | 20000 | 0 | 0 | 0 |
| ftoul2 | 20000 | 20000 | 0 | 0 | 0 |
| IdleCam_SinP1Half | 20000 | 20000 | 0 | 0 | 0 |
| BoneNode_AtanDeg | 20000 | 20000 | 0 | 0 | 0 |
| PostEffects_ExeRasterLog2 | 20000 | 20000 | 0 | 0 | 0 |
| AEPed_JetPackSinSpeeds | 20000 | 20000 | 0 | 0 | 0 |
| AEVehicle_BikeLeanTerm | 20000 | 20000 | 0 | 0 | 0 |

43 rows. Hard mismatches: `Group24b_SinCos` 1 (a double word, 1 ulp), `AudioLog10_x20` / `_x10` (the documented F-4 TODO). Everything else 20000/20000 bit-identical, including all 12 `x87::*` rows, ftol2/ftoul2 with 2^63, inf, NaN and overflow inputs, and 5/5 mutants killed (`MUTATE=1`).

## Site table (census rows "F/C: inline asm / naked", 36 at the start; file:line = the include marker / branch in the current tree)
| # | file:line (current tree) | function / original census line | asm ops | portable branch | bit-exact vs asm (20000 cases) |
|---|---|---|---|---|---|
| 1 | Core/Matrix.cpp:247 (was 235) | CMatrix::SetRotate | fcos fsin fmul fsubp faddp fchs fxch | lifted `Matrix_SetRotate.inc` | yes, 20000/20000 |
| 2-4 | Core/Matrix.cpp:337 / 424 / 509 (321 404 485) | RotateX / RotateY / RotateZ | same family | lifted `Matrix_Rotate{X,Y,Z}.inc` | yes x3 |
| 5 | Core/Matrix.cpp:594 (566) | CMatrix::Rotate | same family | lifted `Matrix_Rotate.inc` | yes |
| 6 | Core/Matrix.cpp:876 (844) | ConvertToEulerAngles (jumps, table lookups via lea/movzx, fsqrt/fcom/fpatan, out pointers as handles) | 200 lines | lifted `Matrix_ConvertToEulerAngles.inc` | yes |
| 7 | Core/Matrix.cpp:1090 (1054) | ConvertFromEulerAngles | idem | lifted | yes |
| 8 | Core/Quaternion.cpp:127 (126) | CQuaternion::Slerp | fcomp fnstsw jne 2x fsin | lifted `Quaternion_Slerp.inc` | yes |
| 9 | Core/Quaternion.cpp:288 (283) | CalcThetaFromQuats | CRT acos sequence, `fld tbyte` qnan, fsin, fdivr | lifted | yes |
| 10-11 | Cover.cpp:443 / 475 (442 470) | FindDirFromVector (fpatan, double result) / FindVectorFromDir (fild, fsin, fcos, out pointer handle) | lifted | yes x2 |
| 12 | Entity/Entity.cpp:1301 (1300) | CEntity::GetIsTouching (fcompp, `fnstsw word ptr`) | lifted | yes |
| 13 | Entity/Placeable.cpp:113 (112) | CPlaceable::GetRoll (loads `m_matrix` from [ecx+0x14]: replaced by the member, layout differs on 64 bit) | lifted with one substituted instruction | yes |
| 14 | Maths.cpp:18 (17) | SinTableEntry (fild fmul fsin at PC53 like the static init in the exe) | lifted, `Cpu(53)` | yes |
| 15 | PathFind.cpp:2165 (2160) | heading fpatan * 57.2957 | lifted | yes |
| 16 | PathFind.cpp:2058 (2056) | `__asm mov eax, this` (register shim for a caller) | `#ifndef NOTSA_PORTABLE` (no register contract in a C++ build) | n/a |
| 17-20 | Scripts/Commands/Ported/Group13_15.cpp:397 / 422 / 580 / 621 (394 415 569 606) | DivideAssign (fdiv, 3x fmul), AttractorDir (fsin fcos), AcosCrt (CRT acos incl. specials), Fsqrt | lifted | yes x4 |
| 21 | Scripts/Commands/Ported/Group24b.cpp:92 (91) | fsin/fcos stored as doubles | lifted | 19999/20000: 1 double word 1 ulp (80 bit residual of the soft fsin, F1_soft.md) |
| 22-23 | Radar.cpp:441/443/449 | `_asm push/pop edx` shims | `#ifndef NOTSA_PORTABLE` | n/a |
| 24-25 | Radar.cpp:508/525 (naked SetRadarAngleAtan2 / SetRadarAngle) | fpatan fst fsin fcos | `X87Sites.h` `RadarAngleAtan2/RadarAngle` | yes x2 |
| 26 | Audio/AEAudioUtility.cpp:91 (naked AudioLog10) | fcomp, fldlg2, fyl2x, result unrounded in ST0 | `X87Sites.h AudioLog10` (float return) | function yes; callers' x10/x20 product 1 ulp in 23 % (F-4 TODO, below) |
| 27-28 | EntryExit.cpp:127/134 | `_asm push/pop edx` | `#ifndef NOTSA_PORTABLE` | n/a |
| 29-35 | standalone/FtolExe.cpp:12-80 (7 naked symbols) | `_ftol2` fistp trunc, `_ftoul2` biased | `notsa::fp::exe::Ftol2/Ftoi2/Ftoui2/Ftoul2` (+ soft fsub at PC24 for >= 2^63) | yes (ftol2, ftoul2 rows incl. 2^63, +-inf, NaN, overflow) |
| extra | IdleCam.cpp, Ragdoll/BoneNode.cpp, PostEffects.cpp, Audio/Entities/AEPedAudioEntity.cpp, AEVehicleAudioEntity.cpp | MSVC-x86-only asm next to an APPROXIMATE libm `#else` (not census rows: they compiled) | `#elif defined(NOTSA_PORTABLE)` -> `X87Sites.h` | yes, 5 rows 20000/20000 (libm fallback was not bit-exact: sin stored to float/double before the multiply) |
| x87:: | Core/X87Intrinsics.h | 12 `x87::*` helpers had a libm fallback | soft x87 | float level: 11 rows x 20000, 0 differences |


## Census / A/B
* Census (tools/portable/snap_census.sh, snapshot = HEAD + my files, PortableMac compdb, ALL 1100 TUs): **"F/C: inline asm / naked" 0 rows (was 36)**; TUs with errors 5 (was 5+; all pre-existing P/W rows of other slices: AnimManager.h assert, General.cpp, VehicleModelInfo.cpp, VisibilityPlugins.cpp, UIRenderer.cpp - none mine, none in a TU I touched). TSV `build/snap_census/f4a_full.tsv`. The 15 TUs of the first targeted run were 15/15 clean.
* Win32: no CMake file touched -> StandaloneRelease compile commands are byte-identical by construction (no `compile_commands.json` input changed; the new `.inc` are only reachable through `#ifdef NOTSA_PORTABLE`).
* Wine cl.exe A/B (A = HEAD tree, B = A + my files, flags of StandaloneRelease via the MAC-P4 driver, `NOTSA_SIG_NORM_STRINGS=1`), 21 TUs (all 18 touched .cpp that are in the Win32 build + PopCycle/Train/Camera as users of X87Intrinsics.h): **16 code+relocs+data identical**; 5 differ ONLY by `__LINE__` immediates (Cover.cpp, Entity.cpp, PostEffects.cpp, AEVehicleAudioEntity.cpp: `push <line>` before an assert/log string, +8 / +2 lines; PathFind.cpp: no code difference, only a path-derived anonymous-namespace name hash and a 1 byte debug-info delta). Verified by a normalised disassembly diff (`dumpbin /disasm`, string names, jump targets and the line immediate stripped): 0 remaining differences in all 5.
  EntryExit/Radar/Maths/Matrix/Quaternion/Placeable/Group13_15/Group24b/AEAudioUtility/FtolExe/IdleCam/BoneNode/AEPedAudioEntity/Train/PopCycle/Camera: identical.

## F-4 TODO / not bit-exact (listed)
* `CAEAudioUtility::AudioLog10` (AEAudioUtility.cpp): the function result is bit-exact (row AudioLog10, 20000/20000), but the exe returns the UNROUNDED 64 bit value in ST0 and ~60 callers multiply by 10 / 20 and round once; a C++ `float` return rounds first. Measured: 22.8 % (x20) / 23.3 % (x10) of the products differ by 1 ulp of float (rows `AudioLog10_x20/_x10`). Audio volume only. Fix needs `AudioLog10Scaled(p, k)` (or an F80 return type) at the 60 call sites, which changes the Win32 call code -> separate A/B-gated slice (F-4b).
* `Group24b_SinCos`, `IdleCam_SinP1Half` style consumers that STORE a transcendental into a `double`: the soft x87 is 80 bit exact in 88..99 % of calls (F1_soft.md), the residual 1 ulp(80) flips a 53 bit rounding in ~3e-5 of calls: Group24b_SinCos 1 / 20000 differed (double word, 1 ulp). Float outputs: 0 differences in every row.
* NaN payload / sign: not compared (nan-only column 0 in this run).
* `f2xm1` / `fscale` are not modelled (no site uses them).
* FPU control: `_set_SSE2_enable(0)`, `_clearfp()` (WinMain.cpp:641/647), `Fixups::LogFpuState` (`_controlfp/_statusfp`), `rwmath_exact.h Pc53Scope` (`_controlfp_s`) are already portable no-ops/log-once from MAC-C6 (`#ifndef NOTSA_PORTABLE` / portable `LogFpuState` prints one line "portable build (no x87 precision control)"); nothing left to do here.
* tests/portable/f4a_lift_test.cpp is not in `portable_tests.cmake` (MAC-P2k owns that file): wire `run_f4a_test.sh` there (needs wine + MSVC for the golden file; the host half alone can run from a committed golden file if one is wanted).
