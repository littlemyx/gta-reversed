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

void CTaskSimpleGangDriveBy::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleGangDriveBy, 0x86D944, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(FireGun, 0x627CC0);
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

// Not reversed yet (called by `ProcessPed`/`MakeAbortable`)
bool CTaskSimpleGangDriveBy::StartDriveByAnim(CPed* ped) { // 0x627B20 (name guessed)
    return plugin::CallMethodAndReturn<bool, 0x627B20, CTaskSimpleGangDriveBy*, CPed*>(this, ped);
}

void CTaskSimpleGangDriveBy::ProcessPlayerPed(CPed* ped) { // 0x621960 (name guessed)
    plugin::CallMethod<0x621960, CTaskSimpleGangDriveBy*, CPed*>(this, ped);
}

void CTaskSimpleGangDriveBy::ProcessAIPed(CPed* ped) { // 0x627600 (name guessed)
    plugin::CallMethod<0x627600, CTaskSimpleGangDriveBy*, CPed*>(this, ped);
}

void CTaskSimpleGangDriveBy::ProcessAimIK(CPed* ped) { // 0x628350 (name guessed)
    plugin::CallMethod<0x628350, CTaskSimpleGangDriveBy*, CPed*>(this, ped);
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
