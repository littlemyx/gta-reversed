#pragma once

#include "Vehicle.h"

enum eTrafficLightsState {
    LIGHT_GREEN = 0,
    LIGHT_YELLOW = 1,
    LIGHT_RED = 2,
    LIGHT_OFF = 3
};

enum eTrafficLightsDirection {
    DIR_DISABLED = 0,
    DIR_NORTH_SOUTH = 1,
    DIR_WEST_EAST = 2
};

class CTrafficLights {
private:
    static inline NOTSA_GLOBAL(aTrafficLightColoursR, 0x8A6214, (std::array<uint8, 3>), { 0, 255, 255 });
    static inline NOTSA_GLOBAL(aTrafficLightColoursG, 0x8A6218, (std::array<uint8, 3>), { 255, 128 });
    static inline NOTSA_GLOBAL(aTrafficLightColoursB, 0xA9AD94, (std::array<uint8, 3>), {});
    static inline NOTSA_GLOBAL(fLightMult, 0x8A621C, (float), { 0.0015686274f });

    static inline NOTSA_GLOBAL(vecTrafficLights5_1, 0xA9ADD4, (CVector), {});
    static inline NOTSA_GLOBAL(vecTrafficLights5_2, 0xA9ADC8, (CVector), {});
    static inline NOTSA_GLOBAL(vecTrafficLights_1, 0xA9ADBC, (CVector), {});
    static inline NOTSA_GLOBAL(vecTrafficLights_2, 0xA9ADB0, (CVector), {});
    static inline NOTSA_GLOBAL(uiPedLightFlags, 0xA9ADE0, (uint32), {});

public:
    static void InjectHooks();

public:
    static bool ShouldCarStopForLight(CVehicle* vehicle, bool bUnkn);
    static bool ShouldCarStopForBridge(CVehicle* vehicle);
    static void DisplayActualLight(CEntity* entity);
    static bool IsMITrafficLight(int32 modelIndex);
    static uint8 LightForPeds();

private:
    static uint8 LightForCars1();
    static uint8 LightForCars2();
    static uint8 LightForCars1_Visual();
    static uint8 LightForCars2_Visual();
    static int32 FindTrafficLightType(CEntity* entity);
    static float FindOrientationForTrafficLightType(CEntity* entity);
    static int32 FindTrafficLightTypeFromOrientation(float fOrientation);
};
