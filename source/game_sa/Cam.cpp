#include "StdInc.h"

#include <numbers>

#include "Cam.h"
#include "TimeCycle.h"
#include "Camera.h"
#include "Shadows.h"
#include "IdleCam.h"
#include "InterestingEvents.h"
#include "ModelIndices.h"
#include "HandShaker.h"
#include "Tasks/TaskTypes/TaskSimpleHoldEntity.h"
#include "Tasks/TaskTypes/TaskSimpleGangDriveBy.h"
#include "Tasks/TaskTypes/TaskSimpleClimb.h"
#include "PostEffects.h"
#include "Tasks/TaskTypes/TaskSimpleSwim.h"
#include "Tasks/TaskTypes/TaskSimpleUseGun.h"
#include "WeaponInfo.h"
#include "PedIntelligence.h"
#include "Ragdoll/IKChainManager.h"
#include "GameLogic.h"
#include "Collision/Collision.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Bike.h"
#include "Entity/Vehicle/Plane.h"
#include "Tasks/TaskTypes/TaskComplexProstituteSolicit.h"

auto& gbFirstPersonRunThisFrame = StaticRef<bool>(0xB6EC20);
auto& gLastFrameProcessedDWCineyCam = StaticRef<uint32>(0x8CCB9C);

// Indexed by the DW cinematic cam id (20..28), see `IsTimeToExitThisDWCineyCamMode`
static inline auto& gbExitCam = StaticRef<std::array<bool, 29>>(0xB6EC5C);
static inline auto& gDWCineyCamMinDist = StaticRef<std::array<float, 9>>(0x8CCBCC);
static inline auto& gDWCineyCamMaxDist = StaticRef<std::array<float, 9>>(0x8CCBF0);
static inline auto& gLastDWCineyCamMode = StaticRef<int32>(0x8CC488);    // NOTE: name made up, holds the id of the last processed DW cinematic cam mode (reset to -1 in `Process`)
static inline auto& gDWCineyCamStartTime = StaticRef<uint32>(0x8CCBA0); // NOTE: name made up
static inline auto& gDWCineyCamEndTime = StaticRef<uint32>(0x8CCBA4); // NOTE: name made up, only compared against the current time in `IsTimeToExitThisDWCineyCamMode`

static inline auto& DWCineyCamLastPos = StaticRef<CVector>(0xB6FE8C);
static inline auto& DWCineyCamLastUp = StaticRef<CVector>(0xB6FE98);
static inline auto& DWCineyCamLastRight = StaticRef<CVector>(0xB6FEA4);
static inline auto& DWCineyCamLastFwd = StaticRef<CVector>(0xB6FEB0);

static inline auto& DWCineyCamLastNearClip = StaticRef<float>(0xB6EC08);
static inline auto& DWCineyCamLastFov = StaticRef<float>(0xB6EC0C);

//! Settings of the DW (David Wood) heli chase cinematic camera, a global at 0xB6FEC0 (names made up)
struct DWHeliChaseCamSettings {
    CVector endPos;                // 0x00
    CVector startPos;              // 0x0C
    float   endDistAhead;          // 0x18
    float   startDistBehind;       // 0x1C
    float   zOffset;               // 0x20
    float   sideOffset;            // 0x24
    float   fovBlendInFraction;    // 0x28
    float   fovStart;              // 0x2C
    float   fovEnd;                // 0x30
    float   targetVelFactor;       // 0x34
    float   rollRate;              // 0x38
    float   nearClip;              // 0x3C
    bool    bPosLocked;            // 0x40
    int32   lockedCounter;         // 0x44
    int32   lockedCounterInit;     // 0x48
    CVector lockedPos;             // 0x4C
    int32   numTries;              // 0x58
    bool    bCollided;             // 0x5C
    int32   clearCounterMax;       // 0x60
    int32   clearCounter;          // 0x64
    float   fovZoomDistMin;        // 0x68
    float   fovZoomDistMax;        // 0x6C
    float   fovZoomAmount;         // 0x70
    float   minDist2D;             // 0x74
    float   sphereRadius;          // 0x78
    float   fovRange;              // 0x7C
    bool    bBlocked;              // 0x80
    bool    bFlag81;               // 0x81
    float   savedFov;              // 0x84
    bool    bFovLerping;           // 0x88
    int32   fovLerpStartTime;      // 0x8C
    int32   fovLerpEndTime;        // 0x90
    float   fovLerpStartFraction;  // 0x94
    int32   fovLerpDuration;       // 0x98

    // 0x50E180
    void SetDefaults() {
        endDistAhead         = 50.0f;
        sideOffset           = 50.0f;
        startDistBehind      = 30.0f;
        zOffset              = 55.0f;
        fovBlendInFraction   = 0.05f;
        fovStart             = 70.0f;
        fovEnd               = 22.0f;
        targetVelFactor      = 1.0f;
        rollRate             = 0.0f;
        nearClip             = 10.0f;
        lockedCounter        = 30;
        numTries             = 8;
        clearCounterMax      = 60;
        clearCounter         = 60;
        fovZoomDistMin       = 100.0f;
        fovZoomDistMax       = 110.0f;
        fovZoomAmount        = 10.0f;
        minDist2D            = 5.0f;
        sphereRadius         = 12.0f;
        fovLerpStartFraction = 0.75f;
        fovLerpDuration      = 4000;
        bPosLocked           = false;
        bCollided            = false;
        bBlocked             = false;
        bFlag81              = false;
        fovRange             = 48.0f;
        lockedCounterInit    = 30;
        bFovLerping          = false;
    }

    // 0x50E090
    void Randomize() {
        endDistAhead       = ((float)CGeneral::GetRandomNumber() * 3.05185094e-05f * 1.4f + 0.1f) * endDistAhead;
        startDistBehind    = ((float)CGeneral::GetRandomNumber() * 3.05185094e-05f * 0.5f + 0.5f) * startDistBehind;
        zOffset            = ((float)CGeneral::GetRandomNumber() * 3.05185094e-05f * 0.5f + 0.5f) * zOffset;
        sideOffset         = ((float)CGeneral::GetRandomNumber() * 3.05185094e-05f * 0.5f + 0.5f) * sideOffset;
        fovBlendInFraction = ((float)CGeneral::GetRandomNumber() * 3.05185094e-05f * 1.5f + 0.5f) * fovBlendInFraction;
    }
};
VALIDATE_SIZE(DWHeliChaseCamSettings, 0x9C);
static inline auto& gDWHeliChaseCamSettings = StaticRef<DWHeliChaseCamSettings>(0xB6FEC0);

// Defined in other translation units (Camera.cpp, World.cpp)
extern float&                      gCurDistForCam;
extern std::array<CColPoint, 32>& gaTempSphereColPoints;

//! Written by `Process_FollowCar_SA`, read by `LookBehind` (name made up)
static inline auto& gCamFollowCarLookAt = StaticRef<CVector>(0xB6F018);

//! A row of the follow ped camera tuning table at 0x8CC548 (2 rows: outside, interior - selected by the current area), see `Process_FollowPed_SA`. All names made up.
struct FollowPedCamTuning {
    float targetZOffset; // 0x00 - Added to the Z of the target
    float zoomDistAdd;   // 0x04 - Added to the smoothed ped zoom to get the camera distance
    float alphaBase;     // 0x08 - Base of the vertical angle offset (the zoom dependent ones are added)
    float minDist;       // 0x0C - Min camera distance
    float clipDistMin;   // 0x10 - Min distance used for the source of the previous camera positions
    float alphaPowBase;  // 0x14 - Base of the vertical angle smoothing factor (raised to the power of the time step)
    float alphaMaxStep;  // 0x18 - Max vertical angle change per time step unit
    float field_1C;      // 0x1C - Unused
    float betaPowBase;   // 0x20 - Base of the horizontal angle speed smoothing factor
    float speedCap;      // 0x24 - Max angle speed
    float betaChaseRate; // 0x28 - How fast the camera follows the ped's heading (per time step unit)
    float betaChaseCap;  // 0x2C - Max of the above (per time step unit)
    float field_30;      // 0x30 - Unused
    float alphaMax;      // 0x34 - Max vertical angle
    float alphaMinMag;   // 0x38 - Magnitude of the min vertical angle
};
VALIDATE_SIZE(FollowPedCamTuning, 0x3C);
static inline auto& gFollowPedCamTuning = StaticRef<std::array<FollowPedCamTuning, 2>>(0x8CC548);

// Globals of `Process_FollowPed_SA` (names made up)
static inline auto& gFollowPedLastZoomDist        = StaticRef<float>(0xB6EC50); // Camera distance of the last frame
static inline auto& gbFollowPedCamBehindPlayer    = StaticRef<bool>(0xB6EC54);  // Set while the "camera behind player" button is held (until the ped moves)
static inline auto& gFollowPedLastAlpha           = StaticRef<float>(0x8CCE74);
static inline auto& gFollowPedLastBeta            = StaticRef<float>(0x8CCE6C);

// Shared by `Process` and `Process_FollowPed_SA` (names made up)
static inline auto& gCamPlayerLastPos             = StaticRef<CVector>(0x8CCC3C); // Position of the followed player of the last frame (`Process_FollowPed_SA` resets it)
static inline auto& gCamPlayerPosVel              = StaticRef<CVector>(0xB6EC7C); // Smoothed velocity of the above (`Process_FollowPed_SA` resets it)
static inline auto& gCamUnkB6FE34                 = StaticRef<int32>(0xB6FE34);   // Compared with 0xB6FDC8 in `Process`, zeroed by `Process_FollowPed_SA` and `CIdleCam::IdleCamGeneralProcess`

// 0x4082C0 (and 0x406DA0 for the squared one) - Kept in the FPU registers in the original (extended precision)
static double SqMagExt(const CVector& v) {
    return ((double)v.x * (double)v.x + (double)v.y * (double)v.y) + (double)v.z * (double)v.z;
}

// 0x40FDB0 - Kept in the FPU registers in the original (extended precision)
static double DotExt(const CVector& a, const CVector& b) {
    return ((double)a.z * (double)b.z + (double)a.y * (double)b.y) + (double)a.x * (double)b.x;
}

//! Heading of the ped the way the original computes it (extended precision), from the matrix if there is one
static double PedHeadingExt(CPed* ped) {
    if (const auto* const mat = ped->m_matrix) {
        const auto& fwd = mat->GetForward();
        return std::atan2((double)-fwd.x, (double)fwd.y);
    }
    return (double)ped->m_placement.m_fHeading;
}

// 0x50A0A0 - Rounds to the given number of decimal digits
static float LimitPrecision(float v, int32 digits) {
    return plugin::CallAndReturn<float, 0x50A0A0, float, int32>(v, digits);
}

// 0x50A120
static void LimitPrecision(CVector& v) {
    v.x = LimitPrecision(v.x, 4);
    v.y = LimitPrecision(v.y, 4);
    v.z = LimitPrecision(v.z, 4);
}

// 0x509CA0 (a `CCam` method in the original, `this->m_pCamTargetEntity` is passed in here)
static bool GetBoatLookLRBehindCamHeight(CEntity* target, float& outHeight) {
    if (!target) {
        return false;
    }
    const auto* const mi = target->GetModelInfo()->AsVehicleModelInfoPtr();
    if (const auto* const boatHandling = gHandlingDataMgr.GetBoatPointer((uint8)mi->m_nHandlingId)) {
        outHeight = boatHandling->m_fLookLRBehindCamHeight;
        return true;
    }
    return false;
}

// 0x5132D0 - Computes the camera position/front for the two player camera (`cam` is the `this` of the original)
static void ComputeTwoPlayerCamPos(CCam& cam, float angle, CVector& outSource, CVector& outFront, CVector& outTarget) {
    const double pitch = cam.m_fVerticalAngle;
    outFront.x         = (float)-(std::cos((double)angle) * std::cos(pitch));
    outFront.y         = (float)-(std::sin((double)angle) * std::cos(pitch));
    outFront.z         = (float)std::sin(pitch);

    CVector flatDir{ outFront.x, outFront.y, 0.0f };
    flatDir.Normalise();

    const auto& pos0 = CWorld::Players[0].m_pPed->GetPosition();
    const auto& pos1 = CWorld::Players[1].m_pPed->GetPosition();

    const float dist = (pos0 - pos1).Magnitude() * 0.67f + 7.0f;

    CVector diff2D{ pos0.x - pos1.x, pos0.y - pos1.y, 0.0f };
    diff2D.Normalise();

    const float dot = diff2D.y * flatDir.y + diff2D.x * flatDir.x + 0.0f * 0.0f;
    const float t   = 0.5f - dot * 0.25f;

    const float w1 = 1.0f - t;
    outTarget.x    = t * pos0.x + w1 * pos1.x;
    outTarget.y    = t * pos0.y + w1 * pos1.y;
    outTarget.z    = t * pos0.z + w1 * pos1.z;

    outSource.x = outTarget.x - outFront.x * dist;
    outSource.y = outTarget.y - outFront.y * dist;
    outSource.z = outTarget.z - outFront.z * dist;
    outSource.z = dist * 0.1f + outSource.z;
}

// 0x513220 - Whether there are no buildings between `pos` and both players
static bool IsTwoPlayerCamPosClear(const CVector& pos) {
    CColPoint colPoint;
    CEntity*  hitEntity{};
    gCurCamColVars = 5;
    if (CWorld::ProcessLineOfSight(pos, CWorld::Players[0].m_pPed->GetPosition(), colPoint, hitEntity, true, false, false, false, false, true, true, false)) {
        return false;
    }
    return !CWorld::ProcessLineOfSight(pos, CWorld::Players[1].m_pPed->GetPosition(), colPoint, hitEntity, true, false, false, false, false, true, true, false);
}

// 0x509BE0 - wraps the angle into [-PI, PI)
static float WrapAngleToPi(float a) {
    for (; a >= PI; a -= (2.0f * PI)) {
        ;
    }
    for (; a < -PI; a += (2.0f * PI)) {
        ;
    }
    return a;
}
static void LimitAngleToPi(float& a) { a = WrapAngleToPi(a); }

// 0x514030 - `CCamera::AvoidTheGeometry` (declared in Camera.h, but not defined there at the time of writing)
static void AvoidTheGeometry(const CVector& src, const CVector& dst, CVector& out, float fov) {
    plugin::CallMethod<0x514030, CCamera*, const CVector*, const CVector*, CVector*, float>(&TheCamera, &src, &dst, &out, fov);
}

// 0x509AE0
static void WellBufferMe(float target, float& valueToChange, float& speedSoFar, float topSpeed, float speedStep, bool isAnAngle) {
    const auto valueToTargetDiff = [&] {
        auto d = target - valueToChange;
        if (isAnAngle) {
            for (; d >= DegreesToRadians(180.0f); d -= DegreesToRadians(360.0f)) {
                ;
            }
            for (; d < DegreesToRadians(-180.0f); d += DegreesToRadians(360.0f)) {
                ;
            }
        }
        return d;
    }();

    const auto fullSpeedStep = valueToTargetDiff * topSpeed;
    speedSoFar += std::abs(std::abs(fullSpeedStep - speedSoFar) * CTimer::GetTimeStep() * speedStep);

    if (fullSpeedStep >= 0.0f || fullSpeedStep <= speedSoFar) {
        if (fullSpeedStep > 0.0f && fullSpeedStep < speedSoFar) {
            speedSoFar = fullSpeedStep;
        }
    } else {
        speedSoFar = fullSpeedStep;
    }

    valueToChange += std::min(CTimer::GetTimeStep(), 10.0f) * speedSoFar;
}

void CCam::InjectHooks() {
    RH_ScopedClass(CCam);
    RH_ScopedCategory("Camera");

    RH_ScopedInstall(Constructor, 0x517730);
    RH_ScopedInstall(Init, 0x50E490);
    RH_ScopedInstall(CacheLastSettingsDWCineyCam, 0x50D7A0);
    RH_ScopedInstall(DoCamBump, 0x50CB30);
    RH_ScopedInstall(Finalise_DW_CineyCams, 0x50DD70);
    RH_ScopedInstall(GetCoreDataForDWCineyCamMode, 0x517130);
    RH_ScopedInstall(GetLookFromLampPostPos, 0x5161A0);
    RH_ScopedInstall(GetVectorsReadyForRW, 0x509CE0);
    RH_ScopedInstall(Get_TwoPlayer_AimVector, 0x513E40);
    RH_ScopedInstall(IsTimeToExitThisDWCineyCamMode, 0x517400);
    RH_ScopedInstall(KeepTrackOfTheSpeed, 0x509DF0);
    RH_ScopedInstall(LookBehind, 0x520690);
    RH_ScopedInstall(LookRight, 0x520E40);
    RH_ScopedInstall(RotCamIfInFrontCar, 0x50A4F0);
    RH_ScopedInstall(Using3rdPersonMouseCam, 0x50A850);
    RH_ScopedInstall(Process, 0x526FC0);
    RH_ScopedInstall(ProcessArrestCamOne, 0x518500, { .Reversed = false });
    RH_ScopedInstall(ProcessPedsDeadBaby, 0x519250);
    RH_ScopedInstall(Process_1rstPersonPedOnPC, 0x50EB70);
    RH_ScopedInstall(Process_1stPerson, 0x517EA0);
    RH_ScopedInstall(Process_AimWeapon, 0x521500, { .Reversed = false });
    RH_ScopedInstall(Process_AttachedCam, 0x512B10);
    RH_ScopedInstall(Process_Cam_TwoPlayer, 0x525E50);
    RH_ScopedInstall(Process_Cam_TwoPlayer_InCarAndShooting, 0x519810);
    RH_ScopedInstall(Process_Cam_TwoPlayer_Separate_Cars, 0x513510);
    RH_ScopedInstall(Process_Cam_TwoPlayer_Separate_Cars_TopDown, 0x513BE0);
    RH_ScopedInstall(Process_DW_BirdyCam, 0x51B850);
    RH_ScopedInstall(Process_DW_CamManCam, 0x51B120);
    RH_ScopedInstall(Process_DW_HeliChaseCam, 0x51A740);
    RH_ScopedInstall(Process_DW_PlaneCam1, 0x51C760);
    RH_ScopedInstall(Process_DW_PlaneCam2, 0x51CC30);
    RH_ScopedInstall(Process_DW_PlaneCam3, 0x51D100);
    RH_ScopedInstall(Process_DW_PlaneSpotterCam, 0x51C250);
    RH_ScopedInstall(Process_Editor, 0x50F3F0);
    RH_ScopedInstall(Process_Fixed, 0x51D470);
    RH_ScopedInstall(Process_FlyBy, 0x5B25F0, { .Reversed = false });
    RH_ScopedInstall(Process_FollowCar_SA, 0x5245B0);
    RH_ScopedInstall(Process_FollowPedWithMouse, 0x50F970);
    RH_ScopedInstall(Process_FollowPed_SA, 0x522D40);
    RH_ScopedInstall(Process_M16_1stPerson, 0x5105C0, { .Reversed = false });
    RH_ScopedInstall(Process_Rocket, 0x511B50);
    RH_ScopedInstall(Process_SpecialFixedForSyphon, 0x517500);
    RH_ScopedInstall(Process_WheelCam, 0x512110);

    RH_ScopedGlobalInstall(WellBufferMe, 0x509AE0);
}

// 0x517730
CCam::CCam() {
    Init();
}

// 0x50E490
void CCam::Init() {
    m_vecFront = CVector(0, 0, -1);
    m_vecUp = CVector(0, 0, 1);
    m_nMode = eCamMode::MODE_FOLLOWPED;
    m_bRotating = false;
    m_nDoCollisionChecksOnFrameNum = 1;
    m_nDoCollisionCheckEveryNumOfFrames = 9;
    m_nFrameNumWereAt = 0;
    m_bCollisionChecksOn = true;
    m_fRealGroundDist = 0.0f;
    m_fBetaSpeed = 0.0f;
    m_fAlphaSpeed = 0.0f;
    m_fCameraHeightMultiplier = 0.75;
    m_fMaxRoleAngle = DegreesToRadians(20.0f);
    m_fDistance = 30.0f;
    m_fDistanceSpeed = 0.0f;
    m_pLastCarEntered = nullptr;
    m_pLastPedLookedAt = nullptr;
    m_bResetStatics = true;
    m_fHorizontalAngle = 0.0f;
    m_fTilt = 0.0f;
    m_fTiltSpeed = 0.0f;
    m_bFixingBeta = false;
    m_fCaMinDistance = 0.0f;
    m_fCaMaxDistance = 0.0f;
    m_bLookingBehind = false;
    m_bLookingLeft = false;
    m_bLookingRight = false;
    m_fPlayerInFrontSyphonAngleOffSet = DegreesToRadians(20.0f);
    m_fSyphonModeTargetZOffSet = 0.5f;
    m_fRadiusForDead = 1.5f;
    m_nDirectionWasLooking = 3; // TODO: enum
    m_bLookBehindCamWasInFront = 0;
    m_fRoll = 0.0f;
    m_fRollSpeed = 0.0f;
    m_fCloseInPedHeightOffset = 0.0f;
    m_fCloseInPedHeightOffsetSpeed = 0.0f;
    m_fCloseInCarHeightOffset = 0.0f;
    m_fCloseInCarHeightOffsetSpeed = 0.0f;
    m_fPedBetweenCameraHeightOffset = 0.0f;
    m_fTargetBeta = 0.0f;
    m_fBufferedTargetBeta = 0.0f;
    m_fBufferedTargetOrientation = 0.0f;
    m_fBufferedTargetOrientationSpeed = 0.0f;
    m_fDimensionOfHighestNearCar = 0.0;
    m_fBeta_Targeting = 0.0f;
    m_fX_Targetting = 0.0f;
    m_fY_Targetting = 0.0f;
    m_pCarWeAreFocussingOn = nullptr;
    m_pCarWeAreFocussingOnI = nullptr;
    m_fCamBumpedHorz = 1.0f;
    m_fCamBumpedVert = 0.0f;
    m_nCamBumpedTime = 0;
    for (int i = 0; i < 4; ++i) {
        m_anTargetHistoryTime[i] = 0;
        m_avecTargetHistoryPos[i] = CVector{};
    }
    m_nCurrentHistoryPoints = 0;
    gPlayerPedVisible = true;
    gbCineyCamMessageDisplayed = 2; // TODO: enum
    gCameraDirection = 3; // TODO: enum
    gCameraMode = (eCamMode)-1;
    gLastTime2PlayerCameraWasOK = 0;
    gLastTime2PlayerCameraCollided = 0;
    TheCamera.m_bCinemaCamera = false;
}

// 0x50D7A0
void CCam::CacheLastSettingsDWCineyCam() {
    DWCineyCamLastUp       = m_vecUp;
    DWCineyCamLastFwd      = m_vecFront;
    DWCineyCamLastRight    = CrossProduct(m_vecFront, m_vecUp);
    DWCineyCamLastFov      = m_fFOV;
    DWCineyCamLastNearClip = RwCameraGetNearClipPlane(Scene.m_pRwCamera);
    DWCineyCamLastPos      = m_vecSource;
}

// 0x50CB30
void CCam::DoCamBump(float horizontal, float vertical) {
    m_fCamBumpedHorz = horizontal;
    m_fCamBumpedVert = vertical;
    m_nCamBumpedTime = CTimer::GetTimeInMS();
}

// 0x50DD70
void CCam::Finalise_DW_CineyCams(const CVector& src, const CVector& dest, float roll, float fov, float nearClip, float shakeDegree) {
    m_vecFront  = (dest - src).Normalized();
    m_vecSource = src;

    // What is this thing?
    {
        auto rightDir = m_vecFront.Cross({ std::sin(roll), 0.0f, std::cos(roll) }).Normalized();
        m_vecUp       = rightDir.Cross(m_vecFront);
        if (m_vecFront.x == 0.0f && m_vecFront.y == 0.0f) {
            m_vecFront.x = m_vecFront.y = 0.0001f;
        }
        rightDir = CrossProduct(m_vecFront, m_vecUp).Normalized();
        m_vecUp  = CrossProduct(rightDir, m_vecFront);
    }

    m_fFOV = fov;
    RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.4f); // meant to use nearClip here?
    CacheLastSettingsDWCineyCam();
    gLastFrameProcessedDWCineyCam = CTimer::GetFrameCounter();

    gHandShaker[0].Process(shakeDegree);
    m_vecFront = gHandShaker[0].m_resultMat.TransformVector(m_vecFront);
    m_vecFront.Normalise();

    {
        auto rightDir = m_vecFront.Cross({ std::sin(roll), 0.0f, std::cos(roll) }).Normalized();
        m_vecUp       = rightDir.Cross(m_vecFront);
        if (m_vecFront.x == 0.0f && m_vecFront.y == 0.0f) {
            m_vecFront.x = m_vecFront.y = 0.0001f;
        }
        rightDir = m_vecFront.Cross(m_vecUp).Normalized();
        m_vecUp  = rightDir.Cross(m_vecFront);
    }
}

// 0x517130
void CCam::GetCoreDataForDWCineyCamMode(
    CEntity*& entity,
    CVehicle*& vehicle,
    CVector& dest,
    CVector& src,
    CVector& targetUp,
    CVector& targetRight,
    CVector& targetFwd,
    CVector& targetVel,
    float& targetSpeed,
    CVector& targetAngVel,
    float& targetAngSpeed,
    CColSphere& colSphere
) {
    entity         = m_pCamTargetEntity;
    vehicle        = entity->AsVehicle();
    dest           = entity->GetPosition();
    src            = DWCineyCamLastPos;
    targetUp       = entity->GetUpVector();
    targetRight    = entity->GetRightVector();
    targetFwd      = entity->GetForwardVector();
    targetVel      = entity->AsPhysical()->GetMoveSpeed();
    targetSpeed    = targetVel.Magnitude();
    targetAngVel   = entity->AsPhysical()->GetTurnSpeed();
    targetAngSpeed = targetAngVel.Magnitude();

    colSphere.Set(
        entity->GetModelInfo()->GetColModel()->GetBoundRadius(),
        entity->GetBoundCentre(),
        eSurfaceType::SURFACE_DEFAULT
    );
}

// 0x5161A0
bool CCam::GetLookFromLampPostPos(CEntity* target, CPed* cop, const CVector& vecTarget, CVector& outSource) {
    CEntity* entities[16]{};
    int16    numEntities{};
    CWorld::FindObjectsInRange(vecTarget, 30.0f, true, &numEntities, 15, entities, false, false, false, true, true);

    float   bestDiff = 10000.0f;
    CEntity* found{};
    for (int16 i = 0; i < numEntities; i++) {
        auto* const entity = entities[i];
        if (!entity->GetIsStatic()) {
            continue;
        }
        if (!(entity->GetMatrix().GetUp().z > 0.9f)) {
            continue;
        }
        if (!IsLampPost(entity->GetModelId())) {
            continue;
        }
        const auto  entityPos = entity->GetPosition();
        const float dist2D    = (CVector2D{ entityPos } - CVector2D{ vecTarget }).Magnitude();
        if (!(dist2D > 5.0f && std::abs(17.0f - dist2D) < bestDiff)) {
            continue;
        }

        const auto  pointOnLamp = entity->GetMatrix().TransformPoint(entity->GetColModel()->m_boundBox.m_vecMax);
        const auto  dir         = (pointOnLamp - vecTarget).Normalized();
        if (CWorld::GetIsLineOfSightClear(pointOnLamp, dir + vecTarget, true, false, false, false, false, true, true)) {
            found      = entity;
            outSource  = pointOnLamp;
            bestDiff   = std::abs(17.0f - dist2D);
        }
    }
    return found != nullptr;
}

// 0x509CE0
void CCam::GetVectorsReadyForRW() {
    m_vecFront.Normalise();
    if (m_vecFront.x == 0.0f && m_vecFront.y == 0.0f) {
        m_vecFront.x = m_vecFront.y = 0.0001f;
    }
    const auto a = CrossProduct(m_vecFront, { 0.0f, 0.0f, 1.0f }).Normalized();
    m_vecUp = CrossProduct(a, m_vecFront);
}

// 0x513E40 -- not tested
void CCam::Get_TwoPlayer_AimVector(CVector& out) {
    const auto player = [&] {
        auto* p1 = FindPlayerPed(PED_TYPE_PLAYER1);
        if (p1->m_pVehicle && !p1->m_pVehicle->IsDriver(p1)) {
            return FindPlayerPed(PED_TYPE_PLAYER2);
        }
        return p1;
    }();

    const auto weaponInfo = player->GetActiveWeapon().GetWeaponInfo(player);
    const auto nearestTargetEntityInScreen = CWeapon::FindNearestTargetEntityWithScreenCoors(
        m_fX_Targetting,
        m_fY_Targetting,
        2 * weaponInfo.m_fWeaponRange,
        player->GetPosition()
    );

    if (nearestTargetEntityInScreen) {
        out = nearestTargetEntityInScreen->GetPosition() - m_vecSource;
    } else {
        const auto right  = CrossProduct(m_vecFront, m_vecUp);
        const auto tanFov = std::tan(m_fFOV * PI / 360.0f);

        out = m_fX_Targetting * m_fY_Targetting * tanFov * right + m_vecFront - tanFov / CDraw::ms_fAspectRatio * m_vecUp;
    }
    out.Normalise();
}

// 0x517400
bool CCam::IsTimeToExitThisDWCineyCamMode(int32 camId, const CVector& src, const CVector& dst, float t, bool lineOfSightCheck) {
    if (gbExitCam[camId]) {
        return true;
    }

    const auto d       = dst - src;
    const auto dist    = std::sqrt(sq(d.z) + sq(d.x) + sq(d.y)); // NOTE: summation order as in the original
    const auto inRange = dist >= gDWCineyCamMinDist[camId - 20] && dist <= gDWCineyCamMaxDist[camId - 20];

    bool isClear = true;
    if (lineOfSightCheck) {
        CColPoint colPoint;
        CEntity*  hitEntity{};
        CWorld::pIgnoreEntity = m_pCamTargetEntity;
        isClear               = !CWorld::ProcessLineOfSight(dst, src, colPoint, hitEntity, true, true, false, false, false, false, false, false);
        CWorld::pIgnoreEntity = nullptr;
    }

    if (camId >= 20 && camId <= 28) {
        if (!inRange || !isClear || gDWCineyCamEndTime < CTimer::GetTimeInMS()) {
            return true;
        }
    }
    return false;
}

