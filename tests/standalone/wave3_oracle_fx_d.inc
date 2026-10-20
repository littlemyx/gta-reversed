// ORACLE-W3-EFFECTS rows D: Clouds (moving fog, volumetric clouds, sky poly), PlantMgr, PlantSurfPropMgr, PPTriPlantBuffer (see wave3_oracle_fx.inc).
#include "Plant/PlantLocTri.h"
#include "Plant/PlantColEntEntry.h"
#include "PPTriPlantBuffer.h"
#include "TimeCycle.h"
#include "Weather.h"
#include "SurfaceInfos_c.h"
#include "Collision/ColModel.h"
#include "Collision/CollisionData.h"
#include "Models/AtomicModelInfo.h"
struct W3fFindTag { using type = CPlantColEntEntry* (*)(CEntity*); friend type W2Get(W3fFindTag); }; template struct W2Rob<W3fFindTag, &CPlantMgr::_ColEntityCache_FindInCache>;   // private in the port
namespace w3fd {
using namespace w3f; using namespace w2;
static void PutFog(Rng& r) {
    FillGarbage(r, &CClouds::ms_mf, sizeof(CClouds::ms_mf));
    const int mode = (int)r.below(4);   // 0: random bools, 1: runs of used slots, 2: all used, 3: all free
    for (int i = 0; i < MAX_MOVING_FOG; ++i) CClouds::ms_mf.m_bFogSlots[i] = mode == 0 ? (r.below(2) != 0) : mode == 1 ? (i < (int)r.below(MAX_MOVING_FOG + 1) || r.below(40) == 0) : mode == 2;
    if (mode == 1) { const int n = (int)r.below(MAX_MOVING_FOG + 1); for (int i = 0; i < MAX_MOVING_FOG; ++i) CClouds::ms_mf.m_bFogSlots[i] = i < n; }
}
static void PutVc(Rng& r) {
    FillGarbage(r, &CClouds::ms_vc, sizeof(CClouds::ms_vc));
    for (int i = 0; i < MAX_VOLUMETRIC_CLOUDS; ++i) { CClouds::ms_vc.bUsed[i] = r.below(2); CClouds::ms_vc.bJustCreated[i] = r.below(2); }
    CClouds::m_VolumetricCloudsUsedNum = r.below(MAX_VOLUMETRIC_CLOUDS + 1);
}
// plant lists: nodes of `stride` bytes in the scratch block form two doubly linked lists (the old list holding `triangle`)
template<class N, size_t NextOff, size_t PrevOff>
static void PutLists(Rng& r, size_t base, size_t stride, int count, N*& oldHead, N*& newHead, N*& pick) {
    auto node = [&](int i) { return reinterpret_cast<N*>(S(base + (size_t)i * stride)); };
    auto nxt = [&](N* n) -> N*& { return *reinterpret_cast<N**>(reinterpret_cast<uint8_t*>(n) + NextOff); };
    auto prv = [&](N* n) -> N*& { return *reinterpret_cast<N**>(reinterpret_cast<uint8_t*>(n) + PrevOff); };
    std::vector<int> order(count); for (int i = 0; i < count; ++i) order[i] = i;
    for (int i = count - 1; i > 0; --i) std::swap(order[i], order[r.below(i + 1)]);
    const int nOld = 1 + (int)r.below(count - 1);   // [0, nOld) old list, the rest new list
    oldHead = newHead = nullptr; N* last = nullptr;
    for (int k = 0; k < nOld; ++k) { N* n = node(order[k]); prv(n) = last; nxt(n) = nullptr; if (last) nxt(last) = n; else oldHead = n; last = n; }
    last = nullptr;
    for (int k = nOld; k < count; ++k) { N* n = node(order[k]); prv(n) = last; nxt(n) = nullptr; if (last) nxt(last) = n; else newHead = n; last = n; }
    pick = node(order[r.below(nOld)]);
}
} // namespace w3fd

