#pragma once

#include <Base.h>

// *** 🤝 UNION 🤝 ***
union tScriptParam {
    uint8  u8Param;
    int8   i8Param;

    uint16 u16Param;
    int16  i16Param;

    uint32 uParam{0u};
    int32  iParam;

    float  fParam;
    bool   bParam;
    // NOTE: the exe's `void* pParam / char* szParam` members were never used and made the union 8 bytes on 64-bit targets (it is dumped raw inside CRunningScript).
};
VALIDATE_SIZE_FILE(tScriptParam, 0x4);
