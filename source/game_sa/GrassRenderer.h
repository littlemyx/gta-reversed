#pragma once

#include "PPTriPlantBuffer.h"

class CVector;

class CGrassRenderer {
public:
    static inline NOTSA_GLOBAL(m_windBending, 0xC02DB8, (float), {});
    static inline NOTSA_GLOBAL(m_closeDist, 0xC02DBC, (float), {});
    static inline NOTSA_GLOBAL(m_vecCameraPos, 0xC02DDC, (CVector), {});
    static inline NOTSA_GLOBAL(m_farDist, 0x8D132C, (float), { 10.0f }); // 10.0f

public:
    static void InjectHooks();

    CGrassRenderer() = default;
    ~CGrassRenderer() = default;

    static bool Initialise();
    static void Shutdown();

    static void AddTriPlant(PPTriPlant* srcPlant, uint32 plantModelSet);
    static void DrawTriPlants(PPTriPlant* triPlants, int32 numTriPlants, RpAtomic** plantModelsTab);
    static void FlushTriPlantBuffer();

    static RpAtomic** GetPlantModelsTab(uint32 type);
    static void SetPlantModelsTab(uint32 type, RpAtomic** atomics);

    static void SetCloseFarAlphaDist(float fCloseDist, float fFarDist);
    static void SetCurrentScanCode(uint16 scanCode);
    static void SetGlobalCameraPos(const CVector& cameraPosn);
    static void SetGlobalWindBending(float bending);

    static CVector& GenPointInTriangle(CVector& outPosn, const CVector& v1, const CVector& v2, const CVector& v3, float randA, float randB);
};

NOTSA_GLOBAL_EXTERN(g_GrassCurrentScanCode, (uint16));
NOTSA_GLOBAL_EXTERN(gTriPlantBuf, (CPPTriPlantBuffer));
