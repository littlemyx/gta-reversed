/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "eStats.h"
#include "eStatModAbilities.h"
#include "eStatsReactions.h"
#include "Text/Text.h"
#include <Enums/eRadioID.h>

enum eStatUpdateState : uint8 {
    STAT_UPDATE_DECREASE = 0,
    STAT_UPDATE_INCREASE = 1
};

enum eStatMessageCondition {
    STATMESSAGE_LESSTHAN = 0,
    STATMESSAGE_MORETHAN = 1
};

struct tStatMessage {
    int16 stat_num; // unique stat id
    bool displayed;
    uint8 condition; // this can be lessthan/morethan, see eStatMessageCondition
    float value;             // value stat must reach to display message
    char text_id[8];         // text id from american.gxt text file to display
};

VALIDATE_SIZE(tStatMessage, 0x10);

class CVehicle;
class CBike;
class CBmx;

class CStats {
public:
    static constexpr size_t FIRST_INT_STAT = 120;
    static constexpr size_t FIRST_UNUSED_STAT = 82;

    static inline NOTSA_GLOBAL(StatMessage, 0xB78200, (std::array<tStatMessage, 128>), {});
    static inline NOTSA_GLOBAL(TotalNumStatMessages, 0xB794D0, (uint32), {});
    static inline NOTSA_GLOBAL(LastMissionPassedName, 0xB78A00, (char[8]), {});
    static inline NOTSA_GLOBAL(TimesMissionAttempted, 0xB78CC8, (std::array<int32, 100>), {});
    static inline NOTSA_GLOBAL(FavoriteRadioStationList, 0xB78E58, (std::array<int32, 14>), {});
    static inline NOTSA_GLOBAL(PedsKilledOfThisType, 0xB78E90, (std::array<int32, 32>), {});
    static inline NOTSA_GLOBAL(StatReactionValue, 0xB78F10, (std::array<float, 59>), {});
    static inline NOTSA_GLOBAL(StatTypesInt, 0xB79000, (std::array<int32, 223>), {});
    static inline NOTSA_GLOBAL(StatTypesFloat, 0xB79380, (std::array<float, 82>), {});
#ifdef NOTSA_GLOBALS_DETACHED
    static inline NOTSA_GLOBAL(m_ThisStatIsABarChart, 0xB794CC, (uint16), {}); // id of the stat currently being built/printed as a stat line (0 = none)
#else
    static uint16& m_ThisStatIsABarChart; // 0xB794CC: id of the stat currently being built/printed as a stat line (0 = none)
#endif
#line 54
    static inline NOTSA_GLOBAL(bStatUpdateMessageDisplayed, 0xB794D4, (bool), {});
    static inline NOTSA_GLOBAL(m_SprintStaminaCounter, 0xB794D8, (uint32), {});
    static inline NOTSA_GLOBAL(m_CycleStaminaCounter, 0xB794DC, (uint32), {});
    static inline NOTSA_GLOBAL(m_CycleSkillCounter, 0xB794E0, (uint32), {});
    static inline NOTSA_GLOBAL(m_SwimStaminaCounter, 0xB794E4, (uint32), {});
    static inline NOTSA_GLOBAL(m_SwimUnderWaterCounter, 0xB794E8, (uint32), {});
    static inline NOTSA_GLOBAL(m_DrivingCounter, 0xB794EC, (uint32), {});
    static inline NOTSA_GLOBAL(m_FlyingCounter, 0xB794F0, (uint32), {});
    static inline NOTSA_GLOBAL(m_BoatCounter, 0xB794F4, (uint32), {});
    static inline NOTSA_GLOBAL(m_BikeCounter, 0xB794F8, (uint32), {});
    static inline NOTSA_GLOBAL(m_FatCounter, 0xB794FC, (uint32), {});
    static inline NOTSA_GLOBAL(m_RunningCounter, 0xB79500, (uint32), {});
    static inline NOTSA_GLOBAL(m_WeaponCounter, 0xB79504, (uint32), {});
    static inline NOTSA_GLOBAL(m_DeathCounter, 0xB79508, (uint32), {});
    static inline NOTSA_GLOBAL(m_MaxHealthCounter, 0xB7950C, (uint32), {});
    static inline NOTSA_GLOBAL(m_AddToHealthCounter, 0xB79510, (uint32), {});
    static inline NOTSA_GLOBAL(m_LastWeaponTypeFired, 0xB79514, (uint32), {});
    static inline NOTSA_GLOBAL(bShowUpdateStats, 0x8CDE56, (bool), { true });

