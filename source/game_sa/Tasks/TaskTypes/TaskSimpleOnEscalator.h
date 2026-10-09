#pragma once

#include "TaskSimpleStandStill.h"

class NOTSA_EXPORT_VTABLE CTaskSimpleOnEscalator : public CTaskSimpleStandStill {
public:
    static constexpr auto Type = TASK_SIMPLE_ON_ESCALATOR;

    static void InjectHooks();

    CTaskSimpleOnEscalator() : CTaskSimpleStandStill{ 0, true, false, 8.f } { } // 0x4BC150 (inlined)
    ~CTaskSimpleOnEscalator() override = default; // 0x4B8730 (scalar deleting dtor)

    eTaskType GetTaskType() const override { return Type; } // 0x4B8720
    CTask*    Clone() const override { return new CTaskSimpleOnEscalator{}; } // 0x4B86C0
    bool      ProcessPed(CPed* ped) override; // 0x62F520
};
VALIDATE_SIZE(CTaskSimpleOnEscalator, 0x20);
