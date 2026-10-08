#include "StdInc.h"

#include "Cam.h"
#include "TimeCycle.h"
#include "Camera.h"
#include "Shadows.h"
#include "IdleCam.h"
#include "InterestingEvents.h"
#include "ModelIndices.h"
#include "HandShaker.h"

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
        fovRange             = 50.0f;
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

//! First hit point of `CWorld::TestSphereAgainstWorld` (`gaTempSphereColPoints[0].m_vecPoint` in World.cpp)
static inline auto& gTempSphereHitPoint0 = StaticRef<CVector>(0xB9B250);

//! Written by `Process_FollowCar_SA`, read by `LookBehind` (name made up)
static inline auto& gCamFollowCarLookAt = StaticRef<CVector>(0xB6F018);

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

//! Seems to be some "line of sight mode" byte consulted by `CWorld::ProcessLineOfSight`, name unknown
static inline auto& gUnkLOSModeByte_8CCB80 = StaticRef<uint8>(0x8CCB80);

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
    gUnkLOSModeByte_8CCB80 = 5;
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
    RH_ScopedInstall(Process, 0x526FC0, { .Reversed = false });
    RH_ScopedInstall(ProcessArrestCamOne, 0x518500, { .Reversed = false });
    RH_ScopedInstall(ProcessPedsDeadBaby, 0x519250);
    RH_ScopedInstall(Process_1rstPersonPedOnPC, 0x50EB70, { .Reversed = false });
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
    RH_ScopedInstall(Process_FollowCar_SA, 0x5245B0, { .Reversed = false });
    RH_ScopedInstall(Process_FollowPedWithMouse, 0x50F970);
    RH_ScopedInstall(Process_FollowPed_SA, 0x522D40, { .Reversed = false });
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

    const auto dist    = (dst - src).Magnitude();
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
    static CVector prevSource = source;
    static CVector prevTarget = target;
    static CVector prevUp     = up;
    static float   prevBeta   = beta;
    static float   prevAlpha  = alpha;
    static float   prevFov    = fov;

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
    const float angle = sign * 1.57079637f + CGeneral::GetATanOfXY(dir.x, dir.y);
    m_vecSource.x     = (float)(std::cos((double)angle) * dist + targetPos.x);
    m_vecSource.y     = (float)(std::sin((double)angle) * dist + targetPos.y);

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
        targetOrientation = std::atan2(-moveSpeed.x, moveSpeed.y) - PI / 2.0f;
    }

    const float dist2D = (CVector2D{ m_vecSource } - CVector2D{ target }).Magnitude();

    if (std::abs(WrapAngleToPi(targetOrientation - m_fHorizontalAngle)) > 0.349065781f && isMovingFwd && !TheCamera.m_bTransitionState) {
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

        m_vecSource.x = target.x - -(std::cos(m_fHorizontalAngle) * dist2D);
        m_vecSource.y = target.y - -(std::sin(m_fHorizontalAngle) * dist2D);

        if (std::abs(WrapAngleToPi(targetOrientation - m_fHorizontalAngle)) < 0.0349065848f) {
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
    if (m_fHorizontalAngle < DegreesToRadians(-180.0f)) {
        m_fHorizontalAngle += DegreesToRadians(360.0f);
    } else {
        m_fHorizontalAngle -= DegreesToRadians(360.0f);
    }
}

// 0x526FC0
void CCam::Process() {
    NOTSA_UNREACHABLE();
}

// 0x518500
void CCam::ProcessArrestCamOne() {
    NOTSA_UNREACHABLE();
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
    static auto& v3d_8CCC54   = StaticRef<CVector>(0x8CCC54);
    static auto& byte_B6FFDC  = StaticRef<bool>(0xB6FFDC);
    static auto& v3d_B6FFC4   = StaticRef<CVector>(0xB6FFC4);
    static auto& v3d_B6FFD0   = StaticRef<CVector>(0xB6FFD0);

    if (m_nMode != MODE_SNIPER_RUNABOUT) {
        m_fFOV = 70.0f;
    }

    if (!m_pCamTargetEntity->GetRwObject()) {
        return;
    }

    if (!m_pCamTargetEntity->GetIsTypePed()) {
        m_bResetStatics = false;
        RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.05f);
        return;
    }

    const auto hier = GetAnimHierarchyFromSkinClump(m_pCamTargetEntity->GetRpClump());
    const auto aIdx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(2)); // todo: enum
    auto&      aMat = RpHAnimHierarchyGetMatrixArray(hier)[aIdx];
    auto*      targetPed = m_pCamTargetEntity->AsPed();

    CVector pointIn = v3d_8CCC54;
    RwV3dTransformPoint(&pointIn, &pointIn, &aMat);
    RwV3d v3dZero{ 0.0f };
    RwMatrixScale(&aMat, &v3dZero, rwCOMBINEPRECONCAT);

    if (m_bResetStatics) {
        // unnecessary entity ped check
        m_fVerticalAngle = 0.0f;
        byte_B6FFDC      = false;
        v3d_B6FFD0.Reset();
        m_fHorizontalAngle            = targetPed->m_fCurrentRotation + DegreesToRadians(90.0f);
        m_bCollisionChecksOn          = true;
        m_fInitialPlayerOrientation   = m_fHorizontalAngle;
        m_vecBufferedPlayerBodyOffset = v3d_B6FFC4 = pointIn;
    }
    m_vecBufferedPlayerBodyOffset.y = pointIn.y;

    if (TheCamera.m_bHeadBob) {
        m_vecBufferedPlayerBodyOffset.x = lerp(
            pointIn.x,
            m_vecBufferedPlayerBodyOffset.x,
            TheCamera.m_fScriptPercentageInterToCatchUp
        );

        m_vecBufferedPlayerBodyOffset.z = lerp(
            pointIn.z,
            m_vecBufferedPlayerBodyOffset.z,
            TheCamera.m_fScriptPercentageInterToCatchUp
        );

        m_vecSource = targetPed->GetMatrix().TransformPoint(m_vecBufferedPlayerBodyOffset);
    } else {
        const auto targetFwd = targetPed->GetForward().Normalized();
        const auto mag       = (pointIn - v3d_B6FFC4).Magnitude2D();

        m_vecSource = targetFwd * mag * 1.23f + targetPed->GetPosition() + CVector{ 0.0f, 0.0f, 0.59f };
    }

    CVector spinePos{};
    targetPed->GetTransformedBonePosition(spinePos, BONE_SPINE1, true);

    // TODO: Put in a function name e.g. 'HandleFreeMouseControl'?
    auto*      pad1   = CPad::GetPad(0);
    const auto fov    = m_fFOV / 80.0f;
    const auto amountMouseMoved = pad1->NewMouseControllerState.GetAmountMouseMoved();

    if (!amountMouseMoved.IsZero()) {
        m_fHorizontalAngle += -3.0f * amountMouseMoved.x * fov * CCamera::m_fMouseAccelHorzntl;
        m_fVerticalAngle += +4.0f * amountMouseMoved.y * fov * CCamera::m_fMouseAccelVertical;
    } else {
        const auto hv = (float)-pad1->LookAroundLeftRight(targetPed);
        const auto vv = (float)pad1->LookAroundUpDown(targetPed);

        m_fHorizontalAngle += sq(hv) / 10000.0f * fov / 17.5f * CTimer::GetTimeStep() * (hv < 0.0f ? -1.0f : 1.0f);
        m_fVerticalAngle += sq(vv) / 22500.0f * fov / 14.0f * CTimer::GetTimeStep() * (vv < 0.0f ? -1.0f : 1.0f);
    }
    ClipBeta();
    ClipAlpha();

    if (const auto* a = targetPed->m_pAttachedTo; targetPed->IsPlayer() && a) {
        // enum?
        switch (targetPed->m_fTurretAngleA) {
        case 0u:
            m_fHorizontalAngle -= a->GetHeading() + DegreesToRadians(90.0f);
            break;
        case 1u:
            m_fHorizontalAngle -= a->GetHeading() + DegreesToRadians(180.0f);
            break;
        case 2u:
            m_fHorizontalAngle -= a->GetHeading() + DegreesToRadians(-90.0f);
            break;
        case 3u:
            m_fHorizontalAngle -= a->GetHeading();
            break;
        default:
            // NOTE(yukani): If this is fired, gimme a call. 0x50F0ED
            NOTSA_UNREACHABLE();
            break;
        }

        // ...
    }
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
void CCam::Process_AimWeapon(const CVector&, float, float, float) {
    NOTSA_UNREACHABLE();
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
    static auto& s_mode                 = StaticRef<int32>(0x8A5E50);   // 0 = follow player 1, 1 = follow player 2, 2 = both
    static auto& s_lastClearTime        = StaticRef<uint32>(0xB6EC24);
    static auto& s_lastBlockedTime      = StaticRef<uint32>(0xB6EC28);
    static auto& s_helpMessageTime      = StaticRef<uint32>(0x96A8B4);
    static auto& s_helpMessageCount     = StaticRef<int32>(0x96A8B8);

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

    if (!IsSwitchPressed(CPad::GetPad(0))) {
        if (IsSwitchPressed(CPad::GetPad(1))) {
            s_mode = (s_mode == 1) + 1;
        }
    } else {
        s_mode = ((s_mode != 0) - 1) & 2;
    }

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
            local70        = (float)(std::atan2((double)-vel.x, (double)vel.y) - (double)(PI / 2.0f));
            const double d = (double)local70 - (double)fVar4;
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

    const float powFactor = std::pow(0.8f, CTimer::GetTimeStep());
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
        gUnkLOSModeByte_8CCB80 = 5;
        if (CWorld::ProcessLineOfSight(m_vecTargetCoorsForFudgeInter, m_vecSource, colPoint, hitEntity, true, false, false, false, false, true, true, false)) {
            m_vecSource = colPoint.m_vecPoint;
        }
        if (s_helpMessageTime < CTimer::GetTimeInMS() && s_helpMessageCount < 6) {
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
        const float f = std::pow(0.98f, CTimer::GetTimeStep());
        m_fFOV        = (m_fFOV - 70.0f) * f + 70.0f;
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
        const double f = std::pow(0.85f, CTimer::GetTimeStep());
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

        const double f2 = std::pow(0.85f, CTimer::GetTimeStep());
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
        const double f = std::pow(0.96f, CTimer::GetTimeStep());
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
        const double cosb = std::cos((double)rotExcess);
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
            const float t      = m_fFOV * 0.00872664619f;
            const float tanT   = std::tan(t);
            const auto  upTerm = m_vecUp * (std::tan(t) / CDraw::ms_fAspectRatio * m_fY_Targetting);
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
        float      ang   = std::atan2(-toAim.x, toAim.y);
        float      diff  = ang - veh->GetHeading();
        if (diff > PI) {
            diff -= 2.0f * PI;
        } else if (diff < -PI) {
            diff += 2.0f * PI;
        }
        diff += 0.7853981852531433f;
        if (diff < 0.0f) {
            diff += 2.0f * PI;
        }
        const auto fakeShootDirn = (int32)(diff * 0.6366197466850281f);

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

    const float angle = m_fTwoPlayerFocusBlend * PI;
    const float s     = (float)std::sin((double)angle);
    const float c     = (float)((std::cos((double)angle) + 1.0) * 0.5);

    const float dist = (veh1->GetPosition() - veh2->GetPosition()).Magnitude();

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
void CCam::Process_DW_BirdyCam(bool) {
    constexpr int32 CAM_ID = 22;

    TheCamera.m_bUseNearClipScript = false;
    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        TheCamera.m_bUseNearClipScript = false;
        return;
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
            return;
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
            return;
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
        return;
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
            return;
        }
    } else {
        const bool wasAbove = 30 < s_clearCounter;
        s_clearCounter++;
        if (wasAbove) {
            s_clearCounter = 30;
        }
    }

    Finalise_DW_CineyCams(src, dest, 0.0f, 70.0f, 0.3f, 0.0f);
}

