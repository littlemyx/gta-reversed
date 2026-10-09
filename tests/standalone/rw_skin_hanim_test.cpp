// P2B-05a unit test: RpSkin* / RpHAnim* of the librw shim (source/standalone/rw/{skin,hanim}.cpp). Runs under Wine; the engine is opened device-less.
// Synthetic part: skin create / sort / getters, hierarchy create / attach / copy / destroy, UpdateMatrices in every flag combination checked against an independent
// reference (tree walk with explicit parents, librw's Matrix::mult), the six key-frame callbacks against double-precision quaternion math, key-frame stream round trip.
// SA part (optional DFF paths on the command line, e.g. male01.dff wmycr.dff): skin bone count / weights / indices, hierarchy node ids / flags, clone + attach +
// bind-pose UpdateMatrices (skin-to-bone matrix x bone matrix = the skinned atomic's frame LTM).
// Usage: rw_skin_hanim_test.exe [skinned.dff ...]. Exit code 0 = all passed.
#include "fakerw.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do { if (c) { ++g_pass; } else { ++g_fail; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Section(const char* n) { std::printf("[%s]\n", n); }
static bool Near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) <= e; }

static bool MatNear(const RwMatrix& a, const RwMatrix& b, float e = 1e-3f) {
    return Near(a.right.x, b.right.x, e) && Near(a.right.y, b.right.y, e) && Near(a.right.z, b.right.z, e) &&
           Near(a.up.x, b.up.x, e) && Near(a.up.y, b.up.y, e) && Near(a.up.z, b.up.z, e) &&
           Near(a.at.x, b.at.x, e) && Near(a.at.y, b.at.y, e) && Near(a.at.z, b.at.z, e) &&
           Near(a.pos.x, b.pos.x, e) && Near(a.pos.y, b.pos.y, e) && Near(a.pos.z, b.pos.z, e);
}
static bool IsIdentity(const RwMatrix& a, float e = 2e-3f) {
    RwMatrix i; i.setIdentity();
    return MatNear(a, i, e);
}

