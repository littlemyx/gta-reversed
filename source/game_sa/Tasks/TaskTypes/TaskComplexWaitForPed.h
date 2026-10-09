#pragma once

#include "TaskComplex.h"
#include "TaskTimer.h"

class CPed;

class NOTSA_EXPORT_VTABLE CTaskComplexWaitForPed : public CTaskComplex {
public:
    static constexpr auto Type = TASK_COMPLEX_WAIT_FOR_PED;

    static void InjectHooks();

    CTaskComplexWaitForPed(CPed* ped, float radius, uint32 timeInMs, bool rotateOtherPedsToWaitingPed);
    ~CTaskComplexWaitForPed() override;

    eTaskType GetTaskType() const override { return Type; } // 0x6833C0
    CTask*    Clone() const override;
    bool      MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override { return true; }
    CTask*    CreateNextSubTask(CPed* ped) override { return nullptr; }
    CTask*    CreateFirstSubTask(CPed* ped) override;
    CTask*    ControlSubTask(CPed* ped) override;

private: // Wrappers for hooks
    // 0x683340
    CTaskComplexWaitForPed* Constructor(CPed* ped, float radius, uint32 timeInMs, bool rotateOthers) {
        this->CTaskComplexWaitForPed::CTaskComplexWaitForPed(ped, radius, timeInMs, rotateOthers);
        return this;
    }

    // 0x6833D0
    CTaskComplexWaitForPed* Destructor() {
        this->CTaskComplexWaitForPed::~CTaskComplexWaitForPed();
        return this;
    }

public:
    CPed*      m_ped;
    float      m_radius;
    uint32     m_timeInMs;
    bool       m_bRotateOtherPedsToWaitingPed;
    CTaskTimer m_timer;
    int32      m_framesToWaitForSettingRotation; // uninitialised in the original ctor (set in CreateFirstSubTask)
};
VALIDATE_SIZE(CTaskComplexWaitForPed, 0x2C);
