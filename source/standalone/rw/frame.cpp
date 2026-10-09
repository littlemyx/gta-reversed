// P2B-02b: RwFrame* on top of librw (rw::Frame).
//   RwFrame{Create,Destroy,AddChild,RemoveChild,Count,ForAllChildren,ForAllObjects,GetLTM,OrthoNormalize,Rotate,Translate,Transform,
//           SetIdentity,UpdateObjects,RegisterPlugin,RegisterPluginStream}, _rwFrameCloneAndLinkClones.
// Function classes (see .notes/P2B_SHIM_PLAN.md): D = direct, A = adapter, W = written here because librw has nothing equivalent.
// librw's frame hierarchy is a port of RW's: same private flags (HIERARCHYSYNCLTM/OBJ, SUBTREESYNCLTM/OBJ), same dirty-list protocol
// (RwFrameUpdateObjects marks the root dirty; Frame::syncDirty, run by Camera::beginUpdate, refreshes the LTMs and the attached objects),
// same child order (AddChild prepends). Not here (not called by the game outside the stock RW headers): RwFrame{Scale,Dirty,GetRoot,
// CloneHierarchy,DestroyHierarchy,AddChildNoUpdate}, _rwFramePurgeClone, user-data arrays, RwFrameSetFreeListCreateParams.
//
// Only needs fakerw + librw + the CRT (also built by the PCH-less unit test tests/standalone/rw_frame_camera_test.cpp); under the game
// target the PCH is force-included.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

#include <cassert>
#include <cmath>

namespace {
inline rw::CombineOp ToCombine(RwOpCombineType c) { return static_cast<rw::CombineOp>(c); }
}

// D (+ null check): Frame::create. The matrix starts as identity, the LTM as identity, the frame is its own root.
RwFrame* RwFrameCreate(void) {
    return rw::Frame::create();
}

// A: RW orphans the children (they are NOT destroyed) and detaches the attached objects. librw's Frame::destroy does the same but leaves
// the orphans' `root` pointing at the freed frame (the next updateObjects() on an orphan would write through it), so every child becomes
// the root of its own hierarchy here first.
RwBool RwFrameDestroy(RwFrame* frame) {
    if (!frame) {
        return FALSE;
    }
    RwFrame* next = nullptr;
    for (RwFrame* child = frame->child; child; child = next) {
        next              = child->next;
        child->object.parent = nullptr;
        child->next          = nullptr;
        child->setHierarchyRoot(child);
        child->updateObjects();
    }
    frame->child = nullptr;
    frame->destroy();
    return TRUE;
}

// D: returns `parent` (RW), a child that already has a parent is moved; the new root's dirty flags are merged.
RwFrame* RwFrameAddChild(RwFrame* parent, RwFrame* child) {
    if (!parent || !child || parent == child) {
        return nullptr;
    }
    return parent->addChild(child);
}

// D: returns `child` (RW). A frame without a parent is returned untouched (librw would dereference the null parent).
RwFrame* RwFrameRemoveChild(RwFrame* child) {
    if (!child || !child->getParent()) {
        return child;
    }
    return child->removeChild();
}

// D: number of frames in the hierarchy including `frame`.
RwInt32 RwFrameCount(RwFrame* frame) {
    return frame ? frame->count() : 0;
}

// D: callback may remove/destroy the frame it receives (librw saves `next` first); stops when it returns null; always returns `frame`.
RwFrame* RwFrameForAllChildren(RwFrame* frame, RwFrameCallBack callBack, void* data) {
    if (!frame || !callBack) {
        return frame;
    }
    return frame->forAllChildren(callBack, data);
}

// A: iterate the frame's object list (rw::ObjectWithFrame::inFrame links); RwObject* == ObjectWithFrame* (the object header is the first
// member). The successor is read before the callback so it may detach/destroy its object (RpAtomicSetFrame(.., nullptr)).
RwFrame* RwFrameForAllObjects(RwFrame* frame, RwObjectCallBack callBack, void* data) {
    if (!frame || !callBack) {
        return frame;
    }
    rw::LLLink* const end = frame->objectList.end();
    for (rw::LLLink* lnk = frame->objectList.link.next; lnk != end;) {
        rw::LLLink* const next = lnk->next;
        RwObject*         obj  = &rw::ObjectWithFrame::fromFrame(lnk)->object;
        if (!callBack(obj, data)) {
            break;
        }
        lnk = next;
    }
    return frame;
}

