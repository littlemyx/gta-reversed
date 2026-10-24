// MAC-FTOL1: notsa::fp::FtolCast<T> (= NOTSA_FTOL on the portable build) against an independent model of the exe's conversion: `fistp qword` with truncation (_ftol2), the LOW dword
// (or its low 8 / 16 bits) is the result; NaN, +-inf and |x| >= 2^63 give the integer indefinite 0x8000000000000000 (low dword 0).
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>
#include "../../source/game_sa/Core/Fp.h"

static int g_bad = 0, g_n = 0;
// reference: exact integer truncation with big-number free math (long double has a 64 bit mantissa on x86 but not arm64: use fmod on doubles)
static uint64_t Ref64(double x) {
    if (std::isnan(x) || std::isinf(x) || std::fabs(x) >= 9223372036854775808.0) return 0x8000000000000000ull;
    return (uint64_t)(int64_t)std::trunc(x);
}
template<class T> static void Check(const char* name, double x, T got, uint64_t want64) {
    ++g_n;
    const T want = (T)want64;
    if (got != want) { ++g_bad; if (g_bad < 40) std::printf("FAIL %s(%.17g): got %lld want %lld\n", name, x, (long long)got, (long long)want); }
}
int main() {
    std::vector<double> xs = {0.0, -0.0, 0.5, -0.5, 1.5, -1.5, 254.9, 255.0, 255.9, 256.0, 65535.5, 65536.0, -1.0, -129.5, 2147483647.0, 2147483647.9, 2147483648.0, 2147483649.0, -2147483648.0,
                              -2147483649.0, 4294967295.0, 4294967296.0, 4294967297.5, -4294967296.0, 1e10, -1e10, 1.8e19, 3.4e38, -3.4e38, 9.2233720368547748e18, 9.2233720368547758e18,
                              -9.2233720368547758e18, 1.8446744073709552e19, 1e30, 1e300, std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::nan("")};
    uint64_t s = 88172645463325252ull;
    auto rnd = [&]() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; };
    for (int i = 0; i < 200000; i++) {
        const uint64_t r = rnd();
        switch (i % 4) {
        case 0: xs.push_back((double)(int64_t)r / 1024.0); break;                                   // up to ~2^53 with fractions
        case 1: xs.push_back(std::ldexp((double)(r >> 11) / 9007199254740992.0 * 2 - 1, (int)(r % 80) - 10)); break;
        case 2: xs.push_back((double)(float)((double)(int32_t)r * (double)(r >> 40))); break;       // floats up to 2^63 and beyond
        default: xs.push_back((double)std::bit_cast<float>((uint32_t)r)); break;                    // any float bit pattern incl. NaN / inf
        }
    }
    for (double x : xs) {
        const uint64_t w = Ref64(x);
        Check<int32_t>("int32", x, notsa::fp::FtolCast<int32_t>(x), w);
        Check<uint32_t>("uint32", x, notsa::fp::FtolCast<uint32_t>(x), w);
        Check<int16_t>("int16", x, notsa::fp::FtolCast<int16_t>(x), w);
        Check<uint16_t>("uint16", x, notsa::fp::FtolCast<uint16_t>(x), w);
        Check<int8_t>("int8", x, notsa::fp::FtolCast<int8_t>(x), w);
        Check<uint8_t>("uint8", x, notsa::fp::FtolCast<uint8_t>(x), w);
        Check<int64_t>("int64", x, notsa::fp::FtolCast<int64_t>(x), w);
        Check<size_t>("size_t", x, notsa::fp::FtolCast<size_t>(x), (uint64_t)(int64_t)(int32_t)w);   // 32 bit size_t in the exe: low dword, sign-extended (wraps mod 2^64 like the 32 bit one mod 2^32)
        const float f = (float)x;
        if (std::isfinite(f) || std::isnan(f)) Check<int32_t>("int32(float)", f, notsa::fp::FtolCast<int32_t>(f), Ref64((double)f));
    }
    // R-11: the rain hang: `for (i = 1 - NOTSA_FTOL(size_t, Rain * -2.f); i > 0; i--)` must run 1 + 2 * Rain times (the 32 bit exe wraps mod 2^32, a 64 bit size_t must wrap mod 2^64)
    Check<size_t>("1 - size_t(-4.0) == 5", -4.0, (size_t)1 - notsa::fp::FtolCast<size_t>(-4.0), 5);
    Check<size_t>("1 - size_t(-0.0) == 1", 0, (size_t)1 - notsa::fp::FtolCast<size_t>(-0.0), 1);
    // the case that crashed the load (MAC-ANIM1): floor(1e19 * 0.02f + 60) must not be INT_MIN..INT_MAX saturation: low dword of the exact integer
    Check<int32_t>("sector1e19", 0, notsa::fp::FtolCast<int32_t>(std::floor(1e19f * 0.02f + 60.0f)), Ref64(std::floor(1e19f * 0.02f + 60.0f)));
    std::printf("FTOLCAST %s (%d checks, %d bad)\n", g_bad ? "FAILED" : "OK", g_n, g_bad);
    return g_bad ? 1 : 0;
}
