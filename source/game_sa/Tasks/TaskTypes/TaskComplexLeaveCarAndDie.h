#pragma once

#include "TaskComplexLeaveCar.h"

class CPed;

class NOTSA_EXPORT_VTABLE CTaskComplexLeaveCarAndDie : public CTaskComplexLeaveCar {
 
public:
    static void InjectHooks();

    static constexpr auto Type = eTaskType::TASK_COMPLEX_LEAVE_CAR_AND_DIE;

    CTaskComplexLeaveCarAndDie(CVehicle* targetVehicle, int32 targetDoor, int32 delayTime); // 0x62F1A0 (NOTSA name: the exe ctor is only reachable from CTaskComplexDieInCar::CreateSubTask)
    CTaskComplexLeaveCarAndDie(const CTaskComplexLeaveCarAndDie&);

    CTask*    Clone() const override { return new CTaskComplexLeaveCarAndDie{ *this }; }
    eTaskType GetTaskType() const override { return Type; }
    CTask*    CreateFirstSubTask(CPed * ped) override;
};
