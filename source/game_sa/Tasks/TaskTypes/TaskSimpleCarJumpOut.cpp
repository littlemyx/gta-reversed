#include "StdInc.h"
#include "PedPlacement.h"
#include "TaskSimpleCarJumpOut.h"
#include "TaskSimpleCarSetPedOut.h"

void CTaskSimpleCarJumpOut::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleCarJumpOut, 0x86ee70, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x64BE40);
    RH_ScopedInstall(Destructor, 0x647490);

    RH_ScopedGlobalInstall(FinishAnimCarJumpOutCB, 0x647700);

    RH_ScopedInstall(ComputeAnimID_ToHook, 0x647690);
    RH_ScopedInstall(StartAnim, 0x64BF00);

    RH_ScopedVMTInstall(Clone, 0x64D220);
    RH_ScopedVMTInstall(GetTaskType, 0x647500);
    RH_ScopedVMTInstall(MakeAbortable, 0x647510);
    RH_ScopedVMTInstall(ProcessPed, 0x64DD60);
    RH_ScopedVMTInstall(SetPedPosition, 0x647590);
}

// 0x64BE40
CTaskSimpleCarJumpOut::CTaskSimpleCarJumpOut(CVehicle* veh, uint32 door, CTaskUtilityLineUpPedWithCar* lineUpTask) :
    m_veh{veh},
    m_door{door},
    m_lineUpTask{lineUpTask},
    m_initialVehSpeed{ veh ? veh->GetMoveSpeed().Magnitude() : 0.f }
{
    CEntity::SafeRegisterRef(m_veh); // 0x64BE40: RegisterReference (the port CLEANED the reference up here: a dead vehicle left a dangling pointer, and the destructor's clean-up found no entry)
}

CTaskSimpleCarJumpOut::CTaskSimpleCarJumpOut(const CTaskSimpleCarJumpOut& o) :
    CTaskSimpleCarJumpOut{o.m_veh, o.m_door, o.m_lineUpTask}
{
}

// 0x647490
CTaskSimpleCarJumpOut::~CTaskSimpleCarJumpOut() {
    CEntity::SafeCleanUpRef(m_veh);

    if (m_anim) {
        m_anim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
    }
}

// 0x647700
void CTaskSimpleCarJumpOut::FinishAnimCarJumpOutCB(CAnimBlendAssociation* anim, void* data) {
    const auto self = static_cast<CTaskSimpleCarJumpOut*>(data);
    self->m_finishedAnim = true;
    self->m_anim = nullptr;
}

// 0x64BF00
void CTaskSimpleCarJumpOut::StartAnim(const CPed* ped) {
    const auto [groupId, animId] = ComputeAnimID();
    m_anim = CAnimManager::BlendAnimation(ped->GetRpClump(), groupId, animId, 8.f);
    m_anim->SetFinishCallback(FinishAnimCarJumpOutCB, this);
}

// Signature changed, `ComputeAnimID_Wrapper` is hooked instead.
// 0X647690
auto CTaskSimpleCarJumpOut::ComputeAnimID() -> std::pair<AssocGroupId, AnimationId> {
    const auto animId = [this] {
        switch (m_door) {
        case TARGET_DOOR_FRONT_RIGHT:
        case TARGET_DOOR_REAR_RIGHT:
            return ANIM_ID_CAR_ROLLOUT_RHS;;
        case TARGET_DOOR_DRIVER:
        case TARGET_DOOR_REAR_LEFT:
            return ANIM_ID_CAR_ROLLOUT_LHS;
        default:
            NOTSA_UNREACHABLE_RET(ANIM_ID_CAR_ROLLOUT_RHS);
        }
    }();
    return { m_veh->GetAnimGroup().GetGroup(animId), animId };
}

// 0x647510
bool CTaskSimpleCarJumpOut::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    switch (priority) {
    case ABORT_PRIORITY_IMMEDIATE: {
        if (m_anim) {
            m_anim->m_BlendDelta = -1000.f;
        }
        return true;
    }
    case ABORT_PRIORITY_URGENT: {
        if (event && m_anim && !ped->bInVehicle) {
            switch (event->GetEventType()) {
            case EVENT_IN_AIR:
            case EVENT_IN_WATER: {
                m_anim->m_BlendDelta = -2.f;
                m_finishedAnim = true;
                return true;
            }
            }
        }
        break;
    }
    }
    return false;
}

