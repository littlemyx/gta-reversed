/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Hud.h"
#include "Garages.h"
#include "IdleCam.h"
#include "MenuSystem.h"
#include "Radar.h"
#include "Vehicle.h"
#include "EntryExitManager.h"
#include "TaskSimpleUseGun.h"
#include "eHud.h"
#include "UserDisplay.h"
#include "AudioEngine.h"
#include "Fx/FxFtol.h"

namespace {
// NOTSA: `CTimer::GetTimeStepInMS()` divides by 50 which is NOT bit-identical to the original `ts * 0.02f * 1000.0f` (differs in the last bit for ~27% of inputs)
// This is used by the functions that were reversed from the original code
float OGTimeStepInMS() {
    return CTimer::GetTimeStep() * 0.02f * 1000.0f;
}

// NOTSA: the exe computes the HUD layout as `maximumWidth * (1/640) * a` / `maximumHeight * (1/448) * a` (multiply by the float
// reciprocal), while common.h's SCREEN_STRETCH_X/Y divide: `a * w / 640`. Not bit-identical, so the reversed code uses these.
float RvStretchX(float a) { return (float)RsGlobal.maximumWidth * (1.0f / 640.0f) * a; }
float RvStretchY(float a) { return (float)RsGlobal.maximumHeight * (1.0f / 448.0f) * a; }
}

// NOTSA: inside this file ALL the layout helpers of common.h are replaced by the exe's exact form (`W * (1/640) * a`, `H * (1/448) * a`, `W - W * (1/640) * a`).
// The original HUD code has no aspect-ratio correction (SCREEN_SCALE_X == SCREEN_STRETCH_X here). Where the exe computes a layout value differently, write it out explicitly.
#define SCREEN_STRETCH_X(a)           RvStretchX(a)
#define SCREEN_STRETCH_Y(a)           RvStretchY(a)
#define SCREEN_SCALE_X(a)             RvStretchX(a)
#define SCREEN_SCALE_Y(a)             RvStretchY(a)
#define SCREEN_STRETCH_FROM_RIGHT(a)  ((float)RsGlobal.maximumWidth  - RvStretchX(a))
#define SCREEN_STRETCH_FROM_BOTTOM(a) ((float)RsGlobal.maximumHeight - RvStretchY(a))
#define SCREEN_SCALE_FROM_RIGHT(a)    SCREEN_STRETCH_FROM_RIGHT(a)
#define SCREEN_SCALE_FROM_BOTTOM(a)   SCREEN_STRETCH_FROM_BOTTOM(a)

void CHud::InjectHooks() {
    RH_ScopedClass(CHud);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Initialise, 0x5BA850);
    RH_ScopedInstall(ReInitialise, 0x588880);
    RH_ScopedInstall(Shutdown, 0x588850);
    RH_ScopedInstall(Draw, 0x58FAE0);
    RH_ScopedInstall(GetRidOfAllHudMessages, 0x588A50);
    RH_ScopedInstall(GetYPosBasedOnHealth, 0x588B60);
    RH_ScopedInstall(HelpMessageDisplayed, 0x588B50);
    RH_ScopedInstall(ResetWastedText, 0x589070);
    RH_ScopedInstall(SetMessage, 0x588F60);
    RH_ScopedInstall(SetBigMessage, 0x588FC0);
    RH_ScopedInstall(SetHelpMessage, 0x588BE0);
    RH_ScopedInstall(SetHelpMessageStatUpdate, 0x588D40);
    RH_ScopedInstall(SetHelpMessageWithNumber, 0x588E30);
    RH_ScopedInstall(SetVehicleName, 0x588F50);
    RH_ScopedInstall(SetZoneName, 0x588BB0);
    RH_ScopedInstall(DrawAfterFade, 0x58D490);
    RH_ScopedInstall(DrawAreaName, 0x58AA50);
    RH_ScopedInstall(DrawBustedWastedMessage, 0x58CA50);
    RH_ScopedInstall(DrawCrossHairs, 0x58E020);
    RH_ScopedInstall(DrawFadeState, 0x58D580);
    RH_ScopedInstall(DrawHelpText, 0x58B6E0);
    RH_ScopedInstall(DrawMissionTimers, 0x58B180);
    RH_ScopedInstall(DrawMissionTitle, 0x58D240);
    RH_ScopedInstall(DrawOddJobMessage, 0x58CC80);
    RH_ScopedInstall(DrawRadar, 0x58A330);
    RH_ScopedInstall(DrawScriptText, 0x58C080);
    RH_ScopedInstall(DrawSubtitles, 0x58C250);
    RH_ScopedInstall(DrawSuccessFailedMessage, 0x58C6A0);
    RH_ScopedInstall(DrawVehicleName, 0x58AEA0);
    RH_ScopedInstall(DrawVitalStats, 0x589650);
    RH_ScopedInstall(DrawAmmo, 0x5893B0);
    RH_ScopedInstall(DrawPlayerInfo, 0x58EAF0);
    RH_ScopedInstall(DrawTripSkip, 0x58A160);
    RH_ScopedInstall(DrawWanted, 0x58D9A0);
    RH_ScopedInstall(DrawWeaponIcon, 0x58D7D0);
    RH_ScopedInstall(RenderArmorBar, 0x5890A0);
    RH_ScopedInstall(RenderBreathBar, 0x589190);
    RH_ScopedInstall(RenderHealthBar, 0x589270);
}

// 0x5BA850
void CHud::Initialise() {
    static constexpr SpriteFileName textures[]= { // 0x8D128C
        { "fist",           "fistm"           },
        { "siteM16",        "siteM16m"        },
        { "siterocket",     "siterocketm"     },
        { "radardisc",      "radardiscA"      },
        { "radarRingPlane", "radarRingPlaneA" },
        { "SkipIcon",       "SkipIconA"       },
    };

    auto txd = CTxdStore::AddTxdSlot("hud");
    CTxdStore::LoadTxd(txd, "MODELS\\HUD.TXD");
    CTxdStore::AddRef(txd);
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(txd);

    for (auto i = 0u; i < std::size(Sprites); i++) {
        const auto& [texture, mask] = textures[i];
        Sprites[i].SetTexture(texture, mask);
    }
    CTxdStore::PopCurrentTxd();
    ReInitialise();
}

// 0x588880
void CHud::ReInitialise() {
    memset(m_pHelpMessageToPrint, 0, sizeof(m_pHelpMessageToPrint));
    memset(m_pLastHelpMessage,    0, sizeof(m_pLastHelpMessage));
    memset(m_pHelpMessage,        0, sizeof(m_pHelpMessage));
    memset(m_Message,             0, sizeof(m_Message));
    memset(m_BigMessage,          0, sizeof(m_BigMessage));
    memset(BigMessageX,           0, sizeof(BigMessageX));

    OddJob2On       = 0;
    OddJob2Timer    = 0;
    OddJob2XOffset  = 0.0f;
    OddJob2OffTimer = 0.0f;
    PagerXOffset    = 150.0f;

    std::ranges::fill(TimerCounterHideState, 0);
    std::ranges::fill(TimerCounterWasDisplayed, false);
    TimerMainCounterWasDisplayed = false;
    TimerMainCounterHideState = 0;

    const CPlayerInfo& playerInfo   = FindPlayerInfo();
    m_LastTimeEnergyLost            = playerInfo.m_nLastTimeEnergyLost;
    m_LastDisplayScore              = playerInfo.m_nDisplayMoney;
    m_fHelpMessageStatUpdateValue   = 0.0f;
    m_Wants_To_Draw_Hud             = true;
    m_bDraw3dMarkers                = true;
    m_ZoneNameTimer                 = 0;
    m_pZoneName                     = nullptr;
    m_pLastZoneName                 = nullptr;
    m_ZoneState                     = NAME_DONT_SHOW;
    m_nHelpMessageTimer             = 0;
    m_nHelpMessageFadeTimer         = 0;
    m_nHelpMessageState             = 0;
    m_bHelpMessageQuick             = false;
    m_nHelpMessageStatId            = 0;
    m_nHelpMessageMaxStatValue      = 1000;
    m_bHelpMessagePermanent         = false;
    m_fHelpMessageTime              = 1.0f;
    m_fHelpMessageBoxWidth          = 200.0f;
    m_pVehicleName                  = nullptr;
    m_pLastVehicleName              = nullptr;
    m_pVehicleNameToPrint           = nullptr;
    m_VehicleNameTimer              = 0;
    m_VehicleFadeTimer              = 0;
    m_VehicleState                  = NAME_DONT_SHOW;
    bScriptDontDisplayRadar         = false;
    bScriptForceDisplayWithCounters = false;
    bScriptDontDisplayVehicleName   = false;
    bScriptDontDisplayAreaName      = false;
    m_ItemToFlash                   = ITEM_NONE;
    m_EnergyLostTimer               = 0;
    m_EnergyLostFadeTimer           = 0;
    m_EnergyLostState               = 5;
    m_DisplayScoreTimer             = 0;
    m_DisplayScoreFadeTimer         = 0;
    m_DisplayScoreState             = 5;
    m_LastWanted                    = 0;
    m_WantedTimer                   = 0;
    m_WantedFadeTimer               = 0;
    m_WantedState                   = 5;
    m_LastWeapon                    = 0;
    m_WeaponTimer                   = 0;
    m_WeaponFadeTimer               = 0;
    m_WeaponState                   = 5;
    bDrawClock                      = true;
    m_LastBreathTime                = 0;
}

// 0x588850
void CHud::Shutdown() {
    std::ranges::for_each(Sprites, [](auto& sprite) { sprite.Delete(); });
    CTxdStore::RemoveTxdSlot(CTxdStore::FindTxdSlot("hud"));
}

// 0x588B60
float CHud::GetYPosBasedOnHealth(uint8 playerId, float pos, int8 offset) {
    return (float)FindPlayerInfo(playerId).m_nMaxHealth < 101.0f
               ? pos - SCREEN_SCALE_Y((float)offset)
               : pos;
}

// 0x588B50
bool CHud::HelpMessageDisplayed() {
    return m_nHelpMessageState != 0;
}

// 0x588F60
void CHud::SetMessage(const GxtChar* message) {
    if (!message) {
        m_Message[0] = '\0';
        return;
    }
    // Raw GXT copy (NO ascii conversion), at most `sizeof(m_Message)` chars then the terminator
    uint16 i = 0;
    for (; message[i]; ) {
        m_Message[i] = message[i];
        if (++i >= sizeof(m_Message)) {
            break;
        }
    }
    // BUG: for a source of >= 400 chars the exe terminates at index 400 (1 byte past the buffer)
    if (i < sizeof(m_Message) || !notsa::IsFixBugs()) {
        reinterpret_cast<GxtChar*>(m_Message)[i] = '\0';
    } else {
        m_Message[sizeof(m_Message) - 1] = '\0';
    }
}

// 0x588FC0
void CHud::SetBigMessage(GxtChar* message, eMessageStyle style) {
    if (BigMessageX[style] != 0.0f) { // NaN counts as "in use" as well
        return;
    }

    constexpr size_t N = sizeof(m_BigMessage[0]);
    GxtChar* const   cur  = m_BigMessage[style];
    GxtChar* const   last = LastBigMessage[style];

    uint16 i = 0;
    if (style == STYLE_WHITE_MIDDLE_SMALLER) {
        for (; message[i]; ) {
            if (message[i] != last[i]) { // The odd-job text changed => restart its animation
                OddJob2OffTimer = 0.0f;
                OddJob2On       = 0;
            }
            cur[i]  = message[i];
            last[i] = message[i];
            if (++i >= N) {
                break;
            }
        }
    } else {
        for (; message[i]; ) {
            cur[i] = message[i];
            if (++i >= N) {
                break;
            }
        }
        message[0] = '\0'; // consumes the caller's string
    }
    // BUG: for a source of >= 128 chars the exe terminates at index 128, i.e. in the next style's slot
    cur[i]  = '\0';
    last[i] = '\0';
}

// 0x588BE0
void CHud::SetHelpMessage(const GxtChar* text, bool quickMessage, bool permanent, bool addToBrief) {
    if (m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] || CGarages::MessageIDString[0] || CReplay::Mode == MODE_PLAYBACK || CCutsceneMgr::IsRunning()) {
        return;
    }

    std::ranges::fill(m_pHelpMessageToPrint, '\0');
    std::ranges::fill(m_pLastHelpMessage, '\0');
    std::ranges::fill(m_pHelpMessage, '\0');

    CMessages::StringCopy(m_pHelpMessage, text, sizeof(m_pHelpMessage));
    CMessages::InsertPlayerControlKeysInString(m_pHelpMessage);
    if (m_nHelpMessageState && CMessages::StringCompare(m_pHelpMessage, m_pHelpMessageToPrint, sizeof(m_pHelpMessage)))
        return;

    std::ranges::fill(m_pLastHelpMessage, '\0');
    if (!text) {
        m_pHelpMessage[0] = '\0';
        m_pHelpMessageToPrint[0] = '\0';
    }

    if (permanent) {
        m_nHelpMessageState = 1;
        CMessages::StringCopy(m_pHelpMessageToPrint, m_pHelpMessage, sizeof(m_pHelpMessage));
        CMessages::StringCopy(m_pLastHelpMessage, m_pHelpMessage, sizeof(m_pHelpMessage));
    } else {
        m_nHelpMessageState = 0;
    }

    if (addToBrief)
        CMessages::AddToPreviousBriefArray(text);

    m_bHelpMessagePermanent = permanent;
    m_bHelpMessageQuick = quickMessage;
    m_nHelpMessageStatId = 0;
    m_nHelpMessageMaxStatValue = 1000;
    m_fHelpMessageStatUpdateValue = 0.0f;
}

