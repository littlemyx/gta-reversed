// ORACLE-W3-AI rows: CarCtrl.cpp part 1 (classes A + B): the vehicles-of-interest list, counters, node validity, plane / heli crash steering, plane gun test.
// Vehicles are full-size garbage blocks in the scratch block (type 2, matrix block, model index); the pool-walking functions use the vehicle pool of the reference world (logic_oracle_refs.inc).
#include "Entity/Vehicle/Heli.h"
#include "Entity/Vehicle/Plane.h"
namespace w3a {
static CVehicle*& Keep(int i) { return At<CVehicle*>(0x969084 + 4 * (uintptr_t)i); }   // apCarsToKeep (file-local global of CarCtrl.cpp)
static uint32_t& KeepT(int i) { return At<uint32_t>(0x96907C + 4 * (uintptr_t)i); }   // aCarsToKeepTime
static CVehicle* VehS(size_t off) { return reinterpret_cast<CVehicle*>(S(off)); }
static void VehWorld(Rng& r, size_t off, int matSlot, bool autoPilot = true) {
    Ent(r, off, matSlot, 2, (uint16_t)(400 + r.below(200)), 0xA20, false);
    auto* v = VehS(off);
    v->m_nCreatedBy = (eVehicleCreatedBy)(r.below(8) == 0 ? r.below(256) : 1 + r.below(5));
    SAt<int8_t>(off + offsetof(CVehicle, m_vehicleSpecialColIndex)) = (int8_t)-1;   // CEntity::GetColModel: >= 0 would index CVehicle::m_aSpecialColModel (4 entries)
    (void)autoPilot;
}
// node address fixtures: area 0..63 or invalid (0xFFFF), node ids random
static uint16_t Area(Rng& r) { return r.below(6) == 0 ? (uint16_t)0xFFFF : (uint16_t)r.below(64); }
static void LoadAreas(Rng& r) {
    for (auto& p : ThePaths.m_pPathNodes) p = r.below(3) == 0 ? nullptr : reinterpret_cast<CPathNode*>(S(oOut + 16 * r.below(16)));
}
} // namespace w3a

