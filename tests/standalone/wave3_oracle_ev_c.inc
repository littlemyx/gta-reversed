// ORACLE-C-EVENTS: hand rows of the class C functions (virtual calls in the exe closure) of game_sa/Events whose generated rows (wave3_oracle_gen_events.inc: garbage `this`) fault in every case. Included by wave3_oracle_ev.inc.
// The event of a row is a buffer of the (concrete) event class with the pointer members pointing into the reference world; g_evVt makes EvCase write the exe vftable of that class into the event (and CPlayerPed's into the ped under test)
// before the exe call and the port vftable before the port call, so the virtual calls of the closure (GetEventType, GetSourceEntity, ...) run the real code on each side. `this` = the zeroed CEventHandler g_hdl of the wired ped.
#include "Events/EventPlayerCollisionWithPed.h"
#include "Events/EventDeadPed.h"
#include "Events/EventPotentialWalkIntoVehicle.h"
#include "Events/EventPotentialWalkIntoObject.h"
#include "Events/EventPotentialWalkIntoFire.h"
#include "Events/EventPotentialWalkIntoPed.h"
#include "Events/EventSoundQuiet.h"
#include "Events/EventAcquaintancePed.h"
#include "Events/EventAcquaintancePedDislike.h"
#include "Events/EventAcquaintancePedHate.h"
#include "Events/EventGunShot.h"
#include "Events/EventGunAimedAt.h"
#include "Events/EventVehicleDamage.h"
#include "Events/EventSeenPanickedPed.h"
#include "Events/EventVehicleHitAndRun.h"

#define W3EC_VT(Cls, Exe) W3_PORT_VT(Cls) static const w3e::EvVt kEvVt_##Cls = { Exe, w3vt_##Cls };
W3EC_VT(CEventPlayerCollisionWithPed, 0x86C9B0)
W3EC_VT(CEventDeadPed, 0x85AE00)
W3EC_VT(CEventPotentialWalkIntoVehicle, 0x85AED0)
W3EC_VT(CEventPotentialWalkIntoObject, 0x85AF18)
W3EC_VT(CEventPotentialWalkIntoFire, 0x85B6A0)
W3EC_VT(CEventPotentialWalkIntoPed, 0x85AF60)
W3EC_VT(CEventSoundQuiet, 0x86C2B0)
W3EC_VT(CEventAcquaintancePedDislike, 0x86CAC8)
W3EC_VT(CEventAcquaintancePedHate, 0x858E68)
W3EC_VT(CEventGunShot, 0x85ABE0)
W3EC_VT(CEventGunAimedAt, 0x85B330)
W3EC_VT(CEventVehicleDamageCollision, 0x8710D0)
W3EC_VT(CEventSeenPanickedPed, 0x85B778)
W3EC_VT(CEventVehicleHitAndRun, 0x85AFA8)

// handler row: ETYPE = the parameter type of the handler function, VTYPE = the concrete class that is built in g_ev (ETYPE itself unless abstract)
#define CROW(NAME, ADDR, ETYPE, VTYPE, N, ...) \
    Run("CEventHandler::" #NAME " " #ADDR, [&](Rng& r, std::string& d) { \
        return EvCase(r, d, [&](Rng& r) { auto* e = reinterpret_cast<ETYPE*>(EvBuf<VTYPE>(r)); (void)e; HInit(r); g_evVt = &kEvVt_##VTYPE; __VA_ARGS__ }, \
            [&]() -> uint64_t { reinterpret_cast<CEventHandler*>(g_hdl)->CEventHandler::NAME(reinterpret_cast<ETYPE*>(g_ev), g_tact, g_tsimp); return 0; }, \
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*, void*, void*)>(ADDR)(g_hdl, 0, g_ev, g_tact, g_tsimp); return 0; }); }, N);