// 0x509DF0
void CCam::KeepTrackOfTheSpeed(const CVector& source, const CVector& target, const CVector& up, const float& alpha, const float& beta, const float& fov) {
    // The original uses function-local statics (+ their MSVC init guard bits at 0xB6FF8C), names made up
    static auto& prevFov       = StaticRef<float>(0xB6FF5C);
    static auto& prevAlpha     = StaticRef<float>(0xB6FF60);
    static auto& prevBeta      = StaticRef<float>(0xB6FF64);
    static auto& prevUp        = StaticRef<CVector>(0xB6FF68);
    static auto& prevTarget    = StaticRef<CVector>(0xB6FF74);
    static auto& prevSource    = StaticRef<CVector>(0xB6FF80);
    static auto& initGuardMask = StaticRef<uint32>(0xB6FF8C);

    if (!(initGuardMask & 0x1)) {
        prevSource = source;
        initGuardMask |= 0x1;
    }
    if (!(initGuardMask & 0x2)) {
        prevTarget = target;
        initGuardMask |= 0x2;
    }
    if (!(initGuardMask & 0x4)) {
        prevUp = up;
        initGuardMask |= 0x4;
    }
    if (!(initGuardMask & 0x8)) {
        prevBeta = beta;
        initGuardMask |= 0x8;
    }
    if (!(initGuardMask & 0x10)) {
        prevAlpha = alpha;
        initGuardMask |= 0x10;
    }
    if (!(initGuardMask & 0x20)) {
        prevFov = fov;
        initGuardMask |= 0x20;
    }

    if (TheCamera.m_bJust_Switched) {
        prevSource = source;
        prevTarget = target;
        prevUp     = up;
    }

    m_vecSourceSpeedOverOneFrame = source - prevSource;
    m_vecTargetSpeedOverOneFrame = target - prevTarget;
    m_vecUpOverOneFrame          = up - prevUp;

    m_fFovSpeedOverOneFrame = fov - prevFov;

    m_fBetaSpeedOverOneFrame = beta - prevBeta;
    LimitAngleToPi(m_fBetaSpeedOverOneFrame);

    m_fAlphaSpeedOverOneFrame = alpha - prevAlpha;
    LimitAngleToPi(m_fAlphaSpeedOverOneFrame);

    prevSource = source;
    prevTarget = target;
    prevUp     = up;
    prevBeta   = beta;
    prevAlpha  = alpha;
    prevFov    = fov;
}

// 0x520690
void CCam::LookBehind() {
    auto* const target = m_pCamTargetEntity;

    const bool isCarMode = (m_nMode == MODE_CAM_ON_A_STRING || m_nMode == MODE_BEHINDBOAT || m_nMode == MODE_BEHINDCAR) && target->GetIsTypeVehicle();
    const bool is1stPersonVeh = m_nMode == MODE_1STPERSON && target->GetIsTypeVehicle();
    const bool isPed          = target->GetIsTypePed();
    if (!isCarMode && !is1stPersonVeh && !isPed) {
        return;
    }

    CVector targetPos = target->GetPosition();
    m_vecFront        = target->GetPosition() - m_vecSource;

    if (isCarMode) {
        targetPos        = gCamFollowCarLookAt;
        m_bLookingBehind = true;

        const float dist = (m_nMode == MODE_CAM_ON_A_STRING) ? m_fCaMaxDistance : 15.5f;

        m_vecSource   = target->GetMatrix().GetForward();
        m_vecSource.z = m_vecSource.z + 0.2f;
        const float scaledZ = dist * m_vecSource.z;
        m_vecSource.x = targetPos.x + dist * m_vecSource.x;
        m_vecSource.y = targetPos.y + dist * m_vecSource.y;
        m_vecSource.z = scaledZ + targetPos.z;

        CVector lookAt = targetPos;
        CWorld::pIgnoreEntity              = target;
        TheCamera.m_nExtraEntitiesCount    = 0;
        TheCamera.CameraVehicleModeSpecialCases(target->AsVehicle());
        TheCamera.CameraColDetAndReact(&m_vecSource, &lookAt);

        m_vecFront = target->GetPosition() - m_vecSource;
        GetVectorsReadyForRW();
        TheCamera.ImproveNearClip(target->AsVehicle(), nullptr, &m_vecSource, &lookAt);
        CWorld::pIgnoreEntity = nullptr;
    }

    if (is1stPersonVeh) {
        auto* const veh  = target->AsVehicle();
        m_bLookingBehind = true;
        RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.05f);
        m_vecFront = target->GetMatrix().GetForward();
        m_vecFront.Normalise();
        if (veh->m_nVehicleType == VEHICLE_TYPE_BOAT) {
            m_vecSource.z = m_vecSource.z - 0.5f;
        }
        const auto appearance = veh->GetVehicleAppearance();
        if (appearance == VEHICLE_APPEARANCE_BIKE) {
            m_vecSource.x = m_vecFront.x * 2.3f + m_vecSource.x;
            m_vecSource.y = m_vecFront.y * 2.3f + m_vecSource.y;
            m_vecSource.z = m_vecFront.z * 2.3f + m_vecSource.z;
            m_vecFront    = -m_vecFront;
            GetVectorsReadyForRW();
        } else if (appearance == VEHICLE_APPEARANCE_HELI) {
            auto& mat  = target->GetMatrix();
            m_vecFront = mat.GetUp() * -1.0f;
            m_vecUp    = mat.GetForward();
            m_vecSource.x = m_vecFront.x * 0.25f + m_vecSource.x;
            m_vecSource.y = m_vecFront.y * 0.25f + m_vecSource.y;
            m_vecSource.z = m_vecFront.z * 0.25f + m_vecSource.z;
        } else {
            m_vecSource.x = m_vecFront.x * 0.25f + m_vecSource.x;
            m_vecSource.y = m_vecFront.y * 0.25f + m_vecSource.y;
            m_vecSource.z = m_vecFront.z * 0.25f + m_vecSource.z;
            m_vecFront    = -m_vecFront;
        }
    }

    if (isPed) {
        auto* const ped = target->AsPed();

        m_vecSource.x = -std::cos(m_fHorizontalAngle);
        m_vecSource.y = -std::sin(m_fHorizontalAngle);
        m_vecSource.z = 0.0f;
        m_vecSource.z = (m_vecSource.z - (ped->field_578.x * m_vecSource.x + ped->field_578.y * m_vecSource.y + ped->field_578.z * m_vecSource.z)) + 0.3f;
        m_vecSource.Normalise();

        const float dist = std::max(2.0f + TheCamera.m_fPedZoomSmoothed, 0.6f);
        m_vecSource.x    = dist * m_vecSource.x + targetPos.x;
        m_vecSource.y    = dist * m_vecSource.y + targetPos.y;
        m_vecSource.z    = dist * m_vecSource.z + targetPos.z;

        // 0x8CCE18, 0x8CCE24, 0x8CCE30, 0x8CCE3C - indexed by the ped zoom level
        constexpr float SWIM_Z_OFFSET[]    = { 0.65f, 0.0f, 1.0f };
        constexpr float SWIM_LERP_FACTOR[] = { 1.0f, -1.0f, -1.0f };
        constexpr float TARGET_Z_OFFSET[]  = { -1.0f, 0.6f, 0.6f };
        constexpr float SOURCE_Z_OFFSET[]  = { 0.6f, 0.6f, 0.0f };
        const auto      zoom               = TheCamera.m_nPedZoom;
        const float     srcZOffset         = SOURCE_Z_OFFSET[zoom];
        const float     targetZOffset      = TARGET_Z_OFFSET[zoom];

        if (ped->GetIntelligence()->GetTaskSwim()) {
            const float lerpFactor = SWIM_LERP_FACTOR[zoom];
            const float swimZ      = SWIM_Z_OFFSET[zoom];
            const float dz         = targetPos.z - m_vecSource.z;
            m_vecSource.x          = (targetPos.x - m_vecSource.x) * lerpFactor + targetPos.x;
            m_vecSource.y          = (targetPos.y - m_vecSource.y) * lerpFactor + targetPos.y;
            m_vecSource.z          = dz * lerpFactor + targetPos.z;
            m_vecSource.z          = swimZ + m_vecSource.z;
        }
        m_vecSource.z = srcZOffset + m_vecSource.z;
        targetPos.z   = targetPos.z + targetZOffset;

        TheCamera.HandleCameraMotionForDucking(ped, &m_vecSource, &targetPos, false);
        TheCamera.m_nExtraEntitiesCount = 0;
        if (m_pCamTargetEntity) {
            if (auto* const hold = ped->GetIntelligence()->GetTaskHold(false); hold && hold->m_pEntityToHold) {
                TheCamera.m_pExtraEntity[TheCamera.m_nExtraEntitiesCount] = hold->m_pEntityToHold;
                TheCamera.m_nExtraEntitiesCount++;
            }
        }
        CCollision::bCamCollideWithVehicles = true;
        CCollision::bCamCollideWithObjects  = true;
        CCollision::bCamCollideWithPeds     = true;
        TheCamera.CameraColDetAndReact(&m_vecSource, &targetPos);

        m_vecFront = targetPos - m_vecSource;
        GetVectorsReadyForRW();
        TheCamera.ImproveNearClip(nullptr, ped, &m_vecSource, &targetPos);
        if (TheCamera.m_nPedZoom == 1 && 0.05f < RwCameraGetNearClipPlane(Scene.m_pRwCamera)) {
            RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.05f);
        }
    }
    GetVectorsReadyForRW();
}

// 0x520E40
void CCam::LookRight(bool bLookRight) {
    auto* const target = m_pCamTargetEntity;

    const bool isCarMode = (m_nMode == MODE_CAM_ON_A_STRING || m_nMode == MODE_BEHINDBOAT || m_nMode == MODE_BEHINDCAR) && target->GetIsTypeVehicle();
    const bool is1stPersonVeh = m_nMode == MODE_1STPERSON && target->GetIsTypeVehicle();

    float sign = 1.0f;
    if (!bLookRight) {
        m_bLookingLeft = true;
        sign           = -1.0f;
    } else {
        m_bLookingRight = true;
    }

    if (!isCarMode) {
        if (is1stPersonVeh) {
            auto* const veh = target->AsVehicle();
            if (!bLookRight) {
                m_bLookingLeft = true;
            } else {
                m_bLookingRight = true;
            }
            RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.05f);

            if (veh->m_nVehicleType == VEHICLE_TYPE_BOAT) {
                if (!veh->m_pDriver) {
                    m_vecSource.z = m_vecSource.z - 0.5f;
                } else {
                    auto* const driver = veh->m_pDriver;
                    CVector     bonePos{};
                    driver->SetPedPositionInCar();
                    driver->UpdateRwMatrix();
                    driver->UpdateRwFrame();
                    driver->UpdateRpHAnim();
                    driver->GetBonePosition(&bonePos, BONE_NECK, true);

                    const float rightFactor = !bLookRight ? 0.3f : 0.7f;
                    bonePos += target->GetMatrix().GetRight() * rightFactor;

                    auto& mat = target->GetMatrix();
                    bonePos.x = 0.2f * mat.GetUp().x + bonePos.x;
                    bonePos.y = 0.2f * mat.GetUp().y + bonePos.y;
                    bonePos.z = 0.2f * mat.GetUp().z + bonePos.z;
                    m_vecSource = bonePos;
                }
            }
            if (veh->m_nVehicleType != VEHICLE_TYPE_BIKE) {
                const auto& mat = target->GetMatrix();
                m_vecSource.x   = m_vecSource.x - mat.GetRight().x * 0.35f;
                m_vecSource.y   = m_vecSource.y - mat.GetRight().y * 0.35f;
                m_vecSource.z   = m_vecSource.z - mat.GetRight().z * 0.35f;
            }
            m_vecUp = target->GetMatrix().GetUp();
            m_vecUp.Normalise();
            m_vecFront = target->GetMatrix().GetForward();
            m_vecFront.Normalise();
            if (!bLookRight) {
                m_vecFront = CrossProduct(m_vecUp, m_vecFront);
            } else {
                m_vecFront = CrossProduct(m_vecFront, m_vecUp);
            }
            m_vecFront.Normalise();
            if (veh->GetVehicleAppearance() == VEHICLE_APPEARANCE_BIKE) {
                m_vecSource.x = m_vecSource.x - m_vecFront.x * 1.45f;
                m_vecSource.y = m_vecSource.y - m_vecFront.y * 1.45f;
                m_vecSource.z = m_vecSource.z - m_vecFront.z * 1.45f;
            }
        } else if (!target->GetIsTypePed()) {
            return;
        }
        return;
    }

    const CVector targetPos = target->GetPosition();
    float         dist;
    if (m_nMode == MODE_CAM_ON_A_STRING) {
        dist = m_fCaMaxDistance;
    } else {
        dist = 9.0f;
        if (m_nMode == MODE_BEHINDBOAT) {
            float height{};
            if (GetBoatLookLRBehindCamHeight(target, height) && !CCullZones::Cam1stPersonForPlayer()) {
                m_vecSource.z = targetPos.z + height;
            }
        }
    }

    auto dir = target->GetMatrix().GetForward();
    dir.Normalise();
    const double angle = (double)sign * (double)1.57079637f + (double)CGeneral::GetATanOfXY(dir.x, dir.y); // NOTE: x87 keeps this in extended precision
    m_vecSource.x      = (float)(std::cos(angle) * dist + targetPos.x);
    m_vecSource.y      = (float)(std::sin(angle) * dist + targetPos.y);

    auto* const colModel = target->GetColModel();
    const float oldZ     = m_vecSource.z;
    CVector     lookAt   = targetPos;
    CWorld::pIgnoreEntity           = target;
    TheCamera.m_nExtraEntitiesCount = 0;
    TheCamera.CameraVehicleModeSpecialCases(target->AsVehicle());
    TheCamera.CameraColDetAndReact(&m_vecSource, &lookAt);
    CWorld::pIgnoreEntity = nullptr;

    const CVector pos = target->GetPosition();
    float         zOffset;
    if (!bLookRight) {
        zOffset = colModel->m_boundBox.m_vecMax.x * target->GetMatrix().GetRight().z;
    } else {
        zOffset = target->GetMatrix().GetRight().z * colModel->m_boundBox.m_vecMin.x;
    }
    float z = pos.z + zOffset;
    z       = colModel->m_boundBox.m_vecMax.z * target->GetMatrix().GetUp().z + z;

    float result = oldZ;
    if (!(oldZ < 0.1f + std::max(z, m_vecTargetCoorsForFudgeInter.z))) {
        result = std::max(z, m_vecTargetCoorsForFudgeInter.z) + 0.1f;
    }
    m_vecSource.z = std::max(result, m_vecSource.z);

    m_vecFront = target->GetPosition() - m_vecSource;
    m_vecFront.z += 1.1f;
    if (m_nMode == MODE_BEHINDBOAT) {
        m_vecFront.z += 1.2f;
    }
    GetVectorsReadyForRW();
}

// 0x50A4F0
void CCam::RotCamIfInFrontCar(const CVector& target, float targetOrientation) {
    if (!m_pCamTargetEntity->GetIsTypeVehicle()) {
        return;
    }
    auto* const veh = m_pCamTargetEntity->AsVehicle();

    const auto& moveSpeed     = veh->m_vecMoveSpeed;
    const bool  isMovingFwd   = DotProduct(veh->GetMatrix().GetForward(), moveSpeed) > 0.1f;
    if (sq(moveSpeed.x) + sq(moveSpeed.y) > 0.0036f) {
        targetOrientation = (float)(std::atan2((double)-moveSpeed.x, (double)moveSpeed.y) - (double)(PI / 2.0f));
    }

    // NOTE: Unlike `WrapAngleToPi` the original loops here use strict comparisons
    const auto WrapDiff = [](float a) {
        while (a > PI) {
            a -= 2.0f * PI;
        }
        while (a < -PI) {
            a += 2.0f * PI;
        }
        return a;
    };

    const float dist2D = (CVector2D{ m_vecSource } - CVector2D{ target }).Magnitude();

    if (std::abs(WrapDiff(targetOrientation - m_fHorizontalAngle)) > 0.349065781f && isMovingFwd && !TheCamera.m_bTransitionState) {
        m_bFixingBeta = true;
    }

    auto* const pad = CPad::GetPad(0);
    if (!pad->GetLookBehindForCar() && !pad->GetLookBehindForPed() && !pad->GetLookLeft() && !pad->GetLookRight() && m_nDirectionWasLooking != 3) {
        TheCamera.m_bCamDirectlyBehind = true;
    }

    if (!m_bFixingBeta && !TheCamera.m_bUseTransitionBeta && !TheCamera.m_bCamDirectlyBehind && !TheCamera.m_bCamDirectlyInFront) {
        return;
    }

    const bool isActiveCam = (TheCamera.m_bCamDirectlyBehind || TheCamera.m_bCamDirectlyInFront || TheCamera.m_bUseTransitionBeta) && &TheCamera.GetActiveCam() == this;
    if (m_bFixingBeta || isActiveCam) {
        WellBufferMe(targetOrientation, m_fHorizontalAngle, m_fBetaSpeed, 0.1f, 0.003f, true);

        const bool isActive = &TheCamera.GetActiveCam() == this;
        if (TheCamera.m_bCamDirectlyBehind && isActive) {
            m_fHorizontalAngle = targetOrientation;
        }
        if (TheCamera.m_bCamDirectlyInFront && isActive) {
            m_fHorizontalAngle = targetOrientation + PI;
        }
        if (TheCamera.m_bUseTransitionBeta && isActive) {
            m_fHorizontalAngle = m_fTransitionBeta;
        }

        m_vecSource.x = (float)((double)target.x - -(std::cos((double)m_fHorizontalAngle) * (double)dist2D));
        m_vecSource.y = (float)((double)target.y - -(std::sin((double)m_fHorizontalAngle) * (double)dist2D));

        if (std::abs(WrapDiff(targetOrientation - m_fHorizontalAngle)) < 0.0349065848f) {
            m_bFixingBeta = false;
        }
    }
    TheCamera.m_bCamDirectlyBehind = false;
    TheCamera.m_bCamDirectlyInFront = false;
}

// 0x50A850
bool CCam::Using3rdPersonMouseCam() const {
    return CCamera::m_bUseMouse3rdPerson && m_nMode == MODE_FOLLOWPED;
}

// 0x509DC0
bool CCam::GetWeaponFirstPersonOn() {
    return m_pCamTargetEntity && m_pCamTargetEntity->GetIsTypePed() && m_pCamTargetEntity->AsPed()->GetActiveWeapon().m_IsFirstPersonWeaponModeSelected;
}

// inlined -- alpha = vertical angle
void CCam::ClipAlpha() {
    m_fVerticalAngle = std::clamp(
        m_fVerticalAngle,
        DegreesToRadians(-85.5f),
        DegreesToRadians(+60.0f)
    );
}

// 0x509C50 -- beta = horizontal angle
void CCam::ClipBeta() {
    // BUG: This used to always subtract 2*PI unless the angle was below -PI, the original (verified against the bytes at 0x509C50) wraps into [-PI, PI]
    if (m_fHorizontalAngle > PI) {
        m_fHorizontalAngle -= 2.0f * PI;
    } else if (m_fHorizontalAngle < -PI) {
        m_fHorizontalAngle += 2.0f * PI;
    }
}

// 0x526FC0
void CCam::Process() {
    // Globals whose purpose is unknown (names made up)
    static auto& s_unk_B6FDC8       = StaticRef<float>(0xB6FDC8);
    static auto& s_unk_C0B184       = StaticRef<uint8>(0xC0B184);
    static auto& s_unk_C8A860       = StaticRef<uint8>(0xC8A860);
    static auto& s_unk_8CCF00       = StaticRef<bool>(0x8CCF00);
    static auto& s_firstPersonFlag  = StaticRef<bool>(0xB6EC20);

    if ((float)gCamUnkB6FE34 <= s_unk_B6FDC8) {
        s_unk_C0B184 &= 0xFE;
    }
    if (TheCamera.m_aCams[TheCamera.m_nActiveCam].m_nMode != MODE_FOLLOWPED) {
        s_unk_C0B184 &= 0xFE;
        gCamUnkB6FE34 = 0;
    }

    float   orientation = 0.0f; // local_78
    float   speedVar    = 0.0f; // local_7c
    CVector target{};           // local_6c

    if (!m_pCamTargetEntity) {
        m_pCamTargetEntity = TheCamera.m_pTargetEntity;
        m_pCamTargetEntity->RegisterReference(&m_pCamTargetEntity);
    }

    if (s_unk_C8A860 == 1) {
        auto* const player = FindPlayerPed();
        if (!player || !player->m_pVehicle || player->m_pVehicle->m_nModelIndex != 0x208) {
            CWeaponEffects::ClearCrossHairImmediately(CrossHairId(0));
        }
    }

    m_nFrameNumWereAt++;
    if (m_nDoCollisionCheckEveryNumOfFrames < m_nFrameNumWereAt) {
        m_nFrameNumWereAt = 1;
    }
    m_bCollisionChecksOn = m_nFrameNumWereAt == m_nDoCollisionChecksOnFrameNum;

    const auto GetAngleOfEntityForward = [](CEntity* e) {
        auto& fwd = e->GetMatrix().GetForward();
        if (fwd.x == 0.0f && fwd.y == 0.0f) {
            return 0.0f;
        }
        return CGeneral::GetATanOfXY(e->GetMatrix().GetForward().x, e->GetMatrix().GetForward().y);
    };

    if (!m_bCamLookingAtVector) {
        auto* const ent = m_pCamTargetEntity;
        if (ent->GetIsTypeVehicle()) {
            target      = ent->GetPosition();
            orientation = GetAngleOfEntityForward(ent);

            CVector fwd2D{ ent->GetMatrix().GetForward().x, ent->GetMatrix().GetForward().y, 0.0f };
            fwd2D.Normalise();
            float len = std::sqrt(fwd2D.x * fwd2D.x + fwd2D.y * fwd2D.y);
            if (len != 0.0f) {
                len = 1.0f / len;
                fwd2D.x = fwd2D.x * len;
                fwd2D.y = len * fwd2D.y;
            }
            const float vx = fwd2D.x * ent->AsPhysical()->m_vecMoveSpeed.x;
            const float vy = fwd2D.y * ent->AsPhysical()->m_vecMoveSpeed.y;
            speedVar       = std::sqrt(vx * vx + vy * vy);
            if (vy + vx <= 0.0f) {
                speedVar = speedVar * 0.555555582f;
                if (0.5f < speedVar) {
                    speedVar = 0.5f;
                }
                speedVar = -speedVar;
            } else {
                speedVar = speedVar * 1.11111116f;
                if (1.0f < speedVar) {
                    speedVar = 1.0f;
                }
            }
            m_fSpeedVar = m_fSpeedVar * 0.894999981f + speedVar * 0.105000019f;

            if (m_nDirectionWasLooking != 3) {
                auto* const pad = CPad::GetPad(0);
                if (pad->GetLookBehindForCar() && (pad->GetLookLeft() || pad->GetLookRight())) {
                    TheCamera.m_bCamDirectlyBehind = true;
                }
            }
        } else {
            auto* const player = FindPlayerPed();
            if (ent == player) {
                CVector playerPos = FindPlayerPed()->GetPosition();
                if (FindPlayerPed()->GetIntelligence()->GetTaskClimb()) {
                    FindPlayerPed()->GetIntelligence()->GetTaskClimb()->GetCameraTargetPos(FindPlayerPed(), playerPos);
                }
                target = playerPos;

                bool bReset =
                    9.0f < sq(gCamPlayerLastPos.z - playerPos.z) + sq(gCamPlayerLastPos.y - playerPos.y) + sq(gCamPlayerLastPos.x - playerPos.x) // NOTE: summation order as in the original
                    || CTimer::GetTimeStep() < 0.2f
                    || Using3rdPersonMouseCam()
                    || TheCamera.m_bCamDirectlyBehind
                    || TheCamera.m_bCamDirectlyInFront;
                if (!bReset) {
                    if (FindPlayerPed()->GetIntelligence()->GetTaskFighting() && m_nMode == MODE_AIMWEAPON) {
                        const float f = (float)std::pow(0.899999976, (double)CTimer::GetTimeStep());
                        target        = playerPos * (1.0f - f) + gCamPlayerLastPos * f;
                        bReset        = true;
                    } else {
                        // NOTE: The original computes `1 - pow(..)` first (rounded to float) and derives the other weight from it
                        const float inv1 = (float)(1.0 - std::pow(0.600000024, (double)CTimer::GetTimeStep()));
                        const float inv2 = (float)(1.0 - std::pow(0.800000012, (double)CTimer::GetTimeStep()));
                        const CVector predicted = gCamPlayerLastPos + gCamPlayerPosVel * CTimer::GetTimeStep();
                        const CVector blended   = predicted * (1.0f - inv1) + playerPos * inv1;
                        target.x                = blended.x;
                        target.y                = blended.y;
                        target.z                = playerPos.z;

                        const CVector delta{ target.x - gCamPlayerLastPos.x, target.y - gCamPlayerLastPos.y, playerPos.z - gCamPlayerLastPos.z };
                        const float   divisor = std::max(1.0f, CTimer::GetTimeStep());
                        const CVector newVel  = gCamPlayerPosVel * (1.0f - inv2) + (delta * inv2) / divisor;
                        gCamPlayerPosVel.x    = newVel.x;
                        gCamPlayerPosVel.y    = newVel.y;
                    }
                }
                if (bReset) {
                    gCamPlayerPosVel.x = 0.0f;
                    gCamPlayerPosVel.y = 0.0f;
                }
                gCamPlayerPosVel.z = 0.0f;
                gCamPlayerLastPos  = target;
            } else {
                target = ent->GetPosition();
            }
            orientation = GetAngleOfEntityForward(m_pCamTargetEntity);
            speedVar    = 0.0f;
            m_fSpeedVar = 0.0f;
        }
    } else {
        target = m_vecCamFixedModeVector;
    }

    // Look behind / left / right handling
    m_nDirectionWasLooking = gCameraDirection;
    gCameraDirection       = 3;
    if (&TheCamera.m_aCams[TheCamera.m_nActiveCam] == this) {
        auto* const ent = m_pCamTargetEntity;
        bool        bTransitionState = TheCamera.m_bTransitionState;
        auto* const pad              = CPad::GetPad(0);
        bool        skipRest         = false;

        if ((m_nMode == MODE_CAM_ON_A_STRING || m_nMode == MODE_1STPERSON || m_nMode == MODE_BEHINDBOAT || m_nMode == MODE_BEHINDCAR) && ent->GetIsTypeVehicle()) {
            const bool bHeliOrPlane = ent->AsVehicle()->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI || ent->AsVehicle()->GetVehicleAppearance() == VEHICLE_APPEARANCE_PLANE;
            if (!pad->GetLookBehindForCar()) {
                const auto ClearTransition = [&] {
                    TheCamera.m_bWaitForInterpolToFinish = bHeliOrPlane;
                    TheCamera.m_bDoingSpecialInterp      = bHeliOrPlane;
                    TheCamera.m_bTransitionState         = bHeliOrPlane;
                };
                if (!pad->GetLookLeft() || bHeliOrPlane) {
                    if (!pad->GetLookRight() || bHeliOrPlane) {
                        gCameraDirection = 3;
                    } else {
                        gCameraDirection = 2;
                        ClearTransition();
                    }
                } else {
                    gCameraDirection = 1;
                    ClearTransition();
                }
                if (m_nDirectionWasLooking != (uint32)gCameraDirection) {
                    TheCamera.m_bJust_Switched = true;
                }
            } else {
                TheCamera.m_bTransitionState         = false;
                TheCamera.m_bDoingSpecialInterp      = false;
                TheCamera.m_bWaitForInterpolToFinish = false;
                bTransitionState                     = m_nDirectionWasLooking == 0;
                if (!bTransitionState) {
                    TheCamera.m_bJust_Switched = true;
                }
                gCameraDirection = 0;
            }
            skipRest = true;
        } else if (m_nMode == MODE_FOLLOWPED && ent->GetIsTypePed()) {
            if (pad->GetLookBehindForPed()) {
                if (m_nDirectionWasLooking != 0 && !bTransitionState) {
                    TheCamera.m_bJust_Switched = true;
                }
                gCameraDirection = 0;
                skipRest         = true;
            }
        } else if (m_nMode != MODE_AIMWEAPON) {
            skipRest = true;
        }
        if (!skipRest) {
            gCameraDirection = 3;
            if (m_nDirectionWasLooking != 3) {
                gCurDistForCam = 1.0f;
            }
        }
    }

    if (TheCamera.m_bJust_Switched) {
        gCurDistForCam                = 1.0f;
        TheCamera.m_bResetOldMatrix   = true;
    }

    if (m_nMode != MODE_BEHINDCAR && m_nMode != MODE_CAM_ON_A_STRING && m_nMode != MODE_BEHINDBOAT && m_nMode != MODE_1STPERSON && m_nMode != MODE_TWOPLAYER_IN_CAR_AND_SHOOTING) {
        CPostEffects::m_bSpeedFXUserFlagCurrentFrame = false;
    }
    s_firstPersonFlag = false;

    switch (m_nMode) {
    case MODE_BEHINDCAR:
    case MODE_CAM_ON_A_STRING:
    case MODE_BEHINDBOAT:
        Process_FollowCar_SA(target, orientation, m_fSpeedVar, speedVar, false);
        break;
    case MODE_FOLLOWPED:
        if (!CCamera::m_bUseMouse3rdPerson || s_unk_8CCF00) {
            Process_FollowPed_SA(target, orientation, m_fSpeedVar, speedVar, false);
        } else {
            Process_FollowPedWithMouse(target, orientation, m_fSpeedVar, speedVar);
        }
        break;
    default:
        m_vecSource = CVector{ 0.0f, 0.0f, 0.0f };
        m_vecFront  = CVector{ 0.0f, 1.0f, 0.0f };
        m_vecUp     = CVector{ 0.0f, 0.0f, 1.0f };
        break;
    case MODE_SNIPER:
    case MODE_M16_1STPERSON:
    case MODE_HELICANNON_1STPERSON:
    case MODE_CAMERA:
        Process_M16_1stPerson(target, orientation, m_fSpeedVar, speedVar);
        break;
    case MODE_ROCKETLAUNCHER:
        Process_Rocket(target, orientation, m_fSpeedVar, speedVar, false);
        break;
    case MODE_WHEELCAM:
        Process_WheelCam(target, orientation, m_fSpeedVar, speedVar);
        break;
    case MODE_FIXED:
        Process_Fixed(target, orientation, m_fSpeedVar, speedVar);
        break;
    case MODE_1STPERSON:
        Process_1stPerson(target, orientation, m_fSpeedVar, speedVar);
        break;
    case MODE_FLYBY:
        Process_FlyBy(target, orientation, m_fSpeedVar, speedVar);
        break;
    case MODE_PED_DEAD_BABY:
        ProcessPedsDeadBaby();
        TheCamera.m_bPlayerIsInGarage    = false;
        TheCamera.m_bJustCameOutOfGarage = false;
        break;
    case MODE_ARRESTCAM_ONE:
        ProcessArrestCamOne();
        break;
    case MODE_ARRESTCAM_TWO:
        break;
    case MODE_SPECIAL_FIXED_FOR_SYPHON:
        Process_SpecialFixedForSyphon(target, orientation, m_fSpeedVar, speedVar);
        break;
    case MODE_SNIPER_RUNABOUT:
    case MODE_ROCKETLAUNCHER_RUNABOUT:
    case MODE_1STPERSON_RUNABOUT:
    case MODE_M16_1STPERSON_RUNABOUT:
    case MODE_FIGHT_CAM_RUNABOUT:
    case MODE_ROCKETLAUNCHER_RUNABOUT_HS:
        Process_1rstPersonPedOnPC(target, orientation, m_fSpeedVar, speedVar);
        break;
    case MODE_EDITOR:
        Process_Editor(target, orientation, m_fSpeedVar, speedVar);
        break;
    case MODE_ATTACHCAM:
        Process_AttachedCam();
        break;
    case MODE_TWOPLAYER:
        Process_Cam_TwoPlayer();
        break;
    case MODE_TWOPLAYER_IN_CAR_AND_SHOOTING:
        Process_Cam_TwoPlayer_InCarAndShooting();
        break;
    case MODE_TWOPLAYER_SEPARATE_CARS:
        Process_Cam_TwoPlayer_Separate_Cars();
        break;
    case MODE_ROCKETLAUNCHER_HS:
        Process_Rocket(target, orientation, m_fSpeedVar, speedVar, true);
        break;
    case MODE_AIMWEAPON:
    case MODE_AIMWEAPON_FROMCAR:
    case MODE_AIMWEAPON_ATTACHED:
        Process_AimWeapon(target, orientation, m_fSpeedVar, speedVar);
        break;
    case MODE_TWOPLAYER_SEPARATE_CARS_TOPDOWN:
        Process_Cam_TwoPlayer_Separate_Cars_TopDown();
        break;
    case MODE_DW_HELI_CHASE:
        Process_DW_HeliChaseCam(false);
        break;
    case MODE_DW_CAM_MAN:
        Process_DW_CamManCam(false);
        break;
    case MODE_DW_BIRDY:
        Process_DW_BirdyCam(false);
        break;
    case MODE_DW_PLANE_SPOTTER:
        Process_DW_PlaneSpotterCam(false);
        break;
    case MODE_DW_DOG_FIGHT:
    case MODE_DW_FISH:
        TheCamera.m_bUseNearClipScript = false;
        break;
    case MODE_DW_PLANECAM1:
        Process_DW_PlaneCam1(false);
        break;
    case MODE_DW_PLANECAM2:
        Process_DW_PlaneCam2(false);
        break;
    case MODE_DW_PLANECAM3:
        Process_DW_PlaneCam3(false);
        break;
    }

    if (m_nMode < MODE_DW_HELI_CHASE || m_nMode > MODE_DW_PLANECAM3) {
        gLastDWCineyCamMode = -1;
    }
    gCameraMode = m_nMode;

    CVector diff = m_vecSource - m_vecTargetCoorsForFudgeInter;
    m_fTrueBeta  = CGeneral::GetATanOfXY(diff.x, diff.y);
    m_fTrueAlpha = CGeneral::GetATanOfXY(std::sqrt(diff.x * diff.x + diff.y * diff.y), diff.z);

    if (!TheCamera.m_bTransitionState) {
        KeepTrackOfTheSpeed(m_vecSource, m_vecTargetCoorsForFudgeInter, m_vecUp, m_fTrueAlpha, m_fTrueBeta, m_fFOV);
    }

    m_vecSourceBeforeLookBehind = m_vecSource;
    m_bLookingRight  = false;
    m_bLookingLeft   = false;
    m_bLookingBehind = false;

    if (&TheCamera.m_aCams[TheCamera.m_nActiveCam] == this) {
        switch (gCameraDirection) {
        case 0: LookBehind(); break;
        case 1: LookRight(false); break;
        case 2: LookRight(true); break;
        }
        m_nDirectionWasLooking = gCameraDirection;
    }

    if (TheCamera.m_bFOVLerpProcessed) {
        m_fFOV                         = TheCamera.m_fFOVNew;
        TheCamera.m_bFOVLerpProcessed = false;
    }
    if (TheCamera.m_bVecMoveLinearProcessed) {
        m_vecSource                           = TheCamera.m_vecMoveLinear;
        TheCamera.m_bVecMoveLinearProcessed = false;
    }
    if (TheCamera.m_bVecTrackLinearProcessed) {
        m_vecFront = TheCamera.m_vecTrackLinear - m_vecSource;
        m_vecFront.Normalise();
        GetVectorsReadyForRW();
        TheCamera.m_bVecTrackLinearProcessed = false;
    }
}

