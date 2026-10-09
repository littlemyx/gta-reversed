#include "StdInc.h"

#include "EventScanner.h"
#include "EventPotentialWalkIntoVehicle.h"
#include "PedGeometryAnalyser.h"
#include "TaskSimpleGoTo.h"
#include "EventInAir.h"
#include "EventOnFire.h"
#include "EventEscalator.h"
#include "EventAreaCodes.h"
#include "EventPedEnteredMyVehicle.h"
#include "EventDamage.h"
#include "Fire.h"
#include "ModelIndices.h"
#include "Weapon.h"
#include "PedStats.h"
#include "EventPotentialWalkIntoPed.h"
#include "EventPotentialWalkIntoObject.h"
#include "EventPotentialWalkIntoFire.h"
#include "EventFireNearby.h"
#include "EventSexyPed.h"
#include "InterestingEvents.h"
#include "FireManager.h"
#include "ObjectScanner.h"
#include "Object.h"
#include "PedScriptedTaskRecord.h"
#include "EventEditableResponse.h"
#include "DecisionMakers/DecisionMakerTypes.h"
#include "PedType.h"
#include "GameLogic.h"
#include "PedGroups.h"
#include "Acquaintance.h"

// Sanity checks for the raw offsets used by the original
static_assert(WEAPON_FLAMETHROWER == 0x25 && WEAPON_FALL == 0x36 && WEAPON_UNIDENTIFIED == 0x37 && PED_PIECE_TORSO == 3);
static_assert(offsetof(CPed, m_fHealth) == 0x540 && offsetof(CPed, m_pContactEntity) == 0x584 && offsetof(CPed, m_pVehicle) == 0x58C && offsetof(CPed, m_pFire) == 0x730);
static_assert(offsetof(CVehicle, m_nVehicleType) == 0x590 && offsetof(CVehicle, m_nVehicleSubType) == 0x594);
static_assert(offsetof(CTaskComplexKillPedOnFoot, m_target) == 0x10);
static_assert(offsetof(CObject, objectFlags) == 0x140 && offsetof(CPedIntelligence, m_nDmNumPedsToScan) == 0xC4);
static_assert(offsetof(CPedIntelligence, m_pedScanner) + offsetof(CPedScanner, m_apEntities) == 0x130);
static_assert(offsetof(CPed, m_nPedType) == 0x598 && offsetof(CPed, m_pStats) == 0x59C);

// x87 helpers (0x406DA0 and 0x40FDB0 return an extended precision value in ST0)
static double SqMagnitude(const CVector& v) {
    return ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
}
static double X87DotProduct(const CVector& a, const CVector& b) {
    return ((double)a.z * b.z + (double)a.y * b.y) + (double)a.x * b.x;
}

void CVehiclePotentialCollisionScanner::InjectHooks() {
    RH_ScopedClass(CVehiclePotentialCollisionScanner);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(ScanForVehiclePotentialCollisionEvents, 0x603720);
}

void CEventScanner::InjectHooks() {
    RH_ScopedClass(CEventScanner);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(ScanForEvents, 0x607E30);
    RH_ScopedInstall(ScanForPedPotentialCollisionEvents, 0x606580);

    // NOTSA: These aren't registered in InjectHooksMain.cpp
    CObjectPotentialCollisionScanner::InjectHooks();
    CPedAcquaintanceScanner::InjectHooks();
    CSexyPedScanner::InjectHooks();
    CNearbyFireScanner::InjectHooks();
}

void CObjectPotentialCollisionScanner::InjectHooks() {
    RH_ScopedClass(CObjectPotentialCollisionScanner);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(ScanForObjectPotentialCollisionEvents, 0x606890);
}

void CPedAcquaintanceScanner::InjectHooks() {
    RH_ScopedClass(CPedAcquaintanceScanner);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(ScanForPedAcquaintanceEvents, 0x607D80);
    RH_ScopedInstall(IsScanAllowed, 0x603A30);
    RH_ScopedInstall(ScanForPedAcquaintances, 0x607A90);
    RH_ScopedInstall(WantsToRiotAgainst, 0x603AF0);
    RH_ScopedInstall(ScanCandidateForAcquaintance, 0x607560);
}

void CSexyPedScanner::InjectHooks() {
    RH_ScopedClass(CSexyPedScanner);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(ScanForSexyPedEvents, 0x603BF0);
}

void CNearbyFireScanner::InjectHooks() {
    RH_ScopedClass(CNearbyFireScanner);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(ScanForNearbyFireEvents, 0x603E70);
}

