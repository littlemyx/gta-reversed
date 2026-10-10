/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

enum {
    TYPE_INDENT      = 0,
    TYPE_ADVERT      = 1,
    TYPE_DJ_BANTER   = 2,
    TYPE_INTRO       = 3,
    TYPE_TRACK       = 4,
    TYPE_OUTRO       = 5,
    TYPE_NONE        = 6,
    TYPE_USER_TRACK  = 7,
};

struct tRadioSettings {
    static constexpr size_t NUM_TRACKS = 5u;

    tRadioSettings(eRadioID currentStation = RADIO_OFF) :
        StationID(currentStation)
    {
        rng::fill(TrackQueue, -1);
        rng::fill(TrackTypes, TYPE_NONE);
        rng::fill(TrackIndices, -1);
    }

    void Reset() {
        *this = {};
    }

    void SwitchToNextTrack() {
        PrevTrackID   = TrackQueue.front();
        PrevTrackType = TrackTypes.front();
        PrevTrackIdx  = TrackIndices.front();

        const auto Rotate = [](auto& arr, auto invalidValue) {
            std::copy(arr.begin() + 1, arr.end(), arr.begin());
            arr.back() = invalidValue;
        };
        Rotate(TrackQueue,   -1);
        Rotate(TrackTypes,   TYPE_NONE);
        Rotate(TrackIndices, -1);
    }

    std::array<int32, NUM_TRACKS> TrackQueue{ -1 };
    int32                         CurrTrackID{ -1 };
    int32                         PrevTrackID{ -1 };
    int32                         PlayTime{ 0 };
    int32                         TrackLengthMs{ 0 };
    int8                          TrackFlags{ 2 };        // TODO: enum
    eRadioID                      StationID{ RADIO_OFF }; // NOTSA init value.
    eBassSetting                  BassSetting{ eBassSetting::NORMAL };
    float                         BassGain{}; // unk. init
    std::array<int8, NUM_TRACKS>  TrackTypes{ TYPE_NONE };
    int8                          CurrTrackType{ TYPE_NONE };
    int8                          PrevTrackType{ TYPE_NONE };
    std::array<int8, NUM_TRACKS>  TrackIndices{ -1 };
    int8                          CurrTrackIdx{ -1 }; //!< Index into `TrackIndices`
    int8                          PrevTrackIdx{ -1 }; //!< Index into `TrackIndices`
};
VALIDATE_SIZE(tRadioSettings, 0x3C);

struct tRadioState {
    std::array<int32, 3> m_aElapsed{0};
    int32 m_iTimeInPauseModeInMs{-1};
    int32 m_iTimeInMs{-1};
    int32 m_iTrackPlayTime{-1};
    std::array<int32, 3> m_aTrackQueue{-1};
    std::array<int8, 3>  m_aTrackTypes{TYPE_NONE};
    int8 m_nGameClockDays{-1};
    int8 m_nGameClockHours{-1};

    void Reset(bool paused = false) {
        rng::fill(m_aElapsed, 0);
        rng::fill(m_aTrackQueue, -1);
        rng::fill(m_aTrackTypes, TYPE_NONE);

        m_iTimeInMs = -1;
        if (!paused)
            m_iTimeInPauseModeInMs = -1;
        m_iTrackPlayTime = -1;
        m_nGameClockDays = -1;
        m_nGameClockHours = -1;
    }
};
VALIDATE_SIZE(tRadioState, 0x2C);

//! Inclusive range of sound IDs. `Start == 0x782` is used for "there are none".
struct tRadioSoundRange {
    int32 Start;
    int32 End;
};
VALIDATE_SIZE(tRadioSoundRange, 0x8);

//typedef int8 RadioStationId; => eRadioID

// NOTSA
template<typename T, size_t Count>
struct tRadioIndexHistory {
    std::array<T, Count> indices{-1};

    void Reset() {
        rng::fill(indices, -1);
    }

