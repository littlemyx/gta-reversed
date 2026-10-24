#pragma once   // ORACLE-C-EVENTS: wave3_oracle_tasks.inc (fixture of the generated class-C rows) includes this file for its fixture part; the events target includes it first
// ORACLE-W3-EVENTS: exe-oracle rows for game_sa/Events (classes A and B of .notes/ORACLE_COVERAGE.md). Included by wave2_oracle_all.inc when W3_EV_ONLY is defined
// (executable wave3_events_oracle_test = the W2 harness: deterministic arena, fake pools, entity reference world, whole exe data image compared after every case).
//
// Fixture (EvCase): the PED under test is the wired ped of wave3_oracle_pedfx.inc (g_src, also CWorld::Players[0]); the reference world's other peds / vehicles get plausible fields (ped type, creator,
// area code, weapons, intelligence); `this` objects are real objects built with the port's constructors or buffers filled field by field; events handed to the code are real objects (TEv: an event
// with a controlled type / priority). The exe data image is restored after every case.
#include "wave3_oracle_pedfx.inc"
#include "Events/EventStuckInAir.h"
#include "Events/EventFireNearby.h"
#include "Events/EventLeaderEntryExit.h"
#include "Events/EventObjectCollision.h"
#include "Events/EventDraggedOutCar.h"
#include "Events/EventPlayerCommandToGroup.h"
#include "Events/EventPlayerCommandToGroupAttack.h"
#include "Events/EventHandler.h"
#include "Events/EventGroup.h"
#include "Events/GroupEventHandler.h"
#include "Events/EventScanner.h"
#include "Pools/TaskAllocatorPool.h"
#include "PedGroup.h"
#include "PedGroups.h"
#include "Models/BaseModelInfo.h"

// NOTSA_UNREACHABLE: Fixups::UnreachableHit lives in wave1_oracle_test.cpp (TRAP-AUDIT: W2_PORT_FAULT_IS_DIFF=1 falls through like the run build, otherwise the old fault at 0x400000)

