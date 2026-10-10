#pragma once

#include "ConversationForPed.h"
#include "PedToPlayerConversations.h"
#include "ConversationNode.h"

class CPed;

enum {
    MAX_NUM_CONVERSATIONS = 14,
    MAX_NUM_CONVERSATION_NODES = 50,
    MAX_NUM_TEMP_CONVERSATION_NODES = 12,
};

class CTempConversationNode {
public:
    char  m_Name[8];
    char  m_NameNodeYes[8];
    char  m_NameNodeNo[8];
    int32 m_FinalSlot;
    int16 m_NodeYes;
    int16 m_NodeNo;
    int32 m_Speech;
    int32 m_SpeechY;
    int32 m_SpeechN;
    //void  Clear();
};

VALIDATE_SIZE(CTempConversationNode, 0x2C);

class CConversations {
public:
    enum class eAwkwardSayStatus : int32 {
        INACTIVE = 0,
        LOADING  = 1,
        PLAYING  = 2,
    };

    static inline NOTSA_GLOBAL(m_AwkwardSayStatus, 0x9691C4, (eAwkwardSayStatus), {});
    static inline NOTSA_GLOBAL(m_SettingUpConversation, 0x9691D0, (bool), {});
    static inline NOTSA_GLOBAL(m_Conversations, 0x9691D8, (std::array<CConversationForPed, MAX_NUM_CONVERSATIONS>), {});
    static inline NOTSA_GLOBAL(m_Nodes, 0x969570, (std::array<CConversationNode, MAX_NUM_CONVERSATION_NODES>), {});
    static inline NOTSA_GLOBAL(m_aTempNodes, 0x969360, (std::array<CTempConversationNode, MAX_NUM_TEMP_CONVERSATION_NODES>), {});
    static inline NOTSA_GLOBAL(m_SettingUpConversationNumNodes, 0x9691C8, (int32), {});
    static inline NOTSA_GLOBAL(m_SettingUpConversationPed, 0x9691CC, (CPed*), {});

    static void InjectHooks();
    static void Clear();
    static void RemoveConversationForPed(CPed* ped);

    static void                 Update();
    static void                 SetUpConversationNode(const char* questionKey, const char* answerYesKey, const char* answerNoKey, int32 questionWAV, int32 answerYesWAV, int32 answerNoWAV);
    static bool                 IsPlayerInPositionForConversation(CPed* ped, bool randomConversation);
    static bool                 IsConversationGoingOn();
    static CConversationForPed* FindConversationForPed(CPed* ped);
    static bool                 IsConversationAtNode(const char* pName, CPed* pPed);
    static void                 AwkwardSay(int32 whatToSay, CPed* speaker);
    static void                 EnableConversation(CPed* ped, bool enabled);
    static void                 StartSettingUpConversation(CPed* ped);
    static void                 DoneSettingUpConversation(bool bSuppressSubtitles);

    /*
    static void FindFreeNodeSlot();
    static void FindFreeConversationSlot();
    
     */
};
