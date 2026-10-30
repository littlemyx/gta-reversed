/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"
#include <imgui.h>
#include "Shadows.h"
#include "FireManager.h"
#include "Collision/ColHelpers.h"
#include "RenderBuffer.hpp"
#include <CustomBuildingDNPipeline.h>

void CShadows::InjectHooks() {
    RH_ScopedClass(CShadows);
    RH_ScopedCategory("Shadows");

    RH_ScopedInstall(Init, 0x706CD0);
    RH_ScopedInstall(Shutdown, 0x706ED0);
    RH_ScopedInstall(TidyUpShadows, 0x707770);
    RH_ScopedInstall(AddPermanentShadow, 0x706F60);
    RH_ScopedInstall(UpdatePermanentShadows, 0x70C950);
    RH_ScopedOverloadedInstall(StoreShadowToBeRendered, "Texture", 0x707390, void(*)(uint8, RwTexture*, const CVector&, float, float, float, float, int16, uint8, uint8, uint8, float, bool, float, CRealTimeShadow*, bool));
    RH_ScopedOverloadedInstall(StoreShadowToBeRendered, "Type", 0x707930, void(*)(uint8, const CVector&, float, float, float, float, int16, uint8, uint8, uint8));
    RH_ScopedInstall(SetRenderModeForShadowType, 0x707460);
    RH_ScopedInstall(RemoveOilInArea, 0x7074F0);
    RH_ScopedInstall(GunShotSetsOilOnFire, 0x707550);
    RH_ScopedInstall(PrintDebugPoly, 0x7076B0);
    RH_ScopedInstall(CalcPedShadowValues, 0x7076C0);
    RH_ScopedInstall(AffectColourWithLighting, 0x707850);
    RH_ScopedInstall(StoreShadowForPedObject, 0x707B40);
    RH_ScopedInstall(StoreRealTimeShadow, 0x707CA0);
    RH_ScopedInstall(UpdateStaticShadows, 0x707F40);
    RH_ScopedInstall(RenderExtraPlayerShadows, 0x707FA0);
    RH_ScopedInstall(RenderStaticShadows, 0x708300);
    RH_ScopedInstall(CastShadowEntityXY, 0x7086B0);
    RH_ScopedInstall(CastShadowEntityXYZ, 0x70A040);
    RH_ScopedInstall(CastPlayerShadowSectorList<CPtrListSingleLink<CPhysical*>>, 0x70A470);
    RH_ScopedInstall(CastShadowSectorList<CPtrListSingleLink<CPhysical*>>, 0x70A630);
    RH_ScopedInstall(CastRealTimeShadowSectorList<CPtrListSingleLink<CPhysical*>>, 0x70A7E0);
    RH_ScopedInstall(RenderStoredShadows, 0x70A960);
    RH_ScopedInstall(GeneratePolysForStaticShadow, 0x70B730);
    RH_ScopedInstall(StoreStaticShadow, 0x70BA00);
    RH_ScopedInstall(StoreShadowForVehicle, 0x70BDA0);
    RH_ScopedInstall(StoreCarLightShadow, 0x70C500);
    RH_ScopedInstall(StoreShadowForPole, 0x70C750);
    RH_ScopedInstall(RenderIndicatorShadow, 0x70CCB0);
    // RH_ScopedGlobalInstall(ShadowRenderTriangleCB, 0x709CF0); // Uses a custom calling convention (`eax`, `ebx`, `edi`), can't be hooked
}

void CStaticShadow::InjectHooks() {
    RH_ScopedClass(CStaticShadow);
    RH_ScopedCategory("Shadows");

    RH_ScopedInstall(Free, 0x707670);
    //RH_ScopedInstall(Init, 0x0);
}

// 0x707670 
void CStaticShadow::Free() {
    if (m_pPolyBunch) {
        const auto prevHead = CShadows::pEmptyBunchList;
        CShadows::pEmptyBunchList = m_pPolyBunch;

        // Find last in the list and make it point to the previous head
        auto it{ m_pPolyBunch };
        while (it->m_pNext) {
            it = static_cast<CPolyBunch*>(it->m_pNext);
        }
        it->m_pNext = prevHead;
    }

    // The exe clears these two also when there is no poly bunch (0x70769E is the target of the `je` of the null test)
    m_pPolyBunch = nullptr;
    m_nId = 0;
}

// 0x706CD0
void CShadows::Init() {
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(CTxdStore::FindTxdSlot("particle"));

    gpShadowCarTex         = RwTextureRead("shad_car",     nullptr);
    gpShadowPedTex         = RwTextureRead("shad_ped",     nullptr);
    gpShadowHeliTex        = RwTextureRead("shad_heli",    nullptr);
    gpShadowBikeTex        = RwTextureRead("shad_bike",    nullptr);
    gpShadowBaronTex       = RwTextureRead("shad_rcbaron", nullptr);
    gpShadowExplosionTex   = RwTextureRead("shad_exp",     nullptr);
    gpShadowHeadLightsTex  = RwTextureRead("headlight",    nullptr);
    gpShadowHeadLightsTex2 = RwTextureRead("headlight1",   nullptr);
    gpBloodPoolTex         = RwTextureRead("bloodpool_64", nullptr);
    gpHandManTex           = RwTextureRead("handman",      nullptr);
    gpCrackedGlassTex      = RwTextureRead("wincrack_32",  nullptr);
    gpPostShadowTex        = RwTextureRead("lamp_shad_64", nullptr);

    CTxdStore::PopCurrentTxd();

    g_ShadowVertices = { 0, 2, 1, 0, 3, 2, 0, 4, 3, 0, 5, 4, 0, 6, 5, 0, 7, 6, 0, 8, 7, 0, 9, 8 };

    std::ranges::for_each(aStaticShadows, [&](auto& shadow) { shadow.Init(); });

    pEmptyBunchList = aPolyBunches.data();

    for (auto i = 0u; i < aPolyBunches.size() - 1; i++) {
        aPolyBunches[i].m_pNext = &aPolyBunches[i + 1];
    }
    aPolyBunches.back().m_pNext = nullptr;

    std::ranges::for_each(aPermanentShadows, [&](auto& shadow) { shadow.Init(); });
}

// 0x706ED0
void CShadows::Shutdown() {
    RwTextureDestroy(gpShadowCarTex);
    RwTextureDestroy(gpShadowPedTex);
    RwTextureDestroy(gpShadowHeliTex);
    RwTextureDestroy(gpShadowBikeTex);
    RwTextureDestroy(gpShadowBaronTex);
    RwTextureDestroy(gpShadowExplosionTex);
    RwTextureDestroy(gpShadowHeadLightsTex);
    RwTextureDestroy(gpShadowHeadLightsTex2);
    RwTextureDestroy(gpBloodPoolTex);
    RwTextureDestroy(gpHandManTex);
    RwTextureDestroy(gpCrackedGlassTex);
    RwTextureDestroy(gpPostShadowTex);
}

// 0x707770
void CShadows::TidyUpShadows() {
    std::ranges::for_each(
        aPermanentShadows,
        [&](auto& shadow) { shadow.m_nType = SHADOW_NONE; }
    );
}

// 0x706F60
void CShadows::AddPermanentShadow(uint8 type, RwTexture* texture, CVector* posn, float topX, float topY, float rightX, float rightY, int16 intensity, uint8 red, uint8 greeb, uint8 blue, float drawDistance, uint32 time, float upDistance) {
    // Find a free slot
    int32 idx = 0;
    for (; idx < (int32)aPermanentShadows.size(); idx++) {
        if (aPermanentShadows[idx].m_nType == SHADOW_NONE) {
            break;
        }
    }

    // None free => replace the oldest one that has a sufficiently small front and side vector
    if (idx >= (int32)aPermanentShadows.size()) {
        uint32 oldestTime = UINT32_MAX;
        for (auto i = 0u; i < aPermanentShadows.size(); i++) {
            const auto& shdw = aPermanentShadows[i];
            if (shdw.m_fFrontY * shdw.m_fFrontY + shdw.m_fFrontX * shdw.m_fFrontX >= 0.25f) {
                continue;
            }
            if (shdw.m_fSideX * shdw.m_fSideX + shdw.m_fSideY * shdw.m_fSideY >= 0.25f) {
                continue;
            }
            if (shdw.m_nTimeCreated < oldestTime) {
                idx = (int32)i;
                oldestTime = shdw.m_nTimeCreated;
            }
        }
    }
    if (idx >= (int32)aPermanentShadows.size()) {
        return;
    }

    auto& shdw = aPermanentShadows[idx];
    shdw.m_nType        = (eShadowType)type;
    shdw.m_pTexture     = texture;
    shdw.m_vecPosn      = *posn;
    shdw.m_fFrontX      = topX;
    shdw.m_fFrontY      = topY;
    shdw.m_fSideX       = rightX;
    shdw.m_fSideY       = rightY;
    shdw.m_nIntensity   = intensity;
    shdw.m_nRed         = red;
    shdw.m_nGreen       = greeb;
    shdw.m_nBlue        = blue;
    shdw.m_fZDistance   = drawDistance;
    shdw.m_nTimeDuration = time;
    shdw.m_fScale       = upDistance;
    shdw.m_nTimeCreated = CTimer::GetTimeInMS();
}

