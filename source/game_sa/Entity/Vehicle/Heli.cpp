/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"
#include <numbers>
#include "WindModifiers.h"
#include "Shadows.h"
#include "CarCtrl.h"
#include "CullZones.h"
#include "InterestingEvents.h"
#include "Ropes.h"
#include "FireManager.h"
#include "Coronas.h"
#include "Population.h"
#include "Streaming.h"
#include "WaterLevel.h"
#include "AnimManager.h"
#include "TaskComplexSequence.h"
#include "TaskComplexUseSwatRope.h"
#include "TaskComplexWanderCop.h"
#include "CustomBuildingDNPipeline.h"

namespace HeliImpl {
// The original does its vector maths on the x87 stack: intermediates stay in extended precision and are rounded to float only where the original stores them.
// These replicate the operation order of the original helpers, as the shared ones in `CVector`/`CMatrix` round (and add) in a different way.

//! 0x59C910 - `CVector::Normalise`. A length of 0 (or less) only writes `x = 1` (NaN takes the sqrt path)
void NormaliseOriginal(CVector& v) {
    const double sumSq = ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
    if (sumSq <= 0.0) {
        v.x = 1.0f;
    } else {
        const double recip = 1.0 / std::sqrt(sumSq);
        v.x = (float)(v.x * recip);
        v.y = (float)(v.y * recip);
        v.z = (float)(v.z * recip);
    }
}

//! 0x59C730 - `CrossProduct`
CVector CrossProductOriginal(const CVector& a, const CVector& b) {
    return {
        (float)((double)b.z * a.y - (double)a.z * b.y),
        (float)((double)a.z * b.x - (double)b.z * a.x),
        (float)((double)a.x * b.y - (double)b.x * a.y),
    };
}

//! 0x59C790 - `CMatrix::Multiply3x3` (matrix * vector, no translation)
CVector Multiply3x3Original(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return CVector{
        (float)(((double)u.x * v.z + (double)f.x * v.y) + (double)r.x * v.x),
        (float)(((double)u.y * v.z + (double)r.y * v.x) + (double)f.y * v.y),
        (float)(((double)u.z * v.z + (double)r.z * v.x) + (double)f.z * v.y)
    };
}

//! 0x821B40 - `_ftol`: truncates towards zero (through a 64 bit `fistp`, the caller gets the low 32 bits). NaN / out of the 64 bit range gives 0 (low half of the "integer indefinite" 0x8000000000000000)
int32 Ftol(double v) {
    if (!(v > -9223372036854775809.0 && v < 9223372036854775808.0)) {
        return 0;
    }
    return (int32)(uint32)(uint64)(int64)v;
}
} // namespace HeliImpl

void CHeli::InjectHooks() {
    RH_ScopedVirtualClass(CHeli, 0x871680, 71);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(InitHelis, 0x6C4560);
    RH_ScopedInstall(AddHeliSearchLight, 0x6C45B0);
    RH_ScopedInstall(Pre_SearchLightCone, 0x6C4650);
    RH_ScopedInstall(Post_SearchLightCone, 0x6C46E0);
    RH_ScopedInstall(SwitchPoliceHelis, 0x6C4800);
    RH_ScopedInstall(SearchLightCone, 0x6C58E0);
    RH_ScopedInstall(GenerateHeli, 0x6C6520);
    RH_ScopedInstall(UpdateHelis, 0x6C79A0);
    RH_ScopedInstall(FindSwatPositionRelativeToHeli, 0x6C4760);
    RH_ScopedInstall(SendDownSwat, 0x6C69C0);
    RH_ScopedInstall(RenderAllHeliSearchLights, 0x6C7C50);
    RH_ScopedInstall(TestSniperCollision, 0x6C6890);
    RH_ScopedVMTInstall(Render, 0x6C4400);
    RH_ScopedVMTInstall(Fix, 0x6C4530);
    RH_ScopedVMTInstall(BurstTyre, 0x6C4330);
    RH_ScopedVMTInstall(SetUpWheelColModel, 0x6C4320);
    RH_ScopedVMTInstall(ProcessControlInputs, 0x6C4830);
    RH_ScopedVMTInstall(ProcessFlyingCarStuff, 0x6C4E60);
    RH_ScopedVMTInstall(PreRender, 0x6C5420);
    RH_ScopedVMTInstall(BlowUpCar, 0x6C6D30);
    RH_ScopedVMTInstall(ProcessControl, 0x6C7050);
}

// 0x6C4190
CHeli::CHeli(int32 modelIndex, eVehicleCreatedBy createdBy) : CAutomobile(modelIndex, createdBy, true) {
    m_nVehicleSubType = VEHICLE_TYPE_HELI;

    m_fLeftRightSkid           = 0.0f;
    m_fSteeringUpDown          = 0.0f;
    m_fSteeringLeftRight       = 0.0f;
    m_fAccelerationBreakStatus = 0.0f;

    field_99C = 0;
    m_fRotorZ = 0;
    m_fSecondRotorZ = 0;

    m_fMinAltitude = 10.0f;
    m_fMaxAltitude = 10.0f;

    field_9AC = 10.0f;
    field_9B4 = 0;

    m_nHeliFlags = m_nHeliFlags & 0xFC;
    m_fSearchLightIntensity = 0.0f;
    physicalFlags.bDontCollideWithFlyers = true;

    if (modelIndex == MODEL_HUNTER) {
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        m_doors[DOOR_LEFT_FRONT].Init((3.0f * PI) / 10.0f, 0.0f, DOOR_AXIS_NEG_X, DOOR_AXIS_Y, DOOR_EXTRA_BASED);
    }

    m_nNumSwatOccupants = 4;
    m_aSwatState.fill(0);

    m_nSearchLightTimer = CTimer::GetTimeInMS();

    m_aSearchLightHistoryX.fill(0.0f);
    m_aSearchLightHistoryY.fill(0.0f);

    m_nShootTimer = 0;
    m_nPoliceShoutTimer = CTimer::GetTimeInMS();

    vehicleFlags.bNeverUseSmallerRemovalRange = true; // 0x6C42BD
    m_autoPilot.m_ucHeliTargetDist2 = 10;

    m_ppGunflashFx = nullptr;
    m_nFiringMultiplier = 16;

    field_9B8 = 0;
    m_bSearchLightEnabled = false;
    field_A14 = CGeneral::GetRandomNumberInRange(2.f, 8.f);
}

// 0x6C4340
CHeli::~CHeli() {
    if (m_ppGunflashFx) {
        for (auto i = 0; i < CVehicle::GetPlaneNumGuns(); i++) {
            if (auto& fx = m_ppGunflashFx[i]) {
                fx->Kill();
                g_fxMan.DestroyFxSystem(fx);
            }
        }
        delete[] m_ppGunflashFx;
        m_ppGunflashFx = nullptr;
    }

    m_vehicleAudio.Terminate();
}

// 0x6C4560
void CHeli::InitHelis() {
    std::ranges::fill(pHelis, nullptr);
    for (auto& light : HeliSearchLights) {
        light.Init();
    }
    NumberOfSearchLights = 0;
    bPoliceHelisAllowed = true;
}

// 0x6C45B0
void CHeli::AddHeliSearchLight(const CVector& origin, const CVector& target, float targetRadius, float power, uint32 coronaIndex, uint8 unknownFlag, uint8 drawShadow) {
    auto& light = HeliSearchLights[NumberOfSearchLights];

    light.m_vecOrigin     = origin;
    light.m_vecTarget     = target;
    light.m_fTargetRadius = targetRadius;
    light.m_fPower        = power;
    light.m_nCoronaIndex  = coronaIndex;
    light.field_24        = unknownFlag;
    light.m_bDrawShadow   = drawShadow;

    NumberOfSearchLights += 1;
}

// 0x6C4640
void CHeli::PreRenderAlways() {
    // NOP
}

// 0x6C4650
void CHeli::Pre_SearchLightCone() {
    ZoneScoped;

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,          RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,             RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,            RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,        RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,            RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,            RWRSTATE(rwSHADEMODEGOURAUD));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION,    RWRSTATE(rwALPHATESTFUNCTIONGREATEREQUAL));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(0));
}

// 0x6C46E0
void CHeli::Post_SearchLightCone() {
    ZoneScoped;

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,         RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,          RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,             RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,            RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATECULLMODE,             RWRSTATE(rwCULLMODECULLBACK));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION,    RWRSTATE(rwALPHATESTFUNCTIONGREATER));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(2u));
}

// 0x6C4750
void CHeli::SpecialHeliPreRender() {
    // NOP
}

// 0x6C4760
CVector CHeli::FindSwatPositionRelativeToHeli(int32 swatNumber) {
    switch ( swatNumber ) {
    case 0:
        return { -1.2f, -1.0f, -0.5f };
    case 1:
        return { 1.2f,  -1.0f, -0.5f };
    case 2:
        return { -1.2f, 1.0f,  -0.5f };
    case 3:
        return { 1.2f,  1.0f,  -0.5f };
    default:
        return { 0.0f,  0.0f,  0.0f  };
    }
}

// 0x6C4800
void CHeli::SwitchPoliceHelis(bool enable) {
    bPoliceHelisAllowed = enable;
}

