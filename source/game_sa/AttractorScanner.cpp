#include "StdInc.h"

#include "AttractorScanner.h"
#include "Scripts/Scripted2dEffects.h"
#include "Events/EventAttractor.h"
#include "Events/EventScriptedAttractor.h"
#include "Plugins/TwoDEffectPlugin/2dEffect.h"
#include "World.h"
#include "Sector.h"
#include "RepeatSector.h"
#include "PedAttractorManager.h"
#include "Models/ModelInfo.h"

static_assert(sizeof(C2dEffect) == 0x40 && sizeof(tUserList) == 0x24); // Strides used by the original (0xC3AB00, 0xC3A200)

// 0x5FE960 - `tUserList` is declared in Scripts/Scripted2dEffects.h (not our zone), so it's a local helper for now, the original is `__thiscall(tUserList*, int32) -> bool`
static bool IsPedTypeInUserList(const tUserList& list, int32 pedType) {
    if (!list.m_bUseList) {
        return true;
    }
    for (auto i = 0; i < 4; i++) {
        if (list.m_UserTypes[i] == -2 && list.m_UserTypesByPedType[i] == pedType) {
            return true;
        }
    }
    return false;
}

void CAttractorScanner::InjectHooks() {
    RH_ScopedClass(CAttractorScanner);
    RH_ScopedCategory("Scanners");

    RH_ScopedInstall(Clear, 0x5FFF90);
    RH_ScopedInstall(ScanForAttractors, 0x6060A0);
    RH_ScopedInstall(ScanForAttractorsInPtrList, 0x6034B0);
    RH_ScopedInstall(AddEffect, 0x5FFFD0);
    RH_ScopedInstall(GetBestEffect, 0x600180);
    RH_ScopedInstall(GetClosestPedToEffect, 0x603570);
}

// 0x5FFF90
void CAttractorScanner::Clear() {
    for (auto i = 0; i < 10; i++) {
        field_18[i] = 0;
        field_40[i] = 0;
        field_68[i] = 15.0f * 15.0f; // 0x86C52C = 15.0f
        if (i != 4 && i != 7) {
            field_68[i] = 25.0f;
        }
    }
}

