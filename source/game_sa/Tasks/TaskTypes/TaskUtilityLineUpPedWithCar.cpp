#include "StdInc.h"

#include <numbers>

#include "TaskUtilityLineUpPedWithCar.h"
#include "Bike.h"
#include "VehicleAnimGroupData.h"
#include "Animation/AnimBlendAssociation.h"
#include "PedPlacement.h"
#include "General.h"
#include "Timer.h"
#include "rtquat.h"
#include "rtslerp.h"

void CTaskUtilityLineUpPedWithCar::InjectHooks() {
    RH_ScopedClass(CTaskUtilityLineUpPedWithCar);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x64FBB0);
    RH_ScopedInstall(Destructor, 0x64FC00);

    RH_ScopedInstall(GetLocalPositionToOpenCarDoor, 0x64FC10);
    RH_ScopedInstall(GetPositionToOpenCarDoor, 0x650A80);
    RH_ScopedInstall(ProcessPed, 0x6513A0);
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

namespace {
constexpr float PI_F     = std::numbers::pi_v<float>; // 0x858CB8
constexpr float TWO_PI_F = 2.f * PI_F;                // 0x858CBC

//! The 2 blend values used by `ProcessPed`
struct LineUpBlends {
    float animProgress{}; //!< Progress used to calculate the door position
    float zBlend{};       //!< How much to blend the Z coordinate towards the door's
};

//! Get the blends based on the animation that is being played (This is the first big switch in `ProcessPed`)
LineUpBlends GetLineUpBlends(CVehicle* vehicle, CAnimBlendAssociation* assoc) {
    const auto animId = assoc->GetAnimId();
    auto&      group  = vehicle->GetAnimGroup();

    const auto progress = [&] { return assoc->GetTimeProgress(); };
    // Note: The original calls `CVehicleAnimGroup::GetGroup` first (0x64E4B0), but doesn't use the result
    const auto critTime = [&] { return group.ComputeCriticalBlendTime(animId); };

    // Used by the anims that open the door (when entering): ramps up around the critical time
    const auto RampUp = [&](double cmp, float edge, float t) -> float {
        if (cmp <= 0.0) {
            return t < edge ? 0.f : (float)(((double)t - edge) / (1.0 - edge));
        }
        return t < edge ? t / edge : 1.f;
    };
    // Used by the anims that close the door (when exiting): ramps down around the critical time
    const auto RampDown = [&](float crit, float edge, float t) -> float {
        if (crit <= 0.f) {
            return t < edge ? 1.f : (float)(1.0 - ((double)t - edge) / (1.0 - edge));
        }
        return t < edge ? (float)(1.0 - (double)t / edge) : 0.f;
    };
    // Hack for the AT-400, it never uses the blend...
    const auto IsAT400 = [&] { return vehicle->GetModelIndex() == MODEL_AT400; }; // 0x241

    switch (animId) {
    case ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_0: // 0x6C
    case ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_1: // 0x6D
        return { .animProgress = progress(), .zBlend = 0.f };

    case ANIM_ID_CAR_ALIGN_LHS:    // 0x15F
    case ANIM_ID_CAR_ALIGN_RHS:
    case ANIM_ID_CAR_ALIGNHI_LHS:
    case ANIM_ID_CAR_ALIGNHI_RHS: { // 0x162
        const auto crit    = critTime();
        const auto critAbs = std::abs(crit);
        float      zBlend  = 0.f;
        if (critAbs >= 10.f) {
            const auto edge = critAbs - 11.f;
            zBlend = RampUp((double)crit - 11.0, edge, progress());
        }
        if (IsAT400()) {
            zBlend = 1.f;
        }
        return { .animProgress = 1.f, .zBlend = zBlend };
    }

    case ANIM_ID_CAR_OPEN_LHS:    // 0x163
    case ANIM_ID_CAR_OPEN_RHS:
    case ANIM_ID_CAR_OPEN_LHS_1:
    case ANIM_ID_CAR_OPEN_RHS_1: { // 0x166
        float zBlend = std::abs(critTime()) >= 10.f ? 1.f : 0.f;
        if (IsAT400()) {
            zBlend = 1.f;
        }
        return { .animProgress = 1.f, .zBlend = zBlend };
    }

    case ANIM_ID_CAR_GETIN_LHS_0:    // 0x167
    case ANIM_ID_CAR_GETIN_RHS_0:
    case ANIM_ID_CAR_GETIN_LHS_1:
    case ANIM_ID_CAR_GETIN_RHS_1:
    case ANIM_ID_CAR_GETIN_BIKE_FRONT: { // 0x16B
        const auto animProgress = 1.f - progress();
        const auto crit         = critTime();
        const auto critAbs      = std::abs(crit);
        float      zBlend       = 1.f;
        if (critAbs < 10.f) {
            zBlend = RampUp(crit, critAbs, progress());
        }
        if (IsAT400()) {
            zBlend = 1.f;
        }
        return { .animProgress = animProgress, .zBlend = zBlend };
    }

    case ANIM_ID_CAR_PULLOUT_LHS: // 0x16C
    case ANIM_ID_CAR_PULLOUT_RHS:
    case ANIM_ID_UNKNOWN_15:      // 0x16E
        return { .animProgress = 1.f, .zBlend = 0.f };

    case ANIM_ID_CAR_CLOSEDOOR_LHS_0: // 0x16F
    case ANIM_ID_CAR_CLOSEDOOR_RHS_0:
    case ANIM_ID_CAR_CLOSEDOOR_LHS_1:
    case ANIM_ID_CAR_CLOSEDOOR_RHS_1:
    case ANIM_ID_CAR_SHUFFLE_RHS_0:
    case ANIM_ID_CAR_SHUFFLE_RHS_1: // 0x174
    case ANIM_ID_CAR_ROLLDOOR:      // 0x182
        return { .animProgress = 0.f, .zBlend = 1.f };

    case ANIM_ID_CAR_GETOUT_LHS_0: // 0x175
    case ANIM_ID_CAR_GETOUT_RHS_0:
    case ANIM_ID_CAR_GETOUT_LHS_1:
    case ANIM_ID_CAR_GETOUT_RHS_1: // 0x178
    case ANIM_ID_CAR_JACKEDLHS:    // 0x17A
    case ANIM_ID_CAR_JACKEDRHS:
    case ANIM_ID_CAR_ROLLOUT_LHS:  // 0x180
    case ANIM_ID_CAR_ROLLOUT_RHS:
    case ANIM_ID_CAR_FALLOUT_LHS:  // 0x183
    case ANIM_ID_CAR_FALLOUT_RHS: { // 0x184
        const auto animProgress = progress();
        const auto crit         = critTime();
        return { .animProgress = animProgress, .zBlend = RampDown(crit, std::abs(crit), progress()) };
    }

    case ANIM_ID_CAR_CLOSE_LHS_0: // 0x17C
    case ANIM_ID_CAR_CLOSE_RHS_0:
    case ANIM_ID_CAR_CLOSE_LHS_1:
    case ANIM_ID_CAR_CLOSE_RHS_1: // 0x17F
        return { .animProgress = 1.f, .zBlend = 0.f };

    case ANIM_ID_CAR_DOORLOCKED_LHS: // 0x185
    case ANIM_ID_CAR_DOORLOCKED_RHS:
        return { .animProgress = 1.f, .zBlend = group.m_specialFlags.bRunSpecialLockedDoor ? 1.f : 0.f };

    default:
        return {};
    }
}

//! The result of `RtQuatSlerp`, but evaluated like the original code (it's inlined, and uses the x87 stack, so intermediate values aren't rounded to float)
RtQuat SlerpOriginal(RtQuat from, RtQuat to, float t) {
    RtQuatSlerpCache cache;
    RtQuatSetupSlerpCache(&from, &to, &cache);

    if (t <= 0.f) {
        return from;
    }
    if (1.f <= t) {
        return to;
    }

    // `RwSinMinusPiToPiMacro`
    const auto Sin = [](double x) {
        const double z = x * x;
        const double v = z * x;
        return x + v * ((((((z * _RW_S6 + _RW_S5) * z + _RW_S4) * z + _RW_S3) * z + _RW_S2) * z) + _RW_S1);
    };

    double sclFrom = 1.0 - t, sclTo = t;
    if (!cache.nearlyZeroOm) {
        sclFrom = Sin(sclFrom * cache.omega);
        sclTo   = Sin(sclTo * cache.omega);
    }

    RtQuat res;
    res.imag.x = (float)(cache.raTo.imag.x * sclTo + cache.raFrom.imag.x * sclFrom);
    res.imag.y = (float)(cache.raTo.imag.y * sclTo + cache.raFrom.imag.y * sclFrom);
    {   // NOTE: The original stores the `from` part to memory (rounding it to float) before adding the `to` part
        const float fromZ = (float)(cache.raFrom.imag.z * sclFrom);
        res.imag.z = (float)(cache.raTo.imag.z * sclTo + fromZ);
    }
    res.real = (float)(cache.raTo.real * sclTo + cache.raFrom.real * sclFrom);
    return res;
}

//! Get the quaternion of the given matrix
RtQuat QuatFromMatrix(const CMatrix& mat) {
    RwMatrix rw;
    mat.CopyToRwMatrix(&rw);
    RtQuat quat;
    RtQuatConvertFromMatrix(&quat, &rw);
    return quat;
}

//! Sets the orientation of the ped to the quaternion `q` and the position to `pos`
//! (`RtQuatConvertToMatrix` essentially, but with the original's evaluation order)
void SetPedMatrixFromQuat(CPed* ped, const RtQuat& q, const CVector& pos) {
    const double x = q.imag.x, y = q.imag.y, z = q.imag.z, w = q.real;

    const double s  = 2.0 / (((y * y + x * x) + z * z) + w * w);
    const double xs = x * s;
    const float  ys = (float)(s * y);
    const double zs = s * z;

    const float xw = (float)(xs * w);
    const float yw = (float)(ys * w);
    const float zw = (float)(w * zs);
    const float xx = (float)(xs * x);
    const float yy = (float)(ys * y);
    const float zz = (float)(z * zs);
    const float yz = (float)(zs * y);
    const double xz = xs * z;
    const double xy = ys * x;

    RwMatrix rw{};
    rw.right = { (float)(1.0 - ((double)zz + yy)), (float)(zw + xy), (float)(xz - yw) };
    rw.flags = 3;
    rw.up    = { (float)(xy - zw), (float)(1.0 - ((double)zz + xx)), (float)((double)yz + xw) };
    rw.at    = { (float)(xz + yw), (float)((double)yz - xw), (float)(1.0 - ((double)yy + xx)) };
    rw.pos   = { 0.f, 0.f, 0.f };

    CMatrix mat;
    mat.UpdateMatrix(&rw);
    mat.GetPosition() = pos;
    ped->SetMatrix(mat);
}
}

