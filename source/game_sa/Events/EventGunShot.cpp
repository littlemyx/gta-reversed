#include "StdInc.h"

#include "EventGunShot.h"

void CEventGunShot::InjectHooks() {
    RH_ScopedVirtualClass(CEventGunShot, 0x85ABE0, 17);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(Constructor, 0x4AC610);
    RH_ScopedVMTInstall(AffectsPed, 0x4B2CD0);
    RH_ScopedVMTInstall(IsCriminalEvent, 0x4AC810);
    RH_ScopedVMTInstall(TakesPriorityOver, 0x4AC780);
    RH_ScopedVMTInstall(CloneEditable, 0x4B6B20);
}

CEventGunShot* CEventGunShot::Constructor(CEntity* entity, CVector startPoint, CVector endPoint, bool bHasNoSound) { this->CEventGunShot::CEventGunShot(entity, startPoint, endPoint, bHasNoSound); return this; }

// 0x4AC610
CEventGunShot::CEventGunShot(CEntity* entity, CVector startPoint, CVector endPoint, bool bHasNoSound) : CEventEditableResponse() {
    m_startPoint = startPoint;
    m_endPoint = endPoint;
    m_firedBy = entity;
    m_bHasNoSound = bHasNoSound;
    CEntity::SafeRegisterRef(m_firedBy);
}

CEventGunShot::~CEventGunShot() {
    CEntity::SafeCleanUpRef(m_firedBy);
}

// 0x4B2CD0
bool CEventGunShot::AffectsPed(CPed* ped) {
    if (!m_firedBy)
        return false;

    if (m_firedBy->GetIsTypePed() && CPedGroups::AreInSameGroup(ped, m_firedBy->AsPed()))
        return false;

    if (!ped->IsInVehicleThatHasADriver()) {
        CWanted* playerWanted = FindPlayerWanted();
        if (ped->m_nPedType == PED_TYPE_COP && playerWanted->GetWantedLevel() > eWantedLevel::WANTED_CLEAN) {
            CCopPed* cop = static_cast<CCopPed*>(ped);
            if (playerWanted->IsInPursuit(cop) || playerWanted->CanCopJoinPursuit(cop)) {
                if (m_firedBy != FindPlayerPed())
                    return false;
            }
        }
        if (ped->IsAlive() && ped != m_firedBy && !ped->IsPlayer()) {
            float fGunShotRange = 45.0f;
            if (CEventGunShot::ms_fGunShotSenseRangeForRiot2 <= 0.0f) {
                if (ped->IsCreatedByMission()) {
                    fGunShotRange = ped->GetIntelligence()->GetPedFOVRange();
                }
            } else if (ped->IsCreatedByMission()) {
                fGunShotRange = CEventGunShot::ms_fGunShotSenseRangeForRiot2;
            }

            const auto& pedPos = ped->GetPosition();
            // exe 0x4B2DDE-0x4B2E57: d = ped - firedBy, |d|^2 = (dz^2 + dy^2) + dx^2 (rounded to float), rejected when range^2 < |d|^2 (NaN is NOT rejected: fcompp + test 0x41)
            const auto& firedPos = m_firedBy->GetPosition();
            const float dx = pedPos.x - firedPos.x, dy = pedPos.y - firedPos.y, dz = pedPos.z - firedPos.z;
            const float distSq = (dz * dz + dy * dy) + dx * dx;
            if (!(distSq > sq(fGunShotRange))) {
                if (!m_bHasNoSound)
                    return true;

                // exe 0x4B2E79-0x4B2EA0: (y*fwd.y + z*fwd.z) + x*fwd.x of (startPoint - pedPos), accepted when NOT (dot <= 0) (NaN passes)
                const CVector toStart = m_startPoint - pedPos;
                const auto&   fwd     = ped->GetForward();
                const float   dot     = (toStart.y * fwd.y + toStart.z * fwd.z) + toStart.x * fwd.x;
                if (!(dot <= 0.0f)) {
                    if (CWorld::GetIsLineOfSightClear(m_startPoint, pedPos, true, true, true, true, true, false, false))
                        return true;
                }
            }
        }
    }
    return false;
}

// 0x4AC810
bool CEventGunShot::IsCriminalEvent() {
    return m_firedBy && m_firedBy->GetIsTypePed() && m_firedBy->AsPed()->IsPlayer();
}

// 0x4AC780
bool CEventGunShot::TakesPriorityOver(const CEvent& refEvent) {
    if (refEvent.GetEventType() == GetEventType()) {
        bool bIsPlayer = false;
        bool otherPedIsPlayer = false;
        const auto refEventGunShot = static_cast<const CEventGunShot*>(&refEvent);
        if (m_firedBy && m_firedBy->AsPed()->GetIsTypePed()) {
            bIsPlayer = m_firedBy->AsPed()->IsPlayer();
        }

        CPed* otherPed = refEventGunShot->m_firedBy->AsPed();
        if (otherPed && otherPed->GetIsTypePed())
            otherPedIsPlayer = otherPed->IsPlayer();

        return bIsPlayer && !otherPedIsPlayer;
    }
    return GetEventPriority() >= refEvent.GetEventPriority();
}

// 0x4B6B20
CEventEditableResponse* CEventGunShot::CloneEditable() const noexcept {
    return new CEventGunShot(m_firedBy, m_startPoint, m_endPoint, m_bHasNoSound);
}
