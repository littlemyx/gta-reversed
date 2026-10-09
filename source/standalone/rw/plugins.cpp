// P2B-04c: plugin-registration thunks + the atomic render-callback slot (trampoline plugin) on top of librw's PluginList.
//   Rp{Atomic,Clump,Geometry}RegisterPlugin / RegisterPluginStream, RpMaterialRegisterPlugin / RegisterPluginStream (RwFrame*RegisterPlugin
//   lives in frame.cpp, RwTexDictionaryRegisterPlugin in texdict.cpp, same recipe).
//   Rp{Atomic,Clump,Geometry,Material}GetPluginOffset, Rp{Atomic,Geometry,Material}SetStreamAlwaysCallBack, Rp{Atomic,Material}SetStreamRightsCallBack
//   (the game does not call these two groups outside the stock RW headers; kept because they are 3 lines each and the plugin lists are the
//   same ones).
//   RpAtomic{Render,SetRenderCallBack,GetRenderCallBack}, AtomicDefaultRenderCallBack, RwShimEnsureAtomicRenderSlot.
//
// Signatures: RW's callbacks differ from librw's only by `const` on the copy source and on the object of write/getSize (same cdecl ABI, same
// layout), and by the return type of the "rights"/"always" callbacks (RwBool vs void: the value is ignored) -> the pointers are reinterpreted.
//
// STARTUP ORDER (documented requirement, see also the asserts): librw's PluginList sizes the object at registration time, so a plugin may only be
// registered while no object of that type exists, and only between rw::Engine::init and the first object creation (Engine::term resets the
// lists). The game's order is preserved by RsRwInitialize: RwEngineInit -> PluginAttach event (every CVisibilityPlugins::PluginAttach /
// NodeNamePluginAttach / C2dEffect::Plugin / CollisionPlugin / ... register here) -> RwEngineOpen -> RwEngineStart. The thunks assert
// `numAllocated == 0` of their type. Offsets returned are byte offsets from the object start, identical in meaning to RW's (RWPLUGINOFFSET).
// Registration with size 0 (the collision clump plugin) is legal: the plugin gets offset == current end and a 0-byte payload.
//
// ATOMIC RENDER CALLBACK. The game's callback has the RW shape `RpAtomic* (*)(RpAtomic*)` (it returns the atomic, NULL = failed); librw's
// `Atomic::renderCB` returns void. Each atomic carries a shim-private 4-byte plugin slot (id kRenderSlotId, never streamed) holding the
// game callback, and `renderCB` is the trampoline below while the slot is non-null; a null slot leaves librw's own `defaultRenderCB`
// (= AtomicDefaultRenderCallBack: pipeline->render) installed so librw-internal paths (Clump::render, World::render) work unchanged.
// The slot plugin is registered lazily by RwShimEnsureAtomicRenderSlot() -- called by the first RpAtomicRegisterPlugin (the game's very first
// atomic plugin, before any atomic exists) and by RpAtomicCreate / RpAtomicClone / RpClumpStreamRead / RpClumpClone -- so no change to
// RpWorldPluginAttach is needed. The slot is copied by Atomic::clone (plugin copy callback) like RW copies renderCallBack.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

#include <cassert>

namespace {
constexpr RwUInt32 kRenderSlotId = 0x7E5C0001; // shim-private plugin id (outside the 0x0253F2xx SA vendor range and librw's ids)

RwInt32 g_slotOffset = -1; // cached; re-validated by RwShimEnsureAtomicRenderSlot (Engine::term/init resets the plugin lists)

RpAtomicCallBackRender& Slot(void* atomic, RwInt32 offset) {
    return *reinterpret_cast<RpAtomicCallBackRender*>(static_cast<char*>(atomic) + offset);
}

void* SlotCtor(void* object, rw::int32 offset, rw::int32) {
    Slot(object, offset) = nullptr;
    return object;
}
void* SlotCopy(void* dst, void* src, rw::int32 offset, rw::int32) {
    Slot(dst, offset) = Slot(src, offset);
    return dst;
}

// installed in rw::Atomic::renderCB while the slot holds a game callback
void RenderTrampoline(rw::Atomic* atomic) {
    if (const auto cb = Slot(atomic, g_slotOffset)) {
        cb(atomic);
    } else {
        rw::Atomic::defaultRenderCB(atomic);
    }
}

template <class T, class Ctor, class Dtor, class Copy>
RwInt32 RegisterPlugin(RwInt32 size, RwUInt32 id, Ctor ctor, Dtor dtor, Copy copy, const char* what) {
    assert(T::numAllocated == 0 && what && "plugin registered after an object of that type was created");
    (void)what;
    return T::registerPlugin(size, id, reinterpret_cast<rw::Constructor>(ctor), reinterpret_cast<rw::Destructor>(dtor), reinterpret_cast<rw::CopyConstructor>(copy));
}

template <class T>
RwInt32 RegisterStream(RwUInt32 id, RwPluginDataChunkReadCallBack read, RwPluginDataChunkWriteCallBack write, RwPluginDataChunkGetSizeCallBack getSize) {
    return T::registerPluginStream(id, reinterpret_cast<rw::StreamRead>(read), reinterpret_cast<rw::StreamWrite>(write), reinterpret_cast<rw::StreamGetSize>(getSize));
}

template <class T>
RwInt32 SetAlways(RwUInt32 id, RwPluginDataChunkAlwaysCallBack cb) {
    return T::setStreamAlwaysCallback(id, reinterpret_cast<rw::AlwaysCallback>(cb));
}

template <class T>
RwInt32 SetRights(RwUInt32 id, RwPluginDataChunkRightsCallBack cb) {
    return T::setStreamRightsCallback(id, reinterpret_cast<rw::RightsCallback>(cb));
}
} // namespace

