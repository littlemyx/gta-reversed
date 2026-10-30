#include "StdInc.h"

#include "app_game.h"
#include "LoadingScreen.h"
#include "PlantMgr.h"
#include "Shadows.h"
#include "Clouds.h"
#include "MovingThings.h"
#include "BreakManager_c.h"
#include "RealTimeShadowManager.h"
#include "Credits.h"
#include "Securom.h"
#include "UserDisplay.h"
#include "Garages.h"
#include "UIRenderer.h"
#include "Gamma.h"
#include "Birds.h"
#include "Skidmarks.h"
#include "Ropes.h"
#include "Glass.h"
#include "WaterCannons.h"
#include "VehicleRecording.h"
#include "PostEffects.h"
#include "CarFXRenderer.h"

#include "extensions/Configs/FastLoader.hpp"
#ifdef NOTSA_STANDALONE_RUN
#include "standalone/Lockstep.h"
#define RD_NOTE(x) notsa::lockstep::Note(x) // REPLAY-DIFF: "X fc what" trace line
#else
#define RD_NOTE(x) ((void)0)
#endif
#ifdef NOTSA_STATEHASH
#include "portable/statehash.h" // RR-1: deterministic state-hash harness (hook lines below are #ifdef NOTSA_STATEHASH)
#endif

void AppGameInjectHooks() {
    RH_ScopedCategory("App");
    RH_ScopedNamespaceName("Game");

    RH_ScopedGlobalInstall(GameInit, 0x5BF3B0);
    RH_ScopedGlobalInstall(InitialiseGame, 0x53E580);
    RH_ScopedGlobalInstall(TheGame, 0x53DF10);

    RH_ScopedGlobalInstall(RwInitialize, 0x5BF390);
    RH_ScopedGlobalInstall(RwTerminate, 0x53D910);

    RH_ScopedGlobalInstall(RenderEffects, 0x53E170);
    RH_ScopedGlobalInstall(RenderScene, 0x53DF40);
    RH_ScopedGlobalInstall(RenderMenus, 0x53E530);
    RH_ScopedGlobalInstall(Render2dStuff, 0x53E230, { .Locked = true }); // Must be hooked at all times otherwise game locks!
    RH_ScopedGlobalInstall(RenderDebugShit, 0x53E160);

    RH_ScopedGlobalInstall(Idle, 0x53E920, { .Locked = true }); // Must be hooked at all times otherwise game locks!
    RH_ScopedGlobalInstall(FrontendIdle, 0x53E770, { .Locked = true }); // Must be hooked at all times otherwise imgui stops working!
}

// 0x5BF3B0
void GameInit() {
    VERIFY(RwInitialize(nullptr));
#ifdef NOTSA_STATEHASH
    if (notsa::rr::Active()) { notsa::rr::PinHeap(); srand(notsa::rr::Seed()); return; } // RR: fixed seed, not the wall clock; RenderWare allocations zeroed from here on (right after RwEngineInit)
#endif
    srand(RsTimer());
}

// 0x53E580
void InitialiseGame() {
    ZoneScoped;

    NOTSA_GLOBAL_LOCAL(version_number, 0xB72C68, (int16), {});
    version_number = 78;

    CGame::Initialise(GAME_LEVEL_FILE);
    AudioEngine.Restart();
    if (FrontEndMenuManager.m_bLoadingData) {
        FrontEndMenuManager.m_bStartGameLoading = true;
    }
}

// unused
// 0x53DF10
void TheGame() {
    NOTSA_LOG_DEBUG("Into TheGame!!!\n");
    CTimer::Initialise();
    CGame::Initialise(GAME_LEVEL_FILE);
    CLoadingScreen::SetLoadingBarMsg("Starting Game", nullptr);
    CLoadingScreen::NewChunkLoaded();
}

// 0x5BF390
bool RwInitialize(void* param) {
    return RsRwInitialize(param) && CGame::InitialiseRenderWare();
}

// 0x53D910
void RwTerminate() {
    CGame::ShutdownRenderWare();
    ResetGammaWhenExiting();
    RsRwTerminate();
}

