// primitives: (1) INFO: inline x87 instruction vs the UCRT, (2) the exe's CRT entry points vs the UCRT (counted)
static double GenD(Rng& r) {
    switch (r.below(6)) {
    case 0: return (double)GenF(r, 7.0f);                                  // radians
    case 1: return (double)GenF(r, 400.0f);
    case 2: return (double)GenF(r, 1e5f);
    case 3: return (double)GenF(r, 1e30f);                                 // beyond 2^63: fsin leaves it unchanged
    case 4: return (double)GenF(r, 3.0f) * (double)GenF(r, 3.0f);          // x87 product kept unrounded by the exe (here: double)
    default: return (double)GenF(r, PickScale(r));
    }
}
// raw call helpers for the exe's CRT (the _CI* entries take their arguments on the x87 stack and return in ST0)
static double ExePowCI(double x, double y) { double res; __asm { fld qword ptr [x]
                                                                  fld qword ptr [y]
                                                                  mov eax, 0x822130
                                                                  call eax
                                                                  fstp qword ptr [res] } return res; }
static double ExeAsinCI(double x) { double res; __asm { fld qword ptr [x]
                                                         mov eax, 0x821E70
                                                         call eax
                                                         fstp qword ptr [res] } return res; }
static double ExeAcosCI(double x) { double res; __asm { fld qword ptr [x]
                                                         mov eax, 0x822380
                                                         call eax
                                                         fstp qword ptr [res] } return res; }
static double ExeExpCI(double x) { double res; __asm { fld qword ptr [x]
                                                        mov eax, 0x825E4C
                                                        call eax
                                                        fstp qword ptr [res] } return res; }

