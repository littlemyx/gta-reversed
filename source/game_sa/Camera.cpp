#include "StdInc.h"

#include "Camera.h"

#include "TaskSimpleGangDriveBy.h"
#include "TaskSimpleHoldEntity.h"
#include "TaskSimpleDuck.h"
#include "Hud.h"
#include "FileLoader.h"
#include "MBlur.h"
#include "TaskSimpleSwim.h"
#include "TaskSimpleArrestPed.h"

#include <numbers>

auto& TheCamera = StaticRef<CCamera>(0xB6F028);
auto& gbModelViewer = StaticRef<bool>(0xBA6728);
auto& gbCineyCamMessageDisplayed = StaticRef<int8>(0x8CC381); // 2
auto& gCameraDirection = StaticRef<int32>(0x8CC384);         // 3
auto& gCameraMode = StaticRef<eCamMode>(0x8CC388);        // -1
auto& gLastTime2PlayerCameraWasOK = StaticRef<uint32>(0xB6EC24);    // 0
auto& gLastTime2PlayerCameraCollided = StaticRef<uint32>(0xB6EC28); // 0
auto& gPlayerPedVisible = StaticRef<bool>(0x8CC380); // true
auto& gCurCamColVars = StaticRef<uint8>(0x8CCB80);
auto& gCurDistForCam = StaticRef<float>(0x8CCB84);
auto& gpCamColVars = StaticRef<float*>(0xB6FE88);
static auto& gCamColLastRadius = StaticRef<float>(0xB6EC6C);
auto& gCamColVars = StaticRef<float[28][6]>(0x8CC8E0);
static auto& gNearClipPedDistDivisor = StaticRef<float>(0xB6EC68); // NOTSA name: unidentified global
static auto& gNearClipPedScale = StaticRef<float>(0x8CCC84);       // NOTSA name: unidentified global, 0.25

// Process: NOTSA names, unidentified globals
static auto& gDrunkCamAngle              = StaticRef<float>(0xB6EC30); // Drunk camera wobble phase in degrees (+5 each frame while drunk)
static auto& gbFirstPersonUpsideDownBlur = StaticRef<bool>(0xB70142);  // Set while the blur for the 1st person cam in an (almost) upside down vehicle is active
static auto& gbCamUnkB70143              = StaticRef<bool>(0xB70143);  // Only ever cleared (in `Process`)

// CopyCameraMatrixToRWCam: previous RW frame vectors (+ MSVC static-init guard bits at 0xB6FFC0)
static auto& gPrevRwCamRight    = StaticRef<CVector>(0xB6FF90);
static auto& gPrevRwCamUp       = StaticRef<CVector>(0xB6FF9C);
static auto& gPrevRwCamAt       = StaticRef<CVector>(0xB6FFA8);
static auto& gPrevRwCamPos      = StaticRef<CVector>(0xB6FFB4);
static auto& gPrevRwCamInitMask = StaticRef<uint32>(0xB6FFC0);

// CameraColDetAndReact: NOTSA names
static auto& gCamColLowestSphereZ  = StaticRef<float>(0xB700EC);
static auto& gCamColLastModelIdx   = StaticRef<int32>(0xB700F0);
static auto& gCamColLastSource     = StaticRef<CVector>(0xB700DC);
static auto& gCamColLastSourceInit = StaticRef<uint32>(0xB700E8); // MSVC static-init guard of `gCamColLastSource`

CCam& CCamera::GetActiveCamera() {
    return TheCamera.m_aCams[TheCamera.m_nActiveCam];
}

void CCamera::InjectHooks() {
    RH_ScopedClass(CCamera);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(GetArrPosForVehicleType, 0x50AF00);
    RH_ScopedInstall(GetPositionAlongSpline, 0x50AF80);
    RH_ScopedInstall(GetRoughDistanceToGround, 0x516B00);
    RH_ScopedInstall(InitialiseCameraForDebugMode, 0x50AF90);
    RH_ScopedInstall(ProcessObbeCinemaCameraPed, 0x50B880);
    RH_ScopedInstall(ProcessWideScreenOn, 0x50B890);
    RH_ScopedInstall(RenderMotionBlur, 0x50B8F0);
    RH_ScopedInstall(SetCameraDirectlyBehindForFollowPed_CamOnAString, 0x50BD40);
    RH_ScopedInstall(SetCameraDirectlyInFrontForFollowPed_CamOnAString, 0x50BD70);
    RH_ScopedInstall(SetCamPositionForFixedMode, 0x50BEC0);
    RH_ScopedInstall(SetFadeColour, 0x50BF00);
    RH_ScopedInstall(SetMotionBlur, 0x50BF40);
    RH_ScopedInstall(SetMotionBlurAlpha, 0x50BF80);
    RH_ScopedInstall(SetNearClipScript, 0x50BF90);
    RH_ScopedInstall(SetNewPlayerWeaponMode, 0x50BFB0);
    RH_ScopedInstall(SetRwCamera, 0x50C100);
    RH_ScopedInstall(SetWideScreenOn, 0x50C140);
    RH_ScopedInstall(SetWideScreenOff, 0x50C150);
    RH_ScopedInstall(StartCooperativeCamMode, 0x50C260);
    RH_ScopedInstall(StopCooperativeCamMode, 0x50C270);
    RH_ScopedInstall(AllowShootingWith2PlayersInCar, 0x50C280);
    RH_ScopedInstall(StoreValuesDuringInterPol, 0x50C290);
    RH_ScopedInstall(ProcessScriptedCommands, 0x516AE0);
    RH_ScopedInstall(FinishCutscene, 0x514950);
    RH_ScopedInstall(LerpFOV, 0x50D280);
    RH_ScopedInstall(UpdateAimingCoors, 0x50CB10);
    RH_ScopedInstall(SetColVarsAimWeapon, 0x50CBF0);
    RH_ScopedInstall(ClearPlayerWeaponMode, 0x50AB10);
    RH_ScopedInstall(DontProcessObbeCinemaCamera, 0x50AB40);
    RH_ScopedInstall(Enable1rstPersonCamCntrlsScript, 0x50AC00);
    RH_ScopedInstall(FindCamFOV, 0x50AD20);
    RH_ScopedInstall(GetFading, 0x50ADE0);
    RH_ScopedInstall(GetFadingDirection, 0x50ADF0);
    RH_ScopedInstall(Get_Just_Switched_Status, 0x50AE10);
    RH_ScopedInstall(GetGameCamPosition, 0x50AE50);

    RH_ScopedInstall(Constructor, 0x51A450);
    RH_ScopedInstall(InitCameraVehicleTweaks, 0x50A3B0);
    RH_ScopedInstall(ApplyVehicleCameraTweaks, 0x50A480);
    RH_ScopedInstall(CamShake, 0x50A9F0);
    RH_ScopedInstall(GetScreenRect, 0x50AB50);
    RH_ScopedInstall(Enable1rstPersonWeaponsCamera, 0x50AC10);
    RH_ScopedInstall(Fade, 0x50AC20);
    RH_ScopedInstall(Find3rdPersonQuickAimPitch, 0x50AD40);
    RH_ScopedInstall(GetCutSceneFinishTime, 0x50AD90);
    RH_ScopedInstall(GetScreenFadeStatus, 0x50AE20);
    RH_ScopedInstall(GetLookingLRBFirstPerson, 0x50AE60);
    RH_ScopedInstall(GetLookDirection, 0x50AE90);
    RH_ScopedInstall(GetLookingForwardFirstPerson, 0x50AED0);
    RH_ScopedInstall(CopyCameraMatrixToRWCam, 0x50AFA0);
    RH_ScopedInstall(CalculateMirroredMatrix, 0x50B380);
    RH_ScopedInstall(DealWithMirrorBeforeConstructRenderList, 0x50B510);
    RH_ScopedInstall(ProcessFade, 0x50B5D0);
    RH_ScopedInstall(ProcessMusicFade, 0x50B6D0);
    RH_ScopedInstall(Restore, 0x50B930);
    RH_ScopedInstall(RestoreWithJumpCut, 0x50BAB0);
    RH_ScopedInstall(SetCamCutSceneOffSet, 0x50BD20);
    RH_ScopedInstall(SetCameraDirectlyBehindForFollowPed_ForAPed_CamOnAString, 0x50BDA0);
    RH_ScopedInstall(SetCameraDirectlyInFrontForFollowPed_ForAPed_CamOnAString, 0x50BE30);
    RH_ScopedInstall(Using1stPersonWeaponMode, 0x50BFF0);
    RH_ScopedInstall(SetParametersForScriptInterpolation, 0x50C030);
    RH_ScopedInstall(SetPercentAlongCutScene, 0x50C070);
    RH_ScopedInstall(SetZoomValueFollowPedScript, 0x50C160);
    RH_ScopedInstall(SetZoomValueCamStringScript, 0x50C1B0);
    RH_ScopedInstall(UpdateTargetEntity, 0x50C360);
    RH_ScopedInstall(TakeControl, 0x50C7C0);
    RH_ScopedInstall(TakeControlNoEntity, 0x50C8B0);
    RH_ScopedInstall(TakeControlAttachToEntity, 0x50C910);
    RH_ScopedInstall(TakeControlWithSpline, 0x50CAE0);
    RH_ScopedInstall(SetCamCollisionVarDataSet, 0x50CB60);
    RH_ScopedInstall(SetNearClipBasedOnPedCollision, 0x50CB90);
    RH_ScopedInstall(SetColVarsPed, 0x50CC50);
    RH_ScopedInstall(SetColVarsVehicle, 0x50CCA0);
    RH_ScopedInstall(StartTransitionWhenNotFinishedInter, 0x515BC0);
    RH_ScopedInstall(StartTransition, 0x515200);
    RH_ScopedInstall(CameraGenericModeSpecialCases, 0x50CD30);
    RH_ScopedInstall(CameraPedModeSpecialCases, 0x50CD80);
    RH_ScopedInstall(CameraPedAimModeSpecialCases, 0x50CDA0);
    RH_ScopedInstall(CameraVehicleModeSpecialCases, 0x50CDE0);
    RH_ScopedInstall(IsExtraEntityToIgnore, 0x50CE80);
    RH_ScopedInstall(ConsiderPedAsDucking, 0x50CEB0);
    RH_ScopedInstall(ResetDuckingSystem, 0x50CEF0);
    RH_ScopedInstall(HandleCameraMotionForDucking, 0x50CFA0);
    RH_ScopedInstall(HandleCameraMotionForDuckingDuringAim, 0x50D090);
    RH_ScopedInstall(VectorMoveLinear, 0x50D160);
    RH_ScopedInstall(VectorTrackLinear, 0x50D1D0);
    RH_ScopedInstall(AddShakeSimple, 0x50D240);
    RH_ScopedInstall(InitialiseScriptableComponents, 0x50D2D0);
    RH_ScopedInstall(DrawBordersForWideScreen, 0x514860);
    RH_ScopedInstall(Find3rdPersonCamTargetVector, 0x514970);
    RH_ScopedInstall(CalculateGroundHeight, 0x514B80);
    RH_ScopedInstall(CalculateFrustumPlanes, 0x514D60);
    RH_ScopedInstall(CalculateDerivedValues, 0x5150E0);
    RH_ScopedInstall(ImproveNearClip, 0x516B20);
    RH_ScopedInstall(SetCameraUpForMirror, 0x51A560);
    RH_ScopedInstall(RestoreCameraAfterMirror, 0x51A5A0);
    RH_ScopedInstall(ConeCastCollisionResolve, 0x51A5D0);
    RH_ScopedInstall(TryToStartNewCamMode, 0x51E560);
    RH_ScopedInstall(CameraColDetAndReact, 0x520190);
    RH_ScopedInstall(CamControl, 0x527FA0);
    RH_ScopedInstall(Process, 0x52B730);
    RH_ScopedInstall(DeleteCutSceneCamDataMemory, 0x5B24A0);
    RH_ScopedInstall(LoadPathSplines, 0x5B24D0);
    RH_ScopedInstall(Init, 0x5BC520);

    RH_ScopedOverloadedInstall(ProcessVectorTrackLinear, "0", 0x50D350, void(CCamera::*)(float));
    RH_ScopedOverloadedInstall(ProcessVectorTrackLinear, "1", 0x516440, void(CCamera::*)());
    RH_ScopedOverloadedInstall(ProcessVectorMoveLinear, "0", 0x50D430, void(CCamera::*)(float));
    RH_ScopedOverloadedInstall(ProcessVectorMoveLinear, "1", 0x5164A0, void(CCamera::*)());
    RH_ScopedOverloadedInstall(ProcessFOVLerp, "0", 0x50D510, void(CCamera::*)(float));
    RH_ScopedOverloadedInstall(ProcessFOVLerp, "1", 0x516500, void(CCamera::*)());
    //RH_ScopedOverloadedInstall(ProcessJiggle, "0", 0x516560, { .Reversed = false });

    RH_ScopedGlobalInstall(CamShakeNoPos, 0x50A970);
}

CCamera* CCamera::Constructor() { this->CCamera::CCamera(); return this; }

// 0x51A450
CCamera::CCamera() : CPlaceable() {
    m_nShakeType = 1;
    m_bMusicFadedOut = false;
    m_matrix = reinterpret_cast<CMatrixLink*>(&m_mCameraMatrix);
    m_fDuckCamMotionFactor = 0.0f;
    m_fDuckAimCamMotionFactor = 0.0f;

    InitialiseScriptableComponents();
}

// 0x50A870
CCamera::~CCamera() {
    m_matrix = nullptr;
}

// 0x5BC520
void CCamera::Init() {
    InitialiseScriptableComponents();
    
    for (auto& camera : m_aCams) {
        camera.Init();
    }

    {
        auto& cam = m_aCams[0];
        cam.m_nMode = MODE_FOLLOWPED;
        cam.m_fTargetCloseInDist = 2.0837801f - 1.85f;
        cam.m_fMinRealGroundDist = 1.85f;
        cam.m_fTargetZoomGroundOne = -0.55f;
        cam.m_fTargetZoomGroundTwo = 1.5f;
        cam.m_fTargetZoomGroundThree = 3.6f;
        cam.m_fTargetZoomOneZExtra = 0.06f;
        cam.m_fTargetZoomTwoZExtra = -0.1f;
        cam.m_fTargetZoomTwoInteriorZExtra = 0.0f;
        cam.m_fTargetZoomThreeZExtra = -0.07f;
        cam.m_fTargetZoomZCloseIn = 0.90040702f;
        cam.m_pCamTargetEntity = nullptr;
        cam.m_fCamBufferedHeight = 0.0f;
        cam.m_fCamBufferedHeightSpeed = 0.0f;
        cam.m_bCamLookingAtVector = false;
        cam.m_fPlayerVelocity = 0.0f;
    }

    {
        auto& cam = m_aCams[1];
        cam.m_nMode = MODE_FOLLOWPED;
        cam.m_pCamTargetEntity = nullptr;
        cam.m_fCamBufferedHeight = 0.0f;
        cam.m_fCamBufferedHeightSpeed = 0.0f;
        cam.m_bCamLookingAtVector = false;
        cam.m_fPlayerVelocity = 0.0f;
    }

    {
        auto& cam = m_aCams[2];
        cam.m_pCamTargetEntity = nullptr;
        cam.m_bCamLookingAtVector = false;
        cam.m_fPlayerVelocity = 0.0f;
    }

    ClearPlayerWeaponMode();

    m_pTargetEntity = FindPlayerEntity();
    CEntity::SafeRegisterRef(m_pTargetEntity);

    if (!FrontEndMenuManager.m_bStartGameLoading) {
        CDraw::FadeValue = 0;
        m_fMouseAccelVertical = notsa::IsFixBugs() ? m_fMouseAccelHorzntl * 0.6f : 0.0015f;
    }
    
    SetMotionBlur(255, 255, 255, 0, eMotionBlurType::NONE);

    m_f3rdPersonCHairMultX = 0.53f;
    m_f3rdPersonCHairMultY = 0.4f;
    gPlayerPedVisible = 1;
    m_bResetOldMatrix = true;
}

// 0x50A3B0
void CCamera::InitCameraVehicleTweaks() {
    m_fCurrentTweakDistance   = 1.0f;
    m_fCurrentTweakAltitude   = 1.0f;
    m_fCurrentTweakAngle      = 0.0f;
    m_nCurrentTweakModelIndex = -1;

    if (!m_bCameraVehicleTweaksInitialized) {
        for (auto& camTweak : m_aCamTweak) {
            camTweak.ModelID = -1;
            camTweak.Dist   = 1.0f;
            camTweak.Alt   = 1.0f;
            camTweak.Angle      = 0.0f;
        }

        m_aCamTweak[0].ModelID = MODEL_RCGOBLIN;
        m_aCamTweak[0].Dist = 1.0f;
        m_aCamTweak[0].Alt = 1.0f;
        m_aCamTweak[0].Angle    = 0.178997f; // todo: magic number

        m_bCameraVehicleTweaksInitialized = true;
    }
}

// 0x50D2D0
void CCamera::InitialiseScriptableComponents() {
    m_fTrackLinearStartTime    = -1.0f;
    m_fTrackLinearEndTime      = -1.0f;
    m_fStartShakeTime          = -1.0f;
    m_fEndShakeTime            = -1.0f;
    m_fEndZoomTime             = -1.0f;
    m_fStartZoomTime           = -1.0f;
    m_fZoomInFactor            = +0.0f;
    m_fZoomOutFactor           = +0.0f;
    m_bTrackLinearWithEase     = true;
    m_nZoomMode                = 1;
    m_bMoveLinearWithEase      = true;
    m_fMoveLinearStartTime     = -1.0f;
    m_fMoveLinearEndTime       = -1.0f;
    m_bBlockZoom               = false;
    m_bCameraPersistPosition   = false;
    m_bCameraPersistTrack      = false;
    m_bVecTrackLinearProcessed = false;
    m_bVecMoveLinearProcessed  = false;
    m_bFOVLerpProcessed        = false;
}

// 0x50AF90
void CCamera::InitialiseCameraForDebugMode() {
#ifndef FINAL
    if (auto* vehicle = FindPlayerVehicle()) {
        m_aCams[2].m_vecSource = vehicle->GetPosition();
    } else if (auto* player = FindPlayerPed()) {
        m_aCams[2].m_vecSource = player->GetPosition();
    }

    m_aCams[2].m_fTrueAlpha = 0.0f;
    m_aCams[2].m_fTrueBeta  = 0.0f;
    m_aCams[2].m_nMode = eCamMode::MODE_DEBUG;
#endif
}

// 0x50A480
void CCamera::ApplyVehicleCameraTweaks(CVehicle* vehicle) {
    if (vehicle->GetModelIndex() == m_nCurrentTweakModelIndex) {
        return;
    }

    InitCameraVehicleTweaks();
    for (auto& camTweak : m_aCamTweak) {
        if (camTweak.ModelID == vehicle->GetModelIndex()) {
            m_fCurrentTweakDistance = camTweak.Dist;
            m_fCurrentTweakAltitude = camTweak.Alt;
            m_fCurrentTweakAngle    = camTweak.Angle;
            return;
        }
    }
}

// 0x50A9F0
void CCamera::CamShake(float strength, CVector from) {
    auto dist = DistanceBetweenPoints(from, GetActiveCamera().m_vecSource);
    dist = std::clamp(dist, 0.0f, 100.0f);

    float percentShakeForce = 1.0f - dist / 100.f;
    float shakeForce = (m_fCamShakeForce - float(CTimer::GetTimeInMS() - m_nCamShakeStart) / 1000.f) * percentShakeForce;

    float toShakeForce = percentShakeForce * strength * 0.35f;
    if (toShakeForce > std::clamp(shakeForce, 0.0f, 2.0f)) {
        m_fCamShakeForce = toShakeForce;
        m_nCamShakeStart = CTimer::GetTimeInMS();
    }
}

// 0x50A970
void CamShakeNoPos(CCamera* camera, float strength) {
    float oldShake = camera->m_fCamShakeForce - float(CTimer::GetTimeInMS() - camera->m_nCamShakeStart) / 1000.f;

    if (strength > std::clamp(oldShake, 0.0f, 2.0f)) {
        camera->m_fCamShakeForce = strength;
        camera->m_nCamShakeStart = CTimer::GetTimeInMS();
    }
}

// 0x50AB10
void CCamera::ClearPlayerWeaponMode() {
    m_PlayerWeaponMode.m_nMode = 0;
    m_PlayerWeaponMode.m_nMaxZoom = 1;
    m_PlayerWeaponMode.m_nMinZoom = -1;
    m_PlayerWeaponMode.m_fDuration = 0.0f;
}

// 0x50AB40
void CCamera::DontProcessObbeCinemaCamera() {
    bDidWeProcessAnyCinemaCam = false;
}

// 0x50AC00
void CCamera::Enable1rstPersonCamCntrlsScript() {
    m_bEnable1rstPersonCamCntrlsScript = true;
}

// 0x50AC10
void CCamera::Enable1rstPersonWeaponsCamera() {
    m_bAllow1rstPersonWeaponsCamera = true;
}

// 0x50AC20
void CCamera::Fade(float duration, eFadeFlag direction) {
    m_fFadeDuration = duration;
    m_bFading = true;
    m_nFadeInOutFlag = direction;
    m_nFadeStartTime = CTimer::GetTimeInMS();

    if (m_bIgnoreFadingStuffForMusic && direction != eFadeFlag::FADE_OUT) {
        return;
    }
    m_bMusicFading           = true;
    m_nMusicFadingDirection  = direction;

    m_fTimeToFadeMusic       = std::min(std::max(duration * 0.3f, 0.3f), duration); //Can't use std::clamp there, duration can be bigger or smaller than 0.3f
    m_nFadeTimeStartedMusic  = CTimer::GetTimeInMS();
    m_fTimeToWaitToFadeMusic = direction == eFadeFlag::FADE_IN
        ? duration - m_fTimeToFadeMusic
        : 0.f;
    if (direction == eFadeFlag::FADE_IN) {
        m_fTimeToFadeMusic = std::max(m_fTimeToFadeMusic - 0.1f, 0.f);
    }
}

// 0x50AD20
float CCamera::FindCamFOV() const {
    return m_aCams[m_nActiveCam].m_fFOV;
}

/*!
* @addr 0x50AD40
* @return Rotation in radians at which the gun should point at, relative to the camera's vertical angle
*/
float CCamera::Find3rdPersonQuickAimPitch() const {
    const auto& cam = m_aCams[m_nActiveCam];

    // https://mathworld.wolfram.com/images/eps-svg/SOHCAHTOA_500.svg
    const auto adjacent = (0.5f - m_f3rdPersonCHairMultY) * 2.f;
    const auto opposite = std::tan(DegreesToRadians(cam.m_fFOV / 2.0f)) * adjacent;
    const auto relAngle = cam.m_fVerticalAngle + std::atan(opposite / CDraw::ms_fAspectRatio);
    return -relAngle; // Flip it
}

// 0x50AD90
uint32 CCamera::GetCutSceneFinishTime() {
    auto& cam = m_aCams[m_nActiveCam];
    if (cam.m_nMode == eCamMode::MODE_FLYBY) {
        return cam.m_nFinishTime;
    }

    cam = m_aCams[(m_nActiveCam + 1) % 2];
    if (cam.m_nMode == eCamMode::MODE_FLYBY) {
        return cam.m_nFinishTime;
    }

    return 0;
}

// 0x50ADE0
bool CCamera::GetFading() const {
    return m_bFading;
}

// TODO: eFadingDirection
// 0x50ADF0
int32 CCamera::GetFadingDirection() const {
    if (m_bFading)
        return m_nFadeInOutFlag == eFadeFlag::FADE_OUT;
    else
        return 2;
}

// 0x50AE10
bool CCamera::Get_Just_Switched_Status() const {
    return m_bJust_Switched;
}

// 0x50AE20
eNameState CCamera::GetScreenFadeStatus() const {
    if (m_fFadeAlpha == 0.0f) {
        return NAME_DONT_SHOW;
    }
    if (m_fFadeAlpha == 255.0f) {
        return NAME_FADE_IN;
    }

    return NAME_SHOW;
}

// 0x50AE50
CVector* CCamera::GetGameCamPosition() {
    return &m_vecGameCamPos;
}

// 0x50AE60
bool CCamera::GetLookingLRBFirstPerson() const {
    return m_aCams[m_nActiveCam].m_nMode == eCamMode::MODE_1STPERSON
        && m_aCams[m_nActiveCam].m_nDirectionWasLooking != LOOKING_DIRECTION_FORWARD;
}

// 0x50AED0
bool CCamera::GetLookingForwardFirstPerson() const {
    return m_aCams[m_nActiveCam].m_nMode == eCamMode::MODE_1STPERSON
        && m_aCams[m_nActiveCam].m_nDirectionWasLooking == LOOKING_DIRECTION_FORWARD;
}

// 0x50AE90
int32 CCamera::GetLookDirection() const {
    const auto& cam = m_aCams[m_nActiveCam];
    if (cam.m_nMode != eCamMode::MODE_CAM_ON_A_STRING &&
        cam.m_nMode != eCamMode::MODE_1STPERSON &&
        cam.m_nMode != eCamMode::MODE_BEHINDBOAT &&
        cam.m_nMode != eCamMode::MODE_FOLLOWPED ||
        (cam.m_nDirectionWasLooking == LOOKING_DIRECTION_FORWARD)
    ) {
        return LOOKING_DIRECTION_FORWARD;
    }

    return cam.m_nDirectionWasLooking; // todo: unsigned/signed
}

// 0x50AF00
bool CCamera::GetArrPosForVehicleType(eVehicleType type, int32& arrPos) {
    switch (type) {
    case VEHICLE_TYPE_MTRUCK:
        arrPos = 0;
        return true;
    case VEHICLE_TYPE_QUAD:
        arrPos = 1;
        return true;
    case VEHICLE_TYPE_HELI:
        arrPos = 2;
        return true;
    case VEHICLE_TYPE_PLANE:
        arrPos = 4;
        return true;
    case VEHICLE_TYPE_BOAT:
        arrPos = 3;
        return true;
    default:
        return false;
    }
}

