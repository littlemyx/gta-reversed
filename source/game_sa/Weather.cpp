/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include <reversiblebugfixes/Bugs.hpp>

#include "eWeatherType.h"
#include "PostEffects.h"
#include "game_sa/Data/Weather.def.h"

// 0x8CCF30
std::array<float, 16> CWeather::saTreeWindOffsets = { 1.0f, 0.5f, 0.2f, 0.7f, 0.4f, 1.0f, 0.5f, 0.3f, 0.2f, 0.1f, 0.7f, 0.6f, 0.3f, 1.0f, 0.5f, 0.2f };

/// 0x8CCF70
std::array<float, 32> CWeather::saBannerWindOffsets = { 0.0f, 0.3f, 0.6f, 0.85f, 0.99f, 0.97f, 0.65f, 0.15f, -0.1f, 0.0f, 0.35f, 0.57f, 0.55f, 0.35f, 0.45f, 0.67f, 0.73f, 0.45f, 0.25f, 0.35f, 0.35f, 0.11f, 0.13f, 0.21f, 0.28f, 0.28f, 0.22f, 0.1f, 0.0f, -0.1f, -0.17f, -0.12f };

void CWeather::InjectHooks() {
    RH_ScopedClass(CWeather);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x72A480);
    RH_ScopedInstall(AddRain, 0x72A9A0);
    RH_ScopedInstall(AddSandStormParticles, 0x72A820);
    RH_ScopedInstall(FindWeatherTypesList, 0x72A520);
    RH_ScopedInstall(ForceWeather, 0x72A4E0);
    RH_ScopedInstall(ForceWeatherNow, 0x72A4F0);
    RH_ScopedInstall(ForecastWeather, 0x72A590);
    RH_ScopedInstall(ReleaseWeather, 0x72A510);
    RH_ScopedInstall(RenderRainStreaks, 0x72AF70);
    RH_ScopedInstall(SetWeatherToAppropriateTypeNow, 0x72A790);
    RH_ScopedInstall(Update, 0x72B850);
    RH_ScopedInstall(UpdateInTunnelness, 0x72B630);
    RH_ScopedInstall(UpdateWeatherRegion, 0x72A640);
    RH_ScopedInstall(IsRainy, 0x4ABF50);
}

// 0x72A480
void CWeather::Init() {
    ZoneScoped;

    NewWeatherType = WEATHER_EXTRASUNNY_LA;
    OldWeatherType = WEATHER_EXTRASUNNY_LA;
    WeatherRegion  = WEATHER_REGION_DEFAULT;

    InterpolationValue = 0.0f;
    WeatherTypeInList = 0;
    ForcedWeatherType = WEATHER_UNDEFINED;
    WhenToPlayLightningSound = 0;
    bScriptsForceRain = false;
    Rain = 0.0f;
    Sandstorm = 0.0f;
    CurrentRainParticleStrength = 0;
    InTunnelness = 0.0f;
    LightningStartX = 0;
    LightningStartY = 0;
    StreamAfterRainTimer = 0;
}

