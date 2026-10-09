#include "StdInc.h"

#include "CustomBuildingDNPipeline.h"
#include "CustomCarEnvMapPipeline.h"
#include "PipelinesCommon.hpp"

auto& s_Magic1 = StaticRef<uint32>(0xC02C14);
auto& s_Magic2 = StaticRef<uint32>(0xC02C18);

// 0x5D72E0
bool CCustomBuildingDNPipeline::ExtraVertColourPluginAttach() {
    if ((ms_extraVertColourPluginOffset = RpGeometryRegisterPlugin(
             sizeof(ExtraVertColour),
             rwID_EXTRAVERCOLOURPLUGIN,
             pluginExtraVertColourConstructorCB,
             pluginExtraVertColourDestructorCB,
             nullptr
         )) == -1
    ) {
        return false;
    }

    if (RpGeometryRegisterPluginStream(
            rwID_EXTRAVERCOLOURPLUGIN,
            pluginExtraVertColourStreamReadCB,
            pluginExtraVertColourStreamWriteCB,
            pluginExtraVertColourStreamGetSizeCB
        ) == -1
    ) {
        ms_extraVertColourPluginOffset = -1;
        return false;
    }

    return true;
}

// 0x5D6D10
void* CCustomBuildingDNPipeline::pluginExtraVertColourConstructorCB(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const auto self = GetExtraVertColourPtr(object);

    self->NightColors = nullptr;
    self->DayColors   = nullptr;
    self->DNBalance   = 0.f;

    return object;
}

// 0x5D6D30
void* CCustomBuildingDNPipeline::pluginExtraVertColourDestructorCB(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const auto self = GetExtraVertColourPtr(object);

    for (const auto ptr : { &self->NightColors, &self->DayColors }) {
        CMemoryMgr::Free(std::exchange(*ptr, nullptr));
    }

    return object;
}

// 0x5D6DE0
RwStream* CCustomBuildingDNPipeline::pluginExtraVertColourStreamReadCB(
    RwStream* stream,
    RwInt32   binaryLength,
    void*     object,
    RwInt32   offsetInObject,
    RwInt32   sizeInObject
) {
    const auto self = GetExtraVertColourPtr(object);
    const auto geo  = static_cast<RpGeometry*>(object);

    uint32 magicNumber;
    VERIFY(RwStreamRead(stream, &magicNumber, sizeof(magicNumber)) != 0);

    if (magicNumber) {
        const auto numVerts = (size_t)RpGeometryGetNumVertices(geo);

        for (const auto ptr : { &self->NightColors, &self->DayColors }) {
            *ptr = static_cast<RwRGBA*>(CMemoryMgr::Malloc(sizeof(RwRGBA) * numVerts));
        }
        self->DNBalance = 1.f;

        VERIFY(RwStreamRead(stream, self->NightColors, sizeof(RwRGBA) * numVerts) != 0);

        if (const auto preLitColors = RpGeometryGetPreLightColors(geo)) {
            rng::copy(std::span{ preLitColors, numVerts }, self->DayColors);
        }
    }

    return stream;
}

// 0x5D6D80
RwStream* CCustomBuildingDNPipeline::pluginExtraVertColourStreamWriteCB(RwStream* stream, RwInt32 binaryLength, const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const auto self = GetExtraVertColourPtr(object);
    const auto geo  = static_cast<const RpGeometry*>(object);

    RwStreamWrite(stream, &self->NightColors, sizeof(self->NightColors));
    if (self->NightColors) {
        RwStreamWrite(stream, self->NightColors, sizeof(RwRGBA) * RpGeometryGetNumVertices(geo));
    }
    return stream;
}

// 0x5D6DC0
RwInt32 CCustomBuildingDNPipeline::pluginExtraVertColourStreamGetSizeCB(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject) {
    const auto geo = static_cast<const RpGeometry*>(object);

    return 2 * sizeof(RwRGBA) * RpGeometryGetNumVertices(geo) /*Day and Night colors*/ + sizeof(ExtraVertColour::DNBalance);
}

