#include "StdInc.h"

#include <bit>
#include "EventPedCollisionWithPed.h"
#if defined(RD_EVTPROBE) && defined(NOTSA_STANDALONE_RUN)
#include "standalone/Lockstep.h"
#endif
#include "TaskComplexAvoidOtherPedWhileWandering.h"

void CEventPedCollisionWithPed::InjectHooks()
{
    RH_ScopedVirtualClass(CEventPedCollisionWithPed, 0x85AC68, 16);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(Constructor, 0x4AC990);
    RH_ScopedVMTInstall(TakesPriorityOver, 0x4ACAD0);
    RH_ScopedVMTInstall(AffectsPed, 0x4ACB10);
}

// 0x4AC990
CEventPedCollisionWithPed::CEventPedCollisionWithPed(int16 pieceType, float damageIntensity, CPed* victim, const CVector& collisionImpactVelocity, const CVector& collisionPos, eMoveState moveState, eMoveState victimMoveState)
{
    m_pieceType               = pieceType;
    m_damageIntensity         = damageIntensity;
    m_victim                  = victim;
    m_collisionImpactVelocity = collisionImpactVelocity;
    m_collisionPos            = collisionPos;
    m_movestate               = moveState;
    m_victimMoveState         = victimMoveState;
    CEntity::SafeRegisterRef(m_victim);
}

CEventPedCollisionWithPed::~CEventPedCollisionWithPed()
{
    CEntity::SafeCleanUpRef(m_victim);
}

// 0x4AC990
CEventPedCollisionWithPed* CEventPedCollisionWithPed::Constructor(int16 pieceType, float damageIntensity, CPed* victim, const CVector& collisionImpactVelocity, const CVector& collisionPos, eMoveState moveState, eMoveState victimMoveState)
{
    this->CEventPedCollisionWithPed::CEventPedCollisionWithPed(pieceType, damageIntensity, victim, collisionImpactVelocity, collisionPos, moveState, victimMoveState);
    return this;
}

// 0x4ACAD0
bool CEventPedCollisionWithPed::TakesPriorityOver(const CEvent& refEvent)
{
    if (CEventHandler::IsTemporaryEvent(refEvent))
        return true;
    return CEvent::TakesPriorityOver(refEvent);
}

// 0x4ACB10
bool CEventPedCollisionWithPed::AffectsPed(CPed* ped) {
#if defined(RD_EVTPROBE) && defined(NOTSA_STANDALONE_RUN) // RD-AB-12: counterpart of the RD_BPAFF probe in tools/standalone/replay_diff/rd_orig.c (BP at 0x4ACB10); inert without the define
    {
        char b[600]; int n = 0;
        auto* const pw = reinterpret_cast<const uint32*>(ped);
        auto* const vw = reinterpret_cast<const uint32*>(m_victim);
        const auto& fw = ped->GetForward();
        auto fb = [](float f) { return (unsigned)std::bit_cast<uint32>(f); };
        n += snprintf(b + n, sizeof(b) - n, "W %d %d ms=%d vms=%d vsc=%d vel=%08x,%08x,%08x fwd=%08x,%08x,%08x fl=", GetPedPool()->GetIndex(ped), m_victim ? GetPedPool()->GetIndex(m_victim) : -1, (int)m_movestate, (int)m_victimMoveState, m_victim ? (int)m_victim->GetIntelligence()->m_AnotherStaticCounter : -1, fb(m_collisionImpactVelocity.x), fb(m_collisionImpactVelocity.y), fb(m_collisionImpactVelocity.z), fb(fw.x), fb(fw.y), fb(fw.z));
        for (int j = 0; j < 4; j++) n += snprintf(b + n, sizeof(b) - n, "%08x,", pw[0x468 / 4 + j]);
        n += snprintf(b + n, sizeof(b) - n, " vfl=");
        for (int j = 0; j < 4 && m_victim; j++) n += snprintf(b + n, sizeof(b) - n, "%08x,", vw[0x468 / 4 + j]);
        n += snprintf(b + n, sizeof(b) - n, " hp=%08x att=%u vpt=%d", pw[0x540 / 4], (unsigned)(uintptr_t)ped->m_pAttachedTo ? 1u : 0u, m_victim ? (int)vw[0x4C4 / 4] : -1);
        notsa::lockstep::Note(b);
    }
#endif
    if (!ped->IsAlive() || ped->m_pAttachedTo || ped->bInVehicle) {
        return false;
    }

    if (!m_victim || m_victim->bInVehicle || ped->GetIntelligence()->IsThreatenedBy(*m_victim) || m_victim->GetIntelligence()->m_AnotherStaticCounter > 30) {
        return false;
    }

    // 0x4ACB8C: `cmp word ptr [event + 0x30], 4; jge 0x4ACBBB` - a walking (or faster) ped skips this check, a still / turning ped is only
    // affected when the victim is the LEADER of a group (the port had it inverted: walkers were rejected, still peds needed a non-leader victim)
    if ((int16)m_movestate < (int16)PEDMOVE_WALK) {
        const auto victimsGroup = CPedGroups::GetPedsGroup(m_victim); // 0x5F7E80
        if (!victimsGroup || !victimsGroup->GetMembership().IsLeader(m_victim)) { // 0x5F69C0
            return false;
        }
    }

    // 0x4ACBC6 CVector::DotProduct (0x40FDB0: z*z + y*y + x*x left in an x87 register, compared as `fcomp [0x858F40]` = -0.5f; false when dot > -0.5, NaN passes)
    const auto& fwd = ped->GetForward();
    const x87d  dot = (x87d)m_collisionImpactVelocity.z * fwd.z + (x87d)m_collisionImpactVelocity.y * fwd.y + (x87d)m_collisionImpactVelocity.x * fwd.x;
    if (dot > -0.5) { // If collision's velocity is not in [-120, 120] - So basically, if coming from behind
        return false;
    }

    if (ped->GetTaskManager().IsFirstFoundTaskMatching<TASK_COMPLEX_PARTNER_DEAL, TASK_COMPLEX_PARTNER_GREET>(m_victim->GetTaskManager())) {
        return false;
    }
                
    if (const auto task = ped->GetTaskManager().GetActiveTaskAs<CTaskComplexAvoidOtherPedWhileWandering>()) {
        if (task->GetPedToAvoid() == m_victim) {
            return false;
        }
    }

    if (const auto task = ped->GetTaskManager().Find<CTaskComplexKillPedOnFoot>()) {
        if (task->m_target == m_victim) {
            if (ped->GetTaskManager().Find<TASK_SIMPLE_FIGHT_CTRL>()) {
                return false;
            }
        }
    }

    return true;
}
