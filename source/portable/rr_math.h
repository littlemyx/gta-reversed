#pragma once
/*
 * SH2 GTASA_RR_PORTABLE_MATH: every game float math call goes through the V8 fdlibm port (source/portable/v8_ieee754, V8 tag 10.2.154.26) so that a TS port
 * (Node 18.15 / V8 10.2.154.26: Math.fround(Math.f(x))) can reproduce the results bit for bit.
 *   float arg(s):  float -> double -> ieee754::f -> float          (TS: Math.fround(Math.f(x)))
 *   double arg(s): ieee754::f                                      (TS: Math.f(x))
 *   integer args:  as double
 * sqrt / sqrtf / floor / ceil / fabs / fmod are not routed (IEEE correctly rounded or exact in every libm).
 * Mechanism (same as msvc_rand.h): function-like macros `sin(...)` -> `rrm_sin(...)` applied after the CRT headers (StdInc.h). `std::sin(x)` becomes `std::rrm_sin(x)`,
 * hence the using-declarations into std at the bottom. Taking the address of a math function is not routed (see .notes/STATEHASH_SPEC.md section 11).
 * Only active with -DGTASA_RR_PORTABLE_MATH=ON (NOTSA_RR_PORTABLE_MATH); the normal builds never see an effect of this file.
 */
#ifdef NOTSA_RR_PORTABLE_MATH
#include <cmath>
#include <math.h>
#include <type_traits>
#include "src/base/ieee754.h"

namespace notsa { namespace rrm { // C++11 spelling: librw (C++11) force-includes this header too
template<class T> struct IsA { static const bool value = std::is_arithmetic<typename std::remove_cv<T>::type>::value; };
template<class T> struct IsF { static const bool value = std::is_same<typename std::remove_cv<T>::type, float>::value; };
// result type: float only if every argument is float (C++ <cmath> overload rule), else double (long double is folded to double: not used by the game)
template<class A, class B = float> struct R { typedef typename std::conditional<IsF<A>::value && IsF<B>::value, float, double>::type type; };
}} // namespace notsa::rrm

#define NOTSA_RRM_UNARY(name) \
    template<class A> inline typename std::enable_if<::notsa::rrm::IsA<A>::value, typename ::notsa::rrm::R<A>::type>::type \
    rrm_##name(A x) { return (typename ::notsa::rrm::R<A>::type)::v8::base::ieee754::name((double)x); }
#define NOTSA_RRM_BINARY(name) \
    template<class A, class B> inline typename std::enable_if<::notsa::rrm::IsA<A>::value && ::notsa::rrm::IsA<B>::value, typename ::notsa::rrm::R<A, B>::type>::type \
    rrm_##name(A x, B y) { return (typename ::notsa::rrm::R<A, B>::type)::v8::base::ieee754::name((double)x, (double)y); }
NOTSA_RRM_UNARY(sin) NOTSA_RRM_UNARY(cos) NOTSA_RRM_UNARY(tan) NOTSA_RRM_UNARY(asin) NOTSA_RRM_UNARY(acos) NOTSA_RRM_UNARY(atan)
NOTSA_RRM_UNARY(exp) NOTSA_RRM_UNARY(log) NOTSA_RRM_UNARY(log10) NOTSA_RRM_UNARY(log2) NOTSA_RRM_UNARY(log1p) NOTSA_RRM_UNARY(expm1)
NOTSA_RRM_UNARY(sinh) NOTSA_RRM_UNARY(cosh) NOTSA_RRM_UNARY(tanh) NOTSA_RRM_UNARY(cbrt)
NOTSA_RRM_BINARY(atan2) NOTSA_RRM_BINARY(pow)
#undef NOTSA_RRM_UNARY
#undef NOTSA_RRM_BINARY
// the f-suffixed C functions (float in, float out)
#define NOTSA_RRM_F(name) inline float rrm_##name##f(float x) { return (float)::v8::base::ieee754::name((double)x); }
NOTSA_RRM_F(sin) NOTSA_RRM_F(cos) NOTSA_RRM_F(tan) NOTSA_RRM_F(asin) NOTSA_RRM_F(acos) NOTSA_RRM_F(atan) NOTSA_RRM_F(exp) NOTSA_RRM_F(log) NOTSA_RRM_F(log10) NOTSA_RRM_F(log2)
NOTSA_RRM_F(sinh) NOTSA_RRM_F(cosh) NOTSA_RRM_F(tanh) NOTSA_RRM_F(cbrt)
#undef NOTSA_RRM_F
inline float rrm_atan2f(float y, float x) { return (float)::v8::base::ieee754::atan2((double)y, (double)x); }
inline float rrm_powf(float x, float y) { return (float)::v8::base::ieee754::pow((double)x, (double)y); }

