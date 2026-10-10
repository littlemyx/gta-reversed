#include "StdInc.h"

#include <numbers>
#include "PedStats.h"
#include "CarEnterExit.h"
#include "TaskSimpleCarSetPedInAsDriver.h"
#include "TaskComplexDriveWander.h"
#include "TaskSimpleCarSetPedInAsPassenger.h"
#include "EventPedEnteredMyVehicle.h"
#include "PlayerPed.h"
#include "Bike.h"
#include "Automobile.h"
#include "VehicleAnimGroupData.h"
#include "Models/VehicleModelInfo.h"

void CCarEnterExit::InjectHooks() {
    RH_ScopedClass(CCarEnterExit);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(AddInCarAnim, 0x64F720);
    RH_ScopedInstall(CarHasDoorToClose, 0x64EE10);
    RH_ScopedInstall(CarHasDoorToOpen, 0x64EDD0);
    RH_ScopedInstall(CarHasOpenableDoor, 0x64EE50);
    RH_ScopedInstall(CarHasPartiallyOpenDoor, 0x64EE70);
    RH_ScopedInstall(ComputeDoorFlag, 0x64E550);
    RH_ScopedInstall(ComputeOppositeDoorFlag, 0x64E610);
    RH_ScopedInstall(ComputePassengerIndexFromCarDoor, 0x64F1E0);
    RH_ScopedInstall(ComputeSlowJackedPed, 0x64F070);
    RH_ScopedInstall(ComputeTargetDoorToEnterAsPassenger, 0x64F190);
    RH_ScopedInstall(ComputeTargetDoorToExit, 0x64F110);
    RH_ScopedInstall(GetNearestCarDoor, 0x6528F0);
    RH_ScopedInstall(GetNearestCarPassengerDoor, 0x650BB0);
    RH_ScopedInstall(GetPositionToOpenCarDoor, 0x64E740);
    RH_ScopedInstall(IsCarDoorInUse, 0x64EC90);
    RH_ScopedInstall(IsCarDoorReady, 0x64ED90);
    RH_ScopedInstall(IsCarQuickJackPossible, 0x64EF00);
    RH_ScopedInstall(IsCarSlowJackRequired, 0x64EF70);
    RH_ScopedInstall(IsClearToDriveAway, 0x6509B0);
    RH_ScopedInstall(IsPathToDoorBlockedByVehicleCollisionModel, 0x651210);
    RH_ScopedInstall(IsPedHealthy, 0x64EEE0);
    RH_ScopedInstall(IsPlayerToQuitCarEnter, 0x64F240);
    RH_ScopedInstall(IsRoomForPedToLeaveCar, 0x6504C0);
    RH_ScopedInstall(IsVehicleHealthy, 0x64EEC0);
    RH_ScopedInstall(IsVehicleStealable, 0x6510D0);
    RH_ScopedInstall(MakeUndraggedDriverPedLeaveCar, 0x64F600);
    RH_ScopedInstall(MakeUndraggedPassengerPedsLeaveCar, 0x64F540);
    RH_ScopedInstall(QuitEnteringCar, 0x650130);
    RH_ScopedInstall(RemoveCarSitAnim, 0x64F680);
    RH_ScopedInstall(RemoveGetInAnims, 0x64F6E0);
    RH_ScopedInstall(SetAnimOffsetForEnterOrExitVehicle, 0x64F860);
    RH_ScopedInstall(SetPedInCarDirect, 0x650280);
}

// 0x64F720
void CCarEnterExit::AddInCarAnim(const CVehicle* vehicle, CPed* ped, bool bAsDriver) {
    const auto [grpId, animId] = [&]() -> std::pair<AssocGroupId, AnimationId> {
        if (bAsDriver) { // Inverted
            if (const auto data = const_cast<CVehicle*>(vehicle)->GetRideAnimData()) {
                return { data->AnimGroup, ANIM_ID_BIKE_RIDE };
            } else if (vehicle->IsBoat()) {
                if (vehicle->m_pHandlingData->m_bSitInBoat) {
                    return { ANIM_GROUP_DEFAULT, ANIM_ID_DRIVE_BOAT };
                }
            } else if (vehicle->vehicleFlags.bLowVehicle) {
                return { ANIM_GROUP_DEFAULT, ANIM_ID_CAR_LSIT };
            }

            return { ANIM_GROUP_DEFAULT, ANIM_ID_CAR_SIT };
        } else {
            if (const auto data = const_cast<CVehicle*>(vehicle)->GetRideAnimData()) {
                return { data->AnimGroup, ANIM_ID_BIKE_RIDE };
            } else if (vehicle->vehicleFlags.bLowVehicle) {
                return { ANIM_GROUP_DEFAULT, ANIM_ID_CAR_SITPLO };
            }

            return { ANIM_GROUP_DEFAULT, ANIM_ID_CAR_SITP };
        }
    }();
    CAnimManager::BlendAnimation(ped->GetRpClump(), grpId, animId, 1000.f);
    ped->StopNonPartialAnims();
}

// 0x64EE10
bool CCarEnterExit::CarHasDoorToClose(const CVehicle* vehicle, int32 doorId) {
    auto& veh = const_cast<CVehicle&>(*vehicle);
    return !veh.IsDoorMissingU32(doorId) && !veh.IsDoorClosedU32(doorId);
}

// 0x64EDD0
bool CCarEnterExit::CarHasDoorToOpen(const CVehicle* vehicle, int32 doorId) {
    auto& veh = const_cast<CVehicle&>(*vehicle);
    return !veh.IsDoorMissingU32((uint32)doorId) && !veh.IsDoorFullyOpenU32((uint32)doorId);
}

// 0x64EE50
bool CCarEnterExit::CarHasOpenableDoor(const CVehicle* vehicle, int32 doorId_UnusedArg, const CPed* ped) {
    return vehicle->CanPedOpenLocks(ped);
}

// 0x64EE70
bool CCarEnterExit::CarHasPartiallyOpenDoor(const CVehicle* vehicle, int32 doorId) {
    auto& veh = const_cast<CVehicle&>(*vehicle); // TODO: Fix
    return !veh.IsDoorMissingU32((uint32)doorId)
        && !veh.IsDoorFullyOpenU32((uint32)doorId)
        && !veh.IsDoorClosedU32((uint32)doorId);
}

