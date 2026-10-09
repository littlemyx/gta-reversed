#include "StdInc.h"

#include "TaskComplexUseMobilePhone.h"
#include "TaskSimpleRunAnim.h"
#include "TaskSimplePause.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimplePlayerOnFoot.h"
#include "PlayerPedData.h"
#include "Events/Event.h"

// Model index of the cell phone prop (0x14A), attached / removed through CPed::AddWeaponModel / RemoveWeaponModel
static constexpr int32 PHONE_MODEL_ID = 0x14A;

static_assert(TASK_COMPLEX_USE_MOBILE_PHONE == 0x640 && TASK_SIMPLE_PHONE_TALK == 0x641 && TASK_SIMPLE_PHONE_IN == 0x642 && TASK_SIMPLE_PHONE_OUT == 0x643 && TASK_SIMPLE_PAUSE == 0xCA);
static_assert(ANIM_ID_PHONE_IN == 0x91 && ANIM_ID_PHONE_OUT == 0x92 && ANIM_ID_PHONE_TALK == 0x93 && WEAPON_UNIDENTIFIED == 0x37);

void CTaskComplexUseMobilePhone::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexUseMobilePhone, 0x86E454, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6348A0);
    RH_ScopedInstall(HidePhone, 0x6348F0);
    RH_ScopedInstall(Quit, 0x634A40);

    RH_ScopedVMTDestructorInstall(0x639660);
    RH_ScopedVMTInstall(Clone, 0x636FD0);
    RH_ScopedVMTInstall(GetTaskType, 0x6348D0);
    RH_ScopedVMTInstall(MakeAbortable, 0x634930);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x634A60);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x634C10);
    RH_ScopedVMTInstall(ControlSubTask, 0x634D60);
}

// 0x6348A0
CTaskComplexUseMobilePhone::CTaskComplexUseMobilePhone(int32 nDuration) :
    CTaskComplex{},
    m_nDuration{ nDuration },
    m_timer{},
    m_bIsAborting{ false },
    m_bQuit{ false }
{
}

// 0x636FD0
CTask* CTaskComplexUseMobilePhone::Clone() const {
    return new CTaskComplexUseMobilePhone{ m_nDuration };
}

//! Clears the two ped flags set by CreateFirstSubTask (bUsingMobilePhone = mask 2 at +0x478, bDontAcceptIKLookAts = mask 0x400000 at +0x474)
static void ClearPhoneFlags(CPed* ped) {
    ped->bUsingMobilePhone     = false;
    ped->bDontAcceptIKLookAts = false;
}

// 0x6348F0 (`this` is not used by the original)
void CTaskComplexUseMobilePhone::HidePhone(CPed* ped) {
    ped->RemoveWeaponModel(PHONE_MODEL_ID);
    ped->SetCurrentWeapon(ped->m_nSavedWeapon);
    ped->m_nSavedWeapon = WEAPON_UNIDENTIFIED;
    if (const auto pd = ped->GetPlayerData()) {
        pd->m_bDontAllowWeaponChange = false; // +0x85
    }
}

// 0x634A40
void CTaskComplexUseMobilePhone::Quit(CPed* ped) {
    if (!m_bQuit) {
        m_bQuit = true;
        MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr);
    }
}

// 0x634930
bool CTaskComplexUseMobilePhone::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority == ABORT_PRIORITY_IMMEDIATE) {
        const bool res = m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, event);
        if (res) {
            ClearPhoneFlags(ped);
            if (ped->m_nSavedWeapon != WEAPON_UNIDENTIFIED) {
                HidePhone(ped);
            }
        }
        return res;
    }

    if (m_pSubTask->GetTaskType() == TASK_SIMPLE_PAUSE) { // 0xCA
        ClearPhoneFlags(ped);
        return true;
    }

    if (event) {
        if (event->GetEventType() == EVENT_ON_FIRE && ped->IsPlayer()) {
            return true;
        }
        m_bIsAborting = true;
    }

    // Not `m_timer.Start(m_nDuration)`: the original stores unconditionally (also for a negative duration)
    m_timer.m_nStartTime = CTimer::GetTimeInMS();
    m_timer.m_nInterval  = m_nDuration;
    m_timer.m_bStarted   = true;

    if (m_pSubTask->GetTaskType() == TASK_SIMPLE_PHONE_OUT) {
        m_pSubTask->MakeAbortable(ped, priority, event);
    }
    return false;
}