// 0x588D40
void CHud::SetHelpMessageStatUpdate(eStatUpdateState state, uint16 statId, float diff, float max) {
    if (m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] || CGarages::MessageIDString[0] || CReplay::Mode == MODE_PLAYBACK || CCutsceneMgr::IsCutsceneProcessing()) {
        return;
    }

    std::ranges::fill(m_pHelpMessageToPrint, '\0');
    std::ranges::fill(m_pLastHelpMessage, '\0');
    std::ranges::fill(m_pHelpMessage, '\0');

    if (m_nHelpMessageState && CMessages::StringCompare(m_pHelpMessage, m_pHelpMessageToPrint, sizeof(m_pHelpMessage)))
        return;

    std::ranges::fill(m_pLastHelpMessage, '\0');
    m_nHelpMessageState = 0;
    m_bHelpMessageQuick = false;
    m_bHelpMessagePermanent = false;
    m_nHelpMessageStatId = statId;
    m_fHelpMessageStatUpdateValue = diff;
    m_nHelpMessageMaxStatValue = (uint16)notsa::detail::Ftol(max); // _ftol2, low word
    sprintf_s(gString, state == STAT_UPDATE_INCREASE ? "+" : "-");
    AsciiToGxtChar(gString, m_pHelpMessage);
}

// 0x588E30
void CHud::SetHelpMessageWithNumber(const GxtChar* text, int32 number, bool quickMessage, bool permanent) {
    if (m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] || CGarages::MessageIDString[0] || CReplay::Mode == MODE_PLAYBACK || CCutsceneMgr::IsCutsceneProcessing()) {
        return;
    }

    GxtChar str[400];
    CMessages::InsertNumberInString(text, number, -1, -1, -1, -1, -1, str);
    CMessages::GetStringLength(str);
    CMessages::StringCopy(m_pHelpMessage, str, sizeof(m_pHelpMessage));
    CMessages::InsertPlayerControlKeysInString(m_pHelpMessage);

    std::ranges::fill(m_pLastHelpMessage, '\0');
    if (permanent) {
        m_nHelpMessageState = 1;
        CMessages::StringCopy(m_pHelpMessageToPrint, m_pHelpMessage, sizeof(m_pHelpMessage));
        CMessages::StringCopy(m_pLastHelpMessage, m_pHelpMessage, sizeof(m_pHelpMessage));
    } else {
        m_nHelpMessageState = 0;
    }

    m_bHelpMessagePermanent = permanent;
    m_bHelpMessageQuick = quickMessage;
    m_nHelpMessageStatId = 0;
    m_nHelpMessageMaxStatValue = 1000;
    m_fHelpMessageStatUpdateValue = 0.0f;
}

// 0x588F50
void CHud::SetVehicleName(const GxtChar* name) {
    m_pVehicleName = name;
}

// 0x588BB0
void CHud::SetZoneName(const GxtChar* name, bool displayImmediately) {
    if (displayImmediately) {
        m_pZoneName = name;
        return;
    }
    if (CGame::currArea || m_ZoneState != NAME_DONT_SHOW) {
        return;
    }
    m_pZoneName = name;
}

// called each frame from Render2dStuff()
// 0x58FAE0
void CHud::Draw() {
    if (CReplay::Mode == MODE_PLAYBACK || CWeapon::ms_bTakePhoto || FrontEndMenuManager.m_bActivateMenuNextFrame || gbCineyCamProcessedOnFrame == CTimer::GetFrameCounter())
        return;

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,        RWRSTATE(rwFILTERNEAREST));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,            RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,             RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,            RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,        RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,       RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,        RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,            RWRSTATE(rwSHADEMODEFLAT));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION,    RWRSTATE(rwALPHATESTFUNCTIONGREATER));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(NULL));

    if (!TheCamera.m_bWideScreenOn) {
        DrawCrossHairs();
        if (FrontEndMenuManager.m_bHudOn && CTheScripts::bDisplayHud) {
            DrawPlayerInfo();
            DrawWanted();
        }
        if (!bScriptDontDisplayVehicleName) {
            DrawVehicleName();
        }
        DrawMissionTimers();
    }

    if (!bScriptDontDisplayRadar && !TheCamera.m_bWideScreenOn) {
        CPed* player = FindPlayerPed();
        CPad* pad = CPad::GetPad();
        if (!pad->GetDisplayVitalStats(player) || FindPlayerVehicle()) {
            bDrawingVitalStats = false;
            DrawRadar();
        } else {
            bDrawingVitalStats = true;
            DrawVitalStats();
        }
        if (!CGameLogic::SkipCanBeActivated() || bDrawingVitalStats) {
            HelpTripSkipShown = false;
        } else {
            DrawTripSkip();
            if (!HelpTripSkipShown) {
                SetHelpMessage(TheText.Get("SKIP_1"), true, false, false);
                HelpTripSkipShown = true;
            }
        }
    }

    if (m_bDraw3dMarkers && !TheCamera.m_bWideScreenOn) {
        CRadar::Draw3dMarkers();
    }

    if (!CTimer::GetIsUserPaused()) {
        if (!m_BigMessage[STYLE_MIDDLE][0]) {
            if (CMenuSystem::GetNumMenusInUse()) {
                CMenuSystem::Process(CMenuSystem::MENU_UNDEFINED);
            }
            DrawScriptText(true);
        }
        if (CTheScripts::bDrawSubtitlesBeforeFade) {
            DrawSubtitles();
        }
        DrawHelpText();
        DrawOddJobMessage(true);
        DrawSuccessFailedMessage();
        DrawBustedWastedMessage();
    }
}

// 0x58D490
void CHud::DrawAfterFade() {
    ZoneScoped;

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERNEAREST));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,    RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));

    if (CTimer::GetIsUserPaused() || CReplay::Mode == MODE_PLAYBACK || CWeapon::ms_bTakePhoto)
        return;

    auto vehicle = FindPlayerVehicle();
    if (!vehicle || (!vehicle->IsSubPlane() && !vehicle->IsSubHeli())) {
        if (!CCutsceneMgr::ms_cutsceneProcessing) {
            if (!FrontEndMenuManager.m_bMenuActive && !TheCamera.m_bWideScreenOn && !bScriptDontDisplayAreaName) {
                DrawAreaName();
            }
        }
    }

    if (!m_BigMessage[STYLE_MIDDLE][0]) {
        DrawScriptText(false);
    }

    if (!CTheScripts::bDrawSubtitlesBeforeFade) {
        DrawSubtitles();
    }

    DrawMissionTitle();
    DrawOddJobMessage(false);
}

// 0x58AA50
void CHud::DrawAreaName() {
    if (!m_pZoneName) {
        return;
    }

    if (m_pZoneName != m_pLastZoneName) {
        switch (m_ZoneState) {
        case NAME_DONT_SHOW:
            if (!CTheScripts::bPlayerIsOffTheMap && CTheScripts::bDisplayHud ||
                CEntryExitManager::ms_exitEnterState == EXIT_ENTER_STATE_1 ||
                CEntryExitManager::ms_exitEnterState == EXIT_ENTER_STATE_2
            ) {
                m_ZoneState = NAME_FADE_IN;
                m_ZoneNameTimer = 0;
                m_ZoneFadeTimer = 0;
                m_ZoneToPrint = m_pZoneName;
                if (m_VehicleState == NAME_SHOW || m_VehicleState == NAME_FADE_IN) {
                    m_VehicleState = NAME_FADE_OUT;
                }
            }
            break;
        case NAME_SHOW:
        case NAME_FADE_IN:
        case NAME_FADE_OUT:
            m_ZoneState = NAME_SWITCH;
            m_ZoneNameTimer = 0;
            break;
        case NAME_SWITCH:
            m_ZoneNameTimer = 0;
            break;
        default:
            break;
        }
        m_pLastZoneName = m_pZoneName;
    }

    if (!m_ZoneState)
        return;

    float alpha = 255.0f;
    switch (m_ZoneState) {
    case NAME_SHOW:
        m_ZoneFadeTimer = 1000;
        if (m_ZoneNameTimer > 3000) {
            m_ZoneState = NAME_FADE_OUT;
            m_ZoneFadeTimer = 1000;
        }
        break;

    case NAME_FADE_IN:
        if (!TheCamera.GetFading() && TheCamera.GetScreenFadeStatus() != NAME_FADE_IN) {
            m_ZoneFadeTimer += (int32)OGTimeStepInMS();
        }

        if (m_ZoneFadeTimer > 1000) {
            m_ZoneFadeTimer = 1000;
            m_ZoneState = NAME_SHOW;
        }

        if (TheCamera.GetScreenFadeStatus() != NAME_FADE_IN) {
            alpha = (float)m_ZoneFadeTimer * 0.001f * 255.0f;
            break;
        }
        m_ZoneState = NAME_FADE_OUT;
        m_ZoneFadeTimer = 1000;
        break;

    case NAME_FADE_OUT:
        if (!TheCamera.GetFading() && TheCamera.GetScreenFadeStatus() != NAME_FADE_IN) {
            m_ZoneFadeTimer -= (int32)OGTimeStepInMS();
        }

        if (m_ZoneFadeTimer < 0) {
            m_ZoneFadeTimer = 0;
            m_ZoneState = NAME_DONT_SHOW;
        }

        if (TheCamera.GetScreenFadeStatus() != NAME_FADE_IN) {
            alpha = (float)m_ZoneFadeTimer * 0.001f * 255.0f;
            break;
        }
        m_ZoneFadeTimer = 1000;
        break;

    case NAME_SWITCH:
        m_ZoneFadeTimer -= (int32)OGTimeStepInMS();
        if (m_ZoneFadeTimer < 0) {
            m_ZoneFadeTimer = 0;
            m_ZoneState = NAME_FADE_IN;
            m_ZoneToPrint = m_pLastZoneName;
        }
        alpha = (float)m_ZoneFadeTimer * 0.001f * 255.0f;
        break;

    default:
        break;
    }

    if (m_Message[0] || BigMessageX[STYLE_BOTTOM_RIGHT] != 0.0f || BigMessageX[STYLE_WHITE_MIDDLE] != 0.0f) {
        m_ZoneState = NAME_FADE_OUT;
        return;
    }

    m_ZoneNameTimer += (uint32)(int32)OGTimeStepInMS();
    CFont::SetProportional(true);
    CFont::SetBackground(false, false);
    CFont::SetScaleForCurrentLanguage(RvStretchX(1.2f), RvStretchY(1.9f));
    CFont::SetEdge(2);
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetRightJustifyWrap(RvStretchX(180.0f));
    CFont::SetDropColor({ 0, 0, 0, (uint8)alpha });
    CFont::SetFontStyle(FONT_GOTHIC);

    const CZoneInfo* info = CPopCycle::m_pCurrZoneInfo;
    const auto& color = info->ZoneColor; // Cppcheck: (warning) nullPointerRedundantCheck: Either the condition 'info' is redundant or there is possible null pointer dereference: info.
    if (CGangWars::bGangWarsActive && info && color.r && color.g && color.b) {
        CFont::SetColor({ color.r, color.g, color.b, (uint8)alpha});
    } else {
        CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_LIGHT_BLUE, (uint8)alpha));
    }

    CFont::PrintStringFromBottom((float)RsGlobal.maximumWidth - RvStretchX(32.0f), ((float)RsGlobal.maximumHeight - RvStretchY(104.0f)) + RvStretchY(76.0f), m_ZoneToPrint);
    CFont::SetSlant(0.0f);
}

// 0x58CA50
void CHud::DrawBustedWastedMessage() {
    auto& message      = m_BigMessage[STYLE_WHITE_MIDDLE];
    auto& messageX     = BigMessageX[STYLE_WHITE_MIDDLE];
    auto& messageAlpha = BigMessageAlpha[STYLE_WHITE_MIDDLE];

    if (!message[0]) {
        messageX = 0.0f;
        return;
    }

    // Function-local `static float posY` (0xBAB220) with its init guard (bit 0 of 0xBAB224). Computed on the first call and every time the message (re)starts.
    static auto& posY      = StaticRef<float>(0xBAB220);
    static auto& posYGuard = StaticRef<uint32>(0xBAB224);
    const auto ComputePosY = [] { return (float)(RsGlobal.maximumHeight / 2) - RvStretchY(30.0f); };
    if (!(posYGuard & 1)) {
        posYGuard |= 1;
        posY = ComputePosY();
    }

    if (messageX == 0.0f) {
        messageX = 1.0f;
        messageAlpha = 0.0f;

        if (m_VehicleState) {
            m_VehicleState = NAME_DONT_SHOW;
        }
        if (m_ZoneState) {
            m_ZoneState = NAME_DONT_SHOW;
        }
        posY = ComputePosY(); // 0xCAF0..0xCB48: both arms of the m_BigMessage[STYLE_MIDDLE][0] test compute the same value
        return;
    }

    messageAlpha += (float)(uint32)(int32)OGTimeStepInMS() * 0.4f;
    messageAlpha = std::min(messageAlpha, 255.0f);

    CFont::SetBackground(false, false);
    CFont::SetScale(RvStretchX(2.1f), RvStretchY(2.1f));
    CFont::SetProportional(true);
    CFont::SetJustify(false);
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
    CFont::SetFontStyle(FONT_GOTHIC);
    CFont::SetEdge(3);
    CFont::SetDropColor({ 0, 0, 0, (uint8)messageAlpha });
    CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_LIGHT_GRAY, (uint8)messageAlpha));
    CFont::PrintStringFromBottom((float)(RsGlobal.maximumWidth / 2), posY, message);
}

// 0x589070
void CHud::ResetWastedText() {
    BigMessageX[STYLE_WHITE_MIDDLE] = 0.0f;
    m_BigMessage[STYLE_WHITE_MIDDLE][0] = '\0';

    BigMessageX[STYLE_MIDDLE] = 0.0f;
    m_BigMessage[STYLE_MIDDLE][0] = '\0';
}

