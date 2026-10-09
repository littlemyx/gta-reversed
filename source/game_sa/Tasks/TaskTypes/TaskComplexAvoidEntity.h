#pragma once

#include "TaskComplex.h"
#include "Vector.h"
#include "Entity.h"
#include "eMoveState.h"
#include <extensions/EntityRef.hpp>

class CColSphere;

//! Walks around an entity (by going to a point on the route computed round it's bounding sphere), while looking at the target point
class NOTSA_EXPORT_VTABLE CTaskComplexAvoidEntity : public CTaskComplex {
public:
    static constexpr auto Type = TASK_COMPLEX_AVOID_ENTITY;

    static void InjectHooks();

    CTaskComplexAvoidEntity(eMoveState moveState, CEntity* entity, const CVector& targetPos); // 0x66AA20
    ~CTaskComplexAvoidEntity() override = default; // 0x66AAE0 (scalar deleting dtor: 0x66F6A0)

    eTaskType GetTaskType() const override { return Type; } // 0x66AAD0
    CTask*    Clone() const override { return new CTaskComplexAvoidEntity{ m_MoveState, m_Entity, m_TargetPos }; } // 0x66D0C0
    bool      MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override; // 0x66AD40
    CTask*    CreateNextSubTask(CPed* ped) override; // 0x66AD70
    CTask*    CreateFirstSubTask(CPed* ped) override; // 0x672530
    CTask*    ControlSubTask(CPed* ped) override; // 0x66ADE0

    //! Make the ped look at the target point (if applicable)
    void LookAtTarget(CPed* ped); // 0x66AB40

    //! Stop looking (if we've started to)
    void StopLooking(CPed* ped); // 0x66AD10

    //! Compute the route round the entity (Into `m_RoutePoint1` and `m_RoutePoint2`). @return If a route is needed
    bool ComputeRoute(CPed* ped); // 0x66FD30

    //! Compute a sphere that contains all given entities (Up to 16)
    void ComputeBoundingSphere(CColSphere& out, CEntity* const* entities); // 0x66F6C0

private:
    CTaskComplexAvoidEntity* Constructor(eMoveState moveState, CEntity* entity, const CVector& targetPos) { this->CTaskComplexAvoidEntity::CTaskComplexAvoidEntity(moveState, entity, targetPos); return this; }

private:
    notsa::EntityRef<CEntity> m_Entity;  // 0x0C - The entity to avoid
    eMoveState   m_MoveState{};           // 0x10
    CVector      m_PedPos{};              // 0x14 - Position of the ped when the task was started
    CVector      m_TargetPos{};           // 0x20
    CVector      m_RoutePoint2{};         // 0x2C - The point to go to (Initially the target)
    CVector      m_RoutePoint1{};         // 0x38
    int32        field_44{};              // 0x44
    int32        field_48{};              // 0x48
    uint8        field_4C{};              // 0x4C
    uint8        field_4D{};              // 0x4D
    uint8        _pad4E[2]{};             // 0x4E
    bool         m_IsLookingAt : 1{};     // 0x50 (bit 0)
};
VALIDATE_SIZE(CTaskComplexAvoidEntity, 0x54);
