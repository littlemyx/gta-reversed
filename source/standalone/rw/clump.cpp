// P2B-04c: RpClump* on top of librw (rw::Clump).
//   RpClump{Create,Destroy,Clone,AddAtomic,RemoveAtomic,ForAllAtomics,GetNumAtomics,Render,StreamRead,StreamWrite,StreamGetSize} and the light /
//   camera members (Add/Remove/ForAll/GetNum). RpClumpGetFrame / RpClumpSetFrame are macros in fakerw.
//   P2B-04d (end of file): RpClumpGtaStreamRead1 / RpClumpGtaStreamRead2 / RpClumpGtaCancelStream, the SA-specific split clump reader.
//   Not here (nothing in the game calls them outside the stock RW headers): RpClumpSet/GetCallBack (librw's clump has no render callback
//   field), RpClumpCreateSpace, RpClumpValidatePlugins, RpClumpSetStreamAlwaysCallBack.
// Verified against the exe: RpClumpGtaStreamRead1 0x72E570, Read2 0x72E620, CancelStream 0x72E700, RpClumpCreate 0x74A290, RpClumpDestroy 0x74A310, RpClumpClone 0x749F70, RpClumpAddAtomic 0x74A490,
// RpClumpRemoveAtomic 0x74A4C0, RpClumpForAllAtomics 0x749B70, RpClumpRender 0x749B20, RpClumpStreamRead 0x74B420.
// ORDER (matters for every ForAllAtomics consumer): the exe links atomics / lights at the HEAD of the clump's list (AddAtomic, and the same
// inline insertion in RpClumpStreamRead) and iterates head -> tail, i.e. the list is in REVERSE insertion order: a streamed clump lists its
// atomics in the reverse of the DFF order, and RpClumpClone (head -> tail, each clone added at the head) reverses once more: the clone of a
// streamed clump lists them in DFF order. librw appends. The shim
// therefore adds at the head (LinkList::add) and reverses the lists librw built in Clump::streamRead.
// Destroy: the plugin destructors run first while the frame hierarchy is intact (CClumpAnim destructor walks it), then atomics / lights /
// cameras, then the whole frame hierarchy (objects of the hierarchy that are not in the clump are only detached) -- librw's
// Frame::destroyHierarchy asserts on such objects and forgets Frame::numAllocated, so the frames go through RwFrameDestroy.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

#include <cassert>
#include <vector>

namespace {
// Post-order: children first, so every RwFrameDestroy sees a childless frame (it removes itself from its parent and detaches its objects).
void DestroyFrameHierarchy(RwFrame* frame) {
    RwFrame* next = nullptr;
    for (RwFrame* child = frame->child; child; child = next) {
        next = child->next;
        DestroyFrameHierarchy(child);
    }
    RwFrameDestroy(frame);
}

// the list librw filled by appending, in insertion order; the exe's order is the reverse
void ReverseList(rw::LinkList& list) {
    std::vector<rw::LLLink*> links;
    for (rw::LLLink* l = list.link.next; l != list.end(); l = l->next) {
        links.push_back(l);
    }
    list.init();
    for (rw::LLLink* l : links) {
        list.add(l);
    }
}
} // namespace

// D: no frame, empty lists.
RpClump* RpClumpCreate() {
    return rw::Clump::create();
}

// W: see the file header. Returns TRUE.
RwBool RpClumpDestroy(RpClump* clump) {
    if (!clump) {
        return FALSE;
    }
    rw::Clump::s_plglist.destruct(clump);
    if (clump->world) {
        clump->world->removeClump(clump); // detaches the atomics / lights / cameras from the world as well
    }
    while (clump->atomics.link.next != clump->atomics.end()) {
        RpAtomic* const atomic = rw::Atomic::fromClump(clump->atomics.link.next);
        clump->removeAtomic(atomic);
        RpAtomicDestroy(atomic);
    }
    while (clump->lights.link.next != clump->lights.end()) {
        rw::Light* const light = rw::Light::fromClump(clump->lights.link.next);
        clump->removeLight(light);
        light->destroy();
    }
    while (clump->cameras.link.next != clump->cameras.end()) {
        rw::Camera* const camera = rw::Camera::fromClump(clump->cameras.link.next);
        clump->removeCamera(camera);
        camera->destroy();
    }
    if (RwFrame* const frame = clump->getFrame()) {
        clump->setFrame(nullptr);
        DestroyFrameHierarchy(frame);
    }
    rwFree(clump);
    rw::Clump::numAllocated--;
    return TRUE;
}

