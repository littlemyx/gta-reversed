#pragma once

#include "FxTools.h"

#include "EmissionInfo.h"
#include "MovementInfo.h"
#include "RenderInfo.h"
#include "eFxInfoType.h"

class NOTSA_EXPORT_VTABLE FxInfo_c {
protected:
    eFxInfoType m_nType;
    bool        m_bTimeModeParticle;

public:
    // NOTSA: the exe allocates every FxInfo in FxInfoManager_c::AddFxInfo (0x4A7B00) from the Fx memory pool: GetMem(sizeof, 4)
    static void* operator new(size_t size);
    static void operator delete(void*) { /* pool memory is released all at once */ }

    FxInfo_c() = default;    // 0x4A4B30
    virtual ~FxInfo_c() = 0; // 0x4A4940
    virtual void Load(FILESTREAM file, int32 version) = 0;
    virtual void GetValue(float currentTime, float mult, float totalTime, float length, bool useConst, void* info) = 0;

    friend class FxInfoManager_c;
    friend class FxEmitterBP_c;
};
VALIDATE_SIZE(FxInfo_c, 0x8);