// 0x6060A0
void CAttractorScanner::ScanForAttractors(CPed& ped) {
    if (!field_0) {
        return;
    }
    if (ped.bInVehicle) {
        return;
    }

    auto* const intel = ped.GetIntelligence();

    // Don't scan while using an effect
    if (const auto activeTask = intel->m_TaskMgr.GetActiveTask(); activeTask && activeTask->GetTaskType() == TASK_COMPLEX_USE_EFFECT) {
        field_4.m_nStartTime = CTimer::GetTimeInMS();
        field_4.m_nInterval  = 3000;
        field_4.m_bStarted   = true; // Note: `m_bStopped` is not touched
        return;
    }

    if (!field_4.m_bStarted) {
        field_4.Start(1500);
        if (!field_4.m_bStarted) { // Always false
            return;
        }
    }
    if (!field_4.IsOutOfTime()) {
        return;
    }
    field_4.Start(1500);

    Clear();

    // Sector range, the original evaluates this on the x87 stack and passes a double to `floor` (0x8219F0)
    // `pos - 15.0f` for the lower bound, `15.0f + pos` for the upper one (0x86C52C = 15.0f, 0x858B38 = 0.02f, 0x858B34 = 60.0f)
    const auto  pos       = ped.GetPosition();
    const auto  ToSector  = [](double v) { return (int32)std::floor(v * (double)0.02f + 60.0); };
    const int32 sectorX0  = ToSector((double)pos.x - 15.0);
    const int32 sectorY0  = ToSector((double)pos.y - 15.0);
    const int32 sectorX1  = ToSector(15.0 + (double)pos.x);
    const int32 sectorY1  = ToSector(15.0 + (double)pos.y);
    const int32 minX      = sectorX0 > 0 ? sectorX0 : 0;
    const int32 minY      = sectorY0 > 0 ? sectorY0 : 0;
    const int32 maxX      = sectorX1 < MAX_SECTORS_X - 1 ? sectorX1 : MAX_SECTORS_X - 1;
    const int32 maxY      = sectorY1 < MAX_SECTORS_Y - 1 ? sectorY1 : MAX_SECTORS_Y - 1;

    for (int32 y = minY; y <= maxY; y++) {
        for (int32 x = minX; x <= maxX; x++) {
            const auto cx = std::min(std::max(x, 0), MAX_SECTORS_X - 1);
            const auto cy = std::min(std::max(y, 0), MAX_SECTORS_Y - 1);
            ScanForAttractorsInPtrList(&CWorld::ms_aSectors[cy][cx].Buildings, ped);          // 0x6063D2
            ScanForAttractorsInPtrList(&CWorld::ms_aRepeatSectors[y & 0xF][x & 0xF].Objects, ped); // 0x6063EF
        }
    }

    // Scripted effects
    for (auto i = 0; i < NUM_SCRIPTED_2D_EFFECTS; i++) {
        if (!CScripted2dEffects::ms_activated[i]) {
            continue;
        }
        auto& userList = CScripted2dEffects::ms_userLists[i];
        if (userList.m_bUseList) {
            const auto modelId = (int32)(int16)ped.m_nModelIndex; // Sign extended
            if (rng::none_of(userList.m_UserTypes, [&](int32 t) { return t == modelId; })) {
                if (!IsPedTypeInUserList(userList, ped.m_nPedType)) { // 0x5FE960
                    continue;
                }
            }
        }
        AddEffect(&CScripted2dEffects::ms_effects[i], nullptr, ped); // 0x606463
    }

    C2dEffect* effect = nullptr;
    CEntity*   entity = nullptr;
    GetBestEffect(effect, entity); // 0x600180
    if (!effect) {
        return;
    }
    if (effect == m_pEffectInUse && m_pEntityInUse == entity) {
        return;
    }

    const auto attractor = reinterpret_cast<C2dEffectPedAttractor*>(effect);
    if (attractor->m_nAttractorType == PED_ATTRACTOR_SCRIPTED) {
        if (GetClosestPedToEffect(effect) != &ped) { // 0x603570
            return;
        }
        CEventScriptedAttractor event{attractor, entity, false};
        if (intel->m_eventGroup.Add(&event, false)) {
            m_pEntityInUse = entity;
            m_pEffectInUse = effect;
        }
    } else {
        CEventAttractor event{attractor, entity, false};
        if (intel->m_eventGroup.Add(&event, false)) {
            m_pEntityInUse = entity;
            m_pEffectInUse = effect;
        }
    }
}

// 0x6034B0
void CAttractorScanner::ScanForAttractorsInPtrList(const void* ptrList, CPed& ped) {
    // Note: Works for both single and double linked lists (item @ +0, next @ +4)
    // Note: `enableDisabled` is deliberately kept across iterations, the original keeps it in a register (`bl`),
    //       so non-object entities see the value of the last object
    bool enableDisabled = false;
    for (auto* node = static_cast<const CPtrListSingleLink<CEntity*>*>(ptrList)->GetNode(); node;) {
        CEntity* const entity = node->Item;
        node = node->Next; // The original reads this before processing the entity

        if (entity->GetType() == ENTITY_TYPE_OBJECT) {
            const auto* const obj = entity->AsObject();
            enableDisabled = obj->objectFlags.bEnableDisabledAttractors;
            if (!entity->m_bIsStatic && !entity->m_bIsStaticWaitingForCollision) {
                continue;
            }
            if (obj->objectFlags.bIsExploded) {
                continue;
            }
        }

        auto* const mi = CModelInfo::ms_modelInfoPtrs[(int16)entity->m_nModelIndex]; // Sign extended
        for (auto i = 0; i < mi->m_n2dfxCount; i++) {
            auto* const effect = mi->Get2dEffect(i);
            if (effect->m_Type != EFFECT_ATTRACTOR) {
                continue;
            }
            if ((effect->pedAttractor.m_nFlags & 1) && !enableDisabled) {
                continue;
            }
            AddEffect(effect, entity, ped); // 0x603543
        }
    }
}

