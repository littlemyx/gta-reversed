/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "Stats.h"
#include "eHud.h"
#include <GxtChar.h>

class CSprite2d;
class CPed;
class CPlayerInfo;
class CPlayerPed;

class CHud {
public:
    static constexpr auto BIG_MESSAGE_SIZE = 128;

    static inline NOTSA_GLOBAL(bScriptDontDisplayAreaName, 0xBAA3F8, (bool), {});
    static inline NOTSA_GLOBAL(bScriptDontDisplayVehicleName, 0xBAA3F9, (bool), {});
    static inline NOTSA_GLOBAL(bScriptForceDisplayWithCounters, 0xBAA3FA, (bool), {});
    static inline NOTSA_GLOBAL(bScriptDontDisplayRadar, 0xBAA3FB, (bool), {});

    static inline NOTSA_GLOBAL(bDrawClock, 0xBAA400, (bool), {});

    static inline NOTSA_GLOBAL(m_pVehicleNameToPrint, 0xBAA444, (const GxtChar*), {});
    static inline NOTSA_GLOBAL(m_VehicleState, 0xBAA448, (eNameState), {});
    static inline NOTSA_GLOBAL(m_VehicleFadeTimer, 0xBAA44C, (int32), {});
    static inline NOTSA_GLOBAL(m_VehicleNameTimer, 0xBAA450, (int32), {});
    static inline NOTSA_GLOBAL(m_pLastVehicleName, 0xBAA454, (const GxtChar*), {});
    static inline NOTSA_GLOBAL(m_pVehicleName, 0xBAA458, (const GxtChar*), {});

    static inline NOTSA_GLOBAL(m_bDraw3dMarkers, 0xBAA45C, (bool), {});
    static inline NOTSA_GLOBAL(m_Wants_To_Draw_Hud, 0xBAA45D, (bool), {});

    static inline NOTSA_GLOBAL(m_fHelpMessageTime, 0xBAA460, (float), {}); // in seconds
    static inline NOTSA_GLOBAL(m_fHelpMessageBoxWidth, 0x8D0934, (float), { 200.0f }); // default 200.0
    static inline NOTSA_GLOBAL(m_bHelpMessagePermanent, 0xBAA464, (bool), {});
    static inline NOTSA_GLOBAL(m_fHelpMessageStatUpdateValue, 0xBAA468, (float), {});
    static inline NOTSA_GLOBAL(m_nHelpMessageMaxStatValue, 0xBAA46C, (uint16), {});
    static inline NOTSA_GLOBAL(m_nHelpMessageStatId, 0xBAA470, (uint16), {});
    static inline NOTSA_GLOBAL(m_bHelpMessageQuick, 0xBAA472, (bool), {});
    static inline NOTSA_GLOBAL(m_nHelpMessageState, 0xBAA474, (int32), {});
    static inline NOTSA_GLOBAL(m_nHelpMessageFadeTimer, 0xBAA478, (uint32), {});
    static inline NOTSA_GLOBAL(m_nHelpMessageTimer, 0xBAA47C, (uint32), {});
    static inline NOTSA_GLOBAL(m_pHelpMessageToPrint, 0xBAA480, (GxtChar[400]), {});
    static inline NOTSA_GLOBAL(m_pLastHelpMessage, 0xBAA610, (GxtChar[400]), {});
    static inline NOTSA_GLOBAL(m_pHelpMessage, 0xBAA7A0, (GxtChar[400]), {});

    static inline NOTSA_GLOBAL(m_ZoneState, 0xBAA930, (eNameState), {});
    static inline NOTSA_GLOBAL(m_ZoneFadeTimer, 0xBAA934, (int32), {});
    static inline NOTSA_GLOBAL(m_ZoneNameTimer, 0xBAA938, (uint32), {});
    static inline NOTSA_GLOBAL(m_ZoneToPrint, 0xBAB1D0, (const GxtChar*), {});
    static inline NOTSA_GLOBAL(m_pLastZoneName, 0xBAB1D4, (const GxtChar*), {});
    static inline NOTSA_GLOBAL(m_pZoneName, 0xBAB1D8, (const GxtChar*), {});

    static inline NOTSA_GLOBAL(m_ItemToFlash, 0xBAB1DC, (eHudItem), {});
    static inline NOTSA_GLOBAL(bDrawingVitalStats, 0xBAB1DE, (bool), {});

    static inline NOTSA_GLOBAL(m_LastBreathTime, 0xBAA3FC, (int32), {});

    static inline NOTSA_GLOBAL(m_WeaponState, 0xBAA404, (uint32), {});
    static inline NOTSA_GLOBAL(m_WeaponFadeTimer, 0xBAA408, (uint32), {});
    static inline NOTSA_GLOBAL(m_WeaponTimer, 0xBAA40C, (uint32), {});
    static inline NOTSA_GLOBAL(m_LastWeapon, 0xBAA410, (uint32), {});

    static inline NOTSA_GLOBAL(m_WantedState, 0xBAA414, (uint32), {});
    static inline NOTSA_GLOBAL(m_WantedFadeTimer, 0xBAA418, (uint32), {});
    static inline NOTSA_GLOBAL(m_WantedTimer, 0xBAA41C, (uint32), {});
    static inline NOTSA_GLOBAL(m_LastWanted, 0xBAA420, (uint32), {});

