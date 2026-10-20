#include "StdInc.h"

#include "IKChain.h"
#include "BoneNodeManager.h"

#include "Enums/eIKChainSlot.h"

void IKChain_c::InjectHooks() {
    RH_ScopedClass(IKChain_c);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x618370);
    RH_ScopedInstall(Exit, 0x617870);
    RH_ScopedInstall(Update, 0x6184B0);
    RH_ScopedInstall(MoveBonesToTarget, 0x6178B0);
    RH_ScopedInstall(SetBlend, 0x617C10);
    RH_ScopedInstall(SetOffsetBoneTag, 0x617C20);
    RH_ScopedInstall(SetOffsetPos, 0x617C30);
    RH_ScopedInstall(ClampLimits, 0x618520);
    RH_ScopedInstall(GetLimits, 0x618590);
    RH_ScopedInstall(SetLimits, 0x6185E0);
    RH_ScopedInstall(GetBoneSpeed, 0x618630);
    RH_ScopedInstall(SetBoneSpeed, 0x618680);
    RH_ScopedInstall(GetPriority, 0x617C50);
    RH_ScopedInstall(GetBoneNodeFromTag, 0x617C60);
    RH_ScopedInstall(SetupBones, 0x617CA0);
    RH_ScopedInstall(UpdateEntity, 0x617E00);
    RH_ScopedInstall(UpdateOffset, 0x617E20);
    RH_ScopedInstall(UpdateTarget, 0x617E50);
    RH_ScopedInstall(IsFacingTarget, 0x617E60);
    RH_ScopedInstall(IsAtTarget, 0x617F30);
}

// 0x618370
bool IKChain_c::Init(
    const char*  name, // unused
    eIKChainSlot ikSlot,
    CPed*        ped,
    eBoneTag32   effectorBone,
    CVector        effectorPos,
    eBoneTag32   pivotBone,
    CEntity*     entity,
    eBoneTag32   offsetBone,
    CVector        offsetPos,
    float        speed,
    int32        priority
) {
    m_Ped = ped;

    const auto frames = m_Ped->GetAnimBlendData().GetFrames();

    if (frames[0].NeedsKeyFrameUpdate || !frames[0].IsUpdatingFrame) {
        return false;
    }

    if (!m_Ped->bHasBeenRendered) { // Bone data isn't present yet in this case
        return false;
    }

    // Check if frame of this bone has non-zero translation
    {
        const auto& boneFrame = frames[RpHAnimIDGetIndex(&m_Ped->GetAnimHierarchy(), (RwInt32)effectorBone)].KeyFrame;
        if (boneFrame->t.IsZero()) {
            return false;
        }
    }

    SetupBones(effectorBone, effectorPos, pivotBone, frames);
    UpdateEntity(entity);

    m_EffectorPos  = effectorPos;
    m_EffectorBone = effectorBone;
    m_OffsetBone   = offsetBone;
    m_OffsetPos    = offsetPos;
    m_Speed        = speed;
    m_UpdateTarget = true;
    m_IKSlot       = ikSlot;
    m_Priority     = priority;

    return true;
}

// 0x617870
void IKChain_c::Exit() {
    for (auto&& bone : GetBones()) {
        g_boneNodeMan.ReturnBoneNode(bone);
    }
    delete[] std::exchange(m_Bones, nullptr);
}

// 0x6184B0
void IKChain_c::Update(float deltaTime) {
    UNUSED(deltaTime);

    m_PivotBoneMatrix = m_Ped->GetBoneMatrix(m_PivotBone);
    GetBones()[m_BonesCount - 1]->CalcWldMat(m_PivotBoneMatrix);
    MoveBonesToTarget();
    for (auto& bone : GetBones()) {
        bone->BlendKeyframe(m_Blend);
    }
}

// 0x617F30
bool IKChain_c::IsAtTarget(float range, float* outDist) const {
    // 0x617F68: the distance is RwV3dLength (0x7EDAC0, RenderWare's table-based approximate sqrt, relative error ~1e-4), NOT an exact sqrt (CVector::Magnitude)
    const CVector diff = m_OffsetPosWS - GetBones().front()->GetPosition();
    const float   len  = RwV3dLength(&diff);
    if (outDist) {
        *outDist = len;
    }
    return len <= range && m_Blend > 0.98f;
}

// 0x617E60
bool IKChain_c::IsFacingTarget() const {
    const auto root = GetBones().front();

    // targetPos = normalize(effectorPos transformed by the root bone's matrix)
    CVector targetPos;
    RwV3dTransformVector(&targetPos, &m_EffectorPos, &root->GetMatrix());
    RwV3dNormalize(&targetPos, &targetPos);

    // 0x617E6B..: dir = normalize(offsetPosWS - rootBonePos) (the vector at root bone + 0x6C = matrix pos),
    // NOT `offsetPosWS - targetPos` (the port subtracted the transformed effector): with that dir the dot with
    // targetPos was almost never >= 0.95, so `bGunReachedTarget` stayed false and AI gunmen never hit anything (U7/U16)
    const auto rootPos = root->GetPosition();
    CVector    dir     = m_OffsetPosWS - rootPos;
    RwV3dNormalize(&dir, &dir);

    // x87: (ty * dy + tx * dx) + tz * dz in extended precision
    const auto dot = ((double)targetPos.y * (double)dir.y + (double)targetPos.x * (double)dir.x) + (double)targetPos.z * (double)dir.z;
    return dot >= 0.95f && m_Blend > 0.98f;
}