// 0x5D5FC0
bool AtomicHasNVCPipeline(RpAtomic* atomic) {
    return atomic->pipeline == CCustomBuildingDNPipeline::ObjPipeline;
}

// 0x5D6850 [AKA `NVC__Process`]
// Update atomic's geometry's pre-lit vertices according
void NVCPipelineProcess(RpAtomic* a, float DNBalance) {
    auto* geo = RpAtomicGetGeometry(a);
    auto* const data = CCustomBuildingDNPipeline::GetExtraVertColourPtr(geo);

    data->DNBalance = DNBalance;
    DNBalance = std::clamp(DNBalance, 0.f, 1.f);

    geo = RpGeometryLock(geo, rpGEOMETRYLOCKPRELIGHT);
    auto* const prelit = RpGeometryGetPreLightColors(geo);
    for (int32 i = RpGeometryGetNumVertices(geo); i --> 0; ) {
        prelit[i] = lerp(data->DayColors[i], data->NightColors[i], DNBalance);
    }
    RpGeometryUnlock(geo);
}

// 0x5D7200
void CCustomBuildingDNPipeline::PreRenderUpdate(RpAtomic* a, bool ignoreDNBalanceParam) {
    if (!a) {
        return;
    }
    if (!AtomicHasNVCPipeline(a)) {
        return;
    }
    const auto intensity = std::abs(GetExtraVertColourPtr(RpAtomicGetGeometry(a))->DNBalance - m_fDNBalanceParam);
    if (intensity <= 0.01f && !ignoreDNBalanceParam) {
        return;
    }
    if (((uint32)a / 0x70) % 16 != s_Magic1 && !s_Magic2 && !ignoreDNBalanceParam && intensity <= 0.3f) {
        return;
    }
    NVCPipelineProcess(a, m_fDNBalanceParam);
}

// 0x5D72C0
void CCustomBuildingDNPipeline::PreRenderUpdate(RpClump* clump, bool ignoreDNBalanceParam) {
    RpClumpForAllAtomics(clump, [](RpAtomic* atomic, void* data) { // 0x5D72A0
        CCustomBuildingDNPipeline::PreRenderUpdate(atomic, *(bool*)(data));
        return atomic;
    }, &ignoreDNBalanceParam);
}

// 0x5D7100
bool CCustomBuildingDNPipeline::CreatePipe() {
    ObjPipeline = CreateCustomObjPipe();
    return ObjPipeline != nullptr;
}

// 0x5D6750
RxPipeline* CCustomBuildingDNPipeline::CreateCustomObjPipe() {
    auto pipeline = RxPipelineCreate();
    auto nodeDef  = RxNodeDefinitionGetD3D9AtomicAllInOne();
    if (!pipeline) {
        return nullptr;
    }

    auto lockedPipe = RxPipelineLock(pipeline);
    if (!lockedPipe || !RxLockedPipeAddFragment(lockedPipe, 0, nodeDef, 0) || !RxLockedPipeUnlock(lockedPipe)) {
        RxPipelineDestroy(pipeline);
        return nullptr;
    }

    auto node = RxPipelineFindNodeByName(pipeline, nodeDef->name, nullptr, nullptr);
    if (*reinterpret_cast<int32*>(0xC02C20)) { // read only (0 by default)
        RxD3D9AllInOneSetInstanceCallBack(node, RxD3D9AllInOneGetInstanceCallBack(node));
        RxD3D9AllInOneSetReinstanceCallBack(node, CustomPipeInstanceCB);
    } else {
        RxD3D9AllInOneSetInstanceCallBack(node, RxD3D9AllInOneGetInstanceCallBack(node));
        RxD3D9AllInOneSetReinstanceCallBack(node, RxD3D9AllInOneGetReinstanceCallBack(node));
    }
    RxD3D9AllInOneSetRenderCallBack(node, CustomPipeRenderCB);

    RwCompatPipelinePluginId(pipeline) = CUSTOM_BUILDING_DN_PIPELINE_ID;
    pipeline->pluginData = CUSTOM_BUILDING_DN_PIPELINE_ID;

    return pipeline;
}

