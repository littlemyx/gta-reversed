// Stream F theory test: which float operations of the original (x87, precision control PC=24) are bit-identical to portable code (IEEE single / the notsa::fp helpers).
// Standalone (no game code, no exe image): the x87 reference is MSVC inline asm executed on the host x87 (Rosetta 2 under Wine here), the "portable" side is plain C++ compiled
// for SSE2 (identical semantics to arm64 with -ffp-contract=off). Build / run (see .notes/STREAM_F_PLAN.md):
//   cl /nologo /O2 /arch:SSE2 /fp:precise /EHsc /std:c++20 tests\standalone\fp_theory_test.cpp   ->   wine fp_theory_test.exe [-n cases] [row-substring ...]
// Exit code 0 = every row whose expectation is "identical" has no mismatch. Rows marked EXPECT-DIFF document known differences (counted, never fail).
#include "../../source/game_sa/Core/Fp.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <float.h>
#include <string>
#include <vector>

using notsa::fp::F24;

// ---------------------------------------------------------------------------------------------------------------------------------
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 0x1234567ull) { for (int i = 0; i < 4; ++i) u32(); }
    uint32_t u32() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 16); }
    uint64_t u64() { return ((uint64_t)u32() << 32) | u32(); }
    int below(int n) { return (int)(u32() % (uint32_t)n); }
};
static float BitsF(uint32_t b) { return std::bit_cast<float>(b); }
static uint32_t FBits(float f) { return std::bit_cast<uint32_t>(f); }
static bool IsNanBits(uint32_t b) { return (b & 0x7F800000u) == 0x7F800000u && (b & 0x7FFFFFu) != 0; }

// float with a random sign, random mantissa and a binary exponent uniform in [lo, hi] (hi <= 127, lo >= -126)
static float GenE(Rng& r, int lo, int hi) {
    const uint32_t e = (uint32_t)(127 + lo + r.below(hi - lo + 1));
    return BitsF(((r.u32() & 1) << 31) | (e << 23) | (r.u32() & 0x7FFFFF));
}
static float GenSpecial(Rng& r) {
    switch (r.below(10)) {
    case 0: return 0.f;
    case 1: return -0.f;
    case 2: return BitsF(0x7F800000u);                                    // +inf
    case 3: return BitsF(0xFF800000u);                                    // -inf
    case 4: return BitsF(0x7FC00000u | (r.u32() & 0x3FFFFF));             // qNaN
    case 5: return BitsF(0x7F800001u + (r.u32() & 0x3FFFFF));             // sNaN
    case 6: return BitsF((r.u32() & 0x80000000u) | (1 + (r.u32() & 0x7FFFFF))); // denormal
    case 7: return BitsF((r.u32() & 0x80000000u) | 0x7F7FFFFFu);          // +-FLT_MAX
    case 8: return BitsF((r.u32() & 0x80000000u) | 0x00800000u);          // +-FLT_MIN
    default: return GenE(r, -126, 127);
    }
}
enum Cls { C_REG, C_MID, C_WIDE, C_SPEC, C_N };
static const char* ClsName[] = { "regular(2^-20..2^20)", "mid(2^-60..2^60)", "wide(any exponent)", "special(0/inf/NaN/denormal/max)" };
static float Gen(Rng& r, Cls c) {
    switch (c) {
    case C_REG: return GenE(r, -20, 20);
    case C_MID: return GenE(r, -60, 60);
    case C_WIDE: return GenE(r, -126, 127);
    default: return r.below(3) ? GenE(r, -126, 127) : GenSpecial(r);
    }
}

static unsigned g_cases = 2000000;
static std::vector<std::string> g_filters;
static int g_unexpected = 0;

struct Tally { unsigned n = 0, hard = 0, nan = 0, denorm = 0; };
// compares two float results; classifies NaN-vs-NaN (payload/sign only) and results in the float denormal range separately
static void Cmp(Tally& t, float got, float ref) {
    ++t.n;
    const uint32_t a = FBits(got), b = FBits(ref);
    if (a == b) return;
    if (IsNanBits(a) && IsNanBits(b)) { ++t.nan; return; }
    const uint32_t m = (b & 0x7FFFFFFFu);
    if (m < 0x00800000u || (a & 0x7FFFFFFFu) < 0x00800000u) { ++t.denorm; return; }
    ++t.hard;
}
static void CmpAll(Tally& t, float got, float ref) { // counts denormal-range differences as hard too
    Cmp(t, got, ref);
}
static bool Wanted(const char* name) {
    if (g_filters.empty()) return true;
    for (auto& f : g_filters) if (std::string(name).find(f) != std::string::npos) return true;
    return false;
}
static void SetPC(int bits) { unsigned cw; _controlfp_s(&cw, bits == 24 ? _PC_24 : _PC_53, _MCW_PC); }