// 0x70C950
void CShadows::UpdatePermanentShadows() {
    ZoneScoped;

    for (auto& shdw : aPermanentShadows) {
        if (shdw.m_nType == SHADOW_NONE) {
            continue;
        }

        const auto elapsed = CTimer::GetTimeInMS() - shdw.m_nTimeCreated;
        if (elapsed < shdw.m_nTimeDuration) {
            int16 intensity;
            uint8 red, green, blue;
            if (elapsed < shdw.m_nTimeDuration * 3 / 4) {
                intensity = (int16)shdw.m_nIntensity;
                red       = shdw.m_nRed;
                green     = shdw.m_nGreen;
                blue      = shdw.m_nBlue;
            } else {
                // Fade out during the last quarter of the duration
                const auto fadeProgress = (float)(elapsed - shdw.m_nTimeDuration * 3 / 4) / (float)(shdw.m_nTimeDuration / 4);
                const auto fadeFactor   = 1.0f - fadeProgress;
                blue      = (uint8)NOTSA_FTOL(int32, (float)shdw.m_nBlue * fadeFactor);
                green     = (uint8)NOTSA_FTOL(int32, (float)shdw.m_nGreen * fadeFactor);
                red       = (uint8)NOTSA_FTOL(int32, (float)shdw.m_nRed * fadeFactor);
                intensity = (int16)NOTSA_FTOL(int32, (float)(int16)shdw.m_nIntensity * fadeFactor); // BUG: Original sign extends the intensity here, but not in the branch above
            }
            const auto stored = StoreStaticShadow(
                (uint32)(uintptr_t)&shdw, // The shadow's address is used as its ID
                shdw.m_nType,
                shdw.m_pTexture,
                shdw.m_vecPosn,
                shdw.m_fFrontX, shdw.m_fFrontY,
                shdw.m_fSideX, shdw.m_fSideY,
                intensity,
                red, green, blue,
                shdw.m_fZDistance,
                1.0f,
                40.0f,
                false,
                0.0f
            );
            if (stored || shdw.m_nType == SHADOW_OIL_5) {
                continue;
            }
        }
        shdw.m_nType = SHADOW_NONE;
    }

    // Distance between 2 points, with the same order of float operations as the original
    const auto Dist = [](const CVector& a, const CVector& b) {
        const auto d = a - b;
        return std::sqrt(d.z * d.z + d.y * d.y + d.x * d.x);
    };

    // Oil fire spreading, every 4th frame
    if ((CTimer::m_FrameCounter & 3) != 0) {
        return;
    }

    for (auto i = 0u; i < aPermanentShadows.size(); i++) {
        auto& shdw = aPermanentShadows[i];
        if (shdw.m_nType != SHADOW_OIL_2) { // Oil on fire
            continue;
        }

        shdw.m_nType         = SHADOW_OIL_3;
        shdw.m_nTimeCreated  = CTimer::GetTimeInMS();
        shdw.m_nTimeDuration = 2000;

        // Find the closest (non burning) oil puddle to set on fire
        int32 closest{ -1 };
        float closestDist{ 3.0f };
        for (auto j = 0u; j < aPermanentShadows.size(); j++) {
            const auto& other = aPermanentShadows[j];
            if (other.m_nType != SHADOW_OIL_1 && other.m_nType != SHADOW_OIL_5) {
                continue;
            }
            const auto dist = Dist(shdw.m_vecPosn, other.m_vecPosn);
            if (dist < closestDist) {
                closest     = (int32)j;
                closestDist = dist;
            }
        }
        if (closest < 0) {
            continue;
        }

        auto& closestShdw = aPermanentShadows[closest];
        closestShdw.m_nType = SHADOW_OIL_4;
        gFireManager.StartFire(closestShdw.m_vecPosn, 1.8f, 0, nullptr, 2000, 0, 1);

        // Find another one, on the opposite side of the first one (relative to this shadow)
        const auto toClosestX = closestShdw.m_vecPosn.x - shdw.m_vecPosn.x;
        const auto toClosestY = closestShdw.m_vecPosn.y - shdw.m_vecPosn.y;
        int32 otherIdx{ -1 };
        float otherDist{ 3.0f };
        for (auto j = 0u; j < aPermanentShadows.size(); j++) {
            const auto& cand = aPermanentShadows[j];
            if (cand.m_nType != SHADOW_OIL_1 && cand.m_nType != SHADOW_OIL_5) {
                continue;
            }
            const auto dist = Dist(shdw.m_vecPosn, cand.m_vecPosn);
            if (!(dist < otherDist)) {
                continue;
            }
            if (!((cand.m_vecPosn.y - shdw.m_vecPosn.y) * toClosestY + (cand.m_vecPosn.x - shdw.m_vecPosn.x) * toClosestX < 0.0f)) {
                continue;
            }
            otherIdx  = (int32)j;
            otherDist = dist;
        }
        if (otherIdx < 0) {
            continue;
        }

        auto& otherShdw = aPermanentShadows[otherIdx];
        otherShdw.m_nType = SHADOW_OIL_4;
        gFireManager.StartFire(otherShdw.m_vecPosn, 1.8f, 0, nullptr, 2000, 0, 1);
    }

    // Shadows set on fire this time will now spread fire themselves the next time
    for (auto& shdw : aPermanentShadows) {
        if (shdw.m_nType == SHADOW_OIL_4) {
            shdw.m_nType = SHADOW_OIL_2;
        }
    }
}

// 0x707930
void CShadows::StoreShadowToBeRendered(uint8 type, const CVector& posn, float frontX, float frontY, float sideX, float sideY, int16 intensity, uint8 red, uint8 green, uint8 blue) {
    const auto Store = [=](auto mtype, auto texture) {
        StoreShadowToBeRendered(mtype, texture, posn, frontX, frontY, sideX, sideY, intensity, red, green, blue, 15.0f, 0, 1.0f, nullptr, 0);
    };

    switch (type) {
    case SHADOW_DEFAULT:
        Store(SHADOW_TEX_CAR, gpShadowCarTex);
        break;
    case SHADOW_ADDITIVE:
        Store(SHADOW_TEX_CAR, gpShadowPedTex);
        break;
    case SHADOW_INVCOLOR:
        Store(SHADOW_TEX_PED, gpShadowExplosionTex);
        break;
    case SHADOW_OIL_1:
        Store(SHADOW_TEX_CAR, gpShadowHeliTex);
        break;
    case SHADOW_OIL_2:
        Store(SHADOW_TEX_PED, gpShadowHeadLightsTex);
        break;
    case SHADOW_OIL_3:
        Store(SHADOW_TEX_CAR, gpBloodPoolTex);
        break;
    default:
        return;
    }
}

// 0x707390
void CShadows::StoreShadowToBeRendered(uint8 type, RwTexture* texture, const CVector& posn, float topX, float topY, float rightX, float rightY, int16 intensity, uint8 red, uint8 green, uint8 blue, float zDistance, bool drawOnWater, float scale, CRealTimeShadow* realTimeShadow, bool drawOnBuildings) {
    if (ShadowsStoredToBeRendered >= asShadowsStored.size())
        return;

    auto& shadow = asShadowsStored[ShadowsStoredToBeRendered];

    shadow.m_nType      = (eShadowType)type;
    shadow.m_pTexture   = texture;
    shadow.m_vecPosn    = posn;
    shadow.m_Front.x    = topX;
    shadow.m_Front.y    = topY;
    shadow.m_Side.x     = rightX;
    shadow.m_Side.y     = rightY;
    shadow.m_nIntensity = intensity;
    shadow.m_nRed       = red;
    shadow.m_nGreen     = green;
    shadow.m_nBlue      = blue;
    shadow.m_fZDistance = zDistance;
    shadow.m_bDrawOnWater     = drawOnWater;
    shadow.m_bDrawOnBuildings = drawOnBuildings;
    shadow.m_fScale     = scale;
    shadow.m_pRTShadow  = realTimeShadow;

    ShadowsStoredToBeRendered++;
}

void CShadows::StoreShadowToBeRendered(eShadowType type, RwTexture* tex, const CVector& posn, CVector2D top, CVector2D right, int16 intensity, uint8 red, uint8 green, uint8 blue, float zDistance, bool drawOnWater, float scale, CRealTimeShadow* realTimeShadow, bool drawOnBuildings) {
    StoreShadowToBeRendered(
        type,
        tex,
        posn,
        top.x, top.y,
        right.x, right.y,
        intensity,
        red, green, blue,
        zDistance,
        drawOnWater,
        scale,
        realTimeShadow,
        drawOnBuildings
    );
}

// 0x707460
void CShadows::SetRenderModeForShadowType(eShadowType type) {
    switch (type) {
    case SHADOW_DEFAULT:
    case SHADOW_OIL_1:
    case SHADOW_OIL_2:
    case SHADOW_OIL_3:
    /* case SHADOW_OIL_4: */ // missing
    case SHADOW_OIL_5:
        RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDSRCALPHA));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));
        break;
    case SHADOW_ADDITIVE:
        RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDONE));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDONE));
        break;
    case SHADOW_INVCOLOR:
        RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDZERO));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCCOLOR));
        break;
    default:
        return;
    }
}

// 0x7074F0
void CShadows::RemoveOilInArea(float minX, float maxX, float minY, float maxY) {
    for (auto& shadow : aPermanentShadows) {
        switch (shadow.m_nType) {
        case SHADOW_OIL_1:
        case SHADOW_OIL_5:
            break;
        default:
            continue;
        }
        // x87 `fcomp` + `test ah, 0x41` / `test ah, 5`: the bounds are STRICT (a point on an edge is outside) and a NaN operand rejects (unlike CRect::IsPointInside)
        if (minX < shadow.m_vecPosn.x && shadow.m_vecPosn.x < maxX
            && minY < shadow.m_vecPosn.y && shadow.m_vecPosn.y < maxY
        ) {
            shadow.m_nType = SHADOW_NONE;
        }
    }
}

// 0x707550
void CShadows::GunShotSetsOilOnFire(const CVector& shotOrigin, const CVector& shotTarget) {
    const auto V3DChangeZ = [] (const CVector& pos) {
        return CVector{ pos.x, pos.y, pos.z * 0.27f };
    };

    const CColLine line{V3DChangeZ(shotOrigin), V3DChangeZ(shotTarget)};

    // Find closest shadow (but not further than 1 unit)
    CPermanentShadow* closest{};
    auto              closestDist{1.f};
    for (auto& shdw : aPermanentShadows) {
        switch (shdw.m_nType) {
        case SHADOW_OIL_1:
        case SHADOW_OIL_5:
            break;
        default:
            continue;
        }

        const auto dist = line.DistTo(V3DChangeZ(shdw.m_vecPosn));
        if (dist < closestDist) {
            closestDist = dist;
            closest     = &shdw;
        }
    }

    if (closest) {
        closest->m_nType = SHADOW_OIL_2;
        gFireManager.StartFire(closest->m_vecPosn, 1.8f, 0, nullptr, 2000u, 0, 1);
    }
}

// 0x7076B0
void CShadows::PrintDebugPoly(CVector* a, CVector* b, CVector* c) {
    // NOP
}

// 0x7076C0
void CShadows::CalcPedShadowValues(
    CVector sunPosn,
    float& frontX,        float& frontY,
    float& sideX,         float& sideY,
    float& displacementX, float& displacementY
) {
    // 0x7076C0: the exe stores -x / -y into the outputs first, runs the x87 math on those, spills every result to float and halves each output in a separate step
    frontX = -sunPosn.x;
    frontY = -sunPosn.y;
    const x87d sunDist = std::sqrt((x87d)frontX * frontX + (x87d)frontY * frontY);
    const x87d recip   = (x87d)1.0 / sunDist;
    const x87d mult    = (sunDist + (x87d)1.0) * recip;

    displacementX = frontX; // the duplicated -x / -y stay on the x87 stack and are stored as the displacement
    displacementY = frontY;
    frontX = (float)(frontX * mult);
    frontY = (float)(frontY * mult);
    sideX  = (float)(-((x87d)sunPosn.y * recip));
    sideY  = (float)((x87d)sunPosn.x * recip);

    for (float* v : { &frontX, &frontY, &sideX, &sideY, &displacementX, &displacementY }) {
        *v = (float)((x87d)*v * 0.5f);
    }
}

// 0x707850
void CShadows::AffectColourWithLighting(
    eShadowType shadowType,
    uint8 dayNightIntensity, // packed 2x4 bits for day/night
    uint8 r, uint8 g, uint8 b,
    uint8& outR, uint8& outG, uint8& outB
) {
    if (shadowType != SHADOW_ADDITIVE) {
        const auto mult = std::min(
            0.4f + 0.6f * (1.f - CCustomBuildingDNPipeline::m_fDNBalanceParam),
            // exe: nibbles are multiplied by 1/30 (0x858F10 = 0.033333335f, NOT divided by 30) and blended as `(1 - t) * day + night * t`
            0.3f + 0.7f * lerpBlend(
                (float)(dayNightIntensity >> 0 & 0b1111) * 0.033333335f,
                (float)(dayNightIntensity >> 4 & 0b1111) * 0.033333335f,
                CCustomBuildingDNPipeline::m_fDNBalanceParam
            )
        );
        outR = NOTSA_FTOL(uint8, (float)r * mult);
        outG = NOTSA_FTOL(uint8, (float)g * mult);
        outB = NOTSA_FTOL(uint8, (float)b * mult);
    } else {
        outR = r;
        outG = g;
        outB = b;
    }
}

