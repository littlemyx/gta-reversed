/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"

#include "Stats.h"
#include "MenuSystem.h"
#include "Hud.h"
#include "AudioEngine.h"
#include "Cheat.h"
#include "CutsceneMgr.h"
#include "GameLogic.h"
#include "GangWars.h"
#include "Localisation.h"
#include "TagManager.h"
#include "StuntJumpManager.h"
#include "WeaponInfo.h"
#include "Models/ModelInfo.h"
#include "Models/VehicleModelInfo.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Bmx.h"

namespace {
// Original (modifiable, in .data) constants of `GetFatAndMuscleModifier`
auto& kMod_Fat1000Min2    = StaticRef<float>(0x8CDE58); // 0.5
auto& kMod_F_Muscle       = StaticRef<float>(0x8CDE5C); // -0.25
auto& kMod_F_Fat          = StaticRef<float>(0x8CDE60); // -0.5
auto& kMod_E_Driving      = StaticRef<float>(0x8CDE64); // 0.5
auto& kMod_E_Fat          = StaticRef<float>(0x8CDE68); // 0.5
auto& kMod_D_Bike         = StaticRef<float>(0x8CDE6C); // 0.25
auto& kMod_C_Bike         = StaticRef<float>(0x8CDE70); // 0.3
auto& kMod_B_Bike         = StaticRef<float>(0x8CDE74); // 0.5
auto& kMod_MaxHealth      = StaticRef<float>(0x8CDE78); // 176.0
auto& kMod_Air            = StaticRef<float>(0x8CDE7C); // 3000.0
auto& kMod_TimeCanRun     = StaticRef<float>(0x8CDE80); // 3000.0
auto& kMod_6_Cycling      = StaticRef<float>(0x8CDE84); // 1.0
auto& kMod_6_Fat          = StaticRef<float>(0x8CDE88); // -0.5
auto& kMod_5_Cycling      = StaticRef<float>(0x8CDE8C); // 1.0
auto& kMod_5_Stamina      = StaticRef<float>(0x8CDE90); // 0.5
auto& kMod_5_Muscle       = StaticRef<float>(0x8CDE94); // 0.5
auto& kMod_5_Fat          = StaticRef<float>(0x8CDE98); // -1.0
auto& kMod_4_Muscle       = StaticRef<float>(0x8CDE9C); // 1.0
auto& kMod_4_Fat          = StaticRef<float>(0x8CDEA0); // 0.5
auto& kMod_3_Muscle       = StaticRef<float>(0x8CDEA4); // -0.1
auto& kMod_3_Fat          = StaticRef<float>(0x8CDEA8); // -0.2
auto& kMod_2_Muscle       = StaticRef<float>(0x8CDEAC); // -0.1
auto& kMod_2_Fat          = StaticRef<float>(0x8CDEB0); // -0.2
auto& kMod_1_Muscle       = StaticRef<float>(0x8CDEB4); // 0.2
auto& kMod_MuscleBase     = StaticRef<float>(0x8CDEB8); // 50.0
auto& kMod_1_Fat          = StaticRef<float>(0x8CDEBC); // -0.4
auto& kMod_FatBase        = StaticRef<float>(0x8CDEC0); // 200.0

//! x87: `CTimer::ms_fTimeStep * 0.02f * 1000.0f` kept in extended precision, then truncated by _ftol (0x821B40)
uint32 TimeStepInMS() {
    return (uint32)(int32)((double)CTimer::ms_fTimeStep * (double)0.02f * 1000.0);
}
}

void CStats::InjectHooks() {
    RH_ScopedClass(CStats);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x55C0C0);
    RH_ScopedInstall(PopulateFavoriteRadioStationList, 0x558EC0);
    RH_ScopedInstall(FindMostFavoriteRadioStation, 0x558FA0);
    RH_ScopedInstall(FindLeastFavoriteRadioStation, 0x559010);
    RH_ScopedInstall(GetFatAndMuscleModifier, 0x559AF0);
    RH_ScopedInstall(FindCriminalRatingString, 0x55A210);
    RH_ScopedInstall(ConstructStatLine, 0x55A780);
    RH_ScopedOverloadedInstall(GetStatValue, "-OG", 0x558E40, float(*)(eStats));
    RH_ScopedInstall(SetStatValue, 0x55A070);
    RH_ScopedInstall(IsStatFloat, 0x558E30);
    /*RH_ScopedInstall(GetFullFavoriteRadioStationList, 0x558F90); - different return type*/
    RH_ScopedInstall(FindCriminalRatingNumber, 0x559080);
    RH_ScopedInstall(GetPercentageProgress, 0x5591E0);
    RH_ScopedInstall(ConvertToMins, 0x559540);
    RH_ScopedInstall(ConvertToSecs, 0x559560);
    RH_ScopedInstall(SafeToShowThisStat, 0x559590);
    RH_ScopedInstall(CheckForThreshold, 0x5595F0);
    RH_ScopedInstall(IsStatCapped, 0x559630);
    RH_ScopedInstall(LoadActionReactionStats, 0x5599B0);
    RH_ScopedInstall(FindMaxNumberOfGroupMembers, 0x559A50);
    RH_ScopedInstall(ProcessReactionStatsOnDecrement, 0x559730);
    RH_ScopedInstall(DecrementStat, 0x559FA0);
    RH_ScopedInstall(SetNewRecordStat, 0x55C410);
    RH_ScopedInstall(RegisterFastestTime, 0x55A0B0);
    RH_ScopedInstall(RegisterBestPosition, 0x55A160);
    RH_ScopedInstall(ProcessReactionStatsOnIncrement, 0x55B900);
    RH_ScopedInstall(DisplayScriptStatUpdateMessage, 0x55B980);
    RH_ScopedInstall(IncrementStat, 0x55C180);
    RH_ScopedInstall(UpdateStatsWhenSprinting, 0x55C660);
    RH_ScopedInstall(UpdateStatsWhenRunning, 0x55C6F0);
    RH_ScopedInstall(UpdateStatsWhenOnMotorBike, 0x55CD60);
    RH_ScopedInstall(UpdateStatsWhenFighting, 0x55CFA0);
    RH_ScopedInstall(ModifyStat, 0x55D090);
    RH_ScopedInstall(BuildStatLine, 0x559230);
    RH_ScopedInstall(CheckForStatsMessage, 0x559760);
    RH_ScopedInstall(LoadStatUpdateConditions, 0x559860);
    RH_ScopedInstall(UpdateRespectStat, 0x55BC50);
    RH_ScopedInstall(UpdateSexAppealStat, 0x55BF20);
    RH_ScopedInstall(UpdateFatAndMuscleStats, 0x55C470);
    RH_ScopedInstall(UpdateStatsWhenCycling, 0x55C780);
    RH_ScopedInstall(UpdateStatsWhenSwimming, 0x55C990);
    RH_ScopedInstall(UpdateStatsWhenDriving, 0x55CAC0);
    RH_ScopedInstall(UpdateStatsWhenFlying, 0x55CC00);
    RH_ScopedInstall(UpdateStatsWhenWeaponHit, 0x55CEB0);
    RH_ScopedInstall(UpdateStatsOnRespawn, 0x55CFC0);
    RH_ScopedInstall(UpdateStatsAddToHealth, 0x55D030);

    // unused
    RH_ScopedInstall(GetStatID, 0x558DE0);
    RH_ScopedInstall(GetTimesMissionAttempted, 0x558E70);
    RH_ScopedInstall(RegisterMissionAttempted, 0x558E80);
    RH_ScopedInstall(RegisterMissionPassed, 0x558EA0);

    RH_ScopedInstall(Load, 0x5D3BF0);
    RH_ScopedInstall(Save, 0x5D3B40);
}

// 0x55C0C0
void CStats::Init() {
    std::ranges::fill(StatTypesFloat, 0.0f);
    std::ranges::fill(StatTypesInt, 0);
    std::ranges::fill(PedsKilledOfThisType, 0);
    std::ranges::fill(TimesMissionAttempted, 0);

    bStatUpdateMessageDisplayed = false;
    CTimer::SetTimeInMS(0);
    std::ranges::fill(LastMissionPassedName, 0);
    m_SprintStaminaCounter = 0;
    m_CycleStaminaCounter = 0;
    m_SwimStaminaCounter = 0;
    m_DrivingCounter = 0;
    m_FlyingCounter = 0;
    m_BoatCounter = 0;
    m_BikeCounter = 0;
    m_FatCounter = 0;
    m_RunningCounter = 0;

    StatTypesFloat[STAT_MAX_HEALTH] = 569.0f;
    CheckForStatsMessage();
    StatTypesFloat[STAT_STAMINA] = 100.0f;
    CheckForStatsMessage();

    PopulateFavoriteRadioStationList();
    LoadActionReactionStats();
    LoadStatUpdateConditions();
}

// 0x558E40
float CStats::GetStatValue(eStats stat) {
    if (!IsStatFloat(stat)) { // int32
        assert(stat >= FIRST_INT_STAT);

        return static_cast<float>(StatTypesInt[stat - FIRST_INT_STAT]);
    }

    return StatTypesFloat[stat];
}

// 0x55A070
void CStats::SetStatValue(eStats stat, float value) {
    if (IsStatFloat(stat)) {
        StatTypesFloat[stat] = value;
    } else { // int32
        assert(stat >= FIRST_INT_STAT);

        StatTypesInt[stat - FIRST_INT_STAT] = static_cast<int32>(value);
    }
    CheckForStatsMessage();
}

// 0x558E30
bool CStats::IsStatFloat(eStats stat) {
    return stat < FIRST_UNUSED_STAT;
}

// 0x558EC0
bool CStats::PopulateFavoriteRadioStationList() {
    const auto* listenTimes = AudioEngine.GetRadioStationListenTimes(); // 0x507020
    bool allZero = true;
    for (auto i = 0u; i < FavoriteRadioStationList.size(); i++) {
        FavoriteRadioStationList[i] = listenTimes[i];
        if (listenTimes[i] != 0) {
            allZero = false;
        }
    }
    return allZero;
}

