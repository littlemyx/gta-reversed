#include "StdInc.h"

#include "AERadioTrackManager.h"
#include "RadioStreamsPC.h"

#include "AEAudioHardware.h"
#include "AEUserRadioTrackManager.h"
#include "AEAudioUtility.h"
#include "AEAudioEnvironment.h"

auto& AERadioTrackManager = StaticRef<CAERadioTrackManager>(0x8CB6F8);

namespace {
//! Value of `tRadioSoundRange::Start` that means "there are no sounds of this kind for this station"
constexpr int32 RADIO_SOUND_NONE = 0x782;

// NOTSA: Names of all tables below are made up (the original has none).
// Tables indexed by station are indexed by `eRadioID`, there's no data for `RADIO_OFF`.

//! Range of all advert sounds
auto& s_AdvertRange = StaticRef<tRadioSoundRange>(0x8C8B88);

//! "Special" (story/stats related) DJ banter, used when `m_nSpecialDJBanterPending == 0`. Only `Start` is used.
auto& s_DJBanterSpecial0 = StaticRef<tRadioSoundRange[12]>(0x8C8BF0);

//! "Special" DJ banter, used when `m_nSpecialDJBanterPending == 1`. Indexed by `m_nSpecialDJBanterIndex` (0 => `Start`, 1 => `End`)
auto& s_DJBanterSpecial1 = StaticRef<tRadioSoundRange[12]>(0x8C8C50);

//! Default DJ banter
auto& s_DJBanterGeneral = StaticRef<tRadioSoundRange[12]>(0x8C8CB0);

//! DJ banter for the evening (18:00 - 20:59)
auto& s_DJBanterEvening = StaticRef<tRadioSoundRange[12]>(0x8C8D70);

//! DJ banter for the morning (6:00 - 8:59)
auto& s_DJBanterMorning = StaticRef<tRadioSoundRange[12]>(0x8C8DD0);

//! DJ banter for the night (22:00 - 2:59), also used by AA when there are riots
auto& s_DJBanterNight = StaticRef<tRadioSoundRange[12]>(0x8C8E30);

//! DJ banter for rainy weather
auto& s_DJBanterRain = StaticRef<tRadioSoundRange[12]>(0x8C8E90);

//! DJ banter for sunny weather (never used because of a bug in `ChooseDJBanterIndex`)
auto& s_DJBanterSunny = StaticRef<tRadioSoundRange[12]>(0x8C8EF0);

//! DJ banter for foggy weather
auto& s_DJBanterFog = StaticRef<tRadioSoundRange[12]>(0x8C8F50);

//! Station idents
auto& s_IdentRange = StaticRef<tRadioSoundRange[12]>(0x8C8FB0);

//! Number of music tracks of each station
auto& s_NumTracks = StaticRef<int32[12]>(0x8C9010);

//! Sound ID of a track: [station][track index]
auto& s_TrackSoundID = StaticRef<int32[12][31]>(0x8C9040);

//! Range of the sound IDs of the intro of a track: [station][track index]
auto& s_TrackIntroRange = StaticRef<tRadioSoundRange[12][31]>(0x8C9610);

//! Range of the sound IDs of the outro of a track: [station][track index]
auto& s_TrackOutroRange = StaticRef<tRadioSoundRange[12][31]>(0x8CA1B0);

//! Length (in ms) of each talk radio show
auto& s_TalkShowLengthMs = StaticRef<int32[32]>(0x8CAD50);

//! Adverts that are (probably) not allowed on the station, `-1` is unused: [station][23] (the last row is for `RADIO_USER_TRACKS`, all zeros)
auto& s_StationBlockedAdverts = StaticRef<int32[13][23]>(0x8CADD0);

//! "Special" DJ banter, used when `m_nSpecialDJBanterPending == 2`. Indexed by `m_nSpecialDJBanterIndex`: [station][22]
auto& s_DJBanterSpecial2 = StaticRef<int32[12][22]>(0x8CB280);

//! Play time that was set in the previous call to `CAERadioTrackManager::Service`
auto& s_PrevServicePlayTime = StaticRef<int32>(0x8CBA68);

//! Camera's float at 0xB6F14C (offset 0x124 from `TheCamera`, probably `m_fCameraAverageSpeed`), affects how fast the radio is retuned.
auto& s_CameraRetuneFactor = StaticRef<float>(0xB6F14C);

//! Checks if `value` is one of the first `count` elements of `history`
template<typename T, size_t N, typename V>
bool IsInHistory(const std::array<T, N>& history, int32 count, V value) {
    for (auto i = 0; i < count; i++) {
        if (history[i] == value) {
            return true;
        }
    }
    return false;
}
} // namespace

void CAERadioTrackManager::InjectHooks() {
    RH_ScopedClass(CAERadioTrackManager);
    RH_ScopedCategory("Audio/Managers");

    RH_ScopedInstall(Load, 0x5D40E0);
    RH_ScopedInstall(Save, 0x5D3EE0);
    RH_ScopedInstall(Initialise, 0x5B9390);
    RH_ScopedInstall(Service, 0x4EB9A0);
    RH_ScopedInstall(DisplayRadioStationName, 0x4E9E50);
    RH_ScopedInstall(CheckForStationRetune, 0x4EB660);
    RH_ScopedInstall(CheckForPause, 0x4EA590);
    RH_ScopedInstall(IsVehicleRadioActive, 0x4E9800);
    RH_ScopedInstall(AddDJBanterIndexToHistory, 0x4E97B0);
    RH_ScopedInstall(AddAdvertIndexToHistory, 0x4E9760);
    RH_ScopedInstall(AddIdentIndexToHistory, 0x4E9720);
    RH_ScopedInstall(AddMusicTrackIndexToHistory, 0x4E96C0);
    RH_ScopedOverloadedInstall(StartRadio, "manual", 0x4EB3C0, void (CAERadioTrackManager::*)(eRadioID, eBassSetting, float, bool));
    RH_ScopedOverloadedInstall(StartRadio, "with-settings", 0x4EB550, void (CAERadioTrackManager::*)(const tVehicleAudioSettings&));
    RH_ScopedInstall(CheckForStationRetuneDuringPause, 0x4EB890);
    RH_ScopedInstall(TrackRadioStation, 0x4EAC30);
    RH_ScopedInstall(ChooseTracksForStation, 0x4EB180);
    RH_ScopedInstall(CheckForTrackConcatenation, 0x4EA930);
    RH_ScopedInstall(QueueUpTracksForStation, 0x4EA670);
    RH_ScopedInstall(ChooseDJBanterIndex, 0x4EA2D0);
    RH_ScopedInstall(ChooseDJBanterIndexFromList, 0x4E95E0);
    RH_ScopedInstall(ChooseAdvertIndex, 0x4E9570);
    RH_ScopedInstall(ChooseIdentIndex, 0x4E94C0);
    RH_ScopedInstall(ChooseMusicTrackIndex, 0x4EA270);
    RH_ScopedInstall(ChooseTalkRadioShow, 0x4E8E40);
    RH_ScopedInstall(CheckForMissionStatsChanges, 0x4E8410);
    RH_ScopedInstall(StartTrackPlayback, 0x4EA640);
    RH_ScopedInstall(UpdateRadioVolumes, 0x4EA010);
    RH_ScopedInstall(PlayRadioAnnouncement, 0x4E8400);
    RH_ScopedInstall(GetCurrentRadioStationID, 0x4E83F0);
    RH_ScopedInstall(GetRadioStationListenTimes, 0x4E83E0);
    RH_ScopedInstall(GetRadioStationName, 0x4E9E10);
    RH_ScopedInstall(GetRadioStationNameKey, 0x4E8380);
    RH_ScopedInstall(HasRadioRetuneJustStarted, 0x4E8370);
    RH_ScopedInstall(StopRadio, 0x4E9820);
    RH_ScopedInstall(IsRadioOn, 0x4E8350, { .Reversed = true });
    RH_ScopedInstall(InitialiseRadioStationID, 0x4E8330);
    RH_ScopedInstall(SetBassEnhanceOnOff, 0x4E9DB0);
    RH_ScopedInstall(SetBassSetting, 0x4E82F0);
    RH_ScopedInstall(SetRadioAutoRetuneOnOff, 0x4E82E0);
    RH_ScopedInstall(RetuneRadio, 0x4E8290, { .Reversed = true });
    RH_ScopedInstall(ResetStatistics, 0x4E8200);
    RH_ScopedInstall(Reset, 0x4E7F80, { .Reversed = true });
}

// Code from 0x5B9390
CAERadioTrackManager::CAERadioTrackManager(int32 hwClientHandle) :
    m_HwClientHandle{ hwClientHandle },
    m_nUserTrackPlayMode{ AEUserRadioTrackManager.GetUserTrackPlayMode() }
{
    // All constant value inits are done using member init lists

    rng::copy(CStats::GetFullFavoriteRadioStationList(), m_aListenTimes.begin());

    for (auto i = 0u; i < RADIO_COUNT; i++) {
        m_nMusicTrackIndexHistory[i].Reset();
        m_nDJBanterIndexHistory[i].Reset();
        m_nAdvertIndexHistory[i].Reset();
        m_nIdentIndexHistory[i].Reset();
    }

    // [1st radio, off]
    m_RequestedSettings = m_ActiveSettings = tRadioSettings{CAEAudioUtility::GetRandomRadioStation()};
}

// 0x5B9390
bool CAERadioTrackManager::Initialise(int32 channelId) {
    *this = CAERadioTrackManager{};
    return true;
}

// 0x4E8330
void CAERadioTrackManager::InitialiseRadioStationID(eRadioID id) {
    m_RequestedSettings.StationID = m_ActiveSettings.StationID = id;
}

// 0x4E7F80
void CAERadioTrackManager::Reset() {
    m_bInitialised = false;
    m_bDisplayStationName = false;
    rng::copy(CStats::GetFullFavoriteRadioStationList(), m_aListenTimes.begin());

    rng::for_each(m_nDJBanterIndexHistory, &DJBanterIndexHistory::Reset);
    rng::for_each(m_nAdvertIndexHistory, &AdvertIndexHistory::Reset);
    rng::for_each(m_nIdentIndexHistory, &IdentIndexHistory::Reset);
    rng::for_each(m_nMusicTrackIndexHistory, &MusicTrackHistory::Reset);
    rng::for_each(m_aRadioState, [](auto& s) { s.Reset(); });

    m_RequestedSettings = m_ActiveSettings = tRadioSettings{CAEAudioUtility::GetRandomRadioStation()};
    m_nStationsListed = m_nStationsListDown = 0;
    m_nTimeRadioStationRetuned = m_nTimeToDisplayRadioName = 0;
    m_prev = field_60 = 0;
    m_nRetuneStartedTime = 0;
    m_bEnabledInPauseMode = false;
    m_nSavedGameClockDays = m_nSavedGameClockHours = -1;
    m_bRadioAutoSelect = m_bBassEnhance = true;
    m_nSavedRadioStationId = m_iRadioStationMenuRequest = m_iRadioStationScriptRequest = RADIO_INVALID;
    m_nSpecialDJBanterPending = 3; // todo: enum
    m_nSpecialDJBanterIndex = -1;
    m_bPauseMode = m_bRetuneJustStarted = false;
    m_f80 = m_f84 = 0.0f;
    ResetStatistics();
}

