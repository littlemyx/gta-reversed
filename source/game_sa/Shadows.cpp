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
    RH_ScopedInstall(CastShadowEntityXY, 0x7086B0, { .Reversed = false });
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

        m_pPolyBunch = nullptr;
        m_nId = 0;
    }
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
                blue      = (uint8)(int32)((float)shdw.m_nBlue * fadeFactor);
                green     = (uint8)(int32)((float)shdw.m_nGreen * fadeFactor);
                red       = (uint8)(int32)((float)shdw.m_nRed * fadeFactor);
                intensity = (int16)(int32)((float)(int16)shdw.m_nIntensity * fadeFactor); // BUG: Original sign extends the intensity here, but not in the branch above
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
    CRect rect{ {minX, minY}, {maxX, maxY} };
    for (auto& shadow : aPermanentShadows) {
        switch (shadow.m_nType) {
        case SHADOW_OIL_1:
        case SHADOW_OIL_5:
            break;
        default:
            continue;
        }
        if (rect.IsPointInside(shadow.m_vecPosn)) {
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
    const auto sunDist = sunPosn.Magnitude2D();
    const auto recip = 1.0f / sunDist;

    const auto mult = (sunDist + 1.0f) * recip;
    frontX = -sunPosn.x * mult / 2.0f;
    frontY = -sunPosn.y * mult / 2.0f;

    sideX = -sunPosn.y * recip / 2.0f;
    sideY = +sunPosn.x * recip / 2.0f;

    displacementX = -sunPosn.x / 2.0f;
    displacementY = -sunPosn.y / 2.0f;
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
            0.3f + 0.7f * lerp(
                (float)(dayNightIntensity >> 0 & 0b1111) / 30.f,
                (float)(dayNightIntensity >> 4 & 0b1111) / 30.f,
                CCustomBuildingDNPipeline::m_fDNBalanceParam
            )
        );
        outR = (uint8)((float)r * mult);
        outG = (uint8)((float)g * mult);
        outB = (uint8)((float)b * mult);
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
        return (uint16)((1.f - (currDist - halfMaxDist) / halfMaxDist) * maxStrength);
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
        isPlayerPed || g_fx.GetFxQuality() >= FX_QUALITY_VERY_HIGH // NOTSA: At higher FX quality draw all ped's shadows
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
    const auto cc       = (uint8)((float)rtshdw->m_nIntensity / 100.f * strength);

    const auto& vecToSun = CTimeCycle::m_VectorToSun[CTimeCycle::m_CurrentStoredValue];
    const auto lightFrame = rtshdw->SetLightProperties(
        RWRAD2DEG(+std::atan2(-vecToSun.x, -vecToSun.y)),
        RWRAD2DEG(-std::atan2(+vecToSun.x, -vecToSun.z)),
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
        g_fx.GetFxQuality() >= FX_QUALITY_VERY_HIGH // NOTSA: At higher FX quality draw all shadows on buildings too
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
                const auto a = (uint8)((float)ishdw.m_nIntensity * (1.f - CWeather::Foggyness * 0.5f));
                for (auto i{ 0 }; i < poly->m_wNumVerts; i++, vtxIt++) {
                    const auto& pos = poly->m_avecPosn[i];
                    RwIm3DVertexSetPos(vtxIt, pos.x, pos.y, pos.z + 0.06f);
                    RwIm3DVertexSetRGBA(vtxIt, r, g, b, a);
                    RwIm3DVertexSetU(vtxIt, (float)poly->m_aU[i] / 200.f);
                    RwIm3DVertexSetV(vtxIt, (float)poly->m_aV[i] / 200.f);
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
    ((void(__cdecl*)(CEntity*, float, float, float, float, CVector*, float, float, float, float, int16, uint8, uint8, uint8, float, float, CPolyBunch**, uint8*, int32))0x7086B0)(entity, conrerAX, cornerAY, cornerBX, cornerBY, posn, frontX, frontY, sideX, sideY, intensity, red, green, blue, zDistance, scale, ppPolyBunch, pDayNightIntensity, shadowType);
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

    const auto viewWindowY = shdwCam->m_pRwCamera->viewWindow.y;
    const RwV3d invScale{
        -0.5f / (0.9f * viewWindowY),
        -0.5f / (0.9f * viewWindowY),
        1.0f / (viewWindowY * 0.8f)
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
    const RwV3d sphereCenterWorld{
        frontX * -1.1f + posn->x,
        posn->y + frontY * -1.1f,
        posn->z - 0.5f
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
    const CRect shadowRect{
        cornerAX, cornerAY,
        cornerBX, cornerBY
    };
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

        // If slightly tilted, ignore
        if (entity->GetMatrix().GetUp().z <= 0.97f) {
            continue;
        }

        // 0x70A526
        if (!entity->GetBoundRect().OverlapsWith(shadowRect)) {
            continue;
        }

        // Quick Z height check of the bounding box
        const auto& cm         = entity->GetColModel();
        const auto  entityPosZ = entity->GetPosition().z;
        if (cm->m_boundBox.m_vecMax.z + entityPosZ <= posn->z - zDistance) {
            continue;
        }
        if (cm->m_boundBox.m_vecMin.z + entityPosZ >= posn->z) {
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

        // If slightly tilted, ignore
        if (entity->GetMatrix().GetUp().z <= 0.97f) {
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
        if (cm->m_boundBox.m_vecMax.z + entityPosZ <= posn->z - zDistance) {
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
        if (cm->m_boundBox.m_vecMax.z + entityPosZ <= posn->z - zDistance) {
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
    // Originally renderstates are still set even though there are no shadows to be rendered
    // I don't think this is necessary, so we early-out here
    if (!CShadows::ShadowsStoredToBeRendered) {
        return;
    }

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

    const auto GetShadowRect = [](CRegisteredShadow& shdw) {
        return CRect{
            shdw.m_vecPosn,
            shdw.m_vecPosn + abs(shdw.m_Side) + abs(shdw.m_Front)
        };
    };

    for (auto o = 0; o < ShadowsStoredToBeRendered; o++) {
        auto& oshdw = asShadowsStored[o];

        // Setup additional render states for this shadow (and others in the batch below)
        SetRenderModeForShadowType(oshdw.m_nType);
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(oshdw.m_pTexture)));

        //
        // Render onto ground (If not already)
        //
        if (!oshdw.m_bAlreadyRenderedInBatch) {
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

                // Do shadow casting
                CWorld::IterateSectorsOverlappedByRect(
                    ishdwRect,
                    [&] (int32 x, int32 y) -> bool {
                        auto& sector = CWorld::GetSector(x, y);

                        if (const auto rtshdw = ishdw.m_pRTShadow) {
                            CastRealTimeShadowSectorList(
                                sector.Buildings,
                                ishdwRect.left, ishdwRect.bottom,
                                ishdwRect.right, ishdwRect.top,
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
                                    ishdwRect.left, ishdwRect.bottom,
                                    ishdwRect.right, ishdwRect.top,
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
                                    ishdwRect.left, ishdwRect.bottom,
                                    ishdwRect.right, ishdwRect.top,
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
                        return true; // Inisde lambda -> Continue sector loop
                    }
                );

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

            // Shadow under water?
            if (waterLevelZ >= oshdw.m_vecPosn.z) {
                continue;
            }

            // Clear the buffer as this is a new render pass (separate from the other)
            RenderBuffer::ClearRenderBuffer();

            // The smaller this value the detailed the shadows, but also (exponentially?) slower
            // This value basically represents the side of a square that is projected onto the 
            const auto STEP_SIZE = 2;

            const auto shdwSideMagSq = oshdw.m_Side.SquaredMagnitude();
            const auto shdwFrontMagSq = oshdw.m_Front.SquaredMagnitude();

            const auto oshdwRect = GetShadowRect(oshdw);

            // Iterate shadow sectors (Using `CWorld::IterateSectors` for convenience)
            CWorld::IterateSectors(
                (int32)(std::floor(oshdwRect.left / (float)STEP_SIZE)),
                (int32)(std::floor(oshdwRect.top / (float)STEP_SIZE)),
                (int32)(std::ceil(oshdwRect.right / (float)STEP_SIZE)),
                (int32)(std::ceil(oshdwRect.bottom / (float)STEP_SIZE)),
                [&](int32 ox, int32 oy) {
                    // 0x70B000 - 0x70B0DA: Start storing 
                    RwIm3DVertex* vtxIt{};
                    RwImVertexIndex* vtxIdxIt{};
                    RenderBuffer::StartStoring(6, 4, vtxIdxIt, vtxIt);

                    // Function to process 1 vertex (out of the 4)
                    const auto ProcessOneVertex = [&] (int32 x, int32 y) {
                        // The x, y passed in are sector coords, so make them into world coords
                        x *= STEP_SIZE;
                        y *= STEP_SIZE;

                        const auto currPos = CVector2D{ (float)x, (float)y };

                        // Set color
                        RwIm3DVertexSetRGBA(vtxIt, oshdw.m_nRed, oshdw.m_nGreen, oshdw.m_nBlue, (uint8)((float)oshdw.m_nIntensity * 0.6f));

                        // Set texture coords (UV)
                        const auto CalcTexCoord  = [
                            currPosToShdw = CVector2D{ currPos - oshdw.m_vecPosn }
                        ](CVector2D v, float vmagsq) {
                            return (currPosToShdw.Dot(v) / vmagsq + 1.f) * 0.5f;
                        };
                        RwIm3DVertexSetU(vtxIt, CalcTexCoord(oshdw.m_Side, shdwSideMagSq));
                        RwIm3DVertexSetV(vtxIt, CalcTexCoord(oshdw.m_Front, shdwFrontMagSq));

                        // Set position
                        RwIm3DVertexSetPos(
                            vtxIt,
                            currPos.x,
                            currPos.y,
                            CWaterLevel::CalculateWavesOnlyForCoordinate2_Direct(x, y, waterLevelZ, bigWavesZ, smallWavesZ) + 0.06f // Z pos of the wave at the given coord
                        );
                    };

                    // Process 4 vertices of this square
                    ProcessOneVertex(ox,     oy    ); // top left
                    ProcessOneVertex(ox + 1, oy    ); // top right
                    ProcessOneVertex(ox,     oy + 1); // bottom left
                    ProcessOneVertex(ox + 1, oy + 1); // bottom right

                    // Copy vertices into index buffer
                    rng::copy(std::to_array({ 0, 1, 2, 1, 3, 2 }), vtxIdxIt);

                    // And we're done with this one... onto the next!
                    RenderBuffer::StopStoring();

                    // Continue iteration...
                    return true;
                }
            );

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
        return (int32)std::floor(v * 0.02f + 60.0f);
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
                intensity = (int16)(int32)((float)intensity * fadeFactor);
                red       = (uint8)(int32)((float)red * fadeFactor);
                green     = (uint8)(int32)((float)green * fadeFactor);
                blue      = (uint8)(int32)((float)blue * fadeFactor);
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
        intensity = (int32)((1.0f - (dist - maxDist * 0.75f) / (maxDist * 0.25f)) * (float)(int16)intensity);
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
    const auto  flipSide = mat.GetUp().z <= 0.0f;

    if (speed * CTimer::GetTimeStep() <= 0.1f && !isDynamic) { // Vehicle is standing still => use a static shadow
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

        if (shdwToCamDist2DSq >= sq(MAX_CAM_TO_LIGHT_DIST)) {
            return;
        }

        // Check if the camera is facing the lights closely (in which case the camera can't see the shadow)
        if (shdwToCam2D.Dot(TheCamera.GetFrontNormal2D()) > maxViewAngleCosine) {
            return;
        }

        // If far enough from the camera, start fading out
        if (const auto dist = std::sqrt(shdwToCamDist2DSq); dist >= MAX_CAM_TO_LIGHT_DIST * 0.75f) {
            const auto t = 1.f - invLerp(MAX_CAM_TO_LIGHT_DIST * (2.f / 3.f), MAX_CAM_TO_LIGHT_DIST, dist);
            red   = (uint8)((float)red * t);
            green = (uint8)((float)green * t);
            blue  = (uint8)((float)blue * t);
        }
    }

    const auto isPlyrVeh = FindPlayerVehicle() == vehicle;
    if (isPlyrVeh || vehicle->GetMoveSpeed().Magnitude() * CTimer::GetTimeStep() >= 0.4f) {
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
            isPlyrVeh || g_fx.GetFxQuality() >= FX_QUALITY_VERY_HIGH // NOTSA: At higher FX quality draw all vehicles's shadows on buildings too
        );
    } else {
        StoreStaticShadow(
            reinterpret_cast<uint32>(vehicle) + id,
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

    StoreStaticShadow(
        reinterpret_cast<uint32>(entity->GetLod()) + localId + 3,
        SHADOW_DEFAULT,
        gpPostShadowTex,
        mat.GetPosition() + CVector{ front, 0.f } + CVector{
            offsetX * mat.GetRight().x + offsetY * mat.GetForward().x, // Simplified matrix transform (Ignoring the Z axis)
            offsetX * mat.GetRight().y + offsetY * mat.GetForward().y, // >^^^
            offsetZ
        },
        front.x, front.y,
        right.x, right.y,
        2 * (int16)intensity / 3,
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
                ? (uint8)(int32)((float)param->shadowValue * fadeFactor)
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