// 0x558FA0
eRadioID CStats::FindMostFavoriteRadioStation() {
    // NOTE: The original only looks at the stations 1..12 (the first and the last two entries are skipped)
    auto best  = 1;
    auto bestN = 0;
    for (auto i = 1; i <= 12; i++) {
        if (FavoriteRadioStationList[i] > bestN) {
            bestN = FavoriteRadioStationList[i];
            best  = i;
        }
    }
    return (eRadioID)best;
}

// 0x559010
int32 CStats::FindLeastFavoriteRadioStation() {
    // NOTE: Same range as in FindMostFavoriteRadioStation (1..12)
    auto best  = 1;
    auto bestN = FavoriteRadioStationList[1];
    for (auto i = 1; i <= 12; i++) {
        if (FavoriteRadioStationList[i] < bestN) {
            bestN = FavoriteRadioStationList[i];
            best  = i;
        }
    }
    return best;
}

// 0x559080
int32 CStats::FindCriminalRatingNumber() {
    CPlayerInfo* playerInfo = FindPlayerPed()->GetPlayerInfoForThisPlayerPed();

    auto value = (int32)(
        GetStatValue(STAT_TOTAL_LEGITIMATE_KILLS)
        - (GetStatValue(STAT_TIMES_BUSTED) - GetStatValue(STAT_NUMBER_OF_HOSPITAL_VISITS)) * 3.0f
        + (GetStatValue(STAT_HIGHEST_FIREFIGHTER_MISSION_LEVEL) + GetStatValue(STAT_HIGHEST_PARAMEDIC_MISSION_LEVEL)) * 10.0f
        + int32((float)playerInfo->m_nMoney / 5000.0f)
        + GetStatValue(STAT_PLANES_HELICOPTERS_DESTROYED) * 30.0f
        + GetStatValue(STAT_TOTAL_FIRES_EXTINGUISHED)
        + GetStatValue(STAT_CRIMINALS_KILLED_ON_VIGILANTE_MISSION)
        + GetStatValue(STAT_PEOPLE_SAVED_IN_AN_AMBULANCE)
    );

    if (CCheat::m_bHasPlayerCheated || GetStatValue(STAT_TIMES_CHEATED) > 0.0f) {
        value -= 10 * (int32)GetStatValue(STAT_TIMES_CHEATED);

        value = std::max(value, -10000);
    } else {
        value = std::max(value, 0);
    }

    float bulletsFired = GetStatValue(STAT_BULLETS_FIRED);

    if (bulletsFired >= 100.0f) {
        value += (int32)(500 * (GetStatValue(STAT_BULLETS_THAT_HIT) / bulletsFired));
    }

    return value + (int32)(10 * GetPercentageProgress());
}

// 0x5591E0
float CStats::GetPercentageProgress() {
    return std::min(StatTypesFloat[STAT_PROGRESS_MADE] / 187.0f * 100.0f, 100.0f);
}

// 0x559230
void CStats::BuildStatLine(char* line, void* pValue1, int32 metrics, void* pValue2, int32 type) {
    if (!line) {
        return;
    }

    gString2[0] = '\0';

    // NOTSA: original uses unbounded sprintf, we bound it to the buffer size
    const auto Print = [](const char* fmt, auto... args) {
        snprintf(gString2, sizeof(gString2), fmt, args...);
    };
    const auto Txt = [](const char* key) { // 0x6A0050 + 0x69F7E0
        return GxtCharToUTF8(TheText.Get(key), 0u);
    };

    if (type == 1) { // minutes:seconds
        // NOTE: original dereferences both pointers unconditionally
        const int32 a = *(int32*)pValue1;
        const int32 b = *(int32*)pValue2;
        Print(b >= 10 ? "%d:%d" : "%d:0%d", a, b);
    } else if (pValue2) {
        switch (metrics) {
        case 0:
            Print(" %d %s %d", *(int32*)pValue1, Txt("FEST_OO"), *(int32*)pValue2);
            break;
        case 1:
            Print("%.2f %s %.2f", (double)*(float*)pValue1, Txt("FEST_OO"), (double)*(float*)pValue2);
            break;
        case 3:
            Print("$%.2f %s $%.2f", (double)*(float*)pValue1, Txt("FEST_OO"), (double)*(float*)pValue2);
            break;
        }
    } else if (pValue1) {
        switch (metrics) {
        case 0:
            Print("%d", *(int32*)pValue1);
            break;
        case 1:
            Print("%.2f", (double)*(float*)pValue1);
            break;
        case 2:
            Print("%0.2f%%", (double)*(float*)pValue1);
            break;
        case 3:
            Print("$%.2f", (double)*(float*)pValue1);
            break;
        case 4:
            Print("%d|", *(int32*)pValue1);
            break;
        case 5: {
            const int32 pounds = *(int32*)pValue1;
            if (CLocalisation::Metric()) {
                const double kgs = (double)pounds * (double)0.4536f; // x87: FILD * 0.4536f, pushed as double
                if constexpr (notsa::IsFixBugs()) {
                    Print("%dkgs", (int32)kgs);
                } else {
                    Print("%dkgs", kgs); // BUG: the original passes a double to "%d"
                }
            } else {
                Print("%dlbs", pounds);
            }
            break;
        }
        case 6: // miles
            Print("%.2f %s", (double)*(float*)pValue1, Txt("ST_MILE"));
            break;
        case 7:
            Print("%.2fm", (double)*(float*)pValue1);
            break;
        case 8: // feet
            Print("%.2fft", (double)*(float*)pValue1 * (double)3.3333333f);
            break;
        case 9: // seconds
            Print("%d %s", *(int32*)pValue1, Txt("ST_SECS"));
            break;
        }
    }

    GxtCharStrcpy(gGxtString, TheText.Get(line)); // 0x718660
    CFont::FilterOutTokensFromString(gGxtString); // 0x719240
    AsciiToGxtChar(gString2, gGxtString2);
}

// 0x559540
int32 CStats::ConvertToMins(int32 statValue) {
    if (statValue > 59)
        return (statValue - 60) / 60 + 1;
    return 0;
}

// 0x559560
int32 CStats::ConvertToSecs(int32 statValue) {
    int32 seconds = statValue;
    if (statValue > 59)
        seconds = -60 - 60 * ((statValue - 60) / 60) + statValue;
    if (seconds < 0)
        seconds = -seconds;
    return seconds;
}

// 0x559590
bool CStats::SafeToShowThisStat(eStats stat) {
    if (!CLocalisation::GermanGame()) {
        return true;
    }

    switch (stat) {
    case STAT_RAMPAGES_ATTEMPTED:
    case STAT_RAMPAGES_PASSED:
    case STAT_TOTAL_LEGITIMATE_KILLS:
    case STAT_HIGHEST_CIVILIAN_PEDS_KILLED_ON_RAMPAGE:
    case STAT_HIGHEST_POLICE_PEDS_KILLED_ON_RAMPAGE:
    case STAT_HIGHEST_CIVILIAN_VEHICLES_DESTROYED_ON_RAMPAGE:
    case STAT_HIGHEST_POLICE_VEHICLES_DESTROYED_ON_RAMPAGE:
    case STAT_HIGHEST_NUMBER_OF_TANKS_DESTROYED_ON_RAMPAGE:
        return false;
    default:
        return true;
    }
}

// 0x5595F0
bool CStats::CheckForThreshold(float* pValue, float range) {
    if (*pValue + 40.0f >= range && *pValue - 40.0f <= range) {
        return false;
    }
    *pValue = range;
    return true;
}

// 0x559630
bool CStats::IsStatCapped(eStats stat) {
    switch (stat) {
    case STAT_GIRLFRIEND_RESPECT:
    case STAT_CLOTHES_RESPECT:
    case STAT_FITNESS_RESPECT:
    case STAT_FAT:
    case STAT_STAMINA:
    case STAT_MUSCLE:
    case STAT_MAX_HEALTH:
    case STAT_SEX_APPEAL:
    case STAT_PISTOL_SKILL:
    case STAT_SILENCED_PISTOL_SKILL:
    case STAT_DESERT_EAGLE_SKILL:
    case STAT_SHOTGUN_SKILL:
    case STAT_SAWN_OFF_SHOTGUN_SKILL:
    case STAT_COMBAT_SHOTGUN_SKILL:
    case STAT_MACHINE_PISTOL_SKILL:
    case STAT_SMG_SKILL:
    case STAT_AK_47_SKILL:
    case STAT_M4_SKILL:
    case STAT_RIFLE_SKILL:
    case STAT_APPEARANCE:
    case STAT_ARMOR:
    case STAT_ENERGY:
    case STAT_DRIVING_SKILL:
    case STAT_FLYING_SKILL:
    case STAT_LUNG_CAPACITY:
    case STAT_BIKE_SKILL:
    case STAT_LUCK:
    case STAT_HORSESHOES_COLLECTED:
    case STAT_TOTAL_HORSESHOES:
    case STAT_OYSTERS_COLLECTED:
    case STAT_TOTAL_OYSTERS:
    case STAT_CYCLING_SKILL:
        return true;
    }
    return false;
}

// 0x559760
void CStats::CheckForStatsMessage() {
    if (CPad::GetPad(0)->JustOutOfFrontEnd
        || !bShowUpdateStats
        || TheCamera.m_bWideScreenOn
        || CCutsceneMgr::ms_cutsceneProcessing
        || CHud::HelpMessageDisplayed()
        || CMenuSystem::num_menus_in_use)
    {
        return;
    }

    for (uint32 i = 0; i < StatMessage.size(); i++) {
        if (i >= TotalNumStatMessages) {
            return;
        }
        auto& msg = StatMessage[i];
        if (msg.displayed) {
            continue;
        }
        const double stat = GetStatValue((eStats)(uint16)msg.stat_num);
        const bool   show = msg.condition
            ? stat >= (double)msg.value  // morethan
            : stat <= (double)msg.value; // lessthan
        if (show) {
            msg.displayed = true;
            CHud::SetHelpMessage(TheText.Get(msg.text_id), false, false, false);
            bStatUpdateMessageDisplayed = true;
        }
    }
}