    void PutAtFirst(int32 index) {
        if constexpr (Count > 1) {
            // rotate all elements to right.
            std::rotate(indices.rbegin(), indices.rbegin() + 1, indices.rend());
        }
        indices[0] = index;
    }
};
static_assert(sizeof(tRadioIndexHistory<int32, 1>) == sizeof(int32)); // No VALIDATE_SIZE because the preprocessor is dumb
template<typename T> inline constexpr T kRadioIndexHistoryZero = std::bit_cast<T>(std::array<std::byte, sizeof(T)>{}); // an all-zero object: the exe's .bss value; the NSDMI -1 defaults must not run for these globals in detached mode
enum class eRadioTrackMode {
    RADIO_STARTING,
    RADIO_WAITING_TO_PLAY,
    RADIO_PLAYING,
    RADIO_STOPPING, // ?
    RADIO_STOPPING_SILENCED,
    RADIO_STOPPING_CHANNELS_STOPPED,
    RADIO_WAITING_TO_STOP,
    RADIO_STOPPED
};

class CAERadioTrackManager {
public:
    bool            m_bInitialised{false};
    bool            m_bDisplayStationName{false};
    char            m_prev{0}; // TODO: make sense of this.
    bool            m_bEnabledInPauseMode{false};
    bool            m_bBassEnhance{true};
    bool            m_bPauseMode{false};
    bool            m_bRetuneJustStarted{false};
    bool            m_bRadioAutoSelect{true};
    std::array<uint8, RADIO_COUNT>       m_nTracksInARow{};
    uint8           m_nSavedGameClockDays{0xff};
    uint8           m_nSavedGameClockHours{0xff};
    std::array<int32, RADIO_COUNT>       m_aListenTimes{}; // Filled from `CStats::FavoriteRadioStationList`
    uint32          m_nTimeRadioStationRetuned{0};
    uint32          m_nTimeToDisplayRadioName{0};
    uint32          m_nSavedTimeMs{0};
    uint32          m_nRetuneStartedTime;
    uint32          field_60{0}; //!< Time (in ms) when the current radio started playing
    int32           m_HwClientHandle;
    eRadioTrackMode m_nMode{eRadioTrackMode::RADIO_STOPPED};
    int32           m_nStationsListed{0};
    int32           m_nStationsListDown{0};
    int32           m_nSavedRadioStationId{-1};         // TODO: convert to eRadioID after finished reversing
    int32           m_iRadioStationMenuRequest{ -1 };   // <-
    int32           m_iRadioStationScriptRequest{ -1 }; // <-
    float           m_f80{0.0f}; // 80 and 84 volume related fields. See ::UpdateRadioVolumes
    float           m_f84{0.0f};
    tRadioSettings  m_RequestedSettings{}; // settings1
    tRadioSettings  m_ActiveSettings{}; // settings2
    std::array<tRadioState, RADIO_COUNT> m_aRadioState{};
    uint32          field_368{0};
    uint8           m_nUserTrackPlayMode{};

public:
    static constexpr auto DJBANTER_INDEX_HISTORY_COUNT = 15;
    static constexpr auto ADVERT_INDEX_HISTORY_COUNT   = 40;
    static constexpr auto IDENT_INDEX_HISTORY_COUNT    = 8;
    static constexpr auto MUSIC_TRACK_HISTORY_COUNT    = 20;
    using DJBanterIndexHistory = tRadioIndexHistory<int32, DJBANTER_INDEX_HISTORY_COUNT>;
    using AdvertIndexHistory   = tRadioIndexHistory<int32, ADVERT_INDEX_HISTORY_COUNT>;
    using IdentIndexHistory    = tRadioIndexHistory<int32, IDENT_INDEX_HISTORY_COUNT>;
    using MusicTrackHistory    = tRadioIndexHistory<int8, MUSIC_TRACK_HISTORY_COUNT>;