// 0x4E8200
void CAERadioTrackManager::ResetStatistics() {
    m_nStatsCitiesPassed = 0;
    m_nStatsLastHitGameClockDays = -1;
    m_nStatsLastHitGameClockHours = -1;
    m_nStatsLastHitTimeOutHours = -1;
    m_nStatsPassedCasino3 = false;
    m_nStatsPassedCasino6 = false;
    m_nStatsPassedCasino10 = false;
    m_nStatsPassedCat1 = false;
    m_nStatsPassedDesert1 = false;
    m_nStatsPassedDesert3 = false;
    m_nStatsPassedDesert5 = false;
    m_nStatsPassedDesert8 = false;
    m_nStatsPassedDesert10 = false;
    m_nStatsPassedFarlie3 = false;
    m_nStatsPassedLAFin2 = false;
    m_nStatsPassedMansion2 = false;
    m_nStatsPassedRyder2 = false;
    m_nStatsPassedRiot1 = false;
    m_nStatsPassedSCrash1 = false;
    m_nStatsPassedStrap4 = false;
    m_nStatsPassedSweet2 = false;
    m_nStatsPassedTruth2 = false;
    m_nStatsPassedVCrash2 = false;
    m_nStatsStartedBadlands = false;
    m_nStatsStartedCat2 = false;
    m_nStatsStartedCrash1 = false;
}

// 0x4E8350
bool CAERadioTrackManager::IsRadioOn() const {
    return m_nMode != eRadioTrackMode::RADIO_STOPPED || m_bInitialised || m_nStationsListed || m_nStationsListDown;
}

// 0x4E8370
bool CAERadioTrackManager::HasRadioRetuneJustStarted() const {
    return m_bRetuneJustStarted;
}

// 0x4E83E0
int32* CAERadioTrackManager::GetRadioStationListenTimes() {
    return m_aListenTimes.data();
}

// 0x4E83F0
eRadioID CAERadioTrackManager::GetCurrentRadioStationID() const {
    return m_RequestedSettings.StationID == RADIO_INVALID ? RADIO_OFF : m_RequestedSettings.StationID;
}

// 0x4E82E0
void CAERadioTrackManager::SetRadioAutoRetuneOnOff(bool enable) {
    m_bRadioAutoSelect = enable;
}

// 0x4E82F0
void CAERadioTrackManager::SetBassSetting(eBassSetting bassSetting, float bassGrain) {
    m_RequestedSettings.BassGain = m_ActiveSettings.BassGain = bassGrain;
    m_RequestedSettings.BassSetting = m_ActiveSettings.BassSetting = bassSetting;
    AEAudioHardware.SetBassSetting(m_bBassEnhance ? bassSetting : eBassSetting::NORMAL, bassGrain);
}

// 0x4E9DB0
void CAERadioTrackManager::SetBassEnhanceOnOff(bool enable) {
    m_bBassEnhance = enable;
    if (m_nMode == eRadioTrackMode::RADIO_PLAYING) {
        m_RequestedSettings.BassSetting = m_ActiveSettings.BassSetting;
        m_RequestedSettings.BassGain = m_ActiveSettings.BassGain;
        if (enable) {
            AEAudioHardware.SetBassSetting(m_ActiveSettings.BassSetting, m_ActiveSettings.BassGain);
        } else {
            AEAudioHardware.SetBassSetting(eBassSetting::NORMAL, m_ActiveSettings.BassGain);
        }
    }
}

// 0x4E8290
void CAERadioTrackManager::RetuneRadio(eRadioID id) {
    const auto retunedStation = [id] {
        if (id == RADIO_USER_TRACKS && !AEUserRadioTrackManager.m_nUserTracksCount) {
            return RADIO_OFF;
        } else {
            return id;
        }
    }();

    if (CTimer::GetIsPaused()) {
        m_iRadioStationMenuRequest = retunedStation;
        m_nRetuneStartedTime = CTimer::GetTimeInMSPauseMode();
    } else {
        m_iRadioStationScriptRequest = retunedStation;
    }
}

// 0x4E9E50
void CAERadioTrackManager::DisplayRadioStationName() {
    if (CTimer::GetIsPaused())
        return;

    if (TheCamera.m_bWideScreenOn)
        return;

    if (!FindPlayerVehicle())
        return;

    if (CReplay::Mode == MODE_PLAYBACK)
        return;

    if (m_bDisplayStationName && IsVehicleRadioActive()) {
        m_nTimeToDisplayRadioName = CTimer::GetTimeInMS() + 2500;
        m_bDisplayStationName = false;
    }

    if (CTimer::GetTimeInMS() < m_nTimeToDisplayRadioName) {
        int station = m_nStationsListed + m_RequestedSettings.StationID;
        if (station) {
            if (station >= RADIO_COUNT) {
                station -= RADIO_COUNT - 1;
            } else if (station <= 0) {
                station += RADIO_COUNT - 1;
            }

            CFont::SetFontStyle(eFontStyle::FONT_MENU);
            CFont::SetJustify(false);
            CFont::SetBackground(false, false);
            CFont::SetScale(SCREEN_SCALE_X(0.6f), SCREEN_SCALE_Y(0.9f));
            CFont::SetProportional(true);
            CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
            CFont::SetRightJustifyWrap(0.0f);
            CFont::SetEdge(1);
            CFont::SetDropColor(CRGBA(0, 0, 0, 255));
            CFont::SetColor(HudColour.GetRGB(m_nStationsListed || m_nStationsListDown ? HUD_COLOUR_DARK_GRAY : HUD_COLOUR_GOLD));
            CFont::PrintString(SCREEN_WIDTH / 2, SCREEN_SCALE_Y(22.0f), GetRadioStationName((eRadioID)station));
            CFont::DrawFonts();
        }
    }
}

// 0x4E9E10
const GxtChar* CAERadioTrackManager::GetRadioStationName(eRadioID id) {
    if (id <= 0) {
        NOTSA_UNREACHABLE();
        return nullptr;
    }

    char key[8];
    GetRadioStationNameKey(id, key);
    return TheText.Get(key);
}

// 0x4E8380
void CAERadioTrackManager::GetRadioStationNameKey(eRadioID id, char* outStr) {
    switch (id) {
    case RADIO_OFF:
        *std::format_to_n(outStr, 7u, "FEA_NON").out = '\0';
        break;
    case RADIO_USER_TRACKS:
        *std::format_to_n(outStr, 7u, "FEA_MP3").out = '\0';
        break;
    default:
        assert(0 <= id && id < RADIO_USER_TRACKS);
        *std::format_to_n(outStr, 7u, "FEA_R{:d}", (int32)id - 1).out = '\0';
        break;
    }
}

// 0x4E9800
bool CAERadioTrackManager::IsVehicleRadioActive() {
    if (const auto opts = CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio()) {
        switch (opts->RadioType) {
        case AE_RT_CIVILIAN:
        case AE_RT_EMERGENCY:
        case AE_RT_UNKNOWN:
            return true;
        default:
            break;
        }
    }
    return false;
}

// 0x4E8410
void CAERadioTrackManager::CheckForMissionStatsChanges() {
    if (m_nSpecialDJBanterPending != 3) {
        int32 days = CClock::GetGameClockDays() - m_nStatsLastHitGameClockDays;
        if (days < 0) {
            auto month = CClock::GetGameClockMonth() - 1;
            if (month < 0) {
                month += 12;
            }
            days += CClock::daysInMonth[month];
        }
        if (CClock::GetGameClockHours() + 24 * days - m_nStatsLastHitGameClockHours >= m_nStatsLastHitTimeOutHours) {
            m_nSpecialDJBanterPending = 3;
        }
    }

    const auto statsCitiesPassed = CStats::GetStatValue<uint8>(STAT_CITY_UNLOCKED);
    if (m_nStatsCitiesPassed < statsCitiesPassed) {
        m_nStatsCitiesPassed = statsCitiesPassed;
        if (statsCitiesPassed == 1 || statsCitiesPassed == 2) {
            m_nStatsLastHitGameClockDays = CClock::GetGameClockDays();
            m_nStatsLastHitGameClockHours = CClock::GetGameClockHours();
            m_nStatsLastHitTimeOutHours = 24;
            m_nSpecialDJBanterPending = 1;
            m_nSpecialDJBanterIndex = m_nStatsCitiesPassed - 1;
        }
    }

    const auto Update = [](uint8& inputStat, const eStats stat, const auto specialDJBanterIndex) {
        const auto statValue = CStats::GetStatValue<uint8>(stat);
        if (inputStat < statValue) {
            inputStat = statValue;
            if (inputStat == 1) {
                m_nStatsLastHitGameClockDays = CClock::GetGameClockDays();
                m_nStatsLastHitGameClockHours = CClock::GetGameClockHours();
                m_nStatsLastHitTimeOutHours = 24 * 7;
                m_nSpecialDJBanterPending = 2;
                m_nSpecialDJBanterIndex = specialDJBanterIndex;
            }
        }
    };

    Update(m_nStatsPassedCasino3, STAT_LEAST_FAVORITE_RADIO_STATION, 0);
    Update(m_nStatsPassedCasino6, STAT_CURRENT_WEAPON_SKILL, 1);
    Update(m_nStatsPassedCasino10, STAT_WEAPON_SKILL_LEVELS, 2);
    Update(m_nStatsPassedCat1, STAT_LOCAL_LIQUOR_STORE_MISSION_ACCOMPLISHED, 3);
    Update(m_nStatsPassedDesert1, STAT_PLAYING_TIME, 4);
    Update(m_nStatsPassedDesert3, STAT_PILOT_RANKING, 5);
    Update(m_nStatsPassedDesert5, STAT_STRONGEST_GANG, 6);
    Update(m_nStatsPassedDesert8, STAT_2ND_STRONGEST_GANG, 7);
    Update(m_nStatsPassedDesert10, STAT_3RD_STRONGEST_GANG, 8);
    Update(m_nStatsPassedFarlie3, STAT_MIKE_TORENO_MISSION_ACCOMPLISHED, 9);
    Update(m_nStatsPassedLAFin2, STAT_LEAST_FAVORITE_GANG, 10);
    Update(m_nStatsPassedMansion2, STAT_A_HOME_IN_THE_HILLS_MISSION_ACCOMPLISHED, 11);
    Update(m_nStatsPassedRyder2, STAT_RYDERS_MISSION_ROBBING_UNCLE_SAM_ACCOMPLISHED, 12);
    Update(m_nStatsPassedRiot1, STAT_RIOT_MISSION_ACCOMPLISHED, 13);
    Update(m_nStatsPassedSCrash1, STAT_GANG_STRENGTH, 14);
    Update(m_nStatsPassedStrap4, STAT_TERRITORY_UNDER_CONTROL, 15);
    Update(m_nStatsPassedSweet2, STAT_DRIVE_THRU_MISSION_ACCOMPLISHED, 16);
    Update(m_nStatsPassedTruth2, STAT_ARE_YOU_GOING_TO_SAN_FIERRO_MISSION_ACCOMPLISHED, 17);
    Update(m_nStatsPassedVCrash2, STAT_HIGH_NOON_MISSION_ACCOMPLISHED, 18);
    Update(m_nStatsStartedBadlands, STAT_THE_GREEN_SABRE_MISSION_ACCOMPLISHED, 19);
    Update(m_nStatsStartedCat2, STAT_MAYBE_CATALINA_MEETING, 20);
    Update(m_nStatsStartedCrash1, STAT_MAYBE_WU_ZI_MEETING, 21);
}

