/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "TaskComplex.h"
#include "TaskTimer.h"

class CPed;

//! Script task TASK_USE_MOBILE_PHONE: PhoneIn -> PhoneChat (timed) -> PhoneOut -> short pause, vtable 0x86E454.
class NOTSA_EXPORT_VTABLE CTaskComplexUseMobilePhone : public CTaskComplex {
public:
    static void InjectHooks();
    int32      m_nDuration;
    CTaskTimer m_timer;
    bool       m_bIsAborting;
    bool       m_bQuit;

public:
    static constexpr auto Type = TASK_COMPLEX_USE_MOBILE_PHONE;

    explicit CTaskComplexUseMobilePhone(int32 nDuration); // 0x6348A0
    ~CTaskComplexUseMobilePhone() override = default;     // 0x639660 (scalar deleting dtor), body 0x6348E0

    CTask*    Clone() const override;                                                                                                       // 0x636FD0
    eTaskType GetTaskType() const override { return Type; }                                                                                  // 0x6348D0
    bool      MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override;              // 0x634930
    CTask*    CreateNextSubTask(CPed* ped) override;                                                                                         // 0x634A60
    CTask*    CreateFirstSubTask(CPed* ped) override;                                                                                        // 0x634C10
    CTask*    ControlSubTask(CPed* ped) override;                                                                                            // 0x634D60

    void Quit(CPed* ped); // 0x634A40
    void HidePhone(CPed* ped); // 0x6348F0 (`this` unused)

private: // Wrappers for hooks
    CTaskComplexUseMobilePhone* Constructor(int32 nDuration) { this->CTaskComplexUseMobilePhone::CTaskComplexUseMobilePhone(nDuration); return this; }
};

VALIDATE_SIZE(CTaskComplexUseMobilePhone, 0x20);