    static inline NOTSA_GLOBAL(m_nDJBanterIndexHistory, 0xB61D78, (DJBanterIndexHistory[RADIO_COUNT]), { kRadioIndexHistoryZero<DJBanterIndexHistory>, kRadioIndexHistoryZero<DJBanterIndexHistory>, kRadioIndexHistoryZero<DJBanterIndexHistory>, kRadioIndexHistoryZero<DJBanterIndexHistory>, kRadioIndexHistoryZero<DJBanterIndexHistory>, kRadioIndexHistoryZero<DJBanterIndexHistory>, kRadioIndexHistoryZero<DJBanterIndexHistory>, kRadioIndexHistoryZero<DJBanterIndexHistory>, kRadioIndexHistoryZero<DJBanterIndexHistory>, kRadioIndexHistoryZero<DJBanterIndexHistory>, kRadioIndexHistoryZero<DJBanterIndexHistory>, kRadioIndexHistoryZero<DJBanterIndexHistory>, kRadioIndexHistoryZero<DJBanterIndexHistory>, kRadioIndexHistoryZero<DJBanterIndexHistory> }); // 210
    static inline NOTSA_GLOBAL(m_nAdvertIndexHistory, 0xB620C0, (AdvertIndexHistory[RADIO_COUNT]), { kRadioIndexHistoryZero<AdvertIndexHistory>, kRadioIndexHistoryZero<AdvertIndexHistory>, kRadioIndexHistoryZero<AdvertIndexHistory>, kRadioIndexHistoryZero<AdvertIndexHistory>, kRadioIndexHistoryZero<AdvertIndexHistory>, kRadioIndexHistoryZero<AdvertIndexHistory>, kRadioIndexHistoryZero<AdvertIndexHistory>, kRadioIndexHistoryZero<AdvertIndexHistory>, kRadioIndexHistoryZero<AdvertIndexHistory>, kRadioIndexHistoryZero<AdvertIndexHistory>, kRadioIndexHistoryZero<AdvertIndexHistory>, kRadioIndexHistoryZero<AdvertIndexHistory>, kRadioIndexHistoryZero<AdvertIndexHistory>, kRadioIndexHistoryZero<AdvertIndexHistory> }); // 560
    static inline NOTSA_GLOBAL(m_nIdentIndexHistory, 0xB62980, (IdentIndexHistory[RADIO_COUNT]), { kRadioIndexHistoryZero<IdentIndexHistory>, kRadioIndexHistoryZero<IdentIndexHistory>, kRadioIndexHistoryZero<IdentIndexHistory>, kRadioIndexHistoryZero<IdentIndexHistory>, kRadioIndexHistoryZero<IdentIndexHistory>, kRadioIndexHistoryZero<IdentIndexHistory>, kRadioIndexHistoryZero<IdentIndexHistory>, kRadioIndexHistoryZero<IdentIndexHistory>, kRadioIndexHistoryZero<IdentIndexHistory>, kRadioIndexHistoryZero<IdentIndexHistory>, kRadioIndexHistoryZero<IdentIndexHistory>, kRadioIndexHistoryZero<IdentIndexHistory>, kRadioIndexHistoryZero<IdentIndexHistory>, kRadioIndexHistoryZero<IdentIndexHistory> }); // 112
    static inline NOTSA_GLOBAL(m_nMusicTrackIndexHistory, 0xB62B40, (MusicTrackHistory[RADIO_COUNT]), { kRadioIndexHistoryZero<MusicTrackHistory>, kRadioIndexHistoryZero<MusicTrackHistory>, kRadioIndexHistoryZero<MusicTrackHistory>, kRadioIndexHistoryZero<MusicTrackHistory>, kRadioIndexHistoryZero<MusicTrackHistory>, kRadioIndexHistoryZero<MusicTrackHistory>, kRadioIndexHistoryZero<MusicTrackHistory>, kRadioIndexHistoryZero<MusicTrackHistory>, kRadioIndexHistoryZero<MusicTrackHistory>, kRadioIndexHistoryZero<MusicTrackHistory>, kRadioIndexHistoryZero<MusicTrackHistory>, kRadioIndexHistoryZero<MusicTrackHistory>, kRadioIndexHistoryZero<MusicTrackHistory>, kRadioIndexHistoryZero<MusicTrackHistory> }); // 280

