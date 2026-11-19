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
            particle->m_RotZ = (uint8)notsa::detail::Ftol((x87d)rotZ * (x87d)0.5); // 0x858B8C
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
            particle->m_RotZ = (uint8)notsa::detail::Ftol((x87d)rotZ * (x87d)0.5); // 0x858B8C
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

    const x87d rateMult = (x87d)m_System->m_nRateMult * 0.001f;                    // 0x858CDC
    const float  lodStart = (float)((x87d)m_PrimBP->m_FxInfoManager.m_nLodStart * 0.015625f); // 0x85A7D4 (1/64)
    const float  lodEnd   = (float)((x87d)m_PrimBP->m_FxInfoManager.m_nLodEnd * 0.015625f);

    const float camDist = m_System->m_fCameraDistance;
    x87d      visibility;
    if (!(camDist < lodStart)) {
        if (!(camDist > lodEnd)) {
            visibility = (x87d)1.0 - ((x87d)camDist - lodStart) / ((x87d)lodEnd - lodStart);
        } else {
            visibility = (x87d)0.0;
        }
    } else {
        visibility = (x87d)1.0;
    }

    m_fEmissionIntensity = (float)((x87d)emission.m_fCount * visibility * rateMult + m_fEmissionIntensity);

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
            const float timeSince = (float)((x87d)i / m_fEmissionIntensity * deltaTime);
            CreateParticle(emission, *finalMat, nullptr, timeSince, prtMult, 1.2f, m_System->m_createLocal);
        }
        m_fEmissionIntensity = (float)((x87d)m_fEmissionIntensity - (x87d)notsa::detail::Ftol(m_fEmissionIntensity));

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

    // 0x4A2599..0x4A25BF: no free particle and the system need not create one, or none left after FreeUpParticle -> the exe returns null
    // (the lambda above only left the lambda: the old code went on to write through the null pointer)
    if (!particle) {
        return nullptr;
    }

    // exe 0x4A25C7..0x4A2AD0: EVERY random draw of this function is `(rand() % 10000)` (0x821B1E, idiv 10000) times a float constant, never the 1/32767
    // range helpers: 1e-4f (0x858FC4 = 0x38D1B717) for [0,1) and 2e-4f (0x85A7D0, = 1/5000) for [-1,1). Draw order = the order below.
    const auto Rnd = [] { return (x87d)(rand() % 10000); };
    constexpr x87d K1E4 = (x87d)std::bit_cast<float>(0x38D1B717u); // 0x858FC4
    constexpr x87d K2E4 = (x87d)ExeRecip(5000.0f);                // 0x85A7D0

    particle->m_fCurrentLife = 0.0f;
    particle->m_fTotalLife = (float)(((Rnd() * K2E4 - (x87d)1.0) * (x87d)emissionInfo.m_fLifeBias + (x87d)emissionInfo.m_fLife) * (x87d)fxMults.m_fLife); // 0x4A25C7
    particle->m_System = m_System;

    particle->m_MultColor = CRGBA{fxMults.m_Color};

    // NOTSA: Fields are raw bytes (the original stores `(uint8)(value * 255)` / `(uint8)(brightness * 100)`)
    particle->m_MultSize = (uint8)NOTSA_FTOL(int32, fxMults.m_fSize * 255.0f);
    particle->m_MultRot  = (uint8)NOTSA_FTOL(int32, fxMults.m_Rot * 255.0f);

    particle->m_bLocalToSystem = createLocal;

    particle->m_RandR = NOTSA_FTOL(uint8, Rnd() * K1E4 * (x87d)255.0f); // 0x4A2675: `fild; fmul 1e-4f; fmul 255.0f (0x859AAC); _ftol2` (low byte)
    particle->m_RandG = NOTSA_FTOL(uint8, Rnd() * K1E4 * (x87d)255.0f);
    particle->m_RandB = NOTSA_FTOL(uint8, Rnd() * K1E4 * (x87d)255.0f);
    particle->m_Brightness = (uint8)NOTSA_FTOL(int32, brightness * 100.0f);

    particle->m_RotZ = 0xFF;
    particle->m_CurrentRotation = (float)(Rnd() * K1E4 * ((x87d)emissionInfo.m_fRotationMaxAngle - (x87d)emissionInfo.m_fRotationMinAngle) + (x87d)emissionInfo.m_fRotationMinAngle); // 0x4A2702

    if (createLocal) {
        m_PrimBP->GetRWMatrix(wldMat);
    }

    CVector vec;
    // 0x4A2743: `fabs(radius) < (double)0.001f` (0x85A7C8, `test ah, 1; jne`): also true for NaN (unordered sets C0)
    if (!(std::fabs(emissionInfo.m_fRadius) >= 0.001f)) {
        // 0x4A283C: x, y, z in draw order, lerp `(max - min) * t + min`
        vec.x = (float)(Rnd() * K1E4 * ((x87d)emissionInfo.m_SizeMax.x - (x87d)emissionInfo.m_SizeMin.x) + (x87d)emissionInfo.m_SizeMin.x);
        vec.y = (float)(Rnd() * K1E4 * ((x87d)emissionInfo.m_SizeMax.y - (x87d)emissionInfo.m_SizeMin.y) + (x87d)emissionInfo.m_SizeMin.y);
        vec.z = (float)(Rnd() * K1E4 * ((x87d)emissionInfo.m_SizeMax.z - (x87d)emissionInfo.m_SizeMin.z) + (x87d)emissionInfo.m_SizeMin.z);
    } else {
        // 0x4A2759: three draws in [-1, 1), spilled to floats; |v|^2 summed z, y, x (fsqrt, 1.0 / len), then (only for radius >= 0 / NaN) a 4th draw
        const float a = (float)(Rnd() * K2E4 - (x87d)1.0);
        const float b = (float)(Rnd() * K2E4 - (x87d)1.0);
        const float c = (float)(Rnd() * K2E4 - (x87d)1.0);
        const x87d  sum = ((x87d)c * (x87d)c + (x87d)b * (x87d)b) + (x87d)a * (x87d)a;
        x87d        scale = (x87d)1.0 / std::sqrt(sum); // 0x85A310 = 1.0 (double)
        if (!(emissionInfo.m_fRadius < 0.0f)) { // 0x4A27FA: `jnp` skips the draw only for radius < 0
            const float invDist = (float)scale; // fstp dword [esp+4C]
            scale = Rnd() * K1E4 * (x87d)invDist;
        }
        scale = scale * (x87d)emissionInfo.m_fRadius;
        vec.x = (float)((x87d)a * scale);
        vec.y = (float)((x87d)b * scale);
        vec.z = (float)((x87d)c * scale);
    }
    vec += emissionInfo.m_Pos;

    particle->m_Pos = vec.z * wldMat.at + vec.y * wldMat.up + vec.x * wldMat.right + wldMat.pos;

    if (velOverride) {
        particle->m_Velocity = *velOverride;
    } else {
        // exe (0x4A2925..0x4A298D): `rand() % 10000 * 0.0001f` (0x858FC4) scaled by its OWN constants 6.2831802f (0x85A7C0) and 0.0174532793f (0x85A7BC);
        // the lerp is `(max - min) * t + min`, angles are spilled to floats
        const auto Rand01 = [] { return (x87d)(rand() % 10000) * (x87d)std::bit_cast<float>(0x38D1B717u); };
        const auto randomAngle = (float)(X87R(Rand01() * (x87d)std::bit_cast<float>(0x40C90FD0u)));
        const auto minAngle    = (float)((x87d)emissionInfo.m_fAngleMin * (x87d)std::bit_cast<float>(0x3C8EFA2Eu));
        const auto randomAngleBetweenMinMax = (float)(X87R(((x87d)emissionInfo.m_fAngleMax * (x87d)std::bit_cast<float>(0x3C8EFA2Eu) - (x87d)minAngle) * Rand01()) + (x87d)minAngle);

        CVector randomizedAngleVec{
            CMaths::GetCosFast(randomAngle) * CMaths::GetSinFast(randomAngleBetweenMinMax),
            CMaths::GetCosFast(randomAngleBetweenMinMax),
            CMaths::GetSinFast(randomAngle) * CMaths::GetSinFast(randomAngleBetweenMinMax)
        };

        CVector vectorsIn;
        CVector vectorsOut;
        if (!(emissionInfo.m_Direction.x > 10.0f)) { // 0x4A2A16: `fcomp 10.0f; test ah, 0x41; jne` => x <= 10 or NaN
            vectorsIn = emissionInfo.m_Direction;
            RwV3dNormalize(&vectorsIn, &vectorsIn); // 0x4A2A59: RW's table-based 0x7ED9B0, not CVector::Normalise
        } else {
            vectorsIn = particle->m_Pos;
        }
        RwV3dTransformVectors(&vectorsOut, &vectorsIn, 1, &wldMat);
        CVector v38;
        RotateVecIntoVec(v38, randomizedAngleVec, vectorsOut);
        const x87d speedMul = (Rnd() * K2E4 - (x87d)1.0) * (x87d)emissionInfo.m_fSpeedBias + (x87d)emissionInfo.m_Speed; // 0x4A2A8C
        particle->m_Velocity.x = (float)((x87d)v38.x * speedMul);
        particle->m_Velocity.y = (float)((x87d)v38.y * speedMul);
        particle->m_Velocity.z = (float)((x87d)v38.z * speedMul);
    }

    particle->m_Velocity += m_System->m_VelAdd;
    static_cast<FxEmitterBP_c*>(m_PrimBP)->UpdateParticle(timeSince, particle);
    m_PrimBP->m_Particles.AddItem(particle);

    return particle;
}
