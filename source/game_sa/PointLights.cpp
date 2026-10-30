/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "PointLights.h"
#include "Clouds.h"

void CPointLights::InjectHooks() {
    RH_ScopedClass(CPointLights);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x6FFB40);
    RH_ScopedInstall(GenerateLightsAffectingObject, 0x6FFBB0);
    RH_ScopedInstall(GetLightMultiplier, 0x6FFE70);
    RH_ScopedInstall(RemoveLightsAffectingObject, 0x6FFFE0);
    RH_ScopedInstall(ProcessVerticalLineUsingCache, 0x6FFFF0);
    RH_ScopedInstall(AddLight, 0x7000E0);
    RH_ScopedInstall(RenderFogEffect, 0x7002D0);
}

// 0x6FFB40
void CPointLights::Init() {
    rng::fill(aCachedMapReadResults, 0.0f);
    NextCachedValue = 0;
    rng::fill(aCachedMapReads, CVector{});
}

// 0x6FFBB0
float CPointLights::GenerateLightsAffectingObject(const CVector* point, float* totalLighting, CEntity* entity) {
    // Exact x87 form of the exe: the deltas and the distance are stored as floats, the sum of squares is z + y + x, the ratio / 1/dist / (1 - ratio) stay in
    // extended precision, the in-range tests are strict and false for NaN
    float antilightMult = 1.0f;
    for (const auto& light : GetActiveLights()) {
        if (light.m_nType == PLTYPE_ONLYFOGEFFECT_ALWAYS || light.m_nType == PLTYPE_ONLYFOGEFFECT) {
            continue;
        }
        const float radius = light.m_fRadius;
        const float nRad   = -radius;
        const float dx = (float)((x87d)light.m_vecPosn.x - (x87d)point->x);
        if (!(dx > nRad && dx < radius)) {
            continue;
        }
        const float dy = (float)((x87d)light.m_vecPosn.y - (x87d)point->y);
        if (!(dy > nRad && dy < radius)) {
            continue;
        }
        const float dz = (float)((x87d)light.m_vecPosn.z - (x87d)point->z);
        if (!(dz > nRad && dz < radius)) {
            continue;
        }
        const float dist = (float)x87::sqrt((x87d)dz * dz + (x87d)dy * dy + (x87d)dx * dx);
        if (!(dist < radius)) {
            continue;
        }

        const x87d ratio = (x87d)dist / (x87d)radius;
        if (light.m_nType == PLTYPE_ANTILIGHT) {
            antilightMult = (float)(ratio * (x87d)antilightMult);
            continue;
        }

        constexpr x87d k3 = (x87d)ExeRecip(3.0f);
        const x87d oneMinusRatio = (x87d)1.0 - ratio;
        if (totalLighting) {
            *totalLighting = (float)(oneMinusRatio * light.m_fColorRed * k3 + *totalLighting);
            *totalLighting = (float)((x87d)*totalLighting + oneMinusRatio * light.m_fColorGreen * k3);
            *totalLighting = (float)(oneMinusRatio * light.m_fColorBlue * k3 + (x87d)*totalLighting);
        }

        float intensity = (ratio >= 0.5 || ratio != ratio) ? (float)((x87d)1.0 - (ratio - (x87d)0.5) * (x87d)2.0) : 1.0f;
        if (dist == 0.0f) {
            continue;
        }
        const x87d inv = (x87d)1.0 / (x87d)dist;
        const float  nx = (float)((x87d)dx * inv), ny = (float)((x87d)dy * inv), nz = (float)((x87d)dz * inv);
        if (light.m_nType == PLTYPE_DIRECTIONAL && light.m_pEntityToLight != entity) {
            const x87d dot = -((x87d)nz * light.m_vecDirection.z + (x87d)ny * light.m_vecDirection.y + (x87d)nx * light.m_vecDirection.x) - (x87d)0.5;
            const float  t   = (float)(dot + dot);
            intensity = (float)((t < 0.0f ? 0.0f : t) * (x87d)intensity);
        }
        if (intensity > 0.0f) {
            AddAnExtraDirectionalLight(Scene.m_pRpWorld, nx, ny, nz,
                (float)((x87d)intensity * light.m_fColorRed),
                (float)((x87d)intensity * light.m_fColorGreen),
                (float)((x87d)intensity * light.m_fColorBlue));
        }
    }
    return antilightMult;
}