// 0x559860
void CStats::LoadStatUpdateConditions() {
    int32 id{};
    float value{};
    char  name[84]{};
    char  condition[12]{};
    char  textId[8]{};

    CFileMgr::SetDir("");
    auto* file = CFileMgr::OpenFile("DATA\\STATDISP.DAT", "rb");

    TotalNumStatMessages = 0;
    uint32 numMessages = 0;

    for (char* line = CFileLoader::LoadLine(file); line != nullptr; line = CFileLoader::LoadLine(file)) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        // NOTSA: sscanf_s instead of the original unbounded sscanf
        sscanf_s(line, "%d %s %s %f %s", &id, name, (unsigned)sizeof(name), condition, (unsigned)sizeof(condition), &value, textId, (unsigned)sizeof(textId));

        auto& msg = StatMessage[numMessages];
        msg.stat_num  = (int16)id;
        msg.displayed = false;
        if (strcmp(condition, "lessthan") == 0) {
            msg.condition = STATMESSAGE_LESSTHAN;
        } else if (strcmp(condition, "morethan") == 0) {
            msg.condition = STATMESSAGE_MORETHAN;
        }
        msg.value = value;
        strcpy_s(msg.text_id, textId);

        numMessages++;
    }

    TotalNumStatMessages = numMessages;
    CFileMgr::CloseFile(file);
}

// 0x5599B0
void CStats::LoadActionReactionStats() {
    CFileMgr::SetDir("");

    auto* file = CFileMgr::OpenFile("DATA\\AR_STATS.DAT", "rb");

    for (char* line = CFileLoader::LoadLine(file); line != nullptr; line = CFileLoader::LoadLine(file)) {
        int32 reactId;
        float reactValue;

        if (line[0] != '#' && line[0] != NULL) {
            VERIFY(sscanf_s(line, "%d %*s %f", &reactId, &reactValue) == 2);

            StatReactionValue[reactId] = reactValue;
        }
    }

    CFileMgr::CloseFile(file);
}

// 0x559A50
int32 CStats::FindMaxNumberOfGroupMembers() {
    float respect = StatTypesFloat[STAT_TOTAL_RESPECT];

    if (respect < 10.0f)
        return 0;
    if (respect < 60.0f)
        return 2;
    if (respect < 160.0f)
        return 3;
    if (respect < 330.0f)
        return 4;
    if (respect < 540.0f)
        return 5;
    if (respect < 800.0f)
        return 6;

    return 7;
}

// 0x559AF0
float CStats::GetFatAndMuscleModifier(eStatModAbilities statMod) {
    // x87: the original keeps every intermediate in extended precision, so doubles are used here
    const double fat      = StatTypesFloat[STAT_FAT];
    const double stamina  = StatTypesFloat[STAT_STAMINA];
    const double muscle   = StatTypesFloat[STAT_MUSCLE];
    const double cycling  = StatTypesInt[STAT_CYCLING_SKILL - FIRST_INT_STAT];

    const double fatBase = kMod_FatBase, muscleBase = kMod_MuscleBase;
    const double fatDen    = 1000.0 - fatBase;
    const double muscleDen = 1000.0 - muscleBase;

    const auto Max0 = [](float b) { return 0.0f > b ? 0.0f : b; };    // 0x420800(0, b): NaN => b
    const auto Min1 = [](float b) { return 1.0f < b ? 1.0f : b; };    // 0x404330(1, b): NaN => b
    const auto BikeTerm = [&]() {
        return Min1((float)((double)StatTypesInt[STAT_BIKE_SKILL - FIRST_INT_STAT] * (double)0.001f));
    };

    switch (statMod) {
    case STAT_MOD_0: {
        const float f = StatTypesFloat[STAT_FAT];
        if (f > 800.0f) {
            return 2.0f;
        }
        if (f > 400.0f) {
            return 1.0f;
        }
        return 0.0f;
    }
    case STAT_MOD_1: {
        const float r = Max0((float)((fat - fatBase) / fatDen));
        const double res = r * (double)kMod_1_Fat + 1.0 + (muscle - muscleBase) * kMod_1_Muscle / muscleDen;
        if (res < 0.7f) {
            return 0.7f;
        }
        return (float)res;
    }
    case STAT_MOD_2: {
        const float r = Max0((float)((fat - fatBase) / fatDen));
        const double res = r * (double)kMod_2_Fat + 1.0 + (muscle - muscleBase) * kMod_2_Muscle / muscleDen;
        if (res < 0.8f) {
            return 0.8f;
        }
        return (float)res;
    }
    case STAT_MOD_3: {
        const double res = (fat - fatBase) * kMod_3_Fat / fatDen + 1.0 + (muscle - muscleBase) * kMod_3_Muscle / muscleDen;
        if (res < 0.8f) {
            return 0.8f;
        }
        return (float)res;
    }
    case STAT_MOD_4: {
        const double res = (fat - fatBase) * kMod_4_Fat / fatDen + 1.0 + (muscle - muscleBase) * kMod_4_Muscle / muscleDen;
        if (!(res > 2.0)) {
            return (float)res;
        }
        return 2.0f;
    }
    case STAT_MOD_5: {
        const double res =
              (fat - fatBase) * kMod_5_Fat / fatDen + 1.0 + (muscle - muscleBase) * kMod_5_Muscle / muscleDen
            + stamina * (double)0.001f * kMod_5_Stamina
            + cycling * (double)0.001f * kMod_5_Cycling;
        if (res > 2.0) {
            return 2.0f;
        }
        if (res < 0.25f) {
            return 0.25f;
        }
        return (float)res;
    }
    case STAT_MOD_6: {
        const double res =
              (fat - fatBase) * kMod_6_Fat / fatDen + 1.0
            + cycling * (double)0.001f * kMod_6_Cycling;
        if (res > 2.0) {
            return 2.0f;
        }
        if (res < 0.5f) {
            return 0.5f;
        }
        return (float)res;
    }
    case STAT_MOD_TIME_CAN_RUN:
        return (float)(stamina * (double)0.001f * kMod_TimeCanRun + 150.0);
    case STAT_MOD_AIR_IN_LUNG: {
        const double lung = StatTypesInt[STAT_LUNG_CAPACITY - FIRST_INT_STAT];
        return (float)((lung + stamina) * (double)0.0005f * kMod_Air + 1000.0);
    }
    case STAT_MOD_MAX_HEALTH:
        return (float)((double)StatTypesFloat[STAT_MAX_HEALTH] * (double)0.001f * kMod_MaxHealth);
    case STAT_MOD_10:
        return kMod_MaxHealth;
    case STAT_MOD_11:
        return (float)(BikeTerm() * (double)kMod_B_Bike + 1.0);
    case STAT_MOD_12:
        return (float)(BikeTerm() * (double)kMod_C_Bike + 1.0);
    case STAT_MOD_13:
        return (float)(BikeTerm() * (double)kMod_D_Bike + 1.0);
    case STAT_MOD_DRIVING_SKILL: {
        const double driving = StatTypesInt[STAT_DRIVING_SKILL - FIRST_INT_STAT];
        const double res = (fat - fatBase) * kMod_E_Fat / fatDen + (driving * (double)0.001f * kMod_E_Driving + 1.0);
        if (res > 1.0) {
            return 1.0f;
        }
        return (float)res;
    }
    case STAT_MOD_15: {
        const double res = (fat - fatBase) * kMod_F_Fat / fatDen + 1.0 + (muscle - muscleBase) * kMod_F_Muscle / muscleDen;
        if (res < kMod_Fat1000Min2) {
            return kMod_Fat1000Min2;
        }
        return (float)res;
    }
    default:
        return 1.0f;
    }
}

// 0x559730
void CStats::ProcessReactionStatsOnDecrement(eStats stat) {
    if (stat == STAT_ENERGY && GetStatValue(STAT_ENERGY) < 0.0f)
        DecrementStat(STAT_FAT, 23.0f);
}

// 0x559FA0
void CStats::DecrementStat(eStats stat, float value) {
    if (value <= 0.0f)
        return;

    float oldValue = GetStatValue(stat);

    SetStatValue(stat, std::max(oldValue - value, 0.0f));

    ProcessReactionStatsOnDecrement(stat);
    CheckForStatsMessage();
}

// 0x55C410
void CStats::SetNewRecordStat(eStats stat, float value) {
    float currentValue = GetStatValue(stat);

    if (currentValue < value)
        SetStatValue(stat, value);

    CheckForStatsMessage();
}

// 0x55A0B0
void CStats::RegisterFastestTime(eStats stat, int32 fastestTime) {
    SetNewRecordStat(stat, (float)fastestTime);
}

// 0x55A160
void CStats::RegisterBestPosition(eStats stat, int32 position) {
    SetNewRecordStat(stat, (float)position);
}

