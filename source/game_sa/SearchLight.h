#pragma once
#include "Base.h"
class CEntity;

class CSearchLight {
public:

    static void InjectHooks();
    static void SetTravelToPoint();
    static void SetFollowEntity();
    static void SetPathBetween();
    static void IsLookingAtPos();
    static void GetOnEntity();
    static bool IsSpottedEntity(uint32 index, const CEntity& entity);

    //! NOTSA name: Is the point inside the ellipse (spanned by the light's two axes) around its target spot
    static bool IsPointInsideLitEllipse(const CVector& point, int32 searchLightIdx);
};