static void Report(const char* row, const char* cls, const char* what, const Tally& t, bool expectIdentical) {
    const bool bad = expectIdentical && (t.hard || t.denorm);
    if (bad) ++g_unexpected;
    std::printf("%-46s %-30s %-34s n=%-9u hard=%-8u denormal-range=%-8u nan-payload=%-8u %s\n", row, cls, what, t.n, t.hard, t.denorm, t.nan,
                bad ? "UNEXPECTED" : (expectIdentical ? "ok" : (t.hard || t.denorm ? "(expected difference)" : "(none seen)")));
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// x87 reference operations (each runs at PC=24). Every function: load floats, operate on the FPU stack, ONE fstp dword.
// The x87 control word is switched to PC=24 only INSIDE the reference helpers: the portable side is SSE2 code, but the 32 bit CRT helpers it may call (uint64 -> double
// conversions, ldexp, ...) use the x87 and would be rounded to 24 bits by a global PC24 (found while building the adversarial inputs of T3).
struct Pc24 {
    unsigned short old;
    Pc24() { unsigned short cw; __asm { fnstcw word ptr [cw] }
             old = cw; cw = (unsigned short)(cw & ~0x300); __asm { fldcw word ptr [cw] } }
    ~Pc24() { unsigned short o = old; __asm { fldcw word ptr [o] } }
};
static __declspec(noinline) float x_add(float a, float b) { Pc24 pc_; float r; __asm { fld dword ptr [a]
                                                                          fadd dword ptr [b]
                                                                          fstp dword ptr [r] } return r; }
static __declspec(noinline) float x_sub(float a, float b) { Pc24 pc_; float r; __asm { fld dword ptr [a]
                                                                          fsub dword ptr [b]
                                                                          fstp dword ptr [r] } return r; }
static __declspec(noinline) float x_mul(float a, float b) { Pc24 pc_; float r; __asm { fld dword ptr [a]
                                                                          fmul dword ptr [b]
                                                                          fstp dword ptr [r] } return r; }
static __declspec(noinline) float x_div(float a, float b) { Pc24 pc_; float r; __asm { fld dword ptr [a]
                                                                          fdiv dword ptr [b]
                                                                          fstp dword ptr [r] } return r; }
static __declspec(noinline) float x_sqrt(float a) { Pc24 pc_; float r; __asm { fld dword ptr [a]
                                                                   fsqrt
                                                                   fstp dword ptr [r] } return r; }
// chains kept on the FPU stack (no spill between the operations)
static __declspec(noinline) float x_mul_div(float a, float b, float c) { Pc24 pc_; float r; __asm { fld dword ptr [a]
                                                                                       fmul dword ptr [b]
                                                                                       fdiv dword ptr [c]
                                                                                       fstp dword ptr [r] } return r; }
static __declspec(noinline) float x_mul_mul(float a, float b, float c) { Pc24 pc_; float r; __asm { fld dword ptr [a]
                                                                                       fmul dword ptr [b]
                                                                                       fmul dword ptr [c]
                                                                                       fstp dword ptr [r] } return r; }
static __declspec(noinline) float x_dot2(float a, float b, float c, float d) { Pc24 pc_; float r; __asm { fld dword ptr [a]
                                                                                              fmul dword ptr [b]
                                                                                              fld dword ptr [c]
                                                                                              fmul dword ptr [d]
                                                                                              faddp st(1), st(0)
                                                                                              fstp dword ptr [r] } return r; }
static __declspec(noinline) float x_len2(float a, float b) { Pc24 pc_; float r; __asm { fld dword ptr [a]
                                                                            fmul st(0), st(0)
                                                                            fld dword ptr [b]
                                                                            fmul st(0), st(0)
                                                                            faddp st(1), st(0)
                                                                            fsqrt
                                                                            fstp dword ptr [r] } return r; }
static __declspec(noinline) float x_diffsq_div(float a, float b, float c) { Pc24 pc_; float r; __asm { fld dword ptr [a]
                                                                                          fsub dword ptr [b]
                                                                                          fld dword ptr [a]
                                                                                          fadd dword ptr [b]
                                                                                          fmulp st(1), st(0)
                                                                                          fdiv dword ptr [c]
                                                                                          fstp dword ptr [r] } return r; }
// a (double) intermediate that is spilled to a double variable
static __declspec(noinline) double x_mul_to_double(float a, float b) { Pc24 pc_; double r; __asm { fld dword ptr [a]
                                                                                      fmul dword ptr [b]
                                                                                      fstp qword ptr [r] } return r; }
static __declspec(noinline) double x_dot2_to_double(float a, float b, float c, float d) { Pc24 pc_; double r; __asm { fld dword ptr [a]
                                                                                                          fmul dword ptr [b]
                                                                                                          fld dword ptr [c]
                                                                                                          fmul dword ptr [d]
                                                                                                          faddp st(1), st(0)
                                                                                                          fstp qword ptr [r] } return r; }
// float * double constant (53 bit load), one rounding at the fmul
static __declspec(noinline) float x_mul_dconst(float a, double c) { Pc24 pc_; float r; __asm { fld dword ptr [a]
                                                                                  fmul qword ptr [c]
                                                                                  fstp dword ptr [r] } return r; }
static __declspec(noinline) float x_add_dconst(float a, double c) { Pc24 pc_; float r; __asm { fld dword ptr [a]
                                                                                  fadd qword ptr [c]
                                                                                  fstp dword ptr [r] } return r; }
// int32 -> fild (exact) -> * float
static __declspec(noinline) float x_imul(int i, float f) { Pc24 pc_; float r; __asm { fild dword ptr [i]
                                                                         fmul dword ptr [f]
                                                                         fstp dword ptr [r] } return r; }
// a*b + c with two separate roundings
static __declspec(noinline) float x_muladd(float a, float b, float c) { Pc24 pc_; float r; __asm { fld dword ptr [a]
                                                                                      fmul dword ptr [b]
                                                                                      fadd dword ptr [c]
                                                                                      fstp dword ptr [r] } return r; }
// _ftol2 sequence (RC = chop, fistp qword, low dword)
static __declspec(noinline) int x_ftol(double x, int* hi) {
    int lo, h;
    __asm {
        sub esp, 12
        fnstcw word ptr [esp]
        movzx eax, word ptr [esp]
        or eax, 0x0C00
        mov word ptr [esp + 2], ax
        fldcw word ptr [esp + 2]
        fld qword ptr [x]
        fistp qword ptr [esp + 4]
        fldcw word ptr [esp]
        mov eax, dword ptr [esp + 4]
        mov edx, dword ptr [esp + 8]
        add esp, 12
        mov lo, eax
        mov h, edx
    }
    if (hi) *hi = h;
    return lo;
}
static __declspec(noinline) unsigned short x_fcom(float a, float b) { unsigned short sw; __asm { fld dword ptr [b]
                                                                                              fld dword ptr [a]
                                                                                              fcompp
                                                                                              fnstsw ax
                                                                                              mov sw, ax } return sw; }

// ---------------------------------------------------------------------------------------------------------------------------------
// Portable versions: plain float (SSE2) and the F24 model
static float f_add(float a, float b) { return a + b; }
static float f_sub(float a, float b) { return a - b; }
static float f_mul(float a, float b) { return a * b; }
static float f_div(float a, float b) { return a / b; }

static void RowBasicOps() {
    struct Op { const char* name; float (*x)(float, float); float (*f)(float, float); F24 (*e)(F24, F24); };
    static const Op ops[] = {
        { "T1 add", x_add, f_add, [](F24 a, F24 b) { return a + b; } },
        { "T1 sub", x_sub, f_sub, [](F24 a, F24 b) { return a - b; } },
        { "T1 mul", x_mul, f_mul, [](F24 a, F24 b) { return a * b; } },
        { "T1 div", x_div, f_div, [](F24 a, F24 b) { return a / b; } },
    };
    for (const Op& op : ops) {
        for (int c = 0; c < C_N; ++c) {
            if (!Wanted(op.name)) continue;
            Tally tf, te;
            for (unsigned i = 0; i < g_cases; ++i) {
                Rng r(0xF00D + i * 7919ull + (uint64_t)c * 104729);
                const float a = Gen(r, (Cls)c), b = Gen(r, (Cls)c);
                const float x = op.x(a, b);
                Cmp(tf, op.f(a, b), x);
                Cmp(te, (float)op.e(a, b), x);
            }
            // regular and mid ranges: plain float must be identical; the wide/special classes differ only through the exponent range / denormals
            Report(op.name, ClsName[c], "float (SSE2) vs x87 PC24", tf, c <= C_MID);
            Report(op.name, ClsName[c], "F24 model vs x87 PC24", te, true);
        }
    }
    if (Wanted("T1 sqrt")) {
        for (int c = 0; c < C_N; ++c) {
            Tally tf, te;
            for (unsigned i = 0; i < g_cases; ++i) {
                Rng r(0xBEEF + i * 7919ull + (uint64_t)c * 104729);
                float a = Gen(r, (Cls)c);
                if (c != C_SPEC) a = std::fabs(a);
                const float x = x_sqrt(a);
                Cmp(tf, std::sqrt(a), x);
                Cmp(te, (float)notsa::fp::sqrt(F24(a)), x);
            }
            Report("T1 sqrt", ClsName[c], "float (SSE2) vs x87 PC24", tf, true);
            Report("T1 sqrt", ClsName[c], "F24 model vs x87 PC24", te, true);
        }
    }
}

template<class T> static T dot2(T a, T b, T c, T d) { return a * b + c * d; }
template<class T> static T len2(T a, T b) { return notsa::fp::sqrt(a * a + b * b); }
template<class T> static T diffsq_div(T a, T b, T c) { return (a - b) * (a + b) / c; }

static void RowChains() {
    for (int c : { (int)C_REG, (int)C_MID, (int)C_WIDE }) {
        Tally tmd, tmm, td2, tl2, tds, emd, emm, ed2, el2, eds;
        if (!Wanted("T2")) break;
        for (unsigned i = 0; i < g_cases; ++i) {
            Rng r(0xC4A1 + i * 7919ull + (uint64_t)c * 104729);
            const float a = Gen(r, (Cls)c), b = Gen(r, (Cls)c), cc = Gen(r, (Cls)c), d = Gen(r, (Cls)c);
            Cmp(tmd, a * b / cc, x_mul_div(a, b, cc));
            Cmp(emd, (float)(F24(a) * F24(b) / F24(cc)), x_mul_div(a, b, cc));
            Cmp(tmm, a * b * cc, x_mul_mul(a, b, cc));
            Cmp(emm, (float)(F24(a) * F24(b) * F24(cc)), x_mul_mul(a, b, cc));
            Cmp(td2, dot2(a, b, cc, d), x_dot2(a, b, cc, d));
            Cmp(ed2, (float)dot2(F24(a), F24(b), F24(cc), F24(d)), x_dot2(a, b, cc, d));
            Cmp(tl2, len2(a, b), x_len2(a, b));
            Cmp(el2, (float)len2(F24(a), F24(b)), x_len2(a, b));
            Cmp(tds, diffsq_div(a, b, cc), x_diffsq_div(a, b, cc));
            Cmp(eds, (float)diffsq_div(F24(a), F24(b), F24(cc)), x_diffsq_div(a, b, cc));
        }
        const bool safe = c <= C_REG; // 2^-20..2^20: no intermediate of these chains leaves the float range
        Report("T2 chain a*b/c", ClsName[c], "float vs x87 (chain on FPU stack)", tmd, safe);
        Report("T2 chain a*b/c", ClsName[c], "F24 vs x87", emd, true);
        Report("T2 chain a*b*c", ClsName[c], "float vs x87", tmm, safe);
        Report("T2 chain a*b*c", ClsName[c], "F24 vs x87", emm, true);
        Report("T2 chain a*b+c*d", ClsName[c], "float vs x87", td2, safe);
        Report("T2 chain a*b+c*d", ClsName[c], "F24 vs x87", ed2, true);
        Report("T2 chain sqrt(a*a+b*b)", ClsName[c], "float vs x87", tl2, safe);
        Report("T2 chain sqrt(a*a+b*b)", ClsName[c], "F24 vs x87", el2, true);
        Report("T2 chain (a-b)*(a+b)/c", ClsName[c], "float vs x87", tds, safe);
        Report("T2 chain (a-b)*(a+b)/c", ClsName[c], "F24 vs x87", eds, true);
    }
}

// T3: float * double constant. Random operands + adversarial ones: a * c lands within half an ulp53 below/above a 24 bit rounding midpoint
static void RowDoubleConst() {
    if (!Wanted("T3")) return;
    Tally naive, exact, naiveAdd, exactAdd;
    unsigned adversarialFails = 0, adversarial = 0, adversarialDiffer = 0;
    static const double consts[] = { 0.01, 0.017453292519943295, 3.14159265358979323846, 1.0 / 3.0, 0.1, 57.29577951308232, 0.0174532925, 2.5, 0.5, 1e-3, 1.0e-2 };
    for (unsigned i = 0; i < g_cases; ++i) {
        Rng r(0xD00B1E + i * 7919ull);
        const float a = GenE(r, -20, 20);
        double c;
        const int mode = r.below(3);
        if (mode == 0) c = consts[r.below((int)(sizeof consts / sizeof consts[0]))];
        else if (mode == 1) c = std::bit_cast<double>((r.u64() & 0x800FFFFFFFFFFFFFull) | ((uint64_t)(1023 - 10 + r.below(21)) << 52));
        else { // adversarial: product ~ a 24 bit midpoint m (25 bit significand, odd): c = m / a rounded to double, +-1 ulp
            const double m = (double)((uint64_t)(r.u32() & 0xFFFFFF) | 0x1000001ull) * std::ldexp(1.0, -24 - 10 + r.below(5)); // 25 bit significand, odd = midpoint of the 24 bit grid
            c = m / (double)a;
            const int k = r.below(4);                  // the exact quotient rounded to double (product within half an ulp53 of the midpoint) or one ulp off
            if (k == 1) c = std::nextafter(c, 1e300); else if (k == 2) c = std::nextafter(c, -1e300);
        }
        const float x = x_mul_dconst(a, c);
        const float nv = (float)((double)a * c);
        const float ex = (float)notsa::fp::MulD(F24(a), c);
        Cmp(naive, nv, x);
        Cmp(exact, ex, x);
        if (mode == 2) { ++adversarial; if (FBits(nv) != FBits(ex)) ++adversarialDiffer; if (FBits(nv) != FBits(x)) ++adversarialFails; }
        const float xa = x_add_dconst(a, c);
        Cmp(naiveAdd, (float)((double)a + c), xa);
        Cmp(exactAdd, (float)notsa::fp::AddD(F24(a), c), xa);
    }
    Report("T3 float*double-constant", "random + adversarial", "(float)((double)a*c) vs x87", naive, false);
    Report("T3 float*double-constant", "random + adversarial", "MulD (single rounding) vs x87", exact, true);
    Report("T3 float+double-constant", "random + adversarial", "(float)((double)a+c) vs x87", naiveAdd, false);
    Report("T3 float+double-constant", "random + adversarial", "AddD vs x87", exactAdd, true);
    std::printf("    (%u adversarial inputs: double-rounded and exactly rounded results differ in %u, the x87 sides with the double-rounded form in %u of them)\n", adversarial, adversarialDiffer, adversarialDiffer - (adversarialDiffer ? adversarialFails : 0));
}

// T4: int32 -> float conversion inside an expression
static void RowIntMul() {
    if (!Wanted("T4")) return;
    Tally naive, viaD, exact, small;
    for (unsigned i = 0; i < g_cases; ++i) {
        Rng r(0x1A7 + i * 7919ull);
        const int bits = 1 + r.below(31);
        int iv = (int)(r.u32() >> (32 - bits));
        if (r.below(2)) iv = -iv;
        const float f = GenE(r, -20, 12);
        const float x = x_imul(iv, f);
        Cmp(naive, (float)iv * f, x);
        Cmp(viaD, (float)((double)iv * f), x);
        Cmp(exact, (float)notsa::fp::Round24Mul((double)iv, f), x);
        if ((iv < 0 ? -iv : iv) <= (1 << 24)) Cmp(small, (float)iv * f, x);
    }
    Report("T4 int*float", "|i| < 2^31", "(float)i * f vs fild+fmul", naive, false);
    Report("T4 int*float", "|i| <= 2^24 only", "(float)i * f vs fild+fmul", small, true);
    Report("T4 int*float", "|i| < 2^31", "(float)((double)i * f) vs x87", viaD, false);
    Report("T4 int*float", "|i| < 2^31", "Round24Mul(i, f) vs x87", exact, true);
}

// T5: float -> int conversion
static void RowFtol() {
    if (!Wanted("T5")) return;
    Tally t1, t2;
    unsigned plainDiff = 0, n = 0;
    std::vector<double> edge = { 0.0, -0.0, 0.5, -0.5, 0.999999, -0.999999, 1.0, -1.0, 2147483647.0, 2147483648.0, -2147483648.0, -2147483649.0, 4294967295.0, 4294967296.0, 4294967297.0, 9223372036854775807.0,
                                 9223372036854775808.0, -9223372036854775808.0, -9223372036854777856.0, 1e19, -1e19, 1e30, HUGE_VAL, -HUGE_VAL, std::nan(""), -std::nan(""), 3e9, -3e9, 123456789012.5 };
    for (unsigned i = 0; i < g_cases + edge.size(); ++i) {
        Rng r(0xF70 + i * 7919ull);
        double x;
        if (i < edge.size()) x = edge[i];
        else switch (r.below(4)) {
            case 0: x = (double)GenE(r, -3, 31); break;
            case 1: x = (double)GenE(r, 31, 66); break;
            case 2: x = (double)GenSpecial(r); break;
            default: x = std::bit_cast<double>(r.u64()); break;
        }
        int hi; const int lo = x_ftol(x, &hi);
        ++t1.n; if (lo != notsa::fp::Ftol(x)) ++t1.hard;
        ++t2.n; if (((int64_t)hi << 32 | (uint32_t)lo) != notsa::fp::Ftol64(x)) ++t2.hard;
        ++n;
        // what a plain C++ cast does on this host (SSE2 cvttsd2si: 0x80000000 for NaN/out of range)
        volatile double xv = x; const int plain = std::fabs(xv) < 2147483648.0 ? (int)xv : (int)0x80000000;
        if (plain != lo) ++plainDiff;
    }
    Report("T5 _ftol2 low dword", "random + edge", "fp::Ftol vs fistp-trunc", t1, true);
    Report("T5 _ftol2 64 bit", "random + edge", "fp::Ftol64 vs fistp-trunc", t2, true);
    std::printf("    (a plain C++ (int) cast disagrees with the exe's _ftol2 in %u of %u cases: NaN/inf/out of range)\n", plainDiff, n);
}

// T6: compare semantics
static void RowCompare() {
    if (!Wanted("T6")) return;
    Tally t;
    unsigned bad = 0, n = 0;
    for (unsigned i = 0; i < g_cases / 4; ++i) {
        Rng r(0xC0A + i * 7919ull);
        const float a = r.below(2) ? GenSpecial(r) : GenE(r, -3, 3), b = r.below(4) ? (r.below(2) ? a : GenSpecial(r)) : GenE(r, -3, 3);
        const unsigned short sw = x_fcom(a, b) & 0x4500; // C3 C2 C0
        const bool lt = sw == 0x0100, gt = sw == 0x0000, eq = sw == 0x4000;
        const bool ok = (a < b) == lt && (a > b) == gt && (a == b) == eq && (a <= b) == (lt || eq) && (a >= b) == (gt || eq) && (a != b) == !eq && !(a < b && a > b);
        ++n; bad += !ok;
    }
    t.n = n; t.hard = bad;
    Report("T6 compare fcom status vs C++ operators", "random + specials", "<,>,==,<=,>=,!= incl. NaN, +-0, inf", t, true);
}

// T7: fused multiply-add must not be used
static void RowFma() {
    if (!Wanted("T7")) return;
    Tally sep, fused;
    for (unsigned i = 0; i < g_cases; ++i) {
        Rng r(0xFA + i * 7919ull);
        const float a = GenE(r, -10, 10), b = GenE(r, -10, 10), c = GenE(r, -10, 10);
        const float x = x_muladd(a, b, c);
        volatile float p = a * b;                    // two roundings (contraction forbidden)
        Cmp(sep, p + c, x);
        Cmp(fused, std::fmaf(a, b, c), x);
    }
    Report("T7 a*b+c", "regular", "two roundings (fp-contract=off) vs x87", sep, true);
    Report("T7 a*b+c", "regular", "std::fmaf (clang -ffp-contract=on) vs x87", fused, false);
}

// T8: the port's `double` intermediates ARE float precision at PC24
static void RowDoubleIntermediates() {
    if (!Wanted("T8")) return;
    Tally sseD, asF, asF24d, dotSseD, dotF, dotF24;
    for (unsigned i = 0; i < g_cases; ++i) {
        Rng r(0xD0B + i * 7919ull);
        const float a = GenE(r, -20, 20), b = GenE(r, -20, 20), c = GenE(r, -20, 20), d = GenE(r, -20, 20);
        const double x = x_mul_to_double(a, b);       // `double t = (double)a * b;` compiled for x87 at PC24 and spilled
        auto cmpD = [&](Tally& t, double got, double ref) { ++t.n; if (std::bit_cast<uint64_t>(got) != std::bit_cast<uint64_t>(ref)) ++t.hard; };
        cmpD(sseD, (double)a * b, x);                 // the same source compiled for SSE2 (53 bit product)
        cmpD(asF, (double)(a * b), x);                // float rewrite
        cmpD(asF24d, F24(a).v * F24(b).v == 0 ? 0.0 : (double)(F24(a) * F24(b)), x);
        const double y = x_dot2_to_double(a, b, c, d); // `double t = (double)a*b + (double)c*d;`
        cmpD(dotSseD, (double)a * b + (double)c * d, y);
        cmpD(dotF, (double)dot2(a, b, c, d), y);
        cmpD(dotF24, (double)dot2(F24(a), F24(b), F24(c), F24(d)), y);
    }
    Report("T8 double t = (double)a*b", "regular", "SSE2 double arithmetic vs x87 PC24", sseD, false);
    Report("T8 double t = (double)a*b", "regular", "(double)(float)(a*b) vs x87 PC24", asF, true);
    Report("T8 double t = (double)a*b", "regular", "F24 vs x87 PC24", asF24d, true);
    Report("T8 double t = a*b + c*d", "regular", "SSE2 double arithmetic vs x87 PC24", dotSseD, false);
    Report("T8 double t = a*b + c*d", "regular", "float rewrite vs x87 PC24", dotF, true);
    Report("T8 double t = a*b + c*d", "regular", "F24 vs x87 PC24", dotF24, true);
}

// T10: results in the float DENORMAL range: the x87 rounds to 24 bits first (extended exponent) and again when storing to the float -> double rounding that plain float does not have
static void RowDenormalResults() {
    if (!Wanted("T10")) return;
    Tally tf, te;
    for (unsigned i = 0; i < g_cases; ++i) {
        Rng r(0xDE9 + i * 7919ull);
        const int ea = -75 + r.below(40);                   // product exponent -150..-70 -> lands in/near the float denormal range
        const int eb = -75 + r.below(40);
        const float a = GenE(r, ea, ea), b = GenE(r, eb, eb);
        Cmp(tf, a * b, x_mul(a, b));
        Cmp(te, (float)(F24(a) * F24(b)), x_mul(a, b));
    }
    Report("T10 mul with float-denormal results", "exp(a)+exp(b) in -150..-70", "float (SSE2) vs x87 PC24", tf, false);
    Report("T10 mul with float-denormal results", "exp(a)+exp(b) in -150..-70", "F24 model vs x87 PC24", te, true);
}

// T9: fast-path guard: inputs accepted by InSafeRange<16> through chains of depth <= 3 never leave the float range (|exponent| <= 48 + a few), so plain float == x87
static void RowGuard() {
    if (!Wanted("T9")) return;
    Tally t;
    unsigned inRange = 0;
    for (unsigned i = 0; i < g_cases; ++i) {
        Rng r(0x6A7D + i * 7919ull);
        const float a = GenE(r, -20, 20), b = GenE(r, -20, 20), c = GenE(r, -20, 20), d = GenE(r, -20, 20);
        if (!(notsa::fp::InSafeRange<16>(a) && notsa::fp::InSafeRange<16>(b) && notsa::fp::InSafeRange<16>(c) && notsa::fp::InSafeRange<16>(d))) continue;
        ++inRange;
        Cmp(t, dot2(a, b, c, d), x_dot2(a, b, c, d));
        Cmp(t, a * b * c, x_mul_mul(a, b, c));
        Cmp(t, a * b / c, x_mul_div(a, b, c));
        Cmp(t, len2(a, b), x_len2(a, b));
        Cmp(t, diffsq_div(a, b, c), x_diffsq_div(a, b, c));
    }
    Report("T9 guard InSafeRange<16>, chains of depth <= 3", "2^-16..2^16 inputs", "float vs x87 (guarded fast path)", t, true);
    std::printf("    (%u of %u cases passed the guard)\n", inRange, g_cases);
}

// T13: the production pattern (RangeGuard + float fast path + F24 fallback) on the same chains: exact in EVERY class, including the ones where plain float differs (T2).
// Also a regression test for the guard itself: the flag reads must not be CSE'd / reordered by the optimizer (compile with /O2).
// NOTE: the chains return through a pointer, like the production Impl functions (bool / out-struct). A float-RETURNING function in an /arch:SSE2 TU may be compiled by MSVC
// to x87 code (the result has to travel in ST0 anyway) and would then be evaluated at the process precision control (PC53 in this test): found while writing this row.
template<class T> NOTSA_FP_NOINLINE static void chain_muldiv(float a, float b, float c, T* o) { *o = T(a) * T(b) / T(c); }
template<class T> NOTSA_FP_NOINLINE static void chain_dot2(float a, float b, float c, float d, T* o) { *o = T(a) * T(b) + T(c) * T(d); }
template<class T> NOTSA_FP_NOINLINE static void chain_len2(float a, float b, T* o) { *o = notsa::fp::sqrt(T(a) * T(a) + T(b) * T(b)); }
template<class T> NOTSA_FP_NOINLINE static void chain_diffsq(float a, float b, float c, T* o) { *o = (T(a) - T(b)) * (T(a) + T(b)) / T(c); }
#define GUARDED(OUT, FAST, SLOW) { notsa::fp::RangeGuard g; float f = 0.f; FAST; if (g.Clean(&f)) { OUT = f; } else { ++slow; F24 e; SLOW; OUT = (float)e; } }
static void RowGuarded() {
    if (!Wanted("T13")) return;
    for (int c : { (int)C_REG, (int)C_MID, (int)C_WIDE, (int)C_SPEC }) {
        Tally t1, t2, t3, t4;
        unsigned slow = 0;
        for (unsigned i = 0; i < g_cases; ++i) {
            Rng r(0x9A4D + i * 7919ull + (uint64_t)c * 104729);
            const float a = Gen(r, (Cls)c), b = Gen(r, (Cls)c), cc = Gen(r, (Cls)c), d = Gen(r, (Cls)c);
            float o1, o2, o3, o4;
            GUARDED(o1, chain_muldiv<float>(a, b, cc, &f), chain_muldiv<F24>(a, b, cc, &e));
            GUARDED(o2, chain_dot2<float>(a, b, cc, d, &f), chain_dot2<F24>(a, b, cc, d, &e));
            GUARDED(o3, chain_len2<float>(a, b, &f), chain_len2<F24>(a, b, &e));
            GUARDED(o4, chain_diffsq<float>(a, b, cc, &f), chain_diffsq<F24>(a, b, cc, &e));
            Cmp(t1, o1, x_mul_div(a, b, cc));
            Cmp(t2, o2, x_dot2(a, b, cc, d));
            Cmp(t3, o3, x_len2(a, b));
            Cmp(t4, o4, x_diffsq_div(a, b, cc));
        }
        Report("T13 guarded a*b/c", ClsName[c], "RangeGuard + float, F24 fallback vs x87", t1, true);
        Report("T13 guarded a*b+c*d", ClsName[c], "RangeGuard + float, F24 fallback vs x87", t2, true);
        Report("T13 guarded sqrt(a*a+b*b)", ClsName[c], "RangeGuard + float, F24 fallback vs x87", t3, true);
        Report("T13 guarded (a-b)*(a+b)/c", ClsName[c], "RangeGuard + float, F24 fallback vs x87", t4, true);
        std::printf("    (slow path taken in %u of %u chain evaluations)\n", slow, 4 * g_cases);
    }
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = (unsigned)std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    std::printf("fp_theory_test: %u cases per row/class; x87 reference at PC24 (game mode); portable side = SSE2 float/double + notsa::fp\n", g_cases);
    RowBasicOps();
    RowChains();
    RowDoubleConst();
    RowIntMul();
    RowFtol();
    RowCompare();
    RowFma();
    RowDoubleIntermediates();
    RowGuard();
    RowDenormalResults();
    RowGuarded();
    std::printf("\nfp_theory_test: %d unexpected mismatches\n", g_unexpected);
    return g_unexpected ? 1 : 0;
}
