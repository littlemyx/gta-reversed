#include "StdInc.h"

#include "TaskComplexPartnerGreet.h"
#include "TaskComplexGangLeader.h"
#include "TaskComplexSequence.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "TaskSimpleRunAnim.h"
#include "TaskSimpleDoHandSignal.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleChat.h"

void CTaskComplexPartnerGreet::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexPartnerGreet, 0x87078c, 14);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x684210);
    RH_ScopedInstall(Destructor, 0x684280);

    RH_ScopedVMTInstall(Clone, 0x684DA0);
    RH_ScopedVMTInstall(GetTaskType, 0x681E00);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x6825A0);
    RH_ScopedVMTInstall(StreamRequiredAnims, 0x6825B0);
    RH_ScopedVMTInstall(GetPartnerSequence, 0x682630);
}

CTaskComplexPartnerGreet::CTaskComplexPartnerGreet(const char* commandName, CPed* partner, bool leadSpeaker, float distanceMultiplier, int32 handShakeType, CVector point) :
    CTaskComplexPartner(commandName, partner, leadSpeaker, distanceMultiplier, true, 1, point)
{
    m_handShakeType = handShakeType;
    m_taskId = TASK_COMPLEX_PARTNER_GREET;
    strcpy_s(m_animBlockName, "gangs");
}

CTaskComplexPartnerGreet::CTaskComplexPartnerGreet(const CTaskComplexPartnerGreet& o) :
    CTaskComplexPartnerGreet{
        o.m_commandName,
        o.m_partner,
        o.m_leadSpeaker,
        o.m_distanceMultiplier,
        o.m_handShakeType,
        o.m_point
    }
{
}

void CTaskComplexPartnerGreet::StreamRequiredAnims() {
    CAnimManager::StreamAnimBlock(m_animBlockName, CTaskComplexGangLeader::ShouldLoadGangAnims(), m_requiredAnimsStreamedIn);
}

// 0x682630
CTaskComplexSequence* CTaskComplexPartnerGreet::GetPartnerSequence() {
    const auto seq = new CTaskComplexSequence();
    seq->AddTask(new CTaskComplexTurnToFaceEntityOrCoord(m_partner, 0.5f, 0.001f));

    switch (m_handShakeType) {
    case 0: seq->AddTask(new CTaskSimpleRunAnim(ANIM_GROUP_GANGS, ANIM_ID_HNDSHKAA, 4.0f, false)); break;
    case 1: seq->AddTask(new CTaskSimpleRunAnim(ANIM_GROUP_GANGS, ANIM_ID_HNDSHKBA, 4.0f, false)); break;
    case 2: seq->AddTask(new CTaskSimpleRunAnim(ANIM_GROUP_GANGS, m_leadSpeaker ? ANIM_ID_HNDSHKCA : ANIM_ID_HNDSHKCB, 4.0f, false)); break;
    case 3: seq->AddTask(new CTaskSimpleRunAnim(ANIM_GROUP_GANGS, ANIM_ID_HNDSHKDA, 4.0f, false)); break;
    case 4: seq->AddTask(new CTaskSimpleRunAnim(ANIM_GROUP_GANGS, ANIM_ID_HNDSHKEA, 4.0f, false)); break;
    case 5: seq->AddTask(new CTaskSimpleRunAnim(ANIM_GROUP_GANGS, ANIM_ID_HNDSHKFA, 4.0f, false)); break;
    default: break; // no task added
    }

    // 3000 + 4000 * [0, 2): `fild; fmul 2^-15; fmul -4000.0f; _ftol` (extended precision, exact product)
    const auto RandDuration = []() -> int32 {
        return 3000 - (int32)((double)(rand() & 0xFFFF) * (double)(1.0f / 32768.0f) * (double)-4000.0f);
    };
    const auto d1 = RandDuration();
    const auto d2 = RandDuration();
    const auto d3 = RandDuration();
    const auto d4 = RandDuration();
    const bool addExtra = rand() < 0x3FFF;

    if (m_leadSpeaker) {
        seq->AddTask(new CTaskSimpleDoHandSignal());
        seq->AddTask(new CTaskSimpleStandStill(1000, false, false, 8.0f));
        seq->AddTask(new CTaskSimpleStandStill(d1, false, false, 8.0f));
        seq->AddTask(new CTaskSimpleChat(d2));
    } else {
        seq->AddTask(new CTaskSimpleStandStill(1000, false, false, 8.0f));
        seq->AddTask(new CTaskSimpleDoHandSignal());
        seq->AddTask(new CTaskSimpleChat(d1));
        seq->AddTask(new CTaskSimpleStandStill(d2, false, false, 8.0f));
    }
    if (addExtra) {
        seq->AddTask(new CTaskSimpleStandStill(d3, false, false, 8.0f));
        seq->AddTask(new CTaskSimpleChat(d4));
    }
    return seq;
}
