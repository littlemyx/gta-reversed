/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "Vector.h"
#include "AEWeatherAudioEntity.h"
#include "eWeatherType.h"

enum eWeatherRegion : int16 {
    WEATHER_REGION_DEFAULT = 0,
    WEATHER_REGION_LA = 1,
    WEATHER_REGION_SF = 2,
    WEATHER_REGION_LV = 3,
    WEATHER_REGION_DESERT = 4
};

class CWeather {
public:
    static inline NOTSA_GLOBAL(TrafficLightsBrightness, 0xC812A8, (float), {});
    static inline NOTSA_GLOBAL(bScriptsForceRain, 0xC812AC, (bool), {});
    static inline NOTSA_GLOBAL(Earthquake, 0xC81340, (float), {});
    static inline NOTSA_GLOBAL(CurrentRainParticleStrength, 0xC812B0, (uint32), {});
    static inline NOTSA_GLOBAL(LightningStartY, 0xC812B4, (uint32), {}); // only initialized (0), not used
    static inline NOTSA_GLOBAL(LightningStartX, 0xC812B8, (uint32), {}); // only initialized (0), not used
    static inline NOTSA_GLOBAL(LightningFlashLastChange, 0xC812BC, (int32), {});
    static inline NOTSA_GLOBAL(WhenToPlayLightningSound, 0xC812C0, (uint32), {});
    static inline NOTSA_GLOBAL(LightningDuration, 0xC812C4, (uint32), {}); // Duration as number of frames
    static inline NOTSA_GLOBAL(LightningStart, 0xC812C8, (uint32), {}); // frame number
    static inline NOTSA_GLOBAL(LightningFlash, 0xC812CC, (bool), {});
    static inline NOTSA_GLOBAL(LightningBurst, 0xC812CD, (bool), {});
    static inline NOTSA_GLOBAL(HeadLightsSpectrum, 0xC812D0, (float), {});
    static inline NOTSA_GLOBAL(WaterFogFXControl, 0xC81338, (float), {});
    static inline NOTSA_GLOBAL(HeatHazeFXControl, 0xC812D8, (float), {});
    static inline NOTSA_GLOBAL(HeatHaze, 0xC812DC, (float), {});
    static inline NOTSA_GLOBAL(SunGlare, 0xC812E0, (float), {});
    static inline NOTSA_GLOBAL(Rainbow, 0xC812E4, (float), {});
    static inline NOTSA_GLOBAL(Wavyness, 0xC812E8, (float), {});
    static inline NOTSA_GLOBAL(WindClipped, 0xC812EC, (float), {});
    static inline NOTSA_GLOBAL(WindDir, 0xC813E0, (CVector), {});
    static inline NOTSA_GLOBAL(Wind, 0xC812F0, (float), {});
    static inline NOTSA_GLOBAL(Sandstorm, 0xC812F4, (float), {});
    static inline NOTSA_GLOBAL(Rain, 0xC81324, (float), {});
    static inline NOTSA_GLOBAL(InTunnelness, 0xC81334, (float), {});
    static inline NOTSA_GLOBAL(WaterDepth, 0xC81330, (float), {});
    static inline NOTSA_GLOBAL(UnderWaterness, 0xC8132C, (float), {});
    static inline NOTSA_GLOBAL(ExtraSunnyness, 0xC812F8, (float), {});
    static inline NOTSA_GLOBAL(Foggyness_SF, 0xC812FC, (float), {});
    static inline NOTSA_GLOBAL(Foggyness, 0xC81300, (float), {});
    static inline NOTSA_GLOBAL(CloudCoverage, 0xC81304, (float), {});
    static inline NOTSA_GLOBAL(WetRoads, 0xC81308, (float), {});
    static inline NOTSA_GLOBAL(InterpolationValue, 0xC8130C, (float), {});
    static inline NOTSA_GLOBAL(WeatherTypeInList, 0xC81310, (uint32), {});
    static inline NOTSA_GLOBAL(WeatherRegion, 0xC81314, (eWeatherRegion), {});
    static inline NOTSA_GLOBAL(ForcedWeatherType, 0xC81318, (eWeatherType), {});
    static inline NOTSA_GLOBAL(NewWeatherType, 0xC8131C, (eWeatherType), {});
    static inline NOTSA_GLOBAL(OldWeatherType, 0xC81320, (eWeatherType), {});
    static inline auto& m_WeatherAudioEntity = StaticRef<CAEWeatherAudioEntity>(0xC81360);
    static inline NOTSA_GLOBAL(StreamAfterRainTimer, 0x8D5EAC, (int32), { 800 });
    static inline NOTSA_GLOBAL(HeatHazeFXFade, 0xC81448, (float), {});
    static inline NOTSA_GLOBAL(HeatHazeFXLastMinute, 0x8D6078, (int32), { -1 });
    static inline NOTSA_GLOBAL(WaterFogFXFade, 0xC81444, (float), {});
    static inline NOTSA_GLOBAL(WaterFogFXFadingOut, 0xC81440, (bool), {});

    // in entity.cpp:

    static std::array<float, 16> saTreeWindOffsets; // orig WindTabel
    static std::array<float, 32> saBannerWindOffsets; // orig BannerWindTabel

public:
    static void InjectHooks();

    static void Init();
    static void AddRain();
    static void AddSandStormParticles();
    static const eWeatherType* FindWeatherTypesList();
    static void ForceWeather(eWeatherType weatherType);
    static void ForceWeatherNow(eWeatherType weatherType);
    static bool ForecastWeather(eWeatherType weatherType, int32 numSteps);
    static void ReleaseWeather();
    static void RenderRainStreaks();
    static void SetWeatherToAppropriateTypeNow();
    static void Update();
    static void UpdateInTunnelness();
    /*!
    * @notsa
    * @details Based on code @ `0x72A640`
    * @return The corresponding weather region at a given 2D position, or `WEATHER_REGION_DEFAULT` if no specific region was found
    */
    static eWeatherRegion FindWeatherRegion(CVector2D pos);
    static void UpdateWeatherRegion(CVector* posn);
    static bool IsRainy();

    static bool IsUnderWater() { return UnderWaterness > 0.0f; } // NOTSA
};
