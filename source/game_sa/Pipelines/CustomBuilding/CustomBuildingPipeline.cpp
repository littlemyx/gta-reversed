#include "StdInc.h"

#include "CustomBuildingPipeline.h"
#include <CustomBuildingDNPipeline.h>
#include <CustomCarEnvMapPipeline.h>
#include <PipelinesCommon.hpp>

constexpr auto PLUGIN_ID = rpPDS_MAKEPIPEID(rwVENDORID_DEVELOPER, 0x9C);

void CCustomBuildingPipeline::InjectHooks() {
    RH_ScopedClass(CCustomBuildingPipeline);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(CreatePipe, 0x5D7D90);
    RH_ScopedInstall(DestroyPipe, 0x5D7380);
    RH_ScopedInstall(CustomPipeAtomicSetup, 0x5D7E50);
    RH_ScopedInstall(CreateCustomObjPipe, 0x5D7AA0);
    RH_ScopedInstall(CustomPipeMaterialSetup, 0x5D7DB0);
    RH_ScopedInstall(CustomPipeRenderCB, 0x5D77D0);
    RH_ScopedInstall(CustomPipeReinstanceCB, 0x5D77A0);
}

// 0x5D7D90
bool CCustomBuildingPipeline::CreatePipe() {
    ObjPipeline = CreateCustomObjPipe();
    return ObjPipeline != nullptr;
}

// 0x5D7380
void CCustomBuildingPipeline::DestroyPipe() {
    if (auto* const pipe = std::exchange(ObjPipeline, nullptr)) {
        RxPipelineDestroy(pipe);
    }
}

// 0x5D7E50
RpAtomic* CCustomBuildingPipeline::CustomPipeAtomicSetup(RpAtomic* atomic) {
    RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), CustomPipeMaterialSetup, nullptr);
    atomic->pipeline = ObjPipeline;
    SetPipelineID(atomic, PLUGIN_ID);
    return atomic;
}

// 0x5D7AA0
RxPipeline* CCustomBuildingPipeline::CreateCustomObjPipe() {
    auto* const pipe = RxPipelineCreate();
    if (!pipe) {
        return nullptr;
    }

    auto* const lockedPipe = RxPipelineLock(pipe);
    if (!lockedPipe || !RxLockedPipeAddFragment(lockedPipe, 0, RxNodeDefinitionGetD3D9AtomicAllInOne(), 0) || !RxLockedPipeUnlock(lockedPipe)) {
        RxPipelineDestroy(pipe);
        return nullptr;
    }

    auto* const node = RxPipelineFindNodeByName(pipe, RxNodeDefinitionGetD3D9AtomicAllInOne()->name, nullptr, nullptr);
    RxD3D9AllInOneSetInstanceCallBack(node, RxD3D9AllInOneGetInstanceCallBack(node));
    RxD3D9AllInOneSetReinstanceCallBack(node, CustomPipeReinstanceCB);
    RxD3D9AllInOneSetRenderCallBack(node, CustomPipeRenderCB);

    RwCompatPipelinePluginId(pipe) = PLUGIN_ID;
    pipe->pluginData = PLUGIN_ID;

    return pipe;
}

// 0x5D7DB0
RpMaterial* CCustomBuildingPipeline::CustomPipeMaterialSetup(RpMaterial* material, void* data) {
    auto** const envMapData = &CCustomCarEnvMapPipeline::EnvMapPlGetData(material);

    if (RpMatFXMaterialGetEffects(material) == rpMATFXEFFECTENVMAP) {
        if (auto* const data = CCustomBuildingDNPipeline::SetFxEnvTexture(envMapData)) {
            data->Texture = (MATFXD3D9ENVMAPGETDATA(material, 0))->texture; // this macro (afaik) isn't otherwise public, I have no clue how they managed to get this working

            if (data->Texture) {
                RwTextureSetAddressing(data->Texture, rwTEXTUREADDRESSWRAP);
                RwTextureSetFilterMode(data->Texture, rwFILTERLINEAR);
            }
        }
    }

    CCustomCarEnvMapPipeline::SetMaterialFlags(
        material,
        *envMapData && (*envMapData)->Shininess != 0.f && (*envMapData)->Texture
        ? CCustomCarEnvMapPipeline::MF_HAS_SHINE_CAM
        : CCustomCarEnvMapPipeline::MF_NONE
    );

    return material;
}

