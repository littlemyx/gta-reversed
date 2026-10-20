// ORACLE-W3-EFFECTS rows E: Coronas (RegisterCorona with a corona type, DoSunAndMoon) and CClouds::VolumetricCloudsGetMaxDistance (see wave3_oracle_fx.inc).
// CSprite::CalcScreenCoors (0x70CE30, DoSunAndMoon's only other callee that reads the RenderWare camera) is replaced on both sides by the same deterministic host function that records its arguments;
// the call log lives in the scratch block and is compared.
#include "Sprite.h"
#include "Game.h"
#include "Draw.h"
#include "RegisteredCorona.h"
namespace w3fe {
using namespace w3f; using namespace w2;
constexpr size_t oRec = 0x1E000;   // u32 count, then 40 byte entries {in xyz, out xyz, w, h, flags}
static bool CalcImpl(const RwV3d* in, RwV3d* out, float* w, float* h, bool a, bool b) {
    uint32_t& n = SAt<uint32_t>(oRec);
    uint32_t hv; std::memcpy(&hv, &in->x, 4); uint32_t hy; std::memcpy(&hy, &in->y, 4); uint32_t hz; std::memcpy(&hz, &in->z, 4);
    const bool vis = (((hv ^ (hy * 3u) ^ (hz * 5u)) >> 3) & 1u) == 0;
    if (vis) { out->x = in->x * 0.25f + 1.f; out->y = in->y * 0.5f - 2.f; out->z = in->z + 0.125f; *w = 3.f; *h = 4.f; }
    if (n < 4) { float* e = reinterpret_cast<float*>(S(oRec + 4 + n * 40)); e[0] = in->x; e[1] = in->y; e[2] = in->z; e[3] = vis ? out->x : 0.f; e[4] = vis ? out->y : 0.f; e[5] = vis ? out->z : 0.f; e[6] = vis ? *w : 0.f; e[7] = vis ? *h : 0.f; uint32_t fl = (a ? 1u : 0u) | (b ? 2u : 0u) | (vis ? 4u : 0u); std::memcpy(&e[8], &fl, 4); }
    ++n;
    return vis;
}
static bool __cdecl X_Calc(const RwV3d* in, RwV3d* out, float* w, float* h, int a, int b) { return CalcImpl(in, out, w, h, (a & 0xFF) != 0, (b & 0xFF) != 0); }
static void InstallStubsE() { static bool done = false; if (done) return; done = true; oracle::Patch(0x70CE30, (void*)&X_Calc); }
static void SeedCoronas(Rng& r) {
    CCoronas::NumCoronas = r.below(8); CCoronas::LightsMult = r.below(3) ? r.f01() : GenF(r, 2.f, S_ALL); CCoronas::bChangeBrightnessImmediately = r.below(3) == 0 ? (int)r.below(4) : 0; CCoronas::SunBlockedByClouds = r.below(2);
    CTimer::ms_fTimeStep = r.below(4) ? r.f01() * 4.f : GenF(r, 8.f, S_ALL);
    FillGarbage(r, &CCoronas::aCoronas, sizeof(CCoronas::aCoronas));
    for (auto& c : CCoronas::aCoronas) {
        c.m_dwId = r.below(3) == 0 ? 0 : 1 + r.below(8); c.m_pTexture = nullptr; c.m_pAttachedTo = nullptr; c.m_bAttached = false; c.m_bCheckObstacles = (eCoronaLOSCheck)0;
        c.m_vPosn = GenV(r, 200.f, S_NAN | S_ZERO | S_TINY); c.m_fFadeSpeed = r.below(4) ? r.f01() * 20.f : GenF(r, 40.f, S_ALL); c.m_fSize = r.f01() * 3.f; c.m_fFarClip = r.f01() * 200.f;
        c.m_FadedIntensity = (uint8)r.u32(); c.m_Color = CRGBA((uint8)r.u32(), (uint8)r.u32(), (uint8)r.u32(), (uint8)r.u32()); c.m_bRegisteredThisFrame = r.below(2); c.m_bJustCreated = r.below(2); c.m_bOffScreen = r.below(2); c.m_bOnlyFromBelow = r.below(2);
    }
    for (int i = 0; i < (int)CORONATYPE_COUNT; ++i) gpCoronaTexture[i] = reinterpret_cast<RwTexture*>(0x1234 + 0x10 * i);
}
static void CamAt(Rng& r) { TheCamera.m_matrix = nullptr; TheCamera.m_placement.m_vPosn = GenV(r, 200.f, S_NAN | S_ZERO | S_TINY); }
} // namespace w3fe
bool CGame::CanSeeOutSideFromCurrArea() { return currArea == AREA_CODE_NORMAL_WORLD; }   // Game.cpp is not linked (the port body)
bool CSprite::CalcScreenCoors(const RwV3d& posn, RwV3d* out, float* w, float* h, bool checkMaxVisible, bool checkMinVisible) { return w3fe::CalcImpl(&posn, out, w, h, checkMaxVisible, checkMinVisible); }