// 0x51B120
void CCam::Process_DW_CamManCam(bool) {
    constexpr int32 CAM_ID = 21;

    TheCamera.m_bUseNearClipScript = false;
    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        TheCamera.m_bUseNearClipScript = false;
        return;
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
            return;
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
            return;
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
        return;
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
            return;
        }
    }

    // `FUN_00420800` is `max(a, b)`; the result is always >= 0.2 so the `< 0` branch of the original is dead
    const float shake = std::min(std::max(vel.Magnitude() * 8.0f, 0.2f), 1.0f);
    Finalise_DW_CineyCams(src, dest, 0.0f, fov, 10.0f - fov * 0.0142857144f * 9.69999981f, shake);
}

// 0x51A740
void CCam::Process_DW_HeliChaseCam(bool) {
    constexpr int32 CAM_ID = 20;

    TheCamera.m_bUseNearClipScript = false;

    // The original picks one of several settings structs using `rand * 0.0` (always the first one), but still consumes a random number
    (void)CGeneral::GetRandomNumber();
    auto& S = gDWHeliChaseCamSettings;

    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        return;
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
            return;
        }
    }

    if (gbExitCam[CAM_ID]) {
        return;
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
            return;
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
            return;
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
        return;
    }
    Finalise_DW_CineyCams(src, dest, roll, fov, S.nearClip, 1.0f);
}