// 0x58E020
void CHud::DrawCrossHairs() {
    CPlayerPed* const player = CWorld::Players[CWorld::PlayerInFocus].m_pPed;
    const auto GetCamMode = [] { return TheCamera.GetActiveCam().m_nMode; };

    bool bDrawCustomCrossHair = false; // OG: bVar9
    bool bDrawAimCircle       = false; // OG: bVar4

    switch (GetCamMode()) {
    case MODE_1STPERSON:
    case MODE_SNIPER:
    case MODE_ROCKETLAUNCHER:
    case MODE_ROCKETLAUNCHER_HS:
    case MODE_M16_1STPERSON:
    case MODE_HELICANNON_1STPERSON:
    case MODE_CAMERA: {
        if (GetCamMode() == MODE_1STPERSON) {
            if (const auto* const veh = FindPlayerVehicle()) {
                if (veh->m_nModelIndex == MODEL_HYDRA || veh->m_nModelIndex == MODEL_HUNTER) {
                    bDrawCustomCrossHair = true;
                }
            }
        }
        if (GetCamMode() != MODE_1STPERSON && player && !player->GetActiveWeapon().IsTypeMelee()) {
            bDrawCustomCrossHair = true;
        }
        break;
    }
    default:
        break;
    }

    switch (GetCamMode()) {
    case MODE_ROCKETLAUNCHER_RUNABOUT:
    case MODE_ROCKETLAUNCHER_RUNABOUT_HS:
    case MODE_SNIPER_RUNABOUT:
    case MODE_M16_1STPERSON_RUNABOUT:
        bDrawAimCircle = true;
        break;
    default:
        break;
    }

    if (!player->m_pTargetedObject && player->GetPlayerData()->m_bFreeAiming) {
        const auto* const taskUseGun = player->GetIntelligence()->GetTaskUseGun();
        if (!taskUseGun || !taskUseGun->m_SkipAim) {
            const auto mode = GetCamMode();
            if (mode == MODE_AIMWEAPON || mode == MODE_AIMWEAPON_FROMCAR || mode == MODE_AIMWEAPON_ATTACHED) {
                if (player->m_nPedState != PEDSTATE_ENTER_CAR && player->m_nPedState != PEDSTATE_CARJACK) {
                    const auto wtype = player->GetActiveWeapon().m_Type;
                    if ((wtype >= WEAPON_PISTOL && wtype <= WEAPON_COUNTRYRIFLE) || wtype == WEAPON_FLAMETHROWER || wtype == WEAPON_MINIGUN) {
                        bDrawAimCircle = !(mode == MODE_AIMWEAPON && TheCamera.m_bTransitionState);
                    }
                }
            }
        }
    }

    if (!bDrawCustomCrossHair && !bDrawAimCircle && CTheScripts::bDrawCrossHair == eCrossHairType::NONE) {
        return;
    }

    // Restores render states on every exit path past this point
    struct RestoreRenderState {
        ~RestoreRenderState() {
            RwRenderStateSet(rwRENDERSTATESRCBLEND,     RWRSTATE(rwBLENDSRCALPHA));
            RwRenderStateSet(rwRENDERSTATEDESTBLEND,    RWRSTATE(rwBLENDINVSRCALPHA));
            RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(TRUE));
        }
    } restoreRenderState;

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,  RWRSTATE(FALSE));

    const CRGBA white{ 255, 255, 255, 255 };
    const auto  halfW = (float)(RsGlobal.maximumWidth / 2);
    const auto  halfH = (float)(RsGlobal.maximumHeight / 2);

    // Draws 4 mirrored quadrants of the "siteM16" sprite. Note: `right`/`bottom` (center) stay the same for all quadrants (the left/top
    // being offset by the full size instead results in the sprite being mirrored)
    const auto DrawM16Quadrants = [&](float left, float top, float w, float h) {
        const float cx = w * 0.5f + left;
        const float cy = h * 0.5f + top;
        Sprites[SPRITE_SITE_M16].Draw(CRect{ left,     cy, cx, top     }, white);
        Sprites[SPRITE_SITE_M16].Draw(CRect{ left + w, cy, cx, top     }, white);
        Sprites[SPRITE_SITE_M16].Draw(CRect{ left,     cy, cx, top + h }, white);
        Sprites[SPRITE_SITE_M16].Draw(CRect{ left + w, cy, cx, top + h }, white);
    };

    const auto mode = GetCamMode();
    if (bDrawAimCircle && (mode == MODE_AIMWEAPON || mode == MODE_AIMWEAPON_FROMCAR || mode == MODE_AIMWEAPON_ATTACHED)) {
        const float centerX = SCREEN_WIDTH * CCamera::m_f3rdPersonCHairMultX;
        const float centerY = SCREEN_HEIGHT * CCamera::m_f3rdPersonCHairMultY;
        const float radius  = player->GetWeaponRadiusOnScreen();

        if (radius == 0.2f) {
            CSprite2d::DrawRect(CRect{ centerX - 1.0f, centerY + 1.0f, centerX + 1.0f, centerY - 1.0f }, white);
        }

        const float w = SCREEN_WIDTH * (1.0f / 640.0f) * 64.0f * radius;
        const float h = SCREEN_HEIGHT * (1.0f / 448.0f) * 64.0f * radius;
        DrawM16Quadrants((w * 0.5f + centerX) - w, (h * 0.5f + centerY) - h, w, h);
    } else if (
        CTheScripts::bDrawCrossHair == eCrossHairType::FIXED_DRAW_1STPERSON_WEAPON ||
        (mode != MODE_M16_1STPERSON && mode != MODE_M16_1STPERSON_RUNABOUT && mode != MODE_1STPERSON_RUNABOUT && mode != MODE_HELICANNON_1STPERSON)
    ) {
        RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));

        RwTexture* texture = nullptr;
        float      sizeX, sizeY;
        float      offsetX = 0.0f, offsetY = 0.0f;

        const auto weaponType = FindPlayerPed()->GetActiveWeapon().m_Type;
        if (weaponType == WEAPON_CAMERA || weaponType == WEAPON_SNIPERRIFLE || CTheScripts::bDrawCrossHair == eCrossHairType::FIXED_DRAW_1STPERSON_WEAPON) {
            if (weaponType == WEAPON_CAMERA || CTheScripts::bDrawCrossHair == eCrossHairType::FIXED_DRAW_1STPERSON_WEAPON) {
                sizeX = SCREEN_WIDTH * (1.0f / 640.0f) * 256.0f;
                sizeY = SCREEN_HEIGHT * (1.0f / 448.0f) * 192.0f;
            } else {
                sizeX = SCREEN_WIDTH * (1.0f / 640.0f) * 210.0f;
                sizeY = SCREEN_HEIGHT * (1.0f / 448.0f) * 210.0f;
            }

            const auto* const wi = CWeaponInfo::GetWeaponInfo(player->GetActiveWeapon().m_Type, eWeaponSkill::STD);
            if (wi->m_nModelId1 < 1) {
                return;
            }
            auto* const mi = CModelInfo::GetModelInfo(wi->m_nModelId1);
            // NOTE: OG dereferences the result of GetAt without a null check
            const auto* const txd = CTxdStore::ms_pTxdPool->GetAt(mi->m_nTxdIndex);
            if (!txd->m_pRwDictionary) {
                return;
            }
            texture = RwTexDictionaryFindHashNamedTexture(txd->m_pRwDictionary, CKeyGen::AppendStringToKey(mi->m_nKey, "CROSSHAIR"));
        } else {
            if (mode != MODE_ROCKETLAUNCHER && mode != MODE_1STPERSON && mode != MODE_ROCKETLAUNCHER_RUNABOUT && mode != MODE_ROCKETLAUNCHER_HS && mode != MODE_ROCKETLAUNCHER_RUNABOUT_HS) {
                return;
            }
            sizeX   = SCREEN_WIDTH * (1.0f / 640.0f) * 24.0f;
            sizeY   = SCREEN_HEIGHT * (1.0f / 448.0f) * 24.0f;
            offsetX = SCREEN_WIDTH * (1.0f / 640.0f) * 20.0f;
            offsetY = SCREEN_HEIGHT * (1.0f / 448.0f) * 20.0f;
            texture = Sprites[SPRITE_SITE_ROCKET].m_pTexture;
        }

        if (texture) {
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE,    RWRSTATE(FALSE));
            RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, RWRSTATE(rwTEXTUREADDRESSCLAMP));
            RwRenderStateSet(rwRENDERSTATETEXTURERASTER,  RWRSTATE(texture->raster));

            sizeY *= 0.5f;
            sizeX *= 0.5f;
            const auto Render = [&](float x, float y, uint8 udir, uint8 vdir) {
                CSprite::RenderOneXLUSprite({ x, y, 1.0f }, { sizeX, sizeY }, 255, 255, 255, 255, 0.01f, 255, udir, vdir);
            };
            Render((halfW - sizeX) - offsetX, (halfH - sizeY) - offsetY, 0, 0);
            Render(halfW + sizeX + offsetX,   (halfH - sizeY) - offsetY, 1, 0);
            Render((halfW - sizeX) - offsetX, halfH + sizeY + offsetY,   0, 1);
            Render(halfW + sizeX + offsetX,   halfH + sizeY + offsetY,   1, 1);

            RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(FALSE));
        }
    } else {
        const float w = SCREEN_WIDTH * (1.0f / 640.0f) * 64.0f;
        const float h = SCREEN_HEIGHT * (1.0f / 448.0f) * 64.0f;
        DrawM16Quadrants(halfW - w * 0.5f, halfH - h * 0.5f, w, h);
    }
}

// 0x58D580
float CHud::DrawFadeState(DRAW_FADE_STATE fadingElement, int32 forceFadingIn) {
    // NOTE: the exe keeps these in registers as signed ints (fild / signed compares); the members are uint32 but only hold small values
    int32 state, timer, fadeTimer;
    switch (fadingElement) {
    case WANTED_STATE:
        fadeTimer = m_WantedFadeTimer;
        state = m_WantedState;
        timer = m_WantedTimer;
        break;
    case ENERGY_LOST_STATE:
        fadeTimer = m_EnergyLostFadeTimer;
        state = m_EnergyLostState;
        timer = m_EnergyLostTimer;
        break;
    case DISPLAY_SCORE_STATE:
        fadeTimer = m_DisplayScoreFadeTimer;
        state = m_DisplayScoreState;
        timer = m_DisplayScoreTimer;
        break;
    case WEAPON_STATE:
        fadeTimer = m_WeaponFadeTimer;
        state = m_WeaponState;
        timer = m_WeaponTimer;
        break;
    default:
        state = fadingElement;
        timer = fadingElement;
        fadeTimer = fadingElement;
        break;
    }

    if (forceFadingIn) {
        switch (state) {
        case NAME_DONT_SHOW: // 0x58D68B: falls through into the case below
            fadeTimer = 0;
            [[fallthrough]];
        case NAME_SHOW:      // 0x58D68D
        case NAME_FADE_OUT:
            timer = 5;
            state = NAME_FADE_IN;
            break;
        default:
            break;
        }
    }

    float alpha = 255.0f;
    if (state != NAME_DONT_SHOW && state != 5) { // 5 = FADE_DISABLED, no state (nor timer) update
        switch (state) {
        case NAME_SHOW:
            fadeTimer = 1000;
            if (timer > 10'000) {
                fadeTimer = 3000;
                state = NAME_FADE_OUT;
            }
            break;
        case NAME_FADE_IN:
            fadeTimer += (int32)OGTimeStepInMS();
            if (fadeTimer > 1000) {
                fadeTimer = 1000;
                state = NAME_SHOW;
            }
            alpha = (float)fadeTimer * 0.001f * 255.0f;
            break;
        case NAME_FADE_OUT:
            fadeTimer += (int32)(CTimer::GetTimeStep() * 0.02f * -1000.0f);
            if (fadeTimer < 0) {
                fadeTimer = 0;
                state = NAME_DONT_SHOW;
            }
            alpha = (float)fadeTimer * 0.001f * 255.0f;
            break;
        default:
            break;
        }
        timer += (int32)OGTimeStepInMS();
    }

    switch (fadingElement) {
    case WANTED_STATE:
        m_WantedFadeTimer = fadeTimer;
        m_WantedState = state;
        m_WantedTimer = timer;
        break;
    case ENERGY_LOST_STATE:
        m_EnergyLostFadeTimer = fadeTimer;
        m_EnergyLostState = state;
        m_EnergyLostTimer = timer;
        break;
    case DISPLAY_SCORE_STATE:
        m_DisplayScoreFadeTimer = fadeTimer;
        m_DisplayScoreState = state;
        m_DisplayScoreTimer = timer;
        break;
    case WEAPON_STATE:
        m_WeaponFadeTimer = fadeTimer;
        m_WeaponState = state;
        m_WeaponTimer = timer;
        break;
    default:
        break;
    }

    return std::clamp(alpha, 0.0f, 255.0f);
}