// 0x605300
CEventScanner::CEventScanner() {
    m_nNextScanTime = CTimer::GetTimeInMS() + CGeneral::GetRandomNumberInRange(3000u); // Originally should be -3000.0f (float value)
}

void CEventScanner::Clear() {
    m_attractorScanner.Clear();
}

// 0x607E30
void CEventScanner::ScanForEvents(CPed& ped) {
    if (CTimer::GetTimeInMS() <= m_nNextScanTime) {
        return;
    }

    auto* const intel = ped.GetIntelligence();

    m_vehiclePotentialCollisionScanner.ScanForVehiclePotentialCollisionEvents(ped, intel->GetVehicleEntities(), 16); // 0x607E76

    ScanForPedPotentialCollisionEvents(&ped, intel->m_pedScanner.GetClosestPedInRange()); // 0x607E8A
    m_objectPotentialCollisionScanner.ScanForObjectPotentialCollisionEvents(ped);          // 0x607E93
    m_pedAcquaintanceScanner.ScanForPedAcquaintanceEvents(ped, intel->GetPedEntities(), 16); // 0x607EA8
    m_attractorScanner.ScanForAttractors(ped);                                             // 0x607EB1
    m_nearbyFireScanner.ScanForNearbyFireEvents(ped);                                      // 0x607EBD
    intel->m_mentalState.Process(ped);                                                     // 0x607EC9

    if (!ped.bIsStanding && (ped.bIsInTheAir || CPedGeometryAnalyser::IsInAir(ped))) { // 0x607ECE
        CEventInAir event{};
        intel->m_eventGroup.Add(&event, false);
    } else if (ped.bIsInTheAir) { // 0x607F25
        const auto curEvent = intel->m_eventHandler.GetHistory().GetCurrentEvent();
        if (!curEvent || curEvent->GetEventType() != EVENT_IN_AIR) {
            ped.bIsInTheAir = false;
        }
    }

    if (ped.m_pFire) { // 0x607F5A
        CEventOnFire event{};
        const auto task = ped.IsPlayer() ? nullptr : intel->m_TaskMgr.GetSimplestActiveTask();
        if (task && !task->MakeAbortable(&ped, ABORT_PRIORITY_URGENT, &event)) {
            CWeapon::GenerateDamageEvent(&ped, ped.m_pFire->GetEntityStartedFire(), WEAPON_FLAMETHROWER, 5, PED_PIECE_TORSO, 0);
        } else {
            intel->m_eventGroup.Add(&event, false);
        }
    }

    m_sexyPedScanner.ScanForSexyPedEvents(ped, intel->GetPedEntities(), 16); // 0x607FFA

    { // 0x607FFF
        auto& taskMgr = ped.GetIntelligence()->m_TaskMgr;
        CTask* killTask = taskMgr.FindTaskByType(TASK_PRIMARY_DEFAULT, TASK_COMPLEX_KILL_PED_ON_FOOT);
        if (!killTask) {
            killTask = taskMgr.FindTaskByType(TASK_PRIMARY_PRIMARY, TASK_COMPLEX_KILL_PED_ON_FOOT);
        }
        if (!killTask) {
            killTask = taskMgr.FindTaskByType(TASK_PRIMARY_EVENT_RESPONSE_TEMP, TASK_COMPLEX_KILL_PED_ON_FOOT);
        }
        if (!killTask) {
            killTask = taskMgr.FindTaskByType(TASK_PRIMARY_EVENT_RESPONSE_NONTEMP, TASK_COMPLEX_KILL_PED_ON_FOOT);
        }
        if (killTask) {
            if (const auto target = static_cast<CTaskComplexKillPedOnFoot*>(killTask)->m_target) { // +0x10
                const auto targetContact = target->m_pContactEntity;
                if (targetContact && ped.m_pContactEntity && targetContact->GetAreaCode() != ped.m_pContactEntity->GetAreaCode()) {
                    CEventAreaCodes event{target};
                    ped.GetIntelligence()->m_eventGroup.Add(&event, false);
                }
            }
        }
    }

    if (ped.m_pContactEntity) { // 0x6080B2
        const auto modelId = (int32)(int16)ped.m_pContactEntity->m_nModelIndex; // Original sign extends it
        if ((int32)(uint16)ModelIndices::MI_ESCALATORSTEP == modelId || (int32)(uint16)ModelIndices::MI_ESCALATORSTEP8 == modelId) {
            CEventEscalator event{};
            ped.GetIntelligence()->m_eventGroup.Add(&event, false);
        }
    }

    if (ped.bInVehicle && ped.m_pVehicle && ped.m_pVehicle->m_nVehicleType == VEHICLE_TYPE_BOAT && !ped.IsPlayer()) { // 0x608114
        const auto veh = ped.m_pVehicle;
        if (FindPlayerPed()->m_pContactEntity == veh) {
            CEventPedEnteredMyVehicle event{FindPlayerPed(), veh, TARGET_DOOR_DRIVER};
            event.m_TaskId = (eTaskType)0x2C2;
            ped.GetIntelligence()->m_eventGroup.Add(&event, false);
        }
    }

    if (ped.m_fHealth <= 0.f && ped.IsAlive() && CTimer::GetTimeInMS() > m_sDeadPedWalkingTimer) { // 0x6081A4
        CEventDamage event{nullptr, 0, WEAPON_UNIDENTIFIED, PED_PIECE_TORSO, 0, false, false};
        if (!ped.GetIntelligence()->m_eventGroup.HasEventOfType(&event)) {
            CWeapon::GenerateDamageEvent(&ped, nullptr, WEAPON_FALL, 10, PED_PIECE_TORSO, 0);
            m_sDeadPedWalkingTimer = CTimer::GetTimeInMS() + 2000;
        }
    }
}

