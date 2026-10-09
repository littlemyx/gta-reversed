#include "StdInc.h"

#include "PlayerRelationshipRecorder.h"
#include "TaskCategories.h"

void CPlayerRelationshipRecorder::InjectHooks() {
    RH_ScopedClass(CPlayerRelationshipRecorder);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x61A130);
    RH_ScopedInstall(Destructor, 0x61A2C0);
    RH_ScopedInstall(Flush, 0x61A2A0);
    RH_ScopedInstall(ClearRelationshipWithPlayer, 0x61A150);
    RH_ScopedInstall(AddRelationship, 0x61A180);
    RH_ScopedInstall(GetRelationshipWithPlayer, 0x61A1A0);
    RH_ScopedInstall(RecordRelationshipWithPlayer, 0x61A1D0);
    RH_ScopedGlobalInstall(GetPlayerRelationshipRecorder, 0x61A2E0);
}

// 0x61A130
CPlayerRelationshipRecorder::CPlayerRelationshipRecorder() {
    Flush();
}

// 0x61A2C0
CPlayerRelationshipRecorder::~CPlayerRelationshipRecorder() {
    Flush();
}

// 0x61A2A0
void CPlayerRelationshipRecorder::Flush() {
    for (auto& relationship : m_Relationships) {
        relationship.Flush();
    }
}

// 0x61A180
void CPlayerRelationshipRecorder::AddRelationship(const CPed* ped, int32 value) {
    // BUG (original): only overwrites slot 0, and only if it is already occupied (`Ped != null`), so nothing is ever recorded.
    auto& rel = m_Relationships[0];
    if (rel.Ped) {
        rel.Ped = ped;
        rel.Relationship = (uint8)value;
    }
}

// 0x61A1D0
void CPlayerRelationshipRecorder::RecordRelationshipWithPlayer(const CPed* ped) {
    ClearRelationshipWithPlayer(ped); // Inlined

    const auto* const task = ped->GetTaskManager().GetActiveTask(); // 0x681720
    if (!task) {
        return;
    }

    // BUG (original): The first (and third) call below sets only the 2nd out parameter, but the 3rd one is tested, which
    // is always `false` (`IsFollowPedTask` always sets both to `false`). So nothing is ever recorded. Kept as is.
    bool isKill{}, unused{};
    CTaskCategories::IsKillPedTask(task, isKill, unused); // 0x6985E0
    if (!unused) {
        bool follow{}, unused2{};
        CTaskCategories::IsFollowPedTask(task, follow, unused2); // 0x698610
        if (!unused2) {
            bool isKill2{}, unused3{};
            CTaskCategories::IsKillPedTask(task, isKill2, unused3); // 0x6985E0
            if (unused3) {
                AddRelationship(ped, 7);
            }
            return;
        }
    }
    AddRelationship(ped, 3);
}

// 0x61A1A0
uint8 CPlayerRelationshipRecorder::GetRelationshipWithPlayer(const CPed* ped) {
    for (auto& relationship : m_Relationships) {
        if (relationship.Ped != ped)
            continue;

        return relationship.Relationship;
    }
    return 0;
}

// 0x61A150
void CPlayerRelationshipRecorder::ClearRelationshipWithPlayer(const CPed* ped) {
    for (auto& relationship : m_Relationships) {
        if (relationship.Ped != ped)
            continue;

        return relationship.Flush();
    }
}

// 0x61A2E0
CPlayerRelationshipRecorder& GetPlayerRelationshipRecorder() {
    static auto& g_sPlayerRelationshipRecorder = StaticRef<CPlayerRelationshipRecorder*>(0xC17084);

    if (!g_sPlayerRelationshipRecorder) {
        g_sPlayerRelationshipRecorder = new CPlayerRelationshipRecorder();
    }
    return *g_sPlayerRelationshipRecorder;
}
