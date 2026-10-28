#include "StdInc.h"

#include "TaskComplexInvestigateDisturbance.h"
#include "TaskSimpleScratchHead.h"
#include "TaskSimpleRunAnim.h"
#include "SeekEntity/TaskComplexSeekEntity.h"
#include "SeekEntity/PosCalculators/EntitySeekPosCalculatorFixedPos.h"
#include "TaskComplexGoToPointAndStandStill.h"

#include <Ragdoll/IKChainManager.h>

void CTaskComplexInvestigateDisturbance::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexInvestigateDisturbance, 0x870010, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x66C720);
    RH_ScopedInstall(Destructor, 0x66C7A0);

    RH_ScopedVMTInstall(Clone, 0x66D3D0);
    RH_ScopedVMTInstall(GetTaskType, 0x66C790);
    RH_ScopedVMTInstall(MakeAbortable, 0x66C800);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x6749F0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x66C820);
    RH_ScopedVMTInstall(ControlSubTask, 0x66CA30);
}

// 0x66C720
CTaskComplexInvestigateDisturbance::CTaskComplexInvestigateDisturbance(CVector const& targetPos, CEntity* srcEntity) :
    m_entity{srcEntity},
    m_pos{targetPos}
{
    CEntity::SafeRegisterRef(m_entity);
}

// NOTSA (For 0x66D3D0)
CTaskComplexInvestigateDisturbance::CTaskComplexInvestigateDisturbance(const CTaskComplexInvestigateDisturbance& o) :
    CTaskComplexInvestigateDisturbance{ o.m_pos, o.m_entity }
{
}

// 0x66C7A0
CTaskComplexInvestigateDisturbance::~CTaskComplexInvestigateDisturbance() {
    CEntity::SafeCleanUpRef(m_entity);
}

// 0x6749F0
CTask* CTaskComplexInvestigateDisturbance::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType())
    {
    case TASK_SIMPLE_ANIM: { // 0x674a23
        ped->Say(CTX_GLOBAL_STEALTH_NOTHING_THERE);
        return new CTaskSimpleScratchHead{};
    }
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
    case TASK_COMPLEX_SEEK_ENTITY: // 0x674a8a
        return new CTaskSimpleRunAnim(ped->m_nAnimGroup, ANIM_ID_ROADCROSS);
    case TASK_COMPLEX_TURN_TO_FACE_ENTITY: { // 0x674A9A
        if (m_entity) {
            return new CTaskComplexSeekEntity<CEntitySeekPosCalculatorFixedPos>{
                m_entity,
                50'000,
                1000,
                0.5f,
                2.f,
                2.f,
                true,
                true,
                CEntitySeekPosCalculatorFixedPos{ m_pos }
            };
        } else {
            return new CTaskComplexGoToPointAndStandStill{
                PEDMOVE_WALK,
                m_pos,
                0.5f, // 0x674B15..: radius 0.5 (0x86FC84), move-state radius 2.0 (0x86FC88)
                2.f
            };
        }
    }
    default:
        return nullptr;
    }
}

// 0x66C820
CTask* CTaskComplexInvestigateDisturbance::CreateFirstSubTask(CPed* ped) {
    // If there's an event, say something
    if (const auto event = ped->GetIntelligence()->GetEventHandler().GetHistory().GetCurrentEvent()) {
        switch (event->GetEventType()) {
        case EVENT_SOUND_QUIET: {
            ped->Say(CGeneral::RandomBool(20) ? CTX_GLOBAL_STEALTH_ALERT_GENERIC : CTX_GLOBAL_STEALTH_ALERT_SOUND);
            break;
        }
        case EVENT_ACQUAINTANCE_PED_HATE_BADLY_LIT: {
            ped->Say(CGeneral::RandomBool(20) ? CTX_GLOBAL_STEALTH_ALERT_GENERIC : CTX_GLOBAL_STEALTH_ALERT_SIGHT);
            break;
        }
        }
    }

    // 0x66C905..0x66C973: the exe NORMALISES the direction to `m_pos`, sums the dot as (z + y) + x, compares against 0.707f (0x863C1C, not COS_45)
    // and ducks unless `dot < 0.707f` (so also for NaN)
    auto dir = m_pos - ped->GetPosition();
    dir.Normalise();
    const auto& fwd = ped->GetForward();
    if (!((dir.z * fwd.z + dir.y * fwd.y) + dir.x * fwd.x < 0.707f)) {
        ped->GetIntelligence()->SetTaskDuckSecondary(5000);
    } else {
        g_ikChainMan.LookAt("TaskInvDisturb", ped, nullptr, 5000, BONE_UNKNOWN, &m_pos, false, 0.25f, 500, 3, false);
    }

    return m_entity
        ? new CTaskComplexTurnToFaceEntityOrCoord{ m_entity, 3.f }
        : new CTaskComplexTurnToFaceEntityOrCoord{ m_pos, 3.f };
}