// 0x4EA930
void CAERadioTrackManager::CheckForTrackConcatenation() {
    auto& as = m_ActiveSettings;
    int8  trackCount = 1;

    // Handle the user changing the play mode (sequential/shuffle) of the user tracks
    if (as.StationID == RADIO_USER_TRACKS) {
        const auto prevPlayMode = m_nUserTrackPlayMode;
        if (prevPlayMode != AEUserRadioTrackManager.GetUserTrackPlayMode()) {
            if ((prevPlayMode == 2 || AEUserRadioTrackManager.GetUserTrackPlayMode() == 2) && as.PlayTime != -4) {
                AEUserRadioTrackManager.SetUserTrackIndex(as.TrackQueue[0]);
                as.TrackQueue[1]   = AEUserRadioTrackManager.SelectUserTrackIndex();
                as.TrackTypes[1]   = TYPE_USER_TRACK;
                as.TrackIndices[1] = (int8)as.TrackQueue[1];
                trackCount = 2;
                AEAudioHardware.PlayTrack(
                    as.TrackQueue[0],
                    as.TrackQueue[1],
                    0,
                    as.TrackFlags,
                    as.TrackTypes[0] == TYPE_USER_TRACK,
                    as.TrackTypes[1] == TYPE_USER_TRACK
                );
            }
            m_nUserTrackPlayMode = AEUserRadioTrackManager.GetUserTrackPlayMode();
        }
    }

    // Has the next track started playing?
    const auto nextTrackID = as.TrackQueue[1];
    if (AEAudioHardware.GetActiveTrackID() != nextTrackID || nextTrackID < 0) {
        return;
    }

    as.SwitchToNextTrack();

    // Need to queue up more tracks?
    if (as.TrackQueue[1] == -1) {
        if (as.StationID == RADIO_USER_TRACKS) {
            if (!FrontEndMenuManager.m_RadioMode && CAEAudioUtility::ResolveProbability(0.17f)) {
                as.TrackQueue[trackCount] = ChooseAdvertIndex(RADIO_USER_TRACKS);
                as.TrackTypes[trackCount] = TYPE_ADVERT;
                trackCount++;
            }
            as.TrackQueue[trackCount]   = AEUserRadioTrackManager.SelectUserTrackIndex();
            as.TrackTypes[trackCount]   = TYPE_USER_TRACK;
            as.TrackIndices[trackCount] = (int8)as.TrackQueue[trackCount];
            trackCount++;
            as.TrackQueue[trackCount]   = AEUserRadioTrackManager.SelectUserTrackIndex();
            as.TrackTypes[trackCount]   = TYPE_USER_TRACK;
            as.TrackIndices[trackCount] = (int8)as.TrackQueue[trackCount];
        } else {
            const auto id = as.StationID;
            if (as.TrackTypes[0] == TYPE_INTRO || as.TrackTypes[0] == TYPE_TRACK || as.TrackTypes[0] == TYPE_OUTRO) {
                if (id == RADIO_EMERGENCY_AA) {
                    QueueUpTracksForStation(id, &trackCount, TYPE_DJ_BANTER, as);
                } else if (static_cast<int8>(m_nTracksInARow[id]) < 2 && CAEAudioUtility::ResolveProbability(0.5f)) {
                    if (CAEAudioUtility::ResolveProbability(0.5f)) {
                        QueueUpTracksForStation(id, &trackCount, TYPE_INDENT, as);
                    }
                    QueueUpTracksForStation(id, &trackCount, TYPE_INTRO, as);
                } else {
                    if (CAEAudioUtility::ResolveProbability(0.5f)) {
                        QueueUpTracksForStation(id, &trackCount, TYPE_INDENT, as);
                    }
                    if (!QueueUpTracksForStation(id, &trackCount, TYPE_DJ_BANTER, as)) {
                        QueueUpTracksForStation(id, &trackCount, TYPE_ADVERT, as);
                    }
                }
            } else {
                QueueUpTracksForStation(id, &trackCount, TYPE_INTRO, as);
            }
        }
    }

    AEAudioHardware.PlayTrack(
        as.TrackQueue[0],
        as.TrackQueue[1],
        0,
        as.TrackFlags,
        as.TrackTypes[0] == TYPE_USER_TRACK,
        as.TrackTypes[1] == TYPE_USER_TRACK
    );
}

// 0x4EB660
void CAERadioTrackManager::CheckForStationRetune() {
    if (m_ActiveSettings.StationID == RADIO_EMERGENCY_AA) {
        return;
    }

    m_bRetuneJustStarted = false;

    const bool isRadioActive = m_nMode == eRadioTrackMode::RADIO_STARTING
        || m_nMode == eRadioTrackMode::RADIO_WAITING_TO_PLAY
        || m_nMode == eRadioTrackMode::RADIO_PLAYING
        || m_bInitialised
        || m_nStationsListed != 0
        || m_nStationsListDown != 0
        || m_ActiveSettings.StationID == RADIO_OFF;
    if (isRadioActive && !AudioEngine.GetCutsceneTrackStatus()) {
        if (const auto* const settings = CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio()) {
            if (notsa::contains({ AE_RT_CIVILIAN, AE_RT_EMERGENCY, AE_RT_UNKNOWN }, settings->RadioType) && CReplay::Mode != MODE_PLAYBACK) {
                // NOTE: Retuning is only possible when in a civilian vehicle
                if (CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio()->RadioType != AE_RT_CIVILIAN) {
                    return;
                }

                if (m_iRadioStationScriptRequest >= 0) {
                    m_nStationsListDown = m_nStationsListed;
                    m_nStationsListed = m_iRadioStationScriptRequest - (int8)m_RequestedSettings.StationID;
                    m_iRadioStationScriptRequest = -1;
                    m_nTimeRadioStationRetuned = CTimer::GetTimeInMS();
                    m_bDisplayStationName = true;
                    m_bRetuneJustStarted = true;
                } else if (CPad::GetPad(0)->NextStationJustUp()) {
                    m_nStationsListDown = m_nStationsListed;
                    m_nStationsListed++;
                    m_nTimeRadioStationRetuned = CTimer::GetTimeInMS();
                    m_bDisplayStationName = true;
                    m_bRetuneJustStarted = true;
                } else if (CPad::GetPad(0)->LastStationJustUp()) {
                    m_nStationsListDown = m_nStationsListed;
                    m_nStationsListed--;
                    m_nTimeRadioStationRetuned = CTimer::GetTimeInMS();
                    m_bDisplayStationName = true;
                    m_bRetuneJustStarted = true;
                }
            }
        }
    }

    if (m_nStationsListed == 0 && m_nStationsListDown == 0) {
        return;
    }

    // Wrap around the stations ([1, 13], where 13 is "off")
    int8 newStation = static_cast<int8>(m_RequestedSettings.StationID + static_cast<int8>(m_nStationsListed));
    if (newStation <= 0) {
        newStation += RADIO_OFF;
    } else if (newStation >= RADIO_OFF + 1) {
        newStation -= RADIO_OFF;
    }

    if (newStation == RADIO_OFF || (newStation == RADIO_USER_TRACKS && AEUserRadioTrackManager.m_nUserTracksCount == 0)) {
        StopRadio(nullptr, false);
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_CLICK_OFF);
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_STOP);
    } else {
        if (m_ActiveSettings.StationID == RADIO_OFF) {
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_CLICK_ON);
            m_ActiveSettings.StationID = RADIO_INVALID;
        } else {
            StopRadio(nullptr, false);
        }
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_START);

        const uint32 retuneDelay = s_CameraRetuneFactor > 0.9f ? 4000u : 2000u;
        if (CTimer::GetTimeInMS() <= m_nTimeRadioStationRetuned + 1500u) {
            return;
        }
        if (CTimer::GetTimeInMS() <= field_60 + retuneDelay) {
            return;
        }
    }

    StartRadio((eRadioID)newStation, m_ActiveSettings.BassSetting, m_ActiveSettings.BassGain, false);
    m_nStationsListed = 0;
    m_nStationsListDown = 0;
}

// 0x4EB890
void CAERadioTrackManager::CheckForStationRetuneDuringPause() {
    if (m_ActiveSettings.StationID == RADIO_EMERGENCY_AA && IsRadioOn() || m_iRadioStationMenuRequest <= RADIO_INVALID)
        return;

    if (m_iRadioStationMenuRequest != RADIO_OFF) {
        if (m_ActiveSettings.StationID == RADIO_OFF) {
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_CLICK_ON);
            m_ActiveSettings.StationID = RADIO_INVALID;
        } else {
            AudioEngine.StopRadio(nullptr, true);
        }

        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_START);
        if (CTimer::GetTimeInMSPauseMode() > m_nRetuneStartedTime + 700u) {
            StartRadio((eRadioID)m_iRadioStationMenuRequest, m_ActiveSettings.BassSetting, m_ActiveSettings.BassGain, 0);
            m_iRadioStationMenuRequest = RADIO_INVALID;
        }
    } else {
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_CLICK_OFF);
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_STOP);
        StartRadio(RADIO_OFF, m_ActiveSettings.BassSetting, m_ActiveSettings.BassGain, 0);
        m_iRadioStationMenuRequest = RADIO_INVALID;
    }
}