// 0x50AF80
float CCamera::GetPositionAlongSpline() const {
    return m_fPositionAlongSpline;
}

// 0x516B00
float CCamera::GetRoughDistanceToGround() {
    return m_aCams[m_nActiveCam].m_vecSource.z - CalculateGroundHeight(eGroundHeightType::ENTITY_BB_BOTTOM);
}

// 0x50AFA0
void CCamera::CopyCameraMatrixToRWCam(bool bUpdateMatrix) {
    auto* const frame = RwCameraGetFrame(m_pRwCamera);
    auto* const rwMat = RwFrameGetMatrix(frame);

    // NOTE: the argument is actually "skip syncing the old matrix with the RW one"
    if (!bUpdateMatrix) {
        m_mCameraMatrixOld.UpdateMatrix(rwMat);
    }

    rwMat->pos   = m_mCameraMatrix.GetPosition();
    rwMat->at    = m_mCameraMatrix.GetForward(); // NOTE: RW's `at` is the game's `forward`..
    rwMat->up    = m_mCameraMatrix.GetUp();      // ..and RW's `up` is the game's `up`
    rwMat->right = m_mCameraMatrix.GetRight();

    // Jitter suppression: if the new vector is almost the same as the previous one - keep the previous one
    // MSVC static-init guards of the 4 `prev` vectors
    const auto InitPrev = [](uint32 bit, CVector& prev) {
        if (!(gPrevRwCamInitMask & bit)) {
            gPrevRwCamInitMask |= bit;
            prev = CVector{ -99999.f, -99999.f, -99999.f };
        }
    };
    InitPrev(1, gPrevRwCamPos);
    InitPrev(2, gPrevRwCamAt);
    InitPrev(4, gPrevRwCamUp);
    InitPrev(8, gPrevRwCamRight);

    constexpr float EPS = 0.00001f;
    const auto Snap = [](RwV3d& cur, const CVector& prev) {
        const float dx = prev.x - cur.x;
        const float dy = prev.y - cur.y;
        const float dz = prev.z - cur.z;
        if ((dz * dz + dy * dy) + dx * dx < EPS * EPS) { // NOTE: addition order as in the original
            cur = prev;
        }
    };
    Snap(rwMat->pos, gPrevRwCamPos);
    Snap(rwMat->at, gPrevRwCamAt);
    Snap(rwMat->up, gPrevRwCamUp);
    Snap(rwMat->right, gPrevRwCamRight);

    gPrevRwCamPos   = rwMat->pos;
    gPrevRwCamAt    = rwMat->at;
    gPrevRwCamUp    = rwMat->up;
    gPrevRwCamRight = rwMat->right;

    RwMatrixUpdate(rwMat);
    RwFrameUpdateObjects(frame);
    RwFrameOrthoNormalize(frame);

    if (m_bResetOldMatrix && !bUpdateMatrix) {
        m_mCameraMatrixOld.UpdateMatrix(rwMat);
        m_bResetOldMatrix = false;
    }
}

// 0x50B380
void CCamera::CalculateMirroredMatrix(CVector posn, float mirrorV, CMatrix *camMatrix, CMatrix* mirrorMatrix) {
    mirrorMatrix->GetPosition() = camMatrix->GetPosition() - posn * 2 * (DotProduct(posn, camMatrix->GetPosition()) - mirrorV);

    const CVector fwd = camMatrix->GetForward() - posn * 2 * DotProduct(posn, camMatrix->GetForward());
    mirrorMatrix->GetForward() = fwd;

    const CVector up = camMatrix->GetUp() - posn * 2 * DotProduct(posn, camMatrix->GetUp());
    mirrorMatrix->GetUp() = up;

    mirrorMatrix->GetRight() = CVector{
        up.y * fwd.z - up.z * fwd.y,
        up.z * fwd.x - up.x * fwd.z,
        up.x * fwd.y - up.y * fwd.x
    };
}

// 0x50B510
void CCamera::DealWithMirrorBeforeConstructRenderList(bool bActiveMirror, CVector mirrorNormal, float mirrorV, CMatrix* matMirror) {
    m_bMirrorActive = bActiveMirror;

    if (!bActiveMirror)
        return;

    if (matMirror)
        m_mMatMirror = *matMirror;
    else
        CalculateMirroredMatrix(mirrorNormal, mirrorV, &m_mCameraMatrix, &m_mMatMirror);

    m_mMatMirrorInverse = Invert(m_mMatMirror);
}

/// III/VC leftover
// 0x50B8F0
void CCamera::RenderMotionBlur() const {
    ZoneScoped;

    if (m_nBlurType != eMotionBlurType::NONE) {
        // CMBlur::MotionBlurRender(); // todo: Add CMBlur::MotionBlurRender is NOP, 0x71D700
    }
}

// 0x50B930
void CCamera::Restore() {
    m_bLookingAtPlayer = true;
    m_bLookingAtVector = false;
    m_nTypeOfSwitch = eSwitchType::INTERPOLATION;
    m_bUseNearClipScript = false;
    m_nModeObbeCamIsInForCar = 30;
    m_fPositionAlongSpline = 0.0f;
    m_bStartingSpline = false;
    m_bScriptParametersSetForInterp = false;
    m_nWhoIsInControlOfTheCamera = 0;

    CVehicle* vehicle = FindPlayerVehicle();
    CPlayerPed* player = FindPlayerPed();

    if (vehicle) {
        m_nModeToGoTo = MODE_CAM_ON_A_STRING;
        CEntity::SafeCleanUpRef(m_pTargetEntity);
        m_pTargetEntity = vehicle;
    } else {
        m_nModeToGoTo = MODE_FOLLOWPED;
        CEntity::SafeCleanUpRef(m_pTargetEntity);
        m_pTargetEntity = player;
    }
    CEntity::SafeRegisterRef(m_pTargetEntity);

    switch (player->m_nPedState) {
    case PEDSTATE_ENTER_CAR:
    case PEDSTATE_CARJACK:
    case PEDSTATE_OPEN_DOOR:
        m_nModeToGoTo = MODE_CAM_ON_A_STRING;
        break;
    }

    if (player->m_nPedState == PEDSTATE_EXIT_CAR) {
        m_nModeToGoTo = MODE_FOLLOWPED;

        CEntity::SafeCleanUpRef(m_pTargetEntity);
        m_pTargetEntity = player;
        CEntity::SafeRegisterRef(m_pTargetEntity);
    }

    CEntity::ClearReference(m_pAttachedEntity);

    m_bEnable1rstPersonCamCntrlsScript = false;
    m_bAllow1rstPersonWeaponsCamera = false;
    m_bUseScriptZoomValuePed = false;
    m_bUseScriptZoomValueCar = false;
    m_fAvoidTheGeometryProbsTimer = 0.0f;
    m_bStartInterScript = true;
    m_bCameraJustRestored = true;
}

// 0x50BAB0
void CCamera::RestoreWithJumpCut() {
    m_bRestoreByJumpCut = true;
    m_bLookingAtPlayer = true;
    m_bLookingAtVector = false;
    m_nTypeOfSwitch = eSwitchType::JUMPCUT;
    m_nWhoIsInControlOfTheCamera = 0;
    m_fPositionAlongSpline = 0.0f;
    m_bStartingSpline = false;
    m_bUseNearClipScript = false;
    m_nModeObbeCamIsInForCar = 30;
    m_bScriptParametersSetForInterp = false;

    CVehicle* vehicle = FindPlayerVehicle();
    CPlayerPed* player = FindPlayerPed();

    if (vehicle) {
        m_nModeToGoTo = MODE_CAM_ON_A_STRING;
        CEntity::SafeCleanUpRef(m_pTargetEntity);
        m_pTargetEntity = vehicle;
    } else {
        m_nModeToGoTo = MODE_FOLLOWPED;
        CEntity::SafeCleanUpRef(m_pTargetEntity);
        m_pTargetEntity = player;
    }
    CEntity::SafeRegisterRef(m_pTargetEntity);

    switch (player->m_nPedState) {
    case PEDSTATE_ENTER_CAR:
    case PEDSTATE_CARJACK:
    case PEDSTATE_OPEN_DOOR:
        m_nModeToGoTo = MODE_CAM_ON_A_STRING;
        break;
    }

    if (player->m_nPedState == PEDSTATE_EXIT_CAR) {
        m_nModeToGoTo = MODE_FOLLOWPED;

        CEntity::SafeCleanUpRef(m_pTargetEntity);
        m_pTargetEntity = player;
        CEntity::SafeRegisterRef(m_pTargetEntity);
    }

    if (!m_bCooperativeCamMode) {
        m_bUseScriptZoomValuePed = false;
        m_bUseScriptZoomValueCar = false;
        return;
    }

    CPlayerPed* player0 = FindPlayerPed(0);
    CPlayerPed* player1 = FindPlayerPed(1);

    if (!player0) {
        m_bUseScriptZoomValuePed = false;
        m_bUseScriptZoomValueCar = false;
        return;
    }

    if (!player1) {
        m_bUseScriptZoomValuePed = false;
        m_bUseScriptZoomValueCar = false;
        return;
    }

    CEntity::SafeCleanUpRef(m_pTargetEntity);

    if (!player0->IsInVehicle() || !player1->IsInVehicle()) {
        m_nModeToGoTo = m_nModeForTwoPlayersNotBothInCar;
        m_pTargetEntity = player0;
        CEntity::SafeRegisterRef(m_pTargetEntity);

        m_bUseScriptZoomValuePed = false;
        m_bUseScriptZoomValueCar = false;
        return;
    }

    if (player0->m_pVehicle == player1->m_pVehicle) {
        if (m_bAllowShootingWith2PlayersInCar) {
            m_nModeToGoTo = m_nModeForTwoPlayersSameCarShootingAllowed;
        } else {
            m_nModeToGoTo = m_nModeForTwoPlayersSameCarShootingNotAllowed;
        }
    } else {
        m_nModeToGoTo = m_nModeForTwoPlayersSeparateCars;
    }

    m_pTargetEntity = player0->m_pVehicle;
    CEntity::SafeRegisterRef(m_pTargetEntity);

    m_bUseScriptZoomValuePed = false;
    m_bUseScriptZoomValueCar = false;
}

// 0x50BD20
void CCamera::SetCamCutSceneOffSet(const CVector& offset) {
    m_vecCutSceneOffset = offset;
}

// 0x50BD40
void CCamera::SetCameraDirectlyBehindForFollowPed_CamOnAString() {
    m_bCamDirectlyBehind = true;
    CPed* player = FindPlayerPed();
    if (player) {
        m_fPedOrientForBehindOrInFront = CGeneral::GetATanOfXY(player->GetForward().x, player->GetForward().y);
    }
}

// 0x50BD70
void CCamera::SetCameraDirectlyInFrontForFollowPed_CamOnAString() {
    m_bCamDirectlyInFront = true;
    CPed* player = FindPlayerPed();
    if (player != nullptr) {
        m_fPedOrientForBehindOrInFront = CGeneral::GetATanOfXY(player->GetForward().x, player->GetForward().y);
    }
}

// unused
// 0x50BDA0
void CCamera::SetCameraDirectlyBehindForFollowPed_ForAPed_CamOnAString(CPed* targetPed) {
    if (!targetPed) {
        return;
    }

    m_bCamDirectlyBehind = true;
    m_bLookingAtPlayer = false;

    TheCamera.m_pTargetEntity = targetPed;
    CEntity::ChangeEntityReference(GetActiveCamera().m_pCamTargetEntity, targetPed);
    m_fPedOrientForBehindOrInFront = targetPed->GetHeading();
}

// 0x50BE30
void CCamera::SetCameraDirectlyInFrontForFollowPed_ForAPed_CamOnAString(CPed* targetPed) {
    if (!targetPed) {
        return;
    }

    m_bLookingAtPlayer = false;
    m_pTargetEntity = targetPed;

    CCam& camera = GetActiveCamera();
    CEntity::SafeCleanUpRef(camera.m_pCamTargetEntity);

    camera.m_pCamTargetEntity = targetPed;
    camera.m_pCamTargetEntity->RegisterReference(camera.m_pCamTargetEntity);

    m_bCamDirectlyInFront = true;
    m_fPedOrientForBehindOrInFront = CGeneral::GetATanOfXY(targetPed->GetForward().x, targetPed->GetForward().y);
}

// 0x50BEC0
void CCamera::SetCamPositionForFixedMode(const CVector& fixedModeSource, const CVector& fixedModeUpOffset) {
    m_vecFixedModeSource = fixedModeSource;
    m_vecFixedModeUpOffSet = fixedModeUpOffset;
    m_bGarageFixedCamPositionSet = false;
}

// 0x50BF00
void CCamera::SetFadeColour(uint8 red, uint8 green, uint8 blue) {
    m_bFadeTargetIsSplashScreen = false;
    if (red == 2 && green == 2 && blue == 2) {
        m_bFadeTargetIsSplashScreen = true;
    }

    CDraw::FadeRed   = red;
    CDraw::FadeGreen = green;
    CDraw::FadeBlue  = blue;
}

// 0x50BF40
void CCamera::SetMotionBlur(uint8 red, uint8 green, uint8 blue, int32 value, eMotionBlurType blurType) {
    m_nBlurRed    = red;
    m_nBlurGreen  = green;
    m_nBlurBlue   = blue;
    m_nBlurType   = blurType;
    m_nMotionBlur = value;
}

// 0x50BF80
void CCamera::SetMotionBlurAlpha(int32 alpha) {
    m_nMotionBlurAddAlpha = alpha;
}

// 0x50BF90
void CCamera::SetNearClipScript(float nearClip) {
    m_fNearClipScript = nearClip;
    m_bUseNearClipScript = true;
}

// 0x50BFB0
void CCamera::SetNewPlayerWeaponMode(eCamMode mode, int16 minZoom, int16 maxZoom) {
    m_PlayerWeaponMode.m_nMode     = mode;
    m_PlayerWeaponMode.m_nMinZoom  = minZoom;
    m_PlayerWeaponMode.m_nMaxZoom  = maxZoom;
    m_PlayerWeaponMode.m_fDuration = 0.0f;
}

// 0x50BFF0
bool CCamera::Using1stPersonWeaponMode() const {
    switch (m_PlayerWeaponMode.m_nMode) {
    case MODE_SNIPER:
    case MODE_M16_1STPERSON:
    case MODE_ROCKETLAUNCHER:
    case MODE_ROCKETLAUNCHER_HS:
    case MODE_HELICANNON_1STPERSON:
    case MODE_CAMERA:
    case MODE_AIMWEAPON_ATTACHED:
        return true;
    default:
        return false;
    }
}

// 0x50C030
void CCamera::SetParametersForScriptInterpolation(float interpolationToStopMoving, float interpolationToCatchUp, uint32 timeForInterpolation) {
    m_nScriptTimeForInterpolation = timeForInterpolation;
    m_bScriptParametersSetForInterp = true;
    m_fScriptPercentageInterToStopMoving = interpolationToStopMoving / 100.0f;
    m_fScriptPercentageInterToCatchUp = interpolationToCatchUp / 100.0f;
}

// 0x50C070
void CCamera::SetPercentAlongCutScene(float percent) {
    auto& cam = m_aCams[m_nActiveCam];
    if (cam.m_nMode == eCamMode::MODE_FLYBY) {
        cam.m_fTimeElapsedFloat = (float)cam.m_nFinishTime * percent / 100.0f;
        return;
    }

    cam = m_aCams[(m_nActiveCam + 1) % 2];
    if (cam.m_nMode == eCamMode::MODE_FLYBY) {
        cam.m_fTimeElapsedFloat = (float)cam.m_nFinishTime * percent / 100.0f;
        return;
    }
}

// 0x50C100
void CCamera::SetRwCamera(RwCamera* camera) {
    m_pRwCamera = camera;
    m_mViewMatrix.Attach(&camera->viewMatrix, false);
}

// 0x50C140
void CCamera::SetWideScreenOn() {
    m_bWideScreenOn = true;
    m_bWantsToSwitchWidescreenOff = false;
}

// 0x50C150
void CCamera::SetWideScreenOff() {
    m_bWantsToSwitchWidescreenOff = m_bWideScreenOn;
}

// 0x50C160
void CCamera::SetZoomValueFollowPedScript(int16 zoomMode) {
    switch (zoomMode) {
    case 1:
        m_fPedZoomValueScript = 1.50f;
        break;
    case 2:
        m_fPedZoomValueScript = 2.90f;
        break;
    default:
        m_fPedZoomValueScript = 0.25f;
    }
    m_bUseScriptZoomValuePed = true;
}

// zoomMode : 0- ZOOM_ONE , 1- ZOOM_TWO , 2- ZOOM_THREE
// 0x50C1B0
void CCamera::SetZoomValueCamStringScript(int16 zoomMode) {
    auto entity = m_aCams[0].m_pCamTargetEntity;

    if (entity->GetStatus() == STATUS_SIMPLE) {
        int32 arrPos{};
        VERIFY(GetArrPosForVehicleType(static_cast<eVehicleType>(entity->AsVehicle()->GetVehicleAppearance()), arrPos));
        m_fCarZoomValueScript = [zoomMode]{
            switch (zoomMode) {
            case 0:
                return std::array{ -1.0f, -0.2f, -3.20f, 0.05f, -2.41f }; // 0x8CC3E0
            case 1:
                return std::array{ +1.0f, +1.4f, +0.65f, 1.90f, +6.49f }; // 0x8CC3F4
            case 2:
                return std::array{ +6.0f, +6.0f, +15.9f, 15.9f, +15.0f }; // 0x8CC408
            default:
                NOTSA_UNREACHABLE("Unexpected zoom mode: {}", zoomMode);
            }
        }()[arrPos];
    
        m_bUseScriptZoomValueCar = true;
    } else {
        SetZoomValueFollowPedScript(zoomMode);
    }
}

// 0x50C260
void CCamera::StartCooperativeCamMode() {
    m_bCooperativeCamMode = true;
    CGameLogic::n2PlayerPedInFocus = eFocusedPlayer::NONE;
}

// 0x50C270
void CCamera::StopCooperativeCamMode() {
    m_bCooperativeCamMode = false;
    CGameLogic::n2PlayerPedInFocus = eFocusedPlayer::NONE;
}

// 0x50C280
void CCamera::AllowShootingWith2PlayersInCar(bool bAllow) {
    m_bAllowShootingWith2PlayersInCar = bAllow;
}

// 0x50C290
void CCamera::StoreValuesDuringInterPol(CVector* sourceDuringInter, CVector* targetDuringInter, CVector* upDuringInter, float* FOVDuringInter) {
    m_vecSourceDuringInter = *sourceDuringInter;
    m_vecTargetDuringInter = *targetDuringInter;
    m_vecUpDuringInter     = *upDuringInter;
    m_fFOVDuringInter      = *FOVDuringInter;

    auto dist = *sourceDuringInter - m_vecTargetDuringInter;
    m_fBetaDuringInterPol = CGeneral::GetATanOfXY(dist.x, dist.y);

    float distOnGround = dist.Magnitude2D();
    m_fAlphaDuringInterPol = CGeneral::GetATanOfXY(distOnGround, dist.z);
}

// 0x50C360
void CCamera::UpdateTargetEntity() {
    m_bPlayerWasOnBike = m_pTargetEntity && m_pTargetEntity->GetIsTypeVehicle() && m_pTargetEntity->AsVehicle()->m_vecMoveSpeed.SquaredMagnitude() > 0.3f;

    const auto player = FindPlayerPed();
    assert(player);

    auto something{ true };
    if (m_nWhoIsInControlOfTheCamera == 2) {
        m_nModeObbeCamIsInForCar = m_nModeObbeCamIsInForCar;
        switch (m_nModeObbeCamIsInForCar) {
        case 8:
        case 7: {
            if (player->m_nPedState != PEDSTATE_ARRESTED) {
                something = false;
            }

            if (!FindPlayerVehicle()) {
                CEntity::ChangeEntityReference(m_pTargetEntity, player);
            }

            break;
        }
        }
    }

    if (!m_bLookingAtPlayer && !something || m_bTransitionState) {
        if (m_pTargetEntity) {
            if (!m_bTargetJustBeenOnTrain) {
                return;
            }
        }
        
    }

    bool playerDoingSomethingWhileDriveBy{};
    if ([&, this]() { // Check is player doing drive-by
        if (!FindPlayerVehicle()) {
            return true;
        }

        if (!CGameLogic::IsCoopGameGoingOn()) {
            if (player->GetTaskManager().GetSimplestActiveTaskAs<CTaskSimpleGangDriveBy>()) {
                return true;
            }
        }

        return false;
    }()) {
        CEntity::ChangeEntityReference(m_pTargetEntity, player);

        playerDoingSomethingWhileDriveBy = [this, player] {
            switch (player->m_nPedState) {
            case PEDSTATE_ENTER_CAR:
            case PEDSTATE_CARJACK:
            case PEDSTATE_OPEN_DOOR:
                return true;
            }
            return false;
        }();

        if (!playerDoingSomethingWhileDriveBy) {
            auto& cam = GetActiveCam();
            if (cam.m_pCamTargetEntity != m_pTargetEntity) {
                CEntity::ChangeEntityReference(cam.m_pCamTargetEntity, m_pTargetEntity);
            }
        }
    } else {
        CEntity::ChangeEntityReference(m_pTargetEntity, FindPlayerVehicle());
    }

    const auto canEnterCar = player && player->m_pVehicle && player->m_pVehicle->CanPedOpenLocks(player); // Inverted this variable

    if (canEnterCar && player->m_nPedState == PEDSTATE_ENTER_CAR && !playerDoingSomethingWhileDriveBy) {
        if (m_nCarZoom) {
            CEntity::ChangeEntityReference(m_pTargetEntity, FindPlayerEntity());
        }
    }

    if (canEnterCar) {
        switch (player->m_nPedState) {
        case PEDSTATE_CARJACK:
        case PEDSTATE_OPEN_DOOR: {
            if (!playerDoingSomethingWhileDriveBy) {
                if (m_nCarZoom) {
                    CEntity::ChangeEntityReference(m_pTargetEntity, FindPlayerEntity());
                }
            }

            if (!FindPlayerVehicle()) {
                CEntity::ChangeEntityReference(m_pTargetEntity, player);
            }
        }
        }
    }

    switch (player->m_nPedState) {
    case PEDSTATE_EXIT_CAR:
    case PEDSTATE_DRAGGED_FROM_CAR:
        CEntity::ChangeEntityReference(m_pTargetEntity, player);
    }

    if (m_pTargetEntity->GetIsTypeVehicle()) {
        if (m_nCarZoom == 0) {
            if (player->m_nPedState == PEDSTATE_ARRESTED) {
                CEntity::ChangeEntityReference(m_pTargetEntity, player);
            }
        }
    }
}

// 0x50C7C0
void CCamera::TakeControl(CEntity* target, eCamMode modeToGoTo, eSwitchType switchType, int32 whoIsInControlOfTheCamera) {
    if (!m_bCinemaCamera) {
        if (whoIsInControlOfTheCamera == 2 && m_nWhoIsInControlOfTheCamera == 1) {
            return;
        }
    }
    m_nWhoIsInControlOfTheCamera = whoIsInControlOfTheCamera;

    const auto [newGoToMode, newTargetEntity] = [&, this]() -> std::tuple<eCamMode, CEntity*>{
        if (target) {
            return {
                [&, this] {
                    if (modeToGoTo == MODE_NONE) {
                        switch (target->GetType()) {
                        case ENTITY_TYPE_PED:
                            return MODE_FOLLOWPED;
                        case ENTITY_TYPE_VEHICLE:
                            return MODE_CAM_ON_A_STRING;
                        }
                    }
                    return modeToGoTo;
                }(),
                target
            };
        }

        return { modeToGoTo, FindPlayerEntity() };
    }();

    CEntity::ChangeEntityReference(m_pTargetEntity, newTargetEntity);
    m_nModeToGoTo = newGoToMode;

    m_nTypeOfSwitch    = switchType;
    m_bLookingAtPlayer = m_bLookingAtVector = false;
    m_bStartInterScript = true;
}

// 0x50C8B0
void CCamera::TakeControlNoEntity(const CVector& fixedModeVector, eSwitchType switchType, int32 whoIsInControlOfTheCamera) {
    if (whoIsInControlOfTheCamera == 2 && m_nWhoIsInControlOfTheCamera == 1)
        return;

    m_nWhoIsInControlOfTheCamera = whoIsInControlOfTheCamera;
    m_bLookingAtVector           = true;
    m_nModeToGoTo                = MODE_FIXED;
    m_bLookingAtPlayer           = false;
    m_vecFixedModeVector         = fixedModeVector;
    m_nTypeOfSwitch              = switchType;
    m_bStartInterScript          = true;
}

// 0x50C910
void CCamera::TakeControlAttachToEntity(CEntity* target, CEntity* attached, CVector* attachedCamOffset, CVector* attachedCamLookAt, float tilt, eSwitchType switchType, int32 whoIsInControlOfTheCamera) {
    plugin::CallMethod<0x50C910, CCamera*, CEntity*, CEntity*, CVector*, CVector*, float, eSwitchType, int32>(this, target, attached, attachedCamOffset, attachedCamLookAt, tilt, switchType, whoIsInControlOfTheCamera);
}

// 0x50CAE0
void CCamera::TakeControlWithSpline(eSwitchType switchType) {
    m_bLookingAtPlayer = false;
    m_bLookingAtVector = false;
    m_bCutsceneFinished = false;
    m_nModeToGoTo = MODE_FLYBY;
    m_nTypeOfSwitch = switchType;
    m_bStartInterScript = true;
}

// 0x50CB10
void CCamera::UpdateAimingCoors(const CVector& aimingTargetCoors) {
    m_vecAimingTargetCoors = aimingTargetCoors;
}

