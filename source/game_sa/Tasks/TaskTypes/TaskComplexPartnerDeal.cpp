#include "StdInc.h"

#include "TaskComplexPartnerDeal.h"
#include "TaskComplexGangLeader.h"
#include "TaskComplexSequence.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "TaskSimpleRunAnim.h"

void CTaskComplexPartnerDeal::InjectHooks()
{
    RH_ScopedVirtualClass(CTaskComplexPartnerDeal, 0x870754, 14);
    RH_ScopedCategory("Tasks/TaskTypes");
    RH_ScopedInstall(Constructor, 0x684190);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x6823B0); // 5-byte thunk: JMP 0x681F20
    RH_ScopedVMTInstall(StreamRequiredAnims, 0x6823C0);
    RH_ScopedVMTInstall(GetPartnerSequence, 0x682440);
}

CTaskComplexPartnerDeal::CTaskComplexPartnerDeal(const char* commandName, CPed* partner, bool leadSpeaker, float distanceMultiplier, CVector point) :
    CTaskComplexPartner(commandName, partner, leadSpeaker, distanceMultiplier, true, 1, point)
{
    m_taskId = TASK_COMPLEX_PARTNER_DEAL;
    strcpy_s(m_animBlockName, "gangs");
}

CTaskComplexPartnerDeal* CTaskComplexPartnerDeal::Constructor(const char* commandName, CPed* partner, bool leadSpeaker, float distanceMultiplier, CVector point)
{
    this->CTaskComplexPartnerDeal::CTaskComplexPartnerDeal(commandName, partner, leadSpeaker, distanceMultiplier, point);
    return this;
}

// 0x6823B0 (JMP 0x681F20)
CTask* CTaskComplexPartnerDeal::CreateFirstSubTask(CPed* ped)
{
    return CTaskComplexPartner::CreateFirstSubTask(ped);
}

// 0x6823C0
void CTaskComplexPartnerDeal::StreamRequiredAnims()
{
    if (!m_requiredAnimsStreamedIn) {
        if (CTaskComplexGangLeader::ShouldLoadGangAnims()) {
            const auto blk = CAnimManager::GetAnimationBlockIndex(m_animBlockName);
            if (!CAnimManager::GetAnimBlocks()[blk].IsLoaded) {
                CStreaming::RequestModel(IFPToModelId(blk), STREAMING_KEEP_IN_MEMORY);
                return;
            }
            CAnimManager::AddAnimBlockRef(blk);
            m_requiredAnimsStreamedIn = true;
        }
    } else if (!CTaskComplexGangLeader::ShouldLoadGangAnims()) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex(m_animBlockName));
        m_requiredAnimsStreamedIn = false;
    }
}

// 0x682440
CTaskComplexSequence* CTaskComplexPartnerDeal::GetPartnerSequence()
{
    const auto seq = new CTaskComplexSequence();
    seq->AddTask(new CTaskComplexTurnToFaceEntityOrCoord(m_partner, 0.5f, 0.02f));
    seq->AddTask(new CTaskSimpleRunAnim(ANIM_GROUP_GANGS, m_leadSpeaker ? ANIM_ID_DEALER_DEAL : ANIM_ID_DRUGS_BUY, 4.0f, false));
    return seq;
}
