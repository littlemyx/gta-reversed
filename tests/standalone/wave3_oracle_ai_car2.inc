// ORACLE-W3-AI rows: CarCtrl.cpp part 2 (classes A + B): slow-down / weave functions of a vehicle that looks at another vehicle / object / ped in front of it.
namespace w3a {
constexpr size_t oHead = oList + 0x400;          // the list header (CPtrListDoubleLink: one node pointer)
// realistic vehicle col model: half sizes of a car, bound sphere around the origin
static void CarModelInfo(Rng& r, int slot, uint16_t idx) {
    ModelInfoFor(r, slot, idx, 3.f + r.f01() * 2.f);
    auto* cm = reinterpret_cast<CColModel*>(S(oCol + (size_t)slot * 0x100));
    const float hx = 0.8f + r.f01() * 0.5f, hy = 2.f + r.f01() * 2.f, hz = 0.6f + r.f01() * 0.8f;
    cm->m_boundBox.m_vecMin = CVector(-hx, -hy, -hz); cm->m_boundBox.m_vecMax = CVector(hx, hy, hz);
    cm->m_boundSphere.m_vecCenter = CVector(0.f, (r.f01() - 0.5f) * 0.4f, (r.f01() - 0.5f) * 0.2f);
}
// pose of an entity: matrix when it has one (yaw about z + a little tilt), else the placement position / heading
static void SetMatPose(size_t off, const CVector& pos, float yaw, float tilt) {
    auto* e = reinterpret_cast<CEntity*>(S(off));
    auto* m = e->m_matrix;
    if (!m) { SAt<float>(off + 4) = pos.x; SAt<float>(off + 8) = pos.y; SAt<float>(off + 12) = pos.z; SAt<float>(off + 16) = yaw; return; }
    const float c = std::cos(yaw), s = std::sin(yaw), ct = std::cos(tilt), st = std::sin(tilt);
    m->GetRight() = CVector(c, s, 0.f);
    m->GetForward() = CVector(-s * ct, c * ct, st);
    m->GetUp() = CVector(s * st, -c * st, ct);
    m->GetPosition() = pos;
}
// A at some position with a yaw-dominated orientation, B in front of / around A
static void TrafficWorld(Rng& r) {
    CReferences::Init();
    VehWorld(r, oVehA, 0); VehWorld(r, oVehB, 1);
    SAt<uint16_t>(oVehA + 0x22) = 400; SAt<uint16_t>(oVehB + 0x22) = 401;
    CarModelInfo(r, 0, 400); CarModelInfo(r, 1, 401);
    const CVector pa(Fl(r, 2000.f, 30), Fl(r, 2000.f, 30), (r.f01() - 0.5f) * 30.f);
    const float yaw = r.f01() * 6.2831853f;
    SetMatPose(oVehA, pa, yaw, r.below(3) ? 0.f : (r.f01() - 0.5f) * 0.3f);
    // the other one: ahead (usually) or around, at a distance that makes the collision window both hit and miss
    const float dist = r.below(4) == 0 ? (r.f01() - 0.3f) * 60.f : 4.f + r.f01() * 22.f;
    const float lat = (r.f01() - 0.5f) * (r.below(2) ? 4.f : 20.f);
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const CVector pb(pa.x - sy * dist + cy * lat, pa.y + cy * dist + sy * lat, pa.z + (r.below(6) ? (r.f01() - 0.5f) * 2.f : (r.f01() - 0.5f) * 30.f));
    SetMatPose(oVehB, pb, r.below(3) ? yaw + (r.below(2) ? 3.1415926f : 0.f) + (r.f01() - 0.5f) * 0.4f : r.f01() * 6.2831853f, r.below(3) ? 0.f : (r.f01() - 0.5f) * 0.3f);
    for (auto off : { oVehA, oVehB }) {
        auto* v = reinterpret_cast<CPhysical*>(S(off));
        v->m_vecMoveSpeed = r.below(3) ? CVector((r.f01() - 0.5f) * 1.2f, (r.f01() - 0.5f) * 1.2f, (r.f01() - 0.5f) * 0.1f) : CVector(0.f, 0.f, 0.f);
        auto* veh = reinterpret_cast<CVehicle*>(v);
        veh->m_bUsesCollision = r.below(8) != 0;
        veh->m_autoPilot.m_nTimeSwitchedToRealPhysics = r.below(2) ? 0u : (uint32_t)(CTimer::m_snTimeInMilliseconds - r.below(40000));
        veh->m_autoPilot.m_nCarMission = (eCarMission)r.below(0x3A);
        veh->m_autoPilot.m_TargetEntity = r.below(3) == 0 ? reinterpret_cast<CVehicle*>(S(off == oVehA ? oVehB : oVehA)) : nullptr;
        veh->m_autoPilot.m_ObstructingEntity = nullptr;
        veh->m_nVehicleType = (eVehicleType)r.below(9);
    }
    VehS(oVehA)->m_autoPilot.m_nCarDrivingStyle = (eCarDrivingStyle)r.below(6);
    CWorld::PlayerInFocus = 0; CWorld::Players[0].m_pPed = nullptr;
    CWorld::ms_nCurrentScanCode = (uint16_t)(1 + r.below(100));
    for (auto off : { oVehA, oVehB }) SAt<uint16_t>(off + 0x2C) = r.below(5) == 0 ? CWorld::ms_nCurrentScanCode : (uint16_t)r.below(100);
}
// list header + nodes of the listed entities
static void BuildList(std::initializer_list<size_t> items) {
    uint8_t* prev = nullptr; uint8_t* first = nullptr; int i = 0;
    for (size_t it : items) {
        uint8_t* nd = S(oList + (size_t)i++ * 0xC);
        *reinterpret_cast<void**>(nd) = S(it); *reinterpret_cast<void**>(nd + 4) = nullptr; *reinterpret_cast<void**>(nd + 8) = prev;
        if (prev) *reinterpret_cast<void**>(prev + 4) = nd; else first = nd;
        prev = nd;
    }
    *reinterpret_cast<void**>(S(oHead)) = first;
}
} // namespace w3a