static void TestW3FxD() {
    using namespace w2; using namespace w3f; using namespace w3fd;
    // ---- Clouds ----
    ROW("CClouds::MovingFogInit", 0x713660, 100) {
        Tu tu([](Rng& rr) { PutFog(rr); });
        return UCase(r, d, [&]() -> uint64_t { CClouds::MovingFogInit(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x713660)(); return 0; });
    } ENDROW;
    ROW("CClouds::MovingFog_Delete", 0x713730, 1500) {
        const int idx = r.below(4) ? (int)r.below(MAX_MOVING_FOG) : (int)r.below(MAX_MOVING_FOG + 40) - 20;
        Tu tu([](Rng& rr) { PutFog(rr); });
        return UCase(r, d, [&]() -> uint64_t { CClouds::MovingFog_Delete(idx); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x713730)(idx); return 0; });
    } ENDROW;
    ROW("CClouds::MovingFog_GetFirstFreeSlot", 0x713710, 1500) {
        Tu tu([](Rng& rr) { PutFog(rr); });
        return UCase(r, d, [&]() -> uint64_t { return (uint32_t)CClouds::MovingFog_GetFirstFreeSlot(); }, [&]() -> uint64_t { return (uint32_t)oracle::Fn<int __cdecl()>(0x713710)(); });
    } ENDROW;
    ROW("CClouds::MovingFog_GetFXIntensity", 0x7136D0, 300) {
        Tu tu([](Rng& rr) { CWeather::Foggyness_SF = rr.below(4) == 0 ? GenF(rr, 2.f, S_NAN | S_ZERO | S_DEN | S_INF) : GF(rr, 1.f); });
        return UCase(r, d, [&]() -> uint64_t { return FB(CClouds::MovingFog_GetFXIntensity()); }, [&]() -> uint64_t { return FB(oracle::Fn<float __cdecl()>(0x7136D0)()); });
    } ENDROW;
    ROW("CClouds::MovingFog_GetWind", 0x7136E0, 300) {
        Tu tu([](Rng& rr) { PutFog(rr); });
        return UCase(r, d, [&]() -> uint64_t { SAt<CVector>(0x400) = CClouds::MovingFog_GetWind(); return 0; },
            [&]() -> uint64_t { oracle::Fn<CVector* __cdecl(CVector*)>(0x7136E0)(&SAt<CVector>(0x400)); return 0; });
    } ENDROW;
    ROW("CClouds::VolumetricCloudsInit", 0x7131C0, 100) {
        Tu tu([](Rng& rr) { PutVc(rr); });
        return UCase(r, d, [&]() -> uint64_t { CClouds::VolumetricCloudsInit(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x7131C0)(); return 0; });
    } ENDROW;
    ROW("CClouds::VolumetricClouds_Delete", 0x7135F0, 1500) {
        const int idx = r.below(4) ? (int)r.below(MAX_VOLUMETRIC_CLOUDS) : (int)r.below(MAX_VOLUMETRIC_CLOUDS + 40) - 20;
        Tu tu([](Rng& rr) { PutVc(rr); });
        return UCase(r, d, [&]() -> uint64_t { CClouds::VolumetricClouds_Delete(idx); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x7135F0)(idx); return 0; });
    } ENDROW;
    ROW("CClouds::VolumetricClouds_GetFirstFreeSlot", 0x7135C0, 1500) {
        Tu tu([](Rng& rr) { PutVc(rr); });
        return UCase(r, d, [&]() -> uint64_t { return (uint32_t)CClouds::VolumetricClouds_GetFirstFreeSlot(); }, [&]() -> uint64_t { return (uint32_t)oracle::Fn<int __cdecl()>(0x7135C0)(); });
    } ENDROW;
    ROW("CClouds::SetUpOneSkyPoly", 0x713060, 1500) {
        float v[12]; for (auto& f : v) f = r.below(14) == 0 ? GenF(r, 2000.f, S_NAN | S_ZERO | S_INF) : GF(r, 3000.f);
        uint8_t c[6]; for (auto& b : c) b = (uint8_t)r.u32();
        Tu tu([](Rng& rr) { uiTempBufferVerticesStored = (uint16)(rr.below(3) ? rr.below(1000) : rr.below(1020)); uiTempBufferIndicesStored = (uint16)rr.below(3000); });
        return UCase(r, d, [&]() -> uint64_t { CClouds::SetUpOneSkyPoly(CVector(v[0], v[1], v[2]), CVector(v[3], v[4], v[5]), CVector(v[6], v[7], v[8]), CVector(v[9], v[10], v[11]), c[0], c[1], c[2], c[3], c[4], c[5]); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(float, float, float, float, float, float, float, float, float, float, float, float, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>(0x713060)(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11], c[0], c[1], c[2], c[3], c[4], c[5]); return 0; });
    } ENDROW;
    // ---- PlantMgr ----
    ROW("CPlantMgr::UpdateAmbientColor", 0x5DB310, 1500) {
        Tu tu([](Rng& rr) { auto& cc = CTimeCycle::m_CurrentColours; const int m = (int)rr.below(3);
            auto f = [&]() { return m == 0 ? rr.f01() * 0.4f : m == 1 ? rr.f01() : (rr.below(8) == 0 ? GenF(rr, 3.f, S_NAN | S_ZERO | S_INF | S_DEN) : rr.f01() * 3.f - 0.2f); };
            cc.m_fAmbientRed = f(); cc.m_fAmbientGreen = f(); cc.m_fAmbientBlue = f(); });
        return UCase(r, d, [&]() -> uint64_t { CPlantMgr::UpdateAmbientColor(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x5DB310)(); return 0; });
    } ENDROW;
    ROW("CPlantMgr::_ColEntityCache_FindInCache", 0x5DB530, 1500) {
        Tu tu([](Rng& rr) {
            for (auto& e : CPlantMgr::m_ColEntCacheTab) { e.m_Entity = reinterpret_cast<CEntity*>(S(0x1000 + 0x40 * rr.below(16))); e.m_NextEntry = nullptr; e.m_PrevEntry = nullptr; }
            const int n = (int)rr.below(MAX_NUM_PROC_OBJECTS + 1); CPlantColEntEntry* head = nullptr;
            for (int i = n - 1; i >= 0; --i) { CPlantMgr::m_ColEntCacheTab[i].m_NextEntry = head; head = &CPlantMgr::m_ColEntCacheTab[i]; }
            CPlantMgr::m_CloseColEntListHead = n ? head : nullptr; });
        auto* ent = reinterpret_cast<CEntity*>(S(0x1000 + 0x40 * r.below(20)));
        return UCase(r, d, [&]() -> uint64_t { return U(W2Get(W3fFindTag{})(ent)); }, [&]() -> uint64_t { return U(oracle::Fn<CPlantColEntEntry* __cdecl(CEntity*)>(0x5DB530)(ent)); });
    } ENDROW;
    ROW("CPlantMgr::MoveLocTriToList", 0x5DB590, 2500) {
        CPlantLocTri** oldH = &SAt<CPlantLocTri*>(0x100); CPlantLocTri** newH = &SAt<CPlantLocTri*>(0x104); CPlantLocTri* pick = nullptr;
        Tu tu([&](Rng& rr) { PutLists<CPlantLocTri, offsetof(CPlantLocTri, m_NextTri), offsetof(CPlantLocTri, m_PrevTri)>(rr, 0x1000, 0x60, 2 + (int)rr.below(7), *oldH, *newH, pick); });
        return UCase(r, d, [&]() -> uint64_t { CPlantMgr::MoveLocTriToList(*oldH, *newH, pick); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(CPlantLocTri**, CPlantLocTri**, CPlantLocTri*)>(0x5DB590)(oldH, newH, pick); return 0; });
    } ENDROW;
    ROW("CPlantMgr::MoveColEntToList", 0x5DB5F0, 2500) {
        CPlantColEntEntry** oldH = &SAt<CPlantColEntEntry*>(0x100); CPlantColEntEntry** newH = &SAt<CPlantColEntEntry*>(0x104); CPlantColEntEntry* pick = nullptr;
        Tu tu([&](Rng& rr) { PutLists<CPlantColEntEntry, offsetof(CPlantColEntEntry, m_NextEntry), offsetof(CPlantColEntEntry, m_PrevEntry)>(rr, 0x1000, 0x20, 2 + (int)rr.below(7), *oldH, *newH, pick); });
        return UCase(r, d, [&]() -> uint64_t { CPlantMgr::MoveColEntToList(*oldH, *newH, pick); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(CPlantColEntEntry**, CPlantColEntEntry**, CPlantColEntEntry*)>(0x5DB5F0)(oldH, newH, pick); return 0; });
    } ENDROW;
    ROW("CPlantMgr::SetPlantFriendlyFlagInAtomicMI", 0x5DB650, 2500) {
        const int nTri = r.below(6) == 0 ? (int)r.below(2) - 1 : (int)r.below(12);   // 0 triangles, -1 (uint16 65535 is not used: the count is a uint16), up to 11
        Tu tu([nTri](Rng& rr) {
            auto* ami = reinterpret_cast<CAtomicModelInfo*>(S(0x1000)); auto* cm = reinterpret_cast<CColModel*>(S(0x1200)); auto* cd = reinterpret_cast<CCollisionData*>(S(0x1400)); auto* tris = reinterpret_cast<CColTriangle*>(S(0x1600));
            *reinterpret_cast<CColModel**>(S(0x1000 + 20)) = cm; cm->m_pColData = rr.below(8) == 0 ? nullptr : cd; cd->m_nNumTriangles = (uint16)(nTri < 0 ? 0 : nTri); cd->m_pTriangles = tris;
            for (int i = 0; i < 12; ++i) *reinterpret_cast<uint8_t*>(S(0x1600 + i * 8 + 6)) = (uint8_t)(rr.below(6) == 0 ? rr.below(178) : rr.below(10));
            for (auto& s : g_surfaceInfos.m_surfaces) { std::memset(&s, 0, sizeof(s)); }
            for (int k = 0; k < 10; ++k) { auto& s = g_surfaceInfos.m_surfaces[k]; s.bCreatesPlants = rr.below(8) == 0; s.bCreatesObjects = rr.below(8) == 0; } });
        return UCase(r, d, [&]() -> uint64_t { CPlantMgr::SetPlantFriendlyFlagInAtomicMI(reinterpret_cast<CAtomicModelInfo*>(S(0x1000))); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(CAtomicModelInfo*)>(0x5DB650)(reinterpret_cast<CAtomicModelInfo*>(S(0x1000))); return 0; });
    } ENDROW;
    // ---- PPTriPlantBuffer ----
    ROW("CPPTriPlantBuffer::GetPlantModelsTab", 0x5DACA0, 600) {
        const uint32_t idx = r.below(3) ? r.below(4) : (r.below(2) ? r.below(10) : r.u32());
        return UCase(r, d, [&]() -> uint64_t { return U(reinterpret_cast<CPPTriPlantBuffer*>(S(0x1000))->GetPlantModelsTab(idx)); },
            [&]() -> uint64_t { return U(oracle::Fn<RpAtomic** __fastcall(CPPTriPlantBuffer*, int, uint32_t)>(0x5DACA0)(reinterpret_cast<CPPTriPlantBuffer*>(S(0x1000)), 0, idx)); });
    } ENDROW;
    ROW("CPPTriPlantBuffer::SetPlantModelsTab", 0x5DAC80, 600) {
        const uint32_t idx = r.below(3) ? r.below(4) : (r.below(2) ? r.below(10) : r.u32()); RpAtomic** pm = reinterpret_cast<RpAtomic**>(S(0x8000 + 8 * r.below(8)));
        return UCase(r, d, [&]() -> uint64_t { reinterpret_cast<CPPTriPlantBuffer*>(S(0x1000))->SetPlantModelsTab(idx, pm); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(CPPTriPlantBuffer*, int, uint32_t, RpAtomic**)>(0x5DAC80)(reinterpret_cast<CPPTriPlantBuffer*>(S(0x1000)), 0, idx, pm); return 0; });
    } ENDROW;
    // ---- PlantSurfPropMgr ----
    ROW("CPlantSurfPropMgr::AllocSurfProperties", 0x5DD370, 2000) {
        const uint16_t sid = (uint16_t)(r.below(8) ? r.below(178) : r.below(178)); const bool clr = r.below(4) == 0;
        Tu tu([](Rng& rr) { FillGarbage(rr, &CPlantSurfPropMgr::m_SurfPropPtrTab, sizeof(CPlantSurfPropMgr::m_SurfPropPtrTab)); CPlantSurfPropMgr::m_countSurfPropsAllocated = rr.below(4) == 0 ? 56 + rr.below(3) : rr.below(60); });
        return UCase(r, d, [&]() -> uint64_t { return U(CPlantSurfPropMgr::AllocSurfProperties(sid, clr)); }, [&]() -> uint64_t { return U(oracle::Fn<CPlantSurfProp* __cdecl(uint32_t, uint32_t)>(0x5DD370)(sid, clr ? 1 : 0)); });
    } ENDROW;
    ROW("CPlantSurfPropMgr::GetSurfProperties", 0x6F9DE0, 1000) {
        const uint16_t idx = (uint16_t)(r.below(4) ? r.below(178) : r.below(400));
        Tu tu([](Rng& rr) { FillGarbage(rr, &CPlantSurfPropMgr::m_SurfPropPtrTab, sizeof(CPlantSurfPropMgr::m_SurfPropPtrTab)); });
        return UCase(r, d, [&]() -> uint64_t { return U(CPlantSurfPropMgr::GetSurfProperties(idx)); }, [&]() -> uint64_t { return U(oracle::Fn<CPlantSurfProp* __cdecl(uint32_t)>(0x6F9DE0)(idx)); });
    } ENDROW;
}
