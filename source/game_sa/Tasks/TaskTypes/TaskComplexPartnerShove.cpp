#include "StdInc.h"

#include "TaskComplexPartnerShove.h"
#include "TaskComplexSequence.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "TaskSimpleRunAnim.h"

void CTaskComplexPartnerShove::InjectHooks()
{
    RH_ScopedVirtualClass(CTaskComplexPartnerShove, 0x870800, 14);
    RH_ScopedCategory("Tasks/TaskTypes");
    RH_ScopedInstall(Constructor, 0x6846F0);
    RH_ScopedVMTInstall(GetPartnerSequence, 0x683120);
}

CTaskComplexPartnerShove::CTaskComplexPartnerShove(const char* commandName, CPed* partner, bool leadSpeaker, float distanceMultiplier, int8 updateDirectionCount, CVector point) :
    CTaskComplexPartner(commandName, partner, leadSpeaker, distanceMultiplier, false, updateDirectionCount, point)
{
    m_updateDirectionCount = updateDirectionCount;
    m_taskId = TASK_COMPLEX_PARTNER_SHOVE;
}

CTaskComplexPartnerShove* CTaskComplexPartnerShove::Constructor(const char* commandName, CPed* partner, bool leadSpeaker, float distanceMultiplier, int8 updateDirectionCount, CVector point)
{
    this->CTaskComplexPartnerShove::CTaskComplexPartnerShove(commandName, partner, leadSpeaker, distanceMultiplier, updateDirectionCount, point);
    return this;
}

// 0x683120
CTaskComplexSequence* CTaskComplexPartnerShove::GetPartnerSequence()
{
    const auto seq = new CTaskComplexSequence();
    seq->AddTask(new CTaskComplexTurnToFaceEntityOrCoord(m_partner, 0.5f, 0.001f));
    seq->AddTask(new CTaskSimpleRunAnim(ANIM_GROUP_DEFAULT, m_leadSpeaker ? ANIM_ID_SHOVE_PARTIAL : ANIM_ID_HANDSUP, 4.0f, false));
    return seq;
}
