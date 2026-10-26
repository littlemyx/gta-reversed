# MAC-FTOL2: remaining float -> int conversions (class B) -> NOTSA_FTOL (2026-10-26, portable/main)
Rules v4 (Windows-only parts skipped). Base d10b51f6 (after be09cffc, the reverse/interior_c merge). Goal level re3: no bit-exactness, but the exe's `_ftol2` semantics as in MAC-FTOL1.

## Preconditions
PortableMac built green on the merged HEAD, `ctest -L portable` 19/19. Re-scan found 927 class B + 1 class A (VehicleModelInfo.cpp:535 `floor(coeff*1000)`, added by the merge) = 928 sites.

## Result
| Item | Result |
|---|---|
| Sites converted | 911 (896 NOTSA_FTOL, 5 _FN, 6 _P, 4 _SC), 182 files; incl. the class A site, 10 macro-body sites by hand (Coronas CalcColorC x3, WaterLevel VertexUnpack x7), 2 template sites (script AssignTo / GetStat, guarded by `if constexpr`) |
| Left (19, listed below) | not converted |
| PortableMac | build green (ninja, 0 errors) |
| ctest -L portable / -L mutation | 19/19 / 12/12 |
| play1 s1 / s2 | NOT run: display inactive (`pmset -g assertions` UserIsActive = 0) |
| Win32 A/B (wine cl.exe, code+relocs, ftol_ab.py, base d10b51f6 vs HEAD 30115025, 269 TUs) | 265 identical, 4 differ only in `std::source_location` column immediates of `rand()` calls inside the converted expression (+ the MSVC `?A0x<hash>` anonymous-namespace name in PathFind): Explosion, PathFind, Population, TaskComplexPartnerGreet. Not codegen; GameRand uses `loc.line()` only. 1 TU not in the Win32 build (SDLWrapper.cpp, `bindings/imgui_impl_sdl3.h`) + GameplayRegress.cpp (not in the Win32 compile db): portable-only |

## Batches (one commit each, all `[skip ci]`; A/B of each batch run against the commit before it, final A/B of the whole series at the end)
| batch | commit | sites | files | A/B TUs (incl. header users) | A/B first run | after fix |
|---|---|---|---|---|---|---|
| tools: ftol_ab.py `--head TREE` | d10b51f6 | - | 1 | - | - | - |
| Tasks / Events / DecisionMakers / Conversations (+TaskComplexSeekEntity.h) | 479ec961 | 56 | 35 | 61 | 60 identical, PartnerGreet column-only | neutral |
| Entity / Models (+VehicleModelInfo class A) | 3009e92d | 103 | 12 | 12 | 12 identical | - |
| Audio | 1aa2699e | 28 | 12 | 12 | 12 identical | - |
| Scripts (+ AssignTo / GetStat `if constexpr`) | d830972c | 10 | 8 | 8 | BasicCommands: only `__LINE__` (4 added lines) | one-liner, identical |
| Collision / Plant / Pipelines (+ColourSet.h) | 86358b6b | 38 | 7 | 9 | 9 identical | - |
| Fx (+RenderInfo.h) | 40a858bb | 35 | 14 | 24 | 24 identical | - |
| Interior / Frontend | 16e89034 | 43 | 7 | 7 | 7 identical | - |
| app/platform | 267e5a96 | 67 | 3 | 2 | WinPs identical; SDLWrapper / GameplayRegress portable-only | - |
| game_sa top A-F (+Birds.h, CullZones.h) | f18db775 | 129 | 24 | 50 | Explosion column-only | neutral |
| game_sa top G-P (+Lines.h) | 9c71510c | 176 | 26 | 27 | GangWars, GrassRenderer, Pickup REAL; PathFind, Population column/hash-only | fixed (below) |
| game_sa top Q-Z (+RGBA.h, ReplayBlockType.h, Shopping.h) | 5a246a15 | 220 | 33 | 118 | 117 identical; Shopping one-off (not reproducible, identical on re-run) | - |
| fix: spellings | 30115025 | - | 6 | - | GangWars, GrassRenderer, Pickup, BasicCommands, StatCommands identical | - |
| p2c host-test stub | d4fe8b16 | - | 1 | - | - | - |

