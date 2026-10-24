#pragma once

#include "FileRecord.hpp"
#include "FileRecords/BreakFile.hpp"

/**
 * Breakable plugin unique rwID
 */
#define rwID_BREAKABLEPLUGIN MAKECHUNKID(rwVENDORID_DEVELOPER, 0xFD)

/*
 * Breakable plugin static offset
 */
NOTSA_GLOBAL_HDR(g_BreakablePlugin, 0xBB4238, (int32), {});

enum eBreakablePluginPositionRule : uint32 {
    OBJECT_ORIGIN    = 0,
    COLLISION_ORIGIN = 1,
};

struct BreakInfoTriangle {
    std::array<uint16, 3> vertIndex;
};

struct BreakInfoColor {
    float red;
    float green;
    float blue;
};
VALIDATE_SIZE_FILE(BreakInfoColor, 0xC);

// RW PLUGIN
struct BreakInfo_t {
    eBreakablePluginPositionRule m_uiPosRule;
    uint16                       m_usNumVertices;
           RwV3d*                m_pVertexPos;
           RwTexCoords*          m_pTexCoors;
           RwRGBA*               m_pVertexColors;

    uint16                       m_usNumTriangles;
    struct BreakInfoTriangle*    m_pTriangles;
    uint16*                      m_pTrianglesMaterialIndices;

    uint16                       m_usNumMaterials;
           RwTexture**           m_pTextures;
    char*                        m_pTextureNames;
    char*                        m_pMaskNames;
    struct BreakInfoColor*       m_pMaterialProperties;
};
VALIDATE_SIZE(BreakInfo_t, 0x34); // x86 in-memory (9 pointers widen it to 0x60 on LP64); the on-disk 0x34 is VALIDATE_SIZE_FILE(BreakInfo_File) in FileRecords/BreakFile.hpp

// P-2i: x86 equivalence proof, compiled in the 32-bit build only: the raw BreakInfo_t read/written by the exe is bit-for-bit the record.
static_assert(sizeof(void*) != 4 || (sizeof(BreakInfo_t) == sizeof(BreakInfo_File)
    && offsetof(BreakInfo_t, m_uiPosRule) == offsetof(BreakInfo_File, posRule) && offsetof(BreakInfo_t, m_usNumVertices) == offsetof(BreakInfo_File, numVertices)
    && offsetof(BreakInfo_t, m_pVertexPos) == offsetof(BreakInfo_File, pVertexPos) && offsetof(BreakInfo_t, m_pTexCoors) == offsetof(BreakInfo_File, pTexCoords)
    && offsetof(BreakInfo_t, m_pVertexColors) == offsetof(BreakInfo_File, pVertexColors) && offsetof(BreakInfo_t, m_usNumTriangles) == offsetof(BreakInfo_File, numTriangles)
    && offsetof(BreakInfo_t, m_pTriangles) == offsetof(BreakInfo_File, pTriangles) && offsetof(BreakInfo_t, m_pTrianglesMaterialIndices) == offsetof(BreakInfo_File, pTriMaterials)
    && offsetof(BreakInfo_t, m_usNumMaterials) == offsetof(BreakInfo_File, numMaterials) && offsetof(BreakInfo_t, m_pTextures) == offsetof(BreakInfo_File, pTextures)
    && offsetof(BreakInfo_t, m_pTextureNames) == offsetof(BreakInfo_File, pTextureNames) && offsetof(BreakInfo_t, m_pMaskNames) == offsetof(BreakInfo_File, pMaskNames)
    && offsetof(BreakInfo_t, m_pMaterialProperties) == offsetof(BreakInfo_File, pMaterialProps)),
    "BreakInfo_File != x86 BreakInfo_t");

// P-2i: file layout (0x34). ToFile: pointer slots and padding are written 0 (the exe wrote the live pointers / padding garbage; the reader never looks at them).
// FromFile: counts and the position rule only; every pointer is null until BreakableStreamRead re-points it into the trailing blob.
template<> struct notsa::FileRecord<BreakInfo_t> {
    using Type = BreakInfo_File;
    static Type ToFile(const BreakInfo_t& i) {
        Type f{};
        f.posRule      = (uint32)i.m_uiPosRule;
        f.numVertices  = i.m_usNumVertices;
        f.numTriangles = i.m_usNumTriangles;
        f.numMaterials = i.m_usNumMaterials;
        return f;
    }
    static void FromFile(BreakInfo_t& i, const Type& f) {
        i = {};
        i.m_uiPosRule      = (eBreakablePluginPositionRule)f.posRule;
        i.m_usNumVertices  = f.numVertices;
        i.m_usNumTriangles = f.numTriangles;
        i.m_usNumMaterials = f.numMaterials;
    }
};

struct BreakablePlugin {
    union {
        uint32       m_SectionHeader;
        BreakInfo_t* m_pBreakableInfo;
    };

    static void InjectHooks();

    auto GetTextures() { return std::span { m_pBreakableInfo->m_pTextures, m_pBreakableInfo->m_usNumMaterials }; }
};
VALIDATE_SIZE(BreakablePlugin, 0x4); // x86 in-memory plugin slot; 8 on LP64 (the stream code reads the 4 header bytes into a local, never into the slot: P-2i)

struct BreakablePluginData {
    BreakInfo_t m_Info;