// 0x617E50
void IKChain_c::UpdateTarget(bool update) {
    m_UpdateTarget = update;
}

// 0x617E20
void IKChain_c::UpdateOffset(eBoneTag32 offsetBone, CVector offsetPos) {
    m_OffsetBone = offsetBone;
    m_OffsetPos = offsetPos;
}

// 0x618520
void IKChain_c::ClampLimits(eBoneTag32 boneTag, bool limitX, bool limitY, bool limitZ, bool current) {
    auto& boneNode = *GetBoneNodeFromTag(boneTag);
    if (current) {
        boneNode.ClampLimitsCurrent(limitX, limitY, limitZ);
    } else {
        boneNode.ClampLimitsDefault(limitX, limitY, limitZ);
    }
}

// 0x617E00
void IKChain_c::UpdateEntity(CEntity* entity) {
    // NOTE: Original code didn't un-register the ref, but we're going to do that here (Or well, the Ref wrapper does it for us)
    m_TargetEntity = entity;
}

// inline
// 0x617C60
inline BoneNode_c* IKChain_c::GetBoneNodeFromTag(eBoneTag32 boneTag) {
    for (auto&& bone : GetBones()) {
        if (bone->GetBoneTag().get_underlying() == boneTag.get_underlying()) { // 0x617C77 `cmp [ebx+8], edi`: all 32 bits (a WEnum->enum compare would truncate to 16 bits)
            return bone;
        }
    }
    NOTSA_LOG_WARN("IKChain_c::GetBoneNodeFromTag: Bone tag {} not found in chain", (int32)(boneTag));
    return nullptr;
}

// 0x617C50
int8 IKChain_c::GetPriority() const {
    return m_Priority;
}

// 0x617C30
void IKChain_c::SetOffsetPos(CVector offsetPos) {
    m_OffsetPos = offsetPos;
}

// 0x617C20
void IKChain_c::SetOffsetBoneTag(eBoneTag32 offsetBone) {
    m_OffsetBone = offsetBone;
}

// 0x617C10
void IKChain_c::SetBlend(float blend) {
    m_Blend = blend;
}

// 0x6178B0
void IKChain_c::MoveBonesToTarget() {
    if (m_UpdateTarget) {
        if (m_TargetEntity) {
            const auto mat = m_TargetEntity->GetModellingMatrix();
            if (m_OffsetBone == BONE_UNKNOWN) {
                if (mat) {
                    RwV3dTransformPoint(&m_OffsetPosWS, &m_OffsetPos, mat);
                }
            } else {
                m_OffsetPosWS = m_TargetEntity->AsPed()->GetBonePosition((eBoneTag)m_OffsetBone);
                if (mat) {
                    CVector transformed;
                    RwV3dTransformVector(&transformed, &m_OffsetPos, mat);
                    m_OffsetPosWS += transformed;
                }
            }
        } else {
            m_OffsetPosWS = m_OffsetPos;
        }
    }

    auto bones = GetBones();
    if (bones.empty()) {
        return;
    }

    constexpr auto DIST_TOLERANCE = 0.00001f; // 0x858C14

    // NOTE: everything below mirrors the x87 code of 0x6178B0 operation by operation (the game runs at PC=24: every instruction rounds to a float mantissa), including the
    // table-based RwV3dLength / RwV3dNormalize (0x7EDAC0 / 0x7ED9B0) and `fcomp` tests that also skip NaN
    const auto rootBone = bones.front();
    for (auto&& bone : bones) {
        CVector transformedBonePos;
        RwV3dTransformVector(&transformedBonePos, &m_EffectorPos, &rootBone->GetMatrix()); // Not sure why it's transformed every iteration..

        // 0x6179B4: (t + rootPos) - bonePos, NOT t + (rootPos - bonePos)
        const auto& rootPos = rootBone->GetPosition();
        const auto& bonePos = bone->GetPosition();
        CVector dirBoneToRoot{
            (transformedBonePos.x + rootPos.x) - bonePos.x,
            (transformedBonePos.y + rootPos.y) - bonePos.y,
            (transformedBonePos.z + rootPos.z) - bonePos.z,
        };
        if (!(RwV3dLength(&dirBoneToRoot) > DIST_TOLERANCE)) {
            continue;
        }
        RwV3dNormalize(&dirBoneToRoot, &dirBoneToRoot);

        CVector dirBoneToOffset = m_OffsetPosWS - bone->GetPosition();
        if (!(RwV3dLength(&dirBoneToOffset) > DIST_TOLERANCE)) {
            continue;
        }
        RwV3dNormalize(&dirBoneToOffset, &dirBoneToOffset);

        // 0x617A6D: (Oy*Ry + Ox*Rx) + Oz*Rz (not DotProduct's z, y, x order); `fcomp 0.997; test ah,5; jp`: only a dot strictly below 0.997 continues (NaN skips)
        const float dot = (dirBoneToOffset.y * dirBoneToRoot.y + dirBoneToOffset.x * dirBoneToRoot.x) + dirBoneToOffset.z * dirBoneToRoot.z;
        if (!(dot < 0.997f)) { // Make sure they don't point in the same direction
            continue;
        }

        const auto boneParentMat = bone->GetParent()
            ? &bone->GetParent()->GetMatrix()
            : m_PivotBoneMatrix;

        RtQuat quat;
        RtQuatConvertFromMatrix(&quat, boneParentMat);

        // NOTE: Normalize Quat.. I wonder why there isn't a macro or something for this?
        {
            const auto mag = RwV3dDotProduct(&quat.imag, &quat.imag) + quat.real * quat.real;
            if (mag > 0.f) {
                const auto recp = 1.f / mag;
                quat.real *= recp;
                RwV3dScale(&quat.imag, &quat.imag, -recp);
            }
        }

        // 0x617AC9: R x O with every product stored as a float
        const CVector& R = dirBoneToRoot;
        const CVector& O = dirBoneToOffset;
        CVector cross{ O.z * R.y - O.y * R.z, O.x * R.z - O.z * R.x, O.y * R.x - O.x * R.y };
        CVector axis;
        RtQuatTransformVectors(&axis, &cross, 1, &quat);
        // 0x617AA3: ((speed * acos(dot)) * m_Speed) * (180/pi float), one rounding to float per multiplication (PC=24). The acos result stays unrounded on the x87 stack (FPATAN ignores the
        // precision control: it is NOT a float; only the copy `fst [esp+0x10]` is), so it must not be cast to float before the first multiplication
        const float angle = RadiansToDegrees((float)((double)bone->GetSpeed() * x87::acos(dot)) * m_Speed);
        RtQuatRotate(reinterpret_cast<RtQuat*>(&bone->GetOrientation()), &axis, angle, rwCOMBINEPOSTCONCAT);

        bone->Limit(m_Blend); // Originally this was in an `if`, but the variable that was checked was always `true`
        bone->CalcWldMat(boneParentMat);
    }
}

