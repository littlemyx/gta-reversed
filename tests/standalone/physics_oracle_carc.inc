// (c) CCarCtrl helicopter AI: GetATanOfXY results kept unrounded on the x87 stack (0x42AF08 -> fadd [pi/2] at 0x42AFED), see physics_oracle_test.cpp
// Only the near path is driven: the far path ends in FlyAIHeliInCertainDirection (line-of-sight probes), SteerAIHeliFlyingAwayFromPlayer asks the real player (FindPlayerCoors) and
// FlyAIPlaneInCertainDirection probes the world for obstacles, which this harness has no fixtures for.
#include "Entity/Vehicle/Heli.h"

static void TestCarc() {
    using namespace cd;
    Run("CCarCtrl::SteerAIHeliToKeepEntityInView 0x42AEB0", [](Rng& r, std::string& d) {
        static VehFx fxT;
        auto* h = MakeVeh(r, GenV(r, r.below(2) ? 100.f : 2900.f, S_ZERO | S_TINY), GenV(r, r.below(2) ? 0.5f : 2.f, S_ZERO | S_TINY), true);
        std::memset(fxT.buf, 0, sizeof fxT.buf); std::memset(fxT.mat, 0, sizeof fxT.mat);
        auto* t = fxT.veh();
        t->m_matrix = reinterpret_cast<CMatrixLink*>(fxT.mat);
        const auto& hp = h->GetPosition();
        t->m_matrix->GetPosition() = CVector{ hp.x + GenF(r, r.below(2) ? 30.f : 300.f, S_ZERO | S_TINY), hp.y + GenF(r, r.below(2) ? 30.f : 300.f, S_ZERO | S_TINY), hp.z + GenF(r, 40.f, S_ZERO | S_TINY) };
        auto& ap = h->m_autoPilot;
        ap.m_TargetEntity = t;
        ap.m_ucHeliTargetDist = (uint8)(r.below(4) ? 60 + r.below(196) : r.u32());
        const uint32 seed = r.u32();
        h->m_nRandomSeed = (uint16)r.u32();
        const std::vector<uint8> snap(g_veh.buf, g_veh.buf + sizeof g_veh.buf);
        SeedBoth(seed);
        CCarCtrl::SteerAIHeliToKeepEntityInView(reinterpret_cast<CAutomobile*>(h));
        const std::vector<uint8> a(g_veh.buf, g_veh.buf + sizeof g_veh.buf);
        std::memcpy(g_veh.buf, snap.data(), snap.size());
        SeedBoth(seed);
        oracle::Fn<void __cdecl(CAutomobile*)>(0x42AEB0)(reinterpret_cast<CAutomobile*>(h));
        const std::vector<uint8> b(g_veh.buf, g_veh.buf + sizeof g_veh.buf);
        Hit(a != snap);
        if (SameBlob(a.data(), b.data(), a.size())) return true;
        d = "pos " + V(hp) + " target " + V(t->GetPosition()) + " dist " + std::to_string(ap.m_ucHeliTargetDist) + " fwd " + V(h->m_matrix->GetForward()) + DiffDesc(a.data(), b.data(), a.size(), "heli");
        return false;
    });
}
