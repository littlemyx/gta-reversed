// ORACLE-W3-UI rows E: Text (key search, loaded mission name), Sprite2d / Sprite helpers, MenuManager helpers (see wave3_oracle_ui.inc).
#include "Text/KeyArray.h"
#include "Frontend/MenuManager_Internal.h"
struct W3uBsTag { using type = CKeyEntry* (CKeyArray::*)(uint32, CKeyEntry*, int16, int16); friend type W2Get(W3uBsTag); };
template struct W2Rob<W3uBsTag, &CKeyArray::BinarySearch>;
namespace w3ue {
using namespace w3u; using namespace w2;
template<class R, class... A> static R ExeThis(uint32_t addr, void* self, A... a) { return oracle::Fn<R __fastcall(void*, int, A...)>(addr)(self, 0, a...); }
constexpr size_t oKeys = 0x6000;   // 64 key entries {string, hash}
static CKeyEntry* Keys() { return reinterpret_cast<CKeyEntry*>(S(oKeys)); }
static void PutKeys(Rng& r, int n) {
    uint32_t h = r.u32() >> 4;
    for (int i = 0; i < n; ++i) { h += 1 + (r.below(8) == 0 ? 0 : r.below(1u << (r.below(3) ? 20 : 28))); Keys()[i].hash = h; Keys()[i].string = reinterpret_cast<const GxtChar*>(S(oStr + 0x100 * (i % 26))); }
}
static uint64_t KeyOff(const CKeyEntry* e) { return e ? (uint64_t)(e - Keys()) + 1 : 0; }
} // namespace w3ue

