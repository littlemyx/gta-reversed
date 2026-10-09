#pragma once

#include <cmath>

#include "Vector.h"

/*!
* Helpers shared by the S6-J script command ports (Group25_26*.cpp): ids 2500..2699
* (exe group processors g25 @0x47A760, switch base 2500, and g26 @0x479DA0, switch base 2600)
*/
namespace notsa::script::commands::ported::g25_26 {
void RegisterHandlers(); // (also declared in Commands.hpp)
void RegisterG25a();     // ids 2500..2546
void RegisterG25b();     // ids 2556..2595
void RegisterG26();      // ids 2602..2632

//! `_ftol2` (0x821B40): the callers only use the LOW dword (EAX) of the int64 truncation; NaN / out of range => 0
inline int32 Ftol(double v) {
    if (!(v > -9223372036854775808.0 && v < 9223372036854775808.0)) { // also catches NaN
        return 0;
    }
    return (int32)(int64)v;
}

//! `CVector::Magnitude` (0x4082C0) as the exe evaluates it: ((x*x + y*y) + z*z) in extended precision, `fsqrt`, result left in ST0 (unrounded)
inline double Magnitude87(const CVector& v) {
    return std::sqrt((double)v.x * v.x + (double)v.y * v.y + (double)v.z * v.z);
}
} // namespace notsa::script::commands::ported::g25_26
