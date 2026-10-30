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
NOTSA_GLOBAL(kMod_Fat1000Min2, 0x8CDE58, (float), { 0.5f }); // 0.5
NOTSA_GLOBAL(kMod_F_Muscle, 0x8CDE5C, (float), { -0.25f }); // -0.25
NOTSA_GLOBAL(kMod_F_Fat, 0x8CDE60, (float), { -0.5f }); // -0.5
NOTSA_GLOBAL(kMod_E_Driving, 0x8CDE64, (float), { 0.5f }); // 0.5
NOTSA_GLOBAL(kMod_E_Fat, 0x8CDE68, (float), { 0.5f }); // 0.5
NOTSA_GLOBAL(kMod_D_Bike, 0x8CDE6C, (float), { 0.25f }); // 0.25
NOTSA_GLOBAL(kMod_C_Bike, 0x8CDE70, (float), { 0.3f }); // 0.3
NOTSA_GLOBAL(kMod_B_Bike, 0x8CDE74, (float), { 0.5f }); // 0.5
NOTSA_GLOBAL(kMod_MaxHealth, 0x8CDE78, (float), { 176.0f }); // 176.0
NOTSA_GLOBAL(kMod_Air, 0x8CDE7C, (float), { 3000.0f }); // 3000.0
NOTSA_GLOBAL(kMod_TimeCanRun, 0x8CDE80, (float), { 3000.0f }); // 3000.0
NOTSA_GLOBAL(kMod_6_Cycling, 0x8CDE84, (float), { 1.0f }); // 1.0
NOTSA_GLOBAL(kMod_6_Fat, 0x8CDE88, (float), { -0.5f }); // -0.5
NOTSA_GLOBAL(kMod_5_Cycling, 0x8CDE8C, (float), { 1.0f }); // 1.0
NOTSA_GLOBAL(kMod_5_Stamina, 0x8CDE90, (float), { 0.5f }); // 0.5
NOTSA_GLOBAL(kMod_5_Muscle, 0x8CDE94, (float), { 0.5f }); // 0.5
NOTSA_GLOBAL(kMod_5_Fat, 0x8CDE98, (float), { -1.0f }); // -1.0
NOTSA_GLOBAL(kMod_4_Muscle, 0x8CDE9C, (float), { 1.0f }); // 1.0
NOTSA_GLOBAL(kMod_4_Fat, 0x8CDEA0, (float), { 0.5f }); // 0.5
NOTSA_GLOBAL(kMod_3_Muscle, 0x8CDEA4, (float), { -0.1f }); // -0.1
NOTSA_GLOBAL(kMod_3_Fat, 0x8CDEA8, (float), { -0.2f }); // -0.2
NOTSA_GLOBAL(kMod_2_Muscle, 0x8CDEAC, (float), { -0.1f }); // -0.1
NOTSA_GLOBAL(kMod_2_Fat, 0x8CDEB0, (float), { -0.2f }); // -0.2
NOTSA_GLOBAL(kMod_1_Muscle, 0x8CDEB4, (float), { 0.2f }); // 0.2
NOTSA_GLOBAL(kMod_MuscleBase, 0x8CDEB8, (float), { 50.0f }); // 50.0
NOTSA_GLOBAL(kMod_1_Fat, 0x8CDEBC, (float), { -0.4f }); // -0.4
NOTSA_GLOBAL(kMod_FatBase, 0x8CDEC0, (float), { 200.0f }); // 200.0

//! The exe addresses the int stats as `[stat * 4 + 0xB78E20]` for EVERY stat id >= 0x52 (no range check, 16 bit stat ids)
#ifdef NOTSA_GLOBALS_DETACHED
// 0xB78E20 == &StatTypesInt[0] - FIRST_INT_STAT * 4: ids >= FIRST_INT_STAT + 223 (and 82..119) address the neighbours of the array, as in the exe
int32& IntStatCell(uint32 stat) { return *(CStats::StatTypesInt.data() + ((ptrdiff_t)(int32)stat - (ptrdiff_t)CStats::FIRST_INT_STAT)); }
#else
int32& IntStatCell(uint32 stat) { return StaticRef<int32>(0xB78E20 + 4u * stat); }
#endif
#line 59

//! x87: `CTimer::ms_fTimeStep * 0.02f * 1000.0f` kept in extended precision, then truncated by _ftol (0x821B40)
uint32 TimeStepInMS() {
    return (uint32)NOTSA_FTOL(int32, (x87d)CTimer::ms_fTimeStep * (x87d)0.02f * (x87d)1000.0);
}
}

#ifndef NOTSA_GLOBALS_DETACHED
uint16& CStats::m_ThisStatIsABarChart = StaticRef<uint16>(0xB794CC);
#endif
#line 67

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
    const auto idx = static_cast<uint16>(stat);
    if (idx < FIRST_UNUSED_STAT) {
        StatTypesFloat[idx] = value;
    } else {
        IntStatCell(idx) = (int32)(int16)NOTSA_FTOL(int32, value); // _ftol + `movsx ecx, ax`: the int stats are 16 bit values
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
    // 0x559080, x87 order; the int stats are read raw
    const auto I = [](eStats st) { return (double)IntStatCell(st); };
    const int32 money = CWorld::Players[CWorld::PlayerInFocus].m_nMoney;

    x87d v = X87R(I(STAT_TOTAL_LEGITIMATE_KILLS) - X87R(I(STAT_TIMES_BUSTED) * 3.0));
    v        = v - X87R(I(STAT_NUMBER_OF_HOSPITAL_VISITS) * 3.0);
    v        = v + (X87R(I(STAT_HIGHEST_FIREFIGHTER_MISSION_LEVEL) + I(STAT_HIGHEST_PARAMEDIC_MISSION_LEVEL))) * (x87d)10.0;
    v        = v + (x87d)(money / 5000);
    v        = v + X87R(I(STAT_PLANES_HELICOPTERS_DESTROYED) * 30.0);
    v        = X87R(v + I(STAT_TOTAL_FIRES_EXTINGUISHED));
    v        = X87R(v + I(STAT_CRIMINALS_KILLED_ON_VIGILANTE_MISSION));
    v        = X87R(v + I(STAT_PEOPLE_SAVED_IN_AN_AMBULANCE));
    int32 value = NOTSA_FTOL(int32, v);

    if (CCheat::m_bHasPlayerCheated || I(STAT_TIMES_CHEATED) > 0.0) {
        value = NOTSA_FTOL(int32, (x87d)value - X87R(I(STAT_TIMES_CHEATED) * 10.0));
        if (!(value > -10000)) {
            value = -10000;
        }
    } else {
        value = std::max(value, 0);
    }

    if (I(STAT_BULLETS_FIRED) > 100.0) { // strictly greater (the port used >=)
        value = NOTSA_FTOL(int32, X87R(I(STAT_BULLETS_THAT_HIT) / I(STAT_BULLETS_FIRED)) * (x87d)500.0 + (x87d)value);
    }

    // the progress part is inlined here: progress / total * 1000 (not GetPercentageProgress), skipped when the total is exactly 0
    const x87d total = (x87d)StatTypesFloat[STAT_TOTAL_PROGRESS];
    if (total == 0.0 && !std::isnan(total)) {
        return value;
    }
    return NOTSA_FTOL(int32, (x87d)StatTypesFloat[STAT_PROGRESS_MADE] / total * (x87d)1000.0 + (x87d)value);
}

