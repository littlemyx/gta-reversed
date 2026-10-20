// ORACLE-W3-AI rows: CarCtrl.cpp part 3 (classes A + B): objects / peds / cars in the sector lists around a vehicle (weave angle window, speed factor).
#include "ModelIndices.h"
namespace w3a {
static const uint16_t kSpecialObjModels[] = { 1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007 };   // traffic light, 3 street lights, double street light, roadwork barrier, 2 road block wrecks
static void SetObjModelIndices() {
    ModelIndices::MI_TRAFFICLIGHTS = (eModelID)1000; ModelIndices::MI_SINGLESTREETLIGHTS1 = (eModelID)1001; ModelIndices::MI_SINGLESTREETLIGHTS2 = (eModelID)1002; ModelIndices::MI_SINGLESTREETLIGHTS3 = (eModelID)1003; ModelIndices::MI_DOUBLESTREETLIGHTS = (eModelID)1004;
    ModelIndices::MI_ROADWORKBARRIER1 = (eModelID)1005; ModelIndices::MI_ROADBLOCKFUCKEDCAR1 = (eModelID)1006; ModelIndices::MI_ROADBLOCKFUCKEDCAR2 = (eModelID)1007;
}
// an object in front of / around vehicle A: model index from the special set (or a plain one), matrix block, col model with a bound radius, scan code, collision flag
static void PlaceObjectNearA(Rng& r, size_t off, int matSlot, int miSlot) {
    ObjWorld(r, off, matSlot);
    SetObjModelIndices();
    const uint16_t idx = r.below(5) == 0 ? (uint16_t)600 : kSpecialObjModels[r.below(8)];
    SAt<uint16_t>(off + 0x22) = idx;
    ModelInfoFor(r, miSlot, idx, r.below(8) == 0 ? Fl(r, 5.f, 3) : 0.3f + r.f01() * 3.f);
    // SwaysInWind of the model info is a bit of the garbage: both answers occur
    auto* a = reinterpret_cast<CEntity*>(S(oVehA));
    const CVector pa = a->m_matrix ? a->m_matrix->GetPosition() : CVector(SAt<float>(oVehA + 4), SAt<float>(oVehA + 8), SAt<float>(oVehA + 12));
    const CVector fwd = a->m_matrix ? a->m_matrix->GetForward() : CVector(-std::sin(SAt<float>(oVehA + 16)), std::cos(SAt<float>(oVehA + 16)), 0.f);
    const float dist = r.below(5) == 0 ? (r.f01() - 0.3f) * 60.f : 3.f + r.f01() * 25.f, lat = (r.f01() - 0.5f) * (r.below(2) ? 3.f : 14.f);
    const CVector pos(pa.x + fwd.x * dist + fwd.y * lat, pa.y + fwd.y * dist - fwd.x * lat, pa.z + (r.below(6) ? (r.f01() - 0.5f) * 3.f : (r.f01() - 0.5f) * 25.f));
    SetMatPose(off, pos, r.f01() * 6.2831853f, r.below(4) ? 0.f : (r.f01() - 0.5f) * 0.8f);
    reinterpret_cast<CEntity*>(S(off))->m_bUsesCollision = r.below(8) != 0;
    SAt<uint16_t>(off + 0x2C) = r.below(6) == 0 ? CWorld::ms_nCurrentScanCode : (uint16_t)r.below(100);
}
static void PlacePedNearA(Rng& r, size_t off, int matSlot) {
    PedWorld(r, off, oIntA, matSlot);
    auto* a = reinterpret_cast<CEntity*>(S(oVehA));
    const CVector pa = a->m_matrix ? a->m_matrix->GetPosition() : CVector(SAt<float>(oVehA + 4), SAt<float>(oVehA + 8), SAt<float>(oVehA + 12));
    const CVector fwd = a->m_matrix ? a->m_matrix->GetForward() : CVector(-std::sin(SAt<float>(oVehA + 16)), std::cos(SAt<float>(oVehA + 16)), 0.f);
    const float dist = r.below(5) == 0 ? (r.f01() - 0.3f) * 40.f : 1.f + r.f01() * 20.f, lat = (r.f01() - 0.5f) * (r.below(2) ? 3.f : 14.f);
    const CVector pos(pa.x + fwd.x * dist + fwd.y * lat, pa.y + fwd.y * dist - fwd.x * lat, pa.z + (r.below(6) ? (r.f01() - 0.5f) * 3.f : (r.f01() - 0.5f) * 12.f));
    SetMatPose(off, pos, r.f01() * 6.2831853f, 0.f);
    reinterpret_cast<CEntity*>(S(off))->m_bUsesCollision = r.below(8) != 0;
    SAt<uint16_t>(off + 0x2C) = r.below(6) == 0 ? CWorld::ms_nCurrentScanCode : (uint16_t)r.below(100);
    if (r.below(6) == 0) SAt<void*>(off + offsetof(CPed, m_pContactEntity)) = S(oVehA);
    if (r.below(6) == 0) SAt<void*>(off + offsetof(CPhysical, m_pAttachedTo)) = S(oVehA);
}
static float Ang(Rng& r) { return r.below(8) == 0 ? (r.f01() - 0.5f) * 12.f : (r.f01() - 0.5f) * 6.4f; }
static void Angles(Rng& r) { SAt<float>(oOut + 4) = Ang(r); SAt<float>(oOut + 8) = r.below(3) ? SAt<float>(oOut + 4) + r.f01() * 2.f : Ang(r); }
} // namespace w3a

