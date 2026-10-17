// ORACLE-W1 / entities + events: CPlaceable::SetPosn / IsWithinArea (script "is in area" checks), and the AI event predicates CEvent*::AffectsPed / IsHeadOnCollision.
// Entities are raw garbage buffers whose matrix pointer is either null (placement position) or a matrix inside the buffer; the ped / vehicle / event objects are typed views on garbage buffers
// (the port's definition is called non-virtually: `ev->CEventX::AffectsPed(ped)`), the exe function gets the same `this`.
#include "Entity/Placeable.h"
#include "Events/EventDanger.h"
#include "Events/EventCarUpsideDown.h"
#include "Events/EventChatPartner.h"
#include "Events/EventDraggedOutCar.h"
#include "Events/EventGotKnockedOverByCar.h"
#include "Events/EventCopCarBeingStolen.h"
#include "Events/EventBuildingCollision.h"
#include "Entity/Vehicle/Vehicle.h"

namespace w1e {
alignas(16) inline uint8 place[0x100];
alignas(16) inline uint8 placeMat[0x40];
alignas(16) inline uint8 ev[0x100];
alignas(16) inline uint8 veh[0x800];
alignas(16) inline uint8 other[0x100];     // a second entity (danger source / jacker / hijacker ...)
alignas(16) inline uint8 otherMat[0x40];
alignas(16) inline uint8 pedMat[0x40];
alignas(16) inline uint8 vehMat[0x40];
inline void Mat(Rng& r, uint8* m) { FillGarbage(r, m, 0x40); float* f = reinterpret_cast<float*>(m); for (int i = 0; i < 16; ++i) f[i] = GF(r, 50.f); }
inline CPlaceable* Place(Rng& r) {
    FillGarbage(r, place, sizeof place); Mat(r, placeMat);
    auto* p = reinterpret_cast<CPlaceable*>(place);
    p->m_placement.m_vPosn = GVf(r, 100.f);
    p->m_matrix = r.below(2) ? reinterpret_cast<CMatrixLink*>(placeMat) : nullptr;
    return p;
}
// ped + vehicle + second entity, with the pointers between them wired (or null)
struct World { CPed* ped; CVehicle* vehicle; CEntity* other; };
inline World Setup(Rng& r) {
    World w{};
    w.ped = w1ped::Setup(r);
    FillGarbage(r, veh, sizeof veh); FillGarbage(r, other, sizeof other); FillGarbage(r, ev, sizeof ev);
    Mat(r, otherMat); Mat(r, pedMat); Mat(r, vehMat);
    w.vehicle = reinterpret_cast<CVehicle*>(veh);
    w.other = reinterpret_cast<CEntity*>(other);
    reinterpret_cast<CPlaceable*>(w.vehicle)->m_matrix = r.below(2) ? reinterpret_cast<CMatrixLink*>(vehMat) : nullptr;
    reinterpret_cast<CPlaceable*>(w.vehicle)->m_placement.m_vPosn = GVf(r, 100.f);
    w.vehicle->m_nVehicleType = (eVehicleType)(r.below(12));
    w.vehicle->m_pVehicleBeingTowed = nullptr;
    w.ped->m_pVehicle = r.below(3) ? w.vehicle : nullptr;
    if (!w.ped->m_pVehicle) w.ped->bInVehicle = false;   // invariant: a ped in a vehicle has m_pVehicle (the port and the exe both dereference it)
    w1ped::A()->m_nCreatedBy = (ePedCreatedBy)r.below(3);
    reinterpret_cast<CPlaceable*>(w.other)->m_matrix = r.below(2) ? reinterpret_cast<CMatrixLink*>(otherMat) : nullptr;
    reinterpret_cast<CPlaceable*>(w.other)->m_placement.m_vPosn = GVf(r, 100.f);
    reinterpret_cast<CPlaceable*>(w.ped)->m_matrix = r.below(2) ? reinterpret_cast<CMatrixLink*>(pedMat) : nullptr;
    reinterpret_cast<CPlaceable*>(w.ped)->m_placement.m_vPosn = GVf(r, 100.f);
    reinterpret_cast<CPlaceable*>(w.ped)->m_placement.m_fHeading = GF(r, 7.f);
    return w;
}
inline CEntity* PickEnt(Rng& r, const World& w) { switch (r.below(4)) { case 0: return nullptr; case 1: return w.other; case 2: return reinterpret_cast<CEntity*>(w.vehicle); default: return reinterpret_cast<CEntity*>(w.ped); } }
inline std::string Ctx(const World& w) { return w1ped::Ctx(w.ped) + " veh " + std::to_string((int)w.vehicle->m_nVehicleType) + " pedVeh " + std::to_string(w.ped->m_pVehicle != nullptr); }
}
#define W1_ENT_RGS Rg{ (uintptr_t)w1ped::buf, sizeof(CPed) }, Rg{ (uintptr_t)w1ped::pdata, sizeof w1ped::pdata }, Rg{ (uintptr_t)w1e::veh, 0x600 }, Rg{ (uintptr_t)w1e::other, 0x100 }, Rg{ (uintptr_t)w1e::ev, 0x100 }, \
                   Rg{ (uintptr_t)w1e::otherMat, 0x40 }, Rg{ (uintptr_t)w1e::pedMat, 0x40 }, Rg{ (uintptr_t)w1e::vehMat, 0x40 }

