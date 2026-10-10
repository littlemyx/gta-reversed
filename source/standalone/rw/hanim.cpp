// P2B-05a: RpHAnim* on top of librw (rw::HAnimHierarchy / rw::HAnimData / rw::AnimInterpolator).
//   RpHAnimPluginAttach, RpHAnimFrame{Set,Get}Hierarchy, RpHAnimFrame{Set,Get}ID, RpHAnimHierarchy{Create,CreateFromHierarchy,Destroy,Attach,Detach,
//   AttachFrameIndex,DetachFrameIndex,GetMatrixArray,GetNodeMatrix,UpdateMatrices}, RpHAnimIDGetIndex and the six standard key-frame callbacks the game
//   registers in its RtAnim interpolation scheme (RpAnimBlend.cpp:88): RpHAnimKeyFrame{Blend,Add,MulRecip,StreamRead,StreamWrite,StreamGetSize}.
// Verified against the exe (all in 0x7C4600..0x7C6800): PluginAttach 0x7C4600, Create 0x7C4C30, CreateFromHierarchy 0x7C4ED0, Destroy 0x7C4D30, Attach 0x7C4F40,
// Detach 0x7C4FF0, AttachFrameIndex 0x7C5020, DetachFrameIndex 0x7C5100, FrameSetHierarchy 0x7C5130, UpdateMatrices 0x7C51D0, IDGetIndex 0x7C51A0,
// KeyFrame{StreamRead 0x7C64C0, StreamWrite 0x7C6540, StreamGetSize 0x7C65B0, MulRecip 0x7C65C0, Blend 0x7C60C0, Add 0x7C6720}.
//
// The frame plugin itself (extension {id, hierarchy}, constructor / destructor / copy / stream) is librw's and matches the exe's (checked against
// 0x7C4750 / 0x7C4770 / 0x7C4830 / 0x7C4A00): the destructor detaches the nodes and frees a hierarchy owned by the frame, the copy clones a
// non-sub hierarchy. Differences from the exe that are deliberate:
//   * sub-hierarchies (rpHANIMHIERARCHYSUBHIERARCHY, RpHAnimHierarchyCreateSubHierarchy) are not supported: librw has no rootParentOffset, the game never
//     creates one. UpdateMatrices treats every hierarchy as a root hierarchy.
//   * the hierarchy node ids of a hierarchy made by Create(nodeIDs = NULL) are 0 (the exe leaves them uninitialised).
// NOT here (the game does not call them outside the stock RW headers): RpHAnimHierarchy{CreateSubHierarchy,Blend,Add,Copy,...}, RpHAnimKeyFrameApply /
// Interpolate / ToMatrix (the game registers its own, RtAnimBlendKeyFrameApply / RpAnimBlendKeyFrameInterpolate), RpHAnimAnimation*.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
#include "rwtrig.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

namespace {
// RW's RpHAnimStdKeyFrame / RpHAnimStdInterpFrame: {pointer, pointer-or-time, quaternion x y z w, translation} = librw's HAnimKeyFrame / HAnimInterpFrame
// (36 bytes). The exe's callbacks address both at the same offsets: q at +8, t at +0x18.
using StdFrame = rw::HAnimInterpFrame;
static_assert(sizeof(rw::HAnimKeyFrame) == 0x24 && sizeof(StdFrame) == 0x24 && offsetof(StdFrame, q) == 8 && offsetof(StdFrame, t) == 0x18 &&
              offsetof(rw::HAnimKeyFrame, q) == 8 && offsetof(rw::HAnimKeyFrame, t) == 0x18 && offsetof(rw::HAnimKeyFrame, time) == 4);

inline rw::HAnimData* Ext(RwFrame* f) { return rw::HAnimData::get(f); }

// Marks `frame` as having changed objects (RwFrameUpdateObjects) and attaches every node whose id equals the frame's id, recursing over the
// children. `onlyId` >= 0 restricts the nodes to that id (AttachFrameIndex), -1 = any (Attach).
struct AttachCtx {
    RpHAnimHierarchy* hier;
    RwInt32           onlyId;
};

void AttachNodesOf(RwFrame* frame, const AttachCtx& ctx) {
    const RwInt32 frameId = Ext(frame)->id;
    for (RwInt32 i = 0; i < ctx.hier->numNodes; i++) {
        rw::HAnimNodeInfo& n = ctx.hier->nodeInfo[i];
        if (frameId == n.id && (ctx.onlyId < 0 || n.id == ctx.onlyId)) {
            n.frame = frame;
        }
    }
}

RwFrame* AttachCB(RwFrame* frame, void* data) {
    const AttachCtx& ctx = *static_cast<AttachCtx*>(data);
    AttachNodesOf(frame, ctx);
    RwFrameUpdateObjects(frame);
    RwFrameForAllChildren(frame, AttachCB, data);
    return frame;
}

// RW's "matrix of the frame above the hierarchy root": the parent's LTM; when its hierarchy is dirty the LTM is not valid and the exe multiplies the
// modelling matrices up the chain instead (so reading it does not synchronise anything).
void ComputeRootMatrix(RwFrame* hierRoot, RwMatrix& out) {
    RwFrame* parent = hierRoot ? hierRoot->getParent() : nullptr;
    if (!parent) {
        out.setIdentity();
        return;
    }
    if (parent->dirty()) {
        out = parent->matrix;
        for (RwFrame* a = parent->getParent(); a; a = a->getParent()) {
            const RwMatrix tmp = out;
            RwMatrixMultiply(&out, &tmp, &a->matrix);
        }
    } else {
        out = parent->ltm;
    }
}
} // namespace

