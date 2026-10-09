/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#ifdef NOTSA_RW_LIBRW
#include <rwcore.h> // fakerw (librw), see source/fakerw/fakerw.h
#include <skeleton.h>
#else
#include "rw/rwcore.h"
#include "rw/rphanim.h"
#include "rw/rpuvanim.h"
#include "rw/rpskin.h"
#include "rw/rpmatfx.h"
#include "rw/skeleton.h"
#include "rw/rwplcore.h"
#endif
#include "RwCompat.h"
#include <type_traits>

#ifdef _DX9_SDK_INSTALLED
#include "d3d9.h"
#endif

static inline auto& RsGlobal =  StaticRef<RsGlobalType>(0xC17040);

#ifdef NOTSA_RW_LIBRW
// RenderWare internals live in librw (source/fakerw): their exe-address statics/helpers are replaced by shim members (P2B-00c ...)
#else
static inline auto& RwInitialized = StaticRef<bool>(0xC920E8);
static inline auto& RwEngineInstance =  StaticRef<RwGlobals*>(0xC97B24);
static inline auto& geometryTKList =  StaticRef<RwPluginRegistry>(0x8D628C);
static inline auto& RpUVAnimDictSchema =  StaticRef<RtDictSchema>(0x8DED50);
static inline auto& AmbientSaturated = StaticRef<RwRGBAReal>(0x8E2418);

inline IDirect3DDevice9 *GetD3D9Device() {
    return *reinterpret_cast<IDirect3DDevice9 **>(0xC97C28);
}

#ifndef D3DMATRIX_DEFINED
struct _D3DMATRIX;
#endif
inline _D3DMATRIX *GetD3DViewTransform() {
    return reinterpret_cast<_D3DMATRIX *>(0xC9BC80);
}

inline _D3DMATRIX *GetD3DProjTransform() {
    return reinterpret_cast<_D3DMATRIX *>(0x8E2458);
}

inline void _rpMaterialSetDefaultSurfaceProperties(RwSurfaceProperties *surfProps) {
    ((void(__cdecl *)(RwSurfaceProperties*))0x74D870)(surfProps);
}
#endif

#define RWRSTATE(a) (reinterpret_cast<void *>(a))
#define PSGLOBAL(var) (((psGlobalType *)(RsGlobal.ps))->var)

/*!
* @brief Generic PipeID construction.
*/
#define rpPDS_MAKEPIPEID(vendorID, pipeID) ((((vendorID) & 0xFFFF) << 16) | ((pipeID) & 0xFFFF))

struct RwResEntrySA : RwResEntry {
    RxD3D9ResEntryHeader header;
    RxD3D9InstanceData meshData;
};


void RwCoreInjectHooks();

template<typename T>
inline T RwStreamRead(RwStream* stream, size_t size = sizeof(T)) {
    T data;
    RwStreamRead(stream, &data, size);
    return data;
}

namespace RtAnim {
    void InjectHooks();
}
