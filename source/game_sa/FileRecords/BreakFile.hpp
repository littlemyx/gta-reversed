#pragma once
// P-2i: on-disk (x86 exe) layout of BreakInfo_t, the header of the Breakable plugin (rwID 0xFD) chunk of a DFF geometry extension.
// Standalone POD header (no StdInc): usable from tests/portable/p2i_*.cpp.
//
// Chunk body (BreakablePlugin.cpp BreakableStreamRead/Write):
//   u32 sectionHeader                      0 => nothing follows (16010 of 16264 chunks in the Steam gta3.img); non-zero => the rest follows. The exe wrote the plugin slot raw (a pointer value).
//   BreakInfo_File (0x34)                  the 9 pointer slots are file garbage; the reader ignores them and re-points them into the trailing blob
//   RwV3d     pos[numVertices]             12 B each
//   RwTexCoords uv[numVertices]            8 B
//   RwRGBA    rgba[numVertices]            4 B
//   BreakInfoTriangle tri[numTriangles]    6 B (3 x u16)
//   u16       triMaterial[numTriangles]    2 B
//   char      textureNames[numMaterials][32]
//   char      maskNames[numMaterials][32]
//   BreakInfoColor props[numMaterials]     12 B (3 floats)
// => chunk length = 4 + 0x34 + 24*numVertices + 8*numTriangles + 76*numMaterials (checked against every 0xFD chunk of gta3.img by tests/portable/p2i_breakable_test.cpp).
#include <cstddef>
#include <cstdint>

struct BreakInfo_File {
    uint32_t posRule;          // 0x00 eBreakablePluginPositionRule
    uint16_t numVertices;      // 0x04
    uint16_t pad06;            // 0x06 written 0 (the exe wrote struct padding)
    uint32_t pVertexPos;       // 0x08 pointer slot: written 0, ignored on read
    uint32_t pTexCoords;       // 0x0C pointer slot
    uint32_t pVertexColors;    // 0x10 pointer slot
    uint16_t numTriangles;     // 0x14
    uint16_t pad16;            // 0x16
    uint32_t pTriangles;       // 0x18 pointer slot
    uint32_t pTriMaterials;    // 0x1C pointer slot
    uint16_t numMaterials;     // 0x20
    uint16_t pad22;            // 0x22
    uint32_t pTextures;        // 0x24 pointer slot
    uint32_t pTextureNames;    // 0x28 pointer slot
    uint32_t pMaskNames;       // 0x2C pointer slot
    uint32_t pMaterialProps;   // 0x30 pointer slot
};
static_assert(sizeof(BreakInfo_File) == 0x34 && alignof(BreakInfo_File) == 4, "Invalid structure size of BreakInfo_File"); // == VALIDATE_SIZE_FILE (this header is StdInc-free)
static_assert(offsetof(BreakInfo_File, numVertices) == 0x04 && offsetof(BreakInfo_File, pVertexPos) == 0x08 && offsetof(BreakInfo_File, pTexCoords) == 0x0C
           && offsetof(BreakInfo_File, pVertexColors) == 0x10 && offsetof(BreakInfo_File, numTriangles) == 0x14 && offsetof(BreakInfo_File, pTriangles) == 0x18
           && offsetof(BreakInfo_File, pTriMaterials) == 0x1C && offsetof(BreakInfo_File, numMaterials) == 0x20 && offsetof(BreakInfo_File, pTextures) == 0x24
           && offsetof(BreakInfo_File, pTextureNames) == 0x28 && offsetof(BreakInfo_File, pMaskNames) == 0x2C && offsetof(BreakInfo_File, pMaterialProps) == 0x30);
