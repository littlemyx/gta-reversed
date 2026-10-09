// P2B-04c unit test: RpAtomic / RpClump / plugin-registration thunks / atomic render-callback slot of the librw shim
// (source/standalone/rw/{atomic,clump,plugins}.cpp). Runs under Wine; the engine is opened device-less (render device stubbed), so no D3D9 /
// wined3d involvement and no actual rendering: the render callback tests count calls.
// Usage: rw_atomic_clump_test.exe [real.dff ...]   (SA DFFs, read through RpClumpStreamRead with fake counting SA plugins). Exit code 0 = all passed.
#include "fakerw.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do { if (c) { ++g_pass; } else { ++g_fail; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Section(const char* n) { std::printf("[%s]\n", n); }
static bool Near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) <= e; }

// ---- fake SA plugins with counters (RW callback signatures, registered through the shim thunks) ----
struct Counters { int ctor = 0, dtor = 0, copy = 0, read = 0, write = 0, size = 0; };
static Counters cPipe, cCol, cName, c2dfx, cEnv;
static int32_t offZero = -1, offPipe = -1, offCol = -1, offName = -1, off2dfx = -1, offEnv = -1, offVis = -1;

struct Raw { uint8_t* data; int32_t len; };
static Raw* RawAt(void* o, int32_t off) { return (Raw*)((uint8_t*)o + off); }
static void* RawCtor(Counters& c, void* o, int32_t off) { c.ctor++; RawAt(o, off)->data = nullptr; RawAt(o, off)->len = 0; return o; }
static void* RawDtor(Counters& c, void* o, int32_t off) { c.dtor++; std::free(RawAt(o, off)->data); RawAt(o, off)->data = nullptr; RawAt(o, off)->len = 0; return o; }
static void* RawCopy(Counters& c, void* d, const void* s, int32_t off) {
    c.copy++;
    Raw* rd = RawAt(d, off); const Raw* rs = RawAt(const_cast<void*>(s), off);
    rd->len = rs->len; rd->data = rs->len ? (uint8_t*)std::malloc(rs->len) : nullptr;
    if (rs->len) std::memcpy(rd->data, rs->data, rs->len);
    return d;
}
static RwStream* RawRead(Counters& c, RwStream* st, int32_t len, void* o, int32_t off) {
    c.read++; Raw* r = RawAt(o, off); r->data = (uint8_t*)std::malloc(len); r->len = len; st->read8(r->data, len); return st;
}
static RwStream* RawWrite(Counters& c, RwStream* st, const void* o, int32_t off) {
    c.write++; const Raw* r = RawAt(const_cast<void*>(o), off); st->write8(r->data, r->len); return st;
}
static int32_t RawSize(Counters& c, const void* o, int32_t off) { c.size++; return RawAt(const_cast<void*>(o), off)->len; }

#define RAW_CBS(NAME, C) \
    static void* NAME##Ctor(void* o, int32_t off, int32_t) { return RawCtor(C, o, off); } \
    static void* NAME##Dtor(void* o, int32_t off, int32_t) { return RawDtor(C, o, off); } \
    static void* NAME##Copy(void* d, const void* s, int32_t off, int32_t) { return RawCopy(C, d, s, off); } \
    static RwStream* NAME##Read(RwStream* st, int32_t len, void* o, int32_t off, int32_t) { return RawRead(C, st, len, o, off); } \
    static RwStream* NAME##Write(RwStream* st, int32_t, const void* o, int32_t off, int32_t) { return RawWrite(C, st, o, off); } \
    static int32_t NAME##Size(const void* o, int32_t off, int32_t) { return RawSize(C, o, off); }
RAW_CBS(Col, cCol)     // clump plugin of size... see below (Raw is 8 bytes here; the real collision plugin has size 0 and keeps its data elsewhere)
RAW_CBS(Dfx, c2dfx)    // geometry plugin 0x253F2F8
RAW_CBS(Env, cEnv)     // material plugin 0x253F2FC

// pipeline id (atomic, 4 bytes)
static void* PipeCtor(void* o, int32_t off, int32_t) { cPipe.ctor++; *(int32_t*)((uint8_t*)o + off) = 0; return o; }
static void* PipeDtor(void* o, int32_t, int32_t) { cPipe.dtor++; return o; }
static void* PipeCopy(void* d, const void* s, int32_t off, int32_t) { cPipe.copy++; *(int32_t*)((uint8_t*)d + off) = *(const int32_t*)((const uint8_t*)s + off); return d; }
static RwStream* PipeRead(RwStream* st, int32_t len, void* o, int32_t off, int32_t) { cPipe.read++; st->read8((uint8_t*)o + off, len); return st; }
static RwStream* PipeWrite(RwStream* st, int32_t len, const void* o, int32_t off, int32_t) { cPipe.write++; st->write8((const uint8_t*)o + off, len); return st; }
static int32_t PipeSize(const void*, int32_t, int32_t) { cPipe.size++; return 4; }

// node name (frame, 24 bytes)
static void* NameCtor(void* o, int32_t off, int32_t) { cName.ctor++; ((char*)o + off)[0] = 0; return o; }
static void* NameDtor(void* o, int32_t, int32_t) { cName.dtor++; return o; }
static void* NameCopy(void* d, const void* s, int32_t off, int32_t) { cName.copy++; std::strncpy((char*)d + off, (const char*)s + off, 23); ((char*)d + off)[23] = 0; return d; }
static RwStream* NameRead(RwStream* st, int32_t len, void* o, int32_t off, int32_t) { cName.read++; if (len > 23) len = 23; st->read8((char*)o + off, len); ((char*)o + off)[len] = 0; return st; }
static RwStream* NameWrite(RwStream* st, int32_t len, const void* o, int32_t off, int32_t) { cName.write++; st->write8((const char*)o + off, len); return st; }
static int32_t NameSize(const void* o, int32_t off, int32_t) { cName.size++; return (int32_t)std::strlen((const char*)o + off); }
static const char* NameOf(RwFrame* f) { return (const char*)f + offName; }