// 0x6053D0
void CEventScanner::ScanForEventsNow(const CPed& ped, bool bDontScan) {
    if (bDontScan)
        return;

    auto scanner = &m_vehiclePotentialCollisionScanner;
    if (scanner->m_timer.m_bStarted) { // todo: inlined?
        scanner->m_timer.m_nStartTime = CTimer::GetTimeInMS();
        scanner->m_timer.m_nInterval = -1;
        scanner->m_timer.m_bStarted = true;
    }
    scanner->ScanForVehiclePotentialCollisionEvents(ped, ped.GetIntelligence()->GetVehicleEntities(), 16);
}

// 0x603720
void CVehiclePotentialCollisionScanner::ScanForVehiclePotentialCollisionEvents(const CPed& ped, CEntity** entities, int32 count) {
    // NOTE: `entities` and `count` are unused by the original

    if (!m_timer.m_bStarted) {
        m_timer.Start(500);
        if (!m_timer.m_bStarted) { // Always false
            return;
        }
    }
    if (!m_timer.IsOutOfTime()) {
        return;
    }
    m_timer.Start(500);

    auto* const intel = ped.GetIntelligence();

    const auto task = intel->m_TaskMgr.GetSimplestActiveTask(); // 0x6037A7
    if (!task || !CTask::IsGoToTask(task)) {
        return;
    }
    const auto goToTask = static_cast<CTaskSimpleGoTo*>(task);

    CVehicle* const veh = intel->m_vehicleScanner.GetClosestVehicleInRange(); // intel + 0x120
    if (!veh) {
        return;
    }

    const CVector pedPos = ped.GetPosition();
    const CVector pedToVeh = pedPos - veh->GetPosition(); // `pedPos - vehPos` (0x6037FB)

    const auto  colModel = veh->GetColModel();
    const auto& vehMat   = veh->GetMatrix();
    CVector     bbMinWS, bbMaxWS;
    bbMinWS.FromMultiply(vehMat, colModel->GetBoundingBox().m_vecMin); // 0x603826
    bbMaxWS.FromMultiply(vehMat, colModel->GetBoundingBox().m_vecMax); // 0x603838

    // NOTE: All of the following is evaluated on the x87 stack (extended precision), only the stores to memory round to float
    const CVector up = vehMat.GetUp();
    const double  planeA = -((double)up.z * bbMaxWS.z + (double)up.y * bbMaxWS.y + (double)up.x * bbMaxWS.x);
    const float   sz     = (float)((double)up.z * -1.0); // 0x858C1C = -1.0f
    const float   sy     = (float)((double)up.y * -1.0);
    const float   sx     = (float)((double)up.x * -1.0);
    const float   planeB = (float)-(((double)bbMinWS.y * sy + (double)bbMinWS.z * sz) + (double)sx * bbMinWS.x);

    bool bClose = false;
    if (((double)up.z * pedPos.z + (double)up.y * pedPos.y + (double)up.x * pedPos.x + planeA) < 0.5) { // 0x86C8E8
        if (((double)sy * pedPos.y + (double)sz * pedPos.z + (double)sx * pedPos.x + (double)planeB) < 0.5) {
            bClose = true;
        }
    }

    // 0x603935
    float dist = veh->m_nVehicleSubType == VEHICLE_TYPE_TRAIN
        ? (float)((double)5.0f + (double)5.0f) // 0x86C8DC
        : 5.0f;
    if (!bClose) {
        return;
    }

    if (!ped.bHasJustLeftCar) { // `TEST byte [ped+0x474], 8` (9th flags byte, bit 3)
        const double sqMag = (double)pedToVeh.x * pedToVeh.x + (double)pedToVeh.y * pedToVeh.y + (double)pedToVeh.z * pedToVeh.z; // 0x406DA0
        if (!((double)dist * dist > sqMag)) {
            return;
        }
    }

    dist = 0.f;
    if (CPedGeometryAnalyser::GetIsLineOfSightClear(ped, goToTask->m_vecTargetPoint, *veh, dist)) { // 0x5F5A30
        return;
    }
    if (!(dist > 0.5f)) { // 0x86C8E0
        return;
    }

    CEventPotentialWalkIntoVehicle event{ veh, (int32)intel->GetMoveStateFromGoToTask() }; // 0x601D70, 0x4AE320
    intel->m_eventGroup.Add(&event, false); // 0x4AB420
}

