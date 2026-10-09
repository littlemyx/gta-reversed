#include "StdInc.h"

#include "TaskSimpleGangDriveBy.h"

#include "Camera.h"
#include "WeaponInfo.h"
#include "PlayerPedData.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Enums/eCamMode.h"
#include "Events/EventDamage.h"
#include "Entity/Ped/PlayerPed.h"
#include "PlayerInfo.h"
#include "Entity/Vehicle/Bike.h"
#include "Pad.h"
#include "GameLogic.h"
#include "World.h"

#include <numbers>

void CTaskSimpleGangDriveBy::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleGangDriveBy, 0x86D944, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(FireGun, 0x627CC0);
    RH_ScopedInstall(StartDriveByAnim, 0x627B20);
    RH_ScopedInstall(ProcessPlayerPed, 0x621960);
    RH_ScopedInstall(ProcessAIPed, 0x627600);
    RH_ScopedInstall(ProcessAimIK, 0x628350);
    RH_ScopedInstall(CheckLineOfSight, 0x621B10);
    RH_ScopedInstall(FinishDriveByAnimCB, 0x621BE0);
    RH_ScopedVMTInstall(Clone, 0x6236D0);
    RH_ScopedVMTInstall(MakeAbortable, 0x62D290);
    RH_ScopedVMTInstall(ProcessPed, 0x62D3B0);
}

CTaskSimpleGangDriveBy::CTaskSimpleGangDriveBy(CEntity* target, const CVector* targetPos, float abortRange, int8 frequencyPercentage, eDrivebyStyle drivebyStyle, bool seatRHS) {
    m_bSeatRHS             = seatRHS;
    m_nDrivebyStyle        = drivebyStyle;
    m_fAbortRange          = abortRange;
    m_pTargetEntity        = target;
    m_nFrequencyPercentage = frequencyPercentage;
    m_bIsFinished          = false;
    m_bAnimsReferenced     = false;
    m_bInRangeToShoot      = false;
    m_bInWeaponRange       = false;
    m_bReachedAbortRange   = false;
    m_bFromScriptCommand   = false;
    m_nBurstShots          = -1;
    m_nFakeShootDirn       = -1;
    m_nAttackTimer         = -1;
    m_nLastCommand         = 0;
    m_nNextCommand         = 1;
    m_nLOSCheckTime        = 0;
    m_nLOSBlocked          = true;
    m_pAnimAssoc           = nullptr;
    m_nRequiredAnimID      = ANIM_ID_NO_ANIMATION_SET;
    m_nRequiredAnimGroup   = ANIM_GROUP_DEFAULT;
    m_pWeaponInfo          = nullptr;
    CEntity::SafeRegisterRef(m_pTargetEntity);
    if (targetPos) {
        m_vecCoords = *targetPos;
    }
}

CTaskSimpleGangDriveBy::~CTaskSimpleGangDriveBy()
{
    if (m_bAnimsReferenced)
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex(m_nRequiredAnimGroup));

    if (m_pAnimAssoc)
        m_pAnimAssoc->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);

    CEntity::SafeCleanUpRef(m_pTargetEntity);
}

// 0x6236D0
CTask* CTaskSimpleGangDriveBy::Clone() const {
    auto* const task = new CTaskSimpleGangDriveBy(m_pTargetEntity, &m_vecCoords, m_fAbortRange, m_nFrequencyPercentage, m_nDrivebyStyle, m_bSeatRHS);
    task->m_bFromScriptCommand = m_bFromScriptCommand;
    return task;
}

// 0x621BE0 - Delete callback of `m_pAnimAssoc` (`data` is the task)
void CTaskSimpleGangDriveBy::FinishDriveByAnimCB(CAnimBlendAssociation* anim, void* data) {
    auto* const task = static_cast<CTaskSimpleGangDriveBy*>(data);
    if (task->m_pAnimAssoc == anim) {
        task->m_pAnimAssoc = nullptr;
    }
}

// 0x621B10 (name guessed) - Periodically (re)checks if the target is visible, returns true if it is
bool CTaskSimpleGangDriveBy::CheckLineOfSight(CPed* ped, const CVector& targetPos) {
    const auto now = CTimer::GetTimeInMS();
    if (now > m_nLOSCheckTime) {
        m_nLOSBlocked = false;
        if (m_pTargetEntity) {
            const auto&   pos = ped->GetPosition();
            const CVector origin{pos.x, pos.y, (float)((double)pos.z + (double)0.5f)}; // 0x858B8C

            CWorld::pIgnoreEntity = m_pTargetEntity; // 0xB7CD68
            if (!CWorld::GetIsLineOfSightClear(origin, targetPos, true, false, false, false, false, true, false)) { // 0x56A490
                m_nLOSBlocked = true;
            }
            CWorld::pIgnoreEntity = nullptr;
        }
        // Next check in [1750, 2250) ms. (x87: rand * 2^-15 * -500.0 (0x85AB78) is truncated, then subtracted)
        m_nLOSCheckTime = (uint32)((int32)now - (int32)((double)(CGeneral::GetRandomNumber() & 0xFFFF) * (double)(1.f / 32768.f) * (double)-500.f) + 0x6D6); // 0x858B14
    }
    return !m_nLOSBlocked;
}