static void TestW3AiCarA() {
    using namespace w2; using namespace w3a;
    ROW("CCarCtrl::ReInit", 0x4213B0, 100) {
        Tu tu([&](Rng& rr) {
            Keep(0) = reinterpret_cast<CVehicle*>(uintptr_t(rr.u32())); Keep(1) = reinterpret_cast<CVehicle*>(uintptr_t(rr.u32()));
            FillGarbage(rr, (uintptr_t)0xC0E998, 46 * 15);   // m_LoadedBoats .. m_LoadedGangCars (3 groups + the 11 groups the exe clears)
            FillGarbage(rr, (uintptr_t)0x969070, 0x80);      // the CarCtrl counters
        });
        return UCase(r, d, [&]() -> uint64_t { CCarCtrl::ReInit(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x4213B0)(); return 0; });
    } ENDROW;
    ROW("CCarCtrl::ClearInterestingVehicleList", 0x423F00, 40) {
        Tu tu([&](Rng& rr) { Keep(0) = reinterpret_cast<CVehicle*>(uintptr_t(rr.u32())); Keep(1) = nullptr; });
        return UCase(r, d, [&]() -> uint64_t { CCarCtrl::ClearInterestingVehicleList(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x423F00)(); return 0; });
    } ENDROW;
    // list fixture: slots hold vehicles A / B / null; the query vehicle is one of them or a third
    const auto listWorld = [&](Rng& rr) {
        VehWorld(rr, oVehA, 0); VehWorld(rr, oVehB, 1);
        Keep(0) = rr.below(3) == 0 ? nullptr : (rr.below(2) ? VehS(oVehA) : VehS(oVehB));
        Keep(1) = rr.below(3) == 0 ? nullptr : (rr.below(2) ? VehS(oVehA) : VehS(oVehB));
        CTimer::m_snTimeInMilliseconds = rr.below(3) ? rr.below(5000000) : rr.u32();
        for (int i = 0; i < 2; ++i) KeepT(i) = rr.below(4) == 0 ? rr.u32() : CTimer::m_snTimeInMilliseconds - rr.below(400000);
    };
    ROW("CCarCtrl::IsThisVehicleInteresting", 0x423EA0, 400) {
        const int q = (int)r.below(3);
        Tu tu([&](Rng& rr) { listWorld(rr); });
        return UCase(r, d, [&]() -> uint64_t { return B8(CCarCtrl::IsThisVehicleInteresting(q == 0 ? VehS(oVehA) : q == 1 ? VehS(oVehB) : nullptr)); },
                           [&]() -> uint64_t { return B8(oracle::Fn<bool __cdecl(void*)>(0x423EA0)(q == 0 ? S(oVehA) : q == 1 ? S(oVehB) : nullptr)); });
    } ENDROW;
    ROW("CCarCtrl::RemoveFromInterestingVehicleList", 0x423ED0, 400) {
        const int q = (int)r.below(3);
        Tu tu([&](Rng& rr) { listWorld(rr); });
        return UCase(r, d, [&]() -> uint64_t { CCarCtrl::RemoveFromInterestingVehicleList(q == 0 ? VehS(oVehA) : q == 1 ? VehS(oVehB) : nullptr); return 0; },
                           [&]() -> uint64_t { oracle::Fn<void __cdecl(void*)>(0x423ED0)(q == 0 ? S(oVehA) : q == 1 ? S(oVehB) : nullptr); return 0; });
    } ENDROW;
    ROW("CCarCtrl::RegisterVehicleOfInterest", 0x423DE0, 800) {
        const int q = (int)r.below(2);
        Tu tu([&](Rng& rr) { listWorld(rr); if (rr.below(4) == 0) Keep(0) = Keep(1) = nullptr; });
        return UCase(r, d, [&]() -> uint64_t { CCarCtrl::RegisterVehicleOfInterest(q == 0 ? VehS(oVehA) : VehS(oVehB)); return 0; },
                           [&]() -> uint64_t { oracle::Fn<void __cdecl(void*)>(0x423DE0)(q == 0 ? S(oVehA) : S(oVehB)); return 0; });
    } ENDROW;
    ROW("CCarCtrl::PruneVehiclesOfInterest", 0x423F10, 800) {
        Tu tu([&](Rng& rr) {
            listWorld(rr);
            CTimer::m_FrameCounter = (rr.below(2) ? 19u : rr.u32()) + (rr.below(2) ? 64u * rr.below(100) : 0u);
            // the player: a ped (CWorld::Players[0]) at z below / above 950
            PedWorld(rr, oPedA, oIntA, 2);
            auto* p = reinterpret_cast<CPed*>(S(oPedA));
            CWorld::PlayerInFocus = 0; CWorld::Players[0].m_pPed = reinterpret_cast<CPlayerPed*>(p);
            if (p->m_matrix) p->m_matrix->GetPosition().z = rr.below(2) ? 100.f : 950.f + (rr.f01() - 0.5f) * 20.f;
            else SAt<float>(oPedA + 12) = rr.below(2) ? 100.f : 950.f + (rr.f01() - 0.5f) * 20.f;
            p->bInVehicle = false;
        });
        return UCase(r, d, [&]() -> uint64_t { CCarCtrl::PruneVehiclesOfInterest(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x423F10)(); return 0; });
    } ENDROW;
    ROW("CCarCtrl::UpdateCarCount", 0x424000, 2000) {
        const uint8_t dec = (uint8_t)r.below(2);
        Tu tu([&](Rng& rr) {
            VehWorld(rr, oVehA, 0);
            CCarCtrl::NumRandomCars = rr.below(4) == 0 ? (int32_t)rr.below(3) - 1 : (int32_t)rr.below(60);
            CCarCtrl::NumLawEnforcerCars = rr.below(4) == 0 ? rr.below(2) : rr.below(30);
            CCarCtrl::NumMissionCars = rr.below(4) == 0 ? (int32_t)rr.below(3) - 1 : (int32_t)rr.below(30);
            CCarCtrl::NumParkedCars = rr.below(3) ? rr.below(30) : 0;
            CCarCtrl::NumPermanentVehicles = rr.below(4) == 0 ? (int32_t)rr.below(3) - 1 : (int32_t)rr.below(30);
        });
        return UCase(r, d, [&]() -> uint64_t { CCarCtrl::UpdateCarCount(VehS(oVehA), dec); return 0; },
                           [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, int)>(0x424000)(S(oVehA), (int)dec); return 0; });
    } ENDROW;
    ROW("CCarCtrl::StopCarIfNodesAreInvalid", 0x422590, 2500) {
        Tu tu([&](Rng& rr) {
            VehWorld(rr, oVehA, 0); LoadAreas(rr);
            auto& ap = VehS(oVehA)->m_autoPilot;
            ap.m_nCurrentPathNodeInfo.m_wAreaId = Area(rr); ap.m_nNextPathNodeInfo.m_wAreaId = Area(rr);
            ap.m_currentAddress.m_wAreaId = Area(rr); ap.m_startingRouteNode.m_wAreaId = Area(rr);
            if (rr.below(2)) { ap.m_nCurrentPathNodeInfo.m_wAreaId = (uint16_t)rr.below(4); ap.m_nNextPathNodeInfo.m_wAreaId = (uint16_t)rr.below(4); ap.m_currentAddress.m_wAreaId = (uint16_t)rr.below(4); ap.m_startingRouteNode.m_wAreaId = (uint16_t)rr.below(4); for (int i = 0; i < 4; ++i) ThePaths.m_pPathNodes[i] = rr.below(8) ? reinterpret_cast<CPathNode*>(S(oOut)) : nullptr; }
        });
        return UCase(r, d, [&]() -> uint64_t { return B8(CCarCtrl::StopCarIfNodesAreInvalid(VehS(oVehA))); }, [&]() -> uint64_t { return B8(oracle::Fn<bool __cdecl(void*)>(0x422590)(S(oVehA))); });
    } ENDROW;
    ROW("CCarCtrl::IsAnyoneParking", 0x42C250, 600) {
        Tu tu([&](Rng& rr) {
            for (int i = 0; i < rf::kV; ++i) {
                auto* v = reinterpret_cast<CVehicle*>(rf::VehE(i));
                v->m_autoPilot.m_nCarMission = (eCarMission)(rr.below(4) == 0 ? (int)rr.below(256) : (rr.below(4) == 0 ? 0x1C + (int)rr.below(8) : (int)rr.below(0x3A)));
            }
        }, true);
        return UCase(r, d, [&]() -> uint64_t { return B8(CCarCtrl::IsAnyoneParking()); }, [&]() -> uint64_t { return B8(oracle::Fn<bool __cdecl()>(0x42C250)()); });
    } ENDROW;
    ROW("CCarCtrl::SteerAIPlaneToCrashAndBurn", 0x423880, 200) {
        Tu tu([&](Rng& rr) { VehWorld(rr, oVehA, 0); });
        return UCase(r, d, [&]() -> uint64_t { CCarCtrl::SteerAIPlaneToCrashAndBurn(reinterpret_cast<CAutomobile*>(S(oVehA))); return 0; },
                           [&]() -> uint64_t { oracle::Fn<void __cdecl(void*)>(0x423880)(S(oVehA)); return 0; });
    } ENDROW;
    ROW("CCarCtrl::SteerAIHeliToCrashAndBurn", 0x4238E0, 200) {
        Tu tu([&](Rng& rr) { VehWorld(rr, oVehA, 0); });
        return UCase(r, d, [&]() -> uint64_t { CCarCtrl::SteerAIHeliToCrashAndBurn(reinterpret_cast<CAutomobile*>(S(oVehA))); return 0; },
                           [&]() -> uint64_t { oracle::Fn<void __cdecl(void*)>(0x4238E0)(S(oVehA)); return 0; });
    } ENDROW;
    ROW("CCarCtrl::TestWhetherToFirePlaneGuns", 0x429520, 3000) {
        const bool nullTarget = r.below(12) == 0;
        Tu tu([&](Rng& rr) {
            VehWorld(rr, oVehA, 0); VehWorld(rr, oVehB, 1);
            VehS(oVehA)->m_nVehicleWeaponInUse = (eCarWeapon)(rr.below(3) == 0 ? (int)rr.below(8) : (rr.below(2) ? 0 : 1));
            // target near the plane in half of the cases so the 150 m test and the dot test go both ways; the plane's matrix nose points at the target in half of those
            auto* a = VehS(oVehA); auto* b = VehS(oVehB);
            if (rr.below(2) && a->m_matrix && b->m_matrix) {
                b->m_matrix->GetPosition() = a->m_matrix->GetPosition() + CVector((rr.f01() - 0.5f) * 300.f, (rr.f01() - 0.5f) * 300.f, (rr.f01() - 0.5f) * 100.f);
                if (rr.below(2)) { CVector dir = b->m_matrix->GetPosition() - a->m_matrix->GetPosition(); const float l = dir.Magnitude(); if (l > 0.001f) { dir /= l; a->m_matrix->GetForward() = dir + CVector((rr.f01() - 0.5f) * 0.3f, (rr.f01() - 0.5f) * 0.3f, (rr.f01() - 0.5f) * 0.3f); } }
            }
        });
        return UCase(r, d, [&]() -> uint64_t { CCarCtrl::TestWhetherToFirePlaneGuns(VehS(oVehA), nullTarget ? nullptr : reinterpret_cast<CEntity*>(S(oVehB))); return 0; },
                           [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, void*)>(0x429520)(S(oVehA), nullTarget ? nullptr : S(oVehB)); return 0; });
    } ENDROW;
}