// NOTSA
uint16 CalculateShadowStrength(float currDist, float maxDist, uint16 maxStrength) {
    assert(maxDist >= currDist); // Otherwise integer underflow will occur

    const auto halfMaxDist = maxDist / 2.f;
    if (currDist >= halfMaxDist) { // Anything further than half the distance is faded out
        return NOTSA_FTOL(uint16, (1.f - (currDist - halfMaxDist) / halfMaxDist) * maxStrength);
    } else { // Anything closer than half the max distance is full strength
        return (uint16)maxStrength;
    }
}

// 0x707B40
void CShadows::StoreShadowForPedObject(CPed* ped, float displacementX, float displacementY, float frontX, float frontY, float sideX, float sideY) {
    // Okay, so.
    // This function is called from `CCutsceneObject::PreRender`
    // And you might ask "what the fuck, an object is not a ped!!"
    // well, in R* world it is.
    // it has bones (as anything can have bones actually, that's a known fact)
    // the `GetBonePosition` below *just works* because it doesn't access any
    // `CPed` specific member variables.
    // But we really should fix this in a sensible way in the future.
    assert(ped->GetIsTypePed() || ped->GetIsTypeObject());

    const auto  bonePos           = ped->GetBonePosition(BONE_ROOT);
    const auto& camPos            = TheCamera.GetPosition();
    const auto  boneToCamDist2DSq = (bonePos - camPos).SquaredMagnitude2D();

    // Check if ped is close enough
    if (boneToCamDist2DSq >= MAX_DISTANCE_PED_SHADOWS_SQR) {
        return;
    }

    const auto isPlayerPed = FindPlayerPed() == ped;

    // Check if ped is visible to the camera
    if (!isPlayerPed) {  // Optimization: Assume player ped is always visible
        if (!TheCamera.IsSphereVisible(ped->GetPosition(), 2.f)) {
            return;
        }
    }

    // Now store a shadow to be rendered
    const auto strength = (uint8)CalculateShadowStrength(std::sqrt(boneToCamDist2DSq), MAX_DISTANCE_PED_SHADOWS, CTimeCycle::m_CurrentColours.m_nShadowStrength);
    StoreShadowToBeRendered(
        SHADOW_DEFAULT,
        gpShadowPedTex,
        bonePos + CVector{displacementX, displacementY, 0.f},
        frontX, frontY,
        sideX, sideY,
        strength,
        strength, strength, strength,
        4.f,
        false,
        1.f,
        nullptr,
        NOTSA_EXE_OR(isPlayerPed, isPlayerPed || g_fx.GetFxQuality() >= FX_QUALITY_VERY_HIGH) // NOTSA: At higher FX quality draw all ped's shadows (the exe passes `isPlayerPed`, 0x707C84; NOTSA-AUDIT)
    );
}

// 0x707CA0
void CShadows::StoreRealTimeShadow(CPhysical* physical, float displacementX, float displacementY, float frontX, float frontY, float sideX, float sideY) {
    const auto rtshdw = physical->m_pShadowData;
    if (!rtshdw) {
        return;
    }
    const auto& camPos = TheCamera.GetPosition();
    const auto  shdwPos = physical->GetIsTypePed()
        ? physical->AsPed()->GetBonePosition(BONE_ROOT)
        : physical->GetPosition();
    const auto shdwToCamDist2DSq = (shdwPos - camPos).SquaredMagnitude2D();

    // Check distance to camera
    if (shdwToCamDist2DSq > MAX_DISTANCE_PED_SHADOWS_SQR) {
        return;
    }

    // Check if the object is visible to the camera
    if (FindPlayerPed() != physical) {  // Optimization: Assume player ped is always visible
        if (!TheCamera.IsSphereVisible(shdwPos, 2.f)) {
            return;
        }
    }

    const auto strength = (float)CalculateShadowStrength(std::sqrt(shdwToCamDist2DSq), MAX_DISTANCE_PED_SHADOWS, CTimeCycle::m_CurrentColours.m_nShadowStrength);
    const auto cc       = NOTSA_FTOL(uint8, (float)rtshdw->m_nIntensity * ExeRecip(100.f) * strength);

    const auto& vecToSun = CTimeCycle::m_VectorToSun[CTimeCycle::m_CurrentStoredValue];
    const auto lightFrame = rtshdw->SetLightProperties(
        RadiansToDegrees(+x87::atan2(-vecToSun.x, -vecToSun.y)),
        RadiansToDegrees(-x87::atan2(+vecToSun.x, -vecToSun.z)),
        true
    );
    CalcPedShadowValues(
        *RwMatrixGetAt(RwFrameGetMatrix(lightFrame)),
        displacementX, displacementY,
        frontX, frontY,
        sideX, sideY
    );
    StoreShadowToBeRendered(
        SHADOW_INVCOLOR,
        rtshdw->GetShadowRwTexture(),
        shdwPos - CVector{ displacementX, displacementY, 0.f } * 2.5f,
        frontX * 1.5f, frontY * 1.5f,
        sideX * 1.5f, sideY * 1.5f,
        cc,
        cc, cc, cc,
        4.f,
        false,
        1.f,
        rtshdw,
        NOTSA_EXE_OR(false, g_fx.GetFxQuality() >= FX_QUALITY_VERY_HIGH) // NOTSA: At higher FX quality draw all shadows on buildings too (the exe passes 0, 0x707EBD; NOTSA-AUDIT)
    );
}

// 0x707F40
void CShadows::UpdateStaticShadows() {
    // Remove shadows that have no polies/are temporary and have expired
    for (auto& sshdw : aStaticShadows) {
        if (!sshdw.m_pPolyBunch || sshdw.m_bJustCreated) {
            goto skip; // Not even created fully
        }

        if (sshdw.m_bTemporaryShadow && CTimer::GetTimeInMS() <= sshdw.m_nTimeCreated + 5000u) {
            goto skip; // Not expired yet
        }

        sshdw.Free();

    skip:
        sshdw.m_bJustCreated = false;
    }
}

// 0x707FA0
void CShadows::RenderExtraPlayerShadows() {
    if (!CTimeCycle::m_CurrentColours.m_nShadowStrength) {
        return;
    }

    const auto plyrVeh = FindPlayerVehicle();
    if (!plyrVeh) {
        return;
    }

    if (plyrVeh->m_nModelIndex == eModelID::MODEL_RCBANDIT) {
        return;
    }

    if (plyrVeh->GetVehicleAppearance() == VEHICLE_APPEARANCE_BIKE) {
        return;
    }

    switch (plyrVeh->m_nVehicleType) {
    case VEHICLE_TYPE_BIKE:
    case VEHICLE_TYPE_FPLANE:
    case VEHICLE_TYPE_BOAT:
        return;
    }

    const auto  plyrPos    = FindPlayerCoors();
    const auto& plyrVehPos = plyrVeh->GetPosition();
    const auto& plyVehMat  = plyrVeh->GetMatrix();
    for (auto& ptl : CPointLights::GetActiveLights()) {
        if (ptl.m_nType != ePointLightType::PLTYPE_POINTLIGHT) {
            continue;
        }
        if (!ptl.m_bGenerateShadows) {
            continue;
        }
        if (ptl.m_fColorRed == 0.f && ptl.m_fColorGreen == 0.f && ptl.m_fColorBlue == 0.f) { // If it's black => ignore
            continue;
        }

        const auto lightToPlyr{ ptl.m_vecPosn - plyrPos };
        const auto lightToPlyrDist = lightToPlyr.Magnitude();
        if (lightToPlyrDist >= ptl.m_fRadius || lightToPlyrDist == 0.f) { // NOTSA: Zero check to prevent (possible) division-by-zero
            continue;
        }

        const auto lightToPlyrDir  = lightToPlyr / lightToPlyrDist; // Normalize vector

        const auto& plyrVehBB = plyrVeh->GetColModel()->GetBoundingBox();
        const auto  shdwSize  = CVector2D{ plyrVehBB.GetSize() } / 2.f;

        StoreShadowToBeRendered(
            SHADOW_DEFAULT,
            gpShadowCarTex,
            plyrVehPos - CVector{
                  CVector2D{ lightToPlyr / lightToPlyrDist } * 1.2f // Compensate for elvation of the light
                - CVector2D{ plyVehMat.GetForward() } * (shdwSize.y - plyrVehBB.m_vecMax.y) // Move point to center (as the shadow's position is it's center)
            },
            CVector2D{ plyVehMat.GetForward() } * shdwSize.y,
            CVector2D{ plyVehMat.GetRight() } * (plyVehMat.GetUp().z >= 0.f ? shdwSize.x : -shdwSize.x), // If vehicle is flipped, we gotta flip the `right` vector back
            CalculateShadowStrength( // 0x70809A
                lightToPlyrDist,
                ptl.m_fRadius,
                5 * CTimeCycle::m_CurrentColours.m_nShadowStrength / 8u // Same as mult by `0.625` and then casting to int
            ),
            0, 0, 0,
            4.5f,
            false,
            1.f,
            nullptr,
            false
        );
    }
}

// 0x708300
void CShadows::RenderStaticShadows() {
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,          RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,            RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,        RWRSTATE(rwFILTERLINEAR));

    RenderBuffer::ClearRenderBuffer();

    // Mark all as not-yet-rendered
    for (auto& shdw : aStaticShadows) {
        shdw.m_bRendered = false;
    }

    // Render all in batches
    for (auto& oshdw : aStaticShadows) {
        if (!oshdw.m_pPolyBunch || oshdw.m_bRendered) {
            continue;
        }

        // Setup additional render states for this shadow
        SetRenderModeForShadowType(oshdw.m_nType);
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(oshdw.m_pTexture)));
         
        // Batch all other shadows with the same texture and type into the buffer
        for (auto& ishdw : aStaticShadows) {
            if (!ishdw.m_pPolyBunch) {
                continue;
            }
            if (ishdw.m_nType != oshdw.m_nType) {
                continue;
            }
            if (ishdw.m_pTexture != oshdw.m_pTexture) {
                continue;
            }
            // No need to check if this one was rendered (because of the batching)
            
            // Render polies of this shadow
            for (auto poly = ishdw.m_pPolyBunch; poly; poly = poly->m_pNext) {
                // 0x70841F: Calculate color
                uint8 r, g, b;
                CShadows::AffectColourWithLighting(
                    ishdw.m_nType,
                    ishdw.m_nDayNightIntensity,
                    ishdw.m_nRed, ishdw.m_nGreen, ishdw.m_nBlue,
                    r, g, b
                );

                const auto totalNoIdx = 3 * (poly->m_wNumVerts - 2); // Total no. of indices we'll use

                // 0x708432: Begin render buffer store
                RwIm3DVertex*    vtxIt{};
                RwImVertexIndex* vtxIdxIt{};
                RenderBuffer::StartStoring(
                    totalNoIdx,
                    poly->m_wNumVerts,
                    vtxIdxIt, vtxIt
                );

                // 0x70851D: Write vertices (`if` not necessary, it's part of the loop condition)
                const auto a = NOTSA_FTOL(uint8, (float)(int16)ishdw.m_nIntensity * (1.f - CWeather::Foggyness * 0.5f)); // exe: `movsx` + `fimul`, so a "negative" intensity stays negative
                for (auto i{ 0 }; i < poly->m_wNumVerts; i++, vtxIt++) {
                    const auto& pos = poly->m_avecPosn[i];
                    RwIm3DVertexSetPos(vtxIt, pos.x, pos.y, pos.z + 0.06f);
                    RwIm3DVertexSetRGBA(vtxIt, r, g, b, a);
                    RwIm3DVertexSetU(vtxIt, (float)poly->m_aU[i] * ExeRecip(200.f));
                    RwIm3DVertexSetV(vtxIt, (float)poly->m_aV[i] * ExeRecip(200.f));
                }

                // 0x7085BC: Write indices  (`if` not necessary, it's part of the loop condition)
                for (auto i = 0; i < totalNoIdx; i++) {
                    *vtxIdxIt++ = g_ShadowVertices[i];
                }
                
                // Finish storing 
                RenderBuffer::StopStoring();

                // Mark this as rendered
                ishdw.m_bRendered = true;
            }
        }

        // Render out this batch
        RenderBuffer::RenderStuffInBuffer();
    }

    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
}