static void TestW3AiCarB() {
    using namespace w2; using namespace w3a;
    ROW("CCarCtrl::SlowCarDownForOtherCar", 0x42D0E0, 3000) {
        const float mult = r.below(10) == 0 ? Fl(r, 60.f, 3) : 5.f + r.f01() * 40.f, sf0 = r.below(2) ? 1.f : r.f01() * 1.5f;
        Tu tu([&](Rng& rr) { TrafficWorld(rr); SAt<float>(oOut) = sf0; });
        return UCase(r, d, [&]() -> uint64_t { CCarCtrl::SlowCarDownForOtherCar(reinterpret_cast<CEntity*>(S(oVehB)), VehS(oVehA), reinterpret_cast<float*>(S(oOut)), mult); return B8(SAt<float>(oOut) != sf0); },
                           [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, void*, float*, float)>(0x42D0E0)(S(oVehB), S(oVehA), reinterpret_cast<float*>(S(oOut)), mult); return B8(SAt<float>(oOut) != sf0); });
    } ENDROW;
    ROW("CCarCtrl::SlowCarDownForCarsSectorList", 0x432420, 3000) {
        const float mult = 5.f + r.f01() * 40.f, sf0 = r.below(2) ? 1.f : r.f01() * 1.5f; const int mode = (int)r.below(3);
        Tu tu([&](Rng& rr) {
            TrafficWorld(rr); SAt<float>(oOut) = sf0;
            if (mode == 0) BuildList({ oVehB }); else if (mode == 1) BuildList({ oVehA, oVehB, oVehB }); else BuildList({ oVehB, oVehA });
        });
        const auto bounds = [&](float* b) { auto* e = reinterpret_cast<CEntity*>(S(oVehB)); const CVector p = e->m_matrix ? e->m_matrix->GetPosition() : CVector(SAt<float>(oVehB + 4), SAt<float>(oVehB + 8), SAt<float>(oVehB + 12)); b[0] = p.x - 12.f; b[1] = p.y - 12.f; b[2] = p.x + 12.f; b[3] = p.y + 12.f; };
        return UCase(r, d, [&]() -> uint64_t { float b[4]; bounds(b); CCarCtrl::SlowCarDownForCarsSectorList(*reinterpret_cast<CPtrListDoubleLink<CVehicle*>*>(S(oHead)), VehS(oVehA), b[0], b[1], b[2], b[3], reinterpret_cast<float*>(S(oOut)), mult); return B8(SAt<float>(oOut) != sf0); },
                           [&]() -> uint64_t { float b[4]; bounds(b); oracle::Fn<void __cdecl(void*, void*, float, float, float, float, float*, float)>(0x432420)(S(oHead), S(oVehA), b[0], b[1], b[2], b[3], reinterpret_cast<float*>(S(oOut)), mult); return B8(SAt<float>(oOut) != sf0); });
    } ENDROW;
}
