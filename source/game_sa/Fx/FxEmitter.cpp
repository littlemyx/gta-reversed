#include "StdInc.h"

#include "FxEmitter.h"
#include "EmissionInfo.h"
#include "FxEmitterPrt.h"
#include "FxPrimBP.h"
#include "FxEmitterBP.h"
#include "FxEmitterPrt.h"
#include "FxFtol.h"

void FxEmitter_c::InjectHooks() {
    RH_ScopedVirtualClass(FxEmitter_c, 0x85A7A4, 6);
    RH_ScopedCategory("Fx");

    // RH_ScopedInstall(Init_Reversed, 0x4A2550);
    // RH_ScopedInstall(Update_Reversed, 0x4A4460); // bad
    // RH_ScopedInstall(Reset_Reversed, 0x4A2570);
    RH_ScopedVMTOverloadedInstall(AddParticle, "Vec", 0x4A3EA0, void(FxEmitter_c::*)(const CVector&, const CVector&, float, const FxPrtMult_c&, float, float, bool));
    RH_ScopedVMTOverloadedInstall(AddParticle, "Mat", 0x4A4050, void(FxEmitter_c::*)(const RwMatrix&, const CVector&, float, const FxPrtMult_c&, float, float, bool));
    RH_ScopedInstall(CreateParticles, 0x4A41E0);
    // RH_ScopedInstall(CreateParticle, 0x4A2580);
}


// 0x4A2550
bool FxEmitter_c::Init(FxPrimBP_c* primBP, FxSystem_c* system) {
    m_PrimBP             = primBP;
    m_System             = system;
    m_fEmissionIntensity = 0.0f;
    return true;
}

// 0x4A4460
void FxEmitter_c::Update(float currentTime, float deltaTime) {
    if (m_bEnabled && !m_System->m_stopParticleCreation) {
        CreateParticles(currentTime, deltaTime);
    }
}

// 0x4A2570
void FxEmitter_c::Reset() {
    m_fEmissionIntensity = 0.0f;
}

// 0x4A3EA0
void FxEmitter_c::AddParticle(const CVector& pos, const CVector& vel, float timeSince, const FxPrtMult_c& fxMults, float rotZ, float brightness, bool createLocal) {
    EmissionInfo_t emission;
    m_PrimBP->m_FxInfoManager.ProcessEmissionInfo(0.0f, 0.0f, m_System->m_SystemBP->m_fLength, m_System->m_UseConstTime, &emission);

    auto* const finalMat  = g_fxMan.FxRwMatrixCreate();
    auto* const worldMat  = g_fxMan.FxRwMatrixCreate();
    auto* const posMat    = g_fxMan.FxRwMatrixCreate();

    // Identity + position (done by hand in the original, the flags are OR'd in)
    posMat->right = {1.0f, 0.0f, 0.0f};
    posMat->up    = {0.0f, 1.0f, 0.0f};
    posMat->at    = {0.0f, 0.0f, 1.0f};
    posMat->pos   = {0.0f, 0.0f, 0.0f};
    posMat->flags |= 0x20003;
    posMat->pos   = {pos.x, pos.y, pos.z};
    RwMatrixUpdate(posMat);

    if (m_System->m_ParentMatrix) {
        RwMatrixMultiply(worldMat, posMat, m_System->m_ParentMatrix);
    } else {
        *worldMat = *posMat;
    }

    auto* const primMat = g_fxMan.FxRwMatrixCreate();
    m_PrimBP->GetRWMatrix(*primMat);
    RwMatrixMultiply(finalMat, primMat, worldMat);
    g_fxMan.FxRwMatrixDestroy(primMat);

    if (auto* const particle = CreateParticle(emission, *finalMat, &vel, timeSince, fxMults, brightness, createLocal)) {
        if (rotZ >= 0.0f) {
            particle->m_RotZ = (uint8)notsa::detail::Ftol((double)rotZ * 0.5); // 0x858B8C
        }
    }

    g_fxMan.FxRwMatrixDestroy(posMat);
    g_fxMan.FxRwMatrixDestroy(worldMat);
    g_fxMan.FxRwMatrixDestroy(finalMat);
}