// 0x7086B0
void CShadows::CastShadowEntityXY(CEntity* entity, float conrerAX, float cornerAY, float cornerBX, float cornerBY, CVector* posn, float frontX, float frontY, float sideX, float sideY, int16 intensity, uint8 red, uint8 green, uint8 blue, float zDistance, float scale, CPolyBunch** ppPolyBunch, uint8* pDayNightIntensity, int32 shadowType) {
    // NOTE: `conrerAX`, `cornerAY`, `cornerBX` and `cornerBY` aren't used by the original code

    // A vertex of the polygon that is being clipped
    struct ClipVert {
        CVector2D uv;
        CVector   pos; // `z` is calculated after clipping
    };
    constexpr size_t MAX_CLIP_VERTS = 10; // Original buffers have room for 10 vertices

    auto* const colModel = entity->GetModelInfo()->GetColModel();
    auto* const colData  = colModel->m_pColData;
    if (!colData || colData->m_nNumTriangles == 0) {
        return;
    }
    CCollision::CalculateTrianglePlanes(colModel);

    const CMatrix& entityMat = entity->GetMatrix();
    const CVector& mRight    = entityMat.GetRight();
    const CVector& mFwd      = entityMat.GetForward();
    const CVector  entityPos = entity->GetPosition();

    // Transform the shadow's `front` and `side` into the entity's space
    const float frontLocalX = frontX * mRight.x + frontY * mRight.y;
    const float frontLocalY = frontY * mFwd.y + frontX * mFwd.x;
    const float sideLocalX  = sideX * mRight.x + sideY * mRight.y;
    const float sideLocalY  = sideY * mFwd.y + sideX * mFwd.x;

    // Transform the shadow's center into the entity's space
    const float relX = posn->x - entityPos.x;
    const float relY = posn->y - entityPos.y;
    const float cX   = relX * mRight.x + relY * mRight.y;
    const float cY   = relX * mFwd.x + relY * mFwd.y;

    // The shadow's 4 corners in the entity's space
    const std::array<CVector2D, 4> corners{{
        { (cX + frontLocalX) - sideLocalX,  (frontLocalY + cY) - sideLocalY  },
        { (cX + sideLocalX) + frontLocalX,  (sideLocalY + cY) + frontLocalY  },
        { sideLocalX + (cX - frontLocalX),  (cY - frontLocalY) + sideLocalY  },
        { (cX - frontLocalX) - sideLocalX,  (cY - frontLocalY) - sideLocalY  },
    }};

    const float zLocal = posn->z - entityPos.z;

    // Bounding rect of the corners
    const float minX = std::min({ corners[0].x, corners[1].x, corners[2].x, corners[3].x });
    const float maxX = std::max({ corners[0].x, corners[1].x, corners[2].x, corners[3].x });
    const float minY = std::min({ corners[0].y, corners[1].y, corners[2].y, corners[3].y });
    const float maxY = std::max({ corners[0].y, corners[1].y, corners[2].y, corners[3].y });

    // Clips the polygon `in` against the half plane defined by point `a` and the direction `d`,
    // and keeps the part where the cross product is `> 0`. Returns the number of vertices written to `out`.
    const auto ClipAgainstEdge = [](const ClipVert* in, int32 numIn, ClipVert* out, const CVector& a, float dx, float dy) {
        const auto Side = [&](const ClipVert& v) {
            return (v.pos.x - a.x) * dy - (v.pos.y - a.y) * dx;
        };
        // Intersection of the edge `prev -> cur`
        const auto Intersect = [&](const ClipVert& prev, const ClipVert& cur, float curSide) {
            float t = std::abs(Side(prev));
            t       = t / (std::abs(curSide) + t);
            const float s = 1.f - t;
            return ClipVert{
                .uv  = { s * prev.uv.x + t * cur.uv.x, s * prev.uv.y + t * cur.uv.y },
                .pos = { s * prev.pos.x + t * cur.pos.x, s * prev.pos.y + t * cur.pos.y, 0.f }
            };
        };

        if (numIn <= 0) { // NOTSA: The original reads stale buffer data here, but never emits anything in this case
            return 0;
        }

        int32 numOut = 0;
        int16 state  = 0; // 0 = Nothing processed yet, 1 = previous was in, 2 = previous was out
        for (int32 i = 0; i < numIn; i++) {
            const float side = Side(in[i]);
            if (!(side > 0.f)) { // exe `test ah, 0x41` / `jne`: a NaN side counts as "out"
                if (state == 1) {
                    out[numOut++] = Intersect(in[i - 1], in[i], side);
                }
                state = 2;
            } else {
                if (state == 2) {
                    out[numOut++] = Intersect(in[i - 1], in[i], side);
                }
                out[numOut++] = in[i];
                state         = 1;
            }
        }

        // Close the polygon
        const float firstSide = Side(in[0]);
        if ((firstSide > 0.f && state == 2) || (firstSide <= 0.f && state == 1)) {
            out[numOut++] = Intersect(in[numIn - 1], in[0], firstSide);
        }
        return numOut;
    };

    // Casts the shadow onto the triangle with the given index
    const auto ProcessTriangle = [&](int32 triIdx) {
        const auto& plane = colData->m_pTrianglePlanes[triIdx];
        const CVector normal = plane.GetNormal();
        if (!(std::abs(normal.z) > 0.1f)) { // Ignore (almost) vertical triangles
            return;
        }

        const auto& tri = colData->m_pTriangles[triIdx];
        CVector p0, p1, p2;
        colData->GetTrianglePoint(p0, tri.vA);
        colData->GetTrianglePoint(p1, tri.vB);
        colData->GetTrianglePoint(p2, tri.vC);

        // Rough overlap test
        if (!(   (minX < p0.x || minX < p1.x || minX < p2.x)
              && (p0.x < maxX || p1.x < maxX || p2.x < maxX)
              && (minY < p0.y || minY < p1.y || minY < p2.y)
              && (p0.y < maxY || p1.y < maxY || p2.y < maxY)
              && (p0.z < zLocal || p1.z < zLocal || p2.z < zLocal)
        )) {
            return;
        }
        const float zLowest = zLocal - zDistance;
        if (!(zLowest < p0.z || zLowest < p1.z || zLowest < p2.z)) {
            return;
        }

        // Clip the shadow's quad by the triangle's edges
        std::array<ClipVert, MAX_CLIP_VERTS> bufA, bufB;
        bufA[0] = { { 0.f, 0.f }, { corners[0].x, corners[0].y, 0.f } };
        bufA[1] = { { 1.f, 0.f }, { corners[1].x, corners[1].y, 0.f } };
        bufA[2] = { { 1.f, 1.f }, { corners[2].x, corners[2].y, 0.f } };
        bufA[3] = { { 0.f, 1.f }, { corners[3].x, corners[3].y, 0.f } };

        int32 n = ClipAgainstEdge(bufA.data(), 4, bufB.data(), p0, p1.x - p0.x, p1.y - p0.y);
        n       = ClipAgainstEdge(bufB.data(), n, bufA.data(), p1, p2.x - p1.x, p2.y - p1.y);
        n       = ClipAgainstEdge(bufA.data(), n, bufB.data(), p2, p0.x - p2.x, p0.y - p2.y);
        if (n <= 2) {
            return;
        }
        auto& poly = bufB; // The clipped polygon

        // Calculate the Z coordinates by projecting the polygon onto the triangle's plane
        const float planeD = p0.z * normal.z + normal.y * p0.y + normal.x * p0.x; // z, y, x order like the exe (0x7096F9)
        const float invNz  = -1.f / normal.z;
        for (int32 i = 0; i < n; i++) {
            poly[i].pos.z = ((normal.x * poly[i].pos.x + normal.y * poly[i].pos.y) - planeD) * invNz;
        }

        // Transform into world space
        for (int32 i = 0; i < n; i++) {
            const float x = poly[i].pos.x;
            const float y = poly[i].pos.y;
            poly[i].pos.x = mRight.x * x + mFwd.x * y + entityPos.x;
            poly[i].pos.y = x * mRight.y + mFwd.y * y + entityPos.y;
            poly[i].pos.z = entityPos.z + poly[i].pos.z;
        }

        *pDayNightIntensity = tri.m_nLight.value;

        if (!ppPolyBunch) {
            // Render directly
            const int32 numIdx = n * 3 - 6;

            RwImVertexIndex* idxIt{};
            RwIm3DVertex*    vtxIt{};
            RenderBuffer::StartStoring(numIdx, n, idxIt, vtxIt);

            uint8 r, g, b;
            AffectColourWithLighting((eShadowType)shadowType, *pDayNightIntensity, red, green, blue, r, g, b);

            for (int32 i = 0; i < n; i++, vtxIt++) {
                RwIm3DVertexSetPos(vtxIt, poly[i].pos.x, poly[i].pos.y, poly[i].pos.z + 0.06f);
                RwIm3DVertexSetRGBA(vtxIt, r, g, b, (uint8)intensity);
                RwIm3DVertexSetU(vtxIt, scale * poly[i].uv.x);
                RwIm3DVertexSetV(vtxIt, scale * poly[i].uv.y);
            }
            for (int32 i = 0; i < numIdx; i++) {
                *idxIt++ = g_ShadowVertices[i];
            }

            RenderBuffer::StopStoring();
        } else if (pEmptyBunchList) {
            // Store in a poly bunch
            auto* const bunch = pEmptyBunchList;
            pEmptyBunchList   = bunch->m_pNext;
            bunch->m_pNext    = *ppPolyBunch;
            *ppPolyBunch      = bunch;
            bunch->m_wNumVerts = (int16)n;
            for (int32 i = 0; i < n; i++) {
                bunch->m_avecPosn[i] = poly[i].pos;
                bunch->m_aU[i]       = NOTSA_FTOL(uint8, poly[i].uv.x * 200.f);
                bunch->m_aV[i]       = NOTSA_FTOL(uint8, poly[i].uv.y * 200.f);
            }
        }
    };

    // Casts the shadow onto the triangles [first, last] (inclusive)
    const auto ProcessTriangles = [&](int32 first, int32 last) {
        for (int32 i = first; i <= last; i++) {
            ProcessTriangle(i);
        }
    };

    if (!colData->bHasFaceGroups) {
        ProcessTriangles(0, (int32)colData->m_nNumTriangles - 1);
    } else {
        // NOTE: The original code iterates the face groups from the last to the first
        for (const auto& fg : colData->GetFaceGroups() | rng::views::reverse) {
            if (   fg.bb.m_vecMin.x < maxX
                && minX < fg.bb.m_vecMax.x
                && fg.bb.m_vecMin.y < maxY
                && minY < fg.bb.m_vecMax.y
            ) {
                ProcessTriangles(fg.first, fg.last);
            }
        }
    }
}