static void TestW3FxE() {
    using namespace w2; using namespace w3f; using namespace w3fe;
    InstallStubsE();
    ROW("CCoronas::RegisterCorona-type", 0x6FC580, 3000) {
        const uint32 id = r.below(12); const uint8 cr = (uint8)r.u32(), cg = (uint8)r.u32(), cb = (uint8)r.u32(), in = r.below(4) ? (uint8)r.u32() : (uint8)0;
        const auto ctype = (eCoronaType)r.below(CORONATYPE_COUNT);
        const float size = GenF(r, 5.f, S_ALL), range = r.below(4) ? r.f01() * 200.f : GenF(r, 400.f, S_ALL), ang = GenF(r, 7.f, S_ALL), pull = GenF(r, 3.f, S_ALL), fade = r.below(4) ? r.f01() * 20.f : GenF(r, 40.f, S_ALL);
        const auto fl = (eCoronaFlareType)r.below(4); const auto rf = (eCoronaReflType)r.below(3); const auto los = (eCoronaLOSCheck)r.below(2); const auto tr = (eCoronaTrail)r.below(2);
        const bool neon = r.below(3) == 0, fullBright = r.below(2), below = r.below(2), white = r.below(2);
        const bool nearPos = r.below(2); const CVector rel = GenV(r, 80.f, S_ALL), absPos = GenV(r, 400.f, S_ALL);
        Tu tu([&](Rng& rr) { SeedCoronas(rr); CamAt(rr); SAt<CVector>(0x100) = nearPos ? CVector{ TheCamera.m_placement.m_vPosn.x + rel.x, TheCamera.m_placement.m_vPosn.y + rel.y, TheCamera.m_placement.m_vPosn.z + rel.z } : absPos; });
        return UCase(r, d, [&]() -> uint64_t { CCoronas::RegisterCorona(id, nullptr, cr, cg, cb, in, SAt<CVector>(0x100), size, range, ctype, fl, rf, los, tr, ang, neon, pull, fullBright, fade, below, white); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(uint32, CEntity*, uint32, uint32, uint32, uint32, const CVector*, float, float, uint32, uint32, uint32, uint32, uint32, float, uint32, float, uint32, float, uint32, uint32)>(0x6FC580)(id, nullptr, cr, cg, cb, in, &SAt<CVector>(0x100), size, range, (uint32)ctype, (uint32)fl, (uint32)rf, (uint32)los, (uint32)tr, ang, neon ? 1 : 0, pull, fullBright ? 1 : 0, fade, below ? 1 : 0, white ? 1 : 0); return 0; });
    } ENDROW;
    ROW("CCoronas::DoSunAndMoon", 0x6FC5A0, 3000) {
        Tu tu([](Rng& rr) { SeedCoronas(rr); CamAt(rr); SAt<uint32_t>(oRec) = 0;
            CGame::currArea = (eAreaCodes)(rr.below(4) == 0 ? rr.below(4) : 0);
            CTimeCycle::m_CurrentStoredValue = rr.below(16);
            for (auto& v : CTimeCycle::m_VectorToSun) v = rr.below(8) == 0 ? GenV(rr, 1.f, S_ALL) : CVector{ rr.f01() * 2.f - 1.f, rr.f01() * 2.f - 1.f, rr.below(2) ? rr.f01() * 1.2f - 0.3f : 0.f };
            CDraw::SetFarClipZ(rr.below(10) == 0 ? GenF(rr, 3000.f, S_ALL) : 100.f + rr.f01() * 2900.f);
            auto& cc = CTimeCycle::m_CurrentColours; cc.m_nSunCoronaRed = (uint8)rr.u32(); cc.m_nSunCoronaGreen = (uint8)rr.u32(); cc.m_nSunCoronaBlue = (uint8)rr.u32(); cc.m_nSunCoreRed = (uint8)rr.u32(); cc.m_nSunCoreGreen = (uint8)rr.u32(); cc.m_nSunCoreBlue = (uint8)rr.u32(); cc.m_fSunSize = (int8)rr.u32();
            CCoronas::SunScreenX = GenF(rr, 100.f, S_ALL); CCoronas::SunScreenY = GenF(rr, 100.f, S_ALL); });
        return UCase(r, d, [&]() -> uint64_t { CCoronas::DoSunAndMoon(); return SAt<uint32_t>(oRec); }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x6FC5A0)(); return SAt<uint32_t>(oRec); });
    } ENDROW;
    ROW("CClouds::VolumetricCloudsGetMaxDistance", 0x713630, 600) {
        // the far plane lives at +0x84 of the exe's RwCamera and at librw's offset in the port's one: the same value is stored at both places (the min(far, 600) logic is what is compared)
        Tu tu([](Rng& rr) { auto* cam = reinterpret_cast<RwCamera*>(S(0x2000)); Scene.m_pRwCamera = cam; *reinterpret_cast<RwCamera**>(0xC1703C) = cam;
            const float farZ = rr.below(6) == 0 ? GenF(rr, 2000.f, S_ALL) : (rr.below(2) ? 100.f + rr.f01() * 900.f : 600.f); cam->farPlane = farZ; *reinterpret_cast<float*>(S(0x2000 + 0x84)) = farZ; });
        return UCase(r, d, [&]() -> uint64_t { return FB(CClouds::VolumetricCloudsGetMaxDistance()); }, [&]() -> uint64_t { return FB(oracle::Fn<float __cdecl()>(0x713630)()); });
    } ENDROW;
}
