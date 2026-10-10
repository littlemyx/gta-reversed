/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "eWeaponType.h"

class CVehicle;

enum class eDarkelStatus : uint16 {
    INITIAL,
    FRENZY_ON_GOING,
    FRENZY_PASSED,
    FRENZY_FAILED,
    FRENZY_ON_GOING_2P,
};

class CDarkel {
public:
    static inline NOTSA_GLOBAL(RegisteredKills, 0x969A50, (std::array<int16[2], 800>), {});
    static inline NOTSA_GLOBAL(pStartMessage, 0x96A6D0, (const GxtChar*), {});
    static inline NOTSA_GLOBAL(AmmoInterruptedWeapon, 0x96A6D4, (uint32), {});
    static inline NOTSA_GLOBAL(InterruptedWeaponType, 0x96A6D8, (eWeaponType), {});
    static inline NOTSA_GLOBAL(InterruptedWeaponTypeSelected, 0x96A6DC, (eWeaponType), {});
    static inline NOTSA_GLOBAL(TimeOfFrenzyStart, 0x96A6E0, (uint32), {});
    static inline NOTSA_GLOBAL(PreviousTime, 0x96A6E4, (int32), {});
    static inline NOTSA_GLOBAL(TimeLimit, 0x96A6E8, (int32), {});
    static inline NOTSA_GLOBAL(KillsNeeded, 0x96A6EC, (int32), {});
    static inline NOTSA_GLOBAL(ModelToKill, 0x96A6F0, (std::array<int32, 4>), {});
    static inline NOTSA_GLOBAL(WeaponType, 0x96A700, (eWeaponType), {});
    static inline NOTSA_GLOBAL(Status, 0x96A704, (eDarkelStatus), {});

    static inline NOTSA_GLOBAL(bHeadShotRequired, 0x969A49, (bool), {});
    static inline NOTSA_GLOBAL(bStandardSoundAndMessages, 0x969A4A, (bool), {});
    static inline NOTSA_GLOBAL(bProperKillFrenzy, 0x969A48, (bool), {});

public:
    static void InjectHooks();

    static bool FrenzyOnGoing();
    static void Init();
    static void DrawMessages();
    static eDarkelStatus ReadStatus();
    static void RegisterKillNotByPlayer(const CPed* killedPed);
    static bool ThisPedShouldBeKilledForFrenzy(const CPed& ped);
    static bool ThisVehicleShouldBeKilledForFrenzy(const CVehicle& vehicle);
    static void StartFrenzy(eWeaponType weaponType, int32 timeLimit, uint16 killsNeeded, int32 modelToKill, const GxtChar* startMessage, int32 modelToKill2, int32 modelToKill3, int32 modelToKill4, bool standardSoundAndMessages, bool needHeadShot);
    static void ResetModelsKilledByPlayer(int32 playerId);
    static int16 QueryModelsKilledByPlayer(eModelID modelId, int32 playerId);
    static int32 FindTotalPedsKilledByPlayer(int32 playerId);
    static void DealWithWeaponChangeAtEndOfFrenzy();
    static bool CheckDamagedWeaponType(eWeaponType damageWeaponId, eWeaponType expectedDamageWeaponId);
    static void Update();
    static void ResetOnPlayerDeath();
    static void FailKillFrenzy();
    static void RegisterKillByPlayer(const CPed& killedPed, eWeaponType damageWeaponId, bool headShotted, int32 playerId);
    static void RegisterCarBlownUpByPlayer(CVehicle& vehicle, int32 playerId);

    static uint8 CalcFade(uint32 t, uint32 begin, uint32 end);
};
