#include "StdInc.h"

#include "TaskSimpleStealthKill.h"

void CTaskSimpleStealthKill::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleStealthKill, 0x86da74, 9);
    RH_ScopedCategory("Tasks/TaskTypes");
    RH_ScopedVMTDestructorInstall(0x6296B0); // scalar deleting destructor; the destructor body is ported (VT-OVERRIDES)

    RH_ScopedInstall(Constructor, 0x6225F0);

    RH_ScopedInstall(ManageAnim, 0x6296D0);
    RH_ScopedInstall(FinishAnimStealthKillCB, 0x622790);

    RH_ScopedVMTInstall(ProcessPed, 0x62E540);
    RH_ScopedVMTInstall(Clone, 0x623830);
    RH_ScopedVMTInstall(GetTaskType, 0x622670);
    RH_ScopedVMTInstall(MakeAbortable, 0x6226F0);
}

// 0x6225F0
CTaskSimpleStealthKill::CTaskSimpleStealthKill(bool keepTargetAlive, CPed* target, AssocGroupId groupId) :
    m_bKeepTargetAlive{ keepTargetAlive },
    m_target{ target },
    m_animGrpId{ groupId }
{
    CEntity::SafeRegisterRef(m_target);
}

// 0x622680. The port had no destructor: the target's reference entry and the anim's delete callback (this freed task as data) stayed
CTaskSimpleStealthKill::~CTaskSimpleStealthKill() {
    CEntity::SafeCleanUpRef(m_target);
    if (m_anim) {
        m_anim->SetDefaultDeleteCallback();
    }
}

// NOTSA (For 0x623830)
CTaskSimpleStealthKill::CTaskSimpleStealthKill(const CTaskSimpleStealthKill& o) :
#ifdef NOTSA_STANDALONE_RUN
    CTaskSimpleStealthKill{ o.m_bKeepTargetAlive, o.m_target, o.m_animGrpId } // NOTSA-AUDIT: the exe's Clone (0x623830) passes THIS task's flag / target / anim group; upstream read the uninitialised members of the new object
#else
#line 31
    CTaskSimpleStealthKill{ m_bKeepTargetAlive, m_target, m_animGrpId }
#endif
#line 32
{
}

// 0x62E540
bool CTaskSimpleStealthKill::ProcessPed(CPed* ped) {
    if (m_bIsAborting || !m_target) {
        return true;
    }

    if (!m_anim) {
        if (!m_bKeepTargetAlive) {
            ped->SetPedState(PEDSTATE_DIE_BY_STEALTH);
        }

        ManageAnim(ped);

        if (!m_anim && m_bIsFinished) {
            return false;
        }
    }

    ped->m_pedIK.bSlopePitch = true;

    const auto pedToTarget = m_target->GetPosition() - ped->GetPosition();

    if (!m_bKeepTargetAlive) {
        if (!m_anim || m_anim->m_AnimId == ANIM_ID_KILL_KNIFE_PED_DAMAGE) {
            ped->m_fAimingRotation = (-pedToTarget).Heading(); // 0x62E690: fpatan(-(-x), -y) = Heading of the NEGATED vector (was `-Heading()`, i.e. atan2(x, y))
        }
        return false;
    }

    ped->m_fAimingRotation = pedToTarget.Heading();

    // exe 0x62E5D7-0x62E5EB: |v|^2 is summed z, x, y
    if (const auto shift = std::sqrt((pedToTarget.z * pedToTarget.z + pedToTarget.x * pedToTarget.x) + pedToTarget.y * pedToTarget.y) - 1.0f; std::abs(shift) > 0.02f) {
        ped->m_vecAnimMovingShiftLocal.y = std::min(CTimer::GetTimeStep() * 0.05f, shift);
        ped->m_vecAnimMovingShiftLocal.x = 0.0f;
    }

    return false;
}

