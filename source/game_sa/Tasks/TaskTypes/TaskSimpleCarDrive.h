#pragma once

#include "TaskSimple.h"
#include "TaskTimer.h"

class CVehicle;
class CTaskUtilityLineUpPedWithCar;

class NOTSA_EXPORT_VTABLE CTaskSimpleCarDrive : public CTaskSimple {
public:
    CVehicle*                     m_pVehicle;
    CAnimBlendAssociation*        m_pAnimCloseDoorRolling;
    CTaskUtilityLineUpPedWithCar* m_pTaskUtilityLineUpPedWithCar;
    CTaskTimer                    m_NoDriverTimer; // Started when the ped is a (front) passenger in a driverless car, once it's out of time the ped leaves the car and wanders around
    int32                         m_nBoppingStartTime;
    int32                         field_24;
    int32                         m_nBoppingEndTime;  // Seemingly not a tick count, but rather the bopping interval
    float                         m_fBoppingProgress; // 0.0 to 1.0
    int32                         m_nBoppingCompletedTimes;
    int32                         m_nHeadBoppingStartTime;
    int32                         m_nHeadBoppingDirection;
    float                         m_fHeadBoppingOrientation;
    float                         m_fRandomHeadBoppingMultiplier;
    float                         m_fHeadBoppingFactor;
    int32                         m_nArmBoppingStartTime;
    int32                         m_nTimePassedSinceCarUpSideDown;
    CTaskTimer                    m_copCarStolenTimer;

    // Inited according to: 0x63C3AE
    uint8 m_bHeadBopping : 1;          // Is the head bopping
    uint8 m_bArmBopping : 1;           // Is the arm bopping
    uint8 m_bUpdateCurrentVehicle : 1; // updates m_pVehicle pointer to the current occupied vehicle by ped
    uint8 m_b08 : 1;                   // Set on construction, and when `MakeAbortable` is called (non-immediately). Reset by `SetPedPosition`.
    uint8 m_bPassengerAnimBlended : 1; // Passenger's "bike passenger" anim has been blended in already
    uint8 m_bClosingDoor : 1;          // Ped is closing the door (rolling it shut), can't be aborted

public:
    static constexpr auto Type = TASK_SIMPLE_CAR_DRIVE;

    CTaskSimpleCarDrive() = delete;
    explicit CTaskSimpleCarDrive(CVehicle* vehicle, CTaskUtilityLineUpPedWithCar* utilityTask = {}, bool updateCurrentVehicle = {});
    ~CTaskSimpleCarDrive() override;



eTaskType GetTaskType() const override { return Type; }

    CTask* Clone() const override;
    bool ProcessPed(class CPed* ped) override;
    bool MakeAbortable(class CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override;
    bool SetPedPosition(CPed* ped) override;

    void TriggerIK(CPed* ped) const;
    void UpdateBopping();
    void StartBopping(CPed* ped);
    void ProcessHeadBopping(CPed* ped, bool a3, float a4);
    void ProcessArmBopping(CPed* pPed, bool a3, float a4);
    void ProcessBopping(CPed* ped, bool a3);
    void StartRollDoorAnim(CPed* ped);

    [[nodiscard]] auto GetVehicle() const { return m_pVehicle; }

private:
    friend void InjectHooksMain();
    static void InjectHooks();

    CTaskSimpleCarDrive* Constructor(CVehicle* pVehicle, CTaskUtilityLineUpPedWithCar* pUtilityTask, int8_t bUpdateCurrentVehicle) { this->CTaskSimpleCarDrive::CTaskSimpleCarDrive(pVehicle, pUtilityTask, bUpdateCurrentVehicle); return this; }
    CTaskSimpleCarDrive* Destructor() { this->CTaskSimpleCarDrive::~CTaskSimpleCarDrive(); return this; }
                    };

VALIDATE_SIZE(CTaskSimpleCarDrive, 0x60);