// 0x72A9A0
void CWeather::AddRain() {
    constexpr float RAIN_HAZE_ALPHA_MULT = 1.0f; // 0x8D5FF0

    NOTSA_GLOBAL_LOCAL(s_RainedRecently, 0xC81328, (int32), {});
    NOTSA_GLOBAL_LOCAL(s_RainHazeAlpha, 0xC81410, (float), {});

    if (CCullZones::CamNoRain() || CCullZones::PlayerNoRain()) {
        return;
    }

    if (IsUnderWater()) {
        return;
    }

    if (!CGame::CanSeeOutSideFromCurrArea()) {
        return;
    }

    if (FindPlayerPed() && FindPlayerPed()->GetAreaCode() != AREA_CODE_NORMAL_WORLD) {
        return;
    }

    if (!(TheCamera.GetPosition().z <= 900.0f)) { // 0x72AA2D: `je` leaves for > 900 and for NaN
        return;
    }

    if (TheCamera.GetLookingLRBFirstPerson()) {
       if (const auto vehicle = FindPlayerVehicle()) {
              if (vehicle->CarHasRoof()) {
                    return;
               }
        }
    }

    // 0x72AA5B
    // TODO: FPS dependent logic. This runs once per frame: `StreamAfterRainTimer` counts
    //       frames (800 frames), the splash and haze particles are emitted every frame,
    //       and the haze alpha moves 0.0025 per frame.
    if (Rain > 0.0f) {
        s_RainedRecently     = 1;
        StreamAfterRainTimer = 800;
    } else if (s_RainedRecently) {
        if (StreamAfterRainTimer > 0) {
            StreamAfterRainTimer--;
        } else {
            s_RainedRecently     = 0;
            StreamAfterRainTimer = 800;
        }
    }

    // 0x72AAA8
    if (Wind > 1.01f && !CCullZones::CamNoRain() && !CCullZones::PlayerNoRain() && !IsUnderWater()) {
        AddSandStormParticles();
    }

    if (!(Rain > 0.1f) && s_RainHazeAlpha == 0.0f) { // 0x72AAF8: NaN rain counts as "not rainy"
        return;
    }

    // 0x72AB0F
    const auto numSplashSpots     = NOTSA_FTOL(int32, Rain * 5.0f);
    const auto maxRadius          = std::max(Rain * 10.0f, 40.0f) * 0.5f; // Always 20 for normal rain values, `std::min` looks intended
    const auto numSplashesPerSpot = 15 - NOTSA_FTOL(int32, Rain * -2.0f);
    for (auto i = 0; i < numSplashSpots; i++) {
        const FxPrtMult_c splashMults(1.0f, 1.0f, 1.0f, 0.25f, 0.02f, 0.0f, 0.03f);
        const CVector     splashVelocity{};
        const auto        radius = CGeneral::GetRandomNumberInRange(0.0f, maxRadius);

        // 0x72ABC2-0x72AC11: the angle is `(rand & 0xFF) * 0.02453125` (the exe's 2pi/256 constant is 6.28/256) or `((rnd & 0xFF) - 128) * 0.00625 + camera heading` (the SAME rand)
        const auto rnd  = CGeneral::GetRandomNumber();
        const auto rads = (rnd & 1)
            ? (float)(CGeneral::GetRandomNumber() & 0xFF) * 0.02453125f
            : (float)((rnd & 0xFF) - 128) * 0.00625f + TheCamera.m_fOrientation;

        // 0x72AC11: sin / cos stay unrounded until the multiply by the radius, then `+ camera`
        const auto&  camPos = TheCamera.GetPosition();
        const CVector2D spot{
            (float)X87R(x87::sin(rads) * radius) + camPos.x,
            (float)X87R(x87::cos(rads) * radius) + camPos.y
        };
        CColPoint colPoint{};
        CEntity*  colEntity{};
        if (!CWorld::ProcessVerticalLine(CVector{ spot, 40.0f }, -40.0f, colPoint, colEntity, true, false, false, false, true)) {
            continue;
        }

        // 0x72ACC0: per splash `((rand * (1/32767)) * 30 + spot) - 15` for x and y, then a THIRD rand() that is consumed and discarded (the exe's z is unchanged)
        const float spotZ = colPoint.m_vecPoint.z + 0.1f;
        for (auto s = 0; s < numSplashesPerSpot; s++) {
            CVector pos{ spot, spotZ };
            pos.x = ((float)CGeneral::GetRandomNumber() * RAND_MAX_FLOAT_RECIPROCAL * 30.0f + pos.x) - 15.0f;
            pos.y = ((float)CGeneral::GetRandomNumber() * RAND_MAX_FLOAT_RECIPROCAL * 30.0f + pos.y) - 15.0f;
            (void)CGeneral::GetRandomNumber();
            g_fx.m_Splash->AddParticle(pos, splashVelocity, 0.0f, splashMults);
        }
    }

    // 0x72AD6E: `if (a < target) a += 0.0025; if (a > target) a -= 0.0025;` (NO clamp to the target: the alpha oscillates within one step of it),
    // then `max(a, 0)`, `* 1.0`, `min(a, 1)`
    {
        const float target = Rain * 0.2f;
        float       a      = s_RainHazeAlpha;
        if (a < target) {
            a += 0.0025f;
        }
        if (a > target) {
            a -= 0.0025f;
        }
        if (0.0f > a) {
            a = 0.0f;
        }
        a = a * RAIN_HAZE_ALPHA_MULT;
        if (1.0f < a) {
            a = 1.0f;
        }
        s_RainHazeAlpha = a;
    }

    // 0x72ADEA: the same position formula as AddSandStormParticles
    CVector hazePos = TheCamera.GetPosition();
    hazePos.x += TheCamera.m_mCameraMatrix.GetForward().x * 10.0f;
    hazePos.y += TheCamera.m_mCameraMatrix.GetForward().y * 10.0f;
    hazePos.x = (CGeneral::GetRandomNumberInRange(0.0f, 40.0f) + hazePos.x) - 20.0f;
    hazePos.y = (CGeneral::GetRandomNumberInRange(0.0f, 40.0f) + hazePos.y) - 20.0f;
    hazePos.z = (CGeneral::GetRandomNumberInRange(0.0f, 7.00f) + hazePos.z) - 2.00f;
    g_fx.m_Sand2->AddParticle(
        hazePos,
        WindDir * 15.0f,
        0.0f,
        FxPrtMult_c(0.9f, 0.9f, 1.0f, s_RainHazeAlpha, 1.0f, 0.0f, 0.2f)
    );
}