// 0x55A210
const GxtChar* CStats::FindCriminalRatingString() {
    struct Tier { int32 below; const char* key; };
    const auto Find = [](int32 rating, std::initializer_list<Tier> tiers) -> const char* {
        for (const auto& t : tiers) {
            if (rating < t.below) {
                return t.key;
            }
        }
        return nullptr;
    };

    const int32 rating = FindCriminalRatingNumber(); // 0x559080

    if (rating < 0) {
        if (rating > -500) {
            return TheText.Get("RATNG53");
        }
        if (rating > -2000) {
            return TheText.Get("RATNG54");
        }
        if (rating > -4000) {
            return TheText.Get("RATNG55");
        }
        if (rating > -6000) {
            return TheText.Get("RATNG56");
        }
        return TheText.Get("RATNG57");
    }

    if (const auto* key = Find(rating, {
        {20, "RATNG1"},
        {50, "RATNG2"},
        {75, "RATNG3"},
        {100, "RATNG4"},
        {120, "RATNG5"},
        {150, "RATNG6"},
        {200, "RATNG7"},
        {240, "RATNG8"},
        {270, "RATNG9"},
        {300, "RATNG10"},
        {335, "RATNG11"},
        {370, "RATNG12"},
        {400, "RATNG13"},
        {450, "RATNG14"},
        {500, "RATNG15"},
        {550, "RATNG16"},
        {600, "RATNG17"},
        {610, "RATNG18"},
        {650, "RATNG19"},
        {700, "RATNG20"},
        {850, "RATNG21"},
        {1000, "RATNG22"},
        {1005, "RATNG23"},
        {1150, "RATNG24"}
    })) {
        return TheText.Get(key);
    }

    if (rating < 1300) { // Special case, TIMES_BUSTED
        return TheText.Get((float)StatTypesInt[STAT_TIMES_BUSTED - FIRST_INT_STAT] > 0.0f ? "RATNG25" : "RATNG24");
    }

    if (const auto* key = Find(rating, {
        {1500, "RATNG26"},
        {1700, "RATNG27"},
        {2000, "RATNG28"},
        {2100, "RATNG29"},
        {2300, "RATNG30"},
        {2500, "RATNG31"},
        {2750, "RATNG32"},
        {3000, "RATNG33"},
        {3500, "RATNG34"},
        {4000, "RATNG35"},
        {5000, "RATNG36"},
        {7500, "RATNG37"},
        {10000, "RATNG38"},
        {20000, "RATNG39"},
        {30000, "RATNG40"},
        {40000, "RATNG41"},
        {50000, "RATNG42"},
        {65000, "RATNG43"},
        {80000, "RATNG44"},
        {100000, "RATNG45"},
        {150000, "RATNG46"},
        {200000, "RATNG47"},
        {300000, "RATNG48"},
        {375000, "RATNG49"}
    })) {
        return TheText.Get(key);
    }

    if (rating < 500000) { // Special case, FLIGHT_TIME; x87: kept in extended precision, then truncated by _ftol
        const auto flightTime = (int32)((double)StatTypesInt[STAT_FLIGHT_TIME - FIRST_INT_STAT] * (double)(1.6666667e-05f) * (double)0.016666668f);
        return TheText.Get(flightTime > 10 ? "RATNG50" : "RATNG49");
    }

    static_assert(offsetof(CPlayerInfo, m_nDisplayMoney) == 0xBC);
    if (rating >= 1000000 && CWorld::Players[CWorld::PlayerInFocus].m_nDisplayMoney > 10000000 /* NOTE: not m_nMoney */) {
        return TheText.Get("RATNG52");
    }
    return TheText.Get("RATNG51");
}