// 0x6226F0
bool CTaskSimpleStealthKill::MakeAbortable(class CPed* ped, eAbortPriority priority, const CEvent* event) {
    // 0x6226F0: the anim's DELETE callback is reset (not the finish callback); IMMEDIATE also fades it out at once (-1000.0f)
    switch (priority) {
    case ABORT_PRIORITY_IMMEDIATE: {
        if (m_anim) {
            m_anim->SetBlendDelta(-1000.0f);
            m_anim->SetDefaultDeleteCallback();
            m_anim = nullptr;
        }
        m_bIsAborting = true;
        return true;
    }
    case ABORT_PRIORITY_URGENT: {
        if (!m_bKeepTargetAlive && m_anim && m_anim->m_AnimId == ANIM_ID_KILL_KNIFE_PED_DIE) {
            if (event && event->GetEventType() == EVENT_DAMAGE && static_cast<const CEventDamage*>(event)->m_pSourceEntity == m_target) {
                m_bIsAborting = true;
                m_anim->SetDefaultDeleteCallback();
                m_anim = nullptr;
                return true;
            }
        }
        return false;
    }
    default:
        return false;
    }
}

// 0x6296D0
void CTaskSimpleStealthKill::ManageAnim(CPed* ped) {
    CAnimBlock* pAnimBlock = CAnimManager::GetAssocGroups()[m_animGrpId].m_AnimBlock;
    if (!pAnimBlock || !pAnimBlock->IsLoaded) {
        m_spentWaitingMs += NOTSA_FTOL(uint32, CTimer::GetTimeStepInMS());
        if (m_spentWaitingMs > 10000) {
            m_bIsAborting = true;
        }
        return;
    }

    m_anim = CAnimManager::BlendAnimation(ped->GetRpClump(), m_animGrpId, [&, this] {
        if (m_bKeepTargetAlive) {
            return ANIM_ID_KILL_KNIFE_PLAYER;
        }

        if (!m_bIsFinished) {
            return ANIM_ID_KILL_KNIFE_PED_DAMAGE;
        }

        CPedDamageResponseCalculator damageCalculator{
            m_target, // exe 0x6297A0: the damager is the target (task+0xC), not the ped running the task
            CPedDamageResponseCalculator::ms_damageFactor,
            m_target->GetActiveWeapon().m_Type,
            PED_PIECE_TORSO,
            false
        };
        CEventDamage eventDamage{
            m_target,
            CTimer::GetTimeInMS(),
            m_target->GetActiveWeapon().m_Type,
            PED_PIECE_TORSO,
            0,
            false,
            !!ped->bInVehicle
        };
        if (eventDamage.AffectsPed(ped)) {
            damageCalculator.ComputeDamageResponse(ped, eventDamage.m_damageResponse, true);

            eventDamage.m_nAnimGroup = m_animGrpId;
            eventDamage.m_nAnimID = ANIM_ID_KILL_KNIFE_PED_DIE;
            eventDamage.m_fAnimBlend = 8.0f;
            eventDamage.m_fAnimSpeed = 1.0f;
            eventDamage.m_ucDirection |= 4u;
            ped->GetEventGroup().Add(&eventDamage, false);
        }
        return ANIM_ID_KILL_KNIFE_PED_DIE;
    }());
    m_anim->SetFinishCallback(FinishAnimStealthKillCB, this);
    m_bIsFinished = true;
}

// 0x622790
void CTaskSimpleStealthKill::FinishAnimStealthKillCB(CAnimBlendAssociation* pAnimAssoc, void* data) {
    const auto self = static_cast<CTaskSimpleStealthKill*>(data);
    // 0x622794: the flag is only SET for the two knife-kill animations, never cleared (it was assigned the comparison result)
    if (notsa::contains<AnimationId>({ ANIM_ID_KILL_KNIFE_PLAYER, ANIM_ID_KILL_KNIFE_PED_DIE }, pAnimAssoc->m_AnimId)) {
        self->m_bIsAborting = true;
    }
    self->m_anim = nullptr;
}