// 0x5D5FA0
void CCustomBuildingDNPipeline::DestroyPipe() {
    if (ObjPipeline) {
        RxPipelineDestroy(std::exchange(ObjPipeline, nullptr));
    }
}

// 0x5D63E0
RwBool CCustomBuildingDNPipeline::CustomPipeInstanceCB(void* object, RwResEntry* resEntry, RxD3D9AllInOneInstanceCallBack instanceCallback) {
    if (instanceCallback) {
        auto entry = *reinterpret_cast<RwResEntrySA*>(resEntry);
        return instanceCallback(object, &entry.header, true);
    }
    return true;
}

// 0x5D71C0
RpAtomic* CCustomBuildingDNPipeline::CustomPipeAtomicSetup(RpAtomic* atomic) {
    RpGeometryForAllMaterials(atomic->geometry, CustomPipeMaterialSetup, nullptr);
    RpD3D9GeometrySetUsageFlags(atomic->geometry, rpD3D9GEOMETRYUSAGE_DYNAMICPRELIT);
    atomic->pipeline = ObjPipeline;
    SetPipelineID(atomic, CUSTOM_BUILDING_DN_PIPELINE_ID);
    return atomic;
}

// android, unused
uint32 CCustomBuildingDNPipeline::UsesThisPipeline(RpAtomic* atomic) {
    return atomic->pipeline
        - ObjPipeline
        + (atomic->pipeline == ObjPipeline) // AtomicHasNVCPipeline	0x5D5FC0
        + ObjPipeline
        - atomic->pipeline;
}

// 0x5D7120
RpMaterial* CCustomBuildingDNPipeline::CustomPipeMaterialSetup(RpMaterial* material, void* data) {
    CCustomCarEnvMapPipeline::SetMaterialFlags(material, 0); // The whole dword (surfaceProps.specular) is cleared
    if (RpMatFXMaterialGetEffects(material) == rpMATFXEFFECTENVMAP) { // 0x812140
        if (auto* const envData = SetFxEnvTexture(&CCustomCarEnvMapPipeline::EnvMapPlGetData(material))) { // 0x5D9570
            auto* const tex = (MATFXD3D9ENVMAPGETDATA(material, rpSECONDPASS))->texture;
            envData->Texture = tex;
            if (tex) { // Linear filtering, wrapped addressing
                auto* const filterAddressing = reinterpret_cast<uint8*>(&tex->filterAddressing);
                filterAddressing[1] = 0x11;
                filterAddressing[0] = 2;
            }
        }
    }

    auto* const envData = CCustomCarEnvMapPipeline::EnvMapPlGetData(material);
    const RwUInt32 hasEnvMap = envData
        && std::bit_cast<uint8>(envData->Shininess) != 0 // raw, unsigned (`(float)raw / 255 != 0`)
        && envData->Texture;
    CCustomCarEnvMapPipeline::SetMaterialFlags(material, (CCustomCarEnvMapPipeline::GetMaterialFlags(material) & ~7u) | hasEnvMap);
    return material;
}