// 0x6513A0
bool CTaskUtilityLineUpPedWithCar::ProcessPed(CPed* ped, CVehicle* vehicle, CAnimBlendAssociation* assoc) {
    if (m_nDoorOpenPosType == 0) {
        ped->m_vecMoveSpeed = CVector{};
    }

    // Heading of the vehicle (in the original it's re-calculated each time, but it's always the same)
    const auto GetVehicleHeading = [&]() -> double {
        if (vehicle->m_matrix) {
            const auto& fwd = vehicle->m_matrix->GetForward();
            return std::atan2(-(double)fwd.x, (double)fwd.y);
        }
        return vehicle->m_placement.m_fHeading;
    };

    // Set the rotation the ped should be facing
    const bool bVehicleFlipped = vehicle->GetUp().z <= -0.8f; // 0x858C9C - (NOTE: Unlike `IsUpsideDown` this is -0.8)
    if (!bVehicleFlipped) {
        if (m_nDoorIdx == 18) {
            ped->m_fAimingRotation = (float)(GetVehicleHeading() + PI_F);
        } else if (m_nDoorOpenPosType != 2) {
            ped->m_fAimingRotation = (float)GetVehicleHeading();
        }
    } else {
        if (m_nDoorIdx == 8 || m_nDoorIdx == 9) {
            ped->m_fAimingRotation = (float)(GetVehicleHeading() - PI_F);
        } else {
            ped->m_fAimingRotation = (float)GetVehicleHeading();
        }
    }

    // Figure out the blends, based on the anim
    const auto  blends       = assoc ? GetLineUpBlends(vehicle, assoc) : LineUpBlends{};
    const float animProgress = blends.animProgress;
    const float zBlend       = blends.zBlend;

    // Position the ped should be at (Current position, if it's `2`)
    CVector targetPos = m_nDoorOpenPosType == 2
        ? ped->GetPosition()
        : GetPositionToOpenCarDoor(vehicle, animProgress, assoc);

    // Position of the door when the animation is done
    CVector doorPos = targetPos;
    if (vehicle->m_nVehicleType != VEHICLE_TYPE_BIKE && m_nDoorOpenPosType != 2) {
        doorPos = GetPositionToOpenCarDoor(vehicle, 1.f, assoc);
    }

    // Figure out the Z coordinate of the door position
    if (!vehicle->physicalFlags.bSubmergedInWater) {
        const auto& vehPos = vehicle->GetPosition();

        // Vehicle's right vector
        float  rightX, rightZ;
        double rightY; // NOTE: Unrounded in the original, if there's no matrix
        if (vehicle->m_matrix) {
            const auto& right = vehicle->m_matrix->GetRight();
            rightX = right.x;
            rightY = right.y;
            rightZ = right.z;
        } else {
            const float heading = vehicle->m_placement.m_fHeading;
            rightX = (float)std::cos((double)heading);
            rightY = std::sin((double)heading);
            rightZ = 0.f;
        }

        // How far (along the right vector) the door position is from the vehicle
        const float dot = (float)((rightZ * ((double)doorPos.z - vehPos.z) + rightY * ((double)doorPos.y - vehPos.y)) + ((double)doorPos.x - vehPos.x) * rightX);

        const float origZ = doorPos.z;
        const auto  z     = (float)((((double)vehPos.z - vehicle->GetHeightAboveRoad()) + (double)rightZ * dot) + 1.0);

        // Find the ground below the door
        doorPos = CPedPlacement::FindZCoorForPed(doorPos).first;

        // Ignore the found ground if it's too low (compared to the vehicle)
        if ((double)z - 0.5 > doorPos.z) {
            doorPos.z = origZ;
        }
    } else if (vehicle->m_nVehicleType == VEHICLE_TYPE_BOAT && vehicle->IsUpsideDown()) {
        doorPos.z += 1.f;
    }
    m_fDoorOpenPosZ = doorPos.z;

    if (m_nDoorOpenPosType == 1 || m_nDoorOpenPosType == 2) {
        const auto& pedPos    = ped->GetPosition();
        const double speedZ   = (double)ped->m_vecMoveSpeed.z - (double)CTimer::GetTimeStep() * 0.008f; // 0x863984
        if (speedZ + pedPos.z < m_fDoorOpenPosZ) { // Below the door
            targetPos.z = m_fDoorOpenPosZ;
            ped->m_vecMoveSpeed = CVector{};
        } else {
            ped->m_vecMoveSpeed.z = (float)speedZ;
            targetPos.z = pedPos.z;
        }
    }

    // Blend the Z coordinate (towards the door)
    const auto BlendTargetZ = [&] {
        targetPos.z = (float)((((double)targetPos.z - m_fDoorOpenPosZ) * zBlend) + m_fDoorOpenPosZ);
    };
    if (m_fDoorOpenPosZ > targetPos.z) {
        if (vehicle->m_nVehicleType == VEHICLE_TYPE_BIKE && assoc && assoc->GetAnimId() != ANIM_ID_CAR_GETIN_BIKE_FRONT) {
            float k;
            switch (assoc->GetAnimId()) {
            case ANIM_ID_CAR_GETOUT_LHS_0:
            case ANIM_ID_CAR_GETOUT_RHS_0:
            case ANIM_ID_CAR_GETOUT_LHS_1:
            case ANIM_ID_CAR_GETOUT_RHS_1:
                k = 1.f - animProgress;
                break;
            case ANIM_ID_CAR_GETIN_LHS_0:
            case ANIM_ID_CAR_GETIN_RHS_0:
            case ANIM_ID_CAR_GETIN_LHS_1:
            case ANIM_ID_CAR_GETIN_RHS_1: {
                const auto t2 = assoc->GetTimeProgress() * 2.f;
                k = t2 <= 1.f ? t2 : 1.f;
                break;
            }
            default:
                k = 0.f;
                break;
            }
            const auto& vehPos = vehicle->GetPosition();
            targetPos.z = (float)(((((double)vehPos.z - vehicle->GetHeightAboveRoad()) + 1.0) - m_fDoorOpenPosZ) * k + m_fDoorOpenPosZ);
        } else {
            BlendTargetZ();
        }
    } else if (m_nDoorOpenPosType == 0) {
        BlendTargetZ();
    }

    // Slowly move the ped to the target (until the time is up)
    if (CTimer::GetTimeInMS() < (uint32)m_fTime) {
        const float aimRot = CGeneral::LimitRadianAngle(ped->m_fAimingRotation);
        const float curRot = ped->m_fCurrentRotation;

        // Seconds-ish left
        const int32 timeLeft = (int32)((uint32)m_fTime - CTimer::GetTimeInMS());
        double t = timeLeft;
        if (timeLeft < 0) {
            t += 4294967296.0; // 0x858C54
        }
        t *= 0.0016666667f; // 0x865060
        if (t <= 0.0) {
            m_Offset.x = 0.f;
            m_Offset.y = 0.f;
        }
        m_Offset.z = 0.f;

        const float offY = (float)(t * m_Offset.y);
        const float offX = (float)(t * m_Offset.x);
        targetPos.x = (float)((double)targetPos.x - offX);
        targetPos.y = (float)((double)targetPos.y - offY);
        targetPos.z = (float)((double)targetPos.z - 0.0 * t);

        // Make sure the rotation we're blending to is within [-pi, pi] of the current one
        double rot = aimRot;
        if ((double)curRot + PI_F < rot) {
            rot -= TWO_PI_F;
        } else if ((double)curRot - PI_F > rot) {
            rot += TWO_PI_F;
        }
        ped->m_fCurrentRotation = (float)(curRot - (curRot - rot) * (1.0 - t));
    } else {
        ped->m_fCurrentRotation = ped->m_fAimingRotation;
    }

    // Anims that rotate the ped to/from the vehicle's orientation
    if (assoc) {
        const auto animId = assoc->GetAnimId();
        switch (animId) {
        case ANIM_ID_CAR_GETIN_LHS_0: // 0x167
        case ANIM_ID_CAR_GETIN_RHS_0:
        case ANIM_ID_CAR_GETIN_LHS_1:
        case ANIM_ID_CAR_GETIN_RHS_1:
        case ANIM_ID_CAR_GETIN_BIKE_FRONT: { // 0x16B
            CMatrix vehMat{ vehicle->GetMatrix() };
            if (animId == ANIM_ID_CAR_GETIN_BIKE_FRONT) { // Turn around
                CMatrix rot;
                rot.SetRotateZ(PI_F);
                vehMat *= rot;
            }
            const auto vehQuat = QuatFromMatrix(vehMat);
            const auto pedQuat = QuatFromMatrix(CMatrix{ ped->GetMatrix() });

            SetPedMatrixFromQuat(ped, SlerpOriginal(pedQuat, vehQuat, assoc->GetTimeProgress()), targetPos);
            return false;
        }
        case ANIM_ID_CAR_GETOUT_LHS_0: // 0x175
        case ANIM_ID_CAR_GETOUT_RHS_0:
        case ANIM_ID_CAR_GETOUT_LHS_1:
        case ANIM_ID_CAR_GETOUT_RHS_1:
        case ANIM_ID_UNKNOWN_26: { // 0x179
            const auto vehQuat = QuatFromMatrix(vehicle->GetMatrix());

            CMatrix pedMat{ ped->GetMatrix() };
            pedMat.SetRotateZOnly(ped->m_fCurrentRotation);
            const auto pedQuat = QuatFromMatrix(pedMat);

            SetPedMatrixFromQuat(ped, SlerpOriginal(vehQuat, pedQuat, assoc->GetTimeProgress()), targetPos);
            return false;
        }
        default:
            break;
        }
    }

    if (animProgress <= 0.2f && !bVehicleFlipped && vehicle->m_nVehicleType != VEHICLE_TYPE_BIKE && vehicle->m_nVehicleSubType != VEHICLE_TYPE_QUAD) {
        // Place the ped relative to the vehicle, with the orientation of the vehicle
        CMatrix vehMat{ vehicle->GetMatrix() };
        targetPos = GetLocalPositionToOpenCarDoor(vehicle, 0.f, assoc);
        vehMat.GetPosition() += TransformVectorOriginal(vehMat, targetPos);
        ped->SetMatrix(vehMat);
    } else {
        ped->SetPosn(targetPos);
        ped->SetOrientation(0.f, 0.f, ped->m_fCurrentRotation);
    }

    // NOTSA: The original always returns false (the result is `AL = 0`)
    return false;
}
