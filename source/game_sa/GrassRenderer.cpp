#include "StdInc.h"

#include "GrassRenderer.h"
#include "PPTriPlantBuffer.h"

NOTSA_GLOBAL(gTriPlantBuf, 0xC02DE8, (CPPTriPlantBuffer), {});
NOTSA_GLOBAL(g_GrassCurrentScanCode, 0x8D1330, (uint16), { 65535 }); // -1

void CGrassRenderer::InjectHooks() {
    RH_ScopedClass(CGrassRenderer);
    RH_ScopedCategory("Plant");

    RH_ScopedInstall(Initialise, 0x5DD6B0);
    RH_ScopedInstall(Shutdown, 0x5DABA0);
    RH_ScopedInstall(AddTriPlant, 0x5DB1D0);
    RH_ScopedInstall(DrawTriPlants, 0x5DAD00);
    RH_ScopedInstall(FlushTriPlantBuffer, 0x5DB250);
    RH_ScopedInstall(GetPlantModelsTab, 0x5DACE0);
    RH_ScopedInstall(SetPlantModelsTab, 0x5DACC0);
    RH_ScopedInstall(SetCloseFarAlphaDist, 0x5DABE0);
    RH_ScopedInstall(SetCurrentScanCode, 0x5DABB0);
    RH_ScopedInstall(SetGlobalCameraPos, 0x5DABC0);
    RH_ScopedInstall(SetGlobalWindBending, 0x5DAC00);

    RH_ScopedInstall(GenPointInTriangle, 0x5DAB00);
}

// 0x5DD6B0
bool CGrassRenderer::Initialise() {
    return true;
}

// 0x5DABA0
void CGrassRenderer::Shutdown() {
    // NOP
}

// 0x5DB1D0
void CGrassRenderer::AddTriPlant(PPTriPlant* srcPlant, uint32 plantModelSet) {
    gTriPlantBuf.ChangeCurrentPlantModelsSet(plantModelSet);

    if (gTriPlantBuf.m_CurrentIndex + 1 > MAX_PLANTS)
        gTriPlantBuf.Flush();

    gTriPlantBuf.CopyToActive(srcPlant);
    gTriPlantBuf.IncreaseBufferIndex(plantModelSet, 1);
}