// W: frame hierarchy cloned with Frame::cloneAndLink (every ORIGINAL frame's `root` is left pointing at its clone, which is how the
// atomics find their frame; purgeClone restores it), atomics cloned head -> tail and added at the head (-> the REVERSE of the source order, as the exe),
// plugin copy callbacks last, tied to the same world. Lights / cameras are not cloned (the game has none inside a clump). A source clump
// without a frame cannot be cloned (NULL, like the exe). An atomic whose frame is outside the clump's hierarchy keeps sharing that frame.
RpClump* RpClumpClone(RpClump* clump) {
    if (!clump || !clump->getFrame()) {
        return nullptr;
    }
    RwShimEnsureAtomicRenderSlot();
    RwFrame* const srcRoot  = clump->getFrame();
    RwFrame* const hierRoot = srcRoot->root; // every frame of the hierarchy has this as `root` before cloning
    std::vector<char> inHierarchy; // per atomic (list order): is its frame part of the cloned hierarchy?
    for (rw::LLLink* lnk = clump->atomics.link.next; lnk != clump->atomics.end(); lnk = lnk->next) {
        RwFrame* const f = rw::Atomic::fromClump(lnk)->getFrame();
        inHierarchy.push_back(f && f->root == hierRoot);
    }
    RpClump* const copy = rw::Clump::create();
    if (!copy) {
        return nullptr;
    }
    copy->setFrame(srcRoot->cloneAndLink());
    size_t i = 0;
    for (rw::LLLink* lnk = clump->atomics.link.next; lnk != clump->atomics.end(); lnk = lnk->next, i++) {
        RpAtomic* const src = rw::Atomic::fromClump(lnk);
        RpAtomic* const dup = RpAtomicClone(src);
        if (!dup) {
            continue;
        }
        if (RwFrame* const f = src->getFrame()) {
            dup->setFrame(inHierarchy[i] ? f->root : f); // `root` of an in-hierarchy frame is its clone now
        }
        dup->object.object.privateFlags |= rw::Atomic::WORLDBOUNDDIRTY;
        RpClumpAddAtomic(copy, dup);
    }
    srcRoot->purgeClone();
    if (clump->world) {
        clump->world->addClump(copy);
    }
    rw::Clump::s_plglist.copy(copy, clump);
    return copy;
}

// D: at the HEAD of the list (exe 0x74A490). An atomic that is already in a clump is moved (RW would corrupt both lists).
RpClump* RpClumpAddAtomic(RpClump* clump, RpAtomic* atomic) {
    if (!clump || !atomic) {
        return nullptr;
    }
    if (atomic->clump) {
        RpClumpRemoveAtomic(atomic->clump, atomic);
    }
    atomic->clump = clump;
    clump->atomics.add(&atomic->inClump);
    if (clump->world && !atomic->world) {
        clump->world->addAtomic(atomic);
    }
    return clump;
}

// D: exe 0x74A4C0 unlinks and clears atomic->clump; an atomic of another clump is left alone.
RpClump* RpClumpRemoveAtomic(RpClump* clump, RpAtomic* atomic) {
    if (!clump || !atomic || atomic->clump != clump) {
        return clump;
    }
    if (clump->world && atomic->world == clump->world) {
        clump->world->removeAtomic(atomic);
    }
    atomic->inClump.remove();
    atomic->clump = nullptr;
    return clump;
}

// A: head -> tail; the successor is read before the callback (it may remove/destroy the atomic); NULL from the callback stops. Returns the clump.
RpClump* RpClumpForAllAtomics(RpClump* clump, RpAtomicCallBack callback, void* data) {
    if (!clump || !callback) {
        return clump;
    }
    for (rw::LLLink* lnk = clump->atomics.link.next; lnk != clump->atomics.end();) {
        rw::LLLink* const next = lnk->next;
        if (!callback(rw::Atomic::fromClump(lnk), data)) {
            break;
        }
        lnk = next;
    }
    return clump;
}

