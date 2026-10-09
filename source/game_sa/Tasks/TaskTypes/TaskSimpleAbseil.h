#pragma once
#include "TaskSimpleRunAnim.h"
#include "AnimationEnums.h"

class NOTSA_EXPORT_VTABLE CTaskSimpleAbseil : public CTaskSimpleRunAnim
{
public:
    static void InjectHooks();
    CTaskSimpleAbseil() : CTaskSimpleRunAnim(ANIM_GROUP_DEFAULT, ANIM_ID_ABSEIL, 4.0F, TASK_SIMPLE_ABSEIL, "Abseil", false) {}
    ~CTaskSimpleAbseil() override {}
    CTask* Clone() const override { return new CTaskSimpleAbseil(); }

    virtual bool IsInterruptable(CPed* ped) { return false; }
};

VALIDATE_SIZE(CTaskSimpleAbseil, 0x20);
