#include "StdInc.h"

#include "BreakablePlugin.h"

void BreakablePlugin::InjectHooks() {
    RH_ScopedClass(BreakablePlugin);
    RH_ScopedCategory("Plugins");

    RH_ScopedGlobalInstall(BreakableConstructor, 0x59CD70);
    RH_ScopedGlobalInstall(BreakableDestructor, 0x59CD90);
    RH_ScopedGlobalInstall(BreakablePluginAttach, 0x59D100);
    RH_ScopedGlobalInstall(BreakableStreamRead, 0x59CEC0);
    RH_ScopedGlobalInstall(BreakableStreamWrite, 0x59CDE0);
    RH_ScopedGlobalInstall(BreakableStreamGetSize, 0x59D0F0);
}

// 0x59D100
bool BreakablePluginAttach() {
    g_BreakablePlugin = RpGeometryRegisterPlugin(sizeof(BreakablePlugin), rwID_BREAKABLEPLUGIN, BreakableConstructor, BreakableDestructor, nullptr);
    if (g_BreakablePlugin == -1) {
        NOTSA_LOG_DEBUG("Failed to register Breakable Plugin");
        return false;
    }

    auto registerResult = RpGeometryRegisterPluginStream(rwID_BREAKABLEPLUGIN, BreakableStreamRead, BreakableStreamWrite, BreakableStreamGetSize);
    if (registerResult < 0) {
        g_BreakablePlugin = -1;
        NOTSA_LOG_DEBUG("Failed to register Breakable Plugin Stream");
        return false;
    }

    return true;
}

// 0x59CD70
void* BreakableConstructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    if (g_BreakablePlugin <= 0)
        return object;

    auto* plugin = RWPLUGINOFFSET(BreakablePlugin, object, g_BreakablePlugin);
    plugin->m_pBreakableInfo = nullptr;
    return object;
}

// 0x59CD90
void* BreakableDestructor(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    if (g_BreakablePlugin <= 0)
        return object;

    auto* plugin = RWPLUGINOFFSET(BreakablePlugin, object, g_BreakablePlugin);
    if (!plugin->m_pBreakableInfo)
        return object;

    for (auto& texture : plugin->GetTextures()) {
        if (texture) {
            RwTextureDestroy(texture);
        }
    }
#ifdef NOTSA_PORTABLE
    // P-2i: the blob came from `operator new(allocSize)` (BreakableStreamRead); `delete BreakInfo_t*` would be a sized delete with the wrong size (UB, ASan new-delete-type-mismatch)
    ::operator delete(plugin->m_pBreakableInfo);
#else
    delete plugin->m_pBreakableInfo;
#endif
    return object;
}

