#include "StdInc.h"

#include "TaskSimplePlayHandSignalAnim.h"

#include "Ragdoll/IKChainManager.h"

void CTaskSimplePlayHandSignalAnim::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimplePlayHandSignalAnim, 0x86D5B8, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTInstall(ProcessPed, 0x61BDA0);
    RH_ScopedInstall(StartAnim, 0x61AF60);
}

// 0x61AE50
CTaskSimplePlayHandSignalAnim::CTaskSimplePlayHandSignalAnim(AnimationId animationId, float fBlendFactor, bool bFatHands, bool bHoldLastFrame) : CTaskSimpleAnim(bHoldLastFrame) {
    m_PlayerIdlesAnimBlockId = animationId;
    m_LeftHand = nullptr;
    m_RightHand = nullptr;
    m_AnimBlenDelta = fBlendFactor;
    m_DoUseFatHands = bFatHands;
}

CTaskSimplePlayHandSignalAnim::~CTaskSimplePlayHandSignalAnim() {
#if defined(NOTSA_PORTABLE) || defined(NOTSA_STANDALONE_RUN)
    // MAC-PLAY1 (portable) / gate 71979d2d1 render_vmode (Win32 run build: access violation in CWorld::Remove during CWorld::ShutDown): CWorld::ShutDown deletes the sector lists one by one; a hand object (CObject) can already be gone when the ped that owns this task is deleted
    // (peds are deleted after objects of earlier sectors). The exe then virtual-calls `Remove` through the stale vtable of a freed pool slot, which happens to be harmless
    // on Win32 (the slot keeps a base vtable followed by other code pointers) but is a null call under clang. Skip hands whose pool slot is already free.
    const auto IsAlive = [](CObject* obj) { return GetObjectPool()->IsObjectValid(obj); };
    if (m_LeftHand && !IsAlive(m_LeftHand)) { m_LeftHand = nullptr; }
    if (m_RightHand && !IsAlive(m_RightHand)) { m_RightHand = nullptr; }
#endif
    if (m_LeftHand) {
        CWorld::Remove(m_LeftHand);
        delete m_LeftHand;
        --CObject::nNoTempObjects;
    }

    if (m_RightHand) {
        CWorld::Remove(m_RightHand);
        delete m_RightHand;
        --CObject::nNoTempObjects;
    }
}

// 0x61BDA0
bool CTaskSimplePlayHandSignalAnim::ProcessPed(CPed* ped) {
    if (g_ikChainMan.IsArmPointing(eIKArm::IK_ARM_LEFT, ped) || m_bIsFinished) {
        return true;
    }

    if (!m_pAnim) {
        if (CObject::nNoTempObjects + 1 >= 150) {
            return true;
        }

        CTaskSimplePlayHandSignalAnim::StartAnim(ped);
    }

    return m_bIsFinished;
}

// 0x61AF60
void CTaskSimplePlayHandSignalAnim::StartAnim(CPed* ped) {
    const auto animId = static_cast<AnimationId>(CGeneral::GetRandomNumberInRange((int32)ANIM_ID_GSIGN1, (int32)ANIM_ID_GSIGN5 + 1));

    //
    // Pointing / weapon logic
    //
    if (ped->GetEntityThatThisPedIsHolding() || g_ikChainMan.IsArmPointing(eIKArm::IK_ARM_RIGHT, ped) || ped->GetActiveWeapon().m_Type != eWeaponType::WEAPON_UNARMED) {
        m_pAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_HANDSIGNALL, animId, m_AnimBlenDelta);
        m_pAnim->SetFinishCallback(CTaskSimpleAnim::FinishRunAnimCB, this);

        if (m_PlayerIdlesAnimBlockId == -1) {
            return;
        }

        const auto handIndex = m_DoUseFatHands ? MODEL_FHANDL : MODEL_SHANDL;
        m_LeftHand    = new CHandObject(handIndex, ped, true);
        if (!m_LeftHand) {
            return;
        }

        CWorld::Add(m_LeftHand);
        auto* animHierarchy = CAnimManager::GetAnimAssociation(ANIM_GROUP_LHAND, m_PlayerIdlesAnimBlockId)->m_BlendHier;
        CAnimManager::AddAnimation(m_LeftHand->GetRpClump(), animHierarchy, 0);
        ++CObject::nNoTempObjects;
        return;
    }

    //
    // Both arms logic
    //
    m_pAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_HANDSIGNAL, animId, m_AnimBlenDelta);
    m_pAnim->SetFinishCallback(CTaskSimpleAnim::FinishRunAnimCB, this);
    if (m_PlayerIdlesAnimBlockId == -1) {
        return;
    }

    const auto CreateHandObject = [ped, this](eModelID handModel, AssocGroupId animGrpId, bool isLeftHand) {
        const auto handObj = new CHandObject(handModel, ped, isLeftHand);
        if (handObj) {
            CWorld::Add(handObj);
            CAnimManager::AddAnimation(
                handObj->GetRpClump(),
                CAnimManager::GetAnimAssociation(animGrpId, m_PlayerIdlesAnimBlockId)->GetAnimHierarchy(), // 0x61B11F: the right hand uses ANIM_GROUP_RHAND (0x50), not LHAND
                0
            );
            CObject::nNoTempObjects++;
        }
        return handObj;
    };
    m_LeftHand = CreateHandObject(m_DoUseFatHands ? MODEL_FHANDL : MODEL_SHANDL, ANIM_GROUP_LHAND, true);
    m_RightHand = CreateHandObject(m_DoUseFatHands ? MODEL_FHANDR : MODEL_SHANDR, ANIM_GROUP_RHAND, false);
}
