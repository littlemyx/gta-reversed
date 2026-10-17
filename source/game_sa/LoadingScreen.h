/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once


enum class eLoadingLogo : uint8 {
    EAX = 0,
    NVIDIA = 1
};

enum class eDisplayedSplash : int32 {
    NONE = -1,

    COPYRIGHT = 0,
    SPLASH_1_NO_BAR = 1,
    SPLASH2,
    SPLASH3,
    SPLASH4,
    SPLASH5,
    SPLASH6,

    COUNT
};

class CSprite2d;

class CLoadingScreen {
private:
    static inline NOTSA_GLOBAL(m_bActive, 0xBAB318, (bool), {});
    static inline NOTSA_GLOBAL(m_bWantToPause, 0xBAB319, (bool), {}); // unused
    static inline NOTSA_GLOBAL(m_bPaused, 0xBAB31A, (bool), {}); // unused
    static bool m_bForceShutdown; // unknown, unused
    static inline NOTSA_GLOBAL(m_aSplashes, 0xBAB35C, (std::array<CSprite2d, +eDisplayedSplash::COUNT>), {});
    static inline NOTSA_GLOBAL(m_currDisplayedSplash, 0x8D093C, (eDisplayedSplash), { eDisplayedSplash::NONE });

    static inline NOTSA_GLOBAL(m_bFading, 0xBAB31C, (bool), {});
    static inline NOTSA_GLOBAL(m_bLegalScreen, 0xBAB31D, (bool), {});
    static inline NOTSA_GLOBAL(m_bFadeInNextSplashFromBlack, 0xBAB31E, (bool), {});
    static inline NOTSA_GLOBAL(m_bFadeOutCurrSplashToBlack, 0xBAB31F, (bool), {});
    static inline NOTSA_GLOBAL(m_FadeAlpha, 0xBAB320, (uint8), {});
    static inline NOTSA_GLOBAL(m_StartFadeTime, 0xBAB324, (float), {});
    static inline NOTSA_GLOBAL(m_ClockTimeOnPause, 0xBAB328, (float), {}); // unused
    static inline NOTSA_GLOBAL(m_PauseTime, 0xBAB32C, (float), {}); // second

    static inline NOTSA_GLOBAL(m_PercentLoaded, 0xBAB330, (float), {});
    static inline NOTSA_GLOBAL(m_TimeStartedLoading, 0xBAB334, (float), {}); // unused
    static inline NOTSA_GLOBAL(m_TimeBarAppeared, 0xBAB338, (float), {});

    static inline NOTSA_GLOBAL(m_LoadingGxtMsg1, 0xBAB2C8, (GxtChar[80]), {});
    static inline NOTSA_GLOBAL(m_LoadingGxtMsg2, 0xBAB278, (GxtChar[80]), {});

    static inline NOTSA_GLOBAL(m_PopUpMessage, 0xBAB268, (char[16]), {});

    static inline NOTSA_GLOBAL(m_bSignalDelete, 0xBAB33C, (bool), {}); // unused
    static inline NOTSA_GLOBAL(m_bReadyToDelete, 0xBAB33D, (bool), {});

    static inline NOTSA_GLOBAL(m_numChunksLoaded, 0x8D0940, (int32), { -1 }); // -1
    // static int32 m_nChunksToLoad; // Added in Mobile
    static inline NOTSA_GLOBAL(m_chunkBarAppeared, 0x8D0944, (int32), { -1 }); // -1
    static inline NOTSA_GLOBAL(m_timeSinceLastScreen, 0xBAB340, (float), {});

public:
    static void LoadSplashes(bool useSplashId, eLoadingLogo id);

    [[nodiscard]] static bool IsActive() { return m_bActive; } // 0x744DB5
    [[nodiscard]] static bool IsPaused() { return m_bPaused; }

    static void Init(bool isLegalScreen, bool isReloadDisabled);
    static void Shutdown(bool isForce = false);

    // static bool Paused(); // unknown
    static void Pause();
    static void Continue();

    static void SetLoadingBarMsg(const char* msg, const char* msg2);

    static void DisplayMessage(const char* msg);
    static void DisplayNextSplash();
    static void RenderSplash();
    static void NewChunkLoaded();

    // static void Pump(); // Added in Mobile

    // static void SetChunksToLoad(int32 num, bool AddTo); // Added in Mobile

    static void DoPCTitleFadeOut();
    static void DoPCTitleFadeIn();
    static void DoPCScreenChange(bool isLastOne);

private:
    static void DisplayPCScreen();

    static void RenderLoadingBar();

    static void Update();

    static void StartFading();

    [[nodiscard]] static float GetClockTime(bool isRealTime = true);

private: // NOTSA
    friend void InjectHooksMain();
    static void InjectHooks();

public:
    static CSprite2d& GetCurrentDisplayedSplash() { return m_aSplashes[+m_currDisplayedSplash]; }

    // Skips the copyright splash
    static void SkipCopyrightSplash();
};

void LoadingScreen(const char* msg1, const char* msg2 = nullptr, const char* msg3 = nullptr);