static void TestPrimitives() {
    // --- INFO: x87 instruction vs std:: (what the ports did before) ---
    Run("INFO fsin vs std::sin (double bits)", [](Rng& r, std::string& d) { const double x = GenD(r); const double a = std::sin(x), b = x87::sin(x); if (SameD(a, b)) return true; d = "x " + D(x) + " std " + D(a) + " x87 " + D(b); return false; });
    Run("INFO fsin vs std::sin (float result)", [](Rng& r, std::string& d) { const double x = GenD(r); const float a = (float)std::sin(x), b = (float)x87::sin(x); if (SameF(a, b)) return true; d = "x " + D(x) + " std " + F(a) + " x87 " + F(b); return false; });
    Run("INFO fcos vs std::cos (double bits)", [](Rng& r, std::string& d) { const double x = GenD(r); const double a = std::cos(x), b = x87::cos(x); if (SameD(a, b)) return true; d = "x " + D(x) + " std " + D(a) + " x87 " + D(b); return false; });
    Run("INFO fcos vs std::cos (float result)", [](Rng& r, std::string& d) { const double x = GenD(r); const float a = (float)std::cos(x), b = (float)x87::cos(x); if (SameF(a, b)) return true; d = "x " + D(x) + " std " + F(a) + " x87 " + F(b); return false; });
    Run("INFO fpatan vs std::atan2 (double bits)", [](Rng& r, std::string& d) { const double y = GenD(r), x = GenD(r); const double a = std::atan2(y, x), b = x87::atan2(y, x); if (SameD(a, b)) return true; d = "y " + D(y) + " x " + D(x) + " std " + D(a) + " x87 " + D(b); return false; });
    Run("INFO fpatan vs std::atan2 (float result)", [](Rng& r, std::string& d) { const double y = GenD(r), x = GenD(r); const float a = (float)std::atan2(y, x), b = (float)x87::atan2(y, x); if (SameF(a, b)) return true; d = "y " + D(y) + " x " + D(x) + " std " + F(a) + " x87 " + F(b); return false; });
    Run("INFO fptan vs std::tan (double bits)", [](Rng& r, std::string& d) { const double x = GenD(r); const double a = std::tan(x), b = x87::tan(x); if (SameD(a, b)) return true; d = "x " + D(x) + " std " + D(a) + " x87 " + D(b); return false; });
    Run("INFO fsqrt vs std::sqrt (double bits)", [](Rng& r, std::string& d) { const double x = std::fabs(GenD(r)); const double a = std::sqrt(x), b = x87::sqrt(x); if (SameD(a, b)) return true; d = "x " + D(x) + " std " + D(a) + " x87 " + D(b); return false; });
    Run("INFO fyl2x log10 vs std::log10 (double bits)", [](Rng& r, std::string& d) { const double x = std::fabs(GenD(r)); const double a = std::log10(x), b = x87::log10(x); if (SameD(a, b)) return true; d = "x " + D(x) + " std " + D(a) + " x87 " + D(b); return false; });

    // --- the CRT entry points the exe calls (counted): exe machine code vs the UCRT ---
    Run("CRT _CIpow 0x822130 vs std::pow", [](Rng& r, std::string& d) {
        const double x = std::fabs(GenD(r)), y = r.below(3) == 0 ? (double)(r.below(9) - 4) : (r.below(2) ? (double)GenF(r, 6.0f) : 0.5);
        const double a = std::pow(x, y), b = ExePowCI(x, y);
        if (SameD(a, b)) return true; d = "x " + D(x) + " y " + D(y) + " std " + D(a) + " exe " + D(b); return false; });
    // x87::pow = the exe's SSE2 pow kernel (X87Pow.h): exact vs the exe, random + ts-like + special values
    Run("CRT _CIpow 0x822130 vs x87::pow (random)", [](Rng& r, std::string& d) {
        const double x = std::fabs(GenD(r)), y = r.below(3) == 0 ? (double)(r.below(9) - 4) : (r.below(2) ? (double)GenF(r, 6.0f) : 0.5);
        const double a = x87::pow(x, y), b = ExePowCI(x, y);
        if (SameD(a, b)) return true; d = "x " + D(x) + " y " + D(y) + " x87 " + D(a) + " exe " + D(b); return false; });
    Run("CRT _CIpow 0x822130 vs x87::pow (ts-like: base 0.8..0.99 or 0.5..2, exponent 0..3 / frame times)", [](Rng& r, std::string& d) {
        const double x = r.below(2) ? 0.8 + 0.19 * (double)GenF(r, 1.0f) * (double)GenF(r, 1.0f) : 0.5 + 1.5 * std::fabs((double)GenF(r, 1.0f));
        const double y = r.below(3) == 0 ? (double)(float)(r.below(300) * 0.01) : r.below(2) ? (double)GenF(r, 3.0f) : (double)(r.below(4));
        const double xf = (double)(float)x, yf = (double)(float)std::fabs(y);
        const double a = x87::pow(xf, yf), b = ExePowCI(xf, yf);
        if (SameD(a, b)) return true; d = "x " + D(xf) + " y " + D(yf) + " x87 " + D(a) + " exe " + D(b); return false; });
    Run("CRT _CIpow 0x822130 vs x87::pow (wide: signs, ints, huge, tiny, denormal)", [](Rng& r, std::string& d) {
        auto pick = [&r]() -> double {
            switch (r.below(8)) {
            case 0: return (double)GenF(r, 1e30f);
            case 1: return (double)GenF(r, 1e-30f) * (double)GenF(r, 1e-30f);
            case 2: return (double)((int)r.below(41) - 20);
            case 3: return (double)((int)r.below(41) - 20) * 0.5;
            case 4: return DBits(((uint64_t)r.below(1u << 20) << 44) | ((uint64_t)r.below(0xFFFFFFFFu) << 12) | r.below(4096));   // random bit pattern (denormals, huge, nan)
            case 5: return std::ldexp((double)GenF(r, 1.0f), (int)r.below(2200) - 1100);
            case 6: return (double)GenF(r, 1e5f);
            default: return (double)GenF(r, PickScale(r));
            }
        };
        const double x = pick(), y = r.below(3) == 0 ? (double)((int)r.below(2001) - 1000) * 0.25 : pick();
        const double a = x87::pow(x, y), b = ExePowCI(x, y);
        if (SameD(a, b)) return true; d = "x " + D(x) + " y " + D(y) + " x87 " + D(a) + " exe " + D(b); return false; });
    Run("CRT _CIpow 0x822130 vs x87::pow (special value grid)", [](Rng& r, std::string& d) {
        static const uint64_t sp[] = { 0x0000000000000000ull, 0x8000000000000000ull, 0x3FF0000000000000ull, 0xBFF0000000000000ull, 0x7FF0000000000000ull, 0xFFF0000000000000ull, 0x7FF8000000000000ull, 0xFFF8000000000000ull,
            0x7FF0000000000001ull, 0x0000000000000001ull, 0x8000000000000001ull, 0x000FFFFFFFFFFFFFull, 0x0010000000000000ull, 0x7FEFFFFFFFFFFFFFull, 0xFFEFFFFFFFFFFFFFull, 0x4000000000000000ull, 0xC000000000000000ull,
            0x3FE0000000000000ull, 0xBFE0000000000000ull, 0x4008000000000000ull, 0xC008000000000000ull, 0x4059000000000000ull, 0x3FF8000000000000ull, 0x4330000000000000ull, 0x4340000000000000ull, 0xC330000000000000ull,
            0x43E0000000000000ull, 0x4090000000000000ull, 0x40A0000000000000ull, 0xC090000000000000ull, 0xC0A0000000000000ull, 0x3F50624DD2F1A9FCull, 0x4024000000000000ull };
        const uint32_t n = (uint32_t)(sizeof sp / sizeof sp[0]);
        const double x = DBits(sp[r.below(n)]), y = DBits(sp[r.below(n)]);
        const double a = x87::pow(x, y), b = ExePowCI(x, y);
        if (SameD(a, b)) return true; d = "x " + D(x) + " y " + D(y) + " x87 " + D(a) + " exe " + D(b); return false; });
    Run("INFO std::pow vs the exe on ts-like operands (UCRT of this host)", [](Rng& r, std::string& d) {
        const double xf = (double)(float)(0.8 + 0.19 * (double)GenF(r, 1.0f) * (double)GenF(r, 1.0f)), yf = (double)(float)(r.below(300) * 0.01);
        const double a = std::pow(xf, yf), b = ExePowCI(xf, yf);
        if (SameD(a, b)) return true; d = "x " + D(xf) + " y " + D(yf) + " std " + D(a) + " exe " + D(b); return false; });
    Run("CRT pow 0x8220F0 vs std::pow", [](Rng& r, std::string& d) {
        const double x = GenD(r), y = r.below(3) == 0 ? (double)(r.below(9) - 4) : (r.below(2) ? (double)GenF(r, 6.0f) : 2.0);
        const double a = std::pow(x, y), b = oracle::Fn<double __cdecl(double, double)>(0x8220F0)(x, y);
        if (SameD(a, b)) return true; d = "x " + D(x) + " y " + D(y) + " std " + D(a) + " exe " + D(b); return false; });
    for (int which = 0; which < 2; ++which) {   // 0 = asin, 1 = acos
        const char* nm = which ? "acos 0x822380" : "asin 0x821E70";
        Run(std::string("INFO CRT _CI") + nm + " vs UCRT std::", [which](Rng& r, std::string& d) {
            const double x = r.below(4) == 0 ? GenD(r) : (double)GenF(r, 1.0f);
            const double a = which ? std::acos(x) : std::asin(x), b = which ? ExeAcosCI(x) : ExeAsinCI(x);
            if (SameD(a, b)) return true; d = "x " + D(x) + " std " + D(a) + " exe " + D(b); return false; });
        Run(std::string("CRT _CI") + nm + " vs x87::", [which](Rng& r, std::string& d) {
            const double x = r.below(4) == 0 ? GenD(r) : (double)GenF(r, 1.0f);
            const double a = which ? x87::acos(x) : x87::asin(x), b = which ? ExeAcosCI(x) : ExeAsinCI(x);
            if (SameD(a, b)) return true; d = "x " + D(x) + " x87 " + D(a) + " exe " + D(b); return false; });
    }
    Run("CRT _CIexp 0x825E4C vs std::exp", [](Rng& r, std::string& d) {
        const double x = GenD(r);
        const double a = std::exp(x), b = ExeExpCI(x);
        if (SameD(a, b)) return true; d = "x " + D(x) + " std " + D(a) + " exe " + D(b); return false; });
    Run("CRT floor 0x8219F0 vs std::floor", [](Rng& r, std::string& d) {
        const double x = GenD(r);
        const double a = std::floor(x), b = oracle::Fn<double __cdecl(double)>(0x8219F0)(x);
        if (SameD(a, b)) return true; d = "x " + D(x) + " std " + D(a) + " exe " + D(b); return false; });
    Run("CRT ceil 0x823820 vs std::ceil", [](Rng& r, std::string& d) {
        const double x = GenD(r);
        const double a = std::ceil(x), b = oracle::Fn<double __cdecl(double)>(0x823820)(x);
        if (SameD(a, b)) return true; d = "x " + D(x) + " std " + D(a) + " exe " + D(b); return false; });
    Run("CRT modf 0x8232F0 vs std::modf", [](Rng& r, std::string& d) {
        const double x = GenD(r); double ia, ib;
        const double a = std::modf(x, &ia), b = oracle::Fn<double __cdecl(double, double*)>(0x8232F0)(x, &ib);
        if (SameD(a, b) && SameD(ia, ib)) return true; d = "x " + D(x) + " std " + D(a) + "/" + D(ia) + " exe " + D(b) + "/" + D(ib); return false; });
}
