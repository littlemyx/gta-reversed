/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "Vector.h"
#include "RGBA.h"

class CEntity;

typedef int32 CrossHairId;

enum eWeaponEffectsLockTexture {
    WEAPONEFFECTS_LOCK_ON = 0,
    WEAPONEFFECTS_LOCK_ON_FIRE = 1
};

class CWeaponEffects {
public:
    bool    m_bActive;
    int32   m_nTimeWhenToDeactivate; // -1 default
    CVector m_vecPosn;
    CRGBA   m_color;
    float   m_fSize;
    float   m_fRingAngle;     // 0x1C: angle (radians) of the rotating lock-on triangles
    float   m_fLockOnFade;    // 0x20: fade value of the flight lock-on sprite (see Render)
    float   m_fRotation;
    bool    m_bClearImmediately;

public:
    static void InjectHooks();

    CWeaponEffects() = default;  // 0x742A90
    ~CWeaponEffects() = default; // 0x742AA0

    static void Init();
    static void Shutdown();
    static bool IsLockedOn(CrossHairId id);
    static void MarkTarget(CrossHairId id, CVector posn, uint8 red, uint8 green, uint8 blue, uint8 alpha, float size, bool bClearImmediately);
    static void ClearCrossHair(CrossHairId id);
    static void ClearCrossHairs();
    static void ClearCrossHairImmediately(CrossHairId id);
    static void ClearCrossHairsImmediately();
    static void Render();
};

VALIDATE_SIZE(CWeaponEffects, 0x2C);

constexpr auto MAX_NUM_WEAPON_CROSSHAIRS{ 2u };
static inline auto& gCrossHair = StaticRef<std::array<CWeaponEffects, MAX_NUM_WEAPON_CROSSHAIRS>>(0xC8A838);
NOTSA_GLOBAL_HDR(gpCrossHairTex, 0xC8A818, (RwTexture*), {});
NOTSA_GLOBAL_HDR(gpCrossHairTexFlight, 0xC8A810, (RwTexture*[2]), {});

// NOTSA names for the globals used by `CWeaponEffects::Render`
NOTSA_GLOBAL_HDR(gLastCrossHairTargetTime, 0xC8A890, (uint32), {});      // Time when `gpLastCrossHairTarget` has changed
NOTSA_GLOBAL_HDR(gpLastCrossHairTarget, 0xC8A894, (CEntity*), {});    // Last target found by the 2nd player's lock-on
NOTSA_GLOBAL_HDR(gCrossHairLockOffsetCos, 0xC8A898, (float), {});       // cos(angle) * m_fLockOnFade, used as Y offset
NOTSA_GLOBAL_HDR(gCrossHairLockOffsetSin, 0xC8A89C, (float), {});       // sin(angle) * m_fLockOnFade, used as X offset
NOTSA_GLOBAL_HDR(gCrossHairRingOffset, 0xC8A8A0, (std::array<float, MAX_NUM_WEAPON_CROSSHAIRS>), {}); // Radius offset of the pulsing ring
NOTSA_GLOBAL_HDR(gCrossHairRingGrowing, 0x8D6144, (std::array<bool, MAX_NUM_WEAPON_CROSSHAIRS>), { true, true });  // Is the pulsing ring currently growing
