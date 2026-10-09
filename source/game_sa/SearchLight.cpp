#include "StdInc.h"
#include "SearchLight.h"

void CSearchLight::InjectHooks() {
    RH_ScopedClass(CSearchLight);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(IsSpottedEntity, 0x493900);
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
    // 0x493280 - Not reversed yet (cdecl, `bool(const CVector* pos, int32 searchLightIdx)`), checks if the point is inside the search light's cone
    return plugin::CallAndReturn<bool, 0x493280, const CVector*, int32>(&pos, idx);
}