// 0x606580
void __stdcall CEventScanner::ScanForPedPotentialCollisionEvents(CPed* ped, CPed* closestPed) {
    // NOTE: Doesn't use `this`, `closestPed` is only used as a boolean
    if (!closestPed) {
        return;
    }
    if (!ped->m_bUsesCollision) {
        return;
    }

    auto* const intel = ped->GetIntelligence();

    const auto task = intel->m_TaskMgr.GetSimplestActiveTask();
    if (!task || !CTask::IsGoToTask(task)) {
        return;
    }
    const auto goToTask = static_cast<CTaskSimpleGoTo*>(task);
    if (intel->GetMoveStateFromGoToTask() == PEDMOVE_STILL) {
        return;
    }

    // Find the closest ped in front of us whose sphere intersects our path
    CPed* bestPed       = nullptr;
    float bestDistSq    = (float)((double)2.5f * (double)2.5f); // 0x86C8C0
    const CVector pedPos = ped->GetPosition();
    for (auto i = 0; i < 16; i++) {
        const auto other = static_cast<CPed*>(intel->GetPedEntities()[i]); // 0x130 + i * 4
        if (!other || !other->IsAlive() || !other->m_bUsesCollision) {
            continue;
        }

        const CVector otherPos = other->GetPosition();
        const CVector diff     = otherPos - pedPos; // 0x40FE00
        const auto&   fwd      = ped->GetForward();
        if (!(((double)diff.y * fwd.y + (double)diff.z * fwd.z) + (double)diff.x * fwd.x > 0.0)) { // 0x858B50 = 0.0
            continue;
        }

        CColSphere sphere;
        sphere.Set(0.7f, otherPos, SURFACE_DEFAULT, 0, tColLighting{0xFF}); // 0x3F333333
        CVector isect1, isect2;
        if (!sphere.IntersectEdge(pedPos, goToTask->m_vecTargetPoint, isect1, isect2)) {
            continue;
        }

        const double sqMag = SqMagnitude(diff);
        if (sqMag < (double)bestDistSq) {
            bestDistSq = (float)sqMag;
            bestPed    = other;
        }
    }
    if (!bestPed) {
        return;
    }

    // If both peds are walking in the same direction at similar speeds there's no need for an event
    const auto bestTask = bestPed->GetIntelligence()->m_TaskMgr.GetSimplestActiveTask();
    if (bestTask && CTask::IsGoToTask(bestTask) && X87DotProduct(ped->GetForward(), bestPed->GetForward()) >= (double)0.923f) { // 0x86CD98
        CVector pedSpeed  = ped->m_vecMoveSpeed * 50.f;     // 0x40FEC0
        CVector bestSpeed = bestPed->m_vecMoveSpeed * 50.f;
        pedSpeed.z  = 0.f;
        bestSpeed.z = 0.f;

        const float pedSpeedSq  = (float)SqMagnitude(pedSpeed);
        const float bestSpeedSq = (float)(SqMagnitude(bestSpeed) + (double)0.25f); // 0x86CD94

        if (bestDistSq > 1.f // 0x86CD90
            && !approxEqual(bestSpeedSq, 0.f, 0.01f) // 0x4EEA80
            && bestSpeedSq > pedSpeedSq
        ) {
            return;
        }
    }

    CEventPotentialWalkIntoPed event{ bestPed, goToTask->m_vecTargetPoint, intel->GetMoveStateFromGoToTask() }; // 0x4AE6E0
    intel->m_eventGroup.Add(&event, false); // 0x4AB420
}