// 0x627B20 (name guessed) - Starts/switches the drive-by animation according to `m_nNextCommand`. Returns true if the animation is faded in enough to be used
bool CTaskSimpleGangDriveBy::StartDriveByAnim(CPed* ped) {
    auto* blk = CAnimManager::GetAnimationBlock(m_nRequiredAnimGroup);
    if (!blk) {
        blk = CAnimManager::GetAnimationBlock(CAnimManager::GetAnimBlockName(m_nRequiredAnimGroup));
    }
    const auto blkIdx = CAnimManager::GetAnimationBlockIndex(blk);

    if (!blk->IsLoaded) {
        CStreaming::RequestModel(IFPToModelId(blkIdx), STREAMING_KEEP_IN_MEMORY); // 8
        return false;
    }

    if (!m_bAnimsReferenced) {
        CAnimManager::AddAnimBlockRef(blkIdx); // 0x4D3FB0
        m_bAnimsReferenced = true;
    }

    if (m_nRequiredAnimID == ANIM_ID_NO_ANIMATION_SET && !m_pAnimAssoc) {
        return false;
    }

    const bool bStart = !m_pAnimAssoc
        ? m_nAttackTimer <= 0 && m_bInWeaponRange
        : m_pAnimAssoc->m_AnimId != m_nRequiredAnimID
            && (m_nNextCommand == 1 || m_nNextCommand == 2 || m_nNextCommand == 3)
            && m_nLastCommand < 4;
    if (bStart) {
        bool bHadAnim = false;
        if (m_pAnimAssoc) {
            m_pAnimAssoc->SetDefaultDeleteCallback(); // 0x4CEBC0
            m_pAnimAssoc->m_Flags &= ~ANIMATION_IS_PLAYING;
            bHadAnim = true;
        }

        m_pAnimAssoc = CAnimManager::BlendAnimation(ped->GetRpClump(), m_nRequiredAnimGroup, (AnimationId)m_nRequiredAnimID, 4.f); // 0x4D4610
        m_pAnimAssoc->SetDeleteCallback(FinishDriveByAnimCB, this); // 0x4CEBC0

        if (m_nNextCommand == 3 && m_nBurstShots <= 1) {
            m_nBurstShots = (char)CGeneral::GetRandomNumberInRange(2, (int32)(int16)m_pWeaponInfo->m_nAmmoClip); // 0x407180
        }

        if (bHadAnim) {
            m_pAnimAssoc->SetCurrentTime(StaticRef<float>(0x8D2E7C)); // 0x4CEA80
            m_pAnimAssoc->m_Flags &= ~ANIMATION_IS_PLAYING;
            m_nLastCommand = 1;
            m_nNextCommand = 0;
            if (!ped->IsPlayer()) { // 0x5DF8F0
                m_nAttackTimer = 100;
            }
        } else {
            m_nLastCommand = m_nNextCommand;
            m_nNextCommand = 0;
        }
    }

    // Is the animation faded in (and not fading out)?
    const auto* const a = m_pAnimAssoc;
    return a && a->m_BlendAmount > 0.9f && a->m_BlendDelta >= 0.f; // 0x858C20, 0x858B50
}

// Same as the matrix-vector multiplication (0x59C890) in the original (the sum stays in the FPU registers, stored as float)
static CVector TransformPointExt(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp(), &p = m.GetPosition();
    return CVector{
        (float)((((double)u.x * v.z + (double)f.x * v.y) + (double)r.x * v.x) + p.x),
        (float)((((double)u.y * v.z + (double)r.y * v.x) + (double)f.y * v.y) + p.y),
        (float)((((double)u.z * v.z + (double)r.z * v.x) + (double)f.z * v.y) + p.z)
    };
}

// Makes sure the entity has a matrix (0x54F560, 0x54F1B0)
static const CMatrix& EnsureEntityMatrix(CEntity* e) {
    if (!e->m_matrix) {
        e->AllocateMatrix();
        e->m_placement.UpdateMatrix(e->m_matrix);
    }
    return *e->m_matrix;
}

// Heading of a vehicle (the original reads the matrix's forward vector, or the placement's heading if there's no matrix)
static double GetVehicleHeading(CVehicle* veh) {
    if (const auto* const mat = veh->m_matrix) {
        return std::atan2(-(double)mat->GetForward().x, (double)mat->GetForward().y);
    }
    return (double)veh->m_placement.m_fHeading;
}

// The animation to use for the sector of the vehicle (0 = front, 1 = left/top, 2 = back, 3 = right/top) the target is in
static AnimationId GetDrivebyAnimForSector(int32 sector, bool bSeatRHS) {
    switch (sector) {
    case 0: return bSeatRHS ? ANIM_ID_DRIVEBYRHS_FWD : ANIM_ID_DRIVEBYLHS_FWD;  // 0xED : 0xE9
    case 1: return bSeatRHS ? ANIM_ID_DRIVEBYTOP_RHS : ANIM_ID_DRIVEBYLHS;      // 0xEC : 0xE7
    case 2: return bSeatRHS ? ANIM_ID_DRIVEBYRHS_BWD : ANIM_ID_DRIVEBYLHS_BWD;  // 0xEE : 0xEA
    case 3: return bSeatRHS ? ANIM_ID_DRIVEBYRHS : ANIM_ID_DRIVEBYTOP_LHS;      // 0xEB : 0xE8
    default: NOTSA_UNREACHABLE();
    }
}