// 0x5D77D0
// Line-by-line from the exe: identical to CCustomBuildingDNPipeline::CustomPipeRenderCB (0x5D6480) except for its own texture matrix
// (0xC02C70 vs 0xC02C28) and COLORARG2 of the env stage (D3DTA_TFACTOR here, D3DTA_DIFFUSE in the DN version).
void CCustomBuildingPipeline::CustomPipeRenderCB(RwResEntry* resEntry, void* object, RwUInt8 type, RwUInt32 rxGeoFlags) {
    // The texture transform matrix of the env map stage, only the diagonal is ever set (and it is never applied: D3DTS_TEXTURE1 isn't set here)
    static auto& s_EnvMapTexMatrix = StaticRef<D3DMATRIX>(0xC02C70);

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

    for (RwUInt32 i = 0; i < header->numMeshes; i++) { // 0x5D78A0
        auto* const mesh             = &meshes[i];
        auto* const mat              = mesh->material;
        const auto  matFlags         = CCustomCarEnvMapPipeline::GetMaterialFlags(mat);
        const auto* const envMapData = CCustomCarEnvMapPipeline::EnvMapPlGetData(mat);

        // 0x5D78B8
        RwD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        RwD3D9SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);

        // 0x5D78CD
        if (matFlags & CCustomCarEnvMapPipeline::MF_HAS_SHINE_CAM) {
            RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, RWRSTATE(rwTEXTUREADDRESSWRAP));

            s_EnvMapTexMatrix._11 = CCustomCarEnvMapPipeline::GetFxEnvScaleX(mat); // (float)(int8)raw * 0.125f
            s_EnvMapTexMatrix._44 = 1.f;
            s_EnvMapTexMatrix._33 = 1.f;
            s_EnvMapTexMatrix._22 = CCustomCarEnvMapPipeline::GetFxEnvScaleY(mat);

            RwD3D9SetTexture(envMapData->Texture, 1);

            // 0x5D7939 - The whole expression is evaluated in extended precision, then truncated (_ftol)
            auto c = (int32)(int64)(((double)std::bit_cast<uint8>(envMapData->Shininess) /* raw, unsigned */ * (double)ExeRecip(255.f)) * 254.0); // 0x859A3C, 0x86BE90
            if (c > 0xFF) {
                c = 0xFF;
            }
            RwD3D9SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(0xFF, (uint8)c, (uint8)c, (uint8)c));

            RwD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);
            RwD3D9SetTextureStageState(1, D3DTSS_COLORARG0, D3DTA_CURRENT);
            RwD3D9SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            RwD3D9SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TFACTOR);

            RwD3D9SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
            RwD3D9SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            RwD3D9SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);

            RwD3D9SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1 | D3DTSS_TCI_CAMERASPACENORMAL);
            RwD3D9SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

            RwD3D9SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
        }

        // 0x5D79EE - exe: call 0x7FE0A0 (_rwD3D9RenderStateVertexAlphaEnable)
        RwCompatVertexAlphaEnable(mesh->vertexAlpha || mesh->material->color.alpha != 0xFF);

        if (geoHasNoLighting) { // 0x5D7A12 - Render without lighting
            RxD3D9InstanceDataRender(header, mesh);
        } else { // 0x5D7A1A - Render with ligthing
            if (isLightingEnabled) {
                RwD3D9SetSurfaceProperties(&mesh->material->surfaceProps, &mesh->material->color, rxGeoFlags);
            }
            RxD3D9InstanceDataRenderLighting(header, mesh, rxGeoFlags, mesh->material->texture); // 0x5D7A41
        }
    }

    RwD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    RwD3D9SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    RwD3D9SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
    RwD3D9SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, NULL);
}

// 0x5D77A0
RwBool CCustomBuildingPipeline::CustomPipeReinstanceCB(void* object, RwResEntry* resEntry, RxD3D9AllInOneInstanceCallBack instanceCallback) {
    auto* const header = (RxD3D9ResEntryHeader*)(resEntry + 1);
    return !instanceCallback || instanceCallback(object, header, true) != 0; // exe: a missing callback counts as success
}
