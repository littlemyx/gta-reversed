// ORACLE-W3-RENDER: exe-oracle rows for the A + B functions of game_sa/Renderer.cpp (render / LOD list helpers, sector scan lists, entity lighting), PostEffects.cpp (script switches, night / infrared vision
// light wrappers, day-night balance store / restore, speed FX), GrassRenderer.cpp (setters, plant model tab, triangle point) and Plugins/TwoDEffectPlugin/2dEffect.cpp (road sign flag decoding, geometry
// plugin accessors / constructors). Included by wave2_oracle_all.inc when W3_RENDER_ONLY is defined (executable wave3_render_oracle_test = the W2 harness: deterministic arena, the whole exe data image +
// a 16 KB fixture block compared after every case).
// RenderWare / callee TUs that are not linked are stubbed: the exe's RpLightSetColor (0x751A90), CVisibilityPlugins::InsertEntityIntoSortedList (0x734570), CPointLights::GenerateLightsAffectingObject
// (0x6FFBB0) / RemoveLightsAffectingObject (0x6FFFE0), CCarFXRenderer::SetFxEnvMapLightMult (0x5D5BA0) and CPhysical::GetLightingFromCol (0x5447B0, thiscall) are patched to host stubs and the port
// symbols of the same names are defined in this TU with the same bodies; every stub records its call (kind, pointers, float bits) into the fixture block, so what is compared is the order / values of
// the calls plus every fixture byte and the whole exe data image.
#include "Renderer.h"
#include "PostEffects.h"
#include "GrassRenderer.h"
#include "PPTriPlantBuffer.h"
#include "Plugins/TwoDEffectPlugin/2dEffect.h"
#include "VisibilityPlugins.h"
#include "PointLights.h"
#include "CarFXRenderer.h"
#include "Entity/Physical.h"
#include "Entity/Ped/Ped.h"
#include "Pipelines/CustomBuilding/CustomBuildingDNPipeline.h"
#include "app/app_light.h"
#include "Entity/Vehicle/Heli.h"
#include "Shadows.h"
#include "Streaming.h"
#include "Clock.h"
#include "World.h"

