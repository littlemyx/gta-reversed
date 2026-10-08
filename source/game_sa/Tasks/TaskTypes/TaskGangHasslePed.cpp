#include "SeekEntity/TaskComplexSeekEntityStandard.h"
#include "StdInc.h"

#include "TaskGangHasslePed.h"
#include "SeekEntity/PosCalculators/EntitySeekPosCalculatorStandard.h"
#include "SeekEntity/TaskComplexSeekEntity.h"
#include "TaskComplexKillPedOnFoot.h"
#include "TaskComplexGangLeader.h"
#include "TaskComplexPlayHandSignalAnim.h"
#include "TaskSimpleRunAnim.h"
#include "General.h"

void CTaskGangHasslePed::InjectHooks() {
    RH_ScopedVirtualClass(CTaskGangHasslePed, 0x86FA00, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x65FED0);
    RH_ScopedInstall(Destructor, 0x65FF60);

    RH_ScopedVMTInstall(CreateNextSubTask, 0x6642C0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x664380);
    RH_ScopedVMTInstall(ControlSubTask, 0x65FFE0);
}

// 0x65FED0
CTaskGangHasslePed::CTaskGangHasslePed(CPed* ped, int32 a3, int32 a4, int32 a5) : CTaskComplex() {
    m_nTime = 0;
    m_nSomeRandomShit = 0;
    m_bFirstSubTaskInitialised = 0;
    m_bRefreshTime = 0;
    dword10 = a3;
    m_RndMin = a4;
    m_Ped = ped;
    m_RndMax = a5;
    m_bAnimBlockRefAdded = false;
    CEntity::SafeRegisterRef(m_Ped);
}

// 0x65FF60
CTaskGangHasslePed::~CTaskGangHasslePed() {
    CEntity::SafeCleanUpRef(m_Ped);

    if (m_bAnimBlockRefAdded) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_bAnimBlockRefAdded = false;
    }
}

namespace {
//! Random distance at which the ped should hassle the target at ([3, 5])
float GetRandomHassleDistance() {
    // `rand() * (1.f / RAND_MAX)` * 2 + 3
    const auto r = (double)CGeneral::GetRandomNumber();
    return (float)(r * (double)RAND_MAX_FLOAT_RECIPROCAL * 2.0 + 3.0);
}
}

// 0x6642C0
CTask* CTaskGangHasslePed::CreateNextSubTask(CPed* ped) {
    const float distance = GetRandomHassleDistance();
    if (m_pSubTask->GetTaskType() == TASK_COMPLEX_KILL_PED_ON_FOOT) {
        return nullptr;
    }
    return new CTaskComplexSeekEntity<CEntitySeekPosCalculatorStandard>{m_Ped, 999'999, 1'000, 1.f * distance, 2.f, 2.f, false, true};
}

// 0x664380
CTask* CTaskGangHasslePed::CreateFirstSubTask(CPed* ped) {
    if (!m_Ped) {
        return nullptr;
    }

    const float distance = GetRandomHassleDistance();

    // `rand() & 0xFFFF` * 2^-15 * (max - min) [truncated] + min
    const auto rnd = (double)(CGeneral::GetRandomNumber() & 0xFFFF);
    m_nTime                    = CTimer::GetTimeInMS();
    m_nSomeRandomShit          = (int32)(rnd * 3.0517578125e-05 * (double)(m_RndMax - m_RndMin)) + m_RndMin;
    m_bFirstSubTaskInitialised = true;

    return new CTaskComplexSeekEntity<CEntitySeekPosCalculatorStandard>{m_Ped, 999'999, 1'000, 1.f * distance, 2.f, 2.f, false, true};
}

// 0x65FFE0
CTask* CTaskGangHasslePed::ControlSubTask(CPed* ped) {
    if (!m_Ped) {
        return nullptr;
    }

    if (m_bFirstSubTaskInitialised) {
        if (m_bRefreshTime) {
            m_nTime        = CTimer::GetTimeInMS();
            m_bRefreshTime = false;
        }
        if ((uint32)(m_nSomeRandomShit + m_nTime) <= CTimer::GetTimeInMS()) { // Hassled long enough?
            if (dword10 == 2) {
                if (m_pSubTask->GetTaskType() != TASK_COMPLEX_KILL_PED_ON_FOOT) {
                    return new CTaskComplexKillPedOnFoot{m_Ped, -1, 0, 0, 0, 1};
                }
            } else if (dword10 != 1) {
                return nullptr;
            }
        }
    }

    // Make sure anmims are loaded (if they can/need to be)
    if (!m_bAnimBlockRefAdded) {
        if (CTaskComplexGangLeader::ShouldLoadGangAnims()) {
            const auto blk = CAnimManager::GetAnimationBlockIndex("gangs");
            if (CAnimManager::GetAnimBlocks()[blk].IsLoaded) {
                CAnimManager::AddAnimBlockRef(blk);
                m_bAnimBlockRefAdded = true;
            } else {
                CStreaming::RequestModel(IFPToModelId(blk), STREAMING_KEEP_IN_MEMORY);
            }
        }
    } else if (!CTaskComplexGangLeader::ShouldLoadGangAnims()) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_bAnimBlockRefAdded = false;
    }

    // Occasionally play some anim
    if (m_bAnimBlockRefAdded && !ped->IsPlayingHandSignal() && !ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM)) {
        const auto r = CGeneral::GetRandomNumberInRange(0, 200);
        CTask* animTask{};
        if (r > 50 && r < 56) {
            // `rand() & 0xFFFF` * 2^-15 * 8 [truncated]
            const auto idx = (int32)((double)(CGeneral::GetRandomNumber() & 0xFFFF) * 3.0517578125e-05 * 8.0);
            animTask = new CTaskSimpleRunAnim{ANIM_GROUP_GANGS, (AnimationId)(ANIM_ID_PRTIAL_GNGTLKA + idx), 4.f, false};
        } else if (r == 100) {
            animTask = new CTaskComplexPlayHandSignalAnim{ANIM_ID_UNDEFINED, 4.f};
        }
        if (animTask) {
            ped->GetTaskManager().SetTaskSecondary(animTask, TASK_SECONDARY_PARTIAL_ANIM);
        }
    }

    switch (dword10) {
    case 0:
        ped->Say(CTX_GLOBAL_EYEING_PED);
        break;
    case 1:
    case 2:
        CTaskComplexGangLeader::DoGangAbuseSpeech(ped, m_Ped);
        break;
    }

    return m_pSubTask;
}
