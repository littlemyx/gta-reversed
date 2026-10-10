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
#include "Tasks/TaskTypes/TaskComplexEnterCar.h"
#include "PostEffects.h"
#include "Tasks/TaskTypes/TaskSimpleSwim.h"
#include "Tasks/TaskTypes/TaskSimpleUseGun.h"
#include "WeaponInfo.h"
#include "PedIntelligence.h"
#include "Ragdoll/IKChainManager.h"
#include "GameLogic.h"
#include "ControllerConfigManager.h"
#include "TheScripts.h"
#include "Collision/Collision.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Bike.h"
#include "Entity/Vehicle/Plane.h"
#include "Tasks/TaskTypes/TaskComplexProstituteSolicit.h"
#include "Tasks/TaskTypes/TaskSimpleArrestPed.h"
#include "Entity/Ped/CopPed.h"
#include "PlayerPedData.h"
#include "Messages.h"
#include "Hud.h"
#include "Text/Text.h"
#include "game_sa/WeaponEffects.h"
#line 37

NOTSA_GLOBAL(gbFirstPersonRunThisFrame, 0xB6EC20, (bool), {});
NOTSA_GLOBAL(gLastFrameProcessedDWCineyCam, 0x8CCB9C, (uint32), { 0xFFFFFFFFU });

// Indexed by the DW cinematic cam id (20..28), see `IsTimeToExitThisDWCineyCamMode`
// NOTE: Only the indices 20..28 are used (= 0xB6EC70..0xB6EC78), the first 20 bytes overlap other (unrelated) variables, eg. `gArrestCamOneCop` (0xB6EC5C)
NOTSA_GLOBAL_LOCAL(gbExitCam, 0xB6EC5C, (std::array<bool, 29>), {}); // detached: own storage (a VIEW over unrelated bytes in the exe, only [20,29) are used; aliases.json "views")
static inline NOTSA_GLOBAL(gDWCineyCamMinDist, 0x8CCBCC, (std::array<float, 9>), { 3.0f, 3.0f, 1.0f, 3.0f, 5.0f, 3.0f, 3.0f, 3.0f, 3.0f });
static inline NOTSA_GLOBAL(gDWCineyCamMaxDist, 0x8CCBF0, (std::array<float, 9>), { 185.0f, 100.0f, 100.0f, 100.0f, 30.0f, 30.0f, 100.0f, 100.0f, 100.0f });
static inline NOTSA_GLOBAL(gLastDWCineyCamMode, 0x8CC488, (int32), { -1 });    // NOTE: name made up, holds the id of the last processed DW cinematic cam mode (reset to -1 in `Process`)
static inline NOTSA_GLOBAL(gDWCineyCamStartTime, 0x8CCBA0, (uint32), { 0xFFFFFFFFU }); // NOTE: name made up
static inline NOTSA_GLOBAL(gDWCineyCamEndTime, 0x8CCBA4, (uint32), { 0xFFFFFFFFU }); // NOTE: name made up, only compared against the current time in `IsTimeToExitThisDWCineyCamMode`

static inline NOTSA_GLOBAL(DWCineyCamLastPos, 0xB6FE8C, (CVector), {});
static inline NOTSA_GLOBAL(DWCineyCamLastUp, 0xB6FE98, (CVector), {});
static inline NOTSA_GLOBAL(DWCineyCamLastRight, 0xB6FEA4, (CVector), {});
static inline NOTSA_GLOBAL(DWCineyCamLastFwd, 0xB6FEB0, (CVector), {});

static inline NOTSA_GLOBAL(DWCineyCamLastNearClip, 0xB6EC08, (float), {});
static inline NOTSA_GLOBAL(DWCineyCamLastFov, 0xB6EC0C, (float), {});

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
static inline NOTSA_GLOBAL(gCamFollowCarLookAt, 0xB6F018, (CVector), {});

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
static inline NOTSA_GLOBAL(gFollowPedCamTuning, 0x8CC548, (std::array<FollowPedCamTuning, 3>), { // the 3rd row (0x8CC5C0) is read by `Process_Cam_TwoPlayer` (exe), same values as the 1st
    FollowPedCamTuning{ 0.6f, 2.0f, 0.15f, 2.0f, 4.0f, 0.8f, 0.1f, 0.5f, 0.8f, 0.1f, 0.1f, 0.02f, 1.0f, 0.7853982f, 1.4835299f },
    FollowPedCamTuning{ 0.6f, 2.0f, 0.15f, 2.0f, 3.0f, 0.9f, 0.1f, 1.0f, 0.8f, 0.1f, 0.3f, 0.05f, 1.0f, 0.7853982f, 0.7853982f },
    FollowPedCamTuning{ 0.6f, 2.0f, 0.15f, 2.0f, 4.0f, 0.8f, 0.1f, 0.5f, 0.8f, 0.1f, 0.1f, 0.02f, 1.0f, 0.7853982f, 1.4835299f },
});
#line 166

// Globals of `Process_FollowPed_SA` (names made up)
static inline NOTSA_GLOBAL(gFollowPedLastZoomDist, 0xB6EC50, (float), {}); // Camera distance of the last frame
static inline NOTSA_GLOBAL(gbFollowPedCamBehindPlayer, 0xB6EC54, (bool), {});  // Set while the "camera behind player" button is held (until the ped moves)
static inline NOTSA_GLOBAL(gFollowPedLastAlpha, 0x8CCE74, (float), { -9999.0f });
static inline NOTSA_GLOBAL(gFollowPedLastBeta, 0x8CCE6C, (float), { -9999.0f });

// Shared by `Process` and `Process_FollowPed_SA` (names made up)
static inline NOTSA_GLOBAL(gCamPlayerLastPos, 0x8CCC3C, (CVector), { 0.0f, 0.0f, 10000.0f }); // Position of the followed player of the last frame (`Process_FollowPed_SA` resets it)
static inline NOTSA_GLOBAL(gCamPlayerPosVel, 0xB6EC7C, (CVector), {}); // Smoothed velocity of the above (`Process_FollowPed_SA` resets it)
static inline NOTSA_GLOBAL_ALIAS(gCamUnkB6FE34, 0xB6FE34, (int32), *reinterpret_cast<int32*>(reinterpret_cast<uint8*>(&gIdleCam) + 148));   // Compared with 0xB6FDC8 in `Process`, zeroed by `Process_FollowPed_SA` and `CIdleCam::IdleCamGeneralProcess`

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
        return x87::atan2((double)-fwd.x, (double)fwd.y);
    }
    return (double)ped->m_placement.m_fHeading;
}

