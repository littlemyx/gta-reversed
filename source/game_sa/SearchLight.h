#pragma once
#include "Base.h"
class CEntity;

class CSearchLight {
public:

    static void InjectHooks();
    //! 0x493480 (no symbol): the light travels to `point` and stops there (state 4)
    static void SetTravelToPoint(int32 scriptIndex, CVector point, float speed);
    //! 0x493420 (no symbol): the light follows `entity` (state 3)
    static void SetFollowEntity(int32 scriptIndex, CEntity* entity, float speed);
    //! 0x493360 (no symbol): the light moves back and forth between `p1` and `p2` (state 1 / 2)
    static void SetPathBetween(int32 scriptIndex, CVector p1, CVector p2, float speed);
    static void IsLookingAtPos();
    static void GetOnEntity();
    static bool IsSpottedEntity(uint32 index, const CEntity& entity);

    //! NOTSA name: Is the point inside the ellipse (spanned by the light's two axes) around its target spot
    static bool IsPointInsideLitEllipse(const CVector& point, int32 searchLightIdx);
};
