#include "StdInc.h"

#include <numbers>
#include "TaskSimpleCarDrive.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Ragdoll/IKChainManager.h"
#include "CarEnterExit.h"
#include "PedGroups.h"
#include "Cheat.h"
#include "Weather.h"
#include "AudioEngine.h"
#include "ModelIndices.h"
#include "GameLogic.h"
#include "Camera.h"
#include "Train.h"
#include "Automobile.h"
#include "VehicleAnimGroupData.h"
#include "EventCarUpsideDown.h"
#include "EventCopCarBeingStolen.h"
#include "EventScriptCommand.h"
#include "TaskSimpleGangDriveBy.h"
#include "TaskSimpleCarSetPedOut.h"
#include "TaskSimpleHoldEntity.h"
#include "TaskComplexSequence.h"
#include "TaskComplexLeaveCar.h"
#include "TaskComplexDriveWander.h"

void CTaskSimpleCarDrive::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleCarDrive, 0x86E904, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x63C340);
    RH_ScopedInstall(Destructor, 0x63C460);

    RH_ScopedInstall(TriggerIK, 0x63C500);
    RH_ScopedInstall(UpdateBopping, 0x63C900);
    RH_ScopedInstall(StartBopping, 0x642760);
    RH_ScopedInstall(ProcessHeadBopping, 0x6428C0);
    RH_ScopedInstall(ProcessArmBopping, 0x642AE0);
    RH_ScopedInstall(ProcessBopping, 0x642E70);
    RH_ScopedVMTInstall(Clone, 0x63DC20);
    RH_ScopedVMTInstall(GetTaskType, 0x63C450);
    RH_ScopedVMTInstall(MakeAbortable, 0x63C670);
    RH_ScopedVMTInstall(ProcessPed, 0x644470);
    RH_ScopedVMTInstall(SetPedPosition, 0x63C770);
}

// 0x63C340
CTaskSimpleCarDrive::CTaskSimpleCarDrive(CVehicle* vehicle, CTaskUtilityLineUpPedWithCar* utilityTask, bool updateCurrentVehicle) : CTaskSimple() {
    m_pVehicle = vehicle;
    m_pAnimCloseDoorRolling = nullptr;
    m_pTaskUtilityLineUpPedWithCar = nullptr;
    m_NoDriverTimer = {};
    m_nTimePassedSinceCarUpSideDown = 0;

    m_bUpdateCurrentVehicle = updateCurrentVehicle;
    m_b08 = true;

    CEntity::SafeRegisterRef(m_pVehicle);

    if (utilityTask) {
        m_pTaskUtilityLineUpPedWithCar = new CTaskUtilityLineUpPedWithCar(CVector{}, 0, utilityTask->m_nDoorOpenPosType, utilityTask->m_nDoorIdx);
    }

    m_fHeadBoppingFactor = 0.0f;
    m_fHeadBoppingOrientation = 0.0f;
    m_fRandomHeadBoppingMultiplier = 0.0f;
    m_nBoppingStartTime = -1;
}

// 0x63C460
CTaskSimpleCarDrive::~CTaskSimpleCarDrive() {
    CEntity::SafeCleanUpRef(m_pVehicle);

    if (m_pTaskUtilityLineUpPedWithCar) {
        delete m_pTaskUtilityLineUpPedWithCar;
        m_pTaskUtilityLineUpPedWithCar = nullptr;
    }

    if (m_bClosingDoor && m_pAnimCloseDoorRolling) {
        // TODO: FIX ME: Keeps triggering, annoying as fuck
        // Seemingly happens when getting of a motorbike (like cops getting off)
        //assert(m_pAnimCloseDoorRolling);
        m_pAnimCloseDoorRolling->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
        if (m_pVehicle) {
            m_pVehicle->ClearGettingOutFlags(1);
        }
    }
}