// 0x64E550
int32 CCarEnterExit::ComputeDoorFlag(const CVehicle* vehicle, int32 doorId, bool bSettingFlags) {
    if (bSettingFlags && (vehicle->IsBike() || vehicle->m_pHandlingData->m_bTandemSeats)) {
        switch (doorId) {
        case 8:
        case 10:
        case 18: return 5;
        case 9:
        case 11: return 10;
        default: NOTSA_UNREACHABLE(); // Originally `return 0`
        }
    } else {
        switch (doorId) {
        case 8:  return 4;
        case 9:  return 8;
        case 10:
        case 18: return 1;
        case 11: return 2;
        default: NOTSA_UNREACHABLE(); // Originally `return 0`
        }
    }
}

// 0x64E610
int32 CCarEnterExit::ComputeOppositeDoorFlag(const CVehicle* vehicle, int32 doorId, bool bCheckVehicleType) {
    if (bCheckVehicleType && (vehicle->IsBike() || vehicle->m_pHandlingData->m_bTandemSeats)) {
        switch (doorId) {
        case 8:
        case 10:
        case 18: return 5;
        case 9:
        case 11: return 10;
        default: NOTSA_UNREACHABLE(); // Originally `return 0`
        }
    } else {
        switch (doorId) {
        case 8: return 1;
        case 9: return 2;
        case 10:
        case 18: return 4;
        case 11: return 8;
        default: NOTSA_UNREACHABLE(); // Originally `return 0`
        }
    }
}

// 0x64F1E0
int32 CCarEnterExit::ComputePassengerIndexFromCarDoor(const CVehicle* vehicle, int32 doorId) {
    if (vehicle->IsBike() || vehicle->m_pHandlingData->m_bTandemSeats) {
        switch (doorId) {
        case 9:
        case 11:
            return 0;
        default:
            return -1;
        }
    }

    switch (doorId) {
    case 8:
        return 0;
    case 9:
        return 2;
    case 11:
        return 1;
    default:
        return -1;
    }
}

// 0x64F070
CPed* CCarEnterExit::ComputeSlowJackedPed(const CVehicle* vehicle, int32 doorId) {
    if (vehicle->IsBike() || vehicle->m_pHandlingData->m_bTandemSeats) {
        switch (doorId) {
        case 8:
        case 10:
        case 18:
            return vehicle->m_pDriver;
        case 9:
        case 11:
            return vehicle->m_apPassengers[0];
        default:
            return nullptr;
        }
    }

    switch (doorId) {
    case 8:
        return vehicle->m_apPassengers[0];
    case 9:
        return vehicle->m_apPassengers[2];
    case 10:
        return vehicle->m_pDriver;
    case 11:
        return vehicle->m_apPassengers[1];
    default:
        return nullptr;
    }
}

// 0x64F190
int32 CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(const CVehicle* vehicle, int32 psgrIdx) {
    if (vehicle->vehicleFlags.bIsBus) {
        return 8;
    }

    switch (psgrIdx) {
    case 0:
        return (vehicle->IsBike() || vehicle->m_pHandlingData->m_bTandemSeats) ? 11 : 8; // Inverted condition
    case 1:
        return 11;
    case 2:
        return 9;
    default:
        return -1;
    }
}

// 0x64F110
int32 CCarEnterExit::ComputeTargetDoorToExit(const CVehicle* vehicle, const CPed* ped) {
    if (vehicle->m_pDriver == ped) {
        return 10;
    }

    // Theoritically the rest here is the same as `ComputeTargetDoorToEnterAsPassenger`
    // but I'm not quite sure, as in that function they just check `bIsBus`, while here they check the anim groups
    // So, using the below switch I make sure the theory is right.
    switch (vehicle->GetAnimGroupId()) {
    case ANIM_GROUP_COACHCARANIMS:
    case ANIM_GROUP_BUSCARANIMS:
        assert(vehicle->vehicleFlags.bIsBus);
    }

    if (const auto optIndex = vehicle->GetPassengerIndex(ped)) {
        return ComputeTargetDoorToEnterAsPassenger(vehicle, *optIndex);
    }

    return -1;
}

