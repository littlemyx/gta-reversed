#include "StdInc.h"

#include <extensions/utility.hpp>

#include "TaskComplexKillCriminal.h"
#include "TaskComplexKillPedOnFoot.h"
#include "TaskSimpleGangDriveBy.h"
#include "TaskComplexEnterCarAsPassenger.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "TaskComplexLeaveCar.h"
#include "TaskSimpleCarDrive.h"
#include "TaskComplexCarDriveMission.h"
#include "CopPed.h"
#include "EventAcquaintancePedHate.h"
#include "LoadMonitor.h"
#include "InterestingEvents.h"


void CTaskComplexKillCriminal::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexKillCriminal, 0x870a00, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x68BE70);
    RH_ScopedInstall(Destructor, 0x68BF30);

    RH_ScopedInstall(CreateSubTask, 0x68C050);
    RH_ScopedInstall(FindNextCriminalToKill, 0x68C3C0);
    RH_ScopedInstall(ChangeTarget, 0x68C6E0);

    RH_ScopedVMTInstall(Clone, 0x68CE50);
    RH_ScopedVMTInstall(GetTaskType, 0x68BF20);
    RH_ScopedVMTInstall(MakeAbortable, 0x68DAD0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x68E4F0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x68DC60);
    RH_ScopedVMTInstall(ControlSubTask, 0x68E950);
}

bool NoPedOrNoHp(CPed* ped) {
    return !ped || ped->m_fHealth <= 0.f;
}

// 0x68BE70
CTaskComplexKillCriminal::CTaskComplexKillCriminal(CPed* criminal, bool randomize) :
    m_Randomize{randomize},
    m_Criminal{criminal}
{
    if (   !m_Criminal
        || m_Criminal->IsPlayer()
        || m_Criminal->IsCreatedByMission()
        || notsa::contains({ PED_TYPE_COP, PED_TYPE_MEDIC, PED_TYPE_FIREMAN, PED_TYPE_MISSION1 }, m_Criminal->m_nPedType)
    ) {
        m_Criminal = nullptr;
    }
}

// NOTSA (For 0x68CE50)
CTaskComplexKillCriminal::CTaskComplexKillCriminal(const CTaskComplexKillCriminal& o) :
    CTaskComplexKillCriminal{o.m_Criminal, false}
{
}

// 0x68BF30
CTaskComplexKillCriminal::~CTaskComplexKillCriminal() {
    if (m_Cop) {
        m_Cop->m_nTimeTillWeNeedThisPed = CTimer::GetTimeInMS();
        m_Cop->bCullExtraFarAway = false;
        m_Cop->m_fRemovalDistMultiplier = 1.f;
        if (const auto veh = m_Cop->m_pVehicle) {
            veh->m_nExtendedRemovalRange = false;
            veh->vehicleFlags.bNeverUseSmallerRemovalRange = false;
            if (veh->IsDriver(m_Cop)) {
                const auto ap = &veh->m_autoPilot;
                ap->SetCarMission(MISSION_CRUISE);
                ap->SetDrivingStyle(DRIVING_STYLE_AVOID_CARS);
                ap->SetCruiseSpeed(10);
                if (veh->GetStatus() != STATUS_SIMPLE) {
                    CCarCtrl::JoinCarWithRoadSystem(veh);
                }
                veh->vehicleFlags.bSirenOrAlarm = false;
            }
            veh->vehicleFlags.bSirenOrAlarm = false;
        }
    }
}

// 0x68C050
CTask* CTaskComplexKillCriminal::CreateSubTask(eTaskType tt, CPed* ped, bool force) {
    if (!force && m_pSubTask && m_pSubTask->GetTaskType() == tt) {
        return m_pSubTask;        
    }

    switch (tt) {
    case TASK_COMPLEX_KILL_PED_ON_FOOT: {
        ped->SetCurrentWeapon(WEAPON_PISTOL);
        return new CTaskComplexKillPedOnFoot{ m_Criminal };
    }
    case TASK_FINISHED: {
        if (m_Criminal) {
            m_Criminal->SetPedDefaultDecisionMaker();
        }
        return nullptr;
    }
    case TASK_SIMPLE_GANG_DRIVEBY:
        return new CTaskSimpleGangDriveBy{
            m_Criminal,
            nullptr,
            70.f,
            70,
            eDrivebyStyle::AI_ALL_DIRN,
            false
        };
    case TASK_COMPLEX_ENTER_CAR_AS_PASSENGER:
        return new CTaskComplexEnterCarAsPassenger{ ped->m_pVehicle };
    case TASK_COMPLEX_ENTER_CAR_AS_DRIVER:
        return new CTaskComplexEnterCarAsDriver{ ped->m_pVehicle };
    case TASK_COMPLEX_LEAVE_CAR:
        return new CTaskComplexLeaveCar{ ped->m_pVehicle, 0, 0, true, false };
    case TASK_SIMPLE_CAR_DRIVE:
        return new CTaskSimpleCarDrive{ ped->m_pVehicle };
    case TASK_COMPLEX_CAR_DRIVE_MISSION: {
        const auto oveh = ped->m_pVehicle; // (o)ur (veh)icle
        if (!oveh) {
            return nullptr;
        }

        const auto CreateDriveMission = [&, this](eCarMission mission, float cruiseSpeed, CEntity* traget) {
            return new CTaskComplexCarDriveMission{
                ped->m_pVehicle,
                traget,
                mission,
                DRIVING_STYLE_AVOID_CARS,
                cruiseSpeed
            };
        };

        if (const auto cveh = m_Criminal->GetVehicleIfInOne()) {
            return CreateDriveMission(
                oveh->IsBike()
                    ? MISSION_FOLLOWCAR_CLOSE
                    : MISSION_BLOCKCAR_CLOSE,
                (float)(m_Criminal->m_pVehicle->m_autoPilot.m_nCruiseSpeed) + 10.f,
                cveh
            );
        } else {
            return CreateDriveMission(MISSION_KILLPED_CLOSE, 20.f, m_Criminal);
        }
    }
    default:
        NOTSA_UNREACHABLE();
        return nullptr; // 0x68C37F: default => edi = 0
    }
}