// 0x63C500
void CTaskSimpleCarDrive::TriggerIK(CPed* ped) const {
    if (!m_pVehicle) {
        return;
    }

    // Made an early-out of this, as this same code is used in both possible cases.
    if (g_ikChainMan.IsLooking(ped) || CGeneral::GetRandomNumberInRange(0, 100) >= 5) {
        return;
    }

    switch (m_pVehicle->m_autoPilot.m_nCarMission) {
    case MISSION_RAMPLAYER_FARAWAY:
    case MISSION_RAMPLAYER_CLOSE:
    case MISSION_BLOCKPLAYER_FARAWAY:
    case MISSION_BLOCKPLAYER_CLOSE:
    case MISSION_BLOCKPLAYER_HANDBRAKESTOP: { // Make ped look at player ped
        g_ikChainMan.LookAt("DriveCar", ped, FindPlayerPed(0), 3000, BONE_HEAD, nullptr, false, 0.25f, 500, 3, false);
        break;
    }
    case MISSION_RAMCAR_FARAWAY:
    case MISSION_RAMCAR_CLOSE: {
        if (const auto vehTargetCar = m_pVehicle->m_autoPilot.m_TargetEntity) {
            if (vehTargetCar->GetIsTypeVehicle()) {
                if (const auto driver = vehTargetCar->m_pDriver) { // Make ped look at target car or it's driver (if any)
                    g_ikChainMan.LookAt("DriveCar", ped, driver, 3000, BONE_HEAD, nullptr, false, 0.25f, 500, 3, false);
                } else {
                    g_ikChainMan.LookAt("DriveCar", ped, vehTargetCar, 3000, BONE_UNKNOWN, nullptr, false, 0.25f, 500, 3, false);
                }
            }
        }
        break;
    }
    }
}

// 0x63C900
void CTaskSimpleCarDrive::UpdateBopping() {
    const auto timeDelta = (int32)CTimer::GetTimeInMS() - m_nBoppingStartTime;
    m_fBoppingProgress = (float)(timeDelta % m_nBoppingEndTime) / (float)m_nBoppingEndTime;
    m_nBoppingCompletedTimes = timeDelta / m_nBoppingEndTime % 2;
}

// 0x642760
void CTaskSimpleCarDrive::StartBopping(CPed* ped) {
    // Try to sync up with an existing bopping from the others in the vehicle
    int32 startTime = -1;
    int32 endTime{};

    const auto GetBoppingOf = [&](CPed* other) {
        if (const auto task = static_cast<CTaskSimpleCarDrive*>(other->GetTaskManager().FindActiveTaskByType(TASK_SIMPLE_CAR_DRIVE))) {
            endTime   = task->m_nBoppingEndTime;
            startTime = task->m_nBoppingStartTime;
        }
    };

    if (const auto driver = ped->m_pVehicle->m_pDriver; driver && driver != ped) {
        GetBoppingOf(driver);
    }
    if (startTime == -1) {
        for (auto i = 0u; i < 3u; i++) { // Note: Only checks the first 3 passengers
            const auto passenger = ped->m_pVehicle->m_apPassengers[i];
            if (passenger && passenger != ped) {
                GetBoppingOf(passenger);
                if (startTime != -1) {
                    break;
                }
            }
        }
    }

    if (startTime == -1) { // No one else is bopping => start our own
        m_nBoppingStartTime = CTimer::GetTimeInMS();
        // `rand() & 0xFFFF` * 2^-15 * -60 [truncated]
        const auto rnd = (int32)((float)(CGeneral::GetRandomNumber() & 0xFFFF) * (1.f / 32768.f) * -60.f);
        // 60..119 BPM => ms/beat
        m_nBoppingEndTime       = (int32)(1.f / ((float)(60 - rnd) * (1.f / 60.f)) * 1000.f);
        m_fBoppingProgress      = 0.f;
        return;
    }

    m_nBoppingStartTime = startTime;
    m_nBoppingEndTime   = endTime;

    const auto timeDelta = (int32)CTimer::GetTimeInMS() - startTime;
    m_fBoppingProgress       = (float)(timeDelta % endTime) / (float)endTime;
    m_nBoppingCompletedTimes = timeDelta / endTime % 2;
}

