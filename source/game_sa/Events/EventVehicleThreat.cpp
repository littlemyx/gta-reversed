#include "StdInc.h"
#include "EventVehicleThreat.h"

void CEventVehicleThreat::InjectHooks() {
    RH_ScopedVirtualClass(CEventVehicleThreat, 0x85B298, 17);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x4B6350);
    RH_ScopedVMTInstall(GetEventType, 0x4AFBC0);
    RH_ScopedVMTInstall(GetEventPriority, 0x4AFBE0);
    RH_ScopedVMTInstall(GetLifeTime, 0x4AFBD0);
    RH_ScopedVMTInstall(AffectsPed, 0x4AFC50);
    RH_ScopedVMTInstall(CloneEditable, 0x4B7520);
}


CEventVehicleThreat::CEventVehicleThreat(CVehicle* vehicle) : CEventEditableResponse() {
    m_Vehicle = vehicle;
    CEntity::SafeRegisterRef(m_Vehicle);
}

CEventVehicleThreat::~CEventVehicleThreat() {
    CEntity::SafeCleanUpRef(m_Vehicle);
}

bool CEventVehicleThreat::AffectsPed(CPed* ped) {
    return ped->IsAlive();
}

