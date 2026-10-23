#pragma once
// P-2i host-test stub of StdInc.h: just enough RenderWare / hook-macro surface for the REAL BreakablePlugin.cpp + BreakablePlugin.h to compile and run on the Mac
// (StdInc is not linkable there yet). The stream is a memory buffer that counts the bytes consumed; RwTextureRead hands out fake textures and counts live ones.
#include <Base.h>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

using RwInt32 = int32_t; using RwUInt32 = uint32_t; using RwUInt8 = uint8_t;
struct RwTexCoords { float u, v; };
struct RwRGBA { uint8_t red, green, blue, alpha; };
struct RwTexture { char name[32]; char mask[32]; };
struct RpGeometry {};
struct RwStream { std::vector<uint8_t> data; size_t pos = 0; bool writing = false; };

#define rwVENDORID_DEVELOPER 0x0253F2
#define MAKECHUNKID(vendor, id) (((vendor & 0xFFFFFF) << 8) | (id & 0xFF))
#define RWPLUGINOFFSET(type, object, offset) ((type*)((RwUInt8*)(object) + (offset)))
#define RWPLUGINOFFSETCONST(type, object, offset) ((const type*)((const RwUInt8*)(object) + (offset)))
#define NOTSA_GLOBAL_HDR(name, addr, type, ...) inline std::type_identity_t<NOTSA_UNPAREN type> name __VA_ARGS__
#define RH_ScopedClass(...)
#define RH_ScopedCategory(...)
#define RH_ScopedGlobalInstall(...)
#define NOTSA_LOG_DEBUG(...) ((void)0)

template<class... A> inline int RpGeometryRegisterPlugin(A&&...) { return 1; }
template<class... A> inline int RpGeometryRegisterPluginStream(A&&...) { return 1; }

// ---- stream ----------------------------------------------------------------------------------------------------------------------------------------------
inline size_t RwStreamRead(RwStream* s, void* dst, size_t n) {
    if (s->pos + n > s->data.size()) { std::fprintf(stderr, "P2I-STUB: read past the end (%zu + %zu > %zu)\n", s->pos, n, s->data.size()); std::abort(); }
    if (n) std::memcpy(dst, s->data.data() + s->pos, n);
    s->pos += n; return n;
}
inline RwStream* RwStreamWrite(RwStream* s, const void* src, size_t n) {
    if (n) { auto* p = (const uint8_t*)src; s->data.insert(s->data.end(), p, p + n); }
    s->pos = s->data.size(); return s;
}
// ---- textures --------------------------------------------------------------------------------------------------------------------------------------------
inline long g_liveTextures = 0, g_textureReads = 0;
inline std::vector<std::pair<std::string, std::string>> g_textureLog; // (name, mask or "<null>") per RwTextureRead
inline RwTexture* RwTextureRead(const char* name, const char* mask) {
    auto* t = new RwTexture{}; std::strncpy(t->name, name, 31); if (mask) std::strncpy(t->mask, mask, 31);
    g_liveTextures++; g_textureReads++; g_textureLog.emplace_back(name, mask ? mask : "<null>"); return t;
}
inline void RwTextureDestroy(RwTexture* t) { g_liveTextures--; delete t; }
