#include "StdInc.h"

#include "TaskComplexPartnerDeal.h"
#include "TaskComplexGangLeader.h"

void CTaskComplexPartnerDeal::InjectHooks()
{
    RH_ScopedVirtualClass(CTaskComplexPartnerDeal, 0x870754, 14);
    RH_ScopedCategory("Tasks/TaskTypes");
    RH_ScopedInstall(Constructor, 0x684190);
    RH_ScopedVMTInstall(StreamRequiredAnims, 0x6823C0);
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

CTask* CTaskComplexPartnerDeal::CreateFirstSubTask(CPed* ped)
{
    return plugin::CallMethodAndReturn<CTask*, 0x6823B0, CTask*, CPed*>(this, ped);
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

CTaskComplexSequence* CTaskComplexPartnerDeal::GetPartnerSequence()
{
    return plugin::CallMethodAndReturn<CTaskComplexSequence*, 0x682440, CTask*>(this);
}