// 0x617CA0
void IKChain_c::SetupBones(eBoneTag32 effector, CVector effectorPos, eBoneTag32 pivot, AnimBlendFrameData* frames) {
    UNUSED(effectorPos);

    m_PivotBoneMatrix = m_Ped->GetBoneMatrix(pivot);
    m_PivotBone       = pivot;

    const auto GetBoneLinkPrev = [](eBoneTag32 b) {
        const auto* info = BoneNodeManager_c::GetBoneInfoFromTag(b);
        return info
            ? info->ParentBoneTag
            : BONE_UNKNOWN; // BUGFIX: Handle case where `info` is null (Bone tag not found, logging done by `GetBoneInfoFromTag`)
    };

    // Allocate bone nodes we use (Going from effectorBone -> pivotBone)
    BoneNode_c* bones[MAX_BONE_NUM];
    m_BonesCount = 0;
    for (auto boneIt = effector; boneIt != pivot && boneIt != BONE_UNKNOWN; boneIt = GetBoneLinkPrev(boneIt)) { // BUGFIX: Handle `BONE_UNKNOWN` case in loop condition
        auto* const node = g_boneNodeMan.GetBoneNode(); // Allocate new node
        node->Init(boneIt, frames[RpHAnimIDGetIndex(&m_Ped->GetAnimHierarchy(), (RwInt32)boneIt)].KeyFrame);
        bones[m_BonesCount++] = node;
    }

    // Now we know the exact size to allocate, so do that
    m_Bones = new BoneNode_c*[m_BonesCount];
    for (auto i = 0; i < m_BonesCount; i++) {
        m_Bones[i] = bones[i];
    }

    // Link bones together
    for (auto& bone : GetBones()) {
        const auto prevBoneId = GetBoneLinkPrev(bone->GetBoneTag());
        if (prevBoneId == BONE_UNKNOWN) { // This should only really happen with the effector bone (0th index)
            continue;
        }
        const auto prevBoneNode = GetBoneNodeFromTag(prevBoneId);
        if (!prevBoneNode) {
            continue;
        }
        prevBoneNode->AddChild(bone);
    }
}

// unused
// 0x6185E0
void IKChain_c::SetLimits(eBoneTag32 boneTag, eRotationAxis axis, float min, float max) {
    GetBoneNodeFromTag(boneTag)->SetLimits(axis, min, max);
}

// unused
// 0x618630
float IKChain_c::GetBoneSpeed(eBoneTag32 boneTag) {
    return GetBoneNodeFromTag(boneTag)->GetSpeed();
}

// unused
// 0x618680
void IKChain_c::SetBoneSpeed(eBoneTag32 boneTag, float speed) {
    GetBoneNodeFromTag(boneTag)->SetSpeed(speed);
}

// 0x618590
void IKChain_c::GetLimits(eBoneTag32 boneTag, eRotationAxis axis, float& min, float& max) {
    GetBoneNodeFromTag(boneTag)->GetLimits(axis, min, max);
}