// 0x6C58E0
void CHeli::SearchLightCone(int32 coronaIndex,
                            CVector origin,
                            CVector target,
                            float targetRadius,
                            float power,
                            uint8 unknownFlag,
                            uint8 drawShadow,
                            CVector& useless0,
                            CVector& useless1,
                            CVector& useless2,
                            bool a11,
                            float baseRadius,
                            float a13,
                            float a14,
                            float a15
) {
    using namespace HeliImpl;

    const CVector& camPos = TheCamera.GetPosition();

    // 0x6C5909 - Direction of the light
    CVector dir{ target.x - origin.x, target.y - origin.y, target.z - origin.z };
    NormaliseOriginal(dir);

    // 0x6C593E - Extend the ray by 3 units, and stop at the first building
    const float dirZ3 = (float)((double)dir.z * 3.0f); // 0x858B3C
    target.x = (float)((double)dir.x * 3.0f + target.x);
    target.y = (float)((double)dir.y * 3.0f + target.y);
    target.z = (float)((double)dirZ3 + target.z);

    CColPoint colPoint;
    CEntity*  hitEntity{};
    if (CWorld::ProcessLineOfSight(origin, target, colPoint, hitEntity, true, false, false, false, false, false, false, false)) { // 0x56BA00
        target = colPoint.m_vecPoint;
    }

    // 0x6C59F7 - The point 100 units down the light
    const float dirZ100 = (float)((double)dir.z * 100.0f); // 0x858628
    const CVector farMid{
        (float)((double)dir.x * 100.0f + origin.x),
        (float)((double)dir.y * 100.0f + origin.y),
        (float)((double)dirZ100 + origin.z)
    };

    // 0x6C5A50 - Direction to the camera
    CVector toCam{ camPos.x - origin.x, camPos.y - origin.y, camPos.z - origin.z };
    NormaliseOriginal(toCam);

    double camDot = ((double)toCam.x * dir.x + (double)toCam.y * dir.y) + (double)toCam.z * dir.z;
    if (camDot < 0.0) { // 0x858B50
        camDot = 0.0;
    }

    // 0x6C5ABF - Corona at the origin, brighter the more we look into the light
    {
        const double pow6 = ((((camDot * camDot) * camDot) * camDot) * camDot) * camDot;
        CCoronas::RegisterCorona(
            coronaIndex,
            nullptr,
            200, 200, 255,
            (uint8)Ftol(pow6 * 255.0f), // 0x859AAC
            origin,
            (float)(20.0f * pow6), // 0x858BA4
            100.0f,
            CORONATYPE_SHINYSTAR,
            FLARETYPE_NONE,
            CORREFL_SIMPLE,
            LOSCHECK_OFF,
            TRAIL_OFF,
            0.0f,
            false,
            1.5f,
            false,
            15.0f,
            false,
            false
        );
    }

    // 0x6C5B27 - Build the cone: a "near" and a "far" vertex for every step around the circle
    uiTempBufferIndicesStored  = 0;
    uiTempBufferVerticesStored = 0;

    float                maxDotSq = 0.0f;
    std::array<float, 82> vtxFade{};  // [idx] = fade of the near vertex, [idx + 1] = 0.0f (far vertex)
    std::array<float, 82> vtxDotSq{}; // Squared dot product of the direction to the vertex with the direction to the camera
    CVector              cornerA{}, cornerB{};

    for (int32 i = 0; i <= 40; i++) {
        // 0x6C5B42 - Basis of the circle
        CVector right = CrossProductOriginal(dir, { 0.0f, 0.0f, 1.0f });
        NormaliseOriginal(right);
        CVector up = CrossProductOriginal(right, dir);
        NormaliseOriginal(up);

        const double angle = (double)i * (double)std::bit_cast<float>(0x3E20D97Cu); // 0x8717B4 (pi / 20)
        const float  sinA  = (float)x87::sin(angle);
        const float  cosA  = (float)x87::cos(angle);

        // 0x6C5BCD
        const double rightXs  = (double)right.x * sinA;
        const float  rightXsF = (float)rightXs;
        const double rightYs  = (double)right.y * sinA;
        const double rightZs  = (double)right.z * sinA;
        const float  rightZsF = (float)rightZs;

        const float  nearRightX = (float)((double)rightXsF * baseRadius);
        const float  nearRightY = (float)(rightYs * baseRadius);
        const double nearRightZ = rightZs * baseRadius;
        const double nearRightXOrigin = (double)nearRightX + origin.x;
        const float  nearTmpY = (float)((double)nearRightY + origin.y);
        const float  nearTmpZ = (float)(nearRightZ + origin.z);

        // 0x6C5C54
        const double upXc = (double)up.x * cosA;
        const double upYc = (double)up.y * cosA;
        const float  upXcF = (float)upXc;
        const float  upYcF = (float)upYc;
        const float  upZcF = (float)((double)up.z * cosA);

        const float nearUpX = (float)(upXc * baseRadius);
        const float nearUpY = (float)(upYc * baseRadius);
        const float nearUpZ = (float)((double)upZcF * baseRadius);

        // The vertex on the small circle (near the origin)
        const CVector P{
            (float)((double)nearUpX + nearRightXOrigin),
            (float)((double)nearUpY + nearTmpY),
            (float)((double)nearUpZ + nearTmpZ)
        };

        // 0x6C5CF3 - The point on the big circle (at the end of the light)
        const float  farRightX = (float)((double)rightXsF * targetRadius);
        const float  farRightY = (float)(rightYs * targetRadius);
        const double farRightZ = (double)rightZsF * targetRadius;

        const float  farTmpY = (float)((double)farRightY + farMid.y);
        const float  farTmpZ = (float)(farRightZ + farMid.z);
        const double farTmpX = (double)farRightX + farMid.x;

        const float farUpX = (float)((double)upXcF * targetRadius);
        const float farUpY = (float)((double)upYcF * targetRadius);
        const float farUpZ = (float)((double)upZcF * targetRadius);

        const double Qx = farTmpX + farUpX;
        const double Qy = (double)farUpY + farTmpY;
        const float  Qz = (float)((double)farUpZ + farTmpZ);

        // 0x6C5DB5 - Intersect P->Q with the plane at the target's height
        const float  t  = (float)(((double)P.z - target.z) / ((double)P.z - Qz));
        const float  dX = (float)(Qx - P.x);
        const double dY = Qy - P.y;
        const double dZ = (double)Qz - P.z;

        const float  tX = (float)((double)dX * t);
        const float  tY = (float)(dY * t);
        const double tZ = dZ * t;

        CVector R{
            (float)((double)tX + P.x),
            (float)((double)tY + P.y),
            (float)(tZ + P.z)
        };

        // 0x6C5E3E - Remember the corners of the lit area
        if (i == 20) {
            cornerA = R;
        } else if (i == 30) {
            cornerB = R;
        }

        // 0x6C5E7B - Limit the length of P->R to 100 units
        const double rpX = (double)R.x - P.x;
        const double rpY = (double)R.y - P.y;
        const double rpZ = (double)R.z - P.z;
        const float  rpYF = (float)rpY;
        const float  rpZF = (float)rpZ;
        if (std::sqrt((rpZ * rpZ + rpY * rpY) + rpX * rpX) > 100.0) { // 0x858628
            CVector v{ (float)rpX, rpYF, rpZF };
            NormaliseOriginal(v);

            const float vZ100 = (float)((double)v.z * 100.0f);
            R.x = (float)((double)v.x * 100.0f + P.x);
            R.y = (float)((double)v.y * 100.0f + P.y);
            R.z = (float)((double)vZ100 + P.z);
        }

        // 0x6C5F5D
        const auto idx = (int32)uiTempBufferVerticesStored;
        const auto fade = (float)((double)CCustomBuildingDNPipeline::m_fDNBalanceParam * 0.15f + 0.1f); // 0x8D12C0, 0x858FCC, 0x858B1C

        auto& vtxNear = TempBufferVertices.m_3d[idx];
        auto& vtxFar  = TempBufferVertices.m_3d[idx + 1];
        RwCompatVertexPos(vtxNear).x = P.x;
        RwCompatVertexPos(vtxNear).y = P.y;
        RwCompatVertexPos(vtxNear).z = P.z;
        RwCompatVertexPos(vtxFar).x  = R.x;
        RwCompatVertexPos(vtxFar).y  = R.y;
        RwCompatVertexPos(vtxFar).z  = R.z;

        CVector toVtx{ P.x - origin.x, P.y - origin.y, P.z - origin.z };
        NormaliseOriginal(toVtx);

        double vtxCamDot = ((double)toVtx.x * toCam.x + (double)toVtx.z * toCam.z) + (double)toVtx.y * toCam.y;
        if (vtxCamDot < 0.0) {
            vtxCamDot = -vtxCamDot;
        }
        const double vtxCamDotSq = vtxCamDot * vtxCamDot;
        if (vtxCamDotSq > (double)maxDotSq) {
            maxDotSq = (float)vtxCamDotSq;
        }

        vtxFade[idx]       = fade;
        vtxFade[idx + 1]   = 0.0f;
        vtxDotSq[idx]      = (float)vtxCamDotSq;
        vtxDotSq[idx + 1]  = (float)vtxCamDotSq;

        if (i != 40) {
            auto indices = (int32)uiTempBufferIndicesStored;
            aTempBufferIndices[indices + 0] = (RxVertexIndex)idx;
            aTempBufferIndices[indices + 1] = (RxVertexIndex)(idx + 3);
            aTempBufferIndices[indices + 2] = (RxVertexIndex)(idx + 1);
            indices += 3;
            uiTempBufferIndicesStored = (uint16)indices;
            if (baseRadius > 0.0f) {
                aTempBufferIndices[indices + 0] = (RxVertexIndex)idx;
                aTempBufferIndices[indices + 1] = (RxVertexIndex)(idx + 2);
                aTempBufferIndices[indices + 2] = (RxVertexIndex)(idx + 3);
                indices += 3;
                uiTempBufferIndicesStored = (uint16)indices;
            }
        }

        uiTempBufferVerticesStored = (uint16)(idx + 2);
    }

    // 0x6C60D5 - Colour of every vertex
    const auto numVertices = (int32)uiTempBufferVerticesStored;
    {
        const double k = 1.0 / (double)maxDotSq; // 0x858624
        for (int32 j = 0; j < numVertices; j++) {
            const double v  = ((double)vtxDotSq[j] * (double)vtxFade[j]) * k;
            const auto   c1 = (uint8)Ftol(200.0f * v); // 0x858A48
            const auto   c2 = (uint8)Ftol(v * 255.0f); // 0x859AAC
            TempBufferVertices.m_3d[j].color = ((uint32)c1 << 16) | ((uint32)c1 << 8) | (uint32)c2;
        }
    }

    // 0x6C6280 - Render
    if (uiTempBufferIndicesStored > 0) {
        if (RwIm3DTransform(TempBufferVertices.m_3d, numVertices, nullptr, rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA)) { // flags = 0x18
            RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, aTempBufferIndices, uiTempBufferIndicesStored);
            RwIm3DEnd();
        }
    }

    // 0x6C62B5 - Results
    useless0 = target;
    useless1 = CVector{
        cornerA.x - target.x,
        cornerA.y - target.y,
        cornerA.z - target.z
    };
    useless2 = CVector{
        cornerB.x - target.x,
        cornerB.y - target.y,
        cornerB.z - target.z
    };

    // 0x6C6387 - Shadow of the light
    if (!drawShadow) {
        return;
    }

    CVector shadowPos{ target.x, target.y, (float)((double)target.z + 5.0f) }; // 0x858C80
    const auto topX   = (float)((double)useless1.x * 1.2f); // 0x858F08
    const auto topY   = (float)((double)useless1.y * 1.2f);
    const auto rightX = (float)((double)useless2.x * 1.2f);
    const auto rightY = (float)((double)useless2.y * 1.2f);

    if (!(std::sqrt((double)topX * topX + (double)topY * topY) < 100.0)) { // 0x858628
        return;
    }
    if (!(std::sqrt((double)rightX * rightX + (double)rightY * rightY) < 100.0)) {
        return;
    }

    const double dx = (double)target.x - camPos.x;
    const double dy = (double)target.y - camPos.y;
    const double camDist = std::sqrt(dy * dy + dx * dx);
    if (camDist > 25.0) { // 0x858FE8
        return;
    }

    const double s = ((1.0 - camDist * 0.04f) * (double)power) * 0.5; // 0x858CEC, 0x858B8C
    const auto red   = (uint8)Ftol(200.0f * s); // 0x858A48
    const auto blue  = (uint8)Ftol(255.0f * s); // 0x859AAC
    const auto inten = (int16)Ftol(s * 128.0f); // 0x858BF4

    CShadows::StoreShadowToBeRendered(
        2,
        gpShadowExplosionTex,
        shadowPos,
        topX, topY,
        rightX, rightY,
        inten,
        red, red, blue,
        15.0f,
        true,
        1.0f,
        nullptr,
        false
    );
}