RwInt32 RpClumpGetNumAtomics(RpClump* clump) {
    return clump ? clump->countAtomics() : 0;
}

// A: exe 0x749B20: every atomic with rpATOMICRENDER, head -> tail, after syncing its frame's LTM; a callback returning NULL makes the clump
// result NULL but does not stop the loop.
RpClump* RpClumpRender(RpClump* clump) {
    if (!clump) {
        return nullptr;
    }
    RpClump* result = clump;
    for (rw::LLLink* lnk = clump->atomics.link.next; lnk != clump->atomics.end(); lnk = lnk->next) {
        RpAtomic* const atomic = rw::Atomic::fromClump(lnk);
        if (atomic->object.object.flags & rw::Atomic::RENDER) {
            if (atomic->getFrame()) {
                atomic->getFrame()->getLTM();
            }
            if (!RpAtomicRender(atomic)) {
                result = nullptr;
            }
        }
    }
    return result;
}

// A: the game has already consumed the Clump chunk header (RwStreamFindChunk(rwID_CLUMP)); same contract as rw::Clump::streamRead, which
// reads the STRUCT onwards. The lists are put into the exe's (reversed) order, see the file header.
RpClump* RpClumpStreamRead(RwStream* stream) {
    if (!stream) {
        return nullptr;
    }
    RwShimEnsureAtomicRenderSlot();
    RpClump* const clump = rw::Clump::streamRead(stream);
    if (clump) {
        for (rw::LLLink* l = clump->atomics.link.next; l != clump->atomics.end(); l = l->next) {
            RwShimGeometryEnsureMesh(rw::Atomic::fromClump(l)->geometry); // geometry chunks read by librw: see RpGeometryStreamRead
        }
        ReverseList(clump->atomics);
        ReverseList(clump->lights);
        ReverseList(clump->cameras);
    }
    return clump;
}

// A: writes the Clump chunk; the atomics are written in list order (the exe's order). NULL on failure.
RpClump* RpClumpStreamWrite(RpClump* clump, RwStream* stream) {
    if (!clump || !stream) {
        return nullptr;
    }
    return clump->streamWrite(stream) ? clump : nullptr;
}

// D: payload size of the Clump chunk (header excluded), as RW.
RwUInt32 RpClumpStreamGetSize(RpClump* clump) {
    return clump ? clump->streamGetSize() : 0;
}

// ---- lights / cameras (head insertion like the atomics) ----
RpClump* RpClumpAddLight(RpClump* clump, RpLight* light) {
    if (!clump || !light) {
        return nullptr;
    }
    if (light->clump) {
        RpClumpRemoveLight(light->clump, light);
    }
    light->clump = clump;
    clump->lights.add(&light->inClump);
    if (clump->world && !light->world) {
        clump->world->addLight(light);
    }
    return clump;
}

RpClump* RpClumpRemoveLight(RpClump* clump, RpLight* light) {
    if (!clump || !light || light->clump != clump) {
        return clump;
    }
    if (clump->world && light->world == clump->world) {
        clump->world->removeLight(light);
    }
    light->inClump.remove();
    light->clump = nullptr;
    return clump;
}

RpClump* RpClumpForAllLights(RpClump* clump, RpLightCallBack callback, void* data) {
    if (!clump || !callback) {
        return clump;
    }
    for (rw::LLLink* lnk = clump->lights.link.next; lnk != clump->lights.end();) {
        rw::LLLink* const next = lnk->next;
        if (!callback(rw::Light::fromClump(lnk), data)) {
            break;
        }
        lnk = next;
    }
    return clump;
}

RwInt32 RpClumpGetNumLights(RpClump* clump) {
    return clump ? clump->countLights() : 0;
}

RpClump* RpClumpAddCamera(RpClump* clump, RwCamera* camera) {
    if (!clump || !camera) {
        return nullptr;
    }
    if (camera->clump) {
        RpClumpRemoveCamera(camera->clump, camera);
    }
    camera->clump = clump;
    clump->cameras.add(&camera->inClump);
    if (clump->world && !camera->world) {
        clump->world->addCamera(camera);
    }
    return clump;
}

