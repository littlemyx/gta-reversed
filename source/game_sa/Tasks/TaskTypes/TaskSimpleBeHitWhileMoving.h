#pragma once

#include "TaskSimple.h"

class CAnimBlendAssociation;

class NOTSA_EXPORT_VTABLE CTaskSimpleBeHitWhileMoving : public CTaskSimple {
public:
    static constexpr auto Type = TASK_SIMPLE_BE_HIT_WHILE_MOVING;

    static void InjectHooks();

    CTaskSimpleBeHitWhileMoving(int32 hitType, int32 hitDir) : m_HitType{hitType}, m_HitDir{hitDir} { } // 0x61FE30
    ~CTaskSimpleBeHitWhileMoving() override; // 0x61FE70 (scalar deleting dtor: 0x625BA0)

    eTaskType GetTaskType() const override { return Type; } // 0x61FE60
    CTask*    Clone() const override { return new CTaskSimpleBeHitWhileMoving{m_HitType, m_HitDir}; } // 0x623190
    bool      MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override; // 0x61FEE0
    bool      ProcessPed(CPed* ped) override; // 0x620290

    void StartAnim(CPed* ped); // 0x61FF40
    static void FinishAnimCB(CAnimBlendAssociation* assoc, void* data); // 0x61FF30

private:
    CTaskSimpleBeHitWhileMoving* Constructor(int32 hitType, int32 hitDir) { this->CTaskSimpleBeHitWhileMoving::CTaskSimpleBeHitWhileMoving(hitType, hitDir); return this; }

private:
    bool                   m_bFinished{};
    int32                  m_HitType{}; //< 2 or 4 select the knock-down animations, anything else the shot-partial ones
    int32                  m_HitDir{};  //< 1, 2, 3 = left/back/right-ish variants, anything else = front
    CAnimBlendAssociation* m_pAnim{};
};
VALIDATE_SIZE(CTaskSimpleBeHitWhileMoving, 0x18);
