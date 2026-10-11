/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "Vector.h"
#include <rwplcore.h>

constexpr auto MAX_MOVING_FOG = 350;
constexpr auto MAX_VOLUMETRIC_CLOUDS = 180;

constexpr auto SKYP_WIDTH_MULT = 1.4f;
constexpr auto SKYP_CAM_DIST = 30.f;

constexpr auto SKYP_ABOVE_HORIZON_Z = 0.5f;
constexpr auto SKYP_HORIZON_Z = 0.0f;
constexpr auto SKYP_SEA_HORIZON_Z = -0.1f;
constexpr auto SKYP_BELOW_HORIZON_Z = -0.3f;

struct tMovingFog {
    std::array<bool, MAX_MOVING_FOG>    m_bFogSlots;
    std::array<CVector, MAX_MOVING_FOG> m_vecPosn;
    std::array<float, MAX_MOVING_FOG>   m_fSize;
    std::array<float, MAX_MOVING_FOG>   m_fIntensity;
    std::array<float, MAX_MOVING_FOG>   m_fMaxIntensity;
    CVector                             m_vecWind;
    std::array<float, MAX_MOVING_FOG>   m_Speed;
    std::array<int, 12>                 field_27B4;
    std::array<uint32, 6>               m_nPrimIndices;
};

struct tVolumetricClouds {
    std::array<bool, MAX_VOLUMETRIC_CLOUDS>    bUsed;
    std::array<bool, MAX_VOLUMETRIC_CLOUDS>    bJustCreated;

    std::array<CVector, MAX_VOLUMETRIC_CLOUDS> pos;
    std::array<CVector, MAX_VOLUMETRIC_CLOUDS> size;

    std::array<int32, MAX_VOLUMETRIC_CLOUDS>   alpha;

    RwTexture* texture;

    std::array<CVector, 3> quadNormal;

    std::array<float, 18> modelX;
    std::array<float, 18> modelY;
    std::array<float, 18> modelZ;

    std::array<float, 18> modelU;
    std::array<float, 18> modelV;
};

class CClouds {
public:
    static inline NOTSA_GLOBAL(m_fVolumetricCloudDensity, 0x8D5388, (float), { 1.0f });
    static inline NOTSA_GLOBAL(m_bVolumetricCloudHeightSwitch, 0x8D538C, (int8), { 1 });
    static inline NOTSA_GLOBAL(m_fVolumetricCloudWindMoveFactor, 0x8D5390, (float), { 0.1f });
    static inline NOTSA_GLOBAL(m_fVolumetricCloudMaxDistance, 0xC6AA58, (float), {});
    static inline NOTSA_GLOBAL(m_VolumetricCloudsUsedNum, 0xC6AA5C, (uint32), {});
    static inline NOTSA_GLOBAL(ms_cameraRoll, 0xC6AA64, (float), {});
    static inline NOTSA_GLOBAL(IndividualRotation, 0xC6AA6C, (int32), {});
    static inline NOTSA_GLOBAL(CloudRotation, 0xC6AA70, (float), {});
    static inline NOTSA_GLOBAL(ms_vc, 0xC6AAB0, (tVolumetricClouds), {});
    static inline NOTSA_GLOBAL(ms_mf, 0xC6C158, (tMovingFog), {});
    static inline NOTSA_GLOBAL(PlayerCoords, 0xC6E958, (CVector), {}); // gVecPlayerCoors
    static inline NOTSA_GLOBAL(CameraCoors, 0xC6E964, (CVector), {});  // gVecCameraCoors

    struct DebugSettings {
        struct RenderSettingPair {
            bool Enabled = true, Force = false;
        } Moon, Rockstar, LowClouds, Rainbow, Streaks, VolumetricClouds;
    }; static DebugSettings s_DebugSettings; // NOTSA: defined at the end of the header (clang: default member initializers of nested types inside the enclosing class)

public:
    static void InjectHooks();

    static void Init();
    static void Update();
    static void Shutdown();

    static void SetUpOneSkyPoly(CVector vert1pos, CVector vert2pos, CVector vert3pos, CVector vert4pos, uint8 topRed, uint8 topGreen, uint8 topBlue, uint8 bottomRed, uint8 bottomGreen, uint8 bottomBlue);

    static void MovingFogInit();
    static void MovingFog_Create(CVector* posn);
    static void MovingFog_Delete(int32 fogSlotIndex);
    static void MovingFog_Update();
    static void MovingFogRender();
    static void Render_MaybeRenderMoon(float colorBalance);
    static void Render_MaybeRenderRockstarLogo(float colorBalance);
    static void Render_RenderLowClouds(float colorBalance);
    static void Render_MaybeRenderRainbows();
    static void Render_MaybeRenderStreaks();
    static float MovingFog_GetFXIntensity();
    static CVector MovingFog_GetWind();
    static int32 MovingFog_GetFirstFreeSlot();

    static void VolumetricCloudsInit();
    static void VolumetricClouds_Create(CVector* posn);
    static void VolumetricClouds_Delete(int32 vcSlotIndex);
    static float VolumetricCloudsGetMaxDistance();
    static int32 VolumetricClouds_GetFirstFreeSlot();
    static void VolumetricCloudsRender();

    static void Render();
    static void RenderSkyPolys();
    static void RenderBottomFromHeight();
};

NOTSA_GLOBAL_EXTERN(CurrentFogIntensity, (float));               // default 1.0f
NOTSA_GLOBAL_EXTERN(gpMoonMask, (RwTexture*));
NOTSA_GLOBAL_EXTERN(gpCloudTex, (RwTexture*));
NOTSA_GLOBAL_EXTERN(gpCloudMaskTex, (RwTexture*));
extern float& flt_C6E954;
extern float& flt_C6E970;

extern int32& dword_C6E974;

inline CClouds::DebugSettings CClouds::s_DebugSettings{};
