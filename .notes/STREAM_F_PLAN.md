# Stream F plan: floating point without x87 (F1, 2026-10-11)

Scope: make the game's arithmetic reproducible on hardware without an x87 (arm64, x64) while the Win32/x86 `/arch:IA32` build stays the fidelity reference.
Evidence lives in `tests/standalone/fp_theory_test.cpp` (full log `.notes/reports/F1_tools/fp_theory_full.log`), `tests/standalone/game_oracle_test.cpp`
(exe-oracle, pilot rows), `.notes/reports/F1_soft.md` (software x87 transcendentals). Header: `source/game_sa/Core/Fp.h` (namespace `notsa::fp`).
Reference machine for every "x87" statement: the x87 of this Mac (Wine on Rosetta 2); the oracle runs the exe's own machine code on it.

## 0. Summary
* Under PC=24 the x87 is IEEE single precision with a 15-bit exponent. For float operands every `+ - * / sqrt` is bit-identical to SSE2/arm64 `float` unless an
  INTERMEDIATE leaves the float exponent range (overflow stays finite on the stack, underflow keeps 24 significant bits until the store). Everything else that
  differs is listed in section 1; each difference has an exact portable substitute in `notsa::fp` and an oracle-proven test.
* The ports' `double` intermediates are NOT 53-bit values at PC=24: they are float-precision values (T8, 2,000,000/2,000,000 SSE-double results differ from the x87,
  0 differ from the float rewrite). So the portable translation of the 8,857 `(double)` casts / 2,388 `double` locals is "float per operation" (or the F24 model),
  never "SSE double".
* Pilot: the 9 sphere/line/triangle primitives of `CCollision` compiled `/arch:SSE2` in their own TU (`CollisionPrimitives.cpp`), one template body per function
  instantiated for `float` (fast path) and `F24` (exact fallback when MXCSR reports overflow/underflow/denormal). Results in section 4.

## 1. Theory table (x87 PC=24 vs portable), oracle evidence
Legend: "ident" = bit-identical on all tested inputs; counts are mismatches / cases (2,000,000 per row and input class unless noted).

