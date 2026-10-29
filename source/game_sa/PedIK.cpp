/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"

static inline NOTSA_GLOBAL(bRotateWithNeck, 0x8D2354, (bool), { true });

namespace {
// The exe tests `v > max` first, then `v < min` (0x5FDAB5..): unlike std::clamp this is well defined (and asserts nothing) when min > max
float ClampMaxFirst(float v, float lo, float hi) {
    if (v > hi) {
        return hi;
    }
    if (v < lo) {
        return lo;
    }
    return v;
}
}

void CPedIK::InjectHooks() {
    RH_ScopedClass(CPedIK);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x5FD8C0);
    RH_ScopedInstall(RotateTorso, 0x5FDDB0);
    RH_ScopedInstall(RotateTorsoForArm, 0x5FDF90);
    RH_ScopedOverloadedInstall(MoveLimb, "non-normalized", 0x5FDA60, MoveLimbResult(CPedIK::*)(LimbOrientation&, float, float, LimbMovementInfo&));
    RH_ScopedOverloadedInstall(MoveLimb, "normalized", 0x5FDB60, MoveLimbResult(CPedIK::*)(LimbOrientation&, float, float, LimbMovementInfo&, float));
    RH_ScopedInstall(PointGunAtPosition, 0x5FDE20);
    RH_ScopedInstall(PointGunInDirection, 0x5FDC00);
    RH_ScopedInstall(PitchForSlope, 0x5FE0E0);
    RH_ScopedInstall(GetWorldMatrix, 0x5FD8F0);
}

// 0x5FDDB0
void CPedIK::RotateTorso(AnimBlendFrameData* bone, LimbOrientation& orientation, bool changeRoll) {
    const auto q = &bone->KeyFrame->q;
    // exe 0x5FDDB0: the yaw rotation replaces the keyframe quaternion unless `changeRoll` (then PRECONCAT, 0x5FDDD0); the final flag is
    // bUpdateMatricesRequired (dword 0x470 |= 0x4000, 0x5FDE0E), not bDontAcceptIKLookAts
    RtQuatRotate(q, &XaxisIK, RadiansToDegrees(orientation.m_fYaw), changeRoll ? rwCOMBINEPRECONCAT : rwCOMBINEREPLACE);
    RtQuatRotate(q, &ZaxisIK, RadiansToDegrees(orientation.m_fPitch), rwCOMBINEPRECONCAT);
    m_pPed->bUpdateMatricesRequired = true;
}

// 0x5FDF90
void CPedIK::RotateTorsoForArm(const CVector& direction) {
    auto destRotation = CVector2D{direction - m_pPed->GetPosition()}.Heading();

    // this might be inlined
    if (m_pPed->m_fCurrentRotation + DegreesToRadians(180.0f) < destRotation) {
        destRotation -= DegreesToRadians(360.0f);
    } else if (m_pPed->m_fCurrentRotation - DegreesToRadians(180.0f) > destRotation) {
        destRotation += DegreesToRadians(360.0f);
    }

    const auto difAngle = destRotation - m_pPed->m_fCurrentRotation;
    auto resultAngle = DegreesToRadians(45.0f);

    if (difAngle > DegreesToRadians(45.0f) && difAngle <= DegreesToRadians(90.0f)) {
        resultAngle = difAngle - DegreesToRadians(45.0f);
    } else {
        if (difAngle >= DegreesToRadians(-60.0f))
            return;

        resultAngle = difAngle + DegreesToRadians(60.0f);

        if (difAngle < DegreesToRadians(-80.0f)) {
            resultAngle = DegreesToRadians(-20.0f);
        }
    }

    if (resultAngle != DegreesToRadians(0.0f)) {
        const auto degreesHalf = RadiansToDegrees(resultAngle / 2.0f);
        if (bRotateWithNeck) { // android doesn't have this check
            RtQuatRotate(&m_pPed->m_apBones[PED_NODE_NECK]->KeyFrame->q, &XaxisIK, degreesHalf, rwCOMBINEPOSTCONCAT);
        }
        RtQuatRotate(&m_pPed->m_apBones[PED_NODE_UPPER_TORSO]->KeyFrame->q, &XaxisIK, degreesHalf, rwCOMBINEPOSTCONCAT);
    }
}

