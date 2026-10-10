#pragma once

class CPed;
class CVehicle;

class CPedToPlayerConversations {
public:
    enum class eP2pState : uint32 {
        INACTIVE = 0,
        PEDHASOPENED,
        WAITINGFORFINALWORD,
        WAITINGTOFINISH,
    };

    static inline NOTSA_GLOBAL(m_State, 0x969A20, (eP2pState), {});
    static inline NOTSA_GLOBAL(m_pPed, 0x9691C0, (CPed*), {});
    static inline NOTSA_GLOBAL(m_Topic, 0x9691BC, (int32), {});
    static inline NOTSA_GLOBAL(m_TimeOfLastPlayerConversation, 0x9691B4, (uint32), {});
    static inline NOTSA_GLOBAL(m_StartTime, 0x9691B8, (uint32), {});
    static inline NOTSA_GLOBAL(m_bPositiveReply, 0x9691B0, (bool), {}); // unused
    static inline NOTSA_GLOBAL(m_bPositiveOpening, 0x9691B1, (bool), {});
    static inline NOTSA_GLOBAL(m_NextPedIndexToCheck, 0x969A3C, (int32), {});     // NOTSA name: index into the ped pool of the ped that is checked next for starting a conversation
    static inline NOTSA_GLOBAL(m_pPlayerVehicle, 0x969A40, (CVehicle*), {}); // NOTSA name: last known vehicle of the player (referenced)

    static void InjectHooks();
    static void Clear();
    static void Update();
    static void EndConversation();
};
