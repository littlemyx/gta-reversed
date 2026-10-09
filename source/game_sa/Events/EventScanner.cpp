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

// Sanity checks for the raw offsets used by the original
static_assert(WEAPON_FLAMETHROWER == 0x25 && WEAPON_FALL == 0x36 && WEAPON_UNIDENTIFIED == 0x37 && PED_PIECE_TORSO == 3);
static_assert(offsetof(CPed, m_fHealth) == 0x540 && offsetof(CPed, m_pContactEntity) == 0x584 && offsetof(CPed, m_pVehicle) == 0x58C && offsetof(CPed, m_pFire) == 0x730);
static_assert(offsetof(CVehicle, m_nVehicleType) == 0x590 && offsetof(CVehicle, m_nVehicleSubType) == 0x594);
static_assert(offsetof(CTaskComplexKillPedOnFoot, m_target) == 0x10);

void CVehiclePotentialCollisionScanner::InjectHooks() {
    RH_ScopedClass(CVehiclePotentialCollisionScanner);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(ScanForVehiclePotentialCollisionEvents, 0x603720);
}

void CEventScanner::InjectHooks() {
    RH_ScopedClass(CEventScanner);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(ScanForEvents, 0x607E30);
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

    // Unreversed scanners (unnamed in the symbol table), the 1st one is a __stdcall that doesn't use `this`
    reinterpret_cast<void(__stdcall*)(CPed*, CEntity*)>(0x606580)(&ped, intel->m_pedScanner.GetClosestPedInRange()); // 0x607E8A
    plugin::CallMethod<0x606890, CObjectPotentialCollisionScanner*, CPed*>(&m_objectPotentialCollisionScanner, &ped); // 0x607E93
    plugin::CallMethod<0x607D80, CPedAcquaintanceScanner*, CPed*, CEntity**, int32>(&m_pedAcquaintanceScanner, &ped, intel->GetPedEntities(), 16); // 0x607EA8
    plugin::CallMethod<0x6060A0, CAttractorScanner*, CPed*>(&m_attractorScanner, &ped); // 0x607EB1
    plugin::CallMethod<0x603E70, CNearbyFireScanner*, CPed*>(&m_nearbyFireScanner, &ped); // 0x607EBD
    plugin::CallMethod<0x6008A0, CMentalState*, CPed*>(&intel->m_mentalState, &ped); // 0x607EC9 (CMentalState::Process, but the stub in MentalHealth.cpp has the wrong signature)

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

    plugin::CallMethod<0x603BF0, CSexyPedScanner*, CPed*, CEntity**, int32>(&m_sexyPedScanner, &ped, intel->GetPedEntities(), 16); // 0x607FFA

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
