#pragma once

#include "ColModel.h"
#include <array>
#include "Base.h"

class CTempColModels {
public:
    static inline NOTSA_GLOBAL(ms_colModelBBox,      0x968A00, (CColModel), {}); static_assert(sizeof(ms_colModelBBox) == 48);
    static inline NOTSA_GLOBAL(ms_colModelPed1,      0x968DF0, (CColModel), {}); static_assert(sizeof(ms_colModelPed1) == 48);
    static inline NOTSA_GLOBAL(ms_colModelPed2,      0x968E20, (CColModel), {}); static_assert(sizeof(ms_colModelPed2) == 48);
    static inline NOTSA_GLOBAL(ms_colModelDoor1,     0x968E50, (CColModel), {}); static_assert(sizeof(ms_colModelDoor1) == 48);
    static inline NOTSA_GLOBAL(ms_colModelBumper1,   0x968E80, (CColModel), {}); static_assert(sizeof(ms_colModelBumper1) == 48);
    static inline NOTSA_GLOBAL(ms_colModelPanel1,    0x968EB0, (CColModel), {}); static_assert(sizeof(ms_colModelPanel1) == 48);
    static inline NOTSA_GLOBAL(ms_colModelBonnet1,   0x968EE0, (CColModel), {}); static_assert(sizeof(ms_colModelBonnet1) == 48);
    static inline NOTSA_GLOBAL(ms_colModelBoot1,     0x968F10, (CColModel), {}); static_assert(sizeof(ms_colModelBoot1) == 48);
    static inline NOTSA_GLOBAL(ms_colModelWheel1,    0x968F40, (CColModel), {}); static_assert(sizeof(ms_colModelWheel1) == 48);
    static inline NOTSA_GLOBAL(ms_colModelBodyPart1, 0x968F70, (CColModel), {}); static_assert(sizeof(ms_colModelBodyPart1) == 48);
    static inline NOTSA_GLOBAL(ms_colModelBodyPart2, 0x968FA0, (CColModel), {}); static_assert(sizeof(ms_colModelBodyPart2) == 48);
    static inline NOTSA_GLOBAL(ms_colModelWeapon,    0x968FD0, (CColModel), {}); static_assert(sizeof(ms_colModelWeapon) == 48);

    static constexpr auto MAX_NUM_CUTSCENE_COLMODELS = MODEL_CUTOBJ20 - MODEL_CUTOBJ01 + 1; // 20
    static inline NOTSA_GLOBAL(ms_colModelCutObj, 0x968A30, (std::array<CColModel, MAX_NUM_CUTSCENE_COLMODELS>), {}); static_assert(sizeof(ms_colModelCutObj) == 960);
    
public:
    static void InjectHooks();

public:
    static void Initialise();
    static void Shutdown();
};