// 0x72A820
void CWeather::AddSandStormParticles() {
    CVector position = TheCamera.GetPosition();
    position.x += TheCamera.m_mCameraMatrix.GetForward().x * 10.0f;
    position.y += TheCamera.m_mCameraMatrix.GetForward().y * 10.0f;

    // exe 0x72A87B-0x72A8F1: `(rand * 40 + pos) - 20` (the offset is added to the position BEFORE the constant is subtracted), rand order x, y, z
    position.x = (CGeneral::GetRandomNumberInRange(0.0f, 40.0f) + position.x) - 20.0f;
    position.y = (CGeneral::GetRandomNumberInRange(0.0f, 40.0f) + position.y) - 20.0f;
    position.z = (CGeneral::GetRandomNumberInRange(0.0f, 7.00f) + position.z) - 2.00f;

    g_fx.m_Sand2->AddParticle(position, CWeather::WindDir * 25.0f, 0.0f, FxPrtMult_c(0.67f, 0.65f, 0.55f, 0.25f, 1.0f, 0.0f, 0.2f));
}

// 0x72A520
const eWeatherType* CWeather::FindWeatherTypesList() {
    switch (WeatherRegion) {
    case WEATHER_REGION_LA:     return WeatherTypesListLA;
    case WEATHER_REGION_SF:     return WeatherTypesListSF;
    case WEATHER_REGION_LV:     return WeatherTypesListVegas;
    case WEATHER_REGION_DESERT: return WeatherTypesListDesert;
    default:                    return WeatherTypesListDefault;
    }
}

// 0x72A4E0
void CWeather::ForceWeather(eWeatherType weatherType) {
    ForcedWeatherType = weatherType;
}

// 0x72A4F0
void CWeather::ForceWeatherNow(eWeatherType weatherType) {
    ForcedWeatherType = weatherType;
    OldWeatherType = weatherType;
    NewWeatherType = weatherType;
}

//! The exe's inlined "move towards" (0x72BC47, 0x72BCD8, 0x72B7EB): a distance below the step snaps to the target, otherwise +-step by the sign of the distance (NaN distance: subtract)
static float ExeStepToward(float cur, float target, float step) {
    const float d = target - cur;
    if (std::fabs(d) < step) {
        return target;
    }
    return d > 0.0f ? cur + step : cur - step;
}

// 0x72A590
bool CWeather::ForecastWeather(eWeatherType weatherType, int32 numSteps) {
    for (auto step = 0; step <= numSteps; step++) {
        if (FindWeatherTypesList()[(WeatherTypeInList + step) % WEATHER_TYPES_LIST_SIZE] == weatherType) {
            return true;
        }
    }
    return false;
}

// 0x72A510
void CWeather::ReleaseWeather() {
    ForcedWeatherType = WEATHER_UNDEFINED;
}