// 0x4EA640
void CAERadioTrackManager::StartTrackPlayback() {
    AEAudioHardware.SetChannelFlags(m_HwClientHandle, 0, 55);
    AEAudioHardware.StartTrackPlayback();
    UpdateRadioVolumes();
}

// 0x4EA010
void CAERadioTrackManager::UpdateRadioVolumes() {
    float volume = -4.0f;

    if (!CTimer::GetIsPaused() || !m_bEnabledInPauseMode) {
        if (CTimer::GetIsSlowMotionActive()) {
            volume = -100.0f;
        } else if (TheCamera.m_bWideScreenOn) {
            volume = -16.0f;
        } else if (AEAudioHardware.GetEffectsMasterScalingFactor() > 0.0f && AEAudioHardware.GetMusicMasterScalingFactor() > 0.0f) {
            // Is there any mission audio close to the camera (or without a position)?
            bool isMissionAudioAudible = CAEPedSpeechAudioEntity::s_bForceAudible;
            if (!isMissionAudioAudible) {
                for (uint8 i = 0; i < 2u; i++) {
                    if (AudioEngine.IsMissionAudioSampleFinished(i)) {
                        continue;
                    }
                    if (AudioEngine.GetMissionAudioEvent(i) == 0xFFFF) {
                        continue;
                    }
                    const auto* const pos = AudioEngine.GetMissionAudioPosition(i);
                    if (!pos || CAEAudioEnvironment::GetPositionRelativeToCamera(*pos).Magnitude() <= 15.0f) {
                        isMissionAudioAudible = true;
                        break;
                    }
                }
            }

            if (isMissionAudioAudible) {
                // Duck the radio so that the mission audio can be heard
                // NOTE: The original code uses the x87 `fyl2x`, so the result might differ slightly
                float duck = std::log10(AEAudioHardware.GetEffectsMasterScalingFactor() / AEAudioHardware.GetMusicMasterScalingFactor()) * 20.0f - 9.0f;
                if (duck >= 0.0f) {
                    duck = 0.0f;
                }
                m_f80 = duck;
                m_f84 = -0.02f * duck;
                volume = duck - 4.0f;
            } else if (m_f80 < 0.0f) {
                // Slowly restore the volume
                float duck = m_f84 + m_f80;
                if (!(duck < 0.0f)) {
                    duck = 0.0f;
                }
                m_f80 = duck;
                volume = duck - 4.0f;
            }
        }

        if (CAudioEngine::IsAmbienceRadioActive()) {
            volume -= 20.0f;
        }
    }

    if (m_bBassEnhance) {
        switch (m_ActiveSettings.BassSetting) {
        case eBassSetting::BOOST:
            volume -= 2.0f;
            break;
        case eBassSetting::CUT:
            volume += 1.5f;
            break;
        default:
            break;
        }
    }

    AEAudioHardware.SetChannelVolume(static_cast<int16>(m_HwClientHandle), 0, volume, 0);
}

// 0x4E8400
void CAERadioTrackManager::PlayRadioAnnouncement(uint32) {
    // NOP
}

// 0x4EB550
void CAERadioTrackManager::StartRadio(const tVehicleAudioSettings& settings) {
    // plugin::CallMethod<0x4EB550, CAERadioTrackManager*, tVehicleAudioSettings*>(this, settings);

    if (CReplay::Mode == MODE_PLAYBACK)
        return;

    if (settings.RadioType == AE_RT_EMERGENCY) {
        StartRadio(RADIO_EMERGENCY_AA, settings.BassSetting, settings.BassFactor, 0);
        return;
    }

    if (settings.RadioType != AE_RT_CIVILIAN)
        return;

    const bool needsRetune = [&] {
       if (!m_bRadioAutoSelect)
           return false;

       const auto savedId = m_nSavedRadioStationId;
       if (savedId < 0 || savedId == settings.RadioStation || savedId == RADIO_OFF || savedId == RADIO_EMERGENCY_AA)
           return false;

       if (CTimer::GetTimeInMS() > m_nSavedTimeMs + 60'000)
           return false;

       const auto savedHours = m_nSavedGameClockHours;
       auto savedDays = m_nSavedGameClockDays;
       if (savedHours < 0 || savedDays < 0)
           return false;

       if (savedDays > CClock::GetGameClockDays()) {
           const auto month = CClock::GetGameClockMonth();
           savedDays += CClock::daysInMonth[month == 0 ? 11 : month - 1]; // prev month
       }

       if (CClock::GetGameClockHours() + 24 * savedDays - savedHours > 5)
           return false;

       return true;
    }();

    if (needsRetune) {
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_START);
        StartRadio((eRadioID)m_nSavedRadioStationId, settings.BassSetting, settings.BassFactor, 0);
    } else {
        StartRadio(settings.RadioStation, settings.BassSetting, settings.BassFactor, 0);
    }
}

// 0x4EB3C0
void CAERadioTrackManager::StartRadio(eRadioID id, eBassSetting bassSetting, float bassGain, bool skipTrack) {
    id = std::min(id, RADIO_OFF);

    if (CTimer::GetIsPaused()) {
        m_bEnabledInPauseMode = true;

        if (IsRadioOn() && id == m_ActiveSettings.StationID) {
            m_aRadioState[id].m_iTimeInPauseModeInMs = CTimer::GetTimeInMSPauseMode();
            return;
        }
    }

    if (id != RADIO_OFF && CAudioEngine::IsAmbienceTrackActive()) {
        if (!CTimer::GetIsPaused() && CAudioEngine::DoesAmbienceTrackOverrideRadio()) {
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_STOP);
            return;
        }
        AudioEngine.StopAmbienceTrack(false);
    }

    auto* const rs = &m_RequestedSettings;
    rs->StationID = id;
    rs->BassSetting             = bassSetting;
    rs->BassGain            = bassGain;
    if (id == RADIO_OFF) {
        rs->Reset();
    } else if (m_aRadioState[id].m_iTimeInMs < 0 || !TrackRadioStation(id, skipTrack)) {
        ChooseTracksForStation(rs->StationID);
        rs->PlayTime = CAEAudioUtility::GetRandomNumberInRange(0, 300'000);
    }
    switch (m_nMode) {
    case eRadioTrackMode::RADIO_STARTING:
    case eRadioTrackMode::RADIO_WAITING_TO_PLAY:
    case eRadioTrackMode::RADIO_PLAYING:
        m_nMode = eRadioTrackMode::RADIO_STOPPING;
    }
    m_aRadioState[rs->StationID].m_iTimeInPauseModeInMs = -1;

    m_bInitialised = true;
}

