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
static inline auto& gpCrossHairTex = StaticRef<RwTexture*>(0xC8A818);
static inline auto& gpCrossHairTexFlight = StaticRef<RwTexture*[2]>(0xC8A810);

// NOTSA names for the globals used by `CWeaponEffects::Render`
static inline auto& gLastCrossHairTargetTime   = StaticRef<uint32>(0xC8A890);      // Time when `gpLastCrossHairTarget` has changed
static inline auto& gpLastCrossHairTarget      = StaticRef<CEntity*>(0xC8A894);    // Last target found by the 2nd player's lock-on
static inline auto& gCrossHairLockOffsetCos    = StaticRef<float>(0xC8A898);       // cos(angle) * m_fLockOnFade, used as Y offset
static inline auto& gCrossHairLockOffsetSin    = StaticRef<float>(0xC8A89C);       // sin(angle) * m_fLockOnFade, used as X offset
static inline auto& gCrossHairRingOffset       = StaticRef<std::array<float, MAX_NUM_WEAPON_CROSSHAIRS>>(0xC8A8A0); // Radius offset of the pulsing ring
static inline auto& gCrossHairRingGrowing      = StaticRef<std::array<bool, MAX_NUM_WEAPON_CROSSHAIRS>>(0x8D6144);  // Is the pulsing ring currently growing