// 0x51C760
void CCam::Process_DW_PlaneCam1(bool) {
    constexpr int32 CAM_ID = 26;

    TheCamera.m_bUseNearClipScript = false;
    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        TheCamera.m_bUseNearClipScript = false;
        return;
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
                return;
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
                return;
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
                return;
            }
        }

        if (!IsTimeToExitThisDWCineyCamMode(CAM_ID, src, dest, t, false)) {
            Finalise_DW_CineyCams(src, dest, 0.0f, 70.0f, 5.0f, 1.0f);
            return;
        }
    }
    gbExitCam[CAM_ID] = true;
}

// 0x51CC30
void CCam::Process_DW_PlaneCam2(bool) {
    constexpr int32 CAM_ID = 27;

    TheCamera.m_bUseNearClipScript = false;
    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        TheCamera.m_bUseNearClipScript = false;
        return;
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
                return;
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
                return;
            }
        }

        if (!IsTimeToExitThisDWCineyCamMode(CAM_ID, src, dest, t, false)) {
            Finalise_DW_CineyCams(src, dest, 0.0f, 70.0f, 5.0f, 1.0f);
            return;
        }
    }
    gbExitCam[CAM_ID] = true;
}

// 0x51D100
void CCam::Process_DW_PlaneCam3(bool) {
    constexpr int32 CAM_ID = 28;

    TheCamera.m_bUseNearClipScript = false;
    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        TheCamera.m_bUseNearClipScript = false;
        return;
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
                return;
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
                return;
            }
        }

        if (!IsTimeToExitThisDWCineyCamMode(CAM_ID, src, dest, t, false)) {
            Finalise_DW_CineyCams(src, dest, 0.0f, 70.0f, 5.0f, 1.0f);
            return;
        }
    }
    gbExitCam[CAM_ID] = true;
}

