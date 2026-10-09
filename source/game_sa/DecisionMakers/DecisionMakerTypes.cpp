#include "StdInc.h"

#include "DecisionMakerTypes.h"
#include "PedStats.h"
#include "PedGroups.h"

void CDecisionMakerTypes::InjectHooks() {
    RH_ScopedClass(CDecisionMakerTypes);
    RH_ScopedCategory("DecisionMakers");

    RH_ScopedInstall(LoadEventIndices, 0x600840);
    RH_ScopedInstall(HasAnyEventResponse, 0x6042B0);
    RH_ScopedInstall(RemoveDecisionMaker, 0x6043A0);
    RH_ScopedInstall(FlushDecisionMakerEventResponse, 0x604490);
    RH_ScopedInstall(AddEventResponse, 0x6044C0);
    RH_ScopedInstall(GetInstance, 0x4684F0);
    RH_ScopedInstall(AddDecisionMaker, 0x607050);
    RH_ScopedOverloadedInstall(MakeDecision, "group", 0x606F80, eTaskType(CDecisionMakerTypes::*)(CPedGroup*, eEventType, int32, bool, eTaskType, eTaskType, eTaskType, eTaskType));
    RH_ScopedOverloadedInstall(MakeDecision, "ped", 0x606E70, void(CDecisionMakerTypes::*)(CPed*, eEventType, int32, bool, eTaskType, eTaskType, eTaskType, eTaskType, bool, int16&, int16&));
}

// 0x607050
int32 CDecisionMakerTypes::AddDecisionMaker(CDecisionMaker* decisionMaker, eDecisionTypes decisionMakerType, bool bDecisionMakerForMission) {
    // NOTE: Mission decision makers use slots [15, 20), the others [0, 15) (not [10, 20) like `eDecisionMakerType::MISSION0` suggests)
    const auto [begin, end] = bDecisionMakerForMission ? std::pair{15, 20} : std::pair{0, 15};
    for (auto i = begin; i < end; i++) {
        if (m_IsActive[i]) {
            continue;
        }
        m_IsActive[i]  = true;
        m_IsGroupDM[i] = decisionMakerType != 0; // The original stores the (byte) `decisionMakerType` as-is
        for (auto j = 0u; j < m_DecisionMakers[i].m_aDecisions.size(); j++) { // 0x6006B0
            m_DecisionMakers[i].m_aDecisions[j].From(decisionMaker->m_aDecisions[j]);
        }
        m_NoOfDecisionMakers++;
        return i;
    }
    return -1;
}

// 0x4684F0
CDecisionMakerTypes* CDecisionMakerTypes::GetInstance() {
    auto& instance = StaticRef<CDecisionMakerTypes*>(0xC0B030);
    if (!instance) {
        instance = new CDecisionMakerTypes(); // 0x4650F0 (ctor)
    }
    return instance;
}

// 0x606E70
void CDecisionMakerTypes::MakeDecision(CPed* ped, eEventType eventType, int32 eventSourceType, bool bIsPedInVehicle, eTaskType taskTypeToAvoid1, eTaskType taskTypeToAvoid2, eTaskType taskTypeToAvoid3, eTaskType taskTypeToSeek, bool bUseInGroupDecisionMaker, int16& taskType, int16& facialTaskType) {
    const auto intel = ped->GetIntelligence();
    const auto dmType = bUseInGroupDecisionMaker
        ? intel->m_nDecisionMakerTypeInGroup
        : intel->m_nDecisionMakerType;
    const auto eventIdx = m_EventIndices[eventType];

    taskType       = 200;
    facialTaskType = -1;

    // NOTSA: Original calls the (unreversed, trivially forwarding) `CDecisionMaker::MakeDecision` @ 0x606060 for the default decision makers
    if (dmType == -2) { // 0x606EBD
        m_DefaultPlayerPedDecisionMaker.m_aDecisions[eventIdx].MakeDecision(eventSourceType, bIsPedInVehicle, taskTypeToAvoid1, taskTypeToAvoid2, taskTypeToAvoid3, taskTypeToSeek, taskType, facialTaskType);
    } else if (dmType == -1) { // 0x606EF5
        auto& dm = ped->IsCreatedBy(PED_MISSION)
            ? m_DefaultMissionPedDecisionMaker
            : m_DefaultRandomPedDecisionMaker;
        dm.m_aDecisions[eventIdx].MakeDecision(eventSourceType, bIsPedInVehicle, taskTypeToAvoid1, taskTypeToAvoid2, taskTypeToAvoid3, taskTypeToSeek, taskType, facialTaskType);
    } else { // 0x606F3F
        m_DecisionMakers[dmType].m_aDecisions[eventIdx].MakeDecision(eventSourceType, bIsPedInVehicle, taskTypeToAvoid1, taskTypeToAvoid2, taskTypeToAvoid3, taskTypeToSeek, taskType, facialTaskType);
    }
}

