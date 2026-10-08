#include "StdInc.h"

#include "TaskUtilityLineUpPedWithCar.h"
#include "Bike.h"
#include "VehicleAnimGroupData.h"
#include "Animation/AnimBlendAssociation.h"

void CTaskUtilityLineUpPedWithCar::InjectHooks() {
    RH_ScopedClass(CTaskUtilityLineUpPedWithCar);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x64FBB0);
    RH_ScopedInstall(Destructor, 0x64FC00);

    RH_ScopedInstall(GetLocalPositionToOpenCarDoor, 0x64FC10);
    RH_ScopedInstall(GetPositionToOpenCarDoor, 0x650A80);
    RH_ScopedInstall(ProcessPed, 0x6513A0, { .Reversed = false });
}

// 0x64FBB0
CTaskUtilityLineUpPedWithCar::CTaskUtilityLineUpPedWithCar(const CVector& offset, int32 time, int32 doorOpenPosType, int32 doorIdx) {
    m_Offset = offset;
    m_fDoorOpenPosZ = -999.99f;
    m_fTime = time;
    m_nDoorOpenPosType = doorOpenPosType;
    m_nDoorIdx = doorIdx;
}

namespace {
// These replicate the operation order of the original (inlined `CMatrix::Multiply3x3`),
// as the ones in `CMatrix` (`TransformVector` etc.) do the additions in a different order.

// 0x59C790
CVector TransformVectorOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return CVector{
        (u.x * v.z + f.x * v.y) + r.x * v.x,
        (u.y * v.z + r.y * v.x) + f.y * v.y,
        (u.z * v.z + r.z * v.x) + f.z * v.y
    };
}

// 0x59C810
CVector InverseTransformVectorOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return CVector{
        (r.y * v.y + r.z * v.z) + v.x * r.x,
        (f.y * v.y + f.x * v.x) + f.z * v.z,
        (u.y * v.y + u.x * v.x) + u.z * v.z
    };
}

//! The door offset (type) to use for the given animation
eVehAnimDoorOffset GetDoorOffsetTypeForAnim(AnimationId animId) {
    if (animId == ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_0 || animId == ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_1) { // 0x6C, 0x6D
        return EXIT_FRONT;
    }
    if (animId >= ANIM_ID_CAR_ALIGN_LHS && animId <= ANIM_ID_CAR_SHUFFLE_RHS_1) { // 0x15F to 0x174
        return ENTER_FRONT;
    }
    if (animId >= ANIM_ID_CAR_GETOUT_LHS_0 && animId <= ANIM_ID_CAR_GETOUT_RHS_1) { // 0x175 to 0x178
        return EXIT_FRONT;
    }
    switch (animId) {
    case ANIM_ID_CAR_JACKEDLHS:
    case ANIM_ID_CAR_JACKEDRHS:
        return JACK_PED_LEFT;
    case ANIM_ID_CAR_CLOSE_LHS_0:
    case ANIM_ID_CAR_CLOSE_RHS_0:
    case ANIM_ID_CAR_CLOSE_LHS_1:
    case ANIM_ID_CAR_CLOSE_RHS_1:
    case ANIM_ID_CAR_FALLOUT_LHS:
    case ANIM_ID_CAR_FALLOUT_RHS:
        return EXIT_FRONT;
    case ANIM_ID_CAR_DOORLOCKED_LHS:
    case ANIM_ID_CAR_DOORLOCKED_RHS:
        return ENTER_FRONT;
    default:
        // BUG: The original doesn't set the type here (0x64FCA2), and uses the `CVehicle*` as the door offset type instead!
        return ENTER_FRONT;
    }
}
}

// The following 2 functions have copy elision on the returned CVector, that is, the compiled functions
// take a vector ptr as their first (stack) arg, which is what our code should compile to as well.