// 0x606890
void CObjectPotentialCollisionScanner::ScanForObjectPotentialCollisionEvents(CPed& ped) {
    if (!m_timer.m_bStarted) {
        m_timer.Start(500);
        if (!m_timer.m_bStarted) { // Always false
            return;
        }
    }
    if (!m_timer.IsOutOfTime()) {
        return;
    }
    m_timer.Start(500);

    auto* const intel = ped.GetIntelligence();

    auto task = intel->m_TaskMgr.GetSimplestActiveTask();
    const auto moveState = task && CTask::IsGoToTask(task)
        ? static_cast<CTaskSimpleGoTo*>(task)->m_moveState
        : PEDMOVE_STILL;
    if (ped.IsPlayer() || moveState == PEDMOVE_STILL) {
        return;
    }

    task = intel->m_TaskMgr.GetSimplestActiveTask();
    if (!task || !CTask::IsGoToTask(task)) {
        return;
    }
    const auto goToTask = static_cast<CTaskSimpleGoTo*>(task);

    CObjectScanner scanner{};
    scanner.ScanForObjectsInRange(ped); // 0x5FFF30
    CObject* const obj = scanner.GetClosestObjectInRange();
    if (!obj || obj->objectFlags.bIsBroken || obj->objectFlags.bIsPickup || !obj->m_bUsesCollision) {
        return;
    }

    const CVector pedPos = ped.GetPosition();
    const CVector diff   = pedPos - obj->GetPosition(); // 0x40FE00
    if (!((double)7.5f * (double)7.5f > SqMagnitude(diff))) { // 0x86C8FC
        return;
    }

    const CVector centre = obj->GetBoundCentre(); // 0x534250
    const float   radius = obj->GetModelInfo()->GetColModel()->GetBoundRadius(); // +0x24 of the col model
    const float   lowZ   = (float)((double)centre.z - radius);
    const float   highZ  = (float)((double)pedPos.z + 1.0); // 0x858624 = 1.0f
    if ((double)pedPos.z - 1.0 > (double)radius + (double)centre.z) {
        return;
    }
    if (highZ < lowZ) {
        return;
    }

    float dist = 0.f;
    if (CPedGeometryAnalyser::GetIsLineOfSightClear(ped, goToTask->m_vecTargetPoint, *obj, dist)) { // 0x5F5A30
        return;
    }
    if (!(dist > 0.5f)) { // 0x86C900
        return;
    }

    const CVector start = ped.GetPosition() - CVector{0.f, 0.f, 0.75f}; // 0x40FE60, 0x3F400000
    if (CPedGeometryAnalyser::GetIsLineOfSightClear(start, goToTask->m_vecTargetPoint, *obj)) { // 0x5F2F00
        return;
    }

    CEventPotentialWalkIntoObject event{ obj, (int32)moveState }; // 0x4AE5D0
    intel->m_eventGroup.Add(&event, false); // 0x4AB420
}

// 0x607D80
void CPedAcquaintanceScanner::ScanForPedAcquaintanceEvents(CPed& ped, CEntity** entities, int32 count) {
    if (!m_timer.m_bStarted) {
        m_timer.m_nStartTime = CTimer::GetTimeInMS();
        m_timer.m_nInterval  = ms_nScanInterval;
        m_timer.m_bStarted   = true;
    }
    if (!m_timer.IsOutOfTime()) {
        return;
    }
    m_timer.m_nStartTime = CTimer::GetTimeInMS();
    m_timer.m_nInterval  = ms_nScanInterval;
    m_timer.m_bStarted   = true;

    if (!IsScanAllowed(ped)) { // 0x607DE4
        return;
    }
    CPed*  outPed = nullptr;
    int32  outIdx = -1;
    ScanForPedAcquaintances(ped, -1, entities, count, outPed, outIdx); // 0x607E16
}

// 0x59C910 - CVector::Normalise as the original evaluates it: the sum of squares and the reciprocal root stay in the FPU
// (extended precision), every component is stored as float. A length of 0 (or less) yields (1, y, z) - only x is written.
static void NormaliseExt(CVector& v) {
    const double sq = ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
    if (sq <= 0.0) { // FCOM + JP: NaN takes the sqrt path
        v.x = 1.0f;
        return;
    }
    const double inv = 1.0 / std::sqrt(sq);
    v.x = (float)(v.x * inv);
    v.y = (float)(v.y * inv);
    v.z = (float)(v.z * inv);
}

