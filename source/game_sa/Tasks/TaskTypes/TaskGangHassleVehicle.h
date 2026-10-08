#pragma once

#include "TaskComplex.h"
#include "TaskTimer.h"
#include "Vector.h"
class CVehicle;
class CEntity;
class CPed;

class NOTSA_EXPORT_VTABLE CTaskGangHassleVehicle : public CTaskComplex {
public:
    uint8     gapC[4];
    CVehicle* m_Vehicle;
    int32     m_nHasslePosId;      // -1 if none
    bool      m_bAggressive;       // Ctor's `a4`. If set, starts to attack the driver once the timer runs out
    float     m_fTriggerDist;      // Ctor's `a5`. Distance (from the target) at which the ped starts to act
    float     m_fOffsetX;          // Ctor's `a6`
    CVector   m_vecPosn;           // Target offset, see `CalcTargetOffset`
    bool      m_bRemoveAnim;       // "gangs" anim block is referenced by this task
    uint8     m_State;             // 0 - Approaching, 1 - Turning, 2 - Hassling, 3 - Hassling ped (see `ControlSubTask`)
    CPed*     m_pEntity;           // The driver of the vehicle
    CTaskTimer m_Timer;

public:
    static constexpr auto Type = eTaskType::TASK_COMPLEX_GANG_HASSLE_VEHICLE;

    CTaskGangHassleVehicle(CVehicle* vehicle, int32 a3, uint8 a4, float a5, float a6);
    ~CTaskGangHassleVehicle() override;

    eTaskType GetTaskType() const override { return Type; }
    CTask*    Clone() const override { return new CTaskGangHassleVehicle(m_Vehicle, m_nHasslePosId, m_bAggressive, m_fTriggerDist, m_fOffsetX); } // 0x65FC00;
    CTask*    CreateNextSubTask(CPed* ped) override;
    CTask*    CreateFirstSubTask(CPed* ped) override;
    CTask*    ControlSubTask(CPed* ped) override;

    float GetTargetHeading(CPed* ped);
    void CalcTargetOffset();

private:
    friend void InjectHooksMain();
    static void InjectHooks();
    CTaskGangHassleVehicle* Constructor(CVehicle* vehicle, int32 a3, uint8 a4, float a5, float a6) { this->CTaskGangHassleVehicle::CTaskGangHassleVehicle(vehicle, a3, a4, a5, a6); return this; }
    CTaskGangHassleVehicle* Destructor() { this->CTaskGangHassleVehicle::~CTaskGangHassleVehicle(); return this; }
};
VALIDATE_SIZE(CTaskGangHassleVehicle, 0x44);