// 0x5FDC00
bool CPedIK::PointGunInDirection(float zAngle, float distance, bool flag, float normalize) {
    bGunReachedTarget = false;
    bTorsoUsed = true;

    const auto angle = CGeneral::LimitRadianAngle(zAngle - m_pPed->m_fCurrentRotation);
    const auto hier  = GetAnimHierarchyFromSkinClump(m_pPed->GetRpClump());
    const auto index = RpHAnimIDGetIndex(hier, m_pPed->m_apBones[PED_NODE_RIGHT_CLAVICLE]->BoneTag);

    // unused code
    // auto* boneMatrix = RwMatrixCreate();
    // *boneMatrix = RpHAnimHierarchyGetMatrixArray(hier)[index];
    // RwMatrixDestroy(boneMatrix);

    const auto limbResult = [&]() -> MoveLimbResult {
        if (normalize < 0.0f) {
            return MoveLimb(m_TorsoOrient, angle, distance, ms_torsoInfo);
        } else {
            return MoveLimb(m_TorsoOrient, angle, distance, ms_torsoInfo, normalize);
        }
    }();

    bool canReach = true;
    switch (limbResult) {
    case CANT_REACH_TARGET:
        canReach = false;
        break;
    case REACHED_TARGET:
        bGunReachedTarget = true;
        break;
    }

    const auto head = &RpHAnimHierarchyGetMatrixArray(hier)[PED_NODE_HEAD];
    const auto headAngle = -CGeneral::LimitRadianAngle(X87R(x87::atan2(-head->at.y, -head->at.x) - m_pPed->m_fCurrentRotation));

    const auto axis = CVector{
        0.0f,
        (float)(flag ? x87::cos(headAngle) : -x87::sin(headAngle)),
        (float)(flag ? x87::sin(headAngle) :  x87::cos(headAngle))
    };

    const auto torsoQ = &m_pPed->m_apBones[PED_NODE_UPPER_TORSO]->KeyFrame->q;
    RtQuatRotate(torsoQ, &axis, RadiansToDegrees(m_TorsoOrient.m_fPitch), rwCOMBINEPOSTCONCAT);
    RtQuatRotate(torsoQ, &XaxisIK, RadiansToDegrees(m_TorsoOrient.m_fYaw), rwCOMBINEPOSTCONCAT);
    m_pPed->bUpdateMatricesRequired = true;

    return canReach;
}

// 0x5FDE20
void CPedIK::PointGunAtPosition(const CVector& aimAt, float normalize) {
    const auto& offset = CWeaponInfo::GetWeaponInfo(m_pPed)->GetAimingOffset();
    const auto aimFrom = m_pPed->GetPosition()
        + (m_pPed->bIsDucking ? offset.DuckX : offset.AimX) * m_pPed->GetRight()
        + (m_pPed->bIsDucking ? offset.DuckZ : offset.AimZ) * m_pPed->GetUp();
    PointGunInDirection(
        CGeneral::GetRadianAngleBetweenPoints(aimAt.x, aimAt.y, aimFrom.x, aimFrom.y),
        CGeneral::GetRadianAngleBetweenPoints(
            aimAt.z, CVector2D::Dist(aimFrom, aimAt),
            aimFrom.z, 0.0f
        ),
        false,
        normalize
    );
}