// W: registers the render-callback slot once per Engine::init cycle. No atomic may exist yet (the plugin changes sizeof(Atomic)).
void RwShimEnsureAtomicRenderSlot() {
    const RwInt32 off = rw::Atomic::s_plglist.getPluginOffset(kRenderSlotId);
    if (off >= 0) {
        g_slotOffset = off;
        return;
    }
    assert(rw::Atomic::numAllocated == 0 && "atomic render-callback slot must be registered before the first atomic exists");
    g_slotOffset = rw::Atomic::registerPlugin(sizeof(RpAtomicCallBackRender), kRenderSlotId, SlotCtor, nullptr, SlotCopy);
}

// ---- thunks (A) ----
RwInt32 RpAtomicRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    RwShimEnsureAtomicRenderSlot();
    return RegisterPlugin<rw::Atomic>(size, pluginID, constructCB, destructCB, copyCB, "RpAtomicRegisterPlugin");
}
RwInt32 RpAtomicRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return RegisterStream<rw::Atomic>(pluginID, readCB, writeCB, getSizeCB);
}
RwInt32 RpClumpRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return RegisterPlugin<rw::Clump>(size, pluginID, constructCB, destructCB, copyCB, "RpClumpRegisterPlugin");
}
RwInt32 RpClumpRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return RegisterStream<rw::Clump>(pluginID, readCB, writeCB, getSizeCB);
}
RwInt32 RpGeometryRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return RegisterPlugin<rw::Geometry>(size, pluginID, constructCB, destructCB, copyCB, "RpGeometryRegisterPlugin");
}
RwInt32 RpGeometryRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return RegisterStream<rw::Geometry>(pluginID, readCB, writeCB, getSizeCB);
}
RwInt32 RpMaterialRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    return RegisterPlugin<rw::Material>(size, pluginID, constructCB, destructCB, copyCB, "RpMaterialRegisterPlugin");
}
RwInt32 RpMaterialRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return RegisterStream<rw::Material>(pluginID, readCB, writeCB, getSizeCB);
}

RwInt32 RpAtomicGetPluginOffset(RwUInt32 pluginID)   { return rw::Atomic::getPluginOffset(pluginID); }
RwInt32 RpClumpGetPluginOffset(RwUInt32 pluginID)    { return rw::Clump::getPluginOffset(pluginID); }
RwInt32 RpGeometryGetPluginOffset(RwUInt32 pluginID) { return rw::Geometry::getPluginOffset(pluginID); }
RwInt32 RpMaterialGetPluginOffset(RwUInt32 pluginID) { return rw::Material::getPluginOffset(pluginID); }
RwInt32 RpAtomicSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack cb)   { return SetAlways<rw::Atomic>(pluginID, cb); }
RwInt32 RpGeometrySetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack cb) { return SetAlways<rw::Geometry>(pluginID, cb); }
RwInt32 RpMaterialSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack cb) { return SetAlways<rw::Material>(pluginID, cb); }
RwInt32 RpAtomicSetStreamRightsCallBack(RwUInt32 pluginID, RwPluginDataChunkRightsCallBack cb)   { return SetRights<rw::Atomic>(pluginID, cb); }
RwInt32 RpMaterialSetStreamRightsCallBack(RwUInt32 pluginID, RwPluginDataChunkRightsCallBack cb) { return SetRights<rw::Material>(pluginID, cb); }

// ---- atomic render callback ----
// D: exe 0x7491C0: the atomic's own pipeline (or the default one) renders it; returns the atomic (NULL when the pipeline failed -- librw's
// pipeline render has no result, so success is assumed).
RpAtomic* AtomicDefaultRenderCallBack(RpAtomic* atomic) {
    if (!atomic) {
        return nullptr;
    }
    rw::Atomic::defaultRenderCB(atomic);
    return atomic;
}

// A: runs the game callback (its result is returned: NULL = failure, which RpClumpRender folds into its own result) or the default one.
RpAtomic* RpAtomicRender(RpAtomic* atomic) {
    if (!atomic) {
        return nullptr;
    }
    if (g_slotOffset >= 0) {
        if (const auto cb = Slot(atomic, g_slotOffset)) {
            return cb(atomic);
        }
    }
    atomic->renderCB(atomic);
    return atomic;
}

// A: NULL (and AtomicDefaultRenderCallBack itself) restores the default; anything else is stored in the slot and the trampoline installed.
RpAtomic* RpAtomicSetRenderCallBack(RpAtomic* atomic, RpAtomicCallBackRender callback) {
    if (!atomic) {
        return nullptr;
    }
    RwShimEnsureAtomicRenderSlot();
    if (!callback || callback == AtomicDefaultRenderCallBack) {
        Slot(atomic, g_slotOffset) = nullptr;
        atomic->renderCB           = rw::Atomic::defaultRenderCB;
    } else {
        Slot(atomic, g_slotOffset) = callback;
        atomic->renderCB           = RenderTrampoline;
    }
    return atomic;
}

// A: the stored game callback, AtomicDefaultRenderCallBack when none is set (RW's field is never NULL).
RpAtomicCallBackRender RpAtomicGetRenderCallBack(const RpAtomic* atomic) {
    if (!atomic) {
        return nullptr;
    }
    if (g_slotOffset >= 0) {
        if (const auto cb = Slot(const_cast<RpAtomic*>(atomic), g_slotOffset)) {
            return cb;
        }
    }
    return AtomicDefaultRenderCallBack;
}
#endif
