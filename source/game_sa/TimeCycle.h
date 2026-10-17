#pragma once

#include "ColourSet.h"
#include "eWeatherType.h"

class CTimeCycleBox;
class CBox;

class CTimeCycleBox {
public:
    CBox  Box;
    int16 FarClip;
    uint8 LodDistMult;
    int32 ExtraColor;
    float Strength;
    float Falloff;
};
VALIDATE_SIZE(CTimeCycleBox, 0x28);

NOTSA_GLOBAL_HDR(gfLaRiotsLightMult, 0x8CD060, (float), { 1.0f }); // 1.0f

enum eTimeType {
    TIME_MIDNIGHT,
    TIME_5AM,
    TIME_6AM,
    TIME_7AM,
    TIME_MIDDAY, // TIME_NOON
    TIME_7PM,
    TIME_8PM,
    TIME_10PM,

    NUM_HOURS
};

class CTimeCycle {
public:
    template<typename T>
    using Colors = notsa::mdarray<T, NUM_HOURS, NUM_WEATHERS>;

    static inline NOTSA_GLOBAL(m_nAmbientRed, 0xB7C3C8, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nAmbientGreen, 0xB7C310, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nAmbientBlue, 0xB7C258, (Colors<uint8>), {});

    static inline NOTSA_GLOBAL(m_nAmbientRed_Obj, 0xB7C1A0, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nAmbientGreen_Obj, 0xB7C0E8, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nAmbientBlue_Obj, 0xB7C030, (Colors<uint8>), {});

    static inline NOTSA_GLOBAL(m_nSkyTopRed, 0xB7BF78, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nSkyTopGreen, 0xB7BEC0, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nSkyTopBlue, 0xB7BE08, (Colors<uint8>), {});

    static inline NOTSA_GLOBAL(m_nSkyBottomRed, 0xB7BD50, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nSkyBottomGreen, 0xB7BC98, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nSkyBottomBlue, 0xB7BBE0, (Colors<uint8>), {});

    static inline NOTSA_GLOBAL(m_nSunCoreRed, 0xB7BB28, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nSunCoreGreen, 0xB7BA70, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nSunCoreBlue, 0xB7B9B8, (Colors<uint8>), {});

    static inline NOTSA_GLOBAL(m_nSunCoronaRed, 0xB7B900, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nSunCoronaGreen, 0xB7B848, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nSunCoronaBlue, 0xB7B790, (Colors<uint8>), {});

    static inline NOTSA_GLOBAL(m_fSunSize, 0xB7B6D8, (Colors<int8>), {});

    static inline NOTSA_GLOBAL(m_fSpriteSize, 0xB7B620, (Colors<int8>), {});
    static inline NOTSA_GLOBAL(m_fSpriteBrightness, 0xB7B568, (Colors<int8>), {});

    static inline NOTSA_GLOBAL(m_nShadowStrength, 0xB7B4B0, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nLightShadowStrength, 0xB7B3F8, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nPoleShadowStrength, 0xB7B340, (Colors<uint8>), {});

    static inline NOTSA_GLOBAL(m_fFarClip, 0xB7B1D0, (Colors<int16>), {});
    static inline NOTSA_GLOBAL(m_fFogStart, 0xB7B060, (Colors<int16>), {});
    static inline NOTSA_GLOBAL(m_fLightsOnGroundBrightness, 0xB7AFA8, (Colors<uint8>), {});

    static inline NOTSA_GLOBAL(m_nLowCloudsRed, 0xB7AEF0, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nLowCloudsGreen, 0xB7AE38, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nLowCloudsBlue, 0xB7AD80, (Colors<uint8>), {});

    static inline NOTSA_GLOBAL(m_nFluffyCloudsBottomRed, 0xB7ACC8, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nFluffyCloudsBottomGreen, 0xB7AC10, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nFluffyCloudsBottomBlue, 0xB7AB58, (Colors<uint8>), {});

