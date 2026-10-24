#include "StdInc.h"
#include "SearchLight.h"

// Offsets the raw-offset accesses of 0x493360 / 0x493420 / 0x493480 resolve to
static_assert(offsetof(tScriptSearchlight, m_Target) == 0x14);
static_assert(offsetof(tScriptSearchlight, m_PathCoord1) == 0x28);
static_assert(offsetof(tScriptSearchlight, m_PathCoord2) == 0x34);
static_assert(offsetof(tScriptSearchlight, m_fPathSpeed) == 0x40);
VALIDATE_OFFSET(tScriptSearchlight, m_FollowingEntity, 0x48);

void CSearchLight::InjectHooks() {
    RH_ScopedClass(CSearchLight);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(IsSpottedEntity, 0x493900);
    RH_ScopedInstall(IsPointInsideLitEllipse, 0x493280);
    RH_ScopedInstall(SetPathBetween, 0x493360);
    RH_ScopedInstall(SetFollowEntity, 0x493420);
    RH_ScopedInstall(SetTravelToPoint, 0x493480);
}

//! 0x59C970 (`CVector::NormaliseAndMag`) the way the original runs it: the sum of squares, the reciprocal and the returned
//! magnitude stay in x87 registers (extended precision, `double` here), only the components are rounded to float.
static double NormaliseAndMagOriginal(CVector& v) {
    const x87d sq = ((x87d)v.x * (x87d)v.x + (x87d)v.y * (x87d)v.y) + (x87d)v.z * (x87d)v.z;
    if (sq <= 0.0) { // FCOM + JP: only taken (=> normalise) for `sq > 0` and NaN
        v.x = 1.0f;
        return 1.0;
    }
    const x87d recip = (x87d)1.0 / std::sqrt(sq);
    v.x = (float)(recip * (x87d)v.x);
    v.y = (float)(recip * (x87d)v.y);
    v.z = (float)(recip * (x87d)v.z);
    return (x87d)1.0 / recip;
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

    const x87d dx = (x87d)point.x - (x87d)light.m_TargetSpot.x;
    const x87d dy = (x87d)point.y - (x87d)light.m_TargetSpot.y;
    const x87d dz = (x87d)point.z - (x87d)light.m_TargetSpot.z;

    const x87d dotA = ((x87d)axisA.z * dz + (x87d)axisA.y * dy) + (x87d)axisA.x * dx;
    const float  distA = (float)(dotA / (x87d)lenA); // Spilled to the stack as a float

    const x87d dotB  = ((x87d)axisB.z * dz + (x87d)axisB.y * dy) + (x87d)axisB.x * dx;
    const x87d distB = X87R(dotB / lenB);

    // FCOMP + JNP: true for `<= 1.0` (and false for NaN)
    return distB * distB + (x87d)distA * (x87d)distA <= 1.0;
}

// NOTSA name (the original has no symbol)
// 0x493480
void CSearchLight::SetTravelToPoint(int32 scriptIndex, CVector point, float speed) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(scriptIndex, SCRIPT_THING_SEARCH_LIGHT);
    if (idx < 0) {
        return;
    }
    auto& light = CTheScripts::ScriptSearchLightArray[idx];

    light.m_PathCoord1 = point;
    light.m_PathCoord2 = CVector{};
    light.m_nCurrentState = eScriptSearchLightState::STATE_4; // the original writes the whole byte (0x84)
    light.m_SomethingFlag = true;
    light.m_fPathSpeed    = speed;
    light.m_FollowingEntity = nullptr; // CleanUpOldReference (0x571A00) + clear
}

// NOTSA name (the original has no symbol)
// 0x493420
void CSearchLight::SetFollowEntity(int32 scriptIndex, CEntity* entity, float speed) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(scriptIndex, SCRIPT_THING_SEARCH_LIGHT);
    if (idx < 0) {
        return;
    }
    auto& light = CTheScripts::ScriptSearchLightArray[idx];

    light.m_nCurrentState = eScriptSearchLightState::STATE_3; // the original writes the whole byte (0x83)
    light.m_SomethingFlag = true;
    light.m_PathCoord1    = CVector{};
    light.m_PathCoord2    = CVector{};
    light.m_fPathSpeed    = speed;
    light.m_FollowingEntity = entity; // CleanUpOldReference (0x571A00) + RegisterReference (0x571B70)
}

// NOTSA name (the original has no symbol)
// 0x493360
void CSearchLight::SetPathBetween(int32 scriptIndex, CVector p1, CVector p2, float speed) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(scriptIndex, SCRIPT_THING_SEARCH_LIGHT);
    if (idx < 0) {
        return;
    }
    auto& light = CTheScripts::ScriptSearchLightArray[idx];

    // The squared (XY) distances of the current target to both path points (x87 extended precision)
    const x87d d1x = (x87d)light.m_Target.x - (x87d)p1.x;
    const x87d d1y = (x87d)light.m_Target.y - (x87d)p1.y;
    const x87d dist1 = d1y * d1y + d1x * d1x;
    const x87d d2x = (x87d)light.m_Target.x - (x87d)p2.x;
    const x87d d2y = (x87d)light.m_Target.y - (x87d)p2.y;
    const x87d dist2 = d2y * d2y + d2x * d2x;

    light.m_PathCoord1 = p1;
    light.m_PathCoord2 = p2;
    // FCOMPP + `test ah, 0x41; jp`: state 2 if `dist1 > dist2` or unordered, state 1 otherwise (the original writes the whole byte: 0x81 / 0x82)
    light.m_nCurrentState = (dist1 <= dist2) ? eScriptSearchLightState::STATE_1 : eScriptSearchLightState::STATE_2;
    light.m_SomethingFlag = true;
    light.m_fPathSpeed    = speed;
    light.m_FollowingEntity = nullptr; // CleanUpOldReference (0x571A00) + clear
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
