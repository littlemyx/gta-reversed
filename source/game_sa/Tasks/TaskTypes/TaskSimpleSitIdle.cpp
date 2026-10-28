#include "StdInc.h"
#include "TaskSimpleSitIdle.h"
#include "Ragdoll/IKChainManager.h"

void CTaskSimpleSitIdle::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleSitIdle, 0x86df18, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x631160);
    RH_ScopedInstall(Destructor, 0x6311B0);

    RH_ScopedInstall(StartAnim, 0x631260);

    RH_ScopedVMTInstall(Clone, 0x636280);
    RH_ScopedVMTInstall(GetTaskType, 0x6311A0);
    RH_ScopedVMTInstall(MakeAbortable, 0x6311E0);
    RH_ScopedVMTInstall(ProcessPed, 0x637A60);
}

// 0x631160
CTaskSimpleSitIdle::CTaskSimpleSitIdle(int32 durationMs, bool sitAfterStep) :
    m_durationMs{durationMs},
    m_bSitAfterStep{sitAfterStep}
{
}

CTaskSimpleSitIdle::CTaskSimpleSitIdle(const CTaskSimpleSitIdle& o) :
    CTaskSimpleSitIdle{o.m_durationMs, o.m_bSitAfterStep}
{
}

// 0x631260
void CTaskSimpleSitIdle::StartAnim(CPed* ped) {
    m_animStartedTimer.StartUnguarded(m_durationMs); // 0x631266-0x631272: the exe stores start/interval/started without the `>= 0` test

    const auto StartAnim = [this, ped](auto grpId, auto animId) { m_anim = CAnimManager::BlendAnimation(ped->GetRpClump(), grpId, animId, 256.f); };
    if (m_bSitAfterStep) {
        StartAnim(ANIM_GROUP_ATTRACTORS, ANIM_ID_STEPSIT_LOOP);
    } else {
        StartAnim(ANIM_GROUP_DEFAULT, ANIM_ID_SEAT_IDLE);
    }

    if (m_anim) {
        m_anim->m_Speed = CGeneral::GetRandomNumberInRange(0.9f, 1.5f);
    }
}

// 0x6311E0
bool CTaskSimpleSitIdle::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    switch (priority) {
    case ABORT_PRIORITY_IMMEDIATE: {
        if (m_anim) {
            m_anim->m_BlendDelta = -1000.f;
            m_anim->SetDefaultFinishCallback();
        }
        g_ikChainMan.AbortLookAt(ped, 750); // Omitted redudant `IsLooking` check.
        return true;
    }
    default: {
        m_durationMs = -1; // 0x631237: [esi+0Ch] = -1 as well
        m_animStartedTimer.SetOutOfTime(); // Eventually leads to `ProcessPed` returning `true` that finishes the task
        return false;
    }
    }
}

// 0x637A60
bool CTaskSimpleSitIdle::ProcessPed(CPed* ped) {
    if (!m_anim) {
        StartAnim(ped);
    }

    // Do look at somewhere/another ped with random chance
    if (!g_ikChainMan.IsLooking(ped) && CGeneral::RandomBool(5.f)) {
        const auto lookTime = CGeneral::GetRandomNumberInRange(3000, 8000); // 0x637A9F-0x637AC0: 3000 + (int)(r * 5000)

        // First try looking at the closest ped (if any)
        const auto hasLookedAtClosestPed = [this, ped, lookTime] {
            const auto closestPed = ped->GetIntelligence()->GetPedScanner().GetClosestPedInRange();
            if (!closestPed) {
                return false;
            }
            // 0x637B06-0x637B8F: the cone test comes first, the 25% roll (a rand() call) only when it passes
            const auto toClosest = closestPed->GetPosition() - ped->GetPosition();
            const auto fwd       = ped->GetForwardVector();
            const float dot      = (toClosest.z * fwd.z + toClosest.y * fwd.y) + toClosest.x * fwd.x; // exe sum order z, y, x
            if (dot <= 0.2f) { // Must be in cone of [-78, 78] deg in front of `ped`
                return false;
            }
            if (!CGeneral::RandomBool(25.f)) {
                return false;
            }

            g_ikChainMan.LookAt(
                "TaskSitIdle",
                ped,
                closestPed,
                lookTime,
                BONE_HEAD,
                nullptr,
                false,
                0.25f,
                750u,
                3,
                false
            );

            return true;
        }();

        // If that failed, look at a random point somewhere in front of the ped
        if (!hasLookedAtClosestPed) {
            // 0x637BA7-0x637CBB: pos = ped pos; rand -> += forward * r1; rand -> += right * r2 (explicitly sequenced)
            CVector pos = ped->GetPosition();
            const float r1 = CGeneral::GetRandomNumberInRange(1.f, 5.f);
            pos += ped->GetForwardVector() * r1;
            const float r2 = CGeneral::GetRandomNumberInRange(-4.f, 4.f);
            pos += ped->GetRightVector() * r2;
            g_ikChainMan.LookAt(
                "TaskSitIdle",
                ped,
                nullptr,
                lookTime,
                BONE_UNKNOWN,
                &pos,
                false,
                0.25f,
                750u,
                3,
                false
            );
        }
    }

    if (m_animStartedTimer.IsOutOfTime()) {
        g_ikChainMan.AbortLookAt(ped, 750u); // Omitted redudant `IsLooking` check
        return true;
    }

    return false;
}