namespace w3r {
using namespace w2;
constexpr size_t kScSize = 0x4000;
alignas(16) static uint8_t g_sc[kScSize];
constexpr size_t kEnt = 0x000 /*6 x 0x200*/, kMi = 0xC00 /*4 x 0x80*/, kNodes = 0xE00 /*5 lists x 8 nodes x 16*/, kRec = 0x1100 /*0x100*/, kOut = 0x1200 /*64 x 4*/, kStub = 0x1300 /*0x40*/,
                 kLtA = 0x1400, kLtD = 0x1440, kLtE = 0x1480 /*6 x 0x40*/, kCol = 0x1600, kGeo = 0x1800 /*0x100*/, kFx = 0x1A00 /*0x110*/, kMat = 0x1B80 /*0x50*/, kV = 0x1C00 /*5 x 0x10*/, kPed = 0x2000 /*0x800*/;
constexpr int kNEnt = 6;
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static uint32_t FBits(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
template<class R, class... A> static R ExeC(uint32_t addr, A... a) { return oracle::Fn<R __cdecl(A...)>(addr)(a...); }
static float Fe(Rng& r, float scale, int special = 10) {
    if (special && r.below((uint32_t)special) == 0) return GenF(r, scale, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF | S_HUGE);
    return GenF(r, scale, S_ZERO);
}
static float Fg(Rng& r, float scale) { return GenF(r, scale, S_ZERO); }

// ---- recorded calls (both sides run the same stubs) ----
static void Rec(uint32_t kind, uint32_t a, uint32_t b, uint32_t c = 0, uint32_t d = 0, uint32_t e = 0) {
    uint32_t& n = SAt<uint32_t>(kRec);
    if (n < 8) { uint32_t* p = &SAt<uint32_t>(kRec + 4 + n * 24); p[0] = kind; p[1] = a; p[2] = b; p[3] = c; p[4] = d; p[5] = e; }
    ++n;
}
static void* StubSetColor(void* light, const float* c) { uint32_t b[4]; std::memcpy(b, c, 16); Rec(1, (uint32_t)(uintptr_t)light, b[0], b[1], b[2], b[3]); return light; }
static bool StubInsert(void* entity, float dist) { Rec(3, (uint32_t)(uintptr_t)entity, FBits(dist)); return SAt<uint8_t>(kStub) != 0; }
static float StubGen(const float* point, float* total, void* entity) {
    Rec(4, (uint32_t)(uintptr_t)entity, FBits(point[0]), FBits(point[1]), FBits(point[2]), (uint32_t)(uintptr_t)total);
    *total = SAt<float>(kStub + 4);
    return SAt<float>(kStub + 8);
}
static void StubRemove() { Rec(5, 0, 0); }
static void StubEnv(float m) { Rec(6, FBits(m), 0); }
static float StubGL(void* self, bool b) { Rec(7, (uint32_t)(uintptr_t)self, b ? 1u : 0u); return SAt<float>(kStub + 12); }
static float __fastcall StubGLFC(void* self, void* /*edx*/, bool b) { return StubGL(self, b); }
static void InstallStubs() {
    static bool done = false; if (done) return; done = true;
    oracle::Patch(0x751A90, (void*)&StubSetColor);
    oracle::Patch(0x734570, (void*)&StubInsert);
    oracle::Patch(0x6FFBB0, (void*)&StubGen);
    oracle::Patch(0x6FFFE0, (void*)&StubRemove);
    oracle::Patch(0x5D5BA0, (void*)&StubEnv);
    oracle::Patch(0x5447B0, (void*)&StubGLFC);
}

static void BuildLightGlobals(Rng& r) {
    pAmbient = reinterpret_cast<RpLight*>(S(kLtA)); pDirect = reinterpret_cast<RpLight*>(S(kLtD));
    for (int i = 0; i < 6; ++i) pExtraDirectionals[i] = reinterpret_cast<RpLight*>(S(kLtE + 0x40 * i));
    auto col = [&](RwRGBAReal& c) { c.red = Fe(r, 1.2f, 12); c.green = Fe(r, 1.2f, 12); c.blue = Fe(r, 1.2f, 12); c.alpha = Fe(r, 1.2f, 12); };
    col(AmbientLightColour); col(AmbientLightColourForFrame); col(AmbientLightColourForFrame_PedsCarsAndObjects); col(DirectionalLightColour); col(DirectionalLightColourForFrame);
    col(DirectAmbientLight[0]); col(DirectAmbientLight[1]); col(CPostEffects::m_fInfraredVisionHeatObjectCol);
    for (float& f : LightStrengths) f = Fe(r, 2.f, 12);
    NumExtraDirectionalLights = r.below(7);
    CTimer::m_snTimeInMilliseconds = r.u32();
    CTimeCycle::m_BrightnessAddedToAmbientRed = Fe(r, 0.5f, 12);
    CClock::ms_nGameClockHours = r.below(10) ? r.below(24) : r.below(256);
    CClock::ms_nGameClockMinutes = r.below(10) ? r.below(60) : r.below(256);
    CClock::ms_nGameClockSeconds = r.below(10) ? r.below(60) : r.below(65536);
}

static CEntity* Ent(int i) { return reinterpret_cast<CEntity*>(S(kEnt + 0x200 * i)); }
static void SetupEntity(Rng& r, int i) {
    CEntity* e = Ent(i);
    e->m_nModelIndex = (uint16)r.below(4);
    SAt<uint32_t>(kEnt + 0x200 * i + 0x18) = r.below(3) ? (uint32_t)(uintptr_t)S(kOut) : 0u;              // CEntity::m_pRwObject
    SAt<uint32_t>(kEnt + 0x200 * i + 0x14) = r.below(2) ? (uint32_t)(uintptr_t)S(kMat) : 0u;      // CPlaceable::m_matrix: null -> placement position
    if (r.below(2)) SAt<uint8_t>(kEnt + 0x200 * i + 0x34) = 0;                                              // CEntity::m_NumLodChildren
    if (r.below(2)) e->m_bUnderwater = false;
}
static void BuildR(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    SAt<uint32_t>(kRec) = 0;
    BuildLightGlobals(r);
    for (int k = 0; k < 4; ++k) StaticRef<uint32_t>(0xA9B0C8 + 4 * k) = (uint32_t)(uintptr_t)S(kMi + 0x80 * k);
    for (int i = 0; i < kNEnt; ++i) SetupEntity(r, i);
    for (int k = 0; k < 3; ++k) SAt<float>(kMat + 0x30 + 4 * k) = Fe(r, 3000.f, 12);
    SAt<uint8_t>(kStub) = (uint8_t)r.below(2);
    SAt<float>(kStub + 4) = Fe(r, 1.5f, 8); SAt<float>(kStub + 8) = r.below(4) ? r.f01() * 2.f : Fe(r, 2.f, 3); SAt<float>(kStub + 12) = r.below(4) ? r.f01() : Fe(r, 2.f, 3);
    CRenderer::ms_nNoOfVisibleLods = (int32)r.below(990); CRenderer::ms_nNoOfVisibleEntities = (int32)r.below(990);
    CRenderer::ms_pLodRenderList = reinterpret_cast<tRenderListEntry*>(&PC_Scratch[24]) + r.below(200);
    CRenderer::ms_pLodDontRenderList = reinterpret_cast<tRenderListEntry*>(&PC_Scratch[0x2000]) + r.below(200);
    CPostEffects::m_bInCutscene = r.below(2); CPostEffects::m_bInfraredVision = r.below(2); CPostEffects::m_bNightVision = r.below(2);
    CPostEffects::m_VisionFXDayNightBalance = Fe(r, 2.f, 6); CCustomBuildingDNPipeline::m_fDNBalanceParam = Fe(r, 2.f, 6); StaticRef<float>(0xC3F860) = Fe(r, 2.f, 6);
}
static std::vector<std::pair<size_t, size_t>> g_unc;
static int g_secX = 0, g_secY = 0;
template<class PF, class EF>
static bool RCase(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&) = &BuildR) {
    const uint32_t seed = r.u32() | 1;
    build(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    sc.mask.assign(kScSize, 0);
    for (auto [a, n] : g_unc) for (size_t b = 0; b < n; ++b) sc.mask[a + b] = 1;
    Reg img{ kImgLo, kImgHi - kImgLo };
    return BothA(d, seed, { sc, img }, std::forward<PF>(pf), std::forward<EF>(ef));
}
// sector scan fixture: the five lists of one sector hold 0..3 nodes each (entities may repeat); the lists of the previous sector are cleared first
static void BuildScan(Rng& rr) {
    {
        auto& s = CWorld::GetSector(g_secX, g_secY); auto& rs = CWorld::GetRepeatSector(g_secX, g_secY);
        std::memset(&s.Buildings, 0, 4); std::memset(&s.Dummies, 0, 4); std::memset(&rs.Objects, 0, 4); std::memset(&rs.Vehicles, 0, 4); std::memset(&rs.Peds, 0, 4);
    }
    BuildR(rr);
    g_secX = (int)rr.below(120); g_secY = (int)rr.below(120);
    auto& s = CWorld::GetSector(g_secX, g_secY); auto& rs = CWorld::GetRepeatSector(g_secX, g_secY);
    void* heads[5] = { &s.Buildings, &rs.Objects, &rs.Vehicles, &rs.Peds, &s.Dummies };
    for (int l = 0; l < 5; ++l) {
        const int n = (int)rr.below(4);
        uint32_t first = 0;
        for (int j = n - 1; j >= 0; --j) {
            uint32_t* node = &SAt<uint32_t>(kNodes + 0x80 * l + 16 * j);
            node[0] = (uint32_t)(uintptr_t)Ent((int)rr.below(kNEnt)); node[1] = first; node[2] = 0;
            first = (uint32_t)(uintptr_t)node;
        }
        *reinterpret_cast<uint32_t*>(heads[l]) = first;
    }
    CWorld::ms_nCurrentScanCode = (uint16)(1 + rr.below(60000));
    const int32 area = rr.below(12) == 0 ? (int32)(0x80 + rr.below(0x80)) : (int32)rr.below(4); StaticRef<int32>(0xB72914) = area;                                  // CGame::currArea
    for (int i = 0; i < kNEnt; ++i) {
        const uint32_t k = rr.below(3); SAt<uint16_t>(kEnt + 0x200 * i + 0x2C) = (uint16)(k == 0 ? CWorld::ms_nCurrentScanCode : k == 1 ? CWorld::ms_nCurrentScanCode + 1 : rr.u32());   // CEntity::m_ScanCode
        const uint32_t a = rr.below(3); SAt<int8_t>(kEnt + 0x200 * i + 0x2F) = (int8_t)(a == 0 ? area : a == 1 ? 13 : (int)rr.below(8));   // CEntity::m_AreaCode
    }
    gpOutEntitiesForGetObjectsInFrustum = reinterpret_cast<CEntity**>(S(kOut)) + rr.below(8);
}
} // namespace w3r

// port-side entry points -> the shared stubs
RpLight* RpLightSetColor(RpLight* light, const RwRGBAReal* c) { return static_cast<RpLight*>(w3r::StubSetColor(light, reinterpret_cast<const float*>(c))); }
bool CVisibilityPlugins::InsertEntityIntoSortedList(CEntity* entity, float dist) { return w3r::StubInsert(entity, dist); }
float CPointLights::GenerateLightsAffectingObject(const CVector* point, float* totalLighting, CEntity* entity) { return w3r::StubGen(reinterpret_cast<const float*>(point), totalLighting, entity); }
void CPointLights::RemoveLightsAffectingObject() { w3r::StubRemove(); }
void CCarFXRenderer::SetFxEnvMapLightMult(float multiplier) { w3r::StubEnv(multiplier); }
float CPhysical::GetLightingFromCol(bool bInteriorLighting) { return w3r::StubGL(this, bInteriorLighting); }

#define RROW(NAME, ADDR) Run(NAME " " #ADDR, [&](Rng& r, std::string& d) -> bool {
#define REND(CASES) }, std::max(10, (CASES) / w3r_div()));
static int w3r_div() { static const int v = std::getenv("W3R_DIV") ? std::max(1, std::atoi(std::getenv("W3R_DIV"))) : 1; return v; }

static void TestW3RenderRenderer() {
    using namespace w2; using namespace w3r;
    CPhysical* const ph = static_cast<CPhysical*>(Ent(0));
    CEntity* const en = Ent(1);
    RROW("CRenderer::RemoveVehiclePedLights", 0x553390)
        return RCase(r, d, [&]() -> uint64_t { CRenderer::RemoveVehiclePedLights(ph); return 0; }, [&]() -> uint64_t { ExeC<void, CPhysical*>(0x553390, ph); return 0; });
    REND(200)
    RROW("CRenderer::AddEntityToRenderList", 0x5534B0)
        const float dist = r.below(8) ? std::fabs(Fg(r, 300.f)) : Fe(r, 300.f, 1);
        return RCase(r, d, [&]() -> uint64_t { CRenderer::AddEntityToRenderList(en, dist); return 0; }, [&]() -> uint64_t { ExeC<void, CEntity*, float>(0x5534B0, en, dist); return 0; });
    REND(1500)
    RROW("CRenderer::GetLodRenderListBase", 0x5536D0)
        return RCase(r, d, [&]() -> uint64_t { return U(CRenderer::GetLodRenderListBase()); }, [&]() -> uint64_t { return U(ExeC<void*>(0x5536D0)); });
    REND(50)
    RROW("CRenderer::GetLodDontRenderListBase", 0x5536E0)
        return RCase(r, d, [&]() -> uint64_t { return U(CRenderer::GetLodDontRenderListBase()); }, [&]() -> uint64_t { return U(ExeC<void*>(0x5536E0)); });
    REND(50)
    RROW("CRenderer::ResetLodRenderLists", 0x5536F0)
        return RCase(r, d, [&]() -> uint64_t { CRenderer::ResetLodRenderLists(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5536F0); return 0; });
    REND(100)
    RROW("CRenderer::AddToLodRenderList", 0x553710)
        const float dist = Fe(r, 800.f, 6);
        return RCase(r, d, [&]() -> uint64_t { CRenderer::AddToLodRenderList(en, dist); return 0; }, [&]() -> uint64_t { ExeC<void, CEntity*, float>(0x553710, en, dist); return 0; });
    REND(400)
    RROW("CRenderer::AddToLodDontRenderList", 0x553740)
        const float dist = Fe(r, 800.f, 6);
        return RCase(r, d, [&]() -> uint64_t { CRenderer::AddToLodDontRenderList(en, dist); return 0; }, [&]() -> uint64_t { ExeC<void, CEntity*, float>(0x553740, en, dist); return 0; });
    REND(400)
    RROW("CRenderer::SetupLightingForEntity", 0x553E40)
        // scorched: the exe builds an RwRGBAReal on its stack for WorldReplaceNormalLightsWithScorched and leaves the alpha uninitialised (see ORACLE-W3-VIS): the recorded alpha of kind-1 records is not compared then
        const auto scrub = [] { for (int i = 0; i < 2; ++i) if (SAt<uint32_t>(kRec + 4 + 24 * i) == 1 && static_cast<CPhysical*>(Ent(0))->physicalFlags.bRenderScorched) SAt<uint32_t>(kRec + 4 + 24 * i + 20) = 0; };
        return RCase(r, d, [&]() -> uint64_t { const bool v = CRenderer::SetupLightingForEntity(ph); scrub(); return v; },
                           [&]() -> uint64_t { const bool v = (uint8_t)ExeC<uint32_t, CPhysical*>(0x553E40, ph) != 0; scrub(); return v; });
    REND(1500)
    RROW("CRenderer::SetupScanLists", 0x553540)
        int sx, sy;
        switch (r.below(4)) { case 0: sx = (int)r.u32(); sy = (int)r.u32(); break; case 1: sx = (int)r.below(140) - 10; sy = (int)r.below(140) - 10; break; default: sx = (int)r.below(120); sy = (int)r.below(120); break; }
        return RCase(r, d, [&]() -> uint64_t { CRenderer::SetupScanLists(sx, sy); return 0; }, [&]() -> uint64_t { ExeC<void, int, int>(0x553540, sx, sy); return 0; });
    REND(1500)
    RROW("CRenderer::ScanSectorList_ListModels", 0x5535D0)
        return RCase(r, d, [&]() -> uint64_t { CRenderer::ScanSectorList_ListModels(g_secX, g_secY); return 0; }, [&]() -> uint64_t { ExeC<void, int, int>(0x5535D0, g_secX, g_secY); return 0; }, &BuildScan);
    REND(1500)
    RROW("CRenderer::ScanSectorList_ListModelsVisible", 0x553650)
        return RCase(r, d, [&]() -> uint64_t { CRenderer::ScanSectorList_ListModelsVisible(g_secX, g_secY); return 0; }, [&]() -> uint64_t { ExeC<void, int, int>(0x553650, g_secX, g_secY); return 0; }, &BuildScan);
    REND(1500)
}

static void TestW3RenderPostEffects() {
    using namespace w2; using namespace w3r;
    CPed* const ped = reinterpret_cast<CPed*>(S(kPed));
    RROW("CPostEffects::FilterFX_StoreAndSetDayNightBalance", 0x7034B0)
        return RCase(r, d, [&]() -> uint64_t { CPostEffects::FilterFX_StoreAndSetDayNightBalance(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x7034B0); return 0; });
    REND(200)
    RROW("CPostEffects::FilterFX_RestoreDayNightBalance", 0x7034D0)
        return RCase(r, d, [&]() -> uint64_t { CPostEffects::FilterFX_RestoreDayNightBalance(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x7034D0); return 0; });
    REND(200)
    RROW("CPostEffects::ScriptCCTVSwitch", 0x7011B0)
        const uint32_t v = r.below(2);   // the exe stores the argument byte as is (m_bCCTV = al), the port stores a normalised bool: only canonical 0 / 1 bools are ever passed (the script handler passes a bool)
        return RCase(r, d, [&]() -> uint64_t { CPostEffects::ScriptCCTVSwitch(v != 0); return 0; }, [&]() -> uint64_t { ExeC<void, uint32_t>(0x7011B0, v); return 0; });
    REND(100)
    RROW("CPostEffects::ScriptResetForEffects", 0x7010F0)
        return RCase(r, d, [&]() -> uint64_t { CPostEffects::ScriptResetForEffects(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x7010F0); return 0; });
    REND(100)
    RROW("CPostEffects::NightVisionSetLights", 0x7012E0)
        return RCase(r, d, [&]() -> uint64_t { CPostEffects::NightVisionSetLights(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x7012E0); return 0; });
    REND(300)
    RROW("CPostEffects::InfraredVisionSetLightsForDefaultObjects", 0x701430)
        return RCase(r, d, [&]() -> uint64_t { CPostEffects::InfraredVisionSetLightsForDefaultObjects(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x701430); return 0; });
    REND(300)
    RROW("CPostEffects::InfraredVisionRestoreLightsForHeatObjects", 0x701410)
        return RCase(r, d, [&]() -> uint64_t { CPostEffects::InfraredVisionRestoreLightsForHeatObjects(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x701410); return 0; });
    REND(300)
    RROW("CPostEffects::InfraredVisionStoreAndSetLightsForHeatObjects", 0x701320)
        return RCase(r, d, [&]() -> uint64_t { CPostEffects::InfraredVisionStoreAndSetLightsForHeatObjects(ped); return 0; }, [&]() -> uint64_t { ExeC<void, CPed*>(0x701320, ped); return 0; },
            [](Rng& rr) {
                BuildR(rr);
                CPed* p = reinterpret_cast<CPed*>(S(kPed));
                p->m_nPedState = rr.below(2) ? PEDSTATE_DEAD : (ePedState)rr.below(60);
                p->m_nDeathTimeMS = rr.below(3) ? (int32)(CTimer::m_snTimeInMilliseconds - rr.below(60000)) : (int32)rr.u32();
                CPostEffects::m_bInfraredVision = rr.below(4) != 0; CPostEffects::m_bInCutscene = rr.below(4) == 0;
            });
    REND(1500)
    RROW("CPostEffects::SetSpeedFXManualSpeedCurrentFrame", 0x700BE0)
        const float v = Fe(r, 1.5f, 4);
        return RCase(r, d, [&]() -> uint64_t { CPostEffects::SetSpeedFXManualSpeedCurrentFrame(v); return 0; }, [&]() -> uint64_t { ExeC<void, float>(0x700BE0, v); return 0; });
    REND(1000)
}

static void TestW3RenderGrass() {
    using namespace w2; using namespace w3r;
    RpAtomic** const tabArg = reinterpret_cast<RpAtomic**>(S(kV + 0x40));
    CVector* const v1 = reinterpret_cast<CVector*>(S(kV)); CVector* const v2 = reinterpret_cast<CVector*>(S(kV + 0x10)); CVector* const v3 = reinterpret_cast<CVector*>(S(kV + 0x20)); CVector* const out = reinterpret_cast<CVector*>(S(kV + 0x30));
    const auto gb = [](Rng& rr) { BuildR(rr); for (size_t k = 0; k < 0x30; k += 4) SAt<float>(kV + k) = Fe(rr, 3000.f, 10); };
    RROW("CGrassRenderer::Initialise", 0x5DD6B0)
        return RCase(r, d, [&]() -> uint64_t { return (uint8_t)CGrassRenderer::Initialise(); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t>(0x5DD6B0); });
    REND(50)
    RROW("CGrassRenderer::Shutdown", 0x5DABA0)
        return RCase(r, d, [&]() -> uint64_t { CGrassRenderer::Shutdown(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5DABA0); return 0; });
    REND(50)
    RROW("CGrassRenderer::GetPlantModelsTab", 0x5DACE0)
        const uint32_t t = r.below(3) ? r.below(6) : r.u32();
        return RCase(r, d, [&]() -> uint64_t { return U(CGrassRenderer::GetPlantModelsTab(t)); }, [&]() -> uint64_t { return U(ExeC<void*, uint32_t>(0x5DACE0, t)); },
            [](Rng& rr) { BuildR(rr); for (auto& p : gTriPlantBuf.m_pPlantModelsTab) p = reinterpret_cast<RpAtomic**>((uintptr_t)rr.u32()); });
    REND(400)
    RROW("CGrassRenderer::SetPlantModelsTab", 0x5DACC0)
        const uint32_t t = r.below(3) ? r.below(6) : r.u32();
        return RCase(r, d, [&]() -> uint64_t { CGrassRenderer::SetPlantModelsTab(t, tabArg); return 0; }, [&]() -> uint64_t { ExeC<void, uint32_t, RpAtomic**>(0x5DACC0, t, tabArg); return 0; });
    REND(400)
    RROW("CGrassRenderer::SetCloseFarAlphaDist", 0x5DABE0)
        const float a = Fe(r, 200.f, 6), b = Fe(r, 200.f, 6);
        return RCase(r, d, [&]() -> uint64_t { CGrassRenderer::SetCloseFarAlphaDist(a, b); return 0; }, [&]() -> uint64_t { ExeC<void, float, float>(0x5DABE0, a, b); return 0; });
    REND(300)
    RROW("CGrassRenderer::SetCurrentScanCode", 0x5DABB0)
        const uint32_t c = r.below(65536);
        return RCase(r, d, [&]() -> uint64_t { CGrassRenderer::SetCurrentScanCode((uint16)c); return 0; }, [&]() -> uint64_t { ExeC<void, uint32_t>(0x5DABB0, c); return 0; });
    REND(200)
    RROW("CGrassRenderer::SetGlobalCameraPos", 0x5DABC0)
        return RCase(r, d, [&]() -> uint64_t { CGrassRenderer::SetGlobalCameraPos(*v1); return 0; }, [&]() -> uint64_t { ExeC<void, CVector*>(0x5DABC0, v1); return 0; }, gb);
    REND(300)
    RROW("CGrassRenderer::SetGlobalWindBending", 0x5DAC00)
        const float f = Fe(r, 2.f, 6);
        return RCase(r, d, [&]() -> uint64_t { CGrassRenderer::SetGlobalWindBending(f); return 0; }, [&]() -> uint64_t { ExeC<void, float>(0x5DAC00, f); return 0; });
    REND(300)
    RROW("CGrassRenderer::GenPointInTriangle", 0x5DAB00)
        const float a = r.below(8) ? r.f01() : Fe(r, 1.f, 1), b = r.below(8) ? r.f01() : Fe(r, 1.f, 1);
        // the exe function is the inlined helper: void, eax is left holding v1 (the port returns the out vector): the return value is not compared
        return RCase(r, d, [&]() -> uint64_t { CGrassRenderer::GenPointInTriangle(*out, *v1, *v2, *v3, a, b); return 0; },
                           [&]() -> uint64_t { ExeC<void, CVector*, CVector*, CVector*, CVector*, float, float>(0x5DAB00, out, v1, v2, v3, a, b); return 0; }, gb);
    REND(2000)
}

static void TestW3RenderTwoD() {
    using namespace w2; using namespace w3r;
    RpGeometry* const geo = reinterpret_cast<RpGeometry*>(S(kGeo));
    RpGeometry* const geo2 = reinterpret_cast<RpGeometry*>(S(kGeo + 0x80));
    const auto gb = [](Rng& rr) {
        BuildR(rr);
        C2dEffect::g2dEffectPluginOffset = 0x10 + 4 * rr.below(0x1C);
        SAt<uint32_t>(kGeo + C2dEffect::g2dEffectPluginOffset) = rr.below(4) ? (uint32_t)(uintptr_t)S(kFx) : 0u;
        SAt<uint32_t>(kFx) = rr.below(3) ? rr.below(5) : rr.u32();
    };
    RROW("RpGeometryGet2dFxCount", 0x4C4340)
        return RCase(r, d, [&]() -> uint64_t { return RpGeometryGet2dFxCount(geo); }, [&]() -> uint64_t { return ExeC<uint32_t, RpGeometry*>(0x4C4340, geo); }, gb);
    REND(300)
    RROW("RpGeometryGet2dFxAtIndex", 0x4C4A40)
        const int32_t idx = r.below(3) ? (int32_t)r.below(4) : (int32_t)r.u32();
        return RCase(r, d, [&]() -> uint64_t { return U(RpGeometryGet2dFxAtIndex(geo, idx)); }, [&]() -> uint64_t { return U(ExeC<void*, RpGeometry*, int32_t>(0x4C4A40, geo, idx)); },
            [](Rng& rr) { BuildR(rr); C2dEffect::g2dEffectPluginOffset = 0x10 + 4 * rr.below(0x1C); SAt<uint32_t>(kGeo + C2dEffect::g2dEffectPluginOffset) = (uint32_t)(uintptr_t)S(kFx); });
    REND(600)
    RROW("t2dEffectPluginConstructor", 0x6F9F90)
        const int off = (int)r.below(0x100), sz = (int)r.below(64);
        return RCase(r, d, [&]() -> uint64_t { return U(t2dEffectPluginConstructor(geo, off, sz)); }, [&]() -> uint64_t { return U(ExeC<void*, void*, int, int>(0x6F9F90, geo, off, sz)); }, gb);
    REND(300)
    RROW("t2dEffectPluginCopyConstructor", 0x6F9FB0)
        const int off = (int)r.below(0x100), sz = (int)r.below(64);
        return RCase(r, d, [&]() -> uint64_t { return U(t2dEffectPluginCopyConstructor(geo, geo2, off, sz)); }, [&]() -> uint64_t { return U(ExeC<void*, void*, const void*, int, int>(0x6F9FB0, geo, geo2, off, sz)); }, gb);
    REND(300)
    RROW("Rwt2dEffectPluginDataChunkWriteCallBack", 0x6FA620)
        const uint32_t stream = r.u32(), len = r.u32(); const int off = (int)r.u32(), sz = (int)r.u32();
        return RCase(r, d, [&]() -> uint64_t { return U(Rwt2dEffectPluginDataChunkWriteCallBack((RwStream*)(uintptr_t)stream, (RwInt32)len, geo, off, sz)); },
                           [&]() -> uint64_t { return U(ExeC<void*, uint32_t, uint32_t, void*, int, int>(0x6FA620, stream, len, geo, off, sz)); });
    REND(100)
    RROW("Rwt2dEffectPluginDataChunkGetSizeCallBack", 0x6FA630)
        const int off = (int)r.u32(), sz = (int)r.u32();
        return RCase(r, d, [&]() -> uint64_t { return (uint32_t)Rwt2dEffectPluginDataChunkGetSizeCallBack(geo, off, sz); }, [&]() -> uint64_t { return ExeC<uint32_t, void*, int, int>(0x6FA630, geo, off, sz); });
    REND(100)
    const auto flagsOf = [](uint32_t v) { CRoadsignAttrFlags f; std::memcpy(&f, &v, 2); return f; };
    RROW("C2dEffect::Roadsign_GetNumLinesFromFlags", 0x6FA640)
        const uint32_t v = r.below(65536);
        return RCase(r, d, [&]() -> uint64_t { return (uint32_t)C2dEffect::Roadsign_GetNumLinesFromFlags(flagsOf(v)); }, [&]() -> uint64_t { return ExeC<uint32_t, uint32_t>(0x6FA640, v); });
    REND(400)
    RROW("C2dEffect::Roadsign_GetNumLettersFromFlags", 0x6FA670)
        const uint32_t v = r.below(65536);
        return RCase(r, d, [&]() -> uint64_t { return (uint32_t)C2dEffect::Roadsign_GetNumLettersFromFlags(flagsOf(v)); }, [&]() -> uint64_t { return ExeC<uint32_t, uint32_t>(0x6FA670, v); });
    REND(400)
    RROW("C2dEffect::Roadsign_GetPaletteIDFromFlags", 0x6FA6A0)
        const uint32_t v = r.below(65536);
        return RCase(r, d, [&]() -> uint64_t { return (uint32_t)C2dEffect::Roadsign_GetPaletteIDFromFlags(flagsOf(v)); }, [&]() -> uint64_t { return ExeC<uint32_t, uint32_t>(0x6FA6A0, v); });
    REND(400)
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// ORACLE-W3-MISC second pass: C-class rows of Renderer.cpp that only need a fake vtable / recording stubs: ShouldModelBeStreamed + the two model-request scan functions (fake model infos with a
// GetTimeInfo stub, col models, draw distances), RenderOneRoad / PreRender (entity vtable stubs that record the call order), ProcessLodRenderLists (LOD render list entries).
// The recording stubs (CStreaming::RequestModel 0x4087E0, CHeli::SpecialHeliPreRender 0x6C4750, CShadows::RenderExtraPlayerShadows 0x707FA0) are patched into the exe and defined for the port.
namespace w3r {
constexpr size_t kVt2 = 0x3000 /*0x80*/, kCm2 = 0x3100 /*4 x 0x40*/, kTi2 = 0x3200 /*4 x 4*/, kPos2 = 0x3240 /*0x10*/, kAl = 0x3300 /*8 x 0x20*/, kRec2 = 0x3500 /*0x100*/;
static void Rec2(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t& n = SAt<uint32_t>(kRec2);
    if (n < 20) { uint32_t* p = &SAt<uint32_t>(kRec2 + 4 + n * 12); p[0] = a; p[1] = b; p[2] = c; }
    ++n;
}
static void __cdecl StubReqModel(int32_t model, int32_t flags) { Rec2(1, (uint32_t)model, (uint32_t)flags); }
static void __cdecl StubHeli() { Rec2(2, 0, 0); }
static void __cdecl StubShadows() { Rec2(3, 0, 0); }
static void* __fastcall FakeTimeInfo(void* self, void*) { return reinterpret_cast<void*>((uintptr_t)*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(self) + 0x30)); }
static void __fastcall FakePreRender(void* self, void*) { Rec2(4, (uint32_t)(uintptr_t)self, 0); }
static void __fastcall FakeRender(void* self, void*) { Rec2(5, (uint32_t)(uintptr_t)self, 0); }
static void InstallStubs2() {
    static bool done = false; if (done) return; done = true;
    oracle::Patch(0x4087E0, (void*)&StubReqModel);
    oracle::Patch(0x6C4750, (void*)&StubHeli);
    oracle::Patch(0x707FA0, (void*)&StubShadows);
}
// the fixture: model infos 0..3 with a vtable (slot 5 = GetTimeInfo), col models, draw distances, time infos; entities with a vtable (PreRender slot 0x44, Render slot 0x48)
static void BuildMi(Rng& rr) {
    for (int i = 0; i < 0x80 / 4; ++i) SAt<uint32_t>(kVt2 + 4 * i) = 0;
    SAt<uint32_t>(kVt2 + 4 * 5) = (uint32_t)(uintptr_t)&FakeTimeInfo;
    SAt<uint32_t>(kVt2 + 0x44) = (uint32_t)(uintptr_t)&FakePreRender;
    SAt<uint32_t>(kVt2 + 0x48) = (uint32_t)(uintptr_t)&FakeRender;
    for (int k = 0; k < 4; ++k) {
        SAt<uint32_t>(kMi + 0x80 * k) = (uint32_t)(uintptr_t)S(kVt2);
        SAt<uint32_t>(kMi + 0x80 * k + 0x14) = (uint32_t)(uintptr_t)S(kCm2 + 0x40 * k);
        SAt<float>(kMi + 0x80 * k + 0x18) = rr.below(10) ? 20.f + rr.f01() * 600.f : Fe(rr, 800.f, 1);
        SAt<float>(kCm2 + 0x40 * k + 0x24) = rr.below(10) ? rr.f01() * 80.f : Fe(rr, 100.f, 1);
        SAt<uint8_t>(kTi2 + 4 * k) = (uint8_t)(rr.below(8) ? rr.below(24) : rr.u32()); SAt<uint8_t>(kTi2 + 4 * k + 1) = (uint8_t)(rr.below(8) ? rr.below(24) : rr.u32());
        SAt<uint32_t>(kMi + 0x80 * k + 0x30) = rr.below(3) ? (uint32_t)(uintptr_t)S(kTi2 + 4 * k) : 0u;
    }
    for (int i = 0; i < kNEnt; ++i) SAt<uint32_t>(kEnt + 0x200 * i) = (uint32_t)(uintptr_t)S(kVt2);
    CClock::ms_nGameClockHours = (uint8)rr.below(24); CClock::ms_nGameClockMinutes = (uint8)rr.below(60);
    StaticRef<float>(0xB6F118) = rr.below(8) ? 0.5f + rr.f01() * 1.5f : Fe(rr, 3.f, 1);          // TheCamera.m_fLODDistMult
    SAt<uint32_t>(kRec2) = 0;
}
// entity positions around the camera (some exactly on the streaming / draw distance boundary of their model)
static void BuildStream(Rng& rr) {
    BuildScan(rr);
    BuildMi(rr);
    float cam[3]; for (float& f : cam) f = Fg(rr, 2500.f);
    std::memcpy(&StaticRef<CVector>(0xB76870), cam, 12);                                  // CRenderer::ms_vecCameraPosition
    StaticRef<float>(0xB76848) = rr.below(8) ? 100.f + rr.f01() * 900.f : Fe(rr, 1200.f, 1);   // ms_fFarClipPlane
    StaticRef<uint32_t>(0xB745C4) = rr.below(2) ? 0x1B : rr.u32();                          // gnRendererModelRequestFlags
    for (int i = -1; i < kNEnt; ++i) {
        float* p = i < 0 ? reinterpret_cast<float*>(S(kMat + 0x30)) : reinterpret_cast<float*>(S(kEnt + 0x200 * i + 4));
        const float len = rr.below(3) == 0 ? 0.f : rr.f01() * 700.f;
        const float a = rr.f01() * 6.2831853f, b = rr.f01() * 3.14159f;
        p[0] = cam[0] + len * std::sin(b) * std::cos(a); p[1] = cam[1] + len * std::sin(b) * std::sin(a); p[2] = cam[2] + len * std::cos(b);
        if (rr.below(10) == 0) { for (int k = 0; k < 3; ++k) p[k] = GenF(rr, 3000.f, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF); }   // no 3e38: the exe squares at 24-bit mantissa but a 15-bit exponent (no overflow), a float square would
        if (rr.below(4) == 0) {   // on the boundary of the streaming range of model `m` (or of the draw distance)
            const int m = (int)SAt<uint16_t>(kEnt + 0x200 * (i < 0 ? 0 : i) + 0x22) & 3;
            const bool farR = rr.below(2);
            const double tgt = farR ? (double)SAt<float>(kCm2 + 0x40 * m + 0x24) + (double)StaticRef<float>(0xB76848) : (double)StaticRef<float>(0xB6F118) * (double)SAt<float>(kMi + 0x80 * m + 0x18);
            const double sc = tgt * (1.0 + (double)((int)rr.below(7) - 3) * 6.0e-8);
            const float dx = (float)sc;
            p[0] = cam[0] + dx; p[1] = cam[1]; p[2] = cam[2];
        }
    }
}
} // namespace w3r
// CStreaming::RequestModel is also linked from Streaming.cpp: this definition wins (/FORCE:MULTIPLE, first definition)
void CStreaming::RequestModel(int32 modelId, int32 flags) { w3r::StubReqModel(modelId, flags); }
void CHeli::SpecialHeliPreRender() { w3r::StubHeli(); }
void CShadows::RenderExtraPlayerShadows() { w3r::StubShadows(); }

static void TestW3RenderRenderer2() {
    using namespace w2; using namespace w3r;
    InstallStubs2();
    CEntity* const en = Ent(1);
    RROW("CRenderer::ShouldModelBeStreamed", 0x554EB0)
        const int e = (int)r.below(kNEnt);
        const bool useCam = r.below(4) != 0;
        const float farClip = Fe(r, 1200.f, 12);
        return RCase(r, d, [&]() -> uint64_t { return (uint8_t)CRenderer::ShouldModelBeStreamed(Ent(e), useCam ? StaticRef<CVector>(0xB76870) : *reinterpret_cast<CVector*>(S(kPos2)), useCam ? StaticRef<float>(0xB76848) : farClip); },
                           [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, CEntity*, const void*, float>(0x554EB0, Ent(e), useCam ? (const void*)0xB76870 : (const void*)S(kPos2), useCam ? StaticRef<float>(0xB76848) : farClip); },
            [](Rng& rr) { BuildStream(rr); for (int k = 0; k < 3; ++k) SAt<float>(kPos2 + 4 * k) = Fg(rr, 2500.f); });
    REND(5000)
    RROW("CRenderer::ScanSectorList_RequestModels (+ ScanPtrList_RequestModels<CPtrListSingleLink<CPhysical*>> 0x555680 on its three lists)", 0x555900)
        const int dx = r.below(8) ? 0 : (int)r.below(300) - 150, dy = r.below(8) ? 0 : (int)r.below(300) - 150;
        return RCase(r, d, [&]() -> uint64_t { CRenderer::ScanSectorList_RequestModels(g_secX + dx, g_secY + dy); return 0; }, [&]() -> uint64_t { ExeC<void, int, int>(0x555900, g_secX + dx, g_secY + dy); return 0; }, &BuildStream);
    REND(4000)
    RROW("CRenderer::RenderOneRoad", 0x553230)
        return RCase(r, d, [&]() -> uint64_t { CRenderer::RenderOneRoad(en); return 0; }, [&]() -> uint64_t { ExeC<void, CEntity*>(0x553230, en); return 0; },
            [](Rng& rr) { BuildR(rr); BuildMi(rr); });
    REND(600)
    // PreRender: the four entity pointer arrays, the two alpha lists (nodes whose callback is CVisibilityPlugins::RenderEntity get the entity's offscreen bit cleared and PreRender called), CHeli / CShadows stubs
    static const auto BuildPre = [](Rng& rr) {
        BuildR(rr); BuildMi(rr);
        const auto fill = [&](uint32_t addr, uint32_t countAddr) {
            const int n = (int)rr.below(5); StaticRef<int32_t>(countAddr) = n;
            for (int j = 0; j < n; ++j) StaticRef<uint32_t>(addr + 4 * j) = (uint32_t)(uintptr_t)Ent((int)rr.below(kNEnt));
        };
        fill(0xB748F8, 0xB76840); fill(0xB75898, 0xB76844); fill(0xB74830, 0xB76838); fill(0xB745D8, 0xB7683C);
        for (int l = 0; l < 2; ++l) {   // alpha lists: head.next at 0xC88130 / 0xC88188, tail node at 0xC88134 / 0xC8818C
            const uint32_t tail = l == 0 ? 0xC88134 : 0xC8818C, headNext = l == 0 ? 0xC88130 : 0xC88188;
            const int n = (int)rr.below(4); uint32_t next = tail;
            for (int j = n - 1; j >= 0; --j) {
                uint8_t* node = S(kAl + 0x80 * l + 0x20 * j);
                SAt<uint32_t>(kAl + 0x80 * l + 0x20 * j) = (uint32_t)(uintptr_t)Ent((int)rr.below(kNEnt));
                SAt<uint32_t>(kAl + 0x80 * l + 0x20 * j + 4) = rr.below(3) ? 0x732B40u : rr.u32();   // canonical marker of "RenderEntity" (patched per side below)
                SAt<uint32_t>(kAl + 0x80 * l + 0x20 * j + 0x10) = next;
                next = (uint32_t)(uintptr_t)node;
            }
            StaticRef<uint32_t>(headNext) = next;
        }
    };
    const auto patchCb = [](uint32_t from, uint32_t val) {   // `from` -> `val` in the fixture alpha nodes
        for (int l = 0; l < 2; ++l) for (int k = 0; k < 4; ++k) if (SAt<uint32_t>(kAl + 0x80 * l + 0x20 * k + 4) == from) SAt<uint32_t>(kAl + 0x80 * l + 0x20 * k + 4) = val;
    };
    RROW("CRenderer::PreRender", 0x553910)
        return RCase(r, d, [&]() -> uint64_t { const uint32_t mine = (uint32_t)(uintptr_t)&CVisibilityPlugins::RenderEntity; patchCb(0x732B40u, mine); CRenderer::PreRender(); patchCb(mine, 0x732B40u); return 0; },
                           [&]() -> uint64_t { ExeC<void>(0x553910); return 0; }, +BuildPre);
    REND(1500)
    // ProcessLodRenderLists: 0..6 entries (entity or null, distance) in the LOD render list; entities with LOD parent / children counters / flags (visible, displayed-super-low-lod, distance fade ...)
    static const auto BuildLod = [](Rng& rr) {
        BuildR(rr); BuildMi(rr);
        for (int i = 0; i < kNEnt; ++i) {
            const size_t o = kEnt + 0x200 * i;
            SAt<uint32_t>(o + 0x1C) = rr.u32() & (rr.below(2) ? 0xFFFFFFFFu : 0x02FFFFFFu);
            if (rr.below(2)) SAt<uint32_t>(o + 0x1C) |= 0x80;
            SAt<uint8_t>(o + 0x34) = (uint8_t)(rr.below(3) ? rr.below(4) : rr.u32());
            SAt<int8_t>(o + 0x35) = (int8_t)(rr.below(3) ? (int)rr.below(4) : (int)rr.u32());
            SAt<uint32_t>(o + 0x30) = rr.below(3) ? (uint32_t)(uintptr_t)Ent((int)rr.below(kNEnt)) : 0u;     // LOD parent
            SAt<uint32_t>(o + 0x18) = rr.below(3) ? (uint32_t)(uintptr_t)S(kOut) : 0u;                      // RW object
        }
        for (int k = 0; k < 4; ++k) SAt<uint8_t>(kMi + 0x80 * k + 0xC) = (uint8_t)(rr.below(2) ? 255 : rr.u32());   // CBaseModelInfo::m_nAlpha
        const int n = (int)rr.below(7);
        tRenderListEntry* base = CRenderer::GetLodRenderListBase();
        for (int j = 0; j < n; ++j) { base[j].entity = rr.below(6) ? Ent((int)rr.below(kNEnt)) : nullptr; base[j].distance = Fe(rr, 500.f, 8); }
        CRenderer::ms_pLodRenderList = base + n;
        CRenderer::ms_nNoOfVisibleEntities = (int32)rr.below(100); CRenderer::ms_nNoOfVisibleLods = (int32)rr.below(100);
    };
    RROW("CRenderer::ProcessLodRenderLists", 0x553770)
        return RCase(r, d, [&]() -> uint64_t { CRenderer::ProcessLodRenderLists(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x553770); return 0; }, +BuildLod);
    REND(4000)
}

static void TestW3RenderAll() {
    using namespace w3r;
    (void)spdlog::default_logger_raw();
    InstallStubs();
    TestW3RenderRenderer();
    TestW3RenderRenderer2();
    TestW3RenderPostEffects();
    TestW3RenderGrass();
    TestW3RenderTwoD();
}