// 0x59CEC0
RwStream* BreakableStreamRead(RwStream* stream, int binaryLength, void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    auto* plugin = RWPLUGINOFFSET(BreakablePlugin, object, g_BreakablePlugin);
#ifdef NOTSA_PORTABLE
    // P-2i: the 4 header bytes go into a local (the plugin slot is a pointer-sized union: on LP64 the read would fill only its low half), and BreakInfo_t is read through
    // its 0x34-byte file record (the in-memory struct is 0x58 bytes there); the 9 pointer slots of the file are garbage and re-pointed below anyway.
    uint32 sectionHeader = 0;
    RwStreamRead(stream, &sectionHeader, sizeof(sectionHeader));
    if (!sectionHeader)
        return stream;

    BreakInfo_File infoFile;
    RwStreamRead(stream, &infoFile, sizeof(infoFile));
    BreakInfo_t infoBuffer;
    notsa::FileRecord<BreakInfo_t>::FromFile(infoBuffer, infoFile);
#else
    RwStreamRead(stream, &plugin->m_SectionHeader, 4u);
    if (!plugin->m_SectionHeader)
        return stream;

    BreakInfo_t infoBuffer;
    RwStreamRead(stream, &infoBuffer, sizeof(BreakInfo_t));
#endif

    auto allocSize = BreakablePluginData::GetRequiredAllocationSize(infoBuffer);
    auto* data = reinterpret_cast<BreakablePluginData*>(operator new(allocSize)); // Allocated on heap dynamically sized BreakablePluginData

    plugin->m_pBreakableInfo = &data->m_Info;
    data->m_Info = infoBuffer;

    data->m_Info.m_pVertexPos = data->GetVertexPosPtr();
    RwStreamRead(stream, data->GetVertexPosPtr(), sizeof(RwV3d) * data->m_Info.m_usNumVertices);

    data->m_Info.m_pTexCoors = data->GetTexCoordsPtr();
    RwStreamRead(stream, data->GetTexCoordsPtr(), sizeof(RwTexCoords) * data->m_Info.m_usNumVertices);

    data->m_Info.m_pVertexColors = data->GetVertColorsPtr();
    RwStreamRead(stream, data->GetVertColorsPtr(), sizeof(RwRGBA) * data->m_Info.m_usNumVertices);

    data->m_Info.m_pTriangles = data->GetTrianglesPtr();
    RwStreamRead(stream, data->GetTrianglesPtr(), sizeof(BreakInfoTriangle) * data->m_Info.m_usNumTriangles);

    data->m_Info.m_pTrianglesMaterialIndices = data->GetMaterialIndicesPtr();
    RwStreamRead(stream, data->GetMaterialIndicesPtr(), sizeof(uint16) * data->m_Info.m_usNumTriangles);

    data->m_Info.m_pTextureNames = data->GetTextureNamesPtr();
    RwStreamRead(stream, data->GetTextureNamesPtr(), sizeof(char[32]) * data->m_Info.m_usNumMaterials);

    data->m_Info.m_pMaskNames = data->GetMaskNamesPtr();
    RwStreamRead(stream, data->GetMaskNamesPtr(), sizeof(char[32]) * data->m_Info.m_usNumMaterials);

    data->m_Info.m_pMaterialProperties = data->GetSurfacePropsPtr();
    RwStreamRead(stream, data->GetSurfacePropsPtr(), sizeof(BreakInfoColor) * data->m_Info.m_usNumMaterials);

    data->m_Info.m_pTextures = data->GetTexturesPtr();
    for (auto i = 0; i < data->m_Info.m_usNumMaterials; ++i) {
        auto charOffset = i * 32;
        data->m_Info.m_pTextures[i] = RwTextureRead(
            &data->m_Info.m_pTextureNames[charOffset],
            data->m_Info.m_pMaskNames[charOffset] ? &data->m_Info.m_pMaskNames[charOffset] : nullptr
        );
    }

    return stream;
}

// 0x59CDE0
RwStream* BreakableStreamWrite(RwStream* stream, RwInt32 binaryLength, const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    auto* plugin = RWPLUGINOFFSET(BreakablePlugin, object, g_BreakablePlugin);
#ifdef NOTSA_PORTABLE
    // P-2i: header = "info follows" flag in a local uint32 (the exe wrote the low 4 bytes of the pointer; the reader only tests for non-zero, and a 64-bit pointer's low half can be 0)
    const uint32 sectionHeader = plugin->m_pBreakableInfo ? 1u : 0u;
    RwStreamWrite(stream, &sectionHeader, sizeof(sectionHeader));
#else
    RwStreamWrite(stream, plugin, 4u);
#endif

    if (plugin->m_pBreakableInfo) {
        auto* info = plugin->m_pBreakableInfo;
#ifdef NOTSA_PORTABLE
        const BreakInfo_File infoFile = notsa::FileRecord<BreakInfo_t>::ToFile(*info);
        RwStreamWrite(stream, &infoFile, sizeof(infoFile));
#else
        RwStreamWrite(stream, info, sizeof(BreakInfo_t));
#endif
        RwStreamWrite(stream, info->m_pVertexPos, sizeof(RwV3d) * info->m_usNumVertices);
        RwStreamWrite(stream, info->m_pTexCoors, sizeof(RwTexCoords) * info->m_usNumVertices);
        RwStreamWrite(stream, info->m_pVertexColors, sizeof(RwRGBA) * info->m_usNumVertices);
        RwStreamWrite(stream, info->m_pTriangles, sizeof(BreakInfoTriangle) * info->m_usNumTriangles);
        RwStreamWrite(stream, info->m_pTrianglesMaterialIndices, sizeof(uint16) * info->m_usNumTriangles);
        RwStreamWrite(stream, info->m_pTextureNames, sizeof(char[32]) * info->m_usNumMaterials);
        RwStreamWrite(stream, info->m_pMaskNames, sizeof(char[32]) * info->m_usNumMaterials);
        RwStreamWrite(stream, info->m_pMaterialProperties, sizeof(BreakInfoColor) * info->m_usNumMaterials);
    }

    return stream;
}

// 0x59D0F0
RwInt32 BreakableStreamGetSize(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    return sizeof(BreakInfo_File); // == sizeof(BreakInfo_t) on x86 (static_assert in BreakablePlugin.h)
}