// 0x6528F0
bool CCarEnterExit::GetNearestCarDoor(const CPed* ped, const CVehicle* vehicle, CVector& outPos, int32& doorId) {
    auto driverDraggedOutOffset = vehicle->m_pDriver ? &ms_vecPedQuickDraggedOutCarAnimOffset : nullptr;
    auto psgrDraggedOutOffset   = vehicle->HasPassengerAtSeat(0) ? &ms_vecPedQuickDraggedOutCarAnimOffset : nullptr;

    if ((vehicle->IsBike() && !vehicle->IsSubBMX()) || vehicle->IsSubQuad()) {
        driverDraggedOutOffset = nullptr;
        psgrDraggedOutOffset = nullptr;

        if (ped->GetTaskManager().GetActiveTask()->GetTaskType() != TASK_COMPLEX_ENTER_CAR_AS_PASSENGER) {
            if (std::abs(vehicle->GetRight().z) < 0.1f) { // Isn't on it's side
                // Check if ped is 30 degrees to the left of the vehicle
                // Original code used atan and whatnot, but this achieves the same result
                if (DotProduct2D(vehicle->GetRight(), ped->GetForward()) > 0 // On the left
                 && DotProduct2D(vehicle->GetForward(), ped->GetForward()) > x87::cos(PI / 6.f)
                ) {
                    if ((ped->IsPlayer() && ped->GetPlayerData()->m_fMoveBlendRatio > 1.5f && doorId == 0) 
                    || (!ped->IsPlayer() && ped->m_nPedType != PED_TYPE_COP && ped->m_nMoveState == PEDMOVE_RUN && ped->m_pStats->m_nTemper > 65 && doorId == 0)
                    ) {
                        // 18 here is probably either from eBikeNodes or eQuadNodes, not sure?
                        if (IsRoomForPedToLeaveCar(vehicle, 18)) {
                            doorId = 18;
                            outPos = GetPositionToOpenCarDoor(vehicle, 18);
                            return true;
                        }
                    }
                }
            }
        }
    } else if (vehicle->vehicleFlags.bIsBus || vehicle->vehicleFlags.bLowVehicle) {
        driverDraggedOutOffset = nullptr;
        psgrDraggedOutOffset = nullptr;
    }


    const auto posDoorFLeft = GetPositionToOpenCarDoor(vehicle, CAR_DOOR_LF);
    const auto posDoorFRight = GetPositionToOpenCarDoor(vehicle, CAR_DOOR_RF);

    CVector2D pedPos2D = ped->GetPosition();
    CVector2D dir2DToDoorFLeft = posDoorFLeft - pedPos2D, dir2DToDoorFRight = posDoorFRight - pedPos2D;

    if (vehicle->m_pVehicleBeingTowed) {
        if (dir2DToDoorFLeft.SquaredMagnitude() < dir2DToDoorFRight.SquaredMagnitude()) {
            if (IsPathToDoorBlockedByVehicleCollisionModel(ped, vehicle, posDoorFRight)) {
                dir2DToDoorFRight = { 999.90002f, 999.90002f };
            } else if (IsPathToDoorBlockedByVehicleCollisionModel(ped, vehicle, posDoorFLeft)) {
                dir2DToDoorFLeft = { 999.90002f, 999.90002f };
            }
        }
    }

    if (vehicle->m_pHandlingData->m_bForceDoorCheck && vehicle->IsAutomobile()) {
        const auto aut = static_cast<const CAutomobile*>(vehicle);

        if (aut->m_aCarNodes[CAR_DOOR_RF]) { // Inverted
            if (!aut->m_aCarNodes[CAR_DOOR_LF]) {
                if (IsRoomForPedToLeaveCar(vehicle, CAR_DOOR_RF, driverDraggedOutOffset)) {
                    doorId = CAR_DOOR_RF;
                    outPos = posDoorFRight;
                    return true;
                }
            }
        } else {
            if (IsRoomForPedToLeaveCar(vehicle, CAR_DOOR_LF, driverDraggedOutOffset)) {
                doorId = CAR_DOOR_LF;
                outPos = posDoorFLeft;
                return true;
            }
        }

        return false;
    }

    if (doorId != CAR_NODE_NONE && IsRoomForPedToLeaveCar(vehicle, doorId, driverDraggedOutOffset)) {
        switch (doorId) {
        case CAR_DOOR_LF: {
            doorId = CAR_DOOR_LF;
            outPos = posDoorFLeft;
            return true;
        }
        case CAR_DOOR_RF: {
            doorId = CAR_DOOR_RF;
            outPos = posDoorFRight;
            return true;
        }
        default:
            return false;
        }
    }

    if (!vehicle->m_pDriver
    || (!CPedGroups::AreInSameGroup(ped, vehicle->m_pDriver) && !vehicle->m_pDriver->bDontDragMeOutCar)
    ) {
        if (vehicle->vehicleFlags.bIsBus
         || dir2DToDoorFRight.SquaredMagnitude() > dir2DToDoorFLeft.SquaredMagnitude()
        ) {
            if (IsRoomForPedToLeaveCar(vehicle, CAR_DOOR_LF, driverDraggedOutOffset)) {
                doorId = CAR_DOOR_LF;
                outPos = posDoorFLeft;
                return true;
            }
            if (IsRoomForPedToLeaveCar(vehicle, CAR_DOOR_RF, driverDraggedOutOffset)) {
                doorId = CAR_DOOR_RF;
                outPos = posDoorFRight;
                return true;
            }
        } else {
            if (IsRoomForPedToLeaveCar(vehicle, CAR_DOOR_RF, driverDraggedOutOffset)) {
                if ((
                        vehicle->HasPassengerAtSeat(0)
                        && !vehicle->IsBike()
                        && !vehicle->m_pHandlingData->m_bTandemSeats
                        && (CPedGroups::AreInSameGroup(vehicle->m_apPassengers[0], ped) || vehicle->m_apPassengers[0]->bDontDragMeOutCar || vehicle->IsMissionVehicle())
                        && IsRoomForPedToLeaveCar(vehicle, CAR_DOOR_LF, driverDraggedOutOffset)
                    ) || (
                        vehicle->m_nGettingInFlags & 4
                        && IsRoomForPedToLeaveCar(vehicle, CAR_DOOR_LF, driverDraggedOutOffset)
                    )
                ) {
                    doorId = CAR_DOOR_LF;
                    outPos = posDoorFLeft;
                    return true;
                } else {
                    doorId = CAR_DOOR_RF;
                    outPos = posDoorFRight;
                    return true;
                }
            }

            if (IsRoomForPedToLeaveCar(vehicle, CAR_DOOR_LF, driverDraggedOutOffset)) {
                doorId = CAR_DOOR_LF;
                outPos = posDoorFLeft;
                return true;
            }
        }
    }
    return false;
}