// 0x6043A0
void CDecisionMakerTypes::RemoveDecisionMaker(eDecisionTypes dm) {
    if (!m_IsActive[dm]) {
        return;
    }
    if (!m_IsGroupDM[dm]) {
        for (auto i = GetPedPool()->GetSize(); i --> 0;) { // Original iterates backwards
            auto* const ped = GetPedPool()->GetAt(i);
            if (ped && ped->GetIntelligence()->m_nDecisionMakerType == dm) {
                ped->GetIntelligence()->SetPedDecisionMakerType(ped->m_pStats->m_nDefaultDecisionMaker);
            }
        }
    } else {
        for (auto i = 0u; i < CPedGroups::ms_activeGroups.size(); i++) {
            if (CPedGroups::ms_activeGroups[i] && CPedGroups::ms_groups[i].GetIntelligence().GetGroupDecisionMakerType() == (eDecisionMakerType)+dm) {
                CPedGroups::ms_groups[i].GetIntelligence().SetGroupDecisionMakerType(eDecisionMakerType::UNKNOWN);
            }
        }
    }
    m_IsActive[dm]  = false;
    m_IsGroupDM[dm] = false;
    m_NoOfDecisionMakers--;
}

// 0x606F80
eTaskType CDecisionMakerTypes::MakeDecision(CPedGroup* pedGroup, eEventType eventType, int32 eventSourceType, bool bIsPedInVehicle, eTaskType taskId1, eTaskType taskId2, eTaskType taskId3, eTaskType taskId4) {
    const auto eventIdx = m_EventIndices[eventType];
    const auto dmType   = (int32)pedGroup->GetIntelligence().GetGroupDecisionMakerType();

    int16 taskType       = 200;
    int16 facialTaskType; // Always written by `CDecision::MakeDecision`, and not used by the original

    CDecisionMaker* dm;
    if (dmType == -1) { // 0x606FA3
        dm = pedGroup->m_bIsMissionGroup
            ? &m_DefaultMissionPedGroupDecisionMaker
            : &m_DefaultRandomPedGroupDecisionMaker;
    } else { // 0x607002
        dm = &m_DecisionMakers[dmType];
    }
    dm->m_aDecisions[eventIdx].MakeDecision(eventSourceType, bIsPedInVehicle, taskId1, taskId2, taskId3, taskId4, taskType, facialTaskType);
    return (eTaskType)taskType;
}

// 0x6044C0
void CDecisionMakerTypes::AddEventResponse(int32 decisionMakerIndex, eEventType eventType, eTaskType taskId, float* responseChances, int32* flags) {
    m_DecisionMakers[decisionMakerIndex].m_aDecisions[m_EventIndices[eventType]].Add(taskId, responseChances, flags);
}

// 0x604490
void CDecisionMakerTypes::FlushDecisionMakerEventResponse(int32 decisionMakerIndex, eEventType eventId) {
    m_DecisionMakers[decisionMakerIndex].m_aDecisions[m_EventIndices[eventId]].SetDefault();
}

// 0x600840
void CDecisionMakerTypes::LoadEventIndices() {
    // 0x5BB9F0 is a __stdcall (RET 8) file loader: (int32 indices[], const char* filename), original passes the string at 0x86CD44
    reinterpret_cast<void(__stdcall*)(int32*, const char*)>(0x5BB9F0)(m_EventIndices.data(), "PedEvent.txt");
}

// 0x6042B0
// Returns true if the ped's decision maker has a non-default (any task != 200) decision for at least one of the given events.
bool CDecisionMakerTypes::HasAnyEventResponse(CPed* ped, const int32* eventTypes, int32 count) {
    const auto dmType = ped->GetIntelligence()->m_nDecisionMakerType;

    bool found = false;
    for (int32 i = 0; i < count && !found; i++) {
        const auto  eventIdx = m_EventIndices[eventTypes[i]];
        CDecision* decision;
        if (dmType == -2) {
            decision = &m_DefaultPlayerPedDecisionMaker.m_aDecisions[eventIdx];
        } else if (dmType == -1) {
            decision = ped->IsCreatedBy(PED_MISSION)
                ? &m_DefaultMissionPedDecisionMaker.m_aDecisions[eventIdx]
                : &m_DefaultRandomPedDecisionMaker.m_aDecisions[eventIdx];
        } else {
            decision = &m_DecisionMakers[dmType].m_aDecisions[eventIdx];
        }
        for (auto& task : decision->m_Tasks) {
            if (task != 200) {
                found = true;
                break;
            }
        }
    }
    return found;
}