// 0x515BD0
void CCamera::UpdateSoundDistances() {
    plugin::CallMethod<0x515BD0, CCamera*>(this);
}

// unused
// 0x50CB90
void CCamera::SetNearClipBasedOnPedCollision(float arg2) {
    const float v0 = gpCamColVars[4];
    float nearClip = (0.3f - v0) * ((std::sqrt(arg2) / gNearClipPedDistDivisor) * gNearClipPedScale) + v0; // NOTE: operation order as in the original
    if (nearClip < v0) {
        nearClip = v0;
    }
    RwCameraSetNearClipPlane(Scene.m_pRwCamera, nearClip);
}

// TODO: eAimingType
// 0x50CBF0
void CCamera::SetColVarsAimWeapon(int32 aimingType) {
    switch (aimingType) {
    case 0:
        CCamera::SetCamCollisionVarDataSet(0);
        break;
    case 1:
        CCamera::SetCamCollisionVarDataSet(1);
        break;
    case 2:
        CCamera::SetCamCollisionVarDataSet(2);
        break;
    case 3:
        CCamera::SetCamCollisionVarDataSet(3);
        break;
    default:
        return;
    }
}

// 0x50CC50
void CCamera::SetColVarsPed(ePedType pedType, int32 nCamPedZoom) {
    const int32 camColVars = [=] {
        switch (pedType) {
        case PED_TYPE_PLAYER1:
            return nCamPedZoom + 3;
        case PED_TYPE_PLAYER2:
            return nCamPedZoom + 6;
        default:
            return 0;
        }
    }();

    if (camColVars != gCurCamColVars) {
        gCurCamColVars = camColVars;
        gCurDistForCam = 1.0f;
        gpCamColVars = gCamColVars[camColVars];
    }
}

// 0x50CD30
void CCamera::CameraGenericModeSpecialCases(CPed* targetPed) {
    m_nExtraEntitiesCount = 0;

    if (!targetPed) {
        return;
    }

    auto* taskHold = static_cast<CTaskSimpleHoldEntity*>(targetPed->GetIntelligence()->GetTaskHold(false));
    if (!taskHold || !taskHold->m_pEntityToHold) {
        return;
    }

    m_pExtraEntity[m_nExtraEntitiesCount++] = targetPed;
}

// 0x50CD80
void CCamera::CameraPedModeSpecialCases() {
    CCollision::bCamCollideWithVehicles = true;
    CCollision::bCamCollideWithObjects  = true;
    CCollision::bCamCollideWithPeds     = true;
}

// 0x50CDA0
void CCamera::CameraPedAimModeSpecialCases(CPed* ped) {
    CameraPedModeSpecialCases();

    if (ped->IsInVehicle()) {
        m_pExtraEntity[m_nExtraEntitiesCount++] = ped->m_pVehicle;
    }
}

// 0x50CDE0
void CCamera::CameraVehicleModeSpecialCases(CVehicle* vehicle) {
    float speed = vehicle->m_vecMoveSpeed.Magnitude();

    const auto slow = speed <= 0.2f;
    CCollision::relVelCamCollisionVehiclesSqr = slow ? 0.1f : 1.0f;
    CCollision::bCamCollideWithVehicles = true;
    CCollision::bCamCollideWithPeds     = slow;
    CCollision::bCamCollideWithObjects  = slow;

    if (vehicle->m_pVehicleBeingTowed) {
        m_pExtraEntity[m_nExtraEntitiesCount++] = vehicle->m_pVehicleBeingTowed;
    }
}

// 0x50CE80
bool CCamera::IsExtraEntityToIgnore(CEntity* entity) {
    if (m_nExtraEntitiesCount <= 0) {
        return false;
    }
    return notsa::contains(m_pExtraEntity, entity);
}

// 0x420C40
bool CCamera::IsSphereVisible(const CVector& origin, float radius, RwMatrix* transformMatrix) {
    return plugin::CallMethodAndReturn<bool, 0x420C40, CCamera*, const CVector&, float, RwMatrix*>(this, origin, radius, transformMatrix);
}

// 0x420D40 - NOTE: Function has no hook
bool CCamera::IsSphereVisible(const CVector& origin, float radius) {
    return IsSphereVisible(origin, radius, (RwMatrix*)&m_mMatInverse)
        || (m_bMirrorActive && IsSphereVisible(origin, radius, (RwMatrix*)&m_mMatMirrorInverse));
}

// 0x50CEB0
bool CCamera::ConsiderPedAsDucking(CPed* ped) {
    auto task = ped->GetIntelligence()->GetTaskDuck(true);
    return task && ped->bIsDucking && !task->m_bIsAborting;
}

// 0x50CEF0
void CCamera::ResetDuckingSystem(CPed* ped) {
    m_fDuckCamMotionFactor    = 0.0f;
    m_fDuckAimCamMotionFactor = 0.0f;
    if (!ped)
        return;

    auto* task = ped->GetIntelligence()->GetTaskDuck(true);
    if (!task)
        return;

    if (!ped->bIsDucking || task->m_bIsAborting)
        return;

    float factor;
    if (ped->m_vecMoveSpeed.Magnitude() <= 0.000001f)
        factor = 0.3f - 1.0f;
    else
        factor = 0.3f - 0.5f;

    m_fDuckCamMotionFactor    = factor;
    m_fDuckAimCamMotionFactor = -0.35f;
}

// arg5 always used as false
// 0x50CFA0
void CCamera::HandleCameraMotionForDucking(CPed* ped, CVector* source, CVector* targPosn, bool arg5) {
    float target = 0.0f;
    if (const auto* const task = ped->GetIntelligence()->GetTaskDuck(true); task && ped->bIsDucking && !task->m_bIsAborting) {
        target = ped->m_vecMoveSpeed.SquaredMagnitude() <= 0.000001f
            ? 0.3f - 1.0f
            : 0.3f - 0.5f;
    }
    if (!arg5) {
        m_fDuckCamMotionFactor += CTimer::GetTimeStep() * 0.1f * (target - m_fDuckCamMotionFactor);
    }
    if (source) {
        source->z += m_fDuckCamMotionFactor;
    }
    if (targPosn) {
        targPosn->z += m_fDuckCamMotionFactor;
    }
}

// arg5 always used as false
// 0x50D090
void CCamera::HandleCameraMotionForDuckingDuringAim(CPed* ped, CVector* source, CVector* targPosn, bool arg5) {
    float target = 0.0f;
    if (const auto* const task = ped->GetIntelligence()->GetTaskDuck(true); task && ped->bIsDucking && !task->m_bIsAborting) {
        target = -0.35f;
    }
    if (!arg5) {
        m_fDuckAimCamMotionFactor += CTimer::GetTimeStep() * 0.13f * (target - m_fDuckAimCamMotionFactor);
    }
    if (source) {
        source->z += m_fDuckAimCamMotionFactor;
    }
    if (targPosn) {
        targPosn->z += m_fDuckAimCamMotionFactor;
    }
}

// 0x50D160
void CCamera::VectorMoveLinear(CVector* to, CVector* from, float duration, bool bMoveLinearWithEase) {
    // NOTE: the original reads the ms timer as signed int and corrects negative values by 2^32
    const float now = static_cast<float>(CTimer::GetTimeInMS());
    m_fMoveLinearStartTime = now;
    m_fMoveLinearEndTime   = now + duration;
    m_vecMoveLinearPosnStart = *from;
    m_vecMoveLinearPosnEnd   = *to;
    m_bMoveLinearWithEase    = bMoveLinearWithEase;
}

// 0x50D1D0
void CCamera::VectorTrackLinear(CVector* to, CVector* from, float duration, bool bEase) {
    const float now = static_cast<float>(CTimer::GetTimeInMS());
    m_fTrackLinearStartTime = now;
    m_fTrackLinearEndTime   = now + duration;
    m_vecTrackLinearStartPoint = *from;
    m_vecTrackLinearEndPoint   = *to;
    m_bTrackLinearWithEase  = bEase;
}

// 0x516400
void CCamera::AddShake(float duration, float a2, float a3, float a4, float a5) {
    return AddShakeSimple(duration, 1, 1.0f);
}

// 0x50D240
void CCamera::AddShakeSimple(float durationMs, int32 type, float intensity) {
    m_fShakeIntensity = intensity;
    m_nShakeType = type;
    m_fStartShakeTime = static_cast<float>(CTimer::GetTimeInMS());
    m_fEndShakeTime = m_fStartShakeTime + durationMs;
}

// 0x50D280
void CCamera::LerpFOV(float zoomInFactor, float zoomOutFactor, float timeLimit, bool bEase) {
    m_fStartZoomTime = static_cast<float>(CTimer::GetTimeInMS());
    m_fEndZoomTime = static_cast<float>(CTimer::GetTimeInMS()) + timeLimit;

    m_nZoomMode = bEase; // TODO: Rename
    m_fZoomInFactor = zoomInFactor;
    m_fZoomOutFactor = zoomOutFactor;
}

// 0x50B5D0
void CCamera::ProcessFade() {
    ZoneScoped;

    if (!m_bFading) {
        return;
    }

    float fadeAlpha = 0.0f;

    if (m_nFadeInOutFlag == eFadeFlag::FADE_OUT) {
        m_fFadeDuration == 0.0f
            ? (m_fFadeAlpha += 0.0f)
            : (m_fFadeAlpha -= CTimer::GetTimeStepInSeconds() / m_fFadeDuration * 255.0f);

        if (m_fFadeAlpha > 0.0f) {
            CDraw::FadeValue = static_cast<uint8>(m_fFadeAlpha);
            return;
        }

        m_bFading = false;
    } else {
        if (m_nFadeInOutFlag == eFadeFlag::FADE_OUT) { // stupid, why not use a switch instead?
            CDraw::FadeValue = static_cast<uint8>(m_fFadeAlpha);
            return;
        }

        if (m_fFadeAlpha >= 255.0f) {
            m_bFading = false;
        }

        fadeAlpha = 255.0f;

        m_fFadeDuration == 0.0f
            ? (m_fFadeAlpha += 255.0f)
            : (m_fFadeAlpha += CTimer::GetTimeStepInSeconds() / m_fFadeDuration * 255.0f);

        if (m_fFadeAlpha < 255.0f) {
            CDraw::FadeValue = static_cast<uint8>(m_fFadeAlpha);
            return;
        }
    }

    m_fFadeAlpha = fadeAlpha;
    CDraw::FadeValue = static_cast<uint8>(m_fFadeAlpha);
}

// 0x50B6D0
void CCamera::ProcessMusicFade() {
    if (!m_bMusicFading)
        return;

    if (m_fTimeToWaitToFadeMusic <= 0.0f) {
        switch (m_nMusicFadingDirection) {
        case eFadeFlag::FADE_OUT: {
            m_fEffectsFaderScalingFactor = m_fTimeToFadeMusic > 0.0f
                ? CTimer::GetTimeStepInSeconds() / m_fTimeToFadeMusic + m_fEffectsFaderScalingFactor
                : 1.f;
            
            if (m_fEffectsFaderScalingFactor >= 1.0f) {
                m_bMusicFadedOut = false;
                m_bMusicFading = false;
                m_fEffectsFaderScalingFactor = 1.0f;
            }
            break;
        }
        case eFadeFlag::FADE_IN: {
            if (m_fEffectsFaderScalingFactor <= 0.0f) {
                m_bMusicFadedOut = true;
                m_bMusicFading = false;
                m_fEffectsFaderScalingFactor = 0.0f;
            }
            m_fEffectsFaderScalingFactor = m_fTimeToFadeMusic > 0.0f
                ? std::max(0.f, m_fEffectsFaderScalingFactor - CTimer::GetTimeStepInSeconds() / m_fTimeToFadeMusic)
                : 0.f;
            break;
        }
        }
    } else {
        m_fTimeToWaitToFadeMusic = m_fTimeToWaitToFadeMusic - CTimer::GetTimeStepInSeconds();
    }

    if (!AudioEngine.IsLoadingTuneActive()) {
        AudioEngine.SetMusicFaderScalingFactor(m_fEffectsFaderScalingFactor);
        AudioEngine.SetEffectsFaderScalingFactor(m_fEffectsFaderScalingFactor);
    }
}

// unused, empty
// 0x50B880
void CCamera::ProcessObbeCinemaCameraPed() {
    // NOP
}

//
void CCamera::ProcessObbeCinemaCameraPlane() {
    assert(0);
}

//
void CCamera::ProcessObbeCinemaCameraTrain() {
    assert(0);
}

// 0x50B890
void CCamera::ProcessWideScreenOn() {
    if (m_bWantsToSwitchWidescreenOff) {
        m_bWantsToSwitchWidescreenOff = false;
        m_bWideScreenOn = false;
        m_fWideScreenReductionAmount = 0.0f;
        m_fScreenReductionPercentage = 0.0f;
        m_fFOV_Wide_Screen = 0.0f;
    } else {
        m_fWideScreenReductionAmount = 1.0f;
        m_fScreenReductionPercentage = 30.0f;
        m_fFOV_Wide_Screen = m_aCams[m_nActiveCam].m_fFOV * 0.3f;
    }
}

// 0x516440
void CCamera::ProcessVectorTrackLinear() {
    const float now = static_cast<float>(CTimer::GetTimeInMS());
    if (now <= m_fTrackLinearEndTime) {
        ProcessVectorTrackLinear((now - m_fTrackLinearStartTime) / (m_fTrackLinearEndTime - m_fTrackLinearStartTime));
    } else if (m_bCameraPersistTrack) {
        m_bVecTrackLinearProcessed = true;
    }
}

// 0x50D350
void CCamera::ProcessVectorTrackLinear(float ratio) {
    m_bVecTrackLinearProcessed = true;
    const CVector& a = m_vecTrackLinearStartPoint;
    const CVector& b = m_vecTrackLinearEndPoint;
    const float t = m_bTrackLinearWithEase
        ? (std::sin((270.0f - ratio * 180.0f) * 0.017453292f) + 1.0f) * 0.5f
        : ratio;
    m_vecTrackLinear.x = (b.x - a.x) * t + a.x;
    m_vecTrackLinear.y = (b.y - a.y) * t + a.y;
    m_vecTrackLinear.z = (b.z - a.z) * t + a.z;
}

//
void CCamera::ProcessObbeCinemaCameraBoat() {
    assert(0);
}

//
void CCamera::ProcessObbeCinemaCameraCar() {
    assert(0);
}

//
void CCamera::ProcessObbeCinemaCameraHeli() {
    assert(0);
}

// 0x50D430
void CCamera::ProcessVectorMoveLinear(float ratio) {
    m_bVecMoveLinearProcessed = true;
    const CVector& a = m_vecMoveLinearPosnStart;
    const CVector& b = m_vecMoveLinearPosnEnd;
    const float t = m_bMoveLinearWithEase
        ? (std::sin((270.0f - ratio * 180.0f) * 0.017453292f) + 1.0f) * 0.5f
        : ratio;
    m_vecMoveLinear.x = (b.x - a.x) * t + a.x;
    m_vecMoveLinear.y = (b.y - a.y) * t + a.y;
    m_vecMoveLinear.z = (b.z - a.z) * t + a.z;
}

// 0x516500
void CCamera::ProcessFOVLerp() {
    if (const auto now = static_cast<float>(CTimer::GetTimeInMS()); now <= m_fEndZoomTime) { /* Check if still processing */
        ProcessFOVLerp(invLerp(m_fStartZoomTime, m_fEndZoomTime, now));
    } else if (m_bBlockZoom) { /* Finished */
        m_bFOVLerpProcessed = true;
    }
}

// 0x50D510
void CCamera::ProcessFOVLerp(float ratio) {
    m_bFOVLerpProcessed = true;
    if (m_nZoomMode) {
        m_fFOVNew = (m_fZoomOutFactor - m_fZoomInFactor) * (std::sin((270.0f - ratio * 180.0f) * 0.017453292f) + 1.0f) * 0.5f + m_fZoomInFactor;
    } else {
        m_fFOVNew = (m_fZoomOutFactor - m_fZoomInFactor) * ratio + m_fZoomInFactor;
    }
}

// 0x5164A0
void CCamera::ProcessVectorMoveLinear() {
    const float now = static_cast<float>(CTimer::GetTimeInMS());
    if (now <= m_fMoveLinearEndTime) {
        ProcessVectorMoveLinear((now - m_fMoveLinearStartTime) / (m_fMoveLinearEndTime - m_fMoveLinearStartTime));
    } else if (m_bCameraPersistPosition) {
        m_bVecMoveLinearProcessed = true;
    }
}

// unused
// 0x51A6F0
void CCamera::ProcessShake() {
    plugin::CallMethod<0x51A6F0, CCamera*>(this);
}

// shakeIntensity not used
// 0x516560
CVector* CCamera::ProcessShake(float intensity) {
    return plugin::CallMethodAndReturn<CVector*, 0x516560, CCamera*, float>(this, intensity);
}

// inlined - 0x52B845
// 0x516AE0
void CCamera::ProcessScriptedCommands() {
    ProcessVectorMoveLinear();
    ProcessVectorTrackLinear();
    ProcessFOVLerp();
}