// 0x518500
void CCam::ProcessArrestCamOne() {
    // NOTSA: Not reversed yet, forwards to the original code (the hook is disabled, see `InjectHooks`)
    plugin::CallMethod<0x518500, CCam*>(this);
}

// 0x519250
void CCam::ProcessPedsDeadBaby() {
    static auto& s_startTime = StaticRef<float>(0xB70054); // NOTE: write-only as far as this function is concerned
    static auto& s_unused    = StaticRef<float>(0xB70050); // ^

    auto* const target = TheCamera.m_pTargetEntity;

    CVector targetPos;
    if (target->GetIsTypePed()) {
        target->AsPed()->GetBonePosition(&targetPos, BONE_SPINE1, true);
    } else if (target->GetIsTypeVehicle()) {
        targetPos = target->GetPosition();
        targetPos.z += target->GetColModel()->m_boundBox.m_vecMax.z;
    } else {
        return;
    }

    float srcX = targetPos.x;
    float srcY = targetPos.y;
    float srcZ{};

    if (!m_bResetStatics) {
        srcX = m_vecSource.x;
        srcY = m_vecSource.y;
        srcZ = m_vecSource.z;
        const float prevX = srcX;
        const float prevY = srcY;

        if (!CWorld::TestSphereAgainstWorld({ srcX, srcY, srcZ + 0.2f }, 0.3f, TheCamera.m_pTargetEntity, true, true, false, true, false, true)) {
            srcZ = CTimer::GetTimeStep() * 0.04f + srcZ;
        }

        const auto fwd = target->GetMatrix().GetForward();
        auto       right = CrossProduct(fwd, CVector{ 0.0f, 0.0f, 1.0f });
        right.z          = 0.0f;
        right.Normalise();

        const float elapsedTime = (float)(CTimer::GetTimeInMS() - TheCamera.m_nTimeLastChange);
        const float clamped     = std::min(elapsedTime, 1000.0f);
        const float savedZ      = srcZ;
        const double orbit      = (double)clamped * 0.001f * std::sin((double)elapsedTime / 600.0);

        const float candX = (float)((double)right.x * 2.0 * orbit + targetPos.x);
        const float candY = (float)((double)(right.y * 2.0f) * orbit) + targetPos.y;

        CVector dir{ candX - prevX, candY - prevY, 0.0f };
        dir.Normalise();
        const CVector probe{ dir.x * 0.2f + prevX, dir.y * 0.2f + prevY, (0.0f * 0.2f) + srcZ };
        if (!CWorld::TestSphereAgainstWorld(probe, 0.3f, TheCamera.m_pTargetEntity, true, true, false, true, false, true)) {
            srcZ = savedZ;
            srcY = candY;
            srcX = candX;
        }

        m_vecFront = CVector{ 0.0f, 0.0f, -1.0f };
        float fVar4 = std::min(elapsedTime, 2000.0f);
        fVar4       = fVar4 * 0.0005f;
        const float cosv = (float)std::cos((double)elapsedTime / 600.0);
        m_vecFront.x = right.x * -0.35f * fVar4 * cosv + m_vecFront.x;
        m_vecFront.y = (right.y * -0.35f) * fVar4 * cosv + m_vecFront.y;
        m_vecFront.z = (-0.35f * right.z * fVar4) * cosv + m_vecFront.z;
        m_vecFront.Normalise();
        m_vecUp = CrossProduct(right, m_vecFront);
        m_vecUp.Normalise();
    } else {
        s_startTime = (float)CTimer::GetTimeInMS();
        srcZ        = 2.0f + targetPos.z;
        TheCamera.m_nTimeLastChange = CTimer::GetTimeInMS(); // NOTE: 0xB6F094
        s_unused = 0.0f;
        const float prevX = targetPos.x;
        const float prevY = targetPos.y;

        float waterLevel = 0.0f;
        if (CWaterLevel::GetWaterLevelNoWaves({ targetPos.x, targetPos.y, srcZ }, &waterLevel) && srcZ < waterLevel + 1.5f) {
            srcZ = waterLevel + 1.5f;
        }

        const auto fwd   = target->GetMatrix().GetForward();
        auto       right = CrossProduct(fwd, CVector{ 0.0f, 0.0f, 1.0f });
        right.z          = 0.0f;
        right.Normalise();

        m_vecFront = CVector{ targetPos.x - prevX, targetPos.y - prevY, targetPos.z - srcZ };
        m_vecFront.Normalise();
        m_vecUp = CrossProduct(right, m_vecFront);
        m_vecUp.Normalise();
        m_bResetStatics = false;
    }

    m_vecSource = CVector{ srcX, srcY, srcZ };
    AvoidTheGeometry(CVector{ srcX, srcY, srcZ }, targetPos, m_vecSource, m_fFOV);
    TheCamera.m_bMoveCamToAvoidGeom = false;
}

// 0x50EB70
void CCam::Process_1rstPersonPedOnPC(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    static auto& v3d_8CCC54  = StaticRef<CVector>(0x8CCC54);
    static auto& byte_B6FFDC = StaticRef<bool>(0xB6FFDC);
    static auto& v3d_B6FFC4  = StaticRef<CVector>(0xB6FFC4);
    static auto& v3d_B6FFD0  = StaticRef<CVector>(0xB6FFD0);
    static auto& guard_B6FFE0 = StaticRef<uint32>(0xB6FFE0); // MSVC static init guard, nothing else reads it

    guard_B6FFE0 |= 1;
    if (m_nMode != MODE_SNIPER_RUNABOUT) {
        m_fFOV = 70.0f;
    }

    TheCamera.m_b1rstPersonRunCloseToAWall = false;
    if (!m_pCamTargetEntity->GetRwObject()) {
        TheCamera.m_b1rstPersonRunCloseToAWall = false;
        return;
    }

    const auto FinishUp = [this] {
        m_bResetStatics = false;
        RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.05f);
    };

    if (!m_pCamTargetEntity->GetIsTypePed()) {
        FinishUp();
        return;
    }
    auto* const targetPed = m_pCamTargetEntity->AsPed();

    const auto hier = GetAnimHierarchyFromSkinClump(m_pCamTargetEntity->GetRpClump());
    const auto aIdx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(2)); // todo: enum
    auto&      aMat = RpHAnimHierarchyGetMatrixArray(hier)[aIdx];

    CVector pointIn = v3d_8CCC54;
    RwV3dTransformPoints(&pointIn, &pointIn, 1, &aMat);
    RwV3d v3dZero{ 0.0f, 0.0f, 0.0f };
    RwMatrixScale(&aMat, &v3dZero, rwCOMBINEPRECONCAT);

    if (m_bResetStatics) {
        m_fHorizontalAngle          = orientation;
        m_fVerticalAngle            = 0.0f;
        m_fInitialPlayerOrientation = orientation;
        if (m_pCamTargetEntity->GetIsTypePed()) {
            m_fVerticalAngle = 0.0f;
            byte_B6FFDC      = false;
            v3d_B6FFD0.y     = 0.0f;
            v3d_B6FFD0.z     = 0.0f;
            m_fHorizontalAngle          = targetPed->m_fCurrentRotation + PI / 2.0f;
            m_bCollisionChecksOn        = true;
            m_fInitialPlayerOrientation = targetPed->m_fCurrentRotation + PI / 2.0f;
        }
        m_vecBufferedPlayerBodyOffset = pointIn;
        v3d_B6FFD0.x                  = 0.0f;
        v3d_B6FFC4                    = pointIn;
    }
    m_vecBufferedPlayerBodyOffset.y = pointIn.y;

    if (!TheCamera.m_bHeadBob) {
        const float dx = pointIn.x - v3d_B6FFC4.x;
        const float dy = pointIn.y - v3d_B6FFC4.y;
        CVector     fwd2D = targetPed->GetMatrix().GetForward();
        fwd2D.z           = 0.0f;
        fwd2D.Normalise();
        const float mag   = std::sqrt(dy * dy + dx * dx);
        const auto& pos   = targetPed->GetPosition();
        pointIn.x         = fwd2D.x * mag * 1.23f + pos.x;
        pointIn.y         = fwd2D.y * mag * 1.23f + pos.y;
        pointIn.z         = (mag * 0.0f * 1.23f + pos.z) + 0.59f;
    } else {
        const float g = TheCamera.m_fGaitSwayBuffer;
        m_vecBufferedPlayerBodyOffset.x = g * m_vecBufferedPlayerBodyOffset.x + (1.0f - g) * pointIn.x;
        m_vecBufferedPlayerBodyOffset.z = g * m_vecBufferedPlayerBodyOffset.z + (1.0f - g) * pointIn.z;
        pointIn = targetPed->GetMatrix().TransformPoint(m_vecBufferedPlayerBodyOffset);
    }
    m_vecSource = pointIn;

    CVector spinePos{};
    targetPed->GetTransformedBonePosition(spinePos, BONE_SPINE1, true);

    auto*      pad   = CPad::GetPad(0);
    const auto mouse = CPad::NewMouseControllerState.GetAmountMouseMoved();
    double     deltaH, deltaV; // NOTE: x87 extended precision in the original
    float      stickV;         // the original reuses `param_3` for this
    if (mouse.x == 0.0f && mouse.y == 0.0f) {
        const int16 lookLR = plugin::CallMethodAndReturn<int16, 0x540E80, CPad*>(pad); // LookAroundLeftRight(void)
        const float stickH = (float)-(int32)lookLR;
        const int16 lookUD = plugin::CallMethodAndReturn<int16, 0x540F80, CPad*>(pad); // LookAroundUpDown(void)
        stickV             = (float)(int32)lookUD;
        float signV        = 1.0f;
        float signH        = 1.0f;
        if (stickH < 0.0f) {
            signH = -1.0f;
        }
        if (stickV < 0.0f) {
            signV = -1.0f;
        }
        const float fovScale = m_fFOV * 0.0125f;
        deltaH               = ((double)stickH * stickH * 0.0001f) * ((double)fovScale * 0.0571428575f) * signH * CTimer::GetTimeStep();
        deltaV               = ((double)stickV * stickV * 4.44444449e-05f) * ((double)fovScale * 0.0714285746f) * signV * CTimer::GetTimeStep();
    } else {
        stickV               = mouse.y * 4.0f;
        const float fovScale = m_fFOV * 0.0125f;
        deltaH               = ((double)mouse.x * -3.0f) * ((double)fovScale * CCamera::m_fMouseAccelHorzntl);
        deltaV               = ((double)fovScale * CCamera::m_fMouseAccelVertical) * stickV;
    }
    m_fHorizontalAngle = (float)(deltaH + (double)m_fHorizontalAngle);
    m_fVerticalAngle   = (float)(deltaV + (double)m_fVerticalAngle);
    ClipBeta();

    if (m_fVerticalAngle <= 1.04719758f) {
        if (m_fVerticalAngle < -1.56206977f) {
            m_fVerticalAngle = -1.56206977f;
        }
    } else {
        m_fVerticalAngle = 1.04719758f;
    }

    if (targetPed->IsPlayer() && targetPed->m_pAttachedTo) {
        double base;
        switch (targetPed->m_fTurretAngleA) {
        case 0:  base = (double)targetPed->m_pAttachedTo->GetHeading() + (double)(PI / 2.0f); break;
        case 1:  base = (double)targetPed->m_pAttachedTo->GetHeading() + (double)PI; break;
        case 2:  base = (double)targetPed->m_pAttachedTo->GetHeading() - (double)(PI / 2.0f); break;
        case 3:  base = (double)targetPed->m_pAttachedTo->GetHeading(); break;
        default: base = (double)stickV; break; // BUG: the original uses the (reused) vertical stick/mouse variable here
        }
        double diff = (double)m_fHorizontalAngle - base;
        if (diff <= (double)PI) {
            if (diff < (double)-PI) {
                diff += 2.0f * PI;
            }
        } else {
            diff -= 2.0f * PI;
        }
        const double limit = targetPed->m_fTurretAngleB;
        if (diff <= limit) {
            if (diff < -limit) {
                diff = -limit;
            }
        } else {
            diff = limit;
        }
        m_fHorizontalAngle = (float)(diff + base);
    }

    const double cosA = std::cos((double)m_fVerticalAngle);
    const double cosH = std::cos((double)m_fHorizontalAngle);
    const double sinH = std::sin((double)m_fHorizontalAngle);
    const float  sinA3 = (float)std::sin((double)m_fVerticalAngle) * 3.0f;
    m_vecFront.x = (float)(cosH * cosA * 3.0 + (double)m_vecSource.x) - m_vecSource.x;
    m_vecFront.y = ((float)(sinH * cosA) * 3.0f + m_vecSource.y) - m_vecSource.y;
    m_vecFront.z = (sinA3 + m_vecSource.z) - m_vecSource.z;
    m_vecFront.Normalise();
    m_vecSource.x = m_vecFront.x * 0.4f + m_vecSource.x;
    m_vecSource.y = m_vecFront.y * 0.4f + m_vecSource.y;
    m_vecSource.z = m_vecFront.z * 0.4f + m_vecSource.z;
    TheCamera.m_fAlphaForPlayerAnim1rstPerson = m_fVerticalAngle;
    GetVectorsReadyForRW();

    {
        const float heading = (float)std::atan2((double)-m_vecFront.x, (double)m_vecFront.y);
        auto* const ped     = TheCamera.m_pTargetEntity->AsPed();
        ped->m_fCurrentRotation = heading;
        ped->m_fAimingRotation  = heading;
        ped->SetHeading(heading);
        ped->UpdateRwMatrix();
    }

    if (m_nMode == MODE_SNIPER_RUNABOUT) {
        const bool zoomOut = pad->SniperZoomOut();
        const bool zoomIn  = zoomOut || pad->SniperZoomIn();
        if (zoomIn) {
            const float factor = CTimer::GetTimeStep() * 255.0f + 10000.0f;
            if (pad->SniperZoomOut()) {
                m_fFOV = factor * m_fFOV * 0.0001f;
            } else if (pad->SniperZoomIn()) {
                m_fFOV = m_fFOV / (factor * 0.0001f);
            }
        }
        TheCamera.SetMotionBlur(180, 255, 180, 120, eMotionBlurType::SNIPER);
        if (70.0f < m_fFOV) {
            m_fFOV = 70.0f;
        }
        if (m_fFOV < 15.0f) {
            m_fFOV = 15.0f;
        }
    }
    FinishUp();
}

// 0x517EA0
void CCam::Process_1stPerson(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    static auto& s_LastWheelieTime = StaticRef<float>(0x8CCD14);
    // Making sure player doesn't see below ground when flipped.
    // Name is made up cuz I found it funny to name it like that.
    static auto& s_GroundFaultProtection = StaticRef<float>(0xB7004C);

    gbFirstPersonRunThisFrame = true;

    m_fFOV = 70.0f;
    if (!m_pCamTargetEntity->GetRwObject()) {
        return;
    }

    if (m_bResetStatics) {
        m_fVerticalAngle   = 0.0f;
        m_fHorizontalAngle = [&] {
            if (m_pCamTargetEntity->GetIsTypePed()) {
                return m_pCamTargetEntity->AsPed()->m_fCurrentRotation + DegreesToRadians(90.0f);
            } else {
                return orientation;
            }
        }();
        m_fInitialPlayerOrientation = m_fHorizontalAngle;

        s_GroundFaultProtection                 = 0.0f;
        TheCamera.m_fAvoidTheGeometryProbsTimer = 0.0f;
    }

    if (m_pCamTargetEntity->GetIsTypePed()) {
        m_bResetStatics = false;
        return;
    }

    const auto wheelieTime = static_cast<float>(CTimer::GetTimeInMS());
    if (s_LastWheelieTime > wheelieTime) {
        s_LastWheelieTime = 0.0f;
    }

    auto* targetVeh = m_pCamTargetEntity->AsVehicle();
    if (targetVeh->IsBike() && targetVeh->AsBike()->bikeFlags.bWheelieForCamera || TheCamera.m_fAvoidTheGeometryProbsTimer > 0.0f) {
        if (wheelieTime - s_LastWheelieTime >= 3000.0f) {
            s_LastWheelieTime = static_cast<float>(CTimer::GetTimeInMS());
        }

        const auto pad1 = CPad::GetPad();
        if (!pad1->NewState.LeftShoulder2 && !pad1->NewState.RightShoulder2) {
            auto* targetBike = targetVeh->AsBike();
            if (Process_WheelCam(target, orientation, speedVar, speedVarWanted)) {
                if (targetBike->bikeFlags.bWheelieForCamera) {
                    TheCamera.m_fAvoidTheGeometryProbsTimer = 50.0f;
                } else {
                    TheCamera.m_fAvoidTheGeometryProbsTimer -= CTimer::GetTimeStep();
                    targetBike->bikeFlags.bWheelieForCamera = true;
                }
                return;
            }
            TheCamera.m_fAvoidTheGeometryProbsTimer = 0.0f;
            targetBike->bikeFlags.bWheelieForCamera = false;

            s_LastWheelieTime = 0.0f;
        }
    }

    const auto& entityWorldMat = [&] {
        if (auto* t = targetVeh->AsBike(); t->IsBike()) {
            t->CalculateLeanMatrix();
            return t->m_mLeanMatrix;
        } else {
            return targetVeh->GetMatrix();
        }
    }();

    const auto dummyPos = [&] {
        const auto* vehStruct = targetVeh->GetVehicleModelInfo()->GetVehicleStruct();
        return vehStruct->m_avDummyPos[targetVeh->IsBoat() ? DUMMY_LIGHT_FRONT_MAIN : DUMMY_SEAT_FRONT] * CVector{0.0f, 1.0f, 1.0f}; // ignore x
    }() + CVector{ 0.0f, 0.08f, 0.62f };

    m_fFOV = 60.0f;
    m_vecSource = entityWorldMat.TransformVector(dummyPos);
    m_vecSource += targetVeh->GetPosition();

    if (targetVeh->IsBike() && targetVeh->m_pDriver) {
        auto*   targetBike = targetVeh->AsBike();
        CVector neckPos{};

        targetVeh->m_pDriver->GetTransformedBonePosition(neckPos, BONE_NECK, true);
        neckPos += targetBike->GetMoveSpeed() * CTimer::GetTimeStep();

        constexpr auto BIKE_1ST_PERSON_ZOFFSET = 0.15f; // 0x8CC7B4
        m_vecSource.z = neckPos.z + BIKE_1ST_PERSON_ZOFFSET;

        const auto right = CrossProduct(m_vecFront, m_vecUp);
        // right *= flt_8CCD0C; (=1.0f)

        if (!CWorld::GetIsLineOfSightClear(
            CrossProduct(m_vecSource, m_vecSource + right),
            CrossProduct(m_vecSource, m_vecSource - right),
            true,
            false,
            false,
            false
        )) {
            m_vecSource = targetBike->GetPosition();
            m_vecSource.z = neckPos.z + BIKE_1ST_PERSON_ZOFFSET + 0.62f;
        }
    } else if (targetVeh->IsBoat()) {
        m_vecSource.z += 0.5f;
    }

    // todo: refactor
    if (targetVeh->IsUpsideDown()) {
        if (s_GroundFaultProtection >= 0.5f) {
            s_GroundFaultProtection = 0.5f;
        } else {
            s_GroundFaultProtection += 0.03f;
        }
    } else if (s_GroundFaultProtection >= 0.0f) {
        s_GroundFaultProtection = 0.0f;
    } else {
        s_GroundFaultProtection -= 0.03f;
    }
    m_vecSource.z += s_GroundFaultProtection;

    m_vecFront = entityWorldMat.GetForward().Normalized();
    m_vecUp    = entityWorldMat.GetUp().Normalized();
    const auto a = CrossProduct(m_vecFront, m_vecUp).Normalized();
    m_vecUp = CrossProduct(a, m_vecFront).Normalized();

    if (float wl{}; CWaterLevel::GetWaterLevel(m_vecSource, wl, true) && m_vecSource.z < wl - 0.3f) {
        ApplyUnderwaterMotionBlur();
    }
    m_bResetStatics = false;
}

// 0x521500
void CCam::Process_AimWeapon(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    // NOTSA: Not reversed yet, forwards to the original code (the hook is disabled, see `InjectHooks`)
    plugin::CallMethod<0x521500, CCam*, const CVector*, float, float, float>(this, &target, orientation, speedVar, speedVarWanted);
}

// 0x512B10
void CCam::Process_AttachedCam() {
    m_fFOV = 70.0f;
    const float angle = TheCamera.m_fAttachedCamAngle * 0.0174532924f;

    auto* const attached = TheCamera.m_pAttachedEntity;
    m_vecSource = attached->GetMatrix().TransformVector(TheCamera.m_vecAttachedCamOffset);
    m_vecSource += attached->GetPosition();

    if (!TheCamera.m_bLookingAtVector) {
        m_vecFront = TheCamera.m_pTargetEntity->GetPosition() - m_vecSource;
    } else {
        m_vecFront = attached->GetMatrix().TransformVector(TheCamera.m_vecAttachedCamLookAt);
        m_vecFront += attached->GetPosition();
        m_vecFront -= m_vecSource;
    }
    m_vecFront.Normalise();

    const CVector worldUp{ 0.0f, 0.0f, 1.0f };
    const auto    right = CrossProduct(m_vecFront, worldUp).Normalized();
    const auto    up    = CrossProduct(right, m_vecFront).Normalized();

    if (float waterLevel = 0.0f; CWaterLevel::GetWaterLevel(m_vecSource.x, m_vecSource.y, m_vecSource.z, waterLevel, true, nullptr) && m_vecSource.z < waterLevel - 0.3f) {
        ApplyUnderwaterMotionBlur();
    }

    const double s = std::sin((double)angle);
    const double c = std::cos((double)angle);
    m_vecUp.x      = (float)((double)(float)((double)up.x * c) + (double)right.x * s);
    m_vecUp.y      = (float)((double)up.y * c) + (float)((double)right.y * s);
    m_vecUp.z      = (float)((double)up.z * c) + (float)((double)right.z * s);

    CWorld::pIgnoreEntity = nullptr;
}

// 0x525E50
void CCam::Process_Cam_TwoPlayer() {
    auto& s_lastClearTime    = gLastTime2PlayerCameraWasOK;
    auto& s_lastBlockedTime  = gLastTime2PlayerCameraCollided;
    auto& s_helpMessageTime  = CGameLogic::nPrintFocusHelpTimer;
    auto& s_helpMessageCount = CGameLogic::nPrintFocusHelpCounter;

    const auto IsSwitchPressed = [](const CPad* pad) { // 0x5404A0
        switch (pad->Mode) {
        case 0:
        case 2:
        case 3:  return pad->IsSelectPressed();
        case 1:  return pad->IsDPadUpPressed();
        default: return false;
        }
    };

    auto* const ped0 = CWorld::Players[0].m_pPed;
    auto* const ped1 = CWorld::Players[1].m_pPed;

    // 0 = follow player 1, 1 = follow player 2, 2 = both
    auto s_mode = (int32)CGameLogic::n2PlayerPedInFocus;
    if (!IsSwitchPressed(CPad::GetPad(0))) {
        if (IsSwitchPressed(CPad::GetPad(1))) {
            s_mode = (s_mode == 1) + 1;
        }
    } else {
        s_mode = ((s_mode != 0) - 1) & 2;
    }
    CGameLogic::n2PlayerPedInFocus = (eFocusedPlayer)s_mode;

    if (s_mode == 0) {
        if (ped0->bInVehicle && ped0->m_pVehicle) {
            m_pCamTargetEntity = ped0->m_pVehicle;
            Process_FollowCar_SA(ped0->m_pVehicle->GetPosition(), 0.0f, 0.0f, 0.0f, false);
            m_pCamTargetEntity = ped0;
            m_bResetStatics    = false;
            return;
        }
        Process_FollowPed_SA(ped0->GetPosition(), 0.0f, 0.0f, 0.0f, false);
        m_bResetStatics = false;
        return;
    }
    if (s_mode == 1) {
        if (ped1->bInVehicle && ped1->m_pVehicle) {
            m_pCamTargetEntity = ped1->m_pVehicle;
            Process_FollowCar_SA(ped1->m_pVehicle->GetPosition(), 0.0f, 0.0f, 0.0f, false);
            m_pCamTargetEntity = ped0;
            m_bResetStatics    = false;
            return;
        }
        m_pCamTargetEntity = ped1;
        Process_FollowPed_SA(ped1->GetPosition(), 0.0f, 0.0f, 0.0f, false);
        m_pCamTargetEntity = ped0;
        m_bResetStatics    = false;
        return;
    }

    m_fVerticalAngle = -0.523598790f; // 0xBF060A92
    m_fAlphaSpeed    = 0.0f;

    CVector src{}, front{};
    float   angle{};
    uint32  foundIdx = 0;
    {
        uint32 i = 0;
        uint32 next;
        do {
            next = i + 1;
            if ((i & 1) == 0) {
                angle = m_fHorizontalAngle - (float)((int32)next / 2) * 0.15f;
            } else {
                angle = (float)((int32)next / 2) * 0.15f + m_fHorizontalAngle;
            }
            ComputeTwoPlayerCamPos(*this, angle, src, front, m_vecTargetCoorsForFudgeInter);
            if (IsTwoPlayerCamPosClear(src)) {
                s_lastClearTime = CTimer::GetTimeInMS();
                next            = i;
                break;
            }
            i = next;
        } while ((int32)next < 21);
        foundIdx = next;
    }

    if (foundIdx == 21) {
        angle           = m_fHorizontalAngle;
        s_lastBlockedTime = CTimer::GetTimeInMS();
    } else if ((int32)foundIdx > 0) {
        s_lastBlockedTime = CTimer::GetTimeInMS();
    }

    if (m_bResetStatics) {
        m_fHorizontalAngle = angle;
    }

    const auto WrapNear = [this](float a) {
        if (a <= m_fHorizontalAngle + PI) {
            if (a < m_fHorizontalAngle - PI) {
                return a + 2.0f * PI;
            }
            return a;
        }
        return a - 2.0f * PI;
    };

    float fVar2 = (WrapNear(angle) - m_fHorizontalAngle) * 0.2f;
    float fVar4 = 0.1f;
    if (fVar2 <= 0.1f) {
        fVar4 = fVar2;
        if (fVar2 < -0.1f) {
            fVar4 = -0.1f;
        }
    }
    fVar4 += m_fHorizontalAngle;

    float local70 = 0.0f;
    const auto TryStep = [&]() { // the repeated "check the new position" block
        ComputeTwoPlayerCamPos(*this, angle, src, front, m_vecTargetCoorsForFudgeInter);
        if (!IsTwoPlayerCamPosClear(src)) {
            if ((local70 > 0.0f && m_fBetaSpeed > 0.0f) || (local70 < 0.0f && m_fBetaSpeed < 0.0f)) {
                m_fBetaSpeed = 0.0f;
            }
            s_lastBlockedTime = CTimer::GetTimeInMS();
            local70           = 0.0f;
        }
    };

    if (foundIdx == 0 && CTimer::GetTimeInMS() >= s_lastBlockedTime + 1000) {
        const CVector vel = ped0->m_vecMoveSpeed + ped1->m_vecMoveSpeed;
        const float   mag2 = vel.y * vel.y + vel.x * vel.x + vel.z * vel.z;
        bool          stepBack = false;
        if (mag2 <= 0.01f) {
            stepBack = true;
        } else {
            const float f3 = 0.1f * CTimer::GetTimeStep();
            const float f5 = 0.02f * CTimer::GetTimeStep();
            const double rawAngle = std::atan2((double)-vel.x, (double)vel.y) - (double)(PI / 2.0f); // NOTE: x87 keeps this unrounded for the next subtraction
            local70               = (float)rawAngle;
            const double d        = rawAngle - (double)fVar4;
            if (d <= (double)PI) {
                if (d < (double)-PI) {
                    local70 += 2.0f * PI;
                }
            } else {
                local70 -= 2.0f * PI;
            }
            double f14;
            if (std::sqrt(mag2) * f3 <= 1.0f) {
                f14 = vel.Magnitude() * f3;
            } else {
                f14 = 1.0;
            }
            const float f2 = (float)(((double)local70 - (double)fVar4) * f14);
            local70        = f5;
            if (f2 <= f5) {
                local70 = f2;
                if (f2 < -f5) {
                    local70 = -f5;
                }
            }
            if (0.01f < local70) {
                angle += 0.15f;
            } else if (local70 < 0.01f) {
                stepBack = true;
            }
        }
        if (stepBack) {
            angle -= 0.15f;
        }
        if (0.01f < std::abs(local70)) {
            TryStep();
        }
    }

    local70 = local70 + fVar4;
    local70 = WrapNear(local70);

    float step = std::max(CTimer::GetTimeStep(), 1.0f);
    fVar2      = (local70 - m_fHorizontalAngle) / step;

    const float powFactor = (float)std::pow(0.800000012, (double)CTimer::GetTimeStep());
    fVar4                 = 0.1f;

    if (foundIdx == 0 && s_lastBlockedTime + 1000 <= CTimer::GetTimeInMS()) {
        const auto aim0 = CPad::GetPad(0)->AimWeaponLeftRight(ped0);
        const auto aim1 = CPad::GetPad(1)->AimWeaponLeftRight(ped1);
        float      f3   = -(float)aim0 - (float)aim1;
        local70         = 128.0f;
        if (f3 <= 128.0f) {
            local70 = f3;
            if (f3 < -128.0f) {
                local70 = -128.0f;
            }
        }
        local70 = m_fFOV * 0.0125f * 0.0714285746f * std::abs(local70) * 0.007f * 0.007f * local70;
        if (local70 > 0.01f) {
            angle += 0.15f;
        } else if (local70 < 0.01f) {
            angle -= 0.15f;
        }
        if (0.01f < std::abs(local70)) {
            TryStep();
        }
        fVar2 = local70 + fVar2;
    }

    if (fVar2 <= fVar4) {
        const float lo = -fVar4;
        fVar4          = fVar2;
        if (fVar2 < lo) {
            fVar4 = lo;
        }
    }

    m_fBetaSpeed       = powFactor * m_fBetaSpeed + (1.0f - powFactor) * fVar4;
    m_fHorizontalAngle = m_fBetaSpeed * CTimer::GetTimeStep() + m_fHorizontalAngle;

    ComputeTwoPlayerCamPos(*this, m_fHorizontalAngle, m_vecSource, m_vecFront, m_vecTargetCoorsForFudgeInter);

    if (foundIdx == 21 && CTimer::GetTimeInMS() - s_lastClearTime > 500) {
        CColPoint colPoint;
        CEntity*  hitEntity{};
        gCurCamColVars = 5;
        if (CWorld::ProcessLineOfSight(m_vecTargetCoorsForFudgeInter, m_vecSource, colPoint, hitEntity, true, false, false, false, false, true, true, false)) {
            m_vecSource = colPoint.m_vecPoint;
        }
        if ((uint32)s_helpMessageTime < CTimer::GetTimeInMS() && s_helpMessageCount < 6) {
            CHud::SetHelpMessage(TheText.Get("WRN2_2P"), false, false, false);
            s_helpMessageTime = CTimer::GetTimeInMS() + 60000;
            s_helpMessageCount++;
        }
    }

    m_vecUp = CVector{ 0.0f, 0.0f, 1.0f };
    m_vecUp.Normalise();
    const auto right = CrossProduct(m_vecUp, m_vecFront).Normalized();
    m_vecUp          = CrossProduct(m_vecFront, right);
    m_vecUp.Normalise();

    m_fFOV          = 70.0f;
    m_bResetStatics = false;
}

