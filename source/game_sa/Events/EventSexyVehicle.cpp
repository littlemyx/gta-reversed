#include "StdInc.h"
#include "EventSexyVehicle.h"

void CEventSexyVehicle::InjectHooks() {
    RH_ScopedVirtualClass(CEventSexyVehicle, 0x85B0F8, 16);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x4B6170);
    RH_ScopedVMTInstall(GetEventType, 0x4AF070);
    RH_ScopedVMTInstall(GetEventPriority, 0x4AF090);
    RH_ScopedVMTInstall(GetLifeTime, 0x4AF080);
    RH_ScopedVMTInstall(Clone, 0x4B72F0);
    RH_ScopedVMTInstall(AffectsPed, 0x4AF100);
}


CEventSexyVehicle::CEventSexyVehicle(CVehicle* vehicle) : CEvent() {
    m_vehicle = vehicle;
    CEntity::SafeRegisterRef(m_vehicle);
}

CEventSexyVehicle::~CEventSexyVehicle() {
    CEntity::SafeCleanUpRef(m_vehicle);
}

bool CEventSexyVehicle::AffectsPed(CPed* ped) {
    return ped->IsAlive() && m_vehicle;
}

