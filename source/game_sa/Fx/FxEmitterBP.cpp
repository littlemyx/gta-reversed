#include "StdInc.h"

#include "FxEmitterBP.h"
#include "FxEmitter.h"
#include "FxPrimBP.h"
#include "FxEmitterPrt.h"
#include "FxInfo.h"
#include "FxInfoManager.h"

#include "Particle.h"
#include "FxTools.h"
#include "Fx.h"
#include "FxManager.h"
#include "FxSystem.h"
#include "FxSystemBP.h"
#include "FxPrtMult.h"
#include "MovementInfo.h"
#include "RenderInfo.h"
#include "Timer.h"
#include "WaterLevel.h"
#include "General.h"

namespace {
// Constants from the original exe
constexpr float kInv255      = 1.0f / 255.0f;   // 0x859A3C
constexpr float kInv128      = 1.0f / 128.0f;   // 0x858B88
constexpr float kDegToRad    = 0.0174532793f;   // 0x85A7BC (not exactly PI/180, so `DegreesToRadians` can't be used)

// 0x8A6230 - Blend function table: { rwBLENDZERO .. rwBLENDSRCALPHASAT } (values 1..11)
auto& s_BlendFunctions = StaticRef<std::array<RwBlendFunction, 11>>(0x8A6230);

//! Set up the temporary (identity) basis matrix the way the original does it
void SetupIdentityBasis(RwMatrix& mat) {
    mat.at.z    = 1.0f;
    mat.up.y    = 1.0f;
    mat.right.x = 1.0f;
    mat.up.x    = 0.0f;
    mat.right.z = 0.0f;
    mat.right.y = 0.0f;
    mat.at.y    = 0.0f;
    mat.at.x    = 0.0f;
    mat.up.z    = 0.0f;
    mat.pos.z   = 0.0f;
    mat.pos.y   = 0.0f;
    mat.pos.x   = 0.0f;
    mat.flags  |= 0x20003;
}

//! Build `right`, `up` (= `dir`) and `at` vectors of a particle facing the camera, but oriented along `dir`
//! @param dir        Direction (up vector), normalized here
//! @param toCam      Vector from the camera to the particle, normalized here
//! @param mat        [out] Matrix to store the basis into
//! @param axis       [out] The `at` vector
void BuildDirectedBasis(RwMatrix& mat, CVector dir, CVector toCam, CVector& axis) {
    RwV3dNormalize(&dir, &dir);
    RwV3dNormalize(&toCam, &toCam);

    const CVector right{
        toCam.z * dir.y - toCam.y * dir.z,
        toCam.x * dir.z - toCam.z * dir.x,
        toCam.y * dir.x - toCam.x * dir.y,
    };
    const CVector at{
        right.z * dir.y - right.y * dir.z,
        right.x * dir.z - right.z * dir.x,
        right.y * dir.x - right.x * dir.y,
    };
    mat.right = right;
    mat.up    = dir;
    mat.at    = at;
    axis      = at;
}

//! Update particle's rotation, and calculate the (rotated) `right` and `up` vectors
void ProcessParticleRotation(FxEmitterPrt_c& prt, const RwMatrix& mat, const CVector& axis, CVector& outRight, CVector& outUp) {
    if (prt.m_RotZ != 0xFF) {
        prt.m_CurrentRotation = (float)prt.m_RotZ + (float)prt.m_RotZ;
    }
    if (prt.m_CurrentRotation < 0.0f) {
        do {
            prt.m_CurrentRotation += 360.0f;
        } while (prt.m_CurrentRotation < 0.0f);
    }
    if (prt.m_CurrentRotation >= 360.0f) {
        do {
            prt.m_CurrentRotation -= 360.0f;
        } while (prt.m_CurrentRotation >= 360.0f);
    }
    if (prt.m_CurrentRotation > 0.0f) {
        RotateVecAboutVec(outRight, mat.right, axis, prt.m_CurrentRotation * kDegToRad);
        outUp = CVector{
            axis.y * outRight.z - axis.z * outRight.y,
            axis.z * outRight.x - axis.x * outRight.z,
            axis.x * outRight.y - axis.y * outRight.x,
        };
    } else {
        outRight = mat.right;
        outUp    = mat.up;
    }
}

//! Apply per-particle random size variation + size multiplier
void ProcessParticleSize(const FxEmitterPrt_c& prt, RenderInfo_t& info) {
    info.m_fSizeX = ((float)prt.m_RandR * kInv255 - 0.5f) * info.m_fSizeXBias + info.m_fSizeX;
    info.m_fSizeY = ((float)prt.m_RandG * kInv255 - 0.5f) * info.m_fSizeYBias + info.m_fSizeY;
    if (prt.m_MultSize < 0xFF) {
        info.m_fSizeX = (float)prt.m_MultSize * kInv255 * info.m_fSizeX;
        info.m_fSizeY = (float)prt.m_MultSize * kInv255 * info.m_fSizeY;
    }
}

//! Emit the 2 triangles of a quad centered at `pos`
void RenderParticleQuad(const CVector& pos, const CVector& right, const CVector& up, const RenderInfo_t& info, uint8 r, uint8 g, uint8 b, uint8 a) {
    const auto topScale    = info.m_fSpriteTop * info.m_fSizeY;
    const auto bottomScale = info.m_fSpriteBottom * info.m_fSizeY;
    const auto leftScale   = info.m_fSpriteLeft * info.m_fSizeX;
    const auto rightScale  = info.m_fSpriteRight * info.m_fSizeX;

    const CVector bottomUp = up * bottomScale;
    const CVector topUp    = up * topScale;
    const CVector leftR    = right * leftScale;
    const CVector rightR   = right * rightScale;

    const CVector leftTop     = leftR + topUp;
    const CVector rightBottom = rightR + bottomUp;
    const CVector rightTop    = rightR + topUp;
    const CVector leftBottom  = leftR + bottomUp;

    RenderAddTri_(
        leftTop.x + pos.x,     leftTop.y + pos.y,     leftTop.z + pos.z,
        rightBottom.x + pos.x, rightBottom.y + pos.y, rightBottom.z + pos.z,
        rightTop.x + pos.x,    rightTop.y + pos.y,    rightTop.z + pos.z,
        0.0f, 0.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        r, g, b, a,
        r, g, b, a,
        r, g, b, a
    );
    RenderAddTri_(
        rightBottom.x + pos.x, rightBottom.y + pos.y, rightBottom.z + pos.z,
        leftTop.x + pos.x,     leftTop.y + pos.y,     leftTop.z + pos.z,
        leftBottom.x + pos.x,  leftBottom.y + pos.y,  leftBottom.z + pos.z,
        1.0f, 1.0f,
        0.0f, 0.0f,
        0.0f, 1.0f,
        r, g, b, a,
        r, g, b, a,
        r, g, b, a
    );
}

//! Calculate world position of the particle
CVector GetParticleWorldPos(const FxEmitterPrt_c& prt) {
    if (!prt.m_bLocalToSystem) {
        return prt.m_Pos;
    }
    CVector out;
    auto* const mat = g_fxMan.FxRwMatrixCreate();
    prt.m_System->GetCompositeMatrix(mat);
    RwV3dTransformPoints(&out, &prt.m_Pos, 1, mat);
    g_fxMan.FxRwMatrixDestroy(mat);
    return out;
}

//! Select the raster to use (animated textures support)
//! @param stale Original leaves the previous value in this case (uninitialized for the 1st particle)
RwRaster* SelectParticleRaster(const std::array<RwTexture*, 4>& textures, const RenderInfo_t& info, RwRaster* stale) {
    if (!info.m_bHasAnimTextures) {
        return textures[0]->raster;
    }
    switch (info.m_nCurrentTexId) {
    case 1:  return textures[0]->raster;
    case 2:  return (textures[1] ? textures[1] : textures[0])->raster;
    case 3:  return (textures[2] ? textures[2] : textures[0])->raster;
    case 4:  return (textures[3] ? textures[3] : textures[0])->raster;
    default: return stale;
    }
}
} // namespace