// 0x650BB0
bool CCarEnterExit::GetNearestCarPassengerDoor(const CPed* ped, const CVehicle* vehicle, CVector* outVec, int32* doorId, bool CheckIfOccupiedTandemSeat, bool CheckIfDoorIsEnterable, bool CheckIfRoomToGetIn) {
    // NOTSA: Original code left these uninitialized if not set (only matters if the function returns false)
    CVector posDoorRF{}, posDoorLR{}, posDoorRR{};
    float   dxRF = 999.f, dyRF = 999.f; // Door 8  (CAR_DOOR_RF) - delta to ped
    float   dxLR = 999.f, dyLR = 999.f; // Door 11 (CAR_DOOR_LR)
    float   dxRR = 999.f, dyRR = 999.f; // Door 9  (CAR_DOOR_RR)
    bool    found = false;

    const auto  animGroup = vehicle->m_pHandlingData->m_nAnimGroup;
    const CVector pedPos  = ped->GetPosition();

    // Busses (anim groups 15, 16) => Only the front door
    if (animGroup == 15 || animGroup == 16) {
        if (CheckIfDoorIsEnterable && (vehicle->m_nGettingInFlags & 4)) {
            return false;
        }
        if (CheckIfRoomToGetIn && !IsRoomForPedToLeaveCar(vehicle, 8, nullptr)) {
            return false;
        }
        *outVec = GetPositionToOpenCarDoor(vehicle, 8);
        *doorId = 8;
        return true;
    }

    if (vehicle->IsBike() || vehicle->m_pHandlingData->m_bTandemSeats) {
        if (!CheckIfOccupiedTandemSeat || !vehicle->m_apPassengers[0]) {
            if ((!CheckIfDoorIsEnterable || !(vehicle->m_nGettingInFlags & 2))
                && (!CheckIfRoomToGetIn || IsRoomForPedToLeaveCar(vehicle, 11, nullptr))
            ) {
                posDoorLR = GetPositionToOpenCarDoor(vehicle, 11);
                dxLR      = posDoorLR.x - pedPos.x;
                dyLR      = posDoorLR.y - pedPos.y;
                found     = true;
            }

            if (!((CheckIfDoorIsEnterable && (vehicle->m_nGettingInFlags & 8))
                || (CheckIfRoomToGetIn && !IsRoomForPedToLeaveCar(vehicle, 9, nullptr)))
            ) {
                posDoorRR = GetPositionToOpenCarDoor(vehicle, 9);
                dxRR      = posDoorRR.x - pedPos.x;
                dyRR      = posDoorRR.y - pedPos.y;
                found     = true;
            }
        }
    } else {
        if (!((CheckIfOccupiedTandemSeat && vehicle->m_apPassengers[0])
            || (CheckIfDoorIsEnterable && (vehicle->m_nGettingInFlags & 4))
            || (CheckIfRoomToGetIn && !IsRoomForPedToLeaveCar(vehicle, 8, nullptr)))
        ) {
            posDoorRF = GetPositionToOpenCarDoor(vehicle, 8);
            dxRF      = posDoorRF.x - pedPos.x;
            dyRF      = posDoorRF.y - pedPos.y;
            found     = true;
        }
    }

    // Rear doors of 4 door vehicles
    if ((int8)vehicle->GetVehicleModelInfo()->m_nNumDoors > 2) {
        const auto IsRearDoorUsable = [&](int32 node) { // Original code checked for the nodes' presence directly (if the handling says so)
            return !vehicle->m_pHandlingData->m_bForceDoorCheck
                || (vehicle->IsAutomobile() && static_cast<const CAutomobile*>(vehicle)->m_aCarNodes[node]);
        };

        if (IsRearDoorUsable(CAR_DOOR_LR)
            && (!CheckIfOccupiedTandemSeat || !vehicle->m_apPassengers[1])
            && (!CheckIfDoorIsEnterable || !(vehicle->m_nGettingInFlags & 2))
            && (!CheckIfRoomToGetIn || IsRoomForPedToLeaveCar(vehicle, 11, nullptr))
        ) {
            posDoorLR = GetPositionToOpenCarDoor(vehicle, 11);
            dxLR      = posDoorLR.x - pedPos.x;
            dyLR      = posDoorLR.y - pedPos.y;
            found     = true;
        }

        if (IsRearDoorUsable(CAR_DOOR_RR)
            && (!CheckIfOccupiedTandemSeat || !vehicle->m_apPassengers[2])
            && (!CheckIfDoorIsEnterable || !(vehicle->m_nGettingInFlags & 8))
            && (!CheckIfRoomToGetIn || IsRoomForPedToLeaveCar(vehicle, 9, nullptr))
        ) {
            posDoorRR = GetPositionToOpenCarDoor(vehicle, 9);
            dxRR      = posDoorRR.x - pedPos.x;
            dyRR      = posDoorRR.y - pedPos.y;
            found     = true;
        }
    }

    // Pick the closest
    *outVec = posDoorRF;
    *doorId = 8;
    if (dxLR * dxLR + dyLR * dyLR < dxRF * dxRF + dyRF * dyRF) {
        *doorId = 11;
        *outVec = posDoorLR;
        dxRF    = dxLR;
        dyRF    = dyLR;
    }
    if (dxRR * dxRR + dyRR * dyRR < dyRF * dyRF + dxRF * dxRF) {
        *doorId = 9;
        *outVec = posDoorRR;
    }
    return found;
}

// 0x64E740
// Originally RVO'd
CVector CCarEnterExit::GetPositionToOpenCarDoor(const CVehicle* vehicle, int32 doorId) {
    auto* const mi  = vehicle->GetVehicleModelInfo();
    auto* const hnd = vehicle->m_pHandlingData;
    auto&       veh = const_cast<CVehicle&>(*vehicle); // TODO: Fix constness
    const auto& mat = veh.GetMatrix();

    const auto GetSeatPos = [&](bool back) -> CVector {
        return back ? mi->GetBackSeatPosn() : mi->GetFrontSeatPosn();
    };

    // Bikes / tandem seat vehicles
    if (vehicle->IsBike() || hnd->m_bTandemSeats) {
        if (doorId == 18) {
            const auto doorOffset = vehicle->GetAnimGroup().ComputeAnimDoorOffsets(ENTER_BIKE_FRONT);
            const auto seat       = mi->GetFrontSeatPosn();
            return mat.TransformPoint({
                seat.x - doorOffset.x,
                seat.y + doorOffset.y,
                seat.z - doorOffset.z
            });
        }

        auto doorOffset = vehicle->GetAnimGroup().ComputeAnimDoorOffsets(ENTER_FRONT);
        auto seat       = GetSeatPos(doorId == 11 || doorId == 9);

        if (vehicle->IsBike()) {
            if (doorId == 9 || doorId == 8) {
                doorOffset.x = doorOffset.x * -1.f;
            }
            CVector out;
            static_cast<CBike&>(veh).GetCorrectedWorldDoorPosition(out, doorOffset, seat);
            return out;
        }

        // Tandem seats, but not a bike
        if (doorId == 9 || doorId == 8) {
            doorOffset.x = doorOffset.x * -1.f;
            seat.x       = seat.x + hnd->m_fSeatOffsetDistance;
        } else {
            seat.x       = seat.x - hnd->m_fSeatOffsetDistance;
        }
        return mat.TransformPoint(seat - doorOffset);
    }

    // NOTE: Original code did pass the seat offset as an anim door offset index for the "default" case, but then ignored the result (and it might have read out of bounds)
    float seatOffset = (hnd->m_nAnimGroup == 0x65 && (doorId == 11 || doorId == 9))
        ? 0.f
        : hnd->m_fSeatOffsetDistance;

    CVector doorOffset{};
    switch (doorId) {
    case 10:
    case 8:  doorOffset = vehicle->GetAnimGroup().ComputeAnimDoorOffsets(ENTER_FRONT); break;
    case 11:
    case 9:  doorOffset = vehicle->GetAnimGroup().ComputeAnimDoorOffsets(ENTER_REAR); break;
    case 18: vehicle->GetAnimGroup().ComputeAnimDoorOffsets(ENTER_BIKE_FRONT); break; // Result isn't used
    default: break;
    }

    CVector local{};
    switch (doorId) {
    case 8: { // RF
        const auto seat = mi->GetFrontSeatPosn();
        local = CVector{ (seat.x + seatOffset) - (-doorOffset.x), seat.y - doorOffset.y, seat.z - doorOffset.z };
        break;
    }
    case 9: { // RR
        const auto seat = mi->GetBackSeatPosn();
        local = CVector{ (seat.x + seatOffset) - (-doorOffset.x), seat.y - doorOffset.y, seat.z - doorOffset.z };
        break;
    }
    case 10: { // LF
        const auto seat = mi->GetFrontSeatPosn();
        local = CVector{ -(seat.x + seatOffset) - doorOffset.x, seat.y - doorOffset.y, seat.z - doorOffset.z };
        break;
    }
    case 11: { // LR
        const auto seat = mi->GetBackSeatPosn();
        local = CVector{ -(seat.x + seatOffset) - doorOffset.x, seat.y - doorOffset.y, seat.z - doorOffset.z };
        break;
    }
    default: {
        const auto seat = mi->GetFrontSeatPosn();
        local = seat; // doorOffset is zero here
        break;
    }
    }

    if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_MTRUCK
        || (hnd->m_bIsBig && vehicle->m_nVehicleSubType != VEHICLE_TYPE_PLANE)
    ) {
        local.z = 0.95f - veh.GetHeightAboveRoad();
    }

    return vehicle->GetPosition() + mat.TransformVector(local);
}

