#include "StdInc.h"

#include "TaskSimpleBeKickedOnGround.h"
#include "Localisation.h"
#include "RwHelper.h"
#include "AnimBlendAssociation.h"

void CTaskSimpleBeKickedOnGround::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleBeKickedOnGround, 0x86D7D8, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(StartAnim, 0x61FD20);
    RH_ScopedInstall(FinishAnimCB, 0x61FD10);
    RH_ScopedVMTDestructorInstall(0x625B80);
    RH_ScopedVMTInstall(Clone, 0x623120);
    RH_ScopedVMTInstall(GetTaskType, 0x61FC40);
    RH_ScopedVMTInstall(MakeAbortable, 0x61FCC0);
    RH_ScopedVMTInstall(ProcessPed, 0x61FE00);
}

// 0x61FC50
CTaskSimpleBeKickedOnGround::~CTaskSimpleBeKickedOnGround() {
    if (m_pAnim) {
        m_pAnim->m_BlendDelta = -4.f;
        m_pAnim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
    }
}

// 0x61FCC0
bool CTaskSimpleBeKickedOnGround::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority == ABORT_PRIORITY_URGENT || priority == ABORT_PRIORITY_IMMEDIATE) {
        if (m_pAnim) {
            m_pAnim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
            m_pAnim = nullptr;
        }
        return true;
    }
    if (m_pAnim) {
        m_pAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
        m_pAnim->m_BlendDelta = -4.f;
    }
    return false;
}

// 0x61FE00
bool CTaskSimpleBeKickedOnGround::ProcessPed(CPed* ped) {
    if (m_bFinished) {
        return true;
    }
    if (!m_pAnim) {
        StartAnim(ped);
    }
    return false;
}

// 0x61FD20
void CTaskSimpleBeKickedOnGround::StartAnim(CPed* ped) {
    if (!CLocalisation::KickingWhenDown()) { // 0x56D270
        m_bFinished = true;
        return;
    }

    const auto clump = ped->GetRpClump();
    m_pAnim = CAnimManager::BlendAnimation(
        clump,
        ANIM_GROUP_DEFAULT,
        RpAnimBlendClumpGetFirstAssociation(clump, ANIMATION_IS_FRONT) ? ANIM_ID_FLOOR_HIT_F : ANIM_ID_FLOOR_HIT,
        8.f
    );
    m_pAnim->SetCurrentTime(0.f);
    m_pAnim->m_Flags |= ANIMATION_IS_PLAYING;
    m_pAnim->m_Flags &= ~ANIMATION_IS_FINISH_AUTO_REMOVE;

    // NOTSA: The original transforms a zero point by the head bone's matrix here, but never uses the result (calls kept for fidelity, they have no side effects)
    if (CLocalisation::KickingWhenDown()) {
        RwV3d pos{};
        const auto hier = GetAnimHierarchyFromSkinClump(clump);
        const auto idx  = RpHAnimIDGetIndex(hier, ped->m_apBones[PED_NODE_HEAD]->BoneTag);
        RwV3dTransformPoints(&pos, &pos, 1, &RpHAnimHierarchyGetMatrixArray(hier)[idx]);
    }

    m_pAnim->SetFinishCallback(FinishAnimCB, this);
}

// 0x61FD10
void CTaskSimpleBeKickedOnGround::FinishAnimCB(CAnimBlendAssociation* assoc, void* data) {
    const auto self = static_cast<CTaskSimpleBeKickedOnGround*>(data);
    self->m_pAnim     = nullptr;
    self->m_bFinished = true;
}