// visibility plugin (atomic, no stream)
static int g_visCtor, g_visDtor, g_visCopy;
static void* VisCtor(void* o, int32_t off, int32_t) { g_visCtor++; std::memset((uint8_t*)o + off, 0, 8); return o; }
static void* VisDtor(void* o, int32_t, int32_t) { g_visDtor++; return o; }
static void* VisCopy(void* d, const void* s, int32_t off, int32_t) { g_visCopy++; std::memcpy((uint8_t*)d + off, (const uint8_t*)s + off, 8); return d; }

static std::vector<uint8_t> ReadFile(const char* path) {
    std::vector<uint8_t> b;
    if (FILE* f = std::fopen(path, "rb")) {
        std::fseek(f, 0, SEEK_END); b.resize(std::ftell(f)); std::fseek(f, 0, SEEK_SET);
        b.resize(std::fread(b.data(), 1, b.size(), f)); std::fclose(f);
    }
    return b;
}

struct Alloc { int a, c, f, g, m, l; };
static Alloc Snap() { return {rw::Atomic::numAllocated, rw::Clump::numAllocated, rw::Frame::numAllocated, rw::Geometry::numAllocated, rw::Material::numAllocated, rw::Light::numAllocated}; }
static bool Same(const Alloc& x, const Alloc& y) { return x.a == y.a && x.c == y.c && x.f == y.f && x.g == y.g && x.m == y.m && x.l == y.l; }

// ---- render callback bookkeeping ----
static int g_cbA, g_cbB;
static std::vector<RpAtomic*> g_order;
static RpAtomic* CbA(RpAtomic* a) { ++g_cbA; g_order.push_back(a); return a; }
static RpAtomic* CbB(RpAtomic* a) { ++g_cbB; g_order.push_back(a); return nullptr; } // "failing" callback
static RpAtomic* CollectCB(RpAtomic* a, void* data) { ((std::vector<RpAtomic*>*)data)->push_back(a); return a; }
static RpAtomic* StopCB(RpAtomic* a, void* data) { auto* v = (std::vector<RpAtomic*>*)data; v->push_back(a); return v->size() >= 2 ? nullptr : a; }