// 0x55A780
int32 CStats::ConstructStatLine(int32 arg0, uint8 arg1) {
    // One line of the per-page stat tables in .data (0x8CD8A0 etc.)
    struct tStatLine {
        int16 statId; // -99 terminates the table
        uint8 type;   // Value format (see below)
        uint8 alwaysShow; // Show even if the stat is 0
        uint8 special;    // Stat is processed by the "special" (non-generic) code
        uint8 pad;
    };
    VALIDATE_SIZE(tStatLine, 6);

    const tStatLine* const table = [&]() -> const tStatLine* {
        switch (arg1) {
        case 0:  return &StaticRef<tStatLine>(0x8CD8A0);
        case 1:  return &StaticRef<tStatLine>(0x8CD938);
        case 2:  return &StaticRef<tStatLine>(0x8CD9B0);
        case 3:  return &StaticRef<tStatLine>(0x8CD9D8);
        case 4:  return &StaticRef<tStatLine>(0x8CDA70);
        case 5:  return &StaticRef<tStatLine>(0x8CDAC8);
        case 6:  return &StaticRef<tStatLine>(0x8CDBF8);
        default: return &StaticRef<tStatLine>(0x8CDE08);
        }
    }();

    static auto& s_CurrentStatId = StaticRef<uint16>(0xB794CC);
    static auto& s_GangValue    = StaticRef<int32[0x200]>(0x96A600); // Indexed by the stat id
    static auto& s_GangIcon    = StaticRef<int32[0x200]>(0x96A60C); // Indexed by the stat id

    int32 line = 0; // Number of lines produced so far (ESI)

    // The stat's int storage, but without the bounds the callers ensure (0xB78E20 + id * 4)
    const auto RawInt = [](uint32 id) { return (StatTypesInt.data() - FIRST_INT_STAT)[id]; };
    const auto IntStat = [&](uint32 id) { return RawInt(id); };

    enum class R { Skip, Count, Done }; // Skip: no line, Count: one more line, Done: the requested line is built

    // 0x55A8D4: Label from the key in `gString`, value from `gString2`
    const auto Tail = []() {
        GxtCharStrcpy(gGxtString, TheText.Get(gString)); // 0x718660
        CFont::FilterOutTokensFromString(gGxtString);    // 0x719240
        AsciiToGxtChar(gString2, gGxtString2);
        return R::Done;
    };
    // 0x55B60D: Label only
    const auto TailNoValue = [&]() {
        gString2[0] = '\0';
        return Tail();
    };
    // NOTSA: The original uses unbounded sprintf
    const auto Print = [](char (&buf)[352], const char* fmt, auto... args) {
        snprintf(buf, sizeof(buf), fmt, args...);
    };
    const auto PrintValue = [&](const char* fmt, auto... args) {
        gString2[0] = '\0';
        Print(gString2, fmt, args...);
        return Tail();
    };
    // 0x55B68F: Value text only, no label
    const auto TextOnly = [](const char* key) {
        gGxtString[0] = '\0';
        GxtCharStrcpy(gGxtString2, TheText.Get(key));
        return R::Done;
    };
    const auto Txt = [](const char* key) { // 0x6A0050 + 0x69F7E0
        return GxtCharToUTF8(TheText.Get(key), 0u);
    };
    const auto PrintPair = [&](int32 a, int32 b) { // 0x55AC1F
        gString2[0] = '\0';
        Print(gString2, " %d %s %d", a, Txt("FEST_OO"), b);
        return Tail();
    };
    // x87: `_ftol(FILD(value) * 1/60000)`, split into minutes and seconds with a signed division by 60
    const auto TimeStat = [&](uint32 id, int32& minutes, int32& seconds) {
        const int32 total = (int32)((double)IntStat(id) * (double)1.6666667e-05f);
        minutes = total / 60;
        seconds = total % 60;
    };

    const auto Step = [&](const tStatLine& e) -> R {
        const uint16 id16 = (uint16)e.statId;
        const int32  id   = (int32)e.statId;

        if (!e.alwaysShow) {
            const float value = id16 < 0x52 ? StatTypesFloat[id16] : (float)RawInt(id16);
            if (!(value > 0.0f)) {
                return R::Skip;
            }
        }

        Print(gString, id < 10 ? "STAT00%d" : id < 100 ? "STAT0%d" : "STAT%d", id);

        if (CLocalisation::GermanGame()) { // 0x56D200
            switch ((uint8)id16) {
            case 0xA7: case 0xA8: case 0xB1:
            case 0xCD: case 0xCE: case 0xCF: case 0xD0: case 0xD1:
                return R::Skip;
            }
        }

        s_CurrentStatId = 0;

        if (!e.special) {
            switch (e.type) {
            case 10: // 0x55A8A0
                s_CurrentStatId = (uint16)id;
                if (line != arg0) {
                    return R::Count;
                }
                return PrintValue("%d", (int32)GetStatValue((eStats)id));
            case 1: // 0x55A9EF
                if (line != arg0) {
                    return R::Count;
                }
                return PrintValue("%.2f", (double)GetStatValue((eStats)id));
            case 3: // 0x55A95E
                if (line != arg0) {
                    return R::Count;
                }
                return PrintValue("$%.2f", (double)GetStatValue((eStats)id));
            case 4: // 0x55A97A
                if (line != arg0) {
                    return R::Count;
                }
                return PrintValue("%d|", (int32)GetStatValue((eStats)id));
            case 5: { // 0x55A914
                if (line != arg0) {
                    return R::Count;
                }
                const int32 pounds = (int32)GetStatValue((eStats)id);
                gString2[0] = '\0';
                if (CLocalisation::Metric()) { // 0x56D220
                    const double kgs = (double)pounds * (double)0.4536f; // x87: FILD * 0.4536f, pushed as a double
                    if constexpr (notsa::IsFixBugs()) {
                        Print(gString2, "%dkgs", (int32)kgs);
                    } else {
                        Print(gString2, "%dkgs", kgs); // BUG: the original passes a double to "%d"
                    }
                } else {
                    Print(gString2, "%dlbs", pounds);
                }
                return Tail();
            }
            case 6: { // 0x55AA0B, miles
                if (line != arg0) {
                    return R::Count;
                }
                const float miles = GetStatValue((eStats)id);
                gString2[0] = '\0';
                const auto* unit = Txt("ST_MILE");
                Print(gString2, "%.2f %s", (double)miles, unit);
                return Tail();
            }
            case 7: // 0x55AA63, metres or feet
                if (CLocalisation::Metric()) {
                    if (line != arg0) {
                        return R::Count;
                    }
                    return PrintValue("%.2fm", (double)GetStatValue((eStats)id));
                }
                if (line != arg0) {
                    return R::Count;
                }
                return PrintValue("%.2fft", (double)GetStatValue((eStats)id) * (double)3.3333333f); // x87: kept in extended precision
            case 9: { // 0x55A998, minutes:seconds
                if (line != arg0) {
                    return R::Count;
                }
                int32 minutes = ConvertToMins((int32)GetStatValue((eStats)id));
                int32 seconds = ConvertToSecs((int32)GetStatValue((eStats)id));
                BuildStatLine(gString, &minutes, 0, &seconds, 1);
                return R::Done;
            }
            default: // 0x55AAAA, plain integer (types 0, 2, 8 and everything above 10)
                if (line != arg0) {
                    return R::Count;
                }
                return PrintValue("%d", id16 < 0x52 ? (int32)StatTypesFloat[id16] : RawInt(id16));
            }
        }

        // Special stats (0x55AAD0)
        if (id > 0x101) {
            const uint32 idx = (uint32)(id - 0x140);
            if (idx > 0x11) {
                return R::Skip;
            }
            switch (id) {
            case 0x140: { // 0x55AE7E, time played (uses the game timer)
                const int32 totalMinutes = CTimer::m_snTimeInMilliseconds / 60000u;
                const int32 minutes      = totalMinutes / 60;
                const int32 seconds      = totalMinutes % 60;
                if (line != arg0) {
                    return R::Count;
                }
                int32 a = minutes, b = seconds;
                BuildStatLine(gString, &a, 0, &b, 1);
                return R::Done;
            }
            case 0x142: // 0x55AE66, tags sprayed
                if (line != arg0) {
                    return R::Count;
                }
                return PrintPair(CTagManager::ms_numTagged, CTagManager::ms_numTags);
            case 0x143: { // 0x55AEFB, gang whose members the player killed the most
                int32  best  = 0;
                uint32 bestN = 0;
                for (int32 i = 7; i < 15; i++) {
                    if ((uint32)PedsKilledOfThisType[i] > bestN) {
                        best  = i;
                        bestN = PedsKilledOfThisType[i];
                    }
                }
                if (best == 0) {
                    return R::Skip;
                }
                if (line == arg0) {
                    return TailNoValue();
                }
                line++;
                if ((uint32)(best - 7) > 7) {
                    return R::Skip;
                }
                if (line != arg0) {
                    return R::Count;
                }
                static constexpr const char* keys[] = {"ST_GNG0", "ST_GNG1", "ST_GNG2", "ST_GNG3", "ST_GNG4", "ST_GNG5", "ST_GNG6", "ST_GNG7"};
                return TextOnly(keys[best - 7]);
            }
            case 0x144: { // 0x55B005, total of the gang kills
                if (line != arg0) {
                    return R::Count;
                }
                int32 sum = 0;
                for (int32 i = 7; i <= 15; i++) {
                    sum += PedsKilledOfThisType[i];
                }
                return PrintValue("%d", sum);
            }
            case 0x145: // 0x55B07A
                if (line != arg0) {
                    return R::Count;
                }
                return PrintValue("%d", PedsKilledOfThisType[20]);
            case 0x146: // 0x55B344, most favourite radio station
            case 0x147: { // 0x55B388, least favourite radio station
                if (PopulateFavoriteRadioStationList()) {
                    return R::Skip;
                }
                if (line == arg0) {
                    return TailNoValue();
                }
                line++;
                char key[8];
                AudioEngine.GetRadioStationNameKey(id == 0x146 ? FindMostFavoriteRadioStation() : (eRadioID)FindLeastFavoriteRadioStation(), key);
                if (line != arg0) {
                    return R::Count;
                }
                return TextOnly(key);
            }
            case 0x148: { // 0x55B3CC, the weapon the player holds
                auto* ped = FindPlayerPed(-1);
                uint16 weaponType = (uint16)ped->m_aWeapons[(int8)ped->m_nActiveWeaponSlot].m_Type;
                if (weaponType == 0x20) {
                    weaponType = 0x1C;
                } else if (weaponType < 0x16 || weaponType > 0x20) {
                    return R::Skip;
                }
                uint8 weaponStatIdx = (uint8)(weaponType - 0x15);
                if (line == arg0) {
                    return PrintValue("%d", (int32)GetStatValue((eStats)(weaponStatIdx + 0x44)));
                }
                line++;
                ped = FindPlayerPed(-1);
                const char* fmt;
                if ((uint32)ped->m_aWeapons[(int8)ped->m_nActiveWeaponSlot].m_Type == 0x20) {
                    weaponStatIdx = 0xB;
                    fmt           = "STWE0%d";
                } else if (weaponStatIdx < 10) {
                    fmt = "STWE00%d";
                } else if (weaponStatIdx < 100) {
                    fmt = "STWE0%d";
                } else {
                    fmt = "STWE%d";
                }
                Print(gString, fmt, (int32)weaponStatIdx);
                if (line != arg0) {
                    return R::Count;
                }
                return TextOnly(gString);
            }
            case 0x149: { // 0x55B492, skills of all the weapons
                if (line == arg0) {
                    return TailNoValue();
                }
                line++;
                for (uint16 w = 1; w < 0xB; w++) {
                    Print(gString, w < 10 ? "STWE00%d" : w < 100 ? "STWE0%d" : "STWE%d", (int32)w);
                    const auto skill = (int8)FindPlayerPed(-1)->GetWeaponSkill((eWeaponType)(w + 0x15)); // 0x5E3B60
                    Print(gString2, skill == 1 ? "WS_STD" : skill == 2 ? "WS_PRO" : "WS_POOR");
                    if (line == arg0) {
                        GxtCharStrcpy(gGxtString, TheText.Get(gString));
                        GxtCharStrcpy(gGxtString2, TheText.Get(gString2));
                        return R::Done;
                    }
                    line++;
                }
                return R::Skip;
            }
            case 0x14A: { // 0x55B0A6, flight time rank
                int32 minutes, seconds;
                TimeStat(169, minutes, seconds);
                if (minutes > 0 || seconds >= 5) {
                    if (line == arg0) {
                        return TailNoValue();
                    }
                    line++;
                }

                const auto Emit = [&](const char* key) {
                    if (line != arg0) {
                        return R::Count;
                    }
                    return TextOnly(key);
                };
                if (minutes <= 0) {
                    if (seconds < 5)  { return R::Skip; }
                    if (seconds < 10) { return Emit("ST_PR01"); }
                    if (seconds < 20) { return Emit("ST_PR02"); }
                    if (seconds < 30) { return Emit("ST_PR03"); }
                }
                if (minutes <= 1) {
                    if (seconds < 0)  { return Emit("ST_PR04"); }
                    if (seconds < 30) { return Emit("ST_PR05"); }
                }
                if (minutes <= 2) {
                    if (seconds < 0)  { return Emit("ST_PR06"); }
                    if (seconds < 30) { return Emit("ST_PR07"); }
                }
                if (minutes <= 3) {
                    if (seconds < 0)  { return Emit("ST_PR08"); }
                    if (seconds < 30) { return Emit("ST_PR09"); }
                }
                if (minutes <= 4 && seconds < 0)   { return Emit("ST_PR10"); }
                if (minutes <= 5 && seconds < 0)   { return Emit("ST_PR11"); }
                if (minutes <= 10 && seconds < 0)  { return Emit("ST_PR12"); }
                if (minutes <= 20 && seconds < 0)  { return Emit("ST_PR13"); }
                if (minutes <= 25 && seconds < 0)  { return Emit("ST_PR14"); }
                if (minutes <= 30 && seconds < 0)  { return Emit("ST_PR15"); }
                if (minutes <= 49 && seconds < 2)  { return Emit("ST_PR16"); }
                if (minutes <= 50 && seconds < 0)  { return Emit("ST_PR17"); }
                if (minutes <= 100 && seconds < 0) { return Emit("ST_PR18"); }
                return Emit("ST_PR19");
            }
            case 0x14B: // 0x55B528
            case 0x14C:
            case 0x14D: {
                const int32 valueB = s_GangValue[id];
                const int32 valueA = s_GangIcon[id];
                if (valueA < 0) {
                    return R::Skip;
                }
                Print(gString2, "ST_GNG%d", valueA);
                Print(gString, "ST_LAB%d", id - 0x14B);
                const GxtChar* label = TheText.Get(gString);
                if (line == arg0) {
                    GxtCharStrcpy(gGxtString, label);
                    GxtCharStrcpy(gGxtString2, TheText.Get(gString2));
                    return R::Done;
                }
                line++;
                Print(gString, "%d", valueB);
                AsciiToGxtChar(gString, gGxtString2);
                if (line == arg0) {
                    gGxtString[0] = '\0';
                    // NOTSA: The original copies `gGxtString2` onto itself here (no-op)
                    return R::Done;
                }
                return R::Count;
            }
            case 0x14E: { // 0x55AECB, money lost gambling (never negative)
                double lost = (double)StatTypesFloat[STAT_MONEY_SPENT_GAMBLING] - (double)StatTypesFloat[STAT_MONEY_WON_GAMBLING]; // x87
                if (lost < 0.0) {
                    lost = 0.0;
                }
                if (line != arg0) {
                    return R::Count;
                }
                return PrintValue("$%.2f", lost);
            }
            case 0x151: // 0x55B056, gang territory percentage
                if (line != arg0) {
                    return R::Count;
                }
                return PrintValue("%0.2f%%", (double)CGangWars::TerritoryUnderControlPercentage * (double)100.0f);
            default: // 0x141, 0x14F, 0x150
                return R::Skip;
            }
        }

        if (id >= 0xFC) { // 0xFC..0x101, 0x55AE07
            const float value = id16 < 0x52 ? StatTypesFloat[id16] : (float)RawInt(id16);
            if (!(value > 0.0f)) {
                return R::Skip;
            }
            if (line != arg0) {
                return R::Count;
            }
            return PrintValue("%0.2f%%", (double)GetStatValue((eStats)id));
        }

        if ((uint32)id > 0xF3) {
            return R::Skip;
        }

        switch (id) {
        case 0x00: // 0x55AB1F
            if (line != arg0) {
                return R::Count;
            }
            return PrintValue("%0.2f%%", (double)GetPercentageProgress());
        case 0x8F: { // 0x55AC59, stuck in the vehicle
            if (line == arg0) {
                return TailNoValue();
            }
            const int32 state = IntStat(0x8F);
            line++;
            const char* key;
            switch ((uint32)(state - 1) > 7 ? 0 : state) {
            case 1:  key = "INSTUN"; break;
            case 2:  key = "PRINST"; break;
            case 3:  key = "DBINST"; break;
            case 4:  key = "DBPINS"; break;
            case 5:  key = "TRINST"; break;
            case 6:  key = "PRTRST"; break;
            case 7:  key = "QUINST"; break;
            case 8:  key = "PQUINS"; break;
            default: key = "NOSTUC"; break;
            }
            if (line != arg0) {
                return R::Count;
            }
            return TextOnly(key);
        }
        case 0x90: // 0x55ABC6
        case 0x91: // 0x55ABE2
            if (line != arg0) {
                return R::Count;
            }
            return PrintPair(IntStat(id), CStuntJumpManager::m_iNumJumps);
        case 0xA4: // 0x55AB02, armour of the player 0
            if (line != arg0) {
                return R::Count;
            }
            return PrintValue("%d", (int32)CWorld::Players[0].m_pPed->m_fArmour);
        case 0xA9: { // 0x55AD5F
            int32 minutes, seconds;
            TimeStat(169, minutes, seconds);
            if (line != arg0) {
                return R::Count;
            }
            BuildStatLine(gString, &minutes, 0, &seconds, 1);
            return R::Done;
        }
        case 0xAD: { // 0x55ADAD
            int32 minutes, seconds;
            TimeStat(173, minutes, seconds);
            if (!(minutes > 0 || seconds > 0)) {
                return R::Skip;
            }
            if (line != arg0) {
                return R::Count;
            }
            BuildStatLine(gString, &minutes, 0, &seconds, 1);
            return R::Done;
        }
        case 0xAE: // 0x55AB58
            if (line != arg0) {
                return R::Count;
            }
            return PrintPair(IntStat(174), 12);
        case 0xAF: // 0x55AB74
            if (line != arg0) {
                return R::Count;
            }
            return PrintPair(IntStat(175), 25);
        case 0xD5: // 0x55AB3C
            if (line != arg0) {
                return R::Count;
            }
            return PrintPair(IntStat(213), 30);
        case 0xE7: // 0x55AB90
            if (line != arg0) {
                return R::Count;
            }
            return PrintPair(IntStat(231), IntStat(232));
        case 0xF1: // 0x55ABFE
            if (line != arg0) {
                return R::Count;
            }
            return PrintPair(IntStat(241), IntStat(242));
        case 0xF3: // 0x55ABAB
            if (line != arg0) {
                return R::Count;
            }
            return PrintPair(IntStat(243), IntStat(244));
        default:
            return R::Skip;
        }
    };

    for (const tStatLine* e = table; e->statId != -99; e++) {
        switch (Step(*e)) {
        case R::Done:
            return 0;
        case R::Count:
            line++;
            break;
        case R::Skip:
            break;
        }
    }
    return line;
}

