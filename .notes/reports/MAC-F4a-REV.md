# MAC-F4a-REV: review of F-4a (afd0b7e3), soft x87 lift of the inline asm sites

Method: (a) `git diff afd0b7e3~1 afd0b7e3` on all non-generated sources: 0 removed lines, every change is an insertion (`#ifdef NOTSA_PORTABLE / #include .inc / #else` ... `#endif`, `#ifndef NOTSA_PORTABLE` around single `_asm push/pop edx`, `#elif defined(NOTSA_PORTABLE)` before an existing fallback) => the MSVC branch is the original asm byte for byte. (b) `x87lift_sites.py gen` regenerates the 20 `.inc` identically (no diff). (c) lifter/Fpu/Cpu read line by line (operand order, ST(i) indexing, fcom flag mapping, sizes). (d) llvm-cov of the host half of f4a_lift_test over all 20 `.inc`, X87Sites.h, FtolExe.cpp: every line of every `.inc` executes, and every `if (..) goto` is taken both ways (Euler: 54..10k hits per side). Uncovered in X87Lift.h: StF64, StSt, ArithF64, Top (unused by any site; their semantics are right by inspection). (e) re-ran the original test (baseline reproduces: 43 rows, 5/5 mutants killed), then extended it (below) and re-ran with fresh golden files: seed 11 x 20000 and seed 5 x 60000, all rows OK. (f) 15 touched TUs syntax-checked with the PortableMac compile flags (PCH bypassed) after my edits: all rc 0.

## Fixed (2 real bugs, 1 latent)
| # | where | problem | fix / evidence |
|---|---|---|---|
| 1 | `FtolExe.cpp` portable `Ftoul2` | `_ftoul2` runs `fldcw` with `or eax,0x0C00` BEFORE the biased `fsub`, so that fsub rounds TOWARD ZERO at PC24; the portable code did a nearest-even PC24 sub. Differs for 2^63 < x < 2^64 with a full mantissa (the old rows never fed such x: only floats and 2^63+1). Found by the new rows: 231 / ~1600 such cases differed | rewritten in integer arithmetic (diff = mantissa field << 11, cut to 24 significant bits; x >= 2^64 / inf / NaN -> 0). ftoul2 now 20000/20000 and 60000/60000 incl. +-2^63..2^66 with random 52 bit mantissas |
| 2 | `X87Intrinsics.h` portable `x87::sincos` | returned two round-to-odd doubles (the float-level helpers), but the asm stores `fstp qword` = nearest-even; |x| >= 2^63 pushes nothing so the asm stores x and an empty-stack indefinite | now SinCos + `ToDouble` (RNE), c2 case reproduced; no game caller today (grep), rows `x87::sincos_f` 20000/20000, `_d` within 1 ulp (see tolerance) |
| 3 | test | no tolerance table although the header said so; run exited 1 on the known deviations | `Tolerance()`: Group24b_SinCos / Group13_AcosCrt / x87::sincos_d (double stores, 1 ulp, ~3e-5 of cases: seed 5 had 2/60000 on AcosCrt) and AudioLog10_x10/x20 (F-4 TODO, ~23 %) are allowed, everything else must be 0 |

## Test extensions (cheap, all in tests/portable/f4a_lift_test.cpp)
* `Rng::special()`: +signalling NaNs, denormal/min-normal edges, PC24 tie operands (1+-2^-24, 2^-24, 2^23, 2^24-1, 2^31, 2^63 +-1), pi/2, pi, 2pi neighbours, 0.5/1.5/2.5 (special prob 2% -> 4%).
* `Rng::dspecial()` (+-0, double denormals, max, inf, NaN, SNaN, +-1 +-ulp, k*pi/2 doubles, 2^52, 2^63 neighbours), mixed into `dbl()` (4%) and FtolArg (+2^63..2^66 full mantissa).
* New rows: x87::{sqrt,log2,log10,ln}_signed (negatives, 0, inf, NaN: fyl2x/fsqrt domain), x87::{atan,asin,acos,tan}_special, x87::sincos_f/_d. 43 -> 53 rows. Side effect: NaN-payload-only differences in the Matrix rows (SNaN inputs), 0 hard.
* RNG calls kept in separate statements (evaluation order of arguments must not differ between cl and clang).

