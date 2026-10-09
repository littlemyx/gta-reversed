#include "StdInc.h"

#include "EventGotKnockedOverByCar.h"

void CEventGotKnockedOverByCar::InjectHooks() {
    RH_ScopedVirtualClass(CEventGotKnockedOverByCar, 0x85B618, 17);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x4B67A0);
    RH_ScopedVMTInstall(GetEventType, 0x4B1BD0);
    RH_ScopedVMTInstall(GetEventPriority, 0x4B1C00);
    RH_ScopedVMTInstall(GetLifeTime, 0x4B1BE0);
    RH_ScopedVMTInstall(AffectsPed, 0x4B1C70);
    RH_ScopedVMTInstall(GetSourceEntity, 0x4B1CA0);
    RH_ScopedVMTInstall(GetLocalSoundLevel, 0x4B1BF0);
    RH_ScopedVMTInstall(CloneEditable, 0x4B7960);
}

// 0x4B1B60
CEventGotKnockedOverByCar::CEventGotKnockedOverByCar(CVehicle* vehicle) {
    m_vehicle = vehicle;
    CEntity::SafeRegisterRef(m_vehicle);
}

CEventGotKnockedOverByCar::~CEventGotKnockedOverByCar() {
    CEntity::SafeCleanUpRef(m_vehicle);
}

// 0x4B1C70
bool CEventGotKnockedOverByCar::AffectsPed(CPed* ped) {
    if (m_vehicle && !ped->IsPlayer())
        return ped->IsAlive();
    return false;
}

// 0x4B7960
CEventEditableResponse* CEventGotKnockedOverByCar::CloneEditable() const noexcept {
    return new CEventGotKnockedOverByCar(m_vehicle);
}