// 0x68C3C0
CPed* CTaskComplexKillCriminal::FindNextCriminalToKill(CPed* ped, bool any) {
    const auto [closest, distSq] = notsa::SpatialQuery(
        m_Cop->m_apCriminalsToKill | rng::views::filter(notsa::Not(NoPedOrNoHp)),
        m_Cop->GetPosition(),
        m_Criminal.Get(),
        !any && NoPedOrNoHp(m_Criminal)
            ? m_Criminal.Get()
            : nullptr
    );
    return closest;
}

// 0x68C6E0
bool CTaskComplexKillCriminal::ChangeTarget(CPed* newTarget) { // TODO: Figure out if `newTarget` is actually the new target, or it's just he ped that is the owner of this task
    if (newTarget == m_Criminal) {
        return true;
    }

    if (NoPedOrNoHp(newTarget)) {
        return false;
    }

    if (m_Criminal && m_Criminal->bInVehicle) {
        return false;
    }

    if (notsa::isa_and_nonnull<CTaskComplexKillPedOnFoot>(m_pSubTask) && !m_pSubTask->MakeAbortable(newTarget, ABORT_PRIORITY_URGENT, nullptr)) {
        return false;
    }

    if (!notsa::contains(m_Cop->m_apCriminalsToKill, newTarget)) { // 0x68c760
        return false;
    }

    m_Criminal = newTarget;

    // Propagate change to partner
    if (const auto partner = m_Cop->m_pCopPartner) {
        if (partner->bInVehicle) {
            if (const auto partnersTask = partner->GetTaskManager().Find<CTaskComplexKillCriminal>(false)) {
                notsa::cast<CTaskComplexKillCriminal>(partnersTask)->ChangeTarget(newTarget);
            }
        }
    }

    m_HasFinished = false;

    return true;
}

// 0x68DAD0
bool CTaskComplexKillCriminal::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if ([&, this]{
        if (!event) {
            return true;
        }

        // Code @ 0x68DB32 has been inlined into the stuff below

        switch (const auto evType = event->GetEventType()) {
        case EVENT_ACQUAINTANCE_PED_HATE:
        case EVENT_VEHICLE_DAMAGE_COLLISION: // Ignore these
            return false;

        case EVENT_DAMAGE:
        case EVENT_VEHICLE_DAMAGE_WEAPON:
        case EVENT_GUN_AIMED_AT:
        case EVENT_SHOT_FIRED: {
            const auto evSrc  = event->GetSourceEntity();
            if (m_Criminal && evSrc == m_Criminal) {
                return false; // As per 0x68DB32
            }
            if (!evSrc || !evSrc->GetIsTypePed() || evSrc->AsPed()->IsPlayer()) {
                return false;
            }
            const auto evSrcPed = evSrc->AsPed();
            if (!m_Cop || m_Cop->AddCriminalToKill(evSrcPed) == (notsa::IsFixBugs() ? -1 : 0)) {
                return false;
            }
            if (notsa::contains({ EVENT_DAMAGE, EVENT_VEHICLE_DAMAGE_WEAPON }, evType)) { // Change target immediately
                if (!m_Criminal || (m_Criminal->GetPosition() - ped->GetPosition()).SquaredMagnitude() <= sq(25.f)) {
                    ChangeTarget(evSrcPed);
                }
            }
            return false;
        }
        }
        return true;
    }()) {
        return m_pSubTask->MakeAbortable(ped, priority, event);
    } else {
        const_cast<CEvent*>(event)->m_nTimeActive++; // ???????
        return false;
    }
}

