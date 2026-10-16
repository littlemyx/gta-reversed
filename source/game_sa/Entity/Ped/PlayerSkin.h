#pragma once

#include "RenderWare.h"

class CPlayerSkin {
public:
    static inline NOTSA_GLOBAL(m_txdSlot, 0xC3F03C, (int32), {});

public:
    static void InjectHooks();

    static void       Initialise();
    static void       Shutdown();
    static void       RenderFrontendSkinEdit();
    static RwTexture* GetSkinTexture(const char* name);
};