// 0x519810
void CCam::Process_Cam_TwoPlayer_InCarAndShooting() {
    if (!m_pCamTargetEntity->GetIsTypeVehicle()) {
        return;
    }

    auto* const player0Ped = CWorld::Players[0].m_pPed;
    auto* const player1Ped = CWorld::Players[1].m_pPed;
    auto* const veh        = player0Ped->m_pVehicle;

    CVector vehPos = veh->GetPosition(); // local_c4
    float   baseAngle = veh->GetHeading() - PI / 2.0f; // local_b0

    CPed* shooter;
    CPad* pad;
    if (veh->m_pDriver == player0Ped) {
        pad     = CPad::GetPad(1);
        shooter = player1Ped;
    } else {
        pad     = CPad::GetPad(0);
        shooter = player0Ped;
    }

    // Field of view
    const auto vehSubType = veh->m_nVehicleSubType;
    if (vehSubType == VEHICLE_TYPE_AUTOMOBILE || vehSubType == VEHICLE_TYPE_BIKE) {
        const auto& fwd = veh->GetMatrix().GetForward();
        const float spd = veh->m_vecMoveSpeed.x * fwd.x + veh->m_vecMoveSpeed.y * fwd.y + veh->m_vecMoveSpeed.z * fwd.z;
        if (0.4f < spd) {
            m_fFOV = (spd - 0.4f) * CTimer::GetTimeStep() + m_fFOV;
        }
    }
    if (70.0f < m_fFOV) {
        const double f = std::pow((double)0.98f, (double)CTimer::GetTimeStep());
        m_fFOV         = (float)(((double)m_fFOV - 70.0) * f + 70.0);
    }
    if (m_fFOV <= 100.0f) {
        if (m_fFOV < 70.0f) {
            m_fFOV = 70.0f;
        }
    } else {
        m_fFOV = 100.0f;
    }

    // Aiming
    const float aimLR = (float)pad->AimWeaponLeftRight(shooter);
    const float aimUD = (float)pad->AimWeaponUpDown(shooter);
    const float absLR = std::abs(aimLR);
    float       fovScale = m_fFOV * 0.0125f;
    m_fX_Targetting = fovScale * 0.0714285746f * absLR * CTimer::GetTimeStep() * 0.004f * 0.004f * aimLR + m_fX_Targetting;
    const float absUD = std::abs(-aimUD);
    m_fY_Targetting = fovScale * 0.042857144f * absUD * CTimer::GetTimeStep() * 0.004f * 0.004f * -aimUD + m_fY_Targetting;

    auto* const weaponInfo = CWeaponInfo::GetWeaponInfo(shooter->GetActiveWeapon().m_Type, shooter->GetWeaponSkill());
    float       targetScreenX{}, targetScreenY{};
    CEntity* const aimTarget = CWeapon::FindNearestTargetEntityWithScreenCoors(
        m_fX_Targetting,
        m_fY_Targetting,
        weaponInfo->m_fWeaponRange + weaponInfo->m_fWeaponRange,
        shooter->GetPosition(),
        &targetScreenX,
        &targetScreenY
    );

    if (aimTarget && absLR < 120.0f && absUD < 120.0f) {
        const double f = std::pow((double)0.85f, (double)CTimer::GetTimeStep());
        double       dx = ((double)targetScreenX - m_fX_Targetting) * (1.0 - f);
        float        lim = CTimer::GetTimeStep() * 0.01f;
        if (dx <= lim) {
            if (dx < -lim) {
                dx = -lim;
            }
        } else {
            dx = lim;
        }
        m_fX_Targetting = (float)(dx + m_fX_Targetting);

        const double f2 = std::pow((double)0.85f, (double)CTimer::GetTimeStep());
        double       dy = ((double)targetScreenY - m_fY_Targetting) * (1.0 - f2);
        lim = CTimer::GetTimeStep() * 0.01f;
        if (dy <= lim) {
            if (dy < -lim) {
                dy = -lim;
            }
        } else {
            dy = lim;
        }
        m_fY_Targetting = (float)(dy + m_fY_Targetting);
    }

    float rotExcess = 0.0f; // local_b8
    if (0.9f < m_fX_Targetting) {
        rotExcess       = m_fX_Targetting;
        m_fX_Targetting = 0.9f;
        rotExcess       = rotExcess - 0.9f;
    }
    if (m_fX_Targetting < -0.9f) {
        rotExcess       = m_fX_Targetting;
        m_fX_Targetting = -0.9f;
        rotExcess       = rotExcess + 0.9f;
    }
    if (0.9f < m_fY_Targetting) {
        const float d   = (m_fY_Targetting - 0.9f) * CTimer::GetTimeStep();
        m_fY_Targetting = 0.9f;
        m_fVerticalAngle = m_fVerticalAngle - d * 0.5f;
    }
    if (m_fY_Targetting < -0.9f) {
        const float d   = (m_fY_Targetting + 0.9f) * CTimer::GetTimeStep();
        m_fY_Targetting = -0.9f;
        m_fVerticalAngle = m_fVerticalAngle - d * 0.5f;
    }
    if (absLR < 1.0f && absUD < 1.0f && !aimTarget) {
        const float x   = m_fX_Targetting;
        const float y   = m_fY_Targetting - -0.4f;
        const float len = std::sqrt(x * x + y * y);
        const float step = CTimer::GetTimeStep() * 0.002f;
        if (step <= len) {
            const float inv = 1.0f / len;
            m_fX_Targetting = m_fX_Targetting - step * inv * x;
            m_fY_Targetting = m_fY_Targetting - inv * step * y;
        } else {
            m_fX_Targetting = 0.0f;
            m_fY_Targetting = -0.4f;
        }
    }

    // Distance / height
    const auto* const colModel = veh->GetColModel();
    float vehMaxZ = colModel->m_boundBox.m_vecMax.z;
    float alphaTarget = -0.34906587f; // local_ac
    float camDist     = 0.5f + std::abs(colModel->m_boundBox.m_vecMin.y) + std::abs(colModel->m_boundBox.m_vecMin.y); // local_cc

    if (veh->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI && veh->GetStatus() != STATUS_REMOTE_CONTROLLED) {
        const auto& up = veh->GetMatrix().GetUp();
        vehPos.x       = 0.6f * up.x * vehMaxZ + vehPos.x;
        vehPos.y       = 0.6f * up.y * vehMaxZ + vehPos.y;
        vehPos.z       = 0.6f * up.z * vehMaxZ + vehPos.z;
    } else {
        const float h = 1.3f * vehMaxZ - 0.4f;
        if (0.0f < h) {
            vehPos.z    = vehPos.z + h;
            camDist     = camDist + h;
            alphaTarget = (0.3f / camDist) * h + alphaTarget;
        }
    }

    const float minDist = camDist * 0.9f;
    camDist             = 2.0f + camDist;
    m_fCaMinDistance    = minDist;
    m_fCaMaxDistance    = camDist;

    const float dy = m_vecSource.y - vehPos.y;
    const float dx = m_vecSource.x - vehPos.x;
    float       dist2D = std::sqrt(dy * dy + dx * dx);
    m_fDistanceBeforeChanges = dist2D;
    if (dist2D < (float)0.002) {
        dist2D = 0.002f;
    }
    m_fHorizontalAngle = (float)(std::atan2((double)-(vehPos.x - m_vecSource.x), (double)(vehPos.y - m_vecSource.y)) - (double)(PI / 2.0f));

    float clampedDist = camDist;
    if (camDist < dist2D || (clampedDist = minDist, dist2D < minDist)) {
        m_vecSource.x = dx * (clampedDist / dist2D) + vehPos.x;
        m_vecSource.y = (clampedDist / dist2D) * dy + vehPos.y;
    }

    const auto& vel = veh->m_vecMoveSpeed;
    if (0.0001f < vel.z * vel.z + vel.y * vel.y + vel.x * vel.x) {
        const double velPitch = std::atan2((double)vel.z, std::sqrt((double)vel.y * vel.y + (double)vel.x * vel.x));
        float        factor   = veh->m_nVehicleSubType == VEHICLE_TYPE_HELI ? 3.0f : 5.0f;
        double       k        = (std::sqrt((double)vel.z * vel.z + (double)vel.y * vel.y + (double)vel.x * vel.x) - 0.01f) * factor;
        if (1.0 < k) {
            k = 1.0;
        }
        alphaTarget  = (float)(k * velPitch + alphaTarget);
        const double f = std::pow((double)0.96f, (double)CTimer::GetTimeStep());
        m_fVerticalAngle = (float)(f * m_fVerticalAngle + (1.0 - f) * alphaTarget);
    }

    float maxAlpha = 0.52359879f;
    float minAlpha = -1.04719758f;
    if (veh->m_nVehicleSubType == VEHICLE_TYPE_HELI) {
        maxAlpha = 0.174532935f;
        minAlpha = -1.04719758f;
    }
    if (minAlpha <= m_fVerticalAngle) {
        if (maxAlpha < m_fVerticalAngle) {
            m_fVerticalAngle = maxAlpha;
        }
    } else {
        m_fVerticalAngle = minAlpha;
    }
    m_vecSource.z = (float)((double)vehPos.z - std::sin((double)m_fVerticalAngle) * camDist);

    RotCamIfInFrontCar(vehPos, baseAngle);

    m_vecTargetCoorsForFudgeInter = vehPos;
    AvoidTheGeometry(CVector{ m_vecSource }, m_vecTargetCoorsForFudgeInter, m_vecSource, m_fFOV);

    {
        const double sinb = std::sin((double)rotExcess);
        const double cosb = (double)(float)std::cos((double)rotExcess); // NOTE: the original rounds the cosine to float
        const float  px   = m_vecSource.x;
        const double dyv  = (double)m_vecSource.y - (double)vehPos.y;
        const float  nx   = (float)(cosb * ((double)px - (double)vehPos.x) + sinb * dyv);
        m_vecSource.x     = nx + vehPos.x;
        m_vecSource.y     = (float)((cosb * dyv - sinb * ((double)px - (double)vehPos.x)) + (double)vehPos.y);
    }

    m_bResetStatics = false;
    m_vecFront.x    = vehPos.x - m_vecSource.x;
    m_vecFront.y    = vehPos.y - m_vecSource.y;
    m_vecFront.z    = vehPos.z - m_vecSource.z;
    GetVectorsReadyForRW();

    // Shooting
    if (!veh->CanPedLeanOut(shooter)) {
        shooter->GetActiveWeapon().Update(shooter);
    }
    auto& shooterWeapon = shooter->GetActiveWeapon();
    if (pad->GetCarGunFired() != 0 && !veh->CanPedLeanOut(shooter) && !shooterWeapon.IsTypeMelee() && shooterWeapon.m_State == eWeaponState::WEAPONSTATE_READY) {
        CVector aimPos;
        if (!aimTarget) {
            const auto  right  = CrossProduct(m_vecFront, m_vecUp);
            // NOTE: x87: `tanT` uses the unrounded product, the up term the float-rounded one
            const double tExt   = (double)m_fFOV * (double)0.00872664619f;
            const float  t      = (float)tExt;
            const float  tanT   = (float)std::tan(tExt);
            const auto   upTerm = m_vecUp * (float)((std::tan((double)t) / (double)CDraw::ms_fAspectRatio) * (double)m_fY_Targetting);
            const auto  rightTerm = (right * m_fX_Targetting) * tanT;
            auto        dir    = (m_vecFront + rightTerm) - upTerm;
            dir *= weaponInfo->m_fWeaponRange * 3.0f;
            dir += m_vecSource;
            aimPos = dir;
        } else {
            aimPos = aimTarget->GetPosition();
        }

        const bool seatRHS = shooter != veh->m_apPassengers[1];

        const auto toAim = aimPos - m_vecSource;
        const double ang  = (double)(float)std::atan2((double)-toAim.x, (double)toAim.y);
        double       diff = ang - (double)veh->GetHeading(); // NOTE: x87 extended precision
        if (diff > (double)PI) {
            diff -= (double)(2.0f * PI);
        } else if (diff < (double)-PI) {
            diff += (double)(2.0f * PI);
        }
        diff += (double)0.7853981852531433f;
        if (diff < 0.0) {
            diff += (double)(2.0f * PI);
        }
        const auto fakeShootDirn = (int32)(diff * (double)0.6366197466850281f);

        CTaskSimpleGangDriveBy task{ nullptr, nullptr, 100.0f, 100, (eDrivebyStyle)8, seatRHS };
        task.m_pWeaponInfo = CWeaponInfo::GetWeaponInfo(shooter->GetActiveWeapon().m_Type, shooter->GetWeaponSkill());
        task.m_nFakeShootDirn = (char)fakeShootDirn;
        plugin::CallMethod<0x627CC0, CTaskSimpleGangDriveBy*, CPed*>(&task, shooter); // FireGun-like
        CamShakeNoPos(&TheCamera, 0.03f);
    }
}

// 0x513510
void CCam::Process_Cam_TwoPlayer_Separate_Cars() {
    m_fFOV = 80.0f;

    auto* const veh2 = CWorld::Players[1].m_pPed->m_pVehicle;
    auto* const veh1 = CWorld::Players[0].m_pPed->m_pVehicle;

    const CVector pos1 = veh1->GetPosition();
    const CVector pos2 = veh2->GetPosition();

    const auto d = (pos2 - pos1).Normalized();

    const CVector pA{
        pos1.x - d.x * 6.0f,
        pos1.y - d.y * 6.0f,
        (pos1.z + veh1->GetColModel()->m_boundBox.m_vecMax.z + 1.0f) - d.z * 6.0f,
    };
    const CVector pB{
        d.x * 6.0f + pos2.x,
        d.y * 6.0f + pos2.y,
        d.z * 6.0f + pos2.z + veh2->GetColModel()->m_boundBox.m_vecMax.z + 1.0f,
    };

    const auto dirA = (veh2->GetPosition() - pA).Normalized();
    const auto dirB = (veh1->GetPosition() - pB).Normalized();

    CVector perp{ pA.y - pB.y, pB.x - pA.x, 0.0f };
    perp.Normalise();
    perp.z = -0.1f;
    perp.Normalise();

    const double angle = (double)m_fTwoPlayerFocusBlend * (double)PI; // NOTE: x87 extended precision
    const float  s     = (float)std::sin(angle);
    const float  c     = (float)((std::cos(angle) + 1.0) * 0.5);

    const CVector dPos = veh1->GetPosition() - veh2->GetPosition();
    const float   dist = std::sqrt(sq(dPos.y) + sq(dPos.z) + sq(dPos.x)); // NOTE: summation order as in the original

    const float perpZ = s * perp.z;
    const float oneMinusC = 1.0f - c;
    m_vecSource.x = (c * pA.x + pB.x * oneMinusC) - s * perp.x * dist * 0.75f;
    m_vecSource.y = (c * pA.y + oneMinusC * pB.y) - s * perp.y * dist * 0.75f;
    m_vecSource.z = (c * pA.z + oneMinusC * pB.z) - perpZ * dist * 0.75f;

    const float oneMinusS = 1.0f - s;
    const float blendX    = (dirA.x * c + dirB.x * oneMinusC) * oneMinusS;
    const float blendY    = (dirA.y * c + dirB.y * oneMinusC) * oneMinusS;
    const float blendZ    = (dirA.z * c + dirB.z * oneMinusC) * oneMinusS;
    m_vecFront.x          = blendX + s * perp.x;
    m_vecFront.y          = blendY + s * perp.y;
    m_vecFront.z          = blendZ + perpZ;
    m_vecFront.Normalise();

    m_vecTargetCoorsForFudgeInter = m_nTwoPlayerFocusedPlayer == 0 ? veh2->GetPosition() : veh1->GetPosition();

    m_vecUp = CVector{ 0.0f, 0.0f, 1.0f };
    m_vecUp.Normalise();
    const auto right = CrossProduct(m_vecUp, m_vecFront).Normalized();
    m_vecUp          = CrossProduct(m_vecFront, right);
    m_vecUp.Normalise();

    auto* const focusedVeh = CWorld::Players[m_nTwoPlayerFocusedPlayer].m_pPed->m_pVehicle;
    auto* const otherVeh   = CWorld::Players[(m_nTwoPlayerFocusedPlayer - 1u) & 1].m_pPed->m_pVehicle;
    const auto  focusedVel = focusedVeh->m_vecMoveSpeed;
    CVector     front2D{ m_vecFront.x, m_vecFront.y, 0.0f };
    front2D.Normalise();
    const float focusedDot = focusedVel.y * front2D.y + focusedVel.z * front2D.z + focusedVel.x * front2D.x;
    if (focusedDot < -0.13f &&
        focusedDot < -front2D.x * otherVeh->m_vecMoveSpeed.x + otherVeh->m_vecMoveSpeed.y * -front2D.y + otherVeh->m_vecMoveSpeed.z * -front2D.z) {
        m_nTwoPlayerFocusedPlayer = (m_nTwoPlayerFocusedPlayer - 1u) & 1;
    }

    float blend;
    if (m_nTwoPlayerFocusedPlayer == 0) {
        blend = m_fTwoPlayerFocusBlend - CTimer::GetTimeStep() * 0.04f;
        if (blend <= 0.0f) {
            blend = 0.0f;
        }
    } else {
        blend = CTimer::GetTimeStep() * 0.04f + m_fTwoPlayerFocusBlend;
        if (1.0f <= blend) {
            m_fTwoPlayerFocusBlend = 1.0f;
            return;
        }
    }
    m_fTwoPlayerFocusBlend = blend;
}

// 0x513BE0
void CCam::Process_Cam_TwoPlayer_Separate_Cars_TopDown() {
    m_fFOV = 80.0f;
    const auto p1 = FindPlayerEntity(PED_TYPE_PLAYER1), p2 = FindPlayerEntity(PED_TYPE_PLAYER2);

    const auto p1p2Centroid = (p1->GetPosition() + p2->GetPosition()) / 2.0f;
    const auto p1p2Distance = DistanceBetweenPoints(p1->GetPosition(), p2->GetPosition());

    auto camHeightMult = std::abs([&] {
        if (FindPlayerVehicle(PED_TYPE_PLAYER1)) {
            return FindPlayerVehicle(PED_TYPE_PLAYER2) ? 1.0f : 0.75f;
        }
        return FindPlayerVehicle(PED_TYPE_PLAYER2) ? 0.75f : 0.45f;
    }() - m_fCameraHeightMultiplier);

    if (const auto s = CTimer::GetTimeStep() / 200.0f; camHeightMult >= s) {
        camHeightMult = (camHeightMult >= 0.0f ? s : -s) + m_fCameraHeightMultiplier;
    }
    m_fCameraHeightMultiplier = camHeightMult;

    const auto v21 = std::max(p1p2Distance + 10.0f, 30.0f);
    m_vecSource.Set(
        p1p2Centroid.x,
        p1p2Centroid.y - v21 * std::sin(0.4f),
        p1p2Centroid.z - v21 * -std::cos(0.4f)
    );
    m_vecFront.Set(0.0f, std::sin(0.4f), -std::cos(0.4f));
    m_vecUp.Set(0.0f, m_vecSource.y, std::sin(0.4f));
    m_vecTargetCoorsForFudgeInter = m_vecSource;
}

// 0x51B850
bool CCam::Process_DW_BirdyCam(bool) {
    constexpr int32 CAM_ID = 22;

    TheCamera.m_bUseNearClipScript = false;
    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        TheCamera.m_bUseNearClipScript = false;
        return false;
    }

    CEntity*   entity{};
    CVehicle*  vehicle{};
    CVector    dest{}, src{}, up{}, right{}, fwd{}, vel{}, angVel{};
    float      speed{}, angSpeed{};
    CColSphere colSphere{};
    GetCoreDataForDWCineyCamMode(entity, vehicle, dest, src, up, right, fwd, vel, speed, angVel, angSpeed, colSphere);

    const auto now = CTimer::GetTimeInMS();

    static CVector s_points[2]{};       // 0xB70080
    static int32   s_clearCounter = 30; // 0xB7007C

    if (gLastDWCineyCamMode != 0x3A || gLastFrameProcessedDWCineyCam < CTimer::GetFrameCounter() - 1u) {
        gLastDWCineyCamMode  = 0x3A;
        gDWCineyCamEndTime   = 5000u + now; // 0x8CCBB0
        gbExitCam[CAM_ID]    = false;
        gDWCineyCamStartTime = now;
        s_clearCounter       = 30;
        gHandShaker[0].Reset();

        const auto centerAhead = CVector{
            fwd.x * 50.0f * 2.0f + dest.x,
            dest.y + fwd.y * 50.0f * 2.0f,
            dest.z + fwd.z * 50.0f * 2.0f,
        };
        CEntity* entitiesAhead[128]{};
        int16    numAhead{};
        CWorld::FindObjectsInRange(centerAhead, 50.0f, true, &numAhead, 127, entitiesAhead, false, false, false, true, true);

        fwd.z *= 50.0f;
        const auto centerBehind = CVector{
            dest.x - fwd.x * 50.0f * 1.0f,
            dest.y - fwd.y * 50.0f * 1.0f,
            dest.z - fwd.z * 1.0f,
        };
        CEntity* entitiesBehind[128]{};
        int16    numBehind{};
        CWorld::FindObjectsInRange(centerBehind, 50.0f, true, &numBehind, 127, entitiesBehind, false, false, false, true, true);

        const auto IsSuitableLampPost = [](CEntity* e) {
            return e->GetIsStatic() && e->GetMatrix().GetUp().z > 0.9f && IsLampPost(e->GetModelId());
        };

        CEntity* lampsAhead[128]{};
        int16    numLampsAhead{};
        for (int32 i = 0; i < numAhead; i++) {
            if (IsSuitableLampPost(entitiesAhead[i])) {
                lampsAhead[numLampsAhead++] = entitiesAhead[i];
            }
        }
        CEntity* lampsBehind[128]{};
        int16    numLampsBehind{};
        for (int32 i = 0; i < numBehind; i++) {
            if (IsSuitableLampPost(entitiesBehind[i])) {
                lampsBehind[numLampsBehind++] = entitiesBehind[i];
            }
        }

        const auto GetRandomPointOnLamp = [](CEntity* lamp) {
            auto* const colModel = lamp->GetColModel();
            auto        pt       = lamp->GetMatrix().TransformPoint(colModel->m_boundBox.m_vecMax);
            const float h        = colModel->m_boundBox.m_vecMax.z - colModel->m_boundBox.m_vecMin.z * 0.5f;
            const float r        = (float)CGeneral::GetRandomNumber();
            pt.z                 = pt.z - ((h - 1.0f) * r * 3.05185094e-05f + 1.0f);
            return pt;
        };

        bool found = false;
        if (numLampsAhead < 1) {
            gbExitCam[CAM_ID] = true;
            return false;
        }
        for (int32 i = 0; i < numLampsAhead && !found; i++) {
            auto* const lampA = lampsAhead[i];
            if (!IsSuitableLampPost(lampA)) {
                continue;
            }
            const auto ptA   = GetRandomPointOnLamp(lampA);
            const auto probe = (ptA - dest).Normalized() + dest;
            // NOTSA: original compares `fabs(0.0) < 6.0` here, which is always true
            for (int32 j = i; j < numLampsBehind && !found; j++) {
                auto* const lampB = lampsBehind[j];
                if (!IsSuitableLampPost(lampB)) {
                    continue;
                }
                const auto ptB = GetRandomPointOnLamp(lampB);
                if (CWorld::GetIsLineOfSightClear(ptA, ptB, true, false, false, false, false, true, true) &&
                    CWorld::GetIsLineOfSightClear(ptB, probe, true, false, false, false, false, true, true)) {
                    src          = ptA;
                    s_points[0]  = ptA;
                    s_points[1]  = ptB;
                    found        = true;
                }
            }
        }
        if (!found) {
            gbExitCam[CAM_ID] = true;
            return false;
        }
    }

    const int32 duration = (int32)(gDWCineyCamEndTime - gDWCineyCamStartTime);
    const float t        = (float)(int32)(now - gDWCineyCamStartTime) / (float)duration;

    const auto p0   = s_points[0];
    const auto p1   = s_points[1];
    const auto n    = (p1 - p0).Normalized() * 1.0f;
    const auto from = p1 - n;
    const auto to   = CVector{ n.x + n.x + p0.x, n.y + n.y + p0.y, n.z + n.z + p0.z };

    if (!gbExitCam[CAM_ID]) {
        const double s = std::sin((270.0 - (double)t * 180.0) * 0.0174532924);
        const double k = (1.0 + s) * 0.5;
        src.x = (float)(((double)to.x - from.x) * k + from.x);
        src.y = (float)(((double)to.y - from.y) * k + from.y);
        src.z = (float)(((double)to.z - from.z) * (s + 1.0) * 0.5 + from.z);
    }

    if (IsTimeToExitThisDWCineyCamMode(CAM_ID, src, dest, t, false)) {
        gbExitCam[CAM_ID] = true;
        return false;
    }

    CWorld::pIgnoreEntity = entity;
    CColPoint colPoint;
    CEntity*  hitEntity{};
    const bool clear      = !CWorld::ProcessLineOfSight(dest, src, colPoint, hitEntity, true, true, false, false, false, false, false, false);
    CWorld::pIgnoreEntity = nullptr;
    if (!clear) {
        const bool wasZero = s_clearCounter == 0;
        s_clearCounter--;
        if (wasZero) {
            gbExitCam[CAM_ID]     = true;
            CWorld::pIgnoreEntity = nullptr;
            return false;
        }
    } else {
        const bool wasAbove = 30 < s_clearCounter;
        s_clearCounter++;
        if (wasAbove) {
            s_clearCounter = 30;
        }
    }

    Finalise_DW_CineyCams(src, dest, 0.0f, 70.0f, 0.3f, 0.0f);
    return true;
}