    static inline NOTSA_GLOBAL(m_nStatsLastHitTimeOutHours, 0xB62C58, (uint8), {}); // = -1;
    static inline NOTSA_GLOBAL(m_nStatsLastHitGameClockHours, 0xB62C59, (uint8), {}); // = -1;
    static inline NOTSA_GLOBAL(m_nStatsLastHitGameClockDays, 0xB62C5A, (uint8), {}); // = -1;
    static inline NOTSA_GLOBAL(m_nStatsStartedCrash1, 0xB62C5B, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsStartedCat2, 0xB62C5C, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsStartedBadlands, 0xB62C5D, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedVCrash2, 0xB62C5E, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedTruth2, 0xB62C5F, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedSweet2, 0xB62C60, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedStrap4, 0xB62C61, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedSCrash1, 0xB62C62, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedRiot1, 0xB62C63, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedRyder2, 0xB62C64, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedMansion2, 0xB62C65, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedLAFin2, 0xB62C66, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedFarlie3, 0xB62C67, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedDesert10, 0xB62C68, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedDesert8, 0xB62C69, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedDesert5, 0xB62C6A, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedDesert3, 0xB62C6B, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedDesert1, 0xB62C6C, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedCat1, 0xB62C6D, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedCasino10, 0xB62C6E, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedCasino6, 0xB62C6F, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsPassedCasino3, 0xB62C70, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nStatsCitiesPassed, 0xB62C71, (uint8), {}); // = 0;
    static inline NOTSA_GLOBAL(m_nSpecialDJBanterIndex, 0xB62C72, (uint8), {}); // = -1;
    static inline NOTSA_GLOBAL(m_nSpecialDJBanterPending, 0xB62C73, (uint8), {}); // = 3; // ?

public:
    static void InjectHooks();

    CAERadioTrackManager(int32 hwClientHandle);

    CAERadioTrackManager() = default; // NOTSA
    ~CAERadioTrackManager() = default;

    bool Initialise(int32 channelId);
    void InitialiseRadioStationID(eRadioID id);

    void Reset();
    static void ResetStatistics();

    bool   IsRadioOn() const;
    bool   HasRadioRetuneJustStarted() const;
    eRadioID GetCurrentRadioStationID() const;
    int32* GetRadioStationListenTimes();
    void   SetRadioAutoRetuneOnOff(bool enable);
    void   SetBassEnhanceOnOff(bool enable);
    void   SetBassSetting(eBassSetting bassSetting, float bassGrain);
    void   RetuneRadio(eRadioID radioId);

    void  DisplayRadioStationName();
    const GxtChar* GetRadioStationName(eRadioID id);
    void  GetRadioStationNameKey(eRadioID id, char* outStr);
    static bool IsVehicleRadioActive();

    void StartTrackPlayback();
    void UpdateRadioVolumes();
    void PlayRadioAnnouncement(uint32);
    void StartRadio(eRadioID id, eBassSetting bassSetting, float bassGain, bool skipTrack);
    void StartRadio(const tVehicleAudioSettings& settings);
    void StopRadio(tVehicleAudioSettings* settings, bool bDuringPause);

    void Service(int32 playTime);

    static void Load();
    static void Save();

protected:
    void AddMusicTrackIndexToHistory(eRadioID id, int8 trackIndex);
    void AddIdentIndexToHistory(eRadioID id, int8 trackIndex);
    void AddAdvertIndexToHistory(eRadioID id, int8 trackIndex);
    void AddDJBanterIndexToHistory(eRadioID id, int8 trackIndex);

    void  ChooseTracksForStation(eRadioID id);
    int32 ChooseIdentIndex(eRadioID id);
    int32 ChooseAdvertIndex(eRadioID id);
    int32 ChooseDJBanterIndex(eRadioID id);
    int32 ChooseDJBanterIndexFromList(eRadioID id, const tRadioSoundRange* list);
    int8  ChooseMusicTrackIndex(eRadioID id);
    static int8  ChooseTalkRadioShow();

    void CheckForTrackConcatenation();
    static void CheckForMissionStatsChanges();
    void CheckForStationRetune();
    void CheckForStationRetuneDuringPause();
    void CheckForPause();

    bool QueueUpTracksForStation(eRadioID id, int8* iTrackCount, int8 radioState, tRadioSettings& settings);
    bool TrackRadioStation(eRadioID id, bool skipTrack);
};
VALIDATE_SIZE(CAERadioTrackManager, 0x370);

extern CAERadioTrackManager& AERadioTrackManager;