// 0x68E4F0
CTask* CTaskComplexKillCriminal::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_GANG_DRIVEBY:
        return CreateSubTask(
            m_Cop->m_isTheDriver
                ? TASK_COMPLEX_CAR_DRIVE_MISSION
                : TASK_SIMPLE_CAR_DRIVE,
            ped
        );
    case TASK_COMPLEX_KILL_PED_ON_FOOT: { // Try finding the next criminal, if none, set `m_finished` and get into *the* car (if possible)
        CPed* const nextCriminal = NoPedOrNoHp(m_Criminal)
            ? FindNextCriminalToKill(ped, true)
            : m_Criminal.Get();
        if (nextCriminal && ChangeTarget(nextCriminal)) {
            return CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped, true);
        }

        // No criminal, or can't target it, so just bail, so try getting back into the car
        m_HasFinished = true;
        if (m_CantGetInCar || !ped->m_pVehicle) {
            return CreateSubTask(TASK_FINISHED, ped);
        }

        if (!m_Cop->m_isTheDriver) {
            if (NoPedOrNoHp(m_Cop->m_pCopPartner)) { // Partner is dead, we get in as the driver
                m_Cop->m_isTheDriver = true;
                m_Cop->SetPartner(nullptr);
            } else {
                return CreateSubTask(TASK_COMPLEX_ENTER_CAR_AS_PASSENGER, ped);
            }
        }

        return CreateSubTask(TASK_COMPLEX_ENTER_CAR_AS_DRIVER, ped);
    }
    case TASK_COMPLEX_ENTER_CAR_AS_DRIVER: { // 0x68E533
        if (!ped->bInVehicle) {
            return CreateSubTask(
                m_HasFinished || NoPedOrNoHp(m_Criminal)
                    ? TASK_FINISHED
                    : TASK_COMPLEX_KILL_PED_ON_FOOT,
                ped
            );
        }
        const auto copPartnerNoneOrInVeh = NoPedOrNoHp(m_Cop->m_pCopPartner) || m_Cop->m_pCopPartner->bInVehicle;
        if (!m_HasFinished && !NoPedOrNoHp(m_Criminal) && !m_Criminal->IsInVehicle()) {
            return CreateSubTask(
                !ped->m_pVehicle || m_Criminal->IsEntityInRange(ped->m_pVehicle, 25.f) // 0x68E5D8
                    ? TASK_COMPLEX_KILL_PED_ON_FOOT
                    : copPartnerNoneOrInVeh // otherwise if criminal is too far chase them with the car
                        ? TASK_COMPLEX_CAR_DRIVE_MISSION
                        : TASK_SIMPLE_CAR_DRIVE,
                ped
            );
        }
        if (const auto next = FindNextCriminalToKill(ped, true); next && ChangeTarget(next)) { // Try finding another criminal to kill
            return CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped, true);
        }
        // No criminal to kill, so get into *the* vehicle and fuck off
        if (ped->IsInVehicle()) {
            ped->m_pVehicle->vehicleFlags.bSirenOrAlarm = false;
        }
        return CreateSubTask(
            copPartnerNoneOrInVeh
                ? TASK_FINISHED
                : TASK_SIMPLE_CAR_DRIVE,
            ped
        );
    }
    case TASK_COMPLEX_ENTER_CAR_AS_PASSENGER: { // 0x68E6FA
        if (ped->bInVehicle) { // (Inverted)
            return CreateSubTask(TASK_SIMPLE_CAR_DRIVE, ped);
        }
        m_CantGetInCar = true;
        if (m_HasFinished || NoPedOrNoHp(m_Criminal)) {
            return CreateSubTask(TASK_FINISHED, ped);
        } 
        return CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped);
    }
    case TASK_COMPLEX_LEAVE_CAR: // 0x68E77F
        return CreateSubTask(
            !ped->bInVehicle || m_CantGetInCar || (!m_HasFinished && !NoPedOrNoHp(m_Criminal) && !m_Criminal->IsInVehicle() && m_Criminal->IsEntityInRange(ped, 25.f))
                ? TASK_COMPLEX_KILL_PED_ON_FOOT     // Criminal can be killed on foot
                : TASK_COMPLEX_ENTER_CAR_AS_DRIVER, // We have to chase the criminal with a vehicle
            ped
        );
    default:
        NOTSA_UNREACHABLE();
        return nullptr; // 0x68E936: default => edi = 0
    }
}