// 0x51B120
bool CCam::Process_DW_CamManCam(bool) {
    constexpr int32 CAM_ID = 21;

    TheCamera.m_bUseNearClipScript = false;
    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        TheCamera.m_bUseNearClipScript = false;
        return false;
    }

    CEntity*   entity{};
    CVehicle*  vehicle{};
    CVector    dest{}, src{}, up{}, right{}, fwd{}, vel{}, angVel{};
    float      speed{}, angSpeed{};
    CColSphere colSphere{};
    GetCoreDataForDWCineyCamMode(entity, vehicle, dest, src, up, right, fwd, vel, speed, angVel, angSpeed, colSphere);

    const auto now = CTimer::GetTimeInMS();

    static int32   s_clearCounter = 100; // 0xB70074
    static CVector s_lampPos{};          // 0xB70068

    if (gLastDWCineyCamMode != 0x39 || gLastFrameProcessedDWCineyCam < CTimer::GetFrameCounter() - 1u) {
        gLastDWCineyCamMode  = 0x39;
        gDWCineyCamEndTime   = 10000u + now; // 0x8CCBAC
        s_clearCounter       = 100;
        gbExitCam[CAM_ID]    = false;
        gDWCineyCamStartTime = now;
        gHandShaker[0].Reset();

        const auto center = dest + fwd * 50.0f;
        CEntity*   entities[16]{};
        int16      numEntities{};
        CWorld::FindObjectsInRange(center, 50.0f, true, &numEntities, 15, entities, false, false, false, true, true);

        float    bestDist = 10000.0f;
        CEntity* found{};
        if (numEntities < 1) {
            gbExitCam[CAM_ID] = true;
            return false;
        }
        for (int16 i = 0; i < numEntities; i++) {
            auto* const e = entities[i];
            if (!e->GetIsStatic() || !(e->GetMatrix().GetUp().z > 0.9f) || !IsLampPost(e->GetModelId())) {
                continue;
            }
            const auto  ePos   = e->GetPosition();
            const float dist2D = (CVector2D{ ePos } - CVector2D{ dest }).Magnitude();
            if (!(dist2D < bestDist && 5.0f < dist2D)) {
                continue;
            }
            auto* const colModel = e->GetColModel();
            auto        pt       = e->GetMatrix().TransformPoint(colModel->m_boundBox.m_vecMax);
            pt.z                 = pt.z - colModel->m_boundBox.m_vecMax.z;
            pt.z                 = colModel->m_boundBox.m_vecMin.z * 0.5f + pt.z;

            const auto probe = (pt - dest).Normalized() + dest;
            // NOTSA: original compares `fabs(0.0) < 6.0` here, which is always true
            if (CWorld::GetIsLineOfSightClear(pt, probe, true, false, false, false, false, true, true)) {
                found     = e;
                bestDist  = dist2D;
                src       = pt;
                s_lampPos = pt;
            }
        }
        if (!found) {
            gbExitCam[CAM_ID] = true;
            return false;
        }
    }

    const int32 duration = (int32)(gDWCineyCamEndTime - gDWCineyCamStartTime);
    float       t        = (float)(int32)(now - gDWCineyCamStartTime) / (float)duration;

    if (!gbExitCam[CAM_ID]) {
        auto dir = dest - s_lampPos;
        src      = s_lampPos;
        dir.Normalise();
        src.x = dir.x * 1.0f + src.x;
        src.y = dir.y * 1.0f + src.y;
        src.z = dir.z * 1.0f + src.z;
    }

    auto k = (float)(std::sqrt((double)sq(dest.y - src.y) + (double)sq(dest.z - src.z) + (double)sq(dest.x - src.x)) / 30.0);
    k      = std::clamp(k, 0.0f, 1.0f);
    float fov = (float)((15.0 - 70.0) * (std::sin((270.0 - (double)k * 180.0) * 0.0174532924) + 1.0) * 0.5 + 70.0);
    if (t < 0.1f) {
        auto k2 = std::clamp(t / 0.1f, 0.0f, 1.0f);
        fov     = (float)(((double)fov - 70.0) * (std::sin((270.0 - (double)k2 * 180.0) * 0.0174532924) + 1.0) * 0.5 + 70.0);
    }

    if (IsTimeToExitThisDWCineyCamMode(CAM_ID, src, dest, t, false)) {
        gbExitCam[CAM_ID] = true;
        return false;
    }

    CWorld::pIgnoreEntity = entity;
    CColPoint colPoint;
    CEntity*  hitEntity{};
    const bool hit        = CWorld::ProcessLineOfSight(dest, src, colPoint, hitEntity, true, true, false, false, false, false, false, false);
    CWorld::pIgnoreEntity = nullptr;
    if (!hit) {
        const bool wasAbove = 100 < s_clearCounter;
        s_clearCounter++;
        if (wasAbove) {
            s_clearCounter = 100;
        }
    } else {
        const bool wasZero = s_clearCounter == 0;
        s_clearCounter--;
        if (wasZero) {
            gbExitCam[CAM_ID]     = true;
            CWorld::pIgnoreEntity = nullptr;
            return false;
        }
    }

    // `FUN_00420800` is `max(a, b)`; the result is always >= 0.2 so the `< 0` branch of the original is dead
    const float shake = std::min(std::max(vel.Magnitude() * 8.0f, 0.2f), 1.0f);
    Finalise_DW_CineyCams(src, dest, 0.0f, fov, 10.0f - fov * 0.0142857144f * 9.69999981f, shake);
    return true;
}

// 0x51A740
bool CCam::Process_DW_HeliChaseCam(bool) {
    constexpr int32 CAM_ID = 20;

    TheCamera.m_bUseNearClipScript = false;

    // The original picks one of several settings structs using `rand * 0.0` (always the first one), but still consumes a random number
    (void)CGeneral::GetRandomNumber();
    auto& S = gDWHeliChaseCamSettings;

    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        return false;
    }

    CEntity*   entity{};
    CVehicle*  vehicle{};
    CVector    dest{}, src{}, up{}, right{}, fwd{}, vel{}, angVel{};
    float      speed{}, angSpeed{};
    CColSphere colSphere{};
    GetCoreDataForDWCineyCamMode(entity, vehicle, dest, src, up, right, fwd, vel, speed, angVel, angSpeed, colSphere);

    const auto now = CTimer::GetTimeInMS();

    if (gLastDWCineyCamMode != 0x38 || gLastFrameProcessedDWCineyCam < CTimer::GetFrameCounter() - 1u) {
        gDWCineyCamEndTime = 20000u + now; // 0x8CCBA8
        gbExitCam[CAM_ID]  = false;
        gLastDWCineyCamMode = 0x38;
        gDWCineyCamStartTime = now;

        S.SetDefaults();
        S.lockedCounter = S.lockedCounterInit;
        S.fovRange      = S.fovStart - S.fovEnd;
        S.bPosLocked    = false;
        S.bCollided     = false;
        S.bBlocked      = false;
        S.bFlag81       = false;
        S.clearCounter  = S.clearCounterMax;
        S.bFovLerping   = false;
        S.Randomize();
        gHandShaker[0].Reset();

        bool found = false;
        for (int32 i = 0; i < S.numTries; i++) {
            S.startPos.x = dest.x - fwd.x * S.startDistBehind;
            S.startPos.y = dest.y - fwd.y * S.startDistBehind;
            S.startPos.z = dest.z - fwd.z * S.startDistBehind;
            S.endPos.x   = fwd.x * S.endDistAhead + dest.x;
            S.endPos.y   = fwd.y * S.endDistAhead + dest.y;
            S.endPos.z   = fwd.z * S.endDistAhead + dest.z;
            S.startPos.z = S.startPos.z + S.zOffset;
            S.endPos.z   = S.zOffset + S.endPos.z;

            const int32 r1    = CGeneral::GetRandomNumber();
            const int32 r2    = CGeneral::GetRandomNumber();
            const float sign1 = r1 < 0x3FFF ? -1.0f : 1.0f;
            const float sign2 = r2 < 0x3FFF ? -1.0f : 1.0f;

            S.startPos.x = right.x * S.sideOffset * sign1 + S.startPos.x;
            S.startPos.y = right.y * S.sideOffset * sign1 + S.startPos.y;
            S.startPos.z = (S.sideOffset * 0.0f * sign1) + S.startPos.z;

            S.endPos.x = right.x * S.sideOffset * sign2 + S.endPos.x;
            S.endPos.y = right.y * S.sideOffset * sign2 + S.endPos.y;
            S.endPos.z = S.sideOffset * 0.0f * sign2 + S.endPos.z;

            if (!CWorld::TestSphereAgainstWorld(S.startPos, S.sphereRadius, nullptr, true, true, false, false, false, false)) {
                CWorld::pIgnoreEntity = entity;
                CColPoint colPoint;
                CEntity*  hitEntity{};
                const bool clear      = !CWorld::ProcessLineOfSight(dest, S.startPos, colPoint, hitEntity, true, true, false, false, false, false, false, false);
                CWorld::pIgnoreEntity = nullptr;
                if (clear) {
                    S.bFlag81     = CGeneral::GetRandomNumber() < 0x3FFF;
                    S.bFovLerping = CGeneral::GetRandomNumber() < 0x3FFF;
                    found         = true;
                    break;
                }
            }
        }
        if (!found) {
            S.bPosLocked   = false;
            S.bCollided    = false;
            S.fovRange     = S.fovStart - S.fovEnd;
            S.bBlocked     = false;
            S.bFlag81      = false;
            S.clearCounter = S.clearCounterMax;
            S.lockedCounter = S.lockedCounterInit;
            S.bFovLerping  = false;
            gbExitCam[CAM_ID] = true;
            return false;
        }
    }

    if (gbExitCam[CAM_ID]) {
        return false;
    }

    const int32 duration = (int32)(gDWCineyCamEndTime - gDWCineyCamStartTime);
    const float t        = (float)(int32)(now - gDWCineyCamStartTime) / (float)duration;

    CVector cur;
    cur.x = (S.endPos.x - S.startPos.x) * t + S.startPos.x;
    cur.y = (S.endPos.y - S.startPos.y) * t + S.startPos.y;
    cur.z = (S.endPos.z - S.startPos.z) * t + S.startPos.z;

    // Move the look-at point ahead of the vehicle
    dest.x = fwd.x * speed * S.targetVelFactor + fwd.x + dest.x;
    dest.y = fwd.y * speed * S.targetVelFactor + fwd.y + dest.y;
    dest.z = fwd.z * speed * S.targetVelFactor + fwd.z + dest.z;

    CVector2D dir{ dest.x - cur.x, dest.y - cur.y };
    const float dist2D = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    {
        const float inv = 1.0f / dist2D;
        dir.x           = dir.x * inv;
        dir.y           = inv * dir.y;
    }

    src.x = cur.x;
    src.y = cur.y;
    src.z = cur.z;
    if (dist2D < S.minDist2D) {
        src.x = dest.x - dir.x * S.minDist2D;
        src.y = dest.y - dir.y * S.minDist2D;
    }

    double fovBase = S.fovEnd;
    if (t < S.fovBlendInFraction && !S.bFlag81) {
        const double s = std::sin((270.0 - (double)(1.0f / S.fovBlendInFraction) * (double)t * 180.0) * 0.0174532924);
        fovBase        = ((double)S.fovEnd - (double)S.fovStart) * (s + 1.0) * 0.5 + (double)S.fovStart;
    }
    double fovZoom = 0.0;
    const float dist3D = std::sqrt(sq(src.x - dest.x) + sq(src.y - dest.y) + sq(cur.z - dest.z));
    if (S.fovZoomDistMin < dist3D) {
        double k = ((double)dist3D - S.fovZoomDistMin) / ((double)S.fovZoomDistMax - S.fovZoomDistMin);
        k        = std::clamp(k, 0.0, 1.0);
        const double s = std::sin((270.0 - k * 180.0) * 0.0174532924);
        fovZoom        = (s + 1.0) * 0.5 * S.fovZoomAmount;
    }
    float fov        = (float)(fovBase - fovZoom);
    const float roll = t * S.rollRate;

    static CVector s_savedPos{};        // 0xB70058
    static int32   s_blendCounter = 100; // 0x8CCD24
    if (S.bCollided || CWorld::TestSphereAgainstWorld(src, 15.0f, nullptr, true, true, false, false, false, false)) {
        if (!S.bCollided) {
            s_savedPos     = src;
            S.bCollided    = true;
            s_blendCounter = 100;
        }
        if (s_blendCounter < 0) {
            s_blendCounter--;
            gbExitCam[CAM_ID] = true;
            return false;
        }
        src.x = (src.x - s_savedPos.x) * 0.5f + s_savedPos.x;
        src.y = (src.y - s_savedPos.y) * 0.5f + s_savedPos.y;
        src.z = (src.z - s_savedPos.z) * 0.5f + s_savedPos.z;
        s_blendCounter--;
    }

    if (!S.bPosLocked) {
        CWorld::pIgnoreEntity = entity;
        CColPoint colPoint;
        CEntity*  hitEntity{};
        const bool clear      = !CWorld::ProcessLineOfSight(dest, src, colPoint, hitEntity, true, true, false, false, false, false, false, false);
        CWorld::pIgnoreEntity = nullptr;
        if (!clear) {
            S.bBlocked = true;
            if (!S.bFovLerping && S.clearCounter < (S.clearCounterMax + ((S.clearCounterMax >> 31) & 3)) >> 2) {
                S.savedFov         = fov;
                S.bFovLerping      = true;
                S.fovLerpStartTime = (int32)now;
                S.fovLerpEndTime   = S.fovLerpDuration + (int32)now;
            }
            const auto prev = S.clearCounter;
            S.clearCounter  = prev - 1;
            if (prev == 0) {
                S.lockedPos  = dest;
                S.bPosLocked = true;
            }
        } else {
            S.clearCounter++;
            if (S.clearCounterMax < S.clearCounter) {
                S.clearCounter = S.clearCounterMax;
            }
        }
    } else {
        dest             = S.lockedPos;
        const auto prev  = S.lockedCounter;
        S.lockedCounter  = prev - 1;
        if (prev == 0) {
            gbExitCam[CAM_ID] = true;
            return false;
        }
    }

    bool doFovLerp = S.bFovLerping;
    if (!doFovLerp) {
        if (S.fovLerpStartFraction <= t) {
            S.savedFov         = fov;
            S.bFovLerping      = true;
            S.fovLerpStartTime = (int32)now;
            S.fovLerpEndTime   = S.fovLerpDuration + (int32)now;
        }
        doFovLerp = S.bFovLerping;
    }
    if (doFovLerp) {
        double f = ((double)(int32)now - (double)S.fovLerpStartTime) / ((double)S.fovLerpEndTime - (double)S.fovLerpStartTime);
        f        = std::clamp(f, 0.0, 1.0);
        const double s = std::sin((270.0 - f * 180.0) * 0.0174532924);
        fov            = (float)(((double)S.fovStart - (double)S.savedFov) * (s + 1.0) * 0.5 + (double)S.savedFov);
    }

    if (IsTimeToExitThisDWCineyCamMode(CAM_ID, src, dest, t, false)) {
        gbExitCam[CAM_ID] = true;
        return false;
    }
    Finalise_DW_CineyCams(src, dest, roll, fov, S.nearClip, 1.0f);
    return true;
}

// 0x51C760
bool CCam::Process_DW_PlaneCam1(bool) {
    constexpr int32 CAM_ID = 26;

    TheCamera.m_bUseNearClipScript = false;
    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        TheCamera.m_bUseNearClipScript = false;
        return false;
    }

    CEntity*   entity{};
    CVehicle*  vehicle{};
    CVector    dest{}, src{}, up{}, right{}, fwd{}, vel{}, angVel{};
    float      speed{}, angSpeed{};
    CColSphere colSphere{};
    GetCoreDataForDWCineyCamMode(entity, vehicle, dest, src, up, right, fwd, vel, speed, angVel, angSpeed, colSphere);

    const auto now = CTimer::GetTimeInMS();
    if (dest.z >= 80.0f) {
        static float s_scale = 1.0f;
        static float s_dirSign = 1.0f;
        if (gLastDWCineyCamMode == 0x3E && CTimer::GetFrameCounter() - 1u <= gLastFrameProcessedDWCineyCam) {
            if (TheCamera.GetRoughDistanceToGround() < 30.0f) {
                gbExitCam[CAM_ID] = true;
                return false;
            }
        } else {
            gLastDWCineyCamMode  = 0x3E;
            gDWCineyCamEndTime   = 12000u + now; // 0x8CCBC0
            gbExitCam[CAM_ID]    = false;
            gDWCineyCamStartTime = now;
            CWorld::pIgnoreEntity = entity;
            CColPoint colPoint;
            CEntity*  hitEntity{};
            const bool hit        = CWorld::ProcessLineOfSight(dest, src, colPoint, hitEntity, true, true, false, false, false, false, false, false);
            CWorld::pIgnoreEntity = nullptr;
            if (hit) {
                gbExitCam[CAM_ID]     = true;
                CWorld::pIgnoreEntity = nullptr;
                return false;
            }
            s_scale = 1.0f;
            if (CGeneral::GetRandomNumber() < 0x3FFF) {
                s_scale *= -1.0f;
            }
            if (CGeneral::GetRandomNumber() < 0x3FFF) {
                s_dirSign = -1.0f;
            }
        }

        const int32 elapsed  = (int32)(now - gDWCineyCamStartTime);
        const int32 duration = (int32)(gDWCineyCamEndTime - gDWCineyCamStartTime);
        const float t        = (float)((double)elapsed / (double)duration);

        auto v1 = fwd;
        auto v2 = right;
        v1.Normalise();
        v2.Normalise();
        v2.x = v2.x * s_dirSign;
        v2.y = v2.y * s_dirSign;
        v2.z = v2.z * s_dirSign;

        const float cy = dest.y + v1.y * 10.0f;
        const float c4 = t - 0.5f;
        const float a  = up.x * -150.0f * c4;
        const float b  = up.y * -150.0f * c4 * s_scale;
        const double sinv = std::sin((double)4 * ((double)elapsed / (double)duration) * 360.0 * 0.0174532924);
        const float c  = up.z * 0.5f;
        const float e  = (float)((double)up.x * 0.5 * sinv);
        const float g  = (float)((double)c * sinv);

        src.x = e + a * s_scale + up.x * 15.0f + v2.x * 30.0f + v1.x * 10.0f + dest.x;
        src.y = (float)((double)up.y * 0.5 * sinv + (double)(b + up.y * 15.0f + v2.y * 30.0f + cy));
        src.z = g + up.z * -150.0f * c4 * s_scale + up.z * 15.0f + v1.z * 10.0f + dest.z + v2.z * 30.0f;

        CWorld::pIgnoreEntity = entity;
        CColPoint colPoint;
        CEntity*  hitEntity{};
        const bool hit        = CWorld::ProcessLineOfSight(dest, src, colPoint, hitEntity, true, true, false, false, false, false, false, false);
        CWorld::pIgnoreEntity = nullptr;

        static int32 s_clearCounter = 100;
        if (!hit) {
            const auto prev = s_clearCounter;
            s_clearCounter  = prev + 1;
            if (prev > 100) {
                s_clearCounter = 100;
            }
        } else {
            const auto prev = s_clearCounter;
            s_clearCounter  = prev - 1;
            if (prev == 0) {
                gbExitCam[CAM_ID]     = true;
                CWorld::pIgnoreEntity = nullptr;
                return false;
            }
        }

        if (!IsTimeToExitThisDWCineyCamMode(CAM_ID, src, dest, t, false)) {
            Finalise_DW_CineyCams(src, dest, 0.0f, 70.0f, 5.0f, 1.0f);
            return true;
        }
    }
    gbExitCam[CAM_ID] = true;
    return false;
}

// 0x51CC30
bool CCam::Process_DW_PlaneCam2(bool) {
    constexpr int32 CAM_ID = 27;

    TheCamera.m_bUseNearClipScript = false;
    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        TheCamera.m_bUseNearClipScript = false;
        return false;
    }

    CEntity*   entity{};
    CVehicle*  vehicle{};
    CVector    dest{}, src{}, up{}, right{}, fwd{}, vel{}, angVel{};
    float      speed{}, angSpeed{};
    CColSphere colSphere{};
    GetCoreDataForDWCineyCamMode(entity, vehicle, dest, src, up, right, fwd, vel, speed, angVel, angSpeed, colSphere);

    const auto now = CTimer::GetTimeInMS();
    if (dest.z >= 80.0f) {
        static float s_unusedFlip{};
        static float s_scale = 1.0f;
        static float s_dirSign = 1.0f;
        if (gLastDWCineyCamMode != 0x3F || gLastFrameProcessedDWCineyCam < CTimer::GetFrameCounter() - 1u) {
            gLastDWCineyCamMode  = 0x3F;
            gDWCineyCamEndTime   = 7000u + now; // 0x8CCBC4
            gbExitCam[CAM_ID]    = false;
            gDWCineyCamStartTime = now;
            CWorld::pIgnoreEntity = entity;
            CColPoint colPoint;
            CEntity*  hitEntity{};
            const bool hit        = CWorld::ProcessLineOfSight(dest, src, colPoint, hitEntity, true, true, false, false, false, false, false, false);
            CWorld::pIgnoreEntity = nullptr;
            if (hit) {
                gbExitCam[CAM_ID]     = true;
                CWorld::pIgnoreEntity = nullptr;
                return false;
            }
            s_unusedFlip = 1.0f;
            if (CGeneral::GetRandomNumber() < 0x3FFF) {
                s_unusedFlip *= -1.0f;
            }
            s_scale = 1.0f;
            if (CGeneral::GetRandomNumber() < 0x3FFF) {
                s_scale *= -1.0f;
            }
            if (CGeneral::GetRandomNumber() < 0x3FFF) {
                s_dirSign = -1.0f;
            }
        }

        const int32  elapsed  = (int32)(now - gDWCineyCamStartTime);
        const int32  duration = (int32)(gDWCineyCamEndTime - gDWCineyCamStartTime);
        const float  t        = (float)((double)elapsed / (double)duration);

        auto dirA = fwd;
        auto dirB = right;
        dirA.Normalise();
        dirB.Normalise();
        const float f1 = 1.0f - t;
        const float f2 = 1.0f - (t + t);
        dirA.x = dirA.x * f2 * s_scale;
        dirA.y = dirA.y * f2 * s_scale;
        dirA.z = dirA.z * f2 * s_scale;
        dirB.x = dirB.x * f1 * s_dirSign;
        dirB.y = dirB.y * f1 * s_dirSign;
        dirB.z = dirB.z * f1 * s_dirSign;

        fwd.x = dirA.x * 30.0f + dest.x;
        fwd.y = dest.y + dirA.y * 30.0f;
        fwd.z = dirA.z * 30.0f + dest.z;

        const float upZ5 = up.z * 5.0f;
        const double sinv = std::sin((double)4 * ((double)elapsed / (double)duration) * 360.0 * 0.0174532924);
        up.z              = up.z * 0.5f;
        right.x           = (float)((double)up.x * 0.5 * sinv);
        const float rz    = (float)((double)up.z * sinv);

        src.x = right.x + up.x * 5.0f + dirB.x * 30.0f + fwd.x;
        src.y = (float)((double)up.y * 0.5 * sinv + (double)(up.y * 5.0f + dirB.y * 30.0f + fwd.y));
        src.z = rz + upZ5 + fwd.z + dirB.z * 30.0f;

        CWorld::pIgnoreEntity = entity;
        CColPoint colPoint;
        CEntity*  hitEntity{};
        const bool hit        = CWorld::ProcessLineOfSight(dest, src, colPoint, hitEntity, true, true, false, false, false, false, false, false);
        CWorld::pIgnoreEntity = nullptr;

        static int32 s_clearCounter = 100;
        if (!hit) {
            const auto prev = s_clearCounter;
            s_clearCounter  = prev + 1;
            if (prev > 100) {
                s_clearCounter = 100;
            }
        } else {
            const auto prev = s_clearCounter;
            s_clearCounter  = prev - 1;
            if (prev == 0) {
                gbExitCam[CAM_ID]     = true;
                CWorld::pIgnoreEntity = nullptr;
                return false;
            }
        }

        if (!IsTimeToExitThisDWCineyCamMode(CAM_ID, src, dest, t, false)) {
            Finalise_DW_CineyCams(src, dest, 0.0f, 70.0f, 5.0f, 1.0f);
            return true;
        }
    }
    gbExitCam[CAM_ID] = true;
    return false;
}

// 0x51D100
bool CCam::Process_DW_PlaneCam3(bool) {
    constexpr int32 CAM_ID = 28;

    TheCamera.m_bUseNearClipScript = false;
    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        TheCamera.m_bUseNearClipScript = false;
        return false;
    }

    CEntity*   entity{};
    CVehicle*  vehicle{};
    CVector    dest{}, src{}, up{}, right{}, fwd{}, vel{}, angVel{};
    float      speed{}, angSpeed{};
    CColSphere colSphere{};
    GetCoreDataForDWCineyCamMode(entity, vehicle, dest, src, up, right, fwd, vel, speed, angVel, angSpeed, colSphere);

    const auto now = CTimer::GetTimeInMS();
    if (dest.z >= 80.0f) {
        if (gLastDWCineyCamMode != 0x40 || gLastFrameProcessedDWCineyCam < CTimer::GetFrameCounter() - 1u) {
            gLastDWCineyCamMode  = 0x40;
            gDWCineyCamEndTime   = 5000u + now; // 0x8CCBC8
            gbExitCam[CAM_ID]    = false;
            gDWCineyCamStartTime = now;
            CWorld::pIgnoreEntity = entity;
            CColPoint colPoint;
            CEntity*  hitEntity{};
            if (CWorld::ProcessLineOfSight(dest, src, colPoint, hitEntity, true, true, false, false, false, false, false, false)) {
                gbExitCam[CAM_ID]     = true;
                CWorld::pIgnoreEntity = nullptr;
                return false;
            }
        }

        CWorld::pIgnoreEntity = entity;
        auto* const colModel  = entity->GetColModel();
        float       length    = (colModel->m_boundBox.m_vecMax.y - colModel->m_boundBox.m_vecMin.y) * 0.5f;
        length += length;

        const auto ahead = fwd * length + dest;
        src              = up * 5.0f + ahead;

        const auto t = (float)(int32)(now - gDWCineyCamStartTime) / (float)(int32)(gDWCineyCamEndTime - gDWCineyCamStartTime);

        CColPoint colPoint;
        CEntity*  hitEntity{};
        const bool hit        = CWorld::ProcessLineOfSight(dest, src, colPoint, hitEntity, true, true, false, false, false, false, false, false);
        CWorld::pIgnoreEntity = nullptr;

        static int32 s_clearCounter = 100;
        if (!hit) {
            const auto prev = s_clearCounter;
            s_clearCounter  = prev + 1;
            if (prev > 100) {
                s_clearCounter = 100;
            }
        } else {
            const auto prev = s_clearCounter;
            s_clearCounter  = prev - 1;
            if (prev == 0) {
                gbExitCam[CAM_ID]     = true;
                CWorld::pIgnoreEntity = nullptr;
                return false;
            }
        }

        if (!IsTimeToExitThisDWCineyCamMode(CAM_ID, src, dest, t, false)) {
            Finalise_DW_CineyCams(src, dest, 0.0f, 70.0f, 5.0f, 1.0f);
            return true;
        }
    }
    gbExitCam[CAM_ID] = true;
    return false;
}

// 0x51C250
bool CCam::Process_DW_PlaneSpotterCam(bool) {
    constexpr int32 CAM_ID = 23;

    TheCamera.m_bUseNearClipScript = false;
    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        TheCamera.m_bUseNearClipScript = false;
        return false;
    }

    CEntity*   entity{};
    CVehicle*  vehicle{};
    CVector    dest{}, src{}, up{}, right{}, fwd{}, vel{}, angVel{};
    float      speed{}, angSpeed{};
    CColSphere colSphere{};
    GetCoreDataForDWCineyCamMode(entity, vehicle, dest, src, up, right, fwd, vel, speed, angVel, angSpeed, colSphere);

    static CVector s_target{};   // 0xB700A8
    static bool    s_useFovLerp{}; // 0xB700A4
    static float   s_baseFov = 70.0f; // 0xB700A0
    static int32   s_clearCounter = 100; // 0xB7009C

    const auto now = CTimer::GetTimeInMS();
    if (gLastDWCineyCamMode != 0x3B || gLastFrameProcessedDWCineyCam < CTimer::GetFrameCounter() - 1u) {
        gLastDWCineyCamMode  = 0x3B;
        gDWCineyCamEndTime   = 10000u + now; // 0x8CCBB4
        gbExitCam[CAM_ID]    = false;
        gDWCineyCamStartTime = now;

        bool found = false;
        for (int32 i = 0; i < 8; i++) { // 0x8CCD94
            CVector pos;
            pos.z = dest.z - 100.0f;
            pos.x = (100.0f - 50.0f) * ((float)CGeneral::GetRandomNumber() * 3.05185094e-05f) + 50.0f + dest.x;
            pos.y = (100.0f - 50.0f) * ((float)CGeneral::GetRandomNumber() * 3.05185094e-05f) + 50.0f + dest.y;

            CWorld::pIgnoreEntity = entity;
            CColPoint colPoint;
            CEntity*  hitEntity{};
            const bool hit        = CWorld::ProcessLineOfSight(dest, pos, colPoint, hitEntity, true, true, false, false, false, false, false, false);
            CWorld::pIgnoreEntity = nullptr;
            if (hit) {
                s_target = colPoint.m_vecPoint;
                s_target.z += 2.0f;
                found = true;
                break;
            }
        }
        if (!found) {
            gbExitCam[CAM_ID]     = true;
            CWorld::pIgnoreEntity = nullptr;
            return false;
        }
        s_useFovLerp = CGeneral::GetRandomNumber() < 0x3FFF;
    }

    const int32 duration = (int32)(gDWCineyCamEndTime - gDWCineyCamStartTime);
    const float t        = (float)(int32)(now - gDWCineyCamStartTime) / (float)duration;
    src                  = s_target;
    float fov            = 70.0f;

    if (5.0f <= (CVector2D{ s_target } - CVector2D{ dest }).Magnitude()) {
        if (s_useFovLerp) {
            auto k = (dest - s_target).Magnitude() / 100.0f;
            k      = std::clamp(k, 0.0f, 1.0f);
            auto s = std::sin((270.0f - k * 180.0f) * 0.0174532924f);
            fov    = (12.0f - 70.0f) * (s + 1.0f) * 0.5f + 70.0f;
            if (t < 0.1f) {
                auto k2 = std::clamp(t / 0.1f, 0.0f, 1.0f);
                auto s2 = std::sin((270.0f - k2 * 180.0f) * 0.0174532924f);
                fov     = (fov - s_baseFov) * (s2 + 1.0f) * 0.5f + s_baseFov;
            }
        }

        CWorld::pIgnoreEntity = entity;
        CColPoint colPoint;
        CEntity*  hitEntity{};
        const bool hit        = CWorld::ProcessLineOfSight(dest, src, colPoint, hitEntity, true, true, false, false, false, false, false, false);
        CWorld::pIgnoreEntity = nullptr;

        if (!hit) {
            const auto prev = s_clearCounter;
            s_clearCounter  = prev + 1;
            if (prev > 100) {
                s_clearCounter = 100;
            }
        } else {
            const auto prev = s_clearCounter;
            s_clearCounter  = prev - 1;
            if (prev == 0) {
                gbExitCam[CAM_ID]     = true;
                CWorld::pIgnoreEntity = nullptr;
                return false;
            }
        }

        if (!IsTimeToExitThisDWCineyCamMode(CAM_ID, src, dest, t, false)) {
            Finalise_DW_CineyCams(src, dest, 0.0f, fov, 10.0f - fov * 0.0142857144f * 9.69999981f, 1.0f);
            return true;
        }
    }
    gbExitCam[CAM_ID] = true;
    return false;
}

// 0x50F3F0 - debug
void CCam::Process_Editor(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    static auto& s_LookAtAngle     = StaticRef<float>(0xB6FFE4);
    static auto& s_DoRenderShadows = StaticRef<bool>(0xB7295A);

    if (m_bResetStatics) {
        m_vecSource.Set(796.0f, -937.0f, 40.0f);
        CEntity::SafeCleanUpRef(m_pCamTargetEntity);
        m_pCamTargetEntity = nullptr;
        m_bResetStatics    = false;
    }
    RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.3f);
    m_fFOV = 70.0f;

    static constexpr float _90DEG_PER_HOUR_IN_RAD_PER_MIN = 0.02617994f;
    const auto* pad = CPad::GetPad(1);
    m_fHorizontalAngle += pad->GetLeftStickX() * _90DEG_PER_HOUR_IN_RAD_PER_MIN / 19.0f;
    m_fVerticalAngle   += DegreesToRadians(static_cast<float>(pad->GetLeftStickY())) / 50.0f;

    m_fVerticalAngle = std::max(m_fVerticalAngle, DegreesToRadians(85.0f));
    if (m_fVerticalAngle >= DegreesToRadians(-85.0f)) {
        if (pad->IsSquareDown()) {
            s_LookAtAngle += 0.1f;
        } else if (pad->IsCrossDown()) {
            s_LookAtAngle -= 0.1f;
        } else {
            s_LookAtAngle = 0.0f;
        }
    } else {
        m_fVerticalAngle = DegreesToRadians(-85.0f);
    }
    s_LookAtAngle = std::clamp(s_LookAtAngle, -70.0f, 70.0f);

    m_vecFront = (m_pCamTargetEntity ? m_pCamTargetEntity->GetPosition() : m_vecSource - m_vecSource).Normalized();
    m_vecSource += s_LookAtAngle * m_vecFront;
    m_vecSource.z = std::min(m_vecSource.z, -450.0f);

    if (pad->IsRightShoulder2Pressed()) {
        FindPlayerEntity()->Teleport(m_vecSource, false);
    }

    const auto ClampByLoop = [](float& value, float min, float max) {
        while (value > max) {
            value -= 1.0f;
        }
        while (value < min) {
            value += 1.0f;
        }
    };
    ClampByLoop(m_vecSource.x, 5.0f, 115.0f);
    ClampByLoop(m_vecSource.y, 5.0f, 115.0f);

    GetVectorsReadyForRW();

    if (!pad->IsLeftShockPressed() && s_DoRenderShadows) {
        CShadows::StoreShadowToBeRendered(
            eShadowType::SHADOW_ADDITIVE,
            gpShadowExplosionTex,
            m_vecSource,
            {12.0f, 0.0f},
            {0.0f, -12.0f},
            128,
            128,
            128,
            128,
            1000.0f,
            false,
            1.0f,
            nullptr,
            false
        );
    }

    if (CHud::m_Wants_To_Draw_Hud) {
        NOTSA_LOG_DEBUG("CamX: {:0.3f} CamY: {:0.3f}  CamZ:  {:0.3f}", m_vecSource.x, m_vecSource.y, m_vecSource.z);
        NOTSA_LOG_DEBUG("Frontx: {:0.3f}, Fronty: {:0.3f}, Frontz: {:0.3f} ", m_vecFront.x, m_vecFront.y, m_vecFront.z);
        NOTSA_LOG_DEBUG("LookAT: {:0.3f}, LookAT: {:0.3f}, LookAT: {:0.3f} ", m_vecSource.x + m_vecFront.x, m_vecSource.y + m_vecFront.y, m_vecSource.z + m_vecFront.z);
    }
}