// 0x607A90
void CPedAcquaintanceScanner::ScanForPedAcquaintances(CPed& ped, int32 acquaintanceId, CEntity** entities, int32 count, CPed*& outPed, int32& outIdx) {
    outPed = nullptr;

    // The original has room for exactly 16 candidates on its stack (esp+0x3C..0x7C) and `count` is 16 at most (`CPedIntelligence::GetPedEntities`)
    CPed*   candidates[16];
    int32   numCandidates = 0;
    assert(count <= (int32)std::size(candidates));

    for (int32 i = 0; i < count; i++) {
        auto* const other = static_cast<CPed*>(entities[i]);
        if (!other) {
            continue;
        }
        if (!other->IsAlive()) { // 0x5E0170
            continue;
        }

        // Is the other ped in front of us?
        CVector toOther = other->GetPosition() - ped.GetPosition();
        NormaliseExt(toOther); // 0x59C910
        const auto& fwd = ped.GetMatrix().GetForward();
        const double dot = ((double)toOther.z * fwd.z + (double)toOther.y * fwd.y) + (double)toOther.x * fwd.x;
        if (!(dot > (double)ms_fThresholdDotProduct) && !ped.bInVehicle && !other->bInVehicle) {
            continue;
        }

        // Do we have a (matching) acquaintance with the other ped?
        bool isInteresting = acquaintanceId == -1 && other->m_nPedType == PED_TYPE_COP;
        if (!isInteresting) {
            for (auto id = 4; id >= 0 && !isInteresting; id--) {
                if (acquaintanceId == -1 || acquaintanceId == id) {
                    const auto mine   = ped.GetAcquaintance().GetAcquaintances((AcquaintanceId)id); // 0x608970
                    const auto theirs = CPedType::GetPedFlag(other->m_nPedType);                   // 0x608830
                    if (mine & theirs) {
                        isInteresting = true;
                    }
                }
            }
            if (!isInteresting) {
                // Riots
                if (!CGameLogic::LaRiotsActiveHere()) { // 0x441C10
                    continue;
                }
                if (!WantsToRiotAgainst(&ped, other)) { // 0x603AF0
                    continue;
                }
            }
        }

        if (other->m_nPedType != PED_TYPE_COP) {
            // Event types: HATE, DISLIKE, RESPECT, 40 (no name)
            // BUG: The original passes a count of 5, but only initializes 4 entries - the 5th is an uninitialized stack slot (read only if the first 4 all fail).
            //      We can't reproduce garbage, so only the 4 valid ones are checked.
            const int32 eventTypes[]{ EVENT_ACQUAINTANCE_PED_HATE, EVENT_ACQUAINTANCE_PED_DISLIKE, EVENT_ACQUAINTANCE_PED_RESPECT, 40 };
            const bool  rioting = CGameLogic::LaRiotsActiveHere() && WantsToRiotAgainst(&ped, other); // 0x603AF0
            if (!rioting) {
                // 0x4684F0, 0x6042B0
                if (!CDecisionMakerTypes::GetInstance()->HasAnyEventResponse(&ped, eventTypes, (int32)std::size(eventTypes))) {
                    continue;
                }
            }
        }
        candidates[numCandidates++] = other;
    }

    int32 curIdx = -1;
    for (int32 i = 0; i < numCandidates; i++) {
        if (curIdx == 4) {
            continue;
        }
        auto* const other = candidates[i];
        if (CPedGeometryAnalyser::CanPedTargetPed(ped, *other, !ped.bInVehicle && !other->bInVehicle)) { // 0x5F1C40
            curIdx = ScanCandidateForAcquaintance(ped, acquaintanceId, curIdx, other, outPed, outIdx); // 0x607560
        }
    }
}

// 0x603A30
bool CPedAcquaintanceScanner::IsScanAllowed(CPed& ped) {
    if (!ped.IsAlive()) {
        return false;
    }

    bool allowed;
    if (!ped.IsCreatedByMission() || m_bScanAllowedScriptPed) {
        allowed = true;
    } else {
        allowed = ped.bInVehicle && m_bScanAllowedInVehicle;
        if (CPedScriptedTaskRecord::GetStatus(&ped) != eScriptedTaskStatus::EVENT_ASSOCIATED && m_bScanAllowedScriptedTask) {
            allowed = true;
        }
    }
    if (!allowed) {
        return false;
    }

    auto* const intel = ped.GetIntelligence();
    const auto  event = intel->m_eventHandler.GetHistory().GetCurrentEvent();
    if (!event || event->GetEventType() != EVENT_ACQUAINTANCE_PED_HATE) { // 0x24
        return allowed;
    }
    if (!intel->m_nDmNumPedsToScan) {
        return false;
    }
    if (!static_cast<CEventEditableResponse*>(event)->ComputeResponseTaskOfType(&ped, TASK_SIMPLE_INFORM_RESPECTED_FRIENDS)) { // 0x6A4
        return false;
    }
    return intel->FindRespectedFriendInInformRange() ? allowed : false;
}

