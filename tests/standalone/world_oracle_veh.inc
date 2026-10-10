// (d) trains / boats / helis / planes / ropes, see world_oracle_test.cpp
#include "Entity/Vehicle/Train.h"
#include "TrainNode.h"
#include "Models/BaseModelInfo.h"
#include "Models/ModelInfo.h"
#include "Collision/ColModel.h"

// ------------------------------------------------------------------------------------------------------------------------------
// CTrain fixture: 4 tracks of random nodes (statics at 0xC38014..), a fake train object, a fake model info with a collision box
namespace trfx {
static constexpr uintptr_t NUMNODES = 0xC38014, TOTLEN = 0xC37FEC, STATIONS = 0xC38034, PNODES = 0xC38024;
alignas(16) static CTrainNode g_nodes[4][14];
alignas(16) static uint8 g_modelInfo[0x100];
alignas(16) static uint8 g_colModel[0x100];
static CVector NodePos(const CTrainNode& n) { return { n.GetX(), n.GetY(), n.GetZ() }; }
static void Setup(Rng& r) {
    std::memset(g_nodes, 0, sizeof g_nodes);
    for (int tr = 0; tr < 4; ++tr) {
        const int n = r.below(5) ? 3 + r.below(10) : r.below(3);
        *reinterpret_cast<int32_t*>(NUMNODES + 4 * tr) = n;
        *reinterpret_cast<CTrainNode**>(PNODES + 4 * tr) = g_nodes[tr];
        float dist = 0.f; CVector prev{};
        for (int i = 0; i < n; ++i) {
            auto& nd = g_nodes[tr][i];
            nd.x = (int16)(r.below(2) ? (int)(r.u32() % 6000) - 3000 : (int)(r.u32() & 0xFFFF)); nd.y = (int16)(int)(r.u32() % 6000) - 3000; nd.z = (int16)(r.u32() % 800);
            const CVector p = NodePos(nd);
            if (i) dist += (p - prev).Magnitude();
            nd.m_nDistanceFromStart = (uint16)(r.below(4) ? (uint16)(dist * 3.f) : (uint16)r.u32());
            nd.m_bSurfLightingFound = true; nd.m_nSurfaceLighting.value = (uint8)r.u32();   // found => no world probe
            prev = p;
        }
        *reinterpret_cast<float*>(TOTLEN + 4 * tr) = r.below(6) ? dist + r.f01() * 100.f : GenF(r, 5000.f);
    }
    for (int i = 0; i < 6; ++i) *reinterpret_cast<float*>(STATIONS + 4 * i) = r.below(8) ? (float)(i * 500 + r.below(300)) : GenF(r, 3000.f);
}
}   // namespace trfx