// 0x634A60
CTask* CTaskComplexUseMobilePhone::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_PHONE_IN: { // 0x642 => the chat, for `m_nDuration` ms (a negative duration doesn't (re)start the timer)
        m_timer.Start(m_nDuration);
        return new CTaskSimpleRunAnim{ ANIM_GROUP_DEFAULT, ANIM_ID_PHONE_TALK, 4.0f, TASK_SIMPLE_PHONE_TALK, "PhoneChat", false };
    }
    case TASK_SIMPLE_PHONE_TALK: { // 0x641
        return new CTaskSimpleRunAnim{ ANIM_GROUP_DEFAULT, ANIM_ID_PHONE_OUT, 4.0f, TASK_SIMPLE_PHONE_OUT, "PhoneOut", false };
    }
    case TASK_SIMPLE_PHONE_OUT: { // 0x643
        if (!m_bQuit && m_bIsAborting) {
            return new CTaskSimplePause{ 1000 };
        }
        ClearPhoneFlags(ped);
        return nullptr;
    }
    case TASK_SIMPLE_PAUSE: { // 0xCA => the pause is over, start over unless quit
        m_bIsAborting      = false;
        m_timer.m_bStarted = false;
        if (!m_bQuit) {
            return CreateFirstSubTask(ped);
        }
        ClearPhoneFlags(ped);
        return nullptr;
    }
    default:
        return nullptr;
    }
}

// 0x634C10
CTask* CTaskComplexUseMobilePhone::CreateFirstSubTask(CPed* ped) {
    ped->bDontAcceptIKLookAts = true;
    ped->bUsingMobilePhone    = true;

    if (ped->IsPlayer()) {
        ped->AsPlayer()->ClearWeaponTarget();
    }

    if (ped->m_nSavedWeapon == WEAPON_UNIDENTIFIED) { // hide the current weapon while the phone is out
        ped->m_nSavedWeapon = ped->GetActiveWeapon().m_Type;
        ped->SetCurrentWeapon(WEAPON_UNARMED);
    }

    if (const auto pd = ped->GetPlayerData()) {
        pd->m_bDontAllowWeaponChange = true; // +0x85
    }

    if (ped->IsPlayer()) {
        TheCamera.ClearPlayerWeaponMode();
    }

    { // A stack CTaskSimpleStandStill(0, false, false, 8.0f) (ctor inlined) is run once to put the ped into the idle pose
        CTaskSimpleStandStill standStill{ 0, false, false, 8.0f };
        standStill.ProcessPed(ped); // 0x62F370
    }

    return new CTaskSimpleRunAnim{ ANIM_GROUP_DEFAULT, ANIM_ID_PHONE_IN, 4.0f, TASK_SIMPLE_PHONE_IN, "PhoneIn", false };
}

// 0x634D60
CTask* CTaskComplexUseMobilePhone::ControlSubTask(CPed* ped) {
    CTask* newSubTask = m_pSubTask;

    if (const auto pd = ped->GetPlayerData()) {
        pd->m_bDontAllowWeaponChange = true; // +0x85
    }

    if (m_pSubTask->GetTaskType() != TASK_SIMPLE_PAUSE) { // 0xCA
        const auto phoneIn  = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), (uint32)ANIM_ID_PHONE_IN);
        const auto phoneOut = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), (uint32)ANIM_ID_PHONE_OUT);
        if (phoneIn) {
            if (!((double)phoneIn->m_CurrentTime < (double)0.85f)) { // fcomp float 0x862E1C = 0.85f; `jne` on C0 => NaN passes
                ped->AddWeaponModel(PHONE_MODEL_ID);
            }
        } else if (phoneOut) {
            // 0x86E4A0 = 1.566 (double)
            if (!((double)phoneOut->m_CurrentTime < 1.566)) {
                if (!(((double)phoneOut->m_CurrentTime - (double)phoneOut->m_TimeStep) >= 1.566)) { // jp after test ah,5 => taken for ">=" only
                    HidePhone(ped);
                }
            }
        } else if (m_timer.IsOutOfTime()) {
            newSubTask = new CTaskSimpleRunAnim{ ANIM_GROUP_DEFAULT, ANIM_ID_PHONE_OUT, 4.0f, TASK_SIMPLE_PHONE_OUT, "PhoneOut", false };
        }
    }

    if (const auto pd = ped->GetPlayerData()) {
        pd->m_bPlayerSprintDisabled = true; // +0x84
        if (!(pd->m_fMoveBlendRatio >= 0.0f)) { // fcomp 0.0f (0x858B50); jp => skipped for ">=" only (NaN is reset too)
            pd->m_fMoveBlendRatio = 0.0f;
        }
        if (const auto task = static_cast<CTaskSimplePlayerOnFoot*>(ped->GetIntelligence()->FindTaskByType(TASK_SIMPLE_PLAYER_ON_FOOT))) {
            task->PlayerControlZelda(ped->AsPlayer(), true);
        }
        ped->GetPlayerData()->m_bPlayerSprintDisabled = false;
    }

    return newSubTask;
}
