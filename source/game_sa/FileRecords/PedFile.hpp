#pragma once
// P-2d: on-disk (x86 exe) layouts of CWeapon and CPedSaveStructure. Standalone POD header (no StdInc): usable from tests/portable/p2d_*.cpp.
//
// CPedSaveStructure is built by CPed::Save (Construct) and dumped raw by the exe, behind a uint32 size prefix; CPed::Load reads it back raw and asserts the prefix equals its size.
// CWeapon (0x1C) is its only member with a pointer: `FxSystem_c* m_FxSystem` @0x18 (4 bytes on the exe, 8 on an LP64 host => CWeapon 0x20, CPedSaveStructure 0x1EC).
// The exe stored the live pointer there; CPedSaveStructure::Extract never reads it, so the record writes 0. Padding bytes are explicit (the exe wrote stack garbage).
#include <cstddef>
#include <cstdint>

struct CWeapon_File {
    uint32_t type;                 // 0x00 eWeaponType
    uint32_t state;                // 0x04 eWeaponState
    uint32_t ammoInClip;           // 0x08
    uint32_t totalAmmo;            // 0x0C
    uint32_t timeForNextShotMs;    // 0x10
    uint8_t  firstPersonModeSel;   // 0x14 m_IsFirstPersonWeaponModeSelected
    uint8_t  dontPlaceInHand;      // 0x15
    uint8_t  pad16[2];             // 0x16
    uint32_t fxSystem;             // 0x18 FxSystem_c* : always 0 on disk (meaningless in a file, never read back)
};
static_assert(sizeof(CWeapon_File) == 0x1C && alignof(CWeapon_File) == 4, "Invalid structure size of CWeapon_File"); // == VALIDATE_SIZE_FILE (this header is StdInc-free)
static_assert(offsetof(CWeapon_File, ammoInClip) == 0x08 && offsetof(CWeapon_File, timeForNextShotMs) == 0x10 && offsetof(CWeapon_File, firstPersonModeSel) == 0x14
           && offsetof(CWeapon_File, dontPlaceInHand) == 0x15 && offsetof(CWeapon_File, fxSystem) == 0x18);

struct CPedSaveStructure_File {
    float        pos[3];             // 0x00 CVector m_pos
    float        health;             // 0x0C
    float        armor;              // 0x10
    CWeapon_File weapons[13];        // 0x14 (13 * 0x1C = 0x16C)
    uint8_t      createdBy;          // 0x180 ePedCreatedBy (uint8)
    int8_t       activeWeaponSlot;   // 0x181
    int8_t       areaCode;           // 0x182
    uint8_t      pad183;             // 0x183
    int32_t      exitIndex;          // 0x184 -1 = none
    uint8_t      fightingStyle;      // 0x188
    uint8_t      allowedAttackMoves; // 0x189
    uint8_t      pad18A[2];          // 0x18A
};
static_assert(sizeof(CPedSaveStructure_File) == 0x18C && alignof(CPedSaveStructure_File) == 4, "Invalid structure size of CPedSaveStructure_File");
static_assert(offsetof(CPedSaveStructure_File, health) == 0xC && offsetof(CPedSaveStructure_File, weapons) == 0x14 && offsetof(CPedSaveStructure_File, createdBy) == 0x180
           && offsetof(CPedSaveStructure_File, activeWeaponSlot) == 0x181 && offsetof(CPedSaveStructure_File, areaCode) == 0x182 && offsetof(CPedSaveStructure_File, exitIndex) == 0x184
           && offsetof(CPedSaveStructure_File, fightingStyle) == 0x188 && offsetof(CPedSaveStructure_File, allowedAttackMoves) == 0x189);
