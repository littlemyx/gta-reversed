/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "eWeaponType.h"

NOTSA_GLOBAL_HDR(gpFinishFlagTex, 0xC7C718, (RwTexture*), {});

class CSpecialFX {
public:
    static void Init();
    static void Update();
    static void Shutdown();
    static void AddWeaponStreak(eWeaponType weaponType);
    static void Render();
    static void Render2DFXs();
    static void ReplayStarted();

public:
    static inline NOTSA_GLOBAL(bVideoCam, 0xC7C70C, (bool), {});
    static inline NOTSA_GLOBAL(bLiftCam, 0xC7C70D, (bool), {});
    static inline NOTSA_GLOBAL(bSnapShotActive, 0xC7C714, (bool), {});
    static inline NOTSA_GLOBAL(SnapShotFrames, 0xC7C710, (uint32), {});

private:
    friend void InjectHooksMain();
    static void InjectHooks();
};
