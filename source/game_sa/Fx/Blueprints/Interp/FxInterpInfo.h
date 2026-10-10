#pragma once

#include "FxTools.h"

class NOTSA_EXPORT_VTABLE FxInterpInfo_c {
public:
    static void InjectHooks();
    bool    m_bLooped;  // (4 bytes in Manhunt)
    int8    m_nNumKeys;
    int16   m_nCount;
    uint16* m_pTimes;

public:
    FxInterpInfo_c();
    virtual ~FxInterpInfo_c(); // 0x4A8430 (out of line on purpose: the exe's dtor restores the vftable)

    virtual void Load(FILESTREAM file) = 0;
};
