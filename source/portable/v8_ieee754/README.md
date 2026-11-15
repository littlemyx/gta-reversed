# v8_ieee754: V8's fdlibm port, for the RR reference build

* Upstream: V8 tag **10.2.154.26** (the V8 of Node 18.15), files `src/base/ieee754.cc` and `src/base/ieee754.h`
  (https://github.com/v8/v8/blob/10.2.154.26/src/base/ieee754.cc, https://github.com/v8/v8/blob/10.2.154.26/src/base/ieee754.h).
* `src/base/ieee754.cc` and `src/base/ieee754.h` are **byte-for-byte the upstream files** (BSD-style license + the FreeBSD fdlibm notices are in their headers). Do not edit them.
* Functions: acos acosh asin asinh atan atanh atan2 cos exp expm1 log log1p log2 log10 cbrt sin tan sinh cosh tanh pow (namespace `v8::base::ieee754`).
* `src/base/{macros.h,build_config.h,base-export.h,overflowing-math.h}` are minimal stand-ins (written for this repo, not upstream) for the few V8 helpers the two files include
  (`bit_cast`, `Divide`, `NegateWithWraparound`, `SubWithWraparound`, `V8_INLINE`, `V8_CC_MSVC`, ...).
* Used only with `-DGTASA_RR_PORTABLE_MATH=ON` (define `NOTSA_RR_PORTABLE_MATH`): `source/portable/rr_math.h` (force-included by StdInc.h and for librw) turns every game
  `sin/cos/tan/asin/acos/atan/atan2/exp/log/log10/log2/pow/...` call into `float -> double -> ieee754::f -> float` (double args: `ieee754::f`). The normal builds do not compile this directory.
* TS side: `Math.fround(Math.f(x))` in Node 18.15 / V8 10.2.154.26 computes exactly this.