static RpGeometry* MakeGeo(float cx, float r) {
    RpGeometry* g = RpGeometryCreate(3, 1, rpGEOMETRYTRISTRIP | rpGEOMETRYTEXTURED);
    RpMorphTarget* mt = RpGeometryGetMorphTarget(g, 0);
    RwSphere s; s.center = {cx, 0, 0}; s.radius = r;
    RpMorphTargetSetBoundingSphere(mt, &s);
    return g;
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void PluginTests() {
    Section("plugin registration");
    CHECK(offPipe > 0 && offVis > 0 && offName > 0 && off2dfx > 0 && offEnv > 0);
    CHECK(offCol > 0 && offZero == offCol + (int32_t)sizeof(Raw) && RpClumpGetPluginOffset(0x253F2FB) == offZero);
    CHECK(RpAtomicGetPluginOffset(0x253F2F3) == offPipe);
    CHECK(RpClumpGetPluginOffset(0x253F2FA) == offCol);
    CHECK(RpGeometryGetPluginOffset(0x253F2F8) == off2dfx);
    CHECK(RpMaterialGetPluginOffset(0x253F2FC) == offEnv);
    CHECK(RpAtomicGetPluginOffset(0xDEAD) == -1);
    CHECK(RpAtomicRegisterPluginStream(0xDEAD, nullptr, nullptr, nullptr) == -1);   // stream for an unregistered id fails like RW
    // offsets are sizeof-based, 4-aligned and do not overlap
    CHECK(offPipe % 4 == 0 && offVis % 4 == 0 && offName % 4 == 0);
    CHECK(std::abs(offPipe - offVis) >= 4);
    const Alloc base = Snap();
    RpAtomic* a = RpAtomicCreate();
    RpClump* c = RpClumpCreate();
    RwFrame* f = RwFrameCreate();
    RpGeometry* g = RpGeometryCreate(3, 1, rpGEOMETRYTRISTRIP);
    RpMaterial* m = RpMaterialCreate();
    CHECK(cPipe.ctor == 1 && g_visCtor == 1 && cCol.ctor == 1 && cName.ctor == 1 && c2dfx.ctor == 1 && cEnv.ctor == 1);
    // the game writes through RWPLUGINOFFSET(obj, off)
    *(int32_t*)((uint8_t*)a + offPipe) = 0x1234;
    CHECK(*(int32_t*)((uint8_t*)a + offPipe) == 0x1234);
    RpAtomic* a2 = RpAtomicClone(a);
    CHECK(cPipe.copy == 1 && g_visCopy == 1 && *(int32_t*)((uint8_t*)a2 + offPipe) == 0x1234);
    CHECK(RpAtomicDestroy(a2) == TRUE && RpAtomicDestroy(a) == TRUE && RpClumpDestroy(c) == TRUE && RwFrameDestroy(f) == TRUE &&
          RpGeometryDestroy(g) == TRUE && RpMaterialDestroy(m) == TRUE);
    CHECK(cPipe.dtor == 2 && g_visDtor == 2 && cCol.dtor == 1 && cName.dtor == 1 && c2dfx.dtor == 1 && cEnv.dtor == 1);
    CHECK(Same(base, Snap()));
    CHECK(RpAtomicDestroy(nullptr) == FALSE && RpClumpDestroy(nullptr) == FALSE);
}

static void RenderCallbackTests() {
    Section("atomic render callback");
    const Alloc base = Snap();
    RpAtomic* a = RpAtomicCreate();
    CHECK(RpAtomicGetRenderCallBack(a) == AtomicDefaultRenderCallBack);
    CHECK(a->renderCB == rw::Atomic::defaultRenderCB);                       // untouched librw default while no game callback is set
    CHECK(RpAtomicGetFlags(a) == (rpATOMICCOLLISIONTEST | rpATOMICRENDER));
    g_cbA = g_cbB = 0; g_order.clear();
    CHECK(RpAtomicSetRenderCallBack(a, CbA) == a);
    CHECK(RpAtomicGetRenderCallBack(a) == CbA);
    CHECK(RpAtomicRender(a) == a && g_cbA == 1);
    a->renderCB(a);                                                           // the librw entry point (Clump::render / World::render)
    CHECK(g_cbA == 2);
    RpAtomicSetRenderCallBack(a, CbB);
    CHECK(RpAtomicRender(a) == nullptr && g_cbB == 1 && g_cbA == 2);          // the callback's result is returned
    CHECK(RpAtomicGetRenderCallBack(a) == CbB);
    // clone keeps the callback (slot copied by the plugin copy callback)
    RpAtomic* a2 = RpAtomicClone(a);
    CHECK(RpAtomicGetRenderCallBack(a2) == CbB);
    RpAtomicSetRenderCallBack(a2, CbA);
    CHECK(RpAtomicGetRenderCallBack(a) == CbB && RpAtomicGetRenderCallBack(a2) == CbA);
    // NULL and AtomicDefaultRenderCallBack restore the default (ShadowCamera.cpp: Set(NULL), later Set(original))
    RpAtomicSetRenderCallBack(a, nullptr);
    CHECK(RpAtomicGetRenderCallBack(a) == AtomicDefaultRenderCallBack && a->renderCB == rw::Atomic::defaultRenderCB);
    RpAtomicSetRenderCallBack(a, CbA);
    RpAtomicSetRenderCallBack(a, AtomicDefaultRenderCallBack);
    CHECK(RpAtomicGetRenderCallBack(a) == AtomicDefaultRenderCallBack && a->renderCB == rw::Atomic::defaultRenderCB);
    RpAtomicSetRenderCallBack(a, CbA);
    // clump render: head -> tail, only rpATOMICRENDER atomics, NULL result of a callback propagates but does not stop
    RpClump* clump = RpClumpCreate();
    RwFrame* root = RwFrameCreate();
    RpClumpSetFrame(clump, root);
    RpAtomic* x = RpAtomicCreate(); RpAtomic* y = RpAtomicCreate(); RpAtomic* z = RpAtomicCreate();
    for (RpAtomic* t : {x, y, z}) { RwFrame* fr = RwFrameCreate(); RwFrameAddChild(root, fr); RpAtomicSetFrame(t, fr); RpClumpAddAtomic(clump, t); }
    RpAtomicSetRenderCallBack(x, CbA); RpAtomicSetRenderCallBack(y, CbB); RpAtomicSetRenderCallBack(z, CbA);
    RpAtomicSetFlags(y, rpATOMICRENDER); RpAtomicSetFlags(z, rpATOMICCOLLISIONTEST);   // z: not rendered
    g_cbA = g_cbB = 0; g_order.clear();
    CHECK(RpClumpRender(clump) == nullptr);                                   // y failed
    CHECK(g_cbA == 1 && g_cbB == 1 && g_order.size() == 2 && g_order[0] == y && g_order[1] == x);   // head -> tail = reverse insertion
    RpAtomicSetFlags(y, rpATOMICRENDER | rpATOMICCOLLISIONTEST);
    RpAtomicSetRenderCallBack(y, CbA);
    g_cbA = g_cbB = 0;
    CHECK(RpClumpRender(clump) == clump && g_cbA == 2);
    CHECK(RpClumpDestroy(clump) == TRUE);
    RpAtomicDestroy(a); RpAtomicDestroy(a2);
    CHECK(Same(base, Snap()));
}

static void AtomicTests() {
    Section("atomic geometry / frame / bounding sphere");
    const Alloc base = Snap();
    RpAtomic* a = RpAtomicCreate();
    CHECK(a && !a->geometry && !a->clump && !RpAtomicGetFrame(a));
    RpGeometry* g1 = MakeGeo(1.0f, 2.0f);
    RpGeometry* g2 = MakeGeo(5.0f, 3.0f);
    CHECK(g1->refCount == 1);
    CHECK(RpAtomicSetGeometry(a, g1, 0) == a && g1->refCount == 2 && a->geometry == g1);
    CHECK(Near(a->boundingSphere.center.x, 1.0f) && Near(a->boundingSphere.radius, 2.0f));
    CHECK(RpAtomicSetGeometry(a, g1, 0) == a && g1->refCount == 2);           // same geometry: no-op (exe)
    CHECK(RpAtomicSetGeometry(a, g2, rpATOMICSAMEBOUNDINGSPHERE) == a && g1->refCount == 1 && g2->refCount == 2);
    CHECK(Near(a->boundingSphere.center.x, 1.0f) && Near(a->boundingSphere.radius, 2.0f));   // kept
    _rpAtomicResyncInterpolatedSphere(a);
    CHECK(Near(a->boundingSphere.center.x, 5.0f) && Near(a->boundingSphere.radius, 3.0f));
    CHECK(RpAtomicSetGeometry(a, g1, 0) == a && g2->refCount == 1 && Near(a->boundingSphere.radius, 2.0f));
    // frame + world bounding sphere (LTM translation + scale rule)
    RwFrame* f = RwFrameCreate();
    CHECK(RpAtomicSetFrame(a, f) == a && RpAtomicGetFrame(a) == f);
    RwV3d t{10, 20, 30};
    RwFrameTranslate(f, &t, rwCOMBINEREPLACE);
    const RwSphere* ws = RpAtomicGetWorldBoundingSphere(a);
    CHECK(Near(ws->center.x, 11) && Near(ws->center.y, 20) && Near(ws->center.z, 30) && Near(ws->radius, 2.0f));
    f->matrix.right.x = 3.0f;                                                  // scale x by 3 (RwFrameScale does not exist in the shim)
    f->matrix.update();
    RwFrameUpdateObjects(f);
    ws = RpAtomicGetWorldBoundingSphere(a);
    CHECK(Near(ws->radius, 6.0f, 1e-3f));                                     // radius * longest axis (3)
    CHECK(Near(ws->center.x, 10 + 3.0f, 1e-3f));
    CHECK(RpAtomicSetFrame(a, nullptr) == a && !RpAtomicGetFrame(a) && f->objectList.isEmpty());
    // clone: geometry shared (+1), bounding sphere / flags / callback copied, not attached
    RpAtomicSetFlags(a, rpATOMICRENDER);
    RpAtomicSetRenderCallBack(a, CbA);
    RpAtomic* c = RpAtomicClone(a);
    CHECK(c && c != a && c->geometry == g1 && g1->refCount == 3 && RpAtomicGetFlags(c) == rpATOMICRENDER && !c->clump && !RpAtomicGetFrame(c));
    CHECK(Near(c->boundingSphere.radius, 2.0f) && RpAtomicGetRenderCallBack(c) == CbA);
    CHECK(RpAtomicDestroy(c) == TRUE && g1->refCount == 2);
    CHECK(RpAtomicSetGeometry(a, nullptr, 0) == a && g1->refCount == 1 && !a->geometry);
    CHECK(RpAtomicDestroy(a) == TRUE);
    CHECK(RwFrameDestroy(f) == TRUE);
    CHECK(RpGeometryDestroy(g1) == TRUE && RpGeometryDestroy(g2) == TRUE);
    CHECK(Same(base, Snap()));
}

static void ClumpTests() {
    Section("clump list order / add / remove / destroy / clone");
    const Alloc base = Snap();
    RpClump* clump = RpClumpCreate();
    CHECK(clump && RpClumpGetNumAtomics(clump) == 0 && !RpClumpGetFrame(clump) && RpClumpClone(clump) == nullptr);   // no frame: not clonable
    RwFrame* root = RwFrameCreate();
    RwFrame* childA = RwFrameCreate(); RwFrame* childB = RwFrameCreate(); RwFrame* grand = RwFrameCreate();
    strcpy((char*)root + offName, "root"); strcpy((char*)childA + offName, "a"); strcpy((char*)childB + offName, "b"); strcpy((char*)grand + offName, "grand");
    RwFrameAddChild(root, childA); RwFrameAddChild(root, childB); RwFrameAddChild(childA, grand);
    RpClumpSetFrame(clump, root);
    CHECK(RpClumpGetFrame(clump) == root);
    RpGeometry* g = MakeGeo(0, 1);
    RpAtomic* at[3];
    RwFrame* frs[3] = {childA, childB, grand};
    for (int i = 0; i < 3; i++) {
        at[i] = RpAtomicCreate();
        RpAtomicSetGeometry(at[i], g, 0);
        RpAtomicSetFrame(at[i], frs[i]);
        CHECK(RpClumpAddAtomic(clump, at[i]) == clump && at[i]->clump == clump);
        *(int32_t*)((uint8_t*)at[i] + offPipe) = 100 + i;
    }
    CHECK(RpClumpGetNumAtomics(clump) == 3 && g->refCount == 4);
    std::vector<RpAtomic*> v;
    CHECK(RpClumpForAllAtomics(clump, CollectCB, &v) == clump);
    CHECK(v.size() == 3 && v[0] == at[2] && v[1] == at[1] && v[2] == at[0]);   // head insertion = reverse order, as the exe
    v.clear(); RpClumpForAllAtomics(clump, StopCB, &v);
    CHECK(v.size() == 2);                                                      // NULL stops
    // remove the middle one
    CHECK(RpClumpRemoveAtomic(clump, at[1]) == clump && !at[1]->clump && RpClumpGetNumAtomics(clump) == 2);
    CHECK(RpClumpRemoveAtomic(clump, at[1]) == clump && RpClumpGetNumAtomics(clump) == 2);   // not in the clump: untouched
    CHECK(RpClumpAddAtomic(clump, at[1]) == clump);
    v.clear(); RpClumpForAllAtomics(clump, CollectCB, &v);
    CHECK(v[0] == at[1] && v[1] == at[2] && v[2] == at[0]);
    // clone: frame hierarchy cloned (names via the node-name plugin copy), , plugin copies run
    const int copiesBefore = cName.copy, colCopiesBefore = cCol.copy;
    RpClump* cl = RpClumpClone(clump);
    CHECK(cl && cl != clump && RpClumpGetNumAtomics(cl) == 3);
    CHECK(RpClumpGetFrame(cl) && RpClumpGetFrame(cl) != root && std::strcmp(NameOf(RpClumpGetFrame(cl)), "root") == 0);
    CHECK(RwFrameCount(RpClumpGetFrame(cl)) == 4 && cName.copy - copiesBefore == 4 && cCol.copy - colCopiesBefore == 1);
    v.clear(); RpClumpForAllAtomics(cl, CollectCB, &v);
    CHECK(v.size() == 3);
    std::set<RwFrame*> orig{root, childA, childB, grand}, clonedFrames;
    for (RwFrame* f = RpClumpGetFrame(cl); f; f = nullptr) { clonedFrames.insert(f); }
    bool order = v.size() == 3;
    const char* want[3] = {"a", "grand", "b"};   // the exe clones head -> tail and prepends: REVERSE of the source list [b, grand, a]
    for (size_t i = 0; i < v.size(); i++) {
        RwFrame* f = RpAtomicGetFrame(v[i]);
        order = order && f && !orig.count(f) && std::strcmp(NameOf(f), want[i]) == 0 && v[i]->geometry == g;
    }
    CHECK(order);
    CHECK(*(int32_t*)((uint8_t*)v[0] + offPipe) == 100 && *(int32_t*)((uint8_t*)v[2] + offPipe) == 101);   // atomic plugin data copied
    // the originals' `root` was restored (purgeClone): each is the root of the source hierarchy again
    CHECK(childA->root == root && grand->root == root && root->root == root);
    // write / read round trip of the synthetic clump: node names (frame plugin), pipeline ids (atomic plugin), order
    {
        std::vector<uint8_t> out(1 << 16);
        rw::StreamMemory wm;
        wm.open(out.data(), 0, (uint32_t)out.size());
        const int pw = cPipe.write, nw = cName.write;
        CHECK(RpClumpStreamWrite(clump, &wm) == clump && wm.length == RpClumpStreamGetSize(clump) + 12);
        CHECK(cPipe.write - pw == 3 && cName.write - nw == 4);
        rw::StreamMemory rm;
        rm.open(out.data(), wm.length);
        uint32_t len = 0, ver = 0;
        CHECK(rw::findChunk(&rm, rw::ID_CLUMP, &len, &ver));
        RpClump* rt = RpClumpStreamRead(&rm);
        CHECK(rt && RpClumpGetNumAtomics(rt) == 3 && RwFrameCount(RpClumpGetFrame(rt)) == 4);
        if (rt) {
            std::vector<RpAtomic*> rv;
            RpClumpForAllAtomics(rt, CollectCB, &rv);
            // source list [at1, at2, at0] was written in that order; reading reverses: [at0, at2, at1]
            bool ok = rv.size() == 3 && *(int32_t*)((uint8_t*)rv[0] + offPipe) == 100 && *(int32_t*)((uint8_t*)rv[1] + offPipe) == 102 &&
                      *(int32_t*)((uint8_t*)rv[2] + offPipe) == 101 && std::strcmp(NameOf(RpAtomicGetFrame(rv[0])), "a") == 0 &&
                      std::strcmp(NameOf(RpAtomicGetFrame(rv[1])), "grand") == 0 && std::strcmp(NameOf(RpAtomicGetFrame(rv[2])), "b") == 0 &&
                      std::strcmp(NameOf(RpClumpGetFrame(rt)), "root") == 0;
            CHECK(ok);
            CHECK(RpClumpDestroy(rt) == TRUE);
        }
    }
    // destroy clone: its frames are gone, the source untouched
    const Alloc mid = Snap();
    CHECK(RpClumpDestroy(cl) == TRUE);
    CHECK(Snap().f == mid.f - 4 && Snap().a == mid.a - 3 && Snap().c == mid.c - 1 && g->refCount == 4);
    // a stray atomic attached to a frame of the hierarchy but NOT in the clump must not trip librw's frame-list assert on destroy
    RpAtomic* stray = RpAtomicCreate();
    RpAtomicSetFrame(stray, childB);
    const int dtorBefore = cCol.dtor;
    CHECK(RpClumpDestroy(clump) == TRUE);
    CHECK(cCol.dtor == dtorBefore + 1 && RpAtomicGetFrame(stray) == nullptr);
    CHECK(g->refCount == 1);
    RpAtomicDestroy(stray);
    RpGeometryDestroy(g);
    CHECK(Same(base, Snap()));
}

static void LightCameraTests() {
    Section("clump lights / cameras");
    const Alloc base = Snap();
    RpClump* clump = RpClumpCreate();
    RwFrame* root = RwFrameCreate();
    RpClumpSetFrame(clump, root);
    RpLight* l1 = RpLightCreate(rpLIGHTPOINT); RpLight* l2 = RpLightCreate(rpLIGHTAMBIENT);
    CHECK(RpClumpAddLight(clump, l1) == clump && RpClumpAddLight(clump, l2) == clump && RpClumpGetNumLights(clump) == 2 && l1->clump == clump);
    static int visited; visited = 0;
    RpClumpForAllLights(clump, [](RpLight* l, void*) -> RpLight* { ++visited; return l; }, nullptr);
    CHECK(visited == 2);
    CHECK(RpClumpRemoveLight(clump, l1) == clump && RpClumpGetNumLights(clump) == 1 && !l1->clump);
    RpLightDestroy(l1);
    CHECK(RpClumpGetNumCameras(clump) == 0);
    CHECK(RpClumpDestroy(clump) == TRUE);                                     // destroys the remaining light too
    CHECK(Same(base, Snap()));
}

static void StreamTests(const char* path) {
    Section(path);
    auto bytes = ReadFile(path);
    CHECK(!bytes.empty());
    if (bytes.empty()) return;
    const Alloc base = Snap();
    const Counters p0 = cPipe, n0 = cName, d0 = c2dfx, e0 = cEnv, c0 = cCol;

    // reference read straight through librw: atomics in FILE order
    rw::StreamMemory ref;
    ref.open(bytes.data(), (uint32_t)bytes.size());
    uint32_t len = 0, ver = 0;
    CHECK(rw::findChunk(&ref, rw::ID_CLUMP, &len, &ver));
    rw::Clump* rc = rw::Clump::streamRead(&ref);
    CHECK(rc != nullptr);
    std::vector<int> refVerts;
    for (rw::LLLink* l = rc->atomics.link.next; l != &rc->atomics.link; l = l->next) refVerts.push_back(rw::Atomic::fromClump(l)->geometry->numVertices);
    const int nAtomics = RpClumpGetNumAtomics(rc);
    RpClumpDestroy(rc);

    rw::StreamMemory sm;
    sm.open(bytes.data(), (uint32_t)bytes.size());
    CHECK(rw::findChunk(&sm, rw::ID_CLUMP, &len, &ver));
    RpClump* clump = RpClumpStreamRead(&sm);
    CHECK(clump != nullptr);
    if (!clump) return;
    const int frames = RwFrameCount(RpClumpGetFrame(clump));
    std::vector<RpAtomic*> atoms;
    RpClumpForAllAtomics(clump, CollectCB, &atoms);
    int pipeAtoms = 0, dfxGeos = 0, envMats = 0, named = 0;
    std::set<RpGeometry*> geos;
    for (RpAtomic* a : atoms) {
        pipeAtoms += *(int32_t*)((uint8_t*)a + offPipe) != 0;
        if (geos.insert(a->geometry).second) {
            dfxGeos += RawAt(a->geometry, off2dfx)->len != 0;
            for (int i = 0; i < RpGeometryGetNumMaterials(a->geometry); i++) envMats += RawAt(RpGeometryGetMaterial(a->geometry, i), offEnv)->len != 0;
        }
    }
    struct Walk { static void Count(RwFrame* f, int& named) { if (*NameOf(f)) named++; for (RwFrame* c = f->child; c; c = c->next) Count(c, named); } };
    Walk::Count(RpClumpGetFrame(clump), named);
    std::printf("  %d atomics, %d frames (%d named), pipeline-id atomics=%d, 2dfx geometries=%d, env-map materials=%d\n", nAtomics, frames, named, pipeAtoms, dfxGeos, envMats);
    std::printf("  plugin callbacks: pipe ctor/read=%d/%d, name ctor/read=%d/%d, 2dfx read=%d, env read=%d, col ctor=%d\n", cPipe.ctor - p0.ctor, cPipe.read - p0.read,
                cName.ctor - n0.ctor, cName.read - n0.read, c2dfx.read - d0.read, cEnv.read - e0.read, cCol.ctor - c0.ctor);
    CHECK(nAtomics > 0 && (int)atoms.size() == nAtomics && frames >= 1);
    CHECK(named > 0 && cName.read - n0.read == 2 * named);                      // one stream read per named frame, for the reference clump and ours
    CHECK(cCol.ctor - c0.ctor == 2);                                            // reference clump + this one
    // order: the shim's list is the reverse of the file order
    bool rev = (int)atoms.size() == (int)refVerts.size();
    for (size_t i = 0; rev && i < atoms.size(); i++) rev = atoms[i]->geometry->numVertices == refVerts[refVerts.size() - 1 - i];
    CHECK(rev);
    for (RpAtomic* a : atoms) CHECK(RpAtomicGetFrame(a) != nullptr && a->clump == clump);

    // clone + counters
    const Counters nBefore = cName, pBefore = cPipe, dBefore = c2dfx;
    RpClump* cl = RpClumpClone(clump);
    CHECK(cl && RpClumpGetNumAtomics(cl) == nAtomics && RwFrameCount(RpClumpGetFrame(cl)) == frames);
    CHECK(cName.copy - nBefore.copy == frames && cPipe.copy - pBefore.copy == nAtomics);
    std::vector<RpAtomic*> cat;
    RpClumpForAllAtomics(cl, CollectCB, &cat);
    bool same = cat.size() == atoms.size();
    for (size_t i = 0; same && i < cat.size(); i++) {
        RpAtomic* src = atoms[atoms.size() - 1 - i];                            // clone list = reverse of the source list (= DFF order)
        same = cat[i]->geometry == src->geometry && RpAtomicGetFrame(cat[i]) != RpAtomicGetFrame(src) &&
               std::strcmp(NameOf(RpAtomicGetFrame(cat[i])), NameOf(RpAtomicGetFrame(src))) == 0 && cat[i]->clump == cl;
    }
    CHECK(same);
    (void)dBefore;

    // write round trip through the shim (geometry list, atomics, plugins) and read back
    std::vector<uint8_t> out(bytes.size() * 2 + 4096);
    rw::StreamMemory wm;
    wm.open(out.data(), 0, (uint32_t)out.size());
    const int writeBefore = cName.write;
    CHECK(RpClumpStreamWrite(clump, &wm) == clump);
    CHECK(wm.length == RpClumpStreamGetSize(clump) + 12);
    CHECK(cName.write - writeBefore == named);
    rw::StreamMemory rm;
    rm.open(out.data(), wm.length);
    CHECK(rw::findChunk(&rm, rw::ID_CLUMP, &len, &ver));
    RpClump* rt = RpClumpStreamRead(&rm);
    CHECK(rt && RpClumpGetNumAtomics(rt) == nAtomics && RwFrameCount(RpClumpGetFrame(rt)) == frames);
    if (rt) {
        std::vector<RpAtomic*> rat;
        RpClumpForAllAtomics(rt, CollectCB, &rat);
        bool rtsame = rat.size() == atoms.size();
        for (size_t i = 0; rtsame && i < rat.size(); i++) rtsame = rat[i]->geometry->numVertices == atoms[atoms.size() - 1 - i]->geometry->numVertices;   // write is list order, read reverses again
        CHECK(rtsame);
        CHECK(RpClumpDestroy(rt) == TRUE);
    }

    CHECK(RpClumpDestroy(cl) == TRUE);
    CHECK(RpClumpDestroy(clump) == TRUE);
    CHECK(Same(base, Snap()));
    // every constructed plugin was destructed (reference clump + original + clone + round trip)
    CHECK(cPipe.ctor - p0.ctor == cPipe.dtor - p0.dtor && cName.ctor - n0.ctor == cName.dtor - n0.dtor && c2dfx.ctor - d0.ctor == c2dfx.dtor - d0.dtor &&
          cEnv.ctor - e0.ctor == cEnv.dtor - e0.dtor && cCol.ctor - c0.ctor == cCol.dtor - c0.dtor);
}


// P2B-04d: RpClumpGtaStreamRead1/2/CancelStream. Read1 on a stream that only holds the first part of the file (what the streaming thread has
// when a big model is split), Read2 on a fresh stream over the whole file (CStreaming::FinishLoadingLargeFile).
static void GtaSplitTests(const char* path) {
    Section((std::string("gta split read: ") + path).c_str());
    auto bytes = ReadFile(path);
    if (bytes.empty()) { CHECK(false); return; }
    const Alloc base = Snap();
    uint32_t len = 0, ver = 0;

    // reference: the normal reader (atomic order, geometry data, node names)
    rw::StreamMemory ref;
    ref.open(bytes.data(), (uint32_t)bytes.size());
    CHECK(rw::findChunk(&ref, rw::ID_CLUMP, &len, &ver));
    RpClump* full = RpClumpStreamRead(&ref);
    CHECK(full != nullptr);
    if (!full) return;
    std::vector<RpAtomic*> fa;
    RpClumpForAllAtomics(full, CollectCB, &fa);
    const int frames = RwFrameCount(RpClumpGetFrame(full));

    // Read1 over the whole file just to learn where the first half ends (the stream position the exe remembers)
    rw::StreamMemory probe;
    probe.open(bytes.data(), (uint32_t)bytes.size());
    CHECK(rw::findChunk(&probe, rw::ID_CLUMP, &len, &ver));
    CHECK(RpClumpGtaStreamRead1(&probe));
    const uint32_t cut = probe.tell();
    CHECK(cut > 0 && cut < bytes.size());
    const Alloc parked = Snap();
    CHECK(parked.f == base.f + frames + frames && parked.g >= base.g + (int)fa.size() / 2);   // `full` + the parked frames; geometries of the first half
    RpClumpGtaCancelStream();
    CHECK(Snap().f == base.f + frames && Snap().g == base.g + (Snap().g - base.g) && Snap().a == base.a + (int)fa.size());
    const int geosOfFull = Snap().g - base.g;

    // the real flow: Start on a truncated buffer (first part only), Finish on a fresh stream over the whole buffer
    const Counters n0 = cName, p0 = cPipe, d0 = c2dfx, c0 = cCol;
    rw::StreamMemory part;
    part.open(bytes.data(), cut);
    CHECK(rw::findChunk(&part, rw::ID_CLUMP, &len, &ver));
    CHECK(RpClumpGtaStreamRead1(&part) == true);
    CHECK(part.tell() == cut);
    rw::StreamMemory whole;
    whole.open(bytes.data(), (uint32_t)bytes.size());
    CHECK(rw::findChunk(&whole, rw::ID_CLUMP, &len, &ver));          // FinishLoadClumpFile's stream is positioned like a fresh Read1 stream
    RpClump* split = RpClumpGtaStreamRead2(&whole);
    CHECK(split != nullptr);
    if (split) {
        std::vector<RpAtomic*> sa;
        RpClumpForAllAtomics(split, CollectCB, &sa);
        CHECK(sa.size() == fa.size() && RwFrameCount(RpClumpGetFrame(split)) == frames);
        bool same = sa.size() == fa.size();
        int framesNamed = 0, pipeSeen = 0;
        for (size_t i = 0; same && i < sa.size(); i++) {
            same = sa[i]->geometry->numVertices == fa[i]->geometry->numVertices && sa[i]->geometry->numTriangles == fa[i]->geometry->numTriangles &&
                   RpGeometryGetNumMaterials(sa[i]->geometry) == RpGeometryGetNumMaterials(fa[i]->geometry) && RpAtomicGetFlags(sa[i]) == RpAtomicGetFlags(fa[i]) &&
                   std::strcmp(NameOf(RpAtomicGetFrame(sa[i])), NameOf(RpAtomicGetFrame(fa[i]))) == 0 && sa[i]->clump == split &&
                   RawAt(sa[i]->geometry, off2dfx)->len == RawAt(fa[i]->geometry, off2dfx)->len;
            pipeSeen += *(int32_t*)((uint8_t*)sa[i] + offPipe) != 0;
        }
        CHECK(same);
        CHECK(pipeSeen == 0);                                         // atomic extensions are not read on this path (exe 0x72E270)
        framesNamed = cName.read - n0.read;
        CHECK(framesNamed > 0);
        CHECK(cCol.ctor - c0.ctor == 1);                              // exactly one clump created
        std::printf("  split read: %zu atomics, %d frames (names read: %d), atomic-extension reads: %d, 2dfx reads: %d (cut at %u of %zu bytes)\n", sa.size(), frames,
                    framesNamed, cPipe.read - p0.read, c2dfx.read - d0.read, cut, bytes.size());
        CHECK(RpClumpDestroy(split) == TRUE);
    }
    CHECK(Snap().f == base.f + frames && Snap().a == base.a + (int)fa.size() && Snap().g - base.g == geosOfFull);   // nothing parked any more
    CHECK(RpClumpGtaStreamRead2(&whole) == nullptr);                  // nothing parked

    // cancel after Read1 releases the parked geometries / frames
    rw::StreamMemory again;
    again.open(bytes.data(), cut);
    rw::findChunk(&again, rw::ID_CLUMP, &len, &ver);
    CHECK(RpClumpGtaStreamRead1(&again));
    RpClumpGtaCancelStream();
    RpClumpGtaCancelStream();                                         // idempotent
    CHECK(Snap().f == base.f + frames && Snap().g - base.g == geosOfFull);
    // a truncated first part fails and leaves nothing behind
    rw::StreamMemory cutShort;
    cutShort.open(bytes.data(), cut > 200 ? 200 : cut / 2);
    rw::findChunk(&cutShort, rw::ID_CLUMP, &len, &ver);
    CHECK(RpClumpGtaStreamRead1(&cutShort) == false);
    CHECK(Snap().f == base.f + frames && Snap().g - base.g == geosOfFull);
    // a second Read1 drops the previously parked clump
    rw::StreamMemory a1, a2;
    a1.open(bytes.data(), cut); a2.open(bytes.data(), cut);
    rw::findChunk(&a1, rw::ID_CLUMP, &len, &ver); rw::findChunk(&a2, rw::ID_CLUMP, &len, &ver);
    CHECK(RpClumpGtaStreamRead1(&a1) && RpClumpGtaStreamRead1(&a2));
    RpClumpGtaCancelStream();
    CHECK(Snap().f == base.f + frames && Snap().g - base.g == geosOfFull);

    CHECK(RpClumpDestroy(full) == TRUE);
    CHECK(Same(base, Snap()));
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    HWND wnd = CreateWindowA("STATIC", "rw_atomic_clump_test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(wnd != nullptr);
    rw::MemoryFunctions mf{};
    mf.rwmalloc  = [](size_t sz, unsigned) -> void* { return sz ? std::malloc(sz) : nullptr; };
    mf.rwrealloc = [](void* p, size_t sz, unsigned) -> void* { return std::realloc(p, sz); };
    mf.rwfree    = [](void* p) { std::free(p); };
    CHECK(rw::Engine::init(&mf));
    // the game's PluginAttach order: world attach first, then its own plugins, all before RwEngineOpen / any object
    CHECK(RpWorldPluginAttach() == TRUE);
    offVis  = RpAtomicRegisterPlugin(8, 0x253F2F0, VisCtor, VisDtor, VisCopy);                      // CVisibilityPlugins (no stream)
    offPipe = RpAtomicRegisterPlugin(4, 0x253F2F3, PipeCtor, PipeDtor, PipeCopy);
    CHECK(RpAtomicRegisterPluginStream(0x253F2F3, PipeRead, PipeWrite, PipeSize) >= 0);
    offName = RwFrameRegisterPlugin(24, 0x253F2FE, NameCtor, NameDtor, NameCopy);
    CHECK(RwFrameRegisterPluginStream(0x253F2FE, NameRead, NameWrite, NameSize) >= 0);
    offCol  = RpClumpRegisterPlugin(sizeof(Raw), 0x253F2FA, ColCtor, ColDtor, ColCopy);
    CHECK(RpClumpRegisterPluginStream(0x253F2FA, ColRead, ColWrite, ColSize) >= 0);
    offZero = RpClumpRegisterPlugin(0, 0x253F2FB, nullptr, nullptr, nullptr);                        // size 0 is legal (the real collision plugin)
    off2dfx = RpGeometryRegisterPlugin(sizeof(Raw), 0x253F2F8, DfxCtor, DfxDtor, DfxCopy);
    CHECK(RpGeometryRegisterPluginStream(0x253F2F8, DfxRead, DfxWrite, DfxSize) >= 0);
    offEnv  = RpMaterialRegisterPlugin(sizeof(Raw), 0x253F2FC, EnvCtor, EnvDtor, EnvCopy);
    CHECK(RpMaterialRegisterPluginStream(0x253F2FC, EnvRead, EnvWrite, EnvSize) >= 0);
    rw::EngineOpenParams params{};
    params.window = wnd;
    rw::d3d::renderdevice.system = [](rw::DeviceReq, void*, int32_t) -> int32_t { return 1; };   // device-less (see rw_world_geometry_test)
    CHECK(rw::Engine::open(&params));
    rw::Engine::s_plglist.construct(rw::engine);

    PluginTests();
    RenderCallbackTests();
    AtomicTests();
    ClumpTests();
    LightCameraTests();
    for (int i = 1; i < argc; i++) StreamTests(argv[i]);
    for (int i = 1; i < argc; i++) GtaSplitTests(argv[i]);

    rw::Engine::s_plglist.destruct(rw::engine);
    DestroyWindow(wnd);
    std::printf("\nrw_atomic_clump_test: %d checks passed, %d failed\n%s\n", g_pass, g_fail, g_fail ? "FAILED" : "PASSED");
    return g_fail;
}