| # | claim | x87 (PC24) vs plain float (SSE2 / arm64, no contraction) | portable exact form | evidence |
|---|---|---|---|---|
| T1 | `+ - * / sqrt`, float operands, float store | ident for regular (2^-20..2^20), mid (2^-60..2^60), wide (any exponent) and special (0/inf/NaN/denormal/max) classes. Only exception: a result in the float DENORMAL range from `*` `/` (x87 rounds to 24 bits with the extended exponent, then again at the store; 1,931 / 2,000,000 wide `*`, 1,100 special) | `F24` (double + `Round24`) is ident in every class (Figueroa: q=53 >= 2p+2=50 makes double-then-round-to-24 equal the direct rounding for `+ - * / sqrt`) | `fp_theory_test` T1 (2M x 4 classes x 5 ops) |
| T2 | chains kept on the FPU stack (`a*b/c`, `a*b*c`, `a*b+c*d`, `sqrt(a*a+b*b)`, `(a-b)*(a+b)/c`) | ident while no intermediate leaves [2^-126, 2^128] (regular class: 0 / 2M each). Wide class: 121,821 (`a*b/c`), 120,475, 16,194, 919,747, 229,006 of 2M differ (extended exponent: the x87 stays finite where float overflows to inf / underflows to 0) | `F24`: 0 mismatches in every class | T2 |
| T3 | float `*` / `+` a 53-bit double constant or load (`0.01`, `x * 0.017453292519943295`) | `(float)((double)a * c)` DOUBLE-ROUNDS: 157,396 of 668,435 adversarial inputs differ (23%); on random inputs the rate is ~2^-29 (not hit in 2M) | `MulD` / `AddD` / `Round24Mul` (Dekker two-product + round-to-odd + `Round24`, no FMA): 0 mismatches. The x87 rounds the exact product ONCE (Rosetta too) | T3 |
| T4 | `int32` converted inside an expression (`fild; fmul f`, e.g. `CTimer::GetTimeInMS() * 0.001f`) | `(float)i * f` differs for |i| > 2^24: 96,939 of 2M (4.8%), 0 for |i| <= 2^24 | `Round24Mul((double)i, f)` 0 mismatches (`(float)((double)i * f)` happened to give 0 in 2M but double-rounds in theory) | T4 |
| T5 | float -> int (`_ftol2`: `fistp qword`, chop, LOW dword; NaN/inf/|x| >= 2^63 -> 0, 2^31..2^63 wrap) | a plain C++ `(int)` differs from the exe in 1,011,265 of 2,000,029 random + edge inputs (x64 `cvttsd2si` gives 0x80000000, arm64 saturates, both UB) | `fp::Ftol/Ftol64/FtolU` 0 mismatches vs `fistp` | T5 |
| T6 | compares (`fcom/fcomp/fcompp` + `fnstsw; test ah, 0x41/0x05; jp/jbe`) | ident to C++ `< > == <= >= !=` incl. NaN (unordered => all false except `!=`), +-0, inf: 0 / 50,000 | plain C++ operators; keep the exe's polarity (`!(a > b)` is not `a <= b`) | T6 |
| T7 | `a*b + c` | two roundings (x87) == `volatile`-split float ops; a FUSED multiply-add differs in 331,562 of 2M | compile game TUs with `-ffp-contract=off` (clang/gcc), MSVC arm64 `/fp:precise` + `#pragma fp_contract(off)`; never `-ffast-math`, never `std::fma` | T7 |
| T8 | the ports' `double t = (double)a*b + (double)c*d;` | at PC24 the x87 result is float-precision: SSE2 DOUBLE arithmetic differs in 100% (2M / 2M) of the cases; the float rewrite and `F24` differ in 0 | `float` (default) / `F24` | T8 |
| T9 | guard: inputs of `InSafeRange<16>` through chains of depth <= 3 | plain float == x87 (0 / 420,370 comparisons) | (superseded by the flag guard below, kept for static reasoning) | T9 |
| T10 | float-denormal results of `*` (double rounding at the store) | 7,132 / 2M differ (float), 0 for `F24` | `F24` | T10 |
| T11 | `fsin fcos fptan fpatan fyl2x f2xm1 fsqrt-of-double`, CRT `_CIasin/_CIacos/pow/exp` | UCRT/libm differ (`x87_oracle_test` INFO rows: asin/acos 73%); results are 64-bit-mantissa values rounded by the NEXT op | `x87::*` (asm, Win32 reference); portable: `X87Soft.h` (section 3.5, `F1_soft.md`) | x87_oracle_test, F1_soft.md |
| T12 | NaN propagation / payload | x87 picks the larger-significand NaN and quiets sNaN; SSE picks the first operand; arm64 default NaN is +qNaN (0x7FC00000) while x87/SSE give 0xFFC00000 | not reproducible bit-for-bit across architectures; treat NaN payload/sign as don't-care (the oracle classifies it `nanpayload`; replay hashes must canonicalise NaN) | game_oracle rows |

Environment facts found while testing (all matter for the design):
* Rosetta tracks MXCSR OE (0x08), UE (0x10), PE (0x20) and DE (0x02) sticky flags, NOT ZE/IE: the flag guard below uses OE|UE|DE only.
* A global PC24 corrupts SSE2 test code through the 32-bit CRT helpers that still use the x87 (uint64->double conversions, `ldexp`, software `fma`): the x87
  reference helpers switch to PC24 only inside themselves (`Pc24` guard in the test); `Fp.h` therefore avoids `std::fma`.
* The `x87` result of `fmul`/`fadd` with a 53-bit operand at PC24 is the SINGLE-rounded result on Rosetta, as on Intel hardware (T3, 157,396 adversarial cases).