// 0x5FE0E0
void CPedIK::PitchForSlope() {
    const auto clumpData = RpAnimBlendClumpGetData(m_pPed->GetRpClump());
    const auto hier = GetAnimHierarchyFromSkinClump(m_pPed->GetRpClump());

    if (std::abs(m_fBodyRoll) > 0.01f) {
        m_fBodyRoll = std::clamp(m_fBodyRoll, DegreesToRadians(-30.0f), DegreesToRadians(30.0f));
    }

    // 0x5FE0E0: the pitch / roll clamps depend on the ped state alone (0x5FE122 cmp 0x36 / 0x37), but the branch choice below
    // (register bl, 0x5FE120..0x5FE13A) is "dying OR bFallenDown" (flags dword 0x474 bit 20): a knocked-down ped is tilted like a corpse
    const bool isDying = m_pPed->IsStateDying();
    if (!isDying) {
        if (std::abs(m_fSlopePitch) > 0.01f) {
            const auto offset = (m_pPed->m_nMoveState != PEDMOVE_WALK) ? 0.1f : 0.085f;
            m_fSlopePitchLimitMult = (m_fSlopePitchLimitMult * 0.9f) + offset;

            const auto lowBound  = m_fSlopePitchLimitMult * DegreesToRadians(-35.0f);
            const auto highBound = m_fSlopePitchLimitMult * DegreesToRadians(+30.0f);
            m_fSlopePitch = std::clamp(m_fSlopePitch, lowBound, highBound);
        }

        if (std::abs(m_fSlopeRoll) > 0.01f) {
            m_fSlopeRoll = std::clamp(m_fSlopeRoll, DegreesToRadians(-45.0f), DegreesToRadians(45.0f));
        }
    }

    if (!isDying && !m_pPed->bFallenDown) {
        const auto GetAnimHierarchyMatrix = [&hier](eBoneTag bone) {
            return &RpHAnimHierarchyGetMatrixArray(hier)[RpHAnimIDGetIndex(hier, bone)];
        };

        const auto RotateBone = [clumpData, &hier](eBoneTag bone, float angle, const CVector& axis = ZaxisIK, RwOpCombineType combine = rwCOMBINEPRECONCAT) {
            RtQuatRotate(&clumpData->m_FrameDatas[RpHAnimIDGetIndex(hier, bone)].KeyFrame->q, &axis, angle, combine);
        };

        if (std::abs(m_fSlopePitch) > 0.01f) {
            if (const auto* const activeTask = m_pPed->GetIntelligence()->GetTaskManager().GetActiveTask(); activeTask && activeTask->GetTaskType() == TASK_SIMPLE_STEALTH_KILL) { // 0x5FE3A2: GetActiveTask() == null takes the normal branch
                RotateBone(BONE_SPINE, RadiansToDegrees(m_fSlopePitch));
                m_pPed->bUpdateMatricesRequired = true;
            } else {
                RotateBone(BONE_SPINE, RadiansToDegrees(-m_fSlopePitch) * 0.75f);
                m_pPed->bUpdateMatricesRequired = true;

                // 0x5FE443..0x5FE670: both legs are driven by the asin of the THIGH's matrix `up.z` alone (stored as a float, [esp+0x10]) and rotated with rwCOMBINEPOSTCONCAT
                // (`push 2` before every RtQuatRotate of the legs; the spine uses `push 1`). (The old port used the foot's matrix for the foot, negated the left foot and applied
                // the max() to the wrong factor.) The byte at 0xC0AFC0 that selects the fabs() variant of the thigh angle is never written (always 0).
                const auto Leg = [&](eBoneTag thigh, eBoneTag calf, eBoneTag foot) {
                    const float  asinUp = (float)x87::asin((double)GetAnimHierarchyMatrix(thigh)->up.z);
                    const x87d pitch  = (x87d)m_fSlopePitch;

                    RotateBone(thigh, (float)(((x87d)30.0 * pitch) * (x87d)asinUp), ZaxisIK, rwCOMBINEPOSTCONCAT); // 0x86C8A0 = 30

                    x87d calfVal = -((x87d)asinUp * pitch);
                    if (0.0 > calfVal) { // fcomp + `test ah, 0x41; jne`: a NaN keeps the value
                        calfVal = (x87d)0.0;
                    }
                    RotateBone(calf, (float)(calfVal * -(x87d)30.0), ZaxisIK, rwCOMBINEPOSTCONCAT); // 0x86C89C = -30

                    const x87d footVal = (0.0 > (double)asinUp) ? (x87d)0.0 : (x87d)asinUp;
                    RotateBone(foot, (float)((footVal * pitch) * -(x87d)30.0), ZaxisIK, rwCOMBINEPOSTCONCAT); // 0x86C8A4 = -30
                };
                Leg(BONE_R_THIGH, BONE_R_CALF, BONE_R_FOOT);
                Leg(BONE_L_THIGH, BONE_L_CALF, BONE_L_FOOT);

                CMatrix result(m_pPed->GetModellingMatrix(), false);
                CMatrix translate;
                translate.SetTranslate(CVector{0.0f, 0.0f, 0.0f});
                translate.RotateX(-m_fSlopePitch / 2.0f);
                result *= translate;

                result.GetPosition() += m_pPed->GetForward() * 0.75f * x87::sin(m_fSlopePitch);
                result.GetPosition().z += X87R(0.3f / x87::cos(m_fSlopeRoll)) - 0.3f;
                result.UpdateRW();
                m_pPed->UpdateRwFrame();
            }
        } else {
            m_fSlopePitch = 0.0f;
        }

        if (std::abs(m_fSlopeRoll) > 0.01f) {
            const auto RotateFoot = [&](eBoneTag bone) {
                const auto hierMatrix = GetAnimHierarchyMatrix(bone);
                const auto angle = CGeneral::LimitRadianAngle(X87R(x87::atan2(hierMatrix->at.y, hierMatrix->at.x) - m_pPed->m_fCurrentRotation));

                RotateBone(bone, RadiansToDegrees(m_fSlopeRoll), CVector{0.0f, (float)(x87::cos(angle)), (float)(x87::sin(angle))});
            };

            RotateFoot(BONE_L_FOOT);
            RotateFoot(BONE_R_FOOT);
            m_pPed->bUpdateMatricesRequired = true;
        } else {
            m_fSlopeRoll = 0.0f;
        }

    } else {
        CMatrix matResult(m_pPed->GetModellingMatrix());

        CMatrix matTransl;
        matTransl.SetTranslate(ZaxisIK);
        matTransl.RotateX(-m_fSlopePitch);
        matTransl.RotateY(m_fSlopeRoll);
        matResult *= matTransl;
        matResult.GetPosition().z -= 1.0f;
        matResult.UpdateRW();
        m_pPed->UpdateRwFrame();
    }

    bSlopePitch = false;
}