// 0x72AF70
void CWeather::RenderRainStreaks() {
    if (CTimer::GetIsCodePaused())
        return;

    {
        // 0x72AF79-0x72AFAB: `_ftol((64 - fogReduction) * (int)_ftol(rain * 110) * (1/64))` - the rain term is truncated to an int first
        const int32 rainTerm = NOTSA_FTOL(int32, Rain * 110.0f);
        const auto strength = NOTSA_FTOL(int32, (64.0f - (float)CTimeCycle::m_FogReduction) * (float)rainTerm * 0.015625f);
        // 0x72AFA0-0x72AFD0: signed compare; the result is clamped at 0 after the decrement
        int32 cur = (int32)CurrentRainParticleStrength;
        if (cur < strength) {
            cur++;
        } else {
            cur--;
        }
        CurrentRainParticleStrength = (uint32)(cur < 0 ? 0 : cur);
    }

    if (!CurrentRainParticleStrength)
        return;

    if (CCullZones::CamNoRain() || CCullZones::PlayerNoRain())
        return;

    if (UnderWaterness > 0.0f)
        return;

    if (CGame::currArea)
        return;

    const CVector camPos = TheCamera.GetPosition();
    if (camPos.z > 900.0f)
        return;

    uiTempBufferIndicesStored = 0;
    uiTempBufferVerticesStored = 0;

    // (Pirulax) TODO... (refactor)
    constexpr auto RAIN_STREAK_COUNT{ 32u };

    // These are arrays of size `RAIN_STREAK_COUNT`
    NOTSA_GLOBAL_LOCAL(streakPosX, 0xC81420, (int32*), {});
    NOTSA_GLOBAL_LOCAL(streakPosY, 0xC8141C, (int32*), {});
    NOTSA_GLOBAL_LOCAL(streakPosZ, 0xC81418, (int32*), {});
    NOTSA_GLOBAL_LOCAL(streakStrength, 0xC81414, (uint8*), {});

    if (!streakPosX) {
        // This stuff isn't even freed anywhere..
        streakPosX     = new int32[RAIN_STREAK_COUNT];
        streakPosY     = new int32[RAIN_STREAK_COUNT];
        streakPosZ     = new int32[RAIN_STREAK_COUNT];
        streakStrength = new uint8[RAIN_STREAK_COUNT];

        for (unsigned i = 0; i < RAIN_STREAK_COUNT; i++) {
            streakPosX[i] = 0;
            streakPosY[i] = 0;
            streakPosZ[i] = 0;
            streakStrength[i] = NOTSA_FTOL(uint8, (float)CurrentRainParticleStrength * 0.6f);
        }
    }

    const auto GetStreakPosition = [&](unsigned i) {
        return CVector{ (float)(streakPosX[i]), (float)(streakPosY[i]), (float)(streakPosZ[i]) };
    };

    const auto UpdateStreak = [&](unsigned i) {
        const CVector posn = GetStreakPosition(i);
        if (!streakStrength[i] || posn.z <= 0.0f || (camPos - posn).Magnitude() > 8.0f) {
            const CVector newPosn = CVector::Random(0.0f, 5.0f) + TheCamera.GetForward() * 6.0f + camPos - CVector{2.5f, 2.5f, 2.5f};
            streakPosX[i] = NOTSA_FTOL(int32, newPosn.x);
            streakPosY[i] = NOTSA_FTOL(int32, newPosn.y);
            streakPosZ[i] = NOTSA_FTOL(int32, newPosn.z);

            streakStrength[i] = NOTSA_FTOL(uint8, (float)CurrentRainParticleStrength * 0.6f);
        }
    };

    const auto GetRealVertexIndex = [](unsigned i) {
        return uiTempBufferVerticesStored + i;
    };

    for (unsigned s = 0; s < RAIN_STREAK_COUNT; s++) {
        UpdateStreak(s);

        // 0x72B2F3-0x72B4E4: rand order: the extra z lift (t1), then the x / y / z offsets of vertex 0
        const float t1 = (float)CGeneral::GetRandomNumber() * RAND_MAX_FLOAT_RECIPROCAL;
        const float xo = CGeneral::GetRandomNumberInRange(-0.2f, 0.2f);
        const float yo = CGeneral::GetRandomNumberInRange(-0.2f, 0.2f);
        const float zo = CGeneral::GetRandomNumberInRange(-0.1f, 0.1f);

        // `((WindDir * Wind) [* Rain]) * 0.1` (even streaks include the rain), subtracted from the streak position
        const bool  odd = (s % 2) != 0;
        const float wx  = odd ? WindDir.x * Wind * 0.1f : WindDir.x * Wind * Rain * 0.1f;
        const float wy  = odd ? WindDir.y * Wind * 0.1f : WindDir.y * Wind * Rain * 0.1f;
        const float wz  = odd ? WindDir.z * Wind * 0.1f : WindDir.z * Wind * Rain * 0.1f;
        const CVector base1{ (float)streakPosX[s] - wx, (float)streakPosY[s] - wy, (float)streakPosZ[s] - wz };

        // vertex 1 is `offset + (pos - wind)` (not `pos + (offset - wind)`), z gets the extra `t1 * 0.4 + 0.1` lift
        const CVector vertPosns[2]{
            CVector{ (float)streakPosX[s] + xo, (float)streakPosY[s] + yo, (float)streakPosZ[s] + zo },
            CVector{ xo + base1.x, yo + base1.y, ((t1 * 0.4f + 0.1f) + zo) + base1.z }
        };

        const uint8 alphas[]{ streakStrength[s], static_cast<uint8>(streakStrength[s] / 2u) };
        for (auto v = 0u; v < std::size(alphas); v++) {
            RxObjSpace3DVertex* vertex = &TempBufferVertices.m_3d[GetRealVertexIndex(v)];

            const RwRGBA color{ 210, 210, 230, alphas[v] };
            // const RwRGBA color{ 255, 0, 0, 255 }; // For debug (makes it more visible)
            RxObjSpace3DVertexSetPreLitColor(vertex, &color);

            RxObjSpace3DVertexSetPos(vertex, &vertPosns[v]);

            aTempBufferIndices[uiTempBufferIndicesStored + v] = GetRealVertexIndex(v);
        }

        // 0x72B4E4: the new z is `_ftol(z - (rand * 0.09 + 0.01))` (the int z falls by one per frame), NOT `z -= (int)(0.01..0.1)` (always 0)
        streakPosZ[s] = NOTSA_FTOL(int32, (float)streakPosZ[s] - ((float)CGeneral::GetRandomNumber() * RAND_MAX_FLOAT_RECIPROCAL * 0.09f + 0.01f));
        streakStrength[s] = (uint8)std::max(0, (int32)streakStrength[s] - CGeneral::GetRandomNumberInRange(2, 5));

        uiTempBufferVerticesStored += 2;
        uiTempBufferIndicesStored += 2;
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEFOGTYPE,           RWRSTATE(rwFOGTYPELINEAR));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(NULL));

    if (RwIm3DTransform(TempBufferVertices.m_3d, uiTempBufferVerticesStored, nullptr, rwIM3D_VERTEXXYZ)) {
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPELINELIST, aTempBufferIndices, uiTempBufferIndicesStored);
        RwIm3DEnd();
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
}