// 0x5FFFD0
void CAttractorScanner::AddEffect(C2dEffect* effect, CEntity* entity, CPed& ped) {
    const auto type = (uint8)effect->pedAttractor.m_nAttractorType; // The original reads this as a byte (@ +0x34)

    // Shelter attractors only when it's raining, everything else only when it's not (0x858CC4 = 0.2f)
    const bool isNotRaining = !(CWeather::Rain >= 0.2f);
    if (isNotRaining == (type == PED_ATTRACTOR_SHELTER)) {
        return;
    }

    const CVector effectPos = entity ? entity->TransformFromObjectSpace(effect->m_Pos) : effect->m_Pos; // 0x60003E
    const CVector pedPos    = ped.GetPosition();

    // The original keeps the sum on the x87 stack (extended precision): `(dz^2 + dy^2) + dx^2`
    const double dx       = (double)pedPos.x - (double)effectPos.x;
    const double dy       = (double)pedPos.y - (double)effectPos.y;
    const double dz       = (double)pedPos.z - (double)effectPos.z;
    const double distSq   = (dz * dz + dy * dy) + dx * dx;
    const float  distSqF  = (float)distSq;
    if (!(distSq < (double)field_68[type])) {
        return;
    }

    if (type == PED_ATTRACTOR_SCRIPTED) {
        const auto radius = CScripted2dEffects::ms_radii[CScripted2dEffects::GetIndex(reinterpret_cast<C2dEffectPedAttractor*>(effect))]; // 0x6F9F60
        if (!(radius < 0.0f)) { // 0x858B50 = 0.0f
            if (!((double)distSqF < (double)radius * (double)radius)) {
                return;
            }
        }
    }

    if (!GetPedAttractorManager()->HasEmptySlot(reinterpret_cast<C2dEffectPedAttractor*>(effect), entity)) { // 0x5EBB00
        return;
    }

    const auto mat = entity ? CMatrix{entity->GetMatrix()} : CMatrix::Identity(); // 0x60010F | 0x600120
    if (CPedAttractorManager::IsApproachable(reinterpret_cast<C2dEffectPedAttractor*>(effect), mat, 0, &ped)) { // 0x60013B
        field_68[type] = distSqF;
        field_18[type] = (int32)entity;
        field_40[type] = (int32)effect;
    }
}

// 0x600180
void CAttractorScanner::GetBestEffect(C2dEffect*& outEffect, CEntity*& outEntity) {
    outEffect = nullptr;
    outEntity = nullptr;

    // BUG: Checks the entity of slot 4, not the effect (scripted effects have no entity), but slot 4 effects always have one
    if (field_18[4]) {
        outEffect = (C2dEffect*)field_40[4];
        outEntity = (CEntity*)field_18[4];
        return;
    }

    float best = std::numeric_limits<float>::max(); // 0x863A3C
    for (auto i = 0; i < 10; i++) {
        if (best > field_68[i] && field_40[i]) {
            best      = field_68[i];
            outEffect = (C2dEffect*)field_40[i];
            outEntity = (CEntity*)field_18[i];
        }
    }
}

// 0x603570
CPed* CAttractorScanner::GetClosestPedToEffect(C2dEffect* effect) {
    float best    = std::numeric_limits<float>::max();
    CPed* closest = nullptr;

    auto* const pool = GetPedPool();
    for (auto i = pool->GetSize(); i-- > 0;) {
        auto* const ped = pool->GetAt(i);
        if (!ped) {
            continue;
        }

        // Skip peds that are already using an effect
        if (const auto task = ped->GetTaskManager().GetActiveTask(); task && task->GetTaskType() == TASK_COMPLEX_USE_EFFECT) { // 0x681720
            continue;
        }

        const CVector pedPos = ped->GetPosition();
        const double  dx     = (double)effect->m_Pos.x - (double)pedPos.x;
        const double  dy     = (double)effect->m_Pos.y - (double)pedPos.y;
        const double  dz     = (double)effect->m_Pos.z - (double)pedPos.z;
        const float   distSq = (float)((dz * dz + dy * dy) + dx * dx); // Extended precision sum, rounded on store
        if (!(distSq < best)) {
            continue;
        }

        const auto& userList = CScripted2dEffects::ms_userLists[CScripted2dEffects::GetIndex(reinterpret_cast<C2dEffectPedAttractor*>(effect))]; // 0x6F9F60
        if (userList.m_bUseList) {
            const auto modelId = (int32)(int16)ped->m_nModelIndex; // Sign extended
            if (rng::none_of(userList.m_UserTypes, [&](int32 t) { return t == modelId; })) {
                if (!IsPedTypeInUserList(userList, ped->m_nPedType)) {
                    continue;
                }
            }
        }

        if (CPedAttractorManager::IsApproachable(reinterpret_cast<C2dEffectPedAttractor*>(effect), CMatrix::Identity(), 0, ped)) { // 0x6036CE
            best    = distSq;
            closest = ped;
        }
    }
    return closest;
}
