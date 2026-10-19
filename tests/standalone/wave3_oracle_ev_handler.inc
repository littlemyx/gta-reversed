// ORACLE-W3-EVENTS batch 3 (included by wave3_oracle_ev.inc): CEventHandler::Compute*Response of the uncovered event classes.
// `this` is a zeroed CEventHandler whose m_Ped is the wired ped under test (a player ped for some cases: the player's wanted level / ped data are wired), the event a buffer of the event class filled with
// garbage whose pointer members point into the reference world and whose task id is one the decision makers hand out (plus a few others: the exe leaves the response task alone for those).
#include "Events/EventAreaCodes.h"
#include "Events/EventCopCarBeingStolen.h"
#include "Events/EventCreatePartnerTask.h"
#include "Events/EventDanger.h"
#include "Events/EventDeath.h"
#include "Events/EventGotKnockedOverByCar.h"
#include "Events/EventHighAngerAtPlayer.h"
#include "Events/EventLowAngerAtPlayer.h"
#include "Events/EventInteriorUseInfo.h"
#include "Events/EventHealthLow.h"
#include "Events/EventHealthReallyLow.h"
#include "Events/EventOnFire.h"
#include "Events/EventPassObject.h"
#include "Events/EventAcquaintancePedHateBadlyLit.h"
#include "Events/EventPedToChase.h"
#include "Events/EventSeenCop.h"
#include "Events/EventSexyPed.h"
#include "Events/EventSignalAtPed.h"
#include "Events/EventVehicleToSteal.h"
#include "Events/EventSpecial.h"
#include "Tasks/TaskTypes/Interior/TaskInteriorUseInfo.h"

namespace w3e {
template<class E> static E* EvBuf(Rng& r) { FillGarbage(r, g_ev, (sizeof(E) + 3) & ~size_t(3)); return reinterpret_cast<E*>(g_ev); }
// handler state: ComputeEventResponseTask (0x4C3870) clears all five response tasks before it dispatches, so they are null here (an early-out that stores null instead of leaving the field alone is not observable)
static void HInit(Rng&) {}
static eTaskType Pick(Rng& r, std::initializer_list<eTaskType> ids) { const auto n = ids.size(); return r.below(8) == 0 ? (eTaskType)r.below(1000) : *(ids.begin() + r.below((uint32)n)); }
} // namespace w3e

#define HROW(NAME, ADDR, ETYPE, N, ...) \
    Run("CEventHandler::" #NAME " " #ADDR, [&](Rng& r, std::string& d) { \
        return EvCase(r, d, [&](Rng& r) { auto* e = EvBuf<ETYPE>(r); (void)e; HInit(r); __VA_ARGS__ }, \
            [&]() -> uint64_t { reinterpret_cast<CEventHandler*>(g_hdl)->CEventHandler::NAME(reinterpret_cast<ETYPE*>(g_ev), g_tact, g_tsimp); return 0; }, \
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*, void*, void*)>(ADDR)(g_hdl, 0, g_ev, g_tact, g_tsimp); return 0; }); }, N);

