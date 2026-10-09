#include "StdInc.h"
#include "EventPedEnteredMyVehicle.h"

void CEventPedEnteredMyVehicle::InjectHooks() {
    RH_ScopedVirtualClass(CEventPedEnteredMyVehicle, 0x85AFE8, 17);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x4B6030);
    RH_ScopedVMTInstall(GetEventType, 0x4AEB40);
    RH_ScopedVMTInstall(GetEventPriority, 0x4AEB80);
    RH_ScopedVMTInstall(GetLifeTime, 0x4AEB50);
    RH_ScopedVMTInstall(AffectsPed, 0x4AEC00);
    RH_ScopedVMTInstall(GetSourceEntity, 0x4AEB60);
    RH_ScopedVMTInstall(GetLocalSoundLevel, 0x4AEB70);
    RH_ScopedVMTInstall(CloneEditable, 0x4B7170);
}


CEventPedEnteredMyVehicle::CEventPedEnteredMyVehicle(CPed* pedThatEntered, CVehicle* vehicle, eTargetDoor targetDoor) :
    m_Vehicle{vehicle},
    m_PedThatEntered{pedThatEntered},
    m_TargetDoor{targetDoor}
{
    CEntity::SafeRegisterRef(m_PedThatEntered);
    CEntity::SafeRegisterRef(m_Vehicle);
}

CEventPedEnteredMyVehicle::~CEventPedEnteredMyVehicle() {
    CEntity::SafeCleanUpRef(m_PedThatEntered);
    CEntity::SafeCleanUpRef(m_Vehicle);
}

bool CEventPedEnteredMyVehicle::AffectsPed(CPed* ped) {
    return ped->IsAlive()
        && ped->m_pVehicle
        && ped->bInVehicle
        && ped->m_pVehicle == m_Vehicle
        && m_PedThatEntered;
}