RpClump* RpClumpRemoveCamera(RpClump* clump, RwCamera* camera) {
    if (!clump || !camera || camera->clump != clump) {
        return clump;
    }
    if (clump->world && camera->world == clump->world) {
        clump->world->removeCamera(camera);
    }
    camera->inClump.remove();
    camera->clump = nullptr;
    return clump;
}

RpClump* RpClumpForAllCameras(RpClump* clump, RwCameraCallBack callback, void* data) {
    if (!clump || !callback) {
        return clump;
    }
    for (rw::LLLink* lnk = clump->cameras.link.next; lnk != clump->cameras.end();) {
        rw::LLLink* const next = lnk->next;
        if (!callback(rw::Camera::fromClump(lnk), data)) {
            break;
        }
        lnk = next;
    }
    return clump;
}

RwInt32 RpClumpGetNumCameras(RpClump* clump) {
    return clump ? clump->countCameras() : 0;
}
// ================================================================================================================================
// P2B-04d: SA's split clump reader (used for the "big model" streaming path: CFileLoader::StartLoadClumpFile / FinishLoadClumpFile and
// CStreaming::RemoveModel -> RpClumpGtaCancelStream). The exe reads the front of a DFF while only the first half of the file is in memory
// and finishes the clump later from a FRESH stream over the whole file:
//   Read1 (0x72E570, the stream is already inside the Clump chunk): STRUCT (12 bytes: numAtomics / numLights / numCameras), FRAMELIST (frames
//     built, parked), GEOMETRYLIST: STRUCT (numGeometries), then ONLY THE FIRST numGeometries/2 geometries (RpGeometryStreamRead, with the
//     geometry plugins: 2dfx / breakable / extra colours / materials' env+spec). The stream position after them is remembered. True on success.
//   Read2 (0x72E620): a new clump, the stream (position 0 of the fresh stream) is advanced to the remembered position, the remaining geometries
//     [numGeometries/2, numGeometries) are read, the clump takes frames[0] as its frame, then numAtomics ATOMIC chunks are read with 0x72E270
//     and added at the head. 0x72E270 reads only the 16-byte atomic STRUCT (frame index, geometry index, flags) and sets frame + geometry --
//     the atomic's EXTENSION (pipeline id, rights) is NOT read, and neither are lights, cameras or the clump's own extension (whose plugin
//     chunks are therefore left to the file's skipped bytes). Parked geometry references / frame array are released.
//   Cancel (0x72E700): releases whatever Read1 parked (geometry references; the exe leaks the parked frames, here they are destroyed).
// One clump can be parked at a time (static state in the exe too); a second Read1 drops the previous one (the exe leaks it). Read2 without a
// Read1 returns NULL (undefined in the exe). Indices in the atomic struct are range-checked here.
namespace {
struct ParkedClump {
    bool                      active = false;
    int32_t                   numAtomics = 0, numGeometries = 0, half = 0;
    uint32_t                  position = 0; // stream position after the first half
    rw::FrameList_            frames{0, nullptr};
    std::vector<rw::Geometry*> geometries;

    void ReleaseGeometries() {
        for (rw::Geometry* g : geometries) {
            if (g) {
                RpGeometryDestroy(g);
            }
        }
        geometries.clear();
    }
    void ReleaseFrameArray() {
        if (frames.frames) {
            rwFree(frames.frames);
        }
        frames = {0, nullptr};
    }
    void DestroyFrames() { // frames nobody adopted
        if (frames.frames && frames.numFrames > 0 && frames.frames[0]) {
            DestroyFrameHierarchy(frames.frames[0]);
        }
        ReleaseFrameArray();
    }
    void Reset() {
        ReleaseGeometries();
        ReleaseFrameArray();
        active = false;
    }
};
ParkedClump g_parked;

bool ReadGeometries(rw::Stream* stream, int32_t from, int32_t to) {
    for (int32_t i = from; i < to; i++) {
        if (!rw::findChunk(stream, rw::ID_GEOMETRY, nullptr, nullptr)) {
            return false;
        }
        rw::Geometry* const g = RpGeometryStreamRead(stream); // exe 0x74D190 (builds the mesh header when the file has no Bin Mesh chunk)
        if (!g) {
            return false;
        }
        g_parked.geometries[i] = g;
    }
    return true;
}

// 0x72E270 (see above). Index fields are bounds-checked.
RpAtomic* ReadAtomicStruct(rw::Stream* stream) {
    uint32_t len = 0, ver = 0;
    if (!rw::findChunk(stream, rw::ID_STRUCT, &len, &ver) || len < 16) {
        return nullptr;
    }
    int32_t buf[4] = {};
    if (stream->read8(buf, 16) != 16) {
        return nullptr;
    }
    if (len > 16) {
        stream->seek(static_cast<int32_t>(len - 16));
    }
    const bool ok = buf[0] >= 0 && buf[0] < g_parked.frames.numFrames && buf[1] >= 0 && buf[1] < g_parked.numGeometries && g_parked.geometries[buf[1]];
    if (!ok) {
        return nullptr;
    }
    RpAtomic* const atomic = RpAtomicCreate();
    if (!atomic) {
        return nullptr;
    }
    atomic->object.object.flags = static_cast<uint8_t>(buf[2]);
    RpAtomicSetFrame(atomic, g_parked.frames.frames[buf[0]]);
    RpAtomicSetGeometry(atomic, g_parked.geometries[buf[1]], 0);
    return atomic;
}
} // namespace