// 0x4A4050
void FxEmitter_c::AddParticle(const RwMatrix& mat, const CVector& vel, float timeSince, const FxPrtMult_c& fxMults, float rotZ, float brightness, bool createLocal) {
    EmissionInfo_t emission;
    m_PrimBP->m_FxInfoManager.ProcessEmissionInfo(0.0f, 0.0f, m_System->m_SystemBP->m_fLength, m_System->m_UseConstTime, &emission);

    auto* const finalMat = g_fxMan.FxRwMatrixCreate();
    auto* const worldMat = g_fxMan.FxRwMatrixCreate();
    auto* const inMat    = g_fxMan.FxRwMatrixCreate();

    // The original first sets this matrix to identity, but then overwrites all 16 dwords with `mat`
    *inMat = mat;
    RwMatrixUpdate(inMat);

    if (m_System->m_ParentMatrix) {
        RwMatrixMultiply(worldMat, inMat, m_System->m_ParentMatrix);
    } else {
        *worldMat = *inMat;
    }

    auto* const primMat = g_fxMan.FxRwMatrixCreate();
    m_PrimBP->GetRWMatrix(*primMat);
    RwMatrixMultiply(finalMat, primMat, worldMat);
    g_fxMan.FxRwMatrixDestroy(primMat);

    if (auto* const particle = CreateParticle(emission, *finalMat, &vel, timeSince, fxMults, brightness, createLocal)) {
        if (rotZ >= 0.0f) {
            particle->m_RotZ = (uint8)notsa::detail::Ftol((double)rotZ * 0.5); // 0x858B8C
        }
    }

    g_fxMan.FxRwMatrixDestroy(inMat);
    g_fxMan.FxRwMatrixDestroy(worldMat);
    g_fxMan.FxRwMatrixDestroy(finalMat);
}

// 0x4A41E0
void FxEmitter_c::CreateParticles(float currentTime, float deltaTime) {
    EmissionInfo_t emission;
    m_PrimBP->m_FxInfoManager.ProcessEmissionInfo(currentTime, deltaTime, m_System->m_SystemBP->m_fLength, m_System->m_UseConstTime, &emission);

    const double rateMult = (double)m_System->m_nRateMult * 0.001f;                    // 0x858CDC
    const float  lodStart = (float)((double)m_PrimBP->m_FxInfoManager.m_nLodStart * 0.015625f); // 0x85A7D4 (1/64)
    const float  lodEnd   = (float)((double)m_PrimBP->m_FxInfoManager.m_nLodEnd * 0.015625f);

    const float camDist = m_System->m_fCameraDistance;
    double      visibility;
    if (!(camDist < lodStart)) {
        if (!(camDist > lodEnd)) {
            visibility = 1.0 - ((double)camDist - lodStart) / ((double)lodEnd - lodStart);
        } else {
            visibility = 0.0;
        }
    } else {
        visibility = 1.0;
    }

    m_fEmissionIntensity = (float)((double)emission.m_fCount * visibility * rateMult + m_fEmissionIntensity);

    // NaN semantics: the original only bails out on an ordered "less than" / "greater than"
    if (   !(CWeather::Wind < emission.m_fMinWind)
        && !(CWeather::Wind > emission.m_fMaxWind)
        && !(CWeather::Rain < emission.m_fMinRain)
        && !(CWeather::Rain > emission.m_fMaxRain)
        && !(m_fEmissionIntensity < 1.0f)
    ) {
        auto* const finalMat = g_fxMan.FxRwMatrixCreate();
        auto* const worldMat = g_fxMan.FxRwMatrixCreate();

        RwMatrixUpdate(&m_System->m_LocalMatrix);
        if (m_System->m_ParentMatrix) {
            RwMatrixMultiply(worldMat, &m_System->m_LocalMatrix, m_System->m_ParentMatrix);
        } else {
            *worldMat = m_System->m_LocalMatrix;
        }

        auto* const primMat = g_fxMan.FxRwMatrixCreate();
        m_PrimBP->GetRWMatrix(*primMat);
        RwMatrixMultiply(finalMat, primMat, worldMat);
        g_fxMan.FxRwMatrixDestroy(primMat);

        // The intensity is re-read (and re-truncated) on every iteration, but it doesn't change in the loop
        for (int32 i = 0; i < notsa::detail::Ftol(m_fEmissionIntensity); i++) {
            FxPrtMult_c prtMult;
            const float timeSince = (float)((double)i / m_fEmissionIntensity * deltaTime);
            CreateParticle(emission, *finalMat, nullptr, timeSince, prtMult, 1.2f, m_System->m_createLocal);
        }
        m_fEmissionIntensity = (float)((double)m_fEmissionIntensity - (double)notsa::detail::Ftol(m_fEmissionIntensity));

        g_fxMan.FxRwMatrixDestroy(worldMat);
        g_fxMan.FxRwMatrixDestroy(finalMat);
    }
}