void FxEmitterBP_c::InjectHooks() {
    RH_ScopedVirtualClass(FxEmitterBP_c, 0x85A788, 7);
    RH_ScopedCategory("Fx");

    RH_ScopedInstall(Constructor, 0x4A18D0);
    RH_ScopedInstall(RenderHeatHaze, 0x4A1940);
    RH_ScopedInstall(UpdateParticle, 0x4A21D0);
    RH_ScopedVMTInstall(CreateInstance, 0x4A2B40);
    RH_ScopedVMTInstall(Update, 0x4A2BC0);
    RH_ScopedVMTInstall(Load, 0x5C25F0);
    RH_ScopedVMTInstall(LoadTextures, 0x5C0A30, {.Reversed = true});
    RH_ScopedVMTInstall(Render, 0x4A2C40);
    RH_ScopedVMTInstall(FreePrtFromPrim, 0x4A2510);
}

// 0x4A18D0
FxEmitterBP_c::FxEmitterBP_c() : FxPrimBP_c() {
    m_Type = 0;
}

// 0x4A1940
void FxEmitterBP_c::RenderHeatHaze(RwCamera* camera, uint32 txdHashKey, float brightness) {
    if (!m_Particles.GetNumItems()) {
        return;
    }

    auto* const raster0 = m_apTextures[0]->raster;
    if (!m_Particles.GetNumItems()) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDONE));

    RwRaster* curRaster = raster0;
    RwRaster* raster    = raster0; // NOTSA: Original leaves this uninitialized (only matters for invalid animated texture ids)
    RenderBegin(curRaster, nullptr, rwIM3D_VERTEXUV);

    const auto& camMat = *RwFrameGetMatrix(RwCameraGetFrame(camera));

    for (auto* it = m_Particles.GetHead(); it; it = m_Particles.GetNext(it)) {
        auto* const prt = static_cast<FxEmitterPrt_c*>(it);

        const auto pos = GetParticleWorldPos(*prt);

        RenderInfo_t info{};
        m_FxInfoManager.ProcessRenderInfo(
            prt->m_System->m_fCurrentTime,
            prt->m_fCurrentLife / prt->m_fTotalLife,
            0.0f,
            prt->m_System->m_SystemBP->m_fLength,
            false,
            &info
        );

        auto* const mat = g_fxMan.FxRwMatrixCreate();
        SetupIdentityBasis(*mat);

        CVector axis; // The `at` vector of the basis
        if (info.m_bHasDir) {
            const CVector dir = info.m_bUseVel ? prt->m_Velocity : info.m_Direction;
            const CVector toCam = pos - CVector{ camMat.pos };
            BuildDirectedBasis(*mat, dir, toCam, axis);
        } else if (info.m_bIsFlat) {
            *mat = info.m_FlatMatrix;
            axis = info.m_FlatMatrix.at;
        } else {
            *mat = camMat;
            axis = camMat.at;
        }

        CVector right, up;
        ProcessParticleRotation(*prt, *mat, axis, right, up);
        g_fxMan.FxRwMatrixDestroy(mat);

        ProcessParticleSize(*prt, info);

        // Heat haze particles are rendered with black color (only alpha matters)
        info.m_Color1.red   = 0;
        info.m_Color1.green = 0;
        info.m_Color1.blue  = 0;

        raster = SelectParticleRaster(m_apTextures, info, raster);
        if (curRaster != raster) {
            RenderEnd();
            curRaster = raster;
            RenderBegin(curRaster, nullptr, rwIM3D_VERTEXUV);
        }

        RenderParticleQuad(pos, right, up, info, info.m_Color1.red, info.m_Color1.green, info.m_Color1.blue, info.m_Color1.alpha);
    }

    RenderEnd();
}

