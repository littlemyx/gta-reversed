#include "StdInc.h"
#include "TaskComplexPassObject.h"
#include "TaskSimpleHoldEntity.h"
#include "TaskSimpleStandStill.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "Ragdoll/IKChainManager.h"

void CTaskComplexPassObject::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexPassObject, 0x86fb18, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6611B0);
    RH_ScopedInstall(Destructor, 0x661230);

    RH_ScopedInstall(AbortIK, 0x661290);

    RH_ScopedVMTInstall(Clone, 0x662220);
    RH_ScopedVMTInstall(GetTaskType, 0x661220);
    RH_ScopedVMTInstall(MakeAbortable, 0x664A50);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x6612C0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x6613A0);
    RH_ScopedVMTInstall(ControlSubTask, 0x661570);
}

// 0x6611B0
CTaskComplexPassObject::CTaskComplexPassObject(CPed* ped, bool isPasser) :
    m_ped{ped},
    m_isPasser{isPasser}
{
    CEntity::SafeRegisterRef(m_ped);
}

CTaskComplexPassObject::CTaskComplexPassObject(const CTaskComplexPassObject& o) :
    CTaskComplexPassObject{ o.m_ped, o.m_isPasser }
{
}

// 0x661230
CTaskComplexPassObject::~CTaskComplexPassObject() {
    CEntity::SafeCleanUpRef(m_ped);
#ifdef FIX_BUGS
    CEntity::SafeCleanUpRef(m_entityToPass);
#endif
}

// 0x661290
void CTaskComplexPassObject::AbortIK(CPed* ped) {
    if (g_ikChainMan.IsArmPointing(eIKArm::IK_ARM_RIGHT, ped)) {
        g_ikChainMan.AbortPointArm(eIKArm::IK_ARM_RIGHT, ped, 500);
    }
}

// 0x664A50
bool CTaskComplexPassObject::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    AbortIK(ped);
    return true;
}

// 0x6612C0
CTask* CTaskComplexPassObject::CreateNextSubTask(CPed* ped) {
    if (m_ped && notsa::isa<CTaskComplexTurnToFaceEntityOrCoord>(m_pSubTask)) {
        g_ikChainMan.PointArm("CTaskComplexPassObject", eIKArm::IK_ARM_RIGHT, ped, nullptr, eBoneTag::BONE_UNKNOWN, &m_targetPos, 0.25f, 250, 30.f);
        return new CTaskSimpleStandStill{ 999'999 };
    }
    AbortIK(ped);
    return nullptr;
}

// 0x6613A0
CTask* CTaskComplexPassObject::CreateFirstSubTask(CPed* ped) {
    if (m_ped && !m_ped->IsPlayer() && !ped->IsPlayer()) {
        // Find entity to pass
        {
            const auto passer = m_isPasser ? ped : m_ped;
            m_entityToPass = passer->GetEntityThatThisPedIsHolding();
            CEntity::SafeRegisterRef(m_entityToPass); // 0x661403: `test ecx, ecx / je` the exe does not require an entity here (the port had an assert and an unconditional register)
        }
        // 0x661415..0x661466: x87 evaluation. dx / dy are rounded to float once (stored), dz stays unrounded; dx enters the square as unrounded * rounded
        const auto& otherPos = m_ped->GetPosition();
        const auto& pedPos   = ped->GetPosition();
        const x87d dzU = (x87d)otherPos.z - (x87d)pedPos.z;
        const x87d dxU = (x87d)otherPos.x - (x87d)pedPos.x;
        const float  dx  = (float)dxU;
        const float  dy  = (float)((x87d)otherPos.y - (x87d)pedPos.y);
        const x87d distSq = (dxU * (x87d)dx + (x87d)dy * (x87d)dy) + dzU * dzU;
        if (distSq <= (double)sq(1.5f)) { // 0x66145B: fcomp [0x86E104] (2.25); NaN => no
            m_timer.Start(1000); // 0x66147B..0x66148B: `[this+0x20] = CTimer::m_snTimeInMilliseconds; [this+0x24] = 1000; [this+0x28] = 1` (the port never started the timer)
            m_targetPos = CVector{ (float)((x87d)dx * (x87d)0.5 + otherPos.x), (float)((x87d)dy * (x87d)0.5 + otherPos.y), (float)(dzU * (x87d)0.5 + otherPos.z) }; // position in between the 2 peds
            return new CTaskComplexTurnToFaceEntityOrCoord{ static_cast<CEntity*>(m_ped) }; // 0x66150D..0x661514: the ENTITY constructor (0x66B890) with the other ped, not the position one
        }
    }
    AbortIK(ped);
    return nullptr;
}

// 0x661570
CTask* CTaskComplexPassObject::ControlSubTask(CPed* ped) {
    if (m_ped) {
        if (!m_timer.IsOutOfTime()) {
            return m_pSubTask;
        }

        if (!m_isPasser && m_entityToPass) {
            if (const auto holdTask = m_ped->GetTaskManager().Find<CTaskSimpleHoldEntity>()) {
                auto heldEntity = holdTask->m_pEntityToHold;
                holdTask->DropEntity(m_ped, false);
                ped->GetTaskManager().SetTaskSecondary(
                    new CTaskSimpleHoldEntity{ heldEntity, CVector{}, (uint8)BONE_L_BROW },
                    TASK_SECONDARY_PARTIAL_ANIM
                );
            }
        }
    }

    AbortIK(ped);
    return nullptr;
}
