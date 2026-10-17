/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include <OnscreenTimer.h>
#include <CurrentVehicle.h>
#include <PlaceName.h>

class CUserDisplay {
public:
    static inline NOTSA_GLOBAL(PlaceName, 0xBA18F4, (CPlaceName), {});
    static inline NOTSA_GLOBAL(OnscnTimer, 0xBA1788, (COnscreenTimer), {});
    static inline NOTSA_GLOBAL(CurrentVehicle, 0xBA18FC, (CCurrentVehicle), {});

public:
    static void InjectHooks();

    static void Init();
    static void Process();
};