// 0x51C250
void CCam::Process_DW_PlaneSpotterCam(bool) {
    constexpr int32 CAM_ID = 23;

    TheCamera.m_bUseNearClipScript = false;
    if (!m_pCamTargetEntity || !m_pCamTargetEntity->GetIsTypeVehicle()) {
        TheCamera.m_bUseNearClipScript = false;
        return;
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
            pos.x = (100.0f - 50.0f) * (float)CGeneral::GetRandomNumber() * (1.0f / 32768.0f) + 50.0f + dest.x;
            pos.y = (100.0f - 50.0f) * (float)CGeneral::GetRandomNumber() * (1.0f / 32768.0f) + 50.0f + dest.y;

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
            return;
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
                return;
            }
        }

        if (!IsTimeToExitThisDWCineyCamMode(CAM_ID, src, dest, t, false)) {
            Finalise_DW_CineyCams(src, dest, 0.0f, fov, 10.0f - fov * 0.0142857144f * 9.69999981f, 1.0f);
            return;
        }
    }
    gbExitCam[CAM_ID] = true;
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
void CCam::Process_FlyBy(const CVector&, float, float, float) {
    NOTSA_UNREACHABLE();
}

// 0x5245B0
void CCam::Process_FollowCar_SA(const CVector&, float, float, float, bool) {
    NOTSA_UNREACHABLE();
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
            const float fovScale = m_fFOV * 0.0125f;
            rotH = 0.0714285746f * fovScale * CTimer::GetTimeStep() * 0.01f * (float)-(int32)lookLR;
            rotV = fovScale * 0.042857144f * CTimer::GetTimeStep() * (float)(int32)lookUD * 0.01f;
        } else {
            const float fovScale = m_fFOV * 0.0125f;
            rotH = CCamera::m_fMouseAccelHorzntl * fovScale * mouse.x * -2.5f;
            rotV = fovScale * mouse.y * 4.0f * CCamera::m_fMouseAccelVertical;
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
                const auto& hit = gTempSphereHitPoint0;
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
        const double f = std::pow(0.92f, CTimer::GetTimeStep());
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
void CCam::Process_FollowPed_SA(const CVector&, float, float, float, bool) {
    NOTSA_UNREACHABLE();
}

// 0x5105C0
void CCam::Process_M16_1stPerson(const CVector&, float, float, float) {
    NOTSA_UNREACHABLE();
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
                const auto heading = std::atan2(-dir.x, dir.y);
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
                    pos.z = 1.0f * -0.3f + pos.z + offset.z;
                }
                m_vecSource = pos;
            }
            skipGeneric = true;
        } else if (veh->m_nVehicleType == VEHICLE_TYPE_BIKE) {
            auto& mat = veh->GetMatrix();
            right     = mat.GetRight();
            up        = CVector{ 0.0f, 0.0f, 1.0f };
            m_vecFront = CrossProduct(m_vecUp, right).Normalized();
            colX       = (0.33f - 0.2f) + colX;
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

    const double angle = std::cos((double)(CTimer::GetTimeInMS() & 0x1FFFF) * 4.7936901e-05) * 0.4;
    const double s     = std::sin(angle);
    const double c     = std::cos(angle);
    m_vecUp.x          = (float)((double)up.x * c + (float)((double)right.x * s));
    m_vecUp.y          = (float)((double)up.y * c) + (float)((double)right.y * s);
    m_vecUp.z          = (float)((double)up.z * c + (double)right.z * s);
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
