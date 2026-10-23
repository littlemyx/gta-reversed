#pragma once
// P-2f: on-disk (x86 exe) layout of tRadarTrace (CRadar::Save / CRadar::Load dump every ms_RadarTrace element raw, 0x28 bytes each). Standalone POD header (no StdInc).
//
// The exe record holds `CEntryExit* m_pEntryExit` @0x20 (a union with the uint32 pool index+1 that CRadar::Save writes in place for the duration of the copy); on an LP64 host the
// union is 8 bytes and every later field shifts. The file stores ONLY the pool index+1 (0 = no enex); the pointer view never reaches a file.
// The bit-field tail is two bytes (@0x25, @0x26); they are packed by hand below (LSB first, as on MSVC x86 and clang arm64: proved by tools/portable/layout_diff.py).
#include <cstddef>
#include <cstdint>

struct tRadarTrace_File {
    uint32_t colour;           // 0x00 eBlipColour (uint32)
    uint32_t entityHandle;     // 0x04 m_nEntityHandle
    float    pos[3];           // 0x08 m_vPosition
    uint16_t counter;          // 0x14 m_nCounter
    uint8_t  pad16[2];         // 0x16 written 0
    float    sphereRadius;     // 0x18 m_fSphereRadius
    uint16_t blipSize;         // 0x1C m_nBlipSize
    uint8_t  pad1E[2];         // 0x1E written 0
    uint32_t entryExitPoolInd; // 0x20 CEntryExit pool index + 1, 0 = none (the exe wrote this union slot in place)
    int8_t   blipSprite;       // 0x24 eRadarSprite (int8)
    uint8_t  flags0;           // 0x25 bBright:0 bTrackingBlip:1 bShortRange:2 bFriendly:3 bBlipRemain:4 bBlipFade:5 nCoordBlipAppearance:6..7
    uint8_t  flags1;           // 0x26 nBlipDisplayFlag:0..1 nBlipType:2..5 nAppearance:6..7
    uint8_t  pad27;            // 0x27 written 0
};
static_assert(sizeof(tRadarTrace_File) == 0x28 && alignof(tRadarTrace_File) == 4, "Invalid structure size of tRadarTrace_File"); // == VALIDATE_SIZE_FILE (this header is StdInc-free)
static_assert(offsetof(tRadarTrace_File, entityHandle) == 0x04 && offsetof(tRadarTrace_File, pos) == 0x08 && offsetof(tRadarTrace_File, counter) == 0x14
           && offsetof(tRadarTrace_File, sphereRadius) == 0x18 && offsetof(tRadarTrace_File, blipSize) == 0x1C && offsetof(tRadarTrace_File, entryExitPoolInd) == 0x20
           && offsetof(tRadarTrace_File, blipSprite) == 0x24 && offsetof(tRadarTrace_File, flags0) == 0x25 && offsetof(tRadarTrace_File, flags1) == 0x26);

// bit positions in the two file bytes
inline constexpr uint8_t kRadarFlagBright = 1u << 0, kRadarFlagTracking = 1u << 1, kRadarFlagShortRange = 1u << 2, kRadarFlagFriendly = 1u << 3, kRadarFlagBlipRemain = 1u << 4, kRadarFlagBlipFade = 1u << 5;
inline constexpr unsigned kRadarCoordAppearanceShift = 6, kRadarCoordAppearanceMask = 0x3;                                          // flags0
inline constexpr unsigned kRadarDisplayShift = 0, kRadarDisplayMask = 0x3, kRadarTypeShift = 2, kRadarTypeMask = 0xF, kRadarAppearanceShift = 6, kRadarAppearanceMask = 0x3; // flags1