// D: synchronises the hierarchy's LTMs first when dirty (root flag HIERARCHYSYNCLTM), then returns frame->ltm.
RwMatrix* RwFrameGetLTM(RwFrame* frame) {
    return frame ? frame->getLTM() : nullptr;
}

// W: RW's RwMatrixOrthoNormalize on the modelling matrix: `at` is kept (normalised), right = up x at (normalised), up = at x right;
// the translation is untouched. Marks the matrix orthonormal and drops the identity flag. (librw has no equivalent.)
RwFrame* RwFrameOrthoNormalize(RwFrame* frame) {
    if (!frame) {
        return nullptr;
    }
    rw::Matrix& m = frame->matrix;
    rw::V3d at    = rw::normalize(m.at);
    rw::V3d right = rw::normalize(rw::cross(m.up, at));
    rw::V3d up    = rw::cross(at, right);
    m.right = right;
    m.up    = up;
    m.at    = at;
    m.flags = (m.flags & ~(static_cast<RwUInt32>(rw::Matrix::IDENTITY) | static_cast<RwUInt32>(rw::Matrix::TYPEMASK))) | rw::Matrix::TYPEORTHONORMAL;
    frame->updateObjects();
    return frame;
}

// D: angle in degrees, combine as RwOpCombineType (static_assert'ed against rw::CombineOp in fakerw.h); marks the hierarchy dirty.
RwFrame* RwFrameRotate(RwFrame* frame, const RwV3d* axis, RwReal angle, RwOpCombineType combine) {
    if (!frame || !axis) {
        return nullptr;
    }
    frame->rotate(axis, angle, ToCombine(combine));
    return frame;
}

RwFrame* RwFrameTranslate(RwFrame* frame, const RwV3d* v, RwOpCombineType combine) {
    if (!frame || !v) {
        return nullptr;
    }
    frame->translate(v, ToCombine(combine));
    return frame;
}

RwFrame* RwFrameTransform(RwFrame* frame, const RwMatrix* m, RwOpCombineType combine) {
    if (!frame || !m) {
        return nullptr;
    }
    frame->transform(m, ToCombine(combine));
    return frame;
}

// A: Matrix::setIdentity + updateObjects (RW: RwMatrixSetIdentity(&frame->modelling); RwFrameUpdateObjects).
RwFrame* RwFrameSetIdentity(RwFrame* frame) {
    if (!frame) {
        return nullptr;
    }
    frame->matrix.setIdentity();
    frame->updateObjects();
    return frame;
}

// D: marks the hierarchy root dirty (and puts it on the engine's dirty list) and the frame's subtree as unsynced. The actual sync happens
// in RwCameraBeginUpdate / RwFrameGetLTM.
RwFrame* RwFrameUpdateObjects(RwFrame* frame) {
    if (!frame) {
        return nullptr;
    }
    frame->updateObjects();
    return frame;
}

// D: clone `root`'s hierarchy; each ORIGINAL frame's `root` field is left pointing at its clone (the game reads it in CloneAtomicToClumpCB
// to find the clone of an atomic's frame; RW has no purge call there either).
RwFrame* _rwFrameCloneAndLinkClones(RwFrame* root) {
    return root ? root->cloneAndLink() : nullptr;
}

// A: Frame::registerPlugin. RW's callback types differ from librw's only by const (copy source, write/getSize object); same calling
// convention and layout, so the pointers are reinterpreted. Returns the plugin's byte offset in the frame, -1 on failure (as RW).
// Registration changes sizeof(frame): no frame may exist yet (the game attaches its plugins right after RwEngineOpen, before any RwFrameCreate).
RwInt32 RwFrameRegisterPlugin(RwInt32 size, RwUInt32 pluginID, RwPluginObjectConstructor constructCB, RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB) {
    assert(rw::Frame::numAllocated == 0 && "RwFrameRegisterPlugin after a frame was created");
    return rw::Frame::registerPlugin(size, pluginID, reinterpret_cast<rw::Constructor>(constructCB), reinterpret_cast<rw::Destructor>(destructCB),
                                     reinterpret_cast<rw::CopyConstructor>(copyCB));
}

RwInt32 RwFrameRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB) {
    return rw::Frame::registerPluginStream(pluginID, reinterpret_cast<rw::StreamRead>(readCB), reinterpret_cast<rw::StreamWrite>(writeCB),
                                           reinterpret_cast<rw::StreamGetSize>(getSizeCB));
}
#endif