// 0x621960 (name guessed) - Player's drive-by (the target is where the camera aims)
void CTaskSimpleGangDriveBy::ProcessPlayerPed(CPed* ped) {
    auto& cam = TheCamera.GetActiveCam();

    if (cam.m_nMode != MODE_AIMWEAPON_FROMCAR && cam.m_nMode != MODE_TWOPLAYER_IN_CAR_AND_SHOOTING) { // 0x37, 0x31
        m_bInRangeToShoot = false;
        m_bInWeaponRange  = true;
    } else {
        m_bInRangeToShoot = true;
        m_bInWeaponRange  = true;

        CVector aimDir = cam.m_vecFront;
        if (cam.m_nMode == MODE_TWOPLAYER_IN_CAR_AND_SHOOTING) {
            cam.Get_TwoPlayer_AimVector(aimDir); // 0x513E40
        }

        // x87: Everything stays in extended precision until the sector is truncated
        const double aimHeading = std::atan2(-(double)aimDir.x, (double)aimDir.y);
        double       rel        = aimHeading - GetVehicleHeading(ped->m_pVehicle);
        if (rel > (double)std::numbers::pi_v<float>) { // 0x858CB8
            rel -= (double)(2.f * std::numbers::pi_v<float>); // 0x858CBC
        } else if (rel < -(double)std::numbers::pi_v<float>) { // 0x858CC0
            rel += (double)(2.f * std::numbers::pi_v<float>);
        }
        rel += (double)(std::numbers::pi_v<float> / 4.f); // 0x859AB0
        if (rel < 0.0) {
            rel += (double)(2.f * std::numbers::pi_v<float>);
        }
        rel *= (double)StaticRef<float>(0x858FB8); // 2/pi
        const auto sector = (int8)(int32)rel; // 0x821B40 (truncated), only the low byte is used
        if ((uint32)(int32)sector <= 3) {
            m_nRequiredAnimID = GetDrivebyAnimForSector(sector, m_bSeatRHS);
        }
    }

    if (!CGameLogic::IsCoopGameGoingOn()) { // 0x441390
        TheCamera.SetNewPlayerWeaponMode(MODE_AIMWEAPON_FROMCAR, 0, 0); // 0x50BFB0
    }

    if (auto* const pd = ped->GetPlayerData()) {
        pd->m_bFreeAiming = true; // +0x34 |= 8
    }

    if (ped->IsPlayer()) { // 0x5DF8F0
        static_cast<CPlayerPed*>(ped)->GetPlayerInfoForThisPlayerPed()->m_bCanDoDriveBy = false; // +0x153
    }
}

// 0x627600 (name guessed) - AI's drive-by: picks the animation (side to shoot from) and decides if the target is in range
void CTaskSimpleGangDriveBy::ProcessAIPed(CPed* ped) {
    constexpr float PI          = std::numbers::pi_v<float>;
    constexpr float PI_3_OVER_4 = std::bit_cast<float>(0x4016CBE4u); // 0x86DAF4

    const float weaponRange = m_pWeaponInfo->m_fWeaponRange;
    const auto& pedPos      = ped->GetPosition();

    // Vector to the target (every component is rounded to float)
    CVector toTgt;
    if (m_pTargetEntity) {
        const auto w = TransformPointExt(EnsureEntityMatrix(m_pTargetEntity), m_vecCoords); // 0x59C890
        toTgt = CVector{
            (float)((double)w.x - (double)pedPos.x),
            (float)((double)w.y - (double)pedPos.y),
            (float)((double)w.z - (double)pedPos.z),
        };
    } else {
        toTgt = CVector{
            (float)((double)m_vecCoords.x - (double)pedPos.x),
            (float)((double)m_vecCoords.y - (double)pedPos.y),
            (float)((double)m_vecCoords.z - (double)pedPos.z),
        };
    }

    // x87: The sum of squares stays in extended precision
    const float  dist       = (float)std::sqrt(((double)toTgt.z * toTgt.z + (double)toTgt.x * toTgt.x) + (double)toTgt.y * toTgt.y);
    const double aimHeading = std::atan2(-(double)toTgt.x, (double)toTgt.y);

    // Heading of the target relative to the vehicle's
    float rel = (float)(aimHeading - GetVehicleHeading(ped->m_pVehicle));
    if (rel > PI) { // 0x858CB8
        rel = (float)((double)rel - (double)(2.f * PI)); // 0x858CBC
    } else if (rel < -PI) { // 0x858CC0
        rel = (float)((double)rel + (double)(2.f * PI));
    }

    m_bInRangeToShoot = false;
    m_bInWeaponRange  = dist < weaponRange;

    if (!m_bReachedAbortRange) {
        if (dist < m_fAbortRange) {
            m_bReachedAbortRange = true;
        }
    } else if (dist > m_fAbortRange) {
        m_nNextCommand = 7;
        return;
    }

    // Dead target?
    if (auto* const tgt = m_pTargetEntity) {
        const auto type = tgt->GetType();
        if ((type == ENTITY_TYPE_PED     && static_cast<CPed*>(tgt)->m_fHealth <= 0.f)
         || (type == ENTITY_TYPE_VEHICLE && static_cast<CVehicle*>(tgt)->m_fHealth <= 0.f)
        ) {
            m_nNextCommand = 7;
            return;
        }
    }

    // Target position again (floats this time)
    const CVector tgtPos{
        (float)((double)toTgt.x + (double)pedPos.x),
        (float)((double)toTgt.y + (double)pedPos.y),
        (float)((double)toTgt.z + (double)pedPos.z),
    };
    if (!CheckLineOfSight(ped, tgtPos)) { // 0x621B10
        m_bInRangeToShoot = false;
        return;
    }

    // Which side/direction is the target at?
    const auto style = (int8)m_nDrivebyStyle;
    int8       sector = -1; // The sector of the vehicle (see `GetDrivebyAnimForSector`)
    int8       aimSide = 0; // -1 = left, 1 = right
    int8       aimFwdBwd = 0; // -1 = forward, 1 = backward
    if (style != 0 && style != 1 && style != 5 && style != 6) {
        // x87: Everything stays in extended precision until the sector is truncated
        double v = (double)rel + (double)(PI / 4.f); // 0x859AB0
        if (v < 0.0) {
            v += (double)(2.f * PI);
        }
        sector = (int8)(int32)(v * (double)StaticRef<float>(0x858FB8)); // 2/pi, 0x821B40 (truncated)

        constexpr float DEG_15  = std::bit_cast<float>(0x3E860A92u); // 0x858F1C
        constexpr float DEG_165 = std::bit_cast<float>(0x40384E89u); // 0x86DB10
        constexpr float DEG_75  = std::bit_cast<float>(0x3FA78D36u); // 0x86DB08
        constexpr float DEG_105 = std::bit_cast<float>(0x3FEA9280u); // 0x86DB00
        if (rel > DEG_15 && rel < DEG_165) {
            aimSide = -1;
        } else if (rel < -DEG_15 && rel > -DEG_165) {
            aimSide = 1;
        }
        if (rel < DEG_75 && rel > -DEG_75) {
            aimFwdBwd = -1;
        } else if (rel > DEG_105 && rel < -DEG_105) { // BUG: Can never be true (probably meant `||`)
            aimFwdBwd = 1;
        }
    }

    int8 res = -1;
    const auto bySector = [&] {
        if (sector == 1) {
            res = 1;
        } else if (sector == 3) {
            res = 3;
        }
    };
    const bool bNoAnimYet = m_nRequiredAnimID == ANIM_ID_NO_ANIMATION_SET;
    switch (style) {
    case 0: // FIXED_LHS
        res = 1;
        break;
    case 1: // FIXED_RHS
        res = 3;
        break;
    case 2: // START_FROM_LHS
        if (bNoAnimYet) {
            res = 1;
        } else {
            bySector();
        }
        break;
    case 3: // START_FROM_RHS
        if (bNoAnimYet) {
            res = 3;
        } else {
            bySector();
        }
        break;
    case 4: // AI_SIDE
        if (bNoAnimYet) {
            if (aimSide == -1) {
                res = 1;
            } else if (aimSide == 1) {
                res = 3;
            }
        } else {
            bySector();
        }
        break;
    case 5: // FIXED_FWD
        res = 0;
        break;
    case 6: // FIXED_BAK
        res = 2;
        break;
    case 7: // AI_FWD_BAK
        if (bNoAnimYet) {
            if (aimFwdBwd == -1) {
                res = 0;
            } else if (aimFwdBwd == 1) {
                res = 2;
            }
        } else if (sector == 0) {
            res = 0;
        } else if (sector == 2) {
            res = 2;
        }
        break;
    case 8: // AI_ALL_DIRN
        res = sector;
        break;
    }

    if ((uint32)(int32)res > 3) {
        return;
    }

    // NOTE: The animation is set before checking if the target is in range
    m_nRequiredAnimID = GetDrivebyAnimForSector(res, m_bSeatRHS);
    if (!(dist < weaponRange)) {
        return;
    }
    switch (res) {
    case 0: // Front
        m_bInRangeToShoot = rel >= -PI / 4.f && rel <= PI / 4.f; // 0x86DAF8, 0x859AB0
        break;
    case 1: // Left
        m_bInRangeToShoot = rel >= PI / 4.f && rel <= PI_3_OVER_4; // 0x859AB0, 0x86DAF4
        break;
    case 2: // Back
        m_bInRangeToShoot = rel >= -PI_3_OVER_4 || rel <= PI_3_OVER_4; // 0x86DAF0, 0x86DAF4
        break;
    case 3: // Right
        m_bInRangeToShoot = rel >= -PI_3_OVER_4 && rel <= -PI / 4.f; // 0x86DAF0, 0x86DAF8
        break;
    }
}