    static inline NOTSA_GLOBAL(m_fWaterRed, 0xB7AAA0, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_fWaterGreen, 0xB7A9E8, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_fWaterBlue, 0xB7A930, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_fWaterAlpha, 0xB7A878, (Colors<uint8>), {});

    static inline NOTSA_GLOBAL(m_fPostFx1Red, 0xB7A7C0, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_fPostFx1Green, 0xB7A708, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_fPostFx1Blue, 0xB7A650, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_fPostFx1Alpha, 0xB7A598, (Colors<uint8>), {});

    static inline NOTSA_GLOBAL(m_fPostFx2Red, 0xB7A4E0, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_fPostFx2Green, 0xB7A428, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_fPostFx2Blue, 0xB7A370, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_fPostFx2Alpha, 0xB7A2B8, (Colors<uint8>), {});

    static inline NOTSA_GLOBAL(m_fCloudAlpha, 0xB7A200, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nHighLightMinIntensity, 0xB7A148, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nWaterFogAlpha, 0xB7A090, (Colors<uint8>), {});
    static inline NOTSA_GLOBAL(m_nDirectionalMult, 0xB79FD8, (Colors<uint8>), {});

    static inline NOTSA_GLOBAL(m_CurrentColours, 0xB7C4A0, (CColourSet), {});

    static inline NOTSA_GLOBAL(m_aBoxes, 0xB7C550, (std::array<CTimeCycleBox, 20>), {});
    static inline NOTSA_GLOBAL(m_NumBoxes, 0xB7C480, (uint32), {});

    static inline NOTSA_GLOBAL(m_CurrentStoredValue, 0xB79FD0, (uint32), {});
    static inline NOTSA_GLOBAL(m_VectorToSun, 0xB7CA50, (std::array<CVector, 16>), {});

    // TODO: CVector2D?
    static inline NOTSA_GLOBAL(m_fShadowFrontX, 0xB79F90, (std::array<float, 16>), {});
    static inline NOTSA_GLOBAL(m_fShadowFrontY, 0xB79F50, (std::array<float, 16>), {});
    static inline NOTSA_GLOBAL(m_fShadowSideX, 0xB79F10, (std::array<float, 16>), {});
    static inline NOTSA_GLOBAL(m_fShadowSideY, 0xB79ED0, (std::array<float, 16>), {});
    static inline NOTSA_GLOBAL(m_fShadowDisplacementX, 0xB79E90, (std::array<float, 16>), {});
    static inline NOTSA_GLOBAL(m_fShadowDisplacementY, 0xB79E50, (std::array<float, 16>), {});

    static inline NOTSA_GLOBAL(m_FogReduction, 0xB79E48, (int32), {});

    static inline NOTSA_GLOBAL(m_ExtraColour, 0xB79E44, (int32), {});
    static inline NOTSA_GLOBAL(m_ExtraColourWeatherType, 0xB79E40, (int32), {});
    static inline NOTSA_GLOBAL(m_bExtraColourOn, 0xB7C484, (uint32), {});
    static inline NOTSA_GLOBAL(m_ExtraColourInter, 0xB79E3C, (float), {});

    static inline auto& m_BelowHorizonGrey = StaticRef<RwRGBA>(0xB7CB10);

    static inline NOTSA_GLOBAL(m_BrightnessAddedToAmbientRed, 0xB79E38, (float), {});
    static inline NOTSA_GLOBAL(m_BrightnessAddedToAmbientGreen, 0xB79E34, (float), {});
    static inline NOTSA_GLOBAL(m_BrightnessAddedToAmbientBlue, 0xB79E30, (float), {});

    static inline NOTSA_GLOBAL(m_vecDirnLightToSun, 0xB7CB14, (CVector), {});

public:
    static void InjectHooks();

