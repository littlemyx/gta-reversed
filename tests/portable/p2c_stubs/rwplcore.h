#pragma once
// P-2c host-test stub of the RenderWare types the headers use (layouts of the real RwV3d / RwV2d / RwMatrix: 12 / 8 / 0x40 bytes).
#include <cstdint>
struct RwV2d { float x, y; };
struct RwV3d { float x, y, z; };
struct RwMatrix { RwV3d right; uint32_t flags; RwV3d up; uint32_t pad1; RwV3d at; uint32_t pad2; RwV3d pos; uint32_t pad3; };
static_assert(sizeof(RwMatrix) == 0x40);
struct RwV4d { float x, y, z, w; };
struct RtQuat { RwV3d imag; float real; };
