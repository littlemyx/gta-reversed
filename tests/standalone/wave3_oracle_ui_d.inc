// ORACLE-W3-UI rows D: Font.cpp state setters, letter widths and token filter (see wave3_oracle_ui.inc).
#include "Scripts/TheScripts.h"
namespace w3ud {
using namespace w3u; using namespace w2;
constexpr uintptr_t kFontLo = 0xC71A54, kFontHi = 0xC71AD0, kFontData = 0xC718B0;
static void PutFont(Rng& r) {
    FillGarbage(r, (void*)kFontLo, kFontHi - kFontLo);
    FillGarbage(r, (void*)kFontData, 2 * 210);
    CFont::RenderState.m_wFontTexture = (uint16_t)r.below(2);
    CFont::RenderState.m_bPropOn = r.below(2);
    CFont::m_fFontAlpha = r.below(3) == 0 ? 255.f : Fl(r, 300.f, 6);
    CFont::m_Scale = CVector2D(Fl(r, 5.f, 8), Fl(r, 5.f, 8));
    for (int i = 0; i < 2; ++i) *reinterpret_cast<uint8_t*>(kFontData + 210 * i + 209) = (uint8_t)r.below(40);
    FrontEndMenuManager.m_nPrefsLanguage = (eLanguage)r.below(8);
    FillGarbage(r, &CTheScripts::IntroTextLines, sizeof(CTheScripts::IntroTextLines));
    CTheScripts::NumberOfIntroTextLinesThisFrame = (uint16_t)r.below((uint32)CTheScripts::IntroTextLines.size());
    auto& t = CTheScripts::IntroTextLines[CTheScripts::NumberOfIntroTextLinesThisFrame];
    t.FontStyle = eFontStyleS32(static_cast<eFontStyle>(r.below(4)));
    t.IsProportional = r.below(2); t.Scale = CVector2D(Fl(r, 4.f, 8), Fl(r, 4.f, 8));
}
static uint32_t Rgba(Rng& r) { return r.u32(); }
static uint32_t RgbaArg(uint32_t v) { return v; }
static CRGBA ToC(uint32_t v) { CRGBA c; std::memcpy(&c, &v, 4); return c; }
// token strings of the in-place filter (terminated tokens only: the exe walks off the end of its 252 byte buffer for an unterminated one, the port stops there)
static const char* const kTok[] = { "", "plain", "~r~red~w~ white", "a~b~c~d~e", "~~", "~~~~", "x~y~", "~n~new~n~line", "~h~~r~~g~~b~", "t ~k~~X~ t", "A~1~B", "~a~", "ab~cd~~ef~gh" };
} // namespace w3ud

