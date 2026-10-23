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
    void*  pParam;
    char*  szParam;
    bool   bParam;
};
VALIDATE_SIZE_FILE(tScriptParam, 0x4);
