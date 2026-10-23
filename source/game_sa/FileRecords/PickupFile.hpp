#pragma once
// P-2e: on-disk (x86 exe) layout of CPickup. Standalone POD header (no StdInc): usable from tests/portable/p2e_*.cpp.
//
// CPickups::Save dumps every element of aPickUps raw (0x20 bytes each, 620 of them); CPickups::Load reads them back and, for every pickup that is not PICKUP_NONE and has a
// non-null m_pObject, sets m_pObject = nullptr and bVisible = false. The only pointer is `CObject* m_pObject` @4: 4 bytes on the exe, 8 on an LP64 host (CPickup 0x28 there, every
// later field shifted). Only "was it non-null" matters after a load, so the record keeps a u32 that is 0/1 on write and is never dereferenced.
#include <cstddef>
#include <cstdint>

struct CPickup_File {
    float    revenueValue;   // 0x00 m_fRevenueValue
    uint32_t object;         // 0x04 CObject* m_pObject : 0 / 1 (live object present) when written by the port; the exe wrote the raw pointer (only null / non-null is ever read back)
    uint32_t ammo;           // 0x08 m_nAmmo
    uint32_t regenerationTime; // 0x0C m_nRegenerationTime
    int16_t  pos[3];         // 0x10 m_vecPos (CompressedLargeVector: int16 x,y,z, scale 8)
    uint16_t moneyPerDay;    // 0x16
    int16_t  modelIndex;     // 0x18
    int16_t  referenceIndex; // 0x1A
    uint8_t  pickupType;     // 0x1C ePickupType (uint8)
    uint8_t  flags;          // 0x1D bDisabled:0 bEmpty:1 bHelpMessageDisplayed:2 bVisible:3 nPropertyTextIndex:4..6 (bit 7 unused, written 0)
    uint8_t  pad1E[2];       // 0x1E written 0 (the exe wrote whatever the struct padding held)
};
static_assert(sizeof(CPickup_File) == 0x20 && alignof(CPickup_File) == 4, "Invalid structure size of CPickup_File"); // == VALIDATE_SIZE_FILE (this header is StdInc-free)
static_assert(offsetof(CPickup_File, object) == 0x04 && offsetof(CPickup_File, ammo) == 0x08 && offsetof(CPickup_File, regenerationTime) == 0x0C && offsetof(CPickup_File, pos) == 0x10
           && offsetof(CPickup_File, moneyPerDay) == 0x16 && offsetof(CPickup_File, modelIndex) == 0x18 && offsetof(CPickup_File, referenceIndex) == 0x1A
           && offsetof(CPickup_File, pickupType) == 0x1C && offsetof(CPickup_File, flags) == 0x1D);

// bit positions of CPickup::m_nFlags in the file byte (LSB first; same on MSVC x86 and clang arm64 for uint8 bit-fields: verified by layout_diff.py and the host test)
inline constexpr uint8_t kCPickupFlagDisabled = 1u << 0, kCPickupFlagEmpty = 1u << 1, kCPickupFlagHelpDisplayed = 1u << 2, kCPickupFlagVisible = 1u << 3;
inline constexpr unsigned kCPickupPropertyTextShift = 4, kCPickupPropertyTextMask = 0x7;