// W: registers librw's HAnim plugin (frame extension + the standard interpolation scheme id 1) once per Engine::init cycle. The game's order is
// RwEngineInit -> PluginAttach -> RwEngineOpen, which librw needs: engine / frame plugins must exist before the engine is opened.
RwBool RpHAnimPluginAttach() {
    if (rw::Frame::s_plglist.getPluginOffset(rw::ID_HANIM) < 0) {
        rw::registerHAnimPlugin();
    }
    return rw::Frame::s_plglist.getPluginOffset(rw::ID_HANIM) >= 0 ? TRUE : FALSE;
}

// D: exe 0x7C5130. The previous hierarchy of the frame loses its parent, the new one gets this frame.
RwBool RpHAnimFrameSetHierarchy(RwFrame* frame, RpHAnimHierarchy* hierarchy) {
    rw::HAnimData* ext = Ext(frame);
    if (ext->hierarchy) {
        ext->hierarchy->parentFrame = nullptr;
    }
    ext->hierarchy = hierarchy;
    if (hierarchy) {
        hierarchy->parentFrame = frame;
    }
    return TRUE;
}

RpHAnimHierarchy* RpHAnimFrameGetHierarchy(RwFrame* frame) {
    return Ext(frame)->hierarchy;
}

RwBool RpHAnimFrameSetID(RwFrame* frame, RwInt32 id) {
    Ext(frame)->id = id;
    return TRUE;
}

RwInt32 RpHAnimFrameGetID(RwFrame* frame) {
    return Ext(frame)->id;
}

// A: HAnimHierarchy::create (also creates the AnimInterpolator, 16-byte aligned matrix array and the node array).
RpHAnimHierarchy* RpHAnimHierarchyCreate(RwInt32 numNodes, RwUInt32* nodeFlags, RwInt32* nodeIDs, RpHAnimHierarchyFlag flags, RwInt32 maxInterpKeyFrameSize) {
    return rw::HAnimHierarchy::create(numNodes, reinterpret_cast<rw::int32*>(nodeFlags), nodeIDs, flags, maxInterpKeyFrameSize);
}

// W: exe 0x7C4ED0 = Create(src->numNodes, NULL, NULL, flags, maxKey) + per node {flags, index, id} copied from `hierarchy`; no frame attached.
RpHAnimHierarchy* RpHAnimHierarchyCreateFromHierarchy(RpHAnimHierarchy* hierarchy, RpHAnimHierarchyFlag flags, RwInt32 maxInterpKeyFrameSize) {
    RpHAnimHierarchy* h = rw::HAnimHierarchy::create(hierarchy->numNodes, nullptr, nullptr, flags, maxInterpKeyFrameSize);
    if (!h) {
        return nullptr;
    }
    for (RwInt32 i = 0; i < h->numNodes; i++) {
        h->nodeInfo[i].frame = nullptr;
        h->nodeInfo[i].flags = hierarchy->nodeInfo[i].flags;
        h->nodeInfo[i].index = hierarchy->nodeInfo[i].index;
        h->nodeInfo[i].id    = hierarchy->nodeInfo[i].id;
    }
    return h;
}