// 0x6C6520
CHeli* CHeli::GenerateHeli(CPed* target, bool newsHeli) {
    CHeli* const heli = newsHeli
        ? new CHeli(MODEL_VCNMAV, PERMANENT_VEHICLE)
        : new CHeli(MODEL_POLMAV, PERMANENT_VEHICLE);

    // 0x6C65A0 - Pick a spot 250 units away from the target, in a random direction
    const CVector targetPos = target->GetPosition();
    const double  angle0    = (double)(rand() & 0xFF) * (double)std::bit_cast<float>(0x3CC8F5C3u); // 0x859C44
    float         angle     = (float)angle0;

    CVector pos{
        (float)(x87::cos(angle0) * 250.0f + targetPos.x), // 0x859F80
        (float)(x87::sin((double)angle) * 250.0f + targetPos.y),
        targetPos.z
    };

    // 0x6C660F - Out of the map? Then the opposite direction
    if (pos.x < -3000.0f || pos.x > 3000.0f || pos.y < -3000.0f || pos.y > 3000.0f) { // 0x859A90, 0x859A94
        angle = (float)((double)angle + std::numbers::pi_v<float>); // 0x858CB8

        const CVector targetPos2 = target->GetPosition();
        pos = CVector{
            (float)(x87::cos((double)angle) * 250.0f + targetPos2.x),
            (float)(x87::sin((double)angle) * 250.0f + targetPos2.y),
            targetPos2.z
        };
    }

    // 0x6C66A8
    float z = (float)((double)pos.z + 50.0f); // 0x858B40
    const auto toChiliadX = (double)pos.x - StaticRef<float>(0x8717C0);
    const auto toChiliadY = (double)pos.y - StaticRef<float>(0x8717BC);
    if (std::sqrt(toChiliadY * toChiliadY + toChiliadX * toChiliadX) < 350.0f) { // Too close to Mt. Chiliad? Spawn right above the target. 0x858A4C
        const CVector targetPos3 = target->GetPosition();
        pos.x = targetPos3.x;
        pos.y = targetPos3.y;

        double zz = (double)targetPos3.z + 200.0f; // 0x858A48
        if (!(zz > (double)StaticRef<float>(0x8717B8))) {
            zz = StaticRef<float>(0x8717B8);
        }
        z = (float)zz;
    }

    // 0x6C6730 - Not underground please
    const double groundZ = (double)CWorld::FindGroundZForCoord(pos.x, pos.y) + 20.0f; // 0x858BA4
    const float  finalZ  = !((double)z > groundZ) ? (float)groundZ : z;

    heli->GetMatrix().SetTranslate({ pos.x, pos.y, finalZ });
    heli->vehicleFlags.bIsLocked = true;
    heli->SetStatus(STATUS_PHYSICS);

    // NOTSA: 0x6C6786 - The original searches for a free slot in `pHelis` here, but never uses the result.

    if (newsHeli) {
        heli->m_autoPilot.m_nCarMission = MISSION_HELI_NEWS_BEHAVIOUR;
        // 0x6C67B7 - `FindPlayerPed()` is stored into the target here, but it's overwritten right below
        heli->m_autoPilot.m_nCruiseSpeed = 35;
    } else {
        heli->m_autoPilot.m_nCarMission = MISSION_HELI_POLICE_BEHAVIOUR;
        heli->m_autoPilot.m_nCruiseSpeed = 70;
    }
    heli->m_autoPilot.m_TargetEntity = reinterpret_cast<CVehicle*>(target); // `m_TargetEntity` is a `CEntity*` in practice
    heli->m_fMaxAltitude = 20.0f;
    heli->m_fMinAltitude = 12.0f;
    if (newsHeli) {
        heli->m_fMaxAltitude = 30.0f;
        heli->m_fMinAltitude = 27.0f;
    }

    // 0x6C6813 - Orientation: heading = angle + pi
    const float heading = (float)((double)angle + std::numbers::pi_v<float>);
    heli->m_fHeliRotorSpeed = 0.165f; // 0x3E28F5C3

    auto& mat = heli->GetMatrix();
    mat.GetRight().z   = 0.0f;
    mat.GetForward().z = 0.0f;
    mat.GetUp().x      = 0.0f;
    mat.GetUp().y      = 0.0f;
    mat.GetUp().z      = 1.0f;
    const double sinH = x87::sin((double)heading);
    mat.GetRight().x   = (float)sinH;
    const double cosH = x87::cos((double)heading);
    mat.GetRight().y   = (float)-cosH;
    mat.GetForward().x = (float)cosH;
    mat.GetForward().y = (float)sinH;

    CWorld::Add(heli); // 0x563220
    heli->SetUpDriver(-1, false, false); // 0x6D1A50

    return heli;
}

// 0x6C6890
void CHeli::TestSniperCollision(CVector* origin, CVector* target) {
    CVector point = *target - *origin;

    if (point.z >= point.Magnitude() / 2.0f)
        return;

    for (auto& heli : pHelis) {
        if (!heli || heli->physicalFlags.bBulletProof)
            continue;

        const auto mat = (CMatrix*)heli->m_matrix;
        if (CCollision::DistToLine(*origin, *target, mat->TransformPoint({ -0.43f, 1.49f, 1.5f })) < 0.8f) {
            heli->m_fRotationBalance = (float)(CGeneral::GetRandomNumber() < pow(2, 14) - 1) * 0.1f - 0.05f; // 2^14 - 1 = 16383 [-0.05, 0.05]
            heli->BlowUpCar(FindPlayerPed(), false);
            heli->m_nNumSwatOccupants = 0;
        };
    }
}