// 0x64FC10
CVector CTaskUtilityLineUpPedWithCar::GetLocalPositionToOpenCarDoor(CVehicle* vehicle, float animProgress, CAnimBlendAssociation* assoc) {
    auto* const mi = vehicle->GetVehicleModelInfo();

    // How much to shift the seat by (towards the centre of the vehicle)
    const float seatShift = vehicle->vehicleFlags.bIsVan && (m_nDoorIdx == 11 || m_nDoorIdx == 9) // Rear doors of vans
        ? 0.f
        : animProgress * vehicle->m_pHandlingData->m_fSeatOffsetDistance;

    // Figure out which door offset to use (based on the anim, and the door)
    auto doorOffsetType = assoc ? GetDoorOffsetTypeForAnim(assoc->GetAnimId()) : ENTER_FRONT;
    if (doorOffsetType == ENTER_FRONT || doorOffsetType == EXIT_FRONT) {
        switch (m_nDoorIdx) {
        case 10:
        case 8:  doorOffsetType = ENTER_FRONT;      break;
        case 11:
        case 9:  doorOffsetType = ENTER_REAR;       break;
        case 18: doorOffsetType = ENTER_BIKE_FRONT; break;
        default: break;
        }
    } else if (doorOffsetType == JACK_PED_LEFT) {
        doorOffsetType = ENTER_FRONT;
    }

    // Compute the door offset (Which is a blend of the vehicle's, and the std. car's if the door is locked)
    CVector off;
    if (assoc
        && (assoc->GetAnimId() == ANIM_ID_CAR_DOORLOCKED_LHS || assoc->GetAnimId() == ANIM_ID_CAR_DOORLOCKED_RHS)
        && vehicle->GetAnimGroup().m_specialFlags.bRunSpecialLockedDoor
    ) {
        const auto from = vehicle->GetAnimGroup().ComputeAnimDoorOffsets(doorOffsetType);
        const auto invBlend = 1.f - assoc->m_BlendAmount;
        const CVector fromScaled{ from.x * invBlend, from.y * invBlend, from.z * invBlend };

        // NOTE: This is the first anim group, not the vehicle's!
        const auto to = CVehicleAnimGroupData::GetVehicleAnimGroup(0).ComputeAnimDoorOffsets(doorOffsetType);
        const auto blend = assoc->m_BlendAmount;
        off = CVector{
            to.x * blend + fromScaled.x,
            to.y * blend + fromScaled.y,
            to.z * blend + fromScaled.z
        };
    } else {
        off = vehicle->GetAnimGroup().ComputeAnimDoorOffsets(doorOffsetType);
    }

    // Now compute the position (in local space of the vehicle)
    CVector seat; // The seat position (shifted)
    CVector res;
    switch (m_nDoorIdx) {
    case 8:   // Front right
    case 9: { // Rear right
        const auto& s = m_nDoorIdx == 8 ? mi->GetFrontSeatPosn() : mi->GetBackSeatPosn();
        seat   = CVector{ s.x + seatShift, s.y, s.z };
        off.x  = -off.x;
        res    = CVector{ seat.x - off.x, seat.y - off.y, seat.z - off.z };
        break;
    }
    case 10:   // Front left
    case 11: { // Rear left
        const auto& s = m_nDoorIdx == 10 ? mi->GetFrontSeatPosn() : mi->GetBackSeatPosn();
        seat   = CVector{ -(s.x + seatShift), s.y, s.z };
        res    = CVector{ seat.x - off.x, seat.y - off.y, seat.z - off.z };
        break;
    }
    case 18: { // Bike front
        const auto& s = mi->GetFrontSeatPosn();
        seat = s;
        res  = CVector{ off.x + s.x, s.y + off.y, s.z + off.z };
        break;
    }
    default: {
        seat = mi->GetFrontSeatPosn();
        off  = CVector{};
        res  = seat;
        break;
    }
    }

    // Bikes have special handling
    if (vehicle->IsBike() && m_nDoorIdx != 18) {
        CVector corrected;
        static_cast<CBike*>(vehicle)->GetCorrectedWorldDoorPosition(corrected, off, seat);

        const auto& vehPos = vehicle->GetPosition();
        res = InverseTransformVectorOriginal(
            vehicle->GetMatrix(),
            CVector{ corrected.x - vehPos.x, corrected.y - vehPos.y, corrected.z - vehPos.z }
        );
    }
    return res;
}

// 0x650A80
CVector CTaskUtilityLineUpPedWithCar::GetPositionToOpenCarDoor(CVehicle* vehicle, float animProgress, CAnimBlendAssociation* assoc) {
    const auto local = GetLocalPositionToOpenCarDoor(vehicle, animProgress, assoc);
    const auto rot   = TransformVectorOriginal(vehicle->GetMatrix(), local);
    const auto& pos  = vehicle->GetPosition();
    return CVector{ pos.x + rot.x, pos.y + rot.y, pos.z + rot.z };
}

// 0x6513A0
bool CTaskUtilityLineUpPedWithCar::ProcessPed(CPed* ped, CVehicle* vehicle, CAnimBlendAssociation* assoc) {
    return plugin::CallMethodAndReturn<bool, 0x6513A0, CTaskUtilityLineUpPedWithCar*, CPed*, CVehicle*, CAnimBlendAssociation*>(this, ped, vehicle, assoc);
}