#define W3E_ROB(Name, Cls, Type, Mem) \
    struct Name##_T { using type = Type Cls::*; friend type W2Get(Name##_T); }; template struct W2Rob<Name##_T, &Cls::Mem>; \
    static size_t Name() { auto pm = W2Get(Name##_T{}); uint32_t o = 0; static_assert(sizeof pm == 4); std::memcpy(&o, &pm, 4); return o; }
W3E_ROB(W3EHdlPed, CEventHandler, CPed*, m_Ped)
W3E_ROB(W3EHdlResp, CEventHandler, CTask*, m_EventResponseTask)
W3E_ROB(W3EHdlPartial, CEventHandler, CTask*, m_PartialAnimTask)
using W3EMembersArr = std::array<CPed*, TOTAL_PED_GROUP_MEMBERS>;
W3E_ROB(W3EMemMembers, CPedGroupMembership, W3EMembersArr, m_members)

// an event with a controlled type and priority (CEvent::TakesPriorityOver and friends read them through the virtuals)
struct TEv final : CEvent {
    eEventType type = EVENT_NONE; int32 prio = 0;
    eEventType GetEventType() const override { return type; }
    int32 GetEventPriority() const override { return prio; }
    int32 GetLifeTime() override { return 0; }
    CEvent* Clone() const noexcept override { return nullptr; }
};

W3_PORT_VT(CPlayerPed)
namespace w3e {
using namespace w2; using namespace w3p;
// ORACLE-C-EVENTS: class C rows with a hand-built event: a row's setup sets g_evVt, EvCase then writes the EXE's vftable of the event class into word 0 of the event g_ev and of the ped under test (CPlayerPed 0x86D168) before the exe call
// and the PORT's vftables (??_7 through /alternatename) before the port call, and clears both afterwards
struct EvVt { uint32_t exeVt; const uint32_t* portVt; };
static const EvVt* g_evVt = nullptr;
alignas(16) static uint8_t g_wanted2[0x800]; // the CWanted of the second player (reference ped 0 in some cases)
alignas(16) static uint8_t g_hdl[0x100];     // CEventHandler
alignas(16) static uint8_t g_ev[0x200];      // the event under test (garbage or a real object)
alignas(16) static uint8_t g_ev2[0x200];     // a second event
alignas(16) static uint8_t g_evg[0x80];      // CEventGroup
alignas(16) static TEv g_tev[4];             // controlled events
alignas(16) static uint8_t g_grp[0x300];     // CPedGroup (0x2D4)
alignas(16) static uint8_t g_ints[rf::kP][0x800];   // CPedIntelligence of the reference world's peds
static CTask* g_tact = nullptr; static CTask* g_tsimp = nullptr;
static PoolFx<CTaskAllocatorPool, 32, 0xB744BC> g_tap;   // the task allocator pool (CTaskAllocator::operator new)
static uint8_t g_tpMaskBytes[0x80];   // task bytes of the pool slots that are not compared (embedded vptrs, padding): set by a row
static int g_evTag = -1;   // diagnostic: printed with a failing case
static bool g_evMaskVptr = false;                        // the object in g_ev is constructed by the code under test: its vptr (port vs exe vtable) is not compared
static_assert(sizeof(CEventHandler) <= sizeof(g_hdl) && sizeof(CPedGroup) <= sizeof(g_grp) && sizeof(CEventGroup) <= sizeof(g_evg));

static CPed* RPed(int i) { return static_cast<CPed*>(rf::PedE(i)); }
static CPed* RPedR(Rng& r) { return RPed((int)r.below((uint32)rf::kP)); }
static CPed* RPedN(Rng& r) { return r.below(6) == 0 ? nullptr : RPedR(r); }
static CVehicle* RVeh(Rng& r) { return static_cast<CVehicle*>(rf::VehE((int)r.below((uint32)rf::kV))); }
static CPed* UnderTest() { return reinterpret_cast<CPed*>(g_src); }

// the reference world's peds / vehicles as the event code sees them
static void WireRef(Rng& r) {
    for (int i = 0; i < rf::kP; ++i) {
        CPed* p = RPed(i);
        std::memset(g_ints[i], 0, sizeof g_ints[i]);
        reinterpret_cast<CPedIntelligence*>(g_ints[i])->m_pPed = p;
        Put32(reinterpret_cast<uint8_t*>(p), W3POffOf(W2Get(W3POffIntel_T{})), (uint32_t)(uintptr_t)g_ints[i]);
        p->m_nPedType = (ePedType)(2 + r.below(30));   // never a player type: a player ped has player data (the exe dereferences it), only ped 0 may be the second player (below)
        p->SetCreatedBy((ePedCreatedBy)r.below(3));
        p->SetAreaCode((eAreaCodes)r.below(3));
        p->bDoesntListenToPlayerGroupCommands = r.below(2);
        p->m_nActiveWeaponSlot = (uint8)r.below(13);
        for (auto& w : p->m_aWeapons) { w.m_Type = (eWeaponType)(r.below(3) ? 0 : r.below(48)); }
    }
    if (r.below(2)) {   // reference ped 0 is the second player, with its own CWanted
        std::memcpy(g_wanted2, g_wanted, sizeof g_wanted2);
        auto& w = *reinterpret_cast<CWanted*>(g_wanted2);
        w.m_ChaosLevel = (int32)r.below(5200); w.m_WantedLevel = (eWantedLevel)r.below(7); w.m_LastTimeWantedLevelChanged = r.below(100000);
        auto& pi = CWorld::Players[1];
        FillGarbage(r, &pi, sizeof pi);
        pi.m_PlayerData.m_pWanted = reinterpret_cast<CWanted*>(g_wanted2);
        pi.m_PlayerData.m_pPedClothesDesc = reinterpret_cast<CPedClothesDesc*>(g_clo);
        pi.m_pPed = static_cast<CPlayerPed*>(RPed(0));
        RPed(0)->m_nPedType = PED_TYPE_PLAYER2;
        Put32(reinterpret_cast<uint8_t*>(RPed(0)), W3POffOf(W2Get(W3POffPData_T{})), (uint32_t)(uintptr_t)&pi.m_PlayerData);
    }
    for (int i = 0; i < rf::kV; ++i) {
        auto* v = static_cast<CVehicle*>(rf::VehE(i));
        v->m_pDriver = RPedN(r);
        for (auto& pp : v->m_apPassengers) { pp = r.below(3) ? nullptr : RPedR(r); }
    }
}
// weapon infos: the melee test reads CWeaponInfo::m_nWeaponFire of the weapon type (the table is the port's global at the exe address; the image is restored after the case)
static void WireWeaponInfos(Rng& r) {
    for (int t = 0; t < 80; ++t) {
        auto& wi = CWeaponInfo::aWeaponInfo[t];
        wi.m_nWeaponFire = (eWeaponFire)(r.below(3) == 0 ? WEAPON_FIRE_MELEE : r.below(6));
    }
}
// the ped under test: in a vehicle or not, with a valid vehicle pointer
static void WireUnderTest(Rng& r) {
    CPed* p = UnderTest();
    CVehicle* v = r.below(3) ? RVeh(r) : nullptr;
    p->m_pVehicle = v;
    p->m_pReferences = nullptr;   // the ped is a registration target (tasks aimed at the player): an empty reference list, not garbage
    p->bInVehicle = v && r.below(4) != 0;
    p->SetCreatedBy((ePedCreatedBy)r.below(3));
    p->SetAreaCode((eAreaCodes)r.below(3));
}
static void ClearHandler() {
    std::memset(g_hdl, 0, sizeof g_hdl);
    Put32(g_hdl, W3EHdlPed(), (uint32_t)(uintptr_t)g_src);
}
static void SetEvVt(int side) {   // side 1 = port, 2 = exe, 0 = clear
    if (!g_evVt) return;
    Put32(g_ev, 0, side == 1 ? (uint32_t)(uintptr_t)g_evVt->portVt : side == 2 ? g_evVt->exeVt : 0);
    Put32(g_src, 0, side == 1 ? (uint32_t)(uintptr_t)w3vt_CPlayerPed : side == 2 ? 0x86D168u : 0u);
}
static void NewTevs(Rng& r) {
    for (auto& e : g_tev) { ::new (static_cast<void*>(&e)) TEv(); e.type = (eEventType)(r.below(4) ? (int)r.below(120) : 93); e.prio = (int32)(r.below(2) ? 70 + r.below(10) : r.below(120)); }
}

// one case: fresh worlds, a wired ped (g_src), the reference peds, then `setup` builds the objects of the row; compared after the call: return value, every fixture buffer, pools, arena, the reference world, the whole exe data image
template<class SF, class PF, class EF>
static bool EvCase(Rng& r, std::string& d, SF&& setup, PF&& portFn, EF&& exeFn) {
    static std::vector<uint8_t> img0(kImgHi - kImgLo);
    std::memcpy(img0.data(), (void*)kImgLo, img0.size());
    const uint32_t seed = Prep(r, 0);
    g_tap.Prepare(r, true); g_tap.Enter();
    FillGarbage(r, g_anim, sizeof g_anim);
    g_subTask = nullptr; g_tact = g_tsimp = nullptr;
    FillGarbage(r, g_src, (sizeof(CCopPed) + 3) & ~size_t(3));
    WirePed(r, g_src, sizeof(CPlayerPed));
    // exe statics the CRT start-up would have initialised (the harness does not run it): EntityPosChangeThreshold 0xC18CF0 = 1.0f, fSafeDistance 0xC18CEC = 60.0f (flee task arguments)
    *reinterpret_cast<volatile float*>(0xC18CF0) = 1.0f; *reinterpret_cast<volatile float*>(0xC18CEC) = 60.0f; *reinterpret_cast<volatile float*>(0xC18CE4) = 60.0f;
    WireRef(r); WireUnderTest(r); WireWeaponInfos(r); NewTevs(r); ClearHandler();
    FillGarbage(r, g_ev, sizeof g_ev); FillGarbage(r, g_ev2, sizeof g_ev2); FillGarbage(r, g_grp, sizeof g_grp);
    std::memset(g_evg, 0, sizeof g_evg);
    g_evMaskVptr = false; g_evTag = -1; g_evVt = nullptr; std::memset(g_tpMaskBytes, 0, sizeof g_tpMaskBytes);
    setup(r);
    Reg tpStore = g_tp.RegStore();
    if (std::memchr(g_tpMaskBytes, 1, sizeof g_tpMaskBytes)) tpStore.mask.assign(g_tpMaskBytes, g_tpMaskBytes + 0x80);
    Reg evReg{ (uintptr_t)g_ev, sizeof g_ev };
    if (g_evMaskVptr) { evReg.mask.assign(sizeof g_ev, 0); for (int k = 0; k < 4; ++k) evReg.mask[k] = 1; }
    const bool ok = BothA(d, seed, { Reg{ (uintptr_t)g_src, sizeof(CCopPed) }, Reg{ (uintptr_t)g_hdl, sizeof g_hdl }, evReg, Reg{ (uintptr_t)g_ev2, sizeof g_ev2 }, Reg{ (uintptr_t)g_evg, sizeof g_evg },
            Reg{ (uintptr_t)g_tev, sizeof g_tev }, Reg{ (uintptr_t)g_grp, sizeof g_grp }, Reg{ (uintptr_t)g_ints, sizeof g_ints },
            Reg{ (uintptr_t)g_mat, sizeof g_mat }, Reg{ (uintptr_t)g_mi, sizeof g_mi }, Reg{ (uintptr_t)g_col, sizeof g_col }, Reg{ (uintptr_t)g_intel, sizeof g_intel }, Reg{ (uintptr_t)g_stat, sizeof g_stat },
            Reg{ (uintptr_t)g_wanted, sizeof g_wanted }, Reg{ (uintptr_t)g_wanted2, sizeof g_wanted2 }, Reg{ (uintptr_t)g_clo, sizeof g_clo },
            g_tp.RegStruct(), g_tp.RegFlags(), tpStore, g_ep.RegStruct(), g_ep.RegFlags(), g_ep.RegStore(), g_tap.RegStruct(), g_tap.RegFlags(), g_tap.RegStore(), Reg{ (uintptr_t)rf::pp, sizeof(CPedPool) }, Reg{ (uintptr_t)rf::pedSt, (size_t)rf::kP }, Reg{ kImgLo, kImgHi - kImgLo } },
        [&]() -> uint64_t { SetEvVt(1); const uint64_t rv = portFn(); SetEvVt(0); return rv; }, [&]() -> uint64_t { SetEvVt(2); const uint64_t rv = exeFn(); SetEvVt(0); return rv; }, kRf);
    g_evVt = nullptr;
    if (!ok && g_evTag >= 0) { char t[48]; std::snprintf(t, sizeof t, " [tag %d]", g_evTag); d += t; }
    g_tap.Leave();
    Done();
    UnwireModel();
    std::memcpy((void*)kImgLo, img0.data(), img0.size());
    return ok;
}
// ORACLE-C-EVENTS: an event argument with a controlled type / priority (slot 0..3 of g_tev, a fresh TEv each time: the port's vftable on both sides, the exe calls its virtuals through it)
static CEvent* NewTev(Rng& r, int slot) {
    auto* e = ::new (static_cast<void*>(&g_tev[slot & 3])) TEv();
    e->type = (eEventType)(r.below(4) ? (int)r.below(120) : 93); e->prio = (int32)(r.below(2) ? 70 + r.below(10) : r.below(120));
    return e;
}
static uint64_t B(bool v) { return (uint64_t)(uint8_t)(v ? 1 : 0); }
static uint64_t B8(int v) { return (uint64_t)(uint8_t)v; }
} // namespace w3e

#ifndef W3_EV_FIXTURE_ONLY   // ORACLE-W3-TASKS includes this file for the fixture only (EvCase, the wired reference world, TEv, the task allocator pool)
static void TestW3EvA() {
    using namespace w2; using namespace w3e;
    // ---- TakesPriorityOver that answers true without looking (the class is `return true` in the port): 0x4B1FC0 FireNearby, 0x4B2390 LeaderEntryExit, 0x4ACEF0 ObjectCollision
#define TPO_TRUE(Cls, Addr) \
    Run(#Cls "::TakesPriorityOver " #Addr, [&](Rng& r, std::string& d) { \
        return EvCase(r, d, [&](Rng& r) { auto* e = ::new (static_cast<void*>(g_ev2)) TEv(); e->type = (eEventType)r.below(120); e->prio = (int32)r.below(120); }, \
            [&]() -> uint64_t { return B(reinterpret_cast<Cls*>(g_ev)->Cls::TakesPriorityOver(*reinterpret_cast<CEvent*>(g_ev2))); }, \
            [&]() -> uint64_t { return B8((int)oracle::Fn<int __fastcall(void*, int, void*)>(Addr)(g_ev, 0, g_ev2)); }); }, 30);
    TPO_TRUE(CEventFireNearby, 0x4B1FC0)
    TPO_TRUE(CEventLeaderEntryExit, 0x4B2390)
    TPO_TRUE(CEventObjectCollision, 0x4ACEF0)
#undef TPO_TRUE
    // ---- CEventStuckInAir::TakesPriorityOver 0x4B15B0: `this` is a real event (the base compare calls GetEventPriority), the reference event has a controlled type / priority (73 / 74 / 75 edge, type 93)
    Run("CEventStuckInAir::TakesPriorityOver 0x4B15B0", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) {
                ::new (static_cast<void*>(g_ev)) CEventStuckInAir(RPedR(r));
                auto* e = ::new (static_cast<void*>(g_ev2)) TEv();
                e->type = (eEventType)(r.below(3) ? 93 : r.below(120));
                static constexpr int kPr[] = { 0, 73, 74, 75, 76, 100, 65, 70 };
                e->prio = r.below(4) ? kPr[r.below(8)] : (int32)r.below(130);
            },
            [&]() -> uint64_t { return B(reinterpret_cast<CEventStuckInAir*>(g_ev)->CEventStuckInAir::TakesPriorityOver(*reinterpret_cast<CEvent*>(g_ev2))); },
            [&]() -> uint64_t { return B8((int)oracle::Fn<int __fastcall(void*, int, void*)>(0x4B15B0)(g_ev, 0, g_ev2)); });
    }, 120);
    // ---- CEventStuckInAir::AffectsPed 0x4B1580: ped.bIsStanding, then the stuck checker state of the ped's intelligence
    Run("CEventStuckInAir::AffectsPed 0x4B1580", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) {
                ::new (static_cast<void*>(g_ev)) CEventStuckInAir(RPedR(r));
                UnderTest()->bIsStanding = r.below(2);
                reinterpret_cast<CPedIntelligence*>(g_intel)->m_pedStuckChecker.m_state = (ePedStuckState)(r.below(3) ? 0 : r.below(6));
            },
            [&]() -> uint64_t { return B(reinterpret_cast<CEventStuckInAir*>(g_ev)->CEventStuckInAir::AffectsPed(UnderTest())); },
            [&]() -> uint64_t { return B8((int)oracle::Fn<int __fastcall(void*, int, void*)>(0x4B1580)(g_ev, 0, UnderTest())); });
    }, 120);
}