// 0x70A040
void CShadows::CastShadowEntityXYZ(CEntity* entity, CVector* posn, float frontX, float frontY, float sideX, float sideY, int16 intensity, uint8 red, uint8 green, uint8 blue, float zDistance, float scale, CPolyBunch** ppPolyBunch, CRealTimeShadow* realTimeShadow) {
    if (!realTimeShadow) {
        return;
    }

    auto* const shdwCam = &realTimeShadow->m_camera;

    auto* const colModel = entity->GetModelInfo()->GetColModel();
    auto* const colData  = colModel->m_pColData;
    if (!colData) {
        return;
    }
    CCollision::CalculateTrianglePlanes(colModel);

    _ProjectionParam param;

    // Camera matrix (tilted down by 45 degrees)
    RwMatrix camMat = *RwFrameGetMatrix(RwCameraGetFrame(shdwCam->m_pRwCamera));
    const RwV3d tiltAxis{ 1.f, 0.f, 0.f };
    RwMatrixRotate(&camMat, &tiltAxis, -45.f, rwCOMBINEPRECONCAT);
    param.at = camMat.at;

    // Transform from the entity's space into the shadow camera's space
    entity->GetMatrix().CopyToRwMatrix(&param.entityMatrix);
    RwMatrixInvert(&param.invMatrix, &camMat);

    // NOTE: Original reads the `x` component (offset 0x68), it's the same as `y` for shadow cameras though
    const auto viewWindowX = shdwCam->m_pRwCamera->viewWindow.x;
    const RwV3d invScale{
        -0.5f / (0.9f * viewWindowX),
        -0.5f / (0.9f * viewWindowX),
        1.0f / (viewWindowX * 0.8f)
    };
    RwMatrixScale(&param.invMatrix, &invScale, rwCOMBINEPOSTCONCAT);
    const RwV3d invTranslate{ 0.5f, 0.f, 0.f };
    RwMatrixTranslate(&param.invMatrix, &invTranslate, rwCOMBINEPOSTCONCAT);

    param.shadowValue = (RwUInt8)intensity;
    param.fade        = false;

    // Entity space matrix [Original recomputes it again here]
    RwMatrix entityMat;
    entity->GetMatrix().CopyToRwMatrix(&entityMat);
    RwMatrix entityMatInv;
    RwMatrixInvert(&entityMatInv, &entityMat);

    // The shadow's sphere in the entity's space
    // NOTE: Original ignores `posn` and uses the center of the real time shadow's base sphere instead
    const auto& baseCenter = realTimeShadow->m_baseSphere.m_vecCenter;
    const RwV3d sphereCenterWorld{
        frontX * -1.1f + baseCenter.x,
        baseCenter.y + frontY * -1.1f,
        baseCenter.z - 0.5f
    };
    RwV3d sphereCenter;
    RwV3dTransformPoints(&sphereCenter, &sphereCenterWorld, 1, &entityMatInv);

    CColSphere sphere;
    sphere.Set(2.0f, CVector{ sphereCenter }, (eSurfaceType)0, 0, tColLighting{ 0xFF });

    // Process all triangles that intersect the sphere
    for (auto i = 0; i < colData->m_nNumTriangles; i++) {
        const auto& tri = colData->m_pTriangles[i];

        CVector pts[3];
        colData->GetTrianglePoint(pts[0], tri.vA);
        colData->GetTrianglePoint(pts[1], tri.vB);
        colData->GetTrianglePoint(pts[2], tri.vC);

        if (!CCollision::TestSphereTriangle(sphere, colData->m_pVertices, tri, colData->m_pTrianglePlanes[i])) {
            continue;
        }

        const CVector normal = colData->m_pTrianglePlanes[i].GetNormal();
        const CVector offset{ normal.x * 0.028f, normal.y * 0.028f, normal.z * 0.028f };
        for (auto& pt : pts) {
            pt.x += offset.x;
            pt.y += offset.y;
            pt.z += offset.z;
        }

        CVector normalCopy = normal;
        if (!ShadowRenderTriangleCB(&normalCopy, pts, &param)) {
            return;
        }
    }
}

// 0x70A470
template<typename PtrListType>
void CShadows::CastPlayerShadowSectorList(
    PtrListType& ptrList,
    float cornerAX,
    float cornerAY,
    float cornerBX,
    float cornerBY,
    CVector* posn,
    float frontX,
    float frontY,
    float sideX,
    float sideY,
    int16 intensity,
    uint8 red,
    uint8 green,
    uint8 blue,
    float zDistance,
    float scale,
    CPolyBunch** ppPolyBunch,
    uint8* pDayNightIntensity,
    int32 shadowType
) {
    for (auto* const entity : ptrList) {
        if (entity->IsScanCodeCurrent()) {
            continue;
        }
        entity->SetCurrentScanCode();

        if (!entity->m_bUsesCollision || entity->m_bDontCastShadowsOn) {
            continue;
        }

        if (!entity->IsInCurrentArea()) {
            continue;
        }

        // If slightly tilted, ignore (exe `test ah, 0x41` / `jne`: a NaN up.z also skips)
        if (!(entity->GetMatrix().GetUp().z > 0.97f)) {
            continue;
        }

        // 0x70A526: the exe tests the four edges STRICTLY (and NaN rejects), unlike CRect::OverlapsWith; `top` is the max Y of the entity rect
        const auto entityRect = entity->GetBoundRect();
        if (!(cornerAX < entityRect.right)) {
            continue;
        }
        if (cornerBX <= entityRect.left) {
            continue;
        }
        if (!(cornerAY < entityRect.top)) {
            continue;
        }
        if (cornerBY <= entityRect.bottom) {
            continue;
        }

        // Quick Z height check of the bounding box (the exe goes through CModelInfo, and a NaN skips the entity)
        const auto& cm         = entity->GetModelInfo()->GetColModel();
        const auto  entityPosZ = entity->GetPosition().z;
        if (!(cm->m_boundBox.m_vecMax.z + entityPosZ > posn->z - zDistance)) {
            continue;
        }
        if (!(cm->m_boundBox.m_vecMin.z + entityPosZ < posn->z)) {
            continue;
        }

        CastShadowEntityXY(
            entity,
            cornerAX,
            cornerAY,
            cornerBX,
            cornerBY,
            posn,
            frontX,
            frontY,
            sideX,
            sideY,
            intensity,
            red,
            green,
            blue,
            zDistance,
            scale,
            ppPolyBunch,
            pDayNightIntensity,
            shadowType
        );
    }
}

// 0x70A630
template<typename PtrListType>
void CShadows::CastShadowSectorList(PtrListType& ptrList, float conrerAX, float cornerAY, float cornerBX, float cornerBY, CVector* posn, float frontX, float frontY, float sideX, float sideY, int16 intensity, uint8 red, uint8 green, uint8 blue, float zDistance, float scale, CPolyBunch** ppPolyBunch, uint8* pDayNightIntensity, int32 shadowType) {
    // Nearly identical to `CastPlayerShadowSectorList`, the differences are the (strict) rect check, and that the "don't cast shadows on" flag isn't checked here
    for (auto* const entity : ptrList) {
        if (entity->IsScanCodeCurrent()) {
            continue;
        }
        entity->SetCurrentScanCode();

        if (!entity->m_bUsesCollision) {
            continue;
        }

        if (!entity->IsInCurrentArea()) {
            continue;
        }

        // If slightly tilted, ignore (a NaN up.z also skips, see the exe's `test ah, 0x41`)
        if (!(entity->GetMatrix().GetUp().z > 0.97f)) {
            continue;
        }

        const auto entityRect = entity->GetBoundRect();
        if (!(conrerAX < entityRect.right)) {
            continue;
        }
        if (cornerBX <= entityRect.left) {
            continue;
        }
        if (!(cornerAY < entityRect.top)) { // `top` is the max Y here
            continue;
        }
        if (cornerBY <= entityRect.bottom) {
            continue;
        }

        // Quick Z height check of the bounding box
        const auto& cm         = entity->GetModelInfo()->GetColModel();
        const auto  entityPosZ = entity->GetPosition().z;
        if (!(cm->m_boundBox.m_vecMax.z + entityPosZ > posn->z - zDistance)) {
            continue;
        }
        if (!(cm->m_boundBox.m_vecMin.z + entityPosZ < posn->z)) {
            continue;
        }

        CastShadowEntityXY(
            entity,
            conrerAX,
            cornerAY,
            cornerBX,
            cornerBY,
            posn,
            frontX,
            frontY,
            sideX,
            sideY,
            intensity,
            red,
            green,
            blue,
            zDistance,
            scale,
            ppPolyBunch,
            pDayNightIntensity,
            shadowType
        );
    }
}

// 0x70A7E0
template<typename PtrListType>
void CShadows::CastRealTimeShadowSectorList(PtrListType& ptrList, float conrerAX, float cornerAY, float cornerBX, float cornerBY, CVector* posn, float frontX, float frontY, float sideX, float sideY, int16 intensity, uint8 red, uint8 green, uint8 blue, float zDistance, float scale, CPolyBunch** ppPolyBunch, CRealTimeShadow* realTimeShadow, uint8* pDayNightIntensity) {
    for (auto* const entity : ptrList) {
        if (entity->IsScanCodeCurrent()) {
            continue;
        }
        entity->SetCurrentScanCode();

        if (!entity->m_bUsesCollision || entity->m_bDontCastShadowsOn) {
            continue;
        }

        if (!entity->IsInCurrentArea()) {
            continue;
        }

        const auto entityRect = entity->GetBoundRect();
        if (!(conrerAX < entityRect.right)) {
            continue;
        }
        if (cornerBX <= entityRect.left) {
            continue;
        }
        if (!(cornerAY < entityRect.top)) { // `top` is the max Y here
            continue;
        }
        if (cornerBY <= entityRect.bottom) {
            continue;
        }

        // Quick Z height check of the bounding box
        const auto& cm         = entity->GetModelInfo()->GetColModel();
        const auto  entityPosZ = entity->GetPosition().z;
        if (!(cm->m_boundBox.m_vecMax.z + entityPosZ > posn->z - zDistance)) {
            continue;
        }
        if (!(cm->m_boundBox.m_vecMin.z + entityPosZ < posn->z)) {
            continue;
        }

        // NOTSA: The original also passes `pDayNightIntensity` here as an additional (unused) argument
        CastShadowEntityXYZ(
            entity,
            posn,
            frontX,
            frontY,
            sideX,
            sideY,
            intensity,
            red,
            green,
            blue,
            zDistance,
            scale,
            ppPolyBunch,
            realTimeShadow
        );
    }
}