// 0x51D470
void CCam::Process_Fixed(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    if (m_nDirectionWasLooking != 3) {
        m_nDirectionWasLooking = 3; // todo: enum
    }

    m_vecSource = m_vecCamFixedModeSource;
    m_vecFront = (target - m_vecSource).Normalized();
    m_vecTargetCoorsForFudgeInter = target;
    GetVectorsReadyForRW();

    // inlined?
    const auto a = CrossProduct(
        m_vecFront,
        (m_vecCamFixedModeUpOffSet + CVector{ 0.0f, 0.0f, 1.0f }).Normalized()
    ).Normalized();
    m_vecUp = CrossProduct(
        a,
        m_vecFront
    );
    m_fFOV = 70.0f;

    if (float wl{}; CWaterLevel::GetWaterLevel(m_vecSource, wl, true) && m_vecSource.z < wl) {
        ApplyUnderwaterMotionBlur();
    }

    if (gAllowScriptedFixedCameraCollision) {
        const auto savedIgnoreEntity = CWorld::pIgnoreEntity;

        CWorld::pIgnoreEntity = FindPlayerVehicle();
        CVector out{};
        float   outDist{1.0f};
        if (TheCamera.ConeCastCollisionResolve(m_vecSource, target, out, 2.0f, 0.1f, outDist)) {
            m_vecSource.y = out.y;
            m_vecSource.z = out.z;
        }

        CWorld::pIgnoreEntity = savedIgnoreEntity;
    }
}

// 0x5B25F0
void CCam::Process_FlyBy(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    // NOTSA: Not reversed yet, forwards to the original code (the hook is disabled, see `InjectHooks`)
    plugin::CallMethod<0x5B25F0, CCam*, const CVector*, float, float, float>(this, &target, orientation, speedVar, speedVarWanted);
}

namespace {
//! Tuning of `CCam::Process_FollowCar_SA` for one "camera vehicle type" (see `GetFollowCarCamType`), 7 of them at 0x8CC600 (names made up)
struct FollowCarCamSettings {
    float heightMul;         // 0x00: Multiplier of the vehicle's height
    float minDistOffset;     // 0x04: Added to the zoom
    float heightSub;         // 0x08: Subtracted from the scaled height
    float maxDistMul;        // 0x0C: Multiplier of the (tweaked) vehicle size to get the max distance
    float minDistLimit;      // 0x10: Lower limit of the distance the camera starts at after a reset
    float alphaSmoothBase;   // 0x14: Base of the `pow(base, timestep)` that smooths the vertical angle
    float alphaStepLimitMul; // 0x18: Timestep multiplier => max vertical angle step
    float field_1C;          // 0x1C: Unused
    float betaSmoothBase;    // 0x20: Base of the `pow(base, timestep)` that smooths the horizontal angle (and both speeds)
    float betaSpeedLimit;    // 0x24: Max speed of the horizontal (and, with a bit of fantasy, the vertical) angle
    float betaCatchUpMul;    // 0x28: Timestep multiplier => how fast the horizontal angle follows the vehicle's direction
    float betaStepLimitMul;  // 0x2C: Timestep multiplier => max horizontal angle step
    float inputMul;          // 0x30: Multiplier of the (stick/mouse) input
    float maxAlpha;          // 0x34: Max vertical angle
    float minAlpha;          // 0x38: Min vertical angle (negated)
};
VALIDATE_SIZE(FollowCarCamSettings, 0x3C);

auto& gFollowCarCamSettings = StaticRef<std::array<FollowCarCamSettings, 7>>(0x8CC600);
auto& gFollowCarZoomAngle   = StaticRef<std::array<std::array<float, 5>, 3>>(0x8CC41C); // [zoom][arrPos], see `CCamera::GetArrPosForVehicleType`

// NOTE: These two are the car zoom (the 1..3 "view distance" setting) and the smoothed car zoom of `TheCamera`.
//       The names of `CCamera::m_nCarZoom` and co. don't seem to line up with these addresses, so the originals are used directly.
auto& gCarZoom         = StaticRef<int32>(0xB6F0DC);
auto& gCarZoomSmoothed = StaticRef<float>(0xB6F0E8);

auto& gFollowCarTrailerBlend = StaticRef<float>(0xB7011C); // [0, 1]: Blend factor between the vehicle (alone) and the vehicle + trailer/passenger (names made up)
auto& gFollowCarMouseTimer   = StaticRef<float>(0xB70118); // Set to 50 when the mouse is moved, counts down while it's idle
auto& gbFollowCarAlphaReset  = StaticRef<bool>(0xB70114);  // Set once the vertical angle has been reset for a (special) vehicle
auto& gFollowCarPrevAlpha    = StaticRef<float>(0x8CCEB0);
auto& gFollowCarPrevBeta     = StaticRef<float>(0x8CCEA8);
auto& gbCamUnk_B6F999        = StaticRef<bool>(0xB6F999);  // Set to true when the camera is reset, purpose unknown
auto& gbCamUnk_9655E5        = StaticRef<bool>(0x9655E5);  // Set to true when the followed vehicle is "big" (read by the camera collision code?), purpose unknown

// 0x420800 (see `RopeMax` in Rope.cpp)
float FollowCarMax(float a, float b) {
    return a > b ? a : b; // NOTE: NaN => `b`
}

// 0x50A0A0 - Rounds `value` to `decimals` decimal places (half away from zero)
float RoundToDecimals(float value, int32 decimals) {
    double t = std::pow(10.0, (double)(decimals + 1)) * (double)value;
    t += value < 0.0f ? -5.0 : 5.0;
    double integral;
    std::modf(t * (double)0.1f, &integral);
    return (float)(integral / std::pow(10.0, (double)decimals));
}

// Index into `gFollowCarCamSettings` (the "camera vehicle type") of the vehicle
uint32 GetFollowCarCamType(const CVehicle* veh) {
    switch (veh->m_nModelIndex) {
    case MODEL_RCBANDIT:
    case MODEL_RCBARON:
    case MODEL_RCTIGER:
    case MODEL_RCCAM:
        return 5;
    case MODEL_RCRAIDER:
    case MODEL_RCGOBLIN:
        return 6;
    }
    if (veh->m_nVehicleType == VEHICLE_TYPE_BIKE || veh->m_nVehicleSubType == VEHICLE_TYPE_QUAD) {
        return 1;
    }
    switch (veh->m_nVehicleSubType) {
    case VEHICLE_TYPE_HELI:
        return 2;
    case VEHICLE_TYPE_PLANE:
        if (veh->m_nModelIndex == MODEL_HYDRA && (int32)veh->AsAutomobile()->m_wMiscComponentAngle >= (int32)(int16)CPlane::HARRIER_NOZZLE_SWITCH_LIMIT) {
            return 2;
        }
        return veh->m_nModelIndex == MODEL_VORTEX ? 0 : 3;
    case VEHICLE_TYPE_BOAT:
        return 4;
    }
    return 0;
}
} // namespace

// 0x5245B0
void CCam::Process_FollowCar_SA(const CVector& target, float orientation, float speedVar, float speedVarWanted, bool bFlag) {
    if (!m_pCamTargetEntity->GetIsTypeVehicle()) {
        return;
    }
    auto* const veh = m_pCamTargetEntity->AsVehicle();
    const float ts  = CTimer::GetTimeStep();

    CVector tgt = target; // The point the camera looks at (modified below)

    auto* pad = CPad::GetPad(0);
    if (veh->m_pDriver && veh->m_pDriver->m_nPedType == PED_TYPE_PLAYER2) {
        pad = CPad::GetPad(1);
    }

    TheCamera.ApplyVehicleCameraTweaks(veh);

    const uint32 camType = GetFollowCarCamType(veh);
    const auto&  cfg     = gFollowCarCamSettings[camType];

    float dist = gCarZoomSmoothed + cfg.minDistOffset; // The distance the camera wants to keep from the vehicle

    int32 arrPos = 0;
    TheCamera.GetArrPosForVehicleType(static_cast<eVehicleType>(veh->GetVehicleAppearance()), arrPos);

    // Angle the camera is looking down by
    float elevAngle = 0.0f;
    if (veh->GetStatus() == STATUS_REMOTE_CONTROLLED) {
        elevAngle = gFollowCarZoomAngle[1][arrPos];
    } else if (gCarZoom == 1) {
        elevAngle = gFollowCarZoomAngle[0][arrPos];
    } else if (gCarZoom == 2) {
        elevAngle = gFollowCarZoomAngle[1][arrPos];
    } else if (gCarZoom == 3) {
        elevAngle = gFollowCarZoomAngle[2][arrPos];
    }

    float heightZ  = veh->GetColModel()->GetBoundingBox().m_vecMax.z;
    float colSize  = std::abs(veh->GetColModel()->GetBoundingBox().m_vecMin.y) * 2.0f;
    auto& blend    = gFollowCarTrailerBlend;

    if (auto* const trailer = veh->m_pVehicleBeingTowed) {
        if (blend < 1.0f) {
            const double v = (double)ts * 0.02f + (double)blend;
            blend          = 1.0f < v ? 1.0f : (float)v;
        }

        const auto& vehBB     = veh->GetColModel()->GetBoundingBox();
        const auto& trailerBB = trailer->GetColModel()->GetBoundingBox();
        const double dx       = (double)trailerBB.m_vecMax.x - (double)vehBB.m_vecMin.x;
        const double dy       = (double)trailerBB.m_vecMax.y - (double)vehBB.m_vecMin.y;
        const double dz       = (double)trailerBB.m_vecMax.z - (double)vehBB.m_vecMin.z;
        const double len      = std::sqrt(dz * dz + dy * dy + dx * dx);
        colSize               = (float)(len * 0.5 * (double)blend + (double)colSize);

        const float vehMaxZ = veh->GetColModel()->GetBoundingBox().m_vecMax.z;
        const float maxZ    = heightZ > vehMaxZ ? heightZ : vehMaxZ;
        heightZ             = (float)(((double)maxZ - (double)heightZ) * (double)blend + (double)heightZ);

        // Move the target towards the trailer
        const auto&  trailerPos = trailer->GetPosition();
        const double f          = (double)blend * 0.5f;
        const float  gx         = (float)(f * trailerPos.x);
        const float  gy         = (float)(f * trailerPos.y);
        const double gz         = f * trailerPos.z;
        const double w          = 1.0 - f;
        const float  ty         = (float)(w * tgt.y);
        const float  tz         = (float)(w * tgt.z);
        tgt.x                   = (float)((double)tgt.x * w + (double)gx);
        tgt.y                   = (float)((double)ty + (double)gy);
        tgt.z                   = (float)((double)tz + gz);
    } else if (veh->m_nVehicleSubType == VEHICLE_TYPE_BIKE || veh->m_nVehicleSubType == VEHICLE_TYPE_QUAD) {
        if (veh->m_apPassengers[0]) {
            if (blend < 1.0f) {
                const double v = (double)ts * 0.02f + (double)blend;
                blend          = 1.0f < v ? 1.0f : (float)v;
            }
        } else if (blend > 0.0f) {
            const double v = (double)blend - (double)ts * 0.02f;
            blend          = 0.0f <= v ? (float)v : 0.0f;
        }
        heightZ = (float)(0.4f * (double)blend + (double)heightZ);
    } else {
        blend = 0.0f;
    }

    const double tweakSize = (double)TheCamera.m_fCurrentTweakDistance * (double)colSize;
    dist                   = (float)((double)dist + tweakSize);
    float maxDist          = (float)(tweakSize * cfg.maxDistMul);

    if (veh->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI && veh->GetStatus() != STATUS_REMOTE_CONTROLLED) {
        const auto&  up = veh->GetMatrix().GetUp();
        const double k  = 0.6f;
        const float  ax = (float)(k * (double)up.x * (double)heightZ);
        const double ay = k * (double)up.y * (double)heightZ;
        const float  az = (float)((double)(float)(k * (double)up.z) * (double)heightZ);
        tgt.x           = (float)((double)ax + (double)tgt.x);
        tgt.y           = (float)(ay + (double)tgt.y);
        tgt.z           = (float)((double)TheCamera.m_fCurrentTweakAltitude * ((double)az + (double)tgt.z));
    } else {
        const float hAdj = (float)((double)heightZ * cfg.heightMul - cfg.heightSub);
        double      z    = tgt.z;
        if (hAdj > 0.0f) {
            z += hAdj;
            dist      = hAdj + dist;
            elevAngle = (float)(0.3f / (double)dist * (double)hAdj + (double)elevAngle);
        }
        tgt.z = (float)((double)TheCamera.m_fCurrentTweakAltitude * z);
    }

    elevAngle = TheCamera.m_fCurrentTweakAngle + elevAngle;

    // The closest the camera is allowed to be after a reset
    float minDist;
    {
        double limit = cfg.minDistLimit;
        if (gCarZoom == 1 && (camType == 0 || camType == 1)) {
            limit *= 0.65f;
        }
        minDist = (double)dist > limit ? dist : (float)limit;
    }

    const bool bReset = m_bResetStatics;
    m_fCaMaxDistance  = dist;
    m_fCaMinDistance  = 3.5f;

    if (!bReset) {
        if (veh->m_nVehicleSubType == VEHICLE_TYPE_AUTOMOBILE || veh->m_nVehicleSubType == VEHICLE_TYPE_BIKE) {
            const auto&  fwd   = veh->GetMatrix().GetForward();
            const auto&  speed = veh->m_vecMoveSpeed;
            const double dot   = (double)fwd.z * (double)speed.z + (double)fwd.y * (double)speed.y + (double)speed.x * (double)fwd.x;
            if (dot > 0.4f) {
                m_fFOV = (float)((dot - 0.4f) * (double)ts + (double)m_fFOV);
            }
        }
        if (m_fFOV > 70.0f) {
            const double p = std::pow((double)0.98f, (double)ts);
            m_fFOV         = (float)(((double)m_fFOV - 70.0) * p + 70.0);
        }
        if (m_fFOV > 100.0f) {
            m_fFOV = 100.0f;
        } else if (m_fFOV < 70.0f) {
            m_fFOV = 70.0f;
        }
    } else {
        m_fFOV = 70.0f;
    }

    if (bReset || TheCamera.m_bCamDirectlyBehind || TheCamera.m_bCamDirectlyInFront) {
        m_bResetStatics      = false;
        m_bRotating          = false;
        m_bCollisionChecksOn = true;
        gbCamUnk_B6F999      = true;

        if (!TheCamera.m_bJustCameOutOfGarage && !bFlag) {
            m_fVerticalAngle = 0.0f;
            const double heading = veh->m_matrix
                ? std::atan2(-(double)veh->m_matrix->GetForward().x, (double)veh->m_matrix->GetForward().y)
                : (double)veh->m_placement.m_fHeading;
            m_fHorizontalAngle = (float)(heading - (double)HALF_PI);
            if (TheCamera.m_bCamDirectlyInFront) {
                m_fHorizontalAngle = (float)((heading - (double)HALF_PI) + (double)PI);
            }
        }

        const double cosA = std::cos((double)m_fVerticalAngle);
        m_fBetaSpeed      = 0.0f;
        m_fAlphaSpeed     = 0.0f;
        m_fDistance       = 1000.0f;
        m_vecFront.x      = (float)-(std::cos((double)m_fHorizontalAngle) * cosA);
        m_vecFront.y      = (float)-(std::sin((double)m_fHorizontalAngle) * cosA);
        m_vecFront.z      = (float)std::sin((double)m_fVerticalAngle);

        // `out = tgt - len * front`
        const auto PlaceHistory = [&](CVector& out, float len) {
            const double px = (double)len * (double)m_vecFront.x;
            const double py = (double)len * (double)m_vecFront.y;
            const float  pz = (float)((double)len * (double)m_vecFront.z);
            out.x           = (float)((double)tgt.x - px);
            out.y           = (float)((double)tgt.y - py);
            out.z           = (float)((double)tgt.z - (double)pz);
        };
        PlaceHistory(m_avecTargetHistoryPos[0], minDist);
        m_anTargetHistoryTime[0] = CTimer::m_snTimeInMilliseconds;
        PlaceHistory(m_avecTargetHistoryPos[1], dist);
        m_nCurrentHistoryPoints = 0;

        if (!TheCamera.m_bJustCameOutOfGarage && !bFlag) {
            m_fVerticalAngle = -elevAngle;
        }
    }

    m_vecFront = tgt - m_avecTargetHistoryPos[0];
    m_vecFront.Normalise();

    float currDist; // Distance between the target and where the camera is (supposed to be)
    {
        const double dx = (double)tgt.x - (double)m_avecTargetHistoryPos[1].x;
        const double dy = (double)tgt.y - (double)m_avecTargetHistoryPos[1].y;
        const double dz = (double)tgt.z - (double)m_avecTargetHistoryPos[1].z;
        currDist        = (float)std::sqrt(dx * dx + dz * dz + dy * dy);
    }

    // Horizontal angle behind the vehicle
    double behindBeta = std::atan2(-(double)m_vecFront.x, (double)m_vecFront.y) - (double)HALF_PI;
    if (behindBeta < (double)-PI) {
        behindBeta += (double)(2.0f * PI);
    }

    const auto& moveSpeed = veh->m_vecMoveSpeed;

    // Horizontal angle of the vehicle's movement
    double moveBeta = behindBeta;
    if (std::sqrt((double)moveSpeed.x * (double)moveSpeed.x + (double)moveSpeed.y * (double)moveSpeed.y) > 0.02f) {
        moveBeta = std::atan2(-(double)moveSpeed.x, (double)moveSpeed.y) - (double)HALF_PI;
    }
    if (moveBeta <= behindBeta + (double)PI) {
        if (moveBeta < behindBeta - (double)PI) {
            moveBeta += (double)(2.0f * PI);
        }
    } else {
        moveBeta -= (double)(2.0f * PI);
    }

    const double betaCatchUp    = (double)ts * cfg.betaCatchUpMul;
    const float  betaStepLimit  = ts * cfg.betaStepLimitMul;
    float        betaVel; // How fast the horizontal angle has to change to be at the wanted one
    {
        const double d = ((double)moveSpeed.z * (double)m_vecFront.z + (double)moveSpeed.y * (double)m_vecFront.y) + (double)moveSpeed.x * (double)m_vecFront.x;
        const double rx = (double)moveSpeed.x - d * (double)m_vecFront.x;
        const double ry = (double)moveSpeed.y - (double)(float)(d * (double)m_vecFront.y);
        const double rz = (double)moveSpeed.z - (double)(float)(d * (double)m_vecFront.z);
        const float  lateralSpeed = (float)std::sqrt(rz * rz + rx * rx + ry * ry);

        double catchUp = betaCatchUp * (double)lateralSpeed;
        if (1.0 < catchUp) {
            catchUp = 1.0;
        }
        double step = (moveBeta - behindBeta) * catchUp;
        if (step > (double)betaStepLimit) {
            step = betaStepLimit;
        } else if (step < (double)-betaStepLimit) {
            step = -betaStepLimit;
        }

        double wantedBeta = behindBeta + step;
        if (wantedBeta <= (double)m_fHorizontalAngle + (double)PI) {
            if (wantedBeta < (double)m_fHorizontalAngle - (double)PI) {
                wantedBeta += (double)(2.0f * PI);
            }
        } else {
            wantedBeta -= (double)(2.0f * PI);
        }
        betaVel = (float)((wantedBeta - (double)m_fHorizontalAngle) / (double)(1.0f <= ts ? ts : 1.0f));
    }

    // Vertical angle at which the camera looks at the vehicle (at the moment)
    float alphaTarget = (float)std::asin((double)std::clamp(m_vecFront.z, -1.0f, 1.0f));

    if (currDist < dist && dist > maxDist) {
        dist = maxDist <= currDist ? currDist : maxDist;
    }

    float maxAlpha = cfg.maxAlpha;
    if ((double)moveSpeed.x * moveSpeed.x + (double)moveSpeed.y * moveSpeed.y + (double)moveSpeed.z * moveSpeed.z < 0.04f
        && (veh->m_nVehicleType != VEHICLE_TYPE_BIKE || veh->AsBike()->m_nNoOfContactWheels >= 4)
        && veh->m_nVehicleSubType != VEHICLE_TYPE_HELI
        && (veh->m_nVehicleSubType != VEHICLE_TYPE_PLANE || veh->AsAutomobile()->m_nNumContactWheels != 0)) {
        const auto& mat = veh->GetMatrix();
        const auto  right = CrossProduct(mat.GetForward(), CVector{ 0.0f, 0.0f, 1.0f }).Normalized();
        const auto  up    = CrossProduct(right, mat.GetForward()).Normalized();
        if ((double)up.y * (double)m_vecFront.y + (double)up.z * (double)m_vecFront.z + (double)up.x * (double)m_vecFront.x > 0.0) {
            const auto&  bb          = veh->GetColModel()->GetBoundingBox();
            const double heightAbove = veh->GetHeightAboveRoad();
            const auto&  pos         = veh->GetPosition();
            const float  zRel        = (float)(((double)tgt.z - (double)pos.z) + heightAbove);

            const double relBeta1 = (double)m_fHorizontalAngle - ((double)veh->GetHeading() - (double)HALF_PI);
            const double phi      = std::asin(std::fabs(std::sin(relBeta1)));
            const double thr      = std::atan2((double)bb.m_vecMax.x, -(double)bb.m_vecMin.y);
            double       d1;
            if (phi <= thr) {
                d1 = (1.5f - (double)bb.m_vecMin.y) / std::cos(phi);
            } else {
                const float v   = (float)(1.2f + (double)bb.m_vecMax.x);
                const float arg = (float)((double)HALF_PI - phi);
                d1              = (double)v / std::cos((double)FollowCarMax(0.0f, arg));
            }
            const float d1f = (float)d1;

            const double relBeta2 = (double)m_fHorizontalAngle - ((double)veh->GetHeading() - (double)HALF_PI);
            const double pitch    = std::atan2((double)mat.GetForward().z, std::sqrt((double)mat.GetForward().x * (double)mat.GetForward().x + (double)mat.GetForward().y * (double)mat.GetForward().y));
            const double p1       = pitch * std::cos(relBeta2);
            maxAlpha              = (float)(std::atan2((double)zRel, (double)d1f * 1.2f) + p1);

            if (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE
                && veh->AsAutomobile()->m_nNumContactWheels > 1
                && std::fabs(DotProduct(veh->m_vecTurnSpeed, mat.GetForward())) < 0.05f) {
                const double relBeta3 = ((double)m_fHorizontalAngle - ((double)veh->GetHeading() - (double)HALF_PI)) + (double)HALF_PI;
                const double pitchR   = std::atan2((double)mat.GetRight().z, std::sqrt((double)mat.GetRight().y * (double)mat.GetRight().y + (double)mat.GetRight().x * (double)mat.GetRight().x));
                maxAlpha              = (float)(pitchR * std::cos(relBeta3) + (double)maxAlpha);
            }
        }
    }

    alphaTarget = alphaTarget - elevAngle;
    if (alphaTarget > maxAlpha) {
        alphaTarget = maxAlpha;
    } else if (alphaTarget < -cfg.minAlpha) {
        alphaTarget = -cfg.minAlpha;
    }

    float alphaStepLimit = ts * cfg.alphaStepLimitMul;
    float alphaStep; // How much the vertical angle will change
    {
        const double pw   = std::pow((double)cfg.alphaSmoothBase, (double)ts);
        const double step = ((double)alphaTarget - (double)m_fVerticalAngle) * (1.0 - pw);
        alphaStep         = (float)step;
        if (step > (double)alphaStepLimit) {
            alphaStep = alphaStepLimit;
        } else if (alphaStep < -alphaStepLimit) {
            alphaStep = -alphaStepLimit;
        }
    }

    // Input
    float inX = (float)(-(int32)pad->AimWeaponLeftRight(nullptr)); // Horizontal
    float inY = CCamera::m_bUseMouse3rdPerson ? 0.0f : (float)(int32)pad->AimWeaponUpDown(nullptr); // Vertical
    float yawIn, pitchIn;
    {
        const double fovScale = (double)m_fFOV * 0.0125000002f;
        yawIn   = (float)(((std::fabs((double)inX) * (fovScale * 0.0714285746f)) * (double)inX) * 0.00700000022f * 0.00700000022f);
        pitchIn = (float)(((std::fabs((double)inY) * (fovScale * 0.0428571440f)) * (double)inY) * 0.00700000022f * 0.00700000022f);
    }

    bool bSpecialModel = true;
    switch (veh->m_nModelIndex) {
    case MODEL_PACKER:
    case MODEL_DOZER:
    case MODEL_DUMPER:
    case MODEL_CEMENT:
    case MODEL_ANDROM:
    case MODEL_HYDRA:
    case MODEL_TOWTRUCK:
    case MODEL_FORKLIFT:
    case MODEL_TRACTOR:
        pitchIn = 0.0f;
        break;
    default:
        if (veh->m_nModelIndex == MODEL_RCTIGER || (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE && veh->handlingFlags.bHydraulicInst)) {
            yawIn   = 0.0f;
            pitchIn = 0.0f;
        } else {
            bSpecialModel = false;
        }
        break;
    }

    if (gCameraDirection != 3) {
        yawIn   = 0.0f;
        pitchIn = 0.0f;
    }

    if (camType == 0) {
        const int16 steeringUD = pad->GetSteeringUpDown();
        if (std::fabs((double)steeringUD) > 120.0f && veh->m_pDriver) {
            if (const auto* const task = veh->m_pDriver->GetIntelligence()->GetTaskManager().GetActiveTask(); task && task->GetTaskType() != TASK_COMPLEX_LEAVE_CAR) {
                const double s = (double)steeringUD;
                pitchIn        = (float)(((((std::fabs(s) * (((double)m_fFOV * 0.0125000002f) * 0.0428571440f)) * s) * 0.00700000022f) * 0.00700000022f) * 0.5 + (double)pitchIn);
            }
        }
    }
    if (pitchIn > 0.0f) {
        pitchIn = pitchIn * 0.5f;
    }

    bool bMouse = false; // The mouse is used to control the camera
    if (CCamera::m_bUseMouse3rdPerson && pad->DisablePlayerControls == 0) {
        const float mouseY = CPad::NewMouseControllerState.m_AmountMoved.y + CPad::NewMouseControllerState.m_AmountMoved.y;
        const float mouseX = CPad::NewMouseControllerState.m_AmountMoved.x * -2.0f;
        const bool  bMouseControlsVehicle = (veh->m_nVehicleSubType == VEHICLE_TYPE_PLANE || veh->m_nVehicleSubType == VEHICLE_TYPE_HELI)
            ? CVehicle::m_bEnableMouseFlying
            : CVehicle::m_bEnableMouseSteering;

        if ((mouseX == 0.0f && mouseY == 0.0f) || (pad->NewState.m_bVehicleMouseLook == 0 && bMouseControlsVehicle)) {
            if (gFollowCarMouseTimer > 0.0f) {
                m_fBetaSpeed          = 0.0f;
                m_fAlphaSpeed         = 0.0f;
                pitchIn               = 0.0f;
                yawIn                 = 0.0f;
                alphaTarget           = m_fVerticalAngle;
                gFollowCarMouseTimer  = FollowCarMax(0.0f, gFollowCarMouseTimer - ts);
                bMouse                = true;
            }
        } else {
            const double fovScale = (double)m_fFOV * 0.0125000002f;
            pitchIn               = (float)(((double)mouseY * fovScale) * (double)CCamera::m_fMouseAccelHorzntl);
            yawIn                 = (float)(((double)mouseX * fovScale) * (double)CCamera::m_fMouseAccelHorzntl);
            m_fBetaSpeed          = 0.0f;
            m_fAlphaSpeed         = 0.0f;
            alphaTarget           = m_fVerticalAngle;
            gFollowCarMouseTimer  = 1.0f * 50.0f;
            bMouse                = true;
        }
    }

    // Look down when the passenger is a prostitute that is about to do her job
    if (const auto* const passenger = veh->m_apPassengers[0]) {
        if (const auto* const task = passenger->GetIntelligence()->GetTaskManager().GetActiveTask(); task && task->GetTaskType() == TASK_COMPLEX_PROSTITUTE_SOLICIT) {
            if (static_cast<const CTaskComplexProstituteSolicit*>(task)->bMoveCameraDown) {
                if ((double)maxAlpha - 0.1f <= (double)m_fVerticalAngle) {
                    pitchIn = 0.0f;
                } else {
                    pitchIn = ts * 0.0035f;
                }
            }
        }
    }

    if (bSpecialModel) {
        bool bSetResetFlag = false;
        if (gCameraMode == MODE_CAM_ON_A_STRING) {
            bSetResetFlag = gbFollowCarAlphaReset;
        } else {
            gbFollowCarAlphaReset = false;
        }
        if (!bSetResetFlag) {
            if (std::fabs((double)elevAngle + (double)m_fVerticalAngle) > 0.05f) {
                pitchIn = (float)((-(double)elevAngle - (double)m_fVerticalAngle) * 0.05f);
            } else {
                bSetResetFlag = true;
            }
        }
        if (bSetResetFlag) {
            gbFollowCarAlphaReset = true;
        }
    }

    pitchIn *= cfg.inputMul;
    yawIn   *= cfg.inputMul;

    const float pwBeta = (float)std::pow((double)cfg.betaSmoothBase, (double)ts);
    const float speedLimit = cfg.betaSpeedLimit;
    {
        double v = (double)yawIn + (double)betaVel;
        if (v > (double)speedLimit) {
            v = speedLimit;
        } else if (v < (double)-speedLimit) {
            v = -speedLimit;
        }
        const double oneMinusPw = 1.0 - (double)pwBeta;
        const double betaSpeed  = v * oneMinusPw + (double)pwBeta * (double)m_fBetaSpeed;
        m_fBetaSpeed            = (float)betaSpeed;
        if (std::fabs(betaSpeed) < 0.0001f) {
            m_fBetaSpeed = 0.0f;
        }
    }
    const float oneMinusPwBeta = (float)(1.0 - (double)pwBeta);

    m_fHorizontalAngle = (float)((bMouse ? (double)yawIn : (double)ts * (double)m_fBetaSpeed) + (double)m_fHorizontalAngle);

    if (TheCamera.m_bJustCameOutOfGarage) {
        m_fHorizontalAngle = (float)((double)CGeneral::GetATanOfXY(m_vecFront.x, m_vecFront.y) + (double)PI);
    }
    ClipBeta();

    if (camType <= 1 && alphaTarget < m_fVerticalAngle && !(currDist < dist)) {
        int32 numContactWheels;
        if (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
            numContactWheels = veh->AsAutomobile()->m_nNumContactWheels;
        } else if (veh->m_nVehicleType == VEHICLE_TYPE_BIKE) {
            numContactWheels = veh->AsBike()->m_nNoOfContactWheels;
        } else {
            numContactWheels = 0;
        }
        if (numContactWheels > 1) {
            pitchIn = (float)(((double)alphaTarget - (double)m_fVerticalAngle) * 0.075f + (double)pitchIn);
        }
    }

    {
        const double limit = pitchIn > 0.0f ? (double)speedLimit * 0.5 : (double)speedLimit;
        const float  t     = (float)((double)oneMinusPwBeta * (double)pitchIn + (double)pwBeta * (double)m_fAlphaSpeed);
        m_fAlphaSpeed      = t;
        if ((double)t > limit) {
            m_fAlphaSpeed = (float)limit;
        } else if ((double)t < -limit) {
            m_fAlphaSpeed = (float)-limit;
        }
    }
    if (std::fabs(m_fAlphaSpeed) < 0.0001f) {
        m_fAlphaSpeed = 0.0f;
    }

    float alphaDelta;
    if (bMouse) {
        alphaTarget = pitchIn + alphaTarget;
        alphaDelta  = pitchIn;
    } else {
        alphaTarget = (float)((double)ts * (double)m_fAlphaSpeed + (double)alphaTarget);
        alphaDelta  = alphaStep;
    }
    m_fVerticalAngle = alphaDelta + m_fVerticalAngle;

    if (m_fVerticalAngle > maxAlpha) {
        m_fVerticalAngle = maxAlpha;
        m_fAlphaSpeed    = 0.0f;
    } else if (m_fVerticalAngle < -cfg.minAlpha) {
        m_fVerticalAngle = -cfg.minAlpha;
        m_fAlphaSpeed    = 0.0f;
    }

    // Get rid of tiny changes
    if (std::fabs((double)gFollowCarPrevAlpha - (double)m_fVerticalAngle) < 0.0001f) {
        m_fVerticalAngle = gFollowCarPrevAlpha;
    }
    gFollowCarPrevAlpha = m_fVerticalAngle;
    if (std::fabs((double)gFollowCarPrevBeta - (double)m_fHorizontalAngle) < 0.0001f) {
        m_fHorizontalAngle = gFollowCarPrevBeta;
    }
    gFollowCarPrevBeta = m_fHorizontalAngle;

    {
        const double cosA = std::cos((double)m_fVerticalAngle);
        m_vecFront.x      = (float)-(std::cos((double)m_fHorizontalAngle) * cosA);
        m_vecFront.y      = (float)-(std::sin((double)m_fHorizontalAngle) * cosA);
        m_vecFront.z      = (float)std::sin((double)m_fVerticalAngle);
    }

    // NOTE: This rounds the *previous* source, it's overwritten below
    m_vecSource.x = RoundToDecimals(m_vecSource.x, 4);
    m_vecSource.y = RoundToDecimals(m_vecSource.y, 4);
    m_vecSource.z = RoundToDecimals(m_vecSource.z, 4);
    GetVectorsReadyForRW();
    TheCamera.m_bCamDirectlyBehind  = false;
    TheCamera.m_bCamDirectlyInFront = false;

    {
        const double px = (double)dist * (double)m_vecFront.x;
        const double py = (double)dist * (double)m_vecFront.y;
        const float  pz = (float)((double)dist * (double)m_vecFront.z);
        m_vecSource.x   = (float)((double)tgt.x - px);
        m_vecSource.y   = (float)((double)tgt.y - py);
        m_vecSource.z   = (float)((double)tgt.z - (double)pz);
    }

    alphaTarget = alphaTarget + elevAngle;

    m_vecTargetCoorsForFudgeInter = tgt;
    m_avecTargetHistoryPos[2]     = m_avecTargetHistoryPos[0];

    {
        const float  cosA = (float)std::cos((double)alphaTarget);
        const double ux   = -(std::cos((double)m_fHorizontalAngle) * (double)cosA);
        const double uy   = -(std::sin((double)m_fHorizontalAngle) * (double)cosA);
        const double sinA = std::sin((double)alphaTarget);
        const float  uxf  = (float)ux;
        const float  sinAf = (float)sinA;

        // Where the camera is at its closest
        m_avecTargetHistoryPos[0].x = (float)((double)tgt.x - (double)(float)((double)uxf * (double)minDist));
        m_avecTargetHistoryPos[0].y = (float)((double)tgt.y - (double)(float)(uy * (double)minDist));
        m_avecTargetHistoryPos[0].z = (float)((double)tgt.z - sinA * (double)minDist);

        // Where the camera is supposed to be
        m_avecTargetHistoryPos[1].x = (float)((double)tgt.x - (double)(float)((double)uxf * (double)dist));
        m_avecTargetHistoryPos[1].y = (float)((double)tgt.y - (double)(float)(uy * (double)dist));
        m_avecTargetHistoryPos[1].z = (float)((double)tgt.z - (double)sinAf * (double)dist);
    }

    CCamera::SetColVarsVehicle(static_cast<eVehicleType>(camType), gCarZoom);

    if (gCameraDirection == 3) {
        CWorld::pIgnoreEntity           = veh;
        TheCamera.m_nExtraEntitiesCount = 0;
        TheCamera.CameraVehicleModeSpecialCases(veh);
        if (veh->vehicleFlags.bIsBig) {
            gbCamUnk_9655E5 = true;
        }
        TheCamera.CameraColDetAndReact(&m_vecSource, &tgt);
        TheCamera.ImproveNearClip(veh, nullptr, &m_vecSource, &tgt);
        CWorld::pIgnoreEntity = nullptr;
        m_vecSource.x         = RoundToDecimals(m_vecSource.x, 4);
        m_vecSource.y         = RoundToDecimals(m_vecSource.y, 4);
        m_vecSource.z         = RoundToDecimals(m_vecSource.z, 4);
    }

    TheCamera.m_bCamDirectlyBehind  = false;
    TheCamera.m_bCamDirectlyInFront = false;
    m_vecSource.x                   = RoundToDecimals(m_vecSource.x, 4);
    m_vecSource.y                   = RoundToDecimals(m_vecSource.y, 4);
    m_vecSource.z                   = RoundToDecimals(m_vecSource.z, 4);
    GetVectorsReadyForRW();

    gCamFollowCarLookAt = tgt;
}

