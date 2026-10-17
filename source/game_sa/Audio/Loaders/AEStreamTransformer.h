#pragma once

#include "AEDataStream.h"

class CAEStreamTransformer {
public:
    void Initialise();
    void TransformBuffer(void* buffer, size_t size, uint32 position);

private:
    uint8 table[16]{ 0xEA, 0x3A, 0xC4, 0xA1, 0x9A, 0xA8, 0x14, 0xF3, 0x48, 0xB0, 0xD7, 0x23, 0x9D, 0xE8, 0xFF, 0xF1 }; // the exe fills it with `Initialise` (0x4F1750) from its static ctor table

    friend class CAEDataStream;
    static void InjectHooks();
};

NOTSA_GLOBAL_EXTERN(AEStreamTransformer, (CAEStreamTransformer));