// 0x53CBE0 - `CGeneral::GetRadianAngleBetweenPoints` as the original evaluates it: the quotient, the arctangent and the result
// stay in extended precision (the result is returned in ST0, unrounded), the quadrant offsets are (pi_f / 2) added/subtracted in a fixed order
static double GetRadianAngleBetweenPointsExt(float x1, float y1, float x2, float y2) {
    constexpr double HALF_PI = (double)(std::numbers::pi_v<float> / 2.f); // 0x858FE4 (and -HALF_PI at 0x859998)

    const double x = (double)x2 - (double)x1;
    double       y = (double)y2 - (double)y1;
    if (y == 0.0) { // FCOM + JP: NaN keeps its value
        y = (double)0.0001f; // 0x858FC4
    }

    const double a = std::atan2(x / y, 1.0);
    if (x > 0.0) {
        return y > 0.0
            ? (HALF_PI - a) + HALF_PI
            : HALF_PI - (a + HALF_PI);
    }
    return y > 0.0
        ? -HALF_PI - (a + HALF_PI)
        : (HALF_PI - a) - HALF_PI;
}

// 0x628350 (name guessed) - Points the gun (IK) at the target
void CTaskSimpleGangDriveBy::ProcessAimIK(CPed* ped) {
    ped->m_pedIK.bUseArm = false;

    CVector target{}; // Where we're aiming at
    if (ped->IsPlayer()) { // 0x5DF8F0
        auto& cam = TheCamera.GetActiveCam();

        float range = 20.f; // 0x41A00000
        if (cam.m_nMode == MODE_TWOPLAYER_IN_CAR_AND_SHOOTING) {
            const auto&  pos = ped->GetPosition();
            const double dx  = (double)pos.x - cam.m_vecSource.x;
            const double dy  = (double)pos.y - cam.m_vecSource.y;
            const double dz  = (double)pos.z - cam.m_vecSource.z;
            range            = (float)(std::sqrt((dz * dz + dy * dy) + dx * dx) + 20.0); // 0x858BA4
        } else if (cam.m_nMode != MODE_AIMWEAPON_FROMCAR) {
            return;
        }

        const auto& pos = ped->GetPosition();
        CVector     unused;
        TheCamera.Find3rdPersonCamTargetVector(range, CVector{pos.x, pos.y, (float)((double)pos.z + (double)0.7f)}, unused, target); // 0x514970, 0x858CB0
    } else if (auto* const tgt = m_pTargetEntity) {
        if (m_vecCoords.x == 0.f && m_vecCoords.y == 0.f && m_vecCoords.z == 0.f) {
            if (tgt->GetType() == ENTITY_TYPE_PED) {
                static_cast<CPed*>(tgt)->GetBonePosition(&target, BONE_SPINE1, false); // 0x5E4280
            } else {
                target = tgt->GetPosition();
            }
        } else {
            target = TransformPointExt(EnsureEntityMatrix(tgt), m_vecCoords); // 0x59C890
        }
    } else if (!(m_vecCoords.x == 0.f && m_vecCoords.y == 0.f && m_vecCoords.z == 0.f)) {
        target = m_vecCoords;
    }

    // Only the Z of the bone is used (the position is the ped's)
    CVector bone{};
    ped->GetBonePosition(&bone, BONE_R_UPPER_ARM, false); // 0x5E4280
    const auto&   pedPos = ped->GetPosition();
    const CVector from{pedPos.x, pedPos.y, bone.z};

    float aim  = (float)GetRadianAngleBetweenPointsExt(target.x, target.y, from.x, from.y); // 0x53CBE0
    float tilt = 0.f;

    auto* const veh = ped->m_pVehicle; // NOTE: No check for the vehicle in the player's case either
    if (veh && veh->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        // x87: The differences stay in extended precision
        const double dx    = (double)from.x - (double)target.x;
        const double dy    = (double)from.y - (double)target.y;
        const double pitch = GetRadianAngleBetweenPointsExt(target.z, (float)std::sqrt(dy * dy + dx * dx), from.z, 0.f); // Not rounded to float (stays in the FPU)

        const auto* const bike = static_cast<CBike*>(veh);
        tilt = (float)(std::sin((double)aim - GetVehicleHeading(veh)) * (double)bike->m_RideAnimData.LeanAngle + pitch);

        const float cosHeading = (float)std::cos((double)aim - GetVehicleHeading(veh));

        // BUG: The original doesn't check for the vehicle's matrix here
        const float fwdZ = veh->m_matrix->GetForward().z;
        float       clampedFwdZ;
        if (-1.f > fwdZ) {
            clampedFwdZ = -1.f;
        } else if (1.f < fwdZ) {
            clampedFwdZ = 1.f;
        } else {
            clampedFwdZ = fwdZ;
        }
        tilt = (float)(std::asin((double)clampedFwdZ) * (double)cosHeading + (double)tilt); // 0x821E70
    }

    // Flip it if we're shooting from the other side
    bool bFlip = false;
    // NOTE: The original doesn't check for the animation
    switch ((AnimationId)m_pAnimAssoc->m_AnimId) {
    case ANIM_ID_DRIVEBYLHS:      // 0xE7
    case ANIM_ID_DRIVEBYTOP_RHS:  // 0xEC
        bFlip = true;
        aim   = (float)((double)aim - (double)(std::numbers::pi_v<float> / 2.f)); // 0x858FE4
        tilt  = (float)((double)tilt * (double)-1.f);                              // 0x858C1C
        break;
    case ANIM_ID_DRIVEBYRHS:      // 0xEB
    case ANIM_ID_DRIVEBYTOP_LHS:  // 0xE8
        bFlip = true;
        aim   = (float)((double)aim + (double)(std::numbers::pi_v<float> / 2.f));
        break;
    case ANIM_ID_DRIVEBYLHS_BWD:  // 0xEA
    case ANIM_ID_DRIVEBYRHS_BWD:  // 0xEE
        aim  = (float)((double)aim - (double)std::numbers::pi_v<float>);
        tilt = (float)((double)tilt * (double)-1.f);
        break;
    default:
        break;
    }

    ped->m_pedIK.PointGunInDirection(aim, tilt, bFlip, -1.f); // 0x5FDC00
}