// 0x6FFE70
float CPointLights::GetLightMultiplier(const CVector* point) {
    // Exact x87 form of the exe (see GenerateLightsAffectingObject): the anti-light product and the light sum stay in extended precision, each colour channel is added separately
    x87d antilightMult = (x87d)1.0;
    x87d lightSum      = (x87d)0.0;
    for (const auto& light : GetActiveLights()) {
        if (light.m_nType == PLTYPE_ONLYFOGEFFECT_ALWAYS || light.m_nType == PLTYPE_ONLYFOGEFFECT) {
            continue;
        }
        const float radius = light.m_fRadius;
        const float nRad   = -radius;
        const float dx = (float)((x87d)light.m_vecPosn.x - (x87d)point->x);
        if (!(dx > nRad && dx < radius)) {
            continue;
        }
        const float dy = (float)((x87d)light.m_vecPosn.y - (x87d)point->y);
        if (!(dy > nRad && dy < radius)) {
            continue;
        }
        const float dz = (float)((x87d)light.m_vecPosn.z - (x87d)point->z);
        if (!(dz > nRad && dz < radius)) {
            continue;
        }
        const x87d dist = x87::sqrt((x87d)dz * dz + (x87d)dy * dy + (x87d)dx * dx);
        if (!(dist < radius)) {
            continue;
        }
        const x87d ratio = dist / (x87d)radius;
        if (light.m_nType == PLTYPE_ANTILIGHT) {
            antilightMult *= ratio;
        } else {
            constexpr x87d k3 = (x87d)ExeRecip(3.0f);
            const x87d om = (x87d)1.0 - ratio;
            lightSum += om * light.m_fColorRed * k3;
            lightSum += om * light.m_fColorGreen * k3;
            lightSum += om * light.m_fColorBlue * k3;
        }
    }
    return (float)(antilightMult + lightSum);
}

// 0x6FFFE0
void CPointLights::RemoveLightsAffectingObject() {
    RemoveExtraDirectionalLights(Scene.m_pRpWorld);
}

// 0x6FFFF0
bool CPointLights::ProcessVerticalLineUsingCache(CVector point, float* outZ) {
    for (auto&& [i, cached] : rngv::enumerate(aCachedMapReads)) {
        if (cached == point) {
            *outZ = aCachedMapReadResults[i];
            return true;
        }
    }

    CColPoint colPoint;
    CEntity*  entity;
    if (!CWorld::ProcessVerticalLine(point, point.z - 20.0f, colPoint, entity, true, false, false, false, true, false, nullptr)) {
        return false;
    }

    aCachedMapReadResults[NextCachedValue] = colPoint.m_vecPoint.z;
    aCachedMapReads[NextCachedValue]       = point;
    NextCachedValue                       = (NextCachedValue + 1) % MAX_POINT_LIGHTS;

    *outZ = colPoint.m_vecPoint.z;
    return true;
}

// 0x7000E0
void CPointLights::AddLight(uint8 lightType, CVector point, CVector direction, float radius, float red, float green, float blue, uint8 fogType, bool generateExtraShadows, CEntity* entityAffected) {
    // Exact x87 form of the exe: float deltas (x, y), z only in the sum of squares (z + y + x), strict range tests (false for NaN), the light is written BEFORE the fade
    // test (fields assigned one by one: the padding byte is left alone), the colours are scaled by the extended-precision fade factor
    const float   maxDist = radius + 15.0f;
    const CVector camPos  = TheCamera.GetPosition();
    const float dx = (float)((x87d)point.x - (x87d)camPos.x);
    if (!(dx < maxDist)) {
        return;
    }
    const float nMax = -maxDist;
    if (!(dx > nMax)) {
        return;
    }
    const float dy = (float)((x87d)point.y - (x87d)camPos.y);
    if (!(dy < maxDist) || !(dy > nMax)) {
        return;
    }
    if (NumLights >= MAX_POINT_LIGHTS) {
        return;
    }
    const x87d dz   = (x87d)point.z - (x87d)camPos.z;
    const float  dist = (float)x87::sqrt(dz * dz + (x87d)dy * dy + (x87d)dx * dx);
    if (!(dist < maxDist)) {
        return;
    }

    CPointLight& l = aLights[NumLights];
    l.m_nType           = static_cast<ePointLightType>(lightType);
    l.m_nFogType        = fogType;
    l.m_vecPosn         = point;
    l.m_vecDirection    = direction;
    l.m_fRadius         = radius;
    l.m_bGenerateShadows = generateExtraShadows;
    l.m_pEntityToLight  = entityAffected;
    if ((double)dist < (x87d)maxDist * (x87d)0.75) { // (dist cannot be NaN here)
        l.m_fColorRed   = red;
        l.m_fColorGreen = green;
        l.m_fColorBlue  = blue;
        NumLights++;
    } else {
        NumLights++;
        // fade the colour out starting at 75% of the max distance
        const x87d fade = (x87d)1.0 - ((x87d)dist / (x87d)maxDist - (x87d)0.75) * (x87d)4.0;
        l.m_fColorRed   = (float)(red * fade);
        l.m_fColorGreen = (float)(green * fade);
        l.m_fColorBlue  = (float)(blue * fade);
    }
}