// 0x58B6E0
void CHud::DrawHelpText() {
    if (!m_pHelpMessage[0]) {
        m_nHelpMessageState = 0;
        return;
    }

    if (!CMessages::StringCompare(m_pHelpMessage, m_pLastHelpMessage, sizeof(m_pHelpMessage))) {
        switch (m_nHelpMessageState) {
        case 0: { // New message appeared
            m_nHelpMessageState     = 2;
            m_nHelpMessageTimer     = 0;
            m_nHelpMessageFadeTimer = 0;
            CMessages::StringCopy(m_pHelpMessageToPrint, m_pHelpMessage, sizeof(m_pHelpMessageToPrint));

            CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
            CFont::SetJustify(false);
            const float ws = SCREEN_STRETCH_X(1.0f);
            CFont::SetWrapx((ws * 34.0f + ws * 200.0f) - ws * 4.0f);
            CFont::SetFontStyle(FONT_SUBTITLES);
            CFont::SetBackground(true, true);
            CFont::SetDropShadowPosition(0);

            const auto numLines = CFont::GetNumberLines(SCREEN_STRETCH_X(34.0f), SCREEN_STRETCH_Y(28.0f), m_pHelpMessageToPrint);
            m_fHelpMessageTime = (float)(numLines + 3);

            CFont::SetWrapx(SCREEN_WIDTH);
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_DISPLAY_INFO, 0.0f, 1.0f);
            break;
        }
        case 1:
        case 2:
        case 3:
        case 4:
            m_nHelpMessageState = 4;
            m_nHelpMessageTimer = 5;
            break;
        default:
            break;
        }
        CMessages::StringCopy(m_pLastHelpMessage, m_pHelpMessage, sizeof(m_pLastHelpMessage));
    }

    float alphaFade = 200.0f;
    if (m_nHelpMessageState == 0) {
        return;
    }

    const auto GetFadeAlpha = [] { return (float)(int32)m_nHelpMessageFadeTimer * 0.001f * 200.0f; };

    switch (m_nHelpMessageState) {
    case 1:
        alphaFade = 200.0f;
        m_nHelpMessageFadeTimer = 600;
        if (!m_bHelpMessagePermanent) {
            if (m_fHelpMessageTime * 1000.0f < (float)(int32)m_nHelpMessageTimer || (m_bHelpMessageQuick && 3000.0f < (float)(int32)m_nHelpMessageTimer)) {
                m_nHelpMessageState     = 3;
                m_nHelpMessageFadeTimer = 600;
            }
        }
        break;
    case 2:
        if (!TheCamera.m_bWideScreenOn) {
            m_nHelpMessageFadeTimer += (int32)OGTimeStepInMS() * 2;
            if (0.0f < (float)(int32)m_nHelpMessageFadeTimer) {
                m_nHelpMessageFadeTimer = 0;
                m_nHelpMessageState     = 1;
            }
            alphaFade = GetFadeAlpha();
        }
        break;
    case 3:
        m_nHelpMessageFadeTimer -= (int32)OGTimeStepInMS() * 2;
        if ((float)(int32)m_nHelpMessageFadeTimer < 0.0f || TheCamera.m_bWideScreenOn) {
            m_nHelpMessageFadeTimer = 0;
            m_nHelpMessageState     = 0;
        }
        alphaFade = GetFadeAlpha();
        break;
    case 4:
        m_nHelpMessageFadeTimer -= (int32)OGTimeStepInMS() * 2;
        if ((float)(int32)m_nHelpMessageFadeTimer < 0.0f) {
            m_nHelpMessageFadeTimer = 0;
            m_nHelpMessageState     = 2;
            CMessages::StringCopy(m_pHelpMessageToPrint, m_pLastHelpMessage, sizeof(m_pHelpMessageToPrint));
        }
        alphaFade = GetFadeAlpha();
        break;
    default:
        break;
    }

    if (CCutsceneMgr::IsRunning()) {
        return;
    }

    m_nHelpMessageTimer += (int32)OGTimeStepInMS();
    CFont::SetAlphaFade(alphaFade);
    CFont::SetProportional(true);
    CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.52f), SCREEN_STRETCH_Y(1.1f));

    if (m_nHelpMessageStatId == 0) {
        if (m_BigMessage[STYLE_MIDDLE][0] || m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] || CGarages::MessageIDString[0]) {
            CFont::SetAlphaFade(255.0f);
            return;
        }

        CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
        CFont::SetJustify(false);
        const float ws = SCREEN_STRETCH_X(1.0f);
        if (ws * m_fHelpMessageBoxWidth == 200.0f * ws) {
            CFont::SetWrapx((ws * 34.0f + 200.0f * ws) - ws * 4.0f);
        } else {
            CFont::SetWrapx((m_fHelpMessageBoxWidth - 4.0f) * ws + ws * 34.0f);
        }
        CFont::SetFontStyle(FONT_SUBTITLES);
        CFont::SetBackground(true, true);
        CFont::SetDropShadowPosition(0);
        CFont::SetBackgroundColor(CRGBA{ 0, 0, 0, (uint8)(int32)alphaFade });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));

        const int32 yOffset = (TheCamera.m_bWideScreenOn && !FrontEndMenuManager.m_bWidescreenOn) ? 0x38 : 0;
        const float hs      = SCREEN_STRETCH_Y(1.0f);
        const float y       = ((float)(yOffset + 150) - PagerXOffset) * 0.6f * hs + hs * 28.0f;
        const float x       = ws * 34.0f;
        CFont::PrintString(x, y, m_pHelpMessageToPrint);
        CFont::SetWrapx(SCREEN_WIDTH);
        CFont::SetAlphaFade(255.0f);
        return;
    }

    // Stat update message
    if (TheCamera.m_bWideScreenOn) {
        CFont::SetAlphaFade(255.0f);
        return;
    }

    if (m_nHelpMessageStatId < 10) {
        sprintf_s(gString, "STAT00%d", m_nHelpMessageStatId);
    } else if (m_nHelpMessageStatId < 100) {
        sprintf_s(gString, "STAT0%d", m_nHelpMessageStatId);
    } else {
        sprintf_s(gString, "STAT%d", m_nHelpMessageStatId);
    }

    CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
    CFont::SetJustify(false);
    CFont::SetWrapx(SCREEN_WIDTH);
    CFont::SetFontStyle(FONT_SUBTITLES);
    CFont::SetBackground(true, true);
    CFont::SetDropShadowPosition(0);
    const auto alpha = (uint8)(int32)alphaFade;
    CFont::SetBackgroundColor(CRGBA{ 0, 0, 0, alpha });
    CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));

    const float ws = SCREEN_STRETCH_X(1.0f);
    float       x  = (CFont::GetStringWidth(TheText.Get(gString), true, false) + ws * 34.0f) + ws * 10.0f; // OG: x87 evaluation order
    CFont::SetWrapx(ws * 75.0f + x);

    float hs = SCREEN_STRETCH_Y(1.0f);
    CFont::PrintString(SCREEN_STRETCH_X(34.0f), hs * 28.0f + (150.0f - PagerXOffset) * 0.6f * hs, TheText.Get(gString));

    AsciiToGxtChar("+", gGxtString);

    float statValue;
    if (m_nHelpMessageStatId == 0x150) {
        statValue = (float)CPedGroups::GetGroup(FindPlayerPed()->GetPlayerData()->m_nPlayerGroup).GetMembership().CountMembersExcludingLeader();
    } else {
        statValue = CStats::GetStatValue((eStats)m_nHelpMessageStatId);
    }

    const auto barColor    = HudColour.GetRGBA(HUD_COLOUR_LIGHT_GRAY, alpha);
    const auto barAddColor = HudColour.GetRGBA(m_pHelpMessageToPrint[0] == gGxtString[0] ? HUD_COLOUR_GREEN : HUD_COLOUR_RED, alpha);

    const float invMax = 1.0f / (float)m_nHelpMessageMaxStatValue;
    hs                 = SCREEN_STRETCH_Y(1.0f);
    const float progress    = std::max(statValue * invMax * 100.0f, 2.0f);
    const float progressAdd = std::max(invMax * m_fHelpMessageStatUpdateValue * 100.0f, 3.0f);

    CSprite2d::DrawBarChart(
        x,
        (155.0f - PagerXOffset) * 0.6f + hs * 28.0f,
        (uint16)(int32)SCREEN_STRETCH_X(62.0f),
        (uint8)(int32)(hs * 12.0f),
        progress,
        (int8)(int32)progressAdd,
        0,
        0,
        barColor,
        barAddColor
    );

    hs = SCREEN_STRETCH_Y(1.0f);
    CFont::PrintString(
        SCREEN_STRETCH_X(65.0f) + x,
        hs * 28.0f + (150.0f - PagerXOffset) * 0.6f * hs,
        m_pHelpMessageToPrint
    );
    CFont::SetWrapx(SCREEN_WIDTH);
    CFont::SetAlphaFade(255.0f);
}

// 0x58B180
void CHud::DrawMissionTimers() {
    if ((m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] && !bScriptForceDisplayWithCounters) || CGarages::MessageIDString[0]) {
        return;
    }

    const uint8 playerId = CWorld::PlayerInFocus;
    auto&       timer    = CUserDisplay::OnscnTimer;

    const float hs  = SCREEN_STRETCH_Y(1.0f);
    const float hs2 = hs * 148.0f;
    float       clockY   = GetYPosBasedOnHealth(1, GetYPosBasedOnHealth(playerId, hs2, 12), 12);
    float       counterY = GetYPosBasedOnHealth(1, GetYPosBasedOnHealth(playerId, hs * 20.0f + hs2, 12), 12);
    if (CWorld::Players[1].m_pPed) {
        clockY   = clockY + hs * 72.0f;
        counterY = hs * 72.0f + counterY;
    }

    CFont::SetProportional(true);
    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.5f), SCREEN_STRETCH_Y(1.0f));
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetFontStyle(FONT_MENU);
    CFont::SetWrapx(SCREEN_STRETCH_X(640.0f));
    CFont::SetEdge(2);
    CFont::SetDropColor({ 0, 0, 0, 255 });

    const bool bClockEnabled = timer.m_Clock.m_bEnabled;
    if (CWorld::Players[1].m_pPed && !bClockEnabled) {
        TimerMainCounterWasDisplayed = bClockEnabled;
    }

    for (auto i = 0u; i < COnscreenTimer::NUM_COUNTERS; i++) {
        if (!timer.m_aCounters[i].m_bEnabled) {
            TimerCounterWasDisplayed[i] = false;
        }
    }

    if (timer.m_bDisplay != 1) {
        return;
    }

    if (bClockEnabled == 1) {
        if (!TimerMainCounterWasDisplayed) {
            TimerMainCounterHideState = 1;
        }
        TimerMainCounterWasDisplayed = true;
        if (TimerMainCounterHideState != 0) {
            TimerMainCounterHideState++;
            if (TimerMainCounterHideState > 50) {
                TimerMainCounterHideState = 0;
            }
        }

        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));
        if ((CTimer::GetFrameCounter() & 4) || TimerMainCounterHideState == 0) {
            GxtChar text[200];
            AsciiToGxtChar(timer.m_Clock.m_szDisplayedText, text);
            CFont::PrintString(SCREEN_STRETCH_FROM_RIGHT(32.0f), clockY, text);
            if (timer.m_Clock.m_szDescriptionTextKey[0]) {
                CFont::PrintString(SCREEN_STRETCH_FROM_RIGHT(32.0f) - SCREEN_STRETCH_X(90.0f), clockY, TheText.Get(timer.m_Clock.m_szDescriptionTextKey));
            }
        }
    } else {
        const float hs3 = SCREEN_STRETCH_Y(1.0f);
        counterY        = GetYPosBasedOnHealth(playerId, hs3 * 148.0f, 12);
        // BUG: OG checks the max health of the 2nd player (0xB7D077 == CWorld::Players[1].m_nMaxHealth) instead of `playerId`
        if ((float)CWorld::Players[1].m_nMaxHealth < 101.0f) {
            counterY = counterY - hs3 * 12.0f;
        }
        if (CWorld::Players[1].m_pPed) {
            counterY = hs3 * 72.0f + counterY;
        }
    }

    for (auto i = 0u; i < COnscreenTimer::NUM_COUNTERS; i++) {
        auto& counter = timer.m_aCounters[i];
        if (counter.m_bEnabled != 1) {
            continue;
        }

        if (!TimerCounterWasDisplayed[i] && counter.m_bFlashWhenFirstDisplayed == 1) {
            TimerCounterHideState[i] = 1;
        }
        TimerCounterWasDisplayed[i] = true;
        if (TimerCounterHideState[i] != 0) {
            TimerCounterHideState[i]++;
            if (TimerCounterHideState[i] > 50) {
                TimerCounterHideState[i] = 0;
            }
        }

        if (!(CTimer::GetFrameCounter() & 4) && TimerCounterHideState[i] != 0) {
            continue;
        }

        CFont::SetColor(HudColour.GetRGB(counter.m_nColourId));

        // BUG: OG scales the Y position by the screen height twice
        const float y = SCREEN_STRETCH_Y(20.0f) * (float)i * SCREEN_STRETCH_Y(1.0f) + counterY;
        if (counter.m_nType == eOnscreenCounter::LINE) {
            const auto value = (int16)atol(counter.m_szDisplayedText);
            const float ws   = SCREEN_STRETCH_X(1.0f);
            CSprite2d::DrawBarChart(
                (SCREEN_WIDTH - ws * 32.0f) - ws * 61.0f,
                SCREEN_STRETCH_Y(1.0f) * 6.0f + y,
                (uint16)(int32)(ws * 61.0f),
                (uint8)(int32)(SCREEN_STRETCH_Y(1.0f) * 9.0f),
                (float)value * 0.01f * 100.0f,
                0,
                0,
                1,
                HudColour.GetRGB(counter.m_nColourId),
                CRGBA{ 0, 0, 0, 0 }
            );
        } else {
            GxtChar text[200];
            AsciiToGxtChar(counter.m_szDisplayedText, text);
            CFont::PrintString(SCREEN_STRETCH_FROM_RIGHT(32.0f), y, text);
        }

        if (counter.m_szDescriptionTextKey[0]) {
            CFont::PrintString(SCREEN_STRETCH_FROM_RIGHT(32.0f) - SCREEN_STRETCH_X(90.0f), y, TheText.Get(counter.m_szDescriptionTextKey));
        }
    }
}