## 2. What the exe really does (reference semantics to preserve)
* Main thread: x87 PC=24 after D3D9 `CreateDevice` (no FPU_PRESERVE); other threads (audio streaming) stay PC=53. All game code is MSVC x87, `/arch:IA32 /fp:precise`
  in the standalone build. Float values travel in memory as floats; `double` locals are 8-byte spills of values that were rounded to 24 bits by the producing op.
* Float constants are loaded from `.rdata`; double constants (`fmul qword`) keep 53 bits until the op rounds the product (T3); int operands come in through `fild` (exact).
* Transcendentals are inline `fsin/fcos/fptan/fpatan/fyl2x` (359/302/22/232/51 sites in 327 functions); results are 64-bit-mantissa and stay on the stack until the next op.
* `(int)f` is `_ftol2` (T5). CRT `pow/exp/floor/ceil/modf/_CIasin/_CIacos` are the exe's own CRT (oracle-able machine code).
* Win32 reference build: keeps `/arch:IA32` for everything not yet migrated; migrated TUs are `/arch:SSE2` (per-source property, mixed objects link fine: the x86 cdecl ABI
  passes/returns floats the same way in both modes; float return in ST0 is converted by the compiler).

## 3. Design: `notsa::fp` (`source/game_sa/Core/Fp.h`, header-only)
Layers, in the order a function is migrated:
1. **Plain `float`** (default). Valid whenever no intermediate leaves the float exponent range (T1/T2). `double` intermediates of the ports are rewritten as `float`
   (T8). Build rules for the portable targets: `-ffp-contract=off` (T7), no `-ffast-math`, no x87 (`-mfpmath=sse` on x86), MXCSR/FPCR in the default mode
   (round-to-nearest, no FTZ/DAZ): `SseModeIsGameCompatible()` / `SetSseModeGameCompatible()` called at game-thread start and after every call into a driver/audio/video library.
2. **`F24` + `RangeGuard`** for the oracle'd primitives that must also be exact for NaN/inf/denormal/huge inputs. `RangeGuard g; r = Impl<float>(); if (!g.Clean()) r = Impl<F24>();`.
   `Clean()` is false iff the float run set OE/UE/DE (SSE: MXCSR, arm64: FPSR OFC/UFC/IDC; x87 TU: always clean). Sticky flags are only written when one is left over (one
   `stmxcsr` per call). Rules for the templated body: write it once over `T`; `Spill(x)` where the original stores to a float variable; `Fl(x)` for stored results; only
   `T(float)` constants; commit outputs after the guard (so the slow path can restart from the inputs); `FP_NOINLINE` on the instantiations and `#pragma fenv_access(on)`
   (MSVC) / `#pragma STDC FENV_ACCESS ON` (clang) in the TU.
3. **Mixed-precision helpers** (T3/T4): `MulD(F24, double)`, `AddD`, `Round24Mul(double, double)` (int*float), `Round24Add`, `TwoProductErr`, `RoundToOdd`.
4. **Integer conversions** (T5): `Ftol`, `Ftol64`, `FtolU` (exe `_ftol2` semantics). Replaces the standalone-only `FtolExe.cpp` override for the portable build; every float->int
   cast of game code must go through it (AST-driven codemod, slice F-5).
5. **Transcendentals**: `x87::sin/cos/tan/atan2/asin/acos/atan/sqrt/log2/log10/ln/sincos` keep the signature `double(double...)`. Win32 reference: the naked asm (unchanged).
   Portable: `notsa::fp::soft` (`X87Soft.h`, integer-only software x87 incl. precision-controlled `add/mul/div/sqrt` so the CRT `asin/acos` sequences (rounded per step at PC24)
   can be re-built exactly) returning `ToDoubleRoundToOdd(F80)` (round-to-odd keeps the later rounding to 24 bits identical to rounding the 80-bit value directly). See F1_soft.md.
6. **Library math** (`pow/exp/floor/ceil/modf/fmod`): UCRT == exe CRT on x86 (oracle rows in `x87_oracle_test`), NOT guaranteed on arm64/glibc (`pow`, `exp` can differ in the last
   bit): port the exe's CRT routines (`0x8220F0 pow`, `0x825E10 exp`, floor/ceil/modf are exact in any libm) as `notsa::fp::crt::*` and gate with the same oracle rows (slice F-7).