// 0x53E170
void RenderEffects() {
    ZoneScoped;

    CBirds::Render();
    CSkidmarks::Render();
    CRopes::Render();
    CGlass::Render();
    CMovingThings::Render();
    CVisibilityPlugins::RenderReallyDrawLastObjects();
    CCoronas::Render();
    g_fx.Render(TheCamera.m_pRwCamera, false);
    CWaterCannons::Render();
    CWaterLevel::RenderWaterFog();
    CClouds::MovingFogRender();
    CClouds::VolumetricCloudsRender();
    if (CHeli::NumberOfSearchLights || CTheScripts::NumberOfScriptSearchLights) {
        CHeli::Pre_SearchLightCone();
        CHeli::RenderAllHeliSearchLights();
        CTheScripts::RenderAllSearchLights();
        CHeli::Post_SearchLightCone();
    }
    CWeaponEffects::Render();
    if (CReplay::Mode != MODE_PLAYBACK && !CPad::GetPad(0)->DisablePlayerControls) {
        FindPlayerPed()->DrawTriangleForMouseRecruitPed();
    }
    CSpecialFX::Render();
    CVehicleRecording::Render();
    CPointLights::RenderFogEffect();
    CRenderer::RenderFirstPersonVehicle();
    CPostEffects::Render();

    notsa::ui::UIRenderer::GetInstance().Render3D();
}

// 0x53DF40
void RenderScene() {
    ZoneScoped;

    // 0x53DF49: the exe's flag is `UnderWaterness > 0` (a NaN compares as "not above water level" = false); the port's old `underWater = UnderWaterness <= 0` had the opposite NaN result
    const bool isUnderWater = CWeather::UnderWaterness > 0.0f;

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));

    if (CMirrors::TypeOfMirror == MIRROR_TYPE_NONE) {
        CMovingThings::Render_BeforeClouds();
        CClouds::Render();
    }

    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESHADEMODE, RWRSTATE(rwSHADEMODEGOURAUD));

    CCarFXRenderer::PreRenderUpdate();
    CRenderer::RenderRoads();
    CCoronas::RenderReflections();
    CRenderer::RenderEverythingBarRoads();
    g_breakMan.Render(false);

    CRenderer::RenderFadingInUnderwaterEntities();

    // If under water render this immediately, otherwise
    // only after rendering the stuff below.
    if (!isUnderWater) {
        RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(rwCULLMODECULLNONE));
        CWaterLevel::RenderWater();
        RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(rwCULLMODECULLBACK));
    }

    CRenderer::RenderFadingInEntities();
    if (!CMirrors::bRenderingReflection) {
        float nearClipPlaneOld = RwCameraGetNearClipPlane(Scene.m_pRwCamera);
        float farPlane = RwCameraGetFarClipPlane(Scene.m_pRwCamera);

        float v3;
        float z = CCamera::GetActiveCamera().m_vecFront.z;
        if (!(z > 0.0f)) // 0x53E05A: fcom + `test ah, 0x41; jne` => z <= 0 or NaN takes the -z arm
            v3 = -z;
        else
            v3 = 0.0f;

        constexpr float flt_8CD4F0 = 2.0f;
        constexpr float flt_8CD4EC = 5.9604645e-8f; // Same as 1 / (float)(1 << 24) => 1 / 16777216.f ; Not sure if that means anything..

        float unknown = ((flt_8CD4F0 * flt_8CD4EC * 0.25f - flt_8CD4F0 * flt_8CD4EC) * v3 + flt_8CD4F0 * flt_8CD4EC) * (farPlane - nearClipPlaneOld);

        RwCameraEndUpdate(Scene.m_pRwCamera);

        RwCameraSetNearClipPlane(Scene.m_pRwCamera, unknown + nearClipPlaneOld);
        RwCameraBeginUpdate(Scene.m_pRwCamera);
        CShadows::UpdateStaticShadows();
        CShadows::RenderStaticShadows();
        CShadows::RenderStoredShadows();
        RwCameraEndUpdate(Scene.m_pRwCamera);
        RwCameraSetNearClipPlane(Scene.m_pRwCamera, nearClipPlaneOld);

        RwCameraBeginUpdate(Scene.m_pRwCamera);
    }

    g_breakMan.Render(true);
    CPlantMgr::Render();

    RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(rwCULLMODECULLNONE));

    if (CMirrors::TypeOfMirror == MIRROR_TYPE_NONE) {
        CClouds::RenderBottomFromHeight();
        CWeather::RenderRainStreaks();
        CCoronas::RenderSunReflection();
    }

    if (isUnderWater) {
        RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(rwCULLMODECULLNONE));
        CWaterLevel::RenderWater();
        RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(rwCULLMODECULLBACK));
    }

    CStencilShadows::RenderStencilShadows();
}