static std::vector<uint8_t> ReadFile(const char* path) {
    std::vector<uint8_t> b;
    if (FILE* f = std::fopen(path, "rb")) {
        std::fseek(f, 0, SEEK_END); b.resize(std::ftell(f)); std::fseek(f, 0, SEEK_SET);
        b.resize(std::fread(b.data(), 1, b.size(), f)); std::fclose(f);
    }
    return b;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// skin
static void SkinTests() {
    Section("skin: create / getters / set (weights sorted) / atomic slot");
    // 3 vertices, 4 bones; vertex 0: bones 3,1 (0.25 / 0.75: sorted descending by SetSkin), vertex 1: single bone 2 (1.0), vertex 2: bones 0,2,1 (0.2 0.5 0.3)
    float w[12] = { 0.25f, 0.75f, 0, 0,   1.0f, 0, 0, 0,   0.2f, 0.5f, 0.3f, 0 };
    uint8_t ix[12] = { 3, 1, 0, 0,   2, 0, 0, 0,   0, 2, 1, 0 };
    RwMatrix inv[4];
    for (int i = 0; i < 4; i++) { inv[i].setIdentity(); inv[i].pos = { float(i), float(i * 2), float(-i) }; inv[i].flags = 0x20003 + i; }
    RpSkin* skin = RpSkinCreate(3, 4, reinterpret_cast<RwMatrixWeights*>(w), reinterpret_cast<RwUInt32*>(ix), inv);
    CHECK(skin != nullptr);
    CHECK(RpSkinGetNumBones(skin) == 4);
    CHECK(skin->numWeights == 3);
    // used bones in first-seen order: v0 -> 3,1 ; v1 -> 2 ; v2 -> 0 (2 and 1 already) => 3,1,2,0
    CHECK(skin->numUsedBones == 4 && skin->usedBones[0] == 3 && skin->usedBones[1] == 1 && skin->usedBones[2] == 2 && skin->usedBones[3] == 0);
    const RwMatrix* sm = RpSkinGetSkinToBoneMatrices(skin);
    CHECK(sm != nullptr && ((uintptr_t)sm & 15) == 0 && sm[2].pos.y == 4.0f && sm[3].pos.z == -3.0f && sm[3].flags == 0x20003 + 3);   // 16-byte aligned copy, flags / pads kept
    CHECK(std::memcmp(sm, inv, sizeof(inv)) == 0);
    const RwUInt32* bi = RpSkinGetVertexBoneIndices(skin);
    CHECK(bi != nullptr && std::memcmp(bi, ix, 12) == 0 && bi[0] == 0x00000103u && bi[1] == 2u);
    RwMatrixWeights* bw = RpSkinGetVertexBoneWeights(skin);
    CHECK(bw != nullptr && std::memcmp(bw, w, sizeof(w)) == 0 && bw[1].w0 == 1.0f);
    CHECK((void*)bw != (void*)w && (void*)bi != (void*)ix);                                           // copies

    RpGeometry* geo = RpGeometryCreate(3, 1, rpGEOMETRYTRISTRIP);
    CHECK(RpSkinGeometryGetSkin(geo) == nullptr);
    CHECK(RpSkinGeometrySetSkin(geo, skin) == geo);
    CHECK(RpSkinGeometryGetSkin(geo) == skin);
    // sorted descending, indices follow: v0 -> (1: .75, 3: .25), v1 unchanged, v2 -> (2: .5, 1: .3, 0: .2)
    CHECK(Near(bw[0].w0, 0.75f) && Near(bw[0].w1, 0.25f) && bi[0] == 0x00000301u);
    CHECK(Near(bw[1].w0, 1.0f) && bi[1] == 2u);
    CHECK(Near(bw[2].w0, 0.5f) && Near(bw[2].w1, 0.3f) && Near(bw[2].w2, 0.2f) && bi[2] == 0x00000102u);
    const uint8_t* rb = reinterpret_cast<const uint8_t*>(bi);
    CHECK(rb[8] == 2 && rb[9] == 1 && rb[10] == 0);
    CHECK(skin->usedBones[0] == 0 && skin->usedBones[1] == 1 && skin->usedBones[2] == 2 && skin->usedBones[3] == 3);   // used bones ascending after SetSkin
    CHECK(RpSkinGeometrySetSkin(geo, skin) == geo && RpSkinGeometryGetSkin(geo) == skin);              // same skin: no-op

    // atomic slot / pipeline
    RpAtomic* atomic = RpAtomicCreate();
    CHECK(RpSkinAtomicGetHAnimHierarchy(atomic) == nullptr);
    RpHAnimHierarchy* h = RpHAnimHierarchyCreate(2, nullptr, nullptr, (RpHAnimHierarchyFlag)0, 36);
    CHECK(RpSkinAtomicSetHAnimHierarchy(atomic, h) == atomic);
    CHECK(RpSkinAtomicGetHAnimHierarchy(atomic) == h);
    CHECK(RpSkinAtomicSetType(atomic, rpSKINTYPEGENERIC) == atomic);
    CHECK(atomic->pipeline != nullptr && atomic->pipeline->pluginID == rw::ID_SKIN);
    CHECK(RpSkinAtomicGetType(atomic) == rpSKINTYPEGENERIC);
    RpAtomic* a2 = RpAtomicCreate();
    CHECK(RpSkinAtomicGetType(a2) == rpNASKINTYPE);
    CHECK(RpSkinAtomicSetType(a2, rpSKINTYPEMATFX) == a2);                                               // MatFX plugin not registered: falls back to generic
    CHECK(a2->pipeline == atomic->pipeline);
    CHECK(RpSkinAtomicSetType(a2, rpSKINTYPETOON) == a2 && a2->pipeline == atomic->pipeline);
    RpAtomicDestroy(a2);
    RpAtomicDestroy(atomic);
    RpHAnimHierarchyDestroy(h);
    CHECK(RpGeometryDestroy(geo) == TRUE);                                                             // frees the skin through the plugin destructor

    // a 1-weight-only skin: numWeights 1, only bones with non-zero first weight are used
    float w1[8] = { 1, 0, 0, 0,  1, 0, 0, 0 };
    uint8_t i1[8] = { 5, 7, 7, 7,  6, 7, 7, 7 };
    RpSkin* s1 = RpSkinCreate(2, 8, reinterpret_cast<RwMatrixWeights*>(w1), reinterpret_cast<RwUInt32*>(i1), nullptr);
    CHECK(s1->numWeights == 1 && s1->numUsedBones == 2 && s1->usedBones[0] == 5 && s1->usedBones[1] == 6);
    rwFree(s1->data); rwFree(s1);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// hierarchy
struct Rig {
    static constexpr int N = 6;
    // tree in DFS order: 0 root; 1 (child of 0); 2,3 (children of 1); 4 (child of 0); 5 (child of 4)
    int parent[N] = { -1, 0, 1, 1, 0, 4 };
    int ids[N] = { 10, 11, 12, 13, 14, 15 };
    RwFrame* world = nullptr;     // frame above the hierarchy root
    RwFrame* frames[N] = {};
    RpHAnimHierarchy* h = nullptr;
    rw::Animation* anim = nullptr;
    int nodeFlags[N];

    static int CountChildren(const int* par, int i) { int c = 0; for (int k = 0; k < N; k++) c += par[k] == i; return c; }
    void MakeFlags() {
        for (int i = 0; i < N; i++) {
            const bool leaf = CountChildren(parent, i) == 0;
            bool laterSibling = false;
            for (int k = i + 1; k < N; k++) laterSibling |= parent[k] == parent[i] && parent[i] >= 0;
            int f = 0;
            if (leaf) { f = rpHANIMPOPPARENTMATRIX; if (laterSibling) f |= rpHANIMPUSHPARENTMATRIX; }
            else if (laterSibling) f = rpHANIMPUSHPARENTMATRIX;
            nodeFlags[i] = f;
        }
    }
    void Build(RwUInt32 hflags) {
        MakeFlags();
        world = RwFrameCreate();
        RwV3d wt = { 100, 5, 0 };
        RwFrameTranslate(world, &wt, rwCOMBINEREPLACE);
        for (int i = 0; i < N; i++) {
            frames[i] = RwFrameCreate();
            RpHAnimFrameSetID(frames[i], ids[i]);
            RwFrameAddChild(i == 0 ? world : frames[parent[i]], frames[i]);
        }
        h = RpHAnimHierarchyCreate(N, (RwUInt32*)nodeFlags, ids, (RpHAnimHierarchyFlag)hflags, 36);
        RpHAnimFrameSetHierarchy(frames[0], h);
        RpHAnimHierarchyAttach(h);
        // animation of the standard scheme (id 1): one key frame pair per node, t = 0 -> interpolated frame == first key frame
        anim = rw::Animation::create(rw::AnimInterpolatorInfo::find(1), 2 * N, 0, 1.0f);
        rw::HAnimKeyFrame* kf = static_cast<rw::HAnimKeyFrame*>(anim->keyframes);
        for (int i = 0; i < N; i++) {
            const float ang = 0.3f + 0.2f * i;
            rw::V3d axis = { 0.2f * i, 1.0f, 0.5f };
            rw::Quat q = rw::Quat::rotation(ang, axis);
            kf[i] = { &kf[i], 0.0f, q, { 1.0f + i, 0.5f * i, -0.25f * i } };
            kf[N + i] = { &kf[i], 1.0f, q, { 1.0f + i, 0.5f * i, -0.25f * i } };
        }
        h->interpolator->setCurrentAnim(anim);
    }
    void Destroy() {
        RwFrameDestroy(world);      // children are destroyed by the caller of this test through DestroyTree
    }
    // reference: matrices of every node from the explicit parent array
    void Reference(const RwMatrix& rootMat, RwMatrix* cur, RwMatrix* anims) const {
        for (int i = 0; i < N; i++) {
            const rw::HAnimInterpFrame* f = reinterpret_cast<const rw::HAnimInterpFrame*>(h->interpolator->getInterpFrame(i));
            RwMatrix m;
            m.rotate(f->q, rw::COMBINEREPLACE);
            m.pos = f->t;
            anims[i] = m;
            rw::Matrix::mult(&cur[i], &m, parent[i] < 0 ? const_cast<RwMatrix*>(&rootMat) : &cur[parent[i]]);
        }
    }
};

static void DestroyTree(RwFrame* f) {
    RwFrame* next = nullptr;
    for (RwFrame* c = f->child; c; c = next) { next = c->next; DestroyTree(c); }
    RwFrameDestroy(f);
}

static void ResetFrames(Rig& r) {
    for (int i = 0; i < Rig::N; i++) { r.frames[i]->matrix.setIdentity(); r.frames[i]->ltm.setIdentity(); }
    rw::Frame::syncDirty();
    // syncDirty copies matrices into the LTMs of dirty hierarchies; make the frames clean again
    for (int i = 0; i < Rig::N; i++) { r.frames[i]->matrix.setIdentity(); r.frames[i]->ltm.setIdentity(); }
}

static int DirtyListLength() {
    int n = 0;
    for (rw::LLLink* l = rw::engine->frameDirtyList.link.next; l != rw::engine->frameDirtyList.end(); l = l->next) n++;
    return n;
}

static void HierarchyTests() {
    Section("hierarchy: create / ids / matrix array / copy / destroy");
    int ids[3] = { 7, 8, 9 };
    RwUInt32 nf[3] = { 0, rpHANIMPUSHPARENTMATRIX, rpHANIMPOPPARENTMATRIX };
    RpHAnimHierarchy* h = RpHAnimHierarchyCreate(3, nf, ids, (RpHAnimHierarchyFlag)0, 44);
    CHECK(h != nullptr && h->numNodes == 3 && h->flags == 0 && h->parentFrame == nullptr);
    RwMatrix* arr = RpHAnimHierarchyGetMatrixArray(h);
    CHECK(arr != nullptr && ((uintptr_t)arr & 15) == 0 && h->matricesUnaligned != nullptr && (void*)arr >= h->matricesUnaligned);
    CHECK(h->interpolator != nullptr && h->interpolator->maxInterpKeyFrameSize == 44 && h->interpolator->numNodes == 3);
    CHECK(RpHAnimIDGetIndex(h, 8) == 1 && RpHAnimIDGetIndex(h, 9) == 2 && RpHAnimIDGetIndex(h, 7) == 0 && RpHAnimIDGetIndex(h, 123) == -1);
    CHECK(RpHAnimHierarchyGetNodeMatrix(h, 9) == &arr[2]);
    RpHAnimNodeInfo* ni = reinterpret_cast<RpHAnimNodeInfo*>(h->nodeInfo);
    CHECK(ni[0].nodeID == 7 && ni[1].nodeIndex == 1 && ni[1].flags == rpHANIMPUSHPARENTMATRIX && ni[2].flags == rpHANIMPOPPARENTMATRIX && ni[2].pFrame == nullptr);
    // copy: ids / flags / indices, no frames, own flags + key size, own interpolator + matrices
    ni[2].pFrame = reinterpret_cast<RwFrame*>(0x1000);
    RpHAnimHierarchy* c = RpHAnimHierarchyCreateFromHierarchy(h, (RpHAnimHierarchyFlag)(rpHANIMHIERARCHYUPDATEMODELLINGMATRICES | rpHANIMHIERARCHYUPDATELTMS), 52);
    ni[2].pFrame = nullptr;
    RpHAnimNodeInfo* cn = reinterpret_cast<RpHAnimNodeInfo*>(c->nodeInfo);
    CHECK(c != nullptr && c != h && c->numNodes == 3 && c->flags == 0x3000 && c->interpolator->maxInterpKeyFrameSize == 52 && c->interpolator != h->interpolator);
    CHECK(cn[0].nodeID == 7 && cn[1].nodeID == 8 && cn[2].nodeID == 9 && cn[1].flags == rpHANIMPUSHPARENTMATRIX && cn[2].flags == rpHANIMPOPPARENTMATRIX &&
          cn[2].nodeIndex == 2 && cn[2].pFrame == nullptr && c->matrices != h->matrices && c->parentFrame == nullptr);
    // NOMATRICES: no matrix array
    RpHAnimHierarchy* nm = RpHAnimHierarchyCreate(2, nullptr, nullptr, (RpHAnimHierarchyFlag)rpHANIMHIERARCHYNOMATRICES, 36);
    CHECK(nm != nullptr && RpHAnimHierarchyGetMatrixArray(nm) == nullptr && nm->matricesUnaligned == nullptr);
    CHECK(RpHAnimHierarchyDestroy(nm) == nullptr);

    // frame <-> hierarchy
    RwFrame* f = RwFrameCreate();
    CHECK(RpHAnimFrameGetHierarchy(f) == nullptr && RpHAnimFrameGetID(f) == -1);
    CHECK(RpHAnimFrameSetID(f, 77) == TRUE && RpHAnimFrameGetID(f) == 77);
    CHECK(RpHAnimFrameSetHierarchy(f, h) == TRUE && RpHAnimFrameGetHierarchy(f) == h && h->parentFrame == f);
    RwFrame* f2 = RwFrameCreate();
    CHECK(RpHAnimFrameSetHierarchy(f2, h) == TRUE && h->parentFrame == f2 && RpHAnimFrameGetHierarchy(f2) == h);   // f keeps a stale pointer in RW too
    RpHAnimFrameSetHierarchy(f, c);
    CHECK(c->parentFrame == f);
    RpHAnimFrameSetHierarchy(f, nullptr);
    CHECK(c->parentFrame == nullptr && RpHAnimFrameGetHierarchy(f) == nullptr);
    CHECK(RpHAnimHierarchyDestroy(c) == nullptr);
    RpHAnimFrameSetHierarchy(f2, nullptr);
    RpHAnimHierarchyDestroy(h);
    RwFrameDestroy(f); RwFrameDestroy(f2);

    Section("hierarchy: attach / detach / attach frame index (ids on a frame tree)");
    Rig r;
    r.Build(0);
    RpHAnimNodeInfo* n = reinterpret_cast<RpHAnimNodeInfo*>(r.h->nodeInfo);
    bool all = true;
    for (int i = 0; i < Rig::N; i++) all &= n[i].pFrame == r.frames[i];
    CHECK(all);
    CHECK(RpHAnimHierarchyDetach(r.h) == r.h);
    all = true;
    for (int i = 0; i < Rig::N; i++) all &= n[i].pFrame == nullptr;
    CHECK(all);
    CHECK(RpHAnimHierarchyAttachFrameIndex(r.h, 3) == r.h);
    CHECK(n[3].pFrame == r.frames[3] && n[0].pFrame == nullptr && n[1].pFrame == nullptr && n[2].pFrame == nullptr && n[4].pFrame == nullptr);
    CHECK(RpHAnimHierarchyAttachFrameIndex(r.h, 0) == r.h && n[0].pFrame == r.frames[0]);              // the root frame itself matches
    CHECK(RpHAnimHierarchyDetachFrameIndex(r.h, 3) == r.h && n[3].pFrame == nullptr && n[0].pFrame == r.frames[0]);
    RpHAnimHierarchyAttach(r.h);
    CHECK(n[3].pFrame == r.frames[3]);
    // RW attaches EVERY node with the id (no "unattached" check): a duplicate frame id makes the later frame win
    // (the traversal is pre-order over RW's child order, newest child first: 0, 4, 5, 1, 3, 2 -- frame 2 is visited after frame 5)
    RpHAnimHierarchyDetach(r.h);
    RpHAnimFrameSetID(r.frames[2], 15);                                                                // same id as node 5
    RpHAnimHierarchyAttach(r.h);
    CHECK(n[5].pFrame == r.frames[2] && n[2].pFrame == nullptr);
    RpHAnimFrameSetID(r.frames[2], 12);
    RpHAnimHierarchyAttach(r.h);
    CHECK(n[2].pFrame == r.frames[2] && n[5].pFrame == r.frames[5]);                                   // frame 5 (id 15) re-attached node 5; Attach never clears a node
    // attached frames are marked dirty (RwFrameUpdateObjects)
    CHECK(r.world->dirty());

    Section("hierarchy: frame clone copies the hierarchy (plugin copy) and destroying the frame frees it");
    RwFrame* clone = r.world->cloneHierarchy();
    RpHAnimHierarchy* ch = nullptr;
    for (RwFrame* k = clone->child; k && !ch; k = k->child) ch = RpHAnimFrameGetHierarchy(k);
    CHECK(ch != nullptr && ch != r.h && ch->numNodes == Rig::N && ch->parentFrame != nullptr && ch->parentFrame != r.frames[0] &&
          ch->interpolator->maxInterpKeyFrameSize == 36);
    {
        RpHAnimNodeInfo* cnn = reinterpret_cast<RpHAnimNodeInfo*>(ch->nodeInfo);
        bool same = true;
        for (int i = 0; i < Rig::N; i++) same &= cnn[i].nodeID == r.ids[i] && cnn[i].flags == r.nodeFlags[i] && cnn[i].pFrame == nullptr;
        CHECK(same);
        RpHAnimHierarchyAttach(ch);
        int attached = 0;
        for (int i = 0; i < Rig::N; i++) attached += cnn[i].pFrame != nullptr;
        CHECK(attached == Rig::N);
    }
    DestroyTree(clone);                                                                                // frame destructor frees the cloned hierarchy
    CHECK(true);

    DestroyTree(r.world);
    r.anim->destroy();
}

static bool CompareLtms(Rig& r, const RwMatrix* cur, float e = 1e-3f) {
    bool ok = true;
    for (int i = 0; i < Rig::N; i++) ok &= MatNear(*r.frames[i]->getLTM(), cur[i], e);
    return ok;
}

static void UpdateTests() {
    Section("UpdateMatrices: matrix array only (flags 0): parent chain from the frame above the root, push / pop topology");
    Rig r;
    r.Build(0);
    RwMatrix rootMat = *r.world->getLTM();              // translate(100, 5, 0)
    CHECK(Near(rootMat.pos.x, 100.0f) && Near(rootMat.pos.y, 5.0f));
    RwMatrix cur[Rig::N], an[Rig::N];
    r.Reference(rootMat, cur, an);
    for (int i = 0; i < Rig::N; i++) r.frames[i]->matrix = r.frames[i]->matrix;          // untouched
    RwMatrix before = r.frames[2]->matrix;
    CHECK(RpHAnimHierarchyUpdateMatrices(r.h) == TRUE);
    RwMatrix* arr = RpHAnimHierarchyGetMatrixArray(r.h);
    bool ok = true;
    for (int i = 0; i < Rig::N; i++) ok &= MatNear(arr[i], cur[i]);
    CHECK(ok);
    CHECK(MatNear(r.frames[2]->matrix, before, 0.0f));                                   // flags 0: frames untouched
    // sibling topology check: node 3 is a child of node 1 and node 5 a child of 4: LTM-independent identity cur[3] = an[3] x cur[1]
    RwMatrix t;
    rw::Matrix::mult(&t, &an[3], &cur[1]);
    CHECK(MatNear(arr[3], t));
    rw::Matrix::mult(&t, &an[5], &cur[4]);
    CHECK(MatNear(arr[5], t));
    rw::Matrix::mult(&t, &an[4], &cur[0]);
    CHECK(MatNear(arr[4], t));
    // the root dirty state is untouched by flags 0
    const int dirtyBefore = DirtyListLength();

    Section("UpdateMatrices: UPDATEMODELLINGMATRICES writes the frames' modelling matrices (+ marks them dirty)");
    ResetFrames(r);
    r.h->flags = rpHANIMHIERARCHYUPDATEMODELLINGMATRICES;
    CHECK(!r.world->dirty());
    CHECK(RpHAnimHierarchyUpdateMatrices(r.h) == TRUE);
    ok = true;
    for (int i = 0; i < Rig::N; i++) ok &= MatNear(r.frames[i]->matrix, an[i]);
    CHECK(ok);
    CHECK(r.world->dirty() && (r.frames[0]->object.privateFlags & rw::Frame::SUBTREESYNC) != 0);
    CHECK(CompareLtms(r, cur));                                                         // the frame tree == the node tree, so the synced LTMs equal the array
    CHECK(DirtyListLength() >= dirtyBefore);

    Section("UpdateMatrices: UPDATELTMS writes the LTMs directly (root on the dirty list with OBJ only)");
    ResetFrames(r);
    rw::Frame::syncDirty();
    CHECK(!r.world->dirty());
    r.h->flags = rpHANIMHIERARCHYUPDATELTMS;
    CHECK(RpHAnimHierarchyUpdateMatrices(r.h) == TRUE);
    ok = true;
    for (int i = 0; i < Rig::N; i++) ok &= MatNear(r.frames[i]->ltm, cur[i]) && r.frames[i]->matrix.pos.x == 0.0f;
    CHECK(ok);
    CHECK((r.frames[0]->root->object.privateFlags & rw::Frame::HIERARCHYSYNC) == rw::Frame::HIERARCHYSYNCOBJ);   // LTMs valid, objects not synced
    CHECK((r.frames[3]->object.privateFlags & rw::Frame::SUBTREESYNCLTM) == 0 && (r.frames[3]->object.privateFlags & rw::Frame::SUBTREESYNCOBJ) != 0);
    CHECK(MatNear(*r.frames[4]->getLTM(), cur[4]));                                     // getLTM does not resync (flag LTM clear)
    CHECK(MatNear(r.frames[4]->ltm, cur[4]));
    rw::Frame::syncDirty();
    CHECK(!r.world->dirty() && MatNear(r.frames[5]->ltm, cur[5]));                      // syncDirty keeps the written LTMs

    Section("UpdateMatrices: MODELLING | LTMS (what CClumpModelInfo::CreateInstance sets)");
    ResetFrames(r);
    r.h->flags = rpHANIMHIERARCHYUPDATEMODELLINGMATRICES | rpHANIMHIERARCHYUPDATELTMS;
    CHECK(RpHAnimHierarchyUpdateMatrices(r.h) == TRUE);
    ok = true;
    for (int i = 0; i < Rig::N; i++) ok &= MatNear(r.frames[i]->matrix, an[i]) && MatNear(r.frames[i]->ltm, cur[i]) && MatNear(arr[i], cur[i]);
    CHECK(ok);
    CHECK((r.frames[0]->root->object.privateFlags & rw::Frame::HIERARCHYSYNC) == rw::Frame::HIERARCHYSYNCOBJ);

    Section("UpdateMatrices: LOCALSPACEMATRICES (array relative to the hierarchy root, LTM = local x root matrix)");
    ResetFrames(r);
    r.h->flags = rpHANIMHIERARCHYUPDATEMODELLINGMATRICES | rpHANIMHIERARCHYUPDATELTMS | rpHANIMHIERARCHYLOCALSPACEMATRICES;
    CHECK(RpHAnimHierarchyUpdateMatrices(r.h) == TRUE);
    RwMatrix ident; ident.setIdentity();
    RwMatrix loc[Rig::N], an2[Rig::N];
    r.Reference(ident, loc, an2);
    ok = true;
    for (int i = 0; i < Rig::N; i++) ok &= MatNear(arr[i], loc[i]) && MatNear(r.frames[i]->ltm, cur[i]) && MatNear(r.frames[i]->matrix, an[i]);
    CHECK(ok);
    CHECK(!MatNear(arr[0], cur[0]));                                                    // the array really is local (the world offset is missing)

    Section("UpdateMatrices: NOMATRICES (no array, same frame results)");
    {
        Rig q;
        q.Build(rpHANIMHIERARCHYNOMATRICES | rpHANIMHIERARCHYUPDATEMODELLINGMATRICES | rpHANIMHIERARCHYUPDATELTMS);
        CHECK(RpHAnimHierarchyGetMatrixArray(q.h) == nullptr);
        RwMatrix qroot = *q.world->getLTM();
        RwMatrix qc[Rig::N], qa[Rig::N];
        q.Reference(qroot, qc, qa);
        CHECK(RpHAnimHierarchyUpdateMatrices(q.h) == TRUE);
        ok = true;
        for (int i = 0; i < Rig::N; i++) ok &= MatNear(q.frames[i]->ltm, qc[i]) && MatNear(q.frames[i]->matrix, qa[i]);
        CHECK(ok);
        DestroyTree(q.world);
        q.anim->destroy();
    }

    Section("UpdateMatrices: dirty parent chain is multiplied from the modelling matrices; hierarchy without frames; unattached node");
    {
        Rig q;
        q.Build(0);
        RwV3d tr = { 3, 4, 5 };
        RwFrameTranslate(q.world, &tr, rwCOMBINEPRECONCAT);                              // world frame now dirty; its LTM is not valid
        CHECK(q.world->dirty());
        RwMatrix expect;
        expect.setIdentity();                                                            // reference root = modelling matrix of the world frame (it has no parent)
        expect = q.world->matrix;
        RwMatrix qc[Rig::N], qa[Rig::N];
        q.Reference(expect, qc, qa);
        CHECK(RpHAnimHierarchyUpdateMatrices(q.h) == TRUE);
        RwMatrix* qarr = RpHAnimHierarchyGetMatrixArray(q.h);
        ok = true;
        for (int i = 0; i < Rig::N; i++) ok &= MatNear(qarr[i], qc[i]);
        CHECK(ok);
        CHECK(q.world->dirty());                                                         // nothing was synchronised on the way
        // detach everything: still computes the matrix array
        RpHAnimHierarchyDetach(q.h);
        q.h->flags = rpHANIMHIERARCHYUPDATEMODELLINGMATRICES | rpHANIMHIERARCHYUPDATELTMS;
        CHECK(RpHAnimHierarchyUpdateMatrices(q.h) == TRUE);
        ok = true;
        for (int i = 0; i < Rig::N; i++) ok &= MatNear(qarr[i], qc[i]);
        CHECK(ok);
        // hierarchy root without a parent frame: identity root matrix
        RwFrameRemoveChild(q.frames[0]);
        RpHAnimHierarchyUpdateMatrices(q.h);
        RwMatrix qc2[Rig::N], qa2[Rig::N];
        q.Reference(ident, qc2, qa2);
        ok = true;
        for (int i = 0; i < Rig::N; i++) ok &= MatNear(qarr[i], qc2[i]);
        CHECK(ok);
        DestroyTree(q.frames[0]);
        RwFrameDestroy(q.world);
        q.anim->destroy();
    }
    DestroyTree(r.world);
    r.anim->destroy();
}

// ---------------------------------------------------------------------------------------------------------------------------------
// key-frame callbacks
struct Qd { double w, x, y, z; };
static Qd Ham(const Qd& p, const Qd& q) {
    return { p.w * q.w - (p.x * q.x + p.y * q.y + p.z * q.z),
             p.w * q.x + q.w * p.x + (p.y * q.z - p.z * q.y),
             p.w * q.y + q.w * p.y + (p.z * q.x - p.x * q.z),
             p.w * q.z + q.w * p.z + (p.x * q.y - p.y * q.x) };
}
static Qd ToQd(const rw::Quat& q) { return { q.w, q.x, q.y, q.z }; }
static bool QNear(const rw::Quat& a, const Qd& b, double e = 2e-5) {
    return std::fabs(a.w - b.w) < e && std::fabs(a.x - b.x) < e && std::fabs(a.y - b.y) < e && std::fabs(a.z - b.z) < e;
}
static rw::HAnimInterpFrame MakeFrame(float w, float x, float y, float z, float tx, float ty, float tz) {
    rw::HAnimInterpFrame f{};
    const float n = std::sqrt(w * w + x * x + y * y + z * z);
    f.q = { x / n, y / n, z / n, w / n };
    f.t = { tx, ty, tz };
    return f;
}

static void KeyFrameTests() {
    Section("key frame callbacks: Add / MulRecip / Blend (double precision reference)");
    const rw::HAnimInterpFrame a = MakeFrame(0.9f, 0.1f, -0.3f, 0.2f, 1, 2, 3);
    const rw::HAnimInterpFrame b = MakeFrame(0.4f, 0.5f, 0.1f, -0.6f, -4, 5, 0.5f);
    rw::HAnimInterpFrame out{};
    RpHAnimKeyFrameAdd(&out, const_cast<rw::HAnimInterpFrame*>(&a), const_cast<rw::HAnimInterpFrame*>(&b));
    CHECK(QNear(out.q, Ham(ToQd(a.q), ToQd(b.q))));
    CHECK(Near(out.t.x, -3) && Near(out.t.y, 7) && Near(out.t.z, 3.5f));
    // Add with identity is the identity operation on q
    const rw::HAnimInterpFrame id = MakeFrame(1, 0, 0, 0, 0, 0, 0);
    RpHAnimKeyFrameAdd(&out, const_cast<rw::HAnimInterpFrame*>(&a), const_cast<rw::HAnimInterpFrame*>(&id));
    CHECK(QNear(out.q, ToQd(a.q)));

    rw::HAnimInterpFrame fr = b;
    RpHAnimKeyFrameMulRecip(&fr, const_cast<rw::HAnimInterpFrame*>(&a));
    {
        const Qd s = ToQd(a.q);
        const double n2 = s.w * s.w + s.x * s.x + s.y * s.y + s.z * s.z;
        const Qd inv = { s.w / n2, -s.x / n2, -s.y / n2, -s.z / n2 };
        CHECK(QNear(fr.q, Ham(inv, ToQd(b.q))));
        CHECK(Near(fr.t.x, -5) && Near(fr.t.y, 3) && Near(fr.t.z, -2.5f));
        // start * (start^-1 * frame) == frame
        rw::HAnimInterpFrame back{};
        RpHAnimKeyFrameAdd(&back, &const_cast<rw::HAnimInterpFrame&>(a), &fr);
        CHECK(QNear(back.q, ToQd(b.q)));
    }
    // unit quaternion with itself: identity
    fr = a;
    RpHAnimKeyFrameMulRecip(&fr, const_cast<rw::HAnimInterpFrame*>(&a));
    CHECK(QNear(fr.q, Qd{1, 0, 0, 0}) && Near(fr.t.x, 0) && Near(fr.t.z, 0));

    // Blend: t lerps, q slerps; alpha 0 -> in1, alpha 1 -> in2 (shorter arc), midpoint of two rotations about the same axis = half angle
    rw::HAnimInterpFrame in1 = a, in2 = b;
    RpHAnimKeyFrameBlend(&out, &in1, &in2, 0.0f);
    CHECK(QNear(out.q, ToQd(a.q), 1e-4) && Near(out.t.x, 1) && Near(out.t.y, 2));
    in1 = a; in2 = b;
    RpHAnimKeyFrameBlend(&out, &in1, &in2, 1.0f);
    {
        const double dot = a.q.w * b.q.w + a.q.x * b.q.x + a.q.y * b.q.y + a.q.z * b.q.z;
        const double sg = dot < 0 ? -1.0 : 1.0;
        CHECK(QNear(out.q, Qd{sg * b.q.w, sg * b.q.x, sg * b.q.y, sg * b.q.z}, 1e-4));
        CHECK(Near(out.t.x, -4) && Near(out.t.y, 5) && Near(out.t.z, 0.5f));
        CHECK((dot < 0) == (in2.q.w != b.q.w || in2.q.x != b.q.x));                       // the sign flip is written back into in2 (exe behaviour)
    }
    const rw::HAnimInterpFrame z0 = MakeFrame(std::cos(0.2f), 0, 0, std::sin(0.2f), 0, 0, 0);
    const rw::HAnimInterpFrame z1 = MakeFrame(std::cos(0.8f), 0, 0, std::sin(0.8f), 10, 0, 0);
    in1 = z0; in2 = z1;
    RpHAnimKeyFrameBlend(&out, &in1, &in2, 0.5f);
    CHECK(QNear(out.q, Qd{std::cos(0.5), 0, 0, std::sin(0.5)}, 1e-5) && Near(out.t.x, 5));
    in1 = z0; in2 = z1;
    RpHAnimKeyFrameBlend(&out, &in1, &in2, 0.25f);
    CHECK(QNear(out.q, Qd{std::cos(0.35), 0, 0, std::sin(0.35)}, 1e-5));
    // close quaternions (dot >= 0.999): linear mix
    const rw::HAnimInterpFrame c0 = MakeFrame(1, 0, 0, 0, 0, 0, 0);
    const rw::HAnimInterpFrame c1 = MakeFrame(1, 0.01f, 0, 0, 0, 0, 0);
    in1 = c0; in2 = c1;
    RpHAnimKeyFrameBlend(&out, &in1, &in2, 0.5f);
    CHECK(Near(out.q.x, 0.5f * c1.q.x, 1e-6f) && Near(out.q.w, 0.5f * (c0.q.w + c1.q.w), 1e-6f));
    // opposite hemisphere: negated and blended along the short arc
    rw::HAnimInterpFrame neg = z1;
    neg.q = { -z1.q.x, -z1.q.y, -z1.q.z, -z1.q.w };
    in1 = z0; in2 = neg;
    RpHAnimKeyFrameBlend(&out, &in1, &in2, 0.5f);
    CHECK(QNear(out.q, Qd{std::cos(0.5), 0, 0, std::sin(0.5)}, 1e-5));

    Section("key frame callbacks: stream size / write / read round trip");
    rw::AnimInterpolatorInfo* std1 = rw::AnimInterpolatorInfo::find(1);
    CHECK(std1 != nullptr);
    rw::Animation* an = rw::Animation::create(std1, 6, 0, 2.0f);
    rw::HAnimKeyFrame* kf = static_cast<rw::HAnimKeyFrame*>(an->keyframes);
    for (int i = 0; i < 3; i++) {
        kf[i] = { &kf[i], 0.0f, { 0.1f * i, 0.2f, 0.3f, 0.9f }, { float(i), 2.0f, 3.0f } };
        kf[3 + i] = { &kf[i], 1.5f, { 0.4f, 0.5f * i, 0.6f, 0.7f }, { float(i), 20.0f, 30.0f } };
    }
    CHECK(RpHAnimKeyFrameStreamGetSize(an) == 6 * 36);
    RwStream* w = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMWRITE, nullptr);
    CHECK(RpHAnimKeyFrameStreamWrite(an, w) == TRUE);
    RwMemory mem{};
    CHECK(RwStreamClose(w, &mem) && mem.length == 6 * 36);
    // on-disk form: {time, q, t, prevByteOffset}
    const float* fl = reinterpret_cast<const float*>(mem.start);
    CHECK(fl[0] == 0.0f && Near(fl[3], 0.3f) && Near(fl[5], 0.0f) && *reinterpret_cast<const int32_t*>(mem.start + 32) == 0);
    CHECK(*reinterpret_cast<const int32_t*>(mem.start + 3 * 36 + 32) == 0 && *reinterpret_cast<const int32_t*>(mem.start + 5 * 36 + 32) == 2 * 36);
    rw::Animation* an2 = rw::Animation::create(std1, 6, 0, 2.0f);
    RwStream* r = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &mem);
    CHECK(RpHAnimKeyFrameStreamRead(r, an2) == an2);
    RwStreamClose(r, nullptr);
    rw::HAnimKeyFrame* k2 = static_cast<rw::HAnimKeyFrame*>(an2->keyframes);
    bool same = true;
    for (int i = 0; i < 6; i++) {
        same &= k2[i].time == kf[i].time && k2[i].q.x == kf[i].q.x && k2[i].q.w == kf[i].q.w && k2[i].t.y == kf[i].t.y &&
                (k2[i].prev - k2) == (kf[i].prev - kf);
    }
    CHECK(same);
    // truncated stream: NULL
    RwMemory shortMem{ mem.start, 100 };
    r = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &shortMem);
    CHECK(RpHAnimKeyFrameStreamRead(r, an2) == nullptr);
    RwStreamClose(r, nullptr);
    rwFree(mem.start);
    an->destroy();
    an2->destroy();
}

