#pragma once

#include <utility>

#include "World.h"
#include "Matrix.h"

/*!
* Helpers shared by the S6-B script command ports (Group09_12*.cpp): ids 900..1299
*/
namespace notsa::script::commands::ported::g09_12 {
void RegisterG9();
void RegisterG10();
void RegisterG11();
void RegisterG12();

//! x87-compare idiom `fcom -100.0f; test ah, 0x41; jp skip`: the ground Z is looked up only when `z <= -100.0f` (ordered).
inline float GroundZIfAuto(float x, float y, float z) {
    if (z <= -100.0f) { // 0x859014
        return CWorld::FindGroundZForCoord(x, y);
    }
    return z;
}

//! The `fld a; fcomp b; test ah, 0x41; jne skip; <swap>` idiom: swaps only if `a > b` (ordered)
inline void SortPair(float& a, float& b) {
    if (a > b) {
        std::swap(a, b);
    }
}

//! 0x59C790 (`CMatrix::MultiplyMatrixWithVector`): the sum of the 3 products stays in extended precision, with the add order of the original
inline CVector TransformVectorOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return CVector{
        (float)(((double)u.x * v.z + (double)f.x * v.y) + (double)r.x * v.x),
        (float)(((double)u.y * v.z + (double)r.y * v.x) + (double)f.y * v.y),
        (float)(((double)u.z * v.z + (double)r.z * v.x) + (double)f.z * v.y)
    };
}

//! The id the exe derives from the script pointer + IP (`this + this->m_IP`, used e.g. by the sphere / marker commands)
inline uint32 ScriptThingIdFromIP(const CRunningScript& S) {
    return reinterpret_cast<uint32>(&S) + reinterpret_cast<uint32>(S.m_IP);
}
} // namespace notsa::script::commands::ported::g09_12