// 0x6C69C0
bool CHeli::SendDownSwat() {
    using namespace HeliImpl;

    const CVector targetPos = m_autoPilot.m_TargetEntity->GetPosition();

    if (m_nNumSwatOccupants == 0 || physicalFlags.bSubmergedInWater || !CStreaming::IsModelLoaded(MODEL_SWAT) || (rand() & 0x7F) != 0) { // 0x6C69C0, 0x6C6A24, 0x8E6314, 0x6C6A31
        return false;
    }

    // 0x6C6A3E - Close enough to the target and slow enough?
    const auto& pos = GetPosition();
    const double dX = (double)pos.x - targetPos.x;
    const double dY = (double)pos.y - targetPos.y;
    const double dZ = (double)pos.z - targetPos.z;
    if (std::sqrt((dX * dX + dY * dY) + dZ * dZ) > 50.0f) { // 0x858B40
        return false;
    }

    const double speed = std::sqrt(((double)m_vecMoveSpeed.x * m_vecMoveSpeed.x + (double)m_vecMoveSpeed.y * m_vecMoveSpeed.y) + (double)m_vecMoveSpeed.z * m_vecMoveSpeed.z); // 0x4082C0
    if (speed > 0.1f) { // 0x858B1C
        return false;
    }

    // 0x6C6AA1 - Where's the rope?
    const CMatrix mat{ GetMatrix() };
    const auto    swatIdx   = (uint8)m_nNumSwatOccupants - 1;
    const auto    swatOfs   = FindSwatPositionRelativeToHeli(swatIdx);
    CVector       ropePos   = Multiply3x3Original(mat, swatOfs); // 0x59C790
    ropePos += GetPosition(); // 0x411A00

    const auto groundZ = CWorld::FindGroundZFor3DCoord(ropePos, nullptr, nullptr);

    // 0x6C6B2E - Is the ground close to the target?
    double zDiff = (double)targetPos.z - groundZ;
    if (zDiff < 0.0) { // 0x858B50
        zDiff = -zDiff;
    }
    if (!(zDiff < 2.5f)) { // 0x858FA0
        return false;
    }

    // 0x6C6B52 - Not above the water
    float waterLevel;
    if (CWaterLevel::GetWaterLevelNoWaves(ropePos, &waterLevel, nullptr, nullptr) && !(waterLevel < groundZ)) {
        return false;
    }

    // 0x6C6B89 - Let the rope down.
    // NOTSA: The original checks the result for `< 0`, but it only returns 0 or 1, so this never fails.
    const auto ropeId = GetRopeId(); // The rope is identified by `this + i`
    CRopes::RegisterRope(ropeId, 8, ropePos, false, 0, 0, nullptr, 20000);

    // 0x6C6BCD - Spawn the SWAT guy and make him abseil
    CPed* const swat = CPopulation::AddPed(PED_TYPE_COP, (eModelID)COP_TYPE_SWAT2, ropePos, true); // For cops this is the `eCopType`

    auto* const seq = new CTaskComplexSequence();
    seq->AddTask(new CTaskComplexUseSwatRope(ropeId, this));
    seq->AddTask(new CTaskComplexWanderCop(PEDMOVE_WALK, CGeneral::GetRandomNumberInRange(0, 8)));
    swat->GetTaskManager().SetTask(seq, TASK_PRIMARY_PRIMARY, false); // 0x681AF0

    swat->m_bUsesCollision = false;

    m_nNumSwatOccupants--;
    m_aSwatState[(uint8)m_nNumSwatOccupants] = 0xAA;

    CAnimManager::BlendAnimation(swat->GetRpClump(), ANIM_GROUP_DEFAULT, ANIM_ID_ABSEIL, 4.0f); // 0x4D4610

    return true;
}

// 0x6C79A0
void CHeli::UpdateHelis() {
    ZoneScoped;

    NumberOfSearchLights = 0;

    int32 numHelis{};
    bool  policeHeliExists{};

    int32 numHelisRequired = FindPlayerWanted()->NumOfHelisRequired(); // 0x561FA0

    // 0x6C79C7 - Count the existing helis
    const auto IsAlive = [](CHeli* heli) {
        return !heli->physicalFlags.bRenderScorched && !heli->vehicleFlags.bIsDrowning;
    };
    if (pHelis[0]) {
        numHelis = 1;
        if (pHelis[0]->m_nModelIndex == MODEL_POLMAV && IsAlive(pHelis[0])) {
            policeHeliExists = true;
        }
    }
    if (pHelis[1]) {
        numHelis++;
        if (pHelis[1]->m_nModelIndex == MODEL_POLMAV && IsAlive(pHelis[1])) {
            policeHeliExists = true;
        }
    }

    // 0x6C7A18 - Do we want any?
    if (CCullZones::PlayerNoRain() || CGame::currArea != AREA_CODE_NORMAL_WORLD) {
        numHelisRequired = 0;
    }
    if (!bPoliceHelisAllowed) {
        numHelisRequired = 0;
    }
    if (CWeather::OldWeatherType == WEATHER_SANDSTORM_DESERT || CWeather::NewWeatherType == WEATHER_SANDSTORM_DESERT) {
        numHelisRequired = 0;
    }

    // Generate a news heli, if there is a police one already (and there's no news heli yet)
    bool spawnNewsHeli = policeHeliExists;
    if (pHelis[0] && pHelis[0]->m_nModelIndex == MODEL_VCNMAV) {
        spawnNewsHeli = false;
    }
    if (pHelis[1] && pHelis[1]->m_nModelIndex == MODEL_VCNMAV) {
        spawnNewsHeli = false;
    }
    if (!CWanted::UseNewsHeliInAdditionToPolice) {
        spawnNewsHeli = false;
    }

    // 0x6C7A96 - Model loaded? (0x8E72F0 / 0x8E73A4)
    const bool isModelLoaded = CStreaming::IsModelLoaded(spawnNewsHeli ? MODEL_VCNMAV : MODEL_POLMAV);
    if (isModelLoaded && CTimer::GetTimeInMS() > TestForNewRandomHelisTimer) {
        TestForNewRandomHelisTimer = CTimer::GetTimeInMS() + 15000;

        if (numHelis < numHelisRequired) {
            CHeli* const heli = GenerateHeli(FindPlayerPed(), spawnNewsHeli); // 0x6C7AC1
            if (!pHelis[0]) {
                pHelis[0] = heli;
                heli->RegisterReference(reinterpret_cast<CEntity**>(&pHelis[0]));
            } else if (!pHelis[1]) {
                pHelis[1] = heli;
                heli->RegisterReference(reinterpret_cast<CEntity**>(&pHelis[1]));
            }
            // BUG: The heli is leaked if there's no free slot
        }
    }

    // 0x6C7B12 - Remove wrecked helis, and the ones that flew away
    for (auto& slot : pHelis) {
        CHeli* const heli = slot;
        if (!heli) {
            continue;
        }

        if (!IsAlive(heli)) {
            heli->m_autoPilot.m_nCarMission = MISSION_HELI_FLY_AWAY_FROM_PLAYER;
            slot = nullptr;
            continue;
        }

        if (heli->m_autoPilot.m_nCarMission != MISSION_HELI_FLY_AWAY_FROM_PLAYER) {
            continue;
        }

        const auto& helipos = heli->GetPosition();
        const auto  plypos  = FindPlayerCoors(); // 0x56E010
        const double dX = (double)plypos.x - helipos.x;
        const double dY = (double)plypos.y - helipos.y;
        const double dZ = (double)plypos.z - helipos.z;
        if (std::abs(std::sqrt((dZ * dZ + dY * dY) + dX * dX)) > 170.0f) { // NOTE: the original adds z, y, x in this order // 0x858F98
            CWorld::Remove(heli);
            delete slot;
            slot = nullptr;
        }
    }

    // 0x6C7BCB - Too many helis? Send them away
    const auto FlyAwayIfNotNeeded = [&](CHeli* heli) {
        heli->m_autoPilot.m_nCarMission = MISSION_HELI_FLY_AWAY_FROM_PLAYER;
        heli->m_fMinAltitude = 100.0f;
        heli->m_fMaxAltitude = 100.0f;
    };
    if (pHelis[0] && pHelis[0]->m_autoPilot.m_nCarMission != MISSION_HELI_FLY_AWAY_FROM_PLAYER) {
        if (numHelisRequired > 0) {
            numHelisRequired--;
        } else {
            FlyAwayIfNotNeeded(pHelis[0]);
        }
    }
    if (pHelis[1] && pHelis[1]->m_autoPilot.m_nCarMission != MISSION_HELI_FLY_AWAY_FROM_PLAYER && numHelisRequired <= 0) {
        FlyAwayIfNotNeeded(pHelis[1]);
    }
}

