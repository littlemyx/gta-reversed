#include "StdInc.h"
#include "PedSaveStructure.h"

#include "EntryExitManager.h"
#include "EntryExit.h"
#include "Streaming.h"
#include "WeaponInfo.h"

void CPedSaveStructure::InjectHooks() {
    RH_ScopedClass(CPedSaveStructure);
    RH_ScopedCategory("Entity/Ped");

    RH_ScopedInstall(Construct, 0x5D43D0);
    RH_ScopedInstall(Extract, 0x5D44B0);
}

// 0x5D44B0
void CPedSaveStructure::Extract(CPed* ped) {
    ped->GetPosition() = m_pos; // Matrix position if there is one, else placement
    ped->m_fHealth = m_health;
    ped->m_fArmour = m_armor;
    ped->m_nActiveWeaponSlot = m_activeWeaponSlot;
    ped->SetCharCreatedBy(m_createdBy);
    ped->m_nFightingStyle = (eFightingStyle)m_nFightingStyle;
    ped->m_nAllowedAttackMoves = m_nAllowedAttackMoves;

    for (auto&& weapon : m_weapons) {
        const auto slot = (size_t)(&weapon - m_weapons);
        if (weapon.m_Type == WEAPON_UNARMED) {
            continue;
        }

        const auto* info = CWeaponInfo::GetWeaponInfo(weapon.m_Type, eWeaponSkill::STD);
        if (info->m_nModelId1 != MODEL_INVALID) {
            CStreaming::RequestModel(info->m_nModelId1, STREAMING_KEEP_IN_MEMORY);
            CStreaming::LoadAllRequestedModels(false);
        }
        info = CWeaponInfo::GetWeaponInfo(weapon.m_Type, eWeaponSkill::STD);
        if (info->m_nModelId2 != MODEL_INVALID) {
            CStreaming::RequestModel(info->m_nModelId2, STREAMING_KEEP_IN_MEMORY);
            CStreaming::LoadAllRequestedModels(false);
        }
        ped->GiveWeapon(weapon.m_Type, weapon.m_TotalAmmo, false);
        ped->m_aWeapons[slot].m_AmmoInClip = weapon.m_AmmoInClip; // NOTE: indexed by the loop position, as the original does (not by the slot GiveWeapon returned)
    }

    ped->SetCurrentWeapon((int32)m_activeWeaponSlot);
    ped->SetAreaCode((eAreaCodes)m_areaCode);

    if (m_nExitIndex == -1) {
        ped->m_pEnex = nullptr;
    } else {
        auto* const pool = CEntryExitManager::GetPool();
        ped->m_pEnex = pool->IsFreeSlotAtIndex(m_nExitIndex) ? nullptr : pool->GetAt(m_nExitIndex);
    }
}

// 0x5D43D0
void CPedSaveStructure::Construct(CPed* ped) {
    m_pos            = ped->GetPosition();
    m_health         = ped->m_fHealth;
    m_armor          = ped->m_fArmour;
    m_createdBy      = (ePedCreatedBy)ped->GetCreatedBy();
    m_activeWeaponSlot = (int8)ped->m_nActiveWeaponSlot;
    m_areaCode       = (int8)ped->GetAreaCode();
    m_nFightingStyle = (uint8)ped->m_nFightingStyle;
    m_nAllowedAttackMoves = (uint8)ped->m_nAllowedAttackMoves;

    m_nExitIndex = -1;
    if (const auto* const enex = ped->m_pEnex) {
        const auto* const link = enex->m_pLink ? enex->m_pLink : enex;
        if (link->m_nArea != 0) {
            auto* const pool = CEntryExitManager::GetPool();
            m_nExitIndex = (int32)pool->GetIndex(enex);
        }
    }

    std::ranges::copy(ped->m_aWeapons, m_weapons);
}
