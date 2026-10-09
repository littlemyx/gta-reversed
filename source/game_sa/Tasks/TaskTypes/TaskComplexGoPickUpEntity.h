#pragma once

#include "TaskComplex.h"
#include "AnimationEnums.h"

class CEntity;
class CPed;

class NOTSA_EXPORT_VTABLE CTaskComplexGoPickUpEntity : public CTaskComplex {
public:
    static constexpr auto Type = TASK_COMPLEX_GO_PICKUP_ENTITY;

    static void InjectHooks();

    CTaskComplexGoPickUpEntity(CEntity* entity, AssocGroupId animGroup);
    ~CTaskComplexGoPickUpEntity() override;

    eTaskType GetTaskType() const override { return Type; } // 0x691A40
    CTask*    Clone() const override;                       // 0x692C80
    CTask*    CreateNextSubTask(CPed* ped) override;        // 0x691AE0
    CTask*    CreateFirstSubTask(CPed* ped) override;       // 0x693610
    CTask*    ControlSubTask(CPed* ped) override;           // 0x691D50

private: // Wrappers for hooks
    // 0x6919C0
    CTaskComplexGoPickUpEntity* Constructor(CEntity* entity, AssocGroupId animGroup) {
        this->CTaskComplexGoPickUpEntity::CTaskComplexGoPickUpEntity(entity, animGroup);
        return this;
    }

    // 0x691A50
    CTaskComplexGoPickUpEntity* Destructor() {
        this->CTaskComplexGoPickUpEntity::~CTaskComplexGoPickUpEntity();
        return this;
    }

public:
    CEntity*     m_pEntity;
    CVector      m_vecPosition;       //< Hold offset passed to the hold/pick-up tasks (+0x10)
    CVector      m_vecPickupPosition; //< Point (in the entity's space) to walk to; copied into the pick-up task (+0x1C)
    uint32       m_nTimePassedSinceLastSubTaskCreatedInMs; //< Time of the last (sub-)task creation (+0x28)
    AssocGroupId m_nAnimGroupId;
    bool         m_bAnimBlockReferenced;
};

VALIDATE_SIZE(CTaskComplexGoPickUpEntity, 0x34);