    static void Initialise(bool padFile);
    static void Update();
    static void CalcColoursForPoint(CVector point, CColourSet* set);
    static float FindFarClipForCoors(CVector cameraPos);
    static void Shutdown();

    static float GetAmbientRed();
    static float GetAmbientGreen();
    static float GetAmbientBlue();

    static float GetAmbientRed_Obj();
    static float GetAmbientGreen_Obj();
    static float GetAmbientBlue_Obj();

    static float GetAmbientRed_BeforeBrightness();
    static float GetAmbientGreen_BeforeBrightness();
    static float GetAmbientBlue_BeforeBrightness();

    static uint16 GetSkyTopRed()   { return m_CurrentColours.m_nSkyTopRed; };
    static uint16 GetSkyTopGreen() { return m_CurrentColours.m_nSkyTopGreen; };
    static uint16 GetSkyTopBlue()  { return m_CurrentColours.m_nSkyTopBlue; };

    static uint16 GetSkyBottomRed()   { return m_CurrentColours.m_nSkyBottomRed; };
    static uint16 GetSkyBottomGreen() { return m_CurrentColours.m_nSkyBottomGreen; };
    static uint16 GetSkyBottomBlue()  { return m_CurrentColours.m_nSkyBottomBlue; }

    static uint16 GetFogRed()   { return GetSkyBottomRed(); }
    static uint16 GetFogGreen() { return GetSkyBottomGreen(); }
    static uint16 GetFogBlue()  { return GetSkyBottomBlue(); }

    static uint16 GetSunCoreRed()   { return m_CurrentColours.m_nSunCoreRed; };
    static uint16 GetSunCoreGreen() { return m_CurrentColours.m_nSunCoreGreen; };
    static uint16 GetSunCoreBlue()  { return m_CurrentColours.m_nSunCoreBlue; }

    static uint16 GetSunCoronaRed()   { return m_CurrentColours.m_nSunCoronaRed; }
    static uint16 GetSunCoronaGreen() { return m_CurrentColours.m_nSunCoronaGreen; }
    static uint16 GetSunCoronaBlue()  { return m_CurrentColours.m_nSunCoronaBlue; }

    static float GetSunSize() { return m_CurrentColours.m_fSunSize; }
    static float GetSpriteSize() { return m_CurrentColours.m_fSpriteSize; }
    static float GetSpriteBrightness() { return m_CurrentColours.m_fSpriteBrightness; }

    static int16 GetShadowStrength() { return m_CurrentColours.m_nShadowStrength; }
    static int16 GetLightShadowStrength() { return m_CurrentColours.m_nLightShadowStrength; }
    static int16 GetPoleShadowStrength() { return m_CurrentColours.m_nPoleShadowStrength; }

    static float GetFarClip() { return m_CurrentColours.m_fFarClip; }
    static float GetFogStart() { return m_CurrentColours.m_fFogStart; }
    static float GetLightsOnGroundBrightness() { return m_CurrentColours.m_fLightsOnGroundBrightness; }

    static uint16 GetLowCloudsRed()   { return m_CurrentColours.m_nLowCloudsRed; }
    static uint16 GetLowCloudsGreen() { return m_CurrentColours.m_nLowCloudsGreen; }
    static uint16 GetLowCloudsBlue()  { return m_CurrentColours.m_nLowCloudsBlue; }

    static uint16 GetFluffyCloudsBottomRed()   { return m_CurrentColours.m_nFluffyCloudsBottomRed; }
    static uint16 GetFluffyCloudsBottomGreen() { return m_CurrentColours.m_nFluffyCloudsBottomGreen; }
    static uint16 GetFluffyCloudsBottomBlue()  { return m_CurrentColours.m_nFluffyCloudsBottomBlue; }

    static float GetWaterRed()   { return m_CurrentColours.m_fWaterRed; }
    static float GetWaterGreen() { return m_CurrentColours.m_fWaterGreen; }
    static float GetWaterBlue()  { return m_CurrentColours.m_fWaterBlue; }
    static float GetWaterAlpha() { return m_CurrentColours.m_fWaterAlpha; }

