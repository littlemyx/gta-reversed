#pragma once

#include "TaskComplexCarDrive.h"

// todo: rename -> TaskComplexCarDriveWander.h

class NOTSA_EXPORT_VTABLE CTaskComplexCarDriveWander : public CTaskComplexCarDrive {
public:
    static void InjectHooks();
    static constexpr auto Type = TASK_COMPLEX_CAR_DRIVE_WANDER;

    CTaskComplexCarDriveWander(CVehicle* vehicle, eCarDrivingStyle carDrivingStyle = DRIVING_STYLE_STOP_FOR_CARS, float fSpeed = 10.f);
    ~CTaskComplexCarDriveWander() override = default;

    // 0x63DD00: allocates 0x24 bytes, runs CTaskComplexCarDrive(veh, speed, -1, style) (0x63C940) and then stores THIS class' vtable (0x86E96C): the clone is a Wander task, not a plain CarDrive
    CTask* Clone() const override { return new CTaskComplexCarDriveWander(m_Veh, static_cast<eCarDrivingStyle>(m_CarDrivingStyle), m_CruiseSpeed); };
    eTaskType GetTaskType() const override { return Type; }
    void SetUpCar() override;
    CTask* CreateSubTaskCannotGetInCar(CPed* ped) override;
};

VALIDATE_SIZE(CTaskComplexCarDrive, 0x24);