// 0x55B900
void CStats::ProcessReactionStatsOnIncrement(eStats stat) {
    if (stat != STAT_STAMINA && stat != STAT_ENERGY && stat != STAT_LUNG_CAPACITY)
        return;

    float energy = GetStatValue(STAT_ENERGY);

    if (stat == STAT_STAMINA || stat == STAT_LUNG_CAPACITY) {
        if (energy < 0.0f) {
            StatTypesFloat[STAT_FAT] = std::max(StatTypesFloat[STAT_FAT] - 23.0f, 0.0f);
            CheckForStatsMessage();
        }
        return;
    }

    if (energy > 1000.0f)
        IncrementStat(STAT_FAT, energy - 1000.0f);
}

// 0x55B980
void CStats::DisplayScriptStatUpdateMessage(eStatUpdateState state, eStats stat, float value) {
    if (CPad::GetPad(0)->JustOutOfFrontEnd
        || !bShowUpdateStats
        || TheCamera.m_bWideScreenOn
        || CHud::HelpMessageDisplayed()
        || bStatUpdateMessageDisplayed
        || CMenuSystem::num_menus_in_use)
    {
        bStatUpdateMessageDisplayed = false;

        return;
    }

    if (IsStatCapped(stat) && GetStatValue(stat) >= 1000.0f)
        return;

    switch (stat) {
    case STAT_FAT:
    case STAT_STAMINA:
    case STAT_MUSCLE:
    case STAT_MAX_HEALTH:
    case STAT_SEX_APPEAL:
    case STAT_PISTOL_SKILL:
    case STAT_SILENCED_PISTOL_SKILL:
    case STAT_DESERT_EAGLE_SKILL:
    case STAT_SHOTGUN_SKILL:
    case STAT_SAWN_OFF_SHOTGUN_SKILL:
    case STAT_COMBAT_SHOTGUN_SKILL:
    case STAT_MACHINE_PISTOL_SKILL:
    case STAT_SMG_SKILL:
    case STAT_AK_47_SKILL:
    case STAT_M4_SKILL:
    case STAT_RIFLE_SKILL:
    case STAT_GAMBLING:
    case STAT_DRIVING_SKILL:
    case STAT_ARMOR:
    case STAT_ENERGY:
        if (value > 1.0f)
            CHud::SetHelpMessageStatUpdate(state, stat, value, 1000.0f);
        break;

    case STAT_TOTAL_RESPECT:
        CHud::SetHelpMessageStatUpdate(state, stat, value, 1000.0f);
        break;

    case STAT_FLYING_SKILL:
    case STAT_LUNG_CAPACITY:
    case STAT_BIKE_SKILL:
    case STAT_CYCLING_SKILL:
    case STAT_LUCK:
        if (value > 1.0f)
            CHud::SetHelpMessageStatUpdate(state, stat, value, 1000.0f);
        break;

    case STAT_PROGRESS_WITH_DENISE:
    case STAT_PROGRESS_WITH_MICHELLE:
    case STAT_PROGRESS_WITH_HELENA:
    case STAT_PROGRESS_WITH_BARBARA:
    case STAT_PROGRESS_WITH_KATIE:
    case STAT_PROGRESS_WITH_MILLIE:
        CHud::SetHelpMessageStatUpdate(state, stat, value, 100.0f);
        break;

    case STAT_PIMPING_LEVEL:
        CHud::SetHelpMessageStatUpdate(state, STAT_PIMPING_LEVEL, value, 10.0f);
        break;

    case STAT_GANG_STRENGTH: {
        if (auto player = FindPlayerPed())
        {
            auto maxGroup = std::min<uint8>(FindMaxNumberOfGroupMembers(), player->GetPlayerData()->m_nScriptLimitToGangSize);
            CHud::SetHelpMessageStatUpdate(state, stat, value, maxGroup);
        }
        break;
    }
    default:
        return;
    }
}

// 0x55BC50
void CStats::UpdateRespectStat(uint8 arg0) {
    if (arg0) {
        m_RespectLastValue = -99.0f;
        m_RespectThreshold = -99.0f;
        return;
    }

    // NOTE: the original keeps intermediates on the x87 stack (extended precision), we use doubles
    const double A  = (double)StatTypesFloat[STAT_RESPECT] * (double)0.4f;
    const double B  = (double)StatTypesInt[STAT_RESPECT_MISSION - FIRST_INT_STAT] * 1000.0;
    const double D  = (double)StatTypesInt[STAT_RESPECT_MISSION_TOTAL - FIRST_INT_STAT] < 1.0 ? 1.0 : (double)StatTypesInt[STAT_RESPECT_MISSION_TOTAL - FIRST_INT_STAT];
    const double C  = (B / D) * (double)0.36f;
    const double S1 = (A + C) + (double)StatTypesFloat[STAT_GIRLFRIEND_RESPECT] * (double)0.03f;

    double territory = (double)CGangWars::TerritoryUnderControlPercentage - (double)0.2f;
    if (territory < 0.0) {
        territory = 0.0;
    }
    const float S2 = (float)(territory * 1250.0 * (double)0.05f + S1);

    const double money = (double)CWorld::Players[CWorld::PlayerInFocus].m_nMoney * (double)1.0e-7f;
    const float  moneyFactor = 1.0 < money ? 1.0f : (float)money;

    const float muscle  = StatTypesFloat[STAT_MUSCLE];
    const float clothes = StatTypesFloat[STAT_CLOTHES_RESPECT];

    const double tagged = (double)(CTagManager::GetPercentageTagged() * 10) * (double)0.05f; // 0x49CDA0
    const double sumExt = tagged + (((double)moneyFactor * 1000.0 * (double)0.05f + (double)S2) + (double)muscle * (double)0.03f + (double)clothes * (double)0.03f);

    float respect = (float)sumExt;
    if (sumExt < 0.0) {
        respect = 0.0f;
    }
    if (CCheat::IsActive(CHEAT_MAX_RESPECT)) {
        respect = 1000.0f;
    }

    if ((int32)m_RespectLastValue == (int32)respect) {
        return;
    }

    StatTypesFloat[STAT_TOTAL_RESPECT] = respect;
    CheckForStatsMessage();

    if (respect > m_RespectThreshold || m_RespectLastValue < 2.0f) {
        if (m_RespectLastValue != -99.0f && m_RespectThreshold != -99.0f) {
            if (CheckForThreshold(&m_RespectThreshold, respect) || m_RespectLastValue < 2.0f) {
                DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_TOTAL_RESPECT, respect);
            }
        }
        if (respect < m_RespectLastValue) {
            m_RespectThreshold = respect;
        }
    } else {
        if (m_RespectLastValue != -99.0f && m_RespectThreshold != -99.0f) {
            if (CheckForThreshold(&m_RespectThreshold, respect) || respect < 2.0f) {
                DisplayScriptStatUpdateMessage(STAT_UPDATE_DECREASE, STAT_TOTAL_RESPECT, respect);
            }
        }
        if (m_RespectLastValue < respect) {
            m_RespectThreshold = respect;
            m_RespectLastValue = respect;
            return;
        }
    }
    m_RespectLastValue = respect;
}

// 0x55BF20
void CStats::UpdateSexAppealStat() {
    const float base = StatTypesFloat[STAT_APPEARANCE] * 0.5f;
    float       appeal = 0.0f;

    if (FindPlayerVehicle(-1, false)) {
        m_pSexAppealVehicle = FindPlayerVehicle(-1, false);
        m_pSexAppealVehicle->RegisterReference(reinterpret_cast<CEntity**>(&m_pSexAppealVehicle)); // 0x571B70
    }

    double result;
    if (m_pSexAppealVehicle) {
        const auto& vehPos    = m_pSexAppealVehicle->GetPosition();
        const auto  playerPos = FindPlayerCoors(-1);
        const double dx = (double)vehPos.x - (double)playerPos.x;
        const double dy = (double)vehPos.y - (double)playerPos.y;
        if (std::sqrt(dx * dx + dy * dy) < 35.0) {
            appeal = (float)(m_pSexAppealVehicle->m_fHealth * 2.0 - 1000.0);
            // x87: clamp to [0, 1000]
            double scaled;
            if (1000.0f < appeal) {
                scaled = 1000.0;
            } else if (appeal < 0.0f) {
                scaled = 0.0;
            } else {
                scaled = appeal;
            }
            switch ((int8)CModelInfo::GetVehicleModelInfo(m_pSexAppealVehicle->m_nModelIndex)->m_nVehicleClass) {
            case 1:  // VEHICLE_CLASS_POORFAMILY
            case 5:  // VEHICLE_CLASS_BIG
            case 7:  // VEHICLE_CLASS_MOPED
            case 11: // VEHICLE_CLASS_BICYCLE
                scaled *= (double)0.1f;
                break;
            case 2: // VEHICLE_CLASS_RICHFAMILY
            case 3: // VEHICLE_CLASS_EXECUTIVE
            case 9: // VEHICLE_CLASS_LEISUREBOAT
                break;
            case 4:  // VEHICLE_CLASS_WORKER
            case 6:  // VEHICLE_CLASS_TAXI
            case 10: // VEHICLE_CLASS_WORKERBOAT
                scaled *= (double)0.3f;
                break;
            default:
                scaled *= 0.5;
                break;
            }
            result = scaled * 0.5 + (double)base;
        } else {
            result = (double)appeal * 0.5 + (double)base;
        }
    } else {
        result = (double)appeal * 0.5 + (double)base;
    }

    if (result < 0.0) {
        result = 0.0;
    } else if (result > 1000.0) {
        result = 1000.0;
    }
    if (CCheat::IsActive(CHEAT_MAX_SEX_APPEAL)) {
        result = 1000.0;
    }
    StatTypesFloat[STAT_SEX_APPEAL] = (float)result;

    CheckForStatsMessage(); // tail call
}

