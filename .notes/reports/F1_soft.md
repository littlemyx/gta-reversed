# F1-soft: integer-only software x87 (`source/game_sa/Core/X87Soft.h`)

Files: `source/game_sa/Core/X87Soft.h` (header-only, `notsa::fp::soft`), `tests/standalone/x87_soft_test.cpp` (oracle comparison, build by hand, see its header comment).
Oracle = the x87 of this machine: Wine + Rosetta 2 (Apple silicon), inline asm at PC24/PC53/PC64, full 80-bit result via `fstp tbyte`.
Status: arithmetic (+ - * / sqrt at PC 24/53/64), fsin, fcos, fsincos, fptan, fpatan, fyl2x, fyl2xp1, conversions (float/double/int64/80-bit, round-to-odd double) done and
oracle-tested. Not done (not used inline by the exe): f2xm1, fscale.

## 1. What Rosetta's x87 does (measured)

| instruction | behaviour of the reference |
|---|---|
| fadd/fsub/fmul/fdiv/fsqrt | IEEE nearest-even at the PC (24/53/64 bits), extended exponent range. Denormal results are rounded at the FIXED bit position of the PC (denormal 1 + denormal 1 = 0 at PC24/53). NaN rules = Intel's (SNaN quieted, QNaN beats SNaN, larger significand, unsupported encodings -> real indefinite): all 100% identical to soft on the ~90x90 special-value cross product. Only deviation: fdiv at PC64 for a quotient within ~2^-126 of a rounding tie (x/(1+2^-63): host drops the remainder), 1e-5 of the cases |
| fsin/fcos/fsincos/fptan | **Intel-like**: argument reduced with the 66-bit pi (`0x3243F6A8885A308D3/2^64`): `cos(1.5707963267948966)` = 6.123032e-17 (true 6.123234e-17, rel. err -3.3e-5), `sin(pi_80bit)` = -2^-64 exactly; relative error vs the true function grows ~ k*2^-66 for x near k*pi/2 (k=1e6: 6e-21, k=1e9: 4e-20 on cos). Exact reduction (no rounding of r): `r = x - q*pi66/2` reproduced bit-for-bit incl. |x| up to 2^63-1 (garbage-but-deterministic for x > 2^50). |x| >= 2^63: operand unchanged, C2 set, nothing pushed by fptan/fsincos. NaN -> NaN (SNaN quieted), +-inf -> real indefinite (IE), fptan/fsincos push the same NaN; denormal x -> sin x = x, cos = 1. Result always has the full 64-bit mantissa, independent of PC (PC24 leaves it unrounded in ST0) |
| fsin/fcos accuracy | NOT correctly rounded. Relative to the correctly rounded sin/cos of the (66-bit-pi) reduced argument r: identical for |r| < ~0.5 (mismatch 0-4%, noise <= 0.05 ulp), cos(r) for |r| >= 0.5 has a systematic +0.27 ulp offset (+0.38 at 0.78) and ~0.15 ulp of extra noise, sin(r) shows the same from |r| >= 0.75 (the jump coincides with cos(0.5) = rounded-to-64-bit error +0.258 ulp: looks like a table / angle-addition step with 64-bit constants). Max |error| vs correct rounding: 1.15 ulp (80-bit); fptan: 2 ulp |
| fpatan | all special cases of Intel's table (+-0, +-inf, quadrants, 3pi/4, pi/4) identical; finite: within 1 ulp of correct rounding, not correct rounding (same +bias family as fsin). pi, pi/2 .. are the correctly rounded 64-bit values |
| fyl2x | almost correctly rounded (max 0.507 ulp vs exact), 98.4-99.8% bit-identical to correct rounding; x<0 -> indefinite, +-0 -> -+inf, 1 -> +-0, inf rules = Intel |
| fyl2xp1 | forms **1+x rounded to 64 bits first** (x < 2^-64 gives 0; small x have only the precision of 1+x). x <= -1 (outside the architectural domain) returns meaningless values (-2^31...); 1+x==1 with y != 0,1: +-2^-135 noise instead of 0 (reported as informational rows) |

## 2. What soft reproduces

Per-op totals over all distributions (sum of float/double/angle/wide/special sets, 1M cases per distribution and precision, see section 5 for the run); `float%`/`double%` = identical after rounding the 80-bit result to binary32/binary64:

| op | cases (all PC, random + special) | bit-exact 80-bit | = after float | = after double | 1-ulp diffs | >=2-ulp diffs | max ulp |
|---|---|---|---|---|---|---|---|
| add / sub / mul / sqrt | 12.0M each | 100.00% | 100% | 100% | 0 | 0 | 0 |
| div | 12.0M | 99.9999% | 100% | 100% | 103 (PC64 near-ties) | 0 | 1 |
| sin | 13.5M | 95.05% | 100.0000% | 99.9974% | 667937 | 0 | 1 |
| cos | 13.5M | 96.50% | 100.0000% | 99.9983% | 472966 | 0 | 1 |
| sincos (both outputs) | 3.4M | 95.0 / 96.3% | 100.0000% | 99.9979% | 167607 | 0 | 1 |
| tan (fptan; pushed 1.0/NaN exact) | 13.5M | 88.72% | 100.0000% | 99.9939% | 1522376 | 131 | 2 |
| atan2 (fpatan) | 10.1M | 96.00% | 100.0000% | 99.9988% | 405788 | 0 | 1 |
| asin chain (CRT, PC24/53/64) | 15.0M | 98.08% | 100.0000% | 99.9990% | 288231 | 0 | 1 |
| acos chain | 15.0M | 96.72% | 100.0000% | 99.9984% | 492023 | 0 | 1 |
| log2/log10/ln (fyl2x) | 6.8M | 99.41% | 100.0000% | 99.9997% | 40035 | 0 | 1 |
| fyl2xp1 | 2.3M | 89.39% | 100.0000% | 99.9954% | 239761 | 92 | 2 |
| conversions fld/fstp m32/m64, fild/fistp, round-to-odd property | 1.2M | 100% | | | 0 | 0 | 0 |

