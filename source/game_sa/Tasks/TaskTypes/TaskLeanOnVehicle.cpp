#include "StdInc.h"

#include "TaskLeanOnVehicle.h"
#include "AnimManager.h"

void CTaskLeanOnVehicle::InjectHooks() {
    RH_ScopedVirtualClass(CTaskLeanOnVehicle, 0x86FAF4, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x660F90);
    RH_ScopedInstall(Destructor, 0x661030);

    RH_ScopedInstall(FinishAnimCB, 0x661160);

    RH_ScopedVMTInstall(MakeAbortable, 0x661110);
    RH_ScopedVMTInstall(ProcessPed, 0x6648C0);

}

// 0x660F90
CTaskLeanOnVehicle::CTaskLeanOnVehicle(CEntity* vehicle, int32 leanAnimDurationInMs, uint8 a4) {
    m_LeanAnimDurationInMs = leanAnimDurationInMs;
    m_Vehicle = vehicle->AsVehicle(); // todo: strange
    field_10 = a4;
    m_LeanAnim = nullptr;
    m_LastAnimId = ANIM_ID_UNDEFINED;
    m_bFinished = 0;
    m_LeanAnimId = 0;
    m_StopLeaning = 0;
    CEntity::SafeRegisterRef(m_Vehicle);
}

// 0x661030
CTaskLeanOnVehicle::~CTaskLeanOnVehicle() {
    CEntity::SafeCleanUpRef(m_Vehicle);
    if (m_LeanAnim) {
        m_LeanAnim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
    }
}

// 0x661160
void CTaskLeanOnVehicle::FinishAnimCB(CAnimBlendAssociation* assoc, void* data) {
    const auto self = notsa::cast<CTaskLeanOnVehicle>(static_cast<CTask*>(data));

    self->m_LastAnimId = assoc->m_AnimId;

    if (assoc->m_AnimId == ANIM_ID_LEANOUT) {
        assoc->m_BlendDelta = -1000.0f;
        self->m_bFinished = true;
    }

    if (self->m_LeanAnimId) {
        if (assoc->m_AnimId == ANIM_ID_LEANIN) {
            assoc->m_BlendDelta = -1000.0f;
            self->m_bFinished = true;
        }
    }

    self->m_LeanAnim = nullptr;
}

// 0x661110
bool CTaskLeanOnVehicle::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority == ABORT_PRIORITY_IMMEDIATE) {
        if (m_LeanAnim) {
            m_LeanAnim->m_BlendDelta = -1000.0f;
            m_LeanAnim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
            m_LeanAnim = nullptr;
        }
        m_StopLeaning = true;
        ped->m_pEntityIgnoredCollision = nullptr;
        return true;
    }

    m_LeanAnimId = 1;
    return false;
}

// 0x6648C0
bool CTaskLeanOnVehicle::ProcessPed(CPed* ped) {
    if (!m_Vehicle) {
        m_StopLeaning = true;
    }
    ped->m_pEntityIgnoredCollision = m_Vehicle;

    const int32 curAnimId = m_LeanAnim ? (int32)m_LeanAnim->m_AnimId : -1;

    if (m_StopLeaning) {
        ped->m_pEntityIgnoredCollision = nullptr;
        return true;
    }

    ped->SetMoveState(PEDMOVE_STILL);

    if (m_bFinished && !RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_LEANOUT)) {
        ped->m_pEntityIgnoredCollision = nullptr;
        return true;
    }

    const auto BlendAnim = [&](AnimationId animId, float blendDelta) {
        m_LeanAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_GANGS, animId, blendDelta);
        m_LeanAnim->SetFinishCallback(FinishAnimCB, this);
        return false;
    };

    if (m_LeanAnimId) { // Requested to stop leaning (see `MakeAbortable`)
        switch (curAnimId) {
        case ANIM_ID_LEANIN:
            m_LeanAnim->m_BlendDelta = -8.f;
            break;
        case ANIM_ID_LEANIDLE:
            m_LeanAnim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
            return BlendAnim(ANIM_ID_LEANOUT, 1000.f);
        case ANIM_ID_LEANOUT:
            m_LeanAnim->m_Speed = 3.f;
            break;
        }
    }

    if (!m_LeanAnim) {
        if (m_LastAnimId == -1) {
            if (!field_10) {
                return BlendAnim(ANIM_ID_LEANIN, 4.f);
            }
        } else if (m_LastAnimId != ANIM_ID_LEANIN) {
            return false;
        }
        m_LeanTimer.Start(m_LeanAnimDurationInMs);
        return BlendAnim(ANIM_ID_LEANIDLE, 1000.f);
    }

    if (m_LeanTimer.m_bStarted && m_LeanTimer.IsOutOfTime() && m_LeanAnim->m_AnimId != ANIM_ID_LEANOUT) {
        m_LeanAnim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
        return BlendAnim(ANIM_ID_LEANOUT, 1000.f);
    }
    return false;
}
