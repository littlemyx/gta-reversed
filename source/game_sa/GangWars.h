/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "CarCtrl.h"
#include "Radar.h"
#include "PedType.h"

class CZone;
class CZoneInfo;

enum eGangAttackState {
    NO_ATTACK          = 0,
    WAR_NOTIFIED       = 1,
    PLAYER_CAME_TO_WAR = 2
};

enum eGangWarState {
    NOT_IN_WAR      = 0,
    PRE_FIRST_WAVE  = 1,
    FIRST_WAVE      = 2,
    PRE_SECOND_WAVE = 3,
    SECOND_WAVE     = 4,
    PRE_THIRD_WAVE  = 5,
    THIRD_WAVE      = 6
};

constexpr union {
    struct {
        uint8 r, g, b;
    };
    uint8 components[3];
} gaGangColors[TOTAL_GANGS]{ // based on gaGangColorsR, gaGangColorsG, gaGangColorsB
    { 200, 0,   200 },
    { 70,  200, 0   },
    { 255, 200, 0   },
    { 0,   0,   200 },
    { 255, 220, 190 },
    { 200, 200, 200 },
    { 240, 140, 240 },
    { 0,   200, 255 },
    { 255, 255, 255 },
    { 255, 255, 255 },
};

class CGangWars {
public:
    static inline NOTSA_GLOBAL(ZoneInfoForTraining, 0x8A5F40, (int32), { -1 }); // -1
    static inline NOTSA_GLOBAL(pDriveByCar, 0x96AB28, (CVehicle*), {});
    static inline NOTSA_GLOBAL(GangRatingStrength, 0x96AB2C, (std::array<int32, 3>), {});
    static inline NOTSA_GLOBAL(GangRatings, 0x96AB38, (std::array<int32, 3>), {});
    static inline NOTSA_GLOBAL(FightTimer, 0x96AB44, (uint32), {});
    static inline NOTSA_GLOBAL(TimeTillNextAttack, 0x96AB48, (float), {});
    static inline NOTSA_GLOBAL(Gang2, 0x96AB50, (int32), {});
    static inline NOTSA_GLOBAL(LastTimeInArea, 0x96AB54, (uint32), {});
    static inline NOTSA_GLOBAL(WarFerocity, 0x96AB58, (int32), {});
    static inline NOTSA_GLOBAL(Provocation, 0x96AB5C, (float), {});
    static inline NOTSA_GLOBAL(TimeStarted, 0x96AB60, (uint32), {});
    // State of defending gang war (i.e. fighting for enemy zone)
    static inline NOTSA_GLOBAL(State, 0x96AB64, (eGangWarState), {});
    // State of attacking gang war (i.e. fighting for own zone)
    static inline NOTSA_GLOBAL(State2, 0x96AB4C, (eGangAttackState), {});
    static inline NOTSA_GLOBAL(aSpecificZones, 0x96AB68, (std::array<int32, 6>), {});
    static inline NOTSA_GLOBAL(Difficulty, 0x96AB80, (float), {});
    static inline NOTSA_GLOBAL(Gang1, 0x96AB84, (eGangID), {});
    static inline NOTSA_GLOBAL(pZoneToFightOver, 0x96AB88, (CZone*), {});
    static inline NOTSA_GLOBAL(pZoneInfoToFightOver, 0x96AB8C, (CZoneInfo*), {});
    static inline NOTSA_GLOBAL(bGangWarsActive, 0x96AB90, (bool), {});
    static inline NOTSA_GLOBAL(bTrainingMission, 0x96AB91, (bool), {});
    static inline NOTSA_GLOBAL(bPlayerIsCloseby, 0x96AB92, (bool), {});
    static inline NOTSA_GLOBAL(bCanTriggerGangWarWhenOnAMission, 0x96AB93, (bool), {});
    static inline NOTSA_GLOBAL(NumSpecificZones, 0x96AB94, (int32), {});
    static inline NOTSA_GLOBAL(RadarBlip, 0x96AB98, (int32), {});
    static inline NOTSA_GLOBAL(TerritoryUnderControlPercentage, 0x96AB9C, (float), {});
    static inline NOTSA_GLOBAL(bIsPlayerOnAMission, 0x96ABA0, (bool), {});
    static inline NOTSA_GLOBAL(CoorsOfPlayerAtStartOfWar, 0x96ABBC, (CVector), {});
    static inline NOTSA_GLOBAL(PointOfAttack, 0x96ABC8, (CVector), {});

public:
    static void InjectHooks();

    static void InitAtStartOfGame();

    static bool Load();
    static bool Save();

    static void AddKillToProvocation(ePedType pedType);
    static bool AttackWaveOvercome();
    static uint32 CalculateTimeTillNextAttack(); // NOTE: Original returns an integer in EAX (via `_ftol`), callers convert it to float
    static bool CanPlayerStartAGangWarHere(CZoneInfo* zoneInfo);
    static void CheerVictory();
    static void ClearSpecificZonesToTriggerGangWar();
    static void ClearTheStreets();
    static bool CreateAttackWave(int32 warFerocity, int32 waveID);
    static bool CreateDefendingGroup(int32 unused);
    static void DoStuffWhenPlayerVictorious();
    static bool DoesPlayerControlThisZone(CZoneInfo* zoneInfo);
    static bool DontCreateCivilians();
    static void EndGangWar(bool bEnd);

    static bool GangWarFightingGoingOn();
    static bool GangWarGoingOn();

    static void MakeEnemyGainInfluenceInZone(int32 gangId, int32 density);
    static bool MakePlayerGainInfluenceInZone(float removeMult);

    static bool PedStreamedInForThisGang(eGangID gangId);
    static bool PickStreamedInPedForThisGang(eGangID gangId, int32& outPedId);
    static bool PickZoneToAttack();

    static void ReleaseCarsInAttackWave();
    static uint32 ReleasePedsInAttackWave(bool isEndOfWar, bool restoreGangPedsAcquaintance);

    static void SetGangWarsActive(bool active);
    static void SetSpecificZoneToTriggerGangWar(int32 zoneId);

    static void StartDefensiveGangWar();
    static void StartOffensiveGangWar();

    static void StrengthenPlayerInfluenceInZone(int32 density);
    static void SwitchGangWarsActive();

    static void TellGangMembersTo(bool isGangWarEnding);
    static void TellStreamingWhichGangsAreNeeded(uint32& gangsBitFlags);

    static void Update();
    static void UpdateTerritoryUnderControlPercentage();

    // notsa
    static auto GetSpecificZones() { return aSpecificZones | rngv::take(NumSpecificZones); }

private:
    static eBlipColour GetGangColor(int32 gang);
};