// 0x68DC60
CTask* CTaskComplexKillCriminal::CreateFirstSubTask(CPed* ped) {
    if (!m_Criminal || m_Criminal->IsPlayer()) {
        return nullptr;
    }

    //> 0x68DC96 - Cops only do this if the player isn't wanted & ambient crime is enabled
    if (FindPlayerWanted(-1)->GetWantedLevel() != eWantedLevel::WANTED_CLEAN) {
        return nullptr;
    }
    if (!g_LoadMonitor.IsAmbientCrimeEnabled()) {
        return nullptr;
    }
    if (ped->m_nPedType != PED_TYPE_COP) {
        return nullptr;
    }

    //> 0x68DCDB - Don't bother if the player is in the criminal's vehicle
    if (const auto cveh = m_Criminal->m_pVehicle) {
        if (cveh->m_pDriver && cveh->m_pDriver->IsPlayer()) {
            return nullptr;
        }
        for (auto i = 0u; i < (uint32)m_Criminal->m_pVehicle->m_nMaxPassengers; i++) {
            if (const auto pass = m_Criminal->m_pVehicle->m_apPassengers[i]; pass && pass->IsPlayer()) {
                return nullptr;
            }
        }
    }

    m_Cop = static_cast<CCopPed*>(ped);

    //> 0x68DD63 - If randomizing, only go after the criminal under some conditions
    if (m_Randomize && !m_Criminal->bWantedByPolice) {
        if (!m_Criminal->bInVehicle || !m_Criminal->m_pVehicle || !m_Criminal->m_pVehicle->vehicleFlags.bMadDriver) {
            return nullptr;
        }
        if (CGeneral::GetRandomNumberInRange(0, 3) != 0) {
            return nullptr;
        }
    }

    m_Cop->AddCriminalToKill(m_Criminal);
    if (m_Criminal->bInVehicle) {
        m_Criminal->GetIntelligence()->SetPedDecisionMakerType(6);
    }

    CTask* task{};

    if (m_Cop->bInVehicle && m_Cop->m_pVehicle) {
        const auto cop = m_Cop.Get();
        const auto veh = cop->m_pVehicle;

        const auto InformPartner = [&](CPed* partner) { // Makes the partner target the same criminal
            CEventAcquaintancePedHate event{ m_Criminal, TASK_COMPLEX_KILL_CRIMINAL };
            partner->GetEventGroup().Add(&event, false);
        };

        if (veh->m_pDriver == ped) { //> 0x68DE09 - We're the driver
            cop->m_isTheDriver = true;

            if (!cop->m_pCopPartner) { // Find a partner in the vehicle
                for (auto i = 0u; i < (uint32)cop->m_pVehicle->m_nMaxPassengers; i++) {
                    if (const auto pass = cop->m_pVehicle->m_apPassengers[i]; pass && pass->m_nPedType == PED_TYPE_COP) {
                        cop->SetPartner(static_cast<CCopPed*>(pass));
                        break;
                    }
                }
            }
            if (cop->m_pCopPartner) {
                InformPartner(cop->m_pCopPartner);
            }
            goto SubTaskForVehicle;
        } else { //> 0x68DEF3 - We're a passenger
            if (const auto driver = veh->m_pDriver; driver && driver->m_nPedType == PED_TYPE_COP) {
                cop->SetPartner(static_cast<CCopPed*>(driver));
                static_cast<CCopPed*>(cop->m_pVehicle->m_pDriver)->m_isTheDriver = true;
            }

            if (!cop->m_pCopPartner) { // 0x68DFCD
                cop->m_isTheDriver = true;
                task = m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_LEAVE_CAR
                    ? m_pSubTask
                    : new CTaskComplexLeaveCar{ ped->m_pVehicle, 0, 0, true, false };
                if (task) {
                    goto Finish;
                }
                goto SubTaskForVehicle;
            }

            if (cop->m_pCopPartner->m_isTheDriver) { // 0x68DF2D
                const auto partnersTask = cop->m_pCopPartner->GetIntelligence()->FindTaskByType(TASK_COMPLEX_KILL_CRIMINAL);
                if (partnersTask && static_cast<CTaskComplexKillCriminal*>(partnersTask)->m_Criminal == m_Criminal) {
                    cop->m_isTheDriver = false;
                    cop->bDontDragMeOutCar = true;
                    goto SubTaskForVehicle;
                }
                InformPartner(cop->m_pCopPartner);
                return nullptr;
            }
            goto SubTaskForVehicle;
        }
    } else { //> 0x68E273 - Not in a vehicle
        const auto cop = m_Cop.Get();
        if (!cop->m_pCopPartner || cop->m_pCopPartner->m_fHealth <= 0.f) {
            cop->m_isTheDriver = true;
        }
        task = CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped);
        goto Finish;
    }

SubTaskForVehicle:
    //> 0x68E036
    if (m_Criminal->bInVehicle && m_Criminal->m_pVehicle) {
        if (m_Cop->m_isTheDriver) {
            task = CreateSubTask(TASK_COMPLEX_CAR_DRIVE_MISSION, ped);
        } else {
            task = CreateSubTask(TASK_SIMPLE_CAR_DRIVE, ped);
        }
    } else {
        task = CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped);
    }