## Per-site verdict
| site | verdict | notes |
|---|---|---|
| Matrix SetRotate / RotateX / Y / Z / Rotate | OK | fsubp st(1),st(0) = st1-st0 (16 uses, MSVC follows Intel, test bit exact); fxch/fchs/ST(i) after push/pop verified by coverage + mutants |
| Matrix ConvertTo/FromEulerAngles | OK | only je/jne/jmp (ZF); add/sub/shl set only ZF/PF (no CF user); pointer args via handles; table lookups movzx byte |
| Quaternion Slerp | OK | fcomp + `test ah,0x41; jne`: C0|C3 mapping right; unordered sets all three |
| Quaternion CalcThetaFromQuats | OK | fld tbyte qnan via FromBytes, fdivr, mode dispatch both ways covered |
| Cover FindDirFromVector / FindVectorFromDir | OK | fild dword only (lifter asserts), out pointer as handle |
| Entity GetIsTouching | OK | fcompp = Com(st1, 2 pops); `fnstsw word ptr` -> W16; only C0/C3 masked by caller (TOP/C1 not modelled, harmless) |
| Placeable GetRoll | OK | one substituted instruction (`m_matrix` is 8 bytes in the portable layout); CMatrix right/up/at offsets are static_assert'ed |
| Maths SinTableEntry | OK | Cpu(53) = static init before D3D sets PC24 |
| PathFind heading, Group13 DivideAssign / AttractorDir / AcosCrt / Fsqrt, Group24b SinCos | OK | AcosCrt and Group24b store doubles: 1 ulp in ~3e-5 (Rosetta fpatan/fsin vs soft 80 bit residual, F1_soft.md) |
| Radar SetRadarAngle / Atan2 | OK | fst float + sin/cos of the UNROUNDED value; test copy of the asm identical to Radar.cpp |
| AudioLog10 | OK (function) / F-4 TODO (x10, x20 products) | `test ah,1` = C0 -> floor, NaN included |
| IdleCam SinP1Half, BoneNode AtanDeg, PostEffects ExeRasterLog2, AEPed JetPackSinSpeeds, AEVehicle BikeLeanTerm | OK | test copies of the asm identical to the sources; `test ah,5; jp` => fchs only for C0 xor C2 (ChsAfterTest5 right); ExeRasterLog2 fdiv runs before fldcw (RNE) so no truncation issue; the int range guard cannot be hit (|log2 w| <= 31) |
| ftol2 / ftoi2 / ftoui2 | OK | |
| ftoul2 | FIXED (#1) | |
| x87::* (sin cos tan atan2 sqrt atan asin acos log2 log10 ln) | OK | asin/acos portable = the CRT asm sequence (add/sub/mulp/sqrt/fpatan order, fxch for acos, special cases by exponent bits) checked against the asm text |
| x87::sincos | FIXED (#2) | |
| `#else` MSVC branches / wrapping | OK | insertions only; balanced; .inc regenerates identically |

## Debatable / not changed
1. Reference = Wine + Rosetta 2 x87, not Intel/AMD silicon (F1_soft.md sec. 3). The fsin/fpatan last bits can differ on a real CPU; re-run `run_f4a_test.sh` on a real x86 box if the target is bit-exactness with the Windows exe. The control-word findings (RC truncate in `_ftoul2`) are independent of that.
2. `Cpu`/`Fpu` PC is hard-coded 24 (SinTableEntry 53). Right once D3D9 CreateDevice has run in the real exe; a portable caller before device creation (none found) would see 24 instead of 64/53.
3. `fnstsw` returns only C0/C2/C3 (TOP = 0, C1 = 0). All sites mask with 0x41/0x4100/5/1. A future site testing `cmp ah, ...` or C1 would need more.
4. Soft x87 stack has no overflow/underflow modelling (silent wrap). Sites are balanced; the underflow case that the sincos asm hits for |x| >= 2^63 is handled explicitly.
5. Lifter register file is 64 bit: `add/sub/shl` do not update CF and wrap at 64 bit. Only ZF/PF are consumed (je/jne/jp), table indices are small and positive. An unknown instruction still aborts at generation, so a new site cannot silently mistranslate.
6. x87::* float-level rows only (round-to-odd double => caller's float rounding is exact); a caller that stores the result in a double or multiplies it in double arithmetic is not bit-exact (documented in F1/F4a; asm sites that keep it on the stack are lifted instead).
7. AudioLog10 x10/x20 (F-4 TODO, ~23 % 1 float ulp) untouched: needs call-site changes (separate A/B-gated slice).
8. Win32 code is identical except `__LINE__` immediates in 4 TUs without `#line` (Cover, Entity, PostEffects, AEVehicleAudioEntity), already reported by F4a; harmless unless a byte compare of those TUs is wanted (`#line` restores after each block would fix it).
9. Every `Cpu` call zero-fills a 1 KB stack; Matrix::Rotate-type callers are hot (thousands per frame) but each soft op is integer-only ~100s of ns; measure in a soak if frame time matters.

## Files
source/standalone/FtolExe.cpp (Ftoul2), source/game_sa/Core/X87Intrinsics.h (sincos), tests/portable/f4a_lift_test.cpp. Run: `tools/portable/run_f4a_test.sh 20000 11` (wine, ~15 s), `MUTATE=1` 5/5 killed. No background tasks / wine processes left.