// 0x5D6480
void CCustomBuildingDNPipeline::CustomPipeRenderCB(RwResEntry* resEntry, void* object, uint8 type, uint32 rxGeoFlags) {
    // 0x7FE0A0 - `_rwD3D9RenderStateVertexAlphaEnable`
    const auto D3D9RenderStateVertexAlphaEnable = [](bool enable) {
        plugin::Call<0x7FE0A0, uint32>(enable);
    };

    // The texture transform matrix used by the env map stage, only the diagonal is ever set
    static auto& s_EnvMapTexMatrix = StaticRef<D3DMATRIX>(0xC02C28);

    _rwD3D9EnableClippingIfNeeded(object, type);

    DWORD isLightingEnabled = 0;
    RwD3D9GetRenderState(D3DRS_LIGHTING, &isLightingEnabled);

    const auto geoHasNoLighting = !isLightingEnabled && !(rxGeoFlags & rxGEOMETRY_PRELIT);
    if (geoHasNoLighting) {
        RwD3D9SetTexture(NULL, 0);
        RwD3D9SetRenderState(D3DRS_TEXTUREFACTOR, 0xFF000000);
        RwD3D9SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        RwD3D9SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
        RwD3D9SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
    }

    auto* const header = (RxD3D9ResEntryHeader*)(resEntry + 1);
    auto* const meshes = (RxD3D9InstanceData*)(header + 1);

    if (const auto i = header->indexBuffer) {
        RwD3D9SetIndices(i);
    }

    _rwD3D9SetStreams(header->vertexStream, header->useOffsets);
    RwD3D9SetVertexDeclaration(header->vertexDeclaration);

    for (int32 i = 0; i < (int32)header->numMeshes; i++) { // 0x5D654D
        auto* const mesh     = &meshes[i];
        auto* const mat      = mesh->material;
        const auto  matFlags = CCustomCarEnvMapPipeline::GetMaterialFlags(mat);
        const auto* const envMapData = CCustomCarEnvMapPipeline::EnvMapPlGetData(mat);

        RwD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        RwD3D9SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);

        if (matFlags & CCustomCarEnvMapPipeline::MF_HAS_SHINE_CAM) { // 0x5D657D
            RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, RWRSTATE(rwTEXTUREADDRESSWRAP));

            // Note: Unlike CCustomCarEnvMapPipeline, this matrix is not applied (D3DTS_TEXTURE1 isn't set) here
            s_EnvMapTexMatrix._11 = CCustomCarEnvMapPipeline::GetFxEnvScaleX(mat); // (float)(int8)raw * 0.125f
            s_EnvMapTexMatrix._44 = 1.f;
            s_EnvMapTexMatrix._33 = 1.f;
            s_EnvMapTexMatrix._22 = CCustomCarEnvMapPipeline::GetFxEnvScaleY(mat);

            RwD3D9SetTexture(envMapData->Texture, 1);

            // 0x5D65E9 - The whole expression is evaluated in extended precision, then truncated (_ftol)
            auto c = (int32)(int64)(((double)std::bit_cast<uint8>(envMapData->Shininess) /* raw, unsigned */ * (double)(1.f / 255.f)) * 254.0); // 0x859A3C, 0x86BE90
            if (c > 0xFF) {
                c = 0xFF;
            }
            RwD3D9SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(0xFF, (uint8)c, (uint8)c, (uint8)c));

            RwD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);
            RwD3D9SetTextureStageState(1, D3DTSS_COLORARG0, D3DTA_CURRENT);
            RwD3D9SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            RwD3D9SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

            RwD3D9SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
            RwD3D9SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            RwD3D9SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);

            RwD3D9SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1 | D3DTSS_TCI_CAMERASPACENORMAL);
            RwD3D9SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

            RwD3D9SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
        }

        // 0x5D669E
        D3D9RenderStateVertexAlphaEnable(mesh->vertexAlpha || mesh->material->color.alpha != 0xFF);

        if (geoHasNoLighting) { // 0x5D66FB - Render without lighting
            RxD3D9InstanceDataRender(header, mesh);
        } else { // 0x5D66C4 - Render with lighting
            if (isLightingEnabled) {
                RwD3D9SetSurfaceProperties(&mesh->material->surfaceProps, &mesh->material->color, rxGeoFlags);
            }
            RxD3D9InstanceDataRenderLighting(header, mesh, rxGeoFlags, mesh->material->texture);
        }
    }

    RwD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    RwD3D9SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    RwD3D9SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
    RwD3D9SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, NULL);
}

// 0x5D6E90
ExtraVertColour* CCustomBuildingDNPipeline::GetExtraVertColourPtr(const void* geometry) {
    return RWPLUGINOFFSET(ExtraVertColour, geometry, ms_extraVertColourPluginOffset);
}