Finish:
    //> 0x68E2DE
    if (ped->m_pVehicle && m_Cop->m_isTheDriver) {
        m_OrigDrivingMode = (int8)ped->m_pVehicle->m_autoPilot.m_nCarDrivingStyle;
        m_OrigMission     = (int8)ped->m_pVehicle->m_autoPilot.m_nCarMission;
        m_OrigCruiseSpeed = ped->m_pVehicle->m_autoPilot.m_nCruiseSpeed;
        m_IsSetUp         = true;
    }

    //> 0x68E320 - Arm the criminal (and its passengers)
    if (const auto cveh = m_Criminal->m_pVehicle) {
        if (cveh->vehicleFlags.bMadDriver) {
            if (cveh->m_nNumPassengers > 0) {
                m_Criminal->GiveWeapon(WEAPON_PISTOL, 1000, true);
                m_Criminal->SetCurrentWeapon(WEAPON_PISTOL);
                for (auto i = 0u; i < (uint32)m_Criminal->m_pVehicle->m_nMaxPassengers; i++) {
                    if (const auto pass = m_Criminal->m_pVehicle->m_apPassengers[i]) {
                        pass->GiveWeapon(WEAPON_PISTOL, 1000, true);
                        pass->SetCurrentWeapon(WEAPON_PISTOL);
                        m_Cop->AddCriminalToKill(pass);
                    }
                }
            } else if (rand() & 1) {
                m_Criminal->GiveWeapon(WEAPON_PISTOL, 1000, true);
                m_Criminal->SetCurrentWeapon(WEAPON_PISTOL);
            }
        }
    } else if (rand() & 1) {
        m_Criminal->GiveWeapon(WEAPON_PISTOL, 1000, true);
        m_Criminal->SetCurrentWeapon(WEAPON_PISTOL);
        CEventAcquaintancePedHate event{ ped, TASK_COMPLEX_KILL_PED_ON_FOOT };
        m_Criminal->GetEventGroup().Add(&event, false);
    }

    //> 0x68E471
    ped->m_nTimeTillWeNeedThisPed = CTimer::GetTimeInMS() + 300'000;
    ped->bCullExtraFarAway        = true;
    ped->m_fRemovalDistMultiplier = 0.3f;
    if (const auto veh = ped->m_pVehicle) {
        veh->m_nExtendedRemovalRange                  = 0xFF;
        veh->vehicleFlags.bNeverUseSmallerRemovalRange = true;
    }
    if (ped->m_pVehicle && ped->bInVehicle) {
        ped->m_pVehicle->vehicleFlags.bSirenOrAlarm = true;
    }
    g_InterestingEvents.Add(CInterestingEvents::INTERESTING_EVENT_25, ped);
    return task;
}