    static inline NOTSA_GLOBAL(m_DisplayScoreState, 0xBAA424, (uint32), {});
    static inline NOTSA_GLOBAL(m_DisplayScoreFadeTimer, 0xBAA428, (uint32), {});
    static inline NOTSA_GLOBAL(m_DisplayScoreTimer, 0xBAA42C, (uint32), {});
    static inline NOTSA_GLOBAL(m_LastDisplayScore, 0xBAA430, (uint32), {});

    static inline NOTSA_GLOBAL(m_EnergyLostState, 0xBAA434, (uint32), {});
    static inline NOTSA_GLOBAL(m_EnergyLostFadeTimer, 0xBAA438, (uint32), {});
    static inline NOTSA_GLOBAL(m_EnergyLostTimer, 0xBAA43C, (uint32), {});
    static inline NOTSA_GLOBAL(m_LastTimeEnergyLost, 0xBAA440, (uint32), {});

    static inline NOTSA_GLOBAL(m_Message, 0xBAB040, (GxtChar[400]), {});
    static inline NOTSA_GLOBAL(m_BigMessage, 0xBAACC0, (GxtChar[NUM_MESSAGE_STYLES][BIG_MESSAGE_SIZE]), {});
    static inline NOTSA_GLOBAL(LastBigMessage, 0xBAA940, (GxtChar[NUM_MESSAGE_STYLES][BIG_MESSAGE_SIZE]), {});
    static inline NOTSA_GLOBAL(BigMessageAlpha, 0xBAA3A4, (float[NUM_MESSAGE_STYLES]), {});
    static inline NOTSA_GLOBAL(BigMessageInUse, 0xBAA3C0, (float[NUM_MESSAGE_STYLES]), {});
    static inline NOTSA_GLOBAL(BigMessageX, 0xBAA3DC, (float[NUM_MESSAGE_STYLES]), {});

    static inline auto& Sprites = StaticRef<std::array<CSprite2d, 6>>(0xBAB1FC);

    static inline NOTSA_GLOBAL(TimerMainCounterHideState, 0xBAA388, (int16), {});
    static inline NOTSA_GLOBAL(TimerMainCounterWasDisplayed, 0xBAA38A, (bool), {});
    static inline NOTSA_GLOBAL(TimerCounterHideState, 0xBAA38C, (std::array<int16, 4>), {});
    static inline NOTSA_GLOBAL(TimerCounterWasDisplayed, 0xBAA394, (std::array<bool, 4>), {});

    static inline NOTSA_GLOBAL(OddJob2OffTimer, 0xBAA398, (float), {});
    static inline NOTSA_GLOBAL(OddJob2XOffset, 0xBAA39C, (float), {});
    static inline NOTSA_GLOBAL(OddJob2Timer, 0xBAA3A0, (uint16), {});
    static inline NOTSA_GLOBAL(OddJob2On, 0xBAB1E0, (uint16), {});

    static inline NOTSA_GLOBAL(PagerXOffset, 0x8D0938, (float), { 150.0f }); // 150.0f
    static inline NOTSA_GLOBAL(HelpTripSkipShown, 0xBAB229, (bool), {});

public:
    static void InjectHooks();

    static void Initialise();
    static void ReInitialise();
    static void Shutdown();

    static void GetRidOfAllHudMessages(bool arg0);
    static float GetYPosBasedOnHealth(uint8 playerId, float pos, int8 offset);
    static bool HelpMessageDisplayed();

    static void SetMessage(const GxtChar* message);
    static void SetBigMessage(GxtChar* message, eMessageStyle style);
    static void SetHelpMessage(const GxtChar* text, bool quickMessage = false, bool permanent = false, bool addToBrief = false);
    static void SetHelpMessageStatUpdate(eStatUpdateState state, uint16 statId, float diff, float max);
    static void SetHelpMessageWithNumber(const GxtChar* text, int32 number, bool quickMessage, bool permanent);
    static void SetVehicleName(const GxtChar* name);
    static void SetZoneName(const GxtChar* name, bool displayImmediately);

    static void Draw();
    static void DrawAfterFade();
    static void DrawAreaName();
    static void DrawBustedWastedMessage();
    static void ResetWastedText();
    static void DrawCrossHairs();
    static float DrawFadeState(DRAW_FADE_STATE fadeState, int32 arg1);
    static void DrawHelpText();
    static void DrawMissionTimers();
    static void DrawMissionTitle();
    static void DrawOddJobMessage(bool displayImmediately);
    static void DrawRadar();
    static void DrawScriptText(bool displayImmediately);
    static void DrawSubtitles();
    static void DrawSuccessFailedMessage();
    static void DrawVehicleName();
    static void DrawVitalStats();
    static void DrawAmmo(CPed* ped, int32 x, int32 y, float alpha);
    static void DrawPlayerInfo();
    static inline void DrawClock();
    static inline void DrawMoney(const CPlayerInfo& playerInfo, uint8 alpha);
    static inline void DrawWeapon(CPlayerPed* ped0, CPlayerPed* ped1, float alpha);

    static void DrawTripSkip();
    static void DrawWanted();
    static void DrawWeaponIcon(CPed* ped, int32 x, int32 y, float alpha);
    static void RenderArmorBar(int32 playerId, int32 x, int32 y);
    static void RenderBreathBar(int32 playerId, int32 x, int32 y);
    static void RenderHealthBar(int32 playerId, int32 x, int32 y);
};
