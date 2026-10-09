#pragma once

#include "TaskSimple.h"
#include "TaskTimer.h"

class NOTSA_EXPORT_VTABLE CTaskSimpleWaitForPizza : public CTaskSimple {
public:
    static constexpr auto Type = TASK_SIMPLE_WAIT_FOR_PIZZA;

    static void InjectHooks();

    CTaskSimpleWaitForPizza() = default; // 0x632B30
    ~CTaskSimpleWaitForPizza() override = default; // 0x632B70 (scalar deleting dtor: 0x6389D0)

    eTaskType GetTaskType() const override { return Type; } // 0x632B50
    CTask*    Clone() const override { return new CTaskSimpleWaitForPizza{}; } // 0x636B40
    bool      MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override; // 0x632B60
    bool      ProcessPed(CPed* ped) override; // 0x632B80

private:
    CTaskSimpleWaitForPizza* Constructor() { this->CTaskSimpleWaitForPizza::CTaskSimpleWaitForPizza(); return this; }

private:
    CTaskTimer m_Timer{};
};
VALIDATE_SIZE(CTaskSimpleWaitForPizza, 0x14);
