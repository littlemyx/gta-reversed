#include "StdInc.h"

#include "EmergencyPed.h"

void CEmergencyPed::InjectHooks() {
    RH_ScopedVirtualClass(CEmergencyPed, 0x86C200, 26);
    RH_ScopedCategory("Entity/Ped");

    RH_ScopedVMTDestructorInstall(0x5DE3E0);
    RH_ScopedVMTInstall(ProcessControl, 0x5DE400);
}

// 0x5DE340
CEmergencyPed::CEmergencyPed(ePedType pedType, uint32 modelIndex) : CPed(pedType) {
    SetModelIndex(modelIndex); // V1053 Calling the 'SetModelIndex' virtual function in the constructor may lead to unexpected result at runtime
    m_pedSpeech.Initialise(this);
}

void CEmergencyPed::MedicAI() {
    // NOP
}

void CEmergencyPed::FiremanAI() {
    // NOP
}

// 0x5DE400
void CEmergencyPed::ProcessControl() {
    CPed::ProcessControl();
    if (GetWasPostponed() || m_nPedState == PEDSTATE_DEAD)
        return;

    auto& weapon = GetActiveWeapon();
    weapon.Update(nullptr);
    if (m_nPedState != PEDSTATE_DIE) {
        weapon.Update(nullptr);
        if (m_nPedType != PED_TYPE_MEDIC && m_nPedType == PED_TYPE_FIREMAN) { // todo: bad OG expression
            CPed::IsPedInControl();
        }
    }
}

// 0x5DE470
bool CEmergencyPed::InRange(CPed* ped) {
    if (!m_pVehicle)
        return true;

    return (m_pVehicle->GetPosition() - ped->GetPosition()).Magnitude() <= 30.0f;
}