// 0x6C7C50
void CHeli::RenderAllHeliSearchLights() {
    ZoneScoped;

    for (auto& light : HeliSearchLights) {
        SearchLightCone(
            light.m_nCoronaIndex,
            light.m_vecOrigin,
            light.m_vecTarget,
            light.m_fTargetRadius,
            light.m_fPower,
            light.field_24,
            light.m_bDrawShadow,
            light.m_vecUseless[0],
            light.m_vecUseless[1],
            light.m_vecUseless[2],
            false,
            0.05f,
            0.0f,
            0.0f,
            1.0f
        );
    }
}

// 0x6C6D30
void CHeli::BlowUpCar(CEntity* damager, bool bHideExplosion) {
    if (!vehicleFlags.bCanBeDamaged) {
        return;
    }

    const auto isRCHeli = m_nModelIndex == MODEL_RCRAIDER || m_nModelIndex == MODEL_RCGOBLIN;

    // 0x6C6D42 - Non-player helis crash and burn first
    if (GetStatus() != STATUS_PLAYER && m_autoPilot.m_nCarMission != MISSION_HELI_CRASH_AND_BURN && !isRCHeli) {
        m_autoPilot.m_nCarMission = MISSION_HELI_CRASH_AND_BURN; // Not `SetCarMission`, but the same thing
        m_fHealth = 0.0f;
        return;
    }

    if (damager == FindPlayerPed() || damager == FindPlayerVehicle()) { // 0x6C6D7A
        auto& playerInfo = FindPlayerInfo();
        playerInfo.m_nHavocCaused += 20;
        playerInfo.m_fCurrentChaseValue += 10.0f; // 0x85862C
        CStats::IncrementStat(STAT_COST_OF_PROPERTY_DAMAGED, (float)(rand() % 6000 + 4000));
    }

    if (m_nModelIndex == MODEL_VCNMAV) {
        CWanted::UseNewsHeliInAdditionToPolice = false;
    }

    if (GetStatus() == STATUS_PLAYER) { // 0x6C6DFA
        m_bUsesCollision = false; // `m_nFlags &= 0xFFFFFF7E` (bits 0 and 7)
        m_bIsVisible     = false;
        ResetMoveSpeed();
        ResetTurnSpeed();
    }

    SetStatus(STATUS_WRECKED);
    physicalFlags.bRenderScorched = true;
    m_nTimeWhenBlowedUp = CTimer::GetTimeInMS();
    CVisibilityPlugins::SetClumpForAllAtomicsFlag(GetRpClump(), eAtomicComponentFlag::ATOMIC_PIPE_NO_EXTRA_PASSES); // 0x6C6E3D
    m_damageManager.FuckCarCompletely(false);

    if (!isRCHeli) { // 0x6C6E51
        SetBumperDamage(FRONT_BUMPER, false);
        SetBumperDamage(REAR_BUMPER, false);
        SetDoorDamage(DOOR_BONNET, false);
        SetDoorDamage(DOOR_BOOT, false);
        SetDoorDamage(DOOR_LEFT_FRONT, false);
        SetDoorDamage(DOOR_RIGHT_FRONT, false);
        SetDoorDamage(DOOR_LEFT_REAR, false);
        SetDoorDamage(DOOR_RIGHT_REAR, false);
        SpawnFlyingComponent(CAR_WHEEL_LF, 1);

        // BUG: The original doesn't check if the node exists
        if (notsa::IsFixBugs() ? m_aCarNodes[HELI_WHEEL_LF] != nullptr : true) {
            RpAtomic* atomic = nullptr;
            RwFrameForAllObjects(m_aCarNodes[HELI_WHEEL_LF], GetCurrentAtomicObjectCB, &atomic);
            if (atomic) {
                RpAtomicSetFlags(atomic, 0);
            }
        }
    }

    m_nBombOnBoard = 0; // 0x6C6EEB
    m_fHealth      = 0.0f;
    m_wBombTimer   = 0;

    TheCamera.CamShake(0.4f, GetPosition());
    KillPedsInVehicle();

    m_nOverrideLights          = NO_CAR_LIGHT_OVERRIDE; // 0x6C6F49
    vehicleFlags.bEngineOn     = false;
    vehicleFlags.bLightsOn     = false;
    vehicleFlags.bSirenOrAlarm = false;
    autoFlags.bTaxiLight       = false;

    if (vehicleFlags.bIsAmbulanceOnDuty) {
        vehicleFlags.bIsAmbulanceOnDuty = false;
        CCarCtrl::NumAmbulancesOnDuty--;
    }

    if (vehicleFlags.bIsFireTruckOnDuty) {
        vehicleFlags.bIsFireTruckOnDuty = false;
        CCarCtrl::NumFireTrucksOnDuty--;
    }

    ChangeLawEnforcerState(false);
    gFireManager.StartFire(this, damager, 0.8f, 1, 7000, 0);
    CDarkel::RegisterCarBlownUpByPlayer(*this, 0);
    CExplosion::AddExplosion(
        this,
        damager,
        isRCHeli ? EXPLOSION_RC_VEHICLE : EXPLOSION_AIRCRAFT,
        GetPosition(),
        0,
        1,
        -1.0f,
        0
    );
}

// 0x6C4530
void CHeli::Fix() {
    m_damageManager.ResetDamageStatus();
    SetupDamageAfterLoad();
}

// 0x6C4330
bool CHeli::BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect) {
    return false;
}

// 0x6C4320
bool CHeli::SetUpWheelColModel(CColModel* wheelCol) {
    return false;
}

// 0x6C4830
void CHeli::ProcessControlInputs(uint8 playerNum) {
    const auto pad = CPad::GetPad(playerNum);

    m_fAccelerationBreakStatus = (float)((int32)pad->GetAccelerate() - (int32)pad->GetBrake()) * ExeRecip(255.0f); // 0x859A3C

    // 0x6C4A4E / 0x6C4952
    const auto SteerWithPad = [&] {
        m_nLastControlInput  = eControllerType::KEYBOARD;
        m_fSteeringUpDown    = (float)(int32)pad->GetSteeringUpDown() * (1.0f / 128.0f);
        m_fSteeringLeftRight = (float)(-(int32)pad->GetSteeringLeftRight()) * (1.0f / 128.0f);
    };

    if (!CCamera::m_bUseMouse3rdPerson || !m_bEnableMouseFlying) {
        SteerWithPad();
    } else {
        const auto& mouseMoved = CPad::NewMouseControllerState.m_AmountMoved;

        bool useMouse = false; // 0x6C4993
        if (mouseMoved.x != 0.0f || mouseMoved.y != 0.0f) {
            useMouse = true;
        } else if (   (std::abs(m_fSteeringLeftRight) > 0.0f || std::abs(m_fSteeringUpDown) > 0.0f)
                   && m_nLastControlInput == eControllerType::MOUSE
                   && pad->GetSteeringLeftRight() == 0
                   && pad->GetSteeringUpDown() == 0
        ) {
            useMouse = true;
        }

        if (useMouse) {
            m_nLastControlInput = eControllerType::MOUSE;
            if (pad->NewState.m_bVehicleMouseLook == 0) {
                m_fSteeringLeftRight = (float)((double)m_fSteeringLeftRight - (double)mouseMoved.x * (double)0.0025f); // 0x871674
                m_fSteeringUpDown    = (float)((double)mouseMoved.y * (double)0.0025f + (double)m_fSteeringUpDown);
            }
            if (std::abs(m_fSteeringLeftRight) < 0.5f) {
                m_fSteeringLeftRight = (float)(std::pow((double)0.98f, (double)CTimer::GetTimeStep()) * (double)m_fSteeringLeftRight); // 0x87167C
            }
            if (std::abs(m_fSteeringUpDown) < 0.5f) {
                m_fSteeringUpDown = (float)(std::pow((double)0.98f, (double)CTimer::GetTimeStep()) * (double)m_fSteeringUpDown);
            }
        } else if (pad->GetSteeringLeftRight() != 0 || pad->GetSteeringUpDown() != 0 || m_nLastControlInput != eControllerType::MOUSE) { // 0x6C492E
            SteerWithPad();
        } // else: keep the mouse values as they are
    }

    m_fSteeringUpDown    = std::clamp(m_fSteeringUpDown, -1.0f, 1.0f); // 0x6C4A96
    m_fSteeringLeftRight = std::clamp(m_fSteeringLeftRight, -1.0f, 1.0f);

    m_fLeftRightSkid = (float)pad->GetLookRight();
    if (pad->GetLookLeft()) {
        m_fLeftRightSkid = -1.0f;
    }

    // 0x6C4B4F - Horn: levels the helicopter out
    if (pad->GetHorn() && GetUp().z > 0.0f) {
        m_fLeftRightSkid = 0.0f;

        const auto flying = m_pFlyingHandlingData;
        const auto& speed = m_vecMoveSpeed;

        auto pitchDir = CrossProduct(CVector{ 0.0f, 0.0f, 1.0f }, GetRight());
        pitchDir.Normalise();
        const double pitch = ((double)pitchDir.y * speed.y + (double)pitchDir.z * speed.z + (double)pitchDir.x * speed.x) * (double)flying->m_fPitchStab;
        m_fSteeringUpDown = (float)std::clamp(pitch, -2.0, 2.0);

        auto rollDir = CrossProduct(GetForward(), CVector{ 0.0f, 0.0f, 1.0f });
        rollDir.Normalise();
        const double roll = ((double)rollDir.y * speed.y + (double)rollDir.z * speed.z + (double)rollDir.x * speed.x) * (double)flying->m_fRollStab;
        m_fSteeringLeftRight = (float)std::clamp(roll, -2.0, 2.0);
    }

    m_fSteerAngle = 0.0f; // 0x6C4D75
    m_BrakePedal  = 1.0f;
    m_GasPedal    = 0.0f;
    vehicleFlags.bIsHandbrakeOn = false;

    if (pad->DisablePlayerControls) {
        FindPlayerPed()->KeepAreaAroundPlayerClear();

        const double mag = std::sqrt((double)m_vecMoveSpeed.x * m_vecMoveSpeed.x + (double)m_vecMoveSpeed.y * m_vecMoveSpeed.y + (double)m_vecMoveSpeed.z * m_vecMoveSpeed.z);
        if (mag > (double)0.28f) { // 0x871254
            const double scale = (double)0.28f / mag;
            m_vecMoveSpeed.x = (float)(scale * m_vecMoveSpeed.x);
            m_vecMoveSpeed.y = (float)(scale * m_vecMoveSpeed.y);
            m_vecMoveSpeed.z = (float)(scale * m_vecMoveSpeed.z);
        }
    }

    if (m_fHealth < 250.0f) { // 0x6C4E1A
        m_fAccelerationBreakStatus = -0.1f;
        m_fLeftRightSkid = (float)((double)m_fLeftRightSkid + 0.5);
    }
}

