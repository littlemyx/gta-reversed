#pragma once

#include "rwcore.h"

class CRGBA;

enum eHeatHazeFXType {
    HEAT_HAZE_UNDEFINED = -1,
    HEAT_HAZE_0,
    HEAT_HAZE_1,
    HEAT_HAZE_2,
    HEAT_HAZE_3,
    HEAT_HAZE_4,

    MAX_HEAT_HAZE_TYPES
};

class CPostEffects {
public:
    static void InjectHooks();

    static void Initialise();
    static void Close();
    static void DoScreenModeDependentInitializations();
    static void SetupBackBufferVertex();
    static void Update();

    // X2,Y2 is added to X1,Y1. So they are more like width and height in a rectangle.
    static void DrawQuad(float x1, float y1, float x2, float y2, uint8 red, uint8 green, uint8 blue, uint8 alpha, RwRaster* raster);
    static void DrawQuadSetDefaultUVs();
    static void DrawQuadSetPixelUVs(float u0, float v0, float u1, float v1, float u3, float v3, float u2, float v2);
    static void DrawQuadSetUVs(float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4);

    static void FilterFX_RestoreDayNightBalance();
    static void FilterFX_StoreAndSetDayNightBalance();

    static void ImmediateModeFilterStuffInitialize();
    static void ImmediateModeRenderStatesSet();
    static void ImmediateModeRenderStatesStore();
    static void ImmediateModeRenderStatesReStore();

    static RwRaster* RasterCreatePostEffects(RwRect rect);

    static void ScriptCCTVSwitch(bool enable);
    static void ScriptDarknessFilterSwitch(bool enable, int32 alpha);
    static void ScriptHeatHazeFXSwitch(bool enable);
    static void ScriptInfraredVisionSwitch(bool enable);
    static void ScriptNightVisionSwitch(bool enable);
    static void ScriptResetForEffects();

    static void UnderWaterRipple(CRGBA color, float xoffset, float yoffset, float strength, float speed, float freq);
    static void UnderWaterRippleFadeToFX();

    static void HeatHazeFXInit();
    static void HeatHazeFX(float fIntensity, bool bAlphaMaskMode);

    static bool IsVisionFXActive();

    static void NightVision();
    static void NightVisionSetLights();

    static void SetFilterMainColour(RwRaster* raster, RwRGBA color);
    static void InfraredVision(RwRGBA color, RwRGBA colorMain);
    static void InfraredVisionSetLightsForDefaultObjects();
    static void InfraredVisionSetLightsForHeatObjects();
    static void InfraredVisionStoreAndSetLightsForHeatObjects(CPed* ped);
    static void InfraredVisionRestoreLightsForHeatObjects();

    static void Fog();
    static void CCTV();
    static void Grain(int32 strengthMask, bool update);
    static void SpeedFX(float speed);
    static void DarknessFilter(int32 alpha);
    static void ColourFilter(RwRGBA pass1, RwRGBA pass2);
    static void Radiosity(int32 intensityLimit, int32 filterPasses, int32 renderPasses, int32 intensity);
    static void SetSpeedFXManualSpeedCurrentFrame(float value);

    static void Render();

public:
    static inline NOTSA_GLOBAL(SCREEN_EXTRA_MULT_CHANGE_RATE, 0x8D5168, (float), { 0.0005f }); // 0.0005f;
    static inline NOTSA_GLOBAL(SCREEN_EXTRA_MULT_BASE_CAP, 0x8D516C, (float), { 0.35f }); // 0.35f;
    static inline NOTSA_GLOBAL(SCREEN_EXTRA_MULT_BASE_MULT, 0x8D5170, (float), { 1.0f }); // 1.0f;

    static inline NOTSA_GLOBAL(m_bDisableAllPostEffect, 0xC402CF, (bool), {});
    static inline NOTSA_GLOBAL(m_bSavePhotoFromScript, 0xC402D0, (bool), {});
    static inline NOTSA_GLOBAL(m_bInCutscene, 0xC402B7, (bool), {});