### 3.5 Software x87 transcendentals (helper result, `.notes/reports/F1_soft.md`, `source/game_sa/Core/X87Soft.h`, commits 184c443e 315167e2 cec9eb7e)
Integer-only `notsa::fp::soft` (F80, `Add/Sub/Mul/Div/Sqrt` at PC24/53/64, `Sin/Cos/SinCos/Tan`, `Atan2/Atan`, `Yl2x`, `Yl2xp1`, `ToDoubleRoundToOdd`) compared against the host x87
(Rosetta) with the full 80-bit result. Findings: Rosetta's fsin/fcos/fptan are Intel-like (66-bit pi reduction, not correctly rounded, 1-ulp bias for large reduced arguments), so
bit-exactness cannot come from a correctly rounded implementation.
| op | cases | bit-exact 80 bit | equal after rounding to float | after rounding to double |
|---|---|---|---|---|
| + - * sqrt | 12M each | 100% | 100% | 100% |
| div | 12M | 99.9999% | 100% | 100% |
| sin / cos | 13.5M | 95.1% / 96.5% (game-like inputs 92.9% / 95%) | 100% | 99.997% / 99.998% |
| tan | 13.5M | 88.7% (game-like 83%) | 100% | 99.994% |
| atan2 | 10.1M | 96.0% (float pairs 92.6%) | 100% | 99.9988% |
| asin / acos chain (PC24 steps) | 15M | 98.1% / 96.7% | 100% | 99.999% |
| log2 / fyl2xp1 | 6.8M / 2.3M | 99.4% / 89.4% | 100% | 99.9997% / 99.995% |
Residual misses are always 1 ulp of the 80-bit value (2 for fptan/fyl2xp1); after the next PC24 rounding (float) they vanish in all 10^7-scale runs; at the double level a 1-ulp difference
appears in ~3e-5..6e-5 of calls, i.e. only a consumer that stores the transcendental into a `double` (rare in this code base: results feed float ops) can see it. Speed (clang arm64): sin/cos
~125 ns, sincos 212, tan 255, atan2 211, fyl2x 176, sqrt 170 (MSVC x86 estimated 3-5x slower). Open: wiring into `x87::*` non-asm branch (slice F-4), `f2xm1`/`fscale` not done (unused inline),
the oracle is Rosetta, not Intel/AMD silicon (rerun `x87_soft_test` on real hardware before trusting 80-bit equality there; float-level equality is architecture-independent in practice).

Policy tiers: **tier 1** = oracle'd primitives (exact for all inputs, incl. specials: layers 2-3); **tier 2** = everything else: layer 1 only; exact except for exponent-range
excursions (|values| > ~1e19 or < ~1e-19, i.e. already-broken game states) and NaN payloads. Record/replay hashes (stream goal) must canonicalise NaN.
Where the Win32 reference build runs x87 and a portable build runs SSE, the oracle cannot compare whole frames bit-exactly for tier-2 code in the pathological regime; the
replay harness compares states in the regular regime only.

## 4. Pilot: CCollision sphere / line / triangle primitives (slice F-1)
Files: `source/game_sa/Collision/CollisionPrimitives.cpp` (NEW, 9 functions moved out of `Collision.cpp`: TestSphereSphere 0x411E70, ProcessSphereSphere 0x416450,
TestLineSphere 0x417470, ProcessLineSphere 0x412AA0, TestLineTriangle 0x413AC0, ProcessLineTriangle 0x4140F0, ProcessVerticalLineTriangle 0x4147E0,
TestSphereTriangle 0x4165B0, ProcessSphereTriangle 0x416BA0), `source/CMakeLists.txt` (per-source `/arch:SSE2`, added to the 6 oracle test targets that link Collision.cpp),
`Core/Fp.h`. Hooks are unchanged (`InjectHooks` still takes `&CCollision::X`).
Structure of each function: wrapper (debug-setting check, `RangeGuard`, commit of the outputs) + `template<class T> NOTSA_FP_NOINLINE Impl` instantiated for `float` and `F24`.
Outputs are written to a local struct and committed after the guard, so the slow path restarts from the inputs.