// 0x6428C0
void CTaskSimpleCarDrive::ProcessHeadBopping(CPed* ped, bool a3, float a4) {
    float boppingSin = 0.f;
    if (a3) {
        if (!m_bHeadBopping) {
            if (m_fHeadBoppingFactor == 0.f && CGeneral::GetRandomNumberInRange(0, 1000) > 995) { // Start head bopping
                if (m_nBoppingStartTime == -1) {
                    StartBopping(ped);
                    m_nHeadBoppingStartTime = m_nBoppingStartTime;
                } else {
                    m_nHeadBoppingStartTime = CTimer::GetTimeInMS();
                }
                m_nHeadBoppingDirection        = CGeneral::GetRandomNumberInRange(1, 3);
                m_fRandomHeadBoppingMultiplier = CGeneral::GetRandomNumberInRange(3.f, 8.f);
                m_fHeadBoppingFactor           = 0.f;
                m_fHeadBoppingOrientation      = 0.f;
                boppingSin                     = 0.f;
            } else {
                a3 = false;
            }
        } else {
            boppingSin = std::sin(m_fBoppingProgress * std::numbers::pi_v<float>);
            if (CTimer::GetTimeInMS() - (uint32)m_nHeadBoppingStartTime > 5000u && CGeneral::GetRandomNumberInRange(0, 1000) > 995) { // Stop head bopping
                a3 = false;
            }
        }
    }

    m_bHeadBopping = a3;

    if (m_bHeadBopping) {
        if (m_fHeadBoppingFactor < 1.f) {
            m_fHeadBoppingFactor += 0.05f;
        }
        if (m_fHeadBoppingFactor > 1.f) {
            m_fHeadBoppingFactor = 1.f;
        }
        m_fHeadBoppingOrientation += (boppingSin - m_fHeadBoppingOrientation) * m_fHeadBoppingFactor;
    } else {
        if (m_fHeadBoppingFactor > 0.f) {
            m_fHeadBoppingFactor -= 0.05f;
        }
        if (m_fHeadBoppingFactor < 0.f) {
            m_fHeadBoppingFactor = 0.f;
        }
        m_fHeadBoppingOrientation += (boppingSin - m_fHeadBoppingOrientation) * (1.f - m_fHeadBoppingFactor);
    }

    if (a4 < 64.f && m_fHeadBoppingOrientation > 0.f) {
        const float angle = m_fRandomHeadBoppingMultiplier * m_fHeadBoppingOrientation;
        const auto  quat  = &ped->m_apBones[PED_NODE_HEAD]->KeyFrame->q;
        if (m_nHeadBoppingDirection >= 1) {
            RtQuatRotate(quat, &CPedIK::XaxisIK, m_nBoppingCompletedTimes ? -angle : angle, rwCOMBINEPRECONCAT);
        }
        RtQuatRotate(quat, &CPedIK::ZaxisIK, m_nHeadBoppingDirection == 2 ? angle : -angle, rwCOMBINEPRECONCAT);
        ped->bUpdateMatricesRequired = true;
    }
}

