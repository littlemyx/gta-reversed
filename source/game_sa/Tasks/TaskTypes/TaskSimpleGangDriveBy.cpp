#include "StdInc.h"

#include "TaskSimpleGangDriveBy.h"

#include "Camera.h"
#include "WeaponInfo.h"
#include "PlayerPedData.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Enums/eCamMode.h"

void CTaskSimpleGangDriveBy::InjectHooks() {
    RH_ScopedClass(CTaskSimpleGangDriveBy);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(FireGun, 0x627CC0);
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

CTask* CTaskSimpleGangDriveBy::Clone() const {
    return plugin::CallMethodAndReturn<CTask*, 0x6236D0, const CTask*>(this);
}

bool CTaskSimpleGangDriveBy::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event)
{
    return plugin::CallMethodAndReturn<bool, 0x62D290, CTask*, CPed*, int32, const CEvent*>(this, ped, priority, event);
}

bool CTaskSimpleGangDriveBy::ProcessPed(CPed* ped)
{
    return plugin::CallMethodAndReturn<bool, 0x62D3B0, CTask*, CPed*>(this, ped);
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
