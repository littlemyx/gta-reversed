#include "StdInc.h"

#include "TaskComplexBeInCouple.h"
#include "TaskComplexWanderStandard.h"
#include "TaskComplexWalkAlongsidePed.h"
#include "Ragdoll/IKChainManager.h"

void CTaskComplexBeInCouple::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexBeInCouple, 0x870724, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(GetWalkSide, 0x683840);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x684840);
    RH_ScopedVMTInstall(ControlSubTask, 0x684930);
}

// 0x6836F0
CTaskComplexBeInCouple::CTaskComplexBeInCouple(
    CPed* ped,
    bool  isLeader,
    bool  holdHands,
    bool  lookAtEachOther,
    float giveUpDist
) :
    CTaskComplex(),
    m_partner{ped},
    m_isLeader{isLeader},
    m_holdHands{holdHands},
    m_lookAtEachOther{lookAtEachOther},
    m_giveUpDist{giveUpDist}
{
    CEntity::SafeRegisterRef(m_partner);
}

// 0x683780
CTaskComplexBeInCouple::~CTaskComplexBeInCouple() {
    CEntity::SafeCleanUpRef(m_partner);
}

// 0x6837E0
CTask* CTaskComplexBeInCouple::CreateFirstSubTask(CPed* ped) {
    return CreateNextSubTask(ped);
}

// 0x6837F0
void CTaskComplexBeInCouple::AbortArmIK(CPed* ped) {
    const auto DoAbortArmIK = [ped](eIKArm arm) {
        if (IKChainManager_c::IsArmPointing(arm, ped)) {
            IKChainManager_c::AbortPointArm(arm, ped, 250);
        }
    };
    DoAbortArmIK(eIKArm::IK_ARM_RIGHT);
    DoAbortArmIK(eIKArm::IK_ARM_LEFT);
}

// 0x6847C0
bool CTaskComplexBeInCouple::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (m_partner && event && event->HasEditableResponse()) {
        auto editable = static_cast<CEventEditableResponse*>(const_cast<CEvent*>(event)); // Okay let's go!

        bool bAddToEventGroup = editable->m_bAddToEventGroup;
        editable->m_bAddToEventGroup = false;
        switch (event->GetEventType()) {
        case EVENT_DAMAGE:
        case EVENT_GUN_AIMED_AT:
        case EVENT_SHOT_FIRED:
            m_partner->GetEventGroup().Add(editable, false);
            break;
        }
        editable->m_bAddToEventGroup = bAddToEventGroup;
    }
    AbortArmIK(ped);
    return true;
}

// 0x684840
CTask* CTaskComplexBeInCouple::CreateNextSubTask(CPed* ped) {
    if (!m_partner) {
        AbortArmIK(ped);
        return nullptr;
    }
    if (!m_isLeader) {
        AbortArmIK(ped);
        return new CTaskComplexWalkAlongsidePed{ m_partner, m_giveUpDist };
    }
    AbortArmIK(ped);
    return new CTaskComplexWanderStandard{ PEDMOVE_WALK, (uint8)CGeneral::GetRandomNumberInRange(0, 8), true };
}

// 0x683840
// Returns 1 or 2: the side of the partner (along its right vector) the ped is on
uint32 CTaskComplexBeInCouple::GetWalkSide(CPed* ped) const {
    const CVector partnerRight = m_partner->GetRight(); // copied, as in the original
    const auto& partnerPos  = m_partner->GetPosition();
    const auto& pedPos      = ped->GetPosition();

    const double dotPartner = (double)partnerRight.y * partnerPos.y + (double)partnerRight.z * partnerPos.z + (double)partnerRight.x * partnerPos.x;
    const double dotPed     = (double)partnerRight.y * pedPos.y     + (double)partnerRight.z * pedPos.z     + (double)partnerRight.x * pedPos.x;

    // NOTE: `fcomp` + `test ah, 1` (C0) => true for `<` and for unordered
    return !(dotPed + -dotPartner >= 0.0) ? 2 : 1;
}