// 0x64EC90
bool CCarEnterExit::IsCarDoorInUse(const CVehicle* vehicle, int32 firstDoorId, int32 secondDoorId) {
    const auto CheckIsDoorInUse = [vehicle](int32 door) {
        const auto CheckInOutFlags = [vehicle](uint32 n) {
            const auto flag = 1 << n;
            return (flag & vehicle->m_nGettingInFlags) || (flag & vehicle->m_nGettingOutFlags);
        };
        switch (door) {
        case 8: return CheckInOutFlags(2);
        case 9: return CheckInOutFlags(3);
        case 10:
        case 18: return CheckInOutFlags(0);
        case 11: return CheckInOutFlags(1);
        default: return false;
        }
    };
    return CheckIsDoorInUse(firstDoorId) || CheckIsDoorInUse(secondDoorId);
}

// 0x64ED90
bool CCarEnterExit::IsCarDoorReady(const CVehicle* vehicle, int32 doorId) {
    // TODO: Make IsDoorReadyU32 a const member function to avoid const_cast
    auto& veh = const_cast<CVehicle&>(*vehicle); // TODO: Fix
    return veh.IsDoorReadyU32((uint32)doorId)
        || veh.IsDoorFullyOpenU32((uint32)doorId);
}

// 0x64EF00
bool CCarEnterExit::IsCarQuickJackPossible(CVehicle* vehicle, int32 doorId, const CPed* ped) {
    // I think doorId 10 is the driver's door
    //if (doorId == 10 && vehicle->IsAutomobile() && !vehicle->IsDoorMissingU32(doorId) && vehicle->IsDoorClosedU32(doorId)) {
    //    // This does *nothing* - I tried `return vehicle->CanPedOpenLocks(ped);` but that just breaks everything.
    //    // Basically, returning anything but `false` from here breaks the code (in `CTaskComplexEnterCar`)
    //    vehicle->CanPedOpenLocks(ped); 
    //}
    return false;
}

// 0x64EF70
bool CCarEnterExit::IsCarSlowJackRequired(const CVehicle* vehicle, int32 doorId) {
    if (vehicle->IsBike() || (vehicle->m_pHandlingData->m_bTandemSeats)) {
        switch (doorId) {
        case 8:
        case 10:
        case 18:
            return vehicle->HasDriver();
        case 9:
        case 11:
            return vehicle->HasPassengerAtSeat(0);
        default:
            return false;
        }
    }

    int group = vehicle->GetAnimGroupId();
    if (group == ANIM_GROUP_COACHCARANIMS || group == ANIM_GROUP_BUSCARANIMS) {
        switch (doorId) {
        case 8:
            return false;
        case 10:
            return vehicle->HasDriver();
        }
    } else {
        switch (doorId) {
        case 8:
            return vehicle->HasPassengerAtSeat(0);
        case 9:
            return vehicle->HasPassengerAtSeat(2);
        case 10:
            return vehicle->HasDriver();
        case 11:
            return vehicle->HasPassengerAtSeat(1);
        default:
            return false;
        }
    }
    return false;
}

// 0x6509B0
bool CCarEnterExit::IsClearToDriveAway(const CVehicle* vehicle) {
    const auto& vehPos = vehicle->GetPosition();
    const auto  bbSizeY = vehicle->GetColModel()->GetBoundingBox().GetSize().y;
    CEntity* hitEntity{};
    CColPoint hitCP{};
    return !CWorld::ProcessLineOfSight(vehPos + vehicle->GetForward() * bbSizeY, vehPos, hitCP, hitEntity, true, true, false, false, false, true, true, false) || hitEntity == vehicle;
}

// 0x651210
bool CCarEnterExit::IsPathToDoorBlockedByVehicleCollisionModel(const CPed* ped, const CVehicle* vehicle, const CVector& pos) {
    if (vehicle->GetModelIndex() == eModelID::MODEL_AT400) {
        return false;
    }

    const auto vehMatInv = Invert(*vehicle->m_matrix);
    const CColLine line{
        vehMatInv.TransformPoint(ped->GetPosition()),
        vehMatInv.TransformPoint(pos)
    };

    for (const auto& sp : vehicle->GetColModel()->GetData()->GetSpheres()) {
        if (CCollision::TestLineSphere(line, sp)) {
            return false;
        }
    }

    return true;
}

// 0x64EEE0
bool CCarEnterExit::IsPedHealthy(CPed* ped) {
    return ped->m_fHealth > 0.f;
}

