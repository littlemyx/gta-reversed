#include "StdInc.h"

#include "TaskComplexGotoDoorAndOpen.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskSimpleRunAnim.h"
#include "EventAreaCodes.h"
#include "Animation/AnimManager.h"

void CTaskComplexGotoDoorAndOpen::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGotoDoorAndOpen, 0x86FFC0, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedOverloadedInstall(Constructor, "0", 0x66BB20, CTaskComplexGotoDoorAndOpen*(CTaskComplexGotoDoorAndOpen::*)(CObject *));
    RH_ScopedOverloadedInstall(Constructor, "1", 0x66BBA0, CTaskComplexGotoDoorAndOpen*(CTaskComplexGotoDoorAndOpen::*)(CVector const&, CVector const&));
    RH_ScopedInstall(Destructor, 0x66BC00);
    RH_ScopedVMTInstall(Clone, 0x66BCA0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x66C0D0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x66BD40);
    RH_ScopedVMTInstall(ControlSubTask, 0x66C1F0);
}

// 0x66BB20
CTaskComplexGotoDoorAndOpen::CTaskComplexGotoDoorAndOpen(CObject* door) : CTaskComplex() {
    m_Object = door;
    m_nStartTime = 0;
    m_nOffsetTime = 0;
    byte30 = 0;
    m_bRefreshTime = false;
    m_nFlags = m_nFlags & 0xF0 | 1; // todo: flags
    CEntity::SafeRegisterRef(m_Object);
}

// 0x66BBA0
CTaskComplexGotoDoorAndOpen::CTaskComplexGotoDoorAndOpen(const CVector& start, const CVector& end) : CTaskComplex() {
    m_Object = nullptr;
    m_Start = start;
    m_End = end;
    m_nStartTime = 0;
    m_nOffsetTime = 0;
    byte30 = 0;
    m_bRefreshTime = false;
    m_nFlags &= 244u; // todo: flags
}

// 0x66BC00
CTaskComplexGotoDoorAndOpen::~CTaskComplexGotoDoorAndOpen() {
    CEntity::SafeCleanUpRef(m_Object);
    if ((m_nFlags & 8) != 0) { // transition finished
        CPad::GetPad()->bPlayerOnInteriorTransition = false;
    }
}

// 0x66BCA0
CTask* CTaskComplexGotoDoorAndOpen::Clone() const {
    if ((m_nFlags & 1) != 0) { // todo: flags
        return new CTaskComplexGotoDoorAndOpen(m_Object);
    } else {
        return new CTaskComplexGotoDoorAndOpen(m_Start, m_End);
    }
}

// 0x66BC80
bool CTaskComplexGotoDoorAndOpen::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    return priority == ABORT_PRIORITY_IMMEDIATE && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, event);
}

// 0x66C0D0
CTask* CTaskComplexGotoDoorAndOpen::CreateNextSubTask(CPed* ped) {
    if (m_pSubTask->GetTaskType() != TASK_SIMPLE_GO_TO_POINT) {
        return nullptr;
    }

    if ((m_nFlags & 2) != 0) { // Already walked through the door
        if (ped->IsPlayer()) {
            CPad::GetPad(0)->bPlayerOnInteriorTransition = true;
        }
        return nullptr;
    }

    // Make sure the ped is playing the partial "door" walk anim
    auto& taskMgr = ped->GetTaskManager();
    if (!taskMgr.GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM)) {
        taskMgr.SetTaskSecondary(
            new CTaskSimpleRunAnim{ANIM_GROUP_DEFAULT, ANIM_ID_WALK_DOORPARTIAL, 8.f, false},
            TASK_SECONDARY_PARTIAL_ANIM
        );
    }

    const auto task = new CTaskSimpleGoToPoint{PEDMOVE_WALK, m_End, 0.2f, false, false};
    m_nFlags |= 2;
    return task;
}

// 0x66BD40
CTask* CTaskComplexGotoDoorAndOpen::CreateFirstSubTask(CPed* ped) {
    if (ped->IsPlayer()) {
        CPad::GetPad(0)->bPlayerOnInteriorTransition = true;
        m_nFlags |= 8;
    }

    CEventAreaCodes event{ped};
    GetEventGlobalGroup()->Add(&event);

    m_nStartTime    = CTimer::GetTimeInMS();
    m_nOffsetTime   = 1000;
    byte30          = true;

    if (!m_Object) {
        if ((m_nFlags & 1) != 0) { // The door was destroyed in the meantime
            return nullptr;
        }
        m_nFlags |= 2;
        return new CTaskSimpleGoToPoint{PEDMOVE_WALK, m_End, 0.5f, false, false};
    }

    const auto& objPos = m_Object->GetPosition();
    const auto& right  = m_Object->GetRight();
    const auto& fwd    = m_Object->GetForward();
    const auto& pedPos = ped->GetPosition();

    const auto dot = fwd.y * (objPos.y - pedPos.y) + fwd.z * (objPos.z - pedPos.z) + fwd.x * (objPos.x - pedPos.x);

    // Point next to the door
    const float px = right.x * 0.75f + objPos.x;
    const float py = objPos.y + right.y * 0.75f;
    const float pz = right.z * 0.75f + objPos.z;

    if (dot > 0.f) {
        m_Start = CVector{fwd.x * -0.5f + px, py + fwd.y * -0.5f, fwd.z * -0.5f + pz};
        m_End   = CVector{fwd.x *  2.0f + px, py + fwd.y *  2.0f, fwd.z *  2.0f + pz};
    } else {
        m_Start = CVector{px - fwd.x * -0.5f, py - fwd.y * -0.5f, pz - fwd.z * -0.5f};
        m_End   = CVector{px - fwd.x *  2.0f, py - fwd.y *  2.0f, pz - fwd.z *  2.0f};
    }

    if (m_Object->physicalFlags.bCollidable) {
        m_nFlags |= 4;
        m_Object->physicalFlags.bCollidable = false;
        m_Object->physicalFlags.bDisableCollisionForce = false;
    }

    if (RpAnimBlendClumpGetAssociation(ped->GetRpClump(), (uint32)ANIM_ID_SPRINT)) {
        CAnimManager::BlendAnimation(ped->GetRpClump(), ped->m_nAnimGroup, ANIM_ID_WALK, 1000.f);
    }

    return new CTaskSimpleGoToPoint{PEDMOVE_WALK, m_Start, 0.35f, false, false};
}

// 0x66C1F0
CTask* CTaskComplexGotoDoorAndOpen::ControlSubTask(CPed* ped) {
    bool doAbortCheck;
    if (byte30) {
        if (m_bRefreshTime) {
            m_nStartTime   = CTimer::GetTimeInMS();
            m_bRefreshTime = false;
        }
        doAbortCheck = CTimer::GetTimeInMS() >= m_nOffsetTime + m_nStartTime;
    } else {
        doAbortCheck = false;
    }

    // The door got destroyed while we were using it?
    if (!doAbortCheck && (m_Object || (m_nFlags & 1) == 0)) {
        return m_pSubTask;
    }

    if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
        if (ped->IsPlayer()) {
            CPad::GetPad(0)->bPlayerOnInteriorTransition = true;
        }
        return nullptr;
    }
    return m_pSubTask;
}