// 0x53E530
void RenderMenus() {
    ZoneScoped;

    if (FrontEndMenuManager.m_bMenuActive) {
        FrontEndMenuManager.DrawFrontEnd();
    }
}

// 0x53E230
void Render2dStuff() {
    ZoneScoped;

#ifdef NOTSA_STANDALONE_RUN
    // NOTSA-AUDIT: no RenderDebugShit() here - the exe's Render2dStuff (0x53E230) starts with its first RwRenderStateSet; the call ran RenderTheScriptDebugLines + DefinedState every frame
#else
#line 225
    RenderDebugShit(); // NOTSA, temp
#endif
#line 226

    const auto DrawOuterZoomBox = []() {
        CPed* player = FindPlayerPed();
        eWeaponType weaponType = WEAPON_UNARMED;
        if (player)
            weaponType = player->GetActiveWeapon().m_Type;
        eCamMode camMode = CCamera::GetActiveCamera().m_nMode;
        bool firstPersonWeapon = false;
        if (camMode == MODE_SNIPER || camMode == MODE_SNIPER_RUNABOUT || camMode == MODE_ROCKETLAUNCHER || camMode == MODE_ROCKETLAUNCHER_RUNABOUT || camMode == MODE_CAMERA ||
            camMode == MODE_HELICANNON_1STPERSON) {
            firstPersonWeapon = true;
        }

        if ((weaponType == WEAPON_SNIPERRIFLE || weaponType == WEAPON_ROCKET) && firstPersonWeapon) {
            CRGBA black(0, 0, 0, 255);
            // 0x53E34B/0x53E3A3/0x53E453: the exe halves the integer resolution (cdq/sub/sar = truncating `/ 2`), then converts; `SCREEN_xxx / 2` on the float differs for odd sizes
            const float halfW = (float)(RsGlobal.maximumWidth / 2);
            const float halfH = (float)(RsGlobal.maximumHeight / 2);
            if (weaponType == WEAPON_ROCKET) {
                CSprite2d::DrawRect(CRect(0.0f, 0.0f, SCREEN_WIDTH, halfH - SCREEN_SCALE_Y(180.0f)), black);
                CSprite2d::DrawRect(CRect(0.0f, halfH + SCREEN_SCALE_Y(170.0f), SCREEN_WIDTH, SCREEN_HEIGHT), black);
            } else {
                CSprite2d::DrawRect(CRect(0.0f, 0.0f, SCREEN_WIDTH, halfH - SCREEN_SCALE_Y(210.0f)), black);
                CSprite2d::DrawRect(CRect(0.0f, halfH + SCREEN_SCALE_Y(210.0f), SCREEN_WIDTH, SCREEN_HEIGHT), black);
            }
            CSprite2d::DrawRect(CRect(0.0f, 0.0f, halfW - SCREEN_SCALE_X(210.0f), SCREEN_HEIGHT), black);
            CSprite2d::DrawRect(CRect(halfW + SCREEN_SCALE_X(210.0f), 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT), black);
        }
    };

    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE, RWRSTATE(rwRENDERSTATENARENDERSTATE));
    RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(rwCULLMODECULLNONE));

    CReplay::Display();
    CPickups::RenderPickUpText();
    if (TheCamera.m_bWideScreenOn && !FrontEndMenuManager.m_bWidescreenOn)
        TheCamera.DrawBordersForWideScreen();
    DrawOuterZoomBox();
    AudioEngine.DisplayRadioStationName();
    CHud::Draw();
    CSpecialFX::Render2DFXs();
    CUserDisplay::OnscnTimer.ProcessForDisplay();
    CMessages::Display(true);
    CDarkel::DrawMessages();
    CGarages::PrintMessages();
    CFont::DrawFonts();
}