Game-like distributions only (floats / float products as doubles; PC64, 10^6 each), bit-exact 80-bit: sin angles in [-4pi,4pi] 92.9% (float) / 92.8% (double), [-1e3,1e3] 92.9%, [-1e6,1e6] 92.8%;
cos 95.1 / 95.0 / 95.0 / 94.9%; tan 83.1-83.2%; atan2 of float pairs 92.6%, pairs of very different magnitude 98.6%, x ~ y 85.3%, atan(x) 98.4%; log2(x) x in (0,1e6) float 99.8%, x float 2^+-30 99.3%.
Special-value sets (+-0, denormals, pseudo-denormal, +-inf, SNaN/QNaN with payloads, unnormals, 2^63 +- 1, values at k*pi/2, ...; full cross products for 2-operand ops): arithmetic 100% bit-exact
including NaN payload/sign (div PC64: 12 ties), sin 91.3%, cos 97.8%, tan 89.1%, atan2 99.0%, fyl2x 99.7%, fyl2xp1 94.2% (all remaining = 1 ulp), C2 flag 100%.
Floats: **0 float-level mismatches in 13.5M sin, 13.5M cos, 10.1M atan2, 13.5M tan, 15M asin, 15M acos, 6.8M log2** (PC24 and PC64 runs; every test exits 0).

## 3. Residual mismatches and causes

* sin/cos/fsincos 4-9% (80-bit, game-like angles), tan 10-17%, atan2 2-15% (worst: x ~ y), fyl2x 0.2-1.6%, fyl2xp1 0-21%: ALL are 1 ulp (extended) differences (tan/yl2xp1 sometimes 2), caused by the reference's own non-correct rounding
  (see 1.): soft returns the correctly rounded value of the exact function. Not reproducible without Apple's algorithm; the residual is a deterministic bias (+0.27 ulp for |r| >= 0.5) plus
  ~0.15 ulp pseudo-noise, so a model "rounded 64-bit constants + angle addition" reaches ~90% instead of ~70% in that region but never 100% (feasibility LP: extra slack 0.28 ulp needed). Not implemented (risk / low value).
* After rounding to float (the exe runs at PC24 and stores floats): 0 differences in > 10^7 cases per function. After rounding to double: 99.99% (a 1-ulp-ext difference flips the 53-bit rounding with p ~ 2^-11).
* fdiv PC64 near-tie (above), yl2xp1 noise region, yl2x/fyl2xp1 denormal-result double rounding (1 ulp of a denormal): outside any game use.

## 4. Speed (clang arm64 -O2, ns per call, random float-valued inputs)

| add | mul | div | sqrt | sin | cos | sincos | tan | atan2 | fyl2x |
|---|---|---|---|---|---|---|---|---|---|
| 15 | 10 | 30 | 170 | 125 | 123 | 212 | 255 | 211 | 176 |

At ~2000 calls/frame this is well below 0.5 ms. The same header gives identical results (hash of ~4M results) in clang arm64, clang x86_64, MSVC x86 (/W4 clean) and clang -fsanitize=address,undefined.

## 5. How to run / reproduce

Build: `cl /nologo /O2 /arch:IA32 /EHsc /std:c++20 x87_soft_test.cpp` (MSVC x86, see the test's header), run `wine ./x87_soft_test.exe [N] [seed] [ops]`.
The table is from `N=1000000 seed=101` (sin / cos / patan / the rest as separate processes: 37 s, 37 s, 46 s, 108 s, 269 s). N=100000 for everything takes ~40 s.
The test also contains the CRT asin/acos chain (every step rounded at the current PC, as in `X87Intrinsics.h`) as a regression of the intended use.

## 6. Open issues

1. Not bit-exact for sin/cos/tan/atan2/log2 at 80 bits (see above); double-level results differ in ~1e-5..1e-4 of calls (a 1-ulp(53) difference every ~10^4..10^5 calls); float results never.
2. `ToDoubleRoundToOdd` is only valid when the consumer then rounds to <= 51 bits (PC24 game mode). For PC53 consumers use `ToDouble` (nearest) - the 64-bit value is lost either way.
3. The oracle is Rosetta, not Intel/AMD silicon (those differ from each other and from Rosetta in fsin/fpatan last bits). If the target is real hardware, re-run the test there (same exe).
4. f2xm1 / fscale / fyl2xp1 corner cases not modelled (garbage domain). Exceptions flags other than C2 are not tracked.
5. Speed was measured on arm64 only; on MSVC x86 the 64x64 multiplies are 4x slower (no 64-bit hardware multiply), estimated 3-5x the arm64 times, still ~1 us per sin.
6. Integration into `X87Intrinsics.h` (replace the `<cmath>` fallbacks of `x87::sin(double)` etc. by `ToDoubleRoundToOdd(Sin(FromDouble(x)))`; asin/acos by the Add/Sub/Mul/Sqrt/Atan2 chain at the current PC) is left to the caller; `runAsin` in the test shows the chain is 100% float-identical at PC24/53/64.