// masks above: bytes +0x29..+0x2B of a CTaskComplexCarDriveMissionFleeScene (padding the exe constructor clears with a dword store), +0x40 of a CTaskComplexSeekEntity<> (vptr of the embedded position calculator)
static void TestW3EvHandler() {
    using namespace w2; using namespace w3e;
    HROW(ComputeAreaCodesResponse, 0x4BBF50, CEventAreaCodes, 60, e->m_ped = RPedN(r);)
    HROW(ComputeCopCarBeingStolenResponse, 0x4BB740, CEventCopCarBeingStolen, 150,
        e->m_hijacker = r.below(3) == 0 ? UnderTest() : (r.below(2) ? RPed(0) : RPedN(r)); e->m_vehicle = r.below(4) ? UnderTest()->m_pVehicle : RVeh(r); if (r.below(5) == 0) e->m_vehicle = nullptr;)
    HROW(ComputeCreatePartnerTaskResponse, 0x4BB130, CEventCreatePartnerTask, 150,
        e->m_partner = RPedN(r); e->m_partnerType = (int32)(r.below(9) == 0 ? r.below(20) : r.below(8)); e->m_isLeadSpeaker = r.below(2); e->m_meetDist = GF(r, 20.f);)
    HROW(ComputeDangerResponse, 0x4BC230, CEventDanger, 150,
        e->m_dangerFrom = r.below(6) ? (CEntity*)RPedR(r) : nullptr; e->m_TaskId = Pick(r, { TASK_COMPLEX_CAR_DRIVE_MISSION_FLEE_SCENE, TASK_COMPLEX_SMART_FLEE_ENTITY, TASK_NONE });
        if (UnderTest()->m_pVehicle && r.below(2)) UnderTest()->m_pVehicle->m_pDriver = UnderTest(); if (e->m_TaskId == TASK_COMPLEX_CAR_DRIVE_MISSION_FLEE_SCENE) { std::memset(g_tpMaskBytes + 0x29, 1, 3); })
    HROW(ComputeFireNearbyResponse, 0x4BBFB0, CEventFireNearby, 60, e->m_TaskId = Pick(r, { TASK_COMPLEX_EXTINGUISH_FIRES, TASK_NONE });)
    HROW(ComputeGotKnockedOverByCarResponse, 0x4C3430, CEventGotKnockedOverByCar, 200,
        e->m_vehicle = r.below(6) ? RVeh(r) : nullptr; e->m_TaskId = Pick(r, { TASK_COMPLEX_KILL_PED_ON_FOOT, TASK_COMPLEX_FLEE_ENTITY, TASK_COMPLEX_SMART_FLEE_ENTITY, TASK_COMPLEX_KILL_CRIMINAL, TASK_NONE }); { CPed* dr = e->m_vehicle ? e->m_vehicle->m_pDriver : nullptr; g_evTag = dr ? (int)(((uint8_t*)dr - (uint8_t*)rf::PedE(0)) / sizeof(CCopPed)) * 1000 + rf::pedSt[((uint8_t*)dr - (uint8_t*)rf::PedE(0)) / sizeof(CCopPed) % rf::kP] + (int)e->m_TaskId * 1000000 : -1; })
    HROW(ComputeHighAngerAtPlayerResponse, 0x4BAC10, CEventHighAngerAtPlayer, 150,
        e->m_TaskId = Pick(r, { TASK_COMPLEX_KILL_PED_ON_FOOT, TASK_COMPLEX_SMART_FLEE_ENTITY, TASK_NONE, TASK_SIMPLE_DUCK_FOREVER });)
    HROW(ComputeLowAngerAtPlayerResponse, 0x4BAAD0, CEventLowAngerAtPlayer, 150,
        e->m_TaskId = Pick(r, { TASK_COMPLEX_KILL_PED_ON_FOOT, TASK_COMPLEX_SMART_FLEE_ENTITY, TASK_NONE, TASK_SIMPLE_DUCK_FOREVER });)
    HROW(ComputeLowHealthResponse, 0x4BA990, CEventHealthLow, 100, e->m_TaskId = Pick(r, { TASK_COMPLEX_DIVE_FROM_ATTACHED_ENTITY_AND_GET_UP, TASK_NONE });)
    HROW(ComputeReallyLowHealthResponse, 0x4BAA30, CEventHealthReallyLow, 100, e->m_TaskId = Pick(r, { TASK_COMPLEX_DIVE_FROM_ATTACHED_ENTITY_AND_GET_UP, TASK_NONE });)
    HROW(ComputeOnFireResponse, 0x4BAD50, CEventOnFire, 60, )
    HROW(ComputePassObjectResponse, 0x4BB0C0, CEventPassObject, 60, e->m_giver = RPedN(r); e->m_dontPassObject = r.below(2);)
    HROW(ComputePedThreatBadlyLitResponse, 0x4B9C90, CEventAcquaintancePedHateBadlyLit, 100,
        e->m_AcquaintancePed = RPedN(r); e->m_point = GV(r); e->m_TaskId = Pick(r, { TASK_NONE, TASK_COMPLEX_INVESTIGATE_DISTURBANCE });)
    HROW(ComputePedToChaseResponse, 0x4C1910, CEventPedToChase, 60, e->m_ped = RPedN(r); std::memset(g_tpMaskBytes + 0x40, 1, 4);)   // +0x40: the embedded seek position calculator's vptr (port vtable vs exe vtable)
    HROW(ComputeSeenCopResponse, 0x4BC050, CEventSeenCop, 120,
        e->m_AcquaintancePed = RPedN(r); e->m_TaskId = Pick(r, { TASK_FINISHED, TASK_NONE, TASK_COMPLEX_SMART_FLEE_ENTITY, TASK_COMPLEX_KILL_PED_ON_FOOT });)
    HROW(ComputeSexyPedResponse, 0x4B99F0, CEventSexyPed, 100, e->m_SexyPed = RPedN(r); e->m_TaskId = Pick(r, { TASK_NONE, TASK_COMPLEX_GANG_HASSLE_PED });)
    HROW(ComputeSignalAtPedResponse, 0x4BB050, CEventSignalAtPed, 60, e->m_SignalAt = RPedN(r); e->m_InitialPause = (int32)r.below(5000); e->m_bPlayAnimAtEnd = r.below(2);)
    HROW(ComputeVehicleToStealResponse, 0x4B9F80, CEventVehicleToSteal, 60, e->m_vehicle = r.below(5) ? RVeh(r) : nullptr;)
    HROW(ComputeSpecialResponse, 0x4BB800, CEventSpecial, 300,
        e->m_TaskId = Pick(r, { TASK_COMPLEX_KILL_PED_ON_FOOT, TASK_COMPLEX_SMART_FLEE_ENTITY, TASK_COMPLEX_SMART_FLEE_POINT, TASK_COMPLEX_CAR_DRIVE_MISSION_FLEE_SCENE, TASK_COMPLEX_ARREST_PED,
            TASK_COMPLEX_USE_CLOSEST_FREE_SCRIPTED_ATTRACTOR, TASK_COMPLEX_USE_CLOSEST_FREE_SCRIPTED_ATTRACTOR_RUN, TASK_COMPLEX_USE_CLOSEST_FREE_SCRIPTED_ATTRACTOR_SPRINT,
            TASK_COMPLEX_LEAVE_CAR_AND_WANDER, TASK_COMPLEX_LEAVE_CAR, TASK_COMPLEX_LEAVE_CAR_AND_FLEE, TASK_NONE, TASK_COMPLEX_KILL_CRIMINAL }); if (e->m_TaskId == TASK_COMPLEX_CAR_DRIVE_MISSION_FLEE_SCENE) { std::memset(g_tpMaskBytes + 0x29, 1, 3); })
    // CEventDeath has private members: built with its constructor
    Run("CEventHandler::ComputeDeathResponse 0x4B9400", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) { HInit(r); ::new (static_cast<void*>(g_ev)) CEventDeath((bool)r.below(2), r.u32() >> r.below(20)); },
            [&]() -> uint64_t { reinterpret_cast<CEventHandler*>(g_hdl)->CEventHandler::ComputeDeathResponse(reinterpret_cast<CEventDeath*>(g_ev), g_tact, g_tsimp); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*, void*, void*)>(0x4B9400)(g_hdl, 0, g_ev, g_tact, g_tsimp); return 0; });
    }, 60);
    // ComputeInteriorUseInfoResponse 0x4BAFE0: the exe builds a NEW CTaskInteriorUseInfo from the event's fields (the task argument is not read)
    Run("CEventHandler::ComputeInteriorUseInfoResponse 0x4BAFE0", [&](Rng& r, std::string& d) {
        return EvCase(r, d, [&](Rng& r) {
                HInit(r);
                auto* e = EvBuf<CEventInteriorUseInfo>(r);
                e->m_InteriorInfo = r.below(2) ? reinterpret_cast<InteriorInfo_t*>(g_clump) : nullptr; e->m_Interior = r.below(2) ? reinterpret_cast<Interior_c*>(g_clump + 0x40) : nullptr;
                e->m_ActionAnimTime = r.below(3) ? r.below(10000) : r.u32(); e->m_nLoopAction = (int8)(r.below(2));
            },
            [&]() -> uint64_t { reinterpret_cast<CEventHandler*>(g_hdl)->CEventHandler::ComputeInteriorUseInfoResponse(reinterpret_cast<CEventInteriorUseInfo*>(g_ev), nullptr, g_tsimp); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*, void*, void*)>(0x4BAFE0)(g_hdl, 0, g_ev, nullptr, g_tsimp); return 0; });
    }, 60);
}