static void TestW3UiD() {
    using namespace w2; using namespace w3u; using namespace w3ud;
    ROW("CFont::SetScale", 0x719380, 100) {
        const float w = Fl(r, 5.f, 6), h = Fl(r, 5.f, 6); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetScale(w, h); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(float, float)>(0x719380)(w, h); return 0; });
    } ENDROW;
    ROW("CFont::SetScaleForCurrentLanguage", 0x7193A0, 300) {
        const float w = Fl(r, 5.f, 6), h = Fl(r, 5.f, 6); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetScaleForCurrentLanguage(w, h); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(float, float)>(0x7193A0)(w, h); return 0; });
    } ENDROW;
    ROW("CFont::SetSlantRefPoint", 0x719400, 100) {
        const float x = Fl(r, 700.f, 6), y = Fl(r, 700.f, 6); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetSlantRefPoint(x, y); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(float, float)>(0x719400)(x, y); return 0; });
    } ENDROW;
    ROW("CFont::SetSlant", 0x719420, 100) {
        const float v = Fl(r, 2.f, 6); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetSlant(v); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(float)>(0x719420)(v); return 0; });
    } ENDROW;
    ROW("CFont::SetFontStyle", 0x719490, 200) {
        const int st = (int)r.below(8); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetFontStyle((eFontStyle)st); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x719490)(st); return 0; });
    } ENDROW;
    ROW("CFont::SetWrapx", 0x7194D0, 100) {
        const float v = Fl(r, 700.f, 6); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetWrapx(v); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(float)>(0x7194D0)(v); return 0; });
    } ENDROW;
    ROW("CFont::SetCentreSize", 0x7194E0, 100) {
        const float v = Fl(r, 700.f, 6); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetCentreSize(v); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(float)>(0x7194E0)(v); return 0; });
    } ENDROW;
    ROW("CFont::SetRightJustifyWrap", 0x7194F0, 100) {
        const float v = Fl(r, 700.f, 6); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetRightJustifyWrap(v); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(float)>(0x7194F0)(v); return 0; });
    } ENDROW;
    ROW("CFont::SetAlphaFade", 0x719500, 100) {
        const float v = Fl(r, 300.f, 6); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetAlphaFade(v); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(float)>(0x719500)(v); return 0; });
    } ENDROW;
    ROW("CFont::SetDropColor", 0x719510, 1500) {
        const uint32_t c = Rgba(r); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetDropColor(ToC(c)); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(uint32_t)>(0x719510)(RgbaArg(c)); return 0; });
    } ENDROW;
    ROW("CFont::SetDropShadowPosition", 0x719570, 200) {
        const int v = (int)(int16_t)r.u32(); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetDropShadowPosition((int16_t)v); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x719570)(v); return 0; });
    } ENDROW;
    ROW("CFont::SetEdge", 0x719590, 200) {
        const int v = (int)(int8_t)r.u32(); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetEdge((int8_t)v); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x719590)(v); return 0; });
    } ENDROW;
    ROW("CFont::SetProportional", 0x7195B0, 100) {
        const bool v = r.below(2); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetProportional(v); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x7195B0)(v ? 1 : 0); return 0; });
    } ENDROW;
    ROW("CFont::SetBackground", 0x7195C0, 100) {
        const bool a = r.below(2), b = r.below(2); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetBackground(a, b); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int, int)>(0x7195C0)(a ? 1 : 0, b ? 1 : 0); return 0; });
    } ENDROW;
    ROW("CFont::SetBackgroundColor", 0x7195E0, 100) {
        const uint32_t c = Rgba(r); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetBackgroundColor(ToC(c)); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(uint32_t)>(0x7195E0)(RgbaArg(c)); return 0; });
    } ENDROW;
    ROW("CFont::SetJustify", 0x719600, 100) {
        const bool v = r.below(2); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { CFont::SetJustify(v); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x719600)(v ? 1 : 0); return 0; });
    } ENDROW;
    ROW("CFont::SetOrientation", 0x719610, 200) {
        const int v = (int)r.below(3); Tu tu([](Rng& rr) { PutFont(rr); });   // (the exe leaves the flags alone for an out-of-range alignment; the port clears them: no caller passes one)
        return UCase(r, d, [&]() -> uint64_t { CFont::SetOrientation((eFontAlignment)v); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x719610)(v); return 0; });
    } ENDROW;
    ROW("GetLetterIdPropValue", 0x718770, 1000) {
        const uint8_t id = (uint8_t)(r.below(6) == 0 ? '?' : r.below(208)); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { return FB(GetLetterIdPropValue(id)); }, [&]() -> uint64_t { return FB(oracle::Fn<float __cdecl(int)>(0x718770)(id)); });
    } ENDROW;
    ROW("GetScriptLetterSize", 0x719670, 2000) {
        const uint8_t id = (uint8_t)(r.below(6) == 0 ? '?' : r.below(4) == 0 ? 0x91 + r.below(16) : r.u32()); Tu tu([](Rng& rr) { PutFont(rr); });
        return UCase(r, d, [&]() -> uint64_t { return FB(GetScriptLetterSize(id)); }, [&]() -> uint64_t { return FB(oracle::Fn<float __cdecl(int)>(0x719670)(id)); });
    } ENDROW;
    ROW("CFont::FilterOutTokensFromString", 0x719240, 600) {
        const int k = (int)r.below(sizeof(kTok) / sizeof(kTok[0])); Tu tu([&](Rng& rr) { std::memset(S(oOut), 0xA5, 0x400); std::strcpy((char*)S(oOut), kTok[k]); (void)rr; });
        GxtChar* s = S(oOut);
        return UCase(r, d, [&]() -> uint64_t { CFont::FilterOutTokensFromString(s); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(GxtChar*)>(0x719240)(s); return 0; });
    } ENDROW;
}