// 0x7002D0
void CPointLights::RenderFogEffect() {
    ZoneScoped;

    // 0x8D5068 - Sprite scale per fog puff pattern index
    static constexpr float FogSizes[8] = { 1.3f, 2.0f, 1.7f, 2.0f, 1.4f, 2.1f, 1.5f, 2.3f };

    if (CCutsceneMgr::ms_running) {
        return;
    }

    // 0x7006xx / 0x700Axx: `(float)(ms & mask) * 0.0007666015625` (resp. `* 0.000191650390625`): the exe constants are the DOUBLES 6.28 / 8192 and 6.28 / 32768 (fmul qword),
    // not 6.28f * x / 8192
    [[maybe_unused]] const auto FogAngle = [](uint32 maskedMs, double scale) -> float {
#ifdef NOTSA_PORTABLE
        return (float)notsa::fp::Round24Mul((double)maskedMs, scale);
#else
        return (float)((x87d)maskedMs * scale);
#endif
    };

    RwRenderStateSet(rwRENDERSTATEFOGENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(gpCloudTex)));
    CSprite::InitSpriteBuffer();

    const auto RenderFogSprite = [](const CPointLight& light, const CVector& pos, float intensity, float sizeMult, float aspect, float angle) {
        RwV3d screen;
        float w, h;
        if (!CSprite::CalcScreenCoors(pos, &screen, &w, &h, true, true)) {
            return;
        }
        CSprite::RenderBufferedOneXLUSprite_Rotate_Aspect(
            screen.x, screen.y, screen.z,
            w * sizeMult, h * sizeMult * aspect,
            NOTSA_FTOL(uint8, intensity * light.m_fColorRed),
            NOTSA_FTOL(uint8, intensity * light.m_fColorGreen),
            NOTSA_FTOL(uint8, intensity * light.m_fColorBlue),
            NOTSA_FTOL(int16, intensity),
            1.0f / screen.z,
            angle,
            255
        );
    };

    for (const auto& light : GetActiveLights()) {
        float fogAmount, fogSize;
        switch (light.m_nFogType) {
        case rwFOGTYPELINEAR:
            fogAmount = CWeather::Foggyness;
            fogSize   = 9.0f;
            break;
        case rwFOGTYPEEXPONENTIAL:
            fogAmount = 0.4f;
            fogSize   = 3.0f;
            break;
        default:
            continue;
        }
        if (fogAmount == 0.0f) {
            continue;
        }

        const auto& pos = light.m_vecPosn;
        const auto& dir = light.m_vecDirection;

        if (light.m_nType == PLTYPE_DIRECTIONAL) {
            // Fog cone along the light direction (12 units long, 5 units wide), sampled on a 4x4 grid
            constexpr float FOG_LENGTH = 12.0f;
            constexpr float FOG_RADIUS = 5.0f;

            const CVector2D end = CVector2D{ pos } + CVector2D{ dir } * FOG_LENGTH;
            const auto [minX, maxX] = std::minmax(pos.x, end.x);
            const auto [minY, maxY] = std::minmax(pos.y, end.y);

            // Truncate before snapping to the grid, including at negative coordinates.
            const int32 startX = NOTSA_FTOL(int32, minX - FOG_RADIUS) / 4 * 4;
            const int32 startY = NOTSA_FTOL(int32, minY - FOG_RADIUS) / 4 * 4;
            const int32 endX   = NOTSA_FTOL(int32, maxX + FOG_RADIUS) + 4;
            const int32 endY   = NOTSA_FTOL(int32, maxY + FOG_RADIUS) + 4;
            for (int32 x = startX; x <= endX; x += 4) {
                for (int32 y = startY; y <= endY; y += 4) {
                    const auto pattern = ((x >> 2) ^ (y >> 2)) & 0xF;
                    if (pattern % 2 == 0) {
                        continue;
                    }

                    const CVector2D delta2D = CVector2D{ (float)x, (float)y } - CVector2D{ pos };
                    const float     along2D = DotProduct2D(delta2D, dir);
                    // 0x70052x..: `<= 0` / `>= 12` / `>= 25` skip, written so that NaN skips too (fcomp + `test ah, 0x41` / `test ah, 5`)
                    if (!(along2D > 0.0f) || !(along2D < FOG_LENGTH) || !(delta2D.SquaredMagnitude() - sq(along2D) < sq(FOG_RADIUS))) {
                        continue;
                    }

                    CColPoint colPoint;
                    CEntity*  entity;
                    // 0x70058x: the end z is `(pos.z + 10) - 20` (float rounding after each op), not `pos.z - 10`
                    if (!CWorld::ProcessVerticalLine({ (float)x, (float)y, pos.z + 10.0f }, (pos.z + 10.0f) - 20.0f, colPoint, entity, true, false, false, false, true, false, nullptr)) {
                        continue;
                    }

                    const CVector puffPos{ (float)x, (float)y, colPoint.m_vecPoint.z + 1.3f };
                    const CVector delta = puffPos - pos;
                    // 0x7005Dx: the sums are z, then x, then y: (dz * dirZ + dx * dirX) + dy * dirY; the squared length is (dz^2 + dy^2) + dx^2
                    const float   along = (delta.z * dir.z + delta.x * dir.x) + delta.y * dir.y;
                    if (!(along > 0.0f) || !(along < FOG_LENGTH)) {
                        continue;
                    }
                    const float distSq = (delta.z * delta.z + delta.y * delta.y) + delta.x * delta.x;
                    const float perpSq = distSq - sq(along);
                    if (!(perpSq < sq(FOG_RADIUS))) {
                        continue;
                    }

                    // 0x700680..: x / 12 and x / 5 are multiplications by the exe constants (float 1/12, 1/5)
                    const float alongN = along * ExeRecip(FOG_LENGTH);
                    const float perpN  = std::sqrt(perpSq) * ExeRecip(FOG_RADIUS);
                    const float intensity = along / std::sqrt(distSq) * fogAmount * 50.0f
                                          * (1.0f - alongN * alongN)
                                          * (1.0f - perpN * perpN);
                    const auto puffIndex = pattern >> 1;
                    RenderFogSprite(light, puffPos, intensity, FogSizes[puffIndex], 1.0f,
                        FogAngle(CTimer::GetTimeInMS() & 0x1FFF, 0.0007666015625)); // angle (0, 2pi): 6.28 / 8192
                }
            }
        } else if (light.m_nType == PLTYPE_POINTLIGHT || light.m_nType == PLTYPE_ONLYFOGEFFECT_ALWAYS || light.m_nType == PLTYPE_ONLYFOGEFFECT) {
            // Fog disc around the light, sampled on a 2x2 grid
            float groundZ;
            if (!ProcessVerticalLineUsingCache(pos, &groundZ)) {
                continue;
            }

            // Truncate before snapping to the grid, including at negative coordinates.
            const int32 startX = NOTSA_FTOL(int32, pos.x - fogSize) / 2 * 2;
            const int32 startY = NOTSA_FTOL(int32, pos.y - fogSize) / 2 * 2;
            const int32 endX   = NOTSA_FTOL(int32, pos.x + fogSize) + 2;
            const int32 endY   = NOTSA_FTOL(int32, pos.y + fogSize) + 2;
            for (int32 x = startX; x <= endX; x += 2) {
                for (int32 y = startY; y <= endY; y += 2) {
                    // Cheap spatial hashing to get somewhat a random number for fog generation
                    // Imagine evaluating only a single round of some hashing algorithm (e.g. FNV)
                    const auto hash = ((x / 2) ^ (y / 2)) & 0xF;
                    if (!(hash & 1)) {
                        // is_odd(x/2) != is_odd(y/2)
                        // Filtering this case creates a checkerboard pattern, halving the no of render operations
                        continue;
                    }

                    const CVector2D puffPos2D{ (float)x, (float)y };
                    // 0x7009A0..: the exe compares the (rounded) distance itself, not its square: `!(sqrt(dx^2 + dy^2) < fogSize)`
                    const float     dist = (puffPos2D - CVector2D{ pos }).Magnitude();
                    if (!(dist < fogSize)) {
                        continue;
                    }

                    const float camDist = (puffPos2D - CVector2D{ TheCamera.GetPosition() }).Magnitude();
                    if (!(camDist < 15.0f)) {
                        continue;
                    }
                    const float camFade = camDist < 7.5f ? 1.0f : 1.0f - (camDist - 7.5f) * ExeRecip(7.5f);

                    // 0x700A3x: q = dist * (1.0f / fogSize) (a real division, then a multiplication), intensity = ((1 - q^2) * camFade) * fogAmount * 37
                    const float q = dist * (1.0f / fogSize);
                    const float intensity = (1.0f - q * q) * camFade * fogAmount * 37.0f;
                    const auto puffIndex = hash >> 1;

                    constexpr auto PhaseOffsetMs = 2300; // Intentionally non-power of two to appear asymmetric
                    RenderFogSprite(
                        light,
                        { puffPos2D.x, puffPos2D.y, groundZ + 1.6f },
                        intensity,
                        FogSizes[puffIndex],
                        0.7f,
                        FogAngle((CTimer::GetTimeInMS() + puffIndex * PhaseOffsetMs) & 0x7FFF, 0.000191650390625) // angle (0,2pi): 6.28 / 32768
                    );
                }
            }
        }
    }

    CSprite::FlushSpriteBuffer();
}