// 0x58D240
void CHud::DrawMissionTitle() {
    auto& message      = m_BigMessage[STYLE_BOTTOM_RIGHT];
    auto& messageX     = BigMessageX[STYLE_BOTTOM_RIGHT];
    auto& messageAlpha = BigMessageAlpha[STYLE_BOTTOM_RIGHT];
    auto& messageInUse = BigMessageInUse[STYLE_BOTTOM_RIGHT];

    if (!message[0]) {
        messageX = 0.0f;
        return;
    }

    if (messageX == 0.0f) {
        messageInUse = -60.0f;
        messageX = 1.0f;
        m_ZoneState = NAME_DONT_SHOW;
        m_ZoneFadeTimer = 0;
        SetHelpMessage(nullptr, true, false, false);
        return;
    }

    CFont::SetBackground(false, false);
    CFont::SetProportional(true);
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetFontStyle(FONT_PRICEDOWN);
    CFont::SetScale(SCREEN_STRETCH_X(1.0f), SCREEN_SCALE_Y(1.3f));

    if (!((float)(RsGlobal.maximumWidth - 20) > messageInUse)) { // magic shit; OG: fcomp + JNE (0x41) => taken for <= and unordered
        messageX += CTimer::GetTimeStep();
        if (!(messageX < 120.0f)) {
            messageX = 120.0f;
            messageAlpha -= (float)(uint32)(int32)OGTimeStepInMS();
        }
        if (messageAlpha <= 0.0f) {
            messageAlpha = 0.0f;
            message[0] = '\0';
            messageX = 0.0f;
        }
    } else {
        messageAlpha = 255.0f;
        messageInUse += (float)(uint32)(int32)OGTimeStepInMS() * 0.3f;
    }

    CFont::SetEdge(2);
    CFont::SetDropColor({ 0, 0, 0, uint8(messageAlpha) });
    CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GOLD, (uint8)messageAlpha));
    CFont::PrintStringFromBottom(SCREEN_SCALE_FROM_RIGHT(20.0f), SCREEN_SCALE_FROM_BOTTOM(115.0f), message);
    CFont::SetEdge(0);
}

// It looks like the original, but needs to be recheck
// 0x58CC80
void CHud::DrawOddJobMessage(bool displayImmediately) {
    const auto& m1 = m_BigMessage[STYLE_BOTTOM_RIGHT];
    const auto& m4 = m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER];
    if (displayImmediately == CTheScripts::bDrawOddJobTitleBeforeFade && !m1[0] && m4[0]) {
        CFont::SetBackground(false, false);
        CFont::SetJustify(false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.6f), SCREEN_SCALE_Y(1.35f));
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetProportional(true);
        CFont::SetCentreSize(SCREEN_STRETCH_X(350.0f));
        CFont::SetFontStyle(FONT_MENU);
        CFont::SetEdge(2);
        CFont::SetDropColor({ 0, 0, 0, 255 });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_GOLD));
        CFont::PrintStringFromBottom(static_cast<float>(RsGlobal.maximumWidth / 2), SCREEN_STRETCH_Y(140.0f), m4);
    }

    if (!displayImmediately)
        return;

    const auto& m6 = m_BigMessage[STYLE_LIGHT_BLUE_TOP];
    if (m6[0]) {
        CFont::SetBackground(false, false);
        CFont::SetJustify(false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(1.0f), SCREEN_SCALE_Y(1.8f));
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetProportional(true);
        CFont::SetCentreSize(SCREEN_STRETCH_X(500.0f));
        CFont::SetFontStyle(FONT_PRICEDOWN);
        CFont::SetEdge(2);
        CFont::SetDropColor({ 0, 0, 0, 255 });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_BLUE));
        CFont::PrintString(static_cast<float>(RsGlobal.maximumWidth / 2), SCREEN_STRETCH_Y(60.0f), m6);
    }

    const auto& m3 = m_BigMessage[STYLE_MIDDLE_SMALLER];
    if (m3[0]) {
        CFont::SetBackground(false, false);
        CFont::SetJustify(false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.6f), SCREEN_SCALE_Y(1.35f));
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetProportional(true);
        CFont::SetCentreSize(SCREEN_STRETCH_X(500.0f));
        CFont::SetFontStyle(FONT_MENU);
        CFont::SetEdge(2);
        CFont::SetDropColor({ 0, 0, 0, 255 });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_GOLD));
        CFont::PrintString(static_cast<float>(RsGlobal.maximumWidth / 2), SCREEN_STRETCH_Y(155.0f), m3);
    }

    if (OddJob2OffTimer > 0.0f) {
        OddJob2OffTimer -= (float)(uint32)(int32)OGTimeStepInMS();
    }

    const auto& m5 = m_BigMessage[STYLE_WHITE_MIDDLE_SMALLER];
    if (!m5[0])
        return;

    if (OddJob2OffTimer > 0.0f)
        return;

    switch (OddJob2On) {
    case 0:
        OddJob2XOffset = 380.0f;
        OddJob2On = 1;
        break;
    case 1:
        if (OddJob2XOffset <= 2.0f) {
            OddJob2On = 2;
            OddJob2Timer = 0;
        } else {
            OddJob2XOffset -= std::min(OddJob2XOffset * 0.16666667f, 40.0f); // OG: multiplies by the float 1/6 (0x85F0A0)
        }
        break;
    case 2:
        OddJob2Timer += (uint16)(int32)OGTimeStepInMS();
        if ((int16)OddJob2Timer > 1500) { // OG: signed 16-bit compare
            OddJob2On = 3;
        }
        break;
    case 3:
        OddJob2XOffset -= std::max(OddJob2XOffset * 0.2f, 30.0f); // OG: multiplies by 0.2f (0x858CC4)
        if (OddJob2XOffset < -380.0f) {
            OddJob2On = 0;
            OddJob2OffTimer = 5000.0f;
        }
        break;
    default:
        break;
    }

    if (!m1[0]) {
        CFont::SetBackground(false, false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.6f), SCREEN_SCALE_Y(1.35f));
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetProportional(true);
        CFont::SetCentreSize(SCREEN_STRETCH_X(500.0f));
        CFont::SetFontStyle(FONT_MENU);
        CFont::SetEdge(2);
        CFont::SetDropColor({ 0, 0, 0, 255 });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));
        CFont::PrintString(static_cast<float>(RsGlobal.maximumWidth / 2), SCREEN_STRETCH_Y(217.0f), m5);
    }
}

// 0x58A330
void CHud::DrawRadar() {
    if (CEntryExitManager::ms_exitEnterState == EXIT_ENTER_STATE_1 ||
        CEntryExitManager::ms_exitEnterState == EXIT_ENTER_STATE_2 ||
        FrontEndMenuManager.m_nRadarMode == eRadarMode::RADAR_MODE_OFF ||
        (m_ItemToFlash == ITEM_RADAR && EachFrames(8))
    ) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,     RWRSTATE(rwFILTERNEAREST));

    CRadar::DrawMap();

    if (FrontEndMenuManager.m_nRadarMode == eRadarMode::RADAR_MODE_BLIPS_ONLY) {
        CRadar::DrawBlips();
        return;
    }

    const float ws = RvStretchX(1.0f); // W * (1/640)
    const float hs = RvStretchY(1.0f); // H * (1/448)

    CVehicle* vehicle = FindPlayerVehicle();
    if (vehicle && vehicle->IsSubPlane() && vehicle->m_nModelIndex != MODEL_VORTEX) {
        // NOTE: the exe inlines its own rotating quad here (NOT CRadar::DrawRotatingRadarSprite: other radius, other corner order, no `Limit`).
        const auto& mat = *vehicle->m_matrix;
        const float a   = -mat.GetRight().z;
        const float b   = mat.GetUp().z;
        const double ang0 = -std::atan2((double)a, (double)b) - (double)0.7853982f; // 0x859AB0 = pi/4

        const float X0 = 94.0f * ws;
        const float cx = X0 - 18.0f * ws; // radius X (spilled)
        const float hx = X0 * 0.5f;       // spilled
        const float ex = ws * 40.0f;
        const float T  = 76.0f * hs;      // spilled
        const float cy = T - 18.0f * hs;  // radius Y (spilled)
        const float by = (float)RsGlobal.maximumHeight - 104.0f * hs;
        const float hy = T * 0.5f;

        float vx[4], vy[4];
        for (auto i = 0; i < 4; i++) {
            const double th = (double)i * (double)1.5707964f + ang0; // 0x858FE4 = pi/2
            const double s  = std::sin(th);
            const double c  = std::cos(th);
            vx[i] = (float)((((0.0 * c + s) * (double)cx) + (double)ex) + (double)hx);
            vy[i] = (float)((((c - s * 0.0) * (double)cy) + (double)hy) + (double)by);
        }
        Sprites[SPRITE_RADAR_RING_PLANE].Draw(vx[1], vy[1], vx[0], vy[0], vx[2], vy[2], vx[3], vy[3], CRGBA(255, 255, 255, 255));
    }

    CPlayerPed* player = FindPlayerPed();
    // Draws Altimeter on Planes And Helis or when parachuting down
    // (the exe: `(plane || heli) && model != VORTEX`, else the parachute test)
    if (vehicle && (vehicle->IsSubPlane() || vehicle->IsSubHeli()) && vehicle->m_nModelIndex != MODEL_VORTEX
        || player->GetActiveWeapon().m_Type == WEAPON_PARACHUTE
    ) {
        const float ws40 = ws * 40.0f;
        const float by   = (float)RsGlobal.maximumHeight - hs * 104.0f;
        CRect rect; // NOTE: `top` (+4) holds the larger Y, `bottom` (+0xC) the smaller one, as in the exe
        rect.left   = ws40 - ws * 20.0f;
        rect.right  = ws40 - ws * 10.0f;
        rect.bottom = by;
        rect.top    = hs * 76.0f + by;
        CSprite2d::DrawRect(rect, { 10, 10, 10, 100 }); // rectangle

        const CVector& pos = vehicle ? vehicle->GetPosition() : player->GetPosition();
        const float k = (pos.z > 200.0f) ? 0.0010526315309107304f /* 0x866C08 */ : 0.004999999888241291f /* 0x858B4C */;
        const float h = (pos.z * k) * (hs * 76.0f);
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));

        const float T   = hs * 76.0f;
        const float y0  = ((float)RsGlobal.maximumHeight - hs * 104.0f) + T;
        const float mn  = (T < h) ? T : h; // 0x404330
        const float ybt = y0 - mn;
        rect.left   = ws40 - ws * 25.0f;
        rect.right  = ws40 - 5.0f;
        rect.bottom = ybt;
        rect.top    = ybt + 2.0f;
        CSprite2d::DrawRect(rect, { 200, 200, 200, 200 }); // horizontal line (current height)
    }

    // The 4 quarters of the radar disc mask (the right ones are flipped: left > right). Each rect is computed from scratch in the exe.
    {
        const auto black = CRGBA(0, 0, 0, 255);
        const float ws40 = ws * 40.0f;
        const float by104 = (float)RsGlobal.maximumHeight - hs * 104.0f;
        const float T     = hs * 76.0f;
        CRect rect;
        const auto Quarter = [&](float left, float bottom) {
            rect.left   = left;
            rect.bottom = bottom;
            rect.right  = (ws * 94.0f) * 0.5f + ws40;
            rect.top    = T * 0.5f + by104;
            Sprites[SPRITE_RADAR_DISC].Draw(rect, black);
        };
        const float leftL = ws40 - ws * 4.0f;
        const float leftR = (ws * 4.0f + ws40) + ws * 94.0f;
        Quarter(leftL, by104 - hs * 4.0f);            // top left
        Quarter(leftR, by104 - hs * 4.0f);            // top right
        Quarter(leftL, (hs * 4.0f + by104) + T);      // bottom left
        Quarter(leftR, (hs * 4.0f + by104) + T);      // bottom right
    }

    CRadar::DrawBlips();
}

// 0x58C080
void CHud::DrawScriptText(bool isBeforeFade) {
    CTheScripts::DrawScriptSpritesAndRectangles(isBeforeFade);

    for (auto& t : CTheScripts::IntroTextLines) {
        if (!t.GXTKey[0]) { /* empty key? */
            continue;
        }
        if (t.IsDrawBeforeFade != isBeforeFade) {
            continue;
        }

        CFont::SetScale(SCREEN_SCALE_X(t.Scale.x), RvStretchY(t.Scale.y) * 0.5f);
        CFont::SetColor(t.Color);
        CFont::SetJustify(t.Justify);
        if (t.HasRightJustify) {
            CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
        } else {
            CFont::SetOrientation(t.IsCentered ? eFontAlignment::ALIGN_CENTER : eFontAlignment::ALIGN_LEFT);
        }
        CFont::SetWrapx(SCREEN_SCALE_X(t.WrapX));
        CFont::SetCentreSize(SCREEN_SCALE_X(t.CentreSize));
        CFont::SetBackground(t.HasBg, false);
        CFont::SetBackgroundColor(t.BgColor);
        CFont::SetProportional(t.IsProportional);
        CFont::SetDropColor(t.DropShadowColor);
        if (t.TextEdge) {
            CFont::SetEdge(t.TextEdge);
        } else {
            CFont::SetDropShadowPosition(t.DropShadow);
        }
        CFont::SetFontStyle((eFontStyle)t.FontStyle);

        GxtChar text[400];
        CMessages::InsertNumberInString(
            TheText.Get(t.GXTKey),
            t.NumberToInsert1,
            t.NumberToInsert2,
            -1,
            -1,
            -1,
            -1,
            text
        );
        CMessages::InsertPlayerControlKeysInString(text);
        // todo: Replace DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT
        // The first letter doesn't look good in window mode, but it looks fine in full-screen mode
        CFont::PrintString(
            SCREEN_SCALE_FROM_RIGHT(DEFAULT_SCREEN_WIDTH - t.Pos.x),
            SCREEN_SCALE_FROM_BOTTOM(DEFAULT_SCREEN_HEIGHT - t.Pos.y),
            text
        );
        CFont::SetEdge(0);
    }
}

