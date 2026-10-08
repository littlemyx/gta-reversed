#include "ConversationForPed.h"
#include "Conversations.h"

void CConversationForPed::InjectHooks() {
    RH_ScopedClass(CConversationForPed);
    RH_ScopedCategory("Conversations");
    RH_ScopedInstall(Update, 0x43C190);
    RH_ScopedInstall(IsPlayerInPositionForConversation, 0x43AC40);
}

void CConversationForPed::Clear(bool dontClearNodes) {
    if (!dontClearNodes) {
        CConversations::m_Nodes[m_FirstNode].ClearRecursively();
    }
    m_FirstNode                 = -1;
    m_CurrentNode               = -1;
    m_pPed                      = nullptr;
    m_LastChange                = 0;
    m_LastTimeWeWereCloseEnough = 0;
}

// 0x43C190
void CConversationForPed::Update() {
    if (!m_pPed || !m_Enabled || !IsPlayerInPositionForConversation(false)) {
        m_Status = eStatus::INACTIVE;
        return;
    }

    // Speaks the line (and shows the subtitle) of the current node
    const auto PedSpeaks = [this] {
        if (!m_SuppressSubtitles) {
            CMessages::ClearSmallMessagesOnly();
            CMessages::AddMessageJump(TheText.Get(CConversations::m_Nodes[m_CurrentNode].m_Name), 4000, 1, true);
        }

        const auto speech = CConversations::m_Nodes[m_CurrentNode].m_Speech;
        if (speech > 0) {
            m_pPed->Say((eGlobalSpeechContext)(uint16)speech);
        }
        if (speech < 0) {
            CConversations::AwkwardSay(-speech, m_pPed);
        }

        m_Status     = eStatus::PED_SPEAKING;
        m_LastChange = CTimer::GetTimeInMS();
    };

    // Player answered (suffix: 'Y' or 'N'), go to `nextNode`
    const auto PlayerAnswers = [this](const char* suffix, int32 speech, int32 nextNode) {
        if (!m_SuppressSubtitles) {
            CMessages::ClearSmallMessagesOnly();

            char key[16]{}; // NOTSA: Original used a global buffer at 0xB71670
            strcpy_s(key, CConversations::m_Nodes[m_CurrentNode].m_Name);
            strcat_s(key, suffix);
            CMessages::AddMessageJump(TheText.Get(key), 4000, 3, true);
        }

        if (speech > 0) {
            FindPlayerPed(-1)->Say((eGlobalSpeechContext)(uint16)speech);
        }
        if (speech < 0) {
            CConversations::AwkwardSay(-speech, FindPlayerPed(-1));
        }

        m_CurrentNode = nextNode;
        m_LastChange  = CTimer::GetTimeInMS();

        if (!m_SuppressSubtitles) {
            CMessages::AddMessageQ(TheText.Get(CConversations::m_Nodes[m_CurrentNode].m_Name), 4000, 1, true);
        }
        m_Status = eStatus::PLAYER_SPEAKING;
    };

    [&] {
        if (CTimer::GetTimeInMS() > m_LastTimeWeWereCloseEnough + 10'000u) { // 0x43C1C9
            return PedSpeaks();
        }

        if (m_Status == eStatus::PLAYER_SPEAKING) { // 0x43C1CF
            if (CTimer::GetTimeInMS() > m_LastChange + 4000u) {
                return PedSpeaks();
            }
            if (CPad::GetPad(0)->ConversationNoJustDown()) {
                return PedSpeaks();
            }
            if (CPad::GetPad(0)->ConversationYesJustDown()) {
                return PedSpeaks();
            }
        }

        // 0x43C218
        if (CTimer::GetTimeInMS() <= m_LastChange + 400u) {
            return;
        }
        if (m_Status != eStatus::PED_SPEAKING && m_Status != eStatus::WAITINGFORINPUT && m_Status != eStatus::INACTIVE) {
            return;
        }

        const auto& node = CConversations::m_Nodes[m_CurrentNode];
        if (CPad::GetPad(0)->ConversationNoJustDown() && node.m_NodeNo >= 0) {
            return PlayerAnswers("N", node.m_SpeechN, node.m_NodeNo);
        }
        if (CPad::GetPad(0)->ConversationYesJustDown() && node.m_NodeYes >= 0) {
            return PlayerAnswers("Y", node.m_SpeechY, node.m_NodeYes);
        }
    }();

    // 0x43C55D
    m_LastTimeWeWereCloseEnough = CTimer::GetTimeInMS();
    if (m_Status == eStatus::PED_SPEAKING && CTimer::GetTimeInMS() > m_LastChange + 4000u) {
        m_Status = eStatus::WAITINGFORINPUT;
    }
}

// 0x43AC40
bool CConversationForPed::IsPlayerInPositionForConversation(bool randomConversation) {
    return plugin::CallMethodAndReturn<bool, 0x43AC40, CConversationForPed*, bool>(this, randomConversation);
}