// 0x4EAC30
bool CAERadioTrackManager::TrackRadioStation(eRadioID id, bool skipTrack) {
    auto& state = m_aRadioState[id];
    auto& req   = m_RequestedSettings;
    int8  trackCount = 0;

    // Ignore the state if it's too old (more than 5 game hours)
    if (state.m_nGameClockHours >= 0 && state.m_nGameClockDays >= 0) {
        int32 days = (int32)CClock::GetGameClockDays() - state.m_nGameClockDays;
        if (days < 0) {
            int8 prevMonth = (int8)CClock::GetGameClockMonth() - 1;
            if (prevMonth < 0) {
                prevMonth += 12;
            }
            days += CClock::daysInMonth[prevMonth];
        }
        if (24 * days - state.m_nGameClockHours + CClock::GetGameClockHours() > 5) {
            return false;
        }
    }

    // Time that has passed since the station was last listened to
    int32 elapsed = (int32)(CTimer::GetTimeInMS() - (uint32)state.m_iTimeInMs);
    if (elapsed <= 7000) {
        elapsed = 7000;
    }
    if (skipTrack) {
        elapsed = std::max(elapsed, state.m_aElapsed[0] + 1);
    }

    for (auto i = 0u; i < tRadioSettings::NUM_TRACKS; i++) {
        req.TrackQueue[i]   = -1;
        req.TrackTypes[i]   = TYPE_NONE;
        req.TrackIndices[i] = -1;
    }

    int32 sum = 0;
    for (auto i = 0; i < (int32)state.m_aElapsed.size(); i++) {
        sum += state.m_aElapsed[i];
        if (elapsed > sum) {
            continue;
        }

        if (i == 0) {
            req.PlayTime = state.m_iTrackPlayTime + elapsed;
        } else {
            req.PlayTime = state.m_aElapsed[i] - sum + elapsed;
        }

        const int8 type = state.m_aTrackTypes[i];
        switch (type) {
        case TYPE_INDENT:
        case TYPE_ADVERT:
        case TYPE_DJ_BANTER: {
            req.TrackQueue[0] = state.m_aTrackQueue[i];
            req.TrackTypes[0] = type;
            trackCount = 1;
            if (id == RADIO_USER_TRACKS) {
                QueueUpTracksForStation(RADIO_USER_TRACKS, &trackCount, TYPE_TRACK, req);
            } else {
                QueueUpTracksForStation(id, &trackCount, TYPE_INTRO, req);
            }
            return true;
        }
        case TYPE_INTRO: {
            // BUG: Copies 3 elements, even though there might be less than 3 left in the arrays (reads out of bounds)
            const int32* const srcQueue = &state.m_aTrackQueue[i];
            const int8* const  srcTypes = &state.m_aTrackTypes[i];
            for (auto k = 0u; k < 3u; k++) {
                req.TrackQueue[k] = srcQueue[k];
                req.TrackTypes[k] = srcTypes[k];
            }
            return true;
        }
        case TYPE_TRACK: {
            // BUG: Same as above
            const int32* const srcQueue = &state.m_aTrackQueue[i];
            const int8* const  srcTypes = &state.m_aTrackTypes[i];
            for (auto k = 0u; k < 2u; k++) {
                req.TrackQueue[k] = srcQueue[k];
                req.TrackTypes[k] = srcTypes[k];
            }
            return true;
        }
        case TYPE_OUTRO: {
            req.TrackQueue[0] = state.m_aTrackQueue[i];
            req.TrackTypes[0] = type;
            trackCount = 1;
            if (id == RADIO_EMERGENCY_AA) {
                QueueUpTracksForStation(RADIO_EMERGENCY_AA, &trackCount, TYPE_DJ_BANTER, req);
                return true;
            }
            if (CAEAudioUtility::ResolveProbability(0.5f)) {
                QueueUpTracksForStation(id, &trackCount, TYPE_INDENT, req);
            }
            if (!QueueUpTracksForStation(id, &trackCount, TYPE_DJ_BANTER, req)) {
                QueueUpTracksForStation(id, &trackCount, TYPE_ADVERT, req);
            }
            return true;
        }
        case TYPE_USER_TRACK: {
            // BUG: Same as above
            const int32* const srcQueue = &state.m_aTrackQueue[i];
            const int8* const  srcTypes = &state.m_aTrackTypes[i];
            for (auto k = 0u; k < 2u; k++) {
                req.TrackQueue[k] = srcQueue[k];
                req.TrackTypes[k] = srcTypes[k];
            }
            if (req.TrackQueue[1] == -1) {
                req.TrackQueue[1]   = AEUserRadioTrackManager.SelectUserTrackIndex();
                req.TrackTypes[1]   = TYPE_USER_TRACK;
                req.TrackIndices[1] = (int8)req.TrackQueue[1];
            }
            return true;
        }
        default: // TYPE_NONE and invalid types
            return false;
        }
    }

    // The tracks of the state have all finished
    if (elapsed <= sum + 7000) {
        if (id == RADIO_USER_TRACKS) {
            QueueUpTracksForStation(RADIO_USER_TRACKS, &trackCount, TYPE_TRACK, req);
            QueueUpTracksForStation(RADIO_USER_TRACKS, &trackCount, TYPE_TRACK, req);
        } else {
            QueueUpTracksForStation(id, &trackCount, TYPE_INTRO, req);
        }
        req.PlayTime = std::min(elapsed - sum, 5000);
        return true;
    }

    if (elapsed <= sum + 155'000) {
        QueueUpTracksForStation(id, &trackCount, TYPE_TRACK, req);
        req.PlayTime = elapsed - sum - 5000;
        return true;
    }

    if (elapsed > sum + 160'000) {
        return false;
    }

    if (id == RADIO_USER_TRACKS) {
        QueueUpTracksForStation(RADIO_USER_TRACKS, &trackCount, TYPE_TRACK, req);
        if (!FrontEndMenuManager.m_RadioMode && CAEAudioUtility::ResolveProbability(0.17f)) {
            QueueUpTracksForStation(RADIO_USER_TRACKS, &trackCount, TYPE_ADVERT, req);
        }
    } else {
        QueueUpTracksForStation(id, &trackCount, TYPE_OUTRO, req);
        AddMusicTrackIndexToHistory(id, req.TrackIndices[trackCount - 1]);

        if (id == RADIO_EMERGENCY_AA) {
            QueueUpTracksForStation(RADIO_EMERGENCY_AA, &trackCount, TYPE_DJ_BANTER, req);
        } else if (CAEAudioUtility::ResolveProbability(0.5f)) {
            if (CAEAudioUtility::ResolveProbability(0.5f)) {
                QueueUpTracksForStation(id, &trackCount, TYPE_INDENT, req);
            }
            QueueUpTracksForStation(id, &trackCount, TYPE_INTRO, req);
        } else {
            if (CAEAudioUtility::ResolveProbability(0.5f)) {
                QueueUpTracksForStation(id, &trackCount, TYPE_INDENT, req);
            }
            if (!QueueUpTracksForStation(id, &trackCount, TYPE_DJ_BANTER, req)) {
                QueueUpTracksForStation(id, &trackCount, TYPE_ADVERT, req);
            }
        }
    }
    req.PlayTime = elapsed - sum - 155'000;
    return true;
}

// 0x4EA670
bool CAERadioTrackManager::QueueUpTracksForStation(eRadioID id, int8* iTrackCount, int8 radioState, tRadioSettings& settings) {
    int8& n = *iTrackCount;

    switch (radioState) {
    case TYPE_INDENT: {
        if (id == RADIO_USER_TRACKS) {
            return false;
        }
        settings.TrackQueue[n] = ChooseIdentIndex(id);
        if (settings.TrackQueue[n] == -1) {
            return false;
        }
        settings.TrackTypes[n] = TYPE_INDENT;
        n++;
        return true;
    }
    case TYPE_ADVERT: {
        settings.TrackQueue[n] = ChooseAdvertIndex(id);
        settings.TrackTypes[n] = TYPE_ADVERT;
        n++;
        return true;
    }
    case TYPE_DJ_BANTER: {
        if (id == RADIO_USER_TRACKS) {
            return false;
        }
        settings.TrackQueue[n] = ChooseDJBanterIndex(id);
        if (settings.TrackQueue[n] == -1) {
            return false;
        }
        settings.TrackTypes[n] = TYPE_DJ_BANTER;
        n++;
        return true;
    }
    case TYPE_INTRO: { // Queues up the intro, the track itself and the outro
        if (id == RADIO_USER_TRACKS) {
            return false;
        }

        settings.TrackIndices[n] = ChooseMusicTrackIndex(id);
        const auto& intro = s_TrackIntroRange[id][settings.TrackIndices[n]];
        settings.TrackQueue[n] = CAEAudioUtility::GetRandomNumberInRange(intro.Start, intro.End);
        settings.TrackTypes[n] = TYPE_INTRO;
        n++;

        settings.TrackIndices[n] = settings.TrackIndices[n - 1];
        settings.TrackQueue[n] = s_TrackSoundID[id][settings.TrackIndices[n]];
        settings.TrackTypes[n] = TYPE_TRACK;
        n++;

        settings.TrackIndices[n] = settings.TrackIndices[n - 1];
        const auto& outro = s_TrackOutroRange[id][settings.TrackIndices[n]];
        settings.TrackQueue[n] = CAEAudioUtility::GetRandomNumberInRange(outro.Start, outro.End);
        settings.TrackTypes[n] = TYPE_OUTRO;
        n++;
        return true;
    }
    case TYPE_TRACK: { // Queues up the track itself and the outro
        if (id == RADIO_USER_TRACKS) {
            settings.TrackQueue[n] = AEUserRadioTrackManager.SelectUserTrackIndex();
            settings.TrackTypes[n] = TYPE_USER_TRACK;
            settings.TrackIndices[n] = (int8)settings.TrackQueue[n];
            n++;
            return true;
        }

        settings.TrackIndices[n] = ChooseMusicTrackIndex(id);
        settings.TrackQueue[n] = s_TrackSoundID[id][settings.TrackIndices[n]];
        settings.TrackTypes[n] = TYPE_TRACK;
        n++;

        settings.TrackIndices[n] = settings.TrackIndices[n - 1];
        const auto& outro = s_TrackOutroRange[id][settings.TrackIndices[n]];
        settings.TrackQueue[n] = CAEAudioUtility::GetRandomNumberInRange(outro.Start, outro.End);
        settings.TrackTypes[n] = TYPE_OUTRO;
        n++;
        return true;
    }
    case TYPE_OUTRO: { // Queues up an outro of a (new) random track
        if (id == RADIO_USER_TRACKS) {
            return false;
        }

        settings.TrackIndices[n] = ChooseMusicTrackIndex(id);
        const auto& outro = s_TrackOutroRange[id][settings.TrackIndices[n]];
        settings.TrackQueue[n] = CAEAudioUtility::GetRandomNumberInRange(outro.Start, outro.End);
        settings.TrackTypes[n] = TYPE_OUTRO;
        n++;
        return true;
    }
    default:
        return true;
    }
}

// 0x4E9820
void CAERadioTrackManager::StopRadio(tVehicleAudioSettings* settings, bool duringPause) {
    auto& as = m_ActiveSettings;

    if (m_nMode == eRadioTrackMode::RADIO_STARTING || m_nMode == eRadioTrackMode::RADIO_WAITING_TO_PLAY || m_nMode == eRadioTrackMode::RADIO_PLAYING) {
        if (!CTimer::GetIsPaused() || duringPause) {
            m_nMode = eRadioTrackMode::RADIO_STOPPING;
        }

        // Save the state of the station, so that it can be resumed later
        // BUG: `as.StationID` might be `RADIO_INVALID`, in which case memory before `m_aRadioState` is overwritten
        auto& state = m_aRadioState[as.StationID];
        rng::fill(state.m_aElapsed, 0);
        state.m_iTrackPlayTime = -1;
        rng::fill(state.m_aTrackQueue, -1);
        rng::fill(state.m_aTrackTypes, TYPE_NONE);
        state.m_iTimeInMs = CTimer::GetTimeInMS();
        state.m_nGameClockDays = CClock::GetGameClockDays();
        state.m_nGameClockHours = CClock::GetGameClockHours();

        if (state.m_iTimeInPauseModeInMs >= 0 && as.StationID != RADIO_EMERGENCY_AA && as.StationID != RADIO_OFF) {
            m_aListenTimes[as.StationID] += static_cast<int32>(CTimer::GetTimeInMSPauseMode()) - state.m_iTimeInPauseModeInMs;
        }

        if (as.StationID == RADIO_OFF) {
            state.m_aElapsed[0] = 0;
        } else {
            state.m_aElapsed[0] = as.TrackLengthMs - as.PlayTime - 100;

            switch (as.CurrTrackType) {
            case TYPE_INDENT:
            case TYPE_ADVERT:
            case TYPE_DJ_BANTER:
            case TYPE_OUTRO: {
                state.m_iTrackPlayTime = as.PlayTime;
                state.m_aTrackQueue[0] = as.CurrTrackID;
                state.m_aTrackTypes[0] = as.CurrTrackType;
                break;
            }
            case TYPE_INTRO: {
                if (as.StationID == RADIO_TALK) {
                    state.m_aElapsed[1] = s_TalkShowLengthMs[as.TrackIndices[0]];
                } else {
                    state.m_aElapsed[1] = 150'000;
                }
                state.m_aElapsed[2] = 5000;
                state.m_iTrackPlayTime = as.PlayTime;
                if (as.TrackQueue[0] == as.CurrTrackID) {
                    state.m_aTrackQueue[0] = as.TrackQueue[0];
                    state.m_aTrackTypes[0] = as.TrackTypes[0];
                    state.m_aTrackQueue[1] = as.TrackQueue[1];
                    state.m_aTrackTypes[1] = as.TrackTypes[1];
                    state.m_aTrackQueue[2] = as.TrackQueue[2];
                    state.m_aTrackTypes[2] = as.TrackTypes[2];
                } else {
                    state.m_aTrackQueue[0] = as.CurrTrackID;
                    state.m_aTrackTypes[0] = as.CurrTrackType;
                    state.m_aTrackQueue[1] = as.TrackQueue[0];
                    state.m_aTrackTypes[1] = as.TrackTypes[0];
                    state.m_aTrackQueue[2] = as.TrackQueue[1];
                    state.m_aTrackTypes[2] = as.TrackTypes[1];
                }
                break;
            }
            case TYPE_TRACK:
            case TYPE_USER_TRACK: {
                state.m_aElapsed[1] = 5000;
                state.m_iTrackPlayTime = as.PlayTime;
                if (as.TrackQueue[0] == as.CurrTrackID) {
                    state.m_aTrackQueue[0] = as.TrackQueue[0];
                    state.m_aTrackTypes[0] = as.TrackTypes[0];
                    state.m_aTrackQueue[1] = as.TrackQueue[1];
                    state.m_aTrackTypes[1] = as.TrackTypes[1];
                } else {
                    state.m_aTrackQueue[0] = as.CurrTrackID;
                    state.m_aTrackTypes[0] = as.CurrTrackType;
                    state.m_aTrackQueue[1] = as.TrackQueue[0];
                    state.m_aTrackTypes[1] = as.TrackTypes[0];
                }
                break;
            }
            default: // TYPE_NONE and invalid types
                break;
            }
        }
    }

    m_bInitialised = false;
    m_bEnabledInPauseMode = false;

    if (CTimer::GetIsPaused() && !duringPause) {
        m_iRadioStationMenuRequest = -1;
        m_nRetuneStartedTime = 0;
    }

    if (settings) {
        m_nStationsListed = 0;
        m_nStationsListDown = 0;
        m_iRadioStationScriptRequest = -1;
        m_bDisplayStationName = false;
        m_bRetuneJustStarted = false;
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_STOP);

        if (as.StationID == RADIO_INVALID) {
            as.StationID = RADIO_OFF;
        }
        settings->RadioStation = as.StationID;
        settings->BassSetting  = as.BassSetting;

        if (m_nMode != eRadioTrackMode::RADIO_STOPPED || m_bInitialised || m_nStationsListed != 0 || m_nStationsListDown != 0) {
            m_nSavedTimeMs = CTimer::GetTimeInMS();
            m_nSavedGameClockDays = CClock::GetGameClockDays();
            m_nSavedGameClockHours = CClock::GetGameClockHours();
            m_nSavedRadioStationId = as.StationID;
        }

        if (as.StationID == RADIO_EMERGENCY_AA) {
            as.StationID = CAEAudioUtility::GetRandomRadioStation();
        }
    } else if (duringPause) {
        m_nStationsListed = 0;
        m_nStationsListDown = 0;
        m_bRetuneJustStarted = false;
    }
}

