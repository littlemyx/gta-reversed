#pragma once

#include <rwplcore.h>

class CClumpModelInfo;

/**
 * collision plugin unique rwID
 */
#define rwID_COLLISIONPLUGIN  MAKECHUNKID(rwVENDORID_DEVELOPER, 0xFA)

/*
* Collision plugin static offset
*/
NOTSA_GLOBAL_EXTERN(gCollisionPluginOffset, (RwInt32));

class CCollisionPlugin {
public:
    static inline NOTSA_GLOBAL(ms_currentModel, 0x9689E0, (CClumpModelInfo*), {});

public:
    static void InjectHooks();

    static bool PluginAttach();
    static void SetModelInfo(CClumpModelInfo* modelInfo);
};