    // Dynamically allocated size, depending on loaded model sizes]
    // RwV3d               m_aVertexPos[m_usNumVertices];
    // RwTexCoords         m_aTexCoors[m_usNumVertices];
    // RwRGBA              m_aVertexColors[m_usNumVertices];
    // BreakInfoTriangle   m_aTriangleVertIndices[m_usNumTriangles];
    // uint16              m_aTriangleMaterialIndices[m_usNumTriangles];
    // char                m_aTextureNames[m_usNumMaterials][32];
    // char                m_aMaskNames[m_usNumMaterials][32];
    // BreakInfoColor      m_aMaterialProperties[m_usNumMaterial];
    // RwTexture*          m_aTexturePointers[m_usNumMaterial];   (P-2i: on a 64-bit target preceded by padding so that the pointers are naturally aligned)

// NOTSA: Helper functions
    //! P-2i: the RwTexture* array follows BreakInfoColor[n] (12 B each) => on a 64-bit target the 8-byte pointers would be misaligned. 1 (no pad) on the 32-bit build, so the
    //! rounding below folds away and the x86 code is unchanged (a helper function would add an out-of-line COMDAT to the object).
    static constexpr size_t kTexturesAlign = sizeof(void*) > 4 ? alignof(RwTexture*) : 1;

    static size_t GetRequiredAllocationSize(const BreakInfo_t& baseInfo) {
        auto size = sizeof(BreakInfo_t);
        size += sizeof(RwV3d) * baseInfo.m_usNumVertices;
        size += sizeof(RwTexCoords) * baseInfo.m_usNumVertices;
        size += sizeof(RwRGBA) * baseInfo.m_usNumVertices;
        size += sizeof(BreakInfoTriangle) * baseInfo.m_usNumTriangles;
        size += sizeof(uint16) * baseInfo.m_usNumTriangles;
        size += sizeof(char[32]) * baseInfo.m_usNumMaterials;
        size += sizeof(char[32]) * baseInfo.m_usNumMaterials;
        size += sizeof(BreakInfoColor) * baseInfo.m_usNumMaterials;
        size = (size + kTexturesAlign - 1) & ~(kTexturesAlign - 1);
        size += sizeof(RwTexture*) * baseInfo.m_usNumMaterials;
        return size;
    };

    auto GetVertexPosOffset()       { return sizeof(BreakInfo_t); };
    auto GetTexCoordsOffset()       { return GetVertexPosOffset()       + sizeof(RwV3d)             * m_Info.m_usNumVertices; };
    auto GetVertColorsOffset()      { return GetTexCoordsOffset()       + sizeof(RwTexCoords)       * m_Info.m_usNumVertices; };
    auto GetTrianglesOffset()       { return GetVertColorsOffset()      + sizeof(RwRGBA)            * m_Info.m_usNumVertices; };
    auto GetMaterialIndicesOffset() { return GetTrianglesOffset()       + sizeof(BreakInfoTriangle) * m_Info.m_usNumTriangles; };
    auto GetTextureNamesOffset()    { return GetMaterialIndicesOffset() + sizeof(uint16)            * m_Info.m_usNumTriangles; };
    auto GetMaskNamesOffset()       { return GetTextureNamesOffset()    + sizeof(char[32])          * m_Info.m_usNumMaterials; };
    auto GetSurfacePropsOffset()    { return GetMaskNamesOffset()       + sizeof(char[32])          * m_Info.m_usNumMaterials; };
    auto GetTexturesOffset()        { return (GetSurfacePropsOffset() + sizeof(BreakInfoColor) * m_Info.m_usNumMaterials + kTexturesAlign - 1) & ~(kTexturesAlign - 1); };

    template <typename T>
    auto As(auto byteOffset) {
        return reinterpret_cast<T>(&reinterpret_cast<uint8*>(this)[byteOffset]);
    }

    auto GetVertexPosPtr()       { return As<RwV3d*>(GetVertexPosOffset()); }
    auto GetTexCoordsPtr()       { return As<RwTexCoords*>(GetTexCoordsOffset()); }
    auto GetVertColorsPtr()      { return As<RwRGBA*>(GetVertColorsOffset()); }
    auto GetTrianglesPtr()       { return As<BreakInfoTriangle*>(GetTrianglesOffset()); }
    auto GetMaterialIndicesPtr() { return As<uint16*>(GetMaterialIndicesOffset()); }
    auto GetTextureNamesPtr()    { return As<char*>(GetTextureNamesOffset()); }
    auto GetMaskNamesPtr()       { return As<char*>(GetMaskNamesOffset()); }
    auto GetSurfacePropsPtr()    { return As<BreakInfoColor*>(GetSurfacePropsOffset()); }
    auto GetTexturesPtr()        { return As<RwTexture**>(GetTexturesOffset()); }
};

#define BREAKABLEPLG(geometry, var) \
    (RWPLUGINOFFSET(BreakablePlugin, geometry, g_BreakablePlugin)->var)

#define BREAKABLECONSTPLG(geometry, var) \
    (RWPLUGINOFFSETCONST(BreakablePlugin, geometry, g_BreakablePlugin)->var)

bool BreakablePluginAttach();

void* BreakableConstructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
void* BreakableDestructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);

RwStream* BreakableStreamRead(RwStream* stream, int binaryLength, void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
RwStream* BreakableStreamWrite(RwStream* stream, RwInt32 binaryLength, const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
RwInt32 BreakableStreamGetSize(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