// 0x642AE0
void CTaskSimpleCarDrive::ProcessArmBopping(CPed* ped, bool a3, float a4) {
    if (ped->IsPlayer()) {
        return;
    }

    // Open/close windows
    const auto veh = ped->m_pVehicle;
    if (CWeather::Rain <= 0.f) {
        if (veh->m_pDriver == ped) {
            veh->SetWindowOpenFlag(10);
        } else if (veh->m_apPassengers[0] == ped) {
            veh->SetWindowOpenFlag(8);
        } else if (veh->m_apPassengers[1] == ped) {
            veh->SetWindowOpenFlag(11);
        } else if (veh->m_apPassengers[2] == ped) {
            veh->SetWindowOpenFlag(9);
        }
    } else { // Raining => close all windows
        veh->ClearWindowOpenFlag(10);
        veh->ClearWindowOpenFlag(8);
        veh->ClearWindowOpenFlag(11);
        veh->ClearWindowOpenFlag(9);
        a3 = false;
    }

    // Left/right arm
    const auto animId = veh->m_apPassengers[0] == ped || veh->m_apPassengers[2] == ped
        ? ANIM_ID_TAP_HANDP
        : ANIM_ID_TAP_HAND;
    const auto assoc = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), animId);

    if (!a3) { // Stop bopping
        if (assoc) {
            assoc->m_BlendDelta = -4.f;
        }
        m_bArmBopping = false;
        return;
    }

    if (!m_bArmBopping) { // Maybe start bopping
        if (CGeneral::GetRandomNumberInRange(0, 1000) <= 995) {
            return;
        }

        if (m_pVehicle->m_nVehicleSubType == VEHICLE_TYPE_AUTOMOBILE) {
            const auto door = CCarEnterExit::ComputeTargetDoorToExit(m_pVehicle, ped);

            // Door must be closed, and the ped must be able to lean out
            bool canLeanOut = true;
            if (static_cast<CAutomobile*>(m_pVehicle)->m_damageManager.GetDoorStatus_Component((tComponent)door) != DAMSTATE_OK
                || CCarEnterExit::CarHasDoorToClose(m_pVehicle, door)
                || !m_pVehicle->CanPedLeanOut(ped)
            ) {
                canLeanOut = false;
            }

            // Check if the arm rest is at the right height compared to the seat
            const auto vehStruct = CModelInfo::GetModelInfo(m_pVehicle->m_nModelIndex)->AsVehicleModelInfoPtr()->m_pVehicleStruct;
            const auto& armRest  = vehStruct->m_avDummyPos[DUMMY_HAND_REST];
            if (armRest.x == 0.f && armRest.y == 0.f && armRest.z == 0.f) {
                return;
            }

            const auto armRestHeight = armRest.z - vehStruct->m_avDummyPos[DUMMY_SEAT_FRONT].z;
            if (m_pVehicle->vehicleFlags.bLowVehicle) {
                return;
            }

            if (CVehicleAnimGroupData::GetVehicleAnimGroup(m_pVehicle->m_pHandlingData->m_nAnimGroup).m_specialFlags.bUseTruckDriveAnims) {
                if (armRestHeight < 0.4f || armRestHeight > 0.44f) {
                    return;
                }
            } else {
                if (armRestHeight < 0.39f || armRestHeight > 0.46f) {
                    return;
                }
            }

            if (!canLeanOut) {
                return;
            }
        }

        CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_DEFAULT, animId, 4.f);
        if (m_nBoppingStartTime == -1) {
            StartBopping(ped);
            m_nArmBoppingStartTime = m_nBoppingStartTime;
        } else {
            m_nArmBoppingStartTime = CTimer::GetTimeInMS();
        }
        m_bArmBopping = true;
        return;
    }

    // Already bopping
    if (!assoc) {
        m_bArmBopping = false;
        return;
    }

    if (a4 < 64.f) {
        float angle = 0.f;
        if (m_fBoppingProgress >= 0.75f) {
            angle = (m_fBoppingProgress - 0.75f) * 45.f;
        }
        const auto hand = animId == ANIM_ID_TAP_HAND
            ? PED_NODE_LEFT_HAND
            : PED_NODE_RIGHT_HAND;
        RtQuatRotate(&ped->m_apBones[hand]->KeyFrame->q, &CPedIK::ZaxisIK, -angle, rwCOMBINEPRECONCAT);
        ped->bUpdateMatricesRequired = true;
    }

    if (CTimer::GetTimeInMS() - (uint32)m_nArmBoppingStartTime > 5000u && CGeneral::GetRandomNumberInRange(0, 1000) > 995) { // Stop bopping
        assoc->m_BlendDelta = -4.f;
        m_bArmBopping = false;
    }
}

// 0x642E70
void CTaskSimpleCarDrive::ProcessBopping(CPed* ped, bool a3) {
    if (ped->m_pVehicle->m_pDriver == FindPlayerPed(0)
        || ped->m_nPedType == PED_TYPE_COP
        || ped->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT_AND_STAND_UP)
    ) {
        return;
    }

    auto* vehicle = ped->m_pVehicle;
    if (vehicle->IsAutomobile() && !vehicle->IsSubQuad() && !ped->IsCreatedByMission()) {
        if (m_nBoppingStartTime != -1) { // IsBopping
            UpdateBopping();
        }

        const auto dist = DistanceBetweenPointsSquared(TheCamera.GetPosition(), ped->GetPosition());
        ProcessHeadBopping(ped, a3, dist);
        ProcessArmBopping(ped, a3, dist);
        if (m_nBoppingStartTime != -1 && !m_bHeadBopping && !m_bArmBopping) {
            m_nBoppingStartTime = -1;
        }
    }
}

// 0x63DC20


CTask* CTaskSimpleCarDrive::Clone() const {
    auto task = new CTaskSimpleCarDrive(m_pVehicle);
    task->m_bUpdateCurrentVehicle = m_bUpdateCurrentVehicle;
    return task;
}