// 0x64F240
bool CCarEnterExit::IsPlayerToQuitCarEnter(const CPed* ped, const CVehicle* vehicle, int32 startTime, CTask* task) {
    constexpr float kPi = std::numbers::pi_v<float>, kTwoPi = 2.f * std::numbers::pi_v<float>, kHalfPi = std::numbers::pi_v<float> / 2.f;

    const auto* const pad = static_cast<const CPlayerPed*>(ped)->GetPadFromPlayer();

    float doorAngle      = ped->m_fCurrentRotation;
    bool  checkMeleeKey = false;

    if (task) {
        bool computeAngle = false;
        switch (task->GetTaskType()) {
        case TASK_SIMPLE_CAR_ALIGN:
        case TASK_SIMPLE_STAND_STILL:
        case TASK_COMPLEX_FALL_AND_GET_UP:
        case TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_INSIDE:
        case TASK_SIMPLE_CAR_GET_IN:
        case TASK_SIMPLE_CAR_SHUFFLE:
        case TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER:
        case TASK_SIMPLE_CAR_SET_PED_OUT:
        case TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR:
            computeAngle = true;
            break;
        case TASK_COMPLEX_LEAVE_CAR:
        case TASK_SIMPLE_CAR_OPEN_DOOR_FROM_OUTSIDE:
        case TASK_SIMPLE_CAR_OPEN_LOCKED_DOOR_FROM_OUTSIDE:
        case TASK_SIMPLE_BIKE_PICK_UP:
        case TASK_SIMPLE_CAR_QUICK_DRAG_PED_OUT:
        case TASK_SIMPLE_CAR_SLOW_DRAG_PED_OUT:
            computeAngle  = true;
            checkMeleeKey = true;
            break;
        default:
            break;
        }

        if (computeAngle) {
            const auto& mat      = const_cast<CVehicle*>(vehicle)->GetMatrix();
            const auto  toPed    = ped->GetPosition() - vehicle->GetPosition();
            const auto  heading  = vehicle->GetHeading();
            const float sideDot  = (toPed.z * mat.GetRight().z + toPed.y * mat.GetRight().y) + toPed.x * mat.GetRight().x;

            doorAngle = sideDot > 0.f
                ? heading + kHalfPi
                : heading - kHalfPi;

            if (mat.GetUp().z < 0.f) { // Upside down
                doorAngle += kPi;
                if (doorAngle > kPi) {
                    doorAngle -= kTwoPi;
                }
            }
            if (doorAngle > kPi) {
                doorAngle -= kTwoPi;
            } else if (doorAngle < -kPi) {
                doorAngle += kTwoPi;
            }
        }
    }

    if (vehicle->m_pFire) {
        return true;
    }

    if (pad->DisablePlayerControls) {
        return false;
    }

    if (checkMeleeKey) {
        if (pad->MeleeAttackJustDown(false)) {
            return true;
        }
    } else {
        const auto timeDiff = CTimer::GetTimeInMS() - (uint32)startTime;
        float      timeDiffF = (float)(int32)timeDiff;
        if ((int32)timeDiff < 0) {
            timeDiffF += 4294967296.f;
        }
        if (timeDiffF <= 500.f) {
            return false;
        }
    }

    const float walkUpDown    = (float)pad->GetPedWalkUpDown();
    const float walkLeftRight = (float)pad->GetPedWalkLeftRight();

    float stickAngle = CGeneral::GetRadianAngleBetweenPoints(0.f, 0.f, -walkLeftRight, walkUpDown) - TheCamera.m_fOrientation;
    const float stickMag = std::sqrt(walkUpDown * walkUpDown + walkLeftRight * walkLeftRight) * 0.0078125f;

    // Make sure the angle is in the range of [doorAngle - kPi, doorAngle + kPi]
    if (stickAngle > doorAngle + kPi) {
        stickAngle -= kTwoPi;
    } else if (stickAngle < doorAngle - kPi) {
        stickAngle += kTwoPi;
    }

    return stickMag > 0.75f && std::abs(stickAngle - doorAngle) > kPi / 4.f;
}

// 0x6504C0
bool CCarEnterExit::IsRoomForPedToLeaveCar(const CVehicle* vehicle, int32 doorId, const CVector* pos) {
    auto* const mi  = vehicle->GetVehicleModelInfo();
    auto&       veh = const_cast<CVehicle&>(*vehicle); // TODO: Fix constness
    const auto& mat = veh.GetMatrix();

    // Position of the seat (in the vehicle's space)
    CVector seat;
    if (vehicle->IsBike() || vehicle->m_pHandlingData->m_bTandemSeats) {
        seat = (doorId == 11 || doorId == 9) ? mi->GetBackSeatPosn() : mi->GetFrontSeatPosn();
        if (doorId == 10 || doorId == 11) {
            seat.x = -seat.x;
        }
    } else {
        switch (doorId) {
        case 8:  seat = mi->GetFrontSeatPosn(); break;
        case 9:  seat = mi->GetBackSeatPosn(); break;
        case 10: seat = mi->GetFrontSeatPosn(); seat.x = -seat.x; break;
        case 11: seat = mi->GetBackSeatPosn();  seat.x = -seat.x; break;
        default: return false;
        }
    }

    const auto start = mat.TransformPoint(seat); // Start of the line (world space)
    auto       door  = GetPositionToOpenCarDoor(vehicle, doorId);

    if (pos) {
        auto offset = *pos;
        if (doorId == 8 || doorId == 9) {
            offset.x = -offset.x;
        }
        door += mat.TransformVector(offset);
    }

    CVector startAdj = start;
    if (mat.GetUp().z < 0.f) { // Upside down
        startAdj.z += 0.5f;
        door.z     += 0.5f;
    }

    const float dx = door.x - startAdj.x;
    const float dy = door.y - startAdj.y;

    CVector end;
    if (vehicle->IsBike()) {
        end.x  = door.x;
        end.y  = door.y;
        end.z  = door.z + 0.2f;
        door.z = end.z + 0.35f;
    } else {
        const float len = std::sqrt(dx * dx + dy * dy);
        const float k   = (0.35f + len) / len;
        end.x = dx * k + startAdj.x;
        end.y = dy * k + startAdj.y;
        end.z = door.z;
    }

    if (!CWorld::GetIsLineOfSightClear(startAdj, end, true, false, false, true, false, false, false)) {
        return false;
    }

    const bool testBuildings = vehicle->m_nVehicleType != VEHICLE_TYPE_TRAIN;
    const auto* const hitEntity = CWorld::TestSphereAgainstWorld(door, 0.35f, &veh, testBuildings, true, false, true, false, false);
    if (hitEntity
        && !(hitEntity->GetModelIndex() == MODEL_TUGSTAIR && vehicle->GetModelIndex() == MODEL_AT400)
        && hitEntity != vehicle->m_pAttachedTo
    ) {
        return false;
    }

    CColPoint  colPoint{};
    CEntity*   outEntity{};
    const bool hitGround = CWorld::ProcessVerticalLine(door, 1000.f, colPoint, outEntity, true, false, false, true, false, false, nullptr);
    const float groundZ  = colPoint.m_vecPoint.z;
    if (hitGround && door.z < groundZ && door.z + 0.6f > groundZ) {
        return false;
    }

    float groundZBelow;
    if (vehicle->IsBoat()
        || notsa::contains({ (int32)MODEL_SKIMMER, (int32)MODEL_VORTEX, (int32)MODEL_SEASPAR, (int32)MODEL_LEVIATHN }, (int32)vehicle->GetModelIndex())
    ) {
        groundZBelow = groundZ - 1.f;
    } else {
        if (!CWorld::ProcessVerticalLine(door, -1000.f, colPoint, outEntity, true, false, false, true, false, false, nullptr)) {
            return false;
        }
        groundZBelow = colPoint.m_vecPoint.z;
    }

    return groundZ == 0.f || !(groundZ < groundZBelow);
}

