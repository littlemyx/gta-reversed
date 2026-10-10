#include "StdInc.h"

#include "TaskGoToVehicleAndLean.h"

#include "TaskSimpleStandStill.h"
#include "TaskSimpleAchieveHeading.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskLeanOnVehicle.h"
#include "TaskComplexGangLeader.h"
#include "PedIntelligence.h"

void CTaskGoToVehicleAndLean::InjectHooks() {
    RH_ScopedVirtualClass(CTaskGoToVehicleAndLean, 0x86FAC8, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x660E60);
    RH_ScopedInstall(Destructor, 0x660EE0);
    RH_ScopedInstall(CalcTargetPos, 0x664770);
    RH_ScopedInstall(DoTidyUp, 0x660F60);

    RH_ScopedVMTInstall(Clone, 0x6621B0);
    RH_ScopedVMTInstall(GetTaskType, 0x660ED0);
    RH_ScopedVMTInstall(MakeAbortable, 0x664500);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x664590);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x664D40);
    RH_ScopedVMTInstall(ControlSubTask, 0x664E60);
}

// 0x660E60
CTaskGoToVehicleAndLean::CTaskGoToVehicleAndLean(CVehicle* vehicle, int32 leanAnimDurationInMs) : CTaskComplex() {
    m_Vehicle = vehicle;
    m_LeanAnimDurationInMs = leanAnimDurationInMs;
    m_LeanOnVehicle = false;
    CEntity::SafeRegisterRef(m_Vehicle);
}

// 0x660EE0
CTaskGoToVehicleAndLean::~CTaskGoToVehicleAndLean() {
    if (m_LeanOnVehicle) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_LeanOnVehicle = false;
    }
    CEntity::SafeCleanUpRef(m_Vehicle);
}

// 0x664500
bool CTaskGoToVehicleAndLean::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority == ABORT_PRIORITY_IMMEDIATE) {
        m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, event);
    } else if (!m_pSubTask->MakeAbortable(ped, priority, event)) {
        return false;
    }
    DoTidyUp(ped); // Same code as in the original, inlined
    return true;
}

// 0x664590
CTask* CTaskGoToVehicleAndLean::CreateNextSubTask(CPed* ped) {
    if (!m_Vehicle) {
        if (m_pSubTask) {
            m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr);
        }
        return nullptr;
    }

    if (m_pSubTask->GetTaskType() == TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL) { // Got to the spot, now face the correct direction
        const auto& right = m_Vehicle->GetRight();
        const auto  dirX  = m_bPedOnRightSide ? right.x : -right.x;
        const auto  dirY  = m_bPedOnRightSide ? right.y : -right.y;
        const auto  heading = CGeneral::GetRadianAngleBetweenPoints(dirX, dirY, 0.f, 0.f);
        CGeneral::LimitRadianAngle(heading); // BUG: Result is discarded
        return new CTaskSimpleAchieveHeading{ heading, 0.5f, 0.2f };
    }

    if (m_pSubTask->GetTaskType() != TASK_SIMPLE_ACHIEVE_HEADING && m_pSubTask->GetTaskType() != TASK_SIMPLE_STAND_STILL) {
        DoTidyUp(ped);
        return nullptr;
    }

    if (!m_LeanOnVehicle) { // Anims not loaded yet
        return new CTaskSimpleStandStill{ 500, false, false, 8.f };
    }
    return new CTaskLeanOnVehicle{ m_Vehicle, m_LeanAnimDurationInMs, 0 };
}

// 0x664D40
CTask* CTaskGoToVehicleAndLean::CreateFirstSubTask(CPed* ped) {
    if (m_Vehicle) {
        if (!m_Vehicle->vehicleFlags.bHasGangLeaningOn) {
            m_Vehicle->vehicleFlags.bHasGangLeaningOn = true;

            m_TargetPos = CalcTargetPos(ped);

            if ((m_TargetPos - ped->GetPosition()).SquaredMagnitude() < 1.f) { // Already there
                return nullptr;
            }
            return new CTaskComplexGoToPointAndStandStill{ PEDMOVE_WALK, m_TargetPos, 0.05f, 2.f, false, true };
        }
        // Someone else is already leaning on it
        m_Vehicle->vehicleFlags.bHasGangLeaningOn = false; // BUG? (Original code clears the flag even though it was set by someone else)
    }

    if (m_pSubTask) {
        m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr);
    }
    return nullptr;
}

// 0x664E60
CTask* CTaskGoToVehicleAndLean::ControlSubTask(CPed* ped) {
    if (!m_Vehicle) {
        if (m_pSubTask) {
            m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr);
        }
        return nullptr;
    }

    if (ped->GetIntelligence()->m_AnotherStaticCounter > 30) {
        DoTidyUp(ped);
        return nullptr;
    }

    // Abort if the vehicle is being driven or is moving
    const auto offset = ped->GetPosition() - m_Vehicle->GetPosition();
    const auto speed  = m_Vehicle->GetSpeed(offset);
    if (m_Vehicle->m_pDriver || speed.Magnitude() > 0.01f) {
        if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            DoTidyUp(ped);
            return nullptr;
        }
    }

    // Abort if the spot we want to lean on has moved
    const auto newPos = CalcTargetPos(ped);
    if ((newPos - m_TargetPos).SquaredMagnitude() > 0.010000001f) { // NOTE: 0x3C23D70B, not `0.01f` (0x3C23D70A)
        if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            DoTidyUp(ped);
            return nullptr;
        }
    }

    if (!m_LeanOnVehicle) {
        if (CTaskComplexGangLeader::ShouldLoadGangAnims()) {
            const auto blk = CAnimManager::GetAnimationBlockIndex("gangs");
            if (CAnimManager::GetAnimBlocks()[blk].IsLoaded) {
                CAnimManager::AddAnimBlockRef(blk);
                m_LeanOnVehicle = true;
            } else {
                CStreaming::RequestModel(IFPToModelId(blk), STREAMING_KEEP_IN_MEMORY);
            }
        }
    } else if (!CTaskComplexGangLeader::ShouldLoadGangAnims()) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_LeanOnVehicle = false;
    }

    return m_pSubTask;
}

// 0x664770
CVector CTaskGoToVehicleAndLean::CalcTargetPos(CPed* ped) {
    const auto& bb = CModelInfo::GetModelInfo(m_Vehicle->m_nModelIndex)->GetColModel()->GetBoundingBox();

    const auto  diff  = ped->GetPosition() - m_Vehicle->GetPosition();
    const auto  right = m_Vehicle->m_matrix
        ? m_Vehicle->GetRight()
        : CVector{ (float)(x87::cos(m_Vehicle->m_placement.m_fHeading)), (float)(x87::sin(m_Vehicle->m_placement.m_fHeading)), 0.f };

    // Which side of the vehicle is the ped on?
    float localX;
    if (right.z * diff.z + right.y * diff.y + right.x * diff.x > 0.f) {
        m_bPedOnRightSide = true;
        localX            = bb.m_vecMax.x + 0.5f;
    } else {
        m_bPedOnRightSide = false;
        localX            = bb.m_vecMin.x - 0.5f;
    }
    return m_Vehicle->GetMatrix().TransformPoint(CVector{ localX, 0.f, 0.f });
}

// 0x660F60
void CTaskGoToVehicleAndLean::DoTidyUp(CPed* ped) {
    if (m_Vehicle)
        m_Vehicle->vehicleFlags.bHasGangLeaningOn = false;

    if (m_pSubTask)
        m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr);
}