// 0x6C4400
void CHeli::Render() {
    auto* mi = GetVehicleModelInfo();
    m_nTimeTillWeNeedThisCar = CTimer::GetTimeInMS() + 3000;
    mi->SetVehicleColour(m_nPrimaryColor, m_nSecondaryColor, m_nTertiaryColor, m_nQuaternaryColor);

    auto staticRotor = m_aCarNodes[HELI_STATIC_ROTOR];
    RpAtomic* data = nullptr;
    if (staticRotor) {
        RwFrameForAllObjects(staticRotor, GetCurrentAtomicObjectCB, &data);
        if (data)
            CVehicle::SetComponentAtomicAlpha(data, 255);
    }

    auto staticRotor2 = m_aCarNodes[HELI_STATIC_ROTOR2];
    data = nullptr;
    if (staticRotor2) {
        RwFrameForAllObjects(staticRotor2, GetCurrentAtomicObjectCB, &data);
        if (data)
            CVehicle::SetComponentAtomicAlpha(data, 255);
    }

    auto movingRotor = m_aCarNodes[HELI_MOVING_ROTOR];
    data = nullptr;
    if (movingRotor) {
        RwFrameForAllObjects(movingRotor, GetCurrentAtomicObjectCB, &data);
        if (data)
            CVehicle::SetComponentAtomicAlpha(data, 0);
    }

    auto movingRotor2 = m_aCarNodes[HELI_MOVING_ROTOR2];
    data = nullptr;
    if (movingRotor2) {
        RwFrameForAllObjects(movingRotor2, GetCurrentAtomicObjectCB, &data);
        if (data)
            CVehicle::SetComponentAtomicAlpha(data, 0);
    }

    CEntity::Render(); // exactly CEntity
}

// 0x6C4550
void CHeli::SetupDamageAfterLoad() {
    vehicleFlags.bIsDamaged = false;
}

// 0x6C4E60
void CHeli::ProcessFlyingCarStuff() {
    const auto isRCHeli = [this] { return m_nModelIndex == MODEL_RCRAIDER || m_nModelIndex == MODEL_RCGOBLIN; };

    const auto status = GetStatus();
    if (status != STATUS_PLAYER && status != STATUS_REMOTE_CONTROLLED && status != STATUS_PHYSICS) {
        if (!m_pHandlingData->m_bIsHeli) { // 0x6C4E93
            return;
        }

        vehicleFlags.bEngineOn = false;

        // Rotor spins down
        const double decay = (double)CTimer::GetTimeStep() * (double)0.00055f; // 0x8717B0
        if (decay < m_fHeliRotorSpeed) {
            m_nFakePhysics = 0;
            m_fHeliRotorSpeed = (float)((double)m_fHeliRotorSpeed - decay);
        } else {
            m_fHeliRotorSpeed = 0.0f;
        }
    } else {
        // 0x6C4EF8 - Rotor spins up
        if (m_fHeliRotorSpeed < 0.22f && !physicalFlags.bSubmergedInWater) { // 0x8717AC
            m_fHeliRotorSpeed += isRCHeli() ? 0.003f : 0.001f; // 0x859CD8, 0x858CDC
        }

        if (m_fHeliRotorSpeed > 0.15f) { // 0x858FCC
            const auto isFloatingOnWater = physicalFlags.bTouchingWater && (m_nModelIndex == MODEL_SEASPAR || m_nModelIndex == MODEL_LEVIATHN);
            if (vehicleFlags.bIsRCVehicle) {
                FlyingControl(FLIGHT_MODEL_RC, m_fLeftRightSkid, m_fSteeringUpDown, m_fSteeringLeftRight, m_fAccelerationBreakStatus);
            } else if (   !(m_nNumContactWheels >= 4 || isFloatingOnWater)
                       || m_fAccelerationBreakStatus > 0.0 // 0x859EF8 (double 0.0)
                       || std::abs(m_vecMoveSpeed.x) > 0.02f // 0x858B38
                       || std::abs(m_vecMoveSpeed.y) > 0.02f
                       || std::abs(m_vecMoveSpeed.z) > 0.02f
            ) {
                FlyingControl(FLIGHT_MODEL_HELI, m_fLeftRightSkid, m_fSteeringUpDown, m_fSteeringLeftRight, m_fAccelerationBreakStatus);
            }
        }

        // 0x6C501D - Rotor blades
        if (m_fHeliRotorSpeed > 0.015f && m_aCarNodes[HELI_STATIC_ROTOR]) { // 0x8717A8
            auto* const rotorFrame = m_aCarNodes[HELI_STATIC_ROTOR];
            CMatrix rotorMat{ RwFrameGetMatrix(rotorFrame), false };

            RpAtomic* atomic = nullptr;
            RwFrameForAllObjects(rotorFrame, GetCurrentAtomicObjectCB, &atomic);
            if (atomic) {
                const float radius = RpAtomicGetBoundingSphere(atomic)->radius;
                if (radius > 0.1f) { // 0x858B1C
                    float damageMult = 1.0f;
                    if (isRCHeli()) {
                        damageMult = 0.9f;
                    } else if (m_nModelIndex == MODEL_SPARROW || m_nModelIndex == MODEL_SEASPAR) {
                        damageMult = 0.8f;
                    } else if (m_nModelIndex == MODEL_HUNTER) {
                        damageMult = 0.5f;
                    }
                    if (GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_REMOTE_CONTROLLED) {
                        DoBladeCollision(rotorMat.GetPosition(), GetMatrix(), -3, radius, damageMult); // 0x6C5135
                    }
                }
            }

            // 0x6C513A - Wind
            const auto statusNow = GetStatus();
            if ((statusNow == STATUS_PLAYER || statusNow == STATUS_PHYSICS) && m_fHeliRotorSpeed > 0.0075f) { // 0x8717A4
                const double power = (double)m_fHeliRotorSpeed * (double)6.666667f; // 0x866FBC
                CWindModifiers::RegisterOne(GetPosition(), 1, 1.0 < power ? 1.0f : (float)power);
            } else if (statusNow == STATUS_SIMPLE) {
                CWindModifiers::RegisterOne(GetPosition(), 1, 1.0f);
            }
        }
    }

    // 0x6C5200 - Blade sound
    if (   !isRCHeli()
        && m_fHeliRotorSpeed < 0.154f // 0x8717A0
        && m_fHeliRotorSpeed > 0.0044f // 0x87179C
        && m_aCarNodes[HELI_STATIC_ROTOR]
    ) {
        const auto& pos    = GetPosition();
        const auto& camPos = TheCamera.GetPosition();

        const float  dz  = camPos.z - pos.z;
        const float  dy  = camPos.y - pos.y;
        const double dxe = (double)camPos.x - (double)pos.x; // Not rounded to float on the x87 stack
        const float  dx  = (float)dxe;

        const double distSq = dxe * dx + (double)dy * dy + (double)dz * dz;
        if (distSq < 400.0 && std::abs((double)m_fPropRotate - (double)m_wheelRotation[1]) > (double)0.5235988f) { // 0x85A700, 0x858F20
            CMatrix rotorMat{ RwFrameGetMatrix(m_aCarNodes[HELI_STATIC_ROTOR]), false };
            // NOTSA: The original also constructs a second, never used, local CMatrix here

            const auto& right = rotorMat.GetRight();
            const auto& mat   = GetMatrix();
            const CVector bladeDir{
                (float)((double)mat.GetUp().x * right.z + (double)mat.GetForward().x * right.y + (double)mat.GetRight().x * right.x),
                (float)((double)mat.GetUp().y * right.z + (double)mat.GetRight().y * right.x + (double)mat.GetForward().y * right.y),
                (float)((double)mat.GetUp().z * right.z + (double)mat.GetRight().z * right.x + (double)mat.GetForward().z * right.y),
            };

            const double dist    = std::sqrt((double)(float)distSq);
            const double invDist = 1.0 / (dist < (double)0.01f ? (double)0.01f : dist); // 0x858C58 (clamp: min 0.01)
            const double dot     = (double)bladeDir.z * (invDist * dz) + (double)bladeDir.y * (invDist * dy) + (double)bladeDir.x * (dx * invDist);
            if (std::abs(dot) > (double)0.95f) { // 0x858EF0
                m_vehicleAudio.AddAudioEvent(AE_HELI_BLADE, 0.0f); // 0x6C53CB
                m_fPropRotate = m_wheelRotation[1];
            }
        }
    }
}