// 0x684930
CTask* CTaskComplexBeInCouple::ControlSubTask(CPed* ped) {
    const auto Abort = [&]() -> CTask* {
        AbortArmIK(ped);
        return nullptr;
    };

    if (!m_partner || !m_partner->IsAlive()) {
        return Abort();
    }

    const auto partnerCoupleTask = static_cast<CTaskComplexBeInCouple*>(m_partner->GetIntelligence()->FindTaskByType(TASK_COMPLEX_BE_IN_COUPLE));
    if (!partnerCoupleTask || partnerCoupleTask->m_isLeader == m_isLeader) { // Leader needs a non-leader partner and vice versa
        return Abort();
    }
    if (partnerCoupleTask->m_partner != ped) {
        return Abort();
    }

    const auto pedActive     = ped->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_BE_IN_COUPLE);
    const auto partnerActive = m_partner->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_BE_IN_COUPLE);
    const auto side          = GetWalkSide(ped);

    if (!m_isLeader) {
        if (const auto walkAlongside = static_cast<CTaskComplexWalkAlongsidePed*>(ped->GetIntelligence()->FindTaskByType(TASK_COMPLEX_WALK_ALONGSIDE_PED))) {
            walkAlongside->SetOffset(CVector{ side == 2 ? -1.05f : 1.05f, 0.f, 0.f });
        }
    }

    const auto& partnerPos = m_partner->GetPosition();
    const auto& pedPos     = ped->GetPosition();

    const double dyE = (double)partnerPos.y - (double)pedPos.y;      // stays on the x87 stack
    const float  dx  = (float)((double)partnerPos.x - (double)pedPos.x);
    const float  dy  = (float)dyE;
    const float  distSq = (float)(dyE * (double)dy + (double)dx * (double)dx);

    const double giveUp = m_giveUpDist;
    if (distSq > giveUp * giveUp || !pedActive || !partnerActive) { // NaN => passes
        return Abort();
    }

    if (distSq > 4.f && m_isLeader) {
        AbortArmIK(ped);
        if (!ped->GetIntelligence()->FindTaskByType(TASK_COMPLEX_WAIT_FOR_PED)) {
            // CTaskComplexWaitForPed has no C++ port yet (ctor 0x683340: ped, radius, time, rotateOthers)
            const auto mem = static_cast<CTask*>(CTask::operator new(0x2C));
            if (!mem) {
                return nullptr;
            }
            return plugin::CallMethodAndReturn<CTask*, 0x683340, CTask*, CPed*, float, uint32, bool>(mem, m_partner, 0.75f, 20'000u, false);
        }
    }

    if (m_holdHands) {
        const CVector half{ dx * 0.5f, dy * 0.5f, 0.f };
        const CVector target = pedPos + half; // 0x40FE30
        if (distSq < 2.25f) {
            if (side != (uint32)m_prevSide) {
                AbortArmIK(ped);
            }
            if (side == 2) {
                g_ikChainMan.PointArm("CTaskComplexBeInCouple", IK_ARM_RIGHT, ped, nullptr, BONE_UNKNOWN, const_cast<CVector*>(&target), 0.5f, 250, 30.f);
                m_prevSide = 2;
            } else if (side == 1) {
                g_ikChainMan.PointArm("CTaskComplexBeInCouple", IK_ARM_LEFT, ped, nullptr, BONE_UNKNOWN, const_cast<CVector*>(&target), 0.5f, 250, 30.f);
                m_prevSide = 1;
            }
        } else { // also taken for NaN
            AbortArmIK(ped);
        }
    }

    if (m_lookAtEachOther) {
        if (!g_ikChainMan.IsLooking(ped) && CGeneral::GetRandomNumberInRange(0, 100) > 80) {
            g_ikChainMan.LookAt("TaskBeInCouple", ped, m_partner, CGeneral::GetRandomNumberInRange(2000, 4000), BONE_HEAD, nullptr, false, 0.25f, 500, 3, false);
        }
        if (!g_ikChainMan.IsLooking(m_partner) && CGeneral::GetRandomNumberInRange(0, 100) > 80) {
            g_ikChainMan.LookAt("TaskBeInCouple", m_partner, ped, CGeneral::GetRandomNumberInRange(2000, 4000), BONE_HEAD, nullptr, false, 0.25f, 500, 3, false);
        }
    }

    return m_pSubTask;
}
