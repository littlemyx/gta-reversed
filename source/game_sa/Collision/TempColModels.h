#pragma once

#include "ColModel.h"
#include <array>
#include "Base.h"

class CTempColModels {
public:
    static inline NOTSA_GLOBAL(ms_colModelBBox,      0x968A00, (CColModel), {});
    static inline NOTSA_GLOBAL(ms_colModelPed1,      0x968DF0, (CColModel), {});
    static inline NOTSA_GLOBAL(ms_colModelPed2,      0x968E20, (CColModel), {});
    static inline NOTSA_GLOBAL(ms_colModelDoor1,     0x968E50, (CColModel), {});
    static inline NOTSA_GLOBAL(ms_colModelBumper1,   0x968E80, (CColModel), {});
    static inline NOTSA_GLOBAL(ms_colModelPanel1,    0x968EB0, (CColModel), {});
    static inline NOTSA_GLOBAL(ms_colModelBonnet1,   0x968EE0, (CColModel), {});
    static inline NOTSA_GLOBAL(ms_colModelBoot1,     0x968F10, (CColModel), {});
    static inline NOTSA_GLOBAL(ms_colModelWheel1,    0x968F40, (CColModel), {});
    static inline NOTSA_GLOBAL(ms_colModelBodyPart1, 0x968F70, (CColModel), {});
    static inline NOTSA_GLOBAL(ms_colModelBodyPart2, 0x968FA0, (CColModel), {});
    static inline NOTSA_GLOBAL(ms_colModelWeapon,    0x968FD0, (CColModel), {});

    static constexpr auto MAX_NUM_CUTSCENE_COLMODELS = MODEL_CUTOBJ20 - MODEL_CUTOBJ01 + 1; // 20
    static inline NOTSA_GLOBAL(ms_colModelCutObj, 0x968A30, (std::array<CColModel, MAX_NUM_CUTSCENE_COLMODELS>), {});
    
public:
    static void InjectHooks();

public:
    static void Initialise();
    static void Shutdown();
};