// 0x5591E0
float CStats::GetPercentageProgress() {
    // 0x5591E0: divides by the stat STAT_TOTAL_PROGRESS (0xB79384) and returns 0 while it is exactly 0, instead of a literal 187
    const float total = StatTypesFloat[STAT_TOTAL_PROGRESS];
    if (total == 0.0f) {
        return 0.0f;
    }
    const float percentage = StatTypesFloat[STAT_PROGRESS_MADE] / total * 100.0f;
    return 100.0f < percentage ? 100.0f : percentage;
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
                const x87d kgs = (x87d)pounds * (x87d)0.4536f; // x87: FILD * 0.4536f, pushed as double
                if constexpr (notsa::IsFixBugs()) {
                    Print("%dkgs", NOTSA_FTOL(int32, kgs));
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
            Print("%.2fft", (x87d)*(float*)pValue1 * (x87d)3.3333333f);
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
    // 0x5595F0: (p + 40 < range) || (p - 40 > range); NaN => false
    if ((x87d)*pValue + (x87d)40.0 < (double)range || (x87d)*pValue - (x87d)40.0 > (double)range) {
        *pValue = range;
        return true;
    }
    return false;
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
        const x87d stat = GetStatValue((eStats)(uint16)msg.stat_num);
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
        strncpy(msg.text_id, textId, sizeof(msg.text_id)); // exe 0x559902: sscanf("%s") straight into text_id[8], the NUL spills one byte; an 8 char id aborted strcpy_s

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
    const x87d fat      = StatTypesFloat[STAT_FAT];
    const x87d stamina  = StatTypesFloat[STAT_STAMINA];
    const x87d muscle   = StatTypesFloat[STAT_MUSCLE];
    const x87d cycling  = StatTypesInt[STAT_CYCLING_SKILL - FIRST_INT_STAT];

    const x87d fatBase = kMod_FatBase, muscleBase = kMod_MuscleBase;
    const x87d fatDen    = (x87d)1000.0 - fatBase;
    const x87d muscleDen = (x87d)1000.0 - muscleBase;

    const auto Max0 = [](float b) { return 0.0f > b ? 0.0f : b; };    // 0x420800(0, b): NaN => b
    const auto Min1 = [](float b) { return 1.0f < b ? 1.0f : b; };    // 0x404330(1, b): NaN => b
    const auto BikeTerm = [&]() {
        return Min1((float)((x87d)StatTypesInt[STAT_BIKE_SKILL - FIRST_INT_STAT] * (x87d)0.001f));
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
        const x87d res = r * (x87d)kMod_1_Fat + (x87d)1.0 + (muscle - muscleBase) * kMod_1_Muscle / muscleDen;
        if (res < 0.7f) {
            return 0.7f;
        }
        return (float)res;
    }
    case STAT_MOD_2: {
        const float r = Max0((float)((fat - fatBase) / fatDen));
        const x87d res = r * (x87d)kMod_2_Fat + (x87d)1.0 + (muscle - muscleBase) * kMod_2_Muscle / muscleDen;
        if (res < 0.8f) {
            return 0.8f;
        }
        return (float)res;
    }
    case STAT_MOD_3: {
        const x87d res = (fat - fatBase) * kMod_3_Fat / fatDen + (x87d)1.0 + (muscle - muscleBase) * kMod_3_Muscle / muscleDen;
        if (res < 0.8f) {
            return 0.8f;
        }
        return (float)res;
    }
    case STAT_MOD_4: {
        const x87d res = (fat - fatBase) * kMod_4_Fat / fatDen + (x87d)1.0 + (muscle - muscleBase) * kMod_4_Muscle / muscleDen;
        if (!(res > 2.0)) {
            return (float)res;
        }
        return 2.0f;
    }
    case STAT_MOD_5: {
        const x87d res =
              (fat - fatBase) * kMod_5_Fat / fatDen + (x87d)1.0 + (muscle - muscleBase) * kMod_5_Muscle / muscleDen
            + stamina * (x87d)0.001f * kMod_5_Stamina
            + cycling * (x87d)0.001f * kMod_5_Cycling;
        if (res > 2.0) {
            return 2.0f;
        }
        if (res < 0.25f) {
            return 0.25f;
        }
        return (float)res;
    }
    case STAT_MOD_6: {
        const x87d res =
              (fat - fatBase) * kMod_6_Fat / fatDen + (x87d)1.0
            + cycling * (x87d)0.001f * kMod_6_Cycling;
        if (res > 2.0) {
            return 2.0f;
        }
        if (res < 0.5f) {
            return 0.5f;
        }
        return (float)res;
    }
    case STAT_MOD_TIME_CAN_RUN:
        return (float)(stamina * (x87d)0.001f * kMod_TimeCanRun + (x87d)150.0);
    case STAT_MOD_AIR_IN_LUNG: {
        const x87d lung = StatTypesInt[STAT_LUNG_CAPACITY - FIRST_INT_STAT];
        return (float)((lung + stamina) * (x87d)0.0005f * kMod_Air + (x87d)1000.0);
    }
    case STAT_MOD_MAX_HEALTH:
        return (float)((x87d)StatTypesFloat[STAT_MAX_HEALTH] * (x87d)0.001f * kMod_MaxHealth);
    case STAT_MOD_10:
        return kMod_MaxHealth;
    case STAT_MOD_11:
        return (float)(BikeTerm() * (x87d)kMod_B_Bike + (x87d)1.0);
    case STAT_MOD_12:
        return (float)(BikeTerm() * (x87d)kMod_C_Bike + (x87d)1.0);
    case STAT_MOD_13:
        return (float)(BikeTerm() * (x87d)kMod_D_Bike + (x87d)1.0);
    case STAT_MOD_DRIVING_SKILL: {
        const x87d driving = StatTypesInt[STAT_DRIVING_SKILL - FIRST_INT_STAT];
        const x87d res = (fat - fatBase) * kMod_E_Fat / fatDen + (driving * (x87d)0.001f * kMod_E_Driving + (x87d)1.0);
        if (res > 1.0) {
            return 1.0f;
        }
        return (float)res;
    }
    case STAT_MOD_15: {
        const x87d res = (fat - fatBase) * kMod_F_Fat / fatDen + (x87d)1.0 + (muscle - muscleBase) * kMod_F_Muscle / muscleDen;
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
    // 0x559730: the stat id is compared as a BYTE
    if ((uint8)stat == (uint8)STAT_ENERGY && (double)IntStatCell(STAT_ENERGY) < 0.0) {
        DecrementStat(STAT_FAT, 23.0f);
    }
}

// 0x559FA0
void CStats::DecrementStat(eStats stat, float value) {
    if (!(value > 0.0f)) { // NaN returns too
        return;
    }

    const auto idx = static_cast<uint16>(stat);
    if (idx < FIRST_UNUSED_STAT) {
        const x87d v = (x87d)StatTypesFloat[idx] - (x87d)value;
        StatTypesFloat[idx] = (float)v;
        StatTypesFloat[idx] = (float)(v > 0.0 ? v : 0.0); // `fcom` + je: only a strictly positive result is kept (NaN => 0)
    } else {
        auto& cell = IntStatCell(idx);
        cell -= (int32)(int16)NOTSA_FTOL(int32, value); // movsx ax
        cell = NOTSA_FTOL(int32, (double)cell > 0.0 ? (double)cell : 0.0);
    }

    // inlined ProcessReactionStatsOnDecrement (byte compare, as in 0x559730)
    if ((uint8)stat == (uint8)STAT_ENERGY && (double)IntStatCell(STAT_ENERGY) < 0.0) {
        DecrementStat(STAT_FAT, 23.0f);
    }
    CheckForStatsMessage();
}

// 0x55C410
void CStats::SetNewRecordStat(eStats stat, float value) {
    // 0x55C415: the stat id is a uint16 here, `cmp cx, 0x52` selects the float / int table; the int slot is addressed without any range check
    const auto idx = static_cast<uint16>(stat);
    if (idx < FIRST_UNUSED_STAT) {
        // `fcom`: the current value is kept only if it is STRICTLY greater than the new one (equal and unordered/NaN current => the new value is stored)
        const float current = StatTypesFloat[idx];
        StatTypesFloat[idx] = current > value ? current : value;
    } else {
#ifdef NOTSA_GLOBALS_DETACHED
        auto&        slot    = IntStatCell(idx); // == StatTypesInt[idx - FIRST_INT_STAT]
#else
        auto&        slot    = StaticRef<int32>(0xB78E20 + 4u * idx); // == StatTypesInt[idx - FIRST_INT_STAT]
#endif
#line 715
        const x87d current = slot;
        const x87d best    = current > (x87d)value ? current : (x87d)value;
        slot = (int32)(int16)NOTSA_FTOL(int32, best); // _ftol + `movsx eax, ax`: the record is truncated to 16 bits
    }
    CheckForStatsMessage();
}

// 0x55A0B0 / 0x55A160 (identical code): a stat of 0 takes the value, otherwise the LOWER one wins (a NaN stat is replaced); the int slots are stored as int16
static void RegisterRecordLowest(eStats stat, int32 value) {
    const auto idx = static_cast<uint16>(stat);
    const bool isFloat = idx < CStats::FIRST_UNUSED_STAT;
    const x87d cur = isFloat ? (x87d)CStats::StatTypesFloat[idx] : (x87d)IntStatCell(idx);
    x87d best;
    if (cur == 0.0) {
        best = (x87d)value;
    } else {
        const float vf = (float)value;
        best = (cur < (x87d)vf) ? cur : (x87d)vf;
    }
    if (isFloat) {
        CStats::StatTypesFloat[idx] = (float)best;
    } else {
        IntStatCell(idx) = (int32)(int16)NOTSA_FTOL(int32, best);
    }
    CStats::CheckForStatsMessage();
}

// 0x55A0B0
void CStats::RegisterFastestTime(eStats stat, int32 fastestTime) {
    RegisterRecordLowest(stat, fastestTime);
}

// 0x55A160
void CStats::RegisterBestPosition(eStats stat, int32 position) {
    RegisterRecordLowest(stat, position);
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
        const auto flightTime = NOTSA_FTOL(int32, (x87d)StatTypesInt[STAT_FLIGHT_TIME - FIRST_INT_STAT] * (x87d)(1.6666667e-05f) * (x87d)0.016666668f);
        return TheText.Get(flightTime > 10 ? "RATNG50" : "RATNG49");
    }

    VALIDATE_OFFSET(CPlayerInfo, m_nDisplayMoney, 0xBC);
    if (rating >= 1000000 && CWorld::Players[CWorld::PlayerInFocus].m_nDisplayMoney > 10000000 /* NOTE: not m_nMoney */) {
        return TheText.Get("RATNG52");
    }
    return TheText.Get("RATNG51");
}

namespace {
// One line of the per-page stat tables of ConstructStatLine (.data 0x8CD8A0 etc., committed as literals: slice D2 of .notes/DETACH_DATA_PLAN.md)
struct tStatLine {
    int16 statId; // -99 terminates the table
    uint8 type;   // Value format (see below)
    uint8 alwaysShow; // Show even if the stat is 0
    uint8 special;    // Stat is processed by the "special" (non-generic) code
    uint8 pad;
};
VALIDATE_SIZE(tStatLine, 6);

// 0x8CD8A0, 24 lines + terminator
constexpr tStatLine kStatLines0[] = {
    { 21, 10, 1, 0, 0 }, { 22, 10, 1, 0, 0 }, { 64, 10, 1, 0, 0 }, { 225, 10, 1, 0, 0 },
    { 25, 10, 1, 0, 0 }, { 23, 10, 1, 0, 0 }, { 160, 10, 1, 0, 0 }, { 223, 10, 1, 0, 0 },
    { 229, 10, 1, 0, 0 }, { 230, 10, 1, 0, 0 }, { 164, 0, 1, 1, 0 }, { 81, 10, 0, 0, 0 },
    { 330, 0, 1, 1, 0 }, { 170, 0, 1, 0, 0 }, { 135, 0, 1, 0, 0 }, { 184, 0, 0, 0, 0 },
    { 186, 0, 0, 0, 0 }, { 188, 0, 0, 0, 0 }, { 185, 0, 0, 0, 0 }, { 187, 0, 0, 0, 0 },
    { 189, 0, 0, 0, 0 }, { 190, 0, 0, 0, 0 }, { 200, 0, 1, 0, 0 }, { 233, 0, 1, 0, 0 },
    { -99, 0, 0, 0, 0 },
};
NOTSA_GLOBAL_VERIFY(0x8CD8A0, kStatLines0);

// 0x8CD938, 19 lines + terminator
constexpr tStatLine kStatLines1[] = {
    { 53, 3, 0, 0, 0 }, { 13, 3, 1, 0, 0 }, { 14, 3, 1, 0, 0 }, { 15, 3, 1, 0, 0 },
    { 16, 3, 1, 0, 0 }, { 30, 3, 1, 0, 0 }, { 31, 3, 1, 0, 0 }, { 33, 3, 1, 0, 0 },
    { 54, 3, 1, 0, 0 }, { 55, 3, 1, 0, 0 }, { 20, 3, 1, 0, 0 }, { 62, 3, 1, 0, 0 },
    { 35, 3, 0, 0, 0 }, { 37, 3, 0, 0, 0 }, { 334, 3, 0, 1, 0 }, { 38, 3, 0, 0, 0 },
    { 39, 3, 0, 0, 0 }, { 41, 3, 0, 0, 0 }, { 40, 3, 0, 0, 0 }, { -99, 0, 0, 0, 0 },
};
NOTSA_GLOBAL_VERIFY(0x8CD938, kStatLines1);

// 0x8CD9B0, 5 lines + terminator
constexpr tStatLine kStatLines2[] = {
    { 328, 0, 1, 1, 0 }, { 329, 0, 1, 1, 0 }, { 126, 0, 1, 0, 0 }, { 127, 0, 1, 0, 0 },
    { 128, 0, 1, 0, 0 }, { -99, 0, 0, 0, 0 },
};
NOTSA_GLOBAL_VERIFY(0x8CD9B0, kStatLines2);

// 0x8CD9D8, 24 lines + terminator
constexpr tStatLine kStatLines3[] = {
    { 191, 0, 0, 0, 0 }, { 192, 0, 0, 0, 0 }, { 182, 0, 0, 0, 0 }, { 183, 0, 0, 0, 0 },
    { 194, 0, 0, 0, 0 }, { 131, 0, 1, 0, 0 }, { 132, 0, 1, 0, 0 }, { 133, 0, 1, 0, 0 },
    { 155, 0, 0, 0, 0 }, { 217, 0, 0, 0, 0 }, { 218, 0, 0, 0, 0 }, { 325, 0, 1, 1, 0 },
    { 324, 0, 1, 1, 0 }, { 120, 0, 1, 0, 0 }, { 121, 0, 1, 0, 0 }, { 122, 0, 1, 0, 0 },
    { 123, 0, 1, 0, 0 }, { 124, 0, 1, 0, 0 }, { 125, 3, 1, 0, 0 }, { 129, 0, 1, 0, 0 },
    { 130, 0, 1, 0, 0 }, { 216, 0, 1, 0, 0 }, { 176, 0, 0, 0, 0 }, { 177, 0, 1, 0, 0 },
    { -99, 0, 0, 0, 0 },
};
NOTSA_GLOBAL_VERIFY(0x8CD9D8, kStatLines3);

// 0x8CDA70, 13 lines + terminator
constexpr tStatLine kStatLines4[] = {
    { 323, 0, 1, 1, 0 }, { 234, 0, 0, 0, 0 }, { 235, 0, 0, 0, 0 }, { 337, 0, 0, 1, 0 },
    { 236, 0, 0, 0, 0 }, { 237, 0, 0, 0, 0 }, { 238, 0, 0, 0, 0 }, { 163, 0, 0, 0, 0 },
    { 239, 0, 0, 0, 0 }, { 240, 0, 0, 0, 0 }, { 331, 0, 1, 1, 0 }, { 332, 0, 1, 1, 0 },
    { 333, 0, 1, 1, 0 }, { -99, 0, 0, 0, 0 },
};
NOTSA_GLOBAL_VERIFY(0x8CDA70, kStatLines4);

// 0x8CDAC8, 49 lines + terminator
constexpr tStatLine kStatLines5[] = {
    { 252, 0, 0, 1, 0 }, { 253, 0, 0, 1, 0 }, { 254, 0, 0, 1, 0 }, { 255, 0, 0, 1, 0 },
    { 256, 0, 0, 1, 0 }, { 257, 0, 0, 1, 0 }, { 3, 7, 0, 0, 0 }, { 26, 7, 0, 0, 0 },
    { 4, 7, 0, 0, 0 }, { 27, 7, 0, 0, 0 }, { 5, 7, 0, 0, 0 }, { 6, 7, 0, 0, 0 },
    { 7, 7, 0, 0, 0 }, { 8, 7, 0, 0, 0 }, { 9, 7, 0, 0, 0 }, { 28, 7, 0, 0, 0 },
    { 169, 0, 1, 1, 0 }, { 173, 0, 1, 1, 0 }, { 44, 9, 0, 0, 0 }, { 29, 7, 0, 0, 0 },
    { 45, 9, 0, 0, 0 }, { 143, 0, 1, 1, 0 }, { 144, 0, 1, 1, 0 }, { 145, 0, 1, 1, 0 },
    { 10, 7, 1, 0, 0 }, { 17, 9, 1, 0, 0 }, { 11, 7, 1, 0, 0 }, { 18, 9, 1, 0, 0 },
    { 12, 7, 1, 0, 0 }, { 19, 9, 1, 0, 0 }, { 139, 7, 1, 0, 0 }, { 140, 0, 1, 0, 0 },
    { 141, 4, 1, 0, 0 }, { 142, 4, 1, 0, 0 }, { 52, 9, 1, 0, 0 }, { 51, 9, 1, 0, 0 },
    { 156, 0, 0, 0, 0 }, { 2, 7, 0, 0, 0 }, { 46, 5, 0, 0, 0 }, { 47, 5, 0, 0, 0 },
    { 205, 0, 0, 0, 0 }, { 206, 0, 0, 0, 0 }, { 207, 0, 0, 0, 0 }, { 208, 0, 0, 0, 0 },
    { 209, 0, 0, 0, 0 }, { 322, 0, 1, 1, 0 }, { 231, 0, 1, 1, 0 }, { 243, 0, 1, 1, 0 },
    { 241, 0, 1, 1, 0 }, { -99, 0, 0, 0, 0 },
};
NOTSA_GLOBAL_VERIFY(0x8CDAC8, kStatLines5);

// 0x8CDBF8, 86 lines + terminator
constexpr tStatLine kStatLines6[] = {
    { 146, 0, 1, 0, 0 }, { 147, 0, 1, 0, 0 }, { 157, 0, 0, 0, 0 }, { 152, 0, 0, 0, 0 },
    { 159, 0, 0, 0, 0 }, { 153, 0, 0, 0, 0 }, { 158, 0, 0, 0, 0 }, { 151, 0, 0, 0, 0 },
    { 149, 3, 0, 0, 0 }, { 150, 0, 0, 0, 0 }, { 161, 0, 0, 0, 0 }, { 162, 3, 0, 0, 0 },
    { 210, 0, 0, 0, 0 }, { 171, 0, 0, 0, 0 }, { 36, 3, 0, 0, 0 }, { 213, 0, 0, 1, 0 },
    { 214, 0, 0, 0, 0 }, { 201, 3, 0, 0, 0 }, { 202, 9, 0, 0, 0 }, { 154, 0, 0, 0, 0 },
    { 166, 0, 0, 0, 0 }, { 167, 0, 0, 1, 0 }, { 168, 0, 0, 1, 0 }, { 246, 9, 0, 0, 0 },
    { 247, 9, 0, 0, 0 }, { 248, 9, 0, 0, 0 }, { 249, 9, 0, 0, 0 }, { 250, 9, 0, 0, 0 },
    { 172, 0, 0, 0, 0 }, { 48, 9, 0, 0, 0 }, { 212, 9, 0, 0, 0 }, { 221, 0, 0, 0, 0 },
    { 220, 9, 0, 0, 0 }, { 219, 9, 0, 0, 0 }, { 49, 9, 0, 0, 0 }, { 222, 9, 0, 0, 0 },
    { 174, 0, 0, 1, 0 }, { 175, 0, 0, 1, 0 }, { 179, 9, 0, 0, 0 }, { 178, 0, 0, 0, 0 },
    { 211, 0, 0, 0, 0 }, { 215, 0, 0, 0, 0 }, { 258, 0, 0, 0, 0 }, { 259, 9, 0, 0, 0 },
    { 260, 0, 0, 0, 0 }, { 261, 9, 0, 0, 0 }, { 262, 0, 0, 0, 0 }, { 263, 9, 0, 0, 0 },
    { 264, 0, 0, 0, 0 }, { 265, 9, 0, 0, 0 }, { 266, 0, 0, 0, 0 }, { 267, 9, 0, 0, 0 },
    { 268, 0, 0, 0, 0 }, { 269, 9, 0, 0, 0 }, { 270, 0, 0, 0, 0 }, { 271, 9, 0, 0, 0 },
    { 272, 0, 0, 0, 0 }, { 273, 9, 0, 0, 0 }, { 274, 0, 0, 0, 0 }, { 275, 9, 0, 0, 0 },
    { 276, 0, 0, 0, 0 }, { 277, 9, 0, 0, 0 }, { 278, 0, 0, 0, 0 }, { 279, 9, 0, 0, 0 },
    { 280, 0, 0, 0, 0 }, { 281, 9, 0, 0, 0 }, { 282, 0, 0, 0, 0 }, { 283, 9, 0, 0, 0 },
    { 284, 0, 0, 0, 0 }, { 285, 9, 0, 0, 0 }, { 286, 0, 0, 0, 0 }, { 287, 9, 0, 0, 0 },
    { 288, 0, 0, 0, 0 }, { 289, 9, 0, 0, 0 }, { 290, 0, 0, 0, 0 }, { 291, 9, 0, 0, 0 },
    { 292, 0, 0, 0, 0 }, { 293, 9, 0, 0, 0 }, { 294, 0, 0, 0, 0 }, { 295, 9, 0, 0, 0 },
    { 296, 9, 0, 0, 0 }, { 297, 9, 0, 0, 0 }, { 298, 9, 0, 0, 0 }, { 299, 9, 0, 0, 0 },
    { 300, 9, 0, 0, 0 }, { 301, 9, 0, 0, 0 }, { -99, 0, 0, 0, 0 },
};
NOTSA_GLOBAL_VERIFY(0x8CDBF8, kStatLines6);

// 0x8CDE08, 12 lines + terminator
constexpr tStatLine kStatLines7[] = {
    { 0, 0, 1, 1, 0 }, { 320, 0, 1, 1, 0 }, { 134, 0, 1, 0, 0 }, { 136, 0, 1, 0, 0 },
    { 137, 0, 0, 0, 0 }, { 138, 0, 1, 0, 0 }, { 326, 0, 1, 1, 0 }, { 327, 0, 1, 1, 0 },
    { 195, 0, 0, 0, 0 }, { 196, 0, 0, 0, 0 }, { 197, 0, 0, 0, 0 }, { 198, 0, 0, 0, 0 },
    { -99, 0, 0, 0, 0 },
};
NOTSA_GLOBAL_VERIFY(0x8CDE08, kStatLines7);
} // namespace
#line 867
// 0x55A780
int32 CStats::ConstructStatLine(int32 arg0, uint8 arg1) {
    // (the per-page tables are kStatLines0..7 above)
#line 878

    const tStatLine* const table = [&]() -> const tStatLine* {
        switch (arg1) {
        case 0:  return &NOTSA_GLOBAL_EXPR(0x8CD8A0, (tStatLine), kStatLines0[0]);
        case 1:  return &NOTSA_GLOBAL_EXPR(0x8CD938, (tStatLine), kStatLines1[0]);
        case 2:  return &NOTSA_GLOBAL_EXPR(0x8CD9B0, (tStatLine), kStatLines2[0]);
        case 3:  return &NOTSA_GLOBAL_EXPR(0x8CD9D8, (tStatLine), kStatLines3[0]);
        case 4:  return &NOTSA_GLOBAL_EXPR(0x8CDA70, (tStatLine), kStatLines4[0]);
        case 5:  return &NOTSA_GLOBAL_EXPR(0x8CDAC8, (tStatLine), kStatLines5[0]);
        case 6:  return &NOTSA_GLOBAL_EXPR(0x8CDBF8, (tStatLine), kStatLines6[0]);
        default: return &NOTSA_GLOBAL_EXPR(0x8CDE08, (tStatLine), kStatLines7[0]);
        }
    }();

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
        const int32 total = NOTSA_FTOL(int32, (x87d)IntStat(id) * (x87d)1.6666667e-05f);
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

        m_ThisStatIsABarChart = 0;

        if (!e.special) {
            switch (e.type) {
            case 10: // 0x55A8A0
                m_ThisStatIsABarChart = (uint16)id;
                if (line != arg0) {
                    return R::Count;
                }
                return PrintValue("%d", NOTSA_FTOL(int32, GetStatValue((eStats)id)));
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
                return PrintValue("%d|", NOTSA_FTOL(int32, GetStatValue((eStats)id)));
            case 5: { // 0x55A914
                if (line != arg0) {
                    return R::Count;
                }
                const int32 pounds = NOTSA_FTOL(int32, GetStatValue((eStats)id));
                gString2[0] = '\0';
                if (CLocalisation::Metric()) { // 0x56D220
                    const x87d kgs = (x87d)pounds * (x87d)0.4536f; // x87: FILD * 0.4536f, pushed as a double
                    if constexpr (notsa::IsFixBugs()) {
                        Print(gString2, "%dkgs", NOTSA_FTOL(int32, kgs));
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
                return PrintValue("%.2fft", (x87d)GetStatValue((eStats)id) * (x87d)3.3333333f); // x87: kept in extended precision
            case 9: { // 0x55A998, minutes:seconds
                if (line != arg0) {
                    return R::Count;
                }
                int32 minutes = ConvertToMins(NOTSA_FTOL(int32, GetStatValue((eStats)id)));
                int32 seconds = ConvertToSecs(NOTSA_FTOL(int32, GetStatValue((eStats)id)));
                BuildStatLine(gString, &minutes, 0, &seconds, 1);
                return R::Done;
            }
            default: // 0x55AAAA, plain integer (types 0, 2, 8 and everything above 10)
                if (line != arg0) {
                    return R::Count;
                }
                return PrintValue("%d", id16 < 0x52 ? NOTSA_FTOL(int32, StatTypesFloat[id16]) : RawInt(id16));
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
                    return PrintValue("%d", NOTSA_FTOL(int32, GetStatValue((eStats)(weaponStatIdx + 0x44))));
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
                // 0x96AB2C + id * 4 / 0x96AB38 + id * 4 (id = 0x14B..0x14D)
                const int32 valueB = CGangWars::GangRatingStrength[id - 0x14B];
                const int32 valueA = CGangWars::GangRatings[id - 0x14B];
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
                x87d lost = (x87d)StatTypesFloat[STAT_MONEY_SPENT_GAMBLING] - (x87d)StatTypesFloat[STAT_MONEY_WON_GAMBLING]; // x87
                if (lost < 0.0) {
                    lost = (x87d)0.0;
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
                return PrintValue("%0.2f%%", (x87d)CGangWars::TerritoryUnderControlPercentage * (x87d)100.0f);
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
            return PrintValue("%d", NOTSA_FTOL(int32, CWorld::Players[0].m_pPed->m_fArmour));
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
    // 0x55B900: the stat id is compared as a BYTE (STAT_STAMINA 0x16, STAT_ENERGY 0xA5, STAT_LUNG_CAPACITY 0xE1); STAT_ENERGY is an int stat
    const auto id     = static_cast<uint8>(stat);
    const x87d energy = (x87d)IntStatCell(STAT_ENERGY);
    if (id == (uint8)STAT_STAMINA || id == (uint8)STAT_LUNG_CAPACITY) {
        if (energy < 0.0) {
            const x87d fat = (x87d)StatTypesFloat[STAT_FAT] - (x87d)23.0;
            StatTypesFloat[STAT_FAT] = (float)(fat > 0.0 ? fat : 0.0);
            CheckForStatsMessage();
        }
    } else if (id == (uint8)STAT_ENERGY) {
        if (energy > 1000.0) {
            IncrementStat(STAT_FAT, (float)(energy - (x87d)1000.0));
        }
    }
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
        if (!(value <= 1.0f)) // 0x55BB06: `test ah, 0x41` + jnp - only value <= 1 skips, NaN shows the message
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
        if (!(value <= 1.0f)) // 0x55BB06: `test ah, 0x41` + jnp - only value <= 1 skips, NaN shows the message
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
    const x87d A  = (x87d)StatTypesFloat[STAT_RESPECT] * (x87d)0.4f;
    const x87d B  = (x87d)StatTypesInt[STAT_RESPECT_MISSION - FIRST_INT_STAT] * (x87d)1000.0;
    const x87d D  = (x87d)StatTypesInt[STAT_RESPECT_MISSION_TOTAL - FIRST_INT_STAT] < (x87d)1.0 ? (x87d)1.0 : (x87d)StatTypesInt[STAT_RESPECT_MISSION_TOTAL - FIRST_INT_STAT];
    const x87d C  = (B / D) * (x87d)0.36f;
    const x87d S1 = (A + C) + (x87d)StatTypesFloat[STAT_GIRLFRIEND_RESPECT] * (x87d)0.03f;

    x87d territory = (x87d)CGangWars::TerritoryUnderControlPercentage - (x87d)0.2f;
    if (!(territory >= 0.0)) { // exe 0x55BCD6: fcomp + jne keeps the value only when 0.0 <= territory (NaN takes 0.0)
        territory = (x87d)0.0;
    }
    const float S2 = (float)(territory * (x87d)1250.0 * (x87d)0.05f + S1);

    const x87d money = (x87d)CWorld::Players[CWorld::PlayerInFocus].m_nMoney * (x87d)1.0e-7f;
    const float  moneyFactor = 1.0 < money ? 1.0f : (float)money;

    const float muscle  = StatTypesFloat[STAT_MUSCLE];
    const float clothes = StatTypesFloat[STAT_CLOTHES_RESPECT];

    const x87d tagged = (x87d)(CTagManager::GetPercentageTagged() * 10) * (x87d)0.05f; // 0x49CDA0
    const x87d sumExt = tagged + (((x87d)moneyFactor * (x87d)1000.0 * (x87d)0.05f + (x87d)S2) + (x87d)muscle * (x87d)0.03f + (x87d)clothes * (x87d)0.03f);

    float respect = (float)sumExt;
    if (sumExt < 0.0) {
        respect = 0.0f;
    }
    if (CCheat::IsActive(CHEAT_MAX_RESPECT)) {
        respect = 1000.0f;
    }

    if (NOTSA_FTOL(int32, m_RespectLastValue) == NOTSA_FTOL(int32, respect)) {
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

    x87d result;
    if (m_pSexAppealVehicle) {
        const auto& vehPos    = m_pSexAppealVehicle->GetPosition();
        const auto  playerPos = FindPlayerCoors(-1);
        const x87d dx = (x87d)vehPos.x - (x87d)playerPos.x;
        const x87d dy = (x87d)vehPos.y - (x87d)playerPos.y;
        if (std::sqrt(dx * dx + dy * dy) < 35.0) {
            appeal = (float)(m_pSexAppealVehicle->m_fHealth * (x87d)2.0 - (x87d)1000.0);
            // x87: clamp to [0, 1000]
            x87d scaled;
            if (1000.0f < appeal) {
                scaled = (x87d)1000.0;
            } else if (appeal < 0.0f) {
                scaled = (x87d)0.0;
            } else {
                scaled = appeal;
            }
            switch ((int8)CModelInfo::GetVehicleModelInfo(m_pSexAppealVehicle->m_nModelIndex)->m_nVehicleClass) {
            case 1:  // VEHICLE_CLASS_POORFAMILY
            case 5:  // VEHICLE_CLASS_BIG
            case 7:  // VEHICLE_CLASS_MOPED
            case 11: // VEHICLE_CLASS_BICYCLE
                scaled *= (x87d)0.1f;
                break;
            case 2: // VEHICLE_CLASS_RICHFAMILY
            case 3: // VEHICLE_CLASS_EXECUTIVE
            case 9: // VEHICLE_CLASS_LEISUREBOAT
                break;
            case 4:  // VEHICLE_CLASS_WORKER
            case 6:  // VEHICLE_CLASS_TAXI
            case 10: // VEHICLE_CLASS_WORKERBOAT
                scaled *= (x87d)0.3f;
                break;
            default:
                scaled *= (x87d)0.5;
                break;
            }
            result = scaled * (x87d)0.5 + (x87d)base;
        } else {
            result = (x87d)appeal * (x87d)0.5 + (x87d)base;
        }
    } else {
        result = (x87d)appeal * (x87d)0.5 + (x87d)base;
    }

    if (result < 0.0) {
        result = (x87d)0.0;
    } else if (result > 1000.0) {
        result = (x87d)1000.0;
    }
    if (CCheat::IsActive(CHEAT_MAX_SEX_APPEAL)) {
        result = (x87d)1000.0;
    }
    StatTypesFloat[STAT_SEX_APPEAL] = (float)result;

    CheckForStatsMessage(); // tail call
}

// 0x55C180
void CStats::IncrementStat(eStats stat, float value) {
    if (!(value > 0.0f)) { // 0x55C185: NaN returns too
        return;
    }

    const auto idx = static_cast<uint16>(stat);
    if (idx < FIRST_UNUSED_STAT) { // float
        const x87d sum = (x87d)StatTypesFloat[idx] + (x87d)value;
        StatTypesFloat[idx] = (float)sum;
        if (IsStatCapped(stat)) {
            StatTypesFloat[idx] = (float)(sum < 1000.0 ? sum : 1000.0); // NaN / >= 1000 => 1000
        }
        ProcessReactionStatsOnIncrement(stat);
        CheckForStatsMessage();
        return;
    }

    auto& info = CWorld::Players[CWorld::PlayerInFocus];

    if (idx == STAT_CALORIES) {
        const int32 healthDiff = NOTSA_FTOL(int32, (x87d)info.m_nMaxHealth - (x87d)info.m_pPed->m_fHealth); // _ftol: truncated to an integer, BEFORE the nested increment below

        IncrementStat(STAT_RIOT_MISSION_ACCOMPLISHED, value);

        if ((double)value > (double)healthDiff) {
            IncrementStat(STAT_FAT, (float)(((x87d)value - (x87d)healthDiff) * (x87d)0.5));
        }

        ProcessReactionStatsOnIncrement(stat);
        CheckForStatsMessage();
        return;
    }

    if (idx == STAT_RIOT_MISSION_ACCOMPLISHED) { // eating: the "calories" go into the player's health
        // 0x55C283: (int16) m_nNumHoursDidntEat - value * 0.5, clamped to [0, 36] (NaN => 36)
        const float tmp = (float)((x87d)(int16)info.m_nNumHoursDidntEat - (x87d)value * (x87d)0.5);
        x87d hours;
        if (0.0 > (double)tmp) {
            hours = (x87d)0.0;
        } else {
            hours = (x87d)tmp < (x87d)36.0 ? (x87d)tmp : (x87d)36.0;
        }
        info.m_nNumHoursDidntEat = (uint16)(int16)NOTSA_FTOL(int32, hours);

        const int32 healthDiff = NOTSA_FTOL(int32, (x87d)info.m_nMaxHealth - (x87d)info.m_pPed->m_fHealth);
        if (!((double)value < (double)healthDiff)) { // value >= diff (or NaN): only the missing health is added and the hunger is gone
            value = (float)healthDiff;
            info.m_nNumHoursDidntEat = 0;
        }

        info.m_pPed->m_fHealth = (float)((x87d)value + (x87d)info.m_pPed->m_fHealth);
        UpdateStatsAddToHealth((uint32)NOTSA_FTOL(int32, value));
        ProcessReactionStatsOnIncrement(stat);
        CheckForStatsMessage();
        return;
    }

    auto& cell = IntStatCell(idx);
    cell += (int32)(int16)NOTSA_FTOL(int32, value); // movsx ax: the increment is a 16 bit value
    if (IsStatCapped(stat)) {
        cell = NOTSA_FTOL(int32, (double)cell < 1000.0 ? (double)cell : 1000.0);
    }
    ProcessReactionStatsOnIncrement(stat);
    CheckForStatsMessage();
}

// 0x55C470
void CStats::UpdateFatAndMuscleStats(uint32 value) {
    if ((x87d)StatReactionValue[STAT_TIMELIMIT_FAT_ADJUST] * (x87d)1000.0 < (double)m_FatCounter) {
        m_FatCounter = 0;

        if (StatTypesFloat[STAT_FAT] > 0.0f) {
            if (StatReactionValue[STAT_DEC_FAT] > 0.0f) {
                const x87d fat = (x87d)StatTypesFloat[STAT_FAT] - (x87d)StatReactionValue[STAT_DEC_FAT];
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
                const x87d muscle = (x87d)StatTypesFloat[STAT_MUSCLE] - (x87d)StatReactionValue[STAT_DEC_BODY_MUSCLE];
                StatTypesFloat[STAT_MUSCLE] = muscle > 0.0 ? (float)muscle : 0.0f;
                CheckForStatsMessage();
            }
            m_FatMuscleMessageState = 3;
            DisplayScriptStatUpdateMessage(STAT_UPDATE_DECREASE, STAT_MUSCLE, StatReactionValue[STAT_DEC_BODY_MUSCLE]);
        }
    } else {
        m_FatCounter += (uint32)TimeStepInMS() * value / 10;
    }

    if ((x87d)StatReactionValue[STAT_TIMELIMIT_MAX_HEALTH] * (x87d)1000.0 < (double)m_MaxHealthCounter) {
        IncrementStat(STAT_MAX_HEALTH, StatReactionValue[STAT_INC_MAX_HEALTH]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_MAX_HEALTH, StatReactionValue[STAT_INC_MAX_HEALTH]);
        m_MaxHealthCounter = 0;
        return;
    }
    m_MaxHealthCounter += TimeStepInMS();
}

// 0x55C660
void CStats::UpdateStatsWhenSprinting() {
    UpdateFatAndMuscleStats(NOTSA_FTOL(uint32, StatReactionValue[STAT_EXERCISE_RATE_SPRINT]));
    if (!((double)(StatReactionValue[STAT_TIMELIMIT_SPRINT_STAMINA] * 1000.0f) < (double)m_SprintStaminaCounter)) { // fcompp + jp: only 'limit < counter' resets (equal and NaN add)
        m_SprintStaminaCounter += TimeStepInMS();
    } else {
        m_SprintStaminaCounter = 0;
        IncrementStat(STAT_STAMINA, StatReactionValue[STAT_INC_SPRINT_STAMINA]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_STAMINA, StatReactionValue[STAT_INC_SPRINT_STAMINA]);
    }
}

// 0x55C6F0
void CStats::UpdateStatsWhenRunning() {
    UpdateFatAndMuscleStats(NOTSA_FTOL(uint32, StatReactionValue[STAT_EXERCISE_RATE_RUN]));
    if (!((double)(StatReactionValue[STAT_TIMELIMIT_RUNNING] * 1000.0f) < (double)m_RunningCounter)) { // fcompp + jp: only 'limit < counter' resets (equal and NaN add)
        m_RunningCounter += TimeStepInMS();
    } else {
        m_RunningCounter = 0;
        IncrementStat(STAT_STAMINA, StatReactionValue[STAT_INC_RUNNING]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_STAMINA, StatReactionValue[STAT_INC_RUNNING]);
    }
}

// 0x55C780
void CStats::UpdateStatsWhenCycling(bool arg0, CBmx* bmx) {
    if (std::abs(bmx->m_GasPedal) > 0.0f || arg0) {
        UpdateFatAndMuscleStats(NOTSA_FTOL(uint32, StatReactionValue[arg0 ? STAT_EXERCISE_RATE_CYCLE_SPRINT : STAT_EXERCISE_RATE_CYCLE]));

        if ((x87d)StatReactionValue[STAT_TIMELIMIT_CYCLE_STAMINA] * (x87d)1000.0 < (double)m_CycleStaminaCounter) {
            m_CycleStaminaCounter = 0;
            IncrementStat(STAT_STAMINA, StatReactionValue[STAT_INC_CYCLE_STAMINA]);
            DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_STAMINA, StatReactionValue[STAT_INC_CYCLE_STAMINA]);
        } else {
            m_CycleStaminaCounter += TimeStepInMS();
        }
    }

    if ((x87d)StatReactionValue[STAT_TIMELIMIT_CYCLE_SKILL] * (x87d)1000.0 < (double)m_CycleSkillCounter) {
        m_CycleSkillCounter = 0;
        IncrementStat(STAT_CYCLING_SKILL, StatReactionValue[STAT_INC_CYCLE_SKILL]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_CYCLING_SKILL, StatReactionValue[STAT_INC_CYCLE_SKILL]);
        return;
    }

    // x87: (x*x + y*y) + z*z in extended precision
    const x87d speedSq = (x87d)bmx->m_vecMoveSpeed.x * bmx->m_vecMoveSpeed.x
                         + (x87d)bmx->m_vecMoveSpeed.y * bmx->m_vecMoveSpeed.y
                         + (x87d)bmx->m_vecMoveSpeed.z * bmx->m_vecMoveSpeed.z;

    float rate;
    if (bmx->m_nNoOfContactWheels < 2 && (x87d)0.05f * (x87d)0.05f < speedSq) {
        rate = 3.0f;
    } else if ((x87d)0.2f * (x87d)0.2f < speedSq) {
        rate = arg0 ? 1.5f : 1.0f;
    } else {
        return;
    }
    if (!(rate > 0.0f)) {
        return;
    }

    m_CycleSkillCounter += NOTSA_FTOL(int32, std::ceil((x87d)(uint32)TimeStepInMS() * (x87d)rate));
}

// 0x55C990
void CStats::UpdateStatsWhenSwimming(bool arg0, bool arg1) {
    UpdateFatAndMuscleStats(NOTSA_FTOL(uint32, StatReactionValue[(arg0 || arg1) ? STAT_EXERCISE_RATE_SWIM_SPRINT : STAT_EXERCISE_RATE_SWIM]));

    if ((x87d)StatReactionValue[STAT_TIMELIMIT_SWIM_STAMINA] * (x87d)1000.0 < (double)m_SwimStaminaCounter) {
        m_SwimStaminaCounter = 0;
        IncrementStat(STAT_STAMINA, StatReactionValue[STAT_INC_SWIM_STAMINA]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_STAMINA, StatReactionValue[STAT_INC_SWIM_STAMINA]);
    } else {
        m_SwimStaminaCounter += TimeStepInMS();
    }

    if (arg0) {
        if ((x87d)StatReactionValue[STAT_TIMELIMIT_BREATH_UNDERWATER] * (x87d)1000.0 < (double)m_SwimUnderWaterCounter) {
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
    if ((x87d)StatReactionValue[STAT_TIMELIMIT_DRIVING_SKILL] * (x87d)1000.0 < (double)counter) {
        m_DrivingCounter = 0;
        IncrementStat(STAT_DRIVING_SKILL, StatReactionValue[STAT_INC_DRIVING_SKILL]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_DRIVING_SKILL, StatReactionValue[STAT_INC_DRIVING_SKILL]);
        return;
    }

    const x87d speed = std::sqrt(
          (x87d)vehicle->m_vecMoveSpeed.x * vehicle->m_vecMoveSpeed.x
        + (x87d)vehicle->m_vecMoveSpeed.y * vehicle->m_vecMoveSpeed.y
        + (x87d)vehicle->m_vecMoveSpeed.z * vehicle->m_vecMoveSpeed.z
    );

    float rate;
    if (speed > (double)0.8f || (automobile && automobile->m_nNumContactWheels == 0)) {
        rate = 1.5f;
    } else if (speed > (double)0.2f) {
        rate = 0.5f;
    } else {
        return;
    }
    m_DrivingCounter = NOTSA_FTOL(int32, (x87d)(uint32)TimeStepInMS() * (x87d)rate + (x87d)counter);
}

// 0x55CC00
void CStats::UpdateStatsWhenFlying(CVehicle* vehicle) {
    if (vehicle->m_nVehicleType != VEHICLE_TYPE_AUTOMOBILE) {
        return;
    }

    const float counter = (float)m_FlyingCounter; // x87: spilled to a float temp
    if ((x87d)StatReactionValue[STAT_TIMELIMIT_FLYING_SKILL] * (x87d)1000.0 < (double)counter) {
        m_FlyingCounter = 0;
        IncrementStat(STAT_FLYING_SKILL, StatReactionValue[STAT_INC_FLYING_SKILL]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_FLYING_SKILL, StatReactionValue[STAT_INC_FLYING_SKILL]);
        return;
    }

    if (vehicle->AsAutomobile()->m_nNumContactWheels) {
        return;
    }

    const x87d speed = std::sqrt(
          (x87d)vehicle->m_vecMoveSpeed.x * vehicle->m_vecMoveSpeed.x
        + (x87d)vehicle->m_vecMoveSpeed.y * vehicle->m_vecMoveSpeed.y
        + (x87d)vehicle->m_vecMoveSpeed.z * vehicle->m_vecMoveSpeed.z
    );

    float rate;
    if (speed > (double)1.3f || vehicle->GetMatrix().GetUp().z < 0.0f) {
        rate = 1.5f;
    } else if (speed > (double)0.5f) {
        rate = 0.5f;
    } else {
        return;
    }
    m_FlyingCounter = NOTSA_FTOL(int32, (x87d)(uint32)TimeStepInMS() * (x87d)rate + (x87d)counter);
}

// 0x55CD60
void CStats::UpdateStatsWhenOnMotorBike(CBike* bike) {
    const float bikeCounter = (float)m_BikeCounter; // x87: FILD (+2^32 when negative), spilled to a float temp
    if ((x87d)StatReactionValue[STAT_TIMELIMIT_MOTORBIKE_SKILL] * (x87d)1000.0 < (double)bikeCounter) { // fcomp + jp: only 'limit < counter' levels up (equal and NaN go on)
        m_BikeCounter = 0;
        IncrementStat(STAT_BIKE_SKILL, StatReactionValue[STAT_INC_MOTORBIKE_SKILL]);
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_BIKE_SKILL, StatReactionValue[STAT_INC_MOTORBIKE_SKILL]);
        return;
    }

    // exe 0x55CDC8-0x55CEA4: x87 extended sqrt((x*x + y*y) + z*z), the compares are strict `>` (NaN fails them)
    const x87d speed = std::sqrt(
          (x87d)bike->m_vecMoveSpeed.x * bike->m_vecMoveSpeed.x
        + (x87d)bike->m_vecMoveSpeed.y * bike->m_vecMoveSpeed.y
        + (x87d)bike->m_vecMoveSpeed.z * bike->m_vecMoveSpeed.z
    );

    float rate;
    if (speed > (double)0.6f || (bike->m_nNoOfContactWheels < 3u && speed > (double)0.1f)) {
        rate = 1.5f;
    } else if (speed > (double)0.2f) {
        rate = 0.5f;
    } else {
        return;
    }
    m_BikeCounter = NOTSA_FTOL(int32, (x87d)(uint32)TimeStepInMS() * (x87d)rate + (x87d)bikeCounter);
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

    const x87d counter = (x87d)m_WeaponCounter;
    if (counter > (double)StatReactionValue[STAT_TIMELIMIT_PISTOL_SKILL + reactId]) {
        DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, stat, (float)(counter * (x87d)increment));
        m_WeaponCounter = 0;
        return;
    }
    m_WeaponCounter++;
}

// 0x55CFA0
void CStats::UpdateStatsWhenFighting() {
    UpdateFatAndMuscleStats(NOTSA_FTOL(uint32, StatReactionValue[STAT_EXERCISE_RATE_FIGHT]));
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
    if (!(value >= 0.0f)) { // 0x55D09F: `test ah, 1` after the compare: value < 0 or NaN
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
    PopulateFavoriteRadioStationList(); // 0x5D3B9E: the listen times are refreshed from the audio engine right before they are saved
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
