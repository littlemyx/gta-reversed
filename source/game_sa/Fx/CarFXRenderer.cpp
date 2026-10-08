#include "StdInc.h"

#include "CarFXRenderer.h"
#include "CustomCarEnvMapPipeline.h"
#include "ClothesBuilder.h"
#include "TxdStore.h"

void CCarFXRenderer::InjectHooks() {
    RH_ScopedClass(CCarFXRenderer);
    RH_ScopedCategory("Fx");

    RH_ScopedInstall(RegisterPlugins, 0x5D5B00);
    RH_ScopedInstall(Initialise, 0x5D5AC0);
    RH_ScopedInstall(InitialiseDirtTexture, 0x5D5BC0);
    RH_ScopedInstall(Shutdown, 0x5D5AD0);
    RH_ScopedInstall(PreRenderUpdate, 0x5D5B10);
    RH_ScopedInstall(IsCCPCPipelineAttached, 0x5D5B80);
    RH_ScopedInstall(CustomCarPipeAtomicSetup, 0x5D5B20);
    RH_ScopedInstall(CustomCarPipeClumpSetup, 0x5D5B40);
    RH_ScopedInstall(SetCustomFXAtomicRenderPipelinesVMICB, 0x5D5B60);
    RH_ScopedInstall(SetFxEnvMapLightMult, 0x5D5BA0);
}

// 0x5D5B00
bool CCarFXRenderer::RegisterPlugins() {
    return CCustomCarEnvMapPipeline::RegisterPlugin() != 0;
}

// 0x5D5AC0
bool CCarFXRenderer::Initialise() {
    return CCustomCarEnvMapPipeline::CreatePipe() != 0;
}

// 0x5D5BC0
void CCarFXRenderer::InitialiseDirtTexture() {
    const auto vehicleTxd = CTxdStore::FindTxdSlot("vehicle");
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(vehicleTxd);

    auto* const grunge = RwTextureRead("vehiclegrunge256", nullptr);
    RwTextureSetFilterMode(grunge, rwFILTERLINEAR);

    const auto width  = grunge->raster->width;
    const auto height = grunge->raster->height;

    // Create `NUM_DIRT_TEXTURES` variations of the grunge texture, from white (clean), to the grunge texture itself (dirty)
    for (auto i = 0; i < NUM_DIRT_TEXTURES; i++) {
        auto* const dirtTex = CClothesBuilder::CopyTexture(grunge);
        ms_aDirtTextures[i] = dirtTex;
        RwTextureSetName(dirtTex, "vehiclegrunge256");

        auto* const raster = dirtTex->raster;
        auto* const pixels = RwRasterLock(raster, 0, rwRASTERLOCKWRITE | rwRASTERLOCKREAD);

        const auto bias = (4080 - 255 * i) / 16; // Original: iterates over `i` by decreasing 0xFF from 0xFF0, and divides by 16
        for (auto y = 0; y < height; y++) {
            for (auto x = 0; x < width; x++) {
                auto* const px = &pixels[(y * width + x) * 4];
                for (auto c = 0; c < 3; c++) { // Note: Alpha is untouched
                    px[c] = (uint8)(((px[c] * i) >> 4) + bias);
                }
            }
        }

        RwRasterUnlock(raster);
    }

    CTxdStore::PopCurrentTxd();
}

// 0x5D5AD0
void CCarFXRenderer::Shutdown() {
    for (auto& texture : ms_aDirtTextures) {
        RwTextureDestroy(texture);
    }
    CCustomCarEnvMapPipeline::DestroyPipe();
}

// 0x5D5B10
void CCarFXRenderer::PreRenderUpdate() {
    CCustomCarEnvMapPipeline::PreRenderUpdate();
}

// 0x5D5B80
bool CCarFXRenderer::IsCCPCPipelineAttached(RpAtomic* atomic) {
    return GetPipelineID(atomic) == 0x53F2009A;
}

// 0x5D5B20
void CCarFXRenderer::CustomCarPipeAtomicSetup(RpAtomic* atomic) {
    CCustomCarEnvMapPipeline::CustomPipeAtomicSetup(atomic);
}

// 0x5D5B40
void CCarFXRenderer::CustomCarPipeClumpSetup(RpClump* clump) {
    RpClumpForAllAtomics(clump, SetCustomFXAtomicRenderPipelinesVMICB, nullptr);
}

// 0x5D5B60
RpAtomic* CCarFXRenderer::SetCustomFXAtomicRenderPipelinesVMICB(RpAtomic* atomic, void* data) {
    CCustomCarEnvMapPipeline::CustomPipeAtomicSetup(atomic);
    return atomic;
}

// 0x5D5BA0
void CCarFXRenderer::SetFxEnvMapLightMult(float multiplier) {
    gSpecIntensity = multiplier;
}