// 0x4A21D0
bool FxEmitterBP_c::UpdateParticle(float deltaTime, FxEmitterPrt_c* emitter) {
    auto* const prt = emitter;
    auto* const sys = prt->m_System;

    const auto dt = (float)sys->m_nTimeMult * 0.001f * deltaTime;

    prt->m_fCurrentLife = dt + prt->m_fCurrentLife;
    if (!(prt->m_fCurrentLife < prt->m_fTotalLife)) {
        return true; // Dead
    }

    prt->m_Pos.x = dt * prt->m_Velocity.x + prt->m_Pos.x;
    prt->m_Pos.y = dt * prt->m_Velocity.y + prt->m_Pos.y;
    prt->m_Pos.z = dt * prt->m_Velocity.z + prt->m_Pos.z;

    MovementInfo_t mi{};
    mi.m_Pos = prt->m_Pos;
    mi.m_Vel = prt->m_Velocity;
    m_FxInfoManager.ProcessMovementInfo(
        sys->m_fCurrentTime,
        prt->m_fCurrentLife / prt->m_fTotalLife,
        dt,
        sys->m_SystemBP->m_fLength,
        false,
        &mi
    );
    prt->m_Pos      = mi.m_Pos;
    prt->m_Velocity = mi.m_Vel;

    if (mi.m_bHasFloatInfo || mi.m_bHasUnderwaterInfo) {
        float waterZ;
        const auto hasWater = CWaterLevel::GetWaterLevel(prt->m_Pos.x, prt->m_Pos.y, prt->m_Pos.z, waterZ, true, nullptr);
        if (mi.m_bHasFloatInfo && hasWater && prt->m_Pos.z < waterZ) {
            prt->m_Pos.z = waterZ;
        }
        if (mi.m_bHasUnderwaterInfo) {
            if (!hasWater) {
                return true;
            }
            if (!(prt->m_Pos.z <= waterZ)) {
                return true; // Went out of water
            }
        }
    }

    const auto& rot = mi.m_Rot;
    // `factor` is the (already scaled) per-particle random factor
    const auto SignedRot = [&](float lo, float hi, float factor) {
        return ((hi - lo) * factor + lo) * (float)prt->m_MultRot;
    };

    const auto firstPairNonPositive  = rot[0] <= 0.0f && rot[1] <= 0.0f;
    const auto secondPairNonPositive = rot[2] <= 0.0f && rot[3] <= 0.0f;
    if (firstPairNonPositive || secondPairNonPositive) {
        if (!firstPairNonPositive) { // First pair is positive => increase rotation
            const auto v = SignedRot(rot[0], rot[1], (float)prt->m_RandR * kInv255);
            prt->m_CurrentRotation = v * dt * kInv255 + prt->m_CurrentRotation;
        } else if (!secondPairNonPositive) { // Second pair is positive => decrease rotation
            const auto v = SignedRot(rot[2], rot[3], (float)prt->m_RandR * kInv255);
            prt->m_CurrentRotation = prt->m_CurrentRotation - v * dt * kInv255;
        }
    } else {
        if (prt->m_RandR < 0x80) {
            const auto v = SignedRot(rot[0], rot[1], (float)prt->m_RandR * kInv128);
            prt->m_CurrentRotation = v * dt * kInv255 + prt->m_CurrentRotation;
        } else {
            const auto v = SignedRot(rot[2], rot[3], ((float)prt->m_RandR - 128.0f) * kInv128);
            prt->m_CurrentRotation = prt->m_CurrentRotation - v * dt * kInv255;
        }
    }

    return false;
}