// 0x55C180
void CStats::IncrementStat(eStats stat, float value)
{
    if (value <= 0.0f)
        return;

    if (IsStatFloat(stat)) { // float
        StatTypesFloat[stat] += value;

        if (IsStatCapped(stat))
            StatTypesFloat[stat] = std::min(StatTypesFloat[stat], 1000.0f);

        ProcessReactionStatsOnIncrement(stat);
        CheckForStatsMessage();

        return;
    }

    CPlayerPed* player = FindPlayerPed();
    CPlayerInfo* playerInfo = player->GetPlayerInfoForThisPlayerPed();

    if (stat == STAT_CALORIES) {
        float healthDiff = playerInfo->m_nMaxHealth - player->m_fHealth;

        IncrementStat(STAT_RIOT_MISSION_ACCOMPLISHED, value);

        if (value > healthDiff) {
            float avg = (value - healthDiff) / 2.0f;

            IncrementStat(STAT_FAT, avg);
        }

        ProcessReactionStatsOnIncrement(stat);
        CheckForStatsMessage();

        return;
    }

    if (stat != STAT_RIOT_MISSION_ACCOMPLISHED) {
        assert(stat >= FIRST_INT_STAT);

        StatTypesInt[stat - FIRST_INT_STAT] += (int32)value;

        if (IsStatCapped(stat))
            StatTypesInt[stat - FIRST_INT_STAT] = std::min(StatTypesInt[stat - FIRST_INT_STAT], 1000);

        ProcessReactionStatsOnIncrement(stat);
        CheckForStatsMessage();

        return;
    }

    // STAT_RIOT_MISSION_ACCOMPLISHED increment, enum name incorrect?

    float kcals = playerInfo->m_nNumHoursDidntEat - value / 2.0f;
    kcals = std::clamp(kcals, 0.0f, 36.0f);

    float healthDiff = playerInfo->m_nMaxHealth - player->m_fHealth;

    if (value >= healthDiff) {
        playerInfo->m_nNumHoursDidntEat = 0;
    }

    player->m_fHealth += value;
    UpdateStatsAddToHealth((uint32)value);
    ProcessReactionStatsOnIncrement(stat);
    CheckForStatsMessage();
}

// 0x55C470
void CStats::UpdateFatAndMuscleStats(uint32 value) {
    if ((double)StatReactionValue[STAT_TIMELIMIT_FAT_ADJUST] * 1000.0 < (double)m_FatCounter) {
        m_FatCounter = 0;

        if (StatTypesFloat[STAT_FAT] > 0.0f) {
            if (StatReactionValue[STAT_DEC_FAT] > 0.0f) {
                const double fat = (double)StatTypesFloat[STAT_FAT] - (double)StatReactionValue[STAT_DEC_FAT];
                StatTypesFloat[STAT_FAT] = fat > 0.0 ? (float)fat : 0.0f;
                CheckForStatsMessage();
            }
            IncrementStat(STAT_MUSCLE, StatReactionValue[STAT_INC_BODY_MUSCLE]);
            if (m_FatMuscleMessageState == 1) {
                m_FatMuscleMessageState = 2;
                DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_MUSCLE, StatReactionValue[STAT_INC_BODY_MUSCLE]);
            } else {
                m_FatMuscleMessageState = 1;
                DisplayScriptStatUpdateMessage(STAT_UPDATE_DECREASE, STAT_FAT, StatReactionValue[STAT_DEC_FAT]);
            }
        } else {
            if (StatReactionValue[STAT_DEC_BODY_MUSCLE] > 0.0f) {
                const double muscle = (double)StatTypesFloat[STAT_MUSCLE] - (double)StatReactionValue[STAT_DEC_BODY_MUSCLE];
                StatTypesFloat[STAT_MUSCLE] = muscle > 0.0 ? (float)muscle : 0.0f;
                CheckForStatsMessage();
            }
            m_FatMuscleMessageState = 3;
            DisplayScriptStatUpdateMessage(STAT_UPDATE_DECREASE, STAT_MUSCLE, StatReactionValue[STAT_DEC_BODY_MUSCLE]);
        }
    } else {
        m_FatCounter += (uint32)TimeStepInMS() * value / 10;
    }

    if ((double)StatReactionValue[STAT_TIMELIMIT_MAX_HEALTH] * 1000.0 < (double)m_MaxHealthCounter) {
        IncrementStat(STAT_MAX_HEALTH, StatReactionValue[STAT_INC_MAX_HEALTH]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_MAX_HEALTH, StatReactionValue[STAT_INC_MAX_HEALTH]);
        m_MaxHealthCounter = 0;
        return;
    }
    m_MaxHealthCounter += TimeStepInMS();
}

// 0x55C660
void CStats::UpdateStatsWhenSprinting() {
    UpdateFatAndMuscleStats(static_cast<uint32>(StatReactionValue[STAT_EXERCISE_RATE_SPRINT]));
    if (StatReactionValue[STAT_TIMELIMIT_SPRINT_STAMINA] * 1000.0f >= static_cast<float>(m_SprintStaminaCounter)) {
        m_SprintStaminaCounter += static_cast<uint32>(CTimer::GetTimeStepInMS());
    } else {
        m_SprintStaminaCounter = 0;
        IncrementStat(STAT_STAMINA, StatReactionValue[STAT_INC_SPRINT_STAMINA]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_STAMINA, StatReactionValue[STAT_INC_SPRINT_STAMINA]);
    }
}

// 0x55C6F0
void CStats::UpdateStatsWhenRunning() {
    UpdateFatAndMuscleStats((uint32)StatReactionValue[STAT_EXERCISE_RATE_RUN]);
    if (StatReactionValue[STAT_TIMELIMIT_RUNNING] * 1000.0f >= static_cast<float>(m_RunningCounter)) {
        m_RunningCounter += static_cast<uint32>(CTimer::GetTimeStepInMS());
    } else {
        m_RunningCounter = 0;
        IncrementStat(STAT_STAMINA, StatReactionValue[STAT_INC_RUNNING]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_STAMINA, StatReactionValue[STAT_INC_RUNNING]);
    }
}

// 0x55C780
void CStats::UpdateStatsWhenCycling(bool arg0, CBmx* bmx) {
    if (std::abs(bmx->m_GasPedal) > 0.0f || arg0) {
        UpdateFatAndMuscleStats(static_cast<uint32>(StatReactionValue[arg0 ? STAT_EXERCISE_RATE_CYCLE_SPRINT : STAT_EXERCISE_RATE_CYCLE]));

        if ((double)StatReactionValue[STAT_TIMELIMIT_CYCLE_STAMINA] * 1000.0 < (double)m_CycleStaminaCounter) {
            m_CycleStaminaCounter = 0;
            IncrementStat(STAT_STAMINA, StatReactionValue[STAT_INC_CYCLE_STAMINA]);
            DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_STAMINA, StatReactionValue[STAT_INC_CYCLE_STAMINA]);
        } else {
            m_CycleStaminaCounter += TimeStepInMS();
        }
    }

    if ((double)StatReactionValue[STAT_TIMELIMIT_CYCLE_SKILL] * 1000.0 < (double)m_CycleSkillCounter) {
        m_CycleSkillCounter = 0;
        IncrementStat(STAT_CYCLING_SKILL, StatReactionValue[STAT_INC_CYCLE_SKILL]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_CYCLING_SKILL, StatReactionValue[STAT_INC_CYCLE_SKILL]);
        return;
    }

    // x87: (x*x + y*y) + z*z in extended precision
    const double speedSq = (double)bmx->m_vecMoveSpeed.x * bmx->m_vecMoveSpeed.x
                         + (double)bmx->m_vecMoveSpeed.y * bmx->m_vecMoveSpeed.y
                         + (double)bmx->m_vecMoveSpeed.z * bmx->m_vecMoveSpeed.z;

    float rate;
    if (bmx->m_nNoOfContactWheels < 2 && (double)0.05f * (double)0.05f < speedSq) {
        rate = 3.0f;
    } else if ((double)0.2f * (double)0.2f < speedSq) {
        rate = arg0 ? 1.5f : 1.0f;
    } else {
        return;
    }
    if (!(rate > 0.0f)) {
        return;
    }

    m_CycleSkillCounter += (int32)std::ceil((double)(uint32)TimeStepInMS() * (double)rate);
}

// 0x55C990
void CStats::UpdateStatsWhenSwimming(bool arg0, bool arg1) {
    UpdateFatAndMuscleStats(static_cast<uint32>(StatReactionValue[(arg0 || arg1) ? STAT_EXERCISE_RATE_SWIM_SPRINT : STAT_EXERCISE_RATE_SWIM]));

    if ((double)StatReactionValue[STAT_TIMELIMIT_SWIM_STAMINA] * 1000.0 < (double)m_SwimStaminaCounter) {
        m_SwimStaminaCounter = 0;
        IncrementStat(STAT_STAMINA, StatReactionValue[STAT_INC_SWIM_STAMINA]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_STAMINA, StatReactionValue[STAT_INC_SWIM_STAMINA]);
    } else {
        m_SwimStaminaCounter += TimeStepInMS();
    }

    if (arg0) {
        if ((double)StatReactionValue[STAT_TIMELIMIT_BREATH_UNDERWATER] * 1000.0 < (double)m_SwimUnderWaterCounter) {
            m_SwimUnderWaterCounter = 0;
            IncrementStat(STAT_LUNG_CAPACITY, StatReactionValue[STAT_INC_BREATH_UNDERWATER]);
            DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_LUNG_CAPACITY, StatReactionValue[STAT_INC_BREATH_UNDERWATER]);
            return;
        }
        m_SwimUnderWaterCounter += TimeStepInMS();
    }
}