// unused
// 0x5FD8F0
RwMatrixTag* CPedIK::GetWorldMatrix(RwFrame* frame, RwMatrixTag* transformMat) {
    *transformMat = *RwFrameGetMatrix(frame);
    for (frame = RwFrameGetParent(frame); frame; frame = RwFrameGetParent(frame)) {
        RwMatrixTransform(transformMat, RwFrameGetMatrix(frame), rwCOMBINEPOSTCONCAT);
    }
    return transformMat;
}

// 0x5FDA60
MoveLimbResult CPedIK::MoveLimb(LimbOrientation& limb, float targetYaw, float targetPitch, LimbMovementInfo& moveInfo) {
    auto result = HAVENT_REACHED_TARGET;

    // yaw
    if (std::abs(limb.m_fYaw - targetYaw) < moveInfo.yawD) {
        limb.m_fYaw = targetYaw;
        result = REACHED_TARGET;
    } else {
        if (limb.m_fYaw > targetYaw) {
            limb.m_fYaw -= moveInfo.yawD;
        } else if (limb.m_fYaw < targetYaw) {
            limb.m_fYaw += moveInfo.yawD;
        }
    }

    if (limb.m_fYaw > moveInfo.maxYaw || limb.m_fYaw < moveInfo.minYaw) {
        limb.m_fYaw = ClampMaxFirst(limb.m_fYaw, moveInfo.minYaw, moveInfo.maxYaw);
        result = CANT_REACH_TARGET;
    }

    // pitch
    if (std::abs(limb.m_fPitch - targetPitch) < moveInfo.pitchD) {
        limb.m_fPitch = targetPitch;
    } else {
        // 0x5FDAF1: the result is only reset when the pitch actually moved (not for a NaN pitch / a non-positive step)
        if (limb.m_fPitch < targetPitch) {
            limb.m_fPitch += moveInfo.pitchD;
            result = HAVENT_REACHED_TARGET;
        } else if (limb.m_fPitch > targetPitch) {
            limb.m_fPitch -= moveInfo.pitchD;
            result = HAVENT_REACHED_TARGET;
        }
    }

    if (limb.m_fPitch > moveInfo.maxPitch || limb.m_fPitch < moveInfo.minPitch) {
        limb.m_fPitch = ClampMaxFirst(limb.m_fPitch, moveInfo.minPitch, moveInfo.maxPitch);
        result = CANT_REACH_TARGET;
    }

    return result;
}

// 0x5FDB60
MoveLimbResult CPedIK::MoveLimb(LimbOrientation& limb, float targetYaw, float targetPitch, LimbMovementInfo& moveInfo, float normalize) {
    auto result = HAVENT_REACHED_TARGET;

    limb.m_fYaw = normalize * targetYaw;
    limb.m_fPitch = normalize * targetPitch;

    if (limb.m_fYaw > moveInfo.maxYaw || limb.m_fYaw < moveInfo.minYaw) {
        limb.m_fYaw = ClampMaxFirst(limb.m_fYaw, moveInfo.minYaw, moveInfo.maxYaw);
        result = CANT_REACH_TARGET;
    }

    if (limb.m_fPitch > moveInfo.maxPitch || limb.m_fPitch < moveInfo.minPitch) {
        limb.m_fPitch = ClampMaxFirst(limb.m_fPitch, moveInfo.minPitch, moveInfo.maxPitch);
        result = CANT_REACH_TARGET;
    } else if (normalize > 0.9f && result == HAVENT_REACHED_TARGET) {
        result = REACHED_TARGET;
    }

    return result;
}