// 0x50F970
void CCam::Process_FollowPedWithMouse(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    m_fFOV = 70.0f;
    bool bPlayerInTrain = false;
    if (!m_pCamTargetEntity->GetIsTypePed()) {
        return;
    }

    if (m_bResetStatics) {
        m_bRotating          = false;
        m_bCollisionChecksOn = true;
        CPad::ClearMouseHistory();
        m_bResetStatics = false;
    }

    if (FindPlayerVehicle() && FindPlayerVehicle()->m_nVehicleType == VEHICLE_TYPE_TRAIN) {
        bPlayerInTrain = true;
    }

    CVector tgt{ target.x, target.y, 0.8f + target.z };

    auto* const pad = CPad::GetPad(0);
    float       rotH{}, rotV{}; // local_b0, local_b4
    if (!pad->bPlayerSafe) {
        const auto mouse = CPad::NewMouseControllerState.GetAmountMouseMoved();
        if ((mouse.x == 0.0f && mouse.y == 0.0f) || pad->DisablePlayerControls != 0) {
            // 0x540E80 and 0x540F80 - `LookAroundLeftRight(void)` and `LookAroundUpDown(void)`
            const int16 lookLR = plugin::CallMethodAndReturn<int16, 0x540E80, CPad*>(pad);
            const int16 lookUD = plugin::CallMethodAndReturn<int16, 0x540F80, CPad*>(pad);
            const double fovScale = (double)m_fFOV * 0.0125f; // NOTE: x87 extended precision, same for the expressions below
            rotH = (float)(((0.0714285746f * fovScale) * CTimer::GetTimeStep() * 0.01f) * (float)-(int32)lookLR);
            rotV = (float)(((((double)(float)(int32)lookUD * 0.042857144f) * CTimer::GetTimeStep()) * fovScale) * 0.01f);
        } else {
            const double fovScale = (double)m_fFOV * 0.0125f;
            rotH = (float)((CCamera::m_fMouseAccelHorzntl * fovScale) * ((double)mouse.x * -2.5f));
            rotV = (float)((((double)mouse.y * 4.0f) * fovScale) * CCamera::m_fMouseAccelVertical);
        }
    } else {
        auto toCam = m_vecSource - tgt;
        toCam.Normalise();
        double angle;
        if (-0.9f <= toCam.z) {
            angle = std::atan2((double)toCam.y, (double)toCam.x);
        } else {
            angle = (double)orientation + (double)PI;
        }
        rotV = 0.0f;
        rotH = (float)(angle - (double)m_fHorizontalAngle);
    }

    if ((((TheCamera.m_bFading && TheCamera.m_nFadeInOutFlag == eFadeFlag::FADE_OUT) && 45 < CDraw::FadeValue) || (200 < CDraw::FadeValue || pad->bPlayerSafe))) {
        rotV = 0.05f;
        if (-0.22f - 0.05f <= m_fVerticalAngle) {
            if (-0.22f <= m_fVerticalAngle) {
                rotV = -0.05f;
                if (m_fVerticalAngle <= -0.22f + 0.05f) {
                    rotV = 0.0f;
                    if (-0.22f < m_fVerticalAngle) {
                        rotV = -0.22f - m_fVerticalAngle;
                    }
                }
            } else {
                rotV = -0.22f - m_fVerticalAngle;
            }
        }
    }

    m_fHorizontalAngle = rotH + m_fHorizontalAngle;
    m_fVerticalAngle   = rotV + m_fVerticalAngle;
    ClipBeta();

    if (m_fVerticalAngle <= 0.785398185f) {
        if (m_fVerticalAngle < -1.56206977f) {
            m_fVerticalAngle = -1.56206977f;
        }
    } else {
        m_fVerticalAngle = 0.785398185f;
    }

    double cosAlpha;
    if (m_fVerticalAngle <= 0.0f) {
        cosAlpha = std::cos((double)m_fVerticalAngle);
    } else {
        double v = 3.0 * (double)m_fVerticalAngle;
        if ((double)(PI / 2.0f) < v) {
            v = (double)(PI / 2.0f);
        }
        cosAlpha = std::cos(v);
    }
    float dist = (float)(cosAlpha * 2.0 + 1.7f);

    if (TheCamera.m_bUseTransitionBeta == true) {
        m_fHorizontalAngle = m_fTransitionBeta;
    }
    if (TheCamera.m_bCamDirectlyBehind == true) {
        m_fHorizontalAngle = TheCamera.m_fPedOrientForBehindOrInFront + PI;
    }
    if (TheCamera.m_bCamDirectlyInFront == true) {
        m_fHorizontalAngle = TheCamera.m_fPedOrientForBehindOrInFront;
    }
    if (bPlayerInTrain) {
        m_fHorizontalAngle = orientation;
    }

    const double cosA = std::cos((double)m_fVerticalAngle);
    m_vecFront.x      = (float)-(std::cos((double)m_fHorizontalAngle) * cosA);
    m_vecFront.y      = (float)-(std::sin((double)m_fHorizontalAngle) * cosA);
    m_vecFront.z      = (float)std::sin((double)m_fVerticalAngle);
    m_vecSource.x     = tgt.x - dist * m_vecFront.x;
    m_vecSource.y     = tgt.y - dist * m_vecFront.y;
    m_vecSource.z     = tgt.z - dist * m_vecFront.z;
    m_vecTargetCoorsForFudgeInter = tgt;

    CWorld::pIgnoreEntity = m_pCamTargetEntity;
    CColPoint colPoint;
    CEntity*  hitEntity{};
    if (CWorld::ProcessLineOfSight(tgt, m_vecSource, colPoint, hitEntity, true, true, true, true, false, false, true, false)) {
        float nearClip = (tgt - colPoint.m_vecPoint).Magnitude();
        bool  setNearClip = true;
        if (!hitEntity->GetIsTypePed() || dist - nearClip <= 0.4f) {
            const bool farEnough = 0.6f <= nearClip;
            m_vecSource          = colPoint.m_vecPoint;
            if (farEnough) {
                setNearClip = false;
            } else {
                nearClip = nearClip - 0.3f;
                if (nearClip < 0.05f) {
                    nearClip = 0.05f;
                }
            }
        } else {
            const CVector hitPos = colPoint.m_vecPoint;
            if (!CWorld::ProcessLineOfSight(hitPos, m_vecSource, colPoint, hitEntity, true, true, true, true, false, false, true, false)) {
                setNearClip = false;
                const float f = (dist - nearClip) - 0.35f;
                RwCameraSetNearClipPlane(Scene.m_pRwCamera, f <= 0.9f ? f : 0.9f);
            } else {
                const auto delta = tgt - colPoint.m_vecPoint;
                const double mag = delta.Magnitude();
                m_vecSource      = colPoint.m_vecPoint;
                if (0.6f <= mag) {
                    setNearClip = false;
                } else if (0.05f <= mag - 0.3f) {
                    nearClip = (float)(mag - 0.3f);
                } else {
                    nearClip = 0.05f;
                }
            }
        }
        if (setNearClip) {
            RwCameraSetNearClipPlane(Scene.m_pRwCamera, nearClip);
        }
    }

    CWorld::pIgnoreEntity = nullptr;

    const auto CalcSphereRadius = [&](float nearClip) {
        return (float)(std::tan((double)m_fFOV * 0.0174532924f * 0.5f) * (double)CDraw::ms_fAspectRatio * (double)nearClip * 1.1f);
    };

    int32 iter = 0;
    float nearClipNow = RwCameraGetNearClipPlane(Scene.m_pRwCamera);
    const float sphereFactor = (float)(CDraw::ms_fAspectRatio * std::tan((double)m_fFOV * 0.0174532924f * 0.5f) * 1.1f);
    {
        const CVector probe = m_vecFront * nearClipNow + m_vecSource;
        if (CWorld::TestSphereAgainstWorld(probe, nearClipNow * sphereFactor, nullptr, true, true, false, true, false, false)) {
            const float invFactor = 1.0f / sphereFactor;
            bool        hitAgain;
            do {
                const auto& hit = gaTempSphereColPoints[0].m_vecPoint;
                float       dy  = hit.y - m_vecSource.y;
                float       dz  = hit.z - m_vecSource.z;
                float       along = (hit.x - m_vecSource.x) * m_vecFront.x + dy * m_vecFront.y + dz * m_vecFront.z;
                const float perpY = along * m_vecFront.y;
                const float perpZ = along * m_vecFront.z;
                const float perpX = (hit.x - m_vecSource.x) - along * m_vecFront.x;
                dy -= perpY;
                float newNear = std::sqrt(perpX * perpX + dy * dy + (dz - perpZ) * (dz - perpZ)) * invFactor;
                float tmp     = std::min(newNear, nearClipNow);
                if (0.1f <= tmp) {
                    if (nearClipNow < newNear) {
                        newNear = nearClipNow;
                    }
                } else {
                    newNear = 0.1f;
                }
                if (newNear < nearClipNow) {
                    RwCameraSetNearClipPlane(Scene.m_pRwCamera, newNear);
                }
                if (newNear == 0.1f) {
                    const float zDiff = tgt.z - m_vecSource.z;
                    m_vecSource.x     = (tgt.x - m_vecSource.x) * 0.3f + m_vecSource.x;
                    m_vecSource.y     = (tgt.y - m_vecSource.y) * 0.3f + m_vecSource.y;
                    m_vecSource.z     = zDiff * 0.3f + m_vecSource.z;
                }
                nearClipNow = RwCameraGetNearClipPlane(Scene.m_pRwCamera);
                const CVector probe2{
                    nearClipNow * m_vecFront.x + m_vecSource.x,
                    nearClipNow * m_vecFront.y + m_vecSource.y,
                    nearClipNow * m_vecFront.z + m_vecSource.z,
                };
                hitAgain = CWorld::TestSphereAgainstWorld(probe2, CalcSphereRadius(nearClipNow), nullptr, true, true, false, true, false, false) != nullptr;
                iter++;
            } while (iter < 6 && hitAgain);
        }
    }

    const float dist3D = std::sqrt(sq(tgt.x - m_vecSource.x) + sq(tgt.y - m_vecSource.y) + sq(tgt.z - m_vecSource.z));
    if (m_fDistance <= dist3D) {
        const double f = std::pow((double)0.92f, (double)CTimer::GetTimeStep());
        const double newDist = f * m_fDistance + (1.0 - f) * dist3D;
        m_fDistance = (float)newDist;
        if (0.05f < dist3D) {
            const float zD = m_vecSource.z - tgt.z;
            const float fv = (float)newDist;
            const float inv = 1.0f / dist3D;
            m_vecSource.x = (m_vecSource.x - tgt.x) * fv * inv + tgt.x;
            m_vecSource.y = (m_vecSource.y - tgt.y) * fv * inv + tgt.y;
            m_vecSource.z = zD * fv * inv + tgt.z;
        }
        const double clipTarget = newDist - 0.5f;
        if (clipTarget < (double)RwCameraGetNearClipPlane(Scene.m_pRwCamera)) {
            if (clipTarget <= 0.1f) {
                RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.1f);
            } else {
                RwCameraSetNearClipPlane(Scene.m_pRwCamera, (float)clipTarget);
            }
        }
    } else {
        m_fDistance = dist3D;
    }

    TheCamera.m_bCamDirectlyBehind  = false;
    TheCamera.m_bCamDirectlyInFront = false;
    GetVectorsReadyForRW();

    if (TheCamera.m_bFading && TheCamera.m_nFadeInOutFlag == eFadeFlag::FADE_OUT && 0x80 < CDraw::FadeValue) {
        const float heading = (float)std::atan2((double)-m_vecFront.x, (double)m_vecFront.y);
        auto* const ped     = TheCamera.m_pTargetEntity->AsPed();
        ped->m_fAimingRotation  = heading;
        ped->m_fCurrentRotation = heading;
        ped->SetHeading(heading);
        ped->UpdateRwMatrix();
    }
}