// 0x64EEC0
bool CCarEnterExit::IsVehicleHealthy(const CVehicle* vehicle) {
    return vehicle->GetStatus() != STATUS_WRECKED;
}

// 0x6510D0
bool CCarEnterExit::IsVehicleStealable(const CVehicle* vehicle, const CPed* ped) {
    switch (vehicle->m_nVehicleSubType) {
    case VEHICLE_TYPE_PLANE:
    case VEHICLE_TYPE_HELI:
        return false;
    }

    switch (vehicle->m_nVehicleType) {
    case VEHICLE_TYPE_AUTOMOBILE:
    case VEHICLE_TYPE_BIKE:
        break;
    default:
        return false;
    }

    if (ped->m_pVehicle != vehicle) {
        switch (vehicle->GetCreatedBy()) {
        case RANDOM_VEHICLE:
        case PARKED_VEHICLE:
            break;
        default:
            return false;
        }
    }

    if (CUpsideDownCarCheck{}.IsCarUpsideDown(vehicle)) {
        return false;
    }

    if (!vehicle->CanBeDriven()) {
        return false;
    }

    if (vehicle->IsLawEnforcementVehicle()) {
        return false;
    }

    if (const auto drvr = vehicle->m_pDriver) {
        if (   drvr->IsCreatedByMission()
            || drvr->IsPlayer()
            || ped->GetIntelligence()->IsFriendlyWith(*drvr)
            || CPedGroups::AreInSameGroup(ped, drvr)
        ) {
            return false;
        }
    }

    if (const auto grp = ped->GetGroup()) {
        if (grp->IsAnyoneUsingCar(vehicle)) {
            return false;
        }
    }

    if (vehicle->m_pFire) {
        return false;
    }

    if (vehicle->m_fHealth <= 600.f) {
        return false;
    }

    if (vehicle->IsUpsideDown() || vehicle->IsOnItsSide()) {
        return false;
    }

    if (!IsClearToDriveAway(vehicle)) {
        return false;
    }

    return true;
}

// 0x64F600
void CCarEnterExit::MakeUndraggedDriverPedLeaveCar(const CVehicle* vehicle, const CPed* pedGettingIn) {
    auto& veh = const_cast<CVehicle&>(*vehicle); // TODO: Fix
    auto& ped = const_cast<CPed&>(*pedGettingIn); // TODO: Fix
    veh.m_pDriver->GetEventGroup().Add(CEventDraggedOutCar{ &veh, &ped, true });
}

// 0x64F540
void CCarEnterExit::MakeUndraggedPassengerPedsLeaveCar(const CVehicle* targetVehicle, const CPed* draggedPed, const CPed* ped) {
    for (uint8 i = 0; i < targetVehicle->m_nMaxPassengers; i++) {
        CPed* const passenger = targetVehicle->m_apPassengers[i];
        if (!passenger || passenger == draggedPed || passenger->bStayInCarOnJack) {
            continue;
        }

        CEventPedEnteredMyVehicle event{
            const_cast<CPed*>(ped),
            const_cast<CVehicle*>(targetVehicle),
            (eTargetDoor)ComputeTargetDoorToExit(targetVehicle, passenger)
        };
        passenger->GetEventGroup().Add(&event, false);
    }
}

// unused
// 0x650130
void CCarEnterExit::QuitEnteringCar(CPed* ped, CVehicle* vehicle, int32 doorId, bool bCarWasBeingJacked) {
    RemoveGetInAnims(ped);
    ped->RestartNonPartialAnims();
    if (!RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_IDLE)) {
        CAnimManager::BlendAnimation(ped->GetRpClump(), ped->m_nAnimGroup, ANIM_ID_IDLE, 1000.0f);
    }

    if (bCarWasBeingJacked) {
        vehicle->vehicleFlags.bIsBeingCarJacked = true;
    }
    vehicle->m_nNumGettingIn--;

    if (vehicle->IsBike() || vehicle->m_pHandlingData->m_bTandemSeats) {
        switch (doorId) {
        case 8:
        case 10:
            vehicle->SetGettingInFlags(5);
            break;
        case 9:
        case 11:
            vehicle->SetGettingInFlags(10);
            break;
        }
        vehicle->vehicleFlags.bIsBig = false;
    } else {
        switch (doorId) {
        case 8:
            vehicle->SetGettingInFlags(4);
            break;
        case 9:
            vehicle->SetGettingInFlags(8);
            break;
        case 10:
            vehicle->SetGettingInFlags(vehicle->m_nMaxPassengers ? 1 : 3);
            break;
        case 11:
            vehicle->SetGettingInFlags(vehicle->m_nMaxPassengers ? 2 : 3);
            break;
        }
    }
    ped->SetUsesCollision(false);
}

