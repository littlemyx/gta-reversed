#pragma once

#include <WaterCannon.h>

class CVector;

class CWaterCannons {
public:
    static constexpr uint32 MAX_WATER_CANNONS = 3;
    static inline NOTSA_GLOBAL(aCannons, 0xC80740, (std::array<CWaterCannon, 3>), {}); static_assert(sizeof(aCannons) == 2916);

public:
    static void InjectHooks();

    static void Init();
    static void UpdateOne(uint32 vehicle, CVector* start, CVector* end);
    static void Update();
    static void Render();
};