## Real MSVC differences found by A/B and how they were fixed (Win32 spelling now = the original tokens)
* `NOTSA_FTOL_SC(T, e)` = `static_cast<T>(e)` on Win32: GangWars.cpp:808 `FightTimer = static_cast<uint32>(DistanceBetweenPoints2D(..)*200+240000)` compiled differently as `(uint32)(..)` (x87 stack slots). GrassRenderer:75 and Shopping.h use it too (the others static_cast sites were identical as `(T)(e)`).
* `NOTSA_FTOL_P(T, e)` = `(T)e` on Win32 (no parentheses): Pickup.cpp:226/566 `(uint16)m_fRevenueValue`, `(int32)m_fRevenueValue`, GrassRenderer:85 `(uint8)std::clamp(..)`: register allocation changed with `(T)(x)`.
* `NOTSA_FTOL_FN(T, e)` = `T(e)`: GrassRenderer:92-94 `uint16(mult * ..) / 256`.
* Script templates `AssignTo<T,Y>` / `GetStat<T>`: `NOTSA_FTOL(T, x)` instantiates FtolCast<float, ...>: `if constexpr (floating -> integral)` on ONE line (an added line shifts `__LINE__` immediates).
* The p2c host test's stub Base.h had no NOTSA_FTOL (real ReplayBlockType.h now uses it): macro added there.
All sites were bisected per line with a throw-away tool (single-line revert variants against the base object).

## Not converted (19 sites, scan after the series)
* FxFtol.h:11, Group22.hpp:24, Group25_26.hpp:22: the `Ftol(double)` helpers themselves (`(int64)v`); General.h:156/162 (template `GetRandomNumberInRange<T>` and the x87d helper, T may be float), ReadArg.hpp:90 `safe_arithmetic_cast<T>`, common.h:213 `lerp<T>` (T may be float): a dependent `NOTSA_FTOL(T, ..)` would not compile for float T. 7 sites.
* PostEffects.cpp 845-848, 1220-1229 (12): inside the fakerw macro `RwIm2DVertexSetRealRGBA` (`(RwUInt32)((c)*255.0f+0.5f)`, colours 0..1), library header, not game code.
* Constant-folded / literal casts: none found (every Class B cast had a non-literal operand). Code not compiled for the portable build (Win32-only `#if`) is not visible to the AST scan and was not touched.
* Not visible in the Win32 compile db: SDLWrapper.cpp (48 sites), GameplayRegress.cpp (17): portable only.

## Consumer audit
No value in range changes; out of range / NaN / negative-to-unsigned now yield the exe's low dword (uint8 of -1.5f = 255, not arm64 saturation 0). Readers = the converted expressions only; no layout/indexing semantics changed. Spans of concern (unchanged since MAC-FTOL1): particle/count loops fed by finite quantities. The Mac-side behaviour of colours (uint8 of float*255 out of [0,255]) now wraps like the exe instead of saturating.

## Win32-relevant findings
1. MSVC /arch:IA32 codegen reacts to the CAST SPELLING (static_cast vs C-style, with/without parentheses around a primary operand, functional cast): the codemod must keep the original spelling (NOTSA_FTOL_SC/_FN/_P). Candidate improvement for ftol_scan.py: emit the matching macro by cast kind instead of fixing after A/B (the 15 spots above were found only because of the A/B).
2. `rand()`/`GameRand` call sites carry `std::source_location` (line AND column) in the object code; a converted expression moves the column. Harmless (hook uses line), but any tool that compares objects must mask `movl $imm, 4(reg)` after `movl $line, (reg)` and `?A0x<hash>` names.

## CONTINUE
Nothing pending. Optional: play1 s1/s2 when the display is active (not run: UserIsActive = 0); teach ftol_scan.py to choose the macro by cast kind.
No background tasks, wine, caffeinate or game processes left; temp worktrees removed.