// 0x52B730
void CCamera::Process() {
    ZoneScoped;

    constexpr float PI     = std::numbers::pi_v<float>; // 0x858CB8
    constexpr float TWO_PI = 2.f * PI;                  // 0x858CBC

    // 0.5 - cos(x * pi) * 0.5 - NOTE: x87 extended precision is kept by the original, so callers decide when to round to float
    const auto EaseInOut = [](double x) { return 0.5 - std::cos(x * (double)PI) * 0.5; };

    // Keeps the camera at least 1.3 units (2D) away from the target (pushes it out along the same direction)
    const auto KeepAwayFromTarget = [](CVector& pos, const CVector& target) {
        constexpr float MIN_DIST = 1.3f; // 0x8CCF20
        const float dx = pos.x - target.x;
        const float dy = pos.y - target.y;
        if (std::sqrt((double)dy * dy + (double)dx * dx) < MIN_DIST) {
            const float angle = CGeneral::GetATanOfXY(dx, dy);
            pos.x = (float)(std::cos((double)angle) * MIN_DIST + target.x);
            pos.y = (float)(std::sin((double)angle) * MIN_DIST + target.y);
        }
    };

    // Camera's heading, 0 if looking straight up/down
    const auto GetFrontHeading = [this]() -> float {
        const auto& front = GetActiveCam().m_vecFront;
        if (front.x == 0.f && front.y == 0.f) {
            return 0.f;
        }
        return CGeneral::GetATanOfXY(front.x, front.y);
    };

    ResetMadeInvisibleObjects();

    bool  bFollowingVehicle = false; // (Not looking forward, and the target is a vehicle)
    float fov               = 0.f;
    m_bJust_Switched        = false;

    m_vecRealPreviousCameraPosition = GetPosition();

    if (m_bLookingAtPlayer || m_bTargetJustBeenOnTrain || m_nWhoIsInControlOfTheCamera == 2) {
        UpdateTargetEntity();
    }

    if (!m_pTargetEntity) {
        m_pTargetEntity = FindPlayerPed(-1);
        m_pTargetEntity->RegisterReference(&m_pTargetEntity);
    }
    for (const auto camIdx : { (int32)m_nActiveCam, (m_nActiveCam + 1) % 2 }) {
        auto& camTarget = m_aCams[camIdx].m_pCamTargetEntity;
        if (!camTarget) {
            camTarget = m_pTargetEntity;
            camTarget->RegisterReference(&camTarget);
        }
    }

    CamControl();
    ProcessScriptedCommands(); // 0x52B845 - inlined

    if (m_bFading) {
        ProcessFade();
    }
    if (m_bMusicFading) {
        ProcessMusicFade();
    }
    if (m_bWideScreenOn) {
        ProcessWideScreenOn();
    }

    RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.3f);

    const float headingBefore = GetFrontHeading();
    GetActiveCam().Process();
    const float headingAfter = GetFrontHeading();

    if (m_bTransitionState && CTimer::GetTimeInMS() > m_nTimeTransitionStart + m_nTransitionDuration) {
        m_bTransitionState         = false;
        m_bDoingSpecialInterp      = false;
        m_bWaitForInterpolToFinish = false;
    }

    if (m_bUseNearClipScript) {
        RwCameraSetNearClipPlane(Scene.m_pRwCamera, m_fNearClipScript);
    }

    { // Did the camera heading change by a lot?
        double headingDiff = (double)headingAfter - (double)headingBefore;
        while (headingDiff >= PI) {
            headingDiff -= TWO_PI;
        }
        while (headingDiff < -PI) {
            headingDiff += TWO_PI;
        }
        if (std::abs(headingDiff) > 0.3f) {
            m_bJust_Switched = true;
        }
    }

    if (GetActiveCam().m_nDirectionWasLooking != LOOKING_DIRECTION_FORWARD && m_pTargetEntity->GetIsTypeVehicle()) {
        bFollowingVehicle = true;
    }

    const double timeNow = (double)CTimer::GetTimeInMS();
    if (timeNow <= m_fEndShakeTime) {
        ProcessShake((float)((timeNow - m_fStartShakeTime) / ((double)m_fEndShakeTime - m_fStartShakeTime)));
    }

    // NOTSA: Zero-initialised, the original leaves them uninitialised if the transition progress is NaN or > 1
    CVector source{}, target{}, front{}, up{};

    if (!m_bTransitionState || bFollowingVehicle) {
        auto& cam = GetActiveCam();

        source = cam.m_vecSource;
        up     = cam.m_vecUp;
        if (m_bMoveCamToAvoidGeom) {
            source += m_vecClearGeometryVec;

            front = cam.m_vecTargetCoorsForFudgeInter - source;
            front.Normalise();

            CVector right = CrossProduct(front, up);
            right.Normalise();

            up = CrossProduct(right, front);
            up.Normalise();
        } else {
            front = cam.m_vecFront;
        }

        gbCamUnkB70143 = false;
        fov            = cam.m_fFOV;
    } else {
        // Interpolating between the previous and the current camera
        uint32 elapsed = CTimer::GetTimeInMS() - m_nTimeTransitionStart;
        if (!(elapsed < m_nTransitionDuration)) {
            elapsed = m_nTransitionDuration;
        }

        float t = (float)((double)elapsed / (double)m_nTransitionDuration); // Progress of the source/up/fov interpolation

        // Progress of the target interpolation
        double targetT = (double)elapsed / (double)m_nTransitionDurationTargetCoors;
        if (targetT < 0.0) {
            targetT = 0.0;
        } else if (targetT > 1.0) {
            targetT = 1.0;
        }

        // Target
        if (targetT <= m_fFractionInterToStopMovingTarget) { // Moving with the starting speed
            const double x = m_fFractionInterToStopMovingTarget != 0.f
                ? ((double)m_fFractionInterToStopMovingTarget - targetT) / m_fFractionInterToStopMovingTarget
                : 0.0;
            const double f = EaseInOut(x);

            const float y = (float)(f * m_vecTargetSpeedAtStartInter.y);
            const float z = (float)(f * m_vecTargetSpeedAtStartInter.z);
            m_vecTargetWhenInterPol.x = (float)(f * m_vecTargetSpeedAtStartInter.x + m_vecStartingTargetForInterPol.x);
            m_vecTargetWhenInterPol.y = (float)((double)y + m_vecStartingTargetForInterPol.y);
            m_vecTargetWhenInterPol.z = (float)((double)z + m_vecStartingTargetForInterPol.z);
            target = m_vecTargetWhenInterPol;
        } else if (targetT > m_fFractionInterToStopMovingTarget) { // Catching up with the actual target
            const double x = m_fFractionInterToStopCatchUpTarget != 0.f
                ? (targetT - m_fFractionInterToStopMovingTarget) / m_fFractionInterToStopCatchUpTarget
                : 1.0;
            const float f = (float)EaseInOut(x);

            if (m_fFractionInterToStopMovingTarget == 0.f) {
                m_vecTargetWhenInterPol = m_vecStartingTargetForInterPol;
            }

            const auto& camTarget = GetActiveCam().m_vecTargetCoorsForFudgeInter;
            const double dx = (double)camTarget.x - m_vecTargetWhenInterPol.x;
            const double dy = (double)camTarget.y - m_vecTargetWhenInterPol.y;
            const float  dz = camTarget.z - m_vecTargetWhenInterPol.z;

            const float tx = (float)(dx * f);
            target.x = (float)((double)tx + m_vecTargetWhenInterPol.x);
            target.y = (float)(dy * f + m_vecTargetWhenInterPol.y);
            target.z = (float)((double)dz * f + m_vecTargetWhenInterPol.z);
        }

        // Source, up, fov
        const auto SetupFrontAndUp = [&](bool bNormaliseUp) {
            if (bNormaliseUp) {
                up.Normalise();
            }
            const auto mode = GetActiveCam().m_nMode;
            if (mode == MODE_TOPDOWN || mode == MODE_TOP_DOWN_PED) {
                front.Normalise();
                up = CrossProduct(front, CVector{ -1.f, 0.f, 0.f });
            } else {
                front.Normalise();
                up.Normalise();
                CVector right = CrossProduct(front, up);
                right.Normalise();
                up = CrossProduct(right, front);
            }
            up.Normalise();
        };

        if (t <= m_fFractionInterToStopMoving) { // Moving with the starting speed
            const double x = m_fFractionInterToStopMoving != 0.f
                ? ((double)m_fFractionInterToStopMoving - t) / m_fFractionInterToStopMoving
                : 0.0;
            const double f = EaseInOut(x);
            t = (float)f;

            // Source
            {
                const float z = (float)((double)t * m_vecSourceSpeedAtStartInter.z);
                m_vecSourceWhenInterPol.x = (float)(f * m_vecSourceSpeedAtStartInter.x + m_vecStartingSourceForInterPol.x);
                m_vecSourceWhenInterPol.y = (float)((double)t * m_vecSourceSpeedAtStartInter.y + m_vecStartingSourceForInterPol.y);
                m_vecSourceWhenInterPol.z = (float)((double)z + m_vecStartingSourceForInterPol.z);
            }
            if (m_bLookingAtPlayer) {
                KeepAwayFromTarget(m_vecSourceWhenInterPol, target);
            }

            // Up
            {
                const float z = (float)((double)t * m_vecUpSpeedAtStartInter.z);
                m_vecUpWhenInterPol.x = (float)((double)t * m_vecUpSpeedAtStartInter.x + m_vecStartingUpForInterPol.x);
                m_vecUpWhenInterPol.y = (float)((double)t * m_vecUpSpeedAtStartInter.y + m_vecStartingUpForInterPol.y);
                m_vecUpWhenInterPol.z = (float)((double)z + m_vecStartingUpForInterPol.z);
            }

            // FOV
            m_fFOVWhenInterPol = (float)((double)t * m_fFOVSpeedAtStartInter + m_fStartingFOVForInterPol);

            source = m_vecSourceWhenInterPol;
            front  = target - source;
            StoreValuesDuringInterPol(&source, &m_vecTargetWhenInterPol, &m_vecUpWhenInterPol, &m_fFOVWhenInterPol);
            front.Normalise();

            up = m_bLookingAtPlayer ? CVector{ 0.f, 0.f, 1.f } : m_vecUpWhenInterPol;

            SetupFrontAndUp(true);

            fov = m_fFOVWhenInterPol;
        } else if (t <= 1.f) { // Catching up with the actual camera
            const double x = m_fFractionInterToStopCatchUp != 0.f
                ? ((double)t - m_fFractionInterToStopMoving) / m_fFractionInterToStopCatchUp
                : 1.0;
            const float f = (float)EaseInOut(x);

            const auto& cam = GetActiveCam();

            source = m_vecSourceWhenInterPol + (cam.m_vecSource - m_vecSourceWhenInterPol) * f;
            if (m_bLookingAtPlayer) {
                KeepAwayFromTarget(source, target);
            }

            fov = (float)(((double)cam.m_fFOV - m_fFOVWhenInterPol) * f + m_fFOVWhenInterPol);

            up    = m_vecUpWhenInterPol + (cam.m_vecUp - m_vecUpWhenInterPol) * f;
            front = target - source;
            StoreValuesDuringInterPol(&source, &target, &up, &fov);
            front.Normalise();

            if (m_bLookingAtPlayer) {
                up = CVector{ 0.f, 0.f, 1.f };
            }

            SetupFrontAndUp(false);

            fov = m_fFOVWhenInterPol;
        }

        // Keep track of the speed
        const CVector diff  = source - target;
        const float   alpha = CGeneral::GetATanOfXY((float)std::sqrt((double)diff.y * diff.y + (double)diff.x * diff.x), diff.z);
        const float   beta  = CGeneral::GetATanOfXY(diff.x, diff.y);
        GetActiveCam().KeepTrackOfTheSpeed(source, target, up, alpha, beta, fov);
    }

    // Don't let the camera go through the walls when transitioning
    if (m_bTransitionState && !m_bLookingAtVector && m_bLookingAtPlayer && !CCullZones::CamStairsForPlayer() && !m_bPlayerIsInGarage) {
        CColPoint colPoint;
        CEntity*  hitEntity = nullptr;
        if (CWorld::ProcessLineOfSight(m_pTargetEntity->GetPosition(), source, colPoint, hitEntity, true, false, false, true, false, true, true, false)) {
            source = colPoint.m_vecPoint;
            RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.05f);
        }
    }

    // Drunk camera
    if (CMBlur::Drunkness > 0.f) {
        const double angle = (double)gDrunkCamAngle * (double)0.017453292f; // 0x8595EC
        const double cosA  = std::cos(angle);
        const double sinA  = std::sin(angle);
        const float  cosF  = (float)cosA;
        const float  sinF  = (float)sinA;

        const double sourceMult = (double)CMBlur::Drunkness * (double)-0.02f; // 0x85904C
        source.x = (float)(cosA * sourceMult + source.x);
        source.z = (float)(sinA * sourceMult + source.z);

        up.Normalise();
        const double upMult = (double)CMBlur::Drunkness * (double)0.05f; // 0x858C28
        up.x = (float)((double)cosF * upMult + up.x);
        up.y = (float)((double)sinF * upMult + up.y);
        up.Normalise();

        front.Normalise();
        const double frontMult = (double)CMBlur::Drunkness * (double)-0.1f; // 0x858EF4
        front.x = (float)((double)cosF * frontMult + front.x);
        front.y = (float)((double)sinF * frontMult + front.y);
        front.Normalise();

        CVector right = CrossProduct(front, up);
        right.Normalise();
        up = CrossProduct(right, front);
        up.Normalise();

        gDrunkCamAngle += 5.f; // 0x858C80
    }

    // Update the camera matrix
    m_mCameraMatrix.GetRight()    = CrossProduct(up, front);
    m_mCameraMatrix.GetForward()  = front;
    m_mCameraMatrix.GetUp()       = up;
    m_mCameraMatrix.GetPosition() = source;

    // Camera shake
    {
        const uint32 sinceShakeStart = CTimer::GetTimeInMS() - m_nCamShakeStart;
        float shake = (float)((double)m_fCamShakeForce - (double)sinceShakeStart * (double)0.00028f); // 0x863280
        if (shake < 2.f) {
            if (!(shake > 0.f)) {
                shake = 0.f;
            }
        } else {
            shake = 2.f;
        }

        const uint32 rnd = CGeneral::GetRandomNumber() & 0xFFFF;
        const double mult = (double)shake * (double)0.1f; // 0x858B1C
        auto& camPos = m_mCameraMatrix.GetPosition();
        camPos.x = (float)((double)(int32)((rnd & 0xF) - 7) * mult + camPos.x);
        camPos.y = (float)((double)(int32)(((rnd >> 4) & 0xF) - 7) * mult + camPos.y);
        camPos.z = (float)((double)(int32)(((rnd >> 8) & 0xF) - 7) * mult + camPos.z);

        if (mult > 0.0 && m_nBlurType != eMotionBlurType::SNIPER) {
            const int32 alpha = 25 - (int32)((double)shake * -255.0); // 0x86327C
            m_nMotionBlurAddAlpha = static_cast<uint32>(std::min(alpha, 150));
        }
    }

    // Blur when looking from the first person while the player's vehicle is (close to) upside down
    if (GetActiveCam().m_nMode == MODE_1STPERSON
        && FindPlayerVehicle(-1, false)
        && FindPlayerVehicle(-1, false)->GetUp().z < 0.2f // 0x858CC4
    ) {
        m_nBlurRed   = 255;
        m_nBlurGreen = 255;
        m_nBlurBlue  = 255;
        m_nBlurType  = eMotionBlurType::SNIPER;
        m_nMotionBlur = 240;
        gbFirstPersonUpsideDownBlur = true;
    } else if (gbFirstPersonUpsideDownBlur) {
        gbFirstPersonUpsideDownBlur = false;
    }

    CDraw::SetFOV(fov);
    CalculateDerivedValues(false, true);
    CopyCameraMatrixToRWCam(false);
    m_vecGameCamPos = m_mCameraMatrix.GetPosition();
    UpdateSoundDistances();

    if (CCutsceneMgr::ms_running && !CCutsceneMgr::ms_useLodMultiplier) {
        m_fLODDistMultiplier = 1.f;
    } else {
        m_fLODDistMultiplier = 70.f / CDraw::ms_fFOV; // 0x858CE0
    }
    m_fGenerationDistMultiplier = m_fLODDistMultiplier;
    m_fLODDistMultiplier        = CRenderer::ms_lodDistScale * m_fLODDistMultiplier;

    RwCameraSetFarClipPlane(Scene.m_pRwCamera, (float)((double)(int32)((double)Scene.m_pRwCamera->farPlane * 100.0) * (double)0.01f));
    CDraw::ms_fNearClipZ = m_pRwCamera->nearPlane;
    CDraw::ms_fFarClipZ  = m_pRwCamera->farPlane;

    // Camera's average speed
    if (m_bJustInitialized || m_bJust_Switched) {
        m_vecPreviousCameraPosition = m_mCameraMatrix.GetPosition();
        m_bJustInitialized          = false;
    }
    {
        const auto& camPos = m_mCameraMatrix.GetPosition();
        const double dx    = (double)camPos.x - m_vecPreviousCameraPosition.x;
        const double dy    = (double)camPos.y - m_vecPreviousCameraPosition.y;
        const double dz    = (double)camPos.z - m_vecPreviousCameraPosition.z;

        m_nNumFramesSoFar++;
        const double speedSoFar = std::sqrt(dz * dz + dy * dy + dx * dx) + m_fCameraSpeedSoFar;
        m_fCameraSpeedSoFar     = (float)speedSoFar;
        if (m_nNumFramesSoFar == m_nWorkOutSpeedThisNumFrames) {
            m_fCameraSpeedSoFar  = 0.f;
            m_nNumFramesSoFar    = 0;
            m_fCameraAverageSpeed = (float)(speedSoFar / (double)(int32)m_nWorkOutSpeedThisNumFrames);
        }
    }

    const float newOrientation = m_fOrientation + PI;
    m_vecPreviousCameraPosition = m_mCameraMatrix.GetPosition();

    {
        auto& cam = GetActiveCam();
        if (cam.m_nDirectionWasLooking != LOOKING_DIRECTION_FORWARD && cam.m_nMode != MODE_TOP_DOWN_PED) {
            cam.m_vecSource = cam.m_vecSourceBeforeLookBehind;
            m_fOrientation  = newOrientation;
        }
    }

    if (m_bTransitionState) {
        auto& activeCam = GetActiveCam();
        auto& otherCam  = m_aCams[(m_nActiveCam + 1) % 2];
        if (otherCam.m_pCamTargetEntity
            && m_pTargetEntity
            && m_pTargetEntity->GetIsTypePed()
            && !otherCam.m_pCamTargetEntity->GetIsTypeVehicle()
            && activeCam.m_nMode != MODE_TOP_DOWN_PED
            && otherCam.m_nDirectionWasLooking != LOOKING_DIRECTION_FORWARD
        ) {
            // BUG: Copies the `m_nActiveCam % 2`-th camera's (= the active one's, or the 1st if it's the debug cam) value into the *other* camera,
            //      most likely `otherCam.m_vecSourceBeforeLookBehind` was intended
            otherCam.m_vecSource = m_aCams[m_nActiveCam % 2].m_vecSourceBeforeLookBehind;
            m_fOrientation       = newOrientation;
        }
    }

    m_bCameraJustRestored = false;
    m_bMoveCamToAvoidGeom = false;

    // Underwater-ness: sample the water level slightly in front of the camera
    CVector samplePos;
    {
        // Direction the camera is facing (2D)
        double dirX;
        float  dirY, dirZ;
        if (m_matrix) {
            const auto& fwd = m_matrix->GetForward();
            dirX = fwd.x;
            dirY = fwd.y;
            dirZ = fwd.z;
        } else {
            dirX = -std::sin((double)m_placement.m_fHeading);
            dirY = (float)std::cos((double)m_placement.m_fHeading);
            dirZ = 0.f;
        }

        constexpr float SAMPLE_DIST = 0.4f; // 0x858EE8
        const auto&     pos         = GetPosition();
        const float     zMult       = (float)((double)dirZ * SAMPLE_DIST);
        samplePos.x = (float)(dirX * SAMPLE_DIST + pos.x);
        samplePos.y = (float)((double)dirY * SAMPLE_DIST + pos.y);
        samplePos.z = (float)((double)zMult + pos.z);
    }

    float waterLevel;
    if (!CWaterLevel::GetWaterLevel(samplePos.x, samplePos.y, samplePos.z, waterLevel, true, nullptr)
        || (double)samplePos.z - 0.6f > waterLevel // 0x858CC8 - NOTE: compared in x87 extended precision
    ) {
        CWeather::UnderWaterness = 0.f;
        return;
    }

    const float depth = waterLevel - samplePos.z;
    CWeather::WaterDepth = depth < 0.f ? 0.f : depth;

    if ((double)samplePos.z + 0.6f < waterLevel) {
        CWeather::UnderWaterness = 1.f;
    } else {
        CWeather::UnderWaterness = (float)(1.0 - ((double)samplePos.z - ((double)waterLevel - 0.6f)) * (double)0.8333333f); // 0x863278
    }
}

// 0x514860
void CCamera::DrawBordersForWideScreen() {
    CRect rect;
    GetScreenRect(&rect);
    if (m_nBlurType == eMotionBlurType::NONE || m_nBlurType == eMotionBlurType::LIGHT_SCENE) {
        m_nMotionBlurAddAlpha = 80;
    }
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));
    CSprite2d::DrawRect({ -5.f, -5.f,     SCREEN_WIDTH + 5.f, rect.top         }, { 0, 0, 0, 255 });
    CSprite2d::DrawRect({ -5.f, rect.bottom, SCREEN_WIDTH + 5.f, SCREEN_HEIGHT + 5.f }, { 0, 0, 0, 255 });
}

// 0x4748A0
bool CCamera::VectorMoveRunning() const {
    return CTimer::m_snTimeInMilliseconds <= m_fMoveLinearEndTime;
}

// 0x474891
bool CCamera::VectorTrackRunning() const {
    return CTimer::m_snTimeInMilliseconds <= m_fTrackLinearEndTime;
}

// 0x514950
void CCamera::FinishCutscene() {
    SetPercentAlongCutScene(100.0f);
    m_fPositionAlongSpline = 1.0f;
    m_bCutsceneFinished = true;
}

// 0x514970
void CCamera::Find3rdPersonCamTargetVector(float range, CVector gunMuzzle, CVector& outSource, CVector& outTarget) {
    const auto pActiveCam = &m_aCams[m_nActiveCam];
    const float tanHalfFOV = std::tan(DegreesToRadians(pActiveCam->m_fFOV * 0.5f));
    const float aspectRatio = CDraw::ms_fAspectRatio;
    
    // Calculate aim target direction (This will be a unit vector)
    CVector dir = m_aCams[m_nActiveCam].m_vecFront;
    
    if (pActiveCam->m_nMode == eCamMode::MODE_TWOPLAYER_IN_CAR_AND_SHOOTING) {
        pActiveCam->Get_TwoPlayer_AimVector(dir);
    } else {
        // Vertical offset
        dir += pActiveCam->m_vecUp * (tanHalfFOV * ((0.5f - m_f3rdPersonCHairMultY) * 2.0f) / aspectRatio);

        // Horizontal offset
        const auto right = pActiveCam->m_vecFront.Cross(pActiveCam->m_vecUp);
        dir += right * (tanHalfFOV * ((m_f3rdPersonCHairMultX - 0.5f) * 2.0f));
        
        // Handle zero magnitude case
        if (dir.Magnitude() <= 0.0f) {
            dir = CVector(1.0f, 0.0f, 0.0f);
        } else {
            dir.Normalise();
        }
    }
    
    // Calculate intersection point with muzzle
    outSource = pActiveCam->m_vecSource;
    outSource += (gunMuzzle - outSource).ProjectOnToNormal(dir);

    // Apply final range to target 
    outTarget = outSource + dir * range;
}

// 0x514B80
float CCamera::CalculateGroundHeight(eGroundHeightType type) {
    static auto& lastCalcCamPos    = StaticRef<CVector>(0xB70034);
    static auto& exactGroundHeight = StaticRef<float>(0xB70030);
    static auto& bbTopZ            = StaticRef<float>(0xB7002C);
    static auto& bbBottomZ         = StaticRef<float>(0xB70028);

    const auto& camPos = GetPosition();

    // Possibly update the positions (If the camera has moved enough)
    const auto CheckDelta = [](float d) { return std::abs(d) > 20.f; };
    if (CheckDelta(lastCalcCamPos.x - camPos.x) || CheckDelta(lastCalcCamPos.y - camPos.y) || CheckDelta(lastCalcCamPos.z - camPos.z)) { // Check if there's enough of a delta
        CColPoint cp;
        CEntity* hitEntity;
        if (CWorld::ProcessVerticalLine({ camPos.x, camPos.y, 1000.f }, -1000.f, cp, hitEntity, true, false, false, false, true)) {
            const auto& hitEntPos = hitEntity->GetPosition();
            const auto& hitBB = hitEntity->GetColModel()->GetBoundingBox();

            exactGroundHeight = cp.m_vecPoint.z;

            bbTopZ = hitEntPos.z + hitBB.m_vecMax.z;

            const auto bbsz = hitBB.GetSize();
            bbBottomZ = std::max(
                0.f,
                bbsz.x > 120.f || bbsz.y > 120.f
                    ? exactGroundHeight
                    : hitEntPos.z + hitBB.m_vecMin.z
            );
        }
        lastCalcCamPos = camPos;
    }

    switch (type) {
    case eGroundHeightType::ENTITY_BB_TOP:       return bbTopZ;
    case eGroundHeightType::EXACT_GROUND_HEIGHT: return exactGroundHeight;
    case eGroundHeightType::ENTITY_BB_BOTTOM:    return bbBottomZ;
    default:                                     NOTSA_UNREACHABLE();
    }
}

// 0x514D60
void CCamera::CalculateFrustumPlanes(bool bForMirror) {
    const float halfFov = CDraw::GetFOV() * 0.008726389f; // degrees to radians, halved
    const float c = std::cos(halfFov);
    const float s = std::sin(halfFov);

    m_avecFrustumNormals[0] = CVector{ c, -s, 0.f };
    m_avecFrustumNormals[1] = CVector{ -c, -s, 0.f };

    const float aspect = (float)RsGlobal.maximumHeight / (float)RsGlobal.maximumWidth;
    const float ac = aspect * c;
    const float as = aspect * s;
    m_avecFrustumNormals[2] = CVector{ 0.f, -as, -ac };
    m_avecFrustumNormals[3] = CVector{ 0.f, -as, ac };

    const auto Dot = [](const CVector& n, const CVector& p) { // NOTE: order of additions matters for float results
        return (n.y * p.y + n.z * p.z) + n.x * p.x;
    };

    if (!bForMirror) {
        TransformVectors(m_avecFrustumWorldNormals.data(), 4, m_mCameraMatrix, m_avecFrustumNormals.data());
        for (auto i = 0; i < 4; i++) {
            m_fFrustumPlaneOffsets[i] = Dot(m_avecFrustumWorldNormals[i], GetPosition());
        }
    } else {
        TransformVectors(m_avecFrustumWorldNormals_Mirror.data(), 4, m_mCameraMatrix, m_avecFrustumNormals.data());
        for (auto i = 0; i < 4; i++) {
            m_fFrustumPlaneOffsets_Mirror[i] = Dot(m_avecFrustumWorldNormals_Mirror[i], GetPosition());
        }
    }
}

// 0x5150E0
void CCamera::CalculateDerivedValues(bool bForMirror, bool bOriented) {
    m_mMatInverse = Invert(m_mCameraMatrix);
    CalculateFrustumPlanes(bForMirror);

    auto& fwd = m_mCameraMatrix.GetForward();
    if (fwd.x == 0.f && fwd.y == 0.f) {
        fwd.x = 0.0001f;
    } else if (bOriented) {
        m_fOrientation = std::atan2(fwd.x, fwd.y);
    }

    m_fCamFrontXNorm = fwd.x;
    m_fCamFrontYNorm = fwd.y;
    const float len = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y);
    if (len == 0.f) {
        m_fCamFrontXNorm = 1.f;
    } else {
        const float inv = 1.f / len;
        m_fCamFrontXNorm = inv * fwd.x;
        m_fCamFrontYNorm = inv * fwd.y;
    }
}

// 0x516B20
void CCamera::ImproveNearClip(CVehicle* vehicle, CPed* ped, CVector* source, CVector* targPosn) {
    const auto& cam = GetActiveCam();

    // NOTE: The original keeps the differences in x87 extended precision (no rounding to float), the sum is accumulated in `z, y, x` order
    const auto DistToTarget = [&] {
        const double dx = (double)source->x - targPosn->x;
        const double dy = (double)source->y - targPosn->y;
        const double dz = (double)source->z - targPosn->z;
        return std::sqrt(dz * dz + dy * dy + dx * dx);
    };

    // Far away from the target => push the near clip out (scaled by the current collision distance)
    if (DistToTarget() > 10.0) {
        const float nearClip = 1.f * gCurDistForCam;
        if (Scene.m_pRwCamera->nearPlane < nearClip) {
            RwCameraSetNearClipPlane(Scene.m_pRwCamera, nearClip);
        }
    }

    if (!vehicle) {
        if (ped) {
            if (!ped->bIsStanding) {
                const bool bUsingParachute = ped->GetIntelligence()->GetUsingParachute();
                if (const auto* const swim = ped->GetIntelligence()->GetTaskSwim()) {
                    const bool bUnderwaterSprinting = swim->m_nSwimState == SWIM_UNDERWATER_SPRINTING;

                    float waterLevel = 0.f;
                    const bool bHasWater = CWaterLevel::GetWaterLevel(source->x, source->y, source->z, waterLevel, false, nullptr);
                    if (bHasWater && std::abs((double)waterLevel - source->z) < (double)0.3f) { // On the water surface
                        RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.1f);
                    } else if (bUnderwaterSprinting && m_nPedZoom == 1) {
                        RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.1f);
                    }
                } else if (bUsingParachute || ped->GetIntelligence()->GetTaskJetPack()) {
                    if (GetRoughDistanceToGround() > 10.f) {
                        const float nearClip = std::min(
                            (*source - *targPosn).Magnitude() * 0.3f, // NOTE: the original passes both values to a `min(a, b)` function
                            2.f * gCurDistForCam
                        );
                        if (nearClip > Scene.m_pRwCamera->nearPlane) {
                            RwCameraSetNearClipPlane(Scene.m_pRwCamera, nearClip);
                        }
                    }
                }
            } else {
                // Standing ped => near clip based on the closest hit-col sphere along the camera's front vector
                const float maxDist = (float)(std::sin((90.0 - (double)cam.m_fFOV * 0.5) * (double)0.017453292f) * (double)gCamColLastRadius);

                auto* const pedMI = CModelInfo::GetPedModelInfo(ped->m_nModelIndex);
                pedMI->AnimatePedColModelSkinnedWorld(ped->GetRpClump());
                const auto* const spheres = pedMI->m_pHitColModel->GetData()->m_pSpheres;

                const auto& front = cam.m_vecFront;
                const auto& camPos = cam.m_vecSource;

                // NOTE: x87 extended precision is kept along the chain in the original, only the final value is stored as float
                const float camPosAlongFront = (float)((double)front.z * camPos.z + (double)front.y * camPos.y + (double)camPos.x * front.x);

                float minDist = 999999.f; // 0x497423F0
                for (int32 i = 0; i < CPedModelInfo::NUM_PED_COL_NODE_INFOS; i++) {
                    const auto& sphere = spheres[i];
                    const auto& c      = sphere.m_vecCenter;
                    double dist = ((double)c.x * front.x + (double)c.z * front.z + (double)c.y * front.y) - camPosAlongFront - sphere.m_fRadius;
                    if (sphere.m_Surface.m_nPiece == PED_PIECE_HEAD) {
                        dist -= 1.0 * sphere.m_fRadius; // 0x8CCCD8
                    }
                    if (dist < minDist) {
                        minDist = (float)dist;
                    }
                }

                if (minDist > maxDist) {
                    minDist = maxDist;
                } else if (minDist < 0.02f) {
                    minDist = 0.02f;
                }
                if (minDist > 0.3f) {
                    minDist = 0.3f;
                }

                RwCameraSetNearClipPlane(Scene.m_pRwCamera, (float)((double)(int)((double)minDist * 100.0) * (double)0.01f));
            }
        }
    } else {
        const auto vehType = vehicle->m_nVehicleType;
        if (vehType == VEHICLE_TYPE_PLANE || vehType == VEHICLE_TYPE_HELI) {
            if (gCurDistForCam > 0.3f) {
                if (cam.m_vecSource.z - CalculateGroundHeight(eGroundHeightType::ENTITY_BB_BOTTOM) > 10.f) {
                    float nearClip = 5.f * gCurDistForCam;

                    const double distScaled = DistToTarget() * (double)0.1f;
                    if (distScaled < nearClip) {
                        nearClip = (float)distScaled;
                    }

                    if (nearClip > Scene.m_pRwCamera->nearPlane) {
                        RwCameraSetNearClipPlane(Scene.m_pRwCamera, nearClip);
                    }
                }
            } else if (vehType == VEHICLE_TYPE_HELI) {
                RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.1f);
            }
        }
    }

    float nearest = 999999.f; // 0x497423F0 - unused by the function
    CCollision::CheckPeds(*source, cam.m_vecFront, nearest);
}

static auto& preMirrorMat = StaticRef<CMatrix>(0xB6FE40);

// 0x51A560
void CCamera::SetCameraUpForMirror() {
    preMirrorMat = m_mCameraMatrix;
    m_mCameraMatrix = m_mMatMirror;
    CopyCameraMatrixToRWCam(true);
    CalculateDerivedValues(true, false);
}

// 0x51A5A0
void CCamera::RestoreCameraAfterMirror() {
    SetMatrix(preMirrorMat);
    CopyCameraMatrixToRWCam(true);
    CalculateDerivedValues(false, false);
}

// 0x51A5D0
bool CCamera::ConeCastCollisionResolve(const CVector& pos, const CVector& lookAt, CVector& outDest, float radius, float minDist, float& outDist) {
    if (pos == lookAt) {
        return false;
    }

    if (CCollision::CameraConeCastVsWorldCollision(CSphere{ lookAt, radius }, CSphere{ pos, radius }, outDist, minDist)) {
        outDest = lerp(lookAt, pos, outDist);
        return true;
    } else {
        outDest = pos;
        outDist = 1.f;
        return false;
    }
}