// 0x70A960
void CShadows::RenderStoredShadows() {
    // NOTE: The exe sets these render states (and leaves fog / alpha-test / filter changed) even when there are no shadows, so there is no early-out here

    RenderBuffer::ClearRenderBuffer();

    for (auto i = 0; i < ShadowsStoredToBeRendered; i++) {
        asShadowsStored[i].m_bAlreadyRenderedInBatch = false;
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,         RWRSTATE(rwRENDERSTATENARENDERSTATE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,          RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,            RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,       RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwRenderStateSet(rwRENDERSTATECULLMODE,             RWRSTATE(rwCULLMODECULLNONE));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,        RWRSTATE(rwFILTERLINEAR));

    // The exe's bounding rect of a shadow is the position +- (|side| + |front|) per axis (0x70AA7F..)
    struct ShadowRect {
        float minX, minY, maxX, maxY;
    };
    const auto GetShadowRect = [](const CRegisteredShadow& shdw) {
        const float spanX = std::abs(shdw.m_Side.x) + std::abs(shdw.m_Front.x);
        const float spanY = std::abs(shdw.m_Side.y) + std::abs(shdw.m_Front.y);
        return ShadowRect{
            shdw.m_vecPosn.x - spanX, shdw.m_vecPosn.y - spanY,
            shdw.m_vecPosn.x + spanX, shdw.m_vecPosn.y + spanY
        };
    };
    // NOTSA: Using `CWorld::GetSectorX` isn't exact here, as the original multiplies (instead of dividing) by 1/50
    const auto GetSectorCoord = [](float v) {
        return NOTSA_FTOL(int32, std::floor(v * 0.02f + 60.0f));
    };

    for (auto o = 0; o < ShadowsStoredToBeRendered; o++) {
        auto& oshdw = asShadowsStored[o];

        //
        // Render onto ground (If not already)
        //
        if (!oshdw.m_bAlreadyRenderedInBatch) {
            // Setup additional render states for this shadow (and others in the batch below)
            // NOTE: The exe skips this for shadows that were batched earlier, so their water pass below uses the states of the last batch
            SetRenderModeForShadowType(oshdw.m_nType);
            RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(oshdw.m_pTexture)));

            // We do a batched rendering here:
            // All shadows of the same type and texture are rendered together to save on drawcalls

            for (auto i = o; i < ShadowsStoredToBeRendered; i++) {
                auto& ishdw = asShadowsStored[i];

                if (&ishdw != &oshdw) {
                    if (ishdw.m_nType != oshdw.m_nType) {
                        continue;
                    }
                    if (ishdw.m_pTexture != oshdw.m_pTexture) {
                        continue;
                    }
                }

                CWorld::AdvanceCurrentScanCode();

                const auto ishdwRect = GetShadowRect(ishdw);

                // The sector range is clamped to the map (0x70AAA0..), y is the outer loop
                const auto sectorMinX = std::max(GetSectorCoord(ishdwRect.minX), 0);
                const auto sectorMinY = std::max(GetSectorCoord(ishdwRect.minY), 0);
                const auto sectorMaxX = std::min(GetSectorCoord(ishdwRect.maxX), (int32)MAX_SECTORS_X - 1);
                const auto sectorMaxY = std::min(GetSectorCoord(ishdwRect.maxY), (int32)MAX_SECTORS_Y - 1);

                // Do shadow casting
                const auto CastIntoSector = [&](int32 x, int32 y) {
                        auto& sector = CWorld::GetSector(x, y);

                        if (const auto rtshdw = ishdw.m_pRTShadow) {
                            CastRealTimeShadowSectorList(
                                sector.Buildings,
                                ishdwRect.minX, ishdwRect.minY,
                                ishdwRect.maxX, ishdwRect.maxY,
                                &ishdw.m_vecPosn,
                                ishdw.m_Front.x, ishdw.m_Front.y,
                                ishdw.m_Side.x, ishdw.m_Side.y,
                                ishdw.m_nIntensity,
                                ishdw.m_nRed, ishdw.m_nGreen, ishdw.m_nBlue,
                                ishdw.m_fZDistance,
                                ishdw.m_fScale,
                                nullptr,
                                rtshdw,
                                nullptr // Unused
                            );
                        } else {
                            uint8 unused{};
                            if (ishdw.m_bDrawOnBuildings) {
                                CastShadowSectorList(
                                    sector.Buildings,
                                    ishdwRect.minX, ishdwRect.minY,
                                    ishdwRect.maxX, ishdwRect.maxY,
                                    &ishdw.m_vecPosn,
                                    ishdw.m_Front.x, ishdw.m_Front.y,
                                    ishdw.m_Side.x, ishdw.m_Side.y,
                                    ishdw.m_nIntensity,
                                    ishdw.m_nRed, ishdw.m_nGreen, ishdw.m_nBlue,
                                    ishdw.m_fZDistance,
                                    ishdw.m_fScale,
                                    nullptr,
                                    &unused,
                                    ishdw.m_nType
                                );
                            } else {
                                CastPlayerShadowSectorList(
                                    sector.Buildings,
                                    ishdwRect.minX, ishdwRect.minY,
                                    ishdwRect.maxX, ishdwRect.maxY,
                                    &ishdw.m_vecPosn,
                                    ishdw.m_Front.x, ishdw.m_Front.y,
                                    ishdw.m_Side.x, ishdw.m_Side.y,
                                    ishdw.m_nIntensity,
                                    ishdw.m_nRed, ishdw.m_nGreen, ishdw.m_nBlue,
                                    ishdw.m_fZDistance,
                                    ishdw.m_fScale,
                                    nullptr,
                                    &unused,
                                    ishdw.m_nType
                                );
                            }
                        }
                };
                for (auto y = sectorMinY; y <= sectorMaxY; y++) {
                    for (auto x = sectorMinX; x <= sectorMaxX; x++) {
                        CastIntoSector(x, y);
                    }
                }

                // Mark this as rendered
                ishdw.m_bAlreadyRenderedInBatch = true;
            }

            // Render out shadows (Can't batch together with next iteration, as renderstates change)
            RenderBuffer::RenderStuffInBuffer();
        }
        
        // Render onto water (If needed)
        if (oshdw.m_bDrawOnWater) {
            float waterLevelZ{}, bigWavesZ{}, smallWavesZ{};
            if (!CWaterLevel::GetWaterLevelNoWaves(oshdw.m_vecPosn, &waterLevelZ, &bigWavesZ, &smallWavesZ)) {
                continue;
            }

            // Shadow under water? (exe `fcomp` + `test ah, 5` / `jp`: a NaN level skips too)
            if (!(waterLevelZ < oshdw.m_vecPosn.z)) {
                continue;
            }

            // Clear the buffer as this is a new render pass (separate from the other)
            RenderBuffer::ClearRenderBuffer();

            // The smaller this value the detailed the shadows, but also (exponentially?) slower
            // This value basically represents the side of a square that is projected onto the water
            constexpr auto STEP_SIZE = 2;

            const auto& side  = oshdw.m_Side;
            const auto& front = oshdw.m_Front;

            // Exe: 1.0f / (y*y + x*x), the texture coordinates are multiplied by it (not divided)
            const float sideRecip  = 1.0f / (side.y * side.y + side.x * side.x);
            const float frontRecip = 1.0f / (front.y * front.y + front.x * front.x);

            // The cell range (in world units, always even): floor/ceil of (posn +- (|side| + |front|)) / 2, times 2. NOT the sector coords
            const auto oshdwRect = GetShadowRect(oshdw);
            const auto cellMinX  = NOTSA_FTOL(int32, std::floor(oshdwRect.minX * 0.5f)) * STEP_SIZE;
            const auto cellMinY  = NOTSA_FTOL(int32, std::floor(oshdwRect.minY * 0.5f)) * STEP_SIZE;
            const auto cellMaxX  = NOTSA_FTOL(int32, std::ceil(oshdwRect.maxX * 0.5f)) * STEP_SIZE;
            const auto cellMaxY  = NOTSA_FTOL(int32, std::ceil(oshdwRect.maxY * 0.5f)) * STEP_SIZE;

            // Note the loop order: x is the outer one
            for (auto cx = cellMinX; cx <= cellMaxX; cx += STEP_SIZE) {
                for (auto cy = cellMinY; cy <= cellMaxY; cy += STEP_SIZE) {
                    // 0x70B000 - 0x70B0DA: Start storing
                    RwIm3DVertex*    vtxIt{};
                    RwImVertexIndex* vtxIdxIt{};
                    RenderBuffer::StartStoring(6, 4, vtxIdxIt, vtxIt);

                    // Function to process 1 vertex (out of the 4)
                    const auto ProcessOneVertex = [&](int32 x, int32 y) {
                        // Set color
                        RwIm3DVertexSetRGBA(vtxIt, oshdw.m_nRed, oshdw.m_nGreen, oshdw.m_nBlue, NOTSA_FTOL(uint8, (float)(int16)oshdw.m_nIntensity * 0.6f));

                        // Set texture coords (UV)
                        // NOTE: The exe dots the 3D vector (x - posn.x, y - posn.y, -posn.z) with (side|front, 0), so the (-z * 0) term is part of the sum
                        const float dx = (float)x - oshdw.m_vecPosn.x;
                        const float dy = (float)y - oshdw.m_vecPosn.y;
                        const float dz = -oshdw.m_vecPosn.z;
                        const float dotSide  = (dx * side.x + dy * side.y) + dz * 0.0f;
                        const float dotFront = (dy * front.y + dx * front.x) + dz * 0.0f;
                        RwIm3DVertexSetU(vtxIt, (dotSide * sideRecip + 1.0f) * 0.5f);
                        RwIm3DVertexSetV(vtxIt, 0.5f - (dotFront * frontRecip) * 0.5f);

                        // Set position
                        RwIm3DVertexSetPos(
                            vtxIt,
                            (float)x,
                            (float)y,
                            CWaterLevel::CalculateWavesOnlyForCoordinate2_Direct(x, y, waterLevelZ, bigWavesZ, smallWavesZ) + 0.06f // Z pos of the wave at the given coord
                        );
                        vtxIt++;
                    };

                    // Process 4 vertices of this square
                    ProcessOneVertex(cx,             cy            ); // top left
                    ProcessOneVertex(cx + STEP_SIZE, cy            ); // top right
                    ProcessOneVertex(cx,             cy + STEP_SIZE); // bottom left
                    ProcessOneVertex(cx + STEP_SIZE, cy + STEP_SIZE); // bottom right

                    // Copy vertices into index buffer
                    rng::copy(std::to_array<RwImVertexIndex>({ 0, 1, 2, 1, 3, 2 }), vtxIdxIt);

                    // And we're done with this one... onto the next!
                    RenderBuffer::StopStoring();
                }
            }

            // Render it all
            RenderBuffer::RenderStuffInBuffer();
        }
    }

    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,    RWRSTATE(rwTEXTUREADDRESSWRAP));
    RwRenderStateSet(rwRENDERSTATECULLMODE,          RWRSTATE(rwCULLMODECULLBACK));

    ShadowsStoredToBeRendered = 0;
}