// A: exe 0x7C4D30. Frees the hierarchy and clears the extension of its parent frame; returns NULL (the exe's `xor eax, eax`).
RpHAnimHierarchy* RpHAnimHierarchyDestroy(RpHAnimHierarchy* hierarchy) {
    RwFrame* parent = hierarchy->parentFrame;
    hierarchy->destroy();
    if (parent) {
        Ext(parent)->hierarchy = nullptr;
    }
    return nullptr;
}

// W: exe 0x7C4F40. Unlike librw's attach (first UNattached frame with the id), RW sets pFrame of EVERY node whose id equals the id of each frame of the
// hierarchy (pre-order from the parent frame, itself included), later frames overwrite earlier ones; each visited frame gets RwFrameUpdateObjects.
RpHAnimHierarchy* RpHAnimHierarchyAttach(RpHAnimHierarchy* hierarchy) {
    RwFrame* root = hierarchy->parentFrame;
    AttachCtx ctx{hierarchy, -1};
    AttachNodesOf(root, ctx);
    RwFrameUpdateObjects(root);
    RwFrameForAllChildren(root, AttachCB, &ctx);
    return hierarchy;
}

RpHAnimHierarchy* RpHAnimHierarchyDetach(RpHAnimHierarchy* hierarchy) {
    for (RwInt32 i = 0; i < hierarchy->numNodes; i++) {
        hierarchy->nodeInfo[i].frame = nullptr;
    }
    return hierarchy;
}

// W: exe 0x7C5020 (callback 0x7C50A0): as Attach, but only the nodes with the id of node `nodeIndex`.
RpHAnimHierarchy* RpHAnimHierarchyAttachFrameIndex(RpHAnimHierarchy* hierarchy, RwInt32 nodeIndex) {
    RwFrame* root = hierarchy->parentFrame;
    AttachCtx ctx{hierarchy, hierarchy->nodeInfo[nodeIndex].id};
    AttachNodesOf(root, ctx);
    RwFrameUpdateObjects(root);
    RwFrameForAllChildren(root, AttachCB, &ctx);
    return hierarchy;
}

RpHAnimHierarchy* RpHAnimHierarchyDetachFrameIndex(RpHAnimHierarchy* hierarchy, RwInt32 nodeIndex) {
    hierarchy->nodeInfo[nodeIndex].frame = nullptr;
    return hierarchy;
}

// D: exe 0x7C5120 (pMatrixArray; NULL for rpHANIMHIERARCHYNOMATRICES).
RwMatrix* RpHAnimHierarchyGetMatrixArray(RpHAnimHierarchy* hierarchy) {
    return hierarchy->matrices;
}

// NOTSA helper of the game (rphanim.cpp): matrix of node `nodeID`.
RwMatrix* RpHAnimHierarchyGetNodeMatrix(RpHAnimHierarchy* hierarchy, RwInt32 nodeID) {
    const RwInt32 i = hierarchy->getIndex(nodeID);
    assert(i >= 0 && i < hierarchy->numNodes);
    return i >= 0 ? &hierarchy->matrices[i] : nullptr;
}

// D: exe 0x7C51A0, linear search by node id, -1 when absent.
RwInt32 RpHAnimIDGetIndex(RpHAnimHierarchy* hierarchy, RwInt32 ID) {
    return hierarchy->getIndex(static_cast<rw::int32>(ID));
}

