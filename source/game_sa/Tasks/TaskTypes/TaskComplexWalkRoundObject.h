#pragma once

#include "TaskComplex.h"
#include "Vector.h"

class CPointRoute;

class NOTSA_EXPORT_VTABLE CTaskComplexWalkRoundObject : public CTaskComplex {
public:
    int32        m_moveState;
    CVector      m_targetPoint;
    CEntity*     m_object;
    CPointRoute* m_pointRoute;
    int32        field_24; // Time (ms) at which the route following was started
    int32        field_28; // Time (ms) the ped has to follow the route
    int8         field_2C; // Route following timer is active
    int8         field_2D;
    CVector      field_30; // Position of the object when the route was computed
    CVector      field_3C; // Forward vector of the object when the route was computed
    CVector      field_48; // Right vector of the object when the route was computed

public:
    static constexpr auto Type = TASK_COMPLEX_WALK_ROUND_OBJECT;

    CTaskComplexWalkRoundObject(int32 moveState, const CVector& targetPoint, CEntity* object);
    ~CTaskComplexWalkRoundObject() override;

    eTaskType GetTaskType() const override { return Type; }
    CTask* Clone() const override { return new CTaskComplexWalkRoundObject(m_moveState, m_targetPoint, m_object); }
    CTask* CreateNextSubTask(CPed* ped) override;
    CTask* CreateFirstSubTask(CPed* ped) override;
    CTask* ControlSubTask(CPed* ped) override;

    CTask* CreateRouteTask(CPed* ped);
    CTask* CreateSubTask(eTaskType taskType, CPed* ped);
    float  ComputeRoute(CPed* ped);

private:
    friend void InjectHooksMain();
    static void InjectHooks();

    CTaskComplexWalkRoundObject* Constructor(int32 moveState, const CVector& targetPoint, CEntity* object);
};

VALIDATE_SIZE(CTaskComplexWalkRoundObject, 0x54);
