#include "StdInc.h"

#include "AttractorScanner.h"
#include "Scripts/Scripted2dEffects.h"
#include "Events/EventAttractor.h"
#include "Events/EventScriptedAttractor.h"
#include "Plugins/TwoDEffectPlugin/2dEffect.h"
#include "World.h"
#include "Sector.h"
#include "RepeatSector.h"

static_assert(sizeof(C2dEffect) == 0x40 && sizeof(tUserList) == 0x24); // Strides used by the original (0xC3AB00, 0xC3A200)

void CAttractorScanner::InjectHooks() {
    RH_ScopedClass(CAttractorScanner);
    RH_ScopedCategory("Scanners");

    RH_ScopedInstall(Clear, 0x5FFF90);
    RH_ScopedInstall(ScanForAttractors, 0x6060A0);
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
                // 0x5FE960 - (this = &userList, ped type)
                if (!plugin::CallMethodAndReturn<bool, 0x5FE960, tUserList*, int32>(&userList, ped.m_nPedType)) {
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
void CAttractorScanner::ScanForAttractorsInPtrList(const void* ptrList, const CPed& ped) {
    plugin::CallMethod<0x6034B0, CAttractorScanner*, const void*, const CPed*>(this, ptrList, &ped);
}

// 0x5FFFD0
void CAttractorScanner::AddEffect(C2dEffect* effect, CEntity* entity, const CPed& ped) {
    plugin::CallMethod<0x5FFFD0, CAttractorScanner*, C2dEffect*, CEntity*, const CPed*>(this, effect, entity, &ped);
}

// 0x600180
void CAttractorScanner::GetBestEffect(C2dEffect*& outEffect, CEntity*& outEntity) {
    plugin::CallMethod<0x600180, CAttractorScanner*, C2dEffect**, CEntity**>(this, &outEffect, &outEntity);
}

// 0x603570
CPed* CAttractorScanner::GetClosestPedToEffect(C2dEffect* effect) {
    return plugin::CallAndReturn<CPed*, 0x603570, C2dEffect*>(effect);
}