// ---------------------------------------------------------------------------------------------------------------------------------
// SA model
static void CollectFrames(RwFrame* f, std::vector<RwFrame*>& out) {
    out.push_back(f);
    for (RwFrame* c = f->child; c; c = c->next) CollectFrames(c, out);
}
static RpAtomic* FirstAtomicCB(RpAtomic* a, void* data) { *static_cast<RpAtomic**>(data) = a; return nullptr; }

static RpHAnimHierarchy* HierarchyOf(RpClump* clump) {
    std::vector<RwFrame*> fs;
    CollectFrames(RpClumpGetFrame(clump), fs);
    for (RwFrame* f : fs) if (RpHAnimFrameGetHierarchy(f)) return RpHAnimFrameGetHierarchy(f);
    return nullptr;
}

// quaternion for a rotation matrix such that applying it with the hanim apply callback (Matrix::rotate REPLACE) rebuilds the matrix; tries both conventions
static bool QuatFromMatrix(const RwMatrix& m, rw::Quat& out) {
    const double tr = m.right.x + m.up.y + m.at.z;
    rw::Quat q;
    if (tr > 0) {
        const double s = std::sqrt(tr + 1.0) * 2;
        q.w = float(0.25 * s); q.x = float((m.up.z - m.at.y) / s); q.y = float((m.at.x - m.right.z) / s); q.z = float((m.right.y - m.up.x) / s);
    } else if (m.right.x > m.up.y && m.right.x > m.at.z) {
        const double s = std::sqrt(1.0 + m.right.x - m.up.y - m.at.z) * 2;
        q.w = float((m.up.z - m.at.y) / s); q.x = float(0.25 * s); q.y = float((m.up.x + m.right.y) / s); q.z = float((m.at.x + m.right.z) / s);
    } else if (m.up.y > m.at.z) {
        const double s = std::sqrt(1.0 + m.up.y - m.right.x - m.at.z) * 2;
        q.w = float((m.at.x - m.right.z) / s); q.x = float((m.up.x + m.right.y) / s); q.y = float(0.25 * s); q.z = float((m.at.y + m.up.z) / s);
    } else {
        const double s = std::sqrt(1.0 + m.at.z - m.right.x - m.up.y) * 2;
        q.w = float((m.right.y - m.up.x) / s); q.x = float((m.at.x + m.right.z) / s); q.y = float((m.at.y + m.up.z) / s); q.z = float(0.25 * s);
    }
    for (int attempt = 0; attempt < 2; attempt++) {
        RwMatrix t; t.rotate(q, rw::COMBINEREPLACE); t.pos = m.pos;
        if (MatNear(t, m, 1e-3f)) { out = q; return true; }
        q.x = -q.x; q.y = -q.y; q.z = -q.z;      // conjugate
    }
    return false;
}