// Minimum height above the water level for the fixed cameras of `TryToStartNewCamMode`
static auto& gFixedCamMinHeightAboveWater     = StaticRef<float>(0x8CC8C0); // NOTSA name: 1.0
static auto& gFixedCamMinHeightAboveWaterBoat = StaticRef<float>(0x8CC8C4); // NOTSA name: -2.0

namespace {
// 0x50B830 (unnamed in the original) - Is the active cam's source at or below the water level?
bool IsActiveCamUnderWater() {
    const CVector& src = TheCamera.m_aCams[TheCamera.m_nActiveCam].m_vecSource;
    float level;
    return CWaterLevel::GetWaterLevel(src.x, src.y, src.z, level, true, nullptr) && level >= src.z;
}

// 0x59C790 - `CMatrix::TransformVector`, but with the add order of the original
CVector TransformVectorOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return CVector{
        (u.x * v.z + f.x * v.y) + r.x * v.x,
        (u.y * v.z + r.y * v.x) + f.y * v.y,
        (u.z * v.z + r.z * v.x) + f.z * v.y
    };
}

// The player's coords moved along (normalised) `dir` by `fwd` and sideways by `sideX`/`sideY` (the fixed cam candidates of `TryToStartNewCamMode`)
CVector OffsetFromPlayer(const CVector& pos, const CVector& dir, float fwd, float sideX, float sideY) {
    return CVector{
        (pos.x + dir.x * fwd) + dir.y * sideX,
        (dir.y * fwd + pos.y) + dir.x * sideY,
        pos.z + dir.z * fwd
    };
}

// Move the position onto the ground below it (or onto the roof above it)
void SnapToGroundOrRoof(CVector& pos, float heightOffset) {
    bool       found    = false;
    const auto groundZ  = CWorld::FindGroundZFor3DCoord(CVector{ pos.x, pos.y, pos.z + 5.f }, &found);
    if (found) {
        pos.z = groundZ + heightOffset;
    } else {
        const auto roofZ = CWorld::FindRoofZFor3DCoord(pos.x, pos.y, pos.z - 5.f, &found);
        if (found) {
            pos.z = roofZ + heightOffset;
        }
    }
}

// Rotate (the XY of) the velocity by `angleOffset` around the heading (the original keeps `cos` in extended precision)
void RotateHeading(CVector& vel, float angleOffset) {
    const double angle = static_cast<double>(CGeneral::GetATanOfXY(vel.x, vel.y)) + static_cast<double>(angleOffset);
    const double cosA  = std::cos(angle);
    const float  sinA  = static_cast<float>(std::sin(angle));
    vel.x = static_cast<float>(cosA + static_cast<double>(vel.x));
    vel.y = sinA + vel.y;
}

// Don't let the position go below the water surface (if there is any)
void ClampAboveWater(CVector& pos) {
    float level;
    if (!CWaterLevel::GetWaterLevelNoWaves(pos, &level)) {
        return;
    }

    float margin = gFixedCamMinHeightAboveWater;
    if (const auto* const veh = FindPlayerVehicle(); veh && veh->m_nVehicleType == VEHICLE_TYPE_BOAT) {
        margin = gFixedCamMinHeightAboveWaterBoat;
    }
    if (pos.z < margin + level) {
        pos.z = margin + level;
    }
}

// Is the player's vehicle a boat that is not (target-wise) the Skimmer?
bool IsPlayerBoatWithoutSkimmerTarget(const CCamera& cam) {
    const auto* const veh = FindPlayerVehicle();
    return veh && veh->m_nVehicleType == VEHICLE_TYPE_BOAT && cam.m_pTargetEntity->m_nModelIndex != MODEL_SKIMMER;
}

bool IsLineOfSightClearFromPlayer(const CVector& target) {
    return CWorld::GetIsLineOfSightClear(FindPlayerCoors(), target, true, false, false, false, false, false, false);
}
}

// 0x51E560
bool CCamera::TryToStartNewCamMode(int32 camSequence) {
    // Common finish of the fixed cam cases: the cam is made fixed at `pos`, true if the cam is not under water afterwards
    const auto StartFixedCam = [this](const CVector& pos) {
        SetCamPositionForFixedMode(pos, CVector{ 0.f, 0.f, 0.f });
        TakeControl(FindPlayerEntity(), MODE_FIXED, eSwitchType::JUMPCUT, 2);
        return !IsActiveCamUnderWater();
    };

    switch (camSequence) {
    case 0: {
        auto* const veh = FindPlayerVehicle();
        if (!veh) {
            return false;
        }
        if (veh->m_nVehicleType == VEHICLE_TYPE_BOAT && m_pTargetEntity->m_nModelIndex != MODEL_SKIMMER) {
            return false;
        }
        if (veh->m_nModelIndex == MODEL_RHINO) {
            return false;
        }

        CVector target = TransformVectorOriginal(veh->GetMatrix(), CVector{ -1.4f, -2.3f, 0.3f });
        target += veh->GetPosition();
        if (!CWorld::GetIsLineOfSightClear(veh->GetPosition(), target, true, false, false, false, false, false, false)) {
            return false;
        }

        TakeControl(veh, MODE_WHEELCAM, eSwitchType::JUMPCUT, 2);
        return true;
    }
    case 1: {
        CVector pos = FindPlayerCoors();
        CVector vel = FindPlayerSpeed();
        vel.z       = 0.f;
        vel.Normalise();
        pos = OffsetFromPlayer(pos, vel, 20.f, 3.f, -3.f);

        if (IsPlayerBoatWithoutSkimmerTarget(*this)) {
            return false;
        }

        SnapToGroundOrRoof(pos, 1.5f);

        if (!IsLineOfSightClearFromPlayer(pos)) {
            return false;
        }

        const CVector toPlayer = FindPlayerCoors() - pos;
        const CVector toPlayer2D{ toPlayer.x, toPlayer.y, 0.f };
        if (toPlayer2D.Magnitude() > 40.f) {
            const CVector& speed = FindPlayerSpeed();
            if (((toPlayer2D.y * speed.y) + (toPlayer2D.x * speed.x)) + (speed.z * 0.f) > 0.f) {
                return false;
            }
        }
        if (toPlayer2D.Magnitude() < 4.5f) {
            return false;
        }

        return StartFixedCam(pos);
    }
    case 2: {
        if (IsPlayerBoatWithoutSkimmerTarget(*this)) {
            return false;
        }

        CVector pos = FindPlayerCoors();
        CVector vel = FindPlayerSpeed();
        vel.z       = 0.f;
        vel.Normalise();
        pos = OffsetFromPlayer(pos, vel, 16.f, 2.5f, -2.5f);

        SnapToGroundOrRoof(pos, 0.5f);

        if (!IsLineOfSightClearFromPlayer(pos)) {
            return false;
        }

        const CVector toPlayer = FindPlayerCoors() - pos;
        const CVector toPlayer2D{ toPlayer.x, toPlayer.y, 0.f };
        if (toPlayer2D.Magnitude() > 29.f) {
            const CVector& speed = FindPlayerSpeed();
            if (((toPlayer2D.y * speed.y) + (speed.z * 0.f)) + (toPlayer2D.x * speed.x) > 0.f) {
                return false;
            }
        }
        if (toPlayer2D.Magnitude() < 2.f) {
            return false;
        }

        SetCamPositionForFixedMode(pos, CVector{ 0.f, 0.f, 0.f });
        TakeControl(FindPlayerEntity(), MODE_FIXED, eSwitchType::JUMPCUT, 2);
        RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.15f);
        return !IsActiveCamUnderWater();
    }
    case 3: {
        CVector pos = FindPlayerCoors();
        CVector vel = FindPlayerSpeed();
        vel.z       = 0.f;
        vel.Normalise();
        pos   = OffsetFromPlayer(pos, vel, 30.f, 8.f, -8.f);
        pos.z = pos.z + 16.f;

        if (!IsLineOfSightClearFromPlayer(pos)) {
            return false;
        }

        SetCamPositionForFixedMode(pos, CVector{ 0.f, 0.f, 0.f });
        TakeControl(FindPlayerEntity(), MODE_FIXED, eSwitchType::JUMPCUT, 2);
        if (IsActiveCamUnderWater()) {
            return false;
        }
        RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.15f);
        return true;
    }
    case 5: {
        CVector pos = FindPlayerCoors();
        CVector vel = FindPlayerSpeed();
        vel.z       = 0.f;
        vel.Normalise();
        pos = OffsetFromPlayer(pos, vel, 30.f, -6.f, 6.f);

        SnapToGroundOrRoof(pos, 3.5f);

        if (!IsLineOfSightClearFromPlayer(pos)) {
            return false;
        }

        return StartFixedCam(pos);
    }
    case 6:
        TakeControl(FindPlayerEntity(), MODE_1STPERSON, eSwitchType::JUMPCUT, 2);
        return true;
    case 7: {
        if (FindPlayerPed()->GetWantedLevel() <= eWantedLevel::WANTED_CLEAN || !FindPlayerVehicle()) {
            return false;
        }
        if (IsPlayerBoatWithoutSkimmerTarget(*this)) {
            return false;
        }

        auto* const pool = GetVehiclePool();
        for (auto i = pool->GetSize(); i > 0;) {
            i--;

            auto* const veh = pool->GetAt(i);
            if (!veh || veh->m_nVehicleType != VEHICLE_TYPE_AUTOMOBILE || veh == FindPlayerVehicle()) {
                continue;
            }
            if (!veh->vehicleFlags.bIsLawEnforcer || veh->GetStatus() != STATUS_PHYSICS) {
                continue;
            }

            const float dx = veh->GetPosition().x - FindPlayerCoors().x;
            const float dy = veh->GetPosition().y - FindPlayerCoors().y;
            if (!((FindPlayerCoors() - veh->GetPosition()).Magnitude() < 30.f)) {
                continue;
            }

            const auto& playerFwd = FindPlayerVehicle()->GetForward();
            if (!((dy * playerFwd.y + dx * playerFwd.x) < 0.f)) {
                continue;
            }

            const auto& vehFwd = veh->GetForward();
            if (!((playerFwd.y * vehFwd.y + playerFwd.x * vehFwd.x) > 0.8f)) {
                continue;
            }

            TakeControl(veh, MODE_CAM_ON_A_STRING, eSwitchType::JUMPCUT, 2);
            if (!IsActiveCamUnderWater()) {
                return true;
            }
        }
        return false;
    }
    case 8: {
        if (FindPlayerPed()->GetWantedLevel() <= eWantedLevel::WANTED_CLEAN || !FindPlayerVehicle()) {
            return false;
        }
        if (IsPlayerBoatWithoutSkimmerTarget(*this)) {
            return false;
        }

        auto* const pool = GetVehiclePool();
        for (auto i = pool->GetSize(); i > 0;) {
            i--;

            auto* const veh = pool->GetAt(i);
            if (!veh || veh->m_nVehicleType != VEHICLE_TYPE_AUTOMOBILE || veh == FindPlayerVehicle()) {
                continue;
            }
            if (!veh->vehicleFlags.bIsLawEnforcer) {
                continue;
            }

            const float dx = veh->GetPosition().x - FindPlayerCoors().x;
            const float dy = veh->GetPosition().y - FindPlayerCoors().y;
            if (!((FindPlayerCoors() - veh->GetPosition()).Magnitude() < 30.f)) {
                continue;
            }

            const auto& playerFwd = FindPlayerVehicle()->GetForward();
            if (!((dy * playerFwd.y + dx * playerFwd.x) < 0.f)) {
                continue;
            }

            const auto& vehFwd = veh->GetForward();
            if (!((playerFwd.y * vehFwd.y + playerFwd.x * vehFwd.x) > 0.8f)) {
                continue;
            }

            CVector target = TransformVectorOriginal(veh->GetMatrix(), CVector{ -1.4f, -2.3f, 0.3f });
            target += veh->GetPosition();
            if (!CWorld::GetIsLineOfSightClear(veh->GetPosition(), target, true, false, false, false, false, false, false)) {
                return false;
            }

            TakeControl(veh, MODE_WHEELCAM, eSwitchType::JUMPCUT, 2);
            if (!IsActiveCamUnderWater()) {
                return true;
            }
        }
        return false;
    }
    case 15: {
        if (!FindPlayerVehicle()) {
            return false;
        }

        CVector pos = FindPlayerCoors();
        CVector vel = FindPlayerSpeed();
        vel.z       = 0.f;
        vel.Normalise();
        pos.x = vel.x * 34.f + pos.x;
        pos.y = pos.y + vel.y * 34.f;
        pos.z = FindPlayerCoors().z + 0.5f; // NOTE: The original first adds `vel.z * 34.f`, but then overwrites it
        if (FindPlayerVehicle()->m_nVehicleType == VEHICLE_TYPE_BOAT) {
            pos.z += 1.f;
        }

        if (!IsLineOfSightClearFromPlayer(pos)) {
            return false;
        }

        const CVector toPlayer = FindPlayerCoors() - pos;
        if (toPlayer.Magnitude() > 44.f) {
            const CVector& speed = FindPlayerSpeed();
            if (((toPlayer.y * speed.y) + (speed.z * 0.f)) + (toPlayer.x * speed.x) > 0.f) {
                return false;
            }
        }
        if (toPlayer.Magnitude() < 3.f) {
            return false;
        }

        return StartFixedCam(pos);
    }
    case 16: {
        if (!FindPlayerVehicle()) {
            return false;
        }

        CVector pos = FindPlayerCoors();
        CVector vel = FindPlayerSpeed();
        vel.z       = 0.f;
        vel.Normalise();
        RotateHeading(vel, 1.04719758f); // PI / 3
        vel.Normalise();
        pos.x = vel.x * 30.f + pos.x;
        pos.y = pos.y + vel.y * 30.f;
        pos.z = FindPlayerCoors().z - 5.5f; // NOTE: The original first adds `vel.z * 30.f`, but then overwrites it

        bool       found = false;
        const auto roofZ = CWorld::FindRoofZFor3DCoord(pos.x, pos.y, pos.z, &found);
        if (found) {
            pos.z = roofZ + 0.5f;
        } else {
            ClampAboveWater(pos);
        }

        if (!IsLineOfSightClearFromPlayer(pos)) {
            return false;
        }

        const CVector toPlayer = FindPlayerCoors() - pos;
        if (toPlayer.Magnitude() > 50.f) {
            return false;
        }
        if (toPlayer.Magnitude() < 3.f) {
            return false;
        }

        return StartFixedCam(pos);
    }
    case 17: {
        if (!FindPlayerVehicle()) {
            return false;
        }

        CVector pos = FindPlayerCoors();
        CVector vel = FindPlayerSpeed();
        vel.z       = 0.f;
        vel.Normalise();
        RotateHeading(vel, 3.31612563f); // 190 degrees
        vel.Normalise();
        pos.x = vel.x * 25.f + pos.x;
        pos.y = pos.y + vel.y * 25.f;
        pos.z = FindPlayerCoors().z - 1.f; // NOTE: The original first adds `vel.z * 25.f`, but then overwrites it

        bool       found = false;
        const auto roofZ = CWorld::FindRoofZFor3DCoord(pos.x, pos.y, pos.z, &found);
        if (found) {
            pos.z = roofZ + 0.5f;
        } else {
            ClampAboveWater(pos);
        }

        if (!IsLineOfSightClearFromPlayer(pos)) {
            return false;
        }

        const CVector toPlayer = FindPlayerCoors() - pos;
        if (toPlayer.Magnitude() > 50.f) {
            const CVector& speed = FindPlayerSpeed();
            if (((toPlayer.y * speed.y) + (speed.z * 0.f)) + (toPlayer.x * speed.x) > 0.f) {
                return false;
            }
        }
        if (toPlayer.Magnitude() < 2.f) {
            return false;
        }

        return StartFixedCam(pos);
    }
    case 18:
    case 19: {
        const bool bIs18 = camSequence == 18;

        CVector pos = FindPlayerCoors();
        if (const auto* const veh = FindPlayerVehicle(); veh && veh->m_nVehicleType == VEHICLE_TYPE_BOAT) {
            pos.z += bIs18 ? 23.f : 4.f;
        } else {
            pos.z -= bIs18 ? 23.f : 1.f;
        }

        CVector vel = FindPlayerSpeed();
        RotateHeading(vel, bIs18 ? 2.53072739f /* 145 degrees */ : 0.488692194f /* 28 degrees */);
        vel.z = 0.f;
        vel.Normalise();

        const float dist = bIs18 ? 15.f : 12.5f;
        pos.x = pos.x + vel.x * dist;
        pos.y = pos.y + vel.y * dist;
        pos.z = vel.z * dist + pos.z;

        // BUG: Compares the ground Z to 1.0 and only then moves the camera to the ground. The `found` result is ignored
        bool       found   = false;
        const auto groundZ = CWorld::FindGroundZFor3DCoord(pos, &found);
        if (groundZ == 1.f) {
            if (pos.z < groundZ) {
                pos.z = groundZ + 0.5f;
            }
        } else {
            ClampAboveWater(pos);
        }

        if (!IsLineOfSightClearFromPlayer(pos)) {
            return false;
        }

        const CVector toPlayer = FindPlayerCoors() - pos;
        if (toPlayer.Magnitude() > (bIs18 ? 57.f : 36.f)) {
            return false;
        }
        if (toPlayer.Magnitude() < (bIs18 ? 1.f : 2.f)) {
            return false;
        }

        return StartFixedCam(pos);
    }
    case 20:
        if (!m_aCams[m_nActiveCam].Process_DW_HeliChaseCam(true)) {
            return false;
        }
        TakeControl(FindPlayerEntity(), MODE_DW_HELI_CHASE, eSwitchType::JUMPCUT, 2);
        return !IsActiveCamUnderWater();
    case 21:
        if (!m_aCams[m_nActiveCam].Process_DW_CamManCam(true)) {
            return false;
        }
        TakeControl(FindPlayerEntity(), MODE_DW_CAM_MAN, eSwitchType::JUMPCUT, 2);
        return !IsActiveCamUnderWater();
    case 22:
        if (!m_aCams[m_nActiveCam].Process_DW_BirdyCam(true)) {
            return false;
        }
        TakeControl(FindPlayerEntity(), MODE_DW_BIRDY, eSwitchType::JUMPCUT, 2);
        return !IsActiveCamUnderWater();
    case 23:
        if (!m_aCams[m_nActiveCam].Process_DW_PlaneSpotterCam(true)) {
            return false;
        }
        TakeControl(FindPlayerEntity(), MODE_DW_PLANE_SPOTTER, eSwitchType::JUMPCUT, 2);
        return !IsActiveCamUnderWater();
    case 24:
    case 25:
        TheCamera.m_bUseNearClipScript = false;
        return false;
    case 26:
        if (!m_aCams[m_nActiveCam].Process_DW_PlaneCam1(true)) {
            return false;
        }
        TakeControl(FindPlayerEntity(), MODE_DW_PLANECAM1, eSwitchType::JUMPCUT, 2);
        return !IsActiveCamUnderWater();
    case 27:
        if (!m_aCams[m_nActiveCam].Process_DW_PlaneCam2(true)) {
            return false;
        }
        TakeControl(FindPlayerEntity(), MODE_DW_PLANECAM2, eSwitchType::JUMPCUT, 2);
        return !IsActiveCamUnderWater();
    case 28:
        if (!m_aCams[m_nActiveCam].Process_DW_PlaneCam3(true)) {
            return false;
        }
        TakeControl(FindPlayerEntity(), MODE_DW_PLANECAM3, eSwitchType::JUMPCUT, 2);
        return !IsActiveCamUnderWater();
    case 29:
        TakeControl(FindPlayerEntity(), MODE_CAM_ON_A_STRING, eSwitchType::JUMPCUT, 2);
        return true;
    default: // 4, 9..14, out of range
        return false;
    }
}

// 0x520190
bool CCamera::CameraColDetAndReact(CVector* source, CVector* target) {
    CEntity* const ignored = CWorld::pIgnoreEntity;
    const CVector  diff    = *source - *target;

    float radius = diff.Magnitude() * gpCamColVars[0] * 0.2939f;

    if (gCurCamColVars > 9 && ignored) {
        float limit;
        if (ignored->GetIsTypeVehicle() && ignored->AsVehicle()->m_nVehicleSubType == VEHICLE_TYPE_AUTOMOBILE) {
            if (ignored->m_nModelIndex != gCamColLastModelIdx) {
                gCamColLowestSphereZ = 100.f;
                if (const auto* const cd = ignored->GetColModel()->m_pColData) {
                    for (auto i = 0u; i < cd->m_nNumSpheres; i++) {
                        const auto& sp = cd->m_pSpheres[i];
                        if (sp.m_vecCenter.z - sp.m_fRadius < gCamColLowestSphereZ) {
                            gCamColLowestSphereZ = sp.m_vecCenter.z - sp.m_fRadius;
                        }
                    }
                }
                gCamColLastModelIdx = ignored->m_nModelIndex;
            }

            if (!ignored->m_matrix) {
                ignored->AllocateMatrix();
                ignored->m_placement.UpdateMatrix(ignored->m_matrix);
            }
            const CMatrix& mat = *ignored->m_matrix;
            const CVector& pos = ignored->GetPosition();
            limit = (((target->z - pos.z) * mat.GetUp().z
                    + (target->y - pos.y) * mat.GetUp().y)
                    + (target->x - pos.x) * mat.GetUp().x) - gCamColLowestSphereZ; // NOTE: addition order as in the original
            if (limit < 0.2f) {
                limit = 0.2f;
            }
        } else {
            const auto& bb = CModelInfo::GetModelInfo(ignored->m_nModelIndex)->GetColModel()->GetBoundingBox();
            const float sz = bb.m_vecMax.z - bb.m_vecMin.z;
            const float hx = (bb.m_vecMax.x - bb.m_vecMin.x) * 0.5f;
            const float hy = (bb.m_vecMax.y - bb.m_vecMin.y) * 0.5f;
            limit = sz * 0.5f;
            if (hy <= hx) {
                if (hy < limit) {
                    limit = hy;
                }
            } else if (hx < limit) {
                limit = hx;
            }
        }

        if (gpCamColVars[1] < limit) {
            if (gpCamColVars[1] < radius) {
                radius = gpCamColVars[1];
            }
        } else if (limit < radius) {
            radius = limit;
        }
    }

    if (gpCamColVars[1] < radius) {
        radius = gpCamColVars[1];
    }
    if (radius < 0.65f) {
        radius = 0.65f;
    }

    float minDist = gpCamColVars[2];
    if (gCurCamColVars < 10) {
        minDist = (gCurCamColVars < 4 ? 0.18f : 0.3f) / (*source - *target).Magnitude();
    }

    bool bIsBike = false;
    if (gCurCamColVars > 9 && ignored && ignored->GetIsTypeVehicle() && ignored->AsVehicle()->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        bIsBike = true;
        minDist = 0.05f;
    }

    gCamColLastRadius = radius;

    CVector dest;
    float   dist = 1.f; // NOTSA: original leaves this uninitialized if `source == target`
    const bool bHit = ConeCastCollisionResolve(*source, *target, dest, radius, minDist, dist);
    if (bHit && dist <= gpCamColVars[3]) {
        RwCameraSetNearClipPlane(Scene.m_pRwCamera, gpCamColVars[4]);
    }

    if (gCurDistForCam <= dist) {
        if (!(gCamColLastSourceInit & 1)) { // MSVC static-init guard
            gCamColLastSourceInit |= 1;
            gCamColLastSource = CVector{ 0.f, 0.f, 0.f };
        }
        if (0.01f * 0.01f < (*source - gCamColLastSource).SquaredMagnitude()) {
            float step = (dist - gCurDistForCam) * (CTimer::GetTimeStep() * gpCamColVars[5]);
            if (step > 0.05f) {
                step = 0.05f;
            }
            gCurDistForCam += step;
        }
        gCamColLastSource = *source;
    } else {
        gCurDistForCam = dist;
    }
    if (gCurDistForCam > 1.f) {
        gCurDistForCam = 1.f;
    }

    *source = diff * gCurDistForCam + *target;

    if (bIsBike && gCurDistForCam < 0.5f) {
        RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.05f);
    }
    return bHit;
}

// CamControl: the mode the camera is going to switch to (decided over the course of `CamControl`)
static auto& gNewCamMode = StaticRef<eCamMode>(0xB70140); // NOTSA name
// CamControl: > 0 if a mode was requested (used if the target is a vehicle, then reset to -1)
static auto& gRequestedCamMode = StaticRef<int32>(0x8CC824); // NOTSA name
// CamControl: set if the cam mode was changed by the player (read at the end of CamControl, set by someone else)
static auto& gCamModeChangedByPlayer = StaticRef<bool>(0xB6EC34); // NOTSA name
// CamControl: special aim cam (`MODE_SPECIAL_FIXED_FOR_SYPHON`): set if the fixed cam position was already set up
static auto& gSpecialAimCamPosSet = StaticRef<bool>(0xB7013D); // NOTSA name
// CamControl: arrest cam
static auto& gWasPlayerArrested  = StaticRef<bool>(0xB7013C);   // NOTSA name
static auto& gLastPlayerPedState = StaticRef<int32>(0xB70138);  // NOTSA name
static auto& gArrestCamMode      = StaticRef<int32>(0xB70134);    // NOTSA name (an eCamMode)
// CamControl: the (3 sets of 5) values for `m_fCarZoomBase` (see `SetZoomValueCamStringScript`)
static auto& gCarZoomBaseValues = StaticRef<float[3][5]>(0x8CC3E0); // NOTSA name
// CamControl: zoom of the ped cam when in a cull zone that closes in the camera
static auto& gPedCloseInZoom = StaticRef<float>(0x8CCF14); // NOTSA name: 0.5
// CamControl: tweakable values
static auto& gGarageCamDistVeh   = StaticRef<float>(0x8CCF1C); // NOTSA name: -10.0
static auto& gGarageCamHeightVeh = StaticRef<float>(0x8CCF18); // NOTSA name: 2.0
static auto& gGarageCamDistPed   = StaticRef<float>(0x8CCF10); // NOTSA name: -10.0
static auto& gGarageCamHeightPed = StaticRef<float>(0x8CCF0C); // NOTSA name: 2.0
static auto& gAimCamDistThreshold            = StaticRef<float>(0x8CCF08); // NOTSA name: 3.0 (when the target is a dead ped)
static auto& gAimCamDistThresholdStay        = StaticRef<float>(0x8CCF04); // NOTSA name: 4.0
static auto& gAimCamTargetMaxAngleDeg        = StaticRef<float>(0x8CC46C); // NOTSA name: 30.0