// 0x4E94C0
int32 CAERadioTrackManager::ChooseIdentIndex(eRadioID id) {
    const auto& range = s_IdentRange[id];
    if (range.Start == RADIO_SOUND_NONE) {
        return -1;
    }

    // Don't pick one of the recently played idents (or all of them, if there are only few)
    const auto historySize = std::max(std::min(range.End - range.Start - 1, IDENT_INDEX_HISTORY_COUNT), 0);
    while (true) {
        const auto ident = CAEAudioUtility::GetRandomNumberInRange(range.Start, range.End);

        // Radio Los Santos' ident can only be played after "Are you going to San Fierro?" mission is passed
        if (id == RADIO_MODERN_HIP_HOP && ident == 1100 && CStats::GetStatValue(STAT_ARE_YOU_GOING_TO_SAN_FIERRO_MISSION_ACCOMPLISHED) == 0.0f) {
            continue;
        }

        if (!IsInHistory(m_nIdentIndexHistory[id].indices, historySize, ident)) {
            return ident;
        }
    }
}

// 0x4E9570
int32 CAERadioTrackManager::ChooseAdvertIndex(eRadioID id) {
    while (true) {
        const auto advert = CAEAudioUtility::GetRandomNumberInRange(s_AdvertRange.Start, s_AdvertRange.End);

        // Is the advert blocked for the station?
        const auto& blocked = s_StationBlockedAdverts[id];
        if (std::find(std::begin(blocked), std::end(blocked), advert) != std::end(blocked)) {
            continue;
        }

        // Was it played recently?
        if (IsInHistory(m_nAdvertIndexHistory[id].indices, ADVERT_INDEX_HISTORY_COUNT, advert)) {
            continue;
        }

        return advert;
    }
}

// 0x4EA270
int8 CAERadioTrackManager::ChooseMusicTrackIndex(eRadioID id) {
    if (id == RADIO_TALK) {
        return ChooseTalkRadioShow();
    }

    const auto numTracks   = s_NumTracks[id];
    const auto historySize = std::max(std::min(numTracks - 2, MUSIC_TRACK_HISTORY_COUNT), 0);
    while (true) {
        const auto track = (int8)CAEAudioUtility::GetRandomNumberInRange(0, numTracks - 1);
        if (!IsInHistory(m_nMusicTrackIndexHistory[id].indices, historySize, track)) {
            return track;
        }
    }
}

// 0x4EA2D0
int32 CAERadioTrackManager::ChooseDJBanterIndex(eRadioID id) {
    int32 banter = -1;

    // Story/stats related ("special") banter has priority
    bool isSpecial = false;
    switch (static_cast<int8>(m_nSpecialDJBanterPending)) {
    case 0:
        banter    = s_DJBanterSpecial0[id].Start;
        isSpecial = true;
        break;
    case 1: {
        const auto  idx   = m_nSpecialDJBanterIndex;
        const auto& range = s_DJBanterSpecial1[id];
        if (idx == 0 || (idx == 1 && range.Start != range.End)) {
            banter    = idx == 0 ? range.Start : range.End;
            isSpecial = true;
        }
        break;
    }
    case 2:
        banter    = s_DJBanterSpecial2[id][static_cast<int8>(m_nSpecialDJBanterIndex)];
        isSpecial = true;
        break;
    default:
        break;
    }
    if (isSpecial) {
        if (banter == RADIO_SOUND_NONE) {
            banter = -1;
        } else if (banter >= 0 && IsInHistory(m_nDJBanterIndexHistory[id].indices, DJBANTER_INDEX_HISTORY_COUNT, banter)) {
            banter = -1; // Played recently
        }
    }
    if (banter != -1) {
        return banter;
    }

    // AA only has the general banter
    if (id == RADIO_EMERGENCY_AA) {
        return ChooseDJBanterIndexFromList(RADIO_EMERGENCY_AA, CGameLogic::LaRiotsActiveHere() ? s_DJBanterNight : s_DJBanterGeneral);
    }

    if (!CAEAudioUtility::ResolveProbability(0.6f) || CGame::currArea != AREA_CODE_NORMAL_WORLD) {
        return banter;
    }

    const auto hours = CClock::GetGameClockHours();

    // Weather based banter
    const tRadioSoundRange* weatherList = nullptr;
    if (CWeather::ForecastWeather(WEATHER_RAINY_COUNTRYSIDE, 3) || CWeather::ForecastWeather(WEATHER_RAINY_SF, 3)) {
        if (CAEAudioUtility::ResolveProbability(0.5f)) {
            weatherList = s_DJBanterRain;
        }
    } else if (
           CWeather::ForecastWeather(WEATHER_EXTRASUNNY_LA, 3)
        || CWeather::ForecastWeather(WEATHER_EXTRASUNNY_SMOG_LA, 3)
        || CWeather::ForecastWeather(WEATHER_EXTRASUNNY_COUNTRYSIDE, 3)
        || CWeather::ForecastWeather(WEATHER_EXTRASUNNY_SF, 3)
        || CWeather::ForecastWeather(WEATHER_EXTRASUNNY_VEGAS, 3)
        || CWeather::ForecastWeather(WEATHER_EXTRASUNNY_DESERT, 3)
    ) {
        // BUG: Can never be true, so sunny weather banter is never played.
        //      (Was probably meant to be something like `hours >= 6 && hours < 8`)
        if (hours >= 8 && hours < 6) {
            if (CAEAudioUtility::ResolveProbability(0.5f)) {
                weatherList = s_DJBanterSunny;
            }
        } else if (CWeather::ForecastWeather(WEATHER_FOGGY_SF, 3) && CAEAudioUtility::ResolveProbability(0.5f)) {
            weatherList = s_DJBanterFog;
        }
    } else if (CWeather::ForecastWeather(WEATHER_FOGGY_SF, 3) && CAEAudioUtility::ResolveProbability(0.5f)) {
        weatherList = s_DJBanterFog;
    }
    if (weatherList) {
        banter = ChooseDJBanterIndexFromList(id, weatherList);
        if (banter != -1) {
            return banter;
        }
    }

    // Time of day based banter
    const tRadioSoundRange* timeList = nullptr;
    if (hours >= 6 && hours < 9) {
        if (CAEAudioUtility::ResolveProbability(0.3f)) {
            timeList = s_DJBanterMorning;
        }
    } else if (hours >= 18 && hours < 21) {
        if (CAEAudioUtility::ResolveProbability(0.3f)) {
            timeList = s_DJBanterEvening;
        }
    } else if (hours >= 22 || hours < 3) {
        if (CAEAudioUtility::ResolveProbability(0.3f)) {
            timeList = s_DJBanterNight;
        }
    }
    if (timeList) {
        banter = ChooseDJBanterIndexFromList(id, timeList);
        if (banter != -1) {
            return banter;
        }
    }

    return ChooseDJBanterIndexFromList(id, s_DJBanterGeneral);
}

// 0x4E95E0
int32 CAERadioTrackManager::ChooseDJBanterIndexFromList(eRadioID id, const tRadioSoundRange* list) {
    const auto& range = list[id];
    if (range.Start == RADIO_SOUND_NONE) {
        return -1;
    }

    const auto count  = range.End - range.Start + 1;
    const auto offset = CAEAudioUtility::GetRandomNumberInRange(0, count - 1);
    if (count < 1) {
        return -1;
    }

    // NOTE: The history size is calculated from the general list, not from the one that was passed in
    const auto& generalRange = s_DJBanterGeneral[id];
    const auto  historySize  = std::max(std::min(generalRange.End - generalRange.Start - 1, DJBANTER_INDEX_HISTORY_COUNT), 0);

    // Starting from a random one, find the first that wasn't played recently
    for (auto i = 0; i < count; i++) {
        const auto banter = (i + offset) % count + range.Start;
        if (!IsInHistory(m_nDJBanterIndexHistory[id].indices, historySize, banter)) {
            return banter;
        }
    }
    return -1;
}