    static inline NOTSA_GLOBAL(m_xoffset, 0x8D5130, (float), { 4.0f }); // 4.0f
    static inline NOTSA_GLOBAL(m_yoffset, 0x8D5134, (float), { 24.0f }); // 24.0f

    static inline NOTSA_GLOBAL(m_colour1Multiplier, 0x8D5160, (float), { 1.0f }); // 1.0f;
    static inline NOTSA_GLOBAL(m_colour2Multiplier, 0x8D5164, (float), { 1.0f }); // 1.0f;
    static inline NOTSA_GLOBAL(m_colourLeftUOffset, 0x8D5150, (float), { std::bit_cast<float>(0x00000008u) }); // 8
    static inline NOTSA_GLOBAL(m_colourRightUOffset, 0x8D5154, (float), { std::bit_cast<float>(0x00000008u) }); // 8
    static inline NOTSA_GLOBAL(m_colourTopVOffset, 0x8D5158, (float), { std::bit_cast<float>(0x00000008u) }); // 8
    static inline NOTSA_GLOBAL(m_colourBottomVOffset, 0x8D515C, (float), { std::bit_cast<float>(0x00000008u) }); // 8

    static inline NOTSA_GLOBAL(m_bNightVision, 0xC402B8, (bool), {});
    static inline NOTSA_GLOBAL(m_fNightVisionSwitchOnFXCount, 0xC40300, (float), { 50.0f }); // = CPostEffects::m_fNightVisionSwitchOnFXTime (initterm 0x854810 stores 50.0f)
    static inline NOTSA_GLOBAL(m_fNightVisionSwitchOnFXTime, 0x8D50B0, (float), { 50.0f }); // 50.0f
    static inline NOTSA_GLOBAL(m_NightVisionGrainStrength, 0x8D50A8, (int32), { 48 }); // 48
    static inline NOTSA_GLOBAL(m_NightVisionMainCol, 0x8D50AC, (CRGBA), { std::bit_cast<CRGBA>(0xFF008200U) }); // { 255, 0, 130, 0};

    static inline NOTSA_GLOBAL(m_bDarknessFilter, 0xC402C4, (bool), {});
    static inline NOTSA_GLOBAL(m_DarknessFilterAlpha, 0x8D5204, (int32), { 170 }); // 170
    static inline NOTSA_GLOBAL(m_DarknessFilterAlphaDefault, 0x8D50F4, (int32), { 170 }); // 170
    static inline NOTSA_GLOBAL(m_DarknessFilterRadiosityIntensityLimit, 0x8D50F8, (int32), { 45 }); // 45

    static inline NOTSA_GLOBAL(m_fWaterFXStartUnderWaterness, 0x8D514C, (float), { 0.535f }); // 0.535f;
    static inline NOTSA_GLOBAL(m_fWaterFullDarknessDepth, 0x8D5148, (float), { 90.0f }); // 90.0f
    static inline NOTSA_GLOBAL(m_bWaterDepthDarkness, 0x8D5144, (bool), { true });  // true;

    static inline NOTSA_GLOBAL(m_bHeatHazeFX, 0xC402BA, (bool), {});
    static inline NOTSA_GLOBAL(m_HeatHazeFXSpeedMin, 0x8D50EC, (int32), { 6 }); // 6
    static inline NOTSA_GLOBAL(m_HeatHazeFXSpeedMax, 0x8D50F0, (int32), { 10 }); // 10
    static inline NOTSA_GLOBAL(m_HeatHazeFXIntensity, 0x8D50E8, (int32), { 150 }); // 150
    static inline NOTSA_GLOBAL(m_HeatHazeFXType, 0xC402BC, (int32), {}); // 0
    static inline NOTSA_GLOBAL(m_HeatHazeFXTypeLast, 0x8D50E4, (int32), { -1 }); // -1
    static inline NOTSA_GLOBAL(m_HeatHazeFXRandomShift, 0xC402C0, (int32), {});
    static inline NOTSA_GLOBAL(m_HeatHazeFXScanSizeX, 0xC40304, (int32), {}); // int32(SCREEN_WIDTH_UNIT  * 24.0f);
    static inline NOTSA_GLOBAL(m_HeatHazeFXScanSizeY, 0xC40308, (int32), {}); // int32(SCREEN_HEIGHT_UNIT * 24.0f);
    static inline NOTSA_GLOBAL(m_HeatHazeFXRenderSizeX, 0xC4030C, (int32), {}); // int32(SCREEN_WIDTH_UNIT  * 24.0f);
    static inline NOTSA_GLOBAL(m_HeatHazeFXRenderSizeY, 0xC40310, (int32), {}); // int32(SCREEN_HEIGHT_UNIT * 24.0f);