static void TestW3UiE() {
    using namespace w2; using namespace w3u; using namespace w3ue;
    ROW("CKeyArray::BinarySearch", 0x69F570, 3000) {
        const int n = (int)r.below(40) + 1;
        const bool hit = r.below(3) != 0;
        const int pick = (int)r.below(n);
        const int16_t first = (int16_t)(r.below(6) == 0 ? (int)r.below(n + 2) - 1 : 0), last = (int16_t)(r.below(6) == 0 ? (int)r.below(n + 2) - 1 : n - 1);
        const uint32_t rnd = r.u32() >> (r.below(2) ? 4 : 0);
        Tu tu([&](Rng& rr) { PutKeys(rr, n); });
        return UCase(r, d, [&]() -> uint64_t {
            CKeyArray ka; ka.m_data = Keys(); ka.m_size = (uint32_t)n;
            const uint32_t hs = hit ? Keys()[pick].hash : rnd;
            const auto rv = (ka.*W2Get(W3uBsTag{}))(hs, Keys(), first, last); ka.m_data = nullptr; return KeyOff(rv); },
            [&]() -> uint64_t {
            CKeyArray ka; ka.m_data = Keys(); ka.m_size = (uint32_t)n;
            const uint32_t hs = hit ? Keys()[pick].hash : rnd;
            const auto rv = ExeThis<CKeyEntry*, uint32_t, CKeyEntry*, int, int>(0x69F570, &ka, hs, Keys(), first, last); ka.m_data = nullptr; return KeyOff(rv); });
    } ENDROW;
    ROW("CText::GetNameOfLoadedMissionText", 0x69FBD0, 300) {
        const int len = (int)r.below(8); char nm[8]{}; for (int i = 0; i < len; ++i) nm[i] = (char)('A' + r.below(26));
        Tu tu([&](Rng& rr) { (void)rr; });
        uint8_t* text = S(oBuf);
        char* out = reinterpret_cast<char*>(S(oOut));
        return UCase(r, d, [&]() -> uint64_t { std::memcpy(text + 0x24, nm, 8); reinterpret_cast<CText*>(text)->GetNameOfLoadedMissionText(out); return 0; },
            [&]() -> uint64_t { std::memcpy(text + 0x24, nm, 8); ExeThis<void, char*>(0x69FBD0, text, out); return 0; });
    } ENDROW;
    // ---- Sprite2d / Sprite
    ROW("CSprite2d::Constructor", 0x727230, 100) {
        uint8_t* o = S(oBuf);
        return UCase(r, d, [&]() -> uint64_t { return U(reinterpret_cast<CSprite2d*>(o)->Constructor()) - U(o); }, [&]() -> uint64_t { return U(ExeThis<void*>(0x727230, o)) - U(o); });
    } ENDROW;
    ROW("CSprite2d::SetRecipNearClip", 0x727260, 20) {
        return UCase(r, d, [&]() -> uint64_t { CSprite2d::SetRecipNearClip(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x727260)(); return 0; });
    } ENDROW;
    ROW("CSprite2d::SetAddressing", 0x727320, 400) {
        const int mode = (int)r.below(6); const bool has = r.below(5) != 0;
        uint8_t* o = S(oBuf); uint8_t* tex = S(oBuf + 0x100);
        Tu tu([&](Rng& rr) { (void)rr; });
        return UCase(r, d, [&]() -> uint64_t { *reinterpret_cast<uint8_t**>(o) = has ? tex : nullptr; reinterpret_cast<CSprite2d*>(o)->SetAddressing((RwTextureAddressMode)mode); return 0; },
            [&]() -> uint64_t { *reinterpret_cast<uint8_t**>(o) = has ? tex : nullptr; ExeThis<void, int>(0x727320, o, mode); return 0; });
    } ENDROW;
    ROW("CSprite2d::SetAddressingUV", 0x7272E0, 400) {
        const int mu = (int)r.below(6), mv = (int)r.below(6); const bool has = r.below(5) != 0;
        uint8_t* o = S(oBuf); uint8_t* tex = S(oBuf + 0x100);
        return UCase(r, d, [&]() -> uint64_t { *reinterpret_cast<uint8_t**>(o) = has ? tex : nullptr; reinterpret_cast<CSprite2d*>(o)->SetAddressingUV((RwTextureAddressMode)mu, (RwTextureAddressMode)mv); return 0; },
            [&]() -> uint64_t { *reinterpret_cast<uint8_t**>(o) = has ? tex : nullptr; ExeThis<void, int, int>(0x7272E0, o, mu, mv); return 0; });
    } ENDROW;
    // ---- MenuManager helpers
    ROW("CMenuManager::SetHelperText", 0x57CD10, 200) {
        const int id = (int)r.below(10); Tu tu([](Rng& rr) { FillGarbage(rr, &FrontEndMenuManager, sizeof(FrontEndMenuManager)); });
        return UCase(r, d, [&]() -> uint64_t { FrontEndMenuManager.SetHelperText((eHelperText)id); return 0; }, [&]() -> uint64_t { ExeThis<void, int>(0x57CD10, &FrontEndMenuManager, id); return 0; });
    } ENDROW;
    ROW("CMenuManager::ResetHelperText", 0x57CD30, 100) {
        Tu tu([](Rng& rr) { FillGarbage(rr, &FrontEndMenuManager, sizeof(FrontEndMenuManager)); });
        return UCase(r, d, [&]() -> uint64_t { FrontEndMenuManager.ResetHelperText(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x57CD30, &FrontEndMenuManager); return 0; });
    } ENDROW;
    ROW("CMenuManager::CheckHover", 0x57C4F0, 2000) {
        const int mx = r.below(10) == 0 ? (int)r.u32() : (int)r.below(700) - 20, my = (int)r.below(500) - 20;
        const auto edge = [&](int v) { return r.below(3) == 0 ? v : v + (int)r.below(41) - 20; };   // bounds around the mouse (equal bounds exclude it)
        const int l = r.below(10) == 0 ? (int)r.u32() : edge(mx - (int)r.below(60)), rt = r.below(10) == 0 ? (int)r.u32() : edge(mx + (int)r.below(60)), t = edge(my - (int)r.below(40)), b = edge(my + (int)r.below(40));
        Tu tu([&](Rng& rr) { FillGarbage(rr, &FrontEndMenuManager, sizeof(FrontEndMenuManager)); FrontEndMenuManager.m_nMousePosX = mx; FrontEndMenuManager.m_nMousePosY = my; });
        return UCase(r, d, [&]() -> uint64_t { return B8(FrontEndMenuManager.CheckHover(l, rt, t, b)); }, [&]() -> uint64_t { return ExeThis<uint32_t, int, int, int, int>(0x57C4F0, &FrontEndMenuManager, l, rt, t, b) & 0xFF; });
    } ENDROW;
    ROW("CMenuManager::GetNumberOfMenuOptions", 0x573E70, 1500) {
        const int screen = r.below(12) == 0 ? (int)r.below(3) : (int)r.below(SCREEN_COUNT); const bool cached = r.below(3) == 0; const uint8_t cachedN = (uint8_t)r.below(30);
        Tu tu([&](Rng& rr) { FillGarbage(rr, &FrontEndMenuManager, sizeof(FrontEndMenuManager)); FrontEndMenuManager.m_nCurrentScreen = (eMenuScreen)screen; FrontEndMenuManager.m_nNumberOfMenuOptions = cachedN; *reinterpret_cast<int32_t*>(0x8CDFF0) = cached ? screen : (int)rr.below(SCREEN_COUNT) - 5; });
        return UCase(r, d, [&]() -> uint64_t { return FrontEndMenuManager.GetNumberOfMenuOptions(); }, [&]() -> uint64_t { return ExeThis<uint32_t>(0x573E70, &FrontEndMenuManager) & 0xFF; });   // (the exe returns the count in al only)
    } ENDROW;
    // front-end direction checks: key state, analogue stick (with the function-local "previous stick" static at its exe address) and d-pad of the menu's pad
    {
        struct In { const char* nm; uint32_t addr; bool (CMenuManager::*pf)() const; };
        static const In ins[] = { { "CMenuManager::CheckFrontEndUpInput", 0x573840, &CMenuManager::CheckFrontEndUpInput }, { "CMenuManager::CheckFrontEndDownInput", 0x5738B0, &CMenuManager::CheckFrontEndDownInput },
            { "CMenuManager::CheckFrontEndLeftInput", 0x573920, &CMenuManager::CheckFrontEndLeftInput }, { "CMenuManager::CheckFrontEndRightInput", 0x573990, &CMenuManager::CheckFrontEndRightInput } };
        for (const auto& in : ins) {
            const std::string nm = std::string(in.nm) + " 0x" + [&] { char b[16]; std::snprintf(b, sizeof b, "%X", in.addr); return std::string(b); }();
            RunScaledU(nm, [&](Rng& r, std::string& d) -> bool {
                Tu tu([&](Rng& rr) {
                    FillGarbage(rr, &CPad::Pads, sizeof(CPad::Pads));
                    for (auto& p : CPad::Pads) {
                        auto* b = reinterpret_cast<uint8_t*>(&p);
                        for (size_t i = 0; i < sizeof(CPad); ++i) if (rr.below(3) != 0) b[i] = 0;   // sparse buttons / keys
                    }
                    FillGarbage(rr, &FrontEndMenuManager, sizeof(FrontEndMenuManager)); FrontEndMenuManager.m_nPlayerNumber = (uint8_t)rr.below(2);
                    for (auto& p : CPad::Pads) { p.NewState.LeftStickX = (int16_t)(rr.below(3) ? (int)rr.below(255) - 127 : 0); p.NewState.LeftStickY = (int16_t)(rr.below(3) ? (int)rr.below(255) - 127 : 0); }
                    *reinterpret_cast<int16_t*>(0xB736F0) = (int16_t)((int)rr.below(255) - 127); *reinterpret_cast<int16_t*>(0xB736F4) = (int16_t)((int)rr.below(255) - 127); *reinterpret_cast<int16_t*>(0xB73700) = (int16_t)((int)rr.below(255) - 127); *reinterpret_cast<int16_t*>(0xB73704) = (int16_t)((int)rr.below(255) - 127);
                });
                return UCase(r, d, [&]() -> uint64_t { return B8((FrontEndMenuManager.*in.pf)()); }, [&]() -> uint64_t { return ExeThis<uint32_t>(in.addr, &FrontEndMenuManager) & 0xFF; });
            }, 1500);
        }
    }
}
