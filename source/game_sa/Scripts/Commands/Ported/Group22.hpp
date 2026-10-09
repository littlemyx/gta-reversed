#pragma once

#include "Group09_12.hpp"

#include <cmath>

#include "Population.h"
#include "MissionCleanup.h"
#include "PedGroups.h"

/*!
* Helpers shared by the S6-G script command ports (Group22*.cpp): ids 2200..2299 (exe group processor g22 @0x474900, switch base 2200)
*/
namespace notsa::script::commands::ported::g22 {
void RegisterHandlers(); // (also declared in Commands.hpp)
void RegisterG22a();     // ids 2200..2249
void RegisterG22b();     // ids 2251..2299

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

//! The loop shared by GET_RANDOM_CHAR_IN_SPHERE_ONLY_DRUGS_BUYERS (2206) and GET_RANDOM_CHAR_IN_SPHERE_NO_BRAIN (2277).
//! Peds from the last pool slot to the first; `pedOk` holds the per-command conditions (everything between the createdBy/IsPedDead
//! pre-checks and the group / distance checks differs). The distance is the extended-precision ST0 value: it is compared to the radius
//! unrounded (`dist < radius`), then stored as float, compared with the best distance (an INT32, initially 9999: `fild; fcomp`) and the
//! new best is `_ftol2(dist)`. The chosen ped becomes a MISSION ped (CPed::SetCharCreatedBy(2), ms_nTotalMissionPeds++, mission cleanup list).
template<typename TPedOk>
inline int32 FindRandomCharInSphere(CRunningScript& S, const CVector& c, float radius, TPedOk&& pedOk) {
    int32 result = -1;
    int32 best   = 9999; // 0x270F
    for (auto i = GetPedPool()->GetSize(); i-- > 0;) { // 0x4082A0 = pool->GetAt(i)
        CPed* const ped = GetPedPool()->GetAt(i);
        if (!ped) {
            continue;
        }
        if (ped->GetCreatedBy() != PED_GAME) { // cmp byte [+0x484], 1
            continue;
        }
        if (ped->m_bRemoveFromWorld) { // test [+0x1C], 0x800
            continue;
        }
        if (ped->bFadeOut) { // test byte [+0x470], 8
            continue;
        }
        if (!pedOk(S, ped)) {
            continue;
        }
        const CVector pos = ped->GetPosition();
        const CVector d{ pos.x - c.x, pos.y - c.y, pos.z - c.z }; // each component is rounded to float (fstp)
        const double  dist = Magnitude87(d);
        if (!(dist < (double)radius)) { // fcomp + `test ah, 5; jp skip`
            continue;
        }
        const float distF = (float)dist; // fst [mem]
        if (!((double)best > (double)distF)) { // fild best; fcomp distF; `test ah, 0x41; jne skip`
            continue;
        }
        result = GetPedPool()->GetRef(ped); // 0x4442D0
        best   = Ftol(distF);               // _ftol2
    }
    if (result >= 0) {
        GetPedPool()->GetAtRef(result)->SetCharCreatedBy(PED_MISSION); // 0x5E47E0
        CPopulation::ms_nTotalMissionPeds++;
        if (S.m_UsesMissionCleanup) {
            CTheScripts::MissionCleanUp.AddEntityToList(result, MISSION_CLEANUP_ENTITY_TYPE_PED); // 0x4637E0
        }
    }
    return result;
}

} // namespace notsa::script::commands::ported::g22