// 0x70B730
void CShadows::GeneratePolysForStaticShadow(int16 staticShadowIndex) {
    auto& shdw = aStaticShadows[staticShadowIndex];

    CVector posn = shdw.m_vecPosn;

    // Bounding rect of the shadow
    const auto sumX = std::abs(shdw.m_fFrontX) + std::abs(shdw.m_fSideX);
    const auto sumY = std::abs(shdw.m_fFrontY) + std::abs(shdw.m_fSideY);

    const float minX = posn.x - sumX;
    const float maxX = posn.x + sumX;
    const float minY = posn.y - sumY;
    const float maxY = posn.y + sumY;

    // NOTSA: Using `CWorld::GetSectorX` isn't exact here, as the original multiplies (instead of dividing) by 1/50
    const auto GetSector = [](float v) {
        return NOTSA_FTOL(int32, std::floor(v * 0.02f + 60.0f));
    };
    const auto sectorMinX = std::max(GetSector(minX), 0);
    const auto sectorMinY = std::max(GetSector(minY), 0);
    const auto sectorMaxX = std::min(GetSector(maxX), MAX_SECTORS_X - 1);
    const auto sectorMaxY = std::min(GetSector(maxY), MAX_SECTORS_Y - 1);

    CWorld::AdvanceCurrentScanCode();

    for (auto y = sectorMinY; y <= sectorMaxY; y++) {
        for (auto x = sectorMinX; x <= sectorMaxX; x++) {
            CastPlayerShadowSectorList(
                CWorld::GetSector(x, y).Buildings,
                minX, minY,
                maxX, maxY,
                &posn,
                shdw.m_fFrontX, shdw.m_fFrontY,
                shdw.m_fSideX, shdw.m_fSideY,
                0,
                0, 0, 0,
                shdw.m_fZDistance,
                shdw.m_fScale,
                &shdw.m_pPolyBunch,
                &shdw.m_nDayNightIntensity,
                0
            );
        }
    }
}

// 0x70BA00
bool CShadows::StoreStaticShadow(uint32 id, eShadowType type, RwTexture* texture, const CVector& posn, float frontX, float frontY, float sideX, float sideY, int16 intensity, uint8 red, uint8 green, uint8 blue, float zDistane, float scale, float drawDistance, bool temporaryShadow, float upDistance) {
    const auto& camPos = TheCamera.GetPosition();

    const float distSq = (posn.y - camPos.y) * (posn.y - camPos.y) + (posn.x - camPos.x) * (posn.x - camPos.x);
    if (distSq < drawDistance * drawDistance) {
        if (drawDistance != 0.0f) {
            // Fade out in the last quarter of the draw distance
            const auto dist = std::sqrt(distSq);
            if (!(dist < drawDistance * 0.75f)) {
                const auto fadeFactor = 1.0f - (dist - drawDistance * 0.75f) * (4.0f / drawDistance);
                intensity = (int16)NOTSA_FTOL(int32, (float)intensity * fadeFactor);
                red       = (uint8)NOTSA_FTOL(int32, (float)red * fadeFactor);
                green     = (uint8)NOTSA_FTOL(int32, (float)green * fadeFactor);
                blue      = (uint8)NOTSA_FTOL(int32, (float)blue * fadeFactor);
            }
        }
    } else if (drawDistance != 0.0f) {
        return true; // Too far away
    }

    // Is there already a shadow with this ID?
    int16 i = 0;
    for (; i < (int16)aStaticShadows.size(); i++) {
        if (aStaticShadows[i].m_nId == id && aStaticShadows[i].m_pPolyBunch) {
            break;
        }
    }

    if (i < (int16)aStaticShadows.size()) {
        auto& shdw = aStaticShadows[i];
        if (
            (std::abs(posn.x - shdw.m_vecPosn.x) < upDistance && std::abs(posn.y - shdw.m_vecPosn.y) < upDistance)
            || (
                std::abs(posn.x - shdw.m_vecPosn.x) < 0.05f
             && std::abs(posn.y - shdw.m_vecPosn.y) < 0.05f
             && std::abs(posn.z - shdw.m_vecPosn.z) < 2.0f
             && frontX == shdw.m_fFrontX
             && frontY == shdw.m_fFrontY
             && sideX == shdw.m_fSideX
             && sideY == shdw.m_fSideY
            )
        ) {
            // Close enough to the old one, so just update it
            shdw.m_pTexture         = texture;
            shdw.m_nType            = type;
            shdw.m_nRed             = red;
            shdw.m_nIntensity       = intensity;
            shdw.m_nGreen           = green;
            shdw.m_nBlue            = blue;
            shdw.m_fScale           = scale;
            shdw.m_fZDistance       = zDistane;
            shdw.m_nTimeCreated     = CTimer::GetTimeInMS();
            shdw.m_bJustCreated     = true;
            shdw.m_bTemporaryShadow = temporaryShadow;
            return true;
        }

        // Moved too far, so regenerate it (reusing this slot)
        shdw.Free();
    } else {
        // Find a free slot
        for (i = 0; i < (int16)aStaticShadows.size(); i++) {
            if (!aStaticShadows[i].m_pPolyBunch) {
                break;
            }
        }
        if (i == (int16)aStaticShadows.size()) {
            return true; // No free slot
        }
    }

    auto& shdw = aStaticShadows[i];
    shdw.m_nType            = type;
    shdw.m_pTexture         = texture;
    shdw.m_nIntensity       = intensity;
    shdw.m_nRed             = red;
    shdw.m_nGreen           = green;
    shdw.m_nBlue            = blue;
    shdw.m_fZDistance       = zDistane;
    shdw.m_fScale           = scale;
    shdw.m_nId              = id;
    shdw.m_vecPosn          = posn;
    shdw.m_fFrontX          = frontX;
    shdw.m_fFrontY          = frontY;
    shdw.m_fSideX           = sideX;
    shdw.m_fSideY           = sideY;
    shdw.m_bJustCreated     = true;
    shdw.m_bTemporaryShadow = temporaryShadow;
    shdw.m_nTimeCreated     = CTimer::GetTimeInMS();

    GeneratePolysForStaticShadow(i);

    return shdw.m_pPolyBunch != nullptr;
}

// 0x70BDA0
void CShadows::StoreShadowForVehicle(CVehicle* vehicle, VEH_SHD_TYPE vehShadowType) {
    if (CStencilShadows::GraphicsHighQuality()) {
        return;
    }

    RwTexture* texture = gpShadowCarTex;

    // NOTSA: Original loads the whole dword, but only the lower 16 bits are ever used
    int32 intensity = (int16)CTimeCycle::m_CurrentColours.m_nShadowStrength;
    if (intensity == 0) {
        return;
    }

    float zDistance = 4.5f;

    CVector pos = vehicle->GetPosition();

    // Distance to the camera
    const auto& camPos = TheCamera.GetPosition();
    float distSq = (pos.x - camPos.x) * (pos.x - camPos.x) + (pos.y - camPos.y) * (pos.y - camPos.y);
    if (CCutsceneMgr::ms_running) {
        distSq = distSq / (TheCamera.m_fLODDistMultiplier * TheCamera.m_fLODDistMultiplier * 4.f);
    }

    // Check if the vehicle is close enough
    float maxDist;
    bool  isCloseEnough;
    switch (vehShadowType) {
    case VEH_SHD_HELI:
    case VEH_SHD_PLANE:
    case VEH_SHD_RC:
        isCloseEnough = distSq < 144.f * 144.f;
        maxDist       = 144.f;
        break;
    case VEH_SHD_BIG_PLANE:
        isCloseEnough = distSq < 288.f * 288.f;
        maxDist       = 288.f;
        break;
    default:
        isCloseEnough = distSq < 18.f * 18.f;
        maxDist       = 18.f;
        break;
    }
    if (!isCloseEnough) {
        return;
    }

    // Fade out in the last quarter of the distance
    const auto dist = std::sqrt(distSq);
    if (!(dist < maxDist * 0.75f)) {
        intensity = NOTSA_FTOL(int32, (1.0f - (dist - maxDist * 0.75f) / (maxDist * 0.25f)) * (float)(int16)intensity);
    }

    // Size of the shadow
    const auto  modelId  = (int32)(int16)vehicle->m_nModelIndex;
    auto* const colModel = vehicle->GetColModel();
    const auto& bb       = colModel->GetBoundingBox();
    float       lenY     = bb.m_vecMax.y - bb.m_vecMin.y;
    float       widX     = bb.m_vecMax.x - bb.m_vecMin.x;
    float       mult     = 1.0f;
    switch (modelId) {
    case MODEL_LEVIATHN:
    case MODEL_HUNTER:
    case MODEL_SEASPAR:
    case MODEL_SPARROW:
    case MODEL_MAVERICK:
    case MODEL_POLMAV: // Helicopters
        mult  = 0.5f;
        widX *= 3.0f;
        lenY *= 1.4f;
        break;
    case MODEL_PIZZABOY:
    case MODEL_PCJ600:
    case MODEL_FAGGIO:
    case MODEL_BMX:
    case MODEL_BIKE:
    case MODEL_MTBIKE:
    case MODEL_FCR900:
    case MODEL_NRG500: // Bikes
        mult  = 0.05f;
        lenY *= 1.2f;
        break;
    case MODEL_FREEWAY:
    case MODEL_SANCHEZ:
    case MODEL_COPBIKE:
        mult  = 0.03f;
        lenY *= 1.5f;
        break;
    case MODEL_RCRAIDER:
    case MODEL_RCGOBLIN:
        mult  = 0.2f;
        lenY *= 1.5f;
        widX  = widX + widX;
        break;
    case MODEL_VORTEX:
        return;
    case UNLOAD_MODEL:
        lenY *= 0.9f;
        widX *= 0.4f;
        break;
    }

    // Move the shadow so it's centered
    const auto& mat = vehicle->GetMatrix();
    {
        const auto offset = (lenY * 0.5f - bb.m_vecMax.y) * mult;
        pos.x = pos.x - offset * mat.GetForward().x;
        pos.y = pos.y - offset * mat.GetForward().y;
    }

    // Vehicle type specific stuff
    switch (vehShadowType) {
    case VEH_SHD_CAR:
        texture = gpShadowCarTex;
        break;
    case VEH_SHD_BIKE: {
        float widMult = std::abs(vehicle->AsBike()->m_RideAnimData.LeanAngle) * 5.092958f + 1.0f; // TODO: Magic number
        if (vehicle->GetStatus() == STATUS_ABANDONED) {
            const auto tilt = std::abs(mat.GetRight().z);
            if (tilt > 0.6f) {
                widMult = widMult + tilt * 4.0f;
            }
        }
        widX    = widMult * widX;
        texture = gpShadowBikeTex;
        break;
    }
    case VEH_SHD_HELI:
        texture = gpShadowHeliTex;
        if (FindPlayerVehicle() == vehicle) {
            zDistance = 50.0f;
        }
        break;
    case VEH_SHD_PLANE:
    case VEH_SHD_BIG_PLANE:
        texture   = gpShadowBaronTex;
        intensity = (int16)CTimeCycle::m_CurrentColours.m_nShadowStrength; // Don't fade the shadow
        if (FindPlayerVehicle() == vehicle) {
            zDistance = 50.0f;
        }
        break;
    case VEH_SHD_RC:
        lenY   *= 1.5f;
        widX   *= 2.2f;
        texture = gpShadowBaronTex;
        break;
    default:
        break;
    }

    // Direction vectors of the shadow (Front and side)
    float frontX = mat.GetForward().x;
    float frontY = mat.GetForward().y;

    const auto right = CrossProduct(mat.GetForward(), CVector{ 0.f, 0.f, 1.f });
    float sideX = right.x;
    float sideY = right.y;
    if (const auto len = std::sqrt(right.y * right.y + right.x * right.x); len < 0.5f) {
        const auto invLen = 1.0f / len;
        sideX = right.x * invLen * 0.5f;
        sideY = invLen * right.y * 0.5f;
    }
    if (mat.GetUp().z < 0.0f) {
        sideX = -sideX;
        sideY = -sideY;
    }

    if (vehShadowType == VEH_SHD_BIKE) {
        if (std::abs(mat.GetRight().z) > 0.6f) {
            sideX = mat.GetUp().x;
            sideY = mat.GetUp().y;
        }
    } else if (vehShadowType == VEH_SHD_HELI) {
        if (std::abs(mat.GetRight().z) > 0.57f) {
            sideX = mat.GetUp().x;
            sideY = mat.GetUp().y;
        }
        if (std::abs(mat.GetForward().z) > 0.57f) {
            frontX = mat.GetUp().x;
            frontY = mat.GetUp().y;
        }
    }

    // RC vehicles and the player's vehicle always use dynamic shadows
    const bool isDynamic = vehicle->vehicleFlags.bIsRCVehicle || FindPlayerVehicle() == vehicle;

    const auto& vel     = vehicle->m_vecMoveSpeed;
    const auto  speed   = std::sqrt((vel.x * vel.x + vel.y * vel.y) + vel.z * vel.z);
    const auto  halfWid = widX * 0.5f;
    const auto  halfLen = lenY * 0.5f;
    const auto  flipSide = !(mat.GetUp().z > 0.0f); // exe `test ah, 0x41`: a NaN flips too

    if (!(speed * CTimer::GetTimeStep() > 0.1f) && !isDynamic) { // Vehicle is standing still => use a static shadow
        // BUG: Ignores `zDistance` (always uses 4.5)
        StoreStaticShadow(
            (uint32)(uintptr_t)vehicle + 1, // The ID is the address of the vehicle + 1
            SHADOW_DEFAULT,
            texture,
            pos,
            halfLen * frontX,
            frontY * halfLen,
            flipSide ? -(halfWid * sideX) : halfWid * sideX,
            flipSide ? -(halfWid * sideY) : halfWid * sideY,
            (int16)intensity,
            (uint8)intensity, (uint8)intensity, (uint8)intensity,
            4.5f,
            1.0f,
            0.0f,
            false,
            0.1f
        );
        return;
    }

    StoreShadowToBeRendered(
        SHADOW_DEFAULT,
        texture,
        pos,
        halfLen * frontX,
        frontY * halfLen,
        flipSide ? -(halfWid * sideX) : halfWid * sideX,
        flipSide ? -(halfWid * sideY) : halfWid * sideY,
        (int16)intensity,
        (uint8)intensity, (uint8)intensity, (uint8)intensity,
        zDistance,
        isDynamic, // drawOnWater
        1.0f,
        nullptr,
        isDynamic // drawOnBuildings
    );
}