// 0x4EB180
void CAERadioTrackManager::ChooseTracksForStation(eRadioID id) {
    int8 trackCount = 0;

    for (auto i = 0u; i < tRadioSettings::NUM_TRACKS; i++) {
        m_RequestedSettings.TrackTypes[i] = TYPE_NONE;
        m_RequestedSettings.TrackQueue[i] = -1;
        m_RequestedSettings.TrackIndices[i] = -1;
    }

    if (!CAEAudioUtility::ResolveProbability(0.95f)) {
        if (id) {
            if (CAEAudioUtility::ResolveProbability(0.5f))
                QueueUpTracksForStation(id, &trackCount, TYPE_INDENT, m_RequestedSettings);

            if (!QueueUpTracksForStation(id, &trackCount, TYPE_DJ_BANTER, m_RequestedSettings))
                QueueUpTracksForStation(id, &trackCount, TYPE_ADVERT, m_RequestedSettings);

            if (id == RADIO_USER_TRACKS) {
                QueueUpTracksForStation(RADIO_USER_TRACKS, &trackCount, TYPE_TRACK, m_RequestedSettings);
                return;
            }
        } else {
            QueueUpTracksForStation(RADIO_EMERGENCY_AA, &trackCount, TYPE_DJ_BANTER, m_RequestedSettings);
        }
        QueueUpTracksForStation(id, &trackCount, TYPE_INTRO, m_RequestedSettings);
        return;
    }

    if (id == RADIO_USER_TRACKS) {
        QueueUpTracksForStation(RADIO_USER_TRACKS, &trackCount, TYPE_TRACK, m_RequestedSettings);
        QueueUpTracksForStation(RADIO_USER_TRACKS, &trackCount, TYPE_TRACK, m_RequestedSettings);
        if (!FrontEndMenuManager.m_RadioMode && CAEAudioUtility::ResolveProbability(0.17f)) {
            QueueUpTracksForStation(RADIO_USER_TRACKS, &trackCount, TYPE_ADVERT, m_RequestedSettings);
        }
        return;
    }

    if (CAEAudioUtility::ResolveProbability(0.9f)) {
        QueueUpTracksForStation(id, &trackCount, TYPE_TRACK, m_RequestedSettings);
        return;
    }

    if (CAEAudioUtility::ResolveProbability(0.5f)) {
        if (CAEAudioUtility::ResolveProbability(0.5f)) {
            QueueUpTracksForStation(id, &trackCount, TYPE_INDENT, m_RequestedSettings);
        }
        QueueUpTracksForStation(id, &trackCount, TYPE_INTRO, m_RequestedSettings);
        return;
    }

    QueueUpTracksForStation(id, &trackCount, TYPE_OUTRO, m_RequestedSettings);
    AddMusicTrackIndexToHistory(id, m_RequestedSettings.TrackIndices[trackCount - 1]);

    if (id == RADIO_EMERGENCY_AA) {
        QueueUpTracksForStation(id, &trackCount, TYPE_DJ_BANTER, m_RequestedSettings);
        return;
    }

    if (CAEAudioUtility::ResolveProbability(0.5f)) {
        if (CAEAudioUtility::ResolveProbability(0.5f)) {
            QueueUpTracksForStation(id, &trackCount, TYPE_INDENT, m_RequestedSettings);
        }
        QueueUpTracksForStation(id, &trackCount, TYPE_INTRO, m_RequestedSettings);
        return;
    }

    if (CAEAudioUtility::ResolveProbability(0.5f))
        QueueUpTracksForStation(id, &trackCount, TYPE_INDENT, m_RequestedSettings);

    if (!QueueUpTracksForStation(id, &trackCount, TYPE_DJ_BANTER, m_RequestedSettings))
        QueueUpTracksForStation(id, &trackCount, TYPE_ADVERT, m_RequestedSettings);
}

// 0x4E8E40
int8 CAERadioTrackManager::ChooseTalkRadioShow() {
    // NOTE: The stat IDs are raw, because the names in `eStats` at these indices are unreliable.
    const auto IsStatZero    = [](int32 stat) { return CStats::GetStatValue(static_cast<eStats>(stat)) == 0.0f; };
    const auto IsStatNonZero = [&](int32 stat) { return !IsStatZero(stat); };

    // Find all shows that are available (depends on the progress in the game)
    std::array<int8, 31> shows;
    rng::fill(shows, -1);
    int8 numShows = 0;

    if (IsStatNonZero(0x136) && IsStatZero(0x137)) {
        shows[numShows++] = 14;
    } else if (IsStatNonZero(0x138)) {
        shows[numShows++] = 15;
    }

    if (IsStatZero(0x139)) {
        shows[numShows++] = 12;
    } else if (IsStatZero(0x138)) {
        shows[numShows++] = 13;
    }

    if (IsStatNonZero(0x13B) && IsStatZero(0x13C)) {
        shows[numShows++] = 6;
    }

    if (IsStatNonZero(0x12E) && IsStatZero(0x13A)) {
        shows[numShows++] = 3;
    } else if (IsStatNonZero(0x13C) && IsStatZero(0x13D)) {
        shows[numShows++] = 4;
    } else if (IsStatNonZero(0x13D)) {
        shows[numShows++] = 5;
    }

    if (IsStatZero(0x13E)) {
        shows[numShows++] = 7;
    } else {
        shows[numShows++] = 8;
    }

    if (IsStatZero(0x13F)) {
        shows[numShows++] = 9;
    } else if (IsStatZero(0x130) && IsStatZero(0x140)) {
        shows[numShows++] = 10;
    } else if (IsStatNonZero(0x140)) {
        shows[numShows++] = 11;
    }

    if (IsStatZero(0x141)) {
        shows[numShows++] = 27;
    } else {
        shows[numShows++] = 28;
    }

    if (IsStatZero(0x142)) {
        shows[numShows++] = 29;
    } else {
        shows[numShows++] = 30;
    }

    if (IsStatZero(0x143)) {
        shows[numShows++] = 0;
    } else if (IsStatNonZero(0x144) && IsStatZero(0x145)) {
        shows[numShows++] = 1;
    } else if (IsStatNonZero(0x146)) {
        shows[numShows++] = 2;
    }

    if (IsStatZero(0x12E)) {
        shows[numShows++] = 16;
    } else if (IsStatNonZero(0x12E) && IsStatZero(0x12F)) {
        shows[numShows++] = 17;
    } else if (IsStatNonZero(0x12F) && IsStatZero(0x143)) {
        shows[numShows++] = 18;
    } else if (IsStatNonZero(0x143) && IsStatZero(0x130)) {
        shows[numShows++] = 19;
    } else if (IsStatNonZero(0x130) && IsStatZero(0x131)) {
        shows[numShows++] = 20;
    } else if (IsStatNonZero(0x131) && IsStatZero(0x132)) {
        shows[numShows++] = 21;
    } else if (IsStatNonZero(0x132) && IsStatZero(0x133)) {
        shows[numShows++] = 22;
    } else if (IsStatNonZero(0x133) && IsStatZero(0x134)) {
        shows[numShows++] = 23;
    } else if (IsStatNonZero(0x134) && IsStatZero(0x135)) {
        shows[numShows++] = 24;
    } else if (IsStatNonZero(0x135) && CStats::GetStatValue(STAT_CITY_UNLOCKED) != 4.0f) {
        shows[numShows++] = 25;
    } else if (CStats::GetStatValue(STAT_CITY_UNLOCKED) == 4.0f) {
        shows[numShows++] = 26;
    }

    // Pick a random one that wasn't played recently
    // NOTE: The history of shows is the music track history of the talk radio. It only has 20 entries,
    //       but up to 29 are checked, which reads out of bounds (into the history of the next station).
    const int32 numToCheck = numShows - 1;
    const int8* const history = m_nMusicTrackIndexHistory[RADIO_TALK].indices.data();
    while (true) {
        const auto show = shows[CAEAudioUtility::GetRandomNumberInRange(0, numToCheck)];
        if (numToCheck <= 0) {
            return show;
        }
        auto i = 0;
        for (; i < numToCheck; i++) {
            if (show == history[i]) {
                break;
            }
        }
        if (i >= numToCheck) {
            return show;
        }
    }
}

// 0x4E96C0
void CAERadioTrackManager::AddMusicTrackIndexToHistory(eRadioID id, int8 trackIndex) {
    if (trackIndex >= 0 && m_nMusicTrackIndexHistory[id].indices[0] != trackIndex) {
        m_nMusicTrackIndexHistory[id].PutAtFirst(trackIndex);
        m_nTracksInARow[id]++;
    }
}

// 0x4E9720
void CAERadioTrackManager::AddIdentIndexToHistory(eRadioID id, int8 trackIndex) {
    if (m_nIdentIndexHistory[id].indices[0] != trackIndex)
        m_nIdentIndexHistory[id].PutAtFirst(trackIndex);
}

// 0x4E9760
void CAERadioTrackManager::AddAdvertIndexToHistory(eRadioID id, int8 trackIndex) {
    if (m_nAdvertIndexHistory[id].indices[0] != trackIndex) {
        m_nAdvertIndexHistory[id].PutAtFirst(trackIndex);
        m_nTracksInARow[id] = 0;
    }
}

// 0x4E97B0
void CAERadioTrackManager::AddDJBanterIndexToHistory(eRadioID id, int8 trackIndex) {
    if (m_nDJBanterIndexHistory[id].indices[0] != trackIndex) {
        m_nDJBanterIndexHistory[id].PutAtFirst(trackIndex);
        m_nTracksInARow[id] = 0;
    }
}

// 0x4EA590
void CAERadioTrackManager::CheckForPause() {
    if (CTimer::GetIsPaused()) {
        m_bPauseMode = true;
        AEAudioHardware.SetChannelFrequencyScalingFactor(m_HwClientHandle, 0, m_bEnabledInPauseMode ? 1.0f : 0.0f);
    } else {
        const auto* settings = CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio();
        
        if (settings && notsa::contains({
                AE_RT_CIVILIAN,
                AE_RT_EMERGENCY,
                AE_RT_UNKNOWN
            }, settings->RadioType)
            || CAudioEngine::IsAmbienceRadioActive()
        ) {
            m_bPauseMode = false;
            AEAudioHardware.SetChannelFrequencyScalingFactor(m_HwClientHandle, 0, 1.0f);
        } else {
            StopRadio(nullptr, false);
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_STOP);
            m_bPauseMode = false;
        }
    }
}