static void TestVeh() {
    using TrainFx = Raw<CTrain>;
    Run("CTrain::FindCoorsFromPositionOnTrack 0x6F59E0", [](Rng& r, std::string& d) {
        trfx::Setup(r);
        const int track = r.below(4) + (r.below(32) == 0 ? 0 : 0);
        const float dist = r.below(4) ? r.f01() * 3000.f : GenF(r, PickScale(r));
        CVector a{ 11, 22, 33 }, e{ 11, 22, 33 };
        CTrain::FindCoorsFromPositionOnTrack(dist, track, &a);
        oracle::Fn<void __cdecl(float, int, CVector*)>(0x6F59E0)(dist, track, &e);
        Hit(a.x != 11.f);
        if (SameV(a, e)) return true;
        d = "dist " + F(dist) + " track " + std::to_string(track) + " got " + V(a) + " exe " + V(e); return false;
    });

    Run("CTrain::FindClosestTrackNode 0x6F6BD0", [](Rng& r, std::string& d) {
        trfx::Setup(r);
        const CVector p = r.below(2) ? GenV(r, 3000.f) : GenV(r, PickScale(r));
        int32 ta = 77, te = 77;
        const int32 a = CTrain::FindClosestTrackNode(p, &ta);
        const int32 e = oracle::Fn<int32 __cdecl(float, float, float, int32*)>(0x6F6BD0)(p.x, p.y, p.z, &te);
        if (ta == 77 && te == 77) return true;   // no node closer than 99999.9: the exe returns an uninitialised local
        if (a == e && ta == te) return true;
        d = "p " + V(p) + " got " + std::to_string(a) + "/" + std::to_string(ta) + " exe " + std::to_string(e) + "/" + std::to_string(te); return false;
    });
    Run("CTrain::FindNextStationPositionInDirection 0x6F5F00", [](Rng& r, std::string& d) {
        trfx::Setup(r);
        const bool cw = r.below(2); const float dist = r.below(4) ? r.f01() * 3000.f : GenF(r, PickScale(r));
        float da = 1.f, de = 1.f; int32 na = 9, ne = 9;
        CTrain::FindNextStationPositionInDirection(cw, dist, &da, &na);
        oracle::Fn<void __cdecl(int, float, float*, int32*)>(0x6F5F00)(cw, dist, &de, &ne);
        if (SameF(da, de) && na == ne) return true;
        d = "cw " + std::to_string(cw) + " dist " + F(dist) + " got " + F(da) + "/" + std::to_string(na) + " exe " + F(de) + "/" + std::to_string(ne); return false;
    });
    Run("CTrain::FindMaximumSpeedToStopAtStations 0x6F5BA0", [](Rng& r, std::string& d) {
        trfx::Setup(r);
        TrainFx a; a.Fill(r);
        a->m_nTrackId = r.below(4) ? 0 : (int8)r.below(4); a->trainFlags.bClockwiseDirection = r.below(2);
        a->m_fCurrentRailDistance = r.below(4) ? r.f01() * 3000.f : GenF(r, PickScale(r));
        if (r.below(6) == 0) *reinterpret_cast<float*>(trfx::TOTLEN) = r.below(2) ? 1e-3f : GenF(r, 6000.f);   // the exe's wrap loops spin for tiny lengths: keep it bounded below
        if (*reinterpret_cast<float*>(trfx::TOTLEN) < 50.f) *reinterpret_cast<float*>(trfx::TOTLEN) = 50.f + r.f01() * 4000.f;
        // the wrap loops of the original never end for non-finite / huge values: keep every input finite and moderate
        const auto Sane = [](float& v, float repl) { if (!(std::fabs(v) < 1e6f)) v = repl; };
        Sane(a->m_fCurrentRailDistance, 5.f); for (int i = 0; i < 6; ++i) Sane(*reinterpret_cast<float*>(trfx::STATIONS + 4 * i), 100.f * i); Sane(*reinterpret_cast<float*>(trfx::TOTLEN), 1000.f);
        float sa = 1.f, se = 1.f;
        const bool ra = a->FindMaximumSpeedToStopAtStations(&sa);
        const bool re = oracle::Fn<int __fastcall(CTrain*, int, float*)>(0x6F5BA0)(a.get(), 0, &se) & 0xFF;
        Hit(ra);
        if (ra == re && SameF(sa, se)) return true;
        d = "track " + std::to_string(a->m_nTrackId) + " cw " + std::to_string(a->trainFlags.bClockwiseDirection) + " dist " + F(a->m_fCurrentRailDistance) + " got " + std::to_string(ra) + "/" + F(sa) + " exe " + std::to_string(re) + "/" + F(se); return false;
    });
    Run("CTrain::SetTrainSpeed 0x6F5E20", [](Rng& r, std::string& d) {
        TrainFx a, e; a.Fill(r); std::memcpy(e.b, a.b, sizeof a.b);
        a->trainFlags.bClockwiseDirection = r.below(2); std::memcpy(e.b, a.b, sizeof a.b);
        const float sp = r.below(3) ? GenF(r, 120.f) : GenF(r, PickScale(r));
        CTrain::SetTrainSpeed(a.get(), sp);
        oracle::Fn<void __cdecl(CTrain*, float)>(0x6F5E20)(e.get(), sp);
        if (SameBlob(a.b, e.b, sizeof a.b)) return true;
        d = "speed " + F(sp) + " cw " + std::to_string(a->trainFlags.bClockwiseDirection) + " got " + F(a->m_fTrainSpeed) + " exe " + F(e->m_fTrainSpeed); return false;
    });
    Run("CTrain::SetTrainCruiseSpeed 0x6F5E50", [](Rng& r, std::string& d) {
        TrainFx a, e; a.Fill(r); std::memcpy(e.b, a.b, sizeof a.b);
        const float sp = r.below(3) ? GenF(r, 300.f) : GenF(r, PickScale(r));
        CTrain::SetTrainCruiseSpeed(a.get(), sp);
        oracle::Fn<void __cdecl(CTrain*, float)>(0x6F5E50)(e.get(), sp);
        if (SameBlob(a.b, e.b, sizeof a.b)) return true;
        d = "speed " + F(sp) + " got " + std::to_string(a->m_autoPilot.m_nCruiseSpeed) + " exe " + std::to_string(e->m_autoPilot.m_nCruiseSpeed); return false;
    });
    Run("CTrain::FindEngine/FindCaboose/FindCarriage/FindNumCarriagesPulled 0x6F5E90/70/B0/CD0", [](Rng& r, std::string& d) {
        TrainFx c[5]; for (auto& x : c) x.Fill(r);
        const int len = 1 + r.below(5);
        for (int i = 0; i < 5; ++i) { c[i]->m_pPrevCarriage = (i > 0 && i < len) ? c[i - 1].get() : nullptr; c[i]->m_pNextCarriage = (i + 1 < len) ? c[i + 1].get() : nullptr; }
        CTrain* t = c[r.below(len)].get();
        const uint8 n = (uint8)r.below(7);
        const bool ok = CTrain::FindEngine(t) == oracle::Fn<CTrain* __cdecl(CTrain*)>(0x6F5E90)(t) && CTrain::FindCaboose(t) == oracle::Fn<CTrain* __cdecl(CTrain*)>(0x6F5E70)(t)
            && CTrain::FindCarriage(t, n) == oracle::Fn<CTrain* __cdecl(CTrain*, int)>(0x6F5EB0)(t, n) && t->FindNumCarriagesPulled() == oracle::Fn<uint32 __fastcall(CTrain*, int)>(0x6F5CD0)(t, 0);
        if (ok) return true;
        d = "len " + std::to_string(len); return false;
    });
    Run("CTrain::FindPositionOnTrackFromCoors 0x6F6CC0", [](Rng& r, std::string& d) {
        trfx::Setup(r);
        std::memset(trfx::g_modelInfo, 0, sizeof trfx::g_modelInfo); std::memset(trfx::g_colModel, 0, sizeof trfx::g_colModel);
        reinterpret_cast<CBaseModelInfo*>(trfx::g_modelInfo)->m_pColModel = reinterpret_cast<CColModel*>(trfx::g_colModel);
        auto& bb = reinterpret_cast<CColModel*>(trfx::g_colModel)->m_boundBox; bb.m_vecMin = CVector(-1.5f, -(r.f01() * 12.f), -1.f); bb.m_vecMax = CVector(1.5f, r.f01() * 12.f, 3.f);
        CModelInfo::ms_modelInfoPtrs[537] = reinterpret_cast<CBaseModelInfo*>(trfx::g_modelInfo);
        TrainFx a; a.Fill(r);
        const int track = r.below(4); a->m_nTrackId = (int8)track;
        a->m_nModelIndex = 537;
        a->trainFlags.bClockwiseDirection = r.below(2);
        *reinterpret_cast<uint32_t*>(a.b + 0x14) = 0;   // m_matrix = null: the position is the placement's
        const int nn = *reinterpret_cast<int32_t*>(trfx::NUMNODES + 4 * track);
        CVector pos = GenV(r, 3000.f);
        if (nn > 1) {
            const int i = r.below(nn); const CVector p0 = trfx::NodePos(trfx::g_nodes[track][i]), p1 = trfx::NodePos(trfx::g_nodes[track][(i + 1) % nn]);
            const float tt = r.below(8) ? r.f01() : GenF(r, 1.5f);
            pos = p0 + (p1 - p0) * tt + CVector(r.below(2) ? GenF(r, 2.5f) : GenF(r, 10.f), GenF(r, 2.5f), 0.f);
        }
        std::memcpy(a.b + 4, &pos, 12);
        a->m_vecMoveSpeed = GenV(r, r.below(2) ? 1.f : 3.f);
        a->m_fCurrentRailDistance = r.below(2) ? 0.f : GenF(r, 3000.f);
        a->m_fTrainSpeed = GenF(r, 2.f); a->m_fContactSurfaceBrightness = GenF(r, 1.f);
        TrainFx e; std::memcpy(e.b, a.b, sizeof a.b);
        a->FindPositionOnTrackFromCoors();
        oracle::Fn<void __fastcall(CTrain*, int)>(0x6F6CC0)(e.get(), 0);
        if (SameBlob(a.b, e.b, sizeof a.b)) return true;
        d = "track " + std::to_string(track) + " pos " + V(pos) + DiffDesc(a.b, e.b, sizeof a.b, "t"); return false;
    });
}