    static float GetPostFx1Red()   { return m_CurrentColours.m_fPostFx1Red; }
    static float GetPostFx1Green() { return m_CurrentColours.m_fPostFx1Green; }
    static float GetPostFx1Blue()  { return m_CurrentColours.m_fPostFx1Blue; }
    static float GetPostFx1Alpha() { return m_CurrentColours.m_fPostFx1Alpha; }

    static float GetPostFx2Red()   { return m_CurrentColours.m_fPostFx2Red; }
    static float GetPostFx2Green() { return m_CurrentColours.m_fPostFx2Green; }
    static float GetPostFx2Blue()  { return m_CurrentColours.m_fPostFx2Blue; }
    static float GetPostFx2Alpha() { return m_CurrentColours.m_fPostFx2Alpha; }

    static float GetCloudAlpha() { return m_CurrentColours.m_fCloudAlpha; }
    static int32 GetHighLightMinIntensity() { return m_CurrentColours.m_nHighLightMinIntensity; }

    static float GetLODMultiplier() { return m_CurrentColours.m_fLodDistMult; }

    static void SetFogRed(uint16 value)   { SetSkyBottomRed(value); }
    static void SetFogGreen(uint16 value) { SetSkyBottomGreen(value); }
    static void SetFogBlue(uint16 value)  { SetSkyBottomBlue(value); }

    static void StartExtraColour(int32 colour, bool bNoExtraColorInterior);
    static void StopExtraColour(bool bNoExtraColorInterior);

    static void AddOne(CBox& box, int16 farClip, int32 extraColor, float strength, float falloff, float lodDistMult);
    static void FindTimeCycleBox(CVector pos, CTimeCycleBox** out, float* currentBox, bool isLOD, bool isFarClip, CTimeCycleBox* alreadyFoundBox);
    static void InitForRestart();

    static void SetConstantParametersForPostFX();

public: // NOTSA
    static void SetSkyBottomRed(uint16 value)   { m_CurrentColours.m_nSkyBottomRed = value; }
    static void SetSkyBottomGreen(uint16 value) { m_CurrentColours.m_nSkyBottomGreen = value; }
    static void SetSkyBottomBlue(uint16 value)  { m_CurrentColours.m_nSkyBottomBlue = value; }

    static auto GetVectorToSun() { return m_VectorToSun[m_CurrentStoredValue]; }
    static auto GetShadowSide() { return CVector2D{ m_fShadowSideX[m_CurrentStoredValue], m_fShadowSideY[m_CurrentStoredValue] }; }

    static CRGBA GetCurrentSkyBottomColor()         { return m_CurrentColours.GetSkyBottom(); }

    static auto GetPostFxColors() { return std::make_pair(m_CurrentColours.GetPostFx1(), m_CurrentColours.GetPostFx2()); }

    static float SumOfCurrentRGB1() { return m_CurrentColours.m_fPostFx1Blue + m_CurrentColours.m_fPostFx1Green + m_CurrentColours.m_fPostFx1Red; }
    static float SumOfCurrentRGB2() { return m_CurrentColours.m_fPostFx2Blue + m_CurrentColours.m_fPostFx2Green + m_CurrentColours.m_fPostFx2Red; }

    static auto GetBoxes() { return m_aBoxes | rngv::take(m_NumBoxes); }
    static bool ShouldIgnoreSky() {
        return (
            m_nSkyTopRed[m_ExtraColour][m_ExtraColourWeatherType]   == 0 &&
            m_nSkyTopGreen[m_ExtraColour][m_ExtraColourWeatherType] == 0 &&
            m_nSkyTopBlue[m_ExtraColour][m_ExtraColourWeatherType]  == 0
        );
    }
};
// VALIDATE_SIZE(CTimeCycle, 0x919); Maybe?
