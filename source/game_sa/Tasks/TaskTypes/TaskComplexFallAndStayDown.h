#pragma once

#include "TaskComplex.h"
#include "AnimationEnums.h"

//! Ped falls (using a `CTaskSimpleFall` that practically never ends) and stays down
class NOTSA_EXPORT_VTABLE CTaskComplexFallAndStayDown : public CTaskComplex {
public:
    static constexpr auto Type = TASK_COMPLEX_FALL_AND_STAY_DOWN;

    static void InjectHooks();

    CTaskComplexFallAndStayDown(AnimationId fallAnimId, AssocGroupId fallAnimGroup); // 0x6789D0

    /*!
    * @param dir 0 = ANIM_ID_KO_SKID_FRONT, 1 = ANIM_ID_KO_SPIN_R, 2 = ANIM_ID_KO_SKID_BACK, 3 = ANIM_ID_KO_SPIN_L (Uses the default anim group).
    *            For anything else the anim id is left uninitialized by the original (here it's 0).
    */
    CTaskComplexFallAndStayDown(int32 dir); // 0x678A10

    ~CTaskComplexFallAndStayDown() override = default; // 0x678A90 (scalar deleting dtor: 0x67CBC0)

    eTaskType GetTaskType() const override { return Type; } // 0x678A00
    CTask*    Clone() const override { return new CTaskComplexFallAndStayDown{ m_FallAnimId, m_FallAnimGroup }; } // 0x67C270
    bool      MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override; // 0x678AA0
    CTask*    CreateNextSubTask(CPed* ped) override; // 0x67CBE0
    CTask*    CreateFirstSubTask(CPed* ped) override; // 0x678B20
    CTask*    ControlSubTask(CPed* ped) override; // 0x678BC0

    CTask* CreateSubTask(eTaskType taskType); // 0x678BD0

private:
    CTaskComplexFallAndStayDown* Constructor1(AnimationId fallAnimId, AssocGroupId fallAnimGroup) { this->CTaskComplexFallAndStayDown::CTaskComplexFallAndStayDown(fallAnimId, fallAnimGroup); return this; }
    CTaskComplexFallAndStayDown* Constructor2(int32 dir) { this->CTaskComplexFallAndStayDown::CTaskComplexFallAndStayDown(dir); return this; }

private:
    AnimationId  m_FallAnimId{};    // 0x0C
    AssocGroupId m_FallAnimGroup{}; // 0x10
};
VALIDATE_SIZE(CTaskComplexFallAndStayDown, 0x14);
