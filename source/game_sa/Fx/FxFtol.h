#pragma once

#include "Base.h"

namespace notsa::detail {
// 0x821B40: CRT _ftol2 - truncates to a 64 bit integer (fistp; 0x8000000000000000 on overflow/NaN) and returns the low 32 bits
inline int32 Ftol(double v) {
    if (!(v > -9223372036854775808.0 && v < 9223372036854775808.0)) { // also catches NaN
        return 0;
    }
    return (int32)(int64)v;
}
}; // namespace notsa::detail