// 0x70C500
void CShadows::StoreCarLightShadow(CVehicle* vehicle, int32 id, RwTexture* texture, const CVector& posn, float frontX, float frontY, float sideX, float sideY, uint8 red, uint8 green, uint8 blue, float maxViewAngleCosine) {
    // Maximum distance (from camera to `posn`) after which shadows aren't stored (and rendered)
    constexpr auto MAX_CAM_TO_LIGHT_DIST = 27.f;

    if ([] { // Maybe ignore camera distance?
        switch (CCamera::GetActiveCamera().m_nMode) {
        case MODE_TOPDOWN:
        case MODE_TOP_DOWN_PED:
            return false;
        }

        return !CCutsceneMgr::IsRunning();
    }()) {
        const auto shdwToCam2D       = CVector2D{ TheCamera.GetPosition() - posn };
        const auto shdwToCamDist2DSq = shdwToCam2D.SquaredMagnitude();

        // exe `fcomp` + `test ah, 5` / `jnp`: a NaN distance returns too
        if (!(shdwToCamDist2DSq < sq(MAX_CAM_TO_LIGHT_DIST))) {
            return;
        }

        // Check if the camera is facing the lights closely (in which case the camera can't see the shadow)
        // The exe computes the dot of (posn - cam) and returns for `dot <= -cosine` (0x70C5BF, `fcompp` + `jp`), this is the same test with the vector flipped; NaN returns
        if (!(shdwToCam2D.Dot(TheCamera.GetFrontNormal2D()) < maxViewAngleCosine)) {
            return;
        }

        // If far enough from the camera, start fading out
        if (const auto dist = std::sqrt(shdwToCamDist2DSq); !(dist < MAX_CAM_TO_LIGHT_DIST * 0.75f)) {
            // The exe multiplies by the float constant 1/9 (0x872740) instead of dividing by 27 - 18
            const auto t = 1.f - (dist - MAX_CAM_TO_LIGHT_DIST * (2.f / 3.f)) * (1.f / 9.f);
            red   = NOTSA_FTOL(uint8, (float)red * t);
            green = NOTSA_FTOL(uint8, (float)green * t);
            blue  = NOTSA_FTOL(uint8, (float)blue * t);
        }
    }

    const auto isPlyrVeh = FindPlayerVehicle() == vehicle;
    if (isPlyrVeh || NOTSA_EXE_OR(vehicle->GetMoveSpeed().Magnitude() * CTimer::GetTimeStep() > 0.4f /* 0x70C685: `fcomp; test ah, 0x41; je` = strictly greater */, vehicle->GetMoveSpeed().Magnitude() * CTimer::GetTimeStep() >= 0.4f)) {
        StoreShadowToBeRendered(
            SHADOW_ADDITIVE,
            texture,
            posn,
            frontX, frontY,
            sideX, sideY,
            128,
            red, green, blue,
            6.f,
            false,
            1.f,
            nullptr,
            NOTSA_EXE_OR(isPlyrVeh, isPlyrVeh || g_fx.GetFxQuality() >= FX_QUALITY_VERY_HIGH) // NOTSA: At higher FX quality draw all vehicles's shadows on buildings too (the exe passes `vehicle == FindPlayerVehicle()`, 0x70C705; NOTSA-AUDIT)
        );
    } else {
        StoreStaticShadow(
            NOTSA_PTR_ID32(vehicle) + id,
            SHADOW_ADDITIVE,
            texture,
            posn,
            frontX, frontY,
            sideX, sideY,
            128,
            red, green, blue,
            6.f,
            1.f,
            0.f,
            false,
            0.4f
        );
    }
}

// 0x70C750
void CShadows::StoreShadowForPole(CEntity* entity, float offsetX, float offsetY, float offsetZ, float poleHeight, float poleWidth, uint32 localId) {
    if (GraphicsHighQuality() || !CTimeCycle::m_CurrentColours.m_nPoleShadowStrength) {
        return;
    }

    const auto& mat = entity->GetMatrix();

    if (mat.GetUp().z < .5f) { // More than 45 deg tilted
        return;
    }

    const auto intensity = 2.f * (mat.GetUp().z - 0.5f) * CTimeCycle::m_CurrentColours.m_nPoleShadowStrength;

    const auto front     = CVector2D{ CTimeCycle::GetVectorToSun() } * (-poleHeight / 2.f);
    const auto right     = CVector2D{ CTimeCycle::GetShadowSide() } * poleWidth;

    // Simplified matrix transform (Ignoring the Z axis), the sums are in the exe's order: (offset + position) + front (0x70C80B..0x70C8A5)
    const auto& matPos = mat.GetPosition();
    const CVector shadowPos{
        (offsetX * mat.GetRight().x + offsetY * mat.GetForward().x + matPos.x) + front.x,
        (offsetX * mat.GetRight().y + offsetY * mat.GetForward().y + matPos.y) + front.y,
        matPos.z + offsetZ
    };

    StoreStaticShadow(
        NOTSA_PTR_ID32(entity) + localId + 0x33, // The exe uses the entity itself and 0x33 (0x70C93E: `lea eax, [esi + edx + 0x33]`)
        SHADOW_DEFAULT,
        gpPostShadowTex,
        shadowPos,
        front.x, front.y,
        right.x, right.y,
        2 * NOTSA_FTOL(int16, intensity) / 3,
        0, 0, 0,
        9.f,
        1.f,
        40.f,
        false,
        0.f
    );
}

// 0x70CCB0
void CShadows::RenderIndicatorShadow(
    uint32 id,
    eShadowType,
    RwTexture*,
    const CVector& posn,
    float frontX, float frontY,
    float sideX, float sideY,
    int16 /*intensity*/
) {
    const auto size = std::max(frontX, -sideX);
    for (auto mult : { 0.8f, 0.9f, 1.0f }) {
        CVector markerPos = posn;
        C3dMarkers::PlaceMarkerSet(id, MARKER3D_CYLINDER, markerPos, size * mult, 255, 0, 0, 255u, 2048, 0.2f, 0);
    }
}

// 0x709CF0
// NOTSA: The original uses a custom calling convention (`normal` in `eax`, `trianglePos` in `ebx`, `param` in `edi`), so it can't be hooked
CVector* ShadowRenderTriangleCB(CVector* normal, CVector* trianglePos, _ProjectionParam* param) {
    // Triangle in world space
    CVector worldPts[3];
    RwV3dTransformPoints(worldPts, trianglePos, 3, &param->entityMatrix);

    // Cull faces facing away from the camera
    const auto viewDot = (param->at.y * normal->y + param->at.z * normal->z) + param->at.x * normal->x;
    if (viewDot > 0.0f) {
        return trianglePos;
    }

    // Triangle in the shadow camera's space (x, y => texture coords, z => depth)
    CVector uvPts[3];
    RwV3dTransformPoints(uvPts, worldPts, 3, &param->invMatrix);

    // Behind the camera?
    if (uvPts[0].z < 0.0f && uvPts[1].z < 0.0f && uvPts[2].z < 0.0f) {
        return trianglePos;
    }

    // Outside of the texture?
    if (uvPts[0].x < 0.0f && uvPts[1].x < 0.0f && uvPts[2].x < 0.0f) {
        return trianglePos;
    }
    if (uvPts[0].x > 1.0f && uvPts[1].x > 1.0f && uvPts[2].x > 1.0f) {
        return trianglePos;
    }
    if (uvPts[0].y < 0.0f && uvPts[1].y < 0.0f && uvPts[2].y < 0.0f) {
        return trianglePos;
    }
    if (uvPts[0].y > 1.0f && uvPts[1].y > 1.0f && uvPts[2].y > 1.0f) {
        return trianglePos;
    }

    RwImVertexIndex* idx{};
    RwIm3DVertex*    vtx{};
    RenderBuffer::StartStoring(3, 3, idx, vtx);

    for (auto i = 0; i < 3; i++) {
        RwIm3DVertexSetPos(&vtx[i], worldPts[i].x, worldPts[i].y, worldPts[i].z + 0.06f);
        RwIm3DVertexSetU(&vtx[i], uvPts[i].x);
        RwIm3DVertexSetV(&vtx[i], uvPts[i].y);

        // Shadow's opacity (Optionally fades out with depth)
        uint8 value = param->shadowValue;
        if (param->fade) {
            const float fadeFactor = 1.0f - uvPts[i].z * uvPts[i].z;
            value = fadeFactor >= 0.0f
                ? (uint8)NOTSA_FTOL(int32, (float)param->shadowValue * fadeFactor)
                : (uint8)0;
        }
        RwIm3DVertexSetRGBA(&vtx[i], value, value, value, value);
    }

    idx[0] = 0;
    idx[1] = 1;
    idx[2] = 2;

    RenderBuffer::StopStoring();

    return trianglePos;
}