// 0x68E950
CTask* CTaskComplexKillCriminal::ControlSubTask(CPed* ped) {
    // x87: All the distance related calculations are kept in extended precision until the comparison
    const auto SqMag = [](const CVector& v) { return (double)v.x * (double)v.x + (double)v.y * (double)v.y + (double)v.z * (double)v.z; };
    const auto Mag   = [&](const CVector& v) { return std::sqrt(SqMag(v)); };

    CTask* const origSubTask   = m_pSubTask;
    eTaskType    taskToCreate  = TASK_NONE; // TASK_NONE => keep the current one

    if (m_Criminal) {
        if (   m_Criminal->IsPlayer()
            || m_Criminal->m_nPedType == PED_TYPE_COP
            || m_Criminal->m_nPedType == PED_TYPE_MEDIC
            || m_Criminal->m_nPedType == PED_TYPE_FIREMAN
            || (int32)m_Criminal->m_nPedType >= (int32)PED_TYPE_MISSION1
            || m_Criminal->IsCreatedByMission()
        ) {
            return nullptr;
        }
    }

    if (FindPlayerWanted(-1)->GetWantedLevel() != eWantedLevel::WANTED_CLEAN) {
        if (FindPlayerWanted(-1)->CanCopJoinPursuit(static_cast<CCopPed*>(ped)) && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            return nullptr;
        }
    }

    if (!g_LoadMonitor.IsAmbientCrimeEnabled()) {
        return nullptr;
    }

    const auto cop     = m_Cop.Get();
    const auto partner = cop->m_pCopPartner; // NOTE: Intentionally captured before it's reset below
    if (!cop->m_isTheDriver && (!partner || partner->m_fHealth <= 0.f)) { // Partner is dead or doesn't exist => we're the driver now
        cop->m_isTheDriver = true;
        cop->SetPartner(nullptr);
        if (ped->bInVehicle && ped->m_pVehicle) {
            taskToCreate = TASK_COMPLEX_LEAVE_CAR;
        }
    }

    if (ped->m_fHealth <= 0.f) {
        taskToCreate = TASK_FINISHED;
        goto Passengers;
    }

    if (m_Criminal && m_Criminal->m_fHealth > 0.f) {
        if (!cop->m_isTheDriver) { //> 0x68EAD9
            taskToCreate = TASK_COMPLEX_KILL_PED_ON_FOOT;
            // NOTE: `partner` is not null checked in the original (can't be null here due to the above)
            if (   (partner->bInVehicle || partner->GetIntelligence()->FindTaskByType(TASK_COMPLEX_ENTER_CAR_AS_DRIVER))
                && ped->m_pVehicle
                && !m_CantGetInCar
            ) {
                if (!ped->bInVehicle) {
                    taskToCreate = TASK_COMPLEX_ENTER_CAR_AS_PASSENGER;
                } else if (partner->bInVehicle) {
                    taskToCreate = Mag(m_Criminal->GetPosition() - ped->m_pVehicle->GetPosition()) < 60.0
                        ? TASK_SIMPLE_GANG_DRIVEBY
                        : TASK_SIMPLE_CAR_DRIVE;
                } else {
                    taskToCreate = TASK_SIMPLE_CAR_DRIVE;
                }
            }
            goto BikeSection;
        }

        //> 0x68EBB4 - We're the driver
        if (ped->m_pVehicle && ped->bInVehicle) {
            ped->m_pVehicle->vehicleFlags.bSirenOrAlarm = true;
        }

        if (m_Criminal->bInVehicle && m_Criminal->m_pVehicle) { //> 0x68EBE3
            const auto cveh      = m_Criminal->m_pVehicle;
            const bool isCritical = cveh->m_fHealth < 250.f; // Criminal's vehicle is almost dead

            if (ped->bInVehicle && ped->m_pVehicle) { //> 0x68EC1A
                const auto subTaskType = m_pSubTask->GetTaskType();
                if (subTaskType == TASK_COMPLEX_CAR_DRIVE_MISSION || subTaskType == TASK_SIMPLE_GANG_DRIVEBY) { //> 0x68EC8C
                    if (Mag(m_Criminal->m_pVehicle->GetMoveSpeed()) >= 0.12) {
                        m_TimeToGetOutOfCar = 1.f;
                    } else {
                        m_TimeToGetOutOfCar = (float)((double)m_TimeToGetOutOfCar - (double)CTimer::GetTimeStep() * (double)0.02f);
                        if (m_TimeToGetOutOfCar <= 0.f
                            && m_Criminal->GetIntelligence()->GetEventHandler().GetCurrentEventType() != EVENT_ACQUAINTANCE_PED_HATE) {
                            const auto diff = m_Criminal->m_pVehicle->GetPosition() - ped->m_pVehicle->GetPosition();
                            if (SqMag(diff) < 225.0) {
                                taskToCreate = TASK_COMPLEX_KILL_PED_ON_FOOT;
                                if (const auto next = FindNextCriminalToKill(ped, false)) {
                                    ChangeTarget(next);
                                }
                            }
                        }
                    }
                } else if (subTaskType == TASK_SIMPLE_CAR_DRIVE) {
                    if (!partner || partner->m_fHealth <= 0.f || partner->bInVehicle) {
                        taskToCreate = TASK_COMPLEX_CAR_DRIVE_MISSION;
                    }
                }
                if (!isCritical) {
                    goto BikeSection;
                }
                goto CriticalEvent;
            } else { //> 0x68ED83 - We're on foot
                if (Mag(cveh->GetMoveSpeed()) < 0.2) {
                    if (SqMag(m_Criminal->GetPosition() - ped->GetPosition()) < 36.0) {
                        goto CriticalEvent;
                    }
                    if (partner && !partner->bInVehicle) {
                        if (SqMag(m_Criminal->GetPosition() - partner->GetPosition()) < 36.0) {
                            goto CriticalEvent;
                        }
                    }
                } else {
                    if (ped->m_pVehicle
                        && !m_CantGetInCar
                        && m_pSubTask->GetTaskType() != TASK_COMPLEX_ENTER_CAR_AS_DRIVER
                        && m_pSubTask->GetTaskType() != TASK_COMPLEX_ENTER_CAR_AS_PASSENGER
                    ) {
                        const float vehDistSq = (float)SqMag(m_Criminal->m_pVehicle->GetPosition() - ped->m_pVehicle->GetPosition());
                        if (Mag(m_Criminal->m_pVehicle->GetMoveSpeed()) >= 0.2 || vehDistSq > 400.f) {
                            taskToCreate = cop->m_isTheDriver ? TASK_COMPLEX_ENTER_CAR_AS_DRIVER : TASK_COMPLEX_ENTER_CAR_AS_PASSENGER;
                        }
                    }
                }
                if (!isCritical) {
                    goto BikeSection;
                }
                goto CriticalEvent;
            }
        } else { //> 0x68F0A9 - Criminal is on foot
            if (ped->bInVehicle && ped->m_pVehicle) {
                const auto   veh        = ped->m_pVehicle;
                const double crimDistSq = SqMag(m_Criminal->GetPosition() - veh->GetPosition());
                const bool   partnerGone = !partner || partner->m_fHealth <= 0.f || partner->bInVehicle;
                if (crimDistSq > 225.0) {
                    if (partnerGone) {
                        taskToCreate = TASK_COMPLEX_CAR_DRIVE_MISSION;
                    }
                } else {
                    const double thr = veh->m_nVehicleType == VEHICLE_TYPE_BIKE ? (double)5.f : (double)16.f;
                    const auto   mission = (int32)veh->m_autoPilot.m_nCarMission;
                    if (mission == MISSION_KILLPED_CLOSE || mission == MISSION_KILLPED_FARAWAY) {
                        if (crimDistSq < thr) {
                            taskToCreate = TASK_COMPLEX_KILL_PED_ON_FOOT;
                        }
                    } else {
                        taskToCreate = TASK_COMPLEX_KILL_PED_ON_FOOT;
                    }
                }
            } else { //> 0x68F1B1 - Both on foot
                if (!m_CantGetInCar && ped->m_pVehicle) {
                    bool findNext = true;
                    if (SqMag(m_Criminal->GetPosition() - ped->GetPosition()) <= 625.0) {
                        findNext = SqMag(ped->m_pVehicle->GetPosition() - ped->GetPosition()) > 250.0;
                    }
                    if (findNext) {
                        const auto next = FindNextCriminalToKill(ped, false);
                        taskToCreate = TASK_COMPLEX_KILL_PED_ON_FOOT;
                        if (!next || !ChangeTarget(next)) {
                            taskToCreate = TASK_COMPLEX_ENTER_CAR_AS_DRIVER;
                        }
                    }
                }
            }
            goto BikeSection;
        }
    } else { //> 0x68F476 - Criminal is dead
        if (m_pSubTask->GetTaskType() == TASK_COMPLEX_KILL_PED_ON_FOOT) {
            goto Passengers;
        }
        if (const auto next = FindNextCriminalToKill(ped, true); next && ChangeTarget(next)) {
            goto Passengers;
        }
        if (m_pSubTask->GetTaskType() == TASK_COMPLEX_ENTER_CAR_AS_PASSENGER || m_pSubTask->GetTaskType() == TASK_COMPLEX_ENTER_CAR_AS_DRIVER) {
            goto Passengers;
        }

        if (ped->m_pVehicle && !ped->bInVehicle && !m_CantGetInCar) {
            taskToCreate = cop->m_isTheDriver ? TASK_COMPLEX_ENTER_CAR_AS_DRIVER : TASK_COMPLEX_ENTER_CAR_AS_PASSENGER;
        } else if (!ped->bInVehicle) {
            taskToCreate = TASK_FINISHED;
        } else if (m_pSubTask->GetTaskType() != TASK_SIMPLE_CAR_DRIVE || !partner || partner->m_fHealth <= 0.f || partner->bInVehicle) {
            taskToCreate = TASK_FINISHED;
        }
        goto Passengers;
    }

CriticalEvent:
    //> 0x68EF56 - Make the criminal (and everyone in the vehicle) get out of the vehicle & fight
    {
        const auto cveh = m_Criminal->m_pVehicle;

        CEventAcquaintancePedHate event{ ped };
        if (cveh->m_fHealth >= 250.f) {
            // BUG: The original checks if the address of the criminal's weapon is non-null (which it never is), so the result is always the same
            event.m_TaskId = TASK_COMPLEX_KILL_PED_AND_REENTER_CAR; // Otherwise it would be `TASK_COMPLEX_SMART_FLEE_ENTITY`
        } else {
            event.m_TaskId = TASK_COMPLEX_LEAVE_CAR;
        }

        m_Criminal->GetEventGroup().Add(&event, false);
        m_Criminal->SetPedDefaultDecisionMaker();

        if (const auto driver = cveh->m_pDriver; driver != m_Criminal.Get() && driver && !driver->IsPlayer()) {
            driver->GetEventGroup().Add(&event, false);
        }

        for (auto i = 0u; i < (uint32)m_Criminal->m_pVehicle->m_nMaxPassengers; i++) {
            const auto pass = m_Criminal->m_pVehicle->m_apPassengers[i];
            if (pass && pass != m_Criminal.Get() && !pass->IsPlayer()) {
                pass->GetEventGroup().Add(&event, false);
                pass->SetPedDefaultDecisionMaker();
            }
        }
    }

BikeSection:
    //> 0x68F2A1 - If we're on a bike, do a driveby
    if (ped->m_pVehicle && ped->m_pVehicle->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        bool setupDriveBy = false;
        const auto subTaskType = m_pSubTask->GetTaskType();
        if (subTaskType == TASK_COMPLEX_CAR_DRIVE_MISSION && (taskToCreate == TASK_NONE || taskToCreate == subTaskType)) {
            if (Mag(m_Criminal->GetPosition() - ped->m_pVehicle->GetPosition()) < 60.0) {
                taskToCreate = TASK_SIMPLE_GANG_DRIVEBY;
                setupDriveBy = true;
            }
        }
        if (!setupDriveBy) {
            setupDriveBy = taskToCreate == TASK_SIMPLE_GANG_DRIVEBY || m_pSubTask->GetTaskType() == TASK_SIMPLE_GANG_DRIVEBY;
        }
        if (setupDriveBy) {
            const auto veh = ped->m_pVehicle;
            if (m_Criminal->bInVehicle && m_Criminal->m_pVehicle) {
                veh->SetStatus(STATUS_PHYSICS);
                veh->m_autoPilot.m_nCarMission = MISSION_FOLLOWCAR_CLOSE;
                veh->m_autoPilot.m_nCruiseSpeed = (uint8)(int32)((double)m_Criminal->m_pVehicle->m_autoPilot.m_nCruiseSpeed + 10.0);
                veh->m_autoPilot.m_fMaxTrafficSpeed = (float)veh->m_autoPilot.m_nCruiseSpeed;
                veh->m_autoPilot.m_nCarDrivingStyle = DRIVING_STYLE_AVOID_CARS;
                veh->m_autoPilot.m_TargetEntity = m_Criminal->m_pVehicle;
            } else {
                veh->SetStatus(STATUS_PHYSICS);
                veh->m_autoPilot.m_nCarMission = MISSION_KILLPED_CLOSE;
                veh->m_autoPilot.m_nCruiseSpeed = 20;
                veh->m_autoPilot.m_fMaxTrafficSpeed = (float)veh->m_autoPilot.m_nCruiseSpeed;
                veh->m_autoPilot.m_nCarDrivingStyle = DRIVING_STYLE_AVOID_CARS;
                // BUG: This is a ped (the criminal), not a vehicle!
                veh->m_autoPilot.m_TargetEntity = reinterpret_cast<CVehicle*>(m_Criminal.Get());
            }
        }
    }

Passengers:
    //> 0x68F53C - Make the criminal's passengers do a driveby on us (if they are near)
    if (m_Criminal && m_Criminal->bInVehicle && m_Criminal->m_pVehicle && m_Criminal->m_pVehicle->m_nNumPassengers > 0) {
        if (Mag(m_Criminal->m_pVehicle->GetPosition() - ped->GetPosition()) < 60.0) {
            for (auto i = 0u; i < (uint32)m_Criminal->m_pVehicle->m_nMaxPassengers; i++) {
                const auto pass = m_Criminal->m_pVehicle->m_apPassengers[i];
                if (pass && pass->bInVehicle && !pass->GetIntelligence()->FindTaskByType(TASK_SIMPLE_GANG_DRIVEBY)) {
                    pass->GetTaskManager().SetTask(
                        new CTaskSimpleGangDriveBy{ ped, nullptr, 70.f, 70, eDrivebyStyle::AI_ALL_DIRN, false },
                        TASK_PRIMARY_PRIMARY,
                        false
                    );
                }
            }
        }
    }

    //> 0x68F691 - Create the new subtask (if needed)
    if (taskToCreate == TASK_NONE) {
        return origSubTask;
    }
    if (!m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
        return origSubTask;
    }
    if (m_pSubTask && m_pSubTask->GetTaskType() == taskToCreate) {
        return m_pSubTask;
    }
    switch (taskToCreate) {
    case TASK_COMPLEX_ENTER_CAR_AS_PASSENGER:
    case TASK_COMPLEX_ENTER_CAR_AS_DRIVER:
    case TASK_COMPLEX_LEAVE_CAR:
    case TASK_SIMPLE_CAR_DRIVE:
    case TASK_COMPLEX_CAR_DRIVE_MISSION:
        return CreateSubTask(taskToCreate, ped);
    case TASK_COMPLEX_KILL_PED_ON_FOOT: {
        const auto task = new CTaskComplexKillPedOnFoot{ m_Criminal };
        ped->SetCurrentWeapon(WEAPON_PISTOL);
        return task;
    }
    case TASK_SIMPLE_GANG_DRIVEBY: {
        const auto task = new CTaskSimpleGangDriveBy{ m_Criminal, nullptr, 70.f, 70, eDrivebyStyle::AI_ALL_DIRN, false };
        ped->SetCurrentWeapon(WEAPON_PISTOL);
        return task;
    }
    case TASK_FINISHED:
        return CreateSubTask(TASK_FINISHED, ped);
    default:
        return nullptr;
    }
}
