#pragma once

#include "TaskSimpleDuck.h"

//! Duck task used when bullets whizz by the ped. Same layout as `CTaskSimpleDuck`.
class NOTSA_EXPORT_VTABLE CTaskSimpleDuckWhileShotsWhizzing : public CTaskSimpleDuck {
public:
    static constexpr auto Type = TASK_SIMPLE_DUCK_WHILE_SHOTS_WHIZZING;

    static void InjectHooks();

    CTaskSimpleDuckWhileShotsWhizzing(uint16 lengthOfDuck); // 0x692680
    ~CTaskSimpleDuckWhileShotsWhizzing() override = default; // 0x6926F0 (scalar deleting dtor: 0x693B80)

    eTaskType GetTaskType() const override { return Type; } // 0x6926E0
    CTask*    Clone() const override { return new CTaskSimpleDuckWhileShotsWhizzing{ m_LengthOfDuck }; } // 0x692D60
    bool      MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override; // 0x692700
    bool      ProcessPed(CPed* ped) override; // 0x694640

private:
    CTaskSimpleDuckWhileShotsWhizzing* Constructor(uint16 lengthOfDuck) { this->CTaskSimpleDuckWhileShotsWhizzing::CTaskSimpleDuckWhileShotsWhizzing(lengthOfDuck); return this; }
};
VALIDATE_SIZE(CTaskSimpleDuckWhileShotsWhizzing, 0x28);