static void TestW3AiCarC() {
    using namespace w2; using namespace w3a;
    const auto lo = [] { return reinterpret_cast<float*>(S(oOut + 4)); };
    const auto hi = [] { return reinterpret_cast<float*>(S(oOut + 8)); };
    const auto bounds = [&](size_t off, float* b) { auto* e = reinterpret_cast<CEntity*>(S(off)); const CVector p = e->m_matrix ? e->m_matrix->GetPosition() : CVector(SAt<float>(off + 4), SAt<float>(off + 8), SAt<float>(off + 12)); b[0] = p.x - 14.f; b[1] = p.y - 14.f; b[2] = p.x + 14.f; b[3] = p.y + 14.f; };
    ROW("CCarCtrl::WeaveForObject", 0x426BC0, 3000) {
        Tu tu([&](Rng& rr) { TrafficWorld(rr); PlaceObjectNearA(rr, oObjA, 3, 2); Angles(rr); });
        return UCase(r, d, [&]() -> uint64_t { CCarCtrl::WeaveForObject(reinterpret_cast<CEntity*>(S(oObjA)), VehS(oVehA), lo(), hi()); return B8(SAt<float>(oOut + 4) != 0.f); },
                           [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, void*, float*, float*)>(0x426BC0)(S(oObjA), S(oVehA), lo(), hi()); return B8(SAt<float>(oOut + 4) != 0.f); });
    } ENDROW;
    ROW("CCarCtrl::WeaveThroughObjectsSectorList", 0x42D950, 3000) {
        const int mode = (int)r.below(3);
        Tu tu([&](Rng& rr) { TrafficWorld(rr); PlaceObjectNearA(rr, oObjA, 3, 2); PlaceObjectNearA(rr, oObjB, 4, 3); Angles(rr); if (mode == 0) BuildList({ oObjA }); else if (mode == 1) BuildList({ oObjA, oObjB, oObjA }); else BuildList({ oObjB, oObjA }); });
        return UCase(r, d, [&]() -> uint64_t { float b[4]; bounds(oObjA, b); CCarCtrl::WeaveThroughObjectsSectorList(*reinterpret_cast<CPtrListDoubleLink<CObject*>*>(S(oHead)), VehS(oVehA), b[0], b[1], b[2], b[3], lo(), hi()); return 0; },
                           [&]() -> uint64_t { float b[4]; bounds(oObjA, b); oracle::Fn<void __cdecl(void*, void*, float, float, float, float, float*, float*)>(0x42D950)(S(oHead), S(oVehA), b[0], b[1], b[2], b[3], lo(), hi()); return 0; });
    } ENDROW;
    ROW("CCarCtrl::WeaveThroughPedsSectorList", 0x42D7E0, 3000) {
        const int mode = (int)r.below(3);
        Tu tu([&](Rng& rr) { TrafficWorld(rr); PlacePedNearA(rr, oPedA, 3); PlacePedNearA(rr, oPedB, 4); Angles(rr); if (mode == 0) BuildList({ oPedA }); else if (mode == 1) BuildList({ oPedA, oPedB }); else BuildList({ oPedB, oPedA, oPedB }); });
        return UCase(r, d, [&]() -> uint64_t { float b[4]; bounds(oPedA, b); CCarCtrl::WeaveThroughPedsSectorList(*reinterpret_cast<CPtrListDoubleLink<CPed*>*>(S(oHead)), VehS(oVehA), reinterpret_cast<CPhysical*>(S(mode == 2 ? oPedB : oVehB)), b[0], b[1], b[2], b[3], lo(), hi()); return 0; },
                           [&]() -> uint64_t { float b[4]; bounds(oPedA, b); oracle::Fn<void __cdecl(void*, void*, void*, float, float, float, float, float*, float*)>(0x42D7E0)(S(oHead), S(oVehA), S(mode == 2 ? oPedB : oVehB), b[0], b[1], b[2], b[3], lo(), hi()); return 0; });
    } ENDROW;
    ROW("CCarCtrl::WeaveThroughCarsSectorList", 0x42D680, 3000) {
        const int mode = (int)r.below(3);
        Tu tu([&](Rng& rr) { TrafficWorld(rr); Angles(rr); if (rr.below(3) == 0) { reinterpret_cast<CVehicle*>(S(oVehA))->vehicleFlags.bPartOfConvoy = true; reinterpret_cast<CVehicle*>(S(oVehB))->vehicleFlags.bPartOfConvoy = rr.below(2); } if (mode == 0) BuildList({ oVehB }); else if (mode == 1) BuildList({ oVehA, oVehB }); else BuildList({ oVehB, oVehA, oVehB }); });
        return UCase(r, d, [&]() -> uint64_t { float b[4]; bounds(oVehB, b); CCarCtrl::WeaveThroughCarsSectorList(*reinterpret_cast<CPtrListDoubleLink<CVehicle*>*>(S(oHead)), VehS(oVehA), reinterpret_cast<CPhysical*>(S(mode == 2 ? oVehA : oVehB)), b[0], b[1], b[2], b[3], lo(), hi()); return 0; },
                           [&]() -> uint64_t { float b[4]; bounds(oVehB, b); oracle::Fn<void __cdecl(void*, void*, void*, float, float, float, float, float*, float*)>(0x42D680)(S(oHead), S(oVehA), S(mode == 2 ? oVehA : oVehB), b[0], b[1], b[2], b[3], lo(), hi()); return 0; });
    } ENDROW;
    ROW("CCarCtrl::SlowCarDownForObject", 0x426220, 3000) {
        const float mult = 5.f + r.f01() * 40.f, sf0 = r.below(2) ? 1.f : r.f01() * 1.5f;
        Tu tu([&](Rng& rr) { TrafficWorld(rr); PlaceObjectNearA(rr, oObjA, 3, 2); SAt<float>(oOut) = sf0; });
        return UCase(r, d, [&]() -> uint64_t { CCarCtrl::SlowCarDownForObject(reinterpret_cast<CEntity*>(S(oObjA)), VehS(oVehA), reinterpret_cast<float*>(S(oOut)), mult); return B8(SAt<float>(oOut) != sf0); },
                           [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, void*, float*, float)>(0x426220)(S(oObjA), S(oVehA), reinterpret_cast<float*>(S(oOut)), mult); return B8(SAt<float>(oOut) != sf0); });
    } ENDROW;
    ROW("CCarCtrl::SlowCarDownForObjectsSectorList", 0x42D4F0, 3000) {
        const float mult = 5.f + r.f01() * 40.f, sf0 = r.below(2) ? 1.f : r.f01() * 1.5f; const int mode = (int)r.below(3);
        Tu tu([&](Rng& rr) { TrafficWorld(rr); PlaceObjectNearA(rr, oObjA, 3, 2); PlaceObjectNearA(rr, oObjB, 4, 3); SAt<float>(oOut) = sf0; if (mode == 0) BuildList({ oObjA }); else if (mode == 1) BuildList({ oObjA, oObjB, oObjA }); else BuildList({ oObjB, oObjA }); });
        return UCase(r, d, [&]() -> uint64_t { float b[4]; bounds(oObjA, b); CCarCtrl::SlowCarDownForObjectsSectorList(*reinterpret_cast<CPtrListDoubleLink<CObject*>*>(S(oHead)), VehS(oVehA), b[0], b[1], b[2], b[3], reinterpret_cast<float*>(S(oOut)), mult); return B8(SAt<float>(oOut) != sf0); },
                           [&]() -> uint64_t { float b[4]; bounds(oObjA, b); oracle::Fn<void __cdecl(void*, void*, float, float, float, float, float*, float)>(0x42D4F0)(S(oHead), S(oVehA), b[0], b[1], b[2], b[3], reinterpret_cast<float*>(S(oOut)), mult); return B8(SAt<float>(oOut) != sf0); });
    } ENDROW;
    // the vehicle looks at / next to the player (a ped at a position near the vehicle, with a speed); outputs at oOut: steer / gas / brake floats, handbrake byte
    const auto playerWorld = [&](Rng& rr) {
        TrafficWorld(rr);
        PedWorld(rr, oPedA, oIntA, 3);
        auto* p = reinterpret_cast<CPed*>(S(oPedA)); p->bInVehicle = false;
        CWorld::PlayerInFocus = 0; CWorld::Players[0].m_pPed = reinterpret_cast<CPlayerPed*>(p);
        auto* a = reinterpret_cast<CEntity*>(S(oVehA));
        if (p->m_matrix && a->m_matrix) {
            const float yaw = rr.f01() * 6.2831853f; SetMatPose(oPedA, a->m_matrix->GetPosition() + CVector((rr.f01() - 0.5f) * 80.f, (rr.f01() - 0.5f) * 80.f, (rr.f01() - 0.5f) * (rr.below(4) ? 4.f : 40.f)), yaw, 0.f);
        }
        reinterpret_cast<CPhysical*>(p)->m_vecMoveSpeed = rr.below(3) ? CVector((rr.f01() - 0.5f) * 0.4f, (rr.f01() - 0.5f) * 0.4f, 0.f) : CVector(0.f, 0.f, 0.f);
        reinterpret_cast<CVehicle*>(S(oVehA))->m_nRandomSeed = (uint16_t)rr.u32();
        FillGarbage(rr, S(oOut), 0x40); SAt<uint8_t>(oOut + 0x20) = (uint8_t)rr.below(2);
    };
    const auto outs = [&]() { return U(0) + FBits(SAt<float>(oOut)) + 3 * (uint64_t)FBits(SAt<float>(oOut + 4)) + 5 * (uint64_t)FBits(SAt<float>(oOut + 8)) + SAt<uint8_t>(oOut + 0x20); };
    ROW("CCarCtrl::SteerAICarBlockingPlayerForwardAndBack", 0x422B20, 3000) {
        Tu tu(playerWorld);
        return UCase(r, d, [&]() -> uint64_t { CCarCtrl::SteerAICarBlockingPlayerForwardAndBack(VehS(oVehA), reinterpret_cast<float*>(S(oOut)), reinterpret_cast<float*>(S(oOut + 4)), reinterpret_cast<float*>(S(oOut + 8)), reinterpret_cast<bool*>(S(oOut + 0x20))); return outs(); },
                           [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, float*, float*, float*, bool*)>(0x422B20)(S(oVehA), reinterpret_cast<float*>(S(oOut)), reinterpret_cast<float*>(S(oOut + 4)), reinterpret_cast<float*>(S(oOut + 8)), reinterpret_cast<bool*>(S(oOut + 0x20))); return outs(); });
    } ENDROW;
    ROW("CCarCtrl::SteerAIBoatWithPhysicsCirclingPlayer", 0x429090, 3000) {
        Tu tu(playerWorld);
        return UCase(r, d, [&]() -> uint64_t { CCarCtrl::SteerAIBoatWithPhysicsCirclingPlayer(VehS(oVehA), reinterpret_cast<float*>(S(oOut)), reinterpret_cast<float*>(S(oOut + 4)), reinterpret_cast<float*>(S(oOut + 8)), reinterpret_cast<bool*>(S(oOut + 0x20))); return outs(); },
                           [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, float*, float*, float*, bool*)>(0x429090)(S(oVehA), reinterpret_cast<float*>(S(oOut)), reinterpret_cast<float*>(S(oOut + 4)), reinterpret_cast<float*>(S(oOut + 8)), reinterpret_cast<bool*>(S(oOut + 0x20))); return outs(); });
    } ENDROW;
    // CObject::GrabObjectToCarryWithRope: the object (id = its address) owns a rope, the hook object is entity B of the object block, the grabbed physical is vehicle B (col model behind its model index)
    ROW("CObject::GrabObjectToCarryWithRope", 0x5A1AB0, 2500) {
        const int k = (int)r.below(8);
        Tu tu([&](Rng& rr) {
            TrafficWorld(rr);
            ObjWorld(rr, oObjA, 3); ObjWorld(rr, oObjB, 4);
            FillGarbage(rr, reinterpret_cast<void*>(0xB768B8), 8 * 0x328);
            for (int i = 0; i < 8; ++i) { auto* rp = reinterpret_cast<uint8_t*>(0xB768B8 + i * 0x328); std::memset(rp + 0x310, 0, 0x18); const uint32_t ty = i == k ? 1 + rr.below(6) : rr.below(3) == 0 ? 0 : rr.below(8); std::memcpy(rp + 0x324 - 0, &ty, 0); rp[0x325] = (uint8_t)ty; }
            auto* rope = reinterpret_cast<uint8_t*>(0xB768B8 + k * 0x328);
            const uint32_t id = (uint32_t)(uintptr_t)S(oObjA), hook = (uint32_t)(uintptr_t)S(oObjB), picked = rr.below(2) ? (uint32_t)(uintptr_t)S(oVehA) : 0u;
            std::memcpy(rope + 0x300, &id, 4); std::memcpy(rope + 0x314, &hook, 4); std::memcpy(rope + 0x318, &picked, 4);
        });
        return UCase(r, d, [&]() -> uint64_t { reinterpret_cast<CObject*>(S(oObjA))->GrabObjectToCarryWithRope(reinterpret_cast<CPhysical*>(S(oVehB))); return 0; },
                           [&]() -> uint64_t { ExeThis<void, void*>(0x5A1AB0, S(oObjA), S(oVehB)); return 0; });
    } ENDROW;
}