// 0x4A2580
FxEmitterPrt_c* FxEmitter_c::CreateParticle(const EmissionInfo_t& emissionInfo, RwMatrix& wldMat, const CVector* velOverride, float timeSince, const FxPrtMult_c& fxMults, float brightness, bool createLocal) {
    //return plugin::CallMethodAndReturn<FxEmitterPrt_c*, 0x4A2580, FxEmitter_c*, EmissionInfo_t*, RwMatrix*, CVector*, float, FxPrtMult_c*, float, bool>(this, &emissionInfo, &wldMat, velOverride, timeSince, &fxMults, brightness, createLocal);

    // todo:
    auto* particle = [&]() -> FxEmitterPrt_c* {
        auto* prt = g_fxMan.GetParticle(0);
        if (!prt) {
            if (!m_System->m_MustCreateParticles)
                return nullptr;

            g_fxMan.FreeUpParticle();
            prt = g_fxMan.GetParticle(0);
        }
        return reinterpret_cast<FxEmitterPrt_c*>(prt);
    }();

    particle->m_fCurrentLife = 0.0f;
    particle->m_fTotalLife = (((float)(CGeneral::GetRandomNumber() % 10'000) / 5'000.0f - 1.0f) * emissionInfo.m_fLifeBias + emissionInfo.m_fLife) * fxMults.m_fLife;
    particle->m_System = m_System;

    particle->m_MultColor = CRGBA{fxMults.m_Color};

    // NOTSA: Fields are raw bytes (the original stores `(uint8)(value * 255)` / `(uint8)(brightness * 100)`)
    particle->m_MultSize = (uint8)(int32)(fxMults.m_fSize * 255.0f);
    particle->m_MultRot  = (uint8)(int32)(fxMults.m_Rot * 255.0f);

    particle->m_bLocalToSystem = createLocal;

    particle->m_RandR = CGeneral::GetRandomNumberInRange(0, 256);
    particle->m_RandG = CGeneral::GetRandomNumberInRange(0, 256);
    particle->m_RandB = CGeneral::GetRandomNumberInRange(0, 256);
    particle->m_Brightness = (uint8)(int32)(brightness * 100.0f);

    particle->m_RotZ = 0xFF;
    particle->m_CurrentRotation = CGeneral::GetRandomNumberInRange(0.0f, 1.0f) * (emissionInfo.m_fRotationMaxAngle - emissionInfo.m_fRotationMinAngle) + emissionInfo.m_fRotationMinAngle;

    if (createLocal) {
        m_PrimBP->GetRWMatrix(wldMat);
    }

    CVector vec;
    if (approxEqual(emissionInfo.m_fRadius, 0.0f, 0.001f)) {
        vec = CVector{
            CGeneral::GetRandomNumberInRange(0.0f, 1.0f) * (emissionInfo.m_SizeMax.x - emissionInfo.m_SizeMin.x) + emissionInfo.m_SizeMin.x,
            CGeneral::GetRandomNumberInRange(0.0f, 1.0f) * (emissionInfo.m_SizeMax.y - emissionInfo.m_SizeMin.y) + emissionInfo.m_SizeMin.y,
            CGeneral::GetRandomNumberInRange(0.0f, 1.0f) * (emissionInfo.m_SizeMax.z - emissionInfo.m_SizeMin.z) + emissionInfo.m_SizeMin.z,
        };
    } else {
        vec = CVector{
            CGeneral::GetRandomNumberInRange(0.0f, 2.0f) - 1.0f,
            CGeneral::GetRandomNumberInRange(0.0f, 2.0f) - 1.0f,
            CGeneral::GetRandomNumberInRange(0.0f, 2.0f) - 1.0f,
        };

        float radius;
        auto invDist = 1.0f / vec.Magnitude();
        if (emissionInfo.m_fRadius < 0.0f) { // todo: check comp.
            radius = invDist * emissionInfo.m_fRadius;
        } else {
            radius = (invDist * CGeneral::GetRandomNumberInRange(0.0f, 1.0f)) * emissionInfo.m_fRadius;
        }
        vec *= radius;
    }
    vec += emissionInfo.m_Pos;

    particle->m_Pos = vec.z * wldMat.at + vec.y * wldMat.up + vec.x * wldMat.right + wldMat.pos;

    if (velOverride) {
        particle->m_Velocity = *velOverride;
    } else {
        // exe (0x4A2925..0x4A298D): `rand() % 10000 * 0.0001f` (0x858FC4) scaled by its OWN constants 6.2831802f (0x85A7C0) and 0.0174532793f (0x85A7BC);
        // the lerp is `(max - min) * t + min`, angles are spilled to floats
        const auto Rand01 = [] { return (double)(rand() % 10000) * (double)std::bit_cast<float>(0x38D1B717u); };
        const auto randomAngle = (float)(Rand01() * (double)std::bit_cast<float>(0x40C90FD0u));
        const auto minAngle    = (float)((double)emissionInfo.m_fAngleMin * (double)std::bit_cast<float>(0x3C8EFA2Eu));
        const auto randomAngleBetweenMinMax = (float)(((double)emissionInfo.m_fAngleMax * (double)std::bit_cast<float>(0x3C8EFA2Eu) - (double)minAngle) * Rand01() + (double)minAngle);

        CVector randomizedAngleVec{
            CMaths::GetCosFast(randomAngle) * CMaths::GetSinFast(randomAngleBetweenMinMax),
            CMaths::GetCosFast(randomAngleBetweenMinMax),
            CMaths::GetSinFast(randomAngle) * CMaths::GetSinFast(randomAngleBetweenMinMax)
        };

        CVector vectorsIn;
        CVector vectorsOut;
        if (emissionInfo.m_Direction.x <= 10.0f) {
            vectorsIn = emissionInfo.m_Direction;
            vectorsIn.Normalise();
        } else {
            vectorsIn = particle->m_Pos;
        }
        RwV3dTransformVectors(&vectorsOut, &vectorsIn, 1, &wldMat);
        CVector v38;
        RotateVecIntoVec(v38, randomizedAngleVec, vectorsOut);
        particle->m_Velocity = v38 * ((CGeneral::GetRandomNumberInRange(0.0f, 2.0f) - 1.0f) * emissionInfo.m_fSpeedBias + emissionInfo.m_Speed);
    }

    particle->m_Velocity += m_System->m_VelAdd;
    static_cast<FxEmitterBP_c*>(m_PrimBP)->UpdateParticle(timeSince, particle);
    m_PrimBP->m_Particles.AddItem(particle);

    return particle;
}