static void ModelTests(const char* path) {
    Section((std::string("SA skinned model: ") + path).c_str());
    auto bytes = ReadFile(path);
    if (bytes.empty()) { CHECK(false); return; }
    rw::StreamMemory ms;
    ms.open(bytes.data(), (uint32_t)bytes.size());
    uint32_t len = 0, ver = 0;
    CHECK(rw::findChunk(&ms, rw::ID_CLUMP, &len, &ver));
    RpClump* clump = RpClumpStreamRead(&ms);
    CHECK(clump != nullptr);
    if (!clump) return;
    RpAtomic* first = nullptr;
    RpClumpForAllAtomics(clump, FirstAtomicCB, &first);
    CHECK(first != nullptr);
    RpGeometry* geo = RpAtomicGetGeometry(first);
    RpSkin* skin = RpSkinGeometryGetSkin(geo);
    CHECK(skin != nullptr);
    if (!skin) return;
    const RwUInt32 nBones = RpSkinGetNumBones(skin);
    const RwInt32 nVerts = geo->numVertices;
    std::printf("  skin: %u bones, %d vertices, numWeights %d, used bones %d\n", nBones, nVerts, skin->numWeights, skin->numUsedBones);
    CHECK(nBones > 0 && nBones <= 64 && nVerts > 0);
    const RwMatrix* inv = RpSkinGetSkinToBoneMatrices(skin);
    const RwUInt32* bi = RpSkinGetVertexBoneIndices(skin);
    RwMatrixWeights* bw = RpSkinGetVertexBoneWeights(skin);
    CHECK(inv && bi && bw && ((uintptr_t)inv & 15) == 0);
    // weights: non-negative, sum to 1, indices inside [0, numBones), descending order as streamed
    int badSum = 0, badIdx = 0, unsorted = 0;
    float minw = 1e9f;
    for (RwInt32 v = 0; v < nVerts; v++) {
        const float* w = &bw[v].w0;
        const uint8_t* ix = reinterpret_cast<const uint8_t*>(&bi[v]);
        float sum = w[0] + w[1] + w[2] + w[3];
        badSum += std::fabs(sum - 1.0f) > 1e-3f;
        for (int k = 0; k < 4; k++) { badIdx += w[k] != 0.0f && ix[k] >= nBones; minw = std::min(minw, w[k]); }
        for (int k = 0; k < 3; k++) unsorted += w[k] < w[k + 1];
    }
    std::printf("  weights: bad sums %d, bad indices %d, unsorted vertices-slots %d, min weight %g\n", badSum, badIdx, unsorted, minw);
    CHECK(badSum == 0 && badIdx == 0 && minw >= 0.0f);
    // bone to skin matrices are rigid
    bool rigid = true;
    for (RwUInt32 b = 0; b < nBones; b++) {
        const RwMatrix& m = inv[b];
        const double l = std::sqrt(m.right.x * m.right.x + m.right.y * m.right.y + m.right.z * m.right.z);
        rigid &= std::fabs(l - 1.0) < 1e-3;
    }
    CHECK(rigid);

    RpHAnimHierarchy* h = HierarchyOf(clump);
    CHECK(h != nullptr);
    if (!h) { RpClumpDestroy(clump); return; }
    RpHAnimNodeInfo* ni = reinterpret_cast<RpHAnimNodeInfo*>(h->nodeInfo);
    std::printf("  hierarchy: %d nodes, flags 0x%X, key size %d; node ids:", h->numNodes, h->flags, h->interpolator->maxInterpKeyFrameSize);
    for (int i = 0; i < h->numNodes; i++) std::printf(" %d", ni[i].nodeID);
    int pushes = 0, pops = 0;
    for (int i = 0; i < h->numNodes; i++) { pushes += (ni[i].flags & rpHANIMPUSHPARENTMATRIX) != 0; pops += (ni[i].flags & rpHANIMPOPPARENTMATRIX) != 0; }
    std::printf("\n  push flags %d, pop flags %d\n", pushes, pops);
    CHECK(h->numNodes == (int)nBones);
    bool idsOk = true;
    for (int i = 0; i < h->numNodes; i++) idsOk &= RpHAnimIDGetIndex(h, ni[i].nodeID) == i && ni[i].nodeIndex == i && ni[i].pFrame == nullptr;
    CHECK(idsOk);
    CHECK(h->parentFrame != nullptr && RpHAnimFrameGetHierarchy(h->parentFrame) == h);
    // ped skin atomics: the game makes the hierarchy known to every skinned atomic; stored in the atomic plugin
    RpSkinAtomicSetHAnimHierarchy(first, h);
    CHECK(RpSkinAtomicGetHAnimHierarchy(first) == h);
    RpSkinAtomicSetHAnimHierarchy(first, nullptr);
    CHECK(RpSkinAtomicGetType(first) == rpSKINTYPEGENERIC);                                            // the skin plugin's stream-rights callback attached the skin pipeline

    // clone (what CClumpModelInfo::CreateInstance does), attach, bind pose
    RpClump* cl = RpClumpClone(clump);
    CHECK(cl != nullptr);
    RpHAnimHierarchy* ch = HierarchyOf(cl);
    CHECK(ch != nullptr && ch != h && ch->numNodes == h->numNodes);
    if (ch) {
        RpHAnimNodeInfo* cn = reinterpret_cast<RpHAnimNodeInfo*>(ch->nodeInfo);
        bool same = true;
        for (int i = 0; i < ch->numNodes; i++) same &= cn[i].nodeID == ni[i].nodeID && cn[i].flags == ni[i].flags && cn[i].nodeIndex == ni[i].nodeIndex;
        CHECK(same);
        RpHAnimHierarchyAttach(ch);
        int attached = 0, attachedOk = 0;
        for (int i = 0; i < ch->numNodes; i++) {
            attached += cn[i].pFrame != nullptr;
            attachedOk += cn[i].pFrame && RpHAnimFrameGetID(cn[i].pFrame) == cn[i].nodeID;
        }
        std::printf("  clone: attached %d of %d nodes\n", attached, ch->numNodes);
        CHECK(attached == ch->numNodes && attachedOk == attached);

        // bind pose: key frames from the frames' own matrices
        const int N = ch->numNodes;
        rw::Animation* anim = rw::Animation::create(rw::AnimInterpolatorInfo::find(1), 2 * N, 0, 1.0f);
        rw::HAnimKeyFrame* kf = static_cast<rw::HAnimKeyFrame*>(anim->keyframes);
        int quatFail = 0;
        for (int i = 0; i < N; i++) {
            const RwMatrix& m = cn[i].pFrame->matrix;
            rw::Quat q{0, 0, 0, 1};
            if (!QuatFromMatrix(m, q)) {
                if (quatFail++ < 2) std::printf("  quat fail node %d: right(%g %g %g) up(%g %g %g) at(%g %g %g) pos(%g %g %g) flags %x\n", i, m.right.x, m.right.y, m.right.z, m.up.x, m.up.y, m.up.z,
                                                  m.at.x, m.at.y, m.at.z, m.pos.x, m.pos.y, m.pos.z, m.flags);
            }
            kf[i] = { &kf[i], 0.0f, q, m.pos };
            kf[N + i] = { &kf[i], 1.0f, q, m.pos };
        }
        CHECK(quatFail == 0);
        CHECK(ch->interpolator->setCurrentAnim(anim));
        ch->flags = rpHANIMHIERARCHYUPDATEMODELLINGMATRICES | rpHANIMHIERARCHYUPDATELTMS;           // CClumpModelInfo::CreateInstance
        std::vector<RwMatrix> savedLtm(N);
        for (int i = 0; i < N; i++) savedLtm[i] = *cn[i].pFrame->getLTM();
        RpAtomic* ca = nullptr;
        RpClumpForAllAtomics(cl, FirstAtomicCB, &ca);
        const RwMatrix atomicLtm = *RpAtomicGetFrame(ca)->getLTM();                                    // the skin-to-bone matrices are relative to the atomic's frame
        CHECK(RpHAnimHierarchyUpdateMatrices(ch) == TRUE);
        RwMatrix* arr = RpHAnimHierarchyGetMatrixArray(ch);
        int ltmOk = 0, arrOk = 0, invOk = 0;
        float worst = 0;
        for (int i = 0; i < N; i++) {
            ltmOk += MatNear(cn[i].pFrame->ltm, savedLtm[i], 2e-3f);
            arrOk += MatNear(arr[i], savedLtm[i], 2e-3f);
            RwMatrix prod;
            rw::Matrix::mult(&prod, const_cast<RwMatrix*>(&inv[i]), &arr[i]);                      // skin-to-bone x bone matrix = the skinned atomic's frame LTM in the bind pose
            const bool id = MatNear(prod, atomicLtm, 5e-3f);                                              // bind pose: vertex x inv x bone == vertex x atomic frame LTM
            invOk += id;
            if (!id) worst = std::max(worst, std::fabs(prod.pos.x) + std::fabs(prod.pos.y) + std::fabs(prod.pos.z));
        }
        std::printf("  bind pose: frame LTM == saved for %d/%d nodes, array == frame LTM for %d/%d, inverse x bone == atomic LTM for %d/%d (worst translation error %g)\n",
                    ltmOk, N, arrOk, N, invOk, N, worst);
        CHECK(ltmOk == N && arrOk == N);
        CHECK(invOk == N);
        anim->destroy();
    }
    // the clone's hierarchy is owned by its frame; destroying the clump frees it
    CHECK(RpClumpDestroy(cl) == TRUE);
    CHECK(RpClumpDestroy(clump) == TRUE);
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    HWND wnd = CreateWindowA("STATIC", "rw_skin_hanim_test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(wnd != nullptr);
    rw::MemoryFunctions mf{};
    mf.rwmalloc  = [](size_t sz, unsigned) -> void* { return sz ? std::malloc(sz) : nullptr; };
    mf.rwrealloc = [](void* p, size_t sz, unsigned) -> void* { return std::realloc(p, sz); };
    mf.rwfree    = [](void* p) { std::free(p); };
    CHECK(rw::Engine::init(&mf));
    // the game's PluginAttach order (app.cpp:140): world, skin, (RtAnimInitialize), hanim, ... anisot -- all before RwEngineOpen
    CHECK(RpWorldPluginAttach() == TRUE);
    CHECK(RpSkinPluginAttach() == TRUE);
    CHECK(RpHAnimPluginAttach() == TRUE);
    rw::registerAnisotropyPlugin();                                                                // RpAnisotPluginAttach lives in platform.cpp (batch 09)
    CHECK(RpSkinPluginAttach() == TRUE && RpHAnimPluginAttach() == TRUE);   // idempotent
    CHECK(RpGeometryGetPluginOffset(rw::ID_SKIN) > 0 && RpAtomicGetPluginOffset(rw::ID_SKIN) > 0);
    CHECK(rw::Frame::s_plglist.getPluginOffset(rw::ID_HANIM) > 0 && rw::Texture::s_plglist.getPluginOffset(rw::ID_ANISOT) > 0);
    rw::EngineOpenParams params{};
    params.window = wnd;
    rw::d3d::renderdevice.system = [](rw::DeviceReq, void*, int32_t) -> int32_t { return 1; };   // device-less (see rw_world_geometry_test)
    CHECK(rw::Engine::open(&params));
    rw::Engine::s_plglist.construct(rw::engine);
    // Engine::start also builds the driver plugins; only the platform-independent one (skin: dummy pipelines, pluginID ID_SKIN) here: the D3D9 driver plugin
    // compiles the skin vertex shaders and needs a device
    rw::Driver::s_plglist[rw::PLATFORM_NULL].construct(rw::engine->driver[rw::PLATFORM_NULL]);
    CHECK(rw::AnimInterpolatorInfo::find(1) != nullptr);                                           // hanim's engine plugin registered the standard scheme at start

    SkinTests();
    HierarchyTests();
    UpdateTests();
    KeyFrameTests();
    for (int i = 1; i < argc; i++) ModelTests(argv[i]);

    rw::Driver::s_plglist[rw::PLATFORM_NULL].destruct(rw::engine->driver[rw::PLATFORM_NULL]);
    rw::Engine::s_plglist.destruct(rw::engine);
    DestroyWindow(wnd);
    std::printf("\nrw_skin_hanim_test: %d checks passed, %d failed\n%s\n", g_pass, g_fail, g_fail ? "FAILED" : "PASSED");
    return g_fail;
}