// 0x522D40
void CCam::Process_FollowPed_SA(const CVector& target, float orientation, float speedVar, float speedVarWanted, bool bFlag) {
    // NOTE: x87 extended precision is emulated with `double` in the expressions that stay in the FPU registers in the original

    // NOTE: The values the original uses (0x858CB8, 0x858CBC, 0x858FE4), the ones from `common.h` are slightly different
    constexpr float PI      = std::numbers::pi_v<float>;
    constexpr float TWO_PI  = 2.f * PI;
    constexpr float HALF_PI = PI / 2.f;

    if (!m_pCamTargetEntity->GetIsTypePed()) {
        return;
    }
    auto* const ped = m_pCamTargetEntity->AsPed();
    if (!ped->IsPlayer()) {
        return;
    }

    auto* pad = CPad::GetPad(0);
    if (ped->m_nPedType == PED_TYPE_PLAYER2) {
        pad = CPad::GetPad(1);
    }

    CVector tgt = target;

    // NOTE: Passed as the "ped type" to `SetColVarsPed` below as well (that's what the original does)
    const uint32 areaIdx = CGame::GetCurrentAreaCode() != AREA_CODE_NORMAL_WORLD ? 1u : 0u;
    const auto&  tuning  = gFollowPedCamTuning[areaIdx];
    const float  ts      = CTimer::GetTimeStep();

    float zoomDistAdd = tuning.zoomDistAdd;
    if (!ped->bIsStanding && TheCamera.m_nPedZoom == 3u && ped->GetIntelligence()->GetUsingParachute()) {
        zoomDistAdd += zoomDistAdd;
    }

    float       minDist     = tuning.minDist;
    const float clipDistMin = tuning.clipDistMin;
    const float alphaMax    = tuning.alphaMax;
    const float alphaMinMag = tuning.alphaMinMag;

    const double zoomDistExt = (double)TheCamera.m_fPedZoomSmoothed + (double)zoomDistAdd;
    float        zoomDist    = (float)zoomDistExt;
    if (zoomDistExt > (double)gFollowPedLastZoomDist) {
        minDist = zoomDist;
    }
    gFollowPedLastZoomDist = zoomDist; // NOTE: Stored back into `TheCamera.m_fPedZoomSmoothed` on reset (see below)

    // Not-yet-"extended" alpha offset
    float alphaOff = tuning.alphaBase;
    switch (TheCamera.m_nPedZoom) {
    case 1:
        alphaOff += m_fTargetZoomOneZExtra;
        break;
    case 2:
        alphaOff += (areaIdx == 1) ? m_fTargetZoomTwoInteriorZExtra : m_fTargetZoomTwoZExtra;
        break;
    case 3:
        alphaOff += m_fTargetZoomThreeZExtra;
        break;
    }

    float rotScale   = 0.0f; // Scales the beta (horizontal) catch up speed
    float pitchScale = 0.0f; // Scales the alpha (vertical) catch up speed
    if (auto* const swim = ped->GetIntelligence()->GetTaskSwim()) {
        rotScale = 1.0f;
        if (swim->m_nSwimState != SWIM_UNDERWATER_SPRINTING) {
            pitchScale = 0.5f;
        }
    } else if (ped->GetIntelligence()->GetTaskJetPack()) {
        rotScale = 0.5f;
        if (!ped->bIsStanding) {
            pitchScale = 3.0f;
        }
    }

    // Move the FOV back to the default one
    if (!TheCamera.m_bTransitionState) {
        if (!m_bResetStatics) {
            const float  step = ts * 1.0f;
            const double up   = (double)step + (double)m_fFOV;
            if (70.0f > up) {
                m_fFOV = (float)up;
            } else {
                const double down = (double)m_fFOV - (double)step;
                m_fFOV            = (70.0f < down) ? (float)down : 70.0f;
            }
        } else {
            m_fFOV = 70.0f;
        }
    }

    tgt.z += tuning.targetZOffset;

    float clipDist = (zoomDist > clipDistMin) ? zoomDist : clipDistMin;

    const bool bResetting = m_bResetStatics || TheCamera.m_bCamDirectlyBehind || TheCamera.m_bCamDirectlyInFront;
    if (bFlag || bResetting) {
        if (bFlag) {
            gCamPlayerLastPos = ped->GetPosition();
            gCamPlayerPosVel  = CVector{ 0.0f, 0.0f, 0.0f };
            tgt                     = ped->GetPosition();
            tgt.z += tuning.targetZOffset;
        }

        // Reset the camera
        TheCamera.ResetDuckingSystem(ped);
        m_bRotating                = false;
        gbFollowPedCamBehindPlayer = false;
        m_bCollisionChecksOn       = true;

        const bool bKeepAngles = TheCamera.m_bJustCameOutOfGarage || bFlag;
        if (!bKeepAngles) {
            const double a     = PedHeadingExt(ped) - (double)HALF_PI;
            m_fHorizontalAngle = (float)a;
            if (TheCamera.m_bCamDirectlyInFront) {
                m_fHorizontalAngle = (float)(a + (double)PI);
            }
        }

        m_fBetaSpeed  = 0.0f;
        m_fAlphaSpeed = 0.0f;
        m_fDistance   = 1000.0f;

        if (zoomDist == TheCamera.m_fPedZoomBase) {
            TheCamera.m_fPedZoomSmoothed = TheCamera.m_fPedZoomTotal;
            zoomDist                     = TheCamera.m_fPedZoomTotal;
        }

        if (!bKeepAngles) {
            m_fVerticalAngle = 0.0f;
            if (ped->bIsStanding) {
                const double d = DotExt(ped->field_578, ped->GetForward());
                const float  c = (d < -1.0) ? -1.0f : ((1.0 < d) ? 1.0f : (float)d);
                m_fVerticalAngle = (float)-std::asin((double)c);
            }
        }

        {
            const double cosV = std::cos((double)m_fVerticalAngle);
            m_vecFront.x      = (float)-(std::cos((double)m_fHorizontalAngle) * cosV);
            m_vecFront.y      = (float)-(std::sin((double)m_fHorizontalAngle) * cosV);
            m_vecFront.z      = (float)std::sin((double)m_fVerticalAngle);
        }

        // NOTE: The x/y products stay in the FPU registers (exact), only the z one is stored to a float first
        const auto Offset = [&](float dist) {
            const float zProduct = dist * m_vecFront.z;
            return CVector{
                (float)((double)tgt.x - (double)dist * (double)m_vecFront.x),
                (float)((double)tgt.y - (double)dist * (double)m_vecFront.y),
                tgt.z - zProduct,
            };
        };
        m_avecTargetHistoryPos[0] = Offset(clipDist);
        m_anTargetHistoryTime[0]  = CTimer::GetTimeInMS();
        m_avecTargetHistoryPos[1] = Offset(zoomDist);
        m_nCurrentHistoryPoints   = 0;

        if (!TheCamera.m_bJustCameOutOfGarage && !bFlag) {
            m_fVerticalAngle = -alphaOff;
        }

        if (const auto* const swim = ped->GetIntelligence()->GetTaskSwim(); swim && swim->m_nSwimState != SWIM_UNDERWATER_SPRINTING) {
            m_fVerticalAngle = -0.261799395f + m_fVerticalAngle; // -15 deg
        } else if (ped->GetIntelligence()->GetTaskJetPack()) {
            m_fVerticalAngle = -0.349065870f + m_fVerticalAngle; // -20 deg
        }

        CPad::GetPad(0); // NOTE: No-op, but the original calls it
        CPad::ClearMouseHistory();
    } else if (auto* const standingOn = ped->m_standingOnEntity) {
        // Moves the previous camera positions along with the train the ped is standing on
        // NOTE: The velocity is the one of the entity the ped is standing on, even if the train is the one it's attached to
        const auto IsTrain = [](const CEntity* e) { return e->GetIsTypeVehicle() && e->AsVehicle()->m_nVehicleType == VEHICLE_TYPE_TRAIN; };
        if (IsTrain(standingOn) || (standingOn->AsPhysical()->m_pAttachedTo && IsTrain(standingOn->AsPhysical()->m_pAttachedTo))) {
            const auto&  vel = standingOn->AsPhysical()->m_vecMoveSpeed;
            const double mag = std::sqrt(SqMagExt(vel));

            const float a     = std::max(0.0f, (float)(mag - (double)0.01f)); // 0x420800
            const float b     = std::max(0.01f, (float)mag);
            const float ratio = (float)((double)a / (double)b);

            // NOTE: `ratio * vel * ts`, each product rounded to a float
            const CVector scaled = ratio * vel;
            m_avecTargetHistoryPos[0] += scaled * ts;
            m_avecTargetHistoryPos[1] += scaled * ts;
        }
    }

    m_vecFront.x = tgt.x - m_avecTargetHistoryPos[0].x;
    m_vecFront.y = tgt.y - m_avecTargetHistoryPos[0].y;
    m_vecFront.z = tgt.z - m_avecTargetHistoryPos[0].z;
    m_vecFront.Normalise();

    {
        const double dx   = (double)tgt.x - (double)m_avecTargetHistoryPos[1].x;
        const double dy   = (double)tgt.y - (double)m_avecTargetHistoryPos[1].y;
        const double dz   = (double)tgt.z - (double)m_avecTargetHistoryPos[1].z;
        const double dist = std::sqrt((dx * dx + dz * dz) + dy * dy);
        if (dist < (double)zoomDist && zoomDist > tuning.minDist) {
            const float distF = (float)dist;
            zoomDist          = (minDist > distF) ? minDist : distF;
        }
    }

    // The beta (horizontal angle) the camera should have, and the one the ped is facing
    float camBeta;
    {
        const double h = std::atan2((double)-m_vecFront.x, (double)m_vecFront.y) - (double)HALF_PI;
        camBeta        = (float)h;
        if (h < (double)-PI) {
            camBeta = camBeta + TWO_PI;
        }
    }
    float pedBeta;
    {
        const double h    = PedHeadingExt(ped) - (double)HALF_PI;
        pedBeta           = (float)h;
        const double diff = h - (double)camBeta;
        if (diff > (double)PI) {
            pedBeta = pedBeta - TWO_PI;
        } else if (diff < (double)-PI) {
            pedBeta = pedBeta + TWO_PI;
        }
    }

    if (pad->GetForceCameraBehindPlayer()) {
        gbFollowPedCamBehindPlayer = true;
    } else if (gbFollowPedCamBehindPlayer) {
        if (SqMagExt(ped->m_vecMoveSpeed) > (double)0.001f
            || std::abs((double)pedBeta - (double)camBeta) < (double)0.01f
            || pad->AimWeaponLeftRight(ped) != 0
            || pad->AimWeaponUpDown(ped) != 0
        ) {
            gbFollowPedCamBehindPlayer = false;
        }
    }

    // Beta catch up
    const float diffF = (float)((double)pedBeta - (double)camBeta);
    float       sp1   = 0.0f; // Catch up factor
    float       sp2   = 0.0f; // Max catch up
    double      betaDelta = 0.0;
    if (std::abs((double)pedBeta - (double)camBeta) < (double)2.96705985f || gbFollowPedCamBehindPlayer || rotScale != 0.0f) {
        sp1 = ts * tuning.betaChaseRate;
        sp2 = ts * tuning.betaChaseCap;
        if (!gbFollowPedCamBehindPlayer && rotScale == 0.0f) {
            // Catch up faster the faster the ped moves
            const CVector* standingOnSpeed = ped->m_standingOnEntity ? &ped->m_standingOnEntity->AsPhysical()->m_vecMoveSpeed : nullptr;
            const auto     Speed           = [&] {
                return std::sqrt(SqMagExt(standingOnSpeed ? ped->m_vecMoveSpeed - *standingOnSpeed : ped->m_vecMoveSpeed));
            };
            const double scaled = Speed() * (double)sp1;
            sp1                 = (scaled > 1.0) ? 1.0f : (float)(Speed() * (double)sp1);
        } else {
            sp1 = sp1 * 0.5f;
            if (1.0f < sp1) {
                sp1 = 1.0f;
            }
            sp2 = 2.0f * sp2;
            if (rotScale != 0.0f) {
                sp1 = sp1 * rotScale;
                sp2 = sp2 * rotScale;
            }
        }
        double v = (double)diffF * (double)sp1;
        if (v <= (double)sp2) {
            if (v < -(double)sp2) {
                v = -(double)sp2;
            }
        } else {
            v = (double)sp2;
        }
        betaDelta = v;
    }
    camBeta = (float)((double)camBeta + betaDelta);
    if ((double)m_fHorizontalAngle + (double)PI < (double)camBeta) {
        camBeta = camBeta - TWO_PI;
    } else if ((double)m_fHorizontalAngle - (double)PI > (double)camBeta) {
        camBeta = camBeta + TWO_PI;
    }

    const float tsMax          = (1.0f <= ts) ? ts : 1.0f;
    const float betaVelTarget  = (float)(((double)camBeta - (double)m_fHorizontalAngle) / (double)tsMax);

    // The alpha (vertical angle) the camera should have
    float alphaCur;
    {
        const float fz = m_vecFront.z;
        const float c  = (1.0f < fz) ? 1.0f : ((fz < -1.0f) ? -1.0f : fz);
        alphaCur       = (float)std::asin((double)c);
    }

    {
        // Don't look up/down too much if the ped is walking away from the camera
        const double absDiffExt = std::abs((double)pedBeta - (double)camBeta);
        const float  absDiff    = (float)absDiffExt;
        if (absDiffExt > (double)HALF_PI && SqMagExt(ped->m_vecMoveSpeed) > (double)0.002f) {
            double t = (((double)absDiff - (double)HALF_PI) * (double)1.2f) / ((double)PI - (double)HALF_PI);
            if (1.0 < t) {
                t = 1.0;
            }
            const float  limit = (float)((double)HALF_PI - ((double)HALF_PI - (double)0.349065870f) * t);
            const double pw    = std::pow((double)0.9f, (double)ts);
            if (alphaCur > limit) {
                alphaCur = (float)(pw * (double)alphaCur + (1.0 - pw) * (double)limit);
            } else if (-limit > alphaCur) {
                alphaCur = (float)(pw * (double)alphaCur - (1.0 - pw) * (double)limit);
            }
        }
    }

    double pitchDelta = 0.0;
    if (pitchScale != 0.0f || (gbFollowPedCamBehindPlayer && ped->bIsStanding)) {
        double pitchTarget = 0.0;
        if (ped->GetIntelligence()->GetTaskJetPack()) {
            pitchTarget = -0.349065870f;
        } else if (ped->GetIntelligence()->GetTaskSwim()) {
            pitchTarget = -0.261799395f;
        } else if (ped->bIsStanding) {
            const double d = DotExt(ped->field_578, ped->GetForward());
            const float  c = (d < -1.0) ? -1.0f : ((1.0 < d) ? 1.0f : (float)d);
            pitchTarget    = -std::asin((double)c);
        }

        double pitchRate = (double)(1.0f * sp1);
        if (1.0 < pitchRate) {
            pitchRate = 1.0;
        }
        sp2 = 4.0f * sp2;
        if (pitchScale != 0.0f) {
            pitchRate = pitchRate * (double)pitchScale;
            sp2       = sp2 * pitchScale;
        }

        double v = (pitchTarget - (double)alphaCur) * pitchRate;
        if (v <= (double)sp2) {
            if (v < -(double)sp2) {
                v = -(double)sp2;
            }
        } else {
            v = (double)sp2;
        }
        pitchDelta = v;
    }
    alphaCur = (float)(((double)alphaCur + pitchDelta) - (double)alphaOff);

    if (alphaCur > alphaMax) {
        alphaCur = alphaMax;
    } else if (alphaCur < -alphaMinMag) {
        alphaCur = -alphaMinMag;
    }

    // How much the vertical angle can move this frame
    float alphaStep;
    {
        const float  maxAlphaStep = ts * tuning.alphaMaxStep;
        const double pw           = std::pow((double)tuning.alphaPowBase, (double)ts);
        const double stepExt      = ((double)alphaCur - (double)m_fVerticalAngle) * (1.0 - pw);
        alphaStep                 = (float)stepExt;
        if (stepExt > (double)maxAlphaStep) {
            alphaStep = maxAlphaStep;
        } else if (alphaStep < -maxAlphaStep) {
            alphaStep = -maxAlphaStep;
        }
    }

    // Right stick
    float stickLR = (float)(-(int32)pad->AimWeaponLeftRight(ped));
    float stickUD = (float)(int32)pad->AimWeaponUpDown(ped);

    if (auto* const gun = ped->GetIntelligence()->GetTaskUseGun()) {
        if ((gun->m_LastCmd == eGunCommand::FIRE || gun->m_LastCmd == eGunCommand::FIREBURST) && gun->m_WeaponInfo && !gun->m_WeaponInfo->flags.bAimWithArm) {
            if (std::abs(stickLR) < std::abs((float)(int32)pad->GetPedWalkLeftRight())) {
                stickLR = (float)(-(int32)pad->GetPedWalkLeftRight());
            }
        }
    }

    // Look where the camera is looking at when the ped is standing around and the stick is moved
    if ((stickUD != 0.0f || stickLR != 0.0f) && pad->GetPedWalkLeftRight() == 0 && pad->GetPedWalkUpDown() == 0) {
        auto* const   player    = FindPlayerPed(0);
        const CVector playerFwd = player->GetForward();
        const CVector camFwd    = TheCamera.GetForwardVector();
        const double  dot       = ((double)camFwd.x * (double)playerFwd.x + (double)camFwd.z * (double)playerFwd.z) + (double)camFwd.y * (double)playerFwd.y;
        if (dot > (double)0.3f) {
            CVector lookAt = camFwd * 5.0f;
            lookAt         = player->GetPosition() + lookAt;
            g_ikChainMan.LookAt("FollowPedSA", player, nullptr, 1500, BONE_UNKNOWN, &lookAt, false, 0.25f, 500, 3, false);
        }
    }

    {
        const double fovScale = (double)m_fFOV * (double)0.0125f;
        stickLR = (float)(((((double)std::abs(stickLR) * (fovScale * (double)0.0714285746f)) * (double)0.007f) * (double)0.007f) * (double)stickLR);
        stickUD = (float)(((((double)std::abs(stickUD) * (fovScale * (double)0.042857144f)) * (double)0.007f) * (double)0.007f) * (double)stickUD);
    }

    if (auto* const climb = ped->GetIntelligence()->GetTaskClimb()) {
        climb->GetCameraStickModifier(ped, m_fVerticalAngle, m_fHorizontalAngle, stickUD, stickLR);
    } else if (auto* const activeTask = ped->GetIntelligence()->GetTaskManager().GetActiveTask();
        (activeTask || !notsa::IsFixBugs()) /* BUG: The original doesn't check `activeTask` for null */ && activeTask->GetTaskType() == TASK_COMPLEX_ENTER_CAR_AS_DRIVER
    ) {
        // 0x63A380 - Unreversed `CTaskComplexEnterCar` method (`this + 0xC` is the vehicle), adjusts the camera sticks while the ped is entering a car (as the driver)
        plugin::CallMethod<0x63A380, CTask*, CPed*, float, float*, float*, float*, float*>(activeTask, ped, zoomDist, &m_fVerticalAngle, &m_fHorizontalAngle, &stickUD, &stickLR);
    }

    // Beta
    const float  pwB         = (float)std::pow((double)tuning.betaPowBase, (double)ts);
    const float  speedCap    = tuning.speedCap;
    const float  oneMinusPwB = 1.0f - pwB;
    {
        double v = (double)stickLR + (double)betaVelTarget;
        if (v <= (double)speedCap) {
            if (v < -(double)speedCap) {
                v = -(double)speedCap;
            }
        } else {
            v = (double)speedCap;
        }
        const double betaSpeed = (double)pwB * (double)m_fBetaSpeed + (double)oneMinusPwB * v;
        m_fBetaSpeed           = (float)betaSpeed;
        if (std::abs(betaSpeed) < (double)0.0001f) {
            m_fBetaSpeed = 0.0f;
        }
    }

    const bool bMouseCam = CCamera::m_bUseMouse3rdPerson && pad->DisablePlayerControls == 0;
    {
        double add;
        if (bMouseCam) {
            add = ((((double)CPad::NewMouseControllerState.m_AmountMoved.x * (double)-2.5f) * ((double)m_fFOV * (double)0.0125f)) * (double)CCamera::m_fMouseAccelHorzntl);
            m_fBetaSpeed = 0.0f;
            stickLR      = (float)add;
        } else {
            add = (double)ts * (double)m_fBetaSpeed;
        }
        m_fHorizontalAngle = (float)(add + (double)m_fHorizontalAngle);
    }
    ClipBeta();

    // Alpha
    {
        const double alphaSpeed = (double)oneMinusPwB * (double)m_fAlphaSpeed + (double)stickUD * (double)pwB;
        const float  alphaSpeedF = (float)alphaSpeed;
        m_fAlphaSpeed            = alphaSpeedF;
        if (alphaSpeed <= (double)speedCap) {
            if (alphaSpeedF < -speedCap) {
                m_fAlphaSpeed = -speedCap;
            }
        } else {
            m_fAlphaSpeed = speedCap;
        }
        if (std::abs(m_fAlphaSpeed) < 0.0001f) {
            m_fAlphaSpeed = 0.0f;
        }
    }
    alphaCur = (float)((double)ts * (double)m_fAlphaSpeed + (double)alphaCur);

    float vertDelta;
    if (bMouseCam) {
        // BUG: Uses the horizontal mouse acceleration for the vertical movement as well
        const float mouseAccelY = notsa::IsFixBugs() ? CCamera::m_fMouseAccelVertical : CCamera::m_fMouseAccelHorzntl;
        stickUD = (float)((((double)CPad::NewMouseControllerState.m_AmountMoved.y * (double)2.5f) * ((double)m_fFOV * (double)0.0125f)) * (double)mouseAccelY);

        const uint8 fade = CDraw::FadeValue;
        if ((TheCamera.m_bFading && TheCamera.GetFadingDirection() == 1 && fade > 0x2D) || fade > 200) {
            // Move the camera to the "default" vertical angle while fading
            const float fadeAlpha = -alphaOff;
            if ((double)fadeAlpha - (double)0.05f > (double)m_fVerticalAngle) {
                stickUD = 0.05f;
            } else if (m_fVerticalAngle < fadeAlpha) {
                stickUD = fadeAlpha - m_fVerticalAngle;
            } else if ((double)fadeAlpha + (double)0.05f < (double)m_fVerticalAngle) {
                stickUD = -0.05f;
            } else if (m_fVerticalAngle > fadeAlpha) {
                stickUD = fadeAlpha - m_fVerticalAngle;
            } else {
                stickUD = 0.0f;
            }
        }
        m_fAlphaSpeed = 0.0f;
        vertDelta     = stickUD;
    } else {
        vertDelta = alphaStep;
    }

    m_fVerticalAngle = vertDelta + m_fVerticalAngle;
    if (m_fVerticalAngle > alphaMax) {
        m_fVerticalAngle = alphaMax;
        m_fAlphaSpeed    = 0.0f;
    } else if (-alphaMinMag > m_fVerticalAngle) {
        m_fVerticalAngle = -alphaMinMag;
        m_fAlphaSpeed    = 0.0f;
    }

    // Snap to the angles of the last frame if the change is tiny
    if (std::abs((double)gFollowPedLastAlpha - (double)m_fVerticalAngle) < (double)0.0001f) {
        m_fVerticalAngle = gFollowPedLastAlpha;
    }
    gFollowPedLastAlpha = m_fVerticalAngle;
    if (std::abs((double)gFollowPedLastBeta - (double)m_fHorizontalAngle) < (double)0.0001f) {
        m_fHorizontalAngle = gFollowPedLastBeta;
    }
    gFollowPedLastBeta = m_fHorizontalAngle;

    {
        const double cosV = std::cos((double)m_fVerticalAngle);
        m_vecFront.x      = (float)-(std::cos((double)m_fHorizontalAngle) * cosV);
        m_vecFront.y      = (float)-(std::sin((double)m_fHorizontalAngle) * cosV);
        m_vecFront.z      = (float)std::sin((double)m_fVerticalAngle);
    }
    GetVectorsReadyForRW();

    TheCamera.m_bCamDirectlyBehind  = false;
    TheCamera.m_bCamDirectlyInFront = false;

    {
        const float zProduct = zoomDist * m_vecFront.z;
        m_vecSource.x        = (float)((double)tgt.x - (double)zoomDist * (double)m_vecFront.x);
        m_vecSource.y        = (float)((double)tgt.y - (double)zoomDist * (double)m_vecFront.y);
        m_vecSource.z        = tgt.z - zProduct;
        LimitPrecision(m_vecSource);
    }

    // The camera positions for the next frames (the history)
    {
        const double alphaExt = (double)alphaCur + (double)alphaOff;
        alphaCur              = (float)alphaExt;
        const float  cosA     = (float)std::cos(alphaExt);
        const double t1Ext    = -(std::cos((double)m_fHorizontalAngle) * (double)cosA);
        const float  t1       = (float)t1Ext;
        const double t2Ext    = -(std::sin((double)m_fHorizontalAngle) * (double)cosA);
        const double sinAExt  = std::sin((double)alphaCur);
        const float  sinA     = (float)sinAExt;

        const float p1 = t1 * clipDist;
        const float p2 = (float)(t2Ext * (double)clipDist);
        m_avecTargetHistoryPos[0].x = tgt.x - p1;
        m_avecTargetHistoryPos[0].y = tgt.y - p2;
        m_avecTargetHistoryPos[0].z = (float)((double)tgt.z - sinAExt * (double)clipDist);

        const float r1 = t1 * zoomDist;
        const float r2 = (float)(t2Ext * (double)zoomDist);
        m_avecTargetHistoryPos[1].x = tgt.x - r1;
        m_avecTargetHistoryPos[1].y = tgt.y - r2;
        m_avecTargetHistoryPos[1].z = (float)((double)tgt.z - (double)sinA * (double)zoomDist);
    }

    if (pad->GetForceCameraBehindPlayer() && pad->AimWeaponLeftRight(nullptr) != 0) {
        double d = (double)m_fHorizontalAngle - ((double)ped->m_fCurrentRotation - (double)HALF_PI);
        if (d > (double)PI) {
            d -= (double)TWO_PI;
        } else if (d < (double)-PI) {
            d += (double)TWO_PI;
        }
        if ((double)ts * (double)0.1f > std::abs(d)) {
            ped->m_fAimingRotation = (float)((double)m_fHorizontalAngle + (double)HALF_PI);
        }
    }

    TheCamera.HandleCameraMotionForDucking(ped, &m_vecSource, &tgt, false);
    m_vecTargetCoorsForFudgeInter = tgt;
    LimitPrecision(m_vecSource);

    CCamera::SetColVarsPed((ePedType)areaIdx, (int32)TheCamera.m_nPedZoom);

    if (gCameraDirection == 3) {
        TheCamera.CameraGenericModeSpecialCases(ped);
        CCollision::bCamCollideWithVehicles = true;
        CCollision::bCamCollideWithObjects  = true;
        CCollision::bCamCollideWithPeds     = true;
        TheCamera.CameraColDetAndReact(&m_vecSource, &tgt);
        TheCamera.ImproveNearClip(nullptr, ped, &m_vecSource, &tgt);
        LimitPrecision(m_vecSource);
    }

    TheCamera.m_bCamDirectlyBehind  = false;
    TheCamera.m_bCamDirectlyInFront = false;
    LimitPrecision(m_vecSource);
    GetVectorsReadyForRW();

    if (areaIdx == 0
        && TheCamera.m_nWhoIsInControlOfTheCamera != 1u
        && ped->bIsStanding
        && !CGameLogic::IsCoopGameGoingOn()
        && !TheCamera.m_bFOVLerpProcessed
        && !TheCamera.m_bVecMoveLinearProcessed
        && !TheCamera.m_bVecTrackLinearProcessed
        && SqMagExt(ped->m_vecMoveSpeed) <= (double)0.0001f
    ) {
        gIdleCam.Process();
        m_bResetStatics = false;
        return;
    }

    gCamUnkB6FE34 = 0;
    m_bResetStatics     = false;
}

// 0x5105C0
void CCam::Process_M16_1stPerson(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    // NOTSA: Not reversed yet, forwards to the original code (the hook is disabled, see `InjectHooks`)
    plugin::CallMethod<0x5105C0, CCam*, const CVector*, float, float, float>(this, &target, orientation, speedVar, speedVarWanted);
}

// 0x511B50
void CCam::Process_Rocket(const CVector& target, float orientation, float speedVar, float speedVarWanted, bool isHeatSeeking) {
    static auto& dword_B6FFF8 = StaticRef<uint32>(0xB6FFF8);
    static auto& dword_B6FFFC = StaticRef<uint32>(0xB6FFFC);
    static auto& byte_B70000  = StaticRef<bool>(0xB70000);

    if (!m_pCamTargetEntity->GetIsTypePed()) {
        return;
    }

    auto* targetPed = m_pCamTargetEntity->AsPed();
    m_fFOV = 70.0f;
    if (m_bResetStatics) {
        if (!CCamera::m_bUseMouse3rdPerson || targetPed->m_pTargetedObject) {
            m_fVerticalAngle = 0.0f;
            m_fHorizontalAngle = targetPed->m_fCurrentRotation - DegreesToRadians(90.0f);
        }
        m_fInitialPlayerOrientation = m_fHorizontalAngle;
        m_bResetStatics             = 0;
        m_bCollisionChecksOn        = true;
        byte_B70000                 = 0;
        dword_B6FFFC                = 0;
        dword_B6FFF8                = 0;
    }
    m_pCamTargetEntity->UpdateRwMatrix();
    m_pCamTargetEntity->UpdateRwFrame();
    CVector headPosition{};
    targetPed->GetTransformedBonePosition(headPosition, eBoneTag::BONE_HEAD, true);
    m_vecSource = headPosition + CVector{0.0f, 0.0f, 0.1f};

    auto*      pad1   = CPad::GetPad(0);
    const auto fov    = m_fFOV / 80.0f;
    const auto amountMouseMoved = pad1->NewMouseControllerState.GetAmountMouseMoved();
    
    if (!amountMouseMoved.IsZero()) {
        m_fHorizontalAngle += -3.0f * amountMouseMoved.x * fov * CCamera::m_fMouseAccelHorzntl;
        m_fVerticalAngle += +4.0f * amountMouseMoved.y * fov * CCamera::m_fMouseAccelVertical;
    } else {
        const auto hv  = (float)-pad1->LookAroundLeftRight(targetPed);
        const auto vv  = (float)pad1->LookAroundUpDown(targetPed);

        m_fHorizontalAngle += sq(hv) / 10000.0f * fov / 17.5f * CTimer::GetTimeStep() * (hv < 0.0f ? -1.0f : 1.0f);
        m_fVerticalAngle   += sq(vv) / 22500.0f * fov / 14.0f * CTimer::GetTimeStep() * (vv < 0.0f ? -1.0f : 1.0f);
    }
    ClipBeta();
    ClipAlpha();

    m_vecFront.Set(
        -(std::cos(m_fHorizontalAngle) * std::cos(m_fVerticalAngle)),
        -(std::sin(m_fHorizontalAngle) * std::cos(m_fVerticalAngle)),
        std::sin(m_fVerticalAngle)
    );
    GetVectorsReadyForRW();

    const auto heading = CGeneral::GetATanOfXY(m_vecFront.x, m_vecFront.y) - DegreesToRadians(90.0f);
    TheCamera.m_pTargetEntity->AsPed()->m_fCurrentRotation = heading;
    TheCamera.m_pTargetEntity->AsPed()->m_fAimingRotation  = heading;

    if (isHeatSeeking) {
        auto* player     = FindPlayerPed();
        auto* playerData = player->GetPlayerData();
        if (!playerData->m_nFireHSMissilePressedTime) {
            playerData->m_nFireHSMissilePressedTime = CTimer::GetTimeInMS();
        }

        const auto hsTarget = CWeapon::PickTargetForHeatSeekingMissile(
            m_vecSource,
            m_vecFront,
            1.2f,
            player,
            false,
            playerData->m_LastHSMissileTarget
        );

        // NOTE: not sure about the second one
        if (hsTarget && CTimer::GetTimeInMS() - playerData->m_nLastHSMissileLOSTime > 1'000) {
            playerData->m_nLastHSMissileLOSTime = CTimer::GetTimeInMS();

            const auto targetUsesCollision = hsTarget->GetUsesCollision();
            const auto playerUsesCollision = player->GetUsesCollision();
            hsTarget->SetUsesCollision(false);
            player->SetUsesCollision(false);

            const auto isClear = CWorld::GetIsLineOfSightClear(
                player->GetPosition(),
                hsTarget->GetPosition(),
                true,
                true,
                false,
                true,
                false,
                true
            );
            player->SetUsesCollision(playerUsesCollision);
            hsTarget->SetUsesCollision(targetUsesCollision);
            playerData->m_bLastHSMissileLOS = isClear;
        }

        if (!playerData->m_bLastHSMissileLOS || !hsTarget || hsTarget != playerData->m_LastHSMissileTarget) {
            playerData->m_nFireHSMissilePressedTime = CTimer::GetTimeInMS();
        }

        if (hsTarget) {
            CWeaponEffects::MarkTarget(
                CrossHairId(0),
                hsTarget->GetPosition(),
                255,
                255,
                255,
                100,
                1.3f,
                true
            );
        }

        auto& crosshair = gCrossHair[CrossHairId(0)];
        const auto time = CTimer::GetTimeInMS() - playerData->m_nFireHSMissilePressedTime;

        crosshair.m_nTimeWhenToDeactivate = 0;
        crosshair.m_color.Set(
            255,
            time <= 1'500 ? 255 : 0,
            time <= 1'500 ? 255 : 0
        );
        crosshair.m_fRotation = time <= 1'500 ? 0.0f : 1.0f;
        playerData->m_LastHSMissileTarget = hsTarget;
    }

    constexpr auto ROCKET_CAM_NEARCLIP_PLANE = 0.15f; // 0x8CCC9C
    RwCameraSetNearClipPlane(Scene.m_pRwCamera, ROCKET_CAM_NEARCLIP_PLANE);
}

// 0x517500
void CCam::Process_SpecialFixedForSyphon(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    m_vecSource                   = m_vecCamFixedModeSource;
    m_vecTargetCoorsForFudgeInter = target;
    m_vecTargetCoorsForFudgeInter.z += m_fSyphonModeTargetZOffSet;
    m_vecFront = target - m_vecSource;

    const CVector fixedSource = m_vecCamFixedModeSource;
    AvoidTheGeometry(fixedSource, m_vecTargetCoorsForFudgeInter, m_vecSource, m_fFOV);
    m_vecFront.z += m_fSyphonModeTargetZOffSet;
    GetVectorsReadyForRW();

    m_vecUp += m_vecCamFixedModeUpOffSet;
    m_vecUp.Normalise();
    const auto right = CrossProduct(m_vecUp, m_vecFront).Normalized();
    m_vecFront       = CrossProduct(right, m_vecUp);
    m_vecFront.Normalise();

    m_fFOV = 70.0f;

    auto* const ent = m_pCamTargetEntity;
    if (ent && ent->GetIsTypePed()) {
        auto* const ped = ent->AsPed();
        if (ped->m_pTargetedObject) {
            auto* const wi = CWeaponInfo::GetWeaponInfo(ped->GetActiveWeapon().m_Type, ped->GetWeaponSkill());
            if (wi && (!wi->flags.bAimWithArm || ped->bIsDucking) && wi->m_nWeaponFire != eWeaponFire::WEAPON_FIRE_MELEE) {
                const auto dir = ped->m_pTargetedObject->GetPosition() - ped->GetPosition();
                const auto heading = (float)std::atan2((double)-dir.x, (double)dir.y);
                ped->m_fAimingRotation  = heading;
                ped->m_fCurrentRotation = heading;
                ped->SetHeading(heading);
                ped->UpdateRwMatrix();
            }
        }
    }
}

// 0x512110
bool CCam::Process_WheelCam(const CVector&, float, float, float) {
    m_fFOV = 70.0f;

    auto* const targetEntity = m_pCamTargetEntity;
    CVector     right{}, up{};
    float       colX = 0.0f; // local_50 of the original, reused by the bike case below
    constexpr float camY = -2.3f, camZ = 0.3f;

    if (targetEntity->GetIsTypePed()) {
        m_vecSource = targetEntity->GetMatrix().TransformVector(CVector{ -0.3f, -0.5f, 0.1f });
        m_vecSource += targetEntity->GetPosition();
        m_vecFront = CVector{ 1.0f, 0.0f, 0.0f };
    } else {
        colX = targetEntity->GetColModel()->m_boundBox.m_vecMin.x - 0.33f;
        m_vecSource = targetEntity->GetMatrix().TransformPoint(CVector{ colX, camY, camZ });
        m_vecFront  = targetEntity->GetMatrix().GetForward();
    }

    bool skipGeneric = false;
    if (targetEntity->GetIsTypeVehicle() && (targetEntity->AsVehicle()->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI || targetEntity->AsVehicle()->GetVehicleAppearance() == VEHICLE_APPEARANCE_PLANE)) {
        auto& mat   = targetEntity->GetMatrix();
        right       = mat.GetRight();
        up          = mat.GetUp();
        m_vecSource = mat.TransformPoint(CVector{ -1.55f, camY, camZ });
        skipGeneric = true;
    } else if (targetEntity->GetIsTypeVehicle()) {
        auto* const veh = targetEntity->AsVehicle();
        if (veh->m_nVehicleType == VEHICLE_TYPE_BOAT) {
            right = CrossProduct(m_vecFront, CVector{ 0.0f, 0.0f, 1.0f }).Normalized();
            up    = CrossProduct(right, m_vecFront).Normalized();

            if (!veh->m_pDriver) {
                m_vecSource.z = 0.3f + 0.3f + m_vecSource.z;
            } else {
                CVector pos{};
                veh->m_pDriver->GetBonePosition(&pos, BONE_HEAD, true);
                pos.x = CTimer::GetTimeStep() * veh->m_vecMoveSpeed.x + pos.x;
                pos.y = CTimer::GetTimeStep() * veh->m_vecMoveSpeed.y + pos.y;
                pos.z = CTimer::GetTimeStep() * veh->m_vecMoveSpeed.z + pos.z;
                pos += right * -0.5f;
                const auto& fwd = veh->GetMatrix().GetForward();
                pos.x = -0.8f * fwd.x + pos.x;
                pos.y = -0.8f * fwd.y + pos.y;
                pos.z = -0.8f * fwd.z + pos.z + 0.3f;
                if (targetEntity->m_nModelIndex == MODEL_PREDATOR) {
                    pos += right * 0.2f;
                    const auto offset = ((veh->GetMatrix().GetForward() * -1.0f) * -0.2f);
                    pos.x = pos.x + offset.x;
                    pos.y = pos.y + offset.y;
                    pos.z = (pos.z + offset.z) + 1.0f * -0.3f;
                }
                m_vecSource = pos;
            }
            skipGeneric = true;
        } else if (veh->m_nVehicleType == VEHICLE_TYPE_BIKE) {
            auto& mat = veh->GetMatrix();
            right     = mat.GetRight();
            up        = CVector{ 0.0f, 0.0f, 1.0f };
            m_vecFront = CrossProduct(m_vecUp, right).Normalized();
            colX       = (float)(((double)0.33f - (double)0.2f) + (double)colX); // NOTE: x87 extended precision
            m_vecSource = veh->GetPosition();
            m_vecSource += mat.GetRight() * colX;
            m_vecSource += m_vecFront * camY;
            m_vecSource += m_vecUp * camZ;
            skipGeneric = true;
        } else if (veh->m_nVehicleType == VEHICLE_TYPE_TRAIN) {
            if (veh->m_vecMoveSpeed.x * m_vecFront.x + m_vecFront.y * veh->m_vecMoveSpeed.y + m_vecFront.z * veh->m_vecMoveSpeed.z < 0.0f) {
                m_vecFront = -m_vecFront;
            }
        }
    }

    if (!skipGeneric) {
        right = CrossProduct(m_vecFront, CVector{ 0.0f, 0.0f, 1.0f }).Normalized();
        up    = CrossProduct(right, m_vecFront).Normalized();
    }

    if (float waterLevel = 0.0f; CWaterLevel::GetWaterLevel(m_vecSource.x, m_vecSource.y, m_vecSource.z, waterLevel, true, nullptr) && m_vecSource.z < waterLevel - 0.3f) {
        ApplyUnderwaterMotionBlur();
    }

    const double angle = std::cos((double)(CTimer::GetTimeInMS() & 0x1FFFF) * (double)4.7936901e-05f) * (double)0.4f;
    const double s     = std::sin(angle);
    const double c     = std::cos(angle);
    m_vecUp.x          = (float)((double)up.x * c + (float)((double)right.x * s));
    m_vecUp.y          = (float)((double)up.y * c) + (float)((double)right.y * s);
    m_vecUp.z          = (float)((double)(float)((double)up.z * c) + (double)right.z * s);
    m_vecUp.Normalise();
    m_vecFront.Normalise();

    CWorld::pIgnoreEntity = targetEntity;
    CColPoint colPoint;
    CEntity*  hitEntity{};
    const bool hit        = CWorld::ProcessLineOfSight(m_vecSource, targetEntity->GetPosition(), colPoint, hitEntity, true, false, false, true, false, false, true, false);
    CWorld::pIgnoreEntity = nullptr;
    return !hit;
}

// based on 0x51847C - 0x5184EC
void CCam::ApplyUnderwaterMotionBlur() {
    static constexpr uint32 UNDERWATER_CAM_BLUR      = 20;    // 0x8CC7A4
    static constexpr float  UNDERWATER_CAM_MAG_LIMIT = 10.0f; // 0x8CC7A8

    const auto colorMag = std::sqrt(
        sq(CTimeCycle::GetWaterRed()) +
        sq(CTimeCycle::GetWaterGreen()) +
        sq(CTimeCycle::GetWaterBlue())
    );

    const auto factor = (colorMag <= UNDERWATER_CAM_MAG_LIMIT) ? 1.0f : UNDERWATER_CAM_MAG_LIMIT / colorMag;

    TheCamera.SetMotionBlur(
        static_cast<uint32>(factor * CTimeCycle::GetWaterRed()),
        static_cast<uint32>(factor * CTimeCycle::GetWaterGreen()),
        static_cast<uint32>(factor * CTimeCycle::GetWaterBlue()),
        UNDERWATER_CAM_BLUR,
        eMotionBlurType::LIGHT_SCENE
    );
}

// 0x4D58A0
int32 ConvertPedNode2BoneTag(int32 simpleId) {
    const auto map = notsa::make_mapping<int32, int32>({
        { 1,  3 },
        { 2,  5 },
        { 3,  32},
        { 4,  22},
        { 5,  34},
        { 6,  24},
        { 7,  41},
        { 8,  51},
        { 9,  43},
        { 10, 53},
        { 11, 52},
        { 12, 42},
        { 13, 33},
        { 14, 23},
        { 15, 31},
        { 16, 21},
        { 17, 4 },
        { 18, 8 },
    });
    return notsa::find_value_or(map, simpleId, -1);
}

// 0x509A30
bool IsLampPost(eModelID modelId) {
    using namespace ModelIndices;

    return notsa::contains<eModelID>(
        {
            MI_SINGLESTREETLIGHTS1,
            MI_SINGLESTREETLIGHTS2,
            MI_SINGLESTREETLIGHTS3,
            MI_BOLLARDLIGHT,
            MI_MLAMPPOST,
            MI_STREETLAMP1,
            MI_STREETLAMP2,
            MI_TELPOLE02,
            MI_TRAFFICLIGHTS_MIAMI,
            MI_TRAFFICLIGHTS_TWOVERTICAL,
            MI_TRAFFICLIGHTS_3,
            MI_TRAFFICLIGHTS_4,
            MI_TRAFFICLIGHTS_GAY,
            MI_TRAFFICLIGHTS_5,
        },
        modelId
    );
}