// 0x72A790
void CWeather::SetWeatherToAppropriateTypeNow() {
    CVector playerCoors = FindPlayerCoors();
    UpdateWeatherRegion(&playerCoors);

    auto weatherType = FindWeatherTypesList()[0];
    ForcedWeatherType = WEATHER_UNDEFINED;
    OldWeatherType = weatherType;
    NewWeatherType = weatherType;
}

// 0x72B850
void CWeather::Update() {
    ZoneScoped;

    static constexpr std::array<float, NUM_WEATHERS> WIND_FOR_WEATHER_TYPE = { // 0x8D5E50
        0.0f, 0.25f, 0.0f, 0.2f, 0.7f, 0.25f, 0.0f, 0.7f, 1.0f, 0.0f, 0.2f, 0.0f, 0.4f, 0.0f, 0.3f, 0.7f, 1.0f, 0.0f, 0.3f, 1.5f, 0.0f, 0.0f, 0.0f
    };

    static constexpr std::array<float, 16> WIND_DIR_SCALES = { // 0x8D5FF8
        1.0f, 0.5f, 1.0f, 0.2f, 0.4f, 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 0.0f, 1.0f, 1.0f, 0.7f, 1.0f, 1.0f
    };

    static constexpr std::array<float, 16> WIND_DIR_OFFSETS = { // 0x8D6038
        0.5f, -0.3f, 0.8f, 0.0f, -0.4f, -0.8f, 0.3f, -0.1f, -0.9f, -0.5f, 0.7f, 0.7f, 0.3f, 0.7f, 0.0f, -0.5f
    };

    const auto IsSunny = [](eWeatherType wt) {
        return notsa::contains({ WEATHER_SUNNY_LA, WEATHER_SUNNY_SMOG_LA, WEATHER_SUNNY_COUNTRYSIDE, WEATHER_SUNNY_SF, WEATHER_SUNNY_VEGAS, WEATHER_SUNNY_DESERT }, wt);
    };
    const auto IsCloudy = [](eWeatherType wt) {
        return notsa::contains({ WEATHER_CLOUDY_LA, WEATHER_CLOUDY_COUNTRYSIDE, WEATHER_CLOUDY_VEGAS, WEATHER_CLOUDY_SF }, wt);
    };
    const auto IsRainy = [](eWeatherType wt) {
        return wt == WEATHER_RAINY_COUNTRYSIDE || wt == WEATHER_RAINY_SF;
    };
    const auto IsFoggy = [](eWeatherType wt) {
        return wt == WEATHER_FOGGY_SF || wt == WEATHER_SANDSTORM_DESERT;
    };
    const auto IsHeatHazy = [](eWeatherType wt) {
        return notsa::contains({ WEATHER_SUNNY_DESERT, WEATHER_EXTRASUNNY_DESERT, WEATHER_EXTRASUNNY_VEGAS, WEATHER_EXTRASUNNY_LA }, wt);
    };
    const auto IsSandstorm = [](eWeatherType wt) {
        return wt == WEATHER_SANDSTORM_DESERT;
    };
    const auto IsFoggySF = [](eWeatherType wt) {
        return wt == WEATHER_FOGGY_SF;
    };
    const auto LerpOldToNew = [](auto&& Pred) {
        const auto from = Pred(OldWeatherType) ? 1.0f : 0.0f;
        const auto to   = Pred(NewWeatherType) ? 1.0f : 0.0f;
        return from == to
            ? to
            : lerp(from, to, InterpolationValue);
    };

    if (CTimer::GetFrameCounter() % 16 == 0) {
        UpdateWeatherRegion(nullptr);
    }

    // 0x72B866
    if (CReplay::Mode != MODE_PLAYBACK) {
        const auto prevInterpolationValue = std::exchange(
            InterpolationValue,
            ((float)CClock::GetGameClockSeconds() * ExeRecip(60.f) + (float)CClock::GetGameClockMinutes()) * ExeRecip(60.f)
        );
        if (InterpolationValue < prevInterpolationValue) {
            UpdateWeatherRegion(nullptr);
            OldWeatherType = NewWeatherType;
            if (ForcedWeatherType >= 0) {
                NewWeatherType = ForcedWeatherType;
            } else if (TheCamera.GetPosition().z < 950.0f) { // Above z 950 the list is not advanced, so the weather stops changing
                WeatherTypeInList = (WeatherTypeInList + 1) % WEATHER_TYPES_LIST_SIZE;
                NewWeatherType = FindWeatherTypesList()[WeatherTypeInList];
            }
        }
    }

    // 0x72B96E
    // TODO: FPS dependent logic
    // The burst length is counted in frames via `CTimer::GetFrameCounter()`,
    // and the start/stop chances are rolled once per frame
    if (IsRainy(NewWeatherType) && IsRainy(OldWeatherType) && !CCullZones::CamNoRain() && !CCullZones::PlayerNoRain() && !IsUnderWater() && CGame::CanSeeOutSideFromCurrArea()) {
        if (LightningBurst) {
            if (CGeneral::RandomBool(9.4f)) { // 24 / 256 * 100 ~ 9.4
                LightningBurst = false;
                LightningDuration = std::min(CTimer::GetFrameCounter() - LightningStart, 20u);
                WhenToPlayLightningSound = (20 - LightningDuration) * 150 + CTimer::GetTimeInMS();
                LightningFlash = false;
            } else if (CTimer::GetTimeInMS() - LightningFlashLastChange > 50) {
                const auto wasFlashing = LightningFlash;
                LightningFlash = CGeneral::RandomBit(); // exe 0x72BA55: rand() & 1 (DoCoinFlip is rand >= 16383)
                if (LightningFlash != wasFlashing) {
                    LightningFlashLastChange = CTimer::GetTimeInMS();
                }
            }
        } else if (CGeneral::RandomBool(0.6f)) { // 200 / RAND_MAX * 100 ~ 0.6
            LightningStart = CTimer::GetFrameCounter();
            LightningFlashLastChange = CTimer::GetTimeInMS();
            LightningBurst = true;
            LightningFlash = true;
        } else {
            LightningFlash = false;
        }
    } else {
        LightningFlash = false;
        LightningBurst = false;
    }

    // 0x72BB25
    if (WhenToPlayLightningSound && CTimer::GetTimeInMS() > WhenToPlayLightningSound) {
        m_WeatherAudioEntity.AddAudioEvent(AE_THUNDER);
        CPad::GetPad(0)->StartShake((int16)(40 * LightningDuration + 100), (uint8)((LightningDuration + 40) * 2), 0);
        WhenToPlayLightningSound = 0;
    }

    // 0x72BB87
    WetRoads = IsRainy(OldWeatherType)
        ? (IsRainy(NewWeatherType) ? 1.0f : 1.0f - InterpolationValue)
        : (IsRainy(NewWeatherType) ? InterpolationValue : 0.0f);

    // 0x72BBE1
    const auto step      = CTimer::GetTimeStep() * 0.005f;
    const auto intensity = (float)((CTimer::GetTimeInMS() >> 13) % 4) * 0.1f + 0.7f;

    Rain = ExeStepToward(
        Rain,
        intensity * LerpOldToNew(IsRainy),
        step
    );

    // 0x72BC98
    Sandstorm = ExeStepToward(
        Sandstorm,
        intensity * LerpOldToNew(IsSandstorm),
        step
    );

    // 0x72BD11
    CloudCoverage = LerpOldToNew([&](eWeatherType wt) {
        return !IsSunny(wt) && !IsExtraSunny(wt);
    });

    // 0x72BDD1
    Foggyness = LerpOldToNew(IsFoggy);

    // 0x72BE19
    Foggyness_SF = LerpOldToNew(IsFoggySF);

    // 0x72BE55
    ExtraSunnyness = LerpOldToNew(IsExtraSunny);

    // 0x72BECB
    Rainbow = IsCloudy(OldWeatherType) && IsSunny(NewWeatherType) && InterpolationValue < 0.5f && CClock::GetGameClockHours() > 6 && CClock::GetGameClockHours() < 21
        ? 1.0f - std::abs(InterpolationValue - 0.25f) * 4.0f
        : 0.0f;

    // 0x72BF63
    SunGlare = LerpOldToNew([&](eWeatherType wt) {
        return IsExtraSunny(wt) || IsSunny(wt);
    });

    // 0x72C021
    if (SunGlare > 0.0f) {
        SunGlare *= std::min(CTimeCycle::GetVectorToSun().z * 7.0f, 1.0f);
        SunGlare = std::clamp(SunGlare, 0.0f, 1.0f);
        if (!CSpecialFX::bSnapShotActive) {
            SunGlare *= 1.0f - (float)(CGeneral::GetRandomNumber() % 32) * 0.007f;
        }
    }

    // 0x72C0FB
    HeatHaze = LerpOldToNew(IsHeatHazy);

    // 0x72C157
    if (HeatHaze > 0.0f) {
        const auto minutes       = CClock::GetGameClockMinutes();
        const auto hours         = CClock::GetGameClockHours();
        const auto minuteChanged = minutes != HeatHazeFXLastMinute;

        HeatHazeFXControl = 0.0f;

        const auto fadeIn  = hours >= CPostEffects::m_HeatHazeFXHourOfDayStart && hours < CPostEffects::m_HeatHazeFXHourOfDayEnd;
        const auto fadeOut = hours >= CPostEffects::m_HeatHazeFXHourOfDayEnd;

        // 0x72C1C8
        const auto isOutside = CGame::CanSeeOutSideFromCurrArea()
            && ((notsa::bugfixes::GenericCrashing && !FindPlayerPed()) || FindPlayerPed()->GetAreaCode() == AREA_CODE_NORMAL_WORLD)
            && !CCullZones::CamNoRain()
            && !CCullZones::PlayerNoRain();
        const auto fadeSpeed = isOutside
            ? CPostEffects::m_fHeatHazeFXFadeSpeed
            : CPostEffects::m_fHeatHazeFXInsideBuildingFadeSpeed;

        // 0x72C20E
        if (isOutside && fadeIn) {
            if (minuteChanged) {
                HeatHazeFXFade += fadeSpeed;
            }
            HeatHazeFXFade = std::min(HeatHazeFXFade, 1.0f);
            HeatHazeFXControl = HeatHazeFXFade;
        }

        // 0x72C26D
        if (!isOutside || fadeOut) {
            if (minuteChanged) {
                HeatHazeFXFade -= fadeSpeed;
            }
            HeatHazeFXFade = std::max(HeatHazeFXFade, 0.0f);
            HeatHazeFXControl = HeatHazeFXFade;
        }

        HeatHazeFXControl *= HeatHaze;
        HeatHazeFXLastMinute = minutes;
    }

    // 0x72C2C9
    const auto waterFogAlpha = CTimeCycle::m_CurrentColours.m_nWaterFogAlpha;
    const auto waterFog      = (float)waterFogAlpha * 0.01f;
    if (!waterFogAlpha) {
        WaterFogFXFade = 0.0f;
    } else if (waterFogAlpha >= 95) {
        WaterFogFXFadingOut = true;
    }
    if (WaterFogFXFadingOut) {
        WaterFogFXFade = std::min(WaterFogFXFade, waterFog);
        if (WaterFogFXFade <= 0.0f) {
            WaterFogFXFadingOut = false;
        }
    } else {
        WaterFogFXFade = std::max(WaterFogFXFade, waterFog);
    }
    WaterFogFXControl = std::clamp(WaterFogFXFade * 1.4f, 0.0f, 1.0f);

    // 0x72C398
    Wind = lerpBlend(WIND_FOR_WEATHER_TYPE[OldWeatherType], WIND_FOR_WEATHER_TYPE[NewWeatherType], InterpolationValue); // 0x72C398: from * (1 - t) + to * t
    WindClipped = std::min(Wind, 1.0f);

    // 0x72C3F3
    WindDir.x = WindClipped * 0.7f;
    WindDir.y = WindClipped * 0.7f;

    const auto timeMs = CTimer::GetTimeInMS();

    const auto LerpWindDirOffset = [](size_t idx, float t) {
        return lerpBlend(WIND_DIR_OFFSETS[idx % std::size(WIND_DIR_OFFSETS)], WIND_DIR_OFFSETS[(idx + 1) % std::size(WIND_DIR_OFFSETS)], t);
    };

    const auto slowIdx = (timeMs / 1024) % std::size(WIND_DIR_OFFSETS);
    const auto slowT   = 0.5f - X87R(x87::cos((float)(timeMs % 1024) / 1024.0f * PI) * 0.5f);
    auto windX = LerpWindDirOffset(slowIdx, slowT) * WindClipped * 0.4f + WindDir.x;
    auto windY = LerpWindDirOffset(slowIdx + 3, slowT) * WindClipped * 0.4f + WindDir.y;
    WindDir.z  = LerpWindDirOffset(slowIdx + 6, slowT) * WindClipped * 0.2f;

    // 0x72C4F5
    if (const auto gust = (WindClipped - 0.5f) * 0.4f; gust > 0.0f) {
        const auto fastIdx = (timeMs / 256) % std::size(WIND_DIR_OFFSETS);
        const auto fastT   = (float)(timeMs % 256) / 256.0f;
        windX     += LerpWindDirOffset(fastIdx, fastT) * gust;
        windY     += LerpWindDirOffset(fastIdx + 3, fastT) * gust;
        WindDir.z += LerpWindDirOffset(fastIdx + 6, fastT) * gust;
    }

    // 0x72C5B6
    const auto scaleIdx = (timeMs / 2048) % std::size(WIND_DIR_SCALES);
    const auto scaleT   = 0.5f - X87R(x87::cos((float)(timeMs % 2048) / 2048.0f * PI) * 0.5f);
    const auto scale    = lerpBlend(WIND_DIR_SCALES[scaleIdx], WIND_DIR_SCALES[(scaleIdx + 1) % std::size(WIND_DIR_SCALES)], scaleT);
    WindDir.x  = scale * windX;
    WindDir.y  = scale * windY;
    WindDir.z *= scale;

    // 0x72C63C
    Wavyness = std::min(WindClipped + 0.3f, 1.0f);
    Rain     = Rain < 1.0f - UnderWaterness ? Rain : 1.0f - UnderWaterness; // 0x72C66B: NOT std::min (a NaN Rain takes the other operand)

    // 0x72C690
    const auto hours = CClock::GetGameClockHours();
    if (hours > 20) { // [21, 23]
        TrafficLightsBrightness = 1.0f;
    } else if (hours > 19) { // 20
        TrafficLightsBrightness = (float)CClock::GetGameClockMinutes() * ExeRecip(60.f);
    } else if (hours > 6) { // [7, 19]
        TrafficLightsBrightness = 0.0f;
    } else if (hours > 5) { // 6
        TrafficLightsBrightness = 1.0f - (float)CClock::GetGameClockMinutes() * ExeRecip(60.f);
    } else { // [0, 5]
        TrafficLightsBrightness = 1.0f;
    }

    // 0x72C6FA
    HeadLightsSpectrum      = std::min(Foggyness > Rain ? Foggyness : Rain, 1.0f);
    // 0x72C765: each step is `if (!(current > next)) current = next` (NaN takes the next operand), not std::max({...})
    for (const float next : { WetRoads, Foggyness, Rain }) {
        if (!(TrafficLightsBrightness > next)) {
            TrafficLightsBrightness = next;
        }
    }

    AddRain();

    // 0x72C7C5
    // There's some code here that ends up just calling `FindPlayerPed()` (So basically does *nothing*)

    UpdateInTunnelness();
    m_WeatherAudioEntity.Service();
}