namespace std {
using ::rrm_sin; using ::rrm_cos; using ::rrm_tan; using ::rrm_asin; using ::rrm_acos; using ::rrm_atan; using ::rrm_exp; using ::rrm_log; using ::rrm_log10; using ::rrm_log2;
using ::rrm_log1p; using ::rrm_expm1; using ::rrm_sinh; using ::rrm_cosh; using ::rrm_tanh; using ::rrm_cbrt; using ::rrm_atan2; using ::rrm_pow;
using ::rrm_sinf; using ::rrm_cosf; using ::rrm_tanf; using ::rrm_asinf; using ::rrm_acosf; using ::rrm_atanf; using ::rrm_expf; using ::rrm_logf; using ::rrm_log10f; using ::rrm_log2f;
using ::rrm_sinhf; using ::rrm_coshf; using ::rrm_tanhf; using ::rrm_cbrtf; using ::rrm_atan2f; using ::rrm_powf;
}

#define sin(...) rrm_sin(__VA_ARGS__)
#define cos(...) rrm_cos(__VA_ARGS__)
#define tan(...) rrm_tan(__VA_ARGS__)
#define asin(...) rrm_asin(__VA_ARGS__)
#define acos(...) rrm_acos(__VA_ARGS__)
#define atan(...) rrm_atan(__VA_ARGS__)
#define atan2(...) rrm_atan2(__VA_ARGS__)
#define exp(...) rrm_exp(__VA_ARGS__)
#define log(...) rrm_log(__VA_ARGS__)
#define log10(...) rrm_log10(__VA_ARGS__)
#define log2(...) rrm_log2(__VA_ARGS__)
#define log1p(...) rrm_log1p(__VA_ARGS__)
#define expm1(...) rrm_expm1(__VA_ARGS__)
#define sinh(...) rrm_sinh(__VA_ARGS__)
#define cosh(...) rrm_cosh(__VA_ARGS__)
#define tanh(...) rrm_tanh(__VA_ARGS__)
#define cbrt(...) rrm_cbrt(__VA_ARGS__)
#define pow(...) rrm_pow(__VA_ARGS__)
#define sinf(...) rrm_sinf(__VA_ARGS__)
#define cosf(...) rrm_cosf(__VA_ARGS__)
#define tanf(...) rrm_tanf(__VA_ARGS__)
#define asinf(...) rrm_asinf(__VA_ARGS__)
#define acosf(...) rrm_acosf(__VA_ARGS__)
#define atanf(...) rrm_atanf(__VA_ARGS__)
#define atan2f(...) rrm_atan2f(__VA_ARGS__)
#define expf(...) rrm_expf(__VA_ARGS__)
#define logf(...) rrm_logf(__VA_ARGS__)
#define log10f(...) rrm_log10f(__VA_ARGS__)
#define log2f(...) rrm_log2f(__VA_ARGS__)
#define sinhf(...) rrm_sinhf(__VA_ARGS__)
#define coshf(...) rrm_coshf(__VA_ARGS__)
#define tanhf(...) rrm_tanhf(__VA_ARGS__)
#define cbrtf(...) rrm_cbrtf(__VA_ARGS__)
#define powf(...) rrm_powf(__VA_ARGS__)
#endif // NOTSA_RR_PORTABLE_MATH