// 0x55CAC0
void CStats::UpdateStatsWhenDriving(CVehicle* vehicle) {
    const auto* const automobile = vehicle->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE ? vehicle->AsAutomobile() : nullptr;

    const float counter = (float)m_DrivingCounter; // x87: spilled to a float temp
    if ((double)StatReactionValue[STAT_TIMELIMIT_DRIVING_SKILL] * 1000.0 < (double)counter) {
        m_DrivingCounter = 0;
        IncrementStat(STAT_DRIVING_SKILL, StatReactionValue[STAT_INC_DRIVING_SKILL]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_DRIVING_SKILL, StatReactionValue[STAT_INC_DRIVING_SKILL]);
        return;
    }

    const double speed = std::sqrt(
          (double)vehicle->m_vecMoveSpeed.x * vehicle->m_vecMoveSpeed.x
        + (double)vehicle->m_vecMoveSpeed.y * vehicle->m_vecMoveSpeed.y
        + (double)vehicle->m_vecMoveSpeed.z * vehicle->m_vecMoveSpeed.z
    );

    float rate;
    if (speed > (double)0.8f || (automobile && automobile->m_nNumContactWheels == 0)) {
        rate = 1.5f;
    } else if (speed > (double)0.2f) {
        rate = 0.5f;
    } else {
        return;
    }
    m_DrivingCounter = (int32)((double)(uint32)TimeStepInMS() * (double)rate + (double)counter);
}

// 0x55CC00
void CStats::UpdateStatsWhenFlying(CVehicle* vehicle) {
    if (vehicle->m_nVehicleType != VEHICLE_TYPE_AUTOMOBILE) {
        return;
    }

    const float counter = (float)m_FlyingCounter; // x87: spilled to a float temp
    if ((double)StatReactionValue[STAT_TIMELIMIT_FLYING_SKILL] * 1000.0 < (double)counter) {
        m_FlyingCounter = 0;
        IncrementStat(STAT_FLYING_SKILL, StatReactionValue[STAT_INC_FLYING_SKILL]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_FLYING_SKILL, StatReactionValue[STAT_INC_FLYING_SKILL]);
        return;
    }

    if (vehicle->AsAutomobile()->m_nNumContactWheels) {
        return;
    }

    const double speed = std::sqrt(
          (double)vehicle->m_vecMoveSpeed.x * vehicle->m_vecMoveSpeed.x
        + (double)vehicle->m_vecMoveSpeed.y * vehicle->m_vecMoveSpeed.y
        + (double)vehicle->m_vecMoveSpeed.z * vehicle->m_vecMoveSpeed.z
    );

    float rate;
    if (speed > (double)1.3f || vehicle->GetMatrix().GetUp().z < 0.0f) {
        rate = 1.5f;
    } else if (speed > (double)0.5f) {
        rate = 0.5f;
    } else {
        return;
    }
    m_FlyingCounter = (int32)((double)(uint32)TimeStepInMS() * (double)rate + (double)counter);
}

// 0x55CD60
void CStats::UpdateStatsWhenOnMotorBike(CBike* bike) {
    auto bikeCounter = static_cast<float>(m_BikeCounter);
    if (StatReactionValue[STAT_TIMELIMIT_MOTORBIKE_SKILL] * 1000.0f >= bikeCounter) {
        const float bikeMoveSpeed = bike->m_vecMoveSpeed.Magnitude();
        const auto  fTimeStep = CTimer::GetTimeStepInMS();

        if (bikeMoveSpeed > 0.6f || bike->m_nNoOfContactWheels < 3u && bikeMoveSpeed > 0.1f)
            m_BikeCounter = static_cast<uint32>(fTimeStep * 1.5f + bikeCounter);
        else if (bikeMoveSpeed > 0.2f)
            m_BikeCounter = static_cast<uint32>(fTimeStep * 0.5f + bikeCounter);
    } else {
        m_BikeCounter = 0;
        IncrementStat(STAT_BIKE_SKILL, StatReactionValue[STAT_INC_MOTORBIKE_SKILL]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_BIKE_SKILL, StatReactionValue[STAT_INC_MOTORBIKE_SKILL]);
    }
}

// 0x55CEB0
void CStats::UpdateStatsWhenWeaponHit(eWeaponType weaponType) {
    const auto stat    = CWeaponInfo::GetSkillStatIndex(weaponType);
    const auto statIdx = (uint16)stat;
    const auto reactId = (int32)stat - (int32)STAT_PISTOL_SKILL;

    const float skill = statIdx < StatTypesFloat.size()
        ? StatTypesFloat[statIdx]
        : (float)StatTypesInt[statIdx - FIRST_INT_STAT];

    if (CGameLogic::IsCoopGameGoingOn()) {
        return;
    }
    if (!(skill < 1000.0f)) {
        return;
    }

    const float increment = StatReactionValue[STAT_INC_PISTOL_SKILL + reactId];
    IncrementStat(stat, increment);

    if (m_LastWeaponTypeFired != (uint32)reactId) {
        m_LastWeaponTypeFired = (uint32)reactId;
        m_WeaponCounter = 0;
        return;
    }

    const double counter = (double)m_WeaponCounter;
    if (counter > (double)StatReactionValue[STAT_TIMELIMIT_PISTOL_SKILL + reactId]) {
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, stat, (float)(counter * (double)increment));
        m_WeaponCounter = 0;
        return;
    }
    m_WeaponCounter++;
}

// 0x55CFA0
void CStats::UpdateStatsWhenFighting() {
    UpdateFatAndMuscleStats(static_cast<uint32>(StatReactionValue[STAT_EXERCISE_RATE_FIGHT]));
}

// 0x55CFC0
void CStats::UpdateStatsOnRespawn() {
    if ((double)m_DeathCounter > (double)StatReactionValue[STAT_TIMELIMIT_DEATH_HEALTH]) {
        if (StatTypesFloat[STAT_MAX_HEALTH] > 400.0f) {
            IncrementStat(STAT_MAX_HEALTH, StatReactionValue[STAT_DEC_MAX_HEALTH]);
            DisplayScriptStatUpdateMessage(STAT_UPDATE_DECREASE, STAT_MAX_HEALTH, StatReactionValue[STAT_DEC_MAX_HEALTH]);
        }
        m_DeathCounter = 0;
    } else {
        m_DeathCounter++;
    }
}

// 0x55D030
void CStats::UpdateStatsAddToHealth(uint32 addToHealth) {
    m_AddToHealthCounter += addToHealth;
    if ((double)m_AddToHealthCounter > (double)StatReactionValue[STAT_TIMELIMIT_ADD_TO_HEALTH]) {
        IncrementStat(STAT_MAX_HEALTH, StatReactionValue[STAT_INC_MAX_HEALTH]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_MAX_HEALTH, StatReactionValue[STAT_INC_MAX_HEALTH]);
        m_AddToHealthCounter = 0;
    }
}

// 0x55D090
void CStats::ModifyStat(eStats stat, float value) {
    if (value < 0.0f) {
        CStats::DecrementStat(stat, -value);
    } else {
        CStats::IncrementStat(stat, value);
    }
}

// 0x5D3B40
bool CStats::Save() {
    IncrementStat(STAT_TOTAL_LEGITIMATE_KILLS, GetStatValue(STAT_KILLS_SINCE_LAST_CHECKPOINT));
    SetStatValue(STAT_KILLS_SINCE_LAST_CHECKPOINT, 0.0f);

    CGenericGameStorage::SaveDataToWorkBuffer(StatTypesFloat);
    CGenericGameStorage::SaveDataToWorkBuffer(StatTypesInt);
    CGenericGameStorage::SaveDataToWorkBuffer(PedsKilledOfThisType);
    CGenericGameStorage::SaveDataToWorkBuffer(LastMissionPassedName);
    CGenericGameStorage::SaveDataToWorkBuffer(FavoriteRadioStationList);
    CGenericGameStorage::SaveDataToWorkBuffer(TimesMissionAttempted);
    // TODO: NOTSA: CGenericGameStorage::SaveDataToWorkBuffer(StatMessage);
    for (auto& statMessage : StatMessage) {
        CGenericGameStorage::SaveDataToWorkBuffer(statMessage.displayed);
    }
    return true;
}

// 0x5D3BF0
bool CStats::Load() {
    CGenericGameStorage::LoadDataFromWorkBuffer(StatTypesFloat);
    CGenericGameStorage::LoadDataFromWorkBuffer(StatTypesInt);
    CGenericGameStorage::LoadDataFromWorkBuffer(PedsKilledOfThisType);
    CGenericGameStorage::LoadDataFromWorkBuffer(LastMissionPassedName);
    CGenericGameStorage::LoadDataFromWorkBuffer(FavoriteRadioStationList);
    CGenericGameStorage::LoadDataFromWorkBuffer(TimesMissionAttempted);
    // TODO: NOTSA: CGenericGameStorage::LoadDataFromWorkBuffer(StatMessage);
    for (auto& statMessage : StatMessage) {
        CGenericGameStorage::LoadDataFromWorkBuffer(statMessage.displayed);
    }
    return true;
}

// Unused
// 0x558DE0
char* CStats::GetStatID(eStats stat) {
    if (!IsStatFloat(stat)) // int32
        sprintf_s(gString, "stat_i_%d", stat);
    else
        sprintf_s(gString, "stat_f_%d", stat);

    return gString;
}

// Unused
// 0x558E70
int8 CStats::GetTimesMissionAttempted(uint8 missionId) {
    return TimesMissionAttempted[missionId];
}

// Unused
// 0x558E80
void CStats::RegisterMissionAttempted(uint8 missionId) {
    if (TimesMissionAttempted[missionId] != -1) {
        TimesMissionAttempted[missionId]++;
    }
}

// Unused
// 0x558EA0
void CStats::RegisterMissionPassed(uint8 missionId) {
    TimesMissionAttempted[missionId] = -1;
}
