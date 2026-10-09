#pragma once

#include "TaskSimple.h"

class CAnimBlendAssociation;

class NOTSA_EXPORT_VTABLE CTaskSimpleBeKickedOnGround : public CTaskSimple {
public:
    static constexpr auto Type = TASK_SIMPLE_BE_KICKED_ON_GROUND;

    static void InjectHooks();

    CTaskSimpleBeKickedOnGround() = default; // Inlined into `Clone` @ 0x623120 (never constructed anywhere else in the exe)
    ~CTaskSimpleBeKickedOnGround() override; // 0x61FC50 (scalar deleting dtor: 0x625B80)

    eTaskType GetTaskType() const override { return Type; } // 0x61FC40
    CTask*    Clone() const override { return new CTaskSimpleBeKickedOnGround{}; } // 0x623120
    bool      MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override; // 0x61FCC0
    bool      ProcessPed(CPed* ped) override; // 0x61FE00

    void StartAnim(CPed* ped); // 0x61FD20
    static void FinishAnimCB(CAnimBlendAssociation* assoc, void* data); // 0x61FD10

private:
    bool                   m_bFinished{};
    CAnimBlendAssociation* m_pAnim{};
};
VALIDATE_SIZE(CTaskSimpleBeKickedOnGround, 0x10);
