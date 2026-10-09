/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "TaskSimple.h"
class CAnimBlendAssociation;
class CEntity;
class CPed;
class CPlayerPed;
class CVehicle;
class CObject;

enum eFightAttackType : int8 {
    FIGHT_ATTACK_HIT_1 = 0,
    FIGHT_ATTACK_HIT_2 = 1,
    FIGHT_ATTACK_HIT_3 = 2,
    FIGHT_ATTACK_FIGHT_BLOCK = 3,
    FIGHT_ATTACK_FIGHTIDLE = 4,
};

class NOTSA_EXPORT_VTABLE CMeleeInfo {
public:
    AssocGroupId         m_nAnimGroup;      // 0x00
    float                m_fRanges;         // 0x04
    std::array<float, 5> m_fHit;            // 0x08 - Hit time (in 1/30s => in seconds when loaded from `melee.dat`)
    std::array<float, 5> m_fChain;          // 0x1C
    std::array<float, 5> m_fRadius;         // 0x30
    float                m_fGroundLoop;     // 0x44
    float                ABlockHit;         // 0x48
    float                ABlockChain;       // 0x4C
    std::array<uint8, 5> m_nHitLevel;       // 0x50 - See `GetHitLevel`
    std::array<uint8, 5> m_nDamage;         // 0x55
    uint8                pad_5A[2];         // 0x5A
    std::array<int32, 5> m_Hit;             // 0x5C - Audio event id (See `GetHitSound`)
    std::array<int32, 5> m_AltHit;          // 0x70
    uint16               m_wFlags;          // 0x84
};
VALIDATE_SIZE(CMeleeInfo, 0x88);

class NOTSA_EXPORT_VTABLE CTaskSimpleFight : public CTaskSimple {
public:
    bool                   m_bIsFinished;
    bool                   m_bIsInControl;
    bool                   m_bAnimsReferenced;
    AssocGroupId           m_nRequiredAnimGroup;
    uint16                 m_nIdlePeriod;
    uint16                 m_nIdleCounter;
    int8                   m_nContinueStrike;
    int8                   m_nChainCounter;
    CEntity*               m_pTargetEntity;
    CAnimBlendAssociation* m_pAnim;
    CAnimBlendAssociation* m_pIdleAnim;
    int8                   m_nComboSet;
    eFightAttackType       m_nCurrentMove;
    uint8                  m_nNextCommand;
    uint8                  m_nLastCommand;

    static inline auto& m_aComboData = StaticRef<std::array<CMeleeInfo, 13>>(0xC170D0); // NOTE: 12 fighting styles (indexed with `m_nComboSet - 4`) + 1 (used by the pistol whip, `m_nComboSet == 16`)
    static inline auto& m_aHitOffsets = StaticRef<std::array<CVector, 7>>(0xC177D0); // Loaded from the `START_LEVELS` section of `melee.dat`

public:
    static constexpr auto Type = eTaskType::TASK_SIMPLE_FIGHT;

    CTaskSimpleFight(CEntity* entity, int32 nCommand, uint32 nIdlePeriod = 10000);
    ~CTaskSimpleFight() override;

    eTaskType GetTaskType() const override { return Type; }
    CTask* Clone() const override { return new CTaskSimpleFight(m_pTargetEntity, m_nLastCommand, m_nIdlePeriod); }
    bool MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override;
    bool ProcessPed(CPed* ped) override;

    static void LoadMeleeData();

    bool BeHitWhileBlocking(CPed* victim, CPed* creator, int8 comboSet, int8 move); // 0x61C650 - Called on the *victim's* fight task, `comboSet`/`move` are the attacker's. Returns true if the hit is blocked
    int16 ChooseAttackAI(CPed* ped);     // 0x624A40
    int16 ChooseAttackPlayer(CPed* ped); // 0x624710
    bool ControlFight(CEntity* entity, uint8 command);

    void FightHitCar(CPed* ped, CVehicle* vehicle, const CVector& point, const CVector& normal, int16 piece, int8 surface); // 0x61D0B0
    void FightHitObj(CPed* ped, CObject* object, const CVector& point, const CVector& normal, int16 piece, int8 surface);  // 0x61D400
    CPed* FightHitPed(CPed* creator, CPed* victim, const CVector& point, const CVector& dir, int16);                        // 0x61CBA0 - Returns the ped that was hit (or null)
    void FightSetUpCol(float radius);                                                                                       // 0x61D5F0
    bool FightStrike(CPed* ped, CVector& posn);                                                                             // 0x6240B0 - Always returns false

    void FindTargetOnGround(CPed* ped);
    static void FinishMeleeAnimCB(CAnimBlendAssociation*, void*); // 0x61DAE0

    bool IsComboSet();
    bool IsHitComboSet();

    int8 GetAvailableComboSet(CPed* ped, int8 command); // 0x61C7F0
    void GetComboType(char*);
    AssocGroupId GetComboAnimGroupID();
    static uint8 GetHitLevel(const char*);   // 0x5BD360
    static int32 GetHitSound(int32);         // 0x5BD3B0
    void GetRange();
    float GetStrikeDamage(CPed* ped);        // 0x61C740

    void SetPlayerMoveAnim(CPlayerPed* player); // 0x61C9B0
    void StartAnim(CPed* ped, int32 move);      // 0x623B10

    bool IsTargetInRange(CPed* ped); // 0x61D6F0 - Not reversed yet (name guessed, ~0x3F0 bytes)

private:
    friend void InjectHooksMain();
    static void InjectHooks();

    CTaskSimpleFight* Constructor(CEntity* entity, int32 nCommand, uint32 nIdlePeriod);
    CTaskSimpleFight* Destructor();

};
VALIDATE_SIZE(CTaskSimpleFight, 0x28);
