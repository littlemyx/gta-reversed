// Link stubs for the render API that the portable build does NOT take from the (excluded) D3D9 fixed-function facade
// (stream R, slice R-2, .notes/STREAM_R_PLAN.md). Built ONLY with GTASA_PORTABLE=ON (source/portable/ is globbed for the portable target only).
//
// Excluded in source/CMakeLists.txt (PORTABLE_EXCLUDED_RENDER_FILES): standalone/rw/{pipeline,pipeline_matfx,pipeline_skin,pipeline_skin_cpu,
// skin_vs,rwd3d_ff,im2d_im3d}.cpp and game_sa/RenderWare/D3D{ResourceSystem,TextureBuffer,IndexDataBuffer}.cpp. What kept code still
// references from them is defined here, honestly: the first call of each symbol is logged (R-2 stub), the call fails (FALSE / nullptr) or
// does nothing (void state setters). Nothing here pretends to render. The list is generated and checked by tools/portable/rw_symbol_audit.py
// (.notes/reports/MAC-R2-symbols.tsv); every symbol names the slice that replaces it:
//   R-4  RwIm2D* / RwIm3D*            -> render/im2d_im3d_gl3.cpp
//   R-6  RwShimPipelineEnsure/Shutdown -> stock gl3 pipelines wired in render/pipelines_gl3.cpp
//   R-8/R-9  RxPipeline*, RxD3D9AllInOne*, RwD3D9*, _rwD3D9*  -> ObjPipeline creation in render/pipelines_{building,carenv}_gl3.cpp; the
//        guarded-out FF render callbacks in game_sa/Pipelines get GL3 twins there
//   D3DResourceSystem: inert on purpose (the pools only ever held IDirect3D* objects of the exe's own RenderWare)
#include "StdInc.h"

#include <atomic>

#define R2_STUB(slice)                                                                                                       \
    do {                                                                                                                      \
        static std::atomic_flag once_ = ATOMIC_FLAG_INIT;                                                                     \
        if (!once_.test_and_set()) {                                                                                          \
            NOTSA_LOG_ERR("R-2 stub: {} ({}) is not implemented under GL3, the call fails / does nothing", __func__, slice);  \
        }                                                                                                                     \
    } while (0)

// ---- Im2D / Im3D (R-4) -----------------------------------------------------------------------------------------------
// Return values follow the exe contract: FALSE = nothing rendered.
RwBool RwIm2DRenderPrimitive(RwPrimitiveType, RwIm2DVertex*, RwInt32) { R2_STUB("R-4"); return FALSE; }
RwBool RwIm2DRenderLine(RwIm2DVertex*, RwInt32, RwInt32, RwInt32) { R2_STUB("R-4"); return FALSE; }
RwBool RwIm2DRenderTriangle(RwIm2DVertex*, RwInt32, RwInt32, RwInt32, RwInt32) { R2_STUB("R-4"); return FALSE; }
RwBool RwIm2DRenderIndexedPrimitive(RwPrimitiveType, RwIm2DVertex*, RwInt32, RwImVertexIndex*, RwInt32) { R2_STUB("R-4"); return FALSE; }
void*  RwIm3DTransform(RwIm3DVertex*, RwUInt32, RwMatrix*, RwUInt32) { R2_STUB("R-4"); return nullptr; }
RwBool RwIm3DEnd(void) { R2_STUB("R-4"); return FALSE; }
RwBool RwIm3DRenderPrimitive(RwPrimitiveType) { R2_STUB("R-4"); return FALSE; }
RwBool RwIm3DRenderIndexedPrimitive(RwPrimitiveType, RwImVertexIndex*, RwInt32) { R2_STUB("R-4"); return FALSE; }
RwBool RwIm3DRenderLine(RwInt32, RwInt32) { R2_STUB("R-4"); return FALSE; }

// ---- pipeline bring-up hooks called from the engine shim (R-6) ---------------------------------------------------------
void RwShimPipelineEnsure() { R2_STUB("R-6"); }
void RwShimPipelineShutdown() {}

// ---- RxPipeline / AllInOne node API (R-8, R-9): creating a custom pipeline fails, the atomic keeps the stock (null) pipeline ----------
RxPipeline* RxPipelineCreate(void) { R2_STUB("R-8/R-9"); return nullptr; }
RwBool RxPipelineDestroy(RxPipeline*) { return FALSE; }
RxLockedPipe* RxPipelineLock(RxPipeline*) { R2_STUB("R-8/R-9"); return nullptr; }
RxLockedPipe* RxLockedPipeAddFragment(RxLockedPipe*, RwUInt32*, RxNodeDefinition*, ...) { R2_STUB("R-8/R-9"); return nullptr; }
RxPipeline* RxLockedPipeUnlock(RxLockedPipe*) { R2_STUB("R-8/R-9"); return nullptr; }
RxNodeDefinition* RxNodeDefinitionGetD3D9AtomicAllInOne() { R2_STUB("R-8/R-9"); return nullptr; }
RxPipelineNode* RxPipelineFindNodeByName(RxPipeline*, const RwChar*, RxPipelineNode*, RwInt32*) { R2_STUB("R-8/R-9"); return nullptr; }
RxD3D9AllInOneInstanceCallBack RxD3D9AllInOneGetInstanceCallBack(RxPipelineNode*) { R2_STUB("R-8/R-9"); return nullptr; }
RxD3D9AllInOneReinstanceCallBack RxD3D9AllInOneGetReinstanceCallBack(RxPipelineNode*) { R2_STUB("R-8/R-9"); return nullptr; }
void RxD3D9AllInOneSetInstanceCallBack(RxPipelineNode*, RxD3D9AllInOneInstanceCallBack) { R2_STUB("R-8/R-9"); }
void RxD3D9AllInOneSetReinstanceCallBack(RxPipelineNode*, RxD3D9AllInOneReinstanceCallBack) { R2_STUB("R-8/R-9"); }
void RxD3D9AllInOneSetRenderCallBack(RxPipelineNode*, RxD3D9AllInOneRenderCallBack) { R2_STUB("R-8/R-9"); }

// ---- RwD3D9* fixed-function state API: only renderstate.cpp still calls these two (guarded in R-3b); the rest of the FF API has no kept caller ----
void   RwD3D9SetRenderState(RwUInt32, RwUInt32) { R2_STUB("R-8/R-9"); }
void   RwD3D9SetTextureStageState(RwUInt32, RwUInt32, RwUInt32) { R2_STUB("R-8/R-9"); }

// ---- D3DResourceSystem (inert): pools of IDirect3D* objects of the exe's RenderWare, nothing to pool under GL3 ----------------
void D3DResourceSystem::InjectHooks() {}
void D3DResourceSystem::CancelBuffering() {}
void D3DResourceSystem::Init() {}
void D3DResourceSystem::SetUseD3DResourceBuffering(bool) {}
void D3DResourceSystem::Shutdown() {}
void D3DResourceSystem::TidyUpD3DIndexBuffers(uint32) {}
void D3DResourceSystem::TidyUpD3DTextures(uint32) {}