// 0x603BF0
void CSexyPedScanner::ScanForSexyPedEvents(CPed& ped, CEntity** entities, int32 count) {
    if (!m_timer.m_bStarted) {
        m_timer.Start(500);
        if (!m_timer.m_bStarted) { // Always false
            return;
        }
    }
    if (!m_timer.IsOutOfTime()) {
        return;
    }
    m_timer.Start(500);

    if (!(ped.IsCreatedByMission() || !ped.bInVehicle)) {
        return;
    }

    CPed*         bestPed    = nullptr;
    float         bestDistSq = 1e10f; // 0x501502F9
    const CVector pedPos     = ped.GetPosition();
    for (auto i = 0; i < count; i++) {
        const auto other = static_cast<CPed*>(entities[i]);
        if (!other || other->m_nPedType != PED_TYPE_CIVFEMALE) {
            continue;
        }
        if (!((int8)ped.m_pStats->m_nSexiness < (int8)other->m_pStats->m_nSexiness)) { // Signed compare
            continue;
        }
        if (other->bInVehicle) {
            continue;
        }

        const CVector otherPos = other->GetPosition();
        const CVector diff     = otherPos - pedPos;
        // x87: `diff.z` is stored as a float ([esp+0x20]), but ST0 keeps the unrounded difference, which is what gets squared (times the rounded one)
        const double  dzExact  = (double)otherPos.z - (double)pedPos.z;
        const double  sqDist   = (dzExact * (double)diff.z + (double)diff.y * diff.y) + (double)diff.x * diff.x;
        const float   sqDistF  = (float)sqDist;
        if (!(sqDist < 10000.0)) { // 0x859AA4
            continue;
        }

        if (ped.GetPlayerData()) {
            g_InterestingEvents.Add(CInterestingEvents::INTERESTING_EVENT_9, other); // 0x603D87
        }

        if (!(sqDistF < bestDistSq)) {
            continue;
        }
        const auto& fwd = ped.GetForward();
        if (!(((double)diff.z * fwd.z + (double)diff.y * fwd.y) + (double)diff.x * fwd.x > 0.0)) { // 0x858B50
            continue;
        }
        if (!CWorld::GetIsLineOfSightClear(pedPos, otherPos, true, false, false, true, false, false, false)) { // 0x56A490
            continue;
        }
        bestDistSq = sqDistF;
        bestPed    = other;
    }

    if (bestPed) {
        CEventSexyPed event{bestPed}; // 0x4AEDF0
        ped.GetIntelligence()->m_eventGroup.Add(&event, false); // 0x4AB420
        m_timer.Start(3000);
    }
}

// 0x603E70
void CNearbyFireScanner::ScanForNearbyFireEvents(CPed& ped) {
    if (!m_timer.m_bStarted) {
        m_timer.Start(0);
        if (!m_timer.m_bStarted) { // Always false
            return;
        }
    }
    if (!m_timer.IsOutOfTime()) {
        return;
    }
    m_timer.Start(100);

    auto* const intel = ped.GetIntelligence();

    const auto simplestTask = intel->m_TaskMgr.GetSimplestActiveTask();
    const auto moveState    = simplestTask && CTask::IsGoToTask(simplestTask)
        ? static_cast<CTaskSimpleGoTo*>(simplestTask)->m_moveState
        : PEDMOVE_STILL;
    const auto activeTask = intel->m_TaskMgr.GetActiveTask();

    CFire* const fire = gFireManager.FindNearestFire(ped.GetPosition(), false, false); // 0x538F40

    float sqDistF = 0.f; // Uninitialized in the original, only used if `fire`
    float dz      = 0.f;
    if (fire) {
        // dx and dy are kept in the x87 registers (extended precision), dz is stored as a float
        const auto&  firePos = fire->GetPosition();
        const auto&  pedPos  = ped.GetPosition();
        const double dx      = (double)firePos.x - (double)pedPos.x;
        const double dy      = (double)firePos.y - (double)pedPos.y;
        dz                   = firePos.z - pedPos.z;
        const double sqDist  = (dy * dy + dx * dx) + (double)dz * dz;
        sqDistF              = (float)sqDist;
        if (sqDist < (double)20.f * 20.f && std::fabs(dz) < 2.f) { // 0x86C918, 0x858CA0
            CEventFireNearby event{firePos}; // 0x4B1F10
            intel->m_eventGroup.Add(&event, false); // 0x4AB420
        }
    }

    if (activeTask && activeTask->GetTaskType() != TASK_COMPLEX_WALK_ROUND_FIRE) { // 0x202
        const auto task = intel->m_TaskMgr.GetSimplestActiveTask();
        if (task && CTask::IsGoToTask(task) && fire && (double)4.f * 4.f > sqDistF && std::fabs(dz) < 2.f) { // 0x86C91C
            CEventPotentialWalkIntoFire event{fire->GetPosition(), fire->GetStrength(), moveState}; // 0x4B1E20
            intel->m_eventGroup.Add(&event, false); // 0x4AB420
        }
    }
}