// 0x6C5420
void CHeli::PreRender() {
    CVehicle::PreRender(); // 0x6D6480

    const auto mi = GetVehicleModelInfo();
    CMatrix    rotorMat{}; // Re-attached to each of the rotor frames below

    // 0x6C545D - Search light
    if (m_bSearchLightEnabled && m_fSearchLightIntensity > 0.0f && CClock::GetIsTimeInRange(19, 6)) {
        const auto origin = GetMatrix().TransformPoint({ 0.0f, 3.5f, -0.3f }); // 0x59C890
        AddHeliSearchLight(
            origin,
            m_vecSearchLightTarget,
            20.0f,
            m_fSearchLightIntensity,
            reinterpret_cast<uint32>(this) + 11, // Corona index
            1,
            1
        );
    }

    // NOTSA: `GetColModel()` (0x535300) was called here, the result is unused

    // 0x6C5506 - Wheel positions
    if (vehicleFlags.bVehicleColProcessed) {
        DoBurstAndSoftGroundRatios();

        for (auto i = 0; i < 4; i++) {
            const double t = 1.0 - (double)m_aSuspensionSpringLength[i] / (double)m_aSuspensionLineLength[i];
            const float  v = (float)(((double)m_fWheelsSuspensionCompression[i] - t) / (1.0 - t));

            CVector wheelPos;
            mi->GetWheelPosn(i, wheelPos, true);

            double wheelZ = (double)wheelPos.z + (double)m_pHandlingData->m_fSuspensionUpperLimit;
            if (v > 0.0f) {
                wheelZ -= (double)v * (double)m_aSuspensionSpringLength[i];
            }

            const double curZ = m_wheelPosition[i];
            if (!(wheelZ > curZ)) {
                if (!physicalFlags.bAddMovingCollisionSpeed || !handlingFlags.bHydraulicInst) {
                    wheelZ = (wheelZ - curZ) * (double)0.75f + curZ; // 0x858F34
                }
            }
            m_wheelPosition[i] = (float)wheelZ;
        }
    }

    UpdateWheelMatrix(4, 1);
    UpdateWheelMatrix(7, 1);
    UpdateWheelMatrix(2, 1);
    UpdateWheelMatrix(5, 1);

    if (!(m_nModelIndex == MODEL_RCRAIDER || m_nModelIndex == MODEL_RCGOBLIN)) {
        DoHeliDustEffect(1.0f, 1.0f);
    }

    // 0x6C55E4 - Main rotor angle
    constexpr float TWO_PI_F = 2.0f * std::numbers::pi_v<float>; // 0x858CBC
    {
        const auto isBigRotor = m_nModelIndex == MODEL_SPARROW
                             || m_nModelIndex == MODEL_SEASPAR
                             || m_nModelIndex == MODEL_MAVERICK
                             || m_nModelIndex == MODEL_VCNMAV
                             || m_nModelIndex == MODEL_POLMAV;
        const double step = isBigRotor
            ? (double)CTimer::GetTimeStep() * (double)m_fHeliRotorSpeed * (double)1.66f // 0x8D33A0
            : (double)CTimer::GetTimeStep() * (double)m_fHeliRotorSpeed;
        m_fRotorZ = (float)((double)m_fRotorZ - step);
        if (m_fRotorZ < -TWO_PI_F) { // 0x863234
            double angle = (double)m_fRotorZ + (double)TWO_PI_F;
            while (angle < -(double)TWO_PI_F) {
                angle += (double)TWO_PI_F;
            }
            m_fRotorZ = (float)angle;
        }
    }

    // 0x6C568A - Second rotor angle
    {
        double step = (double)CTimer::GetTimeStep() * (double)m_fHeliRotorSpeed;
        if (m_nModelIndex == MODEL_LEVIATHN) {
            step = step + step;
        } else {
            step = step * (double)2.3f; // 0x858F54
        }
        m_fSecondRotorZ = (float)((double)m_fSecondRotorZ - step);
        if (m_fSecondRotorZ > TWO_PI_F) {
            double angle = m_fSecondRotorZ;
            do {
                angle -= (double)TWO_PI_F;
            } while (angle > (double)TWO_PI_F);
            m_fSecondRotorZ = (float)angle;
        }
    }

    // 0x6C56E9 - Apply the rotation to the rotor frames (keeping their position)
    const auto RotateRotor = [&](eHeliNodes node, float angle, bool aroundZ) {
        const auto frame = m_aCarNodes[node];
        if (!frame) {
            return;
        }
        rotorMat.Attach(RwFrameGetMatrix(frame), false);
        const auto pos = rotorMat.GetPosition();
        if (aroundZ) {
            rotorMat.SetRotateZ(angle);
        } else {
            rotorMat.SetRotateX(angle);
        }
        rotorMat.GetPosition().x += pos.x;
        rotorMat.GetPosition().y += pos.y;
        rotorMat.GetPosition().z += pos.z;
        rotorMat.UpdateRW();
    };
    RotateRotor(HELI_STATIC_ROTOR,  m_fRotorZ,       true);
    RotateRotor(HELI_MOVING_ROTOR,  m_fRotorZ,       true);
    RotateRotor(HELI_STATIC_ROTOR2, m_fSecondRotorZ, false);
    RotateRotor(HELI_MOVING_ROTOR2, m_fSecondRotorZ, false);

    CShadows::StoreShadowForVehicle(this, VEH_SHD_HELI); // 0x6C589D
}