// 0x62D290
bool CTaskSimpleGangDriveBy::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority != ABORT_PRIORITY_URGENT && priority != ABORT_PRIORITY_IMMEDIATE) {
        m_nNextCommand = 6;
        return false;
    }

    if (event && event->GetEventType() == EVENT_DAMAGE) {
        const auto* const dmg = static_cast<const CEventDamage*>(event);
        if (!dmg->m_damageResponse.m_bHealthZero || !dmg->m_bAddToEventGroup) {
            return false;
        }
    }

    m_nNextCommand = 7;
    if (StartDriveByAnim(ped)) { // 0x627B20
        m_nLastCommand = m_nNextCommand;
        if (priority == ABORT_PRIORITY_IMMEDIATE) {
            m_pAnimAssoc->m_BlendDelta = -1000.f; // 0xC47A0000
        } else if (!(m_pAnimAssoc->m_Flags & ANIMATION_IS_PLAYING)) {
            m_pAnimAssoc->m_Flags |= ANIMATION_IS_PLAYING;
        }
    }

    if (auto* const pd = ped->GetPlayerData()) {
        pd->m_bFreeAiming = true; // +0x34 |= 8
    }

    if (ped->IsPlayer()) {
        static_cast<CPlayerPed*>(ped)->GetPlayerInfoForThisPlayerPed()->m_bCanDoDriveBy = true; // +0x153
    }

    if (auto* const a = m_pAnimAssoc) {
        if (!(a->m_Flags & ANIMATION_IS_PLAYING)
            && (a->m_BlendDelta > 0.f || (a->m_BlendAmount > 0.f && a->m_BlendDelta >= 0.f)) // 0x858B50 = 0.0f
        ) {
            a->m_BlendDelta = -8.f; // 0xC1000000
        }
        a->SetDefaultDeleteCallback();
        m_pAnimAssoc = nullptr;
    }

    m_bIsFinished = true;
    return true;
}

