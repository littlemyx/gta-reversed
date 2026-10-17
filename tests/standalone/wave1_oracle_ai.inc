// ORACLE-W1 / ai: small AI bookkeeping objects - CPlayerRelationshipRecorder, CStuckCarCheck, CPedGroupMembership queries, CCarAI::FindSwitchDistance*.
// Objects are static buffers filled with garbage; ped / car pointers are drawn from a tiny pool so matches are frequent; both sides work on the same buffer (restored between the two calls).
#include "PlayerRelationshipRecorder.h"
#include "StuckCarCheck.h"
#include "PedGroupMembership.h"
#include "CarAI.h"

namespace w1ai {
inline const CPed* PedPool(Rng& r) { return r.below(3) ? nullptr : reinterpret_cast<const CPed*>(0x1000u + r.below(5) * 0x40); }
alignas(16) inline uint8 rel[0x80];
alignas(16) inline uint8 stuck[0x240];
alignas(16) inline uint8 mem[0x28];
alignas(16) inline uint8 veh[0x800];
}

static void TestAi() {
    // ---- CPlayerRelationshipRecorder
    auto setupRel = [](Rng& r) {
        FillGarbage(r, w1ai::rel, sizeof w1ai::rel);
        auto* a = reinterpret_cast<CPlayerRelationshipRecorder*>(w1ai::rel);
        for (auto& e : a->m_Relationships) e.Ped = r.below(2) ? w1ai::PedPool(r) : nullptr;
        return a;
    };
    Run("CPlayerRelationshipRecorder::Constructor 0x61A130", [&](Rng& r, std::string& d) {
        setupRel(r);
        return Both(d, { Rg{ (uintptr_t)w1ai::rel, sizeof w1ai::rel } }, [&] { new (w1ai::rel) CPlayerRelationshipRecorder(); return 0ull; }, [&] { oracle::Fn<void __fastcall(void*, int)>(0x61A130)(w1ai::rel, 0); return 0ull; });
    });
    Run("CPlayerRelationshipRecorder::Destructor 0x61A2C0", [&](Rng& r, std::string& d) {
        auto* a = setupRel(r);
        return Both(d, { Rg{ (uintptr_t)w1ai::rel, sizeof w1ai::rel } }, [&] { a->~CPlayerRelationshipRecorder(); return 0ull; }, [&] { oracle::Fn<void __fastcall(void*, int)>(0x61A2C0)(w1ai::rel, 0); return 0ull; });
    });
    Run("CPlayerRelationshipRecorder::Flush 0x61A2A0", [&](Rng& r, std::string& d) {
        auto* a = setupRel(r);
        return Both(d, { Rg{ (uintptr_t)w1ai::rel, sizeof w1ai::rel } }, [&] { a->Flush(); return 0ull; }, [&] { oracle::Fn<void __fastcall(void*, int)>(0x61A2A0)(w1ai::rel, 0); return 0ull; });
    });
    Run("CPlayerRelationshipRecorder::ClearRelationshipWithPlayer 0x61A150", [&](Rng& r, std::string& d) {
        auto* a = setupRel(r); const CPed* p = w1ai::PedPool(r);
        return Both(d, { Rg{ (uintptr_t)w1ai::rel, sizeof w1ai::rel } }, [&] { a->ClearRelationshipWithPlayer(p); return 0ull; }, [&] { oracle::Fn<void __fastcall(void*, int, const CPed*)>(0x61A150)(w1ai::rel, 0, p); return 0ull; });
    });
    Run("CPlayerRelationshipRecorder::AddRelationship 0x61A180", [&](Rng& r, std::string& d) {
        auto* a = setupRel(r); const CPed* p = w1ai::PedPool(r); const int v = r.below(2) ? r.below(10) : (int)r.u32();
        return Both(d, { Rg{ (uintptr_t)w1ai::rel, sizeof w1ai::rel } }, [&] { a->AddRelationship(p, v); return 0ull; }, [&] { oracle::Fn<void __fastcall(void*, int, const CPed*, int)>(0x61A180)(w1ai::rel, 0, p, v); return 0ull; });
    });
    Run("CPlayerRelationshipRecorder::GetRelationshipWithPlayer 0x61A1A0", [&](Rng& r, std::string& d) {
        auto* a = setupRel(r); const CPed* p = w1ai::PedPool(r);
        return Both(d, { Rg{ (uintptr_t)w1ai::rel, sizeof w1ai::rel } }, [&] { return RI(a->GetRelationshipWithPlayer(p)); }, [&] { return RI(oracle::Fn<uint8 __fastcall(void*, int, const CPed*)>(0x61A1A0)(w1ai::rel, 0, p)); });
    });

    // ---- CStuckCarCheck (16 records of 36 bytes: handle, last pos, ..., five canonical bools)
    auto setupStuck = [](Rng& r) {
        FillGarbage(r, w1ai::stuck, sizeof w1ai::stuck);
        auto* s = reinterpret_cast<CStuckCarCheck*>(w1ai::stuck);
        for (auto& c : s->m_aStuckCars) {
            c.CarHandle = r.below(3) ? (int32)r.below(6) - 1 : (int32)r.u32();
            c.CarStuck = r.below(2); c.WarpCar = r.below(2); c.WarpIfStuck = r.below(2); c.WarpIfFlipped = r.below(2); c.WarpIfInWater = r.below(2);
        }
        return s;
    };
    Run("CStuckCarCheck::Init 0x4639E0", [&](Rng& r, std::string& d) {
        auto* s = setupStuck(r);
        return Both(d, { Rg{ (uintptr_t)w1ai::stuck, sizeof w1ai::stuck } }, [&] { s->Init(); return 0ull; }, [&] { oracle::Fn<void __fastcall(void*, int)>(0x4639E0)(w1ai::stuck, 0); return 0ull; });
    });
    Run("CStuckCarCheck::RemoveCarFromCheck 0x463B80", [&](Rng& r, std::string& d) {
        auto* s = setupStuck(r); const int h = (int)r.below(8) - 1;
        return Both(d, { Rg{ (uintptr_t)w1ai::stuck, sizeof w1ai::stuck } }, [&] { s->RemoveCarFromCheck(h); return 0ull; }, [&] { oracle::Fn<void __fastcall(void*, int, int)>(0x463B80)(w1ai::stuck, 0, h); return 0ull; }, "handle " + std::to_string(h));
    });
    Run("CStuckCarCheck::ClearStuckFlagForCar 0x463C40", [&](Rng& r, std::string& d) {
        auto* s = setupStuck(r); const int h = (int)r.below(8) - 1;
        return Both(d, { Rg{ (uintptr_t)w1ai::stuck, sizeof w1ai::stuck } }, [&] { s->ClearStuckFlagForCar(h); return 0ull; }, [&] { oracle::Fn<void __fastcall(void*, int, int)>(0x463C40)(w1ai::stuck, 0, h); return 0ull; }, "handle " + std::to_string(h));
    });
    Run("CStuckCarCheck::HasCarBeenStuckForAWhile 0x463C00", [&](Rng& r, std::string& d) {
        auto* s = setupStuck(r); const int h = (int)r.below(8) - 1;
        return Both(d, { Rg{ (uintptr_t)w1ai::stuck, sizeof w1ai::stuck } }, [&] { return RB(s->HasCarBeenStuckForAWhile(h)); }, [&] { return RB(oracle::Fn<bool __fastcall(void*, int, int)>(0x463C00)(w1ai::stuck, 0, h)); }, "handle " + std::to_string(h));
    });
    Run("CStuckCarCheck::IsCarInStuckCarArray 0x463C70", [&](Rng& r, std::string& d) {
        auto* s = setupStuck(r); const int h = (int)r.below(8) - 1;
        return Both(d, { Rg{ (uintptr_t)w1ai::stuck, sizeof w1ai::stuck } }, [&] { return RB(s->IsCarInStuckCarArray(h)); }, [&] { return RB(oracle::Fn<bool __fastcall(void*, int, int)>(0x463C70)(w1ai::stuck, 0, h)); }, "handle " + std::to_string(h));
    });

    // ---- CPedGroupMembership (group ptr, 8 member slots - the last one is the leader -, separation range)
    auto setupMem = [](Rng& r) {
        FillGarbage(r, w1ai::mem, sizeof w1ai::mem);
        auto* m = reinterpret_cast<CPedGroupMembership*>(w1ai::mem);
        auto** slots = reinterpret_cast<const CPed**>(w1ai::mem + 4);
        for (int i = 0; i < 8; ++i) slots[i] = r.below(3) ? nullptr : reinterpret_cast<const CPed*>(0x1000u + r.below(6) * 0x40);
        return m;
    };
    Run("CPedGroupMembership::CountMembers 0x5F6A50", [&](Rng& r, std::string& d) {
        auto* m = setupMem(r);
        return Both(d, { Rg{ (uintptr_t)w1ai::mem, sizeof w1ai::mem } }, [&] { return RI(m->CountMembers()); }, [&] { return RI(oracle::Fn<int __fastcall(void*, int)>(0x5F6A50)(w1ai::mem, 0)); });
    });
    Run("CPedGroupMembership::CountMembersExcludingLeader 0x5F6AA0", [&](Rng& r, std::string& d) {
        auto* m = setupMem(r);
        return Both(d, { Rg{ (uintptr_t)w1ai::mem, sizeof w1ai::mem } }, [&] { return RI(m->CountMembersExcludingLeader()); }, [&] { return RI(oracle::Fn<int __fastcall(void*, int)>(0x5F6AA0)(w1ai::mem, 0)); });
    });
    Run("CPedGroupMembership::IsLeader 0x5F69C0", [&](Rng& r, std::string& d) {
        auto* m = setupMem(r); const CPed* p = r.below(5) ? reinterpret_cast<const CPed*>(0x1000u + r.below(6) * 0x40) : nullptr;
        return Both(d, { Rg{ (uintptr_t)w1ai::mem, sizeof w1ai::mem } }, [&] { return RB(m->IsLeader(p)); }, [&] { return RB(oracle::Fn<bool __fastcall(void*, int, const CPed*)>(0x5F69C0)(w1ai::mem, 0, p)); });
    });
    Run("CPedGroupMembership::IsFollower 0x5F69E0", [&](Rng& r, std::string& d) {
        auto* m = setupMem(r); const CPed* p = r.below(5) ? reinterpret_cast<const CPed*>(0x1000u + r.below(6) * 0x40) : nullptr;
        return Both(d, { Rg{ (uintptr_t)w1ai::mem, sizeof w1ai::mem } }, [&] { return RB(m->IsFollower(p)); }, [&] { return RB(oracle::Fn<bool __fastcall(void*, int, const CPed*)>(0x5F69E0)(w1ai::mem, 0, p)); });
    });
    Run("CPedGroupMembership::IsMember 0x5F6A10", [&](Rng& r, std::string& d) {
        auto* m = setupMem(r); const CPed* p = r.below(5) ? reinterpret_cast<const CPed*>(0x1000u + r.below(6) * 0x40) : nullptr;
        return Both(d, { Rg{ (uintptr_t)w1ai::mem, sizeof w1ai::mem } }, [&] { return RB(m->IsMember(p)); }, [&] { return RB(oracle::Fn<bool __fastcall(void*, int, const CPed*)>(0x5F6A10)(w1ai::mem, 0, p)); });
    });

    // ---- CCarAI::FindSwitchDistance* (m_autoPilot.m_nStraightLineDistance, vehicleFlags.bIsLawEnforcer)
    auto setupVeh = [](Rng& r) { FillGarbage(r, w1ai::veh, sizeof w1ai::veh); return reinterpret_cast<CVehicle*>(w1ai::veh); };
    Run("CCarAI::FindSwitchDistanceClose 0x41BF50", [&](Rng& r, std::string& d) {
        auto* v = setupVeh(r);
        return Both<true>(d, { Rg{ (uintptr_t)w1ai::veh, sizeof(CVehicle) } }, [&] { return RF(CCarAI::FindSwitchDistanceClose(v)); }, [&] { return RF(oracle::Fn<float __cdecl(CVehicle*)>(0x41BF50)(v)); });
    });
    Run("CCarAI::FindSwitchDistanceFar 0x41BF70", [&](Rng& r, std::string& d) {
        auto* v = setupVeh(r);
        return Both<true>(d, { Rg{ (uintptr_t)w1ai::veh, sizeof(CVehicle) } }, [&] { return RF(CCarAI::FindSwitchDistanceFar(v)); }, [&] { return RF(oracle::Fn<float __cdecl(CVehicle*)>(0x41BF70)(v)); });
    });
    Run("CCarAI::FindPoliceBikeMissionForWantedLevel 0x41CA40", [&](Rng& r, std::string& d) {
        (void)r;
        return Both(d, { Rg{ 0xB7CB84, 4 } }, [&] { return RI((uint8)CCarAI::FindPoliceBikeMissionForWantedLevel()); }, [&] { return RI((uint8)oracle::Fn<int __cdecl()>(0x41CA40)()); });   // the exe sets AL only (`mov al, 43`)
    });
}
