#include "StdInc.h"
#include "EventPotentialGetRunOver.h"

void CEventPotentialGetRunOver::InjectHooks() {
    RH_ScopedVirtualClass(CEventPotentialGetRunOver, 0x85AE88, 17);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x4B5EA0);
    RH_ScopedVMTInstall(GetEventType, 0x4AE210);
    RH_ScopedVMTInstall(GetEventPriority, 0x4AE260);
    RH_ScopedVMTInstall(GetLifeTime, 0x4AE220);
    RH_ScopedVMTInstall(AffectsPed, 0x4AE2D0);
    RH_ScopedVMTInstall(GetSourceEntity, 0x4AE230);
    RH_ScopedVMTInstall(GetLocalSoundLevel, 0x4AE250);
    RH_ScopedVMTInstall(CloneEditable, 0x4B6F40);
}


CEventPotentialGetRunOver::CEventPotentialGetRunOver(CVehicle* vehicle) {
    m_Vehicle = vehicle;
    CEntity::SafeRegisterRef(m_Vehicle);
}

CEventPotentialGetRunOver::~CEventPotentialGetRunOver() {
    CEntity::SafeCleanUpRef(m_Vehicle);
}

bool CEventPotentialGetRunOver::AffectsPed(CPed* ped) {
    return ped->IsAlive()
       && !ped->m_pAttachedTo
       && !ped->bInVehicle
       && m_Vehicle
       && !m_Vehicle->IsBoat();
}