// 0x5D9570
CustomEnvMapPipeMaterialData* CCustomBuildingDNPipeline::SetFxEnvTexture(CustomEnvMapPipeMaterialData** inOutEnvMapData) {
    if (*inOutEnvMapData == &CCustomCarEnvMapPipeline::fakeEnvMapPipeMatData) {
        if (*inOutEnvMapData = CCustomCarEnvMapPipeline::m_gEnvMapPipeMatDataPool->New()) {
            **inOutEnvMapData = CCustomCarEnvMapPipeline::fakeEnvMapPipeMatData;
        }
    }
    return *inOutEnvMapData;
}

RwTexture* CCustomBuildingDNPipeline::GetFxEnvTexture(RpMaterial* material) {
    return CCustomCarEnvMapPipeline::GetFxEnvTexture(material);
}

void CCustomBuildingDNPipeline::SetFxEnvScale(RpMaterial* material, float x, float y) {
    CCustomCarEnvMapPipeline::SetFxEnvScale(material, x, y);
}

float CCustomBuildingDNPipeline::GetFxEnvScaleX(RpMaterial* material) {
    return CCustomCarEnvMapPipeline::GetFxEnvScaleX(material);
}

float CCustomBuildingDNPipeline::GetFxEnvScaleY(RpMaterial* material) {
    return CCustomCarEnvMapPipeline::GetFxEnvScaleY(material);
}

void CCustomBuildingDNPipeline::SetFxEnvTransScl(RpMaterial* material, float x, float y) {
    CCustomCarEnvMapPipeline::SetFxEnvTransScl(material, x, y);
}

float CCustomBuildingDNPipeline::GetFxEnvTransSclX(RpMaterial* material) {
    return CCustomCarEnvMapPipeline::GetFxEnvTransSclX(material);
}

float CCustomBuildingDNPipeline::GetFxEnvTransSclY(RpMaterial* material) {
    return CCustomCarEnvMapPipeline::GetFxEnvTransSclY(material);
}

void CCustomBuildingDNPipeline::SetFxEnvShininess(RpMaterial* material, float value) {
    CCustomCarEnvMapPipeline::SetFxEnvShininess(material, value);
}

float CCustomBuildingDNPipeline::GetFxEnvShininess(RpMaterial* material) {
    return CCustomCarEnvMapPipeline::GetFxEnvShininess(material);
}

void CCustomBuildingDNPipeline::InjectHooks() {
    RH_ScopedClass(CCustomBuildingDNPipeline);
    RH_ScopedCategory("Pipelines");

    RH_ScopedInstall(ExtraVertColourPluginAttach, 0x5D72E0);
    RH_ScopedInstall(pluginExtraVertColourConstructorCB, 0x5D6D10);
    RH_ScopedInstall(pluginExtraVertColourDestructorCB, 0x5D6D30);
    RH_ScopedInstall(pluginExtraVertColourStreamReadCB, 0x5D6DE0);
    RH_ScopedInstall(pluginExtraVertColourStreamWriteCB, 0x5D6D80);
    RH_ScopedInstall(pluginExtraVertColourStreamGetSizeCB, 0x5D6DC0);
    RH_ScopedOverloadedInstall(PreRenderUpdate, "Atomic", 0x5D7200, void(*)(RpAtomic*, bool));
    RH_ScopedOverloadedInstall(PreRenderUpdate, "Clump", 0x5D72C0, void(*)(RpClump*, bool));
    RH_ScopedGlobalInstall(NVCPipelineProcess, 0x5D6850);
    RH_ScopedInstall(CreatePipe, 0x5D7100);
    RH_ScopedInstall(DestroyPipe, 0x5D5FA0);
    RH_ScopedInstall(CreateCustomObjPipe, 0x5D6750);
    RH_ScopedInstall(CustomPipeAtomicSetup, 0x5D71C0);
    RH_ScopedInstall(SetFxEnvTexture, 0x5D9570);
    RH_ScopedInstall(CustomPipeMaterialSetup, 0x5D7120);
    RH_ScopedInstall(CustomPipeRenderCB, 0x5D6480);
}
