#include "StdInc.h"

#include "Event.h"
#include "EventGroup.h"

void CEventGroup::InjectHooks() {
    RH_ScopedVirtualClass(CEventGroup, 0x85AAB0, 1);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(Constructor, 0x4AB340);

    RH_ScopedOverloadedInstall(Add, "", 0x4AB420, CEvent*(CEventGroup::*)(CEvent*, bool));
    RH_ScopedInstall(HasScriptCommandOfTaskType, 0x4AB840);
    RH_ScopedInstall(HasEventOfType, 0x4AB5E0);
    RH_ScopedInstall(HasEvent, 0x4AB6A0);
    RH_ScopedInstall(GetHighestPriorityEvent, 0x4AB7C0);
    RH_ScopedInstall(TickEvents, 0x4AB6D0);
    RH_ScopedInstall(Remove, 0x4AB5A0);
    RH_ScopedInstall(RemoveInvalidEvents, 0x4AB760);
    RH_ScopedInstall(Reorganise, 0x4AB700);
    RH_ScopedInstall(Flush, 0x4AB370);
    RH_ScopedInstall(GetEventOfType, 0x4AB650);
}

// 0x4AB340
CEventGroup::CEventGroup(CPed* ped) :
    m_pPed{ped}
{
}

CEventGroup::~CEventGroup() {
    Flush();
}

// 0x4AB420
CEvent* CEventGroup::Add(CEvent* event, bool bValid) {
    if (m_pPed) {
        bool bAddToEventGroup = true; // exe [esp+0xf]: only an editable response event that will not respond (and is not a damage event that wants to be added) clears it
        if (event->HasEditableResponse()) {
            const auto eventResp = static_cast<CEventEditableResponse*>(event);

            const auto bIsEventDamage          = event->GetEventType() == EVENT_DAMAGE;
            const auto bInformGroup            = eventResp->ComputeResponseTaskOfType(m_pPed, TASK_SIMPLE_INFORM_GROUP);
            const auto bInformRespectedFriends = eventResp->ComputeResponseTaskOfType(m_pPed, TASK_SIMPLE_INFORM_RESPECTED_FRIENDS);
            const auto bTriggerLookAt          = eventResp->ComputeResponseTaskOfType(m_pPed, TASK_SIMPLE_LOOK_AT_ENTITY_OR_COORD);

            eventResp->ComputeResponseTaskType(m_pPed, false);

            bAddToEventGroup                   = eventResp->WillRespond() || (bIsEventDamage && eventResp->m_bAddToEventGroup);

            if (!event->AffectsPed(m_pPed)) {
                return nullptr;
            }
            if (bInformGroup) {
                eventResp->InformGroup(m_pPed);
            }
            if (bInformRespectedFriends && (!bIsEventDamage || !static_cast<CEventDamage*>(event)->m_bStealthMode)) {
                eventResp->InformRespectedFriends(m_pPed);
            }
            if (bTriggerLookAt) {
                eventResp->TriggerLookAt(m_pPed);
            }
            eventResp->InformVehicleOccupants(m_pPed);
        } else {
            if (!event->AffectsPed(m_pPed)) {
                return nullptr;
            }
        }
        m_pPed->GetIntelligence()->RecordEventForScript(event->GetEventType(), event->GetEventPriority());
        // exe 0x4AB549-0x4AB54F: the script record is made BEFORE the "will not be added" bail-out (the port returned first and never recorded such events)
        if (!bAddToEventGroup) {
            return nullptr;
        }
    }

    if (m_count >= TOTAL_EVENTS_PER_EVENTGROUP) {
        return nullptr;
    }

    const auto clonedEvent = event->Clone();
    clonedEvent->m_bValid  = bValid;
    if (m_pPed) {
        clonedEvent->ReportCriminalEvent(m_pPed);
    }
    m_events[m_count++] = clonedEvent;
    
    return clonedEvent;
}