namespace {
// 0x5404A0 - Select (or D-Pad up) just pressed (`CPad` doesn't have this)
bool CycleCameraModeUpJustDown(const CPad& pad) {
    switch (pad.Mode) {
    case 0:
    case 2:
    case 3: return pad.IsSelectPressed();
    case 1: return pad.IsDPadUpPressed();
    default: return false;
    }
}

// 0x509AE0 - `WellBufferMe` is private to Cam.cpp
void WellBufferMe(float target, float& valueToChange, float& speedSoFar, float topSpeed, float speedStep, bool isAnAngle) {
    plugin::Call<0x509AE0, float, float*, float*, float, float, bool>(target, &valueToChange, &speedSoFar, topSpeed, speedStep, isAnAngle);
}

// Moves `value` towards `target` by (at most) `step`
void StepTowards(float& value, float target, float step) {
    if (target <= value) {
        const auto next = value - step;
        value = next < target ? target : next;
    } else {
        const auto next = step + value;
        value = next > target ? target : next;
    }
}

// 0x72D6D0
CVector GetCenterOfZone(const CZoneDef& zone) {
    return CVector{
        static_cast<float>(zone.m_vec2X + zone.m_vec1X) * 0.5f + static_cast<float>(zone.m_cornerX),
        static_cast<float>(zone.m_vec2Y + zone.m_vec1Y) * 0.5f + static_cast<float>(zone.m_cornerY),
        static_cast<float>(zone.m_minZ + zone.m_maxZ) * 0.5f
    };
}

// The bigger of the sums of the extents (along the 2 vectors) of a stairs zone
int32 GetStairsZoneExtent(const CZoneDef& zone) {
    const int32 sumX = std::abs(int32{ zone.m_vec1X }) + std::abs(int32{ zone.m_vec2X });
    const int32 sumY = std::abs(int32{ zone.m_vec1Y }) + std::abs(int32{ zone.m_vec2Y });
    return sumY < sumX ? sumX : sumY;
}

// CGarage::FindDoorsWithGarage (0x449FF0)
void FindGarageDoors(CGarage* garage, CObject*& door1, CObject*& door2) {
    plugin::CallMethod<0x449FF0, CGarage*, CObject**, CObject**>(garage, &door1, &door2);
}

CVector GetCenterOfGarage(const CGarage& garage) {
    return CVector{
        (garage.m_fRightCoord + garage.m_fLeftCoord) * 0.5f,
        (garage.m_fBackCoord + garage.m_fFrontCoord) * 0.5f,
        0.f
    };
}

bool IsLineOfSightClearForGarageCam(const CVector& origin, const CVector& target) {
    return CWorld::GetIsLineOfSightClear(origin, target, true, false, false, false, false, false, true);
}

// Places the fixed camera `dist` away from the doors of a garage (or from `target` if there are none)
void PlaceFixedCamNearGarageDoors(CCamera& cam, const CVector& refPos, CObject* door1, CObject* door2, float dist, float heightOffset) {
    CVector p1{}, p2{}, right{};
    if (door1) {
        auto* const dummy = door1->m_pDummyObject;
        p1    = dummy->GetPosition();
        right = dummy->GetMatrix().GetRight();
    }
    if (door2) {
        auto* const dummy = door2->m_pDummyObject;
        p2    = dummy->GetPosition();
        right = dummy->GetMatrix().GetRight();
    }

    CVector mid;
    if (door1) {
        mid = door2 ? ((p2 - p1) * 0.5f + p1) : p1;
    } else if (door2) {
        mid = p2;
    } else {
        const CVector& targetPos = cam.m_pTargetEntity->GetPosition();
        mid                      = targetPos;
        right                    = CVector{ targetPos.x - refPos.x, targetPos.y - refPos.y, targetPos.z - refPos.z };
        right.z                  = 0.f;
        right.Normalise();
    }

    CVector pos = mid + right * dist;
    pos.z       = pos.z + heightOffset;
    cam.SetCamPositionForFixedMode(pos, CVector{ 0.f, 0.f, 0.f });
}

// The part of `CamControl` (0x528351..0x528A64) that places the fixed camera for when the player is in a vehicle that is in a garage's camera zone
void PlaceFixedCamForVehicleInGarage(CCamera& cam) {
    CObject *door1{}, *door2{};
    CVector refPos;
    if (cam.m_pToGarageWeAreIn) {
        FindGarageDoors(cam.m_pToGarageWeAreIn, door1, door2);
        refPos = GetCenterOfGarage(*cam.m_pToGarageWeAreIn);
    } else {
        // BUG: The original doesn't set the door pointers in this case (so it reads uninitialized stack memory)
        // NOTSA: The original also computed (and tested the line of sight of) some points here, but never used the result
        refPos = cam.m_pTargetEntity->GetPosition();
    }
    PlaceFixedCamNearGarageDoors(cam, refPos, door1, door2, gGarageCamDistVeh, gGarageCamHeightVeh);
    cam.m_bGarageFixedCamPositionSet = true;
}

// The part of `CamControl` (0x5291EB..0x529B9A) that places the fixed camera for when the player (on foot) is in a garage's camera zone (or in a stairs zone)
void PlaceFixedCamForPedInGarage(CCamera& cam, CAttributeZone* stairsZone, bool bInStairsZone) {
    CEntity* const target = cam.m_pTargetEntity;

    CVector pointA{}, pointB{}, refPos{};
    CObject *door1{}, *door2{};
    int32    mode{ 1 }; // 1: use `pointA`, 2: use `pointB`

    if (!cam.m_pToGarageWeAreIn) {
        if (!bInStairsZone) {
            return;
        }

        pointA = cam.GetActiveCam().m_vecSource;
        if (stairsZone) {
            refPos = GetCenterOfZone(stairsZone->zoneDef);

            const CVector targetPos = target->GetPosition();
            pointB   = targetPos - refPos;
            pointB.z = 0.f;
            pointB.Normalise();

            const int32 extent = GetStairsZoneExtent(stairsZone->zoneDef);
            const float scale  = static_cast<float>(extent) + static_cast<float>(extent);

            // Try to find a point (on either side of the target) that is visible from the target
            CVector candidate = targetPos + scale * pointB;
            if (IsLineOfSightClearForGarageCam(targetPos, candidate)) {
                pointA = candidate;
            } else {
                candidate = targetPos - scale * pointB;
                if (IsLineOfSightClearForGarageCam(targetPos, candidate)) {
                    pointA = candidate;
                }
            }
        }
    } else {
        FindGarageDoors(cam.m_pToGarageWeAreIn, door1, door2);
        if (door1) {
            pointA = CVector{ door1->GetPosition().x, door1->GetPosition().y, 0.f };
        } else if (door2) {
            mode = 2; // NOTE: `pointB` is not initialized in the original here
        } else {
            pointA = CVector{ target->GetPosition().x, target->GetPosition().y, 0.f };
        }
    }

    // 0x529507
    if (cam.m_pToGarageWeAreIn) {
        refPos = GetCenterOfGarage(*cam.m_pToGarageWeAreIn);
    } else if (!stairsZone) {
        refPos = CVector{ target->GetPosition().x, target->GetPosition().y, 0.f };
    }

    CVector dir = (mode == 1 ? pointA : pointB) - refPos;

    bool       found{};
    const auto groundZ = CWorld::FindGroundZFor3DCoord(target->GetPosition(), &found);
    const float camGroundZ = found ? groundZ : target->GetPosition().z - 0.2f;

    dir.z = 0.f;
    if (mode == 1) {
        if (!cam.m_pToGarageWeAreIn && bInStairsZone) {
            dir.Normalise();
            if (stairsZone) {
                const auto scale = static_cast<float>(static_cast<double>(GetStairsZoneExtent(stairsZone->zoneDef)) * 0.7f + 3.75f);
                pointA           = refPos + scale * dir;
            } else {
                pointA = pointA + 3.75f * dir;
            }
        } else {
            dir.Normalise();
            pointA = pointA + 13.f * dir;
        }
    } else {
        dir.Normalise();
        pointA = pointB + 13.f * dir;
    }

    if (cam.m_nPedZoom == 4 && !bInStairsZone) {
        pointA   = refPos;
        pointA.z = (refPos.z + FindPlayerPed()->GetPosition().z) + 2.1f;
        if (auto* const garage = cam.m_pToGarageWeAreIn; garage && pointA.z > garage->m_fRightCoord) { // BUG: Compares a Z coord. to a X one (`m_fRightCoord`)
            pointA.z = garage->m_fRightCoord;
        }
    } else {
        pointA.z = camGroundZ + 3.1f;
    }
    cam.SetCamPositionForFixedMode(pointA, CVector{ 0.f, 0.f, 0.f });
    cam.m_bGarageFixedCamPositionSet = true;

    // 0x52990C - The camera is placed near the doors of the garage
    if (cam.m_pToGarageWeAreIn) {
        CObject *doorA{}, *doorB{};
        FindGarageDoors(cam.m_pToGarageWeAreIn, doorA, doorB);
        PlaceFixedCamNearGarageDoors(cam, GetCenterOfGarage(*cam.m_pToGarageWeAreIn), doorA, doorB, gGarageCamDistPed, gGarageCamHeightPed);
        cam.m_bGarageFixedCamPositionSet = true;
    }
}
} // namespace