// 0x64DD60
bool CTaskSimpleCarJumpOut::ProcessPed(CPed* ped) {
    if (m_finishedAnim) {
        return true;
    }

    if (!m_veh) {
        return true;
    }

    // Start calculating animation shift for ped in certain vehicles
    if ([this]{
        if (!m_anim) {
            return true;
        }

        if (m_veh->IsBike() || m_veh->IsSubQuad()) {
            return m_anim->m_CurrentTime < 0.7333333f; // TODO: Magic?
        } else if (m_veh->IsAutomobile()) {
            return m_anim->m_CurrentTime < 0.45f;
        }

        return false;    
    }()) {
        m_initialVehSpeed = (float)(X87R(x87::pow(X87W((x87d)0.96f), X87W((x87d)CTimer::GetTimeStep())) * (x87d)m_initialVehSpeed)); // exe 0x64DDE6 (base = qword 0x86F0E0 = 0.96f widened): pow kept unrounded, the multiply rounds
        ped->m_vecAnimMovingShiftLocal.y += m_initialVehSpeed * CTimer::GetTimeStep();
    }

    // Not in vehicle anymore?
    if (!ped->bInVehicle) { // Inverted
        if (m_veh && m_veh->IsAutomobile()) { // 0x64DE0E
            ped->m_pedIK.bSlopePitch = true; // 0x64DE1F: `or [ped+0x528], 8` is CPedIK flag 0x8 (bSlopePitch); bUseArm is 0x4
            ped->bFallenDown = true;
        }
        return !m_anim;
    }

    // Anim not playing yet?
    if (!m_anim) { // Inverted - 0x64DE56
        StartAnim(ped); 
        return false;
    }

    // Start opening the door - 0x64DE74
    const auto GetDoorOpenTimeThreshold = [this] {
        if (m_veh->IsBike() || m_veh->IsSubQuad()) {
            return 0.35f;
        } else if (m_veh->IsPlane()) {
            return 0.15f;
        }
        return 0.07f;
    };
    if (GetDoorOpenTimeThreshold() >= m_anim->m_CurrentTime) {
        if (m_veh->IsAutomobile()) {
            const auto [grpId, animId] = ComputeAnimID(); // 0x64E00D
            m_veh->ProcessOpenDoor(ped, m_door, grpId, animId, m_anim->m_CurrentTime);
        }
        return false;
    }

    // 0x64DEC0
    {
        CTaskSimpleCarSetPedOut getOutTask{ m_veh, (eTargetDoor)m_door, true };
        getOutTask.m_nDoorFlagsToClear = m_doorFlagsToClear;
        getOutTask.m_nNumGettingInToClear = m_numGettingInToClear;
        getOutTask.ProcessPed(ped);
    }

    CCarEnterExit::RemoveCarSitAnim(ped);

    // Change rotation of ped
    {
        // 0x64DF04: CPlaceable::GetHeading leaves the extended result in ST0 and `+ PI` [0x858CB8] is added to it before the float is passed to LimitRadianAngle
        const auto rot = m_veh->GetHeadingExt();
        ped->m_fAimingRotation = ped->m_fCurrentRotation = m_veh->IsTotallyUpsideDown() ? CGeneral::LimitRadianAngle((float)(X87R(rot + (double)PI))) : (float)rot;
        ped->SetHeading(ped->m_fCurrentRotation);
    }
    ped->GetMoveSpeed() = m_veh->GetMoveSpeed();
    ped->m_pEntityIgnoredCollision = m_veh;
    ped->bKnockedOffBike = true;
    if (m_veh->IsAutomobile()) {
        m_veh->AsAutomobile()->m_damageManager.SetDoorOpen_Component((tComponent)m_door); // 0x64DF86..0x64DFD1: GetDoorStatus_Component/SetDoorStatus_Component (m_door is a TARGET_DOOR 8..11, not an eDoors index)
    }
    
    return false;
}

// 0x647590
bool CTaskSimpleCarJumpOut::SetPedPosition(CPed* ped) {
    if (!ped->bInVehicle) {
        return false;
    }

    ped->SetPedPositionInCar();

    // Do some Z position interpolation for ped for specific anim types
    if (m_anim) {
        switch (m_anim->m_AnimId) {
        case ANIM_ID_CAR_ROLLOUT_RHS:
        case ANIM_ID_CAR_ROLLOUT_LHS: {
            const auto& pedPos = ped->GetPosition();
            auto pedPosAdj{ pedPos };

            CWorld::pIgnoreEntity = m_veh;
            CPedPlacement::FindZCoorForPed(pedPosAdj);
            CWorld::pIgnoreEntity = nullptr;

            if (pedPosAdj.z > pedPos.z) {
                const auto timeDivier = [this] {
                    if (m_veh->IsBike() || m_veh->IsSubQuad()) {
                        return 0.27f;
                    }
                    return 0.07f;
                }();
                ped->GetMatrix().GetPosition().z += m_anim->m_CurrentTime / timeDivier * (pedPosAdj.z - pedPos.z);
            }
            break;
        }
        }
    }

    return true;
}