// 0x4AB840
bool CEventGroup::HasScriptCommandOfTaskType(eTaskType taskId) {
    for (auto& event : GetEvents()) {
        if (const auto eScriptCmd = notsa::dyn_cast<CEventScriptCommand>(event)) {
            if (eScriptCmd->m_task && eScriptCmd->m_task->GetTaskType() == taskId) {
                return true;
            }
        }
    }
    return false;
}

// 0x4AB5E0
bool CEventGroup::HasEventOfType(CEvent* eventOfType) const {
    if (!eventOfType) {
        return true; // 0x4AB5E7..0x4AB5EE: a null event is answered with `true` (the port dereferenced it)
    }
    return GetEventOfType(eventOfType->GetEventType()) != nullptr;
}

// 0x4AB7C0
CEvent* CEventGroup::GetHighestPriorityEvent() {
    CEvent* theEvent = nullptr;
    int32 highestPriority = -1;
    for (auto& event : GetEvents()) {
        int32 eventPriority = event->GetEventPriority();
        bool bIsPriorityGreater = false;
        if (event->GetEventType() == EVENT_SCRIPT_COMMAND) {
            if (eventPriority > highestPriority)
                bIsPriorityGreater = true;
        } else if (eventPriority >= highestPriority) {
            bIsPriorityGreater = true;
        }
        if (bIsPriorityGreater && event->AffectsPed(m_pPed)) {
            highestPriority = eventPriority;
            theEvent = event;
        }
    }
    return theEvent;
}

// 0x4AB6D0
void CEventGroup::TickEvents() {
    rng::for_each(GetEvents(), &CEvent::Tick);
}

// 0x4AB6A0
bool CEventGroup::HasEvent(CEvent* needle) {
    return notsa::contains(GetEvents(), needle);
}

// 0x4AB5A0
void CEventGroup::Remove(CEvent* event) {
    const auto it = rng::find(GetEvents(), event);
    if (it != GetEvents().end()) {
        delete std::exchange(*it, nullptr);
    }
}

// 0x4AB760
void CEventGroup::RemoveInvalidEvents(bool bEverythingButScriptEvents) {
    for (auto& event : GetEvents()) {
        if (!event) {
            continue;
        }
        if (event->IsValid(m_pPed)) {
            if (!bEverythingButScriptEvents || event->GetEventType() == EVENT_SCRIPT_COMMAND) {
                continue;
            }
        }
        delete std::exchange(event, nullptr);
    }
}

// 0x4AB700
void CEventGroup::Reorganise() {
    // 0x4AB700: the exe moves every non-null event into a local array and clears its slot, then writes them back contiguously: the slots behind the new count are null (a copy of the old array would leave stale duplicates there)
    CEvent* moved[TOTAL_EVENTS_PER_EVENTGROUP];
    size_t  n = 0;
    for (size_t i = 0; i < m_count; i++) {
        if (CEvent* const e = m_events[i]) {
            moved[n++] = e;
            m_events[i] = nullptr;
        }
    }
    m_count = n;
    for (size_t i = 0; i < n; i++) {
        m_events[i] = moved[i];
    }
}

// 0x4AB370
void CEventGroup::Flush(bool bKeepJoinGroupScriptCommands) {
    const auto eScriptCmdBeInGroup = [=, this]() -> CEvent* {
        if (bKeepJoinGroupScriptCommands) {
            for (auto& event : GetEvents()) {
                if (const auto eScriptCmd = notsa::dyn_cast<CEventScriptCommand>(event)) {
                    if (eScriptCmd->m_task && eScriptCmd->m_task->GetTaskType() == TASK_COMPLEX_BE_IN_GROUP) {
                        return std::exchange(event, nullptr); // null the slot out so it doesn't get deleted
                    }
                }
            }
        }
        return nullptr;
    }();

    for (auto& event : GetEvents()) {
        delete std::exchange(event, nullptr);
    }

    m_count = 0;
    if (eScriptCmdBeInGroup) {
        m_events[m_count++] = eScriptCmdBeInGroup;
    }
}

// 0x4AB650
CEvent* CEventGroup::GetEventOfType(eEventType type) const noexcept {
    for (auto& event : GetEvents()) {
        if (event->GetEventType() == type) {
            return event;
        }
    }
    return nullptr;
}