// 0x4EB9A0
void CAERadioTrackManager::Service(int32 playTime) {
    s_PrevServicePlayTime = m_ActiveSettings.PlayTime;
    m_ActiveSettings.PlayTime = playTime;
    m_ActiveSettings.TrackLengthMs = AEAudioHardware.GetTrackLengthMs();
    m_ActiveSettings.CurrTrackID = AEAudioHardware.GetPlayingTrackID();

    if (!CTimer::GetIsPaused()) {
        CheckForMissionStatsChanges();
        CheckForStationRetune();
    } else {
        CheckForStationRetuneDuringPause();
    }

    // Apply the requested settings once the previous radio has fully stopped
    if (m_bInitialised && m_nMode == eRadioTrackMode::RADIO_STOPPED) {
        if (m_RequestedSettings.StationID == RADIO_OFF) {
            m_ActiveSettings = m_RequestedSettings;
            if (IsVehicleRadioActive()) {
                m_bDisplayStationName = true;
            }
            m_bInitialised = false;
        } else if (!CAudioEngine::IsAmbienceTrackActive()) {
            m_ActiveSettings = m_RequestedSettings;
            m_nMode = eRadioTrackMode::RADIO_STARTING;
            if (IsVehicleRadioActive()) {
                m_bDisplayStationName = true;
            }
            m_bInitialised = false;
        }
    }

    auto& as = m_ActiveSettings;
    switch (m_nMode) {
    case eRadioTrackMode::RADIO_STARTING: {
        if (as.PlayTime < 0) {
            as.PlayTime = 0;
        }
        if (m_bBassEnhance) {
            AEAudioHardware.SetBassSetting(as.BassSetting, as.BassGain);
        } else {
            AEAudioHardware.SetBassSetting(eBassSetting::NORMAL, as.BassGain);
        }
        AEAudioHardware.PlayTrack(
            as.TrackQueue[0],
            as.TrackQueue[1],
            as.PlayTime,
            as.TrackFlags,
            as.TrackTypes[0] == TYPE_USER_TRACK,
            as.TrackTypes[1] == TYPE_USER_TRACK
        );
        m_nMode = eRadioTrackMode::RADIO_WAITING_TO_PLAY;
        break;
    }
    case eRadioTrackMode::RADIO_WAITING_TO_PLAY: {
        if (as.PlayTime == -2) {
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_STOP);
            StartTrackPlayback();
            field_60 = CTimer::GetTimeInMS();
            m_aRadioState[as.StationID].m_iTimeInPauseModeInMs = CTimer::GetTimeInMSPauseMode();
            m_nMode = eRadioTrackMode::RADIO_PLAYING;
        } else if (as.PlayTime == -8) {
            if (as.CurrTrackID == as.TrackQueue[1] || (as.CurrTrackID == as.TrackQueue[0] && as.TrackQueue[1] == -1)) {
                as.TrackQueue[0] = ChooseAdvertIndex(RADIO_USER_TRACKS);
                as.TrackTypes[0] = TYPE_ADVERT;
                as.TrackQueue[1] = AEUserRadioTrackManager.SelectUserTrackIndex();
                as.TrackTypes[1] = TYPE_USER_TRACK;
                as.TrackIndices[1] = (int8)as.TrackQueue[1];
                m_nMode = eRadioTrackMode::RADIO_STARTING;
            }
        }
        break;
    }
    case eRadioTrackMode::RADIO_PLAYING: {
        if (as.StationID == RADIO_USER_TRACKS && as.PlayTime == -6) {
            if (AEAudioHardware.GetActiveTrackID() == as.TrackQueue[0] && as.TrackQueue[1] != -1) {
                as.TrackQueue[0] = as.TrackQueue[1];
                as.TrackIndices[0] = as.TrackIndices[1];
                as.TrackTypes[0] = as.TrackTypes[1];
            } else {
                as.TrackQueue[0] = AEUserRadioTrackManager.SelectUserTrackIndex();
                as.TrackTypes[0] = TYPE_USER_TRACK;
                as.TrackIndices[0] = (int8)as.TrackQueue[0];
            }
            as.TrackQueue[1] = AEUserRadioTrackManager.SelectUserTrackIndex();
            as.TrackTypes[1] = TYPE_USER_TRACK;
            as.TrackIndices[1] = (int8)as.TrackQueue[1];
            m_nMode = eRadioTrackMode::RADIO_STARTING;
        }

        // Remember that the track that is currently playing has been played
        const auto AddToHistory = [this](int32 trackID, int8 trackType, int8 trackIdx) {
            switch (trackType) {
            case TYPE_INTRO:
            case TYPE_TRACK:
            case TYPE_OUTRO:
            case TYPE_USER_TRACK:
                AddMusicTrackIndexToHistory(m_ActiveSettings.StationID, trackIdx);
                break;
            case TYPE_INDENT:
                AddIdentIndexToHistory(m_ActiveSettings.StationID, (int8)trackID);
                break;
            case TYPE_ADVERT:
                AddAdvertIndexToHistory(m_ActiveSettings.StationID, (int8)trackID);
                break;
            case TYPE_DJ_BANTER:
                AddDJBanterIndexToHistory(m_ActiveSettings.StationID, (int8)trackID);
                break;
            }
        };
        if (as.TrackQueue[0] == as.CurrTrackID) {
            AddToHistory(as.TrackQueue[0], as.TrackTypes[0], as.TrackIndices[0]);
            as.CurrTrackType = as.TrackTypes[0];
            as.CurrTrackIdx = as.TrackIndices[0];
        } else if (as.PrevTrackID == as.CurrTrackID) {
            AddToHistory(as.PrevTrackID, as.PrevTrackType, as.PrevTrackIdx);
            as.CurrTrackType = as.PrevTrackType;
            as.CurrTrackIdx = as.PrevTrackIdx;
        }

        // Skip the user track
        if (as.StationID == RADIO_USER_TRACKS && (as.TrackTypes[0] == TYPE_USER_TRACK || AEUserRadioTrackManager.GetUserTrackPlayMode() == 0)) {
            if (CPad::GetPad(0)->IsRadioTrackSkipPressed()) {
                StopRadio(nullptr, true);
                while (m_nMode != eRadioTrackMode::RADIO_STOPPED || m_bInitialised || m_nStationsListed != 0 || m_nStationsListDown != 0) {
                    Service(AEAudioHardware.GetTrackPlayTime());
                    AEAudioHardware.Service();
                }
                StartRadio(as.StationID, as.BassSetting, as.BassGain, true);
            }
        }

        CheckForPause();
        UpdateRadioVolumes();
        CheckForTrackConcatenation();
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RADIO_RETUNE_STOP);
        break;
    }
    case eRadioTrackMode::RADIO_STOPPING:
    case eRadioTrackMode::RADIO_STOPPING_CHANNELS_STOPPED: {
        AEAudioHardware.StopTrack();
        m_nMode = eRadioTrackMode::RADIO_WAITING_TO_STOP;
        break;
    }
    case eRadioTrackMode::RADIO_STOPPING_SILENCED: {
        m_nMode = eRadioTrackMode::RADIO_STOPPING_CHANNELS_STOPPED;
        break;
    }
    case eRadioTrackMode::RADIO_WAITING_TO_STOP: {
        if (as.PlayTime == -6 || as.PlayTime == -8) {
            m_nMode = eRadioTrackMode::RADIO_STOPPED;
        } else if (as.PlayTime == -7 || as.PlayTime == -2) {
            AEAudioHardware.StopTrack();
        }
        break;
    }
    default:
        break;
    }
}

// 0x5D40E0
void CAERadioTrackManager::Load() {
    for (auto r = 0; r < RADIO_COUNT; r++) {
        for (auto& historyIndex : m_nMusicTrackIndexHistory[r].indices) {
            CGenericGameStorage::LoadDataFromWorkBuffer(historyIndex);
        }

        for (auto& identIndex : m_nIdentIndexHistory[r].indices) {
            CGenericGameStorage::LoadDataFromWorkBuffer(identIndex);
        }

        for (auto& advertIndex : m_nAdvertIndexHistory[r].indices) {
            CGenericGameStorage::LoadDataFromWorkBuffer(advertIndex);
        }

        for (auto& banterIndex : m_nDJBanterIndexHistory[r].indices) {
            CGenericGameStorage::LoadDataFromWorkBuffer(banterIndex);
        }
    }

    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsCitiesPassed);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedCasino3);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedCasino6);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedCasino10);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedCat1);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedDesert1);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedDesert3);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedDesert5);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedDesert8);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedDesert10);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedFarlie3);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedLAFin2);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedMansion2);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedRyder2);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedRiot1);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedSCrash1);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedStrap4);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedSweet2);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedTruth2);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsPassedVCrash2);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsStartedBadlands);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsStartedCat2);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsStartedCrash1);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsLastHitGameClockDays);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsLastHitGameClockHours);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nStatsLastHitTimeOutHours);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nSpecialDJBanterPending);
    CGenericGameStorage::LoadDataFromWorkBuffer(m_nSpecialDJBanterIndex);
}

// 0x5D3EE0
void CAERadioTrackManager::Save() {
    for (auto r = 0; r < RADIO_COUNT; r++) {
        for (auto& historyIndex : m_nMusicTrackIndexHistory[r].indices) {
            CGenericGameStorage::SaveDataToWorkBuffer(historyIndex);
        }

        for (auto& identIndex : m_nIdentIndexHistory[r].indices) {
            CGenericGameStorage::SaveDataToWorkBuffer(identIndex);
        }

        for (auto& advertIndex : m_nAdvertIndexHistory[r].indices) {
            CGenericGameStorage::SaveDataToWorkBuffer(advertIndex);
        }

        for (auto& banterIndex : m_nDJBanterIndexHistory[r].indices) {
            CGenericGameStorage::SaveDataToWorkBuffer(banterIndex);
        }
    }

    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsCitiesPassed);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedCasino3);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedCasino6);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedCasino10);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedCat1);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedDesert1);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedDesert3);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedDesert5);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedDesert8);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedDesert10);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedFarlie3);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedLAFin2);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedMansion2);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedRyder2);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedRiot1);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedSCrash1);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedStrap4);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedSweet2);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedTruth2);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsPassedVCrash2);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsStartedBadlands);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsStartedCat2);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsStartedCrash1);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsLastHitGameClockDays);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsLastHitGameClockHours);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nStatsLastHitTimeOutHours);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nSpecialDJBanterPending);
    CGenericGameStorage::SaveDataToWorkBuffer(m_nSpecialDJBanterIndex);
}