Oracle (game_oracle_test, the exe's own machine code, random + special inputs: NaN, +-0, denormal, 1e30 "huge", +-inf, 1e-20 "tiny"; rows `[wide range]` use scales
1e-18 ... 1e30 for spheres/lines so that squares overflow/underflow the float range while the triangles keep their int16/128 ranges):

| build | cases per row | rows | PC24 hard mismatches | PC24 NaN-payload-only (strict) | notes |
|---|---|---|---|---|---|
| Release (`F1R`, final code) | 1,000,000 | 18 (9 functions x {normal, wide range}) | **0** | 163,456 (1.5% of the Process* rows; the pre-pilot x87 port has the same 1.5%: untouched output words whose NaN payload the port quiets, `fld`+`fstp` vs `mov`) | F24 slow path taken 8.9M times in this run (specials + wide rows) |
| Debug (`F1`), all 36 `CCollision::*` rows | 150,000 | 36 | **0** | 34,821 | |
PC53 (CRT default, informational): thousands of differences in every Process* row, as for the x87 port with `double` intermediates compiled to float, because the SSE
code is float-exact PC24 by construction (PC53 only matters for the audio thread, which does not run collision).

The "5 infinite-radius mismatches" of the PERF prototype: with an INFINITE RADIUS the float and F24 instantiations agree with the exe (rows pass with radius = +-inf in the
special class). The remaining historical mismatches were spheres with an infinite CENTRE: `inf - inf = NaN` in the edge tests makes `numPassed == 0` and the exe continues with
uninitialised stack locals (returns `true` with garbage `0xCCCCCCCC` coordinates) - undefined behaviour that the old x87 port "fails" identically (4 in 100,000 cases);
the test now exempts non-finite sphere centres like it already exempted NaN ones.

Speed (`BENCH=1`, Release, same process, same inputs, port vs exe at PC24, host load ~10; `.notes/reports/F1_tools/pilot_bench.txt`), ns per call:
| function | exe | x87 port (before) | SSE2 port (now) | speed-up vs x87 port |
|---|---|---|---|---|
| TestSphereTriangle | 1,480-1,540 | 1,917-1,954 | 25 | 77x |
| ProcessSphereTriangle | 1,234-1,247 | 1,641-1,645 | 43 | 38x |
| TestLineTriangle | 833-838 | 1,192-1,196 | 19 | 63x |
| ProcessLineTriangle | 887-893 | 1,264-1,280 | 33 | 38x |
| TestSphereSphere | 278-281 | 258 | 10 | 27x |
| ProcessSphereSphere | 553-554 | 689-692 | 22 | 31x |
| TestLineSphere | 1,228-1,237 | 1,439-1,450 | 13 | 111x |
| ProcessLineSphere | 787-800 | 953-963 | 23 | 41x |
Slow-path (F24) calls during the benchmark: 0. In the walking scene of `reports/PERF.md` the Test*/Process* group costs ~4.5 ms of a 36 ms frame (1,216 TestSphereTriangle calls/frame
alone ~2.3 ms): the pilot removes ~95% of it (expected frame ~32 ms = the original's).

Findings that shaped the design (all with regression coverage):
1. `#pragma fenv_access(on)` (needed to keep the compiler from moving FP operations across the flag reads) made the whole TU 16x slower (TestSphereTriangle 452 ns instead of 28 ns).
2. Without it, MSVC /O2 hoisted/CSE'd the inlined `_mm_getcsr` pair and the guard silently never fired for functions without output parameters (59 / 6,834 wrong results in the
   special/wide rows of TestSphereSphere / TestLineSphere). The fix is architectural: sticky flags are read AFTER the computation through an opaque NOINLINE function that
   receives the addresses of the results (data dependency), and cleared there when set; stale flags can only cause a spurious slow path.