// 0x72B630
void CWeather::UpdateInTunnelness() {
    ZoneScoped;

    static const CVector s_TunnelPoint1{ 85.0f, -1020.0f, 0.0f }; // 0xC81430
    static const CVector s_TunnelPoint2{ 1683.0f, -1956.0f, 0.0f }; // 0xC81424

    float target = 0.0f;
    if (CCullZones::CurrentFlags_Camera & 0x2000) { // TODO: Unnamed tunnel-related eZoneAttributes flag (bit 0x2000)
        // exe (oracle-proven): forward = matrix->forward (z cleared) or, without a matrix, (-sin(heading), cos(heading), 0) with the x87 instructions; normalised in 3D (CVector::Normalise)
        const auto  camPos = TheCamera.GetPosition();
        const CVector from{ camPos.x, camPos.y, 0.0f };
        CVector dir;
        if (const auto* mat = TheCamera.m_matrix) {
            dir = mat->GetForward();
        } else {
            const float heading = TheCamera.m_placement.m_fHeading;
            dir.x = (float)-x87::sin(heading);
            dir.y = (float)x87::cos(heading);
            dir.z = 0.0f;
        }
        dir.z = 0.0f; // 0x72B6C2: z is cleared after both branches, then the 3D Normalise (so a zero vector stays handled by CVector::Normalise)
        dir.Normalise();
        const CVector to{ dir.x * 100.0f + from.x, dir.y * 100.0f + from.y, dir.z * 100.0f + from.z };

        // 0x72B77B..: `min(100, d1)` then `min(., d2)` as compare-and-select (NaN takes the new distance), then `v = m * 0.01f`, target = (1 < v) ? 1 : v
        float m = 100.0f;
        const float d1 = CCollision::DistToLine(from, to, s_TunnelPoint1);
        m = (100.0f < d1) ? 100.0f : d1;
        const float d2 = CCollision::DistToLine(from, to, s_TunnelPoint2);
        m = (m < d2) ? m : d2;
        const float v = m * 0.01f;
        target = (1.0f < v) ? 1.0f : v;
    }

    // 0x72B7E7: same idiom as the rain / sandstorm one, but the sign test is `d < 0 ? sub : add` (a NaN or zero distance adds)
    const float step = CTimer::GetTimeStep() * 0.01f;
    const float d    = target - InTunnelness;
    InTunnelness     = std::fabs(d) < step ? target : (d < 0.0f ? InTunnelness - step : InTunnelness + step);
}