static int g_fakeTaskType = 0;
static int __fastcall FakeGetTaskType(void*, int) { return g_fakeTaskType; }
static const void* const g_fakeTaskVt[16] = { nullptr, nullptr, nullptr, nullptr, (const void*)&FakeGetTaskType, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
static const void* g_fakeTask[4];
// tactive: null in a quarter of the cases, else a fake task of a flee type or of an unrelated type
template<class R> static void FakeTact(R& r) {
    using namespace w2; using namespace w3e;
    if (!r.below(4)) { g_tact = nullptr; return; }
    g_fakeTaskType = Pick(r, { TASK_COMPLEX_FLEE_POINT, TASK_COMPLEX_FLEE_ENTITY, TASK_COMPLEX_SMART_FLEE_POINT, TASK_COMPLEX_SMART_FLEE_ENTITY, TASK_NONE, TASK_COMPLEX_KILL_PED_ON_FOOT, TASK_SIMPLE_STAND_STILL });
    g_fakeTask[0] = g_fakeTaskVt; g_tact = reinterpret_cast<CTask*>(g_fakeTask);
}
static void TestW3EvC() {
    using namespace w2; using namespace w3e;
    // the ids of the gun-aimed-at switch (0x4C2975..0x4C2E97), including the ones the exe maps to Flee / SmartFlee / FleeAnyMeans tasks
    const auto TIDG = [](Rng& r) { return Pick(r, { TASK_NONE, TASK_FINISHED, TASK_COMPLEX_SMART_FLEE_ENTITY, TASK_COMPLEX_KILL_PED_ON_FOOT, TASK_COMPLEX_SMART_FLEE_POINT, TASK_COMPLEX_FLEE_ENTITY, TASK_COMPLEX_CAR_DRIVE_MISSION_FLEE_SCENE,
        TASK_COMPLEX_FLEE_ANY_MEANS, TASK_COMPLEX_REACT_TO_GUN_AIMED_AT, TASK_SIMPLE_COWER, TASK_SIMPLE_HANDS_UP, TASK_SIMPLE_DUCK, TASK_SIMPLE_DUCK_FOREVER, TASK_COMPLEX_KILL_CRIMINAL, TASK_COMPLEX_KILL_PED_ON_FOOT_STEALTH, TASK_COMPLEX_USE_CLOSEST_FREE_SCRIPTED_ATTRACTOR }); };
    const auto TID = [](Rng& r) { return Pick(r, { TASK_NONE, TASK_FINISHED, TASK_COMPLEX_SMART_FLEE_ENTITY, TASK_COMPLEX_KILL_PED_ON_FOOT, TASK_COMPLEX_SMART_FLEE_POINT }); };
    // the player-collision event is built with its constructor (the base class keeps its members protected)
    Run("CEventHandler::ComputePlayerCollisionWithPedResponse 0x4B8CE0", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) {
                HInit(r); g_evVt = &kEvVt_CEventPlayerCollisionWithPed;
                ::new (static_cast<void*>(g_ev)) CEventPlayerCollisionWithPed((int16)r.below(30), GF(r, 5.f), RPedN(r), GV(r), GV(r), (eMoveState)r.below(8), (eMoveState)r.below(8));
            },
            [&]() -> uint64_t { reinterpret_cast<CEventHandler*>(g_hdl)->CEventHandler::ComputePlayerCollisionWithPedResponse(reinterpret_cast<CEventPlayerCollisionWithPed*>(g_ev), g_tact, g_tsimp); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*, void*, void*)>(0x4B8CE0)(g_hdl, 0, g_ev, g_tact, g_tsimp); return 0; });
    }, 150);
    Run("CEventHandler::ComputeDeadPedResponse 0x4B9470", [&](Rng& r, std::string& d) {   // private members: built with the constructor
        return EvCase(r, d, [&](Rng& r) {
                HInit(r); g_evVt = &kEvVt_CEventDeadPed;
                auto* e = ::new (static_cast<void*>(g_ev)) CEventDeadPed(RPedN(r), (bool)r.below(2), r.below(100000)); e->m_TaskId = TID(r);
            },
            [&]() -> uint64_t { reinterpret_cast<CEventHandler*>(g_hdl)->CEventHandler::ComputeDeadPedResponse(reinterpret_cast<CEventDeadPed*>(g_ev), g_tact, g_tsimp); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*, void*, void*)>(0x4B9470)(g_hdl, 0, g_ev, g_tact, g_tsimp); return 0; });
    }, 150);
    CROW(ComputeVehiclePotentialPassiveCollisionResponse, 0x4B96D0, CEventPotentialWalkIntoVehicle, CEventPotentialWalkIntoVehicle, 150,
        e->m_vehicle = r.below(5) ? RVeh(r) : nullptr; e->m_moveState = (int32)r.below(8); e->m_TaskId = TID(r);)
    CROW(ComputePedSoundQuietResponse, 0x4B9D40, CEventSoundQuiet, CEventSoundQuiet, 150,
        e->m_entity = r.below(4) ? static_cast<CEntity*>(RPedN(r)) : static_cast<CEntity*>(RVeh(r)); e->m_fLocalSoundLevel = GF(r, 40.f); e->m_startTimeInMs = r.below(100000); e->m_position = GV(r); e->m_TaskId = TID(r);)
    CROW(ComputePedFriendResponse, 0x4B9DD0, CEventAcquaintancePed, CEventAcquaintancePedDislike, 150, e->m_AcquaintancePed = RPedN(r); e->m_TaskId = TID(r);)
    CROW(ComputeObjectCollisionPassiveResponse, 0x4BBB90, CEventPotentialWalkIntoObject, CEventPotentialWalkIntoObject, 150,
        e->m_object = r.below(5) ? static_cast<CObject*>(GObjE(r)) : nullptr; e->m_moveState = (int32)r.below(8); e->m_TaskId = TID(r);)
    CROW(ComputePotentialWalkIntoFireResponse, 0x4BBCD0, CEventPotentialWalkIntoFire, CEventPotentialWalkIntoFire, 150,
        e->m_firePos = GV(r); e->m_fireSize = GF(r, 5.f); e->m_radius = GF(r, 5.f); e->m_TaskId = TID(r);)
    CROW(ComputeShotFiredResponse, 0x4BC710, CEventGunShot, CEventGunShot, 150,
        e->m_firedBy = r.below(6) ? static_cast<CEntity*>(RPedR(r)) : nullptr; e->m_startPoint = GV(r); e->m_endPoint = GV(r); e->m_bHasNoSound = r.below(2); e->m_TaskId = TID(r);)
    CROW(ComputePedThreatResponse, 0x4C19A0, CEventAcquaintancePedHate, CEventAcquaintancePedHate, 150, e->m_AcquaintancePed = RPedN(r); e->m_TaskId = TID(r);)
    CROW(ComputePotentialPedCollideResponse, 0x4C2610, CEventPotentialWalkIntoPed, CEventPotentialWalkIntoPed, 150, e->m_ped = RPedN(r); e->m_targetPoint = GV(r); e->m_TaskId = TID(r);)
    // the active task of the handler: a fake task whose vftable slot 4 (GetTaskType, [eax+0x10] in the exe) returns a chosen type, on both sides (the exe asks it only for tactive, 0x4C2872)
    CROW(ComputeGunAimedAtResponse, 0x4C2840, CEventGunAimedAt, CEventGunAimedAt, 300, e->m_AimedBy = RPedN(r); e->m_TaskId = TIDG(r); FakeTact(r);)
    // CEventVehicleHitAndRun::ReportCriminalEvent (ORACLE-C-EVENTS-2): a real event (victim / vehicle in the reference world; the generated garbage-`this` row faulted)
    Run("CEventVehicleHitAndRun::ReportCriminalEvent 0x4B27D0", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) {
                HInit(r); g_evVt = &kEvVt_CEventVehicleHitAndRun;
                auto* e = reinterpret_cast<CEventVehicleHitAndRun*>(EvBuf<CEventVehicleHitAndRun>(r));
                e->m_victim = RPedN(r); e->m_vehicle = RVeh(r);
            },
            [&]() -> uint64_t { reinterpret_cast<CEventVehicleHitAndRun*>(g_ev)->CEventVehicleHitAndRun::ReportCriminalEvent(reinterpret_cast<CPed*>(g_src)); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*)>(0x4B27D0)(g_ev, 0, g_src); return 0; });
    }, 150);
    CROW(ComputeVehicleDamageResponse, 0x4C2FC0, CEventVehicleDamage, CEventVehicleDamageCollision, 150,
        e->m_vehicle = r.below(6) ? RVeh(r) : nullptr; e->m_attacker = r.below(3) ? static_cast<CEntity*>(RPedN(r)) : static_cast<CEntity*>(RVeh(r)); e->m_TaskId = TID(r);)
    CROW(ComputeSeenPanickedPedResponse, 0x4C35F0, CEventSeenPanickedPed, CEventSeenPanickedPed, 150, e->m_ped = RPedN(r); e->m_TaskId = TID(r);)
}