// 0x5DAD00
void CGrassRenderer::DrawTriPlants(PPTriPlant* triPlants, int32 numTriPlants, RpAtomic** plantModelsTab) {
    const auto farDist = [] { // OG: located in loop
        switch (g_fx.GetFxQuality()) {
        case FX_QUALITY_LOW:
        case FX_QUALITY_MEDIUM:
        default:
            return m_farDist / 2.0f;
        case FX_QUALITY_HIGH:
        case FX_QUALITY_VERY_HIGH:
            return m_farDist;
        }
    }();
    // 0x5DAD1F / 0x5DAF4A: both distances sum z, y, x (CVector::Magnitude sums x, y, z)
    const auto zyxDist = [](const CVector& p) -> float {
        const x87d dx = (x87d)p.x - m_vecCameraPos.x;
        const x87d dy = (x87d)p.y - m_vecCameraPos.y;
        const x87d dz = (x87d)p.z - m_vecCameraPos.z;
        return (float)std::sqrt((dz * dz + dy * dy) + dx * dx);
    };
    for (const auto& plant : std::span{triPlants, (size_t)numTriPlants}) {
        const auto camDist = zyxDist(plant.Center);
        if (camDist > farDist + 20.0f) {
            continue;
        }

        const auto atomic = plantModelsTab[plant.model_id];
        auto frame = RpAtomicGetFrame(atomic);

#if FIX_BUGS
        // The original's srand() is not thread safe
        std::mt19937 randomGen(*(uint32*)&plant.seed);
        std::uniform_real_distribution<float> randomDistribution(0, 1.f);
        const auto nextRand = [&] { return randomDistribution(randomGen); };
#else
        // 0x5DADDF: srand(ftol(seed)) (the seed is in [0:1), so it is always srand(0): every tuft of a triangle gets the same pattern),
        // then the CRT rand() * (1/32767) for every value (port used a std::mt19937 here: different offsets/scales than the exe)
        srand(NOTSA_FTOL_SC(uint32, plant.seed));
        const auto nextRand = [] { return (float)((x87d)CGeneral::GetRandomNumber() * (x87d)RAND_MAX_FLOAT_RECIPROCAL); };
#endif

        RwRGBA newColorIntensity{};
        if (camDist >= farDist) {
            const auto alpha = std::floor((farDist + 20.0f - camDist) * (float)plant.color.a * ExeRecip(20.0f));
            newColorIntensity.alpha = NOTSA_FTOL_P(uint8, std::clamp(alpha, 0.0f, 255.0f));
        } else {
            newColorIntensity.alpha = plant.color.a;
        }

        // 0x5DAE61
        auto mult = std::min(float(plant.intensity), 255.0f);
        newColorIntensity.red   = NOTSA_FTOL_FN(uint16, mult * (float)plant.color.r) / 256;
        newColorIntensity.green = NOTSA_FTOL_FN(uint16, mult * (float)plant.color.g) / 256;
        newColorIntensity.blue  = NOTSA_FTOL_FN(uint16, mult * (float)plant.color.b) / 256;

        // 0x5DAC40
        RenderGrassTexture = plant.texture;
        RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), CPPTriPlantBuffer::SetGrassMaterialCB, &newColorIntensity);

        for (auto j = 0; j < plant.num_plants; j++) {
            const auto randX = nextRand();
            const auto randY = nextRand();
            CVector posn;
            GenPointInTriangle(posn, plant.V1, plant.V2, plant.V3, randX, randY);

            // the 3 rand() calls of the draw path (0x5DAFB5 scale xy, 0x5DAFF5 scale z, 0x5DB01A wind) are made after the close-distance test; the
            // skip path (0x5DAF8A) burns three rand() calls so that the PRNG stays consistent
            if (zyxDist(posn) < m_closeDist - 2.0f) {
                nextRand();
                nextRand();
                nextRand();
                continue;
            }

            RwFrameTranslate(frame, &posn, rwCOMBINEREPLACE);

            const auto rand1 = nextRand();
            const auto xy = rand1 * plant.scale_var_xy + plant.scale.x;
            RwFrameGetMatrix(frame)->right.x *= xy;
            RwFrameGetMatrix(frame)->up.y *= xy;
            const auto rand2 = nextRand();
            RwFrameGetMatrix(frame)->at.z *= (rand2 * plant.scale_var_z + plant.scale.y);

            const auto rand3 = nextRand();
            const auto xy1 = (rand3 * plant.wind_bend_var + 1.0f) * (m_windBending * plant.wind_bend_scale);
            RwFrameGetMatrix(frame)->at.x = xy1;
            RwFrameGetMatrix(frame)->at.y = xy1;

            RwMatrixUpdate(RwFrameGetMatrix(frame));
            RpAtomicRender(atomic);
        }
    }
}

// 0x5DB250
void CGrassRenderer::FlushTriPlantBuffer() {
    gTriPlantBuf.Flush();
}

// 0x5DACE0
RpAtomic** CGrassRenderer::GetPlantModelsTab(uint32 type) {
    return gTriPlantBuf.GetPlantModelsTab(type);
}

// 0x5DACC0
void CGrassRenderer::SetPlantModelsTab(uint32 type, RpAtomic** atomics) {
    gTriPlantBuf.SetPlantModelsTab(type, atomics);
}

// 0x5DABE0
void CGrassRenderer::SetCloseFarAlphaDist(float fCloseDist, float fFarDist) {
    m_closeDist = fCloseDist;
    m_farDist   = fFarDist;
}

// 0x5DABB0
void CGrassRenderer::SetCurrentScanCode(uint16 scanCode) {
    g_GrassCurrentScanCode = scanCode;
}

// 0x5DABC0
void CGrassRenderer::SetGlobalCameraPos(const CVector& cameraPosn) {
    m_vecCameraPos = cameraPosn;
}

// 0x5DAC00
void CGrassRenderer::SetGlobalWindBending(float bending) {
    m_windBending = bending;
}

// Interpolates inside the triangle (v1,v2,v3) with specific step values
// Original name unknown
// 0x5DAB00 (inlined)
CVector& CGrassRenderer::GenPointInTriangle(CVector& outPosn, const CVector& v1, const CVector& v2, const CVector& v3, float stepA, float stepB) {
    if (stepA + stepB > 1.0f) {
        stepA = 1.0f - stepA;
        stepB = 1.0f - stepB;
    }
    outPosn = (1.0f - stepA - stepB) * v1 + stepA * v2 + stepB * v3;
    return outPosn;
}
