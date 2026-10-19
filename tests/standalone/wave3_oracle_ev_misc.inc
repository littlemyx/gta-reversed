// ORACLE-W3-EVENTS batch 2 (included by wave3_oracle_ev.inc): scanner, building collision, clones, copies, group allocator
#include "Events/EventBuildingCollision.h"
#include "Events/EventVehicleDied.h"
#include "Events/EventVehicleHitAndRun.h"
#include "Events/EventPedEnteredMyVehicle.h"
#include "Events/EventDamage.h"
#include "Events/EventKnockOffBike.h"
#include "Events/EventScriptedAttractor.h"
#include "PedType.h"
#include "Collision/ColModel.h"

static void TestW3EvMisc() {
    using namespace w2; using namespace w3e;
    // CPedAcquaintanceScanner::WantsToRiotAgainst 0x603AF0 (static, cdecl): ped / other from the reference world and the ped under test; the 8 ped groups hold random members (the lookup walks them), leader player or not
    Run("CPedAcquaintanceScanner::WantsToRiotAgainst 0x603AF0", [&](Rng& r, std::string& d) {
        CPed *ped = nullptr, *other = nullptr;
        return EvCase(r, d, [&](Rng& r) {
                for (size_t gi = 0; gi < CPedGroups::ms_groups.size(); ++gi) {
                    auto& mem = CPedGroups::ms_groups[gi].GetMembership();
                    auto& arr = *reinterpret_cast<W3EMembersArr*>(reinterpret_cast<uint8_t*>(&mem) + W3EMemMembers());
                    for (auto& m : arr) m = r.below(3) ? nullptr : (r.below(6) ? RPedR(r) : UnderTest());
                    CPedGroups::ms_activeGroups[gi] = (char)r.below(2);
                }
                ped = r.below(3) ? UnderTest() : RPedR(r);
                other = r.below(3) ? RPedR(r) : UnderTest();
                if (r.below(8) == 0) other = ped;
            },
            [&]() -> uint64_t { return B(CPedAcquaintanceScanner::WantsToRiotAgainst(ped, other)); },
            [&]() -> uint64_t { return B8((int)oracle::Fn<int __cdecl(void*, void*)>(0x603AF0)(ped, other)); });
    }, 300);
    // CEventBuildingCollision::CanTreatBuildingAsObject 0x4B3120 (static, cdecl): temp building flag, else the bounding box of the model's collision (edges at 6.0 / 2.0, NaN, inf)
    Run("CEventBuildingCollision::CanTreatBuildingAsObject 0x4B3120", [&](Rng& r, std::string& d) {
        CBuilding* b = nullptr;
        return EvCase(r, d, [&](Rng& r) {
                b = static_cast<CBuilding*>(rf::BldE((int)r.below((uint32)rf::kB)));
                b->m_bIsTempBuilding = r.below(4) == 0;
                b->m_nModelIndex = (uint16)g_model;
                auto edge = [&](float lim) -> float {
                    switch (r.below(9)) {
                    case 0: return lim; case 1: return std::nextafter(lim, 0.f); case 2: return std::nextafter(lim, 1e9f);
                    case 3: return std::numeric_limits<float>::quiet_NaN(); case 4: return std::numeric_limits<float>::infinity();
                    case 5: return 0.f; case 6: return -lim; default: return r.f01() * lim * 2.5f;
                    }
                };
                auto& bb = reinterpret_cast<CColModel*>(g_col)->m_boundBox;
                const float ox = (r.f01() - 0.5f) * 50.f, oy = (r.f01() - 0.5f) * 50.f, oz = (r.f01() - 0.5f) * 50.f;
                const float dx = edge(6.f), dy = edge(6.f), dz = edge(2.f);
                bb.m_vecMin = CVector(ox, oy, oz);
                bb.m_vecMax = CVector(ox + dx, oy + (r.below(5) == 0 ? -dy : dy), oz + dz);
            },
            [&]() -> uint64_t { return B(CEventBuildingCollision::CanTreatBuildingAsObject(b)); },
            [&]() -> uint64_t { return B8((int)oracle::Fn<int __cdecl(void*)>(0x4B3120)(b)); });
    }, 600);
    // Clone / CloneEditable of three events (real source objects built with the port's constructors)
    Run("CEventVehicleDied::Clone 0x4B76D0", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) { auto* e = ::new (static_cast<void*>(g_ev)) CEventVehicleDied(RVeh(r)); e->m_IsOccupantLeavingCar = r.below(2); },
            [&]() -> uint64_t { return (uint64_t)(uintptr_t)reinterpret_cast<CEventVehicleDied*>(g_ev)->CEventVehicleDied::Clone(); },
            [&]() -> uint64_t { return (uint64_t)(uintptr_t)oracle::Fn<void* __fastcall(void*, int)>(0x4B76D0)(g_ev, 0); });
    }, 60);
    Run("CEventVehicleHitAndRun::Clone 0x4B7100", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) { ::new (static_cast<void*>(g_ev)) CEventVehicleHitAndRun(RPedR(r), RVeh(r)); },
            [&]() -> uint64_t { return (uint64_t)(uintptr_t)reinterpret_cast<CEventVehicleHitAndRun*>(g_ev)->CEventVehicleHitAndRun::Clone(); },
            [&]() -> uint64_t { return (uint64_t)(uintptr_t)oracle::Fn<void* __fastcall(void*, int)>(0x4B7100)(g_ev, 0); });
    }, 60);
    Run("CEventPedEnteredMyVehicle::CloneEditable 0x4B7170", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) { ::new (static_cast<void*>(g_ev)) CEventPedEnteredMyVehicle(RPedR(r), RVeh(r), (eTargetDoor)r.below(6)); },
            [&]() -> uint64_t { return (uint64_t)(uintptr_t)reinterpret_cast<CEventPedEnteredMyVehicle*>(g_ev)->CEventPedEnteredMyVehicle::CloneEditable(); },
            [&]() -> uint64_t { return (uint64_t)(uintptr_t)oracle::Fn<void* __fastcall(void*, int)>(0x4B7170)(g_ev, 0); });
    }, 60);
    // CEventDamage copy constructor 0x4B33B0 / From 0x4AD9C0: the source is a real damage event with its trailing fields randomised
    auto damageSrc = [&](Rng& r, uint8_t* buf) {
        auto* e = ::new (static_cast<void*>(buf)) CEventDamage(RPedN(r), r.u32() >> r.below(20), (eWeaponType)r.below(48), (ePedPieceTypes)r.below(20), (uint8)r.below(8), r.below(2), r.below(2));
        FillGarbage(r, buf + sizeof(CEventEditableResponse), sizeof(CEventDamage) - sizeof(CEventEditableResponse));
        e->m_pSourceEntity = RPedN(r);
        e->m_bJumpedOutOfMovingCar = r.below(2); e->m_bFallDown = r.below(2); e->m_bAnimAdded = r.below(2); e->m_bWitnessedInVehicle = r.below(2); e->m_bStealthMode = r.below(2);
        e->m_unk26 = r.below(2); e->m_unk27 = r.below(2);
    };
    Run("CEventDamage::Constructor-copy 0x4B33B0", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) { damageSrc(r, g_ev2); g_evMaskVptr = true; },
            [&]() -> uint64_t { ::new (static_cast<void*>(g_ev)) CEventDamage(*reinterpret_cast<const CEventDamage*>(g_ev2)); return 0; },
            [&]() -> uint64_t { oracle::Fn<void* __fastcall(void*, int, void*)>(0x4B33B0)(g_ev, 0, g_ev2); return 0; });
    }, 120);
    Run("CEventDamage::From 0x4AD9C0", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) { damageSrc(r, g_ev2); damageSrc(r, g_ev); },
            [&]() -> uint64_t { reinterpret_cast<CEventDamage*>(g_ev)->CEventDamage::From(*reinterpret_cast<const CEventDamage*>(g_ev2)); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*)>(0x4AD9C0)(g_ev, 0, g_ev2); return 0; });
    }, 120);
    // CEventKnockOffBike::From 0x4AFDD0: copies the knock-off fields, registers the vehicle and the ped
    auto knockSrc = [&](Rng& r, uint8_t* buf) {
        auto* e = ::new (static_cast<void*>(buf)) CEventKnockOffBike();
        FillGarbage(r, buf + sizeof(CEvent), sizeof(CEventKnockOffBike) - sizeof(CEvent));
        e->m_vehicle = r.below(6) ? RVeh(r) : nullptr; e->m_ped = RPedN(r);
        e->m_isVictimDriver = r.below(2); e->m_forceKnockOff = r.below(2);
    };
    Run("CEventKnockOffBike::From 0x4AFDD0", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) { knockSrc(r, g_ev2); knockSrc(r, g_ev); },
            [&]() -> uint64_t { reinterpret_cast<CEventKnockOffBike*>(g_ev)->CEventKnockOffBike::From(*reinterpret_cast<const CEventKnockOffBike*>(g_ev2)); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*)>(0x4AFDD0)(g_ev, 0, g_ev2); return 0; });
    }, 120);
    // CEventScriptedAttractor constructor 0x5FEF40: the exe takes (2d effect, entity) only (`ret 8`); the port's third parameter is ignored
    Run("CEventScriptedAttractor::Constructor 0x5FEF40", [&](Rng& r, std::string& d) {
        C2dEffectPedAttractor* eff = reinterpret_cast<C2dEffectPedAttractor*>(g_clump); CEntity* ent = r.below(5) ? static_cast<CEntity*>(RPedR(r)) : nullptr;
        return EvCase(r, d, [&](Rng& r) { g_evMaskVptr = true; },
            [&]() -> uint64_t { ::new (static_cast<void*>(g_ev)) CEventScriptedAttractor(eff, ent, false); return 0; },
            [&]() -> uint64_t { oracle::Fn<void* __fastcall(void*, int, void*, void*)>(0x5FEF40)(g_ev, 0, eff, ent); return 0; });
    }, 60);
    // CGroupEventHandler::ComputeDrivebyResponse 0x5F7A00 (static, cdecl): a new killer-driveby allocator for the threat
    Run("CGroupEventHandler::ComputeDrivebyResponse 0x5F7A00", [&](Rng& r, std::string& d) {
        CPed* threat = nullptr;
        return EvCase(r, d, [&](Rng& r) { FillGroup(r, true); threat = RPedN(r); },
            [&]() -> uint64_t { return (uint64_t)(uintptr_t)CGroupEventHandler::ComputeDrivebyResponse(reinterpret_cast<CPedGroup*>(g_grp), threat, UnderTest()); },
            [&]() -> uint64_t { return (uint64_t)(uintptr_t)oracle::Fn<void* __cdecl(void*, void*, void*)>(0x5F7A00)(g_grp, threat, UnderTest()); });
    }, 60);
}