// 0x4A2B40
FxPrim_c* FxEmitterBP_c::CreateInstance() {
    return new FxEmitter_c();
}

// 0x4A2BC0
void FxEmitterBP_c::Update(float deltaTime) {
    if (!m_Particles.GetNumItems()) {
        return;
    }

    for (auto* it = m_Particles.GetHead(); it;) {
        auto* const sys = it->m_System;
        if (sys->m_nKillStatus == eFxSystemKillStatus::FX_3) {
            sys->m_nKillStatus = eFxSystemKillStatus::FX_KILLED;
        }

        if (it->m_System->m_nPlayStatus != eFxSystemPlayStatus::T2 && UpdateParticle(deltaTime, it->AsFxEmitterPrt())) {
            auto* const next = m_Particles.GetNext(it);
            m_Particles.RemoveItem(it);
            g_fxMan.ReturnParticle(it->AsFxEmitterPrt());
            it = next;
        } else {
            it = m_Particles.GetNext(it);
        }
    }
}

// 0x5C25F0
bool FxEmitterBP_c::Load(FILESTREAM file, int32 version, FxName32_t* textureNames) {
    FxPrimBP_c::Load(file, version, textureNames);

    // NOTSA: The lod values live in the FxInfoManager (+0x38 and +0x3A), not after it
    m_FxInfoManager.m_nLodStart = uint16(ReadField<float>(file, "LODSTART:") * 64.0f);
    m_FxInfoManager.m_nLodEnd   = uint16(ReadField<float>(file, "LODEND:") * 64.0f);

    return true;
}

