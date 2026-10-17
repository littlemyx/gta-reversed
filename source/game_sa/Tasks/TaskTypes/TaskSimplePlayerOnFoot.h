/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "TaskSimple.h"
#include "AnimBlendAssociation.h"

NOTSA_GLOBAL_EXTERN(gLastRandomNumberForIdleAnimationID, (int32));
NOTSA_GLOBAL_EXTERN(gLastTouchTimeDelta, (uint32));
NOTSA_GLOBAL_EXTERN(gDuckAnimBlendData, (float));
NOTSA_GLOBAL_EXTERN(gbUnknown_8D2FE8, (bool));

class CPed;

class NOTSA_EXPORT_VTABLE CTaskSimplePlayerOnFoot : public CTaskSimple {
public:
    uint32   m_PlayerIdlesAnimBlockId;
    uint32   m_nFrameCounter;
    int32    m_nTimer;
    int32    dword_14;           // always 0
    CEntity* m_pLookingAtEntity; // always 0

public:
    static constexpr auto Type = TASK_SIMPLE_PLAYER_ON_FOOT;

    CTaskSimplePlayerOnFoot();
    ~CTaskSimplePlayerOnFoot() override = default;

    eTaskType GetTaskType() const override {
        return Type;
    } // 0x6857C0
    CTask* Clone() const override {
        return new CTaskSimplePlayerOnFoot();
    } // 0x68AFF0
    bool MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override;
    bool ProcessPed(CPed* ped) override;

    void ProcessPlayerWeapon(CPlayerPed* player);
    void PlayIdleAnimations(CPlayerPed* player);
    void PlayerControlFighter(CPlayerPed* player);
    static void PlayerControlZeldaWeapon(CPlayerPed* player);
    static void PlayerControlDucked(CPlayerPed* player);
    int32 PlayerControlZelda(CPlayerPed* player, bool bAvoidJumpingAndDucking);

private:
    friend void InjectHooksMain();
    static void InjectHooks();

    // 0x685750
    CTaskSimplePlayerOnFoot* Constructor() {
        this->CTaskSimplePlayerOnFoot::CTaskSimplePlayerOnFoot();
        return this;
    }

    // 0x68B0C0
    CTaskSimplePlayerOnFoot* Destructor() {
        this->CTaskSimplePlayerOnFoot::~CTaskSimplePlayerOnFoot();
        return this;
    }
};

VALIDATE_SIZE(CTaskSimplePlayerOnFoot, 0x1C);
