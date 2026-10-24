#include "StdInc.h"

#include "TaskSimpleChoking.h"
#include "AnimManager.h"
#include "Event.h"

// 0x6202C0
CTaskSimpleChoking::CTaskSimpleChoking(CPed* attacker, bool bIsTeargas) :
    m_pAttacker{attacker},
    m_bIsTeargas{bIsTeargas}
{
    CEntity::SafeRegisterRef(m_pAttacker);
    m_pAnim          = nullptr;
    m_bIsFinished    = false;
    m_nTimeRemaining = CGeneral::GetRandomNumberInRange(1000, 2000); // 0x620327..0x620339: 1000 - ftol(rand * (1/32768) * -1000.0) = 1000 + [0, 1000), not [0, 1000)
    m_nTimeStarted   = CTimer::GetTimeInMS();
}

// 0x620370. The port had no destructor: the anim kept its finish callback (with this freed task as data) and the attacker's reference entry stayed in the ped's list
CTaskSimpleChoking::~CTaskSimpleChoking() {
    if (m_pAnim) {
        m_pAnim->SetBlendDelta(-4.0f);
        m_pAnim->SetDefaultFinishCallback();
    }
    CEntity::SafeCleanUpRef(m_pAttacker);
}

// 0x623220
CTaskSimpleChoking::CTaskSimpleChoking(const CTaskSimpleChoking& o) :
    CTaskSimpleChoking{o.m_pAttacker, o.m_bIsTeargas}
{
}

// 0x620480 (Anim finish callback)
static void ChokeAnimFinishCB(CAnimBlendAssociation*, void* data) {
    const auto self = static_cast<CTaskSimpleChoking*>(data);
    self->m_pAnim       = nullptr;
    self->m_bIsFinished = true;
}

// 0x6203F0
bool CTaskSimpleChoking::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if (priority == ABORT_PRIORITY_URGENT || priority == ABORT_PRIORITY_IMMEDIATE) {
        if (event && event->GetEventPriority() < 57) {
            return false;
        }
        if (m_pAnim) {
            m_pAnim->m_BlendDelta = -4.f;
            m_pAnim->SetDefaultFinishCallback();
            m_pAnim = nullptr;
        }
        m_bIsFinished = true;
        return true;
    }

    if (m_pAnim) {
        m_pAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
        m_pAnim->m_BlendDelta = -4.f;
        m_pAnim->SetDefaultFinishCallback();
        m_pAnim = nullptr;
    }
    return true;
}

// 0x620490
bool CTaskSimpleChoking::ProcessPed(CPed* ped) {
    if (m_bIsFinished) {
        return true;
    }

    if (m_pAttacker && !ped->IsPlayer() && !m_bIsTeargas) {
        // Turn the ped to face the attacker (if the attacker is in front of the ped)
        const auto& pedPos      = ped->GetPosition();
        const auto& attackerPos = m_pAttacker->GetPosition();
        const auto& fwd         = ped->GetForward(); // Original code accesses the matrix directly (BUG: crashes if the ped has no matrix)

        // x87: kept in extended precision
        const x87d dx = (x87d)attackerPos.x - (x87d)pedPos.x;
        const x87d dy = (x87d)attackerPos.y - (x87d)pedPos.y;
        const x87d dz = (x87d)attackerPos.z - (x87d)pedPos.z;
        const x87d dot = dz * (x87d)fwd.z + dy * (x87d)fwd.y + dx * (x87d)fwd.x;
        if (dot > 0.0) {
            ped->m_fAimingRotation = (float)x87::atan2(-dx, dy);
        }
    }

    // x87: `timestep * 0.02f * 1000.f` is computed in extended precision before truncation
    const auto decr = NOTSA_FTOL(uint32, (x87d)CTimer::GetTimeStep() * (x87d)0.02f * (x87d)1000.0);
    if (m_nTimeRemaining > decr) {
        m_nTimeRemaining -= decr;
    } else {
        m_nTimeRemaining = 0;
    }

    if (!m_pAnim) {
        m_pAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_DEFAULT, ANIM_ID_GAS_CWR, 4.f);
        m_pAnim->SetFinishCallback(ChokeAnimFinishCB, this);
        m_pAnim->m_Speed = (float)((x87d)rand() * (x87d)RAND_MAX_FLOAT_RECIPROCAL * (x87d)0.25f + (x87d)0.75f);
    } else if (m_nTimeRemaining == 0) {
        if (m_pAnim->m_AnimId == ANIM_ID_GAS_CWR) {
            m_pAnim->SetDefaultFinishCallback();
            m_pAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_DEFAULT, ANIM_ID_IDLE_TIRED, 4.f);
            m_pAnim->SetFinishCallback(ChokeAnimFinishCB, this);

            m_nTimeRemaining = CTimer::GetTimeInMS() - m_nTimeStarted;
            const auto rnd = (uint32)CGeneral::GetRandomNumberInRange(8000, 12000);
            if (m_nTimeRemaining >= rnd) {
                m_nTimeRemaining = (uint32)CGeneral::GetRandomNumberInRange(8000, 12000);
            }
        } else {
            m_pAnim->m_BlendDelta = -4.f;
            m_pAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
        }
    }

    ped->Say(CTX_GLOBAL_PAIN_COUGH, 0, 1.f, false, false, false);
    return false;
}

// 0x620660
void CTaskSimpleChoking::UpdateChoke(CPed* victim, CPed* attacker, bool bIsTeargas) {
    m_bIsTeargas = bIsTeargas;
    if (m_pAttacker != attacker) {
        CEntity::ChangeEntityReference(m_pAttacker, attacker);
    }
    m_bIsFinished = false;
    if (m_pAnim && m_pAnim->m_AnimId != ANIM_ID_GAS_CWR) {
        m_pAnim->SetDefaultFinishCallback();
        m_pAnim = CAnimManager::BlendAnimation(victim->GetRpClump(), ANIM_GROUP_DEFAULT, ANIM_ID_GAS_CWR, 4.f);
        m_pAnim->SetFinishCallback([](CAnimBlendAssociation* a, void* data) {
            const auto self = static_cast<CTaskSimpleChoking*>(data);
            self->m_bIsFinished = true;
            self->m_pAnim = nullptr;
        });
        m_pAnim->SetSpeed(CGeneral::GetRandomNumberInRange(0.8f, 1.1f));
        m_nTimeStarted = CTimer::GetTimeInMS() - m_nTimeRemaining;
    }
    m_nTimeRemaining = CGeneral::GetRandomNumberInRange<uint32>(1'000, 2'000);
}

void CTaskSimpleChoking::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleChoking, 0x86d820, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6202C0);
    RH_ScopedInstall(Destructor, 0x620370);

    RH_ScopedInstall(UpdateChoke, 0x620660);

    RH_ScopedVMTInstall(Clone, 0x623220);
    RH_ScopedVMTInstall(GetTaskType, 0x620360);
    RH_ScopedVMTInstall(MakeAbortable, 0x6203F0);
    RH_ScopedVMTInstall(ProcessPed, 0x620490);
}