bool RpClumpGtaStreamRead1(RwStream* stream) {
    g_parked.DestroyFrames();
    g_parked.Reset();
    if (!stream) {
        return false;
    }
    uint32_t len = 0, ver = 0;
    int32_t  counts[3] = {};
    if (!rw::findChunk(stream, rw::ID_STRUCT, &len, &ver) || len < 12 || stream->read8(counts, 12) != 12) {
        return false;
    }
    if (len > 12) {
        stream->seek(static_cast<int32_t>(len - 12));
    }
    if (!rw::findChunk(stream, rw::ID_FRAMELIST, nullptr, nullptr) || !g_parked.frames.streamRead(stream)) {
        g_parked.DestroyFrames();
        return false;
    }
    if (!rw::findChunk(stream, rw::ID_GEOMETRYLIST, nullptr, nullptr) || !rw::findChunk(stream, rw::ID_STRUCT, nullptr, nullptr)) {
        g_parked.DestroyFrames();
        return false;
    }
    g_parked.numAtomics    = counts[0];
    g_parked.numGeometries = stream->readI32();
    if (g_parked.numGeometries < 0 || g_parked.numGeometries > 0x10000) {
        g_parked.DestroyFrames();
        return false;
    }
    g_parked.half = g_parked.numGeometries / 2;
    g_parked.geometries.assign(g_parked.numGeometries, nullptr);
    if (!ReadGeometries(stream, 0, g_parked.half)) {
        g_parked.ReleaseGeometries();
        g_parked.DestroyFrames();
        return false;
    }
    g_parked.position = stream->tell();
    g_parked.active   = true;
    return true;
}

RpClump* RpClumpGtaStreamRead2(RwStream* stream) {
    if (!stream || !g_parked.active) {
        return nullptr;
    }
    RpClump* const clump = RpClumpCreate();
    if (!clump) {
        g_parked.DestroyFrames();
        g_parked.Reset();
        return nullptr;
    }
    RwShimEnsureAtomicRenderSlot();
    stream->seek(static_cast<int32_t>(g_parked.position) - static_cast<int32_t>(stream->tell()));
    if (!ReadGeometries(stream, g_parked.half, g_parked.numGeometries)) {
        g_parked.ReleaseGeometries();
        g_parked.DestroyFrames();
        g_parked.active = false;
        RpClumpDestroy(clump);
        return nullptr;
    }
    if (g_parked.frames.numFrames > 0) {
        clump->setFrame(g_parked.frames.frames[0]); // from here on the clump owns the frames
    }
    for (int32_t i = 0; i < g_parked.numAtomics; i++) {
        RpAtomic* atomic = nullptr;
        if (!rw::findChunk(stream, rw::ID_ATOMIC, nullptr, nullptr) || !(atomic = ReadAtomicStruct(stream))) {
            g_parked.Reset();
            RpClumpDestroy(clump);
            return nullptr;
        }
        RpClumpAddAtomic(clump, atomic);
    }
    g_parked.Reset(); // geometry references: the atomics hold their own
    return clump;
}

void RpClumpGtaCancelStream() {
    g_parked.DestroyFrames();
    g_parked.Reset();
}
#endif