// Based on 0x72A640
eWeatherRegion CWeather::FindWeatherRegion(CVector2D pos) {
    if (pos.x > 1000.0f && pos.y > 910.0f) {
        return WEATHER_REGION_LV;
    }
    if (pos.x > -850.0f && pos.x < 1000.0f && pos.y > 1280.0f) {
        return WEATHER_REGION_DESERT;
    }
    if (pos.x < -1430.0f && pos.y > -580.0f && pos.y < 1430.0f) {
        return WEATHER_REGION_SF;
    }
    if (pos.x > 250.0f && pos.x < 3000.0f && pos.y > -3000.0f && pos.y < -850.0f) {
        return WEATHER_REGION_LA;
    }
    return WEATHER_REGION_DEFAULT;
}

// 0x72A640
void CWeather::UpdateWeatherRegion(CVector* posn) {
    WeatherRegion = FindWeatherRegion(posn ? *posn : TheCamera.GetPosition());
}

// 0x4ABF50
bool CWeather::IsRainy() {
    return Rain >= 0.2f;
}

// Defined here: detached build only (Weather.h declares it, the address-mode text is the old inline reference)
NOTSA_GLOBAL_DEF(CWeather, m_WeatherAudioEntity, 0xC81360, (CAEWeatherAudioEntity), {}); VALIDATE_SIZE(CWeather::m_WeatherAudioEntity, 128);