// 0x64F680
void CCarEnterExit::RemoveCarSitAnim(const CPed* ped) {
    for (auto anim = RpAnimBlendClumpGetFirstAssociation(ped->GetRpClump(), ANIMATION_SECONDARY_TASK_ANIM); anim; anim = RpAnimBlendGetNextAssociation(anim, ANIMATION_SECONDARY_TASK_ANIM)) {
        anim->SetFlag(ANIMATION_IS_BLEND_AUTO_REMOVE);
        anim->m_BlendDelta = -1000.f;
    }
    CAnimManager::BlendAnimation(ped->GetRpClump(), ped->m_nAnimGroup, ANIM_ID_IDLE, 1000.0);
}

// 0x64F6E0
void CCarEnterExit::RemoveGetInAnims(const CPed* ped) {
    for (auto anim = RpAnimBlendClumpGetFirstAssociation(ped->GetRpClump(), ANIMATION_IS_PARTIAL); anim; anim = RpAnimBlendGetNextAssociation(anim, ANIMATION_IS_PARTIAL)) {
        anim->SetFlag(ANIMATION_IS_BLEND_AUTO_REMOVE);
        anim->m_BlendDelta = -1000.f;
    }
}

// 0x64F860
void CCarEnterExit::SetAnimOffsetForEnterOrExitVehicle() {
    if (ms_bPedOffsetsCalculated) {
        return;
    }

    const auto animBlockIdxs = {
        CAnimManager::GetAnimationBlockIndex("int_house"),
        CAnimManager::GetAnimationBlockIndex("int_office")
    };

    for (const auto idx : animBlockIdxs) {
        CStreaming::RequestModel(IFPToModelId(idx), STREAMING_KEEP_IN_MEMORY);
    }
    CStreaming::LoadAllRequestedModels(false);

    for (const auto idx : animBlockIdxs) {
        CAnimManager::AddAnimBlockRef(idx);
    }

    {
        const auto anim = CAnimManager::GetAnimAssociation(ANIM_GROUP_DEFAULT, ANIM_ID_GETUP_0);
        CAnimManager::UncompressAnimation(anim->m_BlendHier);
        const auto& seq = anim->m_BlendHier->GetSequences()[0];
        ms_vecPedGetUpAnimOffset = seq.m_FramesNum ? seq.GetUKeyFrame(0)->Trans : CVector{};
    }

    ms_vecPedQuickDraggedOutCarAnimOffset = CVector{ -1.841797f, -0.3261719f, -0.01269531f };

    const std::tuple<AssocGroupId, AnimationId, CVector*> toProcess[]{
        {ANIM_GROUP_INT_HOUSE,  ANIM_ID_BED_IN_L,   &ms_vecPedBedLAnimOffset  },
        {ANIM_GROUP_INT_HOUSE,  ANIM_ID_BED_IN_R,   &ms_vecPedBedRAnimOffset  },
        {ANIM_GROUP_INT_OFFICE, ANIM_ID_OFF_SIT_IN, &ms_vecPedDeskAnimOffset  },
        {ANIM_GROUP_INT_HOUSE,  ANIM_ID_LOU_IN,     &ms_vecPedChairAnimOffset },
    };
    for (const auto [grpId, animId, out] : toProcess) {
        // Calculate translation delta between first and last sequence frames
        *out = [grpId, animId] {
            const auto anim = CAnimManager::GetAnimAssociation(grpId, animId);
            CAnimManager::UncompressAnimation(anim->m_BlendHier);
            const auto& seq = anim->m_BlendHier->GetSequences()[0];
            if (seq.m_FramesNum > 0) {
                return seq.GetUKeyFrame(seq.m_FramesNum - 1)->Trans - seq.GetUKeyFrame(0)->Trans;
            }
            return CVector{};
        }();
    }

    for (const auto idx : animBlockIdxs) {
        CAnimManager::RemoveAnimBlockRef(idx);
    }

    ms_bPedOffsetsCalculated = true;
}

// 0x650280
bool CCarEnterExit::SetPedInCarDirect(CPed* ped, CVehicle* vehicle, int32 doorId, bool bAsDriver) {
    if (bAsDriver) {
        // Warp ped into vehicle
        CTaskSimpleCarSetPedInAsDriver task{ vehicle };
        task.m_bWarpingInToCar = true;
        task.ProcessPed(ped);

        // And make them drive
        ped->GetTaskManager().SetTask(new CTaskComplexCarDriveWander{ vehicle, vehicle->m_autoPilot.m_nCarDrivingStyle, (float)vehicle->m_autoPilot.m_nCruiseSpeed }, TASK_PRIMARY_PRIMARY);

        return true;
    }

    // Warp ped into vehicle
    {
        CTaskSimpleCarSetPedInAsPassenger task{ vehicle, (eTargetDoor)doorId };
        task.m_bWarpingInToCar = true;
        task.ProcessPed(ped);
    }

    if (vehicle->IsBike()) {
        ped->GetTaskManager().SetTask(new CTaskComplexCarDrive{ vehicle, false }, TASK_PRIMARY_PRIMARY);
    }

    // Set mutal acquaintance respect between the ped and all other occupants up to the ped's seat
    // I assume the function is only ever called with `bAsDriver` if there are no passengers
    // So that's why this code-path is only reachable if `bAsDriver` is false

    const auto SetMutalAcquaintanceWith = [ped](CPed* other) {
        if (other) {
            const auto SetWith = [](CPed* of, CPed* with) {
                if (!of->IsCreatedByMission()) {
                    of->GetAcquaintance().SetAsAcquaintance(ACQUAINTANCE_RESPECT, CPedType::GetPedFlag(with->m_nPedType));
                }
            };
            SetWith(ped, other);
            SetWith(other, ped);
        }
    };

    SetMutalAcquaintanceWith(vehicle->m_pDriver);

    const auto psgrIdx = ComputePassengerIndexFromCarDoor(vehicle, doorId);
    assert(psgrIdx != -1); // I really doubt this can happen, if it does, an `if` has to be added
    rng::for_each(vehicle->GetPassengers() | rng::views::take((size_t)psgrIdx), SetMutalAcquaintanceWith); // Set with all other passengers up to the ped's seat

    return true;
}