// 0x53E160
void RenderDebugShit() {
    PUSH_RENDERGROUP("RenderDebugShit");
    CTheScripts::RenderTheScriptDebugLines();
#ifndef FINAL
    // if(gbShowCollisionLines) CRenderer::RenderCollisionLines();
    // ThePaths.DisplayPathData();
    // CDebug::DrawLines();
    DefinedState();
#endif
    POP_RENDERGROUP();
}

// 0x53E920
void Idle(void* param) {
    ZoneScoped;

    // Unconditional frame lock of the original `Idle` (0x53E930..0x53E94D busy-wait, 0x53E94F..0x53E963 store): spin until at least
    // 14 ms (= 71 fps at most) have passed since the previous `Idle`, whatever the frame limiter setting is. Game logic relies on that
    // minimum frame time (at >1000 fps the new-game intro never progresses). All values are unsigned 32-bit (`div`, `sub`, `cmp`/`jb`),
    // so a wrap-around behaves exactly like the exe's. The two discarded calls at 0x53E923/0x53E928 have no side effects and are omitted.
    NOTSA_GLOBAL_LOCAL(g_LastTickTime, 0xB72CA8, (uint32), {});
    uint32 nowMs;
#ifdef NOTSA_STATEHASH
    notsa::rr::AdvanceClock(); // RR: one fixed virtual step (>= 14 ms) per frame: the lock below is satisfied by construction (on a frozen clock it would spin for ever, and a virtual clock must not advance inside a spin)
    if (!notsa::rr::Active())
#endif
    do {
        nowMs = CTimer::GetCurrentTimeInCycles() / CTimer::GetCyclesPerMillisecond();
    } while (nowMs - g_LastTickTime < 14);
    g_LastTickTime = CTimer::GetCurrentTimeInCycles() / CTimer::GetCyclesPerMillisecond();

    CTimer::Update();
    CSprite2d::InitPerFrame();
    CFont::InitPerFrame();
    CPointLights::NumLights = 0;
    CGame::Process();
#ifdef NOTSA_STATEHASH
    if (!notsa::rr::NoAudio()) // NOTSA_RR_NOAUDIO=1: the audio engine is not serviced at all (the cross-build comparison does not depend on how a DirectSound device is emulated)
#endif
    AudioEngine.Service();
#ifdef NOTSA_STATEHASH
    notsa::rr::OnFrame(); // RR: per-frame state hash (end of the simulation part)
#endif
    SetLightsWithTimeOfDayColour(Scene.m_pRpWorld);
    if (!param) {
        RD_NOTE("skip: param false");
        return;
    }

    if (FrontEndMenuManager.m_bMenuActive || TheCamera.GetScreenFadeStatus() == eNameState::NAME_FADE_IN) {
        CDraw::CalculateAspectRatio();
        CameraSize(Scene.m_pRwCamera, nullptr, SCREEN_VIEW_WINDOW, SCREEN_ASPECT_RATIO);
        CVisibilityPlugins::SetRenderWareCamera(Scene.m_pRwCamera);
        RwCameraClear(Scene.m_pRwCamera, &gColourTop, rwCAMERACLEARZ);
        if (!RsCameraBeginUpdate(Scene.m_pRwCamera)) {
            RD_NOTE("skip: menu BeginUpdate failed");
            return;
        }
    }

    if (!FrontEndMenuManager.m_bMenuActive && TheCamera.GetScreenFadeStatus() != eNameState::NAME_FADE_IN) {

        // SDL already constraints the mouse pointer using relative mode
#ifndef NOTSA_USE_SDL3
        if (!notsa::ui::UIRenderer::GetInstance().IsActive()) {
            FrontEndMenuManager.CentreMousePointer();
        }
#endif

        CRenderer::ConstructRenderList();
        CRenderer::PreRender();
        CWorld::ProcessPedsAfterPreRender();
        g_realTimeShadowMan.Update();
        CMirrors::BeforeMainRender();

        bool started;
        if (CWeather::LightningFlash) {
            CTimeCycle::SetFogRed(255);
            CTimeCycle::SetFogGreen(255);
            CTimeCycle::SetFogBlue(255);
            started = DoRWStuffStartOfFrame_Horizon(255, 255, 255, 255, 255, 255, 255);
        } else {
            started = DoRWStuffStartOfFrame_Horizon(
                CTimeCycle::GetSkyTopRed(), CTimeCycle::GetSkyTopGreen(), CTimeCycle::GetSkyTopBlue(),
                CTimeCycle::GetSkyBottomRed(), CTimeCycle::GetSkyBottomGreen(), CTimeCycle::GetSkyBottomBlue(),
                255
            );
        }
        if (!started) {
            RD_NOTE("skip: Horizon failed");
            return;
        }

        DefinedState();
        RwCameraSetFarClipPlane(Scene.m_pRwCamera, CTimeCycle::GetFarClip());
        Scene.m_pRwCamera->fogPlane = CTimeCycle::GetFogStart();
        CMirrors::RenderMirrorBuffer();
        RenderScene();
        CVisibilityPlugins::RenderWeaponPedsForPC();
        CVisibilityPlugins::ResetWeaponPedsForPC();
        RenderEffects();
        if (TheCamera.m_nBlurType == eMotionBlurType::NONE || TheCamera.m_nBlurType == eMotionBlurType::LIGHT_SCENE) {
            if (TheCamera.m_fScreenReductionPercentage > 0.0f) {
                TheCamera.SetMotionBlurAlpha(150);
            }
        }
        TheCamera.RenderMotionBlur();
        Render2dStuff();
    }

    RenderMenus();
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));
    DoFade();
    CHud::DrawAfterFade();
    CMessages::Display(false);
    CFont::DrawFonts();
    CCredits::Render();
    CDebug::DebugDisplayTextBuffer();
    FlushObrsPrintfs();

    // NOTSA: ImGui menu draw loop
    notsa::ui::UIRenderer::GetInstance().DrawLoop();

    RwCameraEndUpdate(Scene.m_pRwCamera);
    RsCameraShowRaster(Scene.m_pRwCamera);
    RD_NOTE("rendered");
}

