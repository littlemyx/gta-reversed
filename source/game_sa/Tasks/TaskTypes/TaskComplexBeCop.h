#pragma once

#include "TaskComplexWanderCop.h"

class NOTSA_EXPORT_VTABLE CTaskComplexBeCop : public CTaskComplexWanderCop {
public:
    static void InjectHooks();
    static constexpr auto Type = TASK_COMPLEX_BE_COP;

    CTaskComplexBeCop(eMoveState moveState, uint8 dir, CTask* task) : CTaskComplexWanderCop(moveState, dir) { // 0x4993D0: `dir` is the node direction byte (0..7), it was truncated to a bool
        m_pGoToPointAndStandStillTask = static_cast<CTaskComplexGoToPointAndStandStill*>(task);
    };

    eTaskType GetTaskType() const override { return Type; }
    CTask* Clone() const override { return new CTaskComplexBeCop(m_nMoveState, m_nDir, m_pGoToPointAndStandStillTask ? m_pGoToPointAndStandStillTask->Clone() : nullptr); } // 0x499461: the exe clones the go-to task only when there is one
};
VALIDATE_SIZE(CTaskComplexBeCop, 0x50);