// 0x62D3B0
bool CTaskSimpleGangDriveBy::ProcessPed(CPed* ped) {
    const auto* const wi = CWeaponInfo::GetWeaponInfo(ped->GetActiveWeapon().m_Type, ped->GetWeaponSkill());
    if ((wi && wi->m_nWeaponFire != 1) // 1 = INSTANT_HIT
        || (ped->m_pVehicle && !ped->m_pVehicle->CanPedLeanOut(ped))
        || m_bIsFinished
    ) {
        return true;
    }

    if (m_nLOSCheckTime == 0) {
        const auto now = CTimer::GetTimeInMS();
        m_nLOSCheckTime = CGeneral::GetRandomNumberInRange(0, 1000) + now;
    }

    if (!m_pWeaponInfo) {
        m_pWeaponInfo = CWeaponInfo::GetWeaponInfo(ped->GetActiveWeapon().m_Type, ped->GetWeaponSkill());
    }

    if (!ped->bInVehicle || !ped->m_pVehicle || ped->m_fHealth < 1.f) { // 0x858624 = 1.0f
        MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr); // vtable +0x18
        return true;
    }

    if (!ped->IsPlayer()) {
        ped->bTestForShotInVehicle = true; // +0x478 |= 0x100000
    }

    if (m_nRequiredAnimGroup == 0) {
        auto* const veh = ped->m_pVehicle;
        if (veh->m_nVehicleSubType == VEHICLE_TYPE_QUAD) {
            m_nRequiredAnimGroup = ANIM_GROUP_QUAD_DBZ; // 0x4B
            m_bSeatRHS           = false;
        } else if (veh->m_nVehicleType == VEHICLE_TYPE_BIKE) {
            if (static_cast<CBike*>(veh)->m_RideAnimData.AnimGroup == 2 && veh->m_pDriver == ped) { // +0x640
                m_nRequiredAnimGroup = ANIM_GROUP_COP_DBZ; // 0x4A
            } else {
                m_nRequiredAnimGroup = ANIM_GROUP_BIKE_DBZ; // 0x49
            }
            m_bSeatRHS = false;
        } else {
            if (veh->m_nVehicleSubType == VEHICLE_TYPE_AUTOMOBILE) {
                bool bSetFlag = true;
                uint8 windowFlag;
                if (ped == veh->m_apPassengers[0]) {
                    windowFlag = 8;
                    m_bSeatRHS = true;
                } else if (ped == veh->m_apPassengers[1]) {
                    windowFlag = 11;
                    m_bSeatRHS = false;
                } else if (ped == veh->m_apPassengers[2]) {
                    windowFlag = 9;
                    m_bSeatRHS = true;
                } else {
                    bSetFlag = false;
                }
                if (bSetFlag) {
                    veh->SetWindowOpenFlag(windowFlag); // 0x6D3080
                }
            }
            m_nRequiredAnimGroup = ANIM_GROUP_DRIVEBYS; // 0x48
        }

        if (!ped->IsPlayer() && !ped->m_pWeaponObject) {
            ped->AddWeaponModel(CWeaponInfo::GetWeaponInfo(ped->GetActiveWeapon().m_Type, (eWeaponSkill)1)->m_nModelId1); // 0x5E5ED0
        }
    }

    if (ped->IsPlayer()) {
        ProcessPlayerPed(ped); // 0x621960
    } else {
        ProcessAIPed(ped); // 0x627600
    }

    if (m_nRequiredAnimGroup == ANIM_GROUP_COP_DBZ && ped->m_pVehicle->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        static_cast<CBike*>(ped->m_pVehicle)->m_nFixLeftHand = 1; // +0x7B4
    }

    if (StartDriveByAnim(ped)) { // 0x627B20
        auto* a = m_pAnimAssoc;
        if (a->m_Flags & ANIMATION_IS_PLAYING) {
            const double cur = a->m_CurrentTime;
            if (cur > (double)StaticRef<float>(0x8D2E84) // 0.3f
                && cur - (double)a->m_TimeStep <= (double)StaticRef<float>(0x8D2E84)
                && (m_nLastCommand == 2 || m_nLastCommand == 3)
            ) {
                FireGun(ped);
            }
        }

        a = m_pAnimAssoc;
        const bool bPlaying = (a->m_Flags & ANIMATION_IS_PLAYING) != 0;
        const float t26 = StaticRef<float>(0x8D2E7C);
        if (!bPlaying && m_nAttackTimer <= 0 && (m_nNextCommand == 2 || m_nNextCommand == 3)
            && (a->m_BlendHier->m_fTotalTime > a->m_CurrentTime || a->m_BlendDelta >= 0.f)
        ) {
            a->m_Flags |= ANIMATION_IS_PLAYING;
            m_nBurstShots = (int8)CGeneral::GetRandomNumberInRange(1, (int32)((double)(int16)m_pWeaponInfo->m_nAmmoClip * (double)0.5f)); // 0x858B8C
            m_nLastCommand = m_nNextCommand;
            m_nNextCommand = 0;
        } else if (!bPlaying) {
            if (m_nNextCommand == 7) {
                a->m_Flags |= ANIMATION_IS_PLAYING;
                m_nLastCommand = m_nNextCommand;
                m_nNextCommand = 7;
            } else if (a->m_AnimId != m_nRequiredAnimID) {
                a->m_Flags |= ANIMATION_IS_PLAYING;
            }
        } else if (m_nLastCommand == 1 && a->m_AnimId == m_nRequiredAnimID
            && (double)a->m_CurrentTime > (double)t26
            && (double)a->m_CurrentTime - (double)a->m_TimeStep <= (double)t26
        ) {
            a->m_Flags &= ~ANIMATION_IS_PLAYING;
            m_pAnimAssoc->SetCurrentTime(t26);
        }

        a = m_pAnimAssoc;
        const float t43 = StaticRef<float>(0x8D2E80);
        if (a->m_AnimId == m_nRequiredAnimID
            && (double)a->m_CurrentTime > (double)t43
            && (double)a->m_CurrentTime - (double)a->m_TimeStep <= (double)t43
        ) {
            if (ped->GetActiveWeapon().m_State == WEAPONSTATE_RELOADING && m_nNextCommand <= 2) {
                m_nNextCommand = 2;
                m_nAttackTimer = 2000;
            } else if ((m_nNextCommand == 2 || m_nNextCommand == 3 || (m_nLastCommand == 3 && m_nBurstShots > 0)) && m_bInRangeToShoot) {
                a->SetCurrentTime(t26);
                m_pAnimAssoc->m_Flags |= ANIMATION_IS_PLAYING;
                if (m_nNextCommand > m_nLastCommand) {
                    m_nLastCommand = m_nNextCommand;
                }
                m_nNextCommand = 0;
                if (m_nLastCommand == 3) {
                    m_nBurstShots--;
                } else {
                    m_nBurstShots = 0;
                }
            } else if (m_nNextCommand == 1) {
                a->SetCurrentTime(t26);
                m_pAnimAssoc->m_Flags &= ~ANIMATION_IS_PLAYING;
                m_nLastCommand  = 1;
                m_nNextCommand  = 0;
                m_nAttackTimer  = -1;
            }
        }
    }

    if (m_nLastCommand != 0 && m_nLastCommand < 4 && m_pAnimAssoc) {
        const auto* const a = m_pAnimAssoc;
        if (a->m_BlendAmount > 0.5f
            || (a->m_BlendDelta > 0.f && !(a->m_Flags & ANIMATION_IS_PLAYING))
        ) {
            ProcessAimIK(ped); // 0x628350
        }
    }

    // x87: Time step * 0.02 (0x858B38) * 1000.0 (0x858C4C) is truncated
    const auto TimeStepMS = [] { return (int32)((double)CTimer::GetTimeStep() * (double)0.02f * (double)1000.0f); };

    if (m_nAttackTimer < 0) {
        m_nAttackTimer = (int16)CGeneral::GetRandomNumberInRange(200, (100 - m_nFrequencyPercentage) * 100);
    } else {
        m_nAttackTimer = (int16)(m_nAttackTimer - TimeStepMS());
    }

    if (ped->IsPlayer() && m_nNextCommand < 4) {
        m_nAttackTimer = 0;
        m_nNextCommand = (char)((static_cast<CPlayerPed*>(ped)->GetPadFromPlayer()->GetWeapon(ped) != 0) + 1);
    } else if (m_nNextCommand == 0) {
        switch ((uint32)(int32)m_nLastCommand) {
        case 1:
            if (m_nFrequencyPercentage == 0 && m_bInWeaponRange) {
                m_nNextCommand = 1;
            } else if (m_bInRangeToShoot) {
                m_nNextCommand = 3;
            } else if (m_nAttackTimer <= 0) {
                m_nAttackTimer = (int16)(TimeStepMS() << 1);
            }
            break;
        case 6:
        case 7:
            m_nNextCommand = 7;
            break;
        default:
            if (m_bInWeaponRange) {
                m_nNextCommand = 1;
            }
            break;
        }
    }

    if (m_nLastCommand != 4 && !m_pAnimAssoc && (m_nLastCommand == 7 || m_nNextCommand == 7)) {
        m_bIsFinished = true;
    }

    if (ped->IsPlayer()) {
        ped->GetActiveWeapon().Update(ped); // 0x73DB40
    }

    // x87: Sum of squares stays in extended precision
    const auto& spd = ped->m_pVehicle->m_vecMoveSpeed; // BUG: Not checked for null (checked above, but `ProcessPlayerPed`/`ProcessAIPed` might have changed it)
    const double speedSq = (double)spd.x * spd.x + (double)spd.y * spd.y;
    if (speedSq > (double)0.5f) { // 0x858B8C
        ped->Say(CTX_GLOBAL_CAR_DRIVEBY_TOO_FAST, 0, 1.f, false, false, false); // 0x1F
        return false;
    }
    if (speedSq < (double)0.01f) { // 0x858C58
        ped->Say(CTX_GLOBAL_CAR_DRIVEBY_BURN_RUBBER, 0, 1.f, false, false, false); // 0x1E
    }
    return false;
}

