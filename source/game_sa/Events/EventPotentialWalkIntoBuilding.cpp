#include "StdInc.h"
#include "EventPotentialWalkIntoBuilding.h"


// The class does not exist in the PC exe (no vtable / ctor to compare; EVENT_POTENTIAL_WALK_INTO_BUILDING is console-only, see CEventBuildingCollision::AffectsPed).
// The console ctor handed the base its own not yet constructed members (indeterminate read + SafeRegisterRef on it); a null building / zero vectors is the only defined reading.
CEventPotentialWalkIntoBuilding::CEventPotentialWalkIntoBuilding() : CEventBuildingCollision(0, 0.0f, nullptr, CVector{}, CVector{}, 0) {}

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

