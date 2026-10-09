#pragma once

#include "TaskSimple.h"
#include "eTargetDoor.h"

class CVehicle;

class NOTSA_EXPORT_VTABLE CTaskSimpleCarForcePedOut : public CTaskSimple {
public:
    static constexpr auto Type = TASK_SIMPLE_CAR_FORCE_PED_OUT;

    static void InjectHooks();

    CTaskSimpleCarForcePedOut(CVehicle* vehicle, eTargetDoor door);
    ~CTaskSimpleCarForcePedOut() override;

    eTaskType GetTaskType() const override { return Type; } // 0x647770
    CTask*    Clone() const override;
    bool      MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override { return false; } // 0x647780
    bool      ProcessPed(CPed* ped) override;

private: // Wrappers for hooks
    // 0x647710
    CTaskSimpleCarForcePedOut* Constructor(CVehicle* vehicle, eTargetDoor door) {
        this->CTaskSimpleCarForcePedOut::CTaskSimpleCarForcePedOut(vehicle, door);
        return this;
    }

    // 0x647790
    CTaskSimpleCarForcePedOut* Destructor() {
        this->CTaskSimpleCarForcePedOut::~CTaskSimpleCarForcePedOut();
        return this;
    }

public:
    CVehicle*   m_vehicle{};
    eTargetDoor m_door{};
};
VALIDATE_SIZE(CTaskSimpleCarForcePedOut, 0x10);