// 0x5C0A30


bool FxEmitterBP_c::LoadTextures(FxName32_t* textureNames, int32 version) {
    assert(textureNames);

    const auto LoadTexture = [&](auto ind) -> RwTexture* {
        char mask[64];
        sprintf(mask, "%sm", textureNames[ind]);

        auto* texture = RwTextureRead(textureNames[ind], mask);
        return texture ? texture : RwTextureRead(textureNames[ind], nullptr);
    };

    const auto LoadTextureIfExists = [=](auto ind) -> RwTexture* {
        assert(&textureNames[ind]);
        return strncmp(textureNames[ind], "NULL", 5u) != 0 ? LoadTexture(ind) : nullptr;
    };

    m_apTextures[0] = LoadTexture(0);

    if (version > 101) {
        m_apTextures[1] = LoadTextureIfExists(1);
        m_apTextures[2] = LoadTextureIfExists(2);
        m_apTextures[3] = LoadTextureIfExists(3);
    }

    return true;
}

// 0x4A2C40
void FxEmitterBP_c::Render(RwCamera* camera, uint32 txdHashKey, float brightness, bool doHeatHaze) {
    const auto hasHeatHazeInfo = IsFxInfoPresent(FX_INFO_HEATHAZE_DATA);

    if (doHeatHaze) {
        if (m_FxInfoManager.m_bHasHeatHazeParticleEmitter) {
            RenderHeatHaze(camera, txdHashKey, brightness);
        }
        return;
    }

    if (hasHeatHazeInfo) {
        if (m_Particles.GetNumItems() > 0u) {
            g_fxMan.m_bHeatHazeEnabled = true; // Will be rendered in the heat haze pass
        }
        return;
    }

    if (!m_Particles.GetNumItems()) {
        return;
    }

    auto* const raster0 = m_apTextures[0]->raster;
    if (!m_Particles.GetNumItems()) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE((int32)m_bAlphaOn));
    if (m_bAlphaOn) {
        RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(s_BlendFunctions[(int8)m_nSrcBlendId]));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(s_BlendFunctions[(int8)m_nDstBlendId]));
    } else {
        RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(s_BlendFunctions[1]));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(s_BlendFunctions[0]));
    }

    RwRaster* curRaster = raster0;
    RwRaster* raster    = raster0; // NOTSA: Original leaves this uninitialized (only matters for invalid animated texture ids)
    RenderBegin(curRaster, nullptr, rwIM3D_VERTEXUV);

    // BUG: Original reuses the (dead) `doHeatHaze` argument slot for this, so it's not initialized properly, only matters if `m_Brightness > 100`
    float brightnessMult = 0.0f;

    const auto& camMat = *RwFrameGetMatrix(RwCameraGetFrame(camera));

    for (auto* it = m_Particles.GetHead(); it; it = m_Particles.GetNext(it)) {
        auto* const prt = static_cast<FxEmitterPrt_c*>(it);

        const auto pos = GetParticleWorldPos(*prt);

        RenderInfo_t info{};
        m_FxInfoManager.ProcessRenderInfo(
            prt->m_System->m_fCurrentTime,
            prt->m_fCurrentLife / prt->m_fTotalLife,
            0.0f,
            prt->m_System->m_SystemBP->m_fLength,
            false,
            &info
        );

        // Smoke emitted by this particle
        if (info.m_SmokeType > -1) {
            const float smokeBrightness    = info.m_SmokeBrightness;
            const float negSmokeBrightness = -smokeBrightness;
            const float randOffset = ((smokeBrightness - negSmokeBrightness) * ((float)CGeneral::GetRandomNumber() * RAND_MAX_FLOAT_RECIPROCAL) + negSmokeBrightness) * kInv255;

            FxPrtMult_c mult{};
            const auto ProcessColor = [&](float color) {
                const auto c = color * kInv255 + randOffset;
                if (1.0f < c) {
                    return 1.0f;
                }
                if (c < 0.0f) {
                    return 0.0f;
                }
                return c;
            };
            mult.m_Color.red   = ProcessColor(info.m_SmokeColor.red);
            mult.m_Color.green = ProcessColor(info.m_SmokeColor.green);
            mult.m_Color.blue  = ProcessColor(info.m_SmokeColor.blue);
            mult.m_Color.alpha = info.m_SmokeColor.alpha * kInv255;
            mult.m_fSize       = info.m_SmokeSize;
            mult.m_Rot         = 1.0f;
            mult.m_fLife       = info.m_SmokeLife;

            // NOTSA: Original always uses `m_SmokeHuge`, regardless of the smoke type
            g_fx.m_SmokeHuge->AddParticle(prt->m_Pos, CVector{ 0.0f, 0.0f, 0.0f }, 0.0f, mult, -1.0f, 1.2f, 0.6f, false);
        }

        auto* const mat = g_fxMan.FxRwMatrixCreate();
        SetupIdentityBasis(*mat);

        CVector axis; // The `at` vector of the basis
        CVector trailVec{};
        if (info.m_nTrailScreenMode > 0) {
            const auto& pp = prt->m_Pos; // BUG: Local-space position is used even if the particle is local to its system

            if (info.m_nTrailScreenMode == 2) {
                const auto k = CTimer::ms_fTimeStep * 0.02f;

                const CVector toParticle{
                    pp.x - camMat.pos.x,
                    pp.y - camMat.pos.y,
                    pp.z - camMat.pos.z,
                };
                const CVector vk{
                    prt->m_Velocity.x * k,
                    prt->m_Velocity.y * k,
                    prt->m_Velocity.z * k,
                };
                const auto& prevCamPos = TheCamera.m_mCameraMatrixOld.GetPosition(); // Original: global at 0xB6FA14 (TheCamera + 0x9EC)
                const CVector prevPos{
                    (pp.x - vk.x) - prevCamPos.x,
                    (pp.y - vk.y) - prevCamPos.y,
                    (pp.z - vk.z) - prevCamPos.z,
                };
                trailVec = CVector{
                    (toParticle.x - prevPos.x) * info.m_fTrailTime,
                    (toParticle.y - prevPos.y) * info.m_fTrailTime,
                    (toParticle.z - prevPos.z) * info.m_fTrailTime,
                };
            } else {
                const CVector prevPos{
                    pp.x - info.m_fTrailTime * prt->m_Velocity.x,
                    pp.y - info.m_fTrailTime * prt->m_Velocity.y,
                    pp.z - info.m_fTrailTime * prt->m_Velocity.z,
                };
                trailVec = CVector{
                    pp.x - prevPos.x,
                    pp.y - prevPos.y,
                    pp.z - prevPos.z,
                };
            }

            CVector dir = trailVec; // Normalized in `BuildDirectedBasis`
            if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) {
                dir.z = 1.0f;
            }

            const CVector toCam{
                pp.x - camMat.pos.x,
                pp.y - camMat.pos.y,
                pp.z - camMat.pos.z,
            };
            BuildDirectedBasis(*mat, dir, toCam, axis);
        } else if (info.m_bHasDir) {
            CVector dir;
            if (info.m_bUseVel) {
                dir = prt->m_Velocity;
                if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f) {
                    dir.z = 1.0f;
                }
            } else {
                dir = info.m_Direction;
            }
            const CVector toCam = pos - CVector{ camMat.pos };
            BuildDirectedBasis(*mat, dir, toCam, axis);
        } else if (info.m_bIsFlat) {
            *mat = info.m_FlatMatrix;
            axis = info.m_FlatMatrix.at;
        } else {
            *mat = camMat;
            axis = camMat.at;
        }

        CVector right, up;
        ProcessParticleRotation(*prt, *mat, axis, right, up);
        g_fxMan.FxRwMatrixDestroy(mat);

        ProcessParticleSize(*prt, info);

        // Calculate the final color
        float r = (float)info.m_Color1.red;
        float g = (float)info.m_Color1.green;
        float b = (float)info.m_Color1.blue;
        float a = (float)info.m_Color1.alpha;
        if (info.m_nColorType == ERenderColorType::RANGE) {
            r = ((float)info.m_Color2.red   - r) * ((float)prt->m_RandR * kInv255) + r;
            g = ((float)info.m_Color2.green - g) * ((float)prt->m_RandG * kInv255) + g;
            b = ((float)info.m_Color2.blue  - b) * ((float)prt->m_RandB * kInv255) + b;
        } else if (info.m_nColorType == ERenderColorType::BRIGHT) {
            const auto delta = ((float)prt->m_RandR * kInv128 - 1.0f) * (float)info.m_Color2.alpha;
            r += delta;
            g += delta;
            b += delta;
            if (r < 0.0f) { r = 0.0f; }
            if (g < 0.0f) { g = 0.0f; }
            if (b < 0.0f) { b = 0.0f; }
            if (255.0f < r) { r = 255.0f; }
            if (255.0f < g) { g = 255.0f; }
            if (255.0f < b) { b = 255.0f; }
        }
        if (prt->m_MultColor.r < 0xFF) { r = (float)prt->m_MultColor.r * kInv255 * r; }
        if (prt->m_MultColor.g < 0xFF) { g = (float)prt->m_MultColor.g * kInv255 * g; }
        if (prt->m_MultColor.b < 0xFF) { b = (float)prt->m_MultColor.b * kInv255 * b; }
        if (prt->m_MultColor.a < 0xFF) { a = (float)prt->m_MultColor.a * kInv255 * a; }

        uint8 cr, cg, cb;
        if (info.m_bSelfLit) {
            cr = (uint8)(int32)r;
            cg = (uint8)(int32)g;
            cb = (uint8)(int32)b;
        } else {
            if (prt->m_Brightness <= 100u) {
                brightnessMult = (float)prt->m_Brightness * 0.01f;
            }
            cr = (uint8)(int32)(r * brightnessMult);
            cg = (uint8)(int32)(g * brightnessMult);
            cb = (uint8)(int32)(b * brightnessMult);
        }
        const auto ca = (uint8)(int32)a;

        raster = SelectParticleRaster(m_apTextures, info, raster);
        if (curRaster != raster) {
            RenderEnd();
            curRaster = raster;
            RenderBegin(curRaster, nullptr, rwIM3D_VERTEXUV);
        }

        if (info.m_nTrailScreenMode > 0) {
            info.m_fSpriteTop    = RwV3dLength(&trailVec);
            info.m_fSpriteBottom = 0.0f;
        }

        RenderParticleQuad(pos, right, up, info, cr, cg, cb, ca);
    }

    RenderEnd();
}

// 0x4A2510
bool FxEmitterBP_c::FreePrtFromPrim(FxSystem_c* system) {
    for (auto* it = m_Particles.GetHead(); it; it = m_Particles.GetNext(it)) {
        if (it->m_System == system) {
            m_Particles.RemoveItem(it);
            g_fxMan.ReturnParticle(it->AsFxEmitterPrt());
            return true;
        }
    }
    return false;
}

// todo: eFxInfo
// 0x4A24D0
bool FxEmitterBP_c::IsFxInfoPresent(eFxInfoType type) const {
    if (m_FxInfoManager.m_nNumInfos <= 0)
        return false;

    for (auto& info : m_FxInfoManager.GetInfos()) {
        if (info->m_nType == type) {
            return true;
        }
    }
    return false;
}
