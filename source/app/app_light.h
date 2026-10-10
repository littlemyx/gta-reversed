#pragma once

#include "RenderWare.h"

extern void AppLightInjectHooks();

NOTSA_GLOBAL_HDR(pAmbient, 0xC886E8, (RpLight*), {});
NOTSA_GLOBAL_HDR(AmbientLightColour, 0xC886A4, (RwRGBAReal), {});
NOTSA_GLOBAL_HDR(AmbientLightColourForFrame, 0xC886D4, (RwRGBAReal), {});
NOTSA_GLOBAL_HDR(AmbientLightColourForFrame_PedsCarsAndObjects, 0xC886C4, (RwRGBAReal), {});

NOTSA_GLOBAL_HDR(pDirect, 0xC886EC, (RpLight*), {});
NOTSA_GLOBAL_HDR(DirectionalLightColour, 0xC88694, (RwRGBAReal), {});
NOTSA_GLOBAL_HDR(DirectionalLightColourForFrame, 0xC886B4, (RwRGBAReal), {});
NOTSA_GLOBAL_HDR(DirectAmbientLight, 0xC8865C, (std::array<RwRGBAReal, 2>), {}); // Direct, Ambient Light
NOTSA_GLOBAL_HDR(pExtraDirectionals, 0xC886F0, (std::array<RpLight*, 6>), {});
NOTSA_GLOBAL_HDR(NumExtraDirectionalLights, 0xC88708, (int32), {});
NOTSA_GLOBAL_HDR(LightStrengths, 0xC8867C, (std::array<float, 6>), {});

extern void ActivateDirectional();
extern void DeActivateDirectional();

extern void LightsCreate(RpWorld* world);
extern void LightsDestroy(RpWorld* world);
extern void LightsEnable(int32 enable);

extern void SetLightsWithTimeOfDayColour(RpWorld* world);

extern void WorldReplaceNormalLightsWithScorched(RpWorld* world, float lighting);
extern void WorldReplaceScorchedLightsWithNormal(RpWorld* world);

extern void AddAnExtraDirectionalLight(RpWorld* world, float x, float y, float z, float red, float green, float blue);
extern void RemoveExtraDirectionalLights(RpWorld* world);

extern void SetAmbientAndDirectionalColours(float fMult);
extern void ReSetAmbientAndDirectionalColours();

extern void SetFlashyColours(float fMult);
extern void SetFlashyColours_Mild(float fMult);

extern void SetBrightMarkerColours(float lighting);
extern void SetDirectionalColours(RwRGBAReal* color);

extern void SetAmbientColoursToIndicateRoadGroup(int32 idx);
extern void SetFullAmbient();
extern void SetAmbientColours();
extern void SetAmbientColours(RwRGBAReal* color);

extern void SetLightColoursForPedsCarsAndObjects(float fMult);

extern void SetLightsForInfraredVisionHeatObjects();
extern void StoreAndSetLightsForInfraredVisionHeatObjects();
extern void RestoreLightsForInfraredVisionHeatObjects();
extern void SetLightsForInfraredVisionDefaultObjects();
extern void SetLightsForNightVision();

float GetDayNightBalance();
