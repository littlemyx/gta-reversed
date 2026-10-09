#include "StdInc.h"

#include "TaskSimpleTriggerLookAt.h"
#include "Ragdoll/IKChainManager.h"

void CTaskSimpleTriggerLookAt::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleTriggerLookAt, 0x86E3CC, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x634440);
    RH_ScopedVMTDestructorInstall(0x6394D0);
    RH_ScopedVMTInstall(Clone, 0x634560);
    RH_ScopedVMTInstall(GetTaskType, 0x6344F0);
    RH_ScopedVMTInstall(MakeAbortable, 0x634610);
    RH_ScopedVMTInstall(ProcessPed, 0x634620);
}

// 0x634440
CTaskSimpleTriggerLookAt::CTaskSimpleTriggerLookAt(CEntity* entity, int32 time, int32 offsetBoneTag, RwV3d offsetPos, bool bUseTorso, float speed, int32 blendTime, int32 priority) :
    CTaskSimple{},
    m_pEntity{ entity },
    m_time{ time },
    m_nOffsetBoneTag{ offsetBoneTag },
    m_vOffsetPos{ offsetPos },
    m_bUseTorso{ bUseTorso },
    m_Speed{ speed },
    m_nBlendTime{ blendTime },
    m_bEntityExist{ false },
    m_nPriority{ (int8)priority }
{
    if (m_pEntity) {
        m_pEntity->RegisterReference(&m_pEntity);
        m_bEntityExist = true;
    }
}

// 0x634560
CTask* CTaskSimpleTriggerLookAt::Clone() const {
    auto time = m_time;
    auto boneTag = m_nOffsetBoneTag;
    // NOTSA: the asm tests the field at +0x10 (the 3rd ctor argument) and replaces both the +0xC and +0x10 values, kept as is
    if (boneTag > -1 && !m_pEntity) {
        boneTag = -1;
        time    = 100;
    }
    return new CTaskSimpleTriggerLookAt{ m_pEntity, time, boneTag, m_vOffsetPos, m_bUseTorso, m_Speed, m_nBlendTime, m_nPriority };
}

// 0x634620
bool CTaskSimpleTriggerLookAt::ProcessPed(CPed* ped) {
    if (!m_bEntityExist || m_pEntity) {
        g_ikChainMan.LookAt(
            "TaskTriggerLookAt",
            ped,
            m_pEntity,
            m_time,
            (eBoneTag)m_nOffsetBoneTag,
            reinterpret_cast<CVector*>(&m_vOffsetPos), // lea [this + 0x14]
            m_bUseTorso,
            m_Speed,
            m_nBlendTime,
            m_nPriority,
            false
        );
    }
    return true;
}