// 0x527FA0
void CCamera::CamControl() {
    const auto initialMode = static_cast<int16>(GetActiveCam().m_nMode);

    m_bObbeCinematicPedCamOn                   = false;
    m_bObbeCinematicCarCamOn                   = false;
    m_bUseTransitionBeta                       = false;
    m_bUseSpecialFovTrain                      = false;
    m_bJustCameOutOfGarage                     = false;
    m_bTargetJustCameOffTrain                  = false;
    m_bInATunnelAndABigVehicle                 = false;
    m_bJustJumpedOutOf1stPersonBecauseOfTarget = false;

    bool bJumpCut      = false; // Switch to the new mode immediately (instead of interpolating)
    bool bInStairsZone = false; // Is the player in a "stairs" zone (the camera is placed outside)

    gCamModeChangedByPlayer = false;

    if (!GetActiveCam().m_pCamTargetEntity && !m_pTargetEntity) {
        m_pTargetEntity = CWorld::Players[CWorld::PlayerInFocus].m_pPed;
        CEntity::RegisterReference(m_pTargetEntity);
    }

    // Check whether the cull zone is "close in" every `m_nCheckCullZoneThisNumFrames` frames
    m_nZoneCullFrameNumWereAt++;
    if (static_cast<int32>(m_nZoneCullFrameNumWereAt) > static_cast<int32>(m_nCheckCullZoneThisNumFrames)) {
        m_nZoneCullFrameNumWereAt = 1;
    }
    m_bCullZoneChecksOn = m_nZoneCullFrameNumWereAt == m_nCheckCullZoneThisNumFrames;
    if (m_bCullZoneChecksOn == true) {
        m_bFailedCullZoneTestPreviously = CCullZones::CamCloseInForPlayer();
    }

    if (m_bLookingAtPlayer == true) {
        CPad::GetPad(0)->bCamera = false;
        FindPlayerPed()->SetIsVisible(true);
    }

    if (!CTimer::m_UserPause && !CTimer::m_CodePause && !m_bIdleOn) {
        float zoomOffsetCar = 0.f; // local_a4: target of the buffered close-in height offset of the car cam
        float zoomOffsetPed = 0.f; // local_88: ^ ped cam

        if (m_bTargetJustBeenOnTrain == true && (m_pTargetEntity->GetType() != ENTITY_TYPE_VEHICLE || m_pTargetEntity->AsVehicle()->m_nVehicleType != VEHICLE_TYPE_TRAIN)) {
            Restore();
            m_bTargetJustCameOffTrain       = true;
            m_bTargetJustBeenOnTrain        = false;
            m_bWantsToSwitchWidescreenOff   = m_bWideScreenOn == true;
        }

        const auto targetType = m_pTargetEntity->GetType();
        if (targetType == ENTITY_TYPE_VEHICLE) {
            [&] { // Using a lambda so `return` can be used to skip to the next part
                auto* const veh = m_pTargetEntity->AsVehicle();

                if (gRequestedCamMode > 0) {
                    gNewCamMode        = static_cast<eCamMode>(gRequestedCamMode);
                    gRequestedCamMode  = -1;
                }

                if (veh->m_nVehicleType == VEHICLE_TYPE_TRAIN) {
                    gNewCamMode = MODE_BEHINDCAR;
                    return;
                }

                // Change the zoom
                if (CycleCameraModeUpJustDown(*CPad::GetPad(0)) || CPad::GetPad(0)->sub_5404F0()) {
                    if (CReplay::Mode != MODE_PLAYBACK && !m_bWideScreenOn && !m_bFailedCullZoneTestPreviously
                        && (m_bLookingAtPlayer == true || m_nWhoIsInControlOfTheCamera == 2)
                        && !CGameLogic::IsCoopGameGoingOn()
                    ) {
                        const bool bZoomIn = CycleCameraModeUpJustDown(*CPad::GetPad(0));
                        auto       zoom    = static_cast<int32>(m_nCarZoom) + (bZoomIn ? -1 : +1);
                        m_nCarZoom         = zoom;
                        if (zoom > 5) {
                            m_nCarZoom = 0;
                        } else if (zoom < 0) {
                            m_nCarZoom = 5;
                        }

                        if (m_nCarZoom == 4) {
                            m_nCarZoom = CycleCameraModeUpJustDown(*CPad::GetPad(0)) ? 3 : 5;
                        } else if (m_nCarZoom == 0 && m_bDisableFirstPersonInCar) {
                            m_nCarZoom = CycleCameraModeUpJustDown(*CPad::GetPad(0)) ? 5 : 1;
                        }
                    }
                }

                if (m_bFailedCullZoneTestPreviously && m_nCarZoom != 4 && m_nCarZoom != 0) {
                    gNewCamMode = MODE_CAM_ON_A_STRING;
                }

                // Setup the mode depending on the vehicle's type
                auto vehType     = veh->m_nVehicleType;
                bool bCheckGarage{};
                if (vehType == VEHICLE_TYPE_BOAT) {
                    if (veh->m_nModelIndex == MODEL_SKIMMER) {
                        vehType      = VEHICLE_TYPE_AUTOMOBILE;
                        bCheckGarage = true;
                    } else {
                        gNewCamMode = MODE_BEHINDBOAT;
                    }
                } else if (vehType == VEHICLE_TYPE_AUTOMOBILE || vehType == VEHICLE_TYPE_BIKE) {
                    bCheckGarage = true;
                }

                if (bCheckGarage) {
                    CAttributeZone* stairsZone{};
                    if (vehType == VEHICLE_TYPE_BIKE && CCullZones::CamStairsForPlayer()) {
                        stairsZone = CCullZones::FindZoneWithStairsAttributeForPlayer();
                        if (stairsZone) {
                            bInStairsZone = true;
                        }
                    }

                    if (CGarages::IsPointInAGarageCameraZone(veh->GetPosition()) || bInStairsZone) {
                        if (((!m_bGarageFixedCamPositionSet && m_bLookingAtPlayer == true) || m_nWhoIsInControlOfTheCamera == 2)
                            && (m_pToGarageWeAreIn || stairsZone)
                        ) {
                            PlaceFixedCamForVehicleInGarage(*this);
                        }

                        if ((CGarages::CameraShouldBeOutside() || bInStairsZone)
                            && m_bGarageFixedCamPositionSet == true
                            && (m_bLookingAtPlayer == true || m_nWhoIsInControlOfTheCamera == 2)
                        ) {
                            if (m_pToGarageWeAreIn || stairsZone) {
                                gNewCamMode          = MODE_FIXED;
                                m_bPlayerIsInGarage  = true;
                            }
                        } else {
                            if (m_bPlayerIsInGarage) {
                                m_bJustCameOutOfGarage = true;
                                m_bPlayerIsInGarage    = false;
                            }
                            gNewCamMode = MODE_CAM_ON_A_STRING;
                        }
                    } else {
                        if (m_bPlayerIsInGarage) {
                            m_bJustCameOutOfGarage = true;
                            m_bPlayerIsInGarage    = false;
                        }
                        m_bGarageFixedCamPositionSet = false;
                        gNewCamMode                  = MODE_CAM_ON_A_STRING;
                    }
                }

                // Zoom
                int32 arrPos{};
                GetArrPosForVehicleType(static_cast<eVehicleType>(veh->GetVehicleAppearance()), arrPos);

                const auto carZoom = static_cast<int32>(m_nCarZoom);
                if (carZoom == 0 && !m_bPlayerIsInGarage) {
                    m_fCarZoomBase = 0.f;
                    gNewCamMode    = MODE_1STPERSON;
                } else if (carZoom == 1) {
                    m_fCarZoomBase = gCarZoomBaseValues[0][arrPos];
                } else if (carZoom == 2) {
                    m_fCarZoomBase = gCarZoomBaseValues[1][arrPos];
                } else if (carZoom == 3) {
                    m_fCarZoomBase = gCarZoomBaseValues[2][arrPos];
                }

                if (carZoom == 4 && !m_bPlayerIsInGarage) {
                    m_fCarZoomBase = 1.f;
                }

                if (m_fCarZoomTotal == 0.f) {
                    m_fCarZoomTotal = m_fCarZoomBase;
                }

                const float zoomStep = CTimer::GetTimeStep() * 0.12f;
                if (m_bUseScriptZoomValueCar == true) {
                    StepTowards(m_fCarZoomSmoothed, m_fCarZoomValueScript, zoomStep);
                } else if (m_bFailedCullZoneTestPreviously) {
                    zoomOffsetCar = 0.65f;
                    StepTowards(m_fCarZoomSmoothed, -0.65f, zoomStep);
                } else {
                    StepTowards(m_fCarZoomSmoothed, m_fCarZoomBase, zoomStep);
                    if (carZoom == 3 && m_fCarZoomBase == 0.f) {
                        m_fCarZoomSmoothed = m_fCarZoomBase;
                    }
                }

                auto& cam = GetActiveCam();
                WellBufferMe(zoomOffsetCar, cam.m_fCloseInCarHeightOffset, cam.m_fCloseInCarHeightOffsetSpeed, 0.1f, 0.25f, false);
            }();
        } else if (targetType == ENTITY_TYPE_PED) {
            [&] { // Using a lambda so `return` can be used to skip to the next part
                CPad* const pad = CPad::GetPad(0);

                // Change the zoom
                if (CycleCameraModeUpJustDown(*pad) || pad->sub_5404F0()) {
                    if (CReplay::Mode != MODE_PLAYBACK && !m_bWideScreenOn && !m_bFailedCullZoneTestPreviously && !m_bFirstPersonBeingUsed
                        && (m_bLookingAtPlayer == true || m_nWhoIsInControlOfTheCamera == 2)
                        && !CGameLogic::IsCoopGameGoingOn()
                    ) {
                        const auto zoom = static_cast<int32>(m_nPedZoom) + (CycleCameraModeUpJustDown(*pad) ? -1 : +1);
                        m_nPedZoom      = zoom;
                        if (zoom > 3) {
                            m_nPedZoom = 1;
                        } else if (zoom < 1) {
                            m_nPedZoom = 3;
                        }
                    }
                }

                gNewCamMode = MODE_FOLLOWPED;

                // First person
                bool bResetFirstPerson = true;
                if ((m_bLookingAtPlayer == true || m_bEnable1rstPersonCamCntrlsScript)
                    && m_pTargetEntity->GetIsTypePed()
                    && (!m_bWideScreenOn || m_bEnable1rstPersonCamCntrlsScript)
                    && !m_aCams[0].Using3rdPersonMouseCam()
                ) {
                    if (!FindPlayerPed()->GetIntelligence()->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_ATTACK)) {
                        // NOTSA: The result of these 2 is discarded (they only read the mouse input)
                        if (plugin::CallMethodAndReturn<int16, 0x540E80, CPad*>(pad) == 0) { // LookAroundLeftRight
                            plugin::CallMethodAndReturn<int16, 0x540F80, CPad*>(pad);          // LookAroundUpDown
                        }
                    }

                    if (!m_bFirstPersonBeingUsed) {
                        bResetFirstPerson = false;
                    } else if (pad->GetPedWalkLeftRight() == 0
                        && pad->GetPedWalkUpDown() == 0
                        && pad->NewState.ButtonSquare == 0
                        && pad->NewState.ButtonTriangle == 0
                        && pad->NewState.ButtonCross == 0
                        && pad->NewState.ButtonCircle == 0
                        && pad->NewState.Select == 0
                    ) {
                        const auto timeSinceInput = CTimer::GetTimeInMS() - m_nFirstPersonCamLastInputTime;
                        if (static_cast<double>(timeSinceInput) <= 2850.0) {
                            if (!pad->GetEnterTargeting()) {
                                bResetFirstPerson = false;
                            } else {
                                m_bJustJumpedOutOf1stPersonBecauseOfTarget = true;
                            }
                        }
                    }
                }
                if (bResetFirstPerson) {
                    m_bFirstPersonBeingUsed = false;
                }

                if (!FindPlayerPed()->IsPedInControl() || FindPlayerPed()->GetPlayerData()->m_fMoveBlendRatio > 0.f) {
                    m_bFirstPersonBeingUsed = false;
                }
                if (m_bFirstPersonBeingUsed) {
                    gNewCamMode = MODE_1STPERSON;
                    CPad::GetPad(0)->bCamera = true;
                }

                // Zoom
                const auto& activeCam = GetActiveCam();
                m_fPedZoomBase = m_nPedZoom == 1 ? activeCam.m_fTargetZoomGroundOne
                               : m_nPedZoom == 3 ? activeCam.m_fTargetZoomGroundThree
                                                 : activeCam.m_fTargetZoomGroundTwo;

                const float zoomStep = CTimer::GetTimeStep() * 0.12f;
                if (m_bUseScriptZoomValuePed == true) {
                    StepTowards(m_fPedZoomSmoothed, m_fPedZoomValueScript, zoomStep);
                } else if (m_bFailedCullZoneTestPreviously) {
                    zoomOffsetPed = 0.7f;
                    StepTowards(m_fPedZoomSmoothed, gPedCloseInZoom, zoomStep);
                } else {
                    StepTowards(m_fPedZoomSmoothed, m_fPedZoomBase, zoomStep);
                    if (m_nPedZoom == 3 && m_fPedZoomBase == 0.f) {
                        m_fPedZoomSmoothed = m_fPedZoomBase;
                    }
                }

                auto& cam = GetActiveCam();
                WellBufferMe(zoomOffsetPed, cam.m_fCloseInPedHeightOffset, cam.m_fCloseInPedHeightOffsetSpeed, 0.1f, 0.025f, false);

                // Garage / stairs
                CAttributeZone* stairsZone{};
                if (CCullZones::CamStairsForPlayer()) {
                    stairsZone = CCullZones::FindZoneWithStairsAttributeForPlayer();
                    if (stairsZone) {
                        bInStairsZone = true;
                    }
                }

                if (CGarages::IsPointInAGarageCameraZone(m_pTargetEntity->GetPosition()) || bInStairsZone) {
                    if (!m_bGarageFixedCamPositionSet && m_bLookingAtPlayer == true) {
                        PlaceFixedCamForPedInGarage(*this, stairsZone, bInStairsZone);
                    }

                    if ((CGarages::CameraShouldBeOutside() || bInStairsZone)
                        && m_bLookingAtPlayer == true
                        && m_bGarageFixedCamPositionSet == true
                    ) {
                        if (m_pToGarageWeAreIn || bInStairsZone) {
                            gNewCamMode         = MODE_FIXED;
                            m_bPlayerIsInGarage = true;
                        }
                    } else {
                        if (m_bPlayerIsInGarage) {
                            m_bJustCameOutOfGarage = true;
                            m_bPlayerIsInGarage    = false;
                        }
                        gNewCamMode = MODE_FOLLOWPED;
                    }
                } else {
                    if (m_bPlayerIsInGarage) {
                        m_bJustCameOutOfGarage = true;
                        m_bPlayerIsInGarage    = false;
                    }
                    m_bGarageFixedCamPositionSet = false;
                }

                // Weapon mode
                if (!CPad::GetPad(0)->GetTarget()) {
                    const auto weaponMode = static_cast<eCamMode>(m_PlayerWeaponMode.m_nMode);
                    if (weaponMode != MODE_NONE
                        && weaponMode != MODE_HELICANNON_1STPERSON
                        && weaponMode != MODE_AIMWEAPON_FROMCAR
                        && weaponMode != MODE_AIMWEAPON_ATTACHED
                        && (weaponMode != MODE_CAMERA || !FindPlayerPed()->m_pAttachedTo)
                    ) {
                        ClearPlayerWeaponMode();
                    }
                }

                if (m_PlayerMode.m_nMode != MODE_NONE) {
                    gNewCamMode = static_cast<eCamMode>(m_PlayerMode.m_nMode);
                }

                const auto weaponMode = static_cast<eCamMode>(m_PlayerWeaponMode.m_nMode);
                if (weaponMode == MODE_NONE || bInStairsZone) {
                    return;
                }

                const bool bIsRangedWeaponMode = notsa::contains({ MODE_SNIPER, MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_HS, MODE_M16_1STPERSON, MODE_HELICANNON_1STPERSON, MODE_CAMERA }, weaponMode);
                if (bIsRangedWeaponMode || GetActiveCam().GetWeaponFirstPersonOn()) {
                    if (CWorld::Players[CWorld::PlayerInFocus].m_pPed->m_nPedState == PEDSTATE_SEEK_CAR
                        && gNewCamMode != MODE_TOP_DOWN_PED
                        && !GetActiveCam().GetWeaponFirstPersonOn()
                    ) {
                        gNewCamMode = MODE_FOLLOWPED;
                    } else {
                        gNewCamMode = weaponMode;
                    }
                    return;
                }

                if (gNewCamMode == MODE_TOP_DOWN_PED) {
                    return;
                }

                auto* const player = FindPlayerPed();
                if (!player->m_pTargetedObject && !player->GetPlayerData()->m_bFreeAiming) {
                    return;
                }

                const CVector targetPos = m_pTargetEntity->GetPosition();
                const CVector toAimTarget = m_vecAimingTargetCoors - targetPos;
                const CVector toCam       = GetActiveCam().m_vecSource - targetPos;
                // NOTSA: The original also computed `GetATanOfXY(toAimTarget)`, but never used it

                // Is the targeted ped dead? (then the camera is placed near it)
                bool  bTargetIsDeadPed = false;
                float deadPedDistThreshold = 0.f; // local_a4
                if (const auto* const targeted = player->m_pTargetedObject; targeted && targeted->GetIsTypePed()) {
                    if (targeted->AsPed()->m_nPedState == PEDSTATE_DEAD || targeted->AsPed()->m_nPedState == PEDSTATE_DIE) {
                        bTargetIsDeadPed     = true;
                        deadPedDistThreshold = gAimCamDistThreshold;
                    }
                }

                const float distToAimTarget = std::sqrt(toAimTarget.y * toAimTarget.y + toAimTarget.x * toAimTarget.x);
                const float angleToCam      = CGeneral::GetATanOfXY(toCam.x, toCam.y);

                gNewCamMode = weaponMode;
                float camDist{}; // local_50
                if (weaponMode == MODE_AIMWEAPON && bTargetIsDeadPed) {
                    if (player->m_pTargetedObject && (!m_bTransitionState || GetActiveCam().m_nMode == MODE_SPECIAL_FIXED_FOR_SYPHON)) {
                        float threshold = deadPedDistThreshold;
                        if (GetActiveCam().m_nMode == MODE_SPECIAL_FIXED_FOR_SYPHON && player->m_pTargetedObject->GetIsTypePed()) {
                            threshold = gAimCamDistThresholdStay;
                        }
                        if (distToAimTarget < threshold) {
                            camDist     = 5.6f;
                            gNewCamMode = MODE_SPECIAL_FIXED_FOR_SYPHON;
                        }
                    }
                }

                if (gNewCamMode != MODE_SPECIAL_FIXED_FOR_SYPHON) {
                    gSpecialAimCamPosSet = false;
                    return;
                }

                if (!gSpecialAimCamPosSet) {
                    CVector camPos{
                        static_cast<float>(std::cos(static_cast<double>(angleToCam)) * static_cast<double>(camDist) + static_cast<double>(targetPos.x)),
                        static_cast<float>(std::sin(static_cast<double>(angleToCam)) * static_cast<double>(camDist) + static_cast<double>(targetPos.y)),
                        targetPos.z + 1.15f
                    };

                    CColPoint colPoint;
                    CEntity*  hitEntity{};
                    if (CWorld::ProcessLineOfSight(targetPos, camPos, colPoint, hitEntity, true, false, false, true, false, true, true, false)) {
                        SetCamPositionForFixedMode(colPoint.m_vecPoint, CVector{ 0.f, 0.f, 0.f });
                    } else {
                        SetCamPositionForFixedMode(camPos, CVector{ 0.f, 0.f, 0.f });
                    }
                    gSpecialAimCamPosSet = true;
                }
            }();
        }
    }

    // 0x52A002 - Two player mode
    if (m_bCooperativeCamMode && CWorld::Players[0].m_pPed && CWorld::Players[1].m_pPed) {
        auto* const ped1 = CWorld::Players[0].m_pPed;
        auto* const ped2 = CWorld::Players[1].m_pPed;
        if (!ped1->bInVehicle || !ped2->bInVehicle || !ped1->m_pVehicle || !ped2->m_pVehicle) {
            gNewCamMode = m_nModeForTwoPlayersNotBothInCar;
        } else {
            m_pTargetEntity = ped1->m_pVehicle; // NOTE: No reference registered
            if (ped1->m_pVehicle == ped2->m_pVehicle) {
                gNewCamMode = m_bAllowShootingWith2PlayersInCar
                    ? m_nModeForTwoPlayersSameCarShootingAllowed
                    : m_nModeForTwoPlayersSameCarShootingNotAllowed;
            } else {
                gNewCamMode = m_nModeForTwoPlayersSeparateCars;
            }
        }
    }

    // Arrest cam
    const auto playerState = CWorld::Players[CWorld::PlayerInFocus].m_pPed->m_nPedState;
    bool bPlayerLeftArrestState = false; // local_b1 in the decompilation
    if (playerState == PEDSTATE_ARRESTED) {
        gWasPlayerArrested = true;
    } else if (gWasPlayerArrested) {
        bPlayerLeftArrestState = true;
        gWasPlayerArrested     = false;
    }

    bool bStartArrestCam = false;
    if (gLastPlayerPedState != PEDSTATE_ARRESTED && playerState == PEDSTATE_ARRESTED) {
        bStartArrestCam = m_nCarZoom != 0 || m_pTargetEntity->GetType() != ENTITY_TYPE_VEHICLE;
    }
    gLastPlayerPedState = playerState;
    if (bStartArrestCam) {
        gNewCamMode                    = MODE_ARRESTCAM_ONE;
        gArrestCamMode                 = MODE_ARRESTCAM_ONE;
        GetActiveCam().m_bResetStatics = true;
    } else if (playerState == PEDSTATE_ARRESTED) {
        gNewCamMode = static_cast<eCamMode>(gArrestCamMode);
    }

    // Dead cam
    if (CWorld::Players[CWorld::PlayerInFocus].m_pPed->m_nPedState == PEDSTATE_DEAD) {
        m_bObbeCinematicCarCamOn = false;

        const auto activeMode = GetActiveCam().m_nMode;
        if (activeMode == MODE_PED_DEAD_BABY) {
            gNewCamMode = MODE_PED_DEAD_BABY;
        } else if (activeMode == MODE_ARRESTCAM_ONE) {
            gNewCamMode = activeMode;
        } else {
            // Is any ped nearby arresting the player?
            bool bBeingArrested = false;
            if (m_pTargetEntity->GetIsTypePed()) {
                auto* const targetPed = m_pTargetEntity->AsPed();
                for (auto i = 0; i < 16; i++) {
                    auto* const entity = targetPed->GetIntelligence()->GetPedEntity(i);
                    if (!entity) {
                        continue;
                    }

                    const auto* const task = static_cast<CTaskSimpleArrestPed*>(entity->AsPed()->GetIntelligence()->GetTaskManager().FindActiveTaskByType(TASK_SIMPLE_ARREST_PED));
                    if (!task || task->m_Ped != FindPlayerPed()) {
                        continue;
                    }

                    if ((entity->GetPosition() - targetPed->GetPosition()).Magnitude() < 4.f) {
                        bBeingArrested = true;
                        break;
                    }
                }
            }
            GetActiveCam().m_bResetStatics = true;
            gNewCamMode                    = bBeingArrested ? MODE_ARRESTCAM_ONE : MODE_PED_DEAD_BABY;
        }
    }

    // Copies the "fixed mode" data to the active camera
    const auto CopyFixedModeDataToActiveCam = [this](bool bLookingAtVector) {
        auto& cam = GetActiveCam();
        cam.m_vecCamFixedModeVector = m_vecFixedModeVector;
        CEntity::ChangeEntityReference(cam.m_pCamTargetEntity, m_pTargetEntity);
        cam.m_vecCamFixedModeSource   = m_vecFixedModeSource;
        cam.m_vecCamFixedModeUpOffSet = m_vecFixedModeUpOffSet;
        cam.m_bCamLookingAtVector     = bLookingAtVector;
        cam.m_vecLastAboveWaterCamPosition = m_aCams[(m_nActiveCam + 1) % 2].m_vecLastAboveWaterCamPosition;
    };

    // The cam modes at which the camera isn't put directly behind the player
    const auto SetCamDirectlyBehindPlayerIfNeeded = [this](std::initializer_list<eCamMode> exceptModes) {
        if (!notsa::contains(exceptModes, gNewCamMode) && !m_bUseMouse3rdPerson) {
            SetCameraDirectlyBehindForFollowPed_CamOnAString();
        }
    };

    if (m_bRestoreByJumpCut == true) {
        SetCamDirectlyBehindPlayerIfNeeded({
            MODE_FOLLOWPED, MODE_M16_1STPERSON, MODE_SNIPER, MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_HS, MODE_CAMERA,
            MODE_SYPHON, MODE_SYPHON_CRIM_IN_FRONT, MODE_SPECIAL_FIXED_FOR_SYPHON, MODE_CAM_ON_A_STRING, MODE_BEHINDCAR
        });

        GetActiveCam().m_nMode = m_nModeToGoTo;
        gNewCamMode            = m_nModeToGoTo;
        m_bJust_Switched       = true;
        GetActiveCam().m_bResetStatics = true;
        CopyFixedModeDataToActiveCam(false);
        m_bRestoreByJumpCut = false;
        GetActiveCam().m_bResetStatics = true;
        m_fCarZoomSmoothed = m_fCarZoomBase;
        m_fPedZoomSmoothed = m_fPedZoomBase;
        m_bTransitionState    = false;
        m_bDoingSpecialInterp = false;
    }

    if (gbModelViewer) {
        gNewCamMode = MODE_MODELVIEW;
    }

    // Is the cinematic cam allowed?
    bool bCinematicCamAllowed = true; // [esp+0x12]
    if (m_pTargetEntity) {
        if (m_pTargetEntity->GetIsTypeVehicle()) {
            if (m_nCarZoom == 5) {
                m_bObbeCinematicCarCamOn = true;
            }
        } else if (m_nPedZoom == 5) {
            m_bObbeCinematicPedCamOn = true;
        }
    }

    if (auto* const playerVeh = FindPlayerVehicle(); playerVeh && playerVeh->m_nVehicleType == VEHICLE_TYPE_TRAIN) {
        m_bObbeCinematicCarCamOn = true;
    }

    if (m_pTargetEntity && m_pTargetEntity->GetIsTypeVehicle()) {
        if (auto* const player = FindPlayerPed(); player && notsa::contains({ PEDSTATE_ARRESTED, PEDSTATE_DEAD }, player->m_nPedState)) {
            m_bObbeCinematicPedCamOn = false;
            bCinematicCamAllowed     = false;
            if (player->m_nPedState == PEDSTATE_ARRESTED) {
                gNewCamMode = MODE_ARRESTCAM_ONE;
            } else if (player->m_nPedState == PEDSTATE_DEAD) {
                gNewCamMode = MODE_PED_DEAD_BABY;
            }
        }
    }

    if (m_bTargetJustBeenOnTrain == true
        || notsa::contains({
            MODE_PED_DEAD_BABY, MODE_PLAYER_FALLEN_WATER, MODE_SYPHON_CRIM_IN_FRONT, MODE_SYPHON, MODE_SNIPER, MODE_SPECIAL_FIXED_FOR_SYPHON,
            MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_HS, MODE_ARRESTCAM_ONE, MODE_ARRESTCAM_TWO, MODE_M16_1STPERSON, MODE_FIGHT_CAM,
            MODE_SNIPER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT_HS, MODE_M16_1STPERSON_RUNABOUT, MODE_FIGHT_CAM_RUNABOUT,
            MODE_1STPERSON_RUNABOUT, MODE_HELICANNON_1STPERSON, MODE_CAMERA
        }, gNewCamMode)
        || m_nWhoIsInControlOfTheCamera == 1
        || m_bJustCameOutOfGarage
        || m_bPlayerIsInGarage
        || GetActiveCam().m_nMode == MODE_PED_DEAD_BABY
    ) {
        bCinematicCamAllowed = false;
    }

    if (m_bCinemaCamera) {
        m_bObbeCinematicCarCamOn = true;
        bCinematicCamAllowed     = true;
    }

    if (!(m_bObbeCinematicPedCamOn && bCinematicCamAllowed)) {
        if (m_bObbeCinematicCarCamOn && bCinematicCamAllowed) {
            CPostEffects::m_bSpeedFXUserFlagCurrentFrame = false;
            if (m_pTargetEntity->GetIsTypeVehicle()) {
                // NOTE: These functions (ProcessObbeCinemaCamera*) are stubs in this class, so call the originals
                const auto vehSubType = m_pTargetEntity->AsVehicle()->m_nVehicleSubType;
                if (vehSubType == VEHICLE_TYPE_PLANE) {
                    plugin::CallMethod<0x526C80, CCamera*>(this); // ProcessObbeCinemaCameraPlane
                } else if (m_pTargetEntity->AsVehicle()->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI) {
                    plugin::CallMethod<0x526AE0, CCamera*>(this); // ProcessObbeCinemaCameraHeli
                } else if (vehSubType == VEHICLE_TYPE_BOAT) {
                    plugin::CallMethod<0x526E20, CCamera*>(this); // ProcessObbeCinemaCameraBoat
                } else if (vehSubType == VEHICLE_TYPE_TRAIN) {
                    plugin::CallMethod<0x526950, CCamera*>(this); // ProcessObbeCinemaCameraTrain
                } else {
                    plugin::CallMethod<0x5267C0, CCamera*>(this); // ProcessObbeCinemaCameraCar
                }
            }
        } else {
            if (m_bPlayerIsInGarage && m_bObbeCinematicCarCamOn) {
                bJumpCut = true;
            }
            bCinematicCamAllowed                = false;
            bDidWeProcessAnyCinemaCam           = false;
        }
    }

    // Decides whether (and how) to switch to the new cam mode
    [&] { // Using a lambda so `return` can be used to jump to the end of this section
        if (m_bLookingAtPlayer == true) {
            bool bSkipAimingAndTwoPlayerChecks = false; // Skip right to the 2 player modes check at 0x52A994
            if (notsa::contains({ MODE_TOPDOWN, MODE_1STPERSON, MODE_TOP_DOWN_PED }, gNewCamMode)) {
                bJumpCut = true;
            } else if (notsa::contains({ MODE_CAM_ON_A_STRING, MODE_BEHINDBOAT }, gNewCamMode)) {
                if (notsa::contains({ MODE_TOPDOWN, MODE_1STPERSON, MODE_TOP_DOWN_PED }, GetActiveCam().m_nMode)) {
                    bJumpCut = true;
                }
            } else if (gNewCamMode == MODE_FIXED) {
                if (GetActiveCam().m_nMode == MODE_TOPDOWN) {
                    bJumpCut = true;
                }
                bSkipAimingAndTwoPlayerChecks = true;
            }

            bool bSkipTwoPlayerChecks = false;
            if (!bSkipAimingAndTwoPlayerChecks) {
                bool bSkipTwoPlayerEnterCheck = false; // Skip the check below, and go right to 0x52A994
                if (notsa::contains({ MODE_AIMWEAPON, MODE_AIMWEAPON_FROMCAR, MODE_AIMWEAPON_ATTACHED }, gNewCamMode) && m_pTargetEntity && m_pTargetEntity->GetIsTypePed()) {
                    auto* const targetPed   = m_pTargetEntity->AsPed();
                    const bool  bHasJetpack = targetPed->GetIntelligence()->GetTaskJetPack() != nullptr;
                    if (gNewCamMode == MODE_AIMWEAPON && GetActiveCam().m_nMode == MODE_FOLLOWPED && !bHasJetpack) {
                        // Switch immediately if the camera is too far away from where it's supposed to be (heading, or distance)
                        constexpr double PI      = static_cast<double>(std::numbers::pi_v<float>);
                        constexpr double HALF_PI = static_cast<double>(std::numbers::pi_v<float> / 2.f);
                        constexpr double TWO_PI  = static_cast<double>(std::numbers::pi_v<float> * 2.f);

                        double aimHeading;
                        if (auto* const targeted = targetPed->m_pTargetedObject) {
                            const CVector dir = targeted->GetPosition() - targetPed->GetPosition();
                            aimHeading        = std::atan2(-static_cast<double>(dir.x), static_cast<double>(dir.y));
                        } else {
                            aimHeading = targetPed->GetHeading();
                        }
                        aimHeading -= HALF_PI;

                        const double camHeading = GetActiveCam().m_fHorizontalAngle;
                        if (aimHeading > camHeading + PI) {
                            aimHeading -= TWO_PI;
                        } else if (aimHeading < camHeading - PI) {
                            aimHeading += TWO_PI;
                        }

                        const double maxHeadingDiff = static_cast<double>(gAimCamTargetMaxAngleDeg) * static_cast<double>(0.0174532924f);
                        const double maxDistance    = (static_cast<double>(m_fPedZoomSmoothed) + 2.0) * static_cast<double>(1.5f);
                        if (maxHeadingDiff < std::abs(aimHeading - camHeading)
                            || maxDistance < static_cast<double>((targetPed->GetPosition() - m_mCameraMatrix.GetPosition()).Magnitude())
                        ) {
                            bJumpCut = true;
                        }
                        if (m_bUseMouse3rdPerson) {
                            bJumpCut = false;
                        }
                        bSkipTwoPlayerEnterCheck = true;
                    } else {
                        bJumpCut = true;
                    }
                }

                if (!bSkipTwoPlayerEnterCheck) {
                    if (gNewCamMode == MODE_TWOPLAYER) {
                        if (GetActiveCam().m_nMode != MODE_TWOPLAYER_IN_CAR_AND_SHOOTING) {
                            bJumpCut             = true;
                            bSkipTwoPlayerChecks = true;
                        }
                    } else if (gNewCamMode == MODE_TWOPLAYER_IN_CAR_AND_SHOOTING) {
                        if (GetActiveCam().m_nMode != MODE_TWOPLAYER) {
                            bJumpCut             = true;
                            bSkipTwoPlayerChecks = true;
                        }
                    }
                }
            }

            if (!bSkipTwoPlayerChecks) {
                // Switch immediately if going out of a 2 player mode
                const auto activeMode = GetActiveCam().m_nMode;
                if ((activeMode == MODE_TWOPLAYER && gNewCamMode != MODE_TWOPLAYER_IN_CAR_AND_SHOOTING)
                    || (activeMode == MODE_TWOPLAYER_IN_CAR_AND_SHOOTING && gNewCamMode != MODE_TWOPLAYER)
                ) {
                    bJumpCut = true;
                }
            }

            // 0x52A9CA - Decide whether to switch immediately (depending on the current and new modes)
            const auto activeMode = GetActiveCam().m_nMode;
            if (notsa::contains({ MODE_TOPDOWN, MODE_TOP_DOWN_PED }, gNewCamMode)) {
                // Going between these 2 is smooth. Going from the dead cam is not.
                if (activeMode == (gNewCamMode == MODE_TOPDOWN ? MODE_TOP_DOWN_PED : MODE_TOPDOWN) || activeMode == MODE_PED_DEAD_BABY) {
                    bJumpCut = activeMode == MODE_PED_DEAD_BABY;
                }
            } else if (m_pTargetEntity->GetIsTypePed() && notsa::contains({
                MODE_1STPERSON, MODE_SNIPER, MODE_M16_1STPERSON, MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_HS, MODE_SNIPER_RUNABOUT,
                MODE_ROCKETLAUNCHER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT_HS, MODE_M16_1STPERSON_RUNABOUT, MODE_FIGHT_CAM_RUNABOUT,
                MODE_1STPERSON_RUNABOUT, MODE_HELICANNON_1STPERSON, MODE_ARRESTCAM_ONE, MODE_ARRESTCAM_TWO, MODE_CAMERA
            }, gNewCamMode)) {
                bJumpCut = true;
            } else if (gNewCamMode == MODE_FIXED) {
                if (m_bPlayerIsInGarage == true) {
                    if (notsa::contains({
                        MODE_SNIPER, MODE_HELICANNON_1STPERSON, MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_HS, MODE_M16_1STPERSON, MODE_TOP_DOWN_PED
                    }, activeMode)
                        || bInStairsZone
                        || notsa::contains({
                            MODE_1STPERSON, MODE_SNIPER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT_HS,
                            MODE_M16_1STPERSON_RUNABOUT, MODE_FIGHT_CAM_RUNABOUT, MODE_1STPERSON_RUNABOUT, MODE_CAMERA
                        }, activeMode)
                    ) {
                        if (m_pTargetEntity && m_pTargetEntity->GetIsTypeVehicle()) {
                            bJumpCut = true;
                        }
                    }
                } else if (activeMode == MODE_PED_DEAD_BABY) {
                    bJumpCut = true;
                }
            } else if (gNewCamMode == MODE_FOLLOWPED) {
                bool bSwitchToFollowPedImmediately = false; // bl
                if (activeMode == MODE_AIMWEAPON) {
                    auto* const targetPed = m_pTargetEntity->AsPed();
                    if (targetPed->CanWeRunAndFireWithWeapon() && !targetPed->bIsDucking) {
                        // Switch immediately if the camera isn't looking in the direction the ped is
                        auto&        cam            = GetActiveCam();
                        const double PI             = static_cast<double>(std::numbers::pi_v<float>);
                        const double HALF_PI        = static_cast<double>(std::numbers::pi_v<float> / 2.f);
                        const double TWO_PI         = static_cast<double>(std::numbers::pi_v<float> * 2.f);
                        double       heading        = static_cast<double>(targetPed->GetHeading()) - HALF_PI;
                        const double camHeading     = cam.m_fHorizontalAngle;
                        if (heading > camHeading + PI) {
                            heading -= TWO_PI;
                        } else if (heading < camHeading - PI) {
                            heading += TWO_PI;
                        }

                        const double maxHeadingDiff = static_cast<double>(gAimCamTargetMaxAngleDeg) * static_cast<double>(0.0174532924f);
                        if (maxHeadingDiff < std::abs(heading - camHeading) || !targetPed->bIsStanding) {
                            bSwitchToFollowPedImmediately = true;
                        }
                        if (m_bUseMouse3rdPerson) {
                            bSwitchToFollowPedImmediately = false;
                            m_bJustCameOutOfGarage        = true;
                        }
                    }
                }

                const auto activeMode2 = GetActiveCam().m_nMode;
                if (notsa::contains({
                    MODE_1STPERSON, MODE_SNIPER, MODE_M16_1STPERSON, MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_HS, MODE_PED_DEAD_BABY,
                    MODE_ARRESTCAM_ONE, MODE_ARRESTCAM_TWO, MODE_PILLOWS_PAPS, MODE_SNIPER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT,
                    MODE_ROCKETLAUNCHER_RUNABOUT_HS, MODE_M16_1STPERSON_RUNABOUT, MODE_FIGHT_CAM_RUNABOUT, MODE_1STPERSON_RUNABOUT,
                    MODE_HELICANNON_1STPERSON, MODE_TOPDOWN, MODE_TOP_DOWN_PED, MODE_CAMERA
                }, activeMode2) || bSwitchToFollowPedImmediately || bPlayerLeftArrestState) {
                    if (!m_bJustCameOutOfGarage) {
                        if (notsa::contains({
                            MODE_SNIPER, MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_HS, MODE_M16_1STPERSON, MODE_1STPERSON,
                            MODE_SNIPER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT_HS, MODE_M16_1STPERSON_RUNABOUT,
                            MODE_FIGHT_CAM_RUNABOUT, MODE_1STPERSON_RUNABOUT, MODE_HELICANNON_1STPERSON, MODE_CAMERA
                        }, activeMode2)) {
                            // Make the ped face where the camera is looking
                            auto&       cam      = GetActiveCam();
                            const float heading  = CGeneral::GetATanOfXY(cam.m_vecFront.x, cam.m_vecFront.y) - std::numbers::pi_v<float> / 2.f;
                            m_pTargetEntity->AsPed()->m_fCurrentRotation = heading;
                            m_pTargetEntity->AsPed()->m_fAimingRotation  = heading;
                        }

                        m_bUseTransitionBeta = true;
                        bJumpCut             = true;
                        // NOTSA: The original computes `cam.m_vecSource - player pos` here, but then overwrites it
                        if (GetActiveCam().m_nMode == MODE_TOP_DOWN_PED) {
                            GetActiveCam().m_fTransitionBeta = CGeneral::GetATanOfXY(0.001f, 1.f);
                        } else {
                            GetActiveCam().m_fTransitionBeta = CGeneral::GetATanOfXY(GetActiveCam().m_vecFront.x, GetActiveCam().m_vecFront.y) + std::numbers::pi_v<float>;
                        }
                    }
                }
            } else {
                if (notsa::contains({ MODE_LIGHTHOUSE, MODE_ARRESTCAM_ONE, MODE_ARRESTCAM_TWO, MODE_PED_DEAD_BABY }, gNewCamMode) || activeMode == MODE_PED_DEAD_BABY) {
                    bJumpCut = true;
                }
            }

            // 0x52ADD6
            if (gNewCamMode != GetActiveCam().m_nMode && !GetActiveCam().m_pCamTargetEntity) {
                bJumpCut = true;
            }

            if (m_bPlayerIsInGarage) {
                if (m_pToGarageWeAreIn
                    && notsa::contains({ BOMBSHOP_TIMED, BOMBSHOP_ENGINE, BOMBSHOP_REMOTE }, m_pToGarageWeAreIn->m_nType)
                    && m_pTargetEntity->GetIsTypeVehicle()
                    && m_pTargetEntity->m_nModelIndex == MODEL_MRWHOOP
                    && gNewCamMode != GetActiveCam().m_nMode
                ) {
                    bJumpCut = true;
                }

                if (const auto* const camTarget = GetActiveCam().m_pCamTargetEntity) {
                    // Switch immediately if the target is behind the fixed camera
                    const CVector camTargetPos = camTarget->GetPosition();
                    const CVector fromFixedCam = camTargetPos - m_vecFixedModeSource;
                    const CVector fromCam      = camTargetPos - GetActiveCam().m_vecSource;
                    if (((fromCam.x * fromFixedCam.x) + (fromCam.z * fromFixedCam.z)) + (fromCam.y * fromFixedCam.y) < 0.f) {
                        bJumpCut = true;
                    }
                }
            }

            const bool bModesDiffer = gNewCamMode != GetActiveCam().m_nMode;
            bool       bSwitchNow   = false;
            if (bModesDiffer) {
                if (!m_bTransitionState) {
                    if (!bJumpCut) {
                        if (!m_bWaitForInterpolToFinish) {
                            StartTransition(gNewCamMode);
                        }
                        return;
                    }
                    bSwitchNow = true;
                } else {
                    bSwitchNow = bJumpCut;
                }
            }

            if (bSwitchNow) {
                if (!m_bPlayerIsInGarage || m_bJustCameOutOfGarage) {
                    SetCamDirectlyBehindPlayerIfNeeded({
                        MODE_FOLLOWPED, MODE_M16_1STPERSON, MODE_SNIPER, MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_HS, MODE_CAMERA,
                        MODE_SYPHON, MODE_1STPERSON, MODE_SYPHON_CRIM_IN_FRONT, MODE_SPECIAL_FIXED_FOR_SYPHON
                    });
                }

                GetActiveCam().m_nMode = gNewCamMode;
                m_bJust_Switched       = true;
                CopyFixedModeDataToActiveCam(m_bLookingAtVector);
                m_fCarZoomSmoothed    = m_fCarZoomBase;
                m_fPedZoomSmoothed    = m_fPedZoomBase;
                m_bTransitionState    = false;
                m_bDoingSpecialInterp = false;
                m_bStartInterScript   = false;
                GetActiveCam().m_bResetStatics = true;
                return;
            }

            // 0x52AEDC
            if (!m_bTransitionState || !bModesDiffer) {
                if (gNewCamMode == MODE_FIXED && m_pTargetEntity != GetActiveCam().m_pCamTargetEntity && m_bPlayerIsInGarage) {
                    if (m_bTransitionState) {
                        m_bDoingSpecialInterp = true;
                    }
                    StartTransition(MODE_FIXED);
                }
                return;
            }

            // The transition is in progress, and the new mode isn't the current one
            if (!m_bWaitForInterpolToFinish && m_bLookingAtPlayer) {
                const CVector toPlayer = FindPlayerPed()->GetPosition() - m_mCameraMatrix.GetPosition();
                if (m_pTargetEntity && m_pTargetEntity->GetIsTypePed() && toPlayer.Magnitude() > 17.5f) {
                    if (gNewCamMode == MODE_SYPHON || gNewCamMode == MODE_SYPHON_CRIM_IN_FRONT) {
                        m_bWaitForInterpolToFinish = true;
                    }
                }
            }
            if (m_bWaitForInterpolToFinish == true) {
                return;
            }
            m_bDoingSpecialInterp = true;
            StartTransition(gNewCamMode);
        } else {
            // 0x52B215 - Not looking at the player (scripted)
            bool bSwitchNow       = false;
            bool bUseWeaponCamMode = false;
            if (m_bEnable1rstPersonCamCntrlsScript == true || m_bAllow1rstPersonWeaponsCamera == true) {
                if (gNewCamMode == MODE_1STPERSON) {
                    if (GetActiveCam().m_nMode != MODE_1STPERSON) {
                        bSwitchNow = true;
                    }
                } else {
                    const auto weaponMode = static_cast<eCamMode>(m_PlayerWeaponMode.m_nMode);
                    if (notsa::contains({ MODE_SNIPER, MODE_1STPERSON, MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_HS }, weaponMode)
                        && CPad::GetPad(0)->GetTarget()
                        && m_bAllow1rstPersonWeaponsCamera
                    ) {
                        bSwitchNow        = true;
                        bUseWeaponCamMode = true;
                    } else if (GetActiveCam().m_nMode != m_nModeToGoTo) {
                        m_bStartInterScript      = true;
                        m_nTypeOfSwitch          = eSwitchType::JUMPCUT;
                        CPad::GetPad(0)->bCamera = false;
                    }
                }
            }

            if (!m_bTransitionState) {
                if (m_bStartInterScript == true && m_nTypeOfSwitch == eSwitchType::INTERPOLATION) {
                    gNewCamMode = m_nModeToGoTo;
                    StartTransition(m_nModeToGoTo);
                    return;
                }
                if (!bSwitchNow && !(m_bStartInterScript == true && m_nTypeOfSwitch == eSwitchType::JUMPCUT)) {
                    return;
                }
            } else {
                if (m_bStartInterScript == true) {
                    if (m_nTypeOfSwitch == eSwitchType::INTERPOLATION) {
                        gNewCamMode           = m_nModeToGoTo;
                        m_bDoingSpecialInterp = true;
                        StartTransition(m_nModeToGoTo);
                        return;
                    }
                    if (!bSwitchNow && m_nTypeOfSwitch != eSwitchType::JUMPCUT) {
                        return;
                    }
                } else if (!bSwitchNow) {
                    return;
                }
            }

            m_bTransitionState    = false;
            m_bDoingSpecialInterp = false;
            if (m_bEnable1rstPersonCamCntrlsScript == true && gNewCamMode == MODE_1STPERSON) {
                GetActiveCam().m_nMode = gNewCamMode;
            } else if (bUseWeaponCamMode) {
                GetActiveCam().m_nMode = static_cast<eCamMode>(m_PlayerWeaponMode.m_nMode);
            } else {
                GetActiveCam().m_nMode = m_nModeToGoTo;
            }
            m_bJust_Switched               = true;
            GetActiveCam().m_bResetStatics = true;
            CopyFixedModeDataToActiveCam(m_bLookingAtVector);
            m_bJust_Switched   = true;
            m_fCarZoomSmoothed = m_fCarZoomBase;
            m_fPedZoomSmoothed = m_fPedZoomBase;
        }
    }();

    // 0x52B4FB
    m_bStartInterScript = false;
    if (!GetActiveCam().m_pCamTargetEntity) {
        GetActiveCam().m_pCamTargetEntity = m_pTargetEntity;
        CEntity::RegisterReference(GetActiveCam().m_pCamTargetEntity);
    }

    // Hide the player in the first person modes
    const auto currentMode = GetActiveCam().m_nMode;
    if (currentMode == MODE_FLYBY
        || (m_pTargetEntity->GetIsTypePed() && notsa::contains({
            MODE_1STPERSON, MODE_SNIPER, MODE_M16_1STPERSON, MODE_CAMERA, MODE_HELICANNON_1STPERSON, MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_HS
        }, currentMode))
    ) {
        if (FindPlayerPed()->GetIsVisible()) {
            FindPlayerPed()->SetIsVisible(false);
            if (auto* const hold = FindPlayerPed()->GetIntelligence()->GetTaskHold(false); hold && hold->m_pEntityToHold) {
                hold->m_pEntityToHold->SetIsVisible(false);
            }
        }
    } else {
        FindPlayerPed()->SetIsVisible(true);
    }

    if (GetActiveCam().m_nMode == MODE_FIXED) {
        FindPlayerPed()->SetIsVisible(gPlayerPedVisible);
    }

    bool bRestoredWithJumpCut = false;
    if (!bCinematicCamAllowed && m_nWhoIsInControlOfTheCamera == 2) {
        RestoreWithJumpCut();
        bRestoredWithJumpCut = true;
        m_bCamDirectlyBehind = true;
        if (auto* const player = FindPlayerPed()) {
            m_fPedOrientForBehindOrInFront = CGeneral::GetATanOfXY(player->GetForward().x, player->GetForward().y);
        }
    }

    // Play the "cam mode changed" sound
    const auto finalMode = GetActiveCam().m_nMode;
    if (initialMode != static_cast<int16>(finalMode) || bRestoredWithJumpCut || finalMode == MODE_FOLLOWPED || finalMode == MODE_CAM_ON_A_STRING) {
        if (CPad::GetPad(0)->sub_540530()
            && CReplay::Mode != MODE_PLAYBACK
            && (m_bLookingAtPlayer == true || m_nWhoIsInControlOfTheCamera == 2)
            && !m_bWideScreenOn
            && (m_nWhoIsInControlOfTheCamera != 2 || (gCamModeChangedByPlayer == true && CPad::GetPad(0)->DisablePlayerControls == 0))
        ) {
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_DISPLAY_INFO, 0.f, 1.f);
        }
    }
}

