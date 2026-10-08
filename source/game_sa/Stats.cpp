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
#include "Cheat.h"
#include "CutsceneMgr.h"
#include "GameLogic.h"
#include "GangWars.h"
#include "Localisation.h"
#include "TagManager.h"
#include "WeaponInfo.h"
#include "Models/ModelInfo.h"
#include "Models/VehicleModelInfo.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Bmx.h"

namespace {
//! x87: `CTimer::ms_fTimeStep * 0.02f * 1000.0f` kept in extended precision, then truncated by _ftol (0x821B40)
uint32 TimeStepInMS() {
    return (uint32)(int32)((double)CTimer::ms_fTimeStep * (double)0.02f * 1000.0);
}
}

void CStats::InjectHooks() {
    RH_ScopedClass(CStats);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x55C0C0);
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
    return plugin::CallAndReturn<bool, 0x558EC0>();
}

// 0x558FA0
eRadioID CStats::FindMostFavoriteRadioStation() {
    return plugin::CallAndReturn<eRadioID, 0x558FA0>();
}

// 0x559010
int32 CStats::FindLeastFavoriteRadioStation() {
    return plugin::CallAndReturn<int32, 0x559010>();
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
    plugin::Call<0x719240, GxtChar*>(gGxtString); // CFont::FilterOutTokensFromString (not reversed yet)
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
    return plugin::CallAndReturn<float, 0x559AF0, eStatModAbilities>(statMod);
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
GxtChar* CStats::FindCriminalRatingString() {
    return plugin::CallAndReturn<GxtChar*, 0x55A210>();
}

// 0x55A780
int32 CStats::ConstructStatLine(int32 arg0, uint8 arg1) {
    return plugin::CallAndReturn<int32, 0x55A780, int32, uint8>(arg0, arg1);
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
