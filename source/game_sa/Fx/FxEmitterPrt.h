#pragma once

#include "ListItem_c.h"
#include "Vector.h"
#include "Particle.h"
#include "RGBA.h"
#include "extensions/FixedFloat.hpp"

#ifndef NOTSA_RW_LIBRW // fakerw: the RW names are aliases of rw::* types, they cannot be forward-declared as structs
struct RwRGBA;
#endif
class FxSystem_c;

//! A particle created by `FxEmitterBP_c` / `FxEmitter_c`
//! The list item (prev/next) links are already part of `Particle_c`, there's no second `ListItem_c` base
//! (See `FxEmitter_c::CreateParticle` (0x4A2580) for the layout)
//!
//! NOTSA: The fixed-point fields are stored as raw bytes (not `FixedFloat`),
//!        because the original code does integer / `1/255` multiplication math on them,
//!        which can't be reproduced bit-exactly with `FixedFloat`'s division.
class FxEmitterPrt_c : public Particle_c {
public:
    CRGBA  m_MultColor;       // 0x2C - R, G, B, A multipliers (255 = unchanged)
    uint8  m_MultSize;        // 0x30 - Size multiplier,     255 => 1.0
    uint8  m_MultRot;         // 0x31 - Rotation multiplier, 255 => 1.0
    uint8  m_RandR;           // 0x32 - Random values [0, 255] (Used to vary the color/size/rotation)
    uint8  m_RandG;           // 0x33
    uint8  m_RandB;           // 0x34
    uint8  m_Brightness;      // 0x35 - Light multiplier,    100 => 1.0
    uint8  m_RotZ;            // 0x36 - Fixed rotation (degrees / 2), 255 => not set
    bool   m_bLocalToSystem;  // 0x37
    float  m_CurrentRotation; // 0x38 - In degrees

public:
    FxEmitterPrt_c() = default;  // 0x4A94E0
    ~FxEmitterPrt_c() = default; // 0x4A94F0

    static void InjectHooks();

    static void* operator new[](size_t size);
};
VALIDATE_SIZE(FxEmitterPrt_c, 0x3C);
VALIDATE_OFFSET(FxEmitterPrt_c, m_MultColor, 0x2C);
VALIDATE_OFFSET(FxEmitterPrt_c, m_bLocalToSystem, 0x37);
VALIDATE_OFFSET(FxEmitterPrt_c, m_CurrentRotation, 0x38);
