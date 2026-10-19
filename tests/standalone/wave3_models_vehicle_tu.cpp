// ORACLE-W3-MODELS: VehicleModelInfo.cpp compiled into this TU so the file-static helpers its header declares (IsValidCompRule, CountCompsInRule, ...) can be called by the oracle rows through the exported wrappers below.
#include "game_sa/Models/VehicleModelInfo.cpp"

bool        W3M_IsValidCompRule(int32 rule) { return IsValidCompRule(rule); }
int32       W3M_CountCompsInRule(int32 comps) { return CountCompsInRule(comps); }
int32       W3M_GetListOfComponentsNotUsedByRules(uint32 compRules, int32 numExtras, int32* outList) { return GetListOfComponentsNotUsedByRules(compRules, numExtras, outList); }
RpMaterial* W3M_RemoveWindowAlphaCB(RpMaterial* material, void* data) { return RemoveWindowAlphaCB(material, data); }
RwObject*   W3M_GetOkAndDamagedAtomicCB(RwObject* object, void* data) { return GetOkAndDamagedAtomicCB(object, data); }
