#include "StdInc.h"

#include "EntityScanner.h"

void CEntityScanner::InjectHooks() {
    RH_ScopedVirtualClass(CEntityScanner, 0x86CD40, 1);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x5FF990);
    RH_ScopedInstall(Destructor, 0x603480);

    RH_ScopedInstall(Clear, 0x5FF9D0);
    RH_ScopedVMTInstall(ScanForEntitiesInRange, 0x5FFA20);
}

// 0x5FF990
CEntityScanner::CEntityScanner() {
    m_timer = {};

    rng::fill(m_apEntities, nullptr);
    m_pClosestEntityInRange = nullptr;

    m_timer.SetPeriod(MAX_NUM_ENTITIES);
}

// 0x603480
CEntityScanner::~CEntityScanner() {
    Clear();
}

// 0x5FF9D0
void CEntityScanner::Clear() {
    for (auto& entity : m_apEntities) {
        CEntity::ClearReference(entity);
    }

    CEntity::ClearReference(m_pClosestEntityInRange);
}

// 0x5FFA20
void CEntityScanner::ScanForEntitiesInRange(const eRepeatSectorList sectorList, const CPed& ped) {
    if (!m_timer.Tick()) {
        return;
    }

    Clear();

    const bool isPlayerPed = FindPlayerPed(-1) == &ped;

    const auto* const intel = ped.m_pIntelligence;
    const float       range = intel->m_fHearingRange > intel->m_fSeeingRange // max(hearing range, seeing range)
        ? intel->m_fHearingRange
        : intel->m_fSeeingRange;

    const CVector pedPos = ped.GetPosition();

    std::array<float, MAX_NUM_ENTITIES> distances;
    distances.fill(FLT_MAX);
    int32 numFound = 0;

    // Sector coordinates of the bounding box around the ped
    const auto ToSector = [](float coord) { return (int32)std::floor(coord * 0.02f + 60.f); };
    const int32 minX = std::max(0, ToSector(pedPos.x - range));
    const int32 minY = std::max(0, ToSector(pedPos.y - range));
    const int32 maxX = std::min((int32)MAX_SECTORS_X - 1, ToSector(range + pedPos.x));
    const int32 maxY = std::min((int32)MAX_SECTORS_Y - 1, ToSector(range + pedPos.y));

    CWorld::AdvanceCurrentScanCode();
    const_cast<CPed&>(ped).SetScanCode(CWorld::ms_nCurrentScanCode); // NOTE: Original code modifies the ped here

    const auto ProcessList = [&](auto& list) {
        for (CEntity* const entity : list) {
            if (entity->GetScanCode() == CWorld::ms_nCurrentScanCode) {
                continue;
            }
            entity->SetScanCode(CWorld::ms_nCurrentScanCode);

            // Ignore dead peds (unless it's the player scanning)
            if (sectorList == REPEATSECTOR_PEDS && !isPlayerPed && entity->AsPed()->m_nPedState == PEDSTATE_DEAD) {
                continue;
            }

            const CVector entityPos = entity->GetPosition();
            const float   dx        = entityPos.x - pedPos.x;
            const float   dy        = entityPos.y - pedPos.y;
            const float   dz        = entityPos.z - pedPos.z;
            const float   distSq    = dz * dz + dx * dx + dy * dy;
            if (!(distSq < range * range)) {
                continue;
            }

            for (int32 i = 0; i < (int32)MAX_NUM_ENTITIES; i++) {
                if (!m_apEntities[i]) { // Free slot
                    distances[i]     = distSq;
                    m_apEntities[i]  = entity;
                    numFound++;
                    break;
                }
                if (distSq < distances[i]) { // Closer than the one in this slot => insert here, shifting the rest
                    for (int32 j = std::min(numFound, (int32)MAX_NUM_ENTITIES - 1); j > i; j--) {
                        m_apEntities[j] = m_apEntities[j - 1];
                        distances[j]    = distances[j - 1];
                    }
                    distances[i]    = distSq;
                    m_apEntities[i] = entity;
                    if (numFound < (int32)MAX_NUM_ENTITIES) {
                        numFound++;
                    }
                    break;
                }
            }
        }
    };

    for (int32 y = minY; y <= maxY; y++) {
        for (int32 x = minX; x <= maxX; x++) {
            auto& sector = CWorld::GetRepeatSector(x, y);
            switch (sectorList) {
            case REPEATSECTOR_VEHICLES: ProcessList(sector.Vehicles); break;
            case REPEATSECTOR_PEDS:     ProcessList(sector.Peds);     break;
            case REPEATSECTOR_OBJECTS:  ProcessList(sector.Objects);  break;
            default:                    break; // Original code uses a null list => no entities
            }
        }
    }

    for (int32 i = 0; i < numFound; i++) {
        if (m_apEntities[i]) {
            m_apEntities[i]->RegisterReference(&m_apEntities[i]);
        }
    }

    if (m_apEntities[0]) {
        m_pClosestEntityInRange = m_apEntities[0];
        m_pClosestEntityInRange->RegisterReference(&m_pClosestEntityInRange);
    }
}
