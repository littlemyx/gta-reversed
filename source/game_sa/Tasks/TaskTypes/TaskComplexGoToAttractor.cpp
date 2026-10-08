#include "StdInc.h"

#include "TaskComplexGoToAttractor.h"
#include "PedAtmAttractor.h"
#include "PedPlacement.h"
#include "TaskComplexSequence.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleSlideToCoord.h"
#include "TaskSimpleStandStill.h"

void CTaskComplexGoToAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGoToAttractor, 0x86FF3C, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x66B640);
    RH_ScopedInstall(Destructor, 0x66B6A0);

    RH_ScopedVMTInstall(Clone, 0x66D130);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x66B6C0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x670420);
}

// 0x66B640
CTaskComplexGoToAttractor::CTaskComplexGoToAttractor(CPedAttractor* attractor, const CVector& pos, float heading, float attrTime, int32 queueNumber, eMoveState ms) : CTaskComplex() {
    m_Attractor = attractor;
    m_vecAttrPosn = pos;
    m_fAttrHeading = heading;
    m_MoveState = ms;
    m_fAttrTime = attrTime;
    m_nQueueNumber = queueNumber;
}

// 0x66B6B0
bool CTaskComplexGoToAttractor::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    return m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x66B6C0
CTask* CTaskComplexGoToAttractor::CreateNextSubTask(CPed* ped) {
    GetPedAttractorManager()->BroadcastArrival(ped, m_Attractor);
    if (ped->bUseAttractorInstantly && m_Attractor->GetHeadOfQueue() != ped) {
        ped->bUseAttractorInstantly = false;
    }
    return nullptr;
}

// 0x670420
CTask* CTaskComplexGoToAttractor::CreateFirstSubTask(CPed* ped) {
    auto moveState = m_MoveState;
    if (m_Attractor->GetType() == PED_ATTRACTOR_SHELTER) { // Get out of the rain quickly
        moveState = PEDMOVE_RUN;
    }

    if (!ped->bUseAttractorInstantly) {
        const auto seq = new CTaskComplexSequence{};
        seq->AddTask(new CTaskComplexGoToPointAndStandStill{moveState, m_vecAttrPosn, 0.5f, 2.f, false, false});
        seq->AddTask(new CTaskSimpleSlideToCoord{m_vecAttrPosn, m_fAttrHeading, 0.5f});
        return seq;
    }

    // Teleport the ped to the attractor
    m_vecAttrPosn = CPedPlacement::FindZCoorForPed(m_vecAttrPosn).first;
    ped->GetPosition() = m_vecAttrPosn;
    ped->m_fCurrentRotation = ped->m_fAimingRotation = m_fAttrHeading;
    return new CTaskSimpleStandStill{0, false, false, 8.f};
}

// 0x66B710
CTask* CTaskComplexGoToAttractor::ControlSubTask(CPed* ped) {
    return m_pSubTask;
}