    static inline NOTSA_GLOBAL(m_bFog, 0xC402C6, (bool), {});

    static inline NOTSA_GLOBAL(m_bSpeedFX, 0x8D5100, (bool), { true }); // true;
    static inline NOTSA_GLOBAL(m_bSpeedFXTestMode, 0xC402C7, (bool), {});
    static inline NOTSA_GLOBAL(m_bSpeedFXUserFlag, 0x8D5108, (bool), { true }); // always true;
    static inline NOTSA_GLOBAL(m_bSpeedFXUserFlagCurrentFrame, 0x8D5109, (bool), { true }); // true
    static inline NOTSA_GLOBAL(m_fSpeedFXManualSpeedCurrentFrame, 0xC402C8, (float), {});
    static inline NOTSA_GLOBAL(m_SpeedFXAlpha, 0x8D5104, (int32), { 36 }); // 36

    static inline NOTSA_GLOBAL(pRasterFrontBuffer, 0xC402D8, (RwRaster*), {});

    static inline NOTSA_GLOBAL(m_bGrainEnable, 0xC402B4, (bool), {});
    static inline NOTSA_GLOBAL(m_pGrainRaster, 0xC402B0, (RwRaster*), {});
    static inline NOTSA_GLOBAL(m_grainStrength, 0x8D5094, (std::array<char, 4>), { 64 }); // the exe passes the whole dword to Grain(int32); only [0] is read here

    static inline NOTSA_GLOBAL(m_bCCTV, 0xC402C5, (bool), {});
    static inline NOTSA_GLOBAL(m_CCTVcol, 0x8D50FC, (CRGBA), { std::bit_cast<CRGBA>(0x40000000U) }); // { 64, 0, 0, 0 }

    static inline NOTSA_GLOBAL(m_bRainEnable, 0xC402D1, (bool), {});
    static inline NOTSA_GLOBAL(m_bColorEnable, 0x8D518C, (bool), { true }); // true;

    static inline NOTSA_GLOBAL(m_bRadiosity, 0xC402CC, (bool), {});
    static inline NOTSA_GLOBAL(m_bRadiosityDebug, 0xC402CD, (bool), {});
    static inline NOTSA_GLOBAL(m_bRadiosityLinearFilter, 0x8D510A, (bool), { true }); // true;
    static inline NOTSA_GLOBAL(m_bRadiosityStripCopyMode, 0x8D510B, (bool), { true }); // true;
    static inline NOTSA_GLOBAL(m_RadiosityFilterUCorrection, 0x8D511C, (int32), { 2 }); // 2;
    static inline NOTSA_GLOBAL(m_RadiosityFilterVCorrection, 0x8D5120, (int32), { 2 }); // 2;
    static inline NOTSA_GLOBAL(m_RadiosityIntensity, 0x8D5118, (int32), { 35 }); // 35
    static inline NOTSA_GLOBAL(m_RadiosityIntensityLimit, 0x8D5114, (int32), { 220 }); // 220
    static inline NOTSA_GLOBAL(m_bRadiosityBypassTimeCycleIntensityLimit, 0xC402CE, (bool), {});
    static inline NOTSA_GLOBAL(m_RadiosityPixelsX, 0xC40314, (int32), {});  // SCREEN_WIDTH
    static inline NOTSA_GLOBAL(m_RadiosityPixelsY, 0xC40318, (int32), {});  // SCREEN_HEIGHT
    static inline NOTSA_GLOBAL(m_RadiosityFilterPasses, 0x8D5110, (uint32), { 1 }); // 1
    static inline NOTSA_GLOBAL(m_RadiosityRenderPasses, 0x8D510C, (uint32), { 2 }); // 2

