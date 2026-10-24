#include "StdInc.h"
#include "EventPotentialWalkIntoBuilding.h"


#ifdef NOTSA_PORTABLE
// MAC-C3 (-Wuninitialized): the exe passes the (not yet constructed) base's own members to the base ctor, i.e. whatever the event's heap block held; the ctor then runs
// `CEntity::SafeRegisterRef(m_building)` on it. clang-only hazard: the portable allocator fills freed blocks with 0xFD, so that would register a reference through 0xFDFD.. -> null / zero.
CEventPotentialWalkIntoBuilding::CEventPotentialWalkIntoBuilding() : CEventBuildingCollision(0, 0.0f, nullptr, CVector{}, CVector{}, 0) {}
#else
CEventPotentialWalkIntoBuilding::CEventPotentialWalkIntoBuilding() : CEventBuildingCollision(0, 0.0f, m_building, m_impactNormal, m_impactPos, m_pieceType) {}
#endif

bool CEventPotentialWalkIntoBuilding::AffectsPed(CPed* ped) {
    if (ped->IsPlayer() || !ped->IsAlive() || ped->m_pAttachedTo)
        return false;

    if (m_pieceType == 1)
        return false;

    if (ped->GetEventHandler().GetCurrentEventType() != EVENT_BUILDING_COLLISION || m_building != ped->GetEventHandlerHistory().GetCurrentEvent()->GetSourceEntity()                                                                                     // +20
    && (ped->GetEventHandler().GetCurrentEventType() != EVENT_BUILDING_COLLISION || m_building != ped->GetEventHandlerHistory().GetCurrentEvent()->GetSourceEntity())
    ) {
        auto* task = ped->GetTaskManager().GetActiveTask();
        if (!task || task->GetTaskType() != TASK_COMPLEX_REACT_TO_GUN_AIMED_AT) {
            return true;
        }
    }
    return false;
}