#define W1_EVENT_AFFECTS(Cls, va, setup) Run(#Cls "::AffectsPed " #va, [](Rng& r, std::string& d) { auto w = w1e::Setup(r); auto* e = reinterpret_cast<Cls*>(w1e::ev); setup; \
    return Both(d, { W1_ENT_RGS }, [&] { return RB(e->Cls::AffectsPed(w.ped)); }, [&] { return RB(oracle::Fn<bool __fastcall(Cls*, int, CPed*)>(va)(e, 0, w.ped)); }, w1e::Ctx(w)); });

static void TestEntities() {
    // ---- CPlaceable
    Run("CPlaceable::SetPosn(xyz) 0x420B80", [](Rng& r, std::string& d) {
        auto* p = w1e::Place(r); const float x = GF(r, 500.f), y = GF(r, 500.f), z = GF(r, 500.f);
        return Both(d, { Rg{ (uintptr_t)w1e::place, sizeof w1e::place }, Rg{ (uintptr_t)w1e::placeMat, 0x40 } }, [&] { p->SetPosn(x, y, z); return 0ull; }, [&] { oracle::Fn<void __fastcall(CPlaceable*, int, float, float, float)>(0x420B80)(p, 0, x, y, z); return 0ull; });
    });
    Run("CPlaceable::SetPosn(vec) 0x4241C0", [](Rng& r, std::string& d) {
        auto* p = w1e::Place(r); const CVector v = GVf(r, 500.f);
        return Both(d, { Rg{ (uintptr_t)w1e::place, sizeof w1e::place }, Rg{ (uintptr_t)w1e::placeMat, 0x40 } }, [&] { p->SetPosn(v); return 0ull; }, [&] { oracle::Fn<void __fastcall(CPlaceable*, int, const CVector*)>(0x4241C0)(p, 0, &v); return 0ull; });
    });
    auto areaVal = [](Rng& r, const CPlaceable* p) { const CVector& pos = p->GetPosition(); const float s = r.below(4) ? 50.f : 1000.f;
        return r.below(4) ? pos.x + GF(r, s) * 0.3f : GF(r, s); };
    Run("CPlaceable::IsWithinArea(xy) 0x54F200", [&](Rng& r, std::string& d) {
        auto* p = w1e::Place(r);
        const CVector& pos = p->GetPosition();
        float a[4] = { areaVal(r, p), areaVal(r, p), areaVal(r, p), areaVal(r, p) };
        if (r.below(3) == 0) { a[0] = pos.x; a[1] = pos.y; }   // boundary equality
        if (r.below(3) == 0) { a[2] = pos.x; a[3] = pos.y; }
        return Both(d, { Rg{ (uintptr_t)w1e::place, sizeof w1e::place }, Rg{ (uintptr_t)w1e::placeMat, 0x40 } }, [&] { return RB(p->IsWithinArea(a[0], a[1], a[2], a[3])); },
                    [&] { return RB(oracle::Fn<bool __fastcall(CPlaceable*, int, float, float, float, float)>(0x54F200)(p, 0, a[0], a[1], a[2], a[3])); }, "pos " + V(pos));
    });
    Run("CPlaceable::IsWithinArea(xyz) 0x54F2B0", [&](Rng& r, std::string& d) {
        auto* p = w1e::Place(r);
        const CVector& pos = p->GetPosition();
        float a[6] = { areaVal(r, p), areaVal(r, p), areaVal(r, p), areaVal(r, p), areaVal(r, p), areaVal(r, p) };
        if (r.below(3) == 0) { a[0] = pos.x; a[1] = pos.y; a[2] = pos.z; }
        if (r.below(3) == 0) { a[3] = pos.x; a[4] = pos.y; a[5] = pos.z; }
        return Both(d, { Rg{ (uintptr_t)w1e::place, sizeof w1e::place }, Rg{ (uintptr_t)w1e::placeMat, 0x40 } }, [&] { return RB(p->IsWithinArea(a[0], a[1], a[2], a[3], a[4], a[5])); },
                    [&] { return RB(oracle::Fn<bool __fastcall(CPlaceable*, int, float, float, float, float, float, float)>(0x54F2B0)(p, 0, a[0], a[1], a[2], a[3], a[4], a[5])); }, "pos " + V(pos));
    });

    // ---- events: AffectsPed
    W1_EVENT_AFFECTS(CEventDanger, 0x4B5470, { e->m_dangerFrom = w1e::PickEnt(r, w); e->m_dangerRadius = r.below(4) ? r.f01() * 60.f : GF(r, 200.f); })
    W1_EVENT_AFFECTS(CEventCarUpsideDown, 0x4B1DB0, { e->m_vehicle = w.vehicle; })
    W1_EVENT_AFFECTS(CEventChatPartner, 0x4AEDC0, { e->m_partner = r.below(2) ? nullptr : w.ped; e->m_leadSpeaker = r.below(2); })
    W1_EVENT_AFFECTS(CEventDraggedOutCar, 0x4AD3A0, { e->m_CarJacker = r.below(2) ? nullptr : w.ped; })
    W1_EVENT_AFFECTS(CEventGotKnockedOverByCar, 0x4B1C70, { e->m_vehicle = r.below(2) ? nullptr : w.vehicle; })
    W1_EVENT_AFFECTS(CEventCopCarBeingStolen, 0x4B1860, { e->m_hijacker = r.below(3) ? w.ped : nullptr; e->m_vehicle = r.below(3) ? w.vehicle : nullptr; })
    Run("CEventBuildingCollision::IsHeadOnCollision 0x4AD1E0", [](Rng& r, std::string& d) {
        auto w = w1e::Setup(r); auto* e = reinterpret_cast<CEventBuildingCollision*>(w1e::ev);
        e->m_impactNormal = r.below(4) ? GVf(r, 1.f) : GVf(r, 1000.f);
        reinterpret_cast<CPlaceable*>(w.ped)->m_matrix = reinterpret_cast<CMatrixLink*>(w1e::pedMat);   // GetForward() needs the matrix (always present for a ped hit by a building)
        { float* f = reinterpret_cast<float*>(w1e::pedMat); const float h = r.f01() * 6.2831853f; f[4] = -std::sin(h); f[5] = std::cos(h); f[6] = GF(r, 0.5f); }   // forward = the matrix's second row
        // the ped's forward vector comes out of its matrix / heading
        return Both(d, { W1_ENT_RGS }, [&] { return RB(e->IsHeadOnCollision(w.ped)); }, [&] { return RB(oracle::Fn<bool __fastcall(CEventBuildingCollision*, int, CPed*)>(0x4AD1E0)(e, 0, w.ped)); }, "normal " + V(e->m_impactNormal) + " " + w1e::Ctx(w));
    });
}