    static inline NOTSA_GLOBAL(m_VisionFXDayNightBalance, 0x8D50A4, (float), { 1.0f }); // 1.0f

    static inline NOTSA_GLOBAL(m_bInfraredVision, 0xC402B9, (bool), {});
    static inline NOTSA_GLOBAL(m_InfraredVisionGrainStrength, 0x8D50B4, (int32), { 64 }); // 64
    static inline NOTSA_GLOBAL(m_fInfraredVisionFilterRadius, 0x8D50B8, (float), { 0.003f }); // 0.003f
    // float& m_fInfraredVisionSwitchOnFXCount;
    static inline NOTSA_GLOBAL(m_InfraredVisionCol, 0x8D50CC, (CRGBA), { std::bit_cast<CRGBA>(0xFF3C286EU) });      // { FF, 3C, 28, 6E }
    static inline NOTSA_GLOBAL(m_InfraredVisionMainCol, 0x8D50D0, (CRGBA), { std::bit_cast<CRGBA>(0xFFC80064U) });      // { FF, C8, 00, 64 }
    static inline NOTSA_GLOBAL(m_fInfraredVisionHeatObjectCol, 0x8D50BC, (RwRGBAReal), { 1.0f, 0.0f, 0.0f, 1.0f }); // { 1.0f, 0.0f, 0.0f, 1.0f }
    static inline NOTSA_GLOBAL(m_HeatHazeFXHourOfDayStart, 0x8D50D4, (int32), { 10 }); // 10
    static inline NOTSA_GLOBAL(m_HeatHazeFXHourOfDayEnd, 0x8D50D8, (int32), { 19 }); // 19
    static inline NOTSA_GLOBAL(m_fHeatHazeFXFadeSpeed, 0x8D50DC, (float), { 0.05f }); // 0.05f
    static inline NOTSA_GLOBAL(m_fHeatHazeFXInsideBuildingFadeSpeed, 0x8D50E0, (float), { 0.5f }); // 0.5f

    static inline NOTSA_GLOBAL(m_waterEnable, 0xC402D3, (bool), {});
    static inline NOTSA_GLOBAL(m_waterStrength, 0x8D512C, (float), { std::bit_cast<float>(0x00000040u) }); // 64
    static inline NOTSA_GLOBAL(m_waterSpeed, 0x8D5138, (float), { 0.0015f }); // 0.0015f
    static inline NOTSA_GLOBAL(m_waterFreq, 0x8D513C, (float), { 0.04f }); // 0.04f
    static inline NOTSA_GLOBAL(m_waterCol, 0x8D5140, (CRGBA), { std::bit_cast<CRGBA>(0x40404040U) }); // { 64, 64, 64, 64 }

    // Immediate Mode Filter
    struct imf {
        float                       screenZ;
        float                       recipCameraZ;
        RwRaster*                   RasterDrawBuffer;
        int32                       sizeDrawBufferX;
        int32                       sizeDrawBufferY;
        float                       fFrontBufferU1;
        float                       fFrontBufferV1;
        float                       fFrontBufferU2;
        float                       fFrontBufferV2;
        std::array<RwIm2DVertex, 3> triangle;
        float                       uMinTri;
        float                       uMaxTri;
        float                       vMinTri;
        float                       vMaxTri;
        std::array<RwIm2DVertex, 6> quad;
        RwBlendFunction             blendSrc;
        RwBlendFunction             blendDst;
        RwBool                      bFog;
        RwCullMode                  cullMode;
        RwBool                      bZTest;
        RwBool                      bZWrite;
        RwShadeMode                 shadeMode;
        RwBool                      bVertexAlpha;
        RwTextureAddressMode        textureAddress;
        RwTextureFilterMode         textureFilter;
    };
    VALIDATE_SIZE(imf, 0x158);

    static inline NOTSA_GLOBAL(ms_imf, 0xC40150, (imf), {});
};
