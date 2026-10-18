#pragma once

#include "TaskComplexEnterCar.h"

class NOTSA_EXPORT_VTABLE CTaskComplexOpenPassengerDoor : public CTaskComplexEnterCar {
public:
    static void InjectHooks();
    static constexpr auto Type = TASK_COMPLEX_CAR_OPEN_PASSENGER_DOOR;

    CTaskComplexOpenPassengerDoor(CVehicle* vehicle, int32 targetSeat) : CTaskComplexEnterCar(vehicle, false, true, false, false) {
        m_TargetSeat = targetSeat;
    } // 0x6403E0
    ~CTaskComplexOpenPassengerDoor() override = default; // 0x640420

    eTaskType GetTaskType() const override{ return Type; } // 0x640410
    CTask* Clone() const override { // 0x6438E0: also copies the move state ([edi+24h] -> [eax+24h], 0x643934); the clone used to restart with the default PEDMOVE_RUN
        auto* const t = new CTaskComplexOpenPassengerDoor(m_Car, m_TargetSeat);
        t->m_MoveState = m_MoveState;
        return t;
    }
};