// 0x5B24A0
void CCamera::DeleteCutSceneCamDataMemory() {
    for (auto& splines : m_aPathArray) {
        delete splines.m_pArrPathData;
        splines.m_pArrPathData = nullptr;
    }
}

// 0x5B24D0
void CCamera::LoadPathSplines(FILE* file) {
    DeleteCutSceneCamDataMemory();

    int32  sectionIdx   = -1;
    bool   bExpectCount = true;
    int32  linesLeft    = 0;
    float* cursor       = nullptr;

    while (const char* line = CFileLoader::LoadLine(file)) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        if (linesLeft == 0) {
            if (bExpectCount) { // Start of a new section: a line containing the number of entries
                if (++sectionIdx > 3) {
                    return;
                }

                (void)sscanf(line, "%d", &linesLeft);

                // Sections 0 and 1 have 4 floats per line, 2 and 3 have 10
                const size_t numFloatsPerLine = (sectionIdx == 0 || sectionIdx == 1) ? 4 : 10;
                auto* const  data             = static_cast<float*>(operator new(linesLeft * numFloatsPerLine * sizeof(float) + sizeof(float)));
                m_aPathArray[sectionIdx].m_pArrPathData = data;
                data[0] = (float)linesLeft;
                cursor  = data + 1;

                bExpectCount = false;
            } else if (line[0] == ';') { // End of section marker
                bExpectCount = true;
            }
        } else {
            linesLeft--;

            char* ctx{};
            for (char* tok = strtok_s(const_cast<char*>(line), ", \t", &ctx); tok; tok = strtok_s(nullptr, ", \t", &ctx)) {
                *cursor++ = (float)atof(tok);
            }
        }
    }
}

// 0x50AB50
void CCamera::GetScreenRect(CRect* rect) const {
    rect->left  = 0.0f;
    rect->right = SCREEN_WIDTH;

    if (m_bWideScreenOn) {
        rect->top    = (float)(RsGlobal.maximumHeight / 2) * m_fScreenReductionPercentage / 100.f - SCREEN_SCALE_Y(22.0f);
        rect->bottom = SCREEN_HEIGHT - (RsGlobal.maximumHeight / 2) * m_fScreenReductionPercentage / 100.f - SCREEN_SCALE_Y(14.0f);
    } else {
        rect->top    = 0.0f;
        rect->bottom = SCREEN_HEIGHT;
    }
}

// 0x50CB60
void CCamera::SetCamCollisionVarDataSet(int32 index) {
    if (index == gCurCamColVars) {
        return;
    }

    gCurCamColVars = index;
    gCurDistForCam = 1.0f;
    gpCamColVars   = gCamColVars[index];
}

// 0x50CCA0
void CCamera::SetColVarsVehicle(eVehicleType vehicleType, int32 camVehicleZoom) {
    switch (vehicleType) {
        case VEHICLE_TYPE_AUTOMOBILE:
        case VEHICLE_TYPE_PLANE:
            SetCamCollisionVarDataSet(camVehicleZoom + 9);
            return;
        case VEHICLE_TYPE_MTRUCK:
            SetCamCollisionVarDataSet(camVehicleZoom + 12);
            return;
        case VEHICLE_TYPE_QUAD:
            SetCamCollisionVarDataSet(camVehicleZoom + 15);
            return;
        case VEHICLE_TYPE_HELI:
            SetCamCollisionVarDataSet(camVehicleZoom + 18);
            return;
        case VEHICLE_TYPE_BOAT:
            SetCamCollisionVarDataSet(camVehicleZoom + 21);
            return;
        case VEHICLE_TYPE_TRAIN:
            SetCamCollisionVarDataSet(camVehicleZoom + 24);
            return;
    }
}

// 0x515BC0
void CCamera::StartTransitionWhenNotFinishedInter(eCamMode newCamMode) {
    m_bDoingSpecialInterp = true;
    StartTransition(newCamMode);
}

// 0x515200
/**
 * @brief Initiates a camera transition to a new camera mode.
 * 
 * This function handles the transition between different camera modes, setting up all necessary parameters
 * for a smooth camera movement. It manages aspects such as:
 * - Camera rotation and positioning
 * - Transition timing and interpolation fractions
 * - Special handling for weapon modes
 * - Entity references and target updates
 * 
 * The transition process includes:
 * 1. Setting up default transition values
 * 2. Handling player rotation for weapon modes
 * 3. Setting up the new camera parameters
 * 4. Managing specific camera mode transitions
 * 5. Initializing transition state and interpolation values
 * 6. Storing starting speeds and final transition parameters
 * 
 * @param newCamMode The camera mode to transition to (type eCamMode)
 * 
 * @note This function is central to the game's camera system and affects how the camera behaves
 * when switching between different views (e.g., from following a ped to aiming a weapon).
 * 
 * @see eCamMode
 * @see CCam
 */
void CCamera::StartTransition(eCamMode newCamMode) {
    CCam& activeCam             = m_aCams[m_nActiveCam];
    const auto activeCamMode    = activeCam.m_nMode;

    // Unused flag, not used in the game.
    // In GTA III/VC it was used for the Colt Python.
    m_bItsOkToLookJustAtThePlayer = false;

    // Default values
    m_bUseTransitionBeta          = false;
    m_fFractionInterToStopMoving  = 0.25f;
    m_fFractionInterToStopCatchUp = 0.75f;

    // Handle player rotation for weapon modes
    if (m_pTargetEntity && m_pTargetEntity->GetIsTypePed() && notsa::contains({ MODE_SNIPER, MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_HS, MODE_M16_1STPERSON, MODE_SNIPER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT_HS, MODE_M16_1STPERSON_RUNABOUT, MODE_FIGHT_CAM_RUNABOUT, MODE_HELICANNON_1STPERSON, MODE_CAMERA, MODE_1STPERSON_RUNABOUT }, activeCamMode)) {
        const float angle                            = CGeneral::GetATanOfXY(activeCam.m_vecFront.x, activeCam.m_vecFront.y) - HALF_PI;
        m_pTargetEntity->AsPed()->m_fCurrentRotation = angle;
        m_pTargetEntity->AsPed()->m_fAimingRotation  = angle;
    }

    // Setup new camera
    activeCam.m_vecCamFixedModeVector = m_vecFixedModeVector;
    CEntity::ChangeEntityReference(activeCam.m_pCamTargetEntity, m_pTargetEntity);

    activeCam.m_vecCamFixedModeSource   = m_vecFixedModeSource;
    activeCam.m_vecCamFixedModeUpOffSet = m_vecFixedModeUpOffSet;
    activeCam.m_bCamLookingAtVector     = m_bLookingAtVector;
    if (m_bItsOkToLookJustAtThePlayer) {
        activeCam.m_nMode = newCamMode;
    }

    // Handle specific camera mode transitions
    switch (newCamMode) {
    case MODE_BEHINDCAR:
    case MODE_BEHINDBOAT:
        activeCam.m_fBetaSpeed = 0.0f;
        break;
    case MODE_FOLLOWPED: {
        if (m_bJustCameOutOfGarage) {
            activeCam.m_fHorizontalAngle = CGeneral::GetATanOfXY(activeCam.m_vecFront.x, activeCam.m_vecFront.y) + PI;
            activeCam.m_fTransitionBeta  = 0.0f;
        }

        m_bCamDirectlyInFront |= m_bTargetJustCameOffTrain;

        if (activeCamMode == MODE_CAM_ON_A_STRING) {
            m_bUseTransitionBeta        = true;
            const float angle           = CGeneral::GetATanOfXY(activeCam.m_vecFront.x, activeCam.m_vecFront.y);
            activeCam.m_fTransitionBeta = angle + (fabs(angle) <= HALF_PI ? DegreesToRadians(235.0f) : DegreesToRadians(55.0f));
        }
        break;
    }
    case MODE_SNIPER:
    case MODE_ROCKETLAUNCHER:
    case MODE_M16_1STPERSON:
    case MODE_SNIPER_RUNABOUT:
    case MODE_ROCKETLAUNCHER_RUNABOUT:
    case MODE_1STPERSON_RUNABOUT:
    case MODE_M16_1STPERSON_RUNABOUT:
    case MODE_FIGHT_CAM_RUNABOUT:
    case MODE_HELICANNON_1STPERSON:
    case MODE_CAMERA:
    case MODE_ROCKETLAUNCHER_HS:
    case MODE_ROCKETLAUNCHER_RUNABOUT_HS: {
        CEntity* vehicle             = FindPlayerVehicle();
        CMatrix* playerMat           = vehicle ? &vehicle->GetMatrix() : &FindPlayerPed()->GetMatrix();
        activeCam.m_fHorizontalAngle = CGeneral::GetATanOfXY(playerMat->GetForward().x, playerMat->GetForward().y);
        activeCam.m_fVerticalAngle   = 0.0f;
        break;
    }
    case MODE_CAM_ON_A_STRING: {
        if (m_bLookingAtPlayer && !m_bJustCameOutOfGarage) {
            m_bUseTransitionBeta = true;
            const float angle    = CGeneral::GetATanOfXY(activeCam.m_vecFront.x, activeCam.m_vecFront.y);
            if (activeCamMode == MODE_FIXED) { // Ghidra
                activeCam.m_fTransitionBeta = angle;
                break;
            }

            // Reconstruced + android simplified
            activeCam.m_fTransitionBeta = angle + (fabs(angle) <= HALF_PI ? DegreesToRadians(235.0f) : DegreesToRadians(55.0f));
        }
        break;
    }
    case MODE_PED_DEAD_BABY:
        activeCam.m_fVerticalAngle = DegreesToRadians(15.0f);
        break;
    }

    // Backup horizontal angle before Init.
    const float horizAngle = activeCam.m_fHorizontalAngle;

    int targetCoorsDuration = 600; // Like android version instead bool.
    m_nTransitionDuration   = 1'350;

    // Switch active camera
    if (activeCamMode == MODE_FOLLOWPED && newCamMode == MODE_CAM_ON_A_STRING
        || activeCamMode == MODE_CAM_ON_A_STRING && newCamMode == MODE_FOLLOWPED) {
        activeCam.m_nMode = newCamMode;
    } else {
        activeCam.Init();
        activeCam.m_nMode            = newCamMode;
        activeCam.m_fHorizontalAngle = horizAngle;
    }

    [&]() -> const void {
        if (newCamMode == MODE_CAM_ON_A_STRING && notsa::contains({ MODE_SYPHON_CRIM_IN_FRONT, MODE_FOLLOWPED, MODE_SYPHON, MODE_SPECIAL_FIXED_FOR_SYPHON, MODE_AIMWEAPON }, activeCamMode)) {
            m_fFractionInterToStopMoving  = 0.1f;
            m_fFractionInterToStopCatchUp = 0.9f;
            m_nTransitionDuration         = 750;
            return;
        }

        switch (activeCamMode) {
        case MODE_SYPHON_CRIM_IN_FRONT:
            if (newCamMode == MODE_SYPHON) {
                m_nTransitionDuration = 1'800;
                return;
            }
            break;
        case MODE_SPECIAL_FIXED_FOR_SYPHON:
            m_fFractionInterToStopMoving  = 0.2f;  // dword_8CCCCC
            m_fFractionInterToStopCatchUp = 0.8f;  // *&dword_8CCCC8
            m_nTransitionDuration         = 1'000; // dword_8CCCC4
            return;
        case MODE_FIXED:
            m_fFractionInterToStopMoving  = 0.05f;
            m_fFractionInterToStopCatchUp = 0.95f;
            return;
        }

        if (m_bPlayerWasOnBike && newCamMode == MODE_FOLLOWPED) {
            if (activeCamMode == MODE_CAM_ON_A_STRING) {
                m_nTransitionDuration         = 800;
                m_fFractionInterToStopMoving  = 0.02f;
                m_fFractionInterToStopCatchUp = 0.98f;
                return;
            }
        } else {
            switch (newCamMode) {
            case MODE_CAM_ON_A_STRING:
            case MODE_BEHINDBOAT:
                if (notsa::contains({ MODE_SNIPER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT, MODE_ROCKETLAUNCHER_RUNABOUT_HS, MODE_1STPERSON_RUNABOUT, MODE_M16_1STPERSON_RUNABOUT, MODE_FIGHT_CAM_RUNABOUT, MODE_CAMERA }, activeCamMode)) {
                    m_fFractionInterToStopMoving  = 0.0f;
                    m_fFractionInterToStopCatchUp = 1.0f;
                    m_nTransitionDuration         = 1;
                    return;
                }
                break;
            case MODE_AIMWEAPON:
                m_fFractionInterToStopMoving  = 0.0f; // dword_B70044 ?
                m_fFractionInterToStopCatchUp = 1.0f; // *&dword_8CCCC0
                m_nTransitionDuration         = 400;  // dword_8CCCBC
                targetCoorsDuration           = 350;
                return;
            }

            if (!notsa::contains({ MODE_FOLLOWPED, MODE_SYPHON_CRIM_IN_FRONT, MODE_SYPHON, MODE_SPECIAL_FIXED_FOR_SYPHON }, newCamMode)) {
                m_nTransitionDuration = 1'350;
                return;
            }
        }
        if (!notsa::contains({ MODE_SYPHON_CRIM_IN_FRONT, MODE_FOLLOWPED, MODE_SYPHON, MODE_AIMWEAPON }, activeCamMode)) {
            m_nTransitionDuration = 1'350;
            return;
        }
        m_fFractionInterToStopMoving  = 0.1f;
        m_fFractionInterToStopCatchUp = 0.9f;
        m_nTransitionDuration         = 350;
        targetCoorsDuration           = 350;
    }();

    // Initialize transition state
    m_bTransitionState       = true;
    m_nTimeTransitionStart   = CTimer::GetTimeInMS();
    m_bTransitionJUSTStarted = true;

    // Store starting interpolation values
    if (m_bDoingSpecialInterp) {
        m_vecStartingSourceForInterPol = m_vecSourceDuringInter;
        m_vecStartingTargetForInterPol = m_vecTargetDuringInter;
        m_vecStartingUpForInterPol     = m_vecUpDuringInter;
        m_fStartingAlphaForInterPol    = m_fAlphaDuringInterPol;
        m_fStartingBetaForInterPol     = m_fBetaDuringInterPol;
    } else {
        m_vecStartingSourceForInterPol = activeCam.m_vecSource;
        m_vecStartingTargetForInterPol = activeCam.m_vecTargetCoorsForFudgeInter;
        m_vecStartingUpForInterPol     = activeCam.m_vecUp;
        m_fStartingAlphaForInterPol    = activeCam.m_fTrueAlpha;
        m_fStartingBetaForInterPol     = activeCam.m_fTrueBeta;
    }

    // Update active camera parameters
    activeCam.m_bCamLookingAtVector     = m_bLookingAtVector;
    activeCam.m_vecCamFixedModeVector   = m_vecFixedModeVector;
    activeCam.m_vecCamFixedModeSource   = m_vecFixedModeSource;
    activeCam.m_vecCamFixedModeUpOffSet = m_vecFixedModeUpOffSet;
    activeCam.m_nMode                   = newCamMode;
    CEntity::ChangeEntityReference(activeCam.m_pCamTargetEntity, m_pTargetEntity);

    // Store starting speeds
    m_fStartingFOVForInterPol    = activeCam.m_fFOV;
    m_vecSourceSpeedAtStartInter = activeCam.m_vecSourceSpeedOverOneFrame;
    m_vecTargetSpeedAtStartInter = activeCam.m_vecTargetSpeedOverOneFrame;
    m_vecUpSpeedAtStartInter     = activeCam.m_vecUpOverOneFrame;
    m_fAlphaSpeedAtStartInter    = activeCam.m_fAlphaSpeedOverOneFrame;
    m_fBetaSpeedAtStartInter     = activeCam.m_fBetaSpeedOverOneFrame;
    m_fFOVSpeedAtStartInter      = activeCam.m_fFovSpeedOverOneFrame;

    // Setup final transition parameters
    if (m_bLookingAtPlayer) {
        m_fFractionInterToStopMovingTarget  = 0.0f;
        m_fFractionInterToStopCatchUpTarget = 1.0f;
        m_nTransitionDurationTargetCoors    = targetCoorsDuration;
    } else {
        if (m_bScriptParametersSetForInterp) {
            m_fFractionInterToStopMoving  = m_fScriptPercentageInterToStopMoving;
            m_fFractionInterToStopCatchUp = m_fScriptPercentageInterToCatchUp;
            m_nTransitionDuration         = m_nScriptTimeForInterpolation;
        }
        m_nTransitionDurationTargetCoors    = m_nTransitionDuration;
        m_fFractionInterToStopMovingTarget  = m_fFractionInterToStopMoving;
        m_fFractionInterToStopCatchUpTarget = m_fFractionInterToStopCatchUp;
    }
}

auto CCamera::GetFrustumPoints() -> std::array<CVector, 5> {
    CVector pts[5]{};

    // First, the corners
    const auto farPlane  = RwCameraGetFarClipPlane(m_pRwCamera);
    const auto farVWSize = CVector2D{ *RwCameraGetViewWindow(m_pRwCamera) } * farPlane;
    const auto corners   = CRect{ -farVWSize, farVWSize }.GetCorners3D(farPlane);

    // Copy it into pts
    rng::copy(corners, pts);

    // Last is the center point
    pts[4] = CVector{ 0.f, 0.f, 0.f };

    // Transform them to world space
    RwV3dTransformPoints(pts, pts, 5, GetRwMatrix());

    // top left, top right, bottom right, bottom left, center
    return std::to_array(pts);
}