3. MSVC compiles a float-RETURNING function of an /arch:SSE2 TU to x87 code (result in ST0): Impl functions return bool / out-parameters (`fp_theory_test` T13 reproduced it).
4. The 32 bit CRT's `fma` (and `ldexp`, uint64->double) use the x87: a process-wide PC24 broke them; `Fp.h` uses Dekker products and no `fma`.
5. PCH: `/arch:SSE2` against the `/arch:IA32` PCH compiles without complaint (and without the PCH the TU re-defines the non-inline header functions -> LNK2005); keep the PCH.
6. ODR: the TU's COMDAT inline functions were checked (`dumpbin /symbols`): only float helpers and `isfinite/abs/fpclassify` wrappers (identical in both modes); no inline function with
   x87-dependent double arithmetic is emitted into it. The rule is in the file header; F-0 adds an automatic check (`dumpbin` on every `/arch:SSE2` TU object).

## 5. Census (grep, `source/` without standalone/fakerw/RW headers; script `.notes/reports/F1_tools/fp_census.py`)
| item | count | files |
|---|---|---|
| `x87::sin/cos/tan/atan2/asin/acos/atan/sqrt/log10/sincos` calls | 692 | 112 (Cam.cpp 152, Automobile 40, Camera 25, Vehicle 21, Bike 20, Birds/PedIK/TaskSimplePlayerOnFoot 16, CarCtrl/Radar/Quaternion 15, WeaponEffects 13, Weapon/Plane/Bmx 12 ...; the top 12 files hold 50%) |
| `(double)` casts | 8,857 | 210 (Cam.cpp 1,235, CarCtrl 1,077, Vehicle 506, Collision.cpp 444 (before the pilot), WaterLevel 391, Heli 245, Fx 208, Camera 201, Automobile 206, PathFind 189, Stats 187, ...) |
| `double` declarations | 2,388 | |
| double literals (no `f`) | 2,681, of which NOT exactly a float: 412 | the 412 need `MulD`/`AddD`/`Round24Mul` (T3); the other 2,269 are exact floats (`0.5`, `2.0`, `1.0`) and can just become float constants |
| `std::` math (sin cos tan atan2 sqrt pow floor ceil exp log fmod modf abs ...) | 1,010 (+31 `sinf`-style) | pow/exp/log: slice F-7; floor/ceil/modf/fmod/abs/sqrt are exact in any libm |
| `ExeRecip*` (already portable constants) | 263 | |
| `Ftol/_ftol2` explicit | 93 (+ every plain float->int cast, count needs the AST tool) | |
| inline asm / naked | 37 asm lines / 15 naked | X87Intrinsics 12, Matrix 10, FtolExe 7, Radar 5, Group13_15 4, PathFind 3 (stream C/F-4) |

## 6. Slice plan
A slice = one sonnet agent + one review pass. Every slice commits its files individually (`git commit -- <files>`), builds the changed TUs, runs the listed oracle rows
(`tools/standalone/run_all_tests.sh <builddir> <regex>`) and must show **0 hard mismatches at PC24, specials included** (NaN-payload-only differences are reported, not counted).
Order: F-0 -> F-1 (done, below) -> F-2/F-3/F-4 in parallel -> F-5 -> F-6a..f -> F-7 -> F-8.