// 0x63C670
bool CTaskSimpleCarDrive::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if (priority != ABORT_PRIORITY_IMMEDIATE) {
        m_b08 = true;
        return !m_bClosingDoor;
    }

    // Immediately get the ped out of the vehicle
    if (ped->bInVehicle && ped->m_pVehicle) {
        const auto door = CCarEnterExit::ComputeTargetDoorToExit(ped->m_pVehicle, ped);
        CTaskSimpleCarSetPedOut setPedOut{m_pVehicle, (eTargetDoor)door, false};
        if (ped->m_pVehicle->IsBike() && event) {
            setPedOut.m_bFallingOutOfCar = true;
        }
        setPedOut.ProcessPed(ped);
    }

    if (g_ikChainMan.IsLooking(ped)) {
        g_ikChainMan.AbortLookAt(ped, 250u);
    }

    return true;
}

// 0x644470
bool CTaskSimpleCarDrive::ProcessPed(CPed* ped) {
    const auto radioRetuneJustStarted = AudioEngine.HasRadioRetuneJustStarted();

    // Update the vehicle pointer if needed
    if (m_bUpdateCurrentVehicle && ped->bInVehicle && ped->m_pVehicle && ped->m_pVehicle != m_pVehicle) {
        CEntity::SafeCleanUpRef(m_pVehicle);
        m_pVehicle = ped->m_pVehicle;
        CEntity::SafeRegisterRef(m_pVehicle);
    }

    // Ped isn't in a vehicle (anymore)
    if (!m_pVehicle || !ped->bInVehicle) {
        ped->bInVehicle = false;
        if (g_ikChainMan.IsLooking(ped)) {
            g_ikChainMan.AbortLookAt(ped, 250u);
        }
        return true;
    }

    const auto pedGroup = CPedGroups::GetPedsGroup(ped);

    // Handle upside down vehicle
    if (CTheScripts::UpsideDownCars.IsCarUpsideDown(m_pVehicle)) {
        if (ped != FindPlayerPed(-1)) {
            if (FindPlayerPed(-1)->bInVehicle && FindPlayerPed(-1)->m_pVehicle == m_pVehicle) {
                ped->Say(CTX_GLOBAL_CAR_FLIPPED, 0, 1.f);
            }
        }
        if (!pedGroup || pedGroup->GetMembership().IsLeader(ped)) {
            m_nTimePassedSinceCarUpSideDown += (int32)(CTimer::GetTimeStep() * 0.02f * 1000.f);
            if ((uint32)m_nTimePassedSinceCarUpSideDown > 2000u) {
                ped->GetEventGroup().Add(CEventCarUpsideDown{m_pVehicle});
            }
        }
    } else {
        m_nTimePassedSinceCarUpSideDown = 0;
    }

    // Player is stealing a cop car
    if (ped->IsPlayer() && m_pVehicle->IsLawEnforcementVehicle()) {
        if (!m_copCarStolenTimer.IsStarted() || m_copCarStolenTimer.IsOutOfTime()) {
            m_copCarStolenTimer.Start(2000);
            GetEventGlobalGroup()->Add(CEventCopCarBeingStolen{ped, m_pVehicle});
        }
    }

    if (m_pVehicle->m_nVehicleType == VEHICLE_TYPE_BIKE
        || m_pVehicle->m_nVehicleSubType == VEHICLE_TYPE_QUAD
        || m_pVehicle->m_nModelIndex == MODEL_TRACTOR
    ) {
        ped->bTestForShotInVehicle = true;
    }

    // Helper that creates (and adds an event for) a drive-by task
    const auto StartDriveBy = [&](bool seatRHS) {
        ped->GetEventGroup().Add(CEventScriptCommand{
            TASK_PRIMARY_PRIMARY,
            new CTaskSimpleGangDriveBy{nullptr, nullptr, 100.f, 100, eDrivebyStyle::AI_ALL_DIRN, seatRHS}
        });
    };
    const auto HasSMGWithAmmo = [&] {
        const auto& smg = ped->GetWeaponInSlot(eWeaponSlot::SMG);
        return smg.m_Type != WEAPON_UNARMED && (int32)smg.m_TotalAmmo > 0;
    };

    if (ped->IsPlayer() && m_pVehicle->m_pDriver == ped) { // Player driving
        const auto pad = ped->AsPlayer()->GetPadFromPlayer();

        if (CCheat::IsActive(CHEAT_WEAPON_AIMING_WHILE_DRIVING) && HasSMGWithAmmo() && m_pVehicle->CanPedLeanOut(ped)) {
            StartDriveBy(false);
        }

        const auto veh = ped->m_pVehicle;

        // Returns true if the player isn't giving any input
        const auto IsPadIdle = [&] {
            return !pad || (pad->GetAccelerate() == 0 && pad->GetSteeringLeftRight() == 0 && pad->GetBrake() == 0);
        };

        if (veh->m_nModelIndex != MODEL_STREAKC && veh->m_nVehicleType == VEHICLE_TYPE_TRAIN && !static_cast<CTrain*>(veh)->m_aDoors[DOOR_LEFT_FRONT].IsClosed()) {
            // Train with open door
            if (IsPadIdle()) {
                veh->ProcessOpenDoor(
                    ped,
                    10,
                    CVehicleAnimGroupData::GetGroupForAnim((AssocGroupId)m_pVehicle->m_pHandlingData->m_nAnimGroup, ANIM_ID_CAR_ROLLDOOR),
                    ANIM_ID_CAR_ROLLDOOR,
                    1.f
                );
            }
        } else if (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE && static_cast<CAutomobile*>(veh)->m_damageManager.GetDoorStatus(DOOR_LEFT_FRONT) == DAMSTATE_OPENED) {
            // Driver's door is open
            if (!veh->IsDoorMissing(DOOR_LEFT_FRONT)) {
                const auto doorAnim = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_CAR_ROLLDOOR);

                bool bProcessOpenDoor = false;
                if (!(veh->m_nGettingOutFlags & 1) && !(veh->m_nGettingInFlags & 1)) {
                    if (doorAnim) {
                        bProcessOpenDoor = true;
                    } else if (IsPadIdle()) {
                        // Start closing the door
                        veh->SetGettingOutFlags(1);
                        m_bClosingDoor = true;
                        plugin::CallMethod<0x642700, CTaskSimpleCarDrive*, CPed*>(this, ped); // Plays the "roll door" anim, and sets `m_pAnimCloseDoorRolling`
                        return false;
                    }
                } else if (doorAnim) {
                    bProcessOpenDoor = true;
                }

                if (bProcessOpenDoor) {
                    veh->ProcessOpenDoor(
                        ped,
                        10,
                        CVehicleAnimGroupData::GetGroupForAnim((AssocGroupId)m_pVehicle->m_pHandlingData->m_nAnimGroup, doorAnim->m_AnimId),
                        ANIM_ID_CAR_ROLLDOOR,
                        doorAnim->m_CurrentTime
                    );
                }
                ProcessBopping(ped, false);
            }
        } else {
            m_pVehicle->ProcessDrivingAnims(ped, radioRetuneJustStarted);
            ProcessBopping(ped, true);
        }
    } else if (m_pVehicle->m_pDriver == ped) { // Non-player driver
        m_pVehicle->ProcessDrivingAnims(ped, radioRetuneJustStarted);
        ProcessBopping(ped, true);
    } else if (m_pVehicle->IsPassenger(ped)) { // Passenger
        if (!m_b08 && !m_bPassengerAnimBlended) {
            if (const auto rideAnimData = m_pVehicle->GetRideAnimData()) {
                CAnimManager::BlendAnimation(ped->GetRpClump(), rideAnimData->AnimGroup, ANIM_ID_BIKE_PASSENGER, 8.f);
            }
            m_bPassengerAnimBlended = true;
        }

        ProcessBopping(ped, true);

        if (ped->IsPlayer()
            && CGameLogic::IsCoopGameGoingOn()
            && HasSMGWithAmmo()
            && m_pVehicle->CanPedLeanOut(ped)
            && TheCamera.GetActiveCam().m_nMode == MODE_TWOPLAYER_IN_CAR_AND_SHOOTING
        ) {
            StartDriveBy(m_pVehicle->m_apPassengers[1] != ped);
        }
    }

    // Smoke anim
    if (const auto held = ped->GetEntityThatThisPedIsHolding()) {
        if (held->m_nModelIndex == ModelIndices::MI_GANG_SMOKE && !g_ikChainMan.IsLooking(ped) && CGeneral::GetRandomNumberInRange(0, 60) == 15) {
            if (const auto holdTask = ped->GetTaskManager().Find<CTaskSimpleHoldEntity>()) {
                holdTask->PlayAnim(
                    m_pVehicle->IsPassenger(ped) ? ANIM_ID_PASS_SMOKE_IN_CAR : ANIM_ID_SMOKE_IN_CAR,
                    ANIM_GROUP_DEFAULT
                );
            }
        }
    }

    if (RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_SMOKE_IN_CAR)) { // Smoking => no looking around
        if (g_ikChainMan.IsLooking(ped)) {
            g_ikChainMan.AbortLookAt(ped, 250u);
        }
        if (m_nBoppingStartTime != -1) {
            m_nBoppingStartTime = -1;
        }
    } else {
        TriggerIK(ped);
    }

    // Ambient ped which is the front passenger of a driverless vehicle => leave and wander around after a while
    if (ped->IsCreatedBy(PED_GAME) && !CPedGroups::GetPedsGroup(ped)) {
        if (!m_pVehicle->m_pDriver && m_pVehicle->m_apPassengers[0] == ped) {
            m_NoDriverTimer.StartIfNotAlready(4000);
            if (m_NoDriverTimer.IsOutOfTime()) {
                const auto seq = new CTaskComplexSequence{};
                seq->AddTask(new CTaskComplexLeaveCar{m_pVehicle, 0, 0, true, false});
                seq->AddTask(new CTaskComplexCarDriveWander{m_pVehicle, DRIVING_STYLE_STOP_FOR_CARS, 10.f});
                ped->GetEventGroup().Add(CEventScriptCommand{TASK_PRIMARY_PRIMARY, seq});
            }
        } else {
            m_NoDriverTimer.Stop();
        }
    }

    // Passenger speech
    if (((uint32)ped->m_nRandomSeed + CTimer::GetFrameCounter()) % 16384u == 0) {
        if (const auto driver = m_pVehicle->m_pDriver; driver && driver != ped && driver->IsPlayer()) { // NOTE: The original checks if the *driver* is the player
            if (m_pVehicle->m_vecMoveSpeed.Magnitude2D() > 0.7f) {
                ped->Say(CTX_GLOBAL_CAR_FAST, 0, 1.f);
            }
            if (m_pVehicle->m_vecMoveSpeed.Magnitude2D() < 0.1f) {
                ped->Say(CTX_GLOBAL_CAR_SLOW, 0, 1.f);
            }
            ped->Say(CTX_GLOBAL_CAR_SINGALONG, 0, 1.f);
        }
    }

    return false;
}

