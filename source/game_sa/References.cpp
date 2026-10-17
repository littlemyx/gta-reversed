#include "StdInc.h"

#include "References.h"

void CReferences::InjectHooks() {
    RH_ScopedClass(CReferences);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x5719B0);
    RH_ScopedInstall(ListSize, 0x5719E0);
    RH_ScopedInstall(RemoveReferencesToPlayer, 0x571AD0);
    RH_ScopedInstall(PruneAllReferencesInWorld, 0x571CC0);
}

// 0x5719B0
void CReferences::Init() {
    ZoneScoped;

    pEmptyList = aRefs;

    // todo: do better
    for (int32 i = 0; i < MAX_NUM_REFERENCES - 1; ++i) {
        (&aRefs[i])->m_pNext = &aRefs[i + 1];
        (&aRefs[i])->m_ppEntity = nullptr;
    }

    (&aRefs[MAX_NUM_REFERENCES - 1])->m_pNext = nullptr;
}

uint32 CReferences::ListSize(CReference* ref) {
    uint32 count = 0;
    while (ref) {
        ref = ref->m_pNext;
        ++count;
    }

    return count;
}

void CReferences::RemoveReferencesToPlayer() {
    auto vehicle = FindPlayerVehicle();
    if (vehicle)
        vehicle->ResolveReferences();

    auto player = FindPlayerPed();
    if (player) {
        player->ResolveReferences();
        auto& group = CPedGroups::GetGroup(player->GetPlayerData()->m_nPlayerGroup);
        group.GetMembership().SetLeader(player);
        group.Process();
    }
}

void CReferences::PruneAllReferencesInWorld() {
    // 0x571CC0: every pool from its LAST slot down to the first (peds, vehicles, objects); the order decides the order of the nodes on the free list
    const auto PruneAll = [](auto* pool) {
        for (auto i = (int32)pool->GetSize(); i-- > 0;) {
            if (auto* const e = pool->GetAt(i)) {
                e->PruneReferences();
            }
        }
    };
    PruneAll(GetPedPool());
    PruneAll(GetVehiclePool());
    PruneAll(GetObjectPool());
}