| slice | content | size | acceptance |
|---|---|---|---|
| F-0 infra | `Fp.h` (done), `fp_theory_test` into `run_all_tests.sh` (it needs no exe), CMake helper `notsa_sse2_sources(<files>)` replacing the per-file property, compile flags for the portable targets (`-ffp-contract=off`, `-mfpmath=sse`, `/fp:precise`, no fast-math), `fp::SetSseModeGameCompatible()` call at game-thread start, AST tool `tools/standalone/fp_audit.py` (clang JSON AST via the C1 infrastructure): classifies every floating BinaryOperator/CallExpr/CastExpr of a TU (float-only chain / mixes a non-float-exact double / int->float conversion / float->int conversion / `double` variable use) and emits a per-function TSV | 1 | theory test green; fp_audit reproduces the census numbers within 2% |
| F-1 pilot | CCollision sphere/line/triangle primitives (9 functions) SSE2, float + F24 | 1 (done) | section 4 |
| F-2 collision rest | `Collision.cpp` remaining ~35 functions (TestLineBox_DW, ProcessSphereBox/LineBox, DistToLine*, ClosestPointOnLine, PointInTriangle, ProcessColModels, camera cone casts ...) moved to the SSE2 TU in the same template style; `CColTrianglePlane`, `CColLine/Sphere/Box` helpers | 3 | `game_oracle_test` all `CCollision::*` rows + `[wide range]` rows; PERF walking frame re-measured (target: collision ms/frame <= original) |
| F-3 math core | `Vector.cpp`, `Vector2D.cpp`, `Matrix.cpp`, `Quaternion.cpp`, `General.cpp`, `Maths.cpp`, `Placeable.cpp`, `CMatrix` inline users: float rewrite (Normalise, Magnitude, TransformPoint/Vector, Multiply, Rotate*, Quat slerp ...); `CVector` operators stay float | 3 | game_oracle/physics_oracle/x87_oracle rows for these functions; bench: Normalise 62 ns -> < 10 ns |
| F-4 transcendentals | `X87Soft.h` finished (F1_soft.md) and wired as the non-x87 branch of `x87::*`; CRT `asin/acos` rebuilt from the exact sequences; fyl2x/fscale for `log10` | 2 | x87_oracle_test INFO rows turn into counted rows: soft vs host x87 0 mismatches after float/double rounding on 10^7 random + specials; speed <= 2 us per call |
| F-5 ftol | `fp::Ftol` everywhere: the AST tool lists every FloatingToIntegral cast; codemod `(int32)f` -> `notsa::fp::Ftol(f)` (and short/uint8/uint32/int64 variants) in game TUs; portable build no longer needs `FtolExe.cpp` | 3 | `review_oracle_test` "_ftol2 override" row; AST tool reports 0 raw casts in game_sa/ |
| F-6a..f per-op float sweep | the 8,857 casts in ~25-file batches, ordered by oracle coverage: (a) vehicles (Automobile/Bike/Bmx/Plane/Heli/Boat/Train/Vehicle ~ 1.1k casts), (b) peds + tasks (TaskTypes 629 files, 1.2k casts), (c) camera (Cam 1.2k + Camera + IdleCam + CamPath), (d) world/physics (Physical, WaterLevel, Rope, Garage, PathFind, CarCtrl 1.1k), (e) scripts/commands + stats (Group*.cpp, Stats 187), (f) fx/audio/render/hud (Fx, Audio, Radar, PointLights, TimeCycle, Weather) | 8 | per batch: oracle rows covering its functions at 0 hard mismatches (physics/gameplay/world/logic oracle tests already cover ~60% of the functions), `fp_audit` finds no remaining `double` arithmetic on float-derived values and no non-exact double literal outside `MulD`; functions without an oracle row get one before the rewrite (the old x87 version is the oracle: keep it behind `#if NOTSA_FP_REFERENCE` for one release and compare both in a unit test) |
| F-7 library math | `notsa::fp::crt::pow/exp/log` ported from the exe CRT machine code (0x8220F0 pow, 0x825E10 exp), floor/ceil/modf verified; 120+30 pow sites, 58+19 asin/acos | 2 | `x87_oracle_test` CRT rows 0 mismatches vs the exe at PC24 with the portable code compiled for SSE2 |
| F-8 flip | Win32 reference gets `/arch:SSE2` globally (x87 only inside `x87::*` asm) -> run the whole oracle suite + soak; arm64/x64 targets compile the same sources | 1 | full `run_all_tests.sh` (35 tests) PC24 0 hard mismatches; soak 30 min no NaN diff vs the previous reference; replay hash equal on x86 SSE2 vs x87 for the regular regime |
| total | | 28 | |
