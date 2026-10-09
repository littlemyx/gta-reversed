#pragma once

#include "TaskComplex.h"
#include "Ped.h"

//! Ped turns to the cop and puts their hands up
class NOTSA_EXPORT_VTABLE CTaskComplexPresentIdToCop : public CTaskComplex {
public:
    static constexpr auto Type = TASK_COMPLEX_PRESENT_ID_TO_COP;

    static void InjectHooks();

    CTaskComplexPresentIdToCop(CPed* cop); // 0x6591D0
    ~CTaskComplexPresentIdToCop() override = default; // 0x659240 (scalar deleting dtor: 0x65A0F0)

    eTaskType GetTaskType() const override { return Type; } // 0x659230
    CTask*    Clone() const override { return new CTaskComplexPresentIdToCop{ m_Cop }; } // 0x659B60
    CTask*    CreateNextSubTask(CPed* ped) override; // 0x65AFE0
    CTask*    CreateFirstSubTask(CPed* ped) override; // 0x65B050
    CTask*    ControlSubTask(CPed* ped) override; // 0x65B070

    CTask* CreateSubTask(eTaskType taskType, CPed* ped); // 0x65A110
    float  ComputeHeadingToCop(CPed* ped) const; // 0x6592A0

private:
    CTaskComplexPresentIdToCop* Constructor(CPed* cop) { this->CTaskComplexPresentIdToCop::CTaskComplexPresentIdToCop(cop); return this; }

private:
    CPed::Ref m_Cop{};
};
VALIDATE_SIZE(CTaskComplexPresentIdToCop, 0x10);