// 0x603AF0
// Should `ped` riot against `other`? (cdecl, called from the acquaintance scanner)
bool CPedAcquaintanceScanner::WantsToRiotAgainst(CPed* ped, CPed* other) {
    const auto pedType = ped->m_nPedType;
    if (pedType == PED_TYPE_COP || pedType == PED_TYPE_MEDIC || pedType == PED_TYPE_FIREMAN) {
        return false;
    }
    if (ped->IsPlayer() || ped->IsCreatedBy(PED_MISSION)) {
        return false;
    }

    if (other->IsPlayer()) {
        return !ped->GetIntelligence()->Respects(other);
    }
    if (const auto* const group = CPedGroups::GetPedsGroup(other)) { // 0x5F7E80
        auto& membership = group->GetMembership();
        if (const auto leader = membership.GetLeader()) {
            if (leader->IsPlayer()) {
                return !ped->GetIntelligence()->Respects(membership.GetLeader());
            }
        }
    }

    // Members of the same gang don't riot against each other
    const auto otherType = other->m_nPedType;
    return !(IsPedTypeGang(pedType) && IsPedTypeGang(otherType) && pedType == otherType);
}

// 0x607560
// Scans the 5 acquaintance types (4 down to 0) for `candidate`. Returns `acquaintanceId` if a (specific) type matched, the matched
// type if `acquaintanceId == -1` and the event was created, otherwise -1. Also (re)starts the scan timer on a match.
int32 CPedAcquaintanceScanner::ScanCandidateForAcquaintance(CPed& ped, int32 acquaintanceId, int32 curIdx, CPed* candidate, CPed*& outPed, int32& outIdx) {
    const float lightLevel = ped.GetIntelligence()->CanSeeEntityWithLights(candidate, 0); // 0x605550

    // 0x6075B0
    const auto IsAcquaintanceMatching = [&](CPed& p, int32 idx, CPed* cand) {
        const auto mine   = p.GetAcquaintance().GetAcquaintances((AcquaintanceId)idx); // 0x608970
        const auto theirs = CPedType::GetPedFlag(cand->m_nPedType);                    // 0x608830
        if (mine & theirs) {
            return true;
        }
        return CGameLogic::LaRiotsActiveHere() && WantsToRiotAgainst(&p, cand); // 0x441C10, 0x603AF0
    };

    for (int32 idx = 4; idx >= 0; idx--) {
        if (idx == curIdx) {
            break;
        }

        if (acquaintanceId == -1) {
            // Cops are always matched for type 2 (Respect), skipping the checks below
            if (!(idx == 2 && candidate->m_nPedType == PED_TYPE_COP)) {
                if (!IsAcquaintanceMatching(ped, idx, candidate)) {
                    continue;
                }
            }
        } else {
            if (acquaintanceId != idx) {
                continue;
            }
            if (!IsAcquaintanceMatching(ped, idx, candidate)) {
                continue;
            }
        }

        // Light level check (FCOMP + JP semantics: NaN counts as "not greater" and "not equal")
        if (!(lightLevel > 0.0f)) {
            if (idx != 4) {
                continue;
            }
            if (lightLevel == 0.0f) {
                continue;
            }
        }

        outPed = candidate;
        outIdx = idx;
        if (acquaintanceId != -1) {
            return acquaintanceId;
        }
        if (!outPed) { // Note: always false in practice
            continue;
        }

        const bool created = CreateAcquaintanceEvent(ped, idx, outPed); // 0x606BA0

        const int32 interval = (outIdx == 4 && (double)lightLevel < 0.0)
            ? ms_nShortInterval
            : ms_nLongInterval;
        m_timer.m_nStartTime = CTimer::GetTimeInMS();
        m_timer.m_nInterval  = interval;
        m_timer.m_bStarted   = true;
        if (created) {
            return outIdx;
        }
    }
    return -1;
}

// 0x606BA0 (unreversed)
bool CPedAcquaintanceScanner::CreateAcquaintanceEvent(CPed& ped, int32 acquaintanceType, CPed* other) {
    return plugin::CallMethodAndReturn<bool, 0x606BA0, CPedAcquaintanceScanner*, CPed*, int32, CPed*>(this, &ped, acquaintanceType, other);
}
