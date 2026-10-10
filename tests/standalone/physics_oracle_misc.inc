// (e) CGeneral / CTimer / CClock / CWeather / CTimeCycle leftovers
struct Snap {   // snapshot / restore of a range of exe globals (both implementations work on the same statics)
    uintptr_t va; std::vector<uint8> b;
    Snap(uintptr_t v, size_t n) : va(v), b(n) {}
    void Save()          { std::memcpy(b.data(), (void*)va, b.size()); }
    void Restore() const { std::memcpy((void*)va, b.data(), b.size()); }
    bool SameAs(const Snap& o) const { return SameBlob(b.data(), o.b.data(), b.size()); }
    void FillRandom(Rng& r) { for (size_t i = 0; i + 4 <= b.size(); i += 4) { const uint32_t v = r.u32() * 2654435761u; std::memcpy(&b[i], &v, 4); } }
};

static double ExePow(double x, double y) {   // the exe's _CIpow (0x822130): x in ST1, y in ST0
    double r;
    __asm {
        fld x
        fld y
        mov eax, 0x822130
        call eax
        fstp r
    }
    return r;
}
static void TestMisc() {
    Run("pow(double,double) host CRT vs exe _CIpow 0x822130", [](Rng& r, std::string& d) {
        const double x = r.below(4) ? (double)r.f01() * 1.2 : (double)GenF(r, 50.f), y = r.below(3) ? (double)r.f01() * 4.0 : (double)GenF(r, 20.f);
        const double a = std::pow(x, y), b = ExePow(x, y);
        uint64_t ba, bb; std::memcpy(&ba, &a, 8); std::memcpy(&bb, &b, 8);
        if (ba == bb || (std::isnan(a) && std::isnan(b))) return true;
        char t[160]; std::snprintf(t, sizeof t, "x %.17g y %.17g got %.17g (%016llX) exe %.17g (%016llX)", x, y, a, (unsigned long long)ba, b, (unsigned long long)bb); d = t;
        g_hardDiff = true;
        return false;
    });
    Run("CGeneral::SolveQuadratic 0x53CE30", [](Rng& r, std::string& d) {
        const float s = PickScale(r), a = GenF(r, s), b = GenF(r, s), c = GenF(r, s);
        float x1a = 7, x2a = 8, x1e = 7, x2e = 8;
        const bool ra = CGeneral::SolveQuadratic(a, b, c, x1a, x2a);
        const bool re = oracle::Fn<bool __cdecl(float, float, float, float*, float*)>(0x53CE30)(a, b, c, &x1e, &x2e);
        Hit(ra);
        if (ra == re && SameF(x1a, x1e) && SameF(x2a, x2e)) return true;
        d = "abc " + F(a) + "," + F(b) + "," + F(c) + " got " + std::to_string(ra) + F(x1a) + "," + F(x2a) + " exe " + std::to_string(re) + F(x1e) + "," + F(x2e);
        return false;
    });
    // CTimer::UpdateVariables: all CTimer statics (0xB7CB00..0xB7CC00) are randomised, then both versions run from the same state
    Run("CTimer::UpdateVariables 0x5618D0", [](Rng& r, std::string& d) {
        Snap init(0xB7CB00, 0x100), ga(0xB7CB00, 0x100), ge(0xB7CB00, 0x100);
        init.FillRandom(r);
        // keep the divider small and the step sane most of the time
        const uint32_t div = r.below(6) ? 1 + r.below(4) : r.u32();
        std::memcpy(&init.b[0], &init.b[0], 0);
        init.Restore();
        *reinterpret_cast<uint32_t*>(&CTimer::m_snTimerDivider) = div;
        init.Save();
        const float el = r.below(6) ? r.f01() * 200.f : GenF(r, 5000.f);
        CTimer::UpdateVariables(el); ga.Save();
        init.Restore();
        oracle::Fn<void __cdecl(float)>(0x5618D0)(el); ge.Save();
        if (ga.SameAs(ge)) return true;
        d = "el " + F(el) + " div " + std::to_string(div);
        for (size_t i = 0; i < ga.b.size(); i += 4) if (std::memcmp(&ga.b[i], &ge.b[i], 4)) { char t[80]; std::snprintf(t, sizeof t, " [+%zX got %02X%02X%02X%02X exe %02X%02X%02X%02X]", i, ga.b[i+3], ga.b[i+2], ga.b[i+1], ga.b[i], ge.b[i+3], ge.b[i+2], ge.b[i+1], ge.b[i]); d += t; }
        return false;
    });
}