// 0x6C7050
void CHeli::ProcessControl() {
    CAutomobile::ProcessControl(); // 0x6B1880

    if (!vehicleFlags.bEngineOn && m_pDustParticle) {
        m_pDustParticle->Kill();
        m_pDustParticle       = nullptr;
        m_heliDustFxTimeConst = 0.0f;
    }

    // 0x6C7085 - Toggle the search light
    if (CPad::GetPad(m_pDriver && m_pDriver->m_nPedType == PED_TYPE_PLAYER2 ? 1 : 0)->HornJustDown()) {
        m_bSearchLightEnabled = !m_bSearchLightEnabled;
    }

    bool     searchLightOn = false; // 0x6C70C7
    bool     shootAtTarget = false;
    CEntity* target        = nullptr;

    if (physicalFlags.bRenderScorched || CCullZones::PlayerNoRain()) {
        m_fSearchLightIntensity = 0.0f; // 0x6C77E6
    } else {
        if (   m_autoPilot.m_nCarMission == MISSION_HELI_POLICE_BEHAVIOUR
            && (   !FindPlayerVehicle()
                || (FindPlayerVehicle()->m_nVehicleSubType != VEHICLE_TYPE_HELI && FindPlayerVehicle()->m_nVehicleSubType != VEHICLE_TYPE_PLANE)
            )
        ) {
            searchLightOn = true;
            shootAtTarget = true;
            target        = FindPlayerEntity();
        } else if (m_autoPilot.m_nCarMission == MISSION_HELI_FOLLOW_ENTITY && m_autoPilot.m_TargetEntity && (m_nHeliFlags & 2)) {
            searchLightOn = true;
            target        = m_autoPilot.m_TargetEntity;
        } else if (GetStatus() == STATUS_PLAYER && m_nModelIndex == MODEL_POLMAV && m_bSearchLightEnabled) {
            searchLightOn = true;
        }

        if (physicalFlags.bSubmergedInWater) { // 0x6C7195
            searchLightOn = false;
            shootAtTarget = false;
        }

        m_bSearchLightEnabled = searchLightOn;

        if (searchLightOn) { // 0x6C71AB
            // Position and speed of whatever the light is following
            CVector targetPos;
            CVector targetVel;
            if (target) {
                targetPos = target->GetPosition();
                targetVel = static_cast<CPhysical*>(target)->m_vecMoveSpeed;
            } else {
                // Look at the ground in front of the heli
                const auto up = GetUpVector(); // 0x50E420
                const CVector offset{ up.x * -30.0f, up.y * -30.0f, up.z * -30.0f }; // 0x859CE4

                const auto  fwd = GetForwardVector(); // 0x41CCB0
                const auto& pos = GetPosition();
                targetPos.x = (float)((double)fwd.x * 10.0 + (double)pos.x + (double)offset.x); // 0x85862C
                targetPos.y = (float)((double)(float)(fwd.y * 10.0f) + (double)pos.y + (double)offset.y);
                targetPos.z = (float)((double)(float)((float)(fwd.z * 10.0f) + pos.z) + (double)offset.z);
                targetVel   = m_vecMoveSpeed;
            }

            // 0x6C72AE - Record the (predicted) target position once per second
            const uint32 now = CTimer::GetTimeInMS();
            int32        timeSinceLastRecord = (int32)(now - m_nSearchLightTimer);
            if (timeSinceLastRecord > 1000) {
                const double fx = (double)targetVel.x * 100.0; // 0x858628
                const double fy = (double)targetVel.y * 100.0;

                int32 numRecords = (int32)((uint32)(timeSinceLastRecord - 1001) / 1000u) + 1;
                timeSinceLastRecord -= numRecords * 1000;
                do {
                    for (auto i = (int32)m_aSearchLightHistoryX.size() - 1; i > 0; i--) {
                        m_aSearchLightHistoryX[i] = m_aSearchLightHistoryX[i - 1];
                        m_aSearchLightHistoryY[i] = m_aSearchLightHistoryY[i - 1];
                    }
                    m_nSearchLightTimer += 1000;
                    m_aSearchLightHistoryX[0] = (float)(fx + (double)targetPos.x);
                    m_aSearchLightHistoryY[0] = (float)((double)targetPos.y + fy);
                } while (--numRecords != 0);
            }

            // 0x6C7352 - Interpolate between the recorded positions
            const double blend    = (double)timeSinceLastRecord * (double)0.001f; // 0x858CDC
            const double blendInv = 1.0 - blend;
            m_vecSearchLightTarget.z = targetPos.z;
            const float  curX = (float)(blend * m_aSearchLightHistoryX[1] + blendInv * m_aSearchLightHistoryX[2]);
            m_vecSearchLightTarget.x = curX;
            const double curY = blend * m_aSearchLightHistoryY[1] + blendInv * m_aSearchLightHistoryY[2];
            m_vecSearchLightTarget.y = (float)curY;

            // 0x6C73A8 - Light intensity falls off with the distance
            {
                const auto& pos = GetPosition();
                const double dy   = curY - (double)pos.y;
                const double dx   = (double)curX - (double)pos.x; // Not rounded to float on the x87 stack
                const double dist = std::sqrt(dy * dy + dx * dx);
                if (dist > (double)60.0f) { // 0x858B34
                    m_fSearchLightIntensity = 0.0f;
                } else {
                    const float distF = (float)dist;
                    if (distF < 40.0f) { // 0x858A10
                        m_fSearchLightIntensity = 1.0f;
                    } else {
                        m_fSearchLightIntensity = (float)(1.0 - ((double)distF - (double)40.0f) * (double)0.05f); // 0x858C28
                    }
                }
            }

            const float  dxToTarget = (float)((double)targetPos.x - (double)curX);
            const double dyToTarget = (double)targetPos.y - curY;
            if (m_fSearchLightIntensity < 0.9f || dyToTarget * dyToTarget + (double)dxToTarget * dxToTarget > 49.0) { // 0x858C20, 0x8717C4
                m_nShootTimer             = now;
                m_nTimeForMinigunFiring   = now;
            } else if (now > m_nPoliceShoutTimer) {
                m_nPoliceShoutTimer = (uint32)(rand() & 0xFFF) + 4500 + now;
            }

            // 0x6C74B7 - Police heli shooting at the target
            if (shootAtTarget) {
                int32 interval = 0;
                switch ((uint32)FindPlayerPed()->GetPlayerWanted()->m_WantedLevel) { // 0x6C74E5
                case 0:
                case 1:
                case 2: interval = 999999; break;
                case 3: interval = 10000; break;
                case 4: interval = 5000; break;
                case 5: interval = 3500; break;
                case 6: interval = 2000; break;
                default: NOTSA_UNREACHABLE("Invalid wanted level"); // The original reads a leftover stack value here (can't happen, max wanted level is 6)
                }

                if (FindPlayerPed()->GetPlayerWanted()->m_WantedLevel != eWantedLevel::WANTED_CLEAN) {
                    AudioEngine.SayPedless(AE_SPEECH_PED, CTX_GLOBAL_POLICE_HELICOPTER, this, 0, 1.0f, false, false, false); // 0x6C7547
                }

                if (CCullZones::NoPolice()) {
                    interval /= 2;
                }

                if (target != FindPlayerPed()) {
                    interval = 5000;
                }

                if (FindPlayerWanted()->PoliceBackOff()) { // 0x6C7585
                    m_nShootTimer           = CTimer::GetTimeInMS();
                    m_nTimeForMinigunFiring = CTimer::GetTimeInMS();
                } else {
                    const auto origin = GetMatrix().TransformPoint({ 0.0f, 3.5f, -1.0f }); // 0x59C890

                    const uint32 shootTime = m_nShootTimer + (uint32)interval;
                    if (CTimer::GetTimeInMS() > shootTime && CTimer::GetPreviousTimeInMS() <= shootTime) {
                        if (!CWorld::GetIsLineOfSightClear(origin, targetPos, true, false, false, false, false, false, false)) {
                            m_nShootTimer           = CTimer::GetTimeInMS();
                            m_nTimeForMinigunFiring = CTimer::GetTimeInMS();
                        }
                    }

                    if (CTimer::GetTimeInMS() > m_nShootTimer + (uint32)interval && CTimer::GetTimeInMS() > m_nTimeForMinigunFiring) { // 0x6C760F
                        CVector shotTarget = targetPos;
                        shotTarget.x = (float)((double)((rand() & 0xFF) - 0x80) * (double)0.02f + (double)shotTarget.x); // 0x858B38
                        shotTarget.y = (float)((double)((rand() & 0xFF) - 0x80) * (double)0.02f + (double)shotTarget.y);

                        CVector dir{
                            targetPos.x - origin.x,
                            targetPos.y - origin.y,
                            targetPos.z - origin.z,
                        };
                        dir.Normalise();

                        // 3.0f = 0x858B3C
                        const float  dx3f   = (float)((double)dir.x * 3.0);
                        const double dy3    = (double)dir.y * 3.0;
                        const double dz3    = (double)dir.z * 3.0;
                        const float  dz3f   = (float)dz3;
                        shotTarget.x = (float)((double)dx3f + (double)shotTarget.x);
                        shotTarget.y = (float)(dy3 + (double)shotTarget.y);
                        shotTarget.z = (float)((double)shotTarget.z + dz3);

                        const CVector start{
                            (float)((double)dx3f + (double)origin.x),
                            (float)((double)origin.y + dy3),
                            (float)((double)origin.z + (double)dz3f),
                        };

                        FireOneInstantHitRound(start, shotTarget, 20); // 0x6C7773
                        AudioEngine.ReportWeaponEvent(AE_WEAPON_FIRE, WEAPON_M4, this); // 0x6C7788

                        m_nTimeForMinigunFiring = CTimer::GetTimeInMS() + (CGeneral::GetRandomNumberInRange(0.0f, 1.0f) >= 0.15f ? 150u : 400u); // 0x8D33A4
                    }
                }
            }
        }
    }

    // 0x6C77EC - Dropping the SWAT team
    if (m_autoPilot.m_nCarMission == MISSION_HELI_POLICE_BEHAVIOUR && m_nNumSwatOccupants != 0) {
        SendDownSwat();
        g_InterestingEvents.Add(CInterestingEvents::ZELDICK_OCCUPATION, this);
    }

    for (auto i = 0; i < (int32)m_aSwatState.size(); i++) { // 0x6C7824
        auto& state = m_aSwatState[i];
        if (state == 0) {
            continue;
        }

        state--;

        const auto ropeId = reinterpret_cast<uint32>(this) + i; // The rope is identified by `this + i`
        const auto swatOffset = FindSwatPositionRelativeToHeli(i);
        CRopes::RegisterRope(ropeId, 8, GetMatrix().TransformPoint(swatOffset), false, 0, 0, nullptr, 20000);

        if (state == 0) {
            const auto swatOffset2 = FindSwatPositionRelativeToHeli(i);
            const CVector v{ swatOffset2.x * 0.05f, swatOffset2.y * 0.05f, swatOffset2.z * 0.05f }; // 0x858C28
            const auto& mat = GetMatrix();
            // 0x59C790 (Multiply3x3)
            const auto rotated = CVector{
                (float)((double)mat.GetUp().x * v.z + (double)mat.GetForward().x * v.y + (double)mat.GetRight().x * v.x),
                (float)((double)mat.GetUp().y * v.z + (double)mat.GetRight().y * v.x + (double)mat.GetForward().y * v.y),
                0.0f, // z isn't used
            };
            CRopes::SetSpeedOfTopNode(ropeId, rotated);
        }
    }

    UpdateWinch();
    ProcessWeapons();

    if (g_InterestingEvents.m_b1) { // 0xC0B184
        float chance = (float)((double)CTimer::GetTimeStep() * (double)0.02f * (double)0.1f); // 0x858B38, 0x858B1C
        if (shootAtTarget) {
            chance += chance;
        }
        if ((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL < (double)chance) { // 0x858C7C
            g_InterestingEvents.Add(CInterestingEvents::INTERESTING_EVENT_21, this);
        }
    }
}