// W: exe 0x7C51D0 (librw's updateMatrices skips the frame write-back, LTMs, local space and NOMATRICES). Per node: `animMat` = applyCB(interpolated frame),
// `cur = animMat x parent` (into pMatrixArray[i]), then the attached frame (pFrame) is written:
//   UPDATEMODELLINGMATRICES: frame modelling = animMat (+ RwFrameUpdateObjects unless UPDATELTMS also set),
//   UPDATELTMS:              frame ltm = cur (LOCALSPACEMATRICES: cur x rootMatrix), SUBTREESYNCLTM cleared / SUBTREESYNCOBJ set; the hierarchy root is put on
//                            the engine's dirty list with HIERARCHYSYNCOBJ only (the LTMs are already valid),
// then the matrix stack is maintained from the node flags (POP = 1: parent = stack pop; PUSH = 2: push parent, parent = cur; both: unchanged; none: parent = cur).
// Root matrix: the LTM of the frame above parentFrame (identity if none); LOCALSPACEMATRICES starts from identity and only uses the root matrix for the LTMs.
// rpHANIMHIERARCHYNOMATRICES: no matrix array, the same walk on stack copies, LTM = cur.
RwBool RpHAnimHierarchyUpdateMatrices(RpHAnimHierarchy* hierarchy) {
    const RwInt32 flags = hierarchy->flags;
    const bool wantMod  = (flags & rpHANIMHIERARCHYUPDATEMODELLINGMATRICES) != 0;
    const bool wantLTM  = (flags & rpHANIMHIERARCHYUPDATELTMS) != 0;
    const bool local    = (flags & rpHANIMHIERARCHYLOCALSPACEMATRICES) != 0;
    const bool noMat    = (flags & rpHANIMHIERARCHYNOMATRICES) != 0;
    rw::AnimInterpolator* interp = hierarchy->interpolator;
    RwFrame* const parentFrame   = hierarchy->parentFrame;

    RwMatrix rootMat;
    rootMat.setIdentity();
    RwMatrix identity;
    identity.setIdentity();
    const bool needRoot = noMat || !local || wantLTM;
    if (needRoot) {
        ComputeRootMatrix(parentFrame, rootMat);
    }
    if (wantLTM && parentFrame) {
        RwFrame* root = parentFrame->root;
        if (!(root->object.privateFlags & rw::Frame::HIERARCHYSYNC)) {
            rw::engine->frameDirtyList.add(&root->inDirtyList);
            root->object.privateFlags |= rw::Frame::HIERARCHYSYNCOBJ;
        }
    }

    auto computeAnim = [&](RwInt32 i, RwMatrix& animMat) {
        if (interp && interp->applyCB && interp->currentAnim) {
            interp->applyCB(&animMat, interp->getInterpFrame(i));
        } else {
            animMat.setIdentity();
        }
    };
    auto writeFrame = [&](RwFrame* frame, const RwMatrix& animMat, const RwMatrix& cur, bool localLtm) {
        if (!frame) {
            return;
        }
        if (wantMod) {
            frame->matrix = animMat;
            if (!wantLTM) {
                RwFrameUpdateObjects(frame);
                return;
            }
        } else if (!wantLTM) {
            return;
        }
        if (localLtm) {
            RwMatrixMultiply(&frame->ltm, &cur, &rootMat);
        } else {
            frame->ltm = cur;
        }
        frame->object.privateFlags = (frame->object.privateFlags & ~rw::Frame::SUBTREESYNCLTM) | rw::Frame::SUBTREESYNCOBJ;
    };

    constexpr int kMaxDepth = 256;   // the exe's pointer stack (matrix path) holds ~600 entries before it hits the return address; real rigs nest < 20
    if (!noMat) {
        RwMatrix* stack[kMaxDepth];
        int sp = 0;
        const RwMatrix* parent = local ? &identity : &rootMat;
        RwMatrix* array = hierarchy->matrices;
        for (RwInt32 i = 0; i < hierarchy->numNodes; i++) {
            RwMatrix animMat;
            computeAnim(i, animMat);
            RwMatrix* cur = &array[i];
            RwMatrixMultiply(cur, &animMat, parent);
            const rw::HAnimNodeInfo& node = hierarchy->nodeInfo[i];
            writeFrame(node.frame, animMat, *cur, local);
            switch (node.flags & 3) {
            case 0: parent = cur; break;
            case rpHANIMPOPPARENTMATRIX: parent = sp > 0 ? stack[--sp] : parent; break; // (an unbalanced final pop reads garbage in the exe, harmless there)
            case rpHANIMPUSHPARENTMATRIX: assert(sp < kMaxDepth); stack[sp++] = const_cast<RwMatrix*>(parent); parent = cur; break;
            default: break; // push + pop: parent unchanged
            }
        }
    } else {
        RwMatrix stack[kMaxDepth];
        int sp = 0;
        RwMatrix parent = rootMat;
        for (RwInt32 i = 0; i < hierarchy->numNodes; i++) {
            RwMatrix animMat, cur;
            computeAnim(i, animMat);
            RwMatrixMultiply(&cur, &animMat, &parent);
            const rw::HAnimNodeInfo& node = hierarchy->nodeInfo[i];
            writeFrame(node.frame, animMat, cur, false);
            switch (node.flags & 3) {
            case 0: parent = cur; break;
            case rpHANIMPOPPARENTMATRIX: if (sp > 0) { parent = stack[--sp]; } break;
            case rpHANIMPUSHPARENTMATRIX: assert(sp < kMaxDepth); stack[sp++] = parent; parent = cur; break;
            default: break;
            }
        }
    }
    return TRUE;
}

