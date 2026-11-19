#include "StdInc.h"
#include "TaskSimpleCarAlign.h"
#include "TaskUtilityLineUpPedWithCar.h"

void CTaskSimpleCarAlign::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleCarAlign, 0x86EC78, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x645B40);
    RH_ScopedInstall(Destructor, 0x645BE0);

    RH_ScopedGlobalInstall(FinishAnimCarAlignCB, 0x645E40);

    RH_ScopedInstall(FixHeading, 0x645CB0);
    RH_ScopedInstall(StartAnim, 0x64AC00);

    RH_ScopedVMTInstall(Clone, 0x649930);
    RH_ScopedVMTInstall(GetTaskType, 0x645BD0);
    RH_ScopedVMTInstall(MakeAbortable, 0x645C50);
    RH_ScopedVMTInstall(ProcessPed, 0x64D6D0);
    RH_ScopedVMTInstall(SetPedPosition, 0x645C70);
}

// 0x645B40
CTaskSimpleCarAlign::CTaskSimpleCarAlign(CVehicle* veh, CVector const& doorPos, uint32 targetDoor, CTaskUtilityLineUpPedWithCar* taskLineUp) :
    m_veh{veh},
    m_doorPos{doorPos},
    m_door{targetDoor},
    m_lineUpTask{taskLineUp}
{
    CEntity::SafeRegisterRef(m_veh);
}

// NOTSA
CTaskSimpleCarAlign::CTaskSimpleCarAlign(const CTaskSimpleCarAlign& o) :
    CTaskSimpleCarAlign{o.m_veh, o.m_doorPos, o.m_door, o.m_lineUpTask}
{
}

// 0x645BE0
CTaskSimpleCarAlign::~CTaskSimpleCarAlign() {
    CEntity::SafeCleanUpRef(m_veh);
    if (m_anim) {
        m_anim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
    }
}

// 0x645E40
void CTaskSimpleCarAlign::FinishAnimCarAlignCB(CAnimBlendAssociation* anim, void* data) {
    const auto self = static_cast<CTaskSimpleCarAlign*>(data);
    self->m_anim = nullptr;
    self->m_animHasFinished = true;
}

// 0x645CB0
void CTaskSimpleCarAlign::FixHeading(CPed* ped) {
    if (!m_veh || !m_veh->IsAutomobile()) {
        return;
    }

    const auto carLeftOrRight = [this] {
        switch (m_door) {
        case CAR_DOOR_LF:
        case CAR_DOOR_LR:
            return m_veh->GetRight() * -1.f; // Left
        default:
            return m_veh->GetRight();
        }
    }();

    // exe 0x645D33-0x645D6D: the dot is summed z, y, x in extended precision (one rounding)
    const auto pedFwd = ped->GetForwardVector();
    const x87d dotX   = (x87d)carLeftOrRight.z * pedFwd.z + (x87d)carLeftOrRight.y * pedFwd.y + (x87d)carLeftOrRight.x * pedFwd.x;
    if (!(dotX > 0.0 && dotX < (x87d)0.1f)) { // 0x645D4B: `<= 0` or NaN returns, `>= 0.1` returns
        return;
    }

    // exe 0x645DB1-0x645E20: only X and Y of the car vector are scaled by the dot and the product is subtracted TWICE
    // (fcom-less `fsub` chain at 0x645DDB/0x645DEA and 0x645DE4/0x645DF6) before the heading is derived
    const float tx = (float)(dotX * carLeftOrRight.x);
    const float ty = (float)(dotX * carLeftOrRight.y);
    const float vx = (float)((x87d)pedFwd.x - tx - tx);
    const float vy = (float)((x87d)pedFwd.y - ty - ty);
    ped->m_fAimingRotation = ped->m_fCurrentRotation = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(vx, vy, 0.f, 0.f));
}

// 0x64AC00
void CTaskSimpleCarAlign::StartAnim(CPed* ped) {
    const auto animId = [this, ped] {
        const auto doorOpenPos = CCarEnterExit::GetPositionToOpenCarDoor(m_veh, m_door);
        const auto z = doorOpenPos.z - ped->GetPosition().z;
        switch (m_door) {
        case CAR_DOOR_LF:
        case CAR_DOOR_LR:
            return (z <= 4.4f) ? ANIM_ID_CAR_ALIGN_LHS : ANIM_ID_CAR_ALIGNHI_LHS;
        default:
            return (z <= 4.4f) ? ANIM_ID_CAR_ALIGN_RHS : ANIM_ID_CAR_ALIGNHI_RHS;
        }
    }();
    m_anim = CAnimManager::BlendAnimation(
        ped->GetRpClump(),
        m_veh->GetAnimGroup().GetGroup(animId),
        animId,
        4.f
    );
    m_anim->SetFinishCallback(FinishAnimCarAlignCB, this);
}

// 0x645C50
bool CTaskSimpleCarAlign::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    switch (priority) {
    case ABORT_PRIORITY_IMMEDIATE: {
        if (m_anim) {
            m_anim->m_BlendDelta = -1000.f;
        }
        return true;
    }
    default:
        return false;
    }
}

// 0x64D6D0
bool CTaskSimpleCarAlign::ProcessPed(CPed* ped) {
    if (m_animHasFinished || !m_veh) {
        return true;
    }
    if (!m_anim) {
        FixHeading(ped);
        StartAnim(ped);
    }
    return false;
}

// 0x645C70
bool CTaskSimpleCarAlign::SetPedPosition(CPed* ped) {
    //const auto doorOpenPos = CCarEnterExit::GetPositionToOpenCarDoor(m_veh, m_door); // Result unused
    m_lineUpTask->ProcessPed(ped, m_veh, m_anim);
    return true;
}
