#pragma once

#include "TaskComplex.h"
#include "Vector.h"

class CFire;

//! Ped runs to a nearby fire and puts it out (with a fire extinguisher)
class NOTSA_EXPORT_VTABLE CTaskComplexExtinguishFireOnFoot : public CTaskComplex {
public:
    static constexpr auto Type = TASK_COMPLEX_EXTINGUISH_FIRE_ON_FOOT;

    static void InjectHooks();

    CTaskComplexExtinguishFireOnFoot(const CVector& firePos); // 0x659870
    ~CTaskComplexExtinguishFireOnFoot() override = default; // 0x6598B0 (scalar deleting dtor: 0x65A700)

    eTaskType GetTaskType() const override { return Type; } // 0x6598A0
    CTask*    Clone() const override { return new CTaskComplexExtinguishFireOnFoot{ m_FirePos }; } // 0x659D70
    CTask*    CreateNextSubTask(CPed* ped) override; // 0x6598C0
    CTask*    CreateFirstSubTask(CPed* ped) override; // 0x65A720
    CTask*    ControlSubTask(CPed* ped) override; // 0x659980

    //! @return The nearest fire if it's within 10 units of the ped
    CFire* FindFireNearPed(CPed* ped); // 0x659990

private:
    CTaskComplexExtinguishFireOnFoot* Constructor(const CVector& firePos) { this->CTaskComplexExtinguishFireOnFoot::CTaskComplexExtinguishFireOnFoot(firePos); return this; }

private:
    CVector m_FirePos{}; // 0x0C
};
VALIDATE_SIZE(CTaskComplexExtinguishFireOnFoot, 0x18);