//--------------------------------------------------------------------------------------------------
// Key-frame callbacks of the standard (hanim) scheme, all on {header 8 bytes, quaternion x y z w, translation x y z}.
//--------------------------------------------------------------------------------------------------

// W: exe 0x7C60C0. out.t = lerp(in1.t, in2.t, alpha); out.q = slerp(in1.q, in2.q, alpha) with the sign of in2.q flipped to the shorter arc (IN PLACE, as the
// exe does; a NaN dot product does not flip: fcom + test ah,5 + jp) and a linear mix when the quaternions are closer than 0.999 (a NaN dot takes the slerp
// path: C0 is also set for unordered). The slerp uses the exe's inlined RwACos (FreeBSD e_acosf polynomials) and RwSinMinusPiToPi minimax polynomial, not
// libm: omega = acos(cosom), invSin = 1 / sin(omega), scale0 = sin(omega (1 - alpha)) invSin, scale1 = sin(omega alpha) invSin (rwtrig.h; the exe's RwSqrt
// inside the acos is the table based rwx::Sqrt 0x7EDB30, which rwtrig::ACos uses too).
void RpHAnimKeyFrameBlend(void* voidOut, void* voidIn1, void* voidIn2, RwReal alpha) {
    StdFrame* out = static_cast<StdFrame*>(voidOut);
    StdFrame* in1 = static_cast<StdFrame*>(voidIn1);
    StdFrame* in2 = static_cast<StdFrame*>(voidIn2);
    float cosom = ((in2->q.y * in1->q.y + in2->q.x * in1->q.x) + in2->q.z * in1->q.z) + in2->q.w * in1->q.w;
    out->t.x = (in2->t.x - in1->t.x) * alpha + in1->t.x;
    out->t.y = (in2->t.y - in1->t.y) * alpha + in1->t.y;
    out->t.z = (in2->t.z - in1->t.z) * alpha + in1->t.z;
    if (cosom < 0.0f) {
        cosom = -cosom;
        in2->q.x = -in2->q.x;
        in2->q.y = -in2->q.y;
        in2->q.z = -in2->q.z;
        in2->q.w = -in2->q.w;
    }
    float scale0 = 1.0f - alpha;
    float scale1 = alpha;
    if (!(cosom >= 0.999f)) {
        const double omega  = rwtrig::ACos(cosom);
        const double invSin = 1.0 / rwtrig::SinMinusPiToPi(omega);
        scale0 = (float)(rwtrig::SinMinusPiToPi(omega * scale0) * invSin);
        scale1 = (float)(rwtrig::SinMinusPiToPi(omega * alpha) * invSin);
    }
    out->q.x = scale1 * in2->q.x + scale0 * in1->q.x;
    out->q.y = scale1 * in2->q.y + scale0 * in1->q.y;
    out->q.z = scale1 * in2->q.z + scale0 * in1->q.z;
    out->q.w = scale1 * in2->q.w + scale0 * in1->q.w;
}

// W: exe 0x7C6720. out.q = in1.q x in2.q (Hamilton product: w = w1 w2 - v1.v2, v = w1 v2 + w2 v1 + v1 x v2; the exe's term order), out.t = in1.t + in2.t. `out` must not alias the inputs.
void RpHAnimKeyFrameAdd(void* voidOut, void* voidIn1, void* voidIn2) {
    StdFrame* out = static_cast<StdFrame*>(voidOut);
    const StdFrame* a = static_cast<const StdFrame*>(voidIn1);   // ecx
    const StdFrame* b = static_cast<const StdFrame*>(voidIn2);   // eax
    out->q.w = b->q.w * a->q.w - ((b->q.y * a->q.y + b->q.x * a->q.x) + b->q.z * a->q.z);
    out->q.x = a->q.y * b->q.z - b->q.y * a->q.z;
    out->q.y = b->q.x * a->q.z - a->q.x * b->q.z;
    out->q.z = b->q.y * a->q.x - b->q.x * a->q.y;
    out->q.x = a->q.w * b->q.x + out->q.x;
    out->q.y = a->q.w * b->q.y + out->q.y;
    out->q.z = a->q.w * b->q.z + out->q.z;
    out->q.x = b->q.w * a->q.x + out->q.x;
    out->q.y = b->q.w * a->q.y + out->q.y;
    out->q.z = b->q.w * a->q.z + out->q.z;
    out->t.x = a->t.x + b->t.x;
    out->t.y = a->t.y + b->t.y;
    out->t.z = a->t.z + b->t.z;
}