// 0x63C770
bool CTaskSimpleCarDrive::SetPedPosition(CPed* ped) {
    if (m_b08) {
        // Take over the line-up utility task from the other drive task (if any)
        if (const auto other = static_cast<CTaskSimpleCarDrive*>(ped->GetTaskManager().FindTaskByType(TASK_PRIMARY_DEFAULT, TASK_SIMPLE_CAR_DRIVE))) {
            if (other != this && other->m_pTaskUtilityLineUpPedWithCar) {
                m_pTaskUtilityLineUpPedWithCar = other->m_pTaskUtilityLineUpPedWithCar;
                other->m_pTaskUtilityLineUpPedWithCar = nullptr;
            }
        }
        m_b08 = false;
    }

    if (!m_pTaskUtilityLineUpPedWithCar) {
        if (m_pVehicle && ped->m_pVehicle == m_pVehicle && ped->bInVehicle) {
            ped->SetPedPositionInCar();
        }
        return true;
    }

    // 0x8D2E9C: Anims which are used by the line-up utility
    static auto& s_lineUpAnims = StaticRef<std::array<AnimationId, 12>>(0x8D2E9C);

    CAnimBlendAssociation* anim{};
    for (const auto animId : s_lineUpAnims) {
        anim = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), animId);
        if (anim) {
            break;
        }
    }
    m_pTaskUtilityLineUpPedWithCar->ProcessPed(ped, m_pVehicle, anim);

    delete m_pTaskUtilityLineUpPedWithCar;
    m_pTaskUtilityLineUpPedWithCar = nullptr;

    return true;
}