// 0x627CC0
void CTaskSimpleGangDriveBy::FireGun(CPed* ped) {
    // Muzzle of the gun, in world space
    CVector muzzle = m_pWeaponInfo->m_vecFireOffset;
    {
        const auto hier = GetAnimHierarchyFromSkinClump(ped->GetRpClump());
        const auto idx  = RpHAnimIDGetIndex(hier, ped->m_apBones[PED_NODE_RIGHT_HAND]->BoneTag);
        RwV3dTransformPoints(&muzzle, &muzzle, 1, &RpHAnimHierarchyGetMatrixArray(hier)[idx]);
    }

    auto* const veh      = ped->m_pVehicle;
    const auto& vehBox   = CModelInfo::GetModelInfo(veh->m_nModelIndex)->GetColModel()->m_boundBox;
    const auto& vehMat   = *veh->m_matrix; // BUG: Not checked for null (but vehicles should always have one)
    const auto& vehRight = vehMat.GetRight();
    const auto& vehFwd   = vehMat.GetForward();

    // Muzzle relative to the vehicle
    const CVector rel = muzzle - veh->GetPosition();

    // Same as `MultiplyMatrixWithVector` (0x59C890)
    const auto TransformPoint = [](const CMatrix& m, const CVector& v) {
        const auto &mr = m.GetRight(), &mf = m.GetForward(), &mu = m.GetUp(), &mp = m.GetPosition();
        return CVector{
            (float)(mu.x * (double)v.z + mf.x * (double)v.y + mr.x * (double)v.x + mp.x),
            (float)(mu.y * (double)v.z + mr.y * (double)v.x + mf.y * (double)v.y + mp.y),
            (float)(mu.z * (double)v.z + mr.z * (double)v.x + mf.z * (double)v.y + mp.z)
        };
    };
    const auto EnsureMatrix = [](CEntity* e) -> const CMatrix& {
        if (!e->m_matrix) {
            e->AllocateMatrix();
            e->m_placement.UpdateMatrix(e->m_matrix);
        }
        return *e->m_matrix;
    };

    // Direction we want to shoot in
    CVector dir;
    if (ped->IsPlayer()) {
        float dist = 20.f; // 0x41A00000
        auto& cam  = TheCamera.GetActiveCam();
        if (cam.m_nMode == MODE_TWOPLAYER_IN_CAR_AND_SHOOTING) {
            const auto&  pos = ped->GetPosition();
            const double dx  = (double)pos.x - cam.m_vecSource.x;
            const double dy  = (double)pos.y - cam.m_vecSource.y;
            const double dz  = (double)pos.z - cam.m_vecSource.z;
            dist             = (float)(std::sqrt((dz * dz + dy * dy) + dx * dx) + 20.0); // 0x858BA4
        } else if (cam.m_nMode != MODE_AIMWEAPON_FROMCAR) {
            return;
        }
        CVector camPos; // Unused
        TheCamera.Find3rdPersonCamTargetVector(dist, muzzle, camPos, dir);
        dir -= muzzle;
        ped->GetPlayerData()->m_fAttackButtonCounter = 0.f;
    } else if (m_pTargetEntity) {
        dir = TransformPoint(EnsureMatrix(m_pTargetEntity), m_vecCoords) - muzzle;
    } else {
        dir = m_vecCoords - muzzle;
    }
    dir.Normalise();

    const auto IsAnim = [&](AnimationId a, AnimationId b) {
        return m_pAnimAssoc && (m_pAnimAssoc->m_AnimId == a || m_pAnimAssoc->m_AnimId == b);
    };

    // Point on the vehicle's bounding box (the side we're shooting from), that's in `dir` from the muzzle
    constexpr float MIN_DOT = 0.1f; // 0x858B1C
    constexpr float MARGIN  = 0.2f; // 0x858CC4
    CVector         aim     = muzzle;
    if (IsAnim(ANIM_ID_DRIVEBYLHS, ANIM_ID_DRIVEBYTOP_RHS) || m_nFakeShootDirn == 1) {
        const double v = -(((double)dir.z * vehRight.z + (double)dir.y * vehRight.y) + (double)dir.x * vehRight.x);
        if (v > MIN_DOT) {
            const double relR = ((double)rel.y * vehRight.y + (double)rel.z * vehRight.z) + (double)rel.x * vehRight.x;
            const double s    = (((double)vehBox.m_vecMin.x - relR) - MARGIN) / v;
            aim = CVector{
                (float)(dir.x * s + muzzle.x),
                (float)((double)(float)(dir.y * s) + muzzle.y),
                (float)((double)(float)(dir.z * s) + muzzle.z),
            };
        }
    } else if (IsAnim(ANIM_ID_DRIVEBYRHS, ANIM_ID_DRIVEBYTOP_LHS) || m_nFakeShootDirn == 3) {
        const double v = ((double)dir.z * vehRight.z + (double)dir.y * vehRight.y) + (double)dir.x * vehRight.x;
        if (v > MIN_DOT) {
            const double relR = ((double)rel.y * vehRight.y + (double)rel.z * vehRight.z) + (double)rel.x * vehRight.x;
            const double s    = (((double)vehBox.m_vecMax.x - relR) + MARGIN) / v;
            aim = CVector{
                (float)(dir.x * s + muzzle.x),
                (float)((double)(float)(dir.y * s) + muzzle.y),
                (float)((double)(float)(dir.z * s) + muzzle.z),
            };
        }
    } else if (IsAnim(ANIM_ID_DRIVEBYLHS_BWD, ANIM_ID_DRIVEBYRHS_BWD) || m_nFakeShootDirn == 2) {
        const double v = -(((double)dir.z * vehFwd.z + (double)dir.y * vehFwd.y) + (double)dir.x * vehFwd.x);
        if (v > MIN_DOT) {
            const double relF = ((double)rel.y * vehFwd.y + (double)rel.z * vehFwd.z) + (double)rel.x * vehFwd.x;
            const auto   s    = (float)((((double)vehBox.m_vecMin.y - relF) - MARGIN) / v);
            aim               = muzzle + dir * s;
        }
    } else {
        const double v = ((double)dir.z * vehFwd.z + (double)dir.y * vehFwd.y) + (double)dir.x * vehFwd.x;
        if (v > MIN_DOT) {
            const double relF = ((double)rel.y * vehFwd.y + (double)rel.z * vehFwd.z) + (double)rel.x * vehFwd.x;
            const auto   s    = (float)((((double)vehBox.m_vecMax.y - relF) + MARGIN) / v);
            aim               = muzzle + dir * s;
        }
    }

    ped->m_pedIK.bGunReachedTarget = true;

    auto& weapon = ped->GetActiveWeapon();
    if (m_vecCoords.x == 0.f && m_vecCoords.y == 0.f && m_vecCoords.z == 0.f) {
        weapon.Fire(ped, &muzzle, &muzzle, m_pTargetEntity, nullptr, &aim);
    } else {
        CVector targetPos = m_vecCoords;
        if (m_pTargetEntity) {
            targetPos = TransformPoint(EnsureMatrix(m_pTargetEntity), targetPos);
        }
        weapon.Fire(ped, &muzzle, &muzzle, m_pTargetEntity, &targetPos, &aim);
    }
    ped->DoGunFlash(StaticRef<uint16>(0x8D2E90), false); // Flash lifetime (a .data global in the original, initialised to 250)
}