// 0x53E770
void FrontendIdle() {
    ZoneScoped;

#ifdef NOTSA_STATEHASH
    notsa::rr::AdvanceClock();
#endif
    CDraw::CalculateAspectRatio();
    CTimer::Update();
    CSprite2d::SetRecipNearClip();
    CSprite2d::InitPerFrame();
    CFont::InitPerFrame();
    CPad::UpdatePads();

    if (FrontEndMenuManager.m_bActivateMenuNextFrame) {
        FrontEndMenuManager.m_bMenuActive = true;
    }
    FrontEndMenuManager.Process();

    if (RsGlobal.quit) {
        return;
    }

    AudioEngine.Service();
    CameraSize(Scene.m_pRwCamera, nullptr, SCREEN_VIEW_WINDOW, SCREEN_ASPECT_RATIO);
    CVisibilityPlugins::SetRenderWareCamera(Scene.m_pRwCamera);

    if (g_FastLoaderConfig.ShouldLoadSaveGame()) {
        return; // Don't render anything
    }

    RwCameraClear(Scene.m_pRwCamera, &gColourTop, rwCAMERACLEARZ);
    if (RsCameraBeginUpdate(Scene.m_pRwCamera)) {
        DefinedState();

        if (FrontEndMenuManager.m_bMenuActive) {
            FrontEndMenuManager.DrawFrontEnd();
        }

        SecuromStateDisplay();
        DoFade();
        CHud::DrawAfterFade();
        CMessages::Display(false);
        CFont::DrawFonts();

        if (CCredits::bCreditsGoing && !FrontEndMenuManager.m_bMenuActive) {
            CCredits::RenderCredits();
        }

        CFont::DrawFonts();
        CDebug::DebugDisplayTextBuffer();
        FlushObrsPrintfs();

        // NOTSA: ImGui menu draw loop
        notsa::ui::UIRenderer::GetInstance().DrawLoop();

        RwCameraEndUpdate(Scene.m_pRwCamera);
        RsCameraShowRaster(Scene.m_pRwCamera);
    }
}