// 0x50A0A0 - Rounds to the given number of decimal digits (hooked below)
static float LimitPrecision(float v, int32 digits) {
    // NOTE: x87 extended precision in the original, the intermediates are kept in `double` here
    //       (0x822130 = pow, 0x8232F0 = modf; the 5.0f / 0.1f are float constants from .rdata)
    double ipart{};
    std::modf(
        (std::pow(10.0, (double)(digits + 1)) * (double)v + (v < 0.0f ? -5.0 : 5.0)) * (double)0.1f, // !(v < 0) <=> the original's JP after FCOMP (NaN adds)
        &ipart
    );
    return (float)(ipart / std::pow(10.0, (double)digits));
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
    outFront.x         = (float)-(x87::cos((double)angle) * x87::cos(pitch));
    outFront.y         = (float)-(x87::sin((double)angle) * x87::cos(pitch));
    outFront.z         = (float)x87::sin(pitch);

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

void WellBufferMe(float target, float& valueToChange, float& speedSoFar, float topSpeed, float speedStep, bool isAnAngle); // Defined below

//! Names made up: `AvoidTheGeometry` smooths the strength of the push-away vector with these (see `WellBufferMe`)
static inline NOTSA_GLOBAL(gAvoidGeometryStrength, 0xB6EC38, (float), {});
static inline NOTSA_GLOBAL(gAvoidGeometrySpeed, 0xB6EC3C, (float), {});
static inline NOTSA_GLOBAL(gbAvoidGeometryDoSecondLOS, 0xB6EC65, (bool), {}); // NOTSA name

// 0x514030 - Defined here (and not in Camera.cpp), as it's only used by the cams
void CCamera::AvoidTheGeometry(const CVector* src, const CVector* dst, CVector* out, float FOV) {
    // Camera -> target. NOTE: x87 extended precision is kept by the original for `dx` (only its stored copy is rounded)
    const double dx   = (double)dst->x - (double)src->x;
    const float  dxf  = (float)dx;
    const float  dy   = dst->y - src->y;
    const float  dz   = dst->z - src->z;
    CVector      dir{};
    m_vecClearGeometryVec = CVector{};
    const float len3D = (float)std::sqrt(((double)dz * dz + dx * dx) + (double)dy * dy);
    const float len2D = (float)std::sqrt(dx * dx + (double)dy * dy);

    // Direction (heading, pitch) of the camera
    const float heading = (dx == 0.0 && dy == 0.f)
        ? CGeneral::GetATanOfXY(m_mCameraMatrix.GetForward().x, m_mCameraMatrix.GetForward().y)
        : CGeneral::GetATanOfXY(dxf, dy);
    // 0x514133: the 2nd result is NOT spilled, `fcos` / `fsin` below use it unrounded (`fld st(0); fcos; ...; fsin`); the 1st one is stored as float (0x514100)
    const double pitch = (len2D == 0.f && dz == 0.f)
        ? 0.0
        : CGeneral::GetATanOfXYExt(len2D, dz);
    dir.x = (float)(x87::cos((double)heading) * x87::cos(pitch));
    dir.y = (float)(x87::sin((double)heading) * x87::cos(pitch));
    dir.z = (float)x87::sin(pitch);

    // Move the camera `len3D` away from the target, along `dir`
    {
        const double ex  = (double)dir.x * len3D;
        const double ey  = (double)dir.y * len3D;
        const float  ez  = (float)((double)dir.z * len3D);
        out->x = (float)((double)dst->x - ex);
        out->y = (float)((double)dst->y - ey);
        out->z = (float)((double)dst->z - (double)ez);
    }
    dir.Normalise();

    const auto GetNearClip = [] { return RwCameraGetNearClipPlane(Scene.m_pRwCamera); };
    const auto Dist = [](const CVector& a, const CVector& b) { // (x^2 + z^2) + y^2
        const double x = (double)a.x - b.x, y = (double)a.y - b.y, z = (double)a.z - b.z;
        return std::sqrt((x * x + z * z) + y * y);
    };

    // Line of sight from the target to the camera
    CColPoint colPoint;
    CEntity*  hitEntity{};
    CWorld::pIgnoreEntity = m_pTargetEntity;
    if (CWorld::ProcessLineOfSight(*dst, *out, colPoint, hitEntity, true, false, false, true, false, false, true, false)) {
        const CVector hit1 = colPoint.m_vecPoint;
        *out               = hit1;
        if (gbAvoidGeometryDoSecondLOS) {
            if (CWorld::ProcessLineOfSight(*out, *dst, colPoint, hitEntity, false, true, true, true, false, false, true, false)) {
                if (Dist(*out, colPoint.m_vecPoint) < (double)GetNearClip()) {
                    *out = colPoint.m_vecPoint;
                } else if (Dist(*out, hit1) < (double)GetNearClip()) {
                    *out = hit1;
                }
            }
        }
    }
    CWorld::pIgnoreEntity = nullptr;

    // Don't let the near clip plane cut the player
    if (FindPlayerPed(-1)) {
        const CVector toTarget = *dst - *out;
        const double  d        = Dist(toTarget, CVector{}) - (double)0.5f; // 0x8CC38C
        if (d < (double)GetNearClip()) {
            constexpr float MIN_NEAR_CLIP = 0.15f; // 0x8CC390
            RwCameraSetNearClipPlane(Scene.m_pRwCamera, d <= (double)MIN_NEAR_CLIP ? MIN_NEAR_CLIP : (float)d);
        }
    }

    // Check if anything is in the way of the near clip plane's "box", and push the camera away from it
    float strengthTarget = 0.f;
    {
        const double halfTan = x87::tan((double)FOV * (double)0.017453292f * (double)0.5f); // 0x8595EC, 0x858B8C
        const double widthK  = halfTan * (double)CDraw::ms_fAspectRatio * (double)1.15f;    // 0x8CC820
        const float  nearClip = GetNearClip();
        const float  radius   = (float)((double)nearClip * widthK);
        const float  offX     = (float)((double)dir.x * nearClip);
        const float  offY     = (float)((double)dir.y * nearClip);
        const CVector center{
            (float)((double)offX + out->x),
            (float)((double)offY + out->y),
            (float)((double)dir.z * nearClip + out->z)
        };

        if (CWorld::TestSphereAgainstWorld(center, radius, nullptr, true, false, false, true, false, true)) {
            const auto& hit = gaTempSphereColPoints[0];
            CVector     toHit{ hit.m_vecPoint.x - center.x, hit.m_vecPoint.y - center.y, hit.m_vecPoint.z - center.z };

            // How far in front of the camera is the hit
            const double depth = ((((double)hit.m_vecPoint.x - out->x) * dir.x + ((double)hit.m_vecPoint.z - out->z) * dir.z)) + ((double)hit.m_vecPoint.y - out->y) * dir.y;
            constexpr float MIN_NEAR = 0.15f; // 0x8CC390
            constexpr float MAX_NEAR = 0.9f;  // 0x858C20
            if (depth > (double)MIN_NEAR) {
                if (depth < (double)MAX_NEAR && depth < (double)GetNearClip()) {
                    RwCameraSetNearClipPlane(Scene.m_pRwCamera, (float)depth);
                }
            } else if (depth < (double)MIN_NEAR) {
                RwCameraSetNearClipPlane(Scene.m_pRwCamera, MIN_NEAR);
            }

            const float penetration = (float)((double)radius - Dist(toHit, CVector{}));
            toHit.Normalise();
            CVector normal = hit.m_vecNormal;
            normal.Normalise();

            const double xr = (double)toHit.x * penetration; // NOTE: kept in extended precision by the original
            const float  yr = toHit.y * penetration;
            const float  zr = toHit.z * penetration;
            if (((-(double)toHit.x * normal.x + -(double)toHit.z * normal.z) + -(double)toHit.y * normal.y) < 0.0) {
                normal = -normal;
            }
            strengthTarget = 1.f;
            const double push = ((-xr * normal.x + -(double)zr * normal.z) + -(double)yr * normal.y);
            m_vecClearGeometryVec.x = (float)(normal.x * push);
            m_vecClearGeometryVec.y = (float)(normal.y * push);
            m_vecClearGeometryVec.z = (float)(normal.z * push);

            // Remember on which side of the ped the camera was pushed to (used by `Process`?)
            if (m_pTargetEntity && m_pTargetEntity->GetIsTypePed() && 2.f * MIN_NEAR /* 0x8CC390 */ > GetNearClip()) {
                const auto& top = m_pTargetEntity->GetMatrix().GetForward();
                const auto NdotTop = [&] { return ((double)normal.z * top.z + (double)normal.y * top.y) + (double)normal.x * top.x; };
                if (NdotTop() < 0.0) {
                    if (m_fAvoidTheGeometryProbsTimer < 0.f) {
                        m_fAvoidTheGeometryProbsTimer = 0.f;
                    }
                    m_fAvoidTheGeometryProbsTimer = (float)((double)CTimer::ms_fTimeStep + m_fAvoidTheGeometryProbsTimer);
                } else if (NdotTop() > 0.5) { // 0x858B8C
                    if (m_fAvoidTheGeometryProbsTimer > 0.f) {
                        m_fAvoidTheGeometryProbsTimer = 0.f;
                    }
                    m_fAvoidTheGeometryProbsTimer = (float)((double)m_fAvoidTheGeometryProbsTimer - CTimer::ms_fTimeStep);
                }
                if (m_nAvoidTheGeometryProbsDirn == 0) {
                    const auto cross = CrossProduct(m_pTargetEntity->GetPosition() - *out, normal);
                    m_nAvoidTheGeometryProbsDirn = cross.z > 0.f ? (uint16)0xFFFF : (uint16)1;
                }
            }
        }
    }

    m_fAvoidTheGeometryProbsTimer = (float)(std::pow((double)0.9f, (double)CTimer::ms_fTimeStep) * m_fAvoidTheGeometryProbsTimer); // 0x8CC81C
    WellBufferMe(strengthTarget, gAvoidGeometryStrength, gAvoidGeometrySpeed, 0.2f, 0.05f, false);
    m_vecClearGeometryVec.x = gAvoidGeometryStrength * m_vecClearGeometryVec.x;
    m_vecClearGeometryVec.y = gAvoidGeometryStrength * m_vecClearGeometryVec.y;
    m_vecClearGeometryVec.z = gAvoidGeometryStrength * m_vecClearGeometryVec.z;
    m_bMoveCamToAvoidGeom   = true;
}

// 0x509AE0 - the single implementation, shared with Camera.cpp (declared there)
void WellBufferMe(float target, float& valueToChange, float& speedSoFar, float topSpeed, float speedStep, bool isAnAngle) {
    constexpr double PI     = (double)std::numbers::pi_v<float>;         // 0x858CB8
    constexpr double TWO_PI = (double)(2.f * std::numbers::pi_v<float>); // 0x858CBC

    // x87 extended precision is kept by the original, except for the (angle) difference which is spilled to a float
    double diff = (double)target - valueToChange;
    if (isAnAngle) {
        diff = (double)(float)diff;
        for (; diff >= PI; diff -= TWO_PI) {
            ;
        }
        for (; diff < -PI; diff += TWO_PI) {
            ;
        }
    }

    const double fullSpeed = diff * topSpeed;
    const double speedDiff = fullSpeed - speedSoFar;
    const double change    = std::abs(speedDiff) * CTimer::GetTimeStep() * speedStep;
    // (FCOM + TEST 0x41): subtracts if `speedDiff <= 0` or unordered
    speedSoFar = (float)(speedDiff <= 0.0 || std::isnan(speedDiff) ? speedSoFar - change : change + speedSoFar);

    if (fullSpeed < 0.0 && fullSpeed > speedSoFar) {
        speedSoFar = (float)fullSpeed;
    } else if (fullSpeed > 0.0 && fullSpeed < speedSoFar) {
        speedSoFar = (float)fullSpeed;
    }

    const float timeStep = CTimer::GetTimeStep();
    valueToChange = (float)((10.0f < timeStep ? 10.0f : timeStep) * (double)speedSoFar + valueToChange);
}

namespace {
// Defined above `CCam::Process_FlyBy`, declared here so they can be hooked
void FlyBySplineVec3(CVector* out, float* data, float time, int32* idx);
void FlyBySplineFloat(float* out, float* data, float time, int32* idx);
} // namespace

namespace {
// Defined above `CCam::ProcessArrestCamOne`, declared here so they can be hooked
bool __stdcall GetArrestCamPosBesideCop(CEntity* target, CPed* cop, const CVector* targetPos, CVector* outPos);
bool __stdcall GetArrestCamPosBehindTarget(CEntity* target, CPed* cop, const CVector* targetPos, CVector* outPos);
} // namespace

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
    RH_ScopedInstall(ProcessArrestCamOne, 0x518500);
    RH_ScopedInstall(ArrestCamLookAtCopHead, 0x512EF0);
    RH_ScopedInstall(ProcessPedsDeadBaby, 0x519250);
    RH_ScopedInstall(Process_1rstPersonPedOnPC, 0x50EB70);
    RH_ScopedInstall(Process_1stPerson, 0x517EA0);
    RH_ScopedInstall(Process_AimWeapon, 0x521500);
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
    RH_ScopedInstall(Process_FlyBy, 0x5B25F0);
    RH_ScopedInstall(Process_FollowCar_SA, 0x5245B0);
    RH_ScopedInstall(Process_FollowPedWithMouse, 0x50F970);
    RH_ScopedInstall(Process_FollowPed_SA, 0x522D40);
    RH_ScopedInstall(Process_M16_1stPerson, 0x5105C0);
    RH_ScopedInstall(Process_Rocket, 0x511B50);
    RH_ScopedInstall(Process_SpecialFixedForSyphon, 0x517500);
    RH_ScopedInstall(Process_WheelCam, 0x512110);

    RH_ScopedGlobalInstall(WellBufferMe, 0x509AE0);
    RH_ScopedGlobalOverloadedInstall(LimitPrecision, "float", 0x50A0A0, float(*)(float, int32));
    RH_ScopedGlobalInstall(FlyBySplineVec3, 0x5B2090);
    RH_ScopedGlobalInstall(FlyBySplineFloat, 0x5B2330);
    RH_ScopedGlobalInstall(GetArrestCamPosBesideCop, 0x515D80);
    RH_ScopedGlobalInstall(GetArrestCamPosBehindTarget, 0x516010);

    // `CCamera::AvoidTheGeometry` is defined in this file (see above)
    {
        RH_ScopedClass(CCamera);
        RH_ScopedCategory("Camera");
        RH_ScopedInstall(AvoidTheGeometry, 0x514030);
    }
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
        auto rightDir = m_vecFront.Cross({ (float)(x87::sin(roll)), 0.0f, (float)(x87::cos(roll)) }).Normalized();
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
        auto rightDir = m_vecFront.Cross({ (float)(x87::sin(roll)), 0.0f, (float)(x87::cos(roll)) }).Normalized();
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
        const auto tanFov = x87::tan(m_fFOV * PI / 360.0f);

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
    NOTSA_GLOBAL_LOCAL(prevFov, 0xB6FF5C, (float), {});
    NOTSA_GLOBAL_LOCAL(prevAlpha, 0xB6FF60, (float), {});
    NOTSA_GLOBAL_LOCAL(prevBeta, 0xB6FF64, (float), {});
    NOTSA_GLOBAL_LOCAL(prevUp, 0xB6FF68, (CVector), {});
    NOTSA_GLOBAL_LOCAL(prevTarget, 0xB6FF74, (CVector), {});
    NOTSA_GLOBAL_LOCAL(prevSource, 0xB6FF80, (CVector), {});
    NOTSA_GLOBAL_LOCAL(initGuardMask, 0xB6FF8C, (uint32), {});

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

        m_vecSource.x = -x87::cos(m_fHorizontalAngle);
        m_vecSource.y = -x87::sin(m_fHorizontalAngle);
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
    const double angle = (double)sign * (double)1.57079637f + CGeneral::GetATanOfXYExt(dir.x, dir.y); // NOTE: x87 keeps this in extended precision
    m_vecSource.x      = (float)(x87::cos(angle) * dist + targetPos.x);
    m_vecSource.y      = (float)(x87::sin(angle) * dist + targetPos.y);

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
        targetOrientation = (float)(x87::atan2((double)-moveSpeed.x, (double)moveSpeed.y) - (double)(PI / 2.0f));
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

        m_vecSource.x = (float)((double)target.x - -(x87::cos((double)m_fHorizontalAngle) * (double)dist2D));
        m_vecSource.y = (float)((double)target.y - -(x87::sin((double)m_fHorizontalAngle) * (double)dist2D));

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
    static NOTSA_GLOBAL_ALIAS(s_unk_B6FDC8, 0xB6FDC8, (float), *reinterpret_cast<float*>(reinterpret_cast<uint8*>(&gIdleCam) + 40));
    static NOTSA_GLOBAL_ALIAS(s_unk_C0B184, 0xC0B184, (uint8), *reinterpret_cast<uint8*>(reinterpret_cast<uint8*>(&g_InterestingEvents) + 300));
    static NOTSA_GLOBAL_ALIAS(s_unk_C8A860, 0xC8A860, (uint8), *reinterpret_cast<uint8*>(reinterpret_cast<uint8*>(&gCrossHair) + 40));
    NOTSA_GLOBAL_LOCAL(s_unk_8CCF00, 0x8CCF00, (bool), { true });
    static NOTSA_GLOBAL_ALIAS(s_firstPersonFlag, 0xB6EC20, (bool), gbFirstPersonRunThisFrame);

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

namespace {
// The state of the "arrest cam one" (names made up)
NOTSA_GLOBAL(gArrestCamOneMode, 0xB6EC58, (int32), {});    // 0 = none yet, 1 = from the cop's head, 2 = beside the cop, 3..7 = free cams (see `ProcessArrestCamOne`), 8 = from a lamp post
NOTSA_GLOBAL(gArrestCamOneCop, 0xB6EC5C, (CEntity*), {}); // The cop the cam is set up at (registered reference)
NOTSA_GLOBAL(gArrestCamOneStartTime, 0xB6EC60, (float), {});    // `CTimer::m_snTimeInMilliseconds` when the "from the cop's head" mode was started

// Function local statics (with their init flags) of 0x512EF0 (names made up)
NOTSA_GLOBAL(gArrestCamHeadOffset, 0xB70004, (CVector), {}); // Offset applied to the camera's position, (0, 0, -0.5)
NOTSA_GLOBAL(gArrestCamUnusedVec, 0xB70010, (CVector), {}); // Only initialised to (0, 0, 0), never read by the original code
NOTSA_GLOBAL(gArrestCamStaticsInit, 0xB7001C, (uint32), {});  // Bit 0: `gArrestCamUnusedVec` is initialised, bit 1: `gArrestCamHeadOffset` is initialised
NOTSA_GLOBAL(gbArrestCamHeadPhase, 0xB70020, (bool), {});    // Flips every call once the time passed
NOTSA_GLOBAL(gArrestCamHeadTimeShift, 0xB70024, (float), {});   // Subtracted from the elapsed time (never written to by the code seen)

constexpr float RAND_RECIPROCAL = 3.05185094e-05f; // 0x858C7C, ~ 1/RAND_MAX

} // namespace

// 0x512EF0
// Puts the camera at the arrested player and makes it look at the head of the arresting cop (`cop`).
// If `checkLineOfSight` is set it also might randomly fail, and fails if the cop's head isn't visible.
bool CCam::ArrestCamLookAtCopHead(CPed* cop, bool checkLineOfSight) {
    if (checkLineOfSight) {
        const double rnd = (double)CGeneral::GetRandomNumber() * (double)RAND_RECIPROCAL;
        if (rnd > (double)0.65f) {
            return false;
        }
    }

    auto* const target = TheCamera.m_pTargetEntity;
    if (target->GetType() != ENTITY_TYPE_PED || !cop) {
        return false;
    }

    // `t` is the time since the start of the mode, in seconds (clamped to [0, 1])
    // NOTE: The whole expression is calculated in extended precision, then rounded to float
    const double tExt = (((double)CTimer::GetTimeInMS() - (double)gArrestCamOneStartTime) - (double)gArrestCamHeadTimeShift) / 1000.0;
    float        t    = (float)tExt;
    bool         flipPhase;
    if (tExt > 1.0) {
        t         = 1.0f;
        flipPhase = true;
    } else if (t < 0.0f) {
        t         = 0.0f;
        flipPhase = false;
    } else {
        flipPhase = !(t < 1.0f);
    }
    if (flipPhase) {
        gbArrestCamHeadPhase = !gbArrestCamHeadPhase;
    } else {
        gbArrestCamHeadPhase = true;
    }

    if (!(gArrestCamStaticsInit & 1)) {
        gArrestCamStaticsInit |= 1;
        gArrestCamUnusedVec = CVector{ 0.0f, 0.0f, 0.0f };
    }
    if (!(gArrestCamStaticsInit & 2)) {
        gArrestCamStaticsInit |= 2;
        gArrestCamHeadOffset = CVector{ 0.0f, 0.0f, -0.5f };
    }

    const CVector oldSource = m_vecSource;
    m_vecSource             = target->GetPosition();
    m_fFOV                  = 100.0f;

    // Position of the head bone (Not using `CPed::GetBonePosition` as that one doesn't read the matrix directly)
    auto* const    hierarchy = GetAnimHierarchyFromSkinClump(cop->GetRpClump());
    const RwInt32  headIdx   = RpHAnimIDGetIndex(hierarchy, BONE_HEAD);
    const RwV3d&   headPos   = *RwMatrixGetPos(&RpHAnimHierarchyGetMatrixArray(hierarchy)[headIdx]);

    const CVector lookAt{
        headPos.x,
        headPos.y,
        (float)(((double)headPos.z + (double)-0.06f) - 0.5 * (double)t)
    };

    m_vecSource.x += gArrestCamHeadOffset.x;
    m_vecSource.y += gArrestCamHeadOffset.y;
    m_vecSource.z += gArrestCamHeadOffset.z;

    m_vecFront = CVector{
        lookAt.x - m_vecSource.x,
        lookAt.y - m_vecSource.y,
        lookAt.z - m_vecSource.z
    };
    m_vecFront.Normalise();

    m_vecUp    = CVector{ 0.0f, 0.0f, 1.0f };
    auto right = CrossProduct(m_vecFront, m_vecUp);
    right.Normalise();
    m_vecUp = CrossProduct(right, m_vecFront);

    if (checkLineOfSight) {
        if (!CWorld::GetIsLineOfSightClear(m_vecSource, lookAt, true, true, false, true, false, false, true)) {
            return false;
        }
        if (!CWorld::GetIsLineOfSightClear(m_vecSource, lookAt, true, false, false, true, false, false, false)) {
            m_vecSource = oldSource;
        }
    }

    target->SetIsVisible(false);
    return true;
}

namespace {

// 0x515D80 - A `CCam` method in the original (`thiscall`, `ret 10h`), but it never uses `this`, so it's the same as a `stdcall`.
// Finds a position for the camera beside the cop (`cop`), looking at `targetPos` (at least 8 units away from it).
bool __stdcall GetArrestCamPosBesideCop(CEntity* target, CPed* cop, const CVector* targetPos, CVector* outPos) {
    if (!target || !cop) {
        return false;
    }

    const CVector  copPos = cop->GetPosition();
    const CVector& tgt    = *targetPos;

    // Direction from the cop to the target
    CVector dir{ tgt.x - copPos.x, tgt.y - copPos.y, tgt.z - copPos.z };

    // The point 3 units to the side of the cop
    auto right = CrossProduct(dir, CVector{ 0.0f, 0.0f, 1.0f });
    right.Normalise();
    const CVector sidePos{
        (float)(3.0 * (double)right.x + (double)copPos.x),
        (float)(3.0 * (double)right.y + (double)copPos.y),
        (float)((double)(float)(3.0 * (double)right.z) + (double)copPos.z)
    };

    dir.Normalise();
    if (dir.z < -0.7071f) {
        dir.z = -0.7071f;
        const double xyLen = std::sqrt((double)dir.y * (double)dir.y + (double)dir.x * (double)dir.x) * (double)1.4142271f;
        if (xyLen > 0.0) {
            const double invLen = 1.0 / xyLen;
            dir.x = (float)((double)dir.x * invLen);
            dir.y = (float)((double)dir.y * invLen);
        }
        dir.Normalise();
    } else if (dir.z > 0.0f) {
        dir.z = 0.0f;
        dir.Normalise();
    }

    // The point 5 units behind the side point (towards the cop)
    // NOTE: `y` and `z` of the point are not rounded to a float in the original code
    const double fromSideX = 5.0 * (double)dir.x;
    const double fromSideY = 5.0 * (double)dir.y;
    const float  fromSideZ = (float)(5.0 * (double)dir.z);
    const float  pointX    = (float)((double)sidePos.x - fromSideX);
    const double pointY    = (double)sidePos.y - fromSideY;
    const double pointZ    = (double)sidePos.z - (double)fromSideZ;

    // Vector from that point to the target
    CVector toTarget{
        tgt.x - pointX,
        (float)((double)tgt.y - pointY),
        (float)((double)tgt.z - pointZ)
    };

    const double len  = std::sqrt(((double)toTarget.z * (double)toTarget.z + (double)toTarget.y * (double)toTarget.y) + (double)toTarget.x * (double)toTarget.x);
    const float  lenF = (float)len;
    double       zDiff = toTarget.z;
    if (len < 8.0 && lenF > 0.0f) { // Make it at least 8 units away
        const double scale = 8.0 / (double)lenF;
        toTarget.x         = (float)((double)toTarget.x * scale);
        toTarget.y         = (float)((double)toTarget.y * scale);
        zDiff              = scale * (double)toTarget.z;
    }

    outPos->x = tgt.x - toTarget.x;
    outPos->y = tgt.y - toTarget.y;
    outPos->z = (float)((double)tgt.z - zDiff);
    return true;
}

// 0x516010 - A `CCam` method in the original (`thiscall`, `ret 10h`), but it never uses `this`, so it's the same as a `stdcall`.
// Finds a position for the camera behind (relative to the cop `cop`) and to the side of the target, looking at `targetPos`.
bool __stdcall GetArrestCamPosBehindTarget(CEntity* target, CPed* cop, const CVector* targetPos, CVector* outPos) {
    if (!target || !cop) {
        return false;
    }

    const CVector  copPos = cop->GetPosition();
    const CVector& tgt    = *targetPos;

    CVector dir{ tgt.x - copPos.x, tgt.y - copPos.y, 0.0f };
    dir.Normalise();

    // 5 units further from the cop than the target
    // NOTE: The original also calculates `z` here, but overwrites it later
    outPos->x = (float)(5.0 * (double)dir.x + (double)tgt.x);
    outPos->y = (float)(5.0 * (double)dir.y + (double)tgt.y);

    // 10 units to the side (NOTE: Not normalised, but `dir.z` is 0 and `dir` is normalised, so it is unit length anyway)
    const auto side = CrossProduct(dir, CVector{ 0.0f, 0.0f, 1.0f });
    outPos->x       = (float)(10.0 * (double)side.x + (double)outPos->x);
    outPos->y       = (float)(10.0 * (double)side.y + (double)outPos->y);

    outPos->z = tgt.z + 5.0f;

    bool       groundFound{};
    const auto groundZ = CWorld::FindGroundZFor3DCoord(*outPos, &groundFound);
    if (groundFound) {
        outPos->z = (float)((double)0.7f + (double)groundZ);
    }
    return true;
}
} // namespace

// 0x518500
bool CCam::ProcessArrestCamOne() {
    std::array<int32, 6> modes;
    modes.fill(-1);

    bool    found = false;
    CPed*   cop   = nullptr; // The cop arresting the player
    CVector targetPos{};     // The position of the arrested player
    CVector camPos = m_vecSource; // NOTSA: This is not initialised in the original, and might be used by it if no cam mode could be found

    m_fFOV = 45.0f;

    // Position of the arrested player (a bone of him/her if possible), returns the player ped (if any), `false` if the target is not a ped/vehicle
    const auto GetArrestedPos = [&](CPed*& outPed) {
        auto* const target = TheCamera.m_pTargetEntity;
        outPed             = nullptr;
        switch (target->GetType()) {
        case ENTITY_TYPE_PED: {
            outPed = target->AsPed();
            outPed->GetBonePosition(&targetPos, BONE_SPINE1, true);
            return true;
        }
        case ENTITY_TYPE_VEHICLE: {
            auto* const driver = target->AsVehicle()->m_pDriver;
            if (driver && driver->IsPlayer()) {
                outPed = driver;
                driver->GetBonePosition(&targetPos, BONE_SPINE1, true);
            } else {
                targetPos = target->GetPosition();
            }
            return true;
        }
        default:
            return false;
        }
    };
    const auto FindArrestingCop = [] {
        auto* const player = FindPlayerPed();
        return player && player->GetPlayerData()->m_pArrestingCop ? player->GetPlayerData()->m_pArrestingCop : nullptr;
    };
    // Finishes the camera: sets the front and up vectors, looking from the source to the target
    const auto SetupVectors = [&] {
        m_vecFront = CVector{
            targetPos.x - m_vecSource.x,
            targetPos.y - m_vecSource.y,
            targetPos.z - m_vecSource.z
        };
        m_vecFront.Normalise();

        m_vecUp    = CVector{ 0.0f, 0.0f, 1.0f };
        auto right = CrossProduct(m_vecFront, m_vecUp);
        right.Normalise();
        m_vecUp = CrossProduct(right, m_vecFront);
    };
    // Moves the camera source to `camPos`, and the source to a position where the geometry doesn't obstruct the view
    const auto ApplyCamPos = [&] {
        m_vecSource            = camPos;
        const CVector srcCopy = camPos;
        TheCamera.AvoidTheGeometry(&srcCopy, &targetPos, &m_vecSource, m_fFOV);
    };

    if (m_bResetStatics) {
        gArrestCamOneMode = 0;

        CPed* arrestedPlayerPed;
        if (!GetArrestedPos(arrestedPlayerPed)) {
            return false;
        }

        cop = FindArrestingCop();

        // Prefer the cam modes with the cop if there's a cop, and (randomly) the dice says so
        const bool isPed  = TheCamera.m_pTargetEntity->GetType() == ENTITY_TYPE_PED;
        bool       isLucky = false;
        if (cop) {
            const double rnd = (double)CGeneral::GetRandomNumber() * (double)RAND_RECIPROCAL;
            isLucky          = rnd > (isPed ? 0.5 : (double)0.65f);
        }
        if (isPed) {
            modes = isLucky ? std::array<int32, 6>{ 1, 2, 3, 2, 8, -1 } : std::array<int32, 6>{ 1, 3, 2, 8, -1, -1 };
        } else {
            modes = isLucky ? std::array<int32, 6>{ 2, 8, 3, 2, -1, -1 } : std::array<int32, 6>{ 8, 3, 2, -1, -1, -1 };
        }

        if (!CHud::m_BigMessage[STYLE_WHITE_MIDDLE][0]) { // 0xBAADC0, the "BUSTED" message is only added if there's no big message of this style yet
            CMessages::AddBigMessage(TheText.Get("BUSTED"), 5000, STYLE_WHITE_MIDDLE);
        }

        for (size_t i = 0; gArrestCamOneMode == 0 && i < modes.size() && modes[i] > 0; i++) {
            gArrestCamOneCop = nullptr;

            switch (modes[i]) {
            case 1: { // Looking from the cop's head
                gArrestCamOneStartTime = (float)(double)CTimer::GetTimeInMS();
                if (ArrestCamLookAtCopHead(cop, true)) {
                    TheCamera.m_pTargetEntity->SetIsVisible(false);
                    gArrestCamOneMode = 1;
                    m_bResetStatics   = false;
                    return true;
                }
                break;
            }
            case 2:   // Beside the cop
            case 3: { // Behind the player (and later randomly one of the free cams 3..7)
                if (cop) {
                    found = modes[i] == 2
                        ? GetArrestCamPosBesideCop(TheCamera.m_pTargetEntity, cop, &targetPos, &camPos)
                        : GetArrestCamPosBehindTarget(TheCamera.m_pTargetEntity, cop, &targetPos, &camPos);
                    gArrestCamOneCop = cop;
                    cop              = nullptr;
                } else if (arrestedPlayerPed) {
                    // Look for a cop that is arresting the player
                    for (auto* const entity : std::span{ arrestedPlayerPed->GetIntelligence()->GetPedEntities(), 16 }) {
                        if (!entity) {
                            continue;
                        }
                        auto* const nearbyPed = entity->AsPed();
                        const auto  task      = nearbyPed->GetTaskManager().FindActiveTaskByType(TASK_SIMPLE_ARREST_PED);
                        if (!task || FindPlayerPed() != static_cast<CTaskSimpleArrestPed*>(task)->m_Ped) {
                            continue;
                        }
                        found = GetArrestCamPosBesideCop(TheCamera.m_pTargetEntity, nearbyPed, &targetPos, &camPos);
                        if (found) {
                            gArrestCamOneCop = nearbyPed;
                            break;
                        }
                    }
                }
                break;
            }
            case 8: { // Looking from a lamp post
                found = GetLookFromLampPostPos(TheCamera.m_pTargetEntity, cop, targetPos, camPos);
                break;
            }
            }

            if (found) {
                if (gArrestCamOneCop) {
                    gArrestCamOneCop->RegisterReference(&gArrestCamOneCop);
                }
                gArrestCamOneMode = modes[i];
                if (gArrestCamOneMode == 3) { // Pick a random free cam
                    const double rnd  = (double)CGeneral::GetRandomNumber() * (double)RAND_RECIPROCAL * 5.0;
                    const float  rndF = (float)rnd;
                    if (rnd < 1.0) {
                        gArrestCamOneMode = 3;
                    } else if (rndF < 2.0f) {
                        gArrestCamOneMode = 4;
                    } else if (rndF < 3.0f) {
                        gArrestCamOneMode = 5;
                    } else if (rndF < 4.0f) {
                        gArrestCamOneMode = 6;
                    } else {
                        gArrestCamOneMode = 7;
                    }
                }
            }
        }

        ApplyCamPos();
        SetupVectors();
        if (gArrestCamOneMode != 0) {
            m_bResetStatics = false;
        }
        return true;
    }

    if (gArrestCamOneMode == 1) {
        TheCamera.m_pTargetEntity->SetIsVisible(false);
        return ArrestCamLookAtCopHead(FindArrestingCop(), false);
    }

    {
        CPed* unused;
        if (!GetArrestedPos(unused)) {
            return false;
        }
    }

    // `true` => the camera position (`camPos`) is updated and applied, `false` => only the current source is corrected (0x5190D9)
    bool apply = false;

    const auto mode = (int32)gArrestCamOneMode;
    // Moves `camPos` by `dir * 0.1 * timeStep` (NOTE: Parts of this are calculated in extended precision in the original)
    const auto MoveCamPos = [&](const CVector& dir) {
        const float  timeStep = CTimer::GetTimeStep();
        const double moveX    = (double)dir.x * (double)0.1f;
        const double moveY    = (double)dir.y * (double)0.1f;
        const float  moveZ    = (float)((double)dir.z * (double)0.1f);
        camPos.x              = (float)((double)(float)(moveX * (double)timeStep) + (double)camPos.x);
        camPos.y              = (float)(moveY * (double)timeStep + (double)camPos.y);
        camPos.z              = (float)((double)moveZ * (double)timeStep + (double)camPos.z);
    };

    if (mode == 2) { // Beside the cop
        if (gArrestCamOneCop) {
            const bool result = GetArrestCamPosBesideCop(TheCamera.m_pTargetEntity, gArrestCamOneCop->AsPed(), &targetPos, &camPos);

            // Don't let the camera go up too fast
            const double maxZ = (double)CTimer::GetTimeStep() * (double)0.1f + (double)m_vecSource.z;
            if (maxZ < (double)camPos.z) {
                camPos.z = (float)maxZ;
            }
            apply = result;
        }
    } else if (mode > 3 && mode <= 7) { // Free cam, to the left (4, 5) or right (6, 7)
        auto dir = CVector{
            targetPos.x - m_vecSource.x,
            targetPos.y - m_vecSource.y,
            targetPos.z - m_vecSource.z
        };
        camPos     = m_vecSource;
        m_vecFront = dir;
        m_vecFront.Normalise();
        m_vecUp = CVector{ 0.0f, 0.0f, 1.0f };
        dir     = CrossProduct(m_vecFront, m_vecUp);
        dir.Normalise();
        if (mode == 6 || mode == 7) {
            dir.x *= -1.0f;
            dir.y *= -1.0f;
            dir.z *= -1.0f;
        }

        const CVector testPos{
            (float)((double)dir.x * 0.5 + (double)m_vecSource.x),
            (float)((double)dir.y * 0.5 + (double)m_vecSource.y),
            (float)((double)dir.z * 0.5 + (double)m_vecSource.z)
        };
        if (!CWorld::TestSphereAgainstWorld(testPos, 0.4f, TheCamera.m_pTargetEntity, true, true, false, true, false, true)) {
            MoveCamPos(dir);
            if (mode == 5 || mode == 7) { // Move up
                camPos.z = (float)((double)CTimer::GetTimeStep() * (double)0.05f + (double)camPos.z);
            } else { // Stick to the ground
                found              = false;
                const auto groundZ = CWorld::FindGroundZFor3DCoord(camPos, &found);
                if (found) {
                    camPos.z = (float)((double)0.7f + (double)groundZ);
                }
            }
            apply = true;
        }
    } else if (mode == 8) { // From a lamp post
        camPos     = m_vecSource;
        m_vecFront = CVector{
            targetPos.x - camPos.x,
            targetPos.y - camPos.y,
            targetPos.z - camPos.z
        };
        m_vecFront.z = 0.0f;
        m_vecFront.Normalise();
        m_vecUp    = CVector{ 0.0f, 0.0f, 1.0f };
        auto right = CrossProduct(m_vecFront, m_vecUp);
        right.Normalise();

        const double sideX = 10.0 * (double)right.x;
        const double sideY = 10.0 * (double)right.y;
        const float  sideZ = (float)(10.0 * (double)right.z);
        const double diffX = (double)targetPos.x - (double)camPos.x;
        const float  diffY = targetPos.y - camPos.y;
        const float  diffZ = targetPos.z - camPos.z;
        m_vecFront         = CVector{
            (float)(diffX + sideX),
            (float)((double)diffY + sideY),
            (float)((double)diffZ + (double)sideZ)
        };
        m_vecFront.z = 0.0f;
        m_vecFront.Normalise();

        const CVector testPos{
            (float)((double)m_vecFront.x * 0.5 + (double)camPos.x),
            (float)((double)m_vecFront.y * 0.5 + (double)camPos.y),
            (float)((double)m_vecFront.z * 0.5 + (double)camPos.z)
        };
        if (!CWorld::TestSphereAgainstWorld(testPos, 0.4f, TheCamera.m_pTargetEntity, true, true, false, true, false, true)) {
            MoveCamPos(m_vecFront);
            apply = true;
        }
    }

    if (apply) {
        ApplyCamPos();
        SetupVectors();
    } else {
        const CVector srcCopy = m_vecSource;
        TheCamera.AvoidTheGeometry(&srcCopy, &targetPos, &m_vecSource, m_fFOV);
    }
    return true;
}

// 0x519250
void CCam::ProcessPedsDeadBaby() {
    NOTSA_GLOBAL_LOCAL(s_startTime, 0xB70054, (float), {}); // NOTE: write-only as far as this function is concerned
    NOTSA_GLOBAL_LOCAL(s_unused, 0xB70050, (float), {}); // ^

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
        const double orbit      = (double)clamped * 0.001f * x87::sin((double)elapsedTime / 600.0);

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
        const float cosv = (float)x87::cos((double)elapsedTime / 600.0);
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
    const CVector srcCopy{ srcX, srcY, srcZ };
    TheCamera.AvoidTheGeometry(&srcCopy, &targetPos, &m_vecSource, m_fFOV);
    TheCamera.m_bMoveCamToAvoidGeom = false;
}

// 0x50EB70
void CCam::Process_1rstPersonPedOnPC(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    NOTSA_GLOBAL_LOCAL(v3d_8CCC54, 0x8CCC54, (CVector), { 0.06f, 0.05f, 0.0f });
    NOTSA_GLOBAL_LOCAL(byte_B6FFDC, 0xB6FFDC, (bool), {});
    NOTSA_GLOBAL_LOCAL(v3d_B6FFC4, 0xB6FFC4, (CVector), {});
    NOTSA_GLOBAL_LOCAL(v3d_B6FFD0, 0xB6FFD0, (CVector), {});
    NOTSA_GLOBAL_LOCAL(guard_B6FFE0, 0xB6FFE0, (uint32), {}); // MSVC static init guard, nothing else reads it

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
        const int16 lookLR = pad->LookAroundLeftRight(); // LookAroundLeftRight(void)
        const float stickH = (float)-(int32)lookLR;
        const int16 lookUD = pad->LookAroundUpDown(); // LookAroundUpDown(void)
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

    const double cosA = x87::cos((double)m_fVerticalAngle);
    const double cosH = x87::cos((double)m_fHorizontalAngle);
    const double sinH = x87::sin((double)m_fHorizontalAngle);
    const float  sinA3 = (float)x87::sin((double)m_fVerticalAngle) * 3.0f;
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
        const float heading = (float)x87::atan2((double)-m_vecFront.x, (double)m_vecFront.y);
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
    NOTSA_GLOBAL_LOCAL(s_LastWheelieTime, 0x8CCD14, (float), { -1.0f });
    // Making sure player doesn't see below ground when flipped.
    // Name is made up cuz I found it funny to name it like that.
    NOTSA_GLOBAL_LOCAL(s_GroundFaultProtection, 0xB7004C, (float), {});

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

namespace {
//! Settings of the aiming camera, indexed by the "aiming situation": 0 = on foot, 1 = jetpack / on a bike or quad, 2 = in any other vehicle, 3 = melee weapon
struct AimWeaponCamSettings {
    float baseDist;     //< Distance of the camera from the target
    float distScale;    //< Added to the above, scaled by the cosine of the vertical angle
    float angleScale;   //< Scales the vertical angle for the cosine above
    float initialAlpha; //< Vertical angle the camera starts at
    float zOffset;      //< Added to the height of the target
    float alphaMax;     //< Maximum vertical angle
    float alphaMin;     //< Minimum vertical angle (negated)
};
static_assert(sizeof(AimWeaponCamSettings) == 0x1C);

NOTSA_GLOBAL(gAimWeaponCamSettings, 0x8CC4C0, (std::array<AimWeaponCamSettings, 4>), {
    AimWeaponCamSettings{ 1.0f, 1.6f, 1.0f, -0.12f, 0.0f, 0.7853982f, 1.553343f },
    AimWeaponCamSettings{ 3.5f, 0.7f, 1.0f, -0.16f, 0.2f, 0.61086524f, 1.2217305f },
    AimWeaponCamSettings{ 6.0f, 0.7f, 1.0f, -0.16f, 0.4f, 0.61086524f, 1.2217305f },
    AimWeaponCamSettings{ 2.5f, 0.7f, 1.0f, -0.12f, 0.15f, 0.7853982f, 0.79412484f },
});
#line 2522

// Tuning values of the aiming camera (names made up)
NOTSA_GLOBAL(gAimStickScale, 0x8CC4A0, (float), { 0.007f }); // Scale of the stick input (shared with other cameras)
NOTSA_GLOBAL(gAimLockOnTurnRate, 0x8CC4A4, (float), { 0.1f }); // Maximum angle change per time step when locked on to a target
NOTSA_GLOBAL(gAimFreeTurnRate, 0x8CC4A8, (float), { 0.1f }); // Maximum angle change per time step when the camera follows the player's heading
NOTSA_GLOBAL(gAimDriverTurnRateScale, 0x8CC4AC, (float), { 0.25f }); // ^ for drivers
NOTSA_GLOBAL(gAimDriverDeadzone, 0x8CC4B0, (float), { 0.17453294f }); // Angle difference (for drivers) the camera doesn't follow
NOTSA_GLOBAL(gAimFovRifle, 0x8CC4B4, (float), { 50.0f }); // FOV when aiming with an AK-47 / M4
NOTSA_GLOBAL(gAimFovSniper, 0x8CC4B8, (float), { 35.0f }); // FOV when aiming with a country rifle
NOTSA_GLOBAL(gAimHeading, 0x8CC530, (float), { -1001.0f }); // Heading the player is turned to when the camera isn't moved for a while (-1001 = not set)
NOTSA_GLOBAL(gAimIdleTimeMax, 0x8CC534, (int32), { 5000 }); // Above this the camera follows `gAimHeading`
NOTSA_GLOBAL(gAimIdleTimeMin, 0x8CC538, (int32), { 2000 });
NOTSA_GLOBAL(gAimLockOnBlend, 0x8CC39C, (float), { 0.9f }); // Base of the `pow` used to smooth the lock on position (also read by `Process`)
NOTSA_GLOBAL(gAimEnterTargetingDelay, 0x8CCE54, (float), { 500.0f }); // Time (ms) after which the camera is turned around again when "enter targeting" is pressed as a passenger
NOTSA_GLOBAL(gAimStickRateCentered, 0x8CCE58, (float), { 0.5f }); // Base of the `pow` used to smooth the stick input (sticks centered)
NOTSA_GLOBAL(gAimStickRate, 0x8CCE5C, (float), { 0.8f }); // ^ otherwise
NOTSA_GLOBAL(gAimMeleeLockZScale, 0x8CCE60, (float), { 0.75f }); // Scale of the height difference added to the lock on position when using melee weapons
NOTSA_GLOBAL(gbAimLookAtUsesCrossProd, 0x8CCE64, (bool), { true });  // Initially true
NOTSA_GLOBAL(gbAimFreeRotation, 0xB6EC44, (bool), {});  // Whether the camera rotates freely (otherwise it's moved towards `gAimHeading`)
NOTSA_GLOBAL(gAimIdleTime, 0xB6EC48, (int32), {}); // Time (ms) the driver hasn't moved the camera
NOTSA_GLOBAL(gAimLastEnterTargeting, 0xB6EC4C, (uint32), {});

//! `CrossProduct` (0x59C730) - the products stay in the FPU registers (extended precision)
CVector AimWeaponCrossExt(const CVector& a, const CVector& b) {
    return {
        (float)((double)a.y * (double)b.z - (double)a.z * (double)b.y),
        (float)((double)a.z * (double)b.x - (double)a.x * (double)b.z),
        (float)((double)a.x * (double)b.y - (double)a.y * (double)b.x),
    };
}

//! The original limits values to [-1, 1] before calling `asin`/`acos` (NaN is passed through)
float AimWeaponClampUnit(double v) {
    return v < -1.0 ? -1.0f : (1.0 < v ? 1.0f : (float)v);
}
} // namespace

// 0x521500
void CCam::Process_AimWeapon(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    // NOTE: The parameters other than `target` are unused
    // NOTE: x87 extended precision is emulated with `double` in the expressions that stay in the FPU registers in the original
    // NOTE: The values the original uses (0x858CB8, 0x858CBC, 0x858FE4), the ones from `common.h` are slightly different
    constexpr float PI       = std::numbers::pi_v<float>;
    constexpr float TWO_PI   = 2.0f * PI;
    constexpr float HALF_PI  = PI / 2.0f;
    constexpr float DEG2RAD  = 0.0174532924f; // 0x8595EC

    // Statics of the function (the original keeps them at these addresses)
    NOTSA_GLOBAL_LOCAL(s_InitGuard, 0xB70110, (uint32), {}); // MSVC static init guard
    NOTSA_GLOBAL_LOCAL(s_LockOnPos, 0xB70104, (CVector), {}); // Smoothed position of the locked on target
    NOTSA_GLOBAL_LOCAL(s_MeleeAimAlpha, 0xB70100, (float), {});   // Smoothed vertical crosshair angle (in degrees) when using melee weapons (initially 3)
    NOTSA_GLOBAL_LOCAL(s_MeleeAimBeta, 0xB700FC, (float), {});   // Smoothed horizontal crosshair angle (in degrees) when using melee weapons (initially 20)
    NOTSA_GLOBAL_LOCAL(s_LockOnLosTimer, 0xB700F8, (float), {});   // Time until the line of sight to the melee target has to be checked again (+/-100 = clear / blocked)
    NOTSA_GLOBAL_LOCAL(s_LockOnBlend, 0xB700F4, (float), {});   // [0, 1], how much the camera is looking at the (melee) target instead of the player

    if (!(s_InitGuard & 1u)) {
        s_InitGuard |= 1u;
        s_LockOnPos = CVector{ 0.0f, 0.0f, 0.0f };
    }

    if (!m_pCamTargetEntity->GetIsTypePed()) {
        return;
    }
    auto* const ped = m_pCamTargetEntity->AsPed();
    if (!ped->IsPlayer()) {
        return;
    }

    auto* const intel = ped->GetIntelligence();
    auto* const wi    = intel->GetTaskUseGun()
        ? intel->GetTaskUseGun()->m_WeaponInfo
        : CWeaponInfo::GetWeaponInfo(ped->GetActiveWeapon().m_Type, eWeaponSkill::STD);
    const bool bMelee = wi->m_nWeaponFire == eWeaponFire::WEAPON_FIRE_MELEE;

    auto* const veh       = ped->m_pVehicle;
    const bool  bIsDriver = ped->bInVehicle && veh && veh->m_pDriver == ped;
    const bool  bIsPassenger = ped->bInVehicle && veh && veh->m_pDriver != ped;

    int32 settingsIdx = 0;
    if (ped->bInVehicle) {
        settingsIdx = (veh && (veh->m_nVehicleType == VEHICLE_TYPE_BIKE || veh->m_nVehicleSubType == VEHICLE_TYPE_QUAD)) ? 1 : 2;
    } else if (intel->GetTaskJetPack()) {
        settingsIdx = 1;
    } else if (ped->GetActiveWeapon().IsTypeMelee()) {
        settingsIdx = 3;
    }
    const auto& settings = gAimWeaponCamSettings[settingsIdx];

    const eWeaponType weaponType = ped->GetActiveWeapon().m_Type;
    const float       ts         = CTimer::GetTimeStep();

    // FOV
    float fovTarget = 70.0f;
    if (weaponType == WEAPON_AK47 || weaponType == WEAPON_M4) {
        fovTarget = gAimFovRifle;
    } else if (weaponType == WEAPON_COUNTRYRIFLE) {
        fovTarget = gAimFovSniper;
    }
    if (!TheCamera.m_bTransitionState) {
        if (!m_bResetStatics || weaponType == WEAPON_COUNTRYRIFLE) {
            const double step  = (double)ts * 1.0;
            const float  stepF = (float)step;
            const double up    = step + (double)m_fFOV;
            if ((double)fovTarget > up) {
                m_fFOV = (float)up;
            } else {
                const double down = (double)m_fFOV - (double)stepF;
                m_fFOV            = ((double)fovTarget < down) ? (float)down : fovTarget;
            }
        } else {
            m_fFOV = fovTarget;
        }
    }

    // Angles (in radians) the crosshair is away from the center of the screen
    float offsetH; // Horizontal
    float offsetV; // Vertical
    if (bMelee) {
        if (!(s_InitGuard & 2u)) {
            s_InitGuard |= 2u;
            s_MeleeAimAlpha = 3.0f;
        }
        if (!(s_InitGuard & 4u)) {
            s_InitGuard |= 4u;
            s_MeleeAimBeta = 20.0f;
        }

        float wantAlpha = 3.0f;
        float wantBeta  = 20.0f;
        float wantBlend = 0.0f;
        if (intel->GetTaskFighting() && ped->m_nMoveState < PEDMOVE_WALK && ped->m_pTargetedObject) {
            bool bLookAtTarget = false;
            bool bCheckLos     = false;
            if (s_LockOnLosTimer > ts) {
                s_LockOnLosTimer -= ts;
                bLookAtTarget = !(s_LockOnLosTimer < 0.0f);
            } else if (-ts > s_LockOnLosTimer) {
                s_LockOnLosTimer += ts;
                bLookAtTarget = !(s_LockOnLosTimer < 0.0f);
            } else {
                bCheckLos = true;
            }

            if (bCheckLos) {
                // Is there a clear line of sight to the target?
                const CVector pedPos   = ped->GetPosition();
                const CVector tgtPos   = ped->m_pTargetedObject->GetPosition();
                const CVector probeSrc = pedPos + CVector{ 0.0f, 0.0f, 0.75f };

                CVector side      = AimWeaponCrossExt(tgtPos - pedPos, CVector{ 0.0f, 0.0f, 1.0f });
                const float mag   = (float)std::sqrt(SqMagExt(side));
                const float limit = (0.7f > mag) ? 0.7f : mag;
                side *= (float)(2.0 / (double)limit);

                const CVector probeDst = side + probeSrc;
                if (CWorld::GetIsLineOfSightClear(probeSrc, probeDst, true, true, false, true, false, true, true)) {
                    s_LockOnLosTimer = 100.0f;
                    bLookAtTarget    = true;
                } else {
                    s_LockOnLosTimer = -100.0f;
                }
            }

            if (bLookAtTarget) {
                wantAlpha = 0.0f;
                wantBeta  = 70.0f;
                wantBlend = 1.0f;
            }
        }

        if (m_bResetStatics) {
            s_MeleeAimAlpha = wantAlpha;
            s_MeleeAimBeta  = wantBeta;
            s_LockOnBlend   = 0.0f;
        } else if (!TheCamera.m_bTransitionState) {
            const double p = std::pow((double)0.96f, (double)ts);
            const double q = 1.0 - p;
            s_MeleeAimAlpha = (float)((double)wantAlpha * q + (double)s_MeleeAimAlpha * p);
            s_MeleeAimBeta  = (float)((double)wantBeta * q + (double)s_MeleeAimBeta * p);
            s_LockOnBlend   = (float)(q * (double)wantBlend + (double)s_LockOnBlend * p);
        }
        offsetH = s_MeleeAimBeta * DEG2RAD;
        offsetV = s_MeleeAimAlpha * DEG2RAD;
    } else {
        s_LockOnBlend = 0.0f;
        const double tanHalfFov = x87::tan(((double)m_fFOV * (double)0.5f) * (double)DEG2RAD);
        offsetH = (float)x87::atan2((((double)CCamera::m_f3rdPersonCHairMultX - (double)0.5f) * 2.0) * tanHalfFov, 1.0);
        offsetV = (float)x87::atan2(tanHalfFov * (((double)0.5f - (double)CCamera::m_f3rdPersonCHairMultY) * 2.0 * (1.0 / (double)CDraw::ms_fAspectRatio)), 1.0);
    }

    if (m_bResetStatics) {
        TheCamera.ResetDuckingSystem(ped);
        m_bRotating          = false;
        m_bCollisionChecksOn = true;
        m_fAlphaSpeed        = 0.0f;
        m_fBetaSpeed         = 0.0f;
        gbAimFreeRotation    = true;
        gAimIdleTime         = 60000;
        gAimHeading          = -1001.0f;
        gAimLastEnterTargeting = 0;

        if (!CCamera::m_bUseMouse3rdPerson || ped->m_pTargetedObject) {
            m_fVerticalAngle = settings.initialAlpha;
            if (ped->bInVehicle && veh) {
                m_fHorizontalAngle = (float)(((double)ped->m_fCurrentRotation - (double)HALF_PI) - (double)offsetH);
                const float fz     = AimWeaponClampUnit(veh->GetMatrix().GetForward().z);
                m_fVerticalAngle   = (float)(x87::asin((double)fz) + (double)m_fVerticalAngle);
            } else if (!ped->m_pTargetedObject) {
                m_fHorizontalAngle = (float)(((double)ped->m_fCurrentRotation - (double)HALF_PI) + (double)offsetH);
                if (ped->bIsStanding) {
                    const float  c = AimWeaponClampUnit(DotExt(ped->field_578, ped->GetMatrix().GetForward()));
                    const double a = (double)m_fVerticalAngle - x87::asin((double)c);
                    m_fVerticalAngle = (float)a;
                    if (weaponType == WEAPON_EXTINGUISHER) {
                        m_fVerticalAngle = (float)(a + (double)CWeapon::ms_fExtinguisherAimAngle);
                    }
                }
            }
        }
    }

    // Move the heading towards the one set by the scripts
    if (CTheScripts::fCameraHeadingStepWhenPlayerIsAttached > 0.0f) {
        const float step   = CTheScripts::fCameraHeadingStepWhenPlayerIsAttached;
        const float wanted = CTheScripts::fCameraHeadingWhenPlayerIsAttached;
        double      d      = (double)m_fHorizontalAngle - (double)wanted;
        if (d < 0.0) {
            d += (double)TWO_PI;
        }
        const float e = (float)((double)TWO_PI - d);
        if (d < (double)step || e < step) {
            m_fHorizontalAngle                                  = wanted;
            CTheScripts::fCameraHeadingStepWhenPlayerIsAttached = 0.0f;
        } else if (d <= (double)e) {
            m_fHorizontalAngle = (float)((double)m_fHorizontalAngle - (double)step);
        } else {
            m_fHorizontalAngle = (float)((double)step + (double)m_fHorizontalAngle);
        }
    }

    CVector     tgt   = target;
    const float tgtZ0 = target.z;
    ped->UpdateRpHAnim();

    double tgtZExt = ((double)ped->GetPosition().z + (double)0.5f) + (double)settings.zOffset;
    if (m_fFOV < 70.0f) {
        double r = (70.0 - (double)m_fFOV) / (70.0 - (double)gAimFovRifle);
        if (1.0 < r) {
            r = 1.0;
        }
        tgtZExt = tgtZExt + r * (double)0.1f;
    }
    tgt.z = (float)tgtZExt;
    const float zDelta = (float)(tgtZExt - (double)tgtZ0);

    // How far the target is moved to the right
    float lookAtScale = 0.2f;
    if (!wi->flags.bAimWithArm && ped->GetPlayerData()->m_pPedClothesDesc->HasVisibleNewHairCut(1)) {
        lookAtScale = 0.3f;
    } else if (m_fFOV < 70.0f) {
        double r = (70.0 - (double)m_fFOV) / (70.0 - (double)gAimFovSniper);
        if (1.0 < r) {
            r = 1.0;
        }
        lookAtScale = (float)(r * (double)0.1f + (double)0.2f);
    }

    if (gbAimLookAtUsesCrossProd) {
        const CVector cross = AimWeaponCrossExt(m_vecFront, m_vecUp);
        const auto&   right = ped->GetMatrix().GetRight();
        const float   d     = (float)(((double)cross.y * (double)right.y + (double)cross.z * (double)right.z) + (double)cross.x * (double)right.x);
        const float   c     = (1.0f < d) ? 1.0f : ((d < 0.0f) ? 0.0f : d);
        const double  f     = (1.0 - x87::acos((double)c) * (double)0.63661975f) * (double)lookAtScale;
        const double  cx    = (double)cross.x * f;
        const float   cy    = (float)((double)cross.y * f);
        const float   cz    = (float)((double)cross.z * f);
        tgt.x               = (float)(cx + (double)tgt.x);
        tgt.y               = (float)((double)tgt.y + (double)cy);
        tgt.z               = (float)((double)cz + (double)tgt.z);
    } else {
        const auto&  right = ped->GetMatrix().GetRight();
        const double sx    = (double)lookAtScale * (double)right.x;
        const double sy    = (double)lookAtScale * (double)right.y;
        const float  sz    = (float)((double)lookAtScale * (double)right.z);
        tgt.x              = (float)(sx + (double)tgt.x);
        tgt.y              = (float)(sy + (double)tgt.y);
        tgt.z              = (float)((double)sz + (double)tgt.z);
    }

    // Position of the locked on target
    if (auto* const locked = ped->m_pTargetedObject) {
        CVector lockPos{ 0.0f, 0.0f, 0.0f };
        if (locked->GetIsTypePed() && !bMelee) {
            locked->AsPed()->GetBonePosition(&lockPos, BONE_SPINE1, true);
        } else {
            lockPos = locked->GetPosition();
        }
        if (bMelee) {
            lockPos.z = (float)((double)zDelta * (double)gAimMeleeLockZScale + (double)lockPos.z);
        }
        if (!m_bResetStatics && intel->GetTaskFighting()) {
            const float p = (float)std::pow((double)gAimLockOnBlend, (double)ts);
            const float q = (float)(1.0 - (double)p);
            s_LockOnPos   = p * s_LockOnPos + q * lockPos;
        } else {
            s_LockOnPos = lockPos;
        }
    }

    if (ped->m_pTargetedObject) {
        // Locked on to a target
        const double dx = (double)s_LockOnPos.x - (double)tgt.x;
        const double dy = (double)s_LockOnPos.y - (double)tgt.y;
        const float  dz = (float)((double)s_LockOnPos.z - (double)tgt.z);

        const float yawToTarget = (float)(x87::atan2(-dx, dy) - (double)HALF_PI);
        const float hSq         = (float)(dx * dx + dy * dy);
        float       pitchToTarget = (float)x87::atan2((double)dz, std::sqrt((double)hSq));

        if (bMelee) {
            pitchToTarget = (float)(x87::cos((double)offsetH) * (double)pitchToTarget);
        } else {
            // Move the target so the crosshair ends up on it
            const double ex  = (double)tgt.x - (double)m_vecSource.x;
            const double ey  = (double)tgt.y - (double)m_vecSource.y;
            const double ez  = (double)tgt.z - (double)m_vecSource.z;
            const double len = std::sqrt((ez * ez + ey * ey) + ex * ex);

            double dist;
            if (len < (double)settings.baseDist) {
                dist = std::sqrt(SqMagExt(tgt - m_vecSource));
            } else {
                dist = (double)settings.baseDist;
            }
            const double R     = std::sqrt((double)dz * (double)dz + (double)hSq);
            const double ratio = (dist + R) / R;
            offsetV            = (float)((double)offsetV * ratio);
            offsetH            = (float)(ratio * (double)offsetH);
        }

        double yaw   = (double)yawToTarget + (double)offsetH;
        double pitch = (double)pitchToTarget - (double)offsetV;
        if (pitch < -(double)PI) {
            pitch += (double)TWO_PI;
        } else if (pitch > (double)PI) {
            pitch -= (double)TWO_PI;
        }

        float maxStep = ts * gAimLockOnTurnRate;
        if (m_bResetStatics) {
            maxStep = 1000.0f;
        }

        // Vertical
        {
            const double delta  = pitch - (double)m_fVerticalAngle;
            const float  deltaF = (float)delta;
            if (std::abs(delta) < (double)maxStep) {
                m_fVerticalAngle = (float)pitch;
            } else if (!(deltaF < 0.0f)) {
                m_fVerticalAngle = (float)((double)maxStep + (double)m_fVerticalAngle);
            } else {
                m_fVerticalAngle = (float)((double)m_fVerticalAngle - (double)maxStep);
            }
        }

        // Horizontal
        {
            const double yd = yaw - (double)m_fHorizontalAngle;
            if (yd > (double)PI) {
                yaw -= (double)TWO_PI;
            } else if (yd < -(double)PI) {
                yaw += (double)TWO_PI;
            }
            const double delta  = yaw - (double)m_fHorizontalAngle;
            const float  deltaF = (float)delta;
            if (std::abs(delta) < (double)maxStep) {
                m_fHorizontalAngle = (float)yaw;
            } else if (!(deltaF < 0.0f)) {
                m_fHorizontalAngle = (float)((double)maxStep + (double)m_fHorizontalAngle);
            } else {
                m_fHorizontalAngle = (float)((double)m_fHorizontalAngle - (double)maxStep);
            }
        }
        m_fAlphaSpeed = 0.0f;
        m_fBetaSpeed  = 0.0f;
    } else {
        auto* const  pad    = CPad::GetPad(0);
        const float  mouseX = CPad::NewMouseControllerState.m_AmountMoved.x;
        const float  mouseY = CPad::NewMouseControllerState.m_AmountMoved.y;
        if (CCamera::m_bUseMouse3rdPerson && pad->DisablePlayerControls == 0 && (mouseX != 0.0f || mouseY != 0.0f)) {
            // Mouse
            const double f      = (double)m_fFOV * (double)0.0125f;
            const double yChain = (((double)mouseY * (double)4.0f) * f) * (double)CCamera::m_fMouseAccelVertical;
            const double xChain = (((double)mouseX * (double)-2.5f) * f) * (double)CCamera::m_fMouseAccelHorzntl;
            m_fBetaSpeed        = 0.0f;
            m_fAlphaSpeed       = 0.0f;
            m_fHorizontalAngle  = (float)(xChain + (double)m_fHorizontalAngle);
            m_fVerticalAngle    = (float)(yChain + (double)m_fVerticalAngle);
        } else {
            // Pad
            const float  lr = (float)(-(int32)pad->AimWeaponLeftRight(ped));
            const float  ud = (float)(int32)pad->AimWeaponUpDown(ped);
            const double f  = (double)m_fFOV * (double)0.0125f;

            const double sc = (double)gAimStickScale;
            const float  stickH = (float)(((((((double)0.0714285746f * f) * (double)std::abs(lr)) * (double)lr) * (double)ts) * sc) * sc);
            const float  stickV = (float)((((((f * (double)0.042857144f) * (double)std::abs(ud)) * (double)ud) * (double)ts) * sc) * sc);

            float stickRate = gAimStickRate;
            if (std::abs(lr) < 2.0f && std::abs(ud) < 2.0f) {
                stickRate = gAimStickRateCentered;
            }
            const float pf = (float)std::pow((double)stickRate, (double)ts);
            const float qf = (float)(1.0 - (double)pf);
            m_fBetaSpeed   = (float)((double)pf * (double)m_fBetaSpeed + (double)qf * (double)stickH);
            m_fAlphaSpeed  = (float)((double)qf * (double)stickV + (double)pf * (double)m_fAlphaSpeed);

            float dBeta  = m_fBetaSpeed;
            float dAlpha = m_fAlphaSpeed;

            // Whether the camera is moved towards `gAimHeading` (otherwise it rotates freely)
            bool bFollowHeading;
            if (bIsPassenger) {
                if (pad->GetEnterTargeting()) {
                    const uint32 now = CTimer::GetTimeInMS();
                    if ((double)(now - gAimLastEnterTargeting) < (double)gAimEnterTargetingDelay) {
                        dBeta  = PI;
                        dAlpha = 0.0f;
                    } else {
                        gAimLastEnterTargeting = now;
                    }
                }
                bFollowHeading = !gbAimFreeRotation;
            } else if (bIsDriver) {
                if (lr == 0.0f && ud == 0.0f) {
                    if (!pad->GetWeapon(ped)) {
                        gAimIdleTime += (int32)(((double)ts * (double)0.02f) * (double)1000.0f);
                    }
                } else {
                    gAimIdleTime = 0;
                }

                if (gAimIdleTime > gAimIdleTimeMax) {
                    gbAimFreeRotation = false;
                    gAimHeading       = (float)(((double)ped->m_fCurrentRotation - (double)HALF_PI) + (double)offsetH);
                    bFollowHeading    = true;
                } else if (gAimIdleTime > gAimIdleTimeMin) {
                    const double base = (double)ped->m_fCurrentRotation - (double)HALF_PI;
                    double       diff = (base - (double)offsetH) - (double)m_fHorizontalAngle;
                    if (diff > (double)TWO_PI) {
                        diff -= (double)TWO_PI;
                    } else if (diff < -(double)TWO_PI) {
                        diff += (double)TWO_PI;
                    }
                    if (diff < (double)0.5235988f) {
                        gAimHeading       = (float)(base + (double)offsetH);
                        gAimIdleTime      = gAimIdleTimeMax + 1;
                        gbAimFreeRotation = false;
                        bFollowHeading    = true;
                    } else {
                        gbAimFreeRotation = true;
                        bFollowHeading    = false;
                    }
                } else {
                    gbAimFreeRotation = true;
                    if (pad->GetWeapon(ped)) {
                        gAimIdleTime = 0;
                    }
                    bFollowHeading = false;
                }
            } else {
                bFollowHeading = !gbAimFreeRotation;
            }

            if (!bFollowHeading) {
                gAimHeading        = -1001.0f;
                m_fHorizontalAngle = (float)((double)dBeta + (double)m_fHorizontalAngle);
                m_fVerticalAngle   = (float)((double)dAlpha + (double)m_fVerticalAngle);
            } else {
                float heading = (float)((double)ped->m_fCurrentRotation - (double)HALF_PI);
                if (gAimHeading < -1000.0f) {
                    gAimHeading = heading;
                } else {
                    heading = gAimHeading;
                }

                if (!wi->flags.bAimWithArm && !bMelee) {
                    const float rot         = (float)((double)gAimHeading + (double)HALF_PI);
                    ped->m_fAimingRotation  = rot;
                    ped->m_fCurrentRotation = rot;
                    ped->SetHeading(rot);
                    ped->UpdateRwMatrix();
                }

                double beta     = (double)heading - (double)offsetH;
                float  rate     = ts * gAimFreeTurnRate;
                float  deadzone = 0.0f;
                if (bIsDriver) {
                    rate     = (float)((double)rate * (double)gAimDriverTurnRateScale);
                    deadzone = gAimDriverDeadzone;
                }

                {
                    const double u = beta - (double)m_fHorizontalAngle;
                    if (u > (double)PI) {
                        beta -= (double)TWO_PI;
                    } else if (u < -(double)PI) {
                        beta += (double)TWO_PI;
                    }
                }
                double diff = beta - (double)m_fHorizontalAngle;
                if (deadzone > 0.0f) {
                    if (diff > (double)deadzone) {
                        diff -= (double)deadzone;
                    } else if (diff < -(double)deadzone) {
                        diff += (double)deadzone;
                    } else {
                        diff = 0.0;
                    }
                }
                if (std::abs(diff) < (double)rate) {
                    gbAimFreeRotation  = true;
                    m_fHorizontalAngle = (float)(diff + (double)m_fHorizontalAngle);
                } else if (!(diff < 0.0)) {
                    m_fHorizontalAngle = (float)((double)rate + (double)m_fHorizontalAngle);
                } else {
                    m_fHorizontalAngle = (float)((double)m_fHorizontalAngle - (double)rate);
                }

                if (!bIsDriver) {
                    m_fVerticalAngle = (float)((double)dAlpha + (double)m_fVerticalAngle);
                } else {
                    const float fz = AimWeaponClampUnit(veh->GetMatrix().GetForward().z);
                    double      alpha = x87::asin((double)fz) + (double)settings.initialAlpha;
                    {
                        const double u = alpha - (double)m_fVerticalAngle;
                        if (u > (double)PI) {
                            alpha -= (double)TWO_PI;
                        } else if (u < -(double)PI) {
                            alpha += (double)TWO_PI;
                        }
                    }
                    double diffA = alpha - (double)m_fVerticalAngle;
                    if (diffA > (double)deadzone) {
                        diffA -= (double)deadzone;
                    } else if (diffA < -(double)deadzone) {
                        diffA += (double)deadzone;
                    } else {
                        diffA = 0.0;
                    }
                    if (std::abs(diffA) < (double)rate) {
                        m_fVerticalAngle = (float)(diffA + (double)m_fVerticalAngle);
                    } else if (!(diffA < 0.0)) {
                        m_fVerticalAngle = (float)((double)rate + (double)m_fVerticalAngle);
                    } else {
                        m_fVerticalAngle = (float)((double)m_fVerticalAngle - (double)rate);
                    }
                }
            }
        }
    }

    ClipBeta();
    if (m_fVerticalAngle > settings.alphaMax) {
        m_fVerticalAngle = settings.alphaMax;
    } else if (m_fVerticalAngle < -settings.alphaMin) {
        m_fVerticalAngle = -settings.alphaMin;
    }

    // Camera position
    double cosTerm;
    if (m_fVerticalAngle <= 0.0f) {
        cosTerm = x87::cos((double)m_fVerticalAngle);
    } else {
        double w = (double)settings.angleScale * (double)m_fVerticalAngle;
        if ((double)HALF_PI < w) {
            w = (double)HALF_PI;
        }
        cosTerm = x87::cos(w);
    }
    const double dist = (double)settings.baseDist + cosTerm * (double)settings.distScale;

    {
        const double cosV = x87::cos((double)m_fVerticalAngle);
        m_vecFront.x      = (float)-(x87::cos((double)m_fHorizontalAngle) * cosV);
        m_vecFront.y      = (float)-(x87::sin((double)m_fHorizontalAngle) * cosV);
        m_vecFront.z      = (float)x87::sin((double)m_fVerticalAngle);
    }
    {
        const double dfx = dist * (double)m_vecFront.x;
        const float  dfy = (float)(dist * (double)m_vecFront.y);
        const float  dfz = (float)(dist * (double)m_vecFront.z);
        m_vecSource.x    = (float)((double)tgt.x - dfx);
        m_vecSource.y    = (float)((double)tgt.y - (double)dfy);
        m_vecSource.z    = (float)((double)tgt.z - (double)dfz);
    }

    TheCamera.HandleCameraMotionForDuckingDuringAim(ped, &m_vecSource, &tgt, false);
    m_vecTargetCoorsForFudgeInter = tgt;
    CCamera::SetColVarsAimWeapon(settingsIdx);
    if (gCameraDirection == 3) {
        TheCamera.CameraGenericModeSpecialCases(ped);
        TheCamera.CameraPedAimModeSpecialCases(ped);
        TheCamera.CameraColDetAndReact(&m_vecSource, &tgt);
        TheCamera.ImproveNearClip(nullptr, ped, &m_vecSource, &tgt);
    }
    TheCamera.m_bCamDirectlyBehind  = false;
    TheCamera.m_bCamDirectlyInFront = false;

    // Look at the (melee) target a bit
    if (s_LockOnBlend > 0.0f && ped->m_pTargetedObject) {
        const double w   = (double)s_LockOnBlend * (double)0.5f;
        const float  ax  = (float)((double)s_LockOnPos.x * w);
        const float  ay  = (float)((double)s_LockOnPos.y * w);
        const double wz  = w * (double)s_LockOnPos.z;
        const double q   = 1.0 - w;
        const double tqx = (double)tgt.x * q;
        const float  by  = (float)((double)tgt.y * q);
        const float  cz  = (float)(q * (double)tgt.z);
        const float  ex  = (float)(tqx + (double)ax);
        const float  ey  = (float)((double)by + (double)ay);
        const double ez  = (double)cz + wz;
        m_vecFront.x     = (float)((double)ex - (double)m_vecSource.x);
        m_vecFront.y     = (float)((double)ey - (double)m_vecSource.y);
        m_vecFront.z     = (float)(ez - (double)m_vecSource.z);
        m_vecFront.Normalise();
    }

    GetVectorsReadyForRW();

    // Turn the player towards where the camera is looking at
    if ((!wi->flags.bAimWithArm || ped->bIsDucking) && !bMelee && !ped->bInVehicle) {
        const auto FrontHeading = [&] {
            return (float)(x87::atan2(-(double)m_vecFront.x, (double)m_vecFront.y) - (double)offsetH);
        };

        bool  bHasHeading = true;
        float heading{};
        if (weaponType == WEAPON_SPRAYCAN) {
            heading = FrontHeading();
        } else if (auto* const locked = ped->m_pTargetedObject) {
            const CVector diff = locked->GetPosition() - ped->GetPosition();
            heading            = (float)x87::atan2(-(double)diff.x, (double)diff.y);
        } else if (gbAimFreeRotation) {
            heading = FrontHeading();
        } else {
            bHasHeading = false;
        }

        if (bHasHeading && heading > -100.0f) {
            const float rot         = (float)((double)heading + (double)-0.05f);
            ped->m_fCurrentRotation = rot;
            ped->m_fAimingRotation  = rot;
            ped->SetHeading(heading);
            ped->UpdateRwMatrix();
        }

        TheCamera.m_pTargetEntity->AsPed()->GetPlayerData()->m_fLookPitch = TheCamera.Find3rdPersonQuickAimPitch();
    }

    m_bResetStatics = false;
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

    const double s = x87::sin((double)angle);
    const double c = x87::cos((double)angle);
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
            const double rawAngle = x87::atan2((double)-vel.x, (double)vel.y) - (double)(PI / 2.0f); // NOTE: x87 keeps this unrounded for the next subtraction
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
    m_fHorizontalAngle = (float)(x87::atan2((double)-(vehPos.x - m_vecSource.x), (double)(vehPos.y - m_vecSource.y)) - (double)(PI / 2.0f));

    float clampedDist = camDist;
    if (camDist < dist2D || (clampedDist = minDist, dist2D < minDist)) {
        m_vecSource.x = dx * (clampedDist / dist2D) + vehPos.x;
        m_vecSource.y = (clampedDist / dist2D) * dy + vehPos.y;
    }

    const auto& vel = veh->m_vecMoveSpeed;
    if (0.0001f < vel.z * vel.z + vel.y * vel.y + vel.x * vel.x) {
        const double velPitch = x87::atan2((double)vel.z, std::sqrt((double)vel.y * vel.y + (double)vel.x * vel.x));
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
    m_vecSource.z = (float)((double)vehPos.z - x87::sin((double)m_fVerticalAngle) * camDist);

    RotCamIfInFrontCar(vehPos, baseAngle);

    m_vecTargetCoorsForFudgeInter = vehPos;
    const CVector srcCopy{ m_vecSource };
    TheCamera.AvoidTheGeometry(&srcCopy, &m_vecTargetCoorsForFudgeInter, &m_vecSource, m_fFOV);

    {
        const double sinb = x87::sin((double)rotExcess);
        const double cosb = (double)(float)x87::cos((double)rotExcess); // NOTE: the original rounds the cosine to float
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
            const float  tanT   = (float)x87::tan(tExt);
            const auto   upTerm = m_vecUp * (float)((x87::tan((double)t) / (double)CDraw::ms_fAspectRatio) * (double)m_fY_Targetting);
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
        const double ang  = (double)(float)x87::atan2((double)-toAim.x, (double)toAim.y);
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
        task.FireGun(shooter); // 0x627CC0
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
    const float  s     = (float)x87::sin(angle);
    const float  c     = (float)((x87::cos(angle) + 1.0) * 0.5);

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

    if (const auto s = CTimer::GetTimeStep() * ExeRecip(200.0f); camHeightMult >= s) {
        camHeightMult = (camHeightMult >= 0.0f ? s : -s) + m_fCameraHeightMultiplier;
    }
    m_fCameraHeightMultiplier = camHeightMult;

    const auto v21 = std::max(p1p2Distance + 10.0f, 30.0f);
    m_vecSource.Set(
        p1p2Centroid.x,
        p1p2Centroid.y - v21 * x87::sin(0.4f),
        p1p2Centroid.z - v21 * -x87::cos(0.4f)
    );
    m_vecFront.Set(0.0f, x87::sin(0.4f), -x87::cos(0.4f));
    m_vecUp.Set(0.0f, m_vecSource.y, x87::sin(0.4f));
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
        const double s = x87::sin((270.0 - (double)t * 180.0) * (double)0.017453292f);
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
    float fov = (float)((15.0 - 70.0) * (x87::sin((270.0 - (double)k * 180.0) * (double)0.017453292f) + 1.0) * 0.5 + 70.0);
    if (t < 0.1f) {
        auto k2 = std::clamp(t / 0.1f, 0.0f, 1.0f);
        fov     = (float)(((double)fov - 70.0) * (x87::sin((270.0 - (double)k2 * 180.0) * (double)0.017453292f) + 1.0) * 0.5 + 70.0);
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
        const double s = x87::sin((270.0 - (double)(1.0f / S.fovBlendInFraction) * (double)t * 180.0) * (double)0.017453292f);
        fovBase        = ((double)S.fovEnd - (double)S.fovStart) * (s + 1.0) * 0.5 + (double)S.fovStart;
    }
    double fovZoom = 0.0;
    const float dist3D = std::sqrt(sq(src.x - dest.x) + sq(src.y - dest.y) + sq(cur.z - dest.z));
    if (S.fovZoomDistMin < dist3D) {
        double k = ((double)dist3D - S.fovZoomDistMin) / ((double)S.fovZoomDistMax - S.fovZoomDistMin);
        k        = std::clamp(k, 0.0, 1.0);
        const double s = x87::sin((270.0 - k * 180.0) * (double)0.017453292f);
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
        const double s = x87::sin((270.0 - f * 180.0) * (double)0.017453292f);
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
        const double sinv = x87::sin((double)4 * ((double)elapsed / (double)duration) * 360.0 * (double)0.017453292f);
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
        const double sinv = x87::sin((double)4 * ((double)elapsed / (double)duration) * 360.0 * (double)0.017453292f);
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
            auto s = x87::sin((270.0f - k * 180.0f) * 0.0174532924f);
            fov    = (12.0f - 70.0f) * (s + 1.0f) * 0.5f + 70.0f;
            if (t < 0.1f) {
                auto k2 = std::clamp(t / 0.1f, 0.0f, 1.0f);
                auto s2 = x87::sin((270.0f - k2 * 180.0f) * 0.0174532924f);
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
    NOTSA_GLOBAL_LOCAL(s_LookAtAngle, 0xB6FFE4, (float), {});
    NOTSA_GLOBAL_LOCAL(s_DoRenderShadows, 0xB7295A, (bool), {});

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
    m_fHorizontalAngle += pad->GetLeftStickX() * _90DEG_PER_HOUR_IN_RAD_PER_MIN * ExeRecip(19.0f);
    m_fVerticalAngle   += DegreesToRadians(static_cast<float>(pad->GetLeftStickY())) * ExeRecip(50.0f);

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
    m_fFOV = TheCamera.m_bUseSpecialFovTrain ? TheCamera.m_fFovForTrain : 70.0f; // 0x51D5B5 (byte at 0xB6F05E == 1 -> the float at 0xB6F158)

    if (float wl{}; CWaterLevel::GetWaterLevel(m_vecSource, wl, true) && m_vecSource.z < wl) {
        ApplyUnderwaterMotionBlur();
    }

    if (gAllowScriptedFixedCameraCollision) {
        const auto savedIgnoreEntity = CWorld::pIgnoreEntity;

        CWorld::pIgnoreEntity = FindPlayerVehicle();
        CVector out{};
        float   outDist{1.0f};
        if (TheCamera.ConeCastCollisionResolve(m_vecSource, target, out, 2.0f, 0.1f, outDist)) {
            m_vecSource = out; // 0x51D73E: all three components
        }

        CWorld::pIgnoreEntity = savedIgnoreEntity;
    }
}

namespace {
//! Minimal duration (ms) of a fly-by spline segment: shorter ones are skipped while searching the segment (a global at 0x8D0F80, name made up)
NOTSA_GLOBAL(gFlyByMinSegmentTimeMs, 0x8D0F80, (float), { 32.0f });

// Fly-by state (names made up)
NOTSA_GLOBAL(gFlyByFov, 0xBC4074, (float), {}); // Result of the FOV spline
NOTSA_GLOBAL(gFlyByFovCopy, 0xBC4078, (float), {}); // NOTE: Only written (together with `gFlyByFov`) when the fly-by starts, never read
NOTSA_GLOBAL(gFlyByLookSegmentIdx, 0xBC407C, (int32), {}); // Current position in the spline `CCamera::m_aPathArray[3]` (where the camera looks at)
NOTSA_GLOBAL(gFlyBySrcSegmentIdx, 0xBC4080, (int32), {}); // Current position in the spline `CCamera::m_aPathArray[2]` (camera position)
NOTSA_GLOBAL(gFlyByFovSegmentIdx, 0xBC4084, (int32), {}); // Current position in the spline `CCamera::m_aPathArray[0]` (FOV)
NOTSA_GLOBAL(gFlyByRollSegmentIdx, 0xBC4088, (int32), {}); // Current position in the spline `CCamera::m_aPathArray[1]` (roll)

//! Evaluates a Bezier spline of 3D points at the time `time` (ms), 0x5B2090 (cdecl)
//! @param out  Result (the cutscene offset is added to it)
//! @param data `[0]` = number of entries, then (starting at `[1]`) 10 floats per entry: time (s), point (3), incoming control point (3), outgoing control point (3)
//! @param idx  In/out: index (into `data`) of the entry that ends the current segment
void FlyBySplineVec3(CVector* out, float* data, float time, int32* idx) {
    const auto count = static_cast<int32>(data[0]);

    // BUG: The duration is NOT recalculated after `idx` is changed below, so it is the one of the old segment (the original does the same)
    const double duration = (static_cast<double>(data[*idx]) - data[*idx - 10]) * 1000.0;
    const float  lastTime = data[count * 10 - 9] * 1000.0f;

    if (time < lastTime) {
        bool canAdvance = true;
        if (static_cast<uint32>(*idx - 1) / 10u > static_cast<uint32>(count)) {
            *idx       = count * 10 - 9;
            canAdvance = false;
        }
        if (duration <= gFlyByMinSegmentTimeMs && canAdvance) { // Skip the (too) short segment
            *idx += 10;
            if (static_cast<uint32>(*idx - 1) / 10u > static_cast<uint32>(count)) {
                *idx = count * 10 - 9;
            }
        }
    }

    const int32 i = *idx;

    double t = (static_cast<double>(time) - static_cast<double>(data[i - 10]) * 1000.0) / duration;
    if (t > 1.0) {
        t = 1.0;
    } else if (t < 0.0) {
        t = 0.0;
    }
    if (time > lastTime) {
        t = 1.0;
    }

    const CVector p0{ data[i - 9], data[i - 8], data[i - 7] }; // Point of the previous entry
    const CVector c0{ data[i - 3], data[i - 2], data[i - 1] }; // Outgoing control point of the previous entry
    const CVector p1{ data[i + 1], data[i + 2], data[i + 3] }; // Point of this entry
    const CVector c1{ data[i + 4], data[i + 5], data[i + 6] }; // Incoming control point of this entry

    if (c0.x != p0.x || c0.y != p0.y || c0.z != p0.z) {
        // Cubic Bezier: the intermediate results are stored as floats where the original does so
        const double u   = 1.0 - t;
        const double t2  = t * t;
        const double u2  = u * u;
        const float  t3f = static_cast<float>(t2 * t);
        const float  u2f = static_cast<float>(u2);
        const float  u3f = static_cast<float>(u2 * u);
        const float  utf = static_cast<float>(u * t2);
        const double tu2 = t * u2f;

        out->x = static_cast<float>(((utf * static_cast<double>(c1.x) + tu2 * c0.x) * 3.0 + t3f * static_cast<double>(p1.x)) + u3f * static_cast<double>(p0.x));
        out->y = static_cast<float>(((utf * static_cast<double>(c1.y) + tu2 * c0.y) * 3.0 + t3f * static_cast<double>(p1.y)) + u3f * static_cast<double>(p0.y));
        out->z = static_cast<float>(((utf * static_cast<double>(c1.z) + tu2 * c0.z) * 3.0 + t3f * static_cast<double>(p1.z)) + u3f * static_cast<double>(p0.z));
    } else {
        // Straight line
        out->x = static_cast<float>((static_cast<double>(p1.x) - p0.x) * t + p0.x);
        out->y = static_cast<float>((static_cast<double>(p1.y) - p0.y) * t + p0.y);
        out->z = static_cast<float>((static_cast<double>(p1.z) - p0.z) * t + p0.z);
    }

    *out += TheCamera.m_vecCutSceneOffset;
}

//! Same as `FlyBySplineVec3`, but for a spline of floats (4 floats per entry: time (s), value, incoming control value, outgoing control value), 0x5B2330 (cdecl)
//! The cutscene offset is not added.
void FlyBySplineFloat(float* out, float* data, float time, int32* idx) {
    const auto count = static_cast<int32>(data[0]);

    const double duration = (static_cast<double>(data[*idx]) - data[*idx - 4]) * 1000.0;
    const float  lastTime = data[count * 4 - 3] * 1000.0f;

    if (time < lastTime) {
        bool canAdvance = true;
        if (static_cast<uint32>(*idx - 1) / 4u > static_cast<uint32>(count)) {
            *idx       = count * 4 - 3;
            canAdvance = false;
        }
        if (duration <= gFlyByMinSegmentTimeMs && canAdvance) { // Skip the (too) short segment
            *idx += 4;
            if (static_cast<uint32>(*idx - 1) / 4u > static_cast<uint32>(count)) {
                *idx = count * 4 - 3;
            }
        }
    }

    const int32 i = *idx;

    // NOTE: Unlike `FlyBySplineVec3`, the duration is recalculated for the (possibly changed) `idx`
    double t = (static_cast<double>(time) - static_cast<double>(data[i - 4]) * 1000.0)
             / ((static_cast<double>(data[i]) - data[i - 4]) * 1000.0);
    if (t > 1.0) {
        t = 1.0;
    } else if (t < 0.0) {
        t = 0.0;
    }
    if (time > lastTime) {
        t = 1.0;
    }

    const float p0 = data[i - 3]; // Value of the previous entry
    const float c0 = data[i - 1]; // Outgoing control value of the previous entry
    const float p1 = data[i + 1]; // Value of this entry
    const float c1 = data[i + 2]; // Incoming control value of this entry

    if (c0 != p0) {
        // Cubic Bezier (everything is kept in extended precision in the original)
        const double u   = 1.0 - t;
        const double u2  = u * u;
        const double u3  = u2 * u;
        const double acc = (p1 * t + (c1 * u) * 3.0) * t;
        *out = static_cast<float>((acc + (c0 * u2) * 3.0) * t + u3 * p0);
    } else {
        // Straight line
        *out = static_cast<float>((static_cast<double>(p1) - p0) * t + p0);
    }
}
} // namespace

// 0x5B25F0
void CCam::Process_FlyBy(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    if (TheCamera.m_bCutsceneFinished) {
        return;
    }

    float roll = 0.0f;

    m_vecUp = CVector{ 0.0f, 0.0f, 1.0f };

    auto* const fovPath  = TheCamera.m_aPathArray[0].m_pArrPathData;
    auto* const rollPath = TheCamera.m_aPathArray[1].m_pArrPathData;
    auto* const srcPath  = TheCamera.m_aPathArray[2].m_pArrPathData;
    auto* const lookPath = TheCamera.m_aPathArray[3].m_pArrPathData;

    if (!TheCamera.m_bStartingSpline) {
        m_fTimeElapsedFloat = 0.0f;
        m_nFinishTime       = static_cast<uint32>(static_cast<int32>(static_cast<double>(srcPath[static_cast<int32>(srcPath[0]) * 10 - 9]) * 1000.0));

        gFlyByRollSegmentIdx = 5;
        gFlyByFovSegmentIdx  = 5;
        TheCamera.m_bStartingSpline = true;
        gFlyBySrcSegmentIdx  = 11;
        gFlyByLookSegmentIdx = 11;
        gFlyByFov = gFlyByFovCopy = fovPath[2];
    } else {
        m_fTimeElapsedFloat = static_cast<float>(static_cast<double>(CTimer::GetTimeStepNonClipped()) * 0.02f * 1000.0f + m_fTimeElapsedFloat);
    }

    const float time = static_cast<float>(static_cast<uint32>(static_cast<int32>(m_fTimeElapsedFloat)));

    // The up vector is rotated around the front vector by the roll (in degrees)
    const auto ApplyRoll = [&] {
        constexpr float DEG_TO_RAD = static_cast<float>(std::numbers::pi / 180.0);
        constexpr float HALF_PI    = std::numbers::pi_v<float> / 2.0f;
        const double angle = static_cast<double>(roll) * DEG_TO_RAD + HALF_PI;
        m_vecUp.x = static_cast<float>(x87::cos(angle));
        m_vecUp.z = static_cast<float>(x87::sin(angle));
    };

    // Moves `idx` to the segment that contains `time` (the segments of the spline with `stride` floats per entry)
    const auto FindSegment = [&](const float* data, int32& idx, int32 stride) {
        while ((static_cast<double>(data[idx]) - data[1]) * 1000.0 <= time) {
            idx += stride;
        }
    };

    if (static_cast<double>(m_nFinishTime) <= time) { // Finished => stay at the end of the splines
        gFlyBySrcSegmentIdx  = (static_cast<int32>(srcPath[0]) - 1) * 10 + 1;
        gFlyByLookSegmentIdx = (static_cast<int32>(lookPath[0]) - 1) * 10 + 1;
        gFlyByRollSegmentIdx = static_cast<int32>(rollPath[0]) * 4 - 3;
        gFlyByFovSegmentIdx  = static_cast<int32>(fovPath[0]) * 4 - 3;

        FlyBySplineVec3(&m_vecSource, srcPath, time, &gFlyBySrcSegmentIdx);
        FlyBySplineVec3(&m_vecFront, lookPath, time, &gFlyByLookSegmentIdx);
        FlyBySplineFloat(&roll, rollPath, time, &gFlyByRollSegmentIdx);
        ApplyRoll();
        FlyBySplineFloat(&gFlyByFov, fovPath, time, &gFlyByFovSegmentIdx);

        TheCamera.m_fPositionAlongSpline = 1.0f;
        gFlyBySrcSegmentIdx  = 0;
        gFlyByLookSegmentIdx = 0;
        gFlyByRollSegmentIdx = 0;
        gFlyByFovSegmentIdx  = 0;
    } else {
        TheCamera.m_fPositionAlongSpline = static_cast<float>(time / static_cast<double>(m_nFinishTime));

        FindSegment(srcPath, gFlyBySrcSegmentIdx, 10);
        FlyBySplineVec3(&m_vecSource, srcPath, time, &gFlyBySrcSegmentIdx);

        FindSegment(lookPath, gFlyByLookSegmentIdx, 10);
        FlyBySplineVec3(&m_vecFront, lookPath, time, &gFlyByLookSegmentIdx);

        FindSegment(rollPath, gFlyByRollSegmentIdx, 4);
        FlyBySplineFloat(&roll, rollPath, time, &gFlyByRollSegmentIdx);
        ApplyRoll();

        FindSegment(fovPath, gFlyByFovSegmentIdx, 4);
        FlyBySplineFloat(&gFlyByFov, fovPath, time, &gFlyByFovSegmentIdx);
    }

    // `m_vecFront` holds the look-at point until here
    m_vecTargetCoorsForFudgeInter = m_vecFront;
    m_vecFront                    = m_vecFront - m_vecSource;
    m_vecFront.Normalise();

    const CVector right = CrossProduct(m_vecUp, m_vecFront);
    m_vecUp             = CrossProduct(m_vecFront, right);
    m_vecUp.Normalise();

    m_fFOV = gFlyByFov;
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

NOTSA_GLOBAL(gFollowCarCamSettings, 0x8CC600, (std::array<FollowCarCamSettings, 7>), {
    FollowCarCamSettings{ 1.3f, 1.0f, 0.4f, 10.0f, 15.0f, 0.5f, 1.0f, 1.0f, 0.85f, 0.2f, 0.075f, 0.05f, 0.8f, 0.7853982f, 1.553343f },
    FollowCarCamSettings{ 1.1f, 1.0f, 0.1f, 10.0f, 11.0f, 0.5f, 1.0f, 1.0f, 0.85f, 0.2f, 0.075f, 0.05f, 0.75f, 0.7853982f, 1.553343f },
    FollowCarCamSettings{ 1.1f, 1.0f, 0.2f, 10.0f, 15.0f, 0.05f, 0.05f, 0.0f, 0.9f, 0.05f, 0.01f, 0.05f, 1.0f, 0.17453294f, 1.2217305f },
    FollowCarCamSettings{ 1.1f, 3.5f, 0.2f, 10.0f, 25.0f, 0.5f, 1.0f, 1.0f, 0.75f, 0.1f, 0.005f, 0.2f, 1.0f, 1.553343f, 1.553343f },
    FollowCarCamSettings{ 1.3f, 1.0f, 0.4f, 10.0f, 15.0f, 0.5f, 1.0f, 0.0f, 0.9f, 0.05f, 0.005f, 0.05f, 1.0f, 0.34906587f, 1.2217305f },
    FollowCarCamSettings{ 1.1f, 1.0f, 0.2f, 10.0f, 5.0f, 0.5f, 1.0f, 1.0f, 0.75f, 0.1f, 0.005f, 0.2f, 1.0f, 0.7853982f, 1.553343f },
    FollowCarCamSettings{ 1.1f, 1.0f, 0.2f, 10.0f, 5.0f, 0.5f, 1.0f, 1.0f, 0.75f, 0.1f, 0.005f, 0.2f, 1.0f, 0.34906587f, 1.2217305f },
});
#line 5118
NOTSA_GLOBAL(gFollowCarZoomAngle, 0x8CC41C, (std::array<std::array<float, 5>, 3>), { {
    { 0.08f, 0.08f, 0.15f, 0.08f, 0.08f }, { 0.07f, 0.08f, 0.3f, 0.08f, 0.08f }, { 0.055f, 0.05f, 0.15f, 0.06f, 0.08f }
} }); // [zoom][arrPos], see `CCamera::GetArrPosForVehicleType`
#line 5119

NOTSA_GLOBAL(gFollowCarTrailerBlend, 0xB7011C, (float), {}); // [0, 1]: Blend factor between the vehicle (alone) and the vehicle + trailer/passenger (names made up)
NOTSA_GLOBAL(gFollowCarMouseTimer, 0xB70118, (float), {}); // Set to 50 when the mouse is moved, counts down while it's idle
NOTSA_GLOBAL(gbFollowCarAlphaReset, 0xB70114, (bool), {});  // Set once the vertical angle has been reset for a (special) vehicle
NOTSA_GLOBAL(gFollowCarPrevAlpha, 0x8CCEB0, (float), { -9999.0f }); // Initially -9999
NOTSA_GLOBAL(gFollowCarPrevBeta, 0x8CCEA8, (float), { -9999.0f }); // Initially -9999
NOTSA_GLOBAL(gbCamUnk_9655E5, 0x9655E5, (bool), {});  // Set to true when the followed vehicle is "big". Never read anywhere in the exe, purpose unknown

// 0x420800 (see `RopeMax` in Rope.cpp)
float FollowCarMax(float a, float b) {
    return a > b ? a : b; // NOTE: NaN => `b`
}

// Index into `gFollowCarCamSettings` (the "camera vehicle type") of the vehicle: 0 = car (also the Vortex), 1 = bike/quad, 2 = heli (also the Hydra with the nozzles down),
// 3 = plane, 4 = boat, 5 = RC Bandit/Baron/Tiger/Cam, 6 = RC Raider/Goblin
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

    float dist = TheCamera.m_fCarZoomSmoothed + cfg.minDistOffset; // The distance the camera wants to keep from the vehicle

    int32 arrPos = 0;
    TheCamera.GetArrPosForVehicleType(static_cast<eVehicleType>(veh->GetVehicleAppearance()), arrPos);

    // Angle the camera is looking down by
    float elevAngle = 0.0f;
    if (veh->GetStatus() == STATUS_REMOTE_CONTROLLED) {
        elevAngle = gFollowCarZoomAngle[1][arrPos];
    } else if (TheCamera.m_nCarZoom == 1) {
        elevAngle = gFollowCarZoomAngle[0][arrPos];
    } else if (TheCamera.m_nCarZoom == 2) {
        elevAngle = gFollowCarZoomAngle[1][arrPos];
    } else if (TheCamera.m_nCarZoom == 3) {
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

        // NOTE: Everything here uses the *trailer's* collision model (the original never touches the vehicle's again)
        const auto&  trailerBB = trailer->GetColModel()->GetBoundingBox();
        const double dx        = (double)trailerBB.m_vecMax.x - (double)trailerBB.m_vecMin.x;
        const double dy        = (double)trailerBB.m_vecMax.y - (double)trailerBB.m_vecMin.y;
        const double dz        = (double)trailerBB.m_vecMax.z - (double)trailerBB.m_vecMin.z;
        const double len       = std::sqrt(dz * dz + dy * dy + dx * dx);
        colSize                = (float)(len * 0.5 * (double)blend + (double)colSize);

        const float trailerMaxZ = trailerBB.m_vecMax.z;
        const float maxZ        = heightZ > trailerMaxZ ? heightZ : trailerMaxZ;
        heightZ                 = (float)(((double)maxZ - (double)heightZ) * (double)blend + (double)heightZ);

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
        if (TheCamera.m_nCarZoom == 1 && (camType == 0 || camType == 1)) {
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
        m_bResetStatics             = false;
        m_bRotating                 = false;
        m_bCollisionChecksOn        = true;
        TheCamera.m_bResetOldMatrix = true; // 0xB6F999

        if (!TheCamera.m_bJustCameOutOfGarage && !bFlag) {
            m_fVerticalAngle = 0.0f;
            const double heading = veh->m_matrix
                ? x87::atan2(-(double)veh->m_matrix->GetForward().x, (double)veh->m_matrix->GetForward().y)
                : (double)veh->m_placement.m_fHeading;
            m_fHorizontalAngle = (float)(heading - (double)HALF_PI);
            if (TheCamera.m_bCamDirectlyInFront) {
                m_fHorizontalAngle = (float)((heading - (double)HALF_PI) + (double)PI);
            }
        }

        const double cosA = x87::cos((double)m_fVerticalAngle);
        m_fBetaSpeed      = 0.0f;
        m_fAlphaSpeed     = 0.0f;
        m_fDistance       = 1000.0f;
        m_vecFront.x      = (float)-(x87::cos((double)m_fHorizontalAngle) * cosA);
        m_vecFront.y      = (float)-(x87::sin((double)m_fHorizontalAngle) * cosA);
        m_vecFront.z      = (float)x87::sin((double)m_fVerticalAngle);

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
    double behindBeta = x87::atan2(-(double)m_vecFront.x, (double)m_vecFront.y) - (double)HALF_PI;
    if (behindBeta < (double)-PI) {
        behindBeta += (double)(2.0f * PI);
    }

    const auto& moveSpeed = veh->m_vecMoveSpeed;

    // Horizontal angle of the vehicle's movement
    double moveBeta = behindBeta;
    if (std::sqrt((double)moveSpeed.x * (double)moveSpeed.x + (double)moveSpeed.y * (double)moveSpeed.y) > 0.02f) {
        moveBeta = x87::atan2(-(double)moveSpeed.x, (double)moveSpeed.y) - (double)HALF_PI;
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
    float alphaTarget = (float)x87::asin((double)std::clamp(m_vecFront.z, -1.0f, 1.0f));

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
            const double phi      = x87::asin(std::fabs(x87::sin(relBeta1)));
            const double thr      = x87::atan2((double)bb.m_vecMax.x, -(double)bb.m_vecMin.y);
            double       d1;
            if (phi <= thr) {
                d1 = (1.5f - (double)bb.m_vecMin.y) / x87::cos(phi);
            } else {
                const float v   = (float)(1.2f + (double)bb.m_vecMax.x);
                const float arg = (float)((double)HALF_PI - phi);
                d1              = (double)v / x87::cos((double)FollowCarMax(0.0f, arg));
            }
            const float d1f = (float)d1;

            const double relBeta2 = (double)m_fHorizontalAngle - ((double)veh->GetHeading() - (double)HALF_PI);
            const double pitch    = x87::atan2((double)mat.GetForward().z, std::sqrt((double)mat.GetForward().x * (double)mat.GetForward().x + (double)mat.GetForward().y * (double)mat.GetForward().y));
            const double p1       = pitch * x87::cos(relBeta2);
            maxAlpha              = (float)(x87::atan2((double)zRel, (double)d1f * 1.2f) + p1);

            if (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE
                && veh->AsAutomobile()->m_nNumContactWheels > 1
                && std::fabs(DotProduct(veh->m_vecTurnSpeed, mat.GetForward())) < 0.05f) {
                const double relBeta3 = ((double)m_fHorizontalAngle - ((double)veh->GetHeading() - (double)HALF_PI)) + (double)HALF_PI;
                const double pitchR   = x87::atan2((double)mat.GetRight().z, std::sqrt((double)mat.GetRight().y * (double)mat.GetRight().y + (double)mat.GetRight().x * (double)mat.GetRight().x));
                maxAlpha              = (float)(pitchR * x87::cos(relBeta3) + (double)maxAlpha);
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
        m_fHorizontalAngle = (float)(CGeneral::GetATanOfXYExt(m_vecFront.x, m_vecFront.y) + (double)PI);
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
        const double cosA = x87::cos((double)m_fVerticalAngle);
        m_vecFront.x      = (float)-(x87::cos((double)m_fHorizontalAngle) * cosA);
        m_vecFront.y      = (float)-(x87::sin((double)m_fHorizontalAngle) * cosA);
        m_vecFront.z      = (float)x87::sin((double)m_fVerticalAngle);
    }

    // NOTE: This rounds the *previous* source, it's overwritten below
    LimitPrecision(m_vecSource);
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
        const float  cosA = (float)x87::cos((double)alphaTarget);
        const double ux   = -(x87::cos((double)m_fHorizontalAngle) * (double)cosA);
        const double uy   = -(x87::sin((double)m_fHorizontalAngle) * (double)cosA);
        const double sinA = x87::sin((double)alphaTarget);
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

    CCamera::SetColVarsVehicle(static_cast<eVehicleType>(camType), static_cast<int32>(TheCamera.m_nCarZoom));

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
        LimitPrecision(m_vecSource);
    }

    TheCamera.m_bCamDirectlyBehind  = false;
    TheCamera.m_bCamDirectlyInFront = false;
    LimitPrecision(m_vecSource);
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
            const int16 lookLR = pad->LookAroundLeftRight();
            const int16 lookUD = pad->LookAroundUpDown();
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
            angle = x87::atan2((double)toCam.y, (double)toCam.x);
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
        cosAlpha = x87::cos((double)m_fVerticalAngle);
    } else {
        double v = 3.0 * (double)m_fVerticalAngle;
        if ((double)(PI / 2.0f) < v) {
            v = (double)(PI / 2.0f);
        }
        cosAlpha = x87::cos(v);
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

    const double cosA = x87::cos((double)m_fVerticalAngle);
    m_vecFront.x      = (float)-(x87::cos((double)m_fHorizontalAngle) * cosA);
    m_vecFront.y      = (float)-(x87::sin((double)m_fHorizontalAngle) * cosA);
    m_vecFront.z      = (float)x87::sin((double)m_fVerticalAngle);
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
        return (float)(x87::tan((double)m_fFOV * 0.0174532924f * 0.5f) * (double)CDraw::ms_fAspectRatio * (double)nearClip * 1.1f);
    };

    int32 iter = 0;
    float nearClipNow = RwCameraGetNearClipPlane(Scene.m_pRwCamera);
    const float sphereFactor = (float)(CDraw::ms_fAspectRatio * x87::tan((double)m_fFOV * 0.0174532924f * 0.5f) * 1.1f);
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
        const float heading = (float)x87::atan2((double)-m_vecFront.x, (double)m_vecFront.y);
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
                m_fVerticalAngle = (float)-x87::asin((double)c);
            }
        }

        {
            const double cosV = x87::cos((double)m_fVerticalAngle);
            m_vecFront.x      = (float)-(x87::cos((double)m_fHorizontalAngle) * cosV);
            m_vecFront.y      = (float)-(x87::sin((double)m_fHorizontalAngle) * cosV);
            m_vecFront.z      = (float)x87::sin((double)m_fVerticalAngle);
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
        const double h = x87::atan2((double)-m_vecFront.x, (double)m_vecFront.y) - (double)HALF_PI;
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
        alphaCur       = (float)x87::asin((double)c);
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
            pitchTarget    = -x87::asin((double)c);
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
        static_cast<CTaskComplexEnterCar*>(activeTask)->GetCameraStickModifier(ped, zoomDist, m_fVerticalAngle, m_fHorizontalAngle, stickUD, stickLR); // 0x63A380
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
        const double cosV = x87::cos((double)m_fVerticalAngle);
        m_vecFront.x      = (float)-(x87::cos((double)m_fHorizontalAngle) * cosV);
        m_vecFront.y      = (float)-(x87::sin((double)m_fHorizontalAngle) * cosV);
        m_vecFront.z      = (float)x87::sin((double)m_fVerticalAngle);
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
        const float  cosA     = (float)x87::cos(alphaExt);
        const double t1Ext    = -(x87::cos((double)m_fHorizontalAngle) * (double)cosA);
        const float  t1       = (float)t1Ext;
        const double t2Ext    = -(x87::sin((double)m_fHorizontalAngle) * (double)cosA);
        const double sinAExt  = x87::sin((double)alphaCur);
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

namespace {
// Camera bump tuning (names made up), see `DoCamBump`
NOTSA_GLOBAL(gM16CamBumpPeriod, 0x8CC474, (int32), { 800 }); // 800
NOTSA_GLOBAL(gM16CamBumpDuration, 0x8CC478, (int32), { 600 }); // 600
NOTSA_GLOBAL(gM16CamBumpDecay, 0x8CC47C, (float), { 0.95f }); // 0.95
NOTSA_GLOBAL(gM16CamBumpScale, 0x8CC480, (float), { 0.1f }); // 0.1

// Used by `Process_M16_1stPerson` when the ped is crouching (names made up)
NOTSA_GLOBAL(gM16CrouchBackOffset, 0x8CC7BC, (float), { 0.5f }); // 0.5
NOTSA_GLOBAL(gM16CrouchSideOffset, 0x8CC7B8, (float), { 0.18f }); // 0.18

// Pitch limit of `Process_M16_1stPerson` and the stick damping factors (names made up)
NOTSA_GLOBAL(gM16MaxPitch, 0x8CCC90, (float), { 1.2f }); // 1.2
NOTSA_GLOBAL(gM16StickDampSlow, 0x8CCC94, (float), { 0.5f }); // 0.5 (both sticks almost centered)
NOTSA_GLOBAL(gM16StickDampNormal, 0x8CCC98, (float), { 0.8f }); // 0.8

NOTSA_GLOBAL(gM16TargetFov, 0xB6FFE8, (float), {});  // The FOV the zoom is blended towards (names made up)
NOTSA_GLOBAL(gM16Unused_B6FFEC, 0xB6FFEC, (uint32), {}); // Only ever reset by `Process_M16_1stPerson`
NOTSA_GLOBAL(gM16Unused_B6FFF0, 0xB6FFF0, (uint32), {}); // ^
NOTSA_GLOBAL(gbM16CamObstructed, 0xB6FFF4, (bool), {});   // Set when the (collision checked) camera is too close to the geometry, in that case the near clip isn't touched

// Heading of the entity the way `CPlaceable::GetHeading` (0x441DB0) leaves it in the FPU (extended precision)
double M16HeadingExt(const CPlaceable& placeable) {
    if (const auto* const mat = placeable.m_matrix) {
        const auto& fwd = mat->GetForward();
        return x87::atan2((double)-fwd.x, (double)fwd.y);
    }
    return (double)placeable.m_placement.m_fHeading;
}

// dst += dir * c - the x/y sums stay in the FPU registers, the z product is rounded to a float first
void M16AddScaled(CVector& dst, float c, const CVector& dir) {
    dst.x = (float)((double)c * dir.x + dst.x);
    dst.y = (float)((double)c * dir.y + dst.y);
    dst.z = (float)((double)c * dir.z) + dst.z;
}

// dst -= dir * c - ^
void M16SubScaled(CVector& dst, float c, const CVector& dir) {
    dst.x = (float)(dst.x - (double)c * dir.x);
    dst.y = (float)(dst.y - (double)c * dir.y);
    dst.z = dst.z - (float)((double)c * dir.z);
}

// asin of a value clamped to [-1, 1] (stored as a float)
float M16ClampedAsin(float v) {
    return (float)x87::asin((double)std::clamp(v, -1.0f, 1.0f));
}
}

// 0x5105C0
void CCam::Process_M16_1stPerson(const CVector& target, float orientation, float speedVar, float speedVarWanted) {
    // NOTE: `target`, `orientation`, `speedVar` and `speedVarWanted` aren't used by the original (it copies `target` to a local, but only overwrites it)
    constexpr float kPi     = std::numbers::pi_v<float>;
    constexpr float kHalfPi = kPi / 2.0f;
    constexpr float kTwoPi  = 2.0f * kPi;

    if (!m_pCamTargetEntity->GetIsTypePed()) {
        return;
    }
    auto* const targetPed = m_pCamTargetEntity->AsPed();

    float maxPitch = 1.04719758f;  // local_38
    float minPitch = 1.49225652f;  // local_34 (as a magnitude)
    float upOfs    = 0.1f;         // local_48 - offset of the camera along the up vector of the ped
    float backOfs  = 0.3f;         // local_44 - offset of the camera backwards (along the forward vector of the ped)

    // Player that is attached to something (turret, vehicle mounted gun, ...)
    const bool isPlayerAttached = targetPed->IsPlayer() && targetPed->m_pAttachedTo;

    if (m_bResetStatics) {
        if (CCamera::m_bUseMouse3rdPerson && !targetPed->m_pTargetedObject) {
            if (isPlayerAttached) {
                m_fHorizontalAngle = CTheScripts::fCameraHeadingWhenPlayerIsAttached;
                m_fVerticalAngle   = 0.0f;
            }
        } else {
            if (isPlayerAttached) {
                m_fHorizontalAngle = CTheScripts::fCameraHeadingWhenPlayerIsAttached;
            } else {
                m_fHorizontalAngle = targetPed->m_fCurrentRotation - kHalfPi;
            }
            m_fVerticalAngle = 0.0f;
        }

        m_bResetStatics             = false;
        gbM16CamObstructed          = false;
        gM16Unused_B6FFF0           = 0;
        m_fInitialPlayerOrientation = targetPed->m_fCurrentRotation - kHalfPi;
        gM16Unused_B6FFEC           = 0;
        m_bCollisionChecksOn        = true;
        m_fFOVSpeed                 = 0.0f;
        gM16TargetFov               = m_fFOV;
        m_fAlphaSpeed               = 0.0f;
        m_fBetaSpeed                = 0.0f;
    }

    if (m_nMode == MODE_SNIPER || m_nMode == MODE_CAMERA) {
        // Zooming
        bool wheelZoomed = false;

        const auto zoomInBtn  = ControlsManager.GetMouseButtonAssociatedWithAction(eControllerAction::PED_SNIPER_ZOOM_IN);
        const auto zoomOutBtn = ControlsManager.GetMouseButtonAssociatedWithAction(eControllerAction::PED_SNIPER_ZOOM_OUT);
        const float wheel     = CPad::NewMouseControllerState.m_fWheelMoved;

        if ((wheel > 0.0f && zoomOutBtn == 4) || (wheel < 0.0f && zoomOutBtn == 5)) {
            gM16TargetFov = (float)((((double)std::abs(wheel) * 7.0f + 10000.0f) * gM16TargetFov) * 0.0001f);
            wheelZoomed   = true;
        } else if ((wheel > 0.0f && zoomInBtn == 4) || (wheel < 0.0f && zoomInBtn == 5)) {
            gM16TargetFov = (float)((double)gM16TargetFov / (((double)std::abs(wheel) * 7.0f + 10000.0f) * 0.0001f));
            wheelZoomed   = true;
        }

        auto* const pad = CPad::GetPad(0);
        if (pad->SniperZoomOut() && !wheelZoomed) {
            m_fFOV        = (float)((((double)CTimer::GetTimeStep() * 255.0f + 10000.0f) * m_fFOV) * 0.0001f);
            gM16TargetFov = m_fFOV;
            m_fFOVSpeed   = 0.0f;
        } else if (pad->SniperZoomIn() && !wheelZoomed) {
            m_fFOV        = (float)((double)m_fFOV / (((double)CTimer::GetTimeStep() * 255.0f + 10000.0f) * 0.0001f));
            gM16TargetFov = m_fFOV;
            m_fFOVSpeed   = 0.0f;
        } else if (!(std::abs(gM16TargetFov - m_fFOV) > 0.5f)) { // NOTE: written this way so a NaN takes this branch like in the original
            m_fFOVSpeed = 0.0f;
        } else {
            WellBufferMe(gM16TargetFov, m_fFOV, m_fFOVSpeed, 0.5f, 0.25f, false);
        }

        if (m_fFOV > 70.0f) {
            m_fFOV = 70.0f;
        }
        if (gM16TargetFov > 70.0f) {
            gM16TargetFov = 70.0f;
        } else if (m_nMode == MODE_CAMERA) {
            if (m_fFOV < 3.0f) {
                m_fFOV = 3.0f;
            }
            if (gM16TargetFov < 3.0f) {
                gM16TargetFov = 3.0f;
            }
        } else {
            if (m_fFOV < 15.0f) {
                m_fFOV = 15.0f;
            }
            if (gM16TargetFov < 15.0f) {
                gM16TargetFov = 15.0f;
            }
        }

        TheCamera.SetMotionBlur(180, 255, 180, 120, eMotionBlurType::SNIPER);
    } else {
        m_fFOV = 70.0f;
    }

    // Attached player: the camera is steered towards the wanted heading
    if (isPlayerAttached && 0.0f < CTheScripts::fCameraHeadingStepWhenPlayerIsAttached) {
        auto&        step = CTheScripts::fCameraHeadingStepWhenPlayerIsAttached;
        double       d    = (double)m_fHorizontalAngle - CTheScripts::fCameraHeadingWhenPlayerIsAttached;
        if (d < 0.0) {
            d += kTwoPi;
        }
        const float e = (float)((double)kTwoPi - d);
        if (d < step || e < step) {
            m_fHorizontalAngle = CTheScripts::fCameraHeadingWhenPlayerIsAttached;
            step               = 0.0f;
        } else if (!(d > (double)e)) {
            m_fHorizontalAngle = m_fHorizontalAngle - step;
        } else {
            m_fHorizontalAngle = step + m_fHorizontalAngle;
        }
    }

    // Look around (mouse / sticks)
    {
        const auto  mouse = CPad::NewMouseControllerState.GetAmountMouseMoved();
        const float ts    = CTimer::GetTimeStep();
        double      deltaH, deltaV; // NOTE: x87 extended precision in the original
        if (mouse.x == 0.0f && mouse.y == 0.0f) {
            auto* const pad    = CPad::GetPad(0);
            const float stickH = (float)-(int32)pad->LookAroundLeftRight(targetPed);
            const float stickV = (float)(int32)pad->LookAroundUpDown(targetPed);

            float locH, locV;
            if (isPlayerAttached) {
                const double h = (double)stickH * 0.0078125f;
                const double v = (double)stickV * 0.0078125f;
                const double f = (double)m_fFOV * 0.0125f;
                locH           = (float)(std::abs(h) * (f * 0.04f) * ts * h);
                locV           = (float)(std::abs(v) * (f * 0.034285713f) * ts * v);
            } else {
                const float signH = stickH < 0.0f ? -1.0f : 1.0f;
                const float signV = stickV < 0.0f ? -1.0f : 1.0f;
                const double f    = (double)m_fFOV * 0.0125f;
                locH              = (float)((((double)stickH * stickH * 0.0001f) * (f * 0.0571428575f)) * ts * signH);
                locV              = (float)((((double)stickV * stickV * 4.44444449e-05f) * (f * 0.0714285746f)) * ts * signV);
            }

            float damp = gM16StickDampNormal;
            if (std::abs(stickH) < 2.0f && std::abs(stickV) < 2.0f) {
                damp = gM16StickDampSlow;
            }
            damp = (float)std::pow((double)damp, (double)ts);

            const float oneMinusDamp = (float)(1.0 - (double)damp);
            const double betaSpeed   = (1.0 - (double)damp) * locH + (double)damp * m_fBetaSpeed;
            m_fBetaSpeed             = (float)betaSpeed;
            const float alphaSpeed   = (float)((double)damp * m_fAlphaSpeed + (double)oneMinusDamp * locV);
            m_fAlphaSpeed            = alphaSpeed;

            deltaH = betaSpeed;
            deltaV = alphaSpeed;
        } else {
            const float mouseX = mouse.x * -3.0f;
            float       mouseY = mouse.y * 3.0f;
            float       hSrc   = mouseX;
            auto* const pad    = CPad::GetPad(0);
            if (pad->DisablePlayerControls != 0 || pad->JustOutOfFrontEnd != 0 || ts <= 0.0f) {
                mouseY = 0.0f;
                hSrc   = 0.0f;
            }
            const double f  = (double)m_fFOV * 0.0125f;
            const float  fF = (float)f;
            deltaH          = (double)hSrc * (f * CCamera::m_fMouseAccelHorzntl);
            deltaV          = ((double)fF * CCamera::m_fMouseAccelVertical) * mouseY;
            m_fBetaSpeed    = 0.0f;
            m_fAlphaSpeed   = 0.0f;
        }
        m_fHorizontalAngle = (float)(deltaH + m_fHorizontalAngle);
        m_fVerticalAngle   = (float)(deltaV + m_fVerticalAngle);
    }
    ClipBeta();

    // Camera bump
    if ((int32)m_nCamBumpedTime > 0) {
        const uint32 bumpTime = m_nCamBumpedTime;
        const double c        = x87::cos((double)(uint32)(CTimer::GetTimeInMS() - bumpTime) / (double)gM16CamBumpPeriod * (double)kTwoPi);
        m_fHorizontalAngle    = (float)((double)gM16CamBumpScale * c * m_fCamBumpedHorz + m_fHorizontalAngle);
        m_fVerticalAngle      = (float)(c * m_fCamBumpedVert * (double)gM16CamBumpScale + m_fVerticalAngle);
        m_fCamBumpedHorz      = (float)(std::pow((double)gM16CamBumpDecay, (double)CTimer::GetTimeStep()) * m_fCamBumpedHorz);
        m_fCamBumpedVert      = (float)(std::pow((double)gM16CamBumpDecay, (double)CTimer::GetTimeStep()) * m_fCamBumpedVert);
        if (CTimer::GetTimeInMS() > bumpTime + (uint32)gM16CamBumpDuration) {
            m_nCamBumpedTime = 0;
        }
    }

    if (targetPed->bIsDucking) {
        backOfs = 0.8f;
    }

    CVector bonePos{};
    if (isPlayerAttached) {
        auto* const attached = targetPed->m_pAttachedTo;

        // Pitch and heading of the entity the ped is attached to, as seen from the ped's `m_fTurretAngleA` position (0 = default)
        float pitchTarget;   // local_40
        float headingTarget; // local_20
        switch (targetPed->m_fTurretAngleA) {
        case 1:
            pitchTarget   = M16ClampedAsin(-attached->GetMatrix().GetRight().z);
            headingTarget = (float)M16HeadingExt(*attached);
            break;
        case 2:
            pitchTarget   = M16ClampedAsin(-attached->GetMatrix().GetForward().z);
            headingTarget = (float)(M16HeadingExt(*attached) + (double)kHalfPi);
            break;
        case 3:
            pitchTarget   = M16ClampedAsin(attached->GetMatrix().GetRight().z);
            headingTarget = (float)(M16HeadingExt(*attached) - (double)kPi);
            break;
        default:
            pitchTarget   = M16ClampedAsin(attached->GetMatrix().GetForward().z);
            headingTarget = (float)(M16HeadingExt(*attached) - (double)kHalfPi);
            break;
        }

        targetPed->PositionAttachedPed();
        targetPed->UpdateRwMatrix();
        targetPed->UpdateRwFrame();
        targetPed->UpdateRpHAnim();

        if (attached->GetIsTypeVehicle() && attached->AsVehicle()->m_nVehicleType == VEHICLE_TYPE_BIKE) {
            upOfs   = 0.0f;
            backOfs = 0.0f;
            targetPed->GetBonePosition(&bonePos, BONE_HEAD, true);
            maxPitch = targetPed->m_nTurretPosnMode;
            minPitch = maxPitch;
        } else {
            targetPed->GetBonePosition(&bonePos, BONE_HEAD, true);
        }
        m_vecSource = bonePos;

        M16AddScaled(m_vecSource, upOfs, targetPed->GetMatrix().GetUp());
        M16SubScaled(m_vecSource, backOfs, targetPed->GetMatrix().GetForward());

        // Limit how far the camera heading is allowed to go from the heading of the attached entity
        const float  timeFactor = CTimer::GetTimeStep() * 0.05f;
        const double headMargin = 0.75f * (double)targetPed->m_fTurretAngleB;
        const float  headInner  = (float)((1.0f - 0.75f) * (double)targetPed->m_fTurretAngleB);

        double headTarget = headingTarget;
        if (headTarget - m_fHorizontalAngle > (double)kPi) {
            headTarget -= (double)kTwoPi;
        } else if (headTarget - m_fHorizontalAngle < (double)-kPi) {
            headTarget += (double)kTwoPi;
        }

        CAutomobile* heli = nullptr;
        if (attached->GetIsTypeVehicle() && attached->AsVehicle()->m_nVehicleSubType == VEHICLE_TYPE_HELI) {
            heli = attached->AsAutomobile();
        }

        const double headDiff = headTarget - m_fHorizontalAngle;
        double       headOver = 0.0;
        bool         headOut  = true;
        if (headDiff > headMargin) {
            headOver = headDiff - headMargin;
        } else if (headDiff < -headMargin) {
            headOver = headDiff + headMargin;
        } else {
            headOut = false;
        }
        if (headOut) {
            const float over  = (float)headOver;
            float       delta = (float)(timeFactor * std::abs(over));
            if (std::abs(over) > (double)headInner + delta) {
                delta = (float)(std::abs(over) - headInner);
            }
            if (heli && heli->m_fForcedOrientation > 0.0f) {
                heli->SetHeliOrientation((float)((double)heli->m_fForcedOrientation - (double)CTimer::GetTimeStep() * over * 0.1f));
            }
            if (over != 0.0f) {
                if (over < 0.0f) {
                    m_fHorizontalAngle = m_fHorizontalAngle - delta;
                } else {
                    m_fHorizontalAngle = delta + m_fHorizontalAngle;
                }
            }
        }

        // Same for the pitch
        {
            const float m1     = (float)(0.75f * (double)maxPitch);
            const float inner2 = (float)((1.0f - 0.75f) * (double)maxPitch);
            if ((double)m1 + pitchTarget < m_fVerticalAngle) {
                const double q = (double)pitchTarget - m_fVerticalAngle;
                if (q < -(double)m1) {
                    const float t = (float)std::abs(q + m1);
                    double      e = (double)CTimer::GetTimeStep() * 0.05f * t;
                    if ((double)inner2 + e < t) {
                        e = (double)t - inner2;
                    }
                    m_fVerticalAngle = (float)(m_fVerticalAngle - e);
                }
            }
        }
        {
            const float m2     = (float)(0.75f * (double)minPitch);
            const float inner3 = (float)((1.0f - 0.75f) * (double)minPitch);
            if ((double)pitchTarget - m2 > m_fVerticalAngle) {
                const double q = (double)pitchTarget - m_fVerticalAngle;
                if (q > m2) {
                    const float t = (float)std::abs(q - m2);
                    double      e = (double)CTimer::GetTimeStep() * 0.05f * t;
                    if ((double)inner3 + e < t) {
                        e = (double)t - inner3;
                    }
                    m_fVerticalAngle = (float)(e + m_fVerticalAngle);
                }
            }
        }
    } else {
        if (m_fVerticalAngle > 1.04719758f) {
            m_fVerticalAngle = 1.04719758f;
        } else if (m_fVerticalAngle < -1.49225652f) {
            m_fVerticalAngle = -1.49225652f;
        }

        targetPed->UpdateRwMatrix();
        targetPed->UpdateRwFrame();
        targetPed->UpdateRpHAnim();

        targetPed->GetBonePosition(&bonePos, BONE_HEAD, true);
        m_vecSource = bonePos;
        m_vecSource.z = m_vecSource.z + 0.1f;

        if (!targetPed->bIsDucking) {
            m_vecSource.x = (float)(m_vecSource.x - (double)backOfs * targetPed->GetMatrix().GetForward().x);
            m_vecSource.y = (float)(m_vecSource.y - (double)backOfs * targetPed->GetMatrix().GetForward().y);
        } else {
            m_vecSource.x = (float)(m_vecSource.x - (double)gM16CrouchBackOffset * targetPed->GetMatrix().GetForward().x);
            m_vecSource.y = (float)(m_vecSource.y - (double)gM16CrouchBackOffset * targetPed->GetMatrix().GetForward().y);
            m_vecSource.x = (float)(m_vecSource.x - (double)gM16CrouchSideOffset * targetPed->GetMatrix().GetRight().x);
            m_vecSource.y = (float)(m_vecSource.y - (double)gM16CrouchSideOffset * targetPed->GetMatrix().GetRight().y);
        }
    }

    // Limit the pitch
    if (m_fVerticalAngle < -gM16MaxPitch) {
        m_fVerticalAngle = -gM16MaxPitch;
    } else if (m_fVerticalAngle > gM16MaxPitch) {
        m_fVerticalAngle = gM16MaxPitch;
    }

    {
        const double cosV = x87::cos((double)m_fVerticalAngle);
        m_vecFront.x      = (float)-(x87::cos((double)m_fHorizontalAngle) * cosV);
        m_vecFront.y      = (float)-(x87::sin((double)m_fHorizontalAngle) * cosV);
        m_vecFront.z      = (float)x87::sin((double)m_fVerticalAngle);
    }

    CVector lookPos = m_vecSource; // local_24
    M16AddScaled(lookPos, 3.0f, m_vecFront);
    M16AddScaled(m_vecSource, 0.4f, m_vecFront);

    // Make sure the camera isn't inside / too close to the geometry
    // NOTE: The angles stay in the FPU registers (extended precision) in the original
    const auto IsProbeObstructed = [&](double h, double v) {
        const double cv = x87::cos(v);
        const float  x  = (float)(x87::cos(h) * cv);
        const float  y  = (float)(x87::sin(h) * cv);
        const float  z3 = (float)(x87::sin(v) * 3.0f);
        const CVector probe{ (float)((double)x * 3.0f + m_vecSource.x), (float)((double)y * 3.0f) + m_vecSource.y, z3 + m_vecSource.z };
        return !CWorld::GetIsLineOfSightClear(probe, m_vecSource, true, true, false, true, false, true, true);
    };

    bool skipNearClip = false;
    if (m_bCollisionChecksOn) {
        const double v = (double)m_fVerticalAngle - 0.34906587f;
        if (!CWorld::GetIsLineOfSightClear(lookPos, m_vecSource, true, true, false, true, false, true, true)
            || IsProbeObstructed((double)m_fHorizontalAngle + 0.610865235f, v)
            || IsProbeObstructed((double)m_fHorizontalAngle - 0.610865235f, v)
        ) {
            RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.3f);
            gbM16CamObstructed = true;
            skipNearClip       = true;
        } else {
            gbM16CamObstructed = false;
        }
    } else if (gbM16CamObstructed) {
        skipNearClip = true;
    }

    if (!skipNearClip && m_nMode == MODE_CAMERA) {
        const float fov = (15.0f < m_fFOV) ? 15.0f : m_fFOV; // Not `std::min` - differs for NaN
        RwCameraSetNearClipPlane(Scene.m_pRwCamera, (float)((((double)15.0f - fov) * 0.15f + 1.0) * 0.3f));
    }

    M16SubScaled(m_vecSource, 0.4f, m_vecFront);
    GetVectorsReadyForRW();

    const float heading = (float)x87::atan2((double)-m_vecFront.x, (double)m_vecFront.y);
    auto* const camPed  = TheCamera.m_pTargetEntity->AsPed();
    camPed->m_fCurrentRotation = heading;
    camPed->m_fAimingRotation  = heading;
}

// 0x511B50
void CCam::Process_Rocket(const CVector& target, float orientation, float speedVar, float speedVarWanted, bool isHeatSeeking) {
    NOTSA_GLOBAL_LOCAL(dword_B6FFF8, 0xB6FFF8, (uint32), {});
    NOTSA_GLOBAL_LOCAL(dword_B6FFFC, 0xB6FFFC, (uint32), {});
    NOTSA_GLOBAL_LOCAL(byte_B70000, 0xB70000, (bool), {});

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
    const auto fov    = m_fFOV * ExeRecip(80.0f);
    const auto amountMouseMoved = pad1->NewMouseControllerState.GetAmountMouseMoved();
    
    if (!amountMouseMoved.IsZero()) {
        m_fHorizontalAngle += -3.0f * amountMouseMoved.x * fov * CCamera::m_fMouseAccelHorzntl;
        m_fVerticalAngle += +4.0f * amountMouseMoved.y * fov * CCamera::m_fMouseAccelVertical;
    } else {
        const auto hv  = (float)-pad1->LookAroundLeftRight(targetPed);
        const auto vv  = (float)pad1->LookAroundUpDown(targetPed);

        m_fHorizontalAngle += sq(hv) * ExeRecip(10000.0f) * fov * ExeRecip(17.5f) * CTimer::GetTimeStep() * (hv < 0.0f ? -1.0f : 1.0f);
        m_fVerticalAngle   += sq(vv) * ExeRecip(22500.0f) * fov * ExeRecip(14.0f) * CTimer::GetTimeStep() * (vv < 0.0f ? -1.0f : 1.0f);
    }
    ClipBeta();
    ClipAlpha();

    m_vecFront.Set(
        -(x87::cos(m_fHorizontalAngle) * x87::cos(m_fVerticalAngle)),
        -(x87::sin(m_fHorizontalAngle) * x87::cos(m_fVerticalAngle)),
        x87::sin(m_fVerticalAngle)
    );
    GetVectorsReadyForRW();

    // 0x511E94: the unrounded atan result minus pi/2 (`fsub [0x858FE4]`), stored twice (`fld st(0)` + 2x `fstp`)
    const auto heading = (float)(CGeneral::GetATanOfXYExt(m_vecFront.x, m_vecFront.y) - (double)HALF_PI);
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
    TheCamera.AvoidTheGeometry(&fixedSource, &m_vecTargetCoorsForFudgeInter, &m_vecSource, m_fFOV);
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
                const auto heading = (float)x87::atan2((double)-dir.x, (double)dir.y);
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

    const double angle = x87::cos((double)(CTimer::GetTimeInMS() & 0x1FFFF) * (double)4.7936901e-05f) * (double)0.4f;
    const double s     = x87::sin(angle);
    const double c     = x87::cos(angle);
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

    // 0x51D610: blue, green, red order; everything stays on the x87 stack. A NaN magnitude takes the unscaled path (FCOM + JNE 0x41)
    const double red = CTimeCycle::GetWaterRed(), green = CTimeCycle::GetWaterGreen(), blue = CTimeCycle::GetWaterBlue();
    const double colorMag = std::sqrt(blue * blue + green * green + red * red);
    const double factor = colorMag > (double)UNDERWATER_CAM_MAG_LIMIT ? (double)UNDERWATER_CAM_MAG_LIMIT / colorMag : 1.0;

    TheCamera.SetMotionBlur(
        static_cast<uint32>(factor * red),
        static_cast<uint32>(factor * green),
        static_cast<uint32>(factor * blue),
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