// 0x58C250
void CHud::DrawSubtitles() {
    static auto& bWasWidescreen = StaticRef<bool>(0xBAB214); // OG: function local static

    if (!m_Message[0]) {
        return;
    }

    if (m_BigMessage[STYLE_WHITE_MIDDLE][0] && !CGameLogic::IsCoopGameGoingOn()) {
        return;
    }

    if (m_VehicleState != NAME_DONT_SHOW) {
        m_VehicleState = NAME_FADE_OUT;
    }
    if (m_ZoneState != NAME_DONT_SHOW) {
        m_ZoneState = NAME_FADE_OUT;
    }

    CFont::SetBackground(false, false);
    CFont::SetBackgroundColor({ 0, 0, 0, 128 });
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
    CFont::SetProportional(true);
    CFont::SetDropShadowPosition(0);
    CFont::SetFontStyle(FONT_SUBTITLES);
    CFont::SetColor({ 225, 225, 225, 255 });
    CFont::SetDropShadowPosition(2);
    CFont::SetDropColor({ 0, 0, 0, 255 });

    float x, y;
    if (TheCamera.m_bWideScreenOn) {
        bWasWidescreen = true;
        if (!FrontEndMenuManager.m_bShowSubtitles && CCutsceneMgr::IsRunning()) {
            CFont::SetDropShadowPosition(0);
            return;
        }

        CFont::SetCentreSize(SCREEN_WIDTH - SCREEN_STRETCH_X(1.0f) * 60.0f);
        CFont::SetScale(SCREEN_STRETCH_X(1.0f) * 0.58f, SCREEN_STRETCH_Y(1.0f) * 1.2f);
        y = SCREEN_HEIGHT - SCREEN_STRETCH_Y(1.0f) * 80.0f;
        x = (float)(RsGlobal.maximumWidth / 2);
    } else {
        if (bWasWidescreen) {
            m_Message[0] = '\0';
        }
        bWasWidescreen = false;

        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(1.0f) * 0.58f, SCREEN_STRETCH_Y(1.0f) * 1.22f);

        const float ws = SCREEN_STRETCH_X(1.0f);
        const float hs = SCREEN_STRETCH_Y(1.0f);
        if (CTheScripts::bUseMessageFormatting) {
            CFont::SetCentreSize(ws * (float)CTheScripts::MessageWidth);
            y = (SCREEN_HEIGHT - 105.0f * hs) - (hs + hs);
            x = ws * (float)CTheScripts::MessageCentre;
        } else if (!bDrawingVitalStats) {
            const float a = (SCREEN_WIDTH - ws * 20.0f) - ws * 8.0f;
            const float b = ws * 140.0f + ws * 8.0f;
            CFont::SetCentreSize(a - b);
            y = (SCREEN_HEIGHT - 105.0f * hs) - (hs + hs);
            x = (a - b) * 0.5f + b;
        } else {
            CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(1.0f) * 0.58f * 0.8f, SCREEN_STRETCH_Y(1.0f) * 1.22f);
            const float a = (SCREEN_WIDTH - ws * 20.0f) - ws * 8.0f;
            const float b = ws * 140.0f + ws * 8.0f;
            CFont::SetCentreSize((a - b) * 0.8f);
            y = (SCREEN_HEIGHT - 105.0f * hs) - (hs + hs);
            x = ws * 40.0f + ((a - b) * 0.5f + b);
        }
    }

    CFont::PrintString(x, y, m_Message);
    CFont::SetDropShadowPosition(0);
}

// 0x58C6A0
void CHud::DrawSuccessFailedMessage() {
    // OG: function local static `posY`, together with its init-guard flag (bit 0)
    static auto& posY      = StaticRef<float>(0xBAB218);
    static auto& posYGuard = StaticRef<uint32>(0xBAB21C);
    if (!(posYGuard & 1)) {
        posYGuard |= 1;
        posY = (float)(RsGlobal.maximumHeight / 2) - SCREEN_STRETCH_Y(10.0f);
    }

    auto& message      = m_BigMessage[STYLE_MIDDLE];
    auto& messageX     = BigMessageX[STYLE_MIDDLE];
    auto& messageAlpha = BigMessageAlpha[STYLE_MIDDLE];
    auto& messageInUse = BigMessageInUse[STYLE_MIDDLE];

    if (!message[0]) {
        messageX = 0.0f;
        return;
    }

    if (messageX == 0.0f) {
        messageInUse = -60.0f;
        messageX     = 1.0f;
        messageAlpha = 0.0f;

        const float hs = SCREEN_STRETCH_Y(1.0f);
        if (m_BigMessage[STYLE_MIDDLE_SMALLER][0] || m_BigMessage[STYLE_WHITE_MIDDLE_SMALLER][0]) {
            posY = hs * 25.0f + ((float)(RsGlobal.maximumHeight / 2) - hs * 10.0f);
            return;
        }

        if (!m_BigMessage[STYLE_WHITE_MIDDLE][0]) {
            const auto numLines = CFont::GetNumberLines(
                (float)(RsGlobal.maximumWidth / 2),
                (float)(RsGlobal.maximumHeight / 2) - SCREEN_STRETCH_Y(10.0f),
                message
            );
            if (numLines > 1) {
                posY = ((float)(RsGlobal.maximumHeight / 2) - hs * 10.0f) - hs * 15.0f;
                return;
            }
        }
        posY = (float)(RsGlobal.maximumHeight / 2) - SCREEN_STRETCH_Y(10.0f);
        return;
    }

    CFont::SetBackground(false, false);
    CFont::SetScale((float)((double)SCREEN_STRETCH_X(1.0f) * 1.3), (float)((double)SCREEN_STRETCH_Y(1.0f) * 1.8));
    CFont::SetProportional(true);
    CFont::SetJustify(false);
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
    CFont::SetCentreSize(SCREEN_STRETCH_X(1.0f) * 590.0f);
    CFont::SetFontStyle(FONT_PRICEDOWN);
    CFont::SetEdge(2);
    CFont::SetDropColor({ 0, 0, 0, (uint8)(int32)messageAlpha });
    CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GOLD, (uint8)(int32)messageAlpha));

    if ((float)(RsGlobal.maximumWidth - 20) > messageInUse) {
        const float delta = (float)(uint32)(int32)OGTimeStepInMS() * 0.3f;
        messageInUse += delta;
        messageAlpha = delta + messageAlpha;
        if (messageAlpha > 255.0f) {
            messageAlpha = 255.0f;
        }
    } else {
        messageX = CTimer::GetTimeStep() + messageX;
        if (!(messageX < 120.0f)) {
            messageX = 120.0f;
            messageAlpha = messageAlpha - (float)(uint32)(int32)OGTimeStepInMS() * 0.3f;
        }
        if (messageAlpha <= 0.0f) {
            messageAlpha = 0.0f;
            message[0]   = '\0';
        }
    }

    CFont::PrintString((float)(RsGlobal.maximumWidth / 2), posY, message);
}

// 0x58AEA0
void CHud::DrawVehicleName() {
    if (!m_pVehicleName) {
        m_VehicleState = NAME_DONT_SHOW;
        m_VehicleNameTimer = 0;
        m_VehicleFadeTimer = 0;
        m_pLastVehicleName = nullptr;
        return;
    }

    if (m_pVehicleName != m_pLastVehicleName) {
        switch (m_VehicleState) {
        case NAME_DONT_SHOW:
            m_VehicleState = NAME_FADE_IN;
            m_VehicleNameTimer = 0;
            m_VehicleFadeTimer = 0;
            m_pVehicleNameToPrint = m_pVehicleName;
            if (m_ZoneState == NAME_SHOW || m_ZoneState == NAME_FADE_IN) {
                m_ZoneState = NAME_FADE_OUT;
            }
            break;
        case NAME_SHOW:
        case NAME_FADE_IN:
        case NAME_FADE_OUT:
        case NAME_SWITCH:
            m_VehicleState = NAME_SWITCH;
            m_VehicleNameTimer = 0;
            break;
        default:
            break;
        }
        m_pLastVehicleName = m_pVehicleName;
    }

    if (!m_VehicleState)
        return;

    float alpha = 0.0f;
    switch (m_VehicleState) {
    case NAME_SHOW:
        if (m_VehicleNameTimer > 3000) {
            m_VehicleState = NAME_FADE_OUT;
            m_VehicleFadeTimer = 1000;
        }
        alpha = 255.0f;
        break;
    case NAME_FADE_IN:
        m_VehicleFadeTimer += (int32)OGTimeStepInMS();
        if (m_VehicleFadeTimer > 1000) {
            m_VehicleFadeTimer = 1000;
            m_VehicleState = NAME_SHOW;
        }
        alpha = (float)m_VehicleFadeTimer * 0.001f * 255.0f;
        break;
    case NAME_FADE_OUT:
        m_VehicleFadeTimer += (int32)(CTimer::GetTimeStep() * 0.02f * -1000.0f);
        if (m_VehicleFadeTimer < 0) {
            m_VehicleState = NAME_DONT_SHOW;
            m_VehicleFadeTimer = 0;
        }
        alpha = (float)m_VehicleFadeTimer * 0.001f * 255.0f;
        break;
    case NAME_SWITCH:
        m_VehicleFadeTimer += (int32)(CTimer::GetTimeStep() * 0.02f * -1000.0f);
        if (m_VehicleFadeTimer < 0) {
            m_VehicleNameTimer = 0;
            m_VehicleState = NAME_FADE_IN;
            m_VehicleFadeTimer = 0;
            m_pVehicleNameToPrint = m_pLastVehicleName;
        }
        alpha = (float)m_VehicleFadeTimer * 0.001f * 255.0f;
        break;
    default:
        break;
    }

    if (!m_Message[0]) {
        m_VehicleNameTimer += (int32)OGTimeStepInMS();
        CFont::SetProportional(true);
        CFont::SetBackground(false, false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(1.0f), SCREEN_SCALE_Y(1.5f));
        CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
        CFont::SetRightJustifyWrap(0.0f);
        CFont::SetFontStyle(eFontStyle::FONT_MENU);
        CFont::SetEdge(2);
        CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GREEN, (uint8)alpha));
        CFont::SetDropColor({ 0, 0, 0, (uint8)alpha });
        if (CTheScripts::bDisplayHud) {
            CFont::PrintString(
                SCREEN_STRETCH_FROM_RIGHT(32.0f),
                SCREEN_STRETCH_FROM_BOTTOM(104.0f),
                m_pVehicleNameToPrint
            );
        }
        CFont::SetSlant(0.0f);
    }
}

