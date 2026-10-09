#include "StdInc.h"

#include "EventDraggedOutCar.h"

void CEventDraggedOutCar::InjectHooks() {
    RH_ScopedVirtualClass(CEventDraggedOutCar, 0x85AD30, 17);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x4B5A70);
    RH_ScopedVMTInstall(GetEventType, 0x4AD2E0);
    RH_ScopedVMTInstall(GetEventPriority, 0x4AD320);
    RH_ScopedVMTInstall(GetLifeTime, 0x4AD2F0);
    RH_ScopedVMTInstall(AffectsPed, 0x4AD3A0);
    RH_ScopedVMTInstall(AffectsPedGroup, 0x4AD3C0);
    RH_ScopedVMTInstall(GetSourceEntity, 0x4AD300);
    RH_ScopedVMTInstall(GetLocalSoundLevel, 0x4AD310);
    RH_ScopedVMTInstall(CloneEditable, 0x4B6DC0);
}

// 0x4AD250
CEventDraggedOutCar::CEventDraggedOutCar(CVehicle* vehicle, CPed* carjacker, bool IsDriverSeat) : CEventEditableResponse() {
    m_CarJacker = carjacker;
    m_Vehicle   = vehicle;
    m_IsDriverSeat = IsDriverSeat;
    CEntity::SafeRegisterRef(m_Vehicle);
    CEntity::SafeRegisterRef(m_CarJacker);
}

// 0x4AD330
CEventDraggedOutCar::~CEventDraggedOutCar() {
    CEntity::SafeCleanUpRef(m_Vehicle);
    CEntity::SafeCleanUpRef(m_CarJacker);
}

// 0x4AD3A0
bool CEventDraggedOutCar::AffectsPed(CPed* ped) {
    return ped->IsAlive() && m_CarJacker;
}

// 0x4AD3C0
bool CEventDraggedOutCar::AffectsPedGroup(CPedGroup* pedGroup) {
    return FindPlayerPed() == pedGroup->GetMembership().GetLeader();
}

// 0x4B6DC0
CEventEditableResponse* CEventDraggedOutCar::CloneEditable() const noexcept {
    return new CEventDraggedOutCar(m_Vehicle, m_CarJacker, m_IsDriverSeat);
}
