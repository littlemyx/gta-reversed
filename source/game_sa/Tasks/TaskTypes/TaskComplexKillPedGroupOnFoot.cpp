#include "StdInc.h"
#include "TaskComplexKillPedGroupOnFoot.h"
#include "TaskComplexKillPedOnFoot.h"
#include "TaskSimpleLookAbout.h"
#include "TaskSimpleNone.h"

void CTaskComplexKillPedGroupOnFoot::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexKillPedGroupOnFoot, 0x86da48, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x622240);
    RH_ScopedInstall(Destructor, 0x6222B0);

    RH_ScopedInstall(CreateSubTask, 0x622320);
    RH_ScopedInstall(ComputeNearestLivingGroupPed, 0x622450);

    RH_ScopedVMTInstall(Clone, 0x6237C0);
    RH_ScopedVMTInstall(GetTaskType, 0x6222A0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x629570);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x629630);
    RH_ScopedVMTInstall(ControlSubTask, 0x622310);
}

// 0x622240
CTaskComplexKillPedGroupOnFoot::CTaskComplexKillPedGroupOnFoot(int32 groupId, CPed* ped) :
    m_grpId{groupId},
    m_currPedToKill{ped}
{
    CEntity::SafeRegisterRef(m_currPedToKill);
}

// NOTSA (For 0x6237C0)
CTaskComplexKillPedGroupOnFoot::CTaskComplexKillPedGroupOnFoot(const CTaskComplexKillPedGroupOnFoot& o) :
    CTaskComplexKillPedGroupOnFoot{o.m_grpId, o.m_currPedToKill}
{
}

// 0x6222B0
CTaskComplexKillPedGroupOnFoot::~CTaskComplexKillPedGroupOnFoot() {
    CEntity::SafeCleanUpRef(m_currPedToKill);
}

// 0x622320
CTask* CTaskComplexKillPedGroupOnFoot::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType)
    {
    case TASK_FINISHED:
        return nullptr;
    case TASK_COMPLEX_KILL_PED_ON_FOOT: {
        return new CTaskComplexKillPedOnFoot{
            m_currPedToKill,
            -1,
            0,
            0,
            0,
            true
        };
    }
    case TASK_NONE:
        return new CTaskSimpleNone{};
    case TASK_SIMPLE_LOOK_ABOUT:
        return new CTaskSimpleLookAbout{ CGeneral::GetRandomNumberInRange(3000u, 4000u) };
    default:
        NOTSA_UNREACHABLE();
        return nullptr; // 0x622438: default => eax = 0
    }
}

// 0x622450
CPed* CTaskComplexKillPedGroupOnFoot::ComputeNearestLivingGroupPed(CPed& ped, bool flag) {
    // 0x622467..0x6224BE - the ped to ignore is the one the group's leader targets (only when `ped` is not the leader itself);
    // the same ped is also the fallback result (0x6225D7) if nobody else qualified.
    CPed* pedToIgnore{};
    if (!flag) {
        if (const auto grp = ped.GetGroup()) {
            if (const auto leader = grp->GetMembership().GetLeader(); leader && leader != &ped) {
                const auto target = leader->m_pTargetedObject;
                pedToIgnore = target && target->GetIsTypePed()
                    ? target->AsPed()
                    : ComputeNearestLivingGroupPed(*leader, true); // 0x6224B0: the exe passes the LEADER (eax), not `ped`
            }
        }
    }

    auto&        members = CPedGroups::GetGroup(m_grpId).GetMembership();
    const auto   pedPos  = ped.GetPosition();
    CPed*        closest{};
    float        closestDistSq = std::numeric_limits<float>::max(); // 0x7F7FFFFF
    for (int32 i = 0; i < 8; i++) {
        const auto mem = members.GetMember(i);
        if (!mem || mem == pedToIgnore || !mem->IsAlive()) {
            continue;
        }
        if (!(mem->m_fHealth > 0.f)) { // FCOMP + `test ah, 0x41` (<= 0 or NaN => skip)
            continue;
        }
        if (mem == &ped || CPedGroups::AreInSameGroup(mem, &ped)) {
            continue;
        }
        // z, y, x sum in extended precision, stored as float
        const auto& memPos = mem->GetPosition();
        const x87d  dx     = (x87d)memPos.x - (x87d)pedPos.x;
        const x87d  dy     = (x87d)memPos.y - (x87d)pedPos.y;
        const x87d  dz     = (x87d)memPos.z - (x87d)pedPos.z;
        const float distSq = (float)((dz * dz + dy * dy) + dx * dx);
        if (!(distSq < closestDistSq)) { // FCOMP + `test ah, 5` + JP (NaN => skip)
            continue;
        }
        if (mem->IsPlayer()) {
            if (const auto wanted = mem->AsPlayer()->GetWanted()) {
                if (wanted->m_NumCopsInPursuit && ped.m_nPedType != PED_TYPE_COP && ped.IsCreatedBy(PED_GAME)) {
                    continue;
                }
            }
        }
        closestDistSq = distSq;
        closest       = mem;
    }
    return closest ? closest : pedToIgnore;
}

// 0x629570..0x6295D8 / 0x62963F..0x629666: the exe stores the new target raw and registers it only when there is one (the old reference is cleaned up by the caller)
eTaskType CTaskComplexKillPedGroupOnFoot::ComputeNewTargetAndTaskType(CPed* ped, eTaskType noTargetType) {
    m_currPedToKill = ComputeNearestLivingGroupPed(*ped); // Find new target
    if (!m_currPedToKill) {
        return noTargetType;
    }
    CEntity::RegisterReference(m_currPedToKill);
    return TASK_COMPLEX_KILL_PED_ON_FOOT;
}

// 0x629570
CTask* CTaskComplexKillPedGroupOnFoot::CreateNextSubTask(CPed* ped) {
    CEntity::ClearReference(m_currPedToKill); // 0x629577..0x629585: cleaned up and nulled at entry for every sub task type
    switch (m_pSubTask->GetTaskType()) {
    case TASK_NONE:
    case TASK_SIMPLE_LOOK_ABOUT:
        return CreateSubTask(TASK_FINISHED, ped);
    case TASK_COMPLEX_KILL_PED_ON_FOOT:
        return CreateSubTask(ComputeNewTargetAndTaskType(ped, TASK_SIMPLE_LOOK_ABOUT), ped); // Kill next or look about
    default:
        return nullptr; // 0x629619
    }
}

// 0x629630
CTask* CTaskComplexKillPedGroupOnFoot::CreateFirstSubTask(CPed* ped) {
    return CreateSubTask([&, this] {
        if (m_currPedToKill) {
            return CPedGroups::AreInSameGroup(ped, m_currPedToKill)
                ? TASK_NONE
                : TASK_COMPLEX_KILL_PED_ON_FOOT;
        } else {
            return ComputeNewTargetAndTaskType(ped, TASK_NONE); // 0x629651: TASK_NONE (0xC8) here, LOOK_ABOUT only after a kill task
        }
    }(), ped);
}