// 0x589650
void CHud::DrawVitalStats() {
    if (CReplay::Mode == MODE_PLAYBACK) {
        return;
    }
    if (TheCamera.m_bWideScreenOn) {
        return;
    }

    // Weapon type used for the weapon skill row (TEC9 shares the skill with the micro UZI)
    auto weaponType = FindPlayerPed()->GetActiveWeapon().m_Type;
    if (weaponType == WEAPON_TEC9) {
        weaponType = WEAPON_MICRO_UZI;
    }

    CFont::SetBackground(false, false);
    CFont::SetColor({ 225, 225, 225, 255 });
    CFont::SetWrapx(SCREEN_STRETCH_X(640.0f));
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetProportional(true);

    const float ws        = SCREEN_STRETCH_X(1.0f);
    const auto  labelX    = (int16)(int32)(ws * 10.0f + 40.0f); // OG: sVar5
    const auto  barX      = (int16)(int32)(ws * 90.0f + 40.0f); // OG: sVar6
    const auto  barYShift = (int16)(int32)(SCREEN_STRETCH_Y(1.0f) * 5.0f); // OG: sVar7

    const auto GetPlayerTaskSwim = [] {
        return CWorld::Players[CWorld::PlayerInFocus].m_pPed->GetIntelligence()->GetTaskSwim();
    };

    float yf;
    {
        const float hs = SCREEN_STRETCH_Y(1.0f);
        CRect       windowRect;
        if (!GetPlayerTaskSwim() && (weaponType < WEAPON_PISTOL || weaponType > WEAPON_TEC9)) {
            // `weaponType` is in range [WEAPON_PISTOL, WEAPON_TEC9] (22..32), which is the range of weapons with skills
            const float bottom = (float)RsGlobal.maximumHeight - hs * 140.0f;
            windowRect.left   = 40.0f;
            windowRect.bottom = hs * 15.0f + bottom; // NOTE: `top` (+4) holds the larger Y, `bottom` (+0xC) the smaller one, as in the exe
            windowRect.right  = ws * 170.0f + 40.0f;
            windowRect.top    = hs * 127.0f + bottom;
            FrontEndMenuManager.DrawWindow(windowRect, "FEH_STA", 0, CRGBA{ 0, 0, 0, 190 }, false, true);
            yf = bottom + (hs * 15.0f) * 2.0f;
        } else {
            const float bottom = (float)RsGlobal.maximumHeight - hs * 140.0f;
            windowRect.left   = 40.0f;
            windowRect.bottom = bottom;
            windowRect.right  = ws * 170.0f + 40.0f;
            windowRect.top    = hs * 127.0f + bottom;
            FrontEndMenuManager.DrawWindow(windowRect, "FEH_STA", 0, CRGBA{ 0, 0, 0, 190 }, false, true);
            yf = bottom + hs * 15.0f;
        }
    }
    auto y = (int16)(int32)yf;

    CFont::SetFontStyle(FONT_SUBTITLES);
    CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
    CFont::SetScale(SCREEN_STRETCH_X(1.0f) * 0.35f, SCREEN_STRETCH_Y(1.0f) * 0.9f);
    CFont::SetColor({ 225, 225, 225, 255 });
    CFont::SetEdge(0);

    const auto NextRowY = [&] {
        y = (int16)(int32)(SCREEN_STRETCH_Y(1.0f) * 15.0f + (float)y);
    };
    const auto DrawBar = [&](float progress) {
        CSprite2d::DrawBarChart(
            (float)barX,
            (float)(barYShift + y),
            (uint16)(int32)(SCREEN_STRETCH_X(1.0f) * 70.0f),
            (uint8)(int32)(SCREEN_STRETCH_Y(1.0f) * 10.0f),
            progress,
            0,
            0,
            1,
            CRGBA{ 200, 200, 200, 255 },
            CRGBA{ 0, 0, 0, 0 }
        );
    };
    const auto DrawStatRow = [&](const char* gxtKey, eStats stat) {
        CFont::PrintString((float)labelX, (float)y, TheText.Get(gxtKey));
        DrawBar(CStats::GetStatValue(stat) * 0.001f * 100.0f);
        NextRowY();
    };

    // Respect
    DrawStatRow("STAT068", STAT_TOTAL_RESPECT);

    if (GetPlayerTaskSwim()) {
        // Lung capacity
        DrawStatRow("STAT225", STAT_LUNG_CAPACITY);
    } else if (weaponType >= WEAPON_PISTOL && weaponType <= WEAPON_TEC9) {
        // Current weapon skill
        CFont::PrintString((float)labelX, (float)y, TheText.Get("CURWSKL"));

        const auto skillStat = CWeaponInfo::GetSkillStatIndex(weaponType);
        // NOTE: These are `CStats::StatReactionValue` entries, but they're accessed as raw indices in the OG code
        const float a  = CStats::StatReactionValue[skillStat - 23];
        const float b  = CStats::StatReactionValue[skillStat - 0x45 + 12];
        const float ab = a * b;

        float progress;
        const float statValue = CStats::GetStatValue(skillStat);
        if (statValue > 999.0f) {
            progress = 1.0f;
        } else {
            progress = (float)std::floor((double)((b * 0.1f + statValue) / ab)) * ab * 0.001f;
        }
        DrawBar(progress * 100.0f);
        NextRowY();
    }

    DrawStatRow("STAT022", STAT_STAMINA);
    DrawStatRow("STAT023", STAT_MUSCLE);
    DrawStatRow("STAT021", STAT_FAT);
    DrawStatRow("STAT025", STAT_SEX_APPEAL);

    // Game day
    y = (int16)(int32)((SCREEN_STRETCH_Y(1.0f) * 3.0f + SCREEN_STRETCH_Y(15.0f)) + (float)y);

    CFont::SetFontStyle(FONT_PRICEDOWN);
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetScale(SCREEN_STRETCH_X(1.0f) * 0.7f, SCREEN_STRETCH_Y(1.0f) * 0.7f);
    CFont::SetEdge(1);
    CFont::SetColor({ 200, 200, 200, 255 });
    CFont::SetDropColor({ 0, 0, 0, 255 });
    sprintf_s(gString, "DAY_%d", CClock::CurrentDay);
    CFont::PrintString(SCREEN_STRETCH_X(1.0f) * 160.0f + 40.0f, (float)y, TheText.Get(gString));
}

// 0x588A50
void CHud::GetRidOfAllHudMessages(bool arg0) {
    std::ranges::fill(m_pHelpMessageToPrint, '\0');
    std::ranges::fill(m_pLastHelpMessage, '\0');
    std::ranges::fill(m_pHelpMessage, '\0');
    std::ranges::fill(m_Message, '\0');

    m_ZoneNameTimer               = 0;
    m_pZoneName                   = nullptr;
    m_ZoneState                   = NAME_DONT_SHOW;
    m_nHelpMessageTimer           = 0;
    m_nHelpMessageFadeTimer       = 0;
    m_nHelpMessageState           = 0;
    m_bHelpMessageQuick           = false;
    m_nHelpMessageMaxStatValue    = 1000;
    m_nHelpMessageStatId          = 0;
    m_fHelpMessageStatUpdateValue = 0.0f;
    m_bHelpMessagePermanent       = false;
    m_fHelpMessageTime            = 1.0f;
    m_pVehicleName                = nullptr;
    m_pVehicleNameToPrint         = nullptr;
    m_VehicleNameTimer            = 0;
    m_VehicleFadeTimer            = 0;
    m_VehicleState                = NAME_DONT_SHOW;

    for (auto i = 0; i < NUM_MESSAGE_STYLES; ++i) {
        if (BigMessageX[i] != 0.0f)
            continue;

        if (arg0) { // These two styles are kept (compared by slot, not by value)
            if (i == STYLE_BOTTOM_RIGHT || i == STYLE_MIDDLE_SMALLER_HIGHER) {
                continue;
            }
        }
        std::ranges::fill(m_BigMessage[i], '\0');
    }
}

// 0x5893B0
void CHud::DrawAmmo(CPed* ped, int32 x, int32 y, float alpha) {
    const auto MAX_CLIP = 9999;

    const auto& weapon = ped->GetActiveWeapon();
    const auto& totalAmmo = weapon.m_TotalAmmo;
    const auto& ammoInClip = weapon.m_AmmoInClip;
    const auto& ammoClip = CWeaponInfo::GetWeaponInfo(weapon.m_Type, ped->GetWeaponSkill())->m_nAmmoClip;

    // NOTE: the exe works with signed 32 bit values here (imul/idiv-by-10 magic, `jg`)
    const auto inClipS = (int32)ammoInClip;
    const auto totalS  = (int32)totalAmmo;
    if (ammoClip <= 1 || ammoClip >= 1000) {
        sprintf_s(gString, "%d", totalS);
    } else {
        int32 total, current;

        if (weapon.m_Type == WEAPON_FLAMETHROWER) {
            total   = std::min((totalS - inClipS) / 10, MAX_CLIP);
            current = inClipS / 10;
        } else {
            total   = std::min(totalS - inClipS, MAX_CLIP);
            current = inClipS;
        }
        sprintf_s(gString, "%d-%d", total, current);
    }
    AsciiToGxtChar(gString, gGxtString);

    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.3f), SCREEN_STRETCH_Y(0.7f));
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
    CFont::SetCentreSize(SCREEN_STRETCH_X(640.0f)); // exe: W * (1/640) * 640 (X, not Y)
    CFont::SetProportional(true);
    CFont::SetEdge(1);
    CFont::SetDropColor({ 0, 0, 0, 255 });
    CFont::SetFontStyle(eFontStyle::FONT_SUBTITLES);

    if (   (double)(totalS - inClipS) >= (double)MAX_CLIP
        || CDarkel::FrenzyOnGoing()
        || weapon.m_Type == WEAPON_UNARMED
        || weapon.m_Type == WEAPON_DETONATOR
        || weapon.m_Type == WEAPON_DILDO1
        || weapon.m_Type == WEAPON_DILDO2
        || weapon.m_Type == WEAPON_VIBE1
        || weapon.m_Type == WEAPON_VIBE2
        || weapon.m_Type == WEAPON_FLOWERS
        || weapon.m_Type == WEAPON_CANE
        || weapon.m_Type == WEAPON_PARACHUTE
        || CWeaponInfo::GetWeaponInfo(weapon.m_Type)->m_nWeaponFire == WEAPON_FIRE_USE
        || CWeaponInfo::GetWeaponInfo(weapon.m_Type)->m_nSlot <= 1
    ) {
        CFont::SetEdge(0);
        return;
    }

    CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_LIGHT_BLUE, (uint8)alpha));
    CFont::PrintString((float)x, (float)y, gGxtString);
    CFont::SetEdge(0);
}

// NOTSA: In OG this logic is inlined multiple times (CHud::DrawPlayerInfo, CHud::DrawWanted).
// It's a variation of `CHud::DrawFadeState`, with a different handling of the "value changed" case.
// Returns the alpha to draw the element with.
static float ProcessInlinedFadeState(uint32& stateVar, uint32& fadeTimerVar, uint32& timerVar, uint32& lastValueVar, int32 currentValue) {
    int32 state     = (int32)stateVar;
    int32 fadeTimer = (int32)fadeTimerVar;
    int32 timer     = (int32)timerVar;
    float alpha     = 255.0f;

    const bool bValueChanged = (int32)lastValueVar != currentValue;
    if (bValueChanged) {
        switch (state) {
        case NAME_DONT_SHOW:
            fadeTimer = 0;
            [[fallthrough]];
        case NAME_SHOW:
        case NAME_FADE_OUT:
            state = NAME_FADE_IN;
            timer = 5;
            break;
        default:
            break;
        }
    }

    if (state != NAME_DONT_SHOW && state != 5) {
        switch (state) {
        case NAME_SHOW:
            fadeTimer = 1000;
            if (10000.0f < (float)timer) {
                state     = NAME_FADE_OUT;
                fadeTimer = 3000;
            }
            break;
        case NAME_FADE_IN:
            fadeTimer += notsa::detail::Ftol(OGTimeStepInMS());
            if (1000.0f < (float)fadeTimer) {
                fadeTimer = 1000;
                state     = NAME_SHOW;
            }
            alpha = (float)fadeTimer * 0.001f * 255.0f;
            break;
        case NAME_FADE_OUT:
            fadeTimer += notsa::detail::Ftol(CTimer::GetTimeStep() * 0.02f * -1000.0f);
            if ((float)fadeTimer < 0.0f) {
                fadeTimer = 0;
                state     = NAME_DONT_SHOW;
            }
            alpha = (float)fadeTimer * 0.001f * 255.0f;
            break;
        default:
            break;
        }
        timer += notsa::detail::Ftol(OGTimeStepInMS());
    }

    stateVar     = state;
    fadeTimerVar = fadeTimer;
    timerVar     = timer;
    if (bValueChanged) {
        lastValueVar = currentValue;
    }
    return std::clamp(alpha, 0.0f, 255.0f);
}

// 0x58EAF0
void CHud::DrawPlayerInfo() {
    const uint8 focus      = CWorld::PlayerInFocus;
    auto&       playerInfo = CWorld::Players[focus];
    CPlayerPed* const player1 = playerInfo.m_pPed;
    CPlayerPed* const player2 = CWorld::Players[1].m_pPed;

    if (bDrawClock == 1) {
        DrawClock();
    }

    // Energy lost - health/armor/breath bars
    ProcessInlinedFadeState(m_EnergyLostState, m_EnergyLostFadeTimer, m_EnergyLostTimer, m_LastTimeEnergyLost, (int32)playerInfo.m_nLastTimeEnergyLost);
    if (m_EnergyLostState != 0) {
        const auto GetBarY = [&](float pos, int8 offset) {
            return (int32)GetYPosBasedOnHealth(focus, pos, offset);
        };
        // Position for the 2nd player's bars: shifted twice (OG calls `GetYPosBasedOnHealth` with both player IDs)
        const auto GetBarY2P = [&](float pos) {
            return (int32)GetYPosBasedOnHealth(1, GetYPosBasedOnHealth(focus, pos, 12), 12);
        };
        const auto GetBarX = [](float offsetFromRight) {
            return (int32)SCREEN_STRETCH_FROM_RIGHT(offsetFromRight);
        };

        RenderHealthBar(focus, GetBarX(141.0f), GetBarY(SCREEN_STRETCH_Y(77.0f), 10));
        if (player2) {
            RenderHealthBar(1, GetBarX(141.0f), GetBarY2P(SCREEN_STRETCH_Y(194.0f)));
        }

        RenderArmorBar(focus, GetBarX(94.0f), GetBarY(SCREEN_STRETCH_Y(48.0f), 3));
        if (player2) {
            RenderArmorBar(1, GetBarX(94.0f), GetBarY2P(SCREEN_STRETCH_Y(164.0f)));
        }

        const auto IsBreathBarNeeded = [](CPlayerPed* ped, bool bOnlyIfRecentlyShown) {
            if (ped->GetIntelligence()->GetTaskSwim()) {
                return true;
            }
            if (ped->bInVehicle) {
                if (const auto* const veh = ped->m_pVehicle) {
                    if (veh->physicalFlags.bSubmergedInWater && veh->vehicleFlags.bIsDrowning) {
                        return true;
                    }
                }
            }
            if (ped->GetPlayerData()->m_fBreath < CStats::GetFatAndMuscleModifier(STAT_MOD_AIR_IN_LUNG)) {
                // NOTE: OG only does this check for the 1st player
                return !bOnlyIfRecentlyShown || (uint32)(m_LastBreathTime + 500) > CTimer::GetTimeInMS();
            }
            return false;
        };

        bool bDrawBreath1 = false;
        bool bDrawBreath2 = false;
        if (IsBreathBarNeeded(player1, true)) {
            bDrawBreath1    = true;
            m_LastBreathTime = (int32)CTimer::GetTimeInMS();
        }
        if (player2 && IsBreathBarNeeded(player2, false)) {
            bDrawBreath2    = true;
            m_LastBreathTime = (int32)CTimer::GetTimeInMS();
        }

        if (bDrawBreath1) {
            RenderBreathBar(focus, GetBarX(94.0f), GetBarY(SCREEN_STRETCH_Y(62.0f), 6));
        }
        if (bDrawBreath2 && player2) {
            RenderBreathBar(1, GetBarX(94.0f), GetBarY2P(SCREEN_STRETCH_Y(179.0f)));
        }
    }

    // Money
    {
        const auto alpha = ProcessInlinedFadeState(m_DisplayScoreState, m_DisplayScoreFadeTimer, m_DisplayScoreTimer, m_LastDisplayScore, playerInfo.m_nDisplayMoney);
        if (m_DisplayScoreState != 0) {
            DrawMoney(playerInfo, (uint8)(int32)alpha);
        }
    }

    // Weapon icon + ammo
    {
        const auto alpha = ProcessInlinedFadeState(m_WeaponState, m_WeaponFadeTimer, m_WeaponTimer, m_LastWeapon, (int32)player1->GetActiveWeapon().m_Type);
        if (m_WeaponState != 0) {
            DrawWeapon(player1, player2, alpha);
        }
    }
}

