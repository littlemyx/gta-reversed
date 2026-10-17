/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include <CarGenerator.h>
#include <SpecialPlateHandler.h>

static constexpr int32 NUM_CAR_GENERATORS = 500;

class CTheCarGenerators {
public:
    static inline NOTSA_GLOBAL(GenerateEvenIfPlayerIsCloseCounter, 0xC279D0, (uint8), {});
    static inline NOTSA_GLOBAL(ProcessCounter, 0xC279D1, (uint8), {});
    static inline NOTSA_GLOBAL(NumOfCarGenerators, 0xC279D4, (uint32), {});
    static inline NOTSA_GLOBAL(m_SpecialPlateHandler, 0xC279D8, (CSpecialPlateHandler), {});
    static inline NOTSA_GLOBAL(CarGeneratorArray, 0xC27AD0, (CCarGenerator[NUM_CAR_GENERATORS]), {});

public:
    static void InjectHooks();

    static int32 CreateCarGenerator(CVector posn, float angle, int32 modelId, int16 color1, int16 color2, uint8 forceSpawn, uint8 alarmChances, uint8 doorLockChances,
                                         uint16 minDelay, uint16 maxDelay, uint8 iplId, bool ignorePopulationLimit);
    static void Init();
    static void Load();
    static void Process();
    static void RemoveCarGenerators(uint8 IplID);
    static void Save();

    static CCarGenerator* Get(uint16 index);
    static int32 GetIndex(CCarGenerator* pCarGen);
};