// W: exe 0x7C65C0. frame.q = inverse(start.q) x frame.q (Hamilton, inverse = conjugate / norm^2); frame.t -= start.t. (A zero / NaN start quaternion leaves an uninitialised stack value in the exe;
// the shim uses 0 for the inverse's components.)
void RpHAnimKeyFrameMulRecip(void* voidFrame, void* voidStart) {
    StdFrame* f = static_cast<StdFrame*>(voidFrame);
    const StdFrame* s = static_cast<const StdFrame*>(voidStart);
    const float n = ((s->q.x * s->q.x + s->q.y * s->q.y) + s->q.z * s->q.z) + s->q.w * s->q.w;
    float cx = 0.0f, cy = 0.0f, cz = 0.0f, cw = 0.0f;
    if (n > 0.0f) {
        const float inv = 1.0f / n;
        cw = inv * s->q.w;
        cx = s->q.x * -inv;
        cy = s->q.y * -inv;
        cz = s->q.z * -inv;
    }
    const float fx = f->q.x, fy = f->q.y, fz = f->q.z, fw = f->q.w;
    f->q.w = fw * cw - ((fy * cy + fx * cx) + fz * cz);
    f->q.x = ((fz * cy - fy * cz) + fw * cx) + fx * cw;
    f->q.y = ((fx * cz - fz * cx) + fw * cy) + fy * cw;
    f->q.z = ((fy * cx - fx * cy) + fw * cz) + fz * cw;
    f->t.x -= s->t.x;
    f->t.y -= s->t.y;
    f->t.z -= s->t.z;
}

// W: exe 0x7C64C0. Per key frame: 32 bytes {time, q, t}, then the int32 byte offset of the previous key frame of the same node (divided by 36 -> `prev`).
// Returns NULL when the stream ends early.
RtAnimAnimation* RpHAnimKeyFrameStreamRead(RwStream* stream, RtAnimAnimation* animation) {
    rw::HAnimKeyFrame* frames = static_cast<rw::HAnimKeyFrame*>(animation->keyframes);
    for (RwInt32 i = 0; i < animation->numFrames; i++) {
        if (RwStreamRead(stream, &frames[i].time, 0x20) != 0x20) {
            return nullptr;
        }
        RwUInt32 prevOffset = 0;
        if (RwStreamRead(stream, &prevOffset, 4) != 4) {
            return nullptr;
        }
        frames[i].prev = &frames[prevOffset / sizeof(rw::HAnimKeyFrame)];
    }
    return animation;
}

// W: exe 0x7C6540. Mirror of the reader; the offset written is the byte distance of `prev` from the first key frame.
RwBool RpHAnimKeyFrameStreamWrite(const RtAnimAnimation* animation, RwStream* stream) {
    const rw::HAnimKeyFrame* frames = static_cast<const rw::HAnimKeyFrame*>(animation->keyframes);
    for (RwInt32 i = 0; i < animation->numFrames; i++) {
        if (!RwStreamWrite(stream, &frames[i].time, 0x20)) {
            return FALSE;
        }
        const RwInt32 prevOffset = static_cast<RwInt32>(reinterpret_cast<const char*>(frames[i].prev) - reinterpret_cast<const char*>(frames));
        if (!RwStreamWrite(stream, &prevOffset, 4)) {
            return FALSE;
        }
    }
    return TRUE;
}

// W: exe 0x7C65B0.
RwInt32 RpHAnimKeyFrameStreamGetSize(const RtAnimAnimation* animation) {
    return animation->numFrames * static_cast<RwInt32>(sizeof(rw::HAnimKeyFrame));
}
#endif // NOTSA_RW_LIBRW