inline void CHud::DrawClock() {
    char ascii[16];
    GxtChar gxtText[16];
    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.55f), SCREEN_STRETCH_Y(1.1f));
    CFont::SetProportional(false);
    CFont::SetFontStyle(FONT_PRICEDOWN);
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetEdge(2);
    CFont::SetDropColor({0, 0, 0, 255});
    sprintf_s(ascii, "%02d:%02d", CClock::ms_nGameClockHours, CClock::ms_nGameClockMinutes);
    AsciiToGxtChar(ascii, gxtText);
    CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));
    CFont::PrintString(SCREEN_STRETCH_FROM_RIGHT(32.0f), SCREEN_STRETCH_Y(22.0f), gxtText);
    CFont::SetEdge(0);
}

inline void CHud::DrawMoney(const CPlayerInfo& playerInfo, uint8 alpha) {
    char ascii[16];
    GxtChar gxtText[16];

    if (playerInfo.m_nDisplayMoney < 0) {
        CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_RED, alpha));
        auto m_nDisplayMoney = playerInfo.m_nDisplayMoney;
        if (m_nDisplayMoney < 0) {
            m_nDisplayMoney = -m_nDisplayMoney;
        }
        sprintf_s(ascii, "-$%07d", m_nDisplayMoney);
    } else {
        CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GREEN, alpha));
        sprintf_s(ascii, "$%08d", std::abs(playerInfo.m_nDisplayMoney));
    }
    AsciiToGxtChar(ascii, gxtText);
    CFont::SetProportional(false);
    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.55f), SCREEN_STRETCH_Y(1.1f));
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetFontStyle(FONT_PRICEDOWN);
    CFont::SetDropShadowPosition(0);
    CFont::SetEdge(2);
    CFont::SetDropColor({ 0, 0, 0, uint8(alpha) });
    CFont::PrintString(SCREEN_STRETCH_FROM_RIGHT(32.0f), GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_STRETCH_Y(89.0f), 12), gxtText);
    CFont::SetEdge(0);
}

inline void CHud::DrawWeapon(CPlayerPed* ped0, CPlayerPed* ped1, float alpha) {
    const float ws = SCREEN_STRETCH_X(1.0f);
    const float hs = SCREEN_STRETCH_Y(1.0f);
    const float weaponIconOffset = SCREEN_WIDTH * 0.17343046f; // todo: magic

    DrawWeaponIcon(ped0, (int32)(SCREEN_WIDTH - (ws * 32.0f + weaponIconOffset)), (int32)(hs * 20.0f), alpha);
    if (ped1) {
        DrawWeaponIcon(
            ped1,
            (int32)(SCREEN_WIDTH - (ws * 32.0f + 111.0f)),
            (int32)GetYPosBasedOnHealth(CWorld::PlayerInFocus, hs * 138.0f, 12),
            alpha
        );
    }

    const float ammoX = (SCREEN_WIDTH - (SCREEN_WIDTH * 0.17343046f + ws * 32.0f)) + ws * 47.0f * 0.5f;
    DrawAmmo(ped0, (int32)ammoX, (int32)(hs * 20.0f + hs * 43.0f), alpha);
    if (ped1) {
        DrawAmmo(ped1, (int32)ammoX, (int32)GetYPosBasedOnHealth(CWorld::PlayerInFocus, hs * 138.0f + hs * 43.0f, 12), alpha);
    }
}

// 0x58A160
void CHud::DrawTripSkip() {
    // exe: top = (H - hs * 104) - hs * 85 (NOT H - 189 * hs); the sprite is 64 x 64 (in layout units)
    const float ws   = RvStretchX(1.0f);
    const float hs   = RvStretchY(1.0f);
    const float topY = ((float)RsGlobal.maximumHeight - hs * 104.0f) - hs * 85.0f;
    const float left = RvStretchX(54.0f);
    CRect rect; // NOTE: `top` (+4) holds the larger Y, `bottom` (+0xC) the smaller one, as in the exe
    rect.left   = left;
    rect.top    = topY + hs * 64.0f;
    rect.right  = ws * 64.0f + left;
    rect.bottom = topY;
    Sprites[SPRITE_SKIP_ICON].Draw(rect, CRGBA(255, 255, 255, 255));

    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.3f), SCREEN_SCALE_Y(0.7f));
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
    CFont::SetCentreSize(SCREEN_WIDTH);
    CFont::SetProportional(true);
    CFont::SetEdge(1);
    CFont::SetDropColor({ 0, 0, 0, 255 });
    CFont::SetFontStyle(eFontStyle::FONT_MENU);
    CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));
    CFont::PrintString(
        ws * 64.0f * 0.5f + ws * 54.0f,
        (topY + hs * 64.0f) - hs * 2.0f,
        TheText.Get("FEC_TSK") // TRIP SKIP
    );
}

// 0x58D9A0
void CHud::DrawWanted() {
    // OG: function local static, set when the wanted level didn't change since the last frame
    static auto& bWantedLevelUnchanged = StaticRef<bool>(0xBAB228);

    const auto* const wanted         = FindPlayerWanted();
    const auto        wantedLevel    = (int32)wanted->m_WantedLevel;
    const auto        wantedLevelBP  = (int32)FindPlayerWanted()->m_WantedLevelBeforeParole;

    // OG: this is an inlined variation of `DrawFadeState(WANTED_STATE, ...)`
    const bool bUnchanged = (int32)m_LastWanted == wantedLevel;
    const float alpha     = ProcessInlinedFadeState(m_WantedState, m_WantedFadeTimer, m_WantedTimer, m_LastWanted, wantedLevel);
    bWantedLevelUnchanged = bUnchanged;

    if (m_WantedState == NAME_DONT_SHOW) {
        return;
    }

    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(1.0f) * 0.605f, SCREEN_STRETCH_Y(1.0f) * 1.21f);
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetProportional(true);
    CFont::SetFontStyle(FONT_GOTHIC);

    GxtChar starText[8];
    AsciiToGxtChar("]", starText);

    float posX = SCREEN_WIDTH - SCREEN_STRETCH_X(1.0f) * 29.0f;
    if (!((wantedLevel > 0 && bWantedLevelUnchanged) || wantedLevelBP > 0)) {
        return;
    }

    const auto alpha8 = (uint8)(int32)alpha;
    for (auto i = 0; i < 6; i++) {
        CFont::SetEdge(1);
        CFont::SetDropColor({ 0, 0, 0, alpha8 });
        CFont::SetScale(SCREEN_STRETCH_X(1.0f) * 0.605f, SCREEN_STRETCH_Y(1.0f) * 1.21f);

        const auto timeMs = CTimer::GetTimeInMS();
        if (wantedLevel > i && (timeMs > FindPlayerWanted()->m_LastTimeWantedLevelChanged + 2000 || (CTimer::GetFrameCounter() & 4))) {
            // Active wanted star
            CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GOLD, alpha8));
            float posY = SCREEN_STRETCH_Y(1.0f) * 114.0f;
            if (!(101.0f <= (float)CWorld::Players[CWorld::PlayerInFocus].m_nMaxHealth)) {
                posY -= SCREEN_STRETCH_Y(1.0f) * 12.0f;
            }
            CFont::PrintString(posX, posY, starText);
        } else if (wantedLevelBP > i && (CTimer::GetFrameCounter() & 4)) {
            // Flashing "parole" star (darker gold)
            const auto& gold = HudColour.m_aColours[HUD_COLOUR_GOLD];
            CFont::SetColor(CRGBA{
                (uint8)(int32)((float)gold.r * 0.8f),
                (uint8)(int32)((float)gold.g * 0.8f),
                (uint8)(int32)((float)gold.b * 0.8f),
                alpha8
            });
            CFont::PrintString(posX, GetYPosBasedOnHealth(CWorld::PlayerInFocus, SCREEN_STRETCH_Y(1.0f) * 114.0f, 12), starText);
        } else if (wantedLevel <= i) {
            // Empty star
            CFont::SetEdge(0);
            CFont::SetColor(CRGBA{ 0, 0, 0, (uint8)(int32)(alpha * 0.7f) });
            CFont::SetScale(SCREEN_STRETCH_X(1.0f) * 0.605f * 1.2f, SCREEN_STRETCH_Y(1.0f) * 1.21f * 1.2f);
            const float hs = SCREEN_STRETCH_Y(1.0f);
            CFont::PrintString(posX, GetYPosBasedOnHealth(CWorld::PlayerInFocus, hs * 114.0f, 12) - (hs + hs), starText);
        }

        posX -= SCREEN_STRETCH_X(1.0f) * 18.0f;
    }
    CFont::SetEdge(0);
}

// 0x58D7D0
void CHud::DrawWeaponIcon(CPed* ped, int32 x, int32 y, float alpha) {
    const auto x0 = (float)x;
    const auto y0 = (float)y;
    const float width  = SCREEN_STRETCH_X(47.0f);
    const float height = SCREEN_STRETCH_Y(58.0f);
    const float halfWidth  = width / 2.0f;
    const float halfHeight = height / 2.0f;

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERLINEAR));

    auto modelId = ped->GetActiveWeapon().GetWeaponInfo().m_nModelId1;
    if (modelId <= 0) {
        Sprites[SPRITE_FIST].Draw({ x0, y0, width + x0, height + y0 }, CRGBA(255, 255, 255, (uint8)alpha));
        return;
    }

    auto mi = CModelInfo::GetModelInfo(modelId);
    auto txd = CTxdStore::ms_pTxdPool->GetAt(mi->m_nTxdIndex);
    if (!txd || !txd->m_pRwDictionary) // NOTSA: `!txd` (the exe dereferences the null of a free slot); the dictionary check is the exe's
        return;

    auto texture = RwTexDictionaryFindHashNamedTexture(txd->m_pRwDictionary, CKeyGen::AppendStringToKey(mi->m_nKey, "ICON"));
    if (!texture)
        return;

    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,   RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(texture)));
    CSprite::RenderOneXLUSprite(
        { x0 + halfWidth, y0 + halfHeight, 1.0f },
        { halfWidth, halfHeight },
        255u, 255u, 255u, 255,
        1.0f,
        255,
        0, 0
    );
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,  RWRSTATE(FALSE));
}

// 0x5890A0
void CHud::RenderArmorBar(int32 playerId, int32 x, int32 y) {
    auto* player = FindPlayerPed(playerId);
    if ((m_ItemToFlash == ITEM_ARMOUR && EachFrames(8)) || !(player->m_fArmour > 1.0f)) // FCOMP + JNE (C0|C3): NaN returns too
        return;

    const auto info = player->GetPlayerInfoForThisPlayerPed();
    CSprite2d::DrawBarChart(
        (float)x,
        (float)y,
        (uint16)SCREEN_STRETCH_X(62.0f),
        (uint8)SCREEN_STRETCH_Y(9.0f),
        player->m_fArmour / (float)info->m_nMaxArmour * 100.0f,
        false,
        false,
        true,
        HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY),
        CRGBA(0, 0, 0, 0)
    );
}

// 0x589190
void CHud::RenderBreathBar(int32 playerId, int32 x, int32 y) {
    if (m_ItemToFlash == ITEM_BREATH && EachFrames(8))
        return;

    auto* player = FindPlayerPed(playerId);
    CSprite2d::DrawBarChart(
        (float)x,
        (float)y,
        (uint16)SCREEN_STRETCH_X(62.0f),
        (uint8)SCREEN_STRETCH_Y(9.0f),
        player->GetPlayerData()->m_fBreath / CStats::GetFatAndMuscleModifier(STAT_MOD_AIR_IN_LUNG) * 100.0f,
        false,
        false,
        true,
        HudColour.GetRGB(HUD_COLOUR_LIGHT_BLUE),
        CRGBA(0, 0, 0, 0)
    );
}

// 0x589270
void CHud::RenderHealthBar(int32 playerId, int32 x, int32 y) {
    if (m_ItemToFlash == ITEM_HEALTH && EachFrames(8))
        return;

    auto* player = FindPlayerPed(playerId);
    if ((int16)notsa::detail::Ftol(player->m_fHealth) < 10 && EachFrames(8)) // `cmp ax, 0xA` after _ftol2
        return;

    const float x109 = SCREEN_STRETCH_X(109.0f);
    const auto info = player->GetPlayerInfoForThisPlayerPed();
    // exe: (W * (1/640) * maxHealth) * 109 (spilled to float) / modifier; NOT (x109 * maxHealth)
    const float totalWidthF = RvStretchX(1.0f) * (float)info->m_nMaxHealth * 109.0f;
    const auto totalWidth = uint16(notsa::detail::Ftol(totalWidthF / CStats::GetFatAndMuscleModifier(STAT_MOD_10)));

    CSprite2d::DrawBarChart(
        x109 - (float)totalWidth + (float)x,
        (float)y,
        totalWidth,
        (uint8)SCREEN_STRETCH_Y(9.0f),
        player->m_fHealth * 100.0f / (float)info->m_nMaxHealth,
        false,
        false,
        true,
        HudColour.GetRGB(HUD_COLOUR_RED),
        CRGBA(0, 0, 0, 0)
    );
}
