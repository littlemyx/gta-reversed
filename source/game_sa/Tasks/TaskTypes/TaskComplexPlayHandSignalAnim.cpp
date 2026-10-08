#include "StdInc.h"
#include "TaskComplexPlayHandSignalAnim.h"

#include "TaskSimpleStandStill.h"
#include "TaskSimplePlayHandSignalAnim.h"
#include "TaskComplexSequence.h"
#include "Ragdoll/IKChainManager.h"

void CTaskComplexPlayHandSignalAnim::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexPlayHandSignalAnim, 0x86d5dc, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x61B2B0);
    RH_ScopedInstall(Destructor, 0x61BDF0);

    RH_ScopedInstall(GetAnimIdForPed, 0x61B460);
    RH_ScopedInstall(CreateSubTask, 0x61B2F0);

    RH_ScopedVMTInstall(Clone, 0x61BA00);
    RH_ScopedVMTInstall(GetTaskType, 0x61B2E0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x61B570);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x61B4F0);
    RH_ScopedVMTInstall(ControlSubTask, 0x61B580);
}

// 0x61B2B0
CTaskComplexPlayHandSignalAnim::CTaskComplexPlayHandSignalAnim(AnimationId animationId, float blendFactor) :
    m_animationId{animationId},
    m_AnimBlenDelta{blendFactor}
{
}

// 0x61BDF0
CTaskComplexPlayHandSignalAnim::~CTaskComplexPlayHandSignalAnim() {
    const auto leftModel  = CModelInfo::GetModelInfo(m_DoUseFatHands ? MODEL_FHANDL : MODEL_SHANDL);
    const auto rightModel = CModelInfo::GetModelInfo(m_DoUseFatHands ? MODEL_FHANDR : MODEL_SHANDR);

    // Remove hand model refs (only those we've added)
    if (m_bLeftHandLoaded) {
        leftModel->RemoveRef();
    }
    if (m_bRightHandLoaded) {
        rightModel->RemoveRef();
    }

    // Deal with anim
    if (m_bAnimationLoaded) { // Remove anim ref
        CAnimManager::RemoveAnimBlockRef(ms_animBlock);
    } else if (leftModel->m_nRefCount == 0 || rightModel->m_nRefCount == 0) { // Unload anim block if not all of the models have refs
        if (ms_animBlock != (uint32)-1 && !CAnimManager::GetAnimBlocks()[ms_animBlock].RefCnt) {
            CStreaming::RemoveModel(IFPToModelId(ms_animBlock));
        }
    }
}

// 0x61B460 (NOTSA: `this` (ecx) is passed by the original callers but never read, hence `__stdcall` to match the callee-cleanup `ret 4`)
AnimationId __stdcall CTaskComplexPlayHandSignalAnim::GetAnimIdForPed(CPed* ped) {
    switch (ped->m_nPedType) {
    case PED_TYPE_GANG1:  return (AnimationId)0x140;
    case PED_TYPE_GANG2:  return (AnimationId)0x141;
    case PED_TYPE_GANG3:  return (AnimationId)0x142;
    case PED_TYPE_GANG5:  return (AnimationId)0x144;
    case PED_TYPE_GANG8:  return (AnimationId)0x143;
    default:              return ANIM_ID_UNDEFINED;
    }
}

// 0x61B2F0
CTask* CTaskComplexPlayHandSignalAnim::CreateSubTask(eTaskType taskType) {
    switch (taskType) {
    case TASK_SIMPLE_STAND_STILL:
        return new CTaskSimpleStandStill{ 0, true, false, 8.f };
    case TASK_SIMPLE_HANDSIGNAL_ANIM: {
        const auto seq = new CTaskComplexSequence{};
        seq->AddTask(new CTaskSimpleStandStill{ CGeneral::GetRandomNumberInRange(0, 1500), false, false, 8.f });
        seq->AddTask(new CTaskSimplePlayHandSignalAnim{ m_animationId, m_AnimBlenDelta, m_DoUseFatHands, false });
        return seq;
    }
    default:
        return nullptr;
    }
}

// 0x61BA00
CTask* CTaskComplexPlayHandSignalAnim::Clone() const {
    return new CTaskComplexPlayHandSignalAnim{ m_animationId, m_AnimBlenDelta };
}

// 0x61B570
CTask* CTaskComplexPlayHandSignalAnim::CreateNextSubTask(CPed* ped) {
    return nullptr;
}

// 0x61B4F0
CTask* CTaskComplexPlayHandSignalAnim::CreateFirstSubTask(CPed* ped) {
    if (g_ikChainMan.IsArmPointing(eIKArm::IK_ARM_LEFT, ped)) {
        return nullptr;
    }

    if (m_animationId == ANIM_ID_UNDEFINED) {
        m_animationId = GetAnimIdForPed(ped);
        if (m_animationId == ANIM_ID_UNDEFINED) {
            return nullptr;
        }
    }

    const auto modelId = ped->GetModelId();
    m_DoUseFatHands = modelId == MODEL_BALLAS2 || modelId == MODEL_FAM1 || modelId == MODEL_FAM3;
    return CreateSubTask(TASK_SIMPLE_STAND_STILL);
}

// 0x61B580
CTask* CTaskComplexPlayHandSignalAnim::ControlSubTask(CPed* ped) {
    const auto sub = m_pSubTask;

    const auto HandleHand = [](bool& isRefd, eModelID model) {
        if (CStreaming::IsModelLoaded(model)) {
            if (!isRefd) {
                CModelInfo::GetModelInfo(model)->AddRef();
                isRefd = true;
            }
        } else {
            CStreaming::RequestModel(model, STREAMING_KEEP_IN_MEMORY);
        }
    };

    // NOTE: Both hands are tracked by the same bits regardless of whether fat hands are used
    bool leftLoaded  = m_bLeftHandLoaded;
    bool rightLoaded = m_bRightHandLoaded;
    HandleHand(leftLoaded,  m_DoUseFatHands ? MODEL_FHANDL : MODEL_SHANDL);
    HandleHand(rightLoaded, m_DoUseFatHands ? MODEL_FHANDR : MODEL_SHANDR);
    m_bLeftHandLoaded  = leftLoaded;
    m_bRightHandLoaded = rightLoaded;

    if (m_bLeftHandLoaded && m_bRightHandLoaded) {
        if (ms_animBlock == (uint32)-1) {
            ms_animBlock = CAnimManager::GetAnimationBlockIndex("ghands");
        }
        if (CStreaming::IsModelLoaded(IFPToModelId(ms_animBlock))) { // NOTE: Checks the streaming state, not `CAnimBlock::IsLoaded`
            if (!m_bAnimationLoaded) {
                CAnimManager::AddAnimBlockRef(ms_animBlock);
                m_bAnimationLoaded = true;
            }
        } else {
            CStreaming::RequestModel(IFPToModelId(ms_animBlock), STREAMING_KEEP_IN_MEMORY);
        }
    }

    if (m_bAnimationLoaded && sub->GetTaskType() == TASK_SIMPLE_STAND_STILL) {
        return CreateSubTask(TASK_SIMPLE_HANDSIGNAL_ANIM);
    }
    return sub;
}