    // NOTSA names (originals unknown)
    static inline NOTSA_GLOBAL(m_FatMuscleMessageState, 0xB79534, (uint8), {});   // 1/2/3: which fat/muscle update message was shown last
    static inline NOTSA_GLOBAL(m_pSexAppealVehicle, 0xB79530, (CVehicle*), {}); // last vehicle used for sex appeal
    static inline NOTSA_GLOBAL(m_RespectThreshold, 0x8CDEC4, (float), { -99.0f });   // -99.0f if not initialised
    static inline NOTSA_GLOBAL(m_RespectLastValue, 0x8CDEC8, (float), { -99.0f });   // -99.0f if not initialised

    static void InjectHooks();

    static char* GetStatID(eStats stat);
    static bool IsStatFloat(eStats stat);
    static float GetStatValue(eStats stat);
    static int8 GetTimesMissionAttempted(uint8 missionId);
    static void RegisterMissionAttempted(uint8 missionId);
    static void RegisterMissionPassed(uint8 missionId);
    static bool PopulateFavoriteRadioStationList();
    static const auto& GetFullFavoriteRadioStationList() { return FavoriteRadioStationList; }
    static eRadioID FindMostFavoriteRadioStation();
    static int32 FindLeastFavoriteRadioStation();
    static int32 FindCriminalRatingNumber();
    static float GetPercentageProgress();
    static void BuildStatLine(char* line, void* pValue1, int32 metrics, void* pValue2, int32 type);
    static int32 ConvertToMins(int32 value);
    static int32 ConvertToSecs(int32 value);
    static bool SafeToShowThisStat(eStats stat);
    static bool CheckForThreshold(float* pValue, float range);
    static bool IsStatCapped(eStats stat);
    static void ProcessReactionStatsOnDecrement(eStats stat);
    static void CheckForStatsMessage();
    static void LoadStatUpdateConditions();
    static void LoadActionReactionStats();
    static int32 FindMaxNumberOfGroupMembers();
    static float GetFatAndMuscleModifier(eStatModAbilities statMod);
    static void DecrementStat(eStats stat, float value);
    static void SetStatValue(eStats stat, float value);
    static void RegisterFastestTime(eStats stat, int32 fastestTime);
    static void RegisterBestPosition(eStats stat, int32 position);
    static const GxtChar* FindCriminalRatingString();
    static int32 ConstructStatLine(int32 arg0, uint8 arg1);
    static void ProcessReactionStatsOnIncrement(eStats stat);
    static void DisplayScriptStatUpdateMessage(eStatUpdateState state, eStats stat, float value);
    static void UpdateRespectStat(uint8 arg0);
    static void UpdateSexAppealStat();
    static void Init();
    static void IncrementStat(eStats stat, float value = 1.f);
    static void SetNewRecordStat(eStats stat, float value);
    static void UpdateFatAndMuscleStats(uint32 value);
    static void UpdateStatsWhenSprinting();
    static void UpdateStatsWhenRunning();
    static void UpdateStatsWhenCycling(bool arg0, CBmx* bmx);
    static void UpdateStatsWhenSwimming(bool arg0, bool arg1);
    static void UpdateStatsWhenDriving(CVehicle* vehicle);
    static void UpdateStatsWhenFlying(CVehicle* vehicle);
    static void UpdateStatsWhenOnMotorBike(CBike* bike);
    static void UpdateStatsWhenWeaponHit(eWeaponType weaponType);
    static void UpdateStatsWhenFighting();
    static void UpdateStatsOnRespawn();
    static void UpdateStatsAddToHealth(uint32 addToHealth);
    static void ModifyStat(eStats stat, float value);
    static bool Save();
    static bool Load();

    // NOTSA
    template<typename T> requires std::is_arithmetic_v<T>
    static T GetStatValue(eStats stat) {
        if constexpr (std::is_integral_v<T>) {
            // NOTE: First func checks if it's not a float stat, allowing unused ones.
            // Second check eliminates them as well.
            assert(!IsStatFloat(stat) && stat >= FIRST_INT_STAT);
        }
        if constexpr (std::is_floating_point_v<T>) {
            assert(IsStatFloat(stat));
        }
        const float r = CStats::GetStatValue(stat);

        // there shouldn't be any stat value bigger than int64::max.
        assert(std::is_floating_point_v<T> || std::in_range<T>(static_cast<int64>(r)));
        return static_cast<T>(r);
    }
};
