#include "StdInc.h"
#include "SearchLight.h"

void CSearchLight::InjectHooks() {
    RH_ScopedClass(CSearchLight);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(IsSpottedEntity, 0x493900);
    RH_ScopedInstall(IsPointInsideLitEllipse, 0x493280);
}

//! 0x59C970 (`CVector::NormaliseAndMag`) the way the original runs it: the sum of squares, the reciprocal and the returned
//! magnitude stay in x87 registers (extended precision, `double` here), only the components are rounded to float.
static double NormaliseAndMagOriginal(CVector& v) {
    const double sq = ((double)v.x * (double)v.x + (double)v.y * (double)v.y) + (double)v.z * (double)v.z;
    if (sq <= 0.0) { // FCOM + JP: only taken (=> normalise) for `sq > 0` and NaN
        v.x = 1.0f;
        return 1.0;
    }
    const double recip = 1.0 / std::sqrt(sq);
    v.x = (float)(recip * (double)v.x);
    v.y = (float)(recip * (double)v.y);
    v.z = (float)(recip * (double)v.z);
    return 1.0 / recip;
}

// NOTSA name (the original has no symbol)
// 0x493280
bool CSearchLight::IsPointInsideLitEllipse(const CVector& point, int32 searchLightIdx) {
    const auto& light = CTheScripts::ScriptSearchLightArray[searchLightIdx];

    // The two (semi-)axes of the lit ellipse around `m_TargetSpot`, normalised below (the lengths are the radii)
    CVector axisA = light.vf64;
    CVector axisB = light.vf70;
    const float  lenA = (float)NormaliseAndMagOriginal(axisA); // Spilled to the stack as a float
    const double lenB = NormaliseAndMagOriginal(axisB);        // Stays in the x87 stack

    const double dx = (double)point.x - (double)light.m_TargetSpot.x;
    const double dy = (double)point.y - (double)light.m_TargetSpot.y;
    const double dz = (double)point.z - (double)light.m_TargetSpot.z;

    const double dotA = ((double)axisA.z * dz + (double)axisA.y * dy) + (double)axisA.x * dx;
    const float  distA = (float)(dotA / (double)lenA); // Spilled to the stack as a float

    const double dotB  = ((double)axisB.z * dz + (double)axisB.y * dy) + (double)axisB.x * dx;
    const double distB = dotB / lenB;

    // FCOMP + JNP: true for `<= 1.0` (and false for NaN)
    return distB * distB + (double)distA * (double)distA <= 1.0;
}

void CSearchLight::SetTravelToPoint() {
    assert(0);
}

void CSearchLight::SetFollowEntity() {
    assert(0);
}

void CSearchLight::SetPathBetween() {
    assert(0);
}

void CSearchLight::IsLookingAtPos() {
    assert(0);
}

void CSearchLight::GetOnEntity() {
    assert(0);
}

// 0x493900
bool CSearchLight::IsSpottedEntity(uint32 index, const CEntity& entity) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(index, SCRIPT_THING_SEARCH_LIGHT);
    if (idx < 0) {
        return false;
    }

    const CVector pos = entity.GetPosition();
    return IsPointInsideLitEllipse(pos, idx); // 0x493280
}
