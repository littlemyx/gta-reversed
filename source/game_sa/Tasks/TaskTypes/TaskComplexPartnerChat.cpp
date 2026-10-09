#include "StdInc.h"

#include "TaskComplexPartnerChat.h"
#include "TaskComplexSequence.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "TaskComplexChat.h"
#include "TaskSimpleChat.h"
#include "TaskSimpleStandStill.h"

void CTaskComplexPartnerChat::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexPartnerChat, 0x8707C4, 14);
    RH_ScopedCategory("Tasks/TaskTypes");
    RH_ScopedInstall(Constructor, 0x684290);
    RH_ScopedVMTInstall(MakeAbortable, 0x682C60);
    RH_ScopedVMTInstall(GetPartnerSequence, 0x684380);
}

// 0x684290
CTaskComplexPartnerChat::CTaskComplexPartnerChat(const char* commandName, CPed* partner, bool leadSpeaker, float distanceMultiplier, int8 updateDirectionCount, bool conversationEnabled, bool a8, CVector point) :
    CTaskComplexPartner(commandName, partner, leadSpeaker, distanceMultiplier, false, updateDirectionCount, point)
{
    m_taskId = TASK_COMPLEX_PARTNER_CHAT;
    m_pedConversationLoaded = 0;
    m_conversationEnabled = conversationEnabled;
    if (conversationEnabled) {
        m_updateDirectionCount = 4;
    }
    field_75 = a8;
    strcpy_s(m_commandName, commandName);
}

// 0x684320
CTaskComplexPartnerChat::~CTaskComplexPartnerChat() {
    if (m_conversationEnabled && m_pedConversationLoaded) {
        CAEPedSpeechAudioEntity::ReleasePedConversation();
    }
}

CTaskComplexPartnerChat* CTaskComplexPartnerChat::Constructor(const char* commandName, CPed* partner, bool leadSpeaker, float distanceMultiplier, int8 updateDirectionCount, bool conversationEnabled, bool a8, CVector point)
{
    this->CTaskComplexPartnerChat::CTaskComplexPartnerChat(commandName, partner, leadSpeaker, distanceMultiplier, updateDirectionCount, conversationEnabled, a8, point);
    return this;
}

// 0x682C60
bool CTaskComplexPartnerChat::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (!m_pSubTask->MakeAbortable(ped, priority, event)) {
        return false;
    }
    if (m_conversationEnabled && m_pedConversationLoaded) {
        CAEPedSpeechAudioEntity::ReleasePedConversation();
        m_pedConversationLoaded = false;
    }
    return true;
}

// 0x684380
CTaskComplexSequence* CTaskComplexPartnerChat::GetPartnerSequence() {
    const auto seq = new CTaskComplexSequence();
    seq->AddTask(new CTaskComplexTurnToFaceEntityOrCoord(m_partner, 0.5f, 0.001f));

    // Pseudo-random durations seeded by the point; the sum stays in extended precision (x87) until `_ftol`
    const auto seed = (double)(m_updateDirectionCount << 4) + (double)m_point.z + (double)m_point.y + (double)m_point.x;
    const auto t    = (double)((int32)(int64)seed & 0x7F) * (double)0.0078125f;
    const auto inv  = (float)(1.0 - t);
    const auto a    = (int32)(int64)(t * (double)3000.0f + (double)3500.0f);
    const auto b    = (int32)(int64)((double)inv * (double)3000.0f + (double)3500.0f);

    if (!m_leadSpeaker) {
        m_conversationEnabled = static_cast<CTaskComplexPartnerChat*>(m_partner->GetIntelligence()->FindTaskByType(TASK_COMPLEX_PARTNER_CHAT))->m_conversationEnabled;
    }

    if (m_conversationEnabled) {
        // NOTSA: raw reads, the exe indexes `s_Conversation` with `6 - 2 * count` (can reach before the array)
        const auto first  = StaticRef<int16>(0xB613F8 - m_updateDirectionCount * 4);
        const auto second = StaticRef<int16>(0xB613FA - m_updateDirectionCount * 4);
        if (3 - m_updateDirectionCount == (int32)CAEPedSpeechAudioEntity::s_ConversationLength) {
            m_updateDirectionCount = 0;
        }
        if (m_leadSpeaker) {
            seq->AddTask(new CTaskComplexChat(true, m_partner, m_updateDirectionCount, first));
            seq->AddTask(new CTaskComplexChat(false, m_partner, m_updateDirectionCount, first));
        } else {
            seq->AddTask(new CTaskComplexChat(false, m_partner, m_updateDirectionCount, second));
            seq->AddTask(new CTaskComplexChat(true, m_partner, m_updateDirectionCount, second));
        }
    } else if (m_leadSpeaker) {
        seq->AddTask(new CTaskSimpleChat(a));
        seq->AddTask(new CTaskSimpleStandStill(b, false, false, 8.0f));
    } else {
        seq->AddTask(new CTaskSimpleStandStill(a, false, false, 8.0f));
        seq->AddTask(new CTaskSimpleChat(b));
    }
    return seq;
}