// a ped group in g_grp: members (0..6 followers, 7 leader) from the reference peds / the ped under test, mission flag
static void FillGroup(Rng& r, bool leaderPlayer) {
    using namespace w3e;
    FillGarbage(r, g_grp, sizeof g_grp);
    auto* g = reinterpret_cast<CPedGroup*>(g_grp);
    auto& mem = g->GetMembership();
    auto& arr = *reinterpret_cast<std::array<CPed*, TOTAL_PED_GROUP_MEMBERS>*>(reinterpret_cast<uint8_t*>(&mem) + W3EMemMembers());
    for (int i = 0; i < TOTAL_PED_GROUP_FOLLOWERS; ++i) arr[i] = r.below(3) ? nullptr : (r.below(8) ? RPedR(r) : UnderTest());
    arr[CPedGroupMembership::LEADER_MEM_ID] = r.below(8) == 0 ? nullptr : (leaderPlayer && r.below(2) ? UnderTest() : RPedR(r));
    g->m_bIsMissionGroup = r.below(2);
}

static void TestW3EvGroup() {
    using namespace w2; using namespace w3e;
    // ---- CEventGroup: a real group (vptr) with m_count events out of the TEv objects / null / an absent pointer
    auto fillGroup = [&](Rng& r, bool allowNull) {
        auto* g = ::new (static_cast<void*>(g_evg)) CEventGroup(r.below(3) ? RPedR(r) : nullptr);
        g->m_count = r.below(4) == 0 ? (r.below(2) ? 16 : 0) : r.below(17);
        for (auto& e : g->m_events) {
            const uint32 k = r.below(allowNull ? 7 : 4);
            e = k < 4 ? &g_tev[k] : (k == 4 ? nullptr : (CEvent*)(uintptr_t)(0x1000 + 0x10 * r.below(4)));
        }
    };
    Run("CEventGroup::HasEvent 0x4AB6A0", [&](Rng& r, std::string& d) {
        CEvent* needle = nullptr;
        return EvCase(r, d, [&](Rng& r) { fillGroup(r, true); const uint32 k = r.below(7); needle = k < 4 ? &g_tev[k] : (k == 4 ? nullptr : (CEvent*)(uintptr_t)0x1000); },
            [&]() -> uint64_t { return B(reinterpret_cast<CEventGroup*>(g_evg)->CEventGroup::HasEvent(needle)); },
            [&]() -> uint64_t { return B8((int)oracle::Fn<int __fastcall(void*, int, void*)>(0x4AB6A0)(g_evg, 0, needle)); });
    }, 120);
    Run("CEventGroup::TickEvents 0x4AB6D0", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) { fillGroup(r, false); },
            [&]() -> uint64_t { reinterpret_cast<CEventGroup*>(g_evg)->CEventGroup::TickEvents(); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int)>(0x4AB6D0)(g_evg, 0); return 0; });
    }, 60);
    Run("CEventGroup::Reorganise 0x4AB700", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) { fillGroup(r, true); },
            [&]() -> uint64_t { reinterpret_cast<CEventGroup*>(g_evg)->CEventGroup::Reorganise(); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int)>(0x4AB700)(g_evg, 0); return 0; });
    }, 120);
    // ---- AffectsPedGroup of the dragged-out-car and player-command events (the group is the argument; `this` is not read)
    Run("CEventDraggedOutCar::AffectsPedGroup 0x4AD3C0", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) { FillGroup(r, true); },
            [&]() -> uint64_t { return B(reinterpret_cast<CEventDraggedOutCar*>(g_ev)->CEventDraggedOutCar::AffectsPedGroup(reinterpret_cast<CPedGroup*>(g_grp))); },
            [&]() -> uint64_t { return B8((int)oracle::Fn<int __fastcall(void*, int, void*)>(0x4AD3C0)(g_ev, 0, g_grp)); });
    }, 120);
    Run("CEventPlayerCommandToGroup::AffectsPedGroup 0x4B24D0", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) { FillGroup(r, true); },
            [&]() -> uint64_t { return B(reinterpret_cast<CEventPlayerCommandToGroup*>(g_ev)->CEventPlayerCommandToGroup::AffectsPedGroup(reinterpret_cast<CPedGroup*>(g_grp))); },
            [&]() -> uint64_t { return B8((int)oracle::Fn<int __fastcall(void*, int, void*)>(0x4B24D0)(g_ev, 0, g_grp)); });
    }, 200);
    Run("CEventPlayerCommandToGroupAttack::AffectsPedGroup 0x4B2530", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) { FillGroup(r, true); },
            [&]() -> uint64_t { return B(reinterpret_cast<CEventPlayerCommandToGroupAttack*>(g_ev)->CEventPlayerCommandToGroupAttack::AffectsPedGroup(reinterpret_cast<CPedGroup*>(g_grp))); },
            [&]() -> uint64_t { return B8((int)oracle::Fn<int __fastcall(void*, int, void*)>(0x4B2530)(g_ev, 0, g_grp)); });
    }, 200);
    // ---- IsKillTaskAppropriate: the exe's 0x4BC3E0 is a cdecl function (ret, 3 stack args), the port's a member that does not read `this`
    Run("CEventHandler::IsKillTaskAppropriate 0x4BC3E0", [&](Rng& r, std::string& d) {
        CPed* p2 = nullptr;
        return EvCase(r, d, [&](Rng& r) { p2 = RPedR(r); },
            [&]() -> uint64_t { return B(reinterpret_cast<CEventHandler*>(g_hdl)->CEventHandler::IsKillTaskAppropriate(UnderTest(), p2, *reinterpret_cast<CEvent*>(g_ev))); },
            [&]() -> uint64_t { return B8((int)oracle::Fn<int __cdecl(void*, void*, void*)>(0x4BC3E0)(UnderTest(), p2, g_ev)); });
    }, 200);
    Run("CGroupEventHandler::IsKillTaskAppropriate 0x5F7A60", [&](Rng& r, std::string& d) {
        CPed* threat = nullptr;
        return EvCase(r, d, [&](Rng& r) { FillGroup(r, true); threat = r.below(2) ? UnderTest() : RPedR(r); },
            [&]() -> uint64_t { return B(CGroupEventHandler::IsKillTaskAppropriate(reinterpret_cast<CPedGroup*>(g_grp), threat)); },
            [&]() -> uint64_t { return B8((int)oracle::Fn<int __cdecl(void*, void*)>(0x5F7A60)(g_grp, threat)); });
    }, 200);
}

#include "wave3_oracle_ev_misc.inc"
#include "wave3_oracle_ev_handler.inc"
#include "wave3_oracle_ev_c.inc"

// ORACLE-C-EVENTS: class C rows (virtual calls in the exe closure) of game_sa/Events: the task fixture (W3TaskCase: `this` = g_tk, vftables of the event and of the ped swapped around each call) + the generated rows (tools/gen_w3_tasks.py --sub game_sa/Events --prefix events)
#define W3_TASKS_FIXTURE_ONLY 1
#include "wave3_oracle_tasks.inc"
#if __has_include("wave3_oracle_gen_events.inc")
#include "wave3_oracle_gen_events.inc"     // GENERATED (tools/gen_w3_tasks.py --sub game_sa/Events --prefix events)
#define W3_HAVE_EVGEN 1
#endif

static void TestW3Ev() {
#ifdef W3_HAVE_EVGEN
    TestW3EventsFuncs();
#endif
    TestW3EvA();
    TestW3EvGroup();
    TestW3EvMisc();
    TestW3EvHandler();
    TestW3EvC();
}
#endif // !W3_EV_FIXTURE_ONLY
