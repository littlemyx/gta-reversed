// SCRATCH VERSION (agent PERF): installs the exe's D3DResourceSystem pool into librw's D3D9 layer.
#include "StdInc.h"
#include <src/d3d/rwd3dimpl.h>

namespace rw { namespace d3d {
struct ResourceHooks
{
	long (*createTexture)(int32 width, int32 height, int32 levels, uint32 format, void **out);
	void (*destroyTexture)(void *texture);
	long (*createIndexBuffer)(uint32 numIndices, void **out);
	void (*destroyIndexBuffer)(void *indexBuffer);
};
extern ResourceHooks resourceHooks;
}}

namespace {
long HookCreateTexture(rw::int32 width, rw::int32 height, rw::int32 levels, rw::uint32 format, void** out) {
    return D3DResourceSystem::CreateTexture(width, height, levels, format, out);
}
void HookDestroyTexture(void* texture) {
    auto* const tex = (IDirect3DTexture9*)texture;
    D3DSURFACE_DESC desc;
    // only plain managed textures were ever recycled by the exe (render targets / autogen-mip textures take other paths)
    if (SUCCEEDED(tex->GetLevelDesc(0, &desc)) && desc.Pool == D3DPOOL_MANAGED && desc.Usage == 0) {
        D3DResourceSystem::DestroyTexture(texture);
    } else {
        tex->Release();
    }
}
long HookCreateIndexBuffer(rw::uint32 numIndices, void** out) {
    return D3DResourceSystem::CreateIndexBuffer(numIndices, D3DFMT_INDEX16, out);
}
void HookDestroyIndexBuffer(void* indexBuffer) {
    auto* const ib = (IDirect3DIndexBuffer9*)indexBuffer;
    D3DINDEXBUFFER_DESC desc;
    if (SUCCEEDED(ib->GetDesc(&desc)) && desc.Pool == D3DPOOL_MANAGED && !(desc.Usage & D3DUSAGE_DYNAMIC)) {
        D3DResourceSystem::DestroyIndexBuffer(indexBuffer);
    } else {
        ib->Release();
    }
}
}

void RwShimInstallResourcePool() {
    auto& h = rw::d3d::resourceHooks;
    h.createTexture      = HookCreateTexture;
    h.destroyTexture     = HookDestroyTexture;
    h.createIndexBuffer  = HookCreateIndexBuffer;
    h.destroyIndexBuffer = HookDestroyIndexBuffer;
}
