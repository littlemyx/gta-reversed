// P2B-04a/b unit test: RpLight / RpWorld / RpMaterial(+List) / RpGeometry / RpMorphTarget / mesh iteration of the librw shim
// (source/standalone/rw/{light,world,material,geometry}.cpp). Runs under Wine; the engine is opened device-less (render device stubbed), so no D3D9 / wined3d involvement.
// Usage: rw_world_geometry_test.exe [real.dff ...]  (SA DFFs: read with rw::Clump::streamRead directly, RpClumpStreamRead does not exist yet). Exit code 0 = all checks passed.
#include "fakerw.h"

#include <algorithm>
#include <cstdlib>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <vector>

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do { if (c) { ++g_pass; } else { ++g_fail; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Section(const char* n) { std::printf("[%s]\n", n); }
static bool Near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) <= e; }

// ---- raw SA plugin chunks (what the 04c registration thunks will do for 2DFX / extra colours / breakable / env / spec): keep the payload ----
struct Raw { uint8_t* data; int32_t len; };
static int32_t g_off2dfx, g_offExtraCol, g_offBreakable, g_offEnv, g_offSpec;
static void* RawCtor(void* o, int32_t off, int32_t) { auto* r = (Raw*)((uint8_t*)o + off); r->data = nullptr; r->len = 0; return o; }
static void* RawDtor(void* o, int32_t off, int32_t) { auto* r = (Raw*)((uint8_t*)o + off); std::free(r->data); r->data = nullptr; r->len = 0; return o; }
static void* RawCopy(void* d, void* s, int32_t off, int32_t) {
    auto* rd = (Raw*)((uint8_t*)d + off); auto* rs = (Raw*)((uint8_t*)s + off);
    rd->len = rs->len; rd->data = rs->len ? (uint8_t*)std::malloc(rs->len) : nullptr;
    if (rs->len) std::memcpy(rd->data, rs->data, rs->len);
    return d;
}
static rw::Stream* RawRead(rw::Stream* st, int32_t len, void* o, int32_t off, int32_t) {
    auto* r = (Raw*)((uint8_t*)o + off); r->data = (uint8_t*)std::malloc(len); r->len = len; st->read8(r->data, len); return st;
}
static rw::Stream* RawWrite(rw::Stream* st, int32_t, void* o, int32_t off, int32_t) {
    auto* r = (Raw*)((uint8_t*)o + off); st->write8(r->data, r->len); return st;
}
static int32_t RawSize(void* o, int32_t off, int32_t) { return ((Raw*)((uint8_t*)o + off))->len; }
template <class T> static int32_t RegRaw(uint32_t id) {
    const int32_t off = T::registerPlugin(sizeof(Raw), id, RawCtor, RawDtor, RawCopy);
    T::registerPluginStream(id, RawRead, RawWrite, RawSize);
    return off;
}
template <class T> static Raw* RawOf(T* obj, int32_t off) { return (Raw*)((uint8_t*)obj + off); }

// ---- strip decoding: the triangles a D3D triangle strip with degenerate joins draws ----
using Tri = std::array<uint16_t, 3>;
static Tri Canon(Tri t) { // rotate so the smallest index is first (keeps the winding)
    while (!(t[0] <= t[1] && t[0] <= t[2])) { t = {t[1], t[2], t[0]}; }
    return t;
}
static std::vector<Tri> DecodeStrip(const uint16_t* idx, uint32_t n, bool strip) {
    std::vector<Tri> out;
    if (!strip) { for (uint32_t i = 0; i + 2 < n; i += 3) out.push_back(Canon({idx[i], idx[i + 1], idx[i + 2]})); return out; }
    for (uint32_t k = 0; k + 2 < n; k++) {
        Tri t = (k % 2 == 0) ? Tri{idx[k], idx[k + 1], idx[k + 2]} : Tri{idx[k + 1], idx[k], idx[k + 2]};
        if (t[0] == t[1] || t[1] == t[2] || t[0] == t[2]) continue;
        out.push_back(Canon(t));
    }
    return out;
}
// Per material: sorted multiset of canonical triangles from g->triangles vs from the mesh header
static std::map<RpMaterial*, std::vector<Tri>> FromTriangles(RpGeometry* g) {
    std::map<RpMaterial*, std::vector<Tri>> m;
    for (int i = 0; i < g->numTriangles; i++) {
        const RpTriangle& t = g->triangles[i];
        if (t.v[0] == t.v[1] || t.v[1] == t.v[2] || t.v[0] == t.v[2]) continue;
        m[g->matList.materials[t.matId]].push_back(Canon({t.v[0], t.v[1], t.v[2]}));
    }
    for (auto& kv : m) std::sort(kv.second.begin(), kv.second.end());
    return m;
}
static std::map<RpMaterial*, std::vector<Tri>> FromMeshes(RpGeometry* g) {
    std::map<RpMaterial*, std::vector<Tri>> m;
    RpMeshHeader* h = g->meshHeader;
    RpMesh* mesh = h->getMeshes();
    const bool strip = (h->flags & 1) != 0;
    for (unsigned i = 0; i < h->numMeshes; i++, mesh++) {
        auto t = DecodeStrip(mesh->indices, mesh->numIndices, strip);
        auto& dst = m[mesh->material];
        dst.insert(dst.end(), t.begin(), t.end());
    }
    for (auto& kv : m) std::sort(kv.second.begin(), kv.second.end());
    return m;
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void LightTests() {
    Section("light");
    const int baseL = rw::Light::numAllocated;
    RpLight* amb = RpLightCreate(rpLIGHTAMBIENT);
    CHECK(amb && RpLightGetType(amb) == rpLIGHTAMBIENT);
    CHECK(RpLightGetFlags(amb) == (rpLIGHTLIGHTATOMICS | rpLIGHTLIGHTWORLD));
    CHECK(amb->color.red == 1 && amb->color.green == 1 && amb->color.blue == 1 && amb->color.alpha == 1);
    CHECK(amb->radius == 0 && amb->minusCosAngle == 0.0f); // exe default (librw: 1)
    CHECK(amb->object.object.privateFlags == 1);
    const RwRGBAReal c{0.25f, 0.5f, 0.75f, 0.9f};
    CHECK(RpLightSetColor(amb, &c) == amb);
    CHECK(RpLightGetColor(amb)->red == 0.25f && RpLightGetColor(amb)->alpha == 0.9f);
    CHECK(amb->object.object.privateFlags == 0);
    const RwRGBAReal grey{0.4f, 0.4f, 0.4f, 1};
    RpLightSetColor(amb, &grey);
    CHECK(amb->object.object.privateFlags == 1);
    CHECK(RpLightSetColor(nullptr, &c) == nullptr);

    RpLight* pt = RpLightCreate(rpLIGHTPOINT);
    CHECK(RpLightSetRadius(pt, 12.5f) == pt && RpLightGetRadius(pt) == 12.5f);
    RpLightSetFlags(pt, rpLIGHTLIGHTATOMICS);
    CHECK(RpLightGetFlags(pt) == rpLIGHTLIGHTATOMICS);
    CHECK(RpLightSetConeAngle(pt, 0.5f) == pt && Near(RpLightGetConeAngle(pt), 0.5f, 1e-5f));
    rw::Frame* f = rw::Frame::create();
    RpLightSetFrame(pt, f);
    CHECK(RpLightGetFrame(pt) == f);
    CHECK(RpLightSetRadius(pt, 3.0f) == pt && RpLightGetFrame(pt) == f);
    CHECK(RpLightDestroy(pt) == TRUE);
    f->destroy();
    CHECK(RpLightDestroy(amb) == TRUE);
    CHECK(RpLightDestroy(nullptr) == FALSE);
    CHECK(rw::Light::numAllocated == baseL);
}

static int g_cbCount;
static RpLight* LightCB(RpLight*, void*) { ++g_cbCount; return g_cbCount >= 2 ? nullptr : (RpLight*)1; } // stops after the 2nd
static RpClump* ClumpCB(RpClump*, void*) { ++g_cbCount; return (RpClump*)1; }
static RpLight* LightCount(RpLight*, void*) { ++g_cbCount; return (RpLight*)1; }

static void WorldTests() {
    Section("world");
    RwBBox bb{{100, 100, 100}, {-100, -100, -100}};
    RpWorld* w = RpWorldCreate(&bb);
    CHECK(w != nullptr);
    RpLight* amb = RpLightCreate(rpLIGHTAMBIENT);
    RpLight* dir = RpLightCreate(rpLIGHTDIRECTIONAL);
    RpLight* pt  = RpLightCreate(rpLIGHTPOINT);
    CHECK(RpWorldAddLight(w, amb) == w && RpWorldAddLight(w, dir) == w && RpWorldAddLight(w, pt) == w);
    CHECK(amb->world == w && w->globalLights.count() == 2 && w->localLights.count() == 1);
    CHECK(RpWorldAddLight(w, amb) == w && w->globalLights.count() == 2);        // re-adding is a no-op
    g_cbCount = 0; RpWorldForAllLights(w, LightCount, nullptr);
    CHECK(g_cbCount == 3);
    g_cbCount = 0; RpWorldForAllLights(w, LightCB, nullptr);
    CHECK(g_cbCount == 2);                                                      // stops at the first NULL
    CHECK(RpWorldRemoveLight(w, dir) == w && dir->world == nullptr && w->globalLights.count() == 1);
    CHECK(RpWorldRemoveLight(w, dir) == nullptr);                               // not in the world
    CHECK(RpLightDestroy(dir) == TRUE);

    rw::Camera* cam = rw::Camera::create();
    CHECK(RpWorldAddCamera(w, cam) == w && cam->world == w);
    CHECK(RpWorldRemoveCamera(w, cam) == w && cam->world == nullptr);
    CHECK(RpWorldRemoveCamera(w, cam) == nullptr);
    cam->destroy();

    rw::Atomic* at = rw::Atomic::create();
    CHECK(RpWorldAddAtomic(w, at) == w && at->world == w);
    CHECK(RpWorldRemoveAtomic(w, at) == w && at->world == nullptr);
    rw::Clump* cl = rw::Clump::create();
    cl->addAtomic(at);
    CHECK(RpWorldAddClump(w, cl) == w && cl->world == w && at->world == w);
    g_cbCount = 0; RpWorldForAllClumps(w, ClumpCB, nullptr);
    CHECK(g_cbCount == 1);
    CHECK(RpWorldRemoveClump(w, cl) == w && cl->world == nullptr && at->world == nullptr);
    cl->destroy();

    CHECK(RpWorldDestroy(w) == TRUE);                                            // lights still in the world are detached
    CHECK(amb->world == nullptr && pt->world == nullptr);
    RpLightDestroy(amb); RpLightDestroy(pt);
    CHECK(RpWorldDestroy(nullptr) == FALSE);
    CHECK(rw::World::numAllocated == 0);
}

static void MaterialTests() {
    Section("material + material list");
    const int baseM = rw::Material::numAllocated, baseT = rw::Texture::numAllocated;
    RpMaterial* m = RpMaterialCreate();
    CHECK(m && m->refCount == 1 && m->texture == nullptr);
    CHECK(*(uint32_t*)&m->color == 0xFFFFFFFFu);
    CHECK(m->surfaceProps.ambient == 1 && m->surfaceProps.specular == 1 && m->surfaceProps.diffuse == 1);
    const RwRGBA col{10, 20, 30, 40};
    RpMaterialSetColor(m, &col);
    CHECK(RpMaterialGetColor(m)->green == 20);
    const RwSurfaceProperties sp{0.5f, 0.25f, 0.125f};
    RpMaterialSetSurfaceProperties(m, &sp);
    CHECK(RpMaterialGetSurfaceProperties(m)->specular == 0.25f);

    RwTexture* t1 = rw::Texture::create(nullptr);
    RwTexture* t2 = rw::Texture::create(nullptr);
    CHECK(RpMaterialSetTexture(m, t1) == m && t1->refCount == 2 && RpMaterialGetTexture(m) == t1);
    CHECK(RpMaterialSetTexture(m, t1) == m && t1->refCount == 2);                // same texture again: ref taken before the old one is released
    t1->destroy();                                                               // drop our own reference: only the material holds it now
    CHECK(t1->refCount == 1);
    CHECK(RpMaterialSetTexture(m, t1) == m && t1->refCount == 1 && rw::Texture::numAllocated == baseT + 2); // must NOT free it
    CHECK(RpMaterialSetTexture(m, t2) == m && t2->refCount == 2 && rw::Texture::numAllocated == baseT + 1);  // t1 released (freed)
    CHECK(RpMaterialSetTexture(m, nullptr) == m && t2->refCount == 1 && m->texture == nullptr);
    RpMaterialSetTexture(m, t2);
    t2->destroy();
    RpMaterialAddRef(m);
    CHECK(m->refCount == 2);
    CHECK(RpMaterialDestroy(m) == TRUE && m->refCount == 1 && rw::Material::numAllocated == baseM + 1);
    CHECK(RpMaterialDestroy(m) == TRUE && rw::Material::numAllocated == baseM && rw::Texture::numAllocated == baseT);
    CHECK(RpMaterialDestroy(nullptr) == FALSE);

    // list: the game hands in `new RpMaterial*[20]` (VehicleModelInfo): grows past 20, takes references, backwards find, deinit resets all
    RpMaterialList ml{};
    ml.space = 20; ml.numMaterials = 0; ml.materials = new RpMaterial*[ml.space];
    std::vector<RpMaterial*> mats;
    for (int i = 0; i < 25; i++) { mats.push_back(RpMaterialCreate()); CHECK(_rpMaterialListAppendMaterial(&ml, mats.back()) == i); }
    CHECK(ml.numMaterials == 25 && ml.space == 40 && mats[3]->refCount == 2);
    CHECK(_rpMaterialListAppendMaterial(&ml, mats[3]) == 25 && mats[3]->refCount == 3);
    CHECK(_rpMaterialListFindMaterialIndex(&ml, mats[3]) == 25);                  // last occurrence wins
    CHECK(_rpMaterialListFindMaterialIndex(&ml, mats[7]) == 7);
    RpMaterial* stranger = RpMaterialCreate();
    CHECK(_rpMaterialListFindMaterialIndex(&ml, stranger) == -1);
    CHECK(_rpMaterialListDeinitialize(&ml) == nullptr);
    CHECK(ml.materials == nullptr && ml.numMaterials == 0 && ml.space == 0 && mats[3]->refCount == 1 && mats[7]->refCount == 1);
    CHECK(_rpMaterialListFindMaterialIndex(&ml, mats[7]) == -1);
    for (auto* x : mats) RpMaterialDestroy(x);
    RpMaterialDestroy(stranger);
    CHECK(rw::Material::numAllocated == baseM);
}

static int g_matCb, g_stopAt;
static RpMaterial* MatCB(RpMaterial*, void*) { ++g_matCb; return g_matCb >= g_stopAt ? nullptr : (RpMaterial*)1; }

static void GeometryCreateTests() {
    Section("geometry create / flags / destroy / usage flags");
    const int baseG = rw::Geometry::numAllocated;
    RpGeometry* g = RpGeometryCreate(4, 2, rpGEOMETRYTRISTRIP | rpGEOMETRYTEXTURED | rpGEOMETRYNORMALS | rpGEOMETRYLIGHT); // 0x35 (ClothesBuilder)
    CHECK(g && g->flags == 0x35 && RpGeometryGetFlags(g) == 0x35);
    CHECK(RpGeometryGetNumVertices(g) == 4 && RpGeometryGetNumTriangles(g) == 2 && g->numTexCoordSets == 1 && RpGeometryGetNumTexCoordSets(g) == 1);
    CHECK(RpGeometryGetNumMorphTargets(g) == 1 && RpGeometryGetMorphTarget(g, 0)->vertices && RpGeometryGetMorphTarget(g, 0)->normals);
    CHECK(RpGeometryGetVertexTexCoords(g, rwTEXTURECOORDINATEINDEX0) != nullptr && RpGeometryGetPreLightColors(g) == nullptr);
    CHECK(g->triangles[0].matId == 0xFFFF && g->triangles[1].matId == 0xFFFF && g->refCount == 1 && g->meshHeader == nullptr);
    CHECK(RpGeometryGetNumMaterials(g) == 0);
    CHECK(RpGeometryTriangleGetMaterial(g, &g->triangles[0]) == nullptr);
    CHECK(RpD3D9GeometryGetUsageFlags(g) == 0);
    RpD3D9GeometrySetUsageFlags(g, rpD3D9GEOMETRYUSAGE_DYNAMICPRELIT);
    CHECK(RpD3D9GeometryGetUsageFlags(g) == 0x08);
    RpGeometryAddRef(g);
    CHECK(g->refCount == 2 && RpGeometryDestroy(g) == TRUE && g->refCount == 1 && RpD3D9GeometryGetUsageFlags(g) == 0x08);
    CHECK(RpGeometryDestroy(g) == TRUE && RpD3D9GeometryGetUsageFlags((RpGeometry*)g) == 0 && rw::Geometry::numAllocated == baseG);

    // flags / texture set normalisation
    RpGeometry* a = RpGeometryCreate(3, 1, rpGEOMETRYTEXTURED2 | rpGEOMETRYPRELIT);
    CHECK(a->numTexCoordSets == 2 && a->flags == (rpGEOMETRYTEXTURED2 | rpGEOMETRYPRELIT) && a->texCoords[1] && a->colors);
    RpGeometryDestroy(a);
    a = RpGeometryCreate(3, 1, rpGEOMETRYTEXTURED | (3u << 16));                  // explicit count 3 -> TEXTURED2 flag, 3 sets
    CHECK(a->numTexCoordSets == 3 && a->flags == rpGEOMETRYTEXTURED2 && a->texCoords[2]);
    RpGeometryDestroy(a);
    a = RpGeometryCreate(3, 1, rpGEOMETRYPOSITIONS);
    CHECK(a->numTexCoordSets == 0 && a->flags == rpGEOMETRYPOSITIONS && a->texCoords[0] == nullptr);
    RpGeometryDestroy(a);
    a = RpGeometryCreate(3, 1, rpGEOMETRYMODULATEMATERIALCOLOR | rpGEOMETRYPRELIT | rpGEOMETRYTEXTURED | rpGEOMETRYPOSITIONS); // roadsigns
    CHECK(a->flags == 0x4E && a->numTexCoordSets == 1);
    RpGeometryDestroy(a);
    CHECK(RpGeometryCreate(0x10000, 1, 0) == nullptr && RpGeometryCreate(-1, 1, 0) == nullptr && RpGeometryCreate(3, -1, 0) == nullptr);
    a = RpGeometryCreate(0, 0, rpGEOMETRYTRISTRIP);                               // empty geometry is legal
    CHECK(a && a->numVertices == 0);
    CHECK(RpGeometryLock(a, rpGEOMETRYLOCKALL) == a && RpGeometryUnlock(a) == a && a->meshHeader && a->meshHeader->numMeshes == 0);
    RpGeometryDestroy(a);
    CHECK(rw::Geometry::numAllocated == baseG);
}

static void GeometryMeshTests() {
    Section("geometry lock / unlock / mesh header");
    const int baseG = rw::Geometry::numAllocated, baseM = rw::Material::numAllocated;
    RpGeometry* g = RpGeometryCreate(6, 4, rpGEOMETRYTEXTURED | rpGEOMETRYPRELIT); // triangle list
    RpMaterial* m1 = RpMaterialCreate(); RpMaterial* m2 = RpMaterialCreate(); RpMaterial* unused = RpMaterialCreate();
    CHECK(RpGeometryLock(g, rpGEOMETRYLOCKALL) == g && (g->lockedSinceInst & rpGEOMETRYLOCKALL) == rpGEOMETRYLOCKALL);
    RwV3d* v = RpMorphTargetGetVertices(RpGeometryGetMorphTarget(g, 0));
    for (int i = 0; i < 6; i++) v[i] = {float(i), float(i * i), 1.0f};
    RpTriangle* t = RpGeometryGetTriangles(g);
    CHECK(RpGeometryTriangleSetVertexIndices(g, &t[0], 0, 1, 2) == g);
    RpGeometryTriangleSetVertexIndices(g, &t[1], 2, 1, 3);
    RpGeometryTriangleSetVertexIndices(g, &t[2], 3, 4, 5);
    RpGeometryTriangleSetVertexIndices(g, &t[3], 5, 4, 0);
    CHECK(t[0].v[0] == 0 && t[0].v[1] == 1 && t[0].v[2] == 2);
    CHECK(RpGeometryTriangleSetMaterial(g, &t[0], m1) == g && m1->refCount == 2 && t[0].matId == 0 && RpGeometryGetNumMaterials(g) == 1);
    CHECK(RpGeometryTriangleSetMaterial(g, &t[1], m2) == g && t[1].matId == 1 && m2->refCount == 2);
    CHECK(RpGeometryTriangleSetMaterial(g, &t[2], m1) == g && t[2].matId == 0 && m1->refCount == 2);  // already listed: no second reference
    CHECK(RpGeometryTriangleGetMaterial(g, &t[2]) == m1 && RpGeometryGetMaterial(g, 1) == m2);
    CHECK(_rpMaterialListAppendMaterial(&g->matList, unused) == 2);              // listed but no triangle uses it
    // t[3] still has no material: Unlock must fail cleanly (librw would assert)
    CHECK(RpGeometryUnlock(g) == nullptr);
    CHECK(RpGeometryTriangleSetMaterial(g, &t[3], m2) == g);
    CHECK(RpGeometryUnlock(g) == g && g->meshHeader);
    RpMeshHeader* h = g->meshHeader;
    CHECK(h->numMeshes == 2 && h->flags == 0 && h->totalIndices == 12);          // 'unused' has no mesh: empty materials are dropped
    RpMesh* mesh = h->getMeshes();
    CHECK(mesh[0].material == m1 && mesh[0].numIndices == 6 && mesh[1].material == m2 && mesh[1].numIndices == 6);
    CHECK(mesh[0].indices[0] == 0 && mesh[0].indices[1] == 1 && mesh[0].indices[2] == 2 && mesh[0].indices[3] == 3 && mesh[0].indices[5] == 5);
    CHECK(mesh[1].indices[0] == 2 && mesh[1].indices[2] == 3 && mesh[1].indices[3] == 5);
    CHECK(FromMeshes(g) == FromTriangles(g));
    const uint16_t serial = h->serialNum;

    // Lock without polygons keeps the header; the locked bits accumulate until the pipeline instances (it clears them)
    g->lockedSinceInst = 0;
    CHECK(RpGeometryLock(g, rpGEOMETRYLOCKPRELIGHT) == g && g->meshHeader == h && g->lockedSinceInst == rpGEOMETRYLOCKPRELIGHT);
    CHECK(RpGeometryUnlock(g) == g && g->meshHeader == h);
    RpGeometryLock(g, rpGEOMETRYLOCKVERTICES);
    CHECK(g->lockedSinceInst == (rpGEOMETRYLOCKPRELIGHT | rpGEOMETRYLOCKVERTICES));
    // polygons: header dropped, rebuilt on unlock with a new serial number (-> full re-instance)
    CHECK(RpGeometryLock(g, rpGEOMETRYLOCKPOLYGONS) == g && g->meshHeader == nullptr);
    CHECK(RpGeometryUnlock(g) == g && g->meshHeader && g->meshHeader->serialNum != serial);

    // RpAtomicConvertGeometryToTS equivalent: lock all, set the strip flag, unlock
    RpGeometryLock(g, rpGEOMETRYLOCKALL);
    RpGeometrySetFlags(g, g->flags | rpGEOMETRYTRISTRIP);
    CHECK(RpGeometryUnlock(g) == g && g->meshHeader->flags == 1);
    CHECK(FromMeshes(g) == FromTriangles(g));
    // ... and back (ToTL)
    RpGeometryLock(g, rpGEOMETRYLOCKALL);
    RpGeometrySetFlags(g, g->flags & ~rpGEOMETRYTRISTRIP);
    CHECK(RpGeometryUnlock(g) == g && g->meshHeader->flags == 0 && FromMeshes(g) == FromTriangles(g));

    // ForAllMaterials (stops at the first NULL) / ForAllMeshes
    g_matCb = 0; g_stopAt = 99;
    CHECK(RpGeometryForAllMaterials(g, MatCB, nullptr) == g && g_matCb == 3);
    g_matCb = 0; g_stopAt = 2;
    RpGeometryForAllMaterials(g, MatCB, nullptr);
    CHECK(g_matCb == 2);
    int nm = 0;
    RpGeometryForAllMeshes(g, [](RpMesh*, RpMeshHeader*, void* d) -> RpMesh* { ++*(int*)d; return (RpMesh*)1; }, &nm);
    CHECK(nm == 2);

    // destroying the geometry releases the list's references
    CHECK(m1->refCount == 2 && m2->refCount == 2 && unused->refCount == 2);
    CHECK(RpGeometryDestroy(g) == TRUE);
    CHECK(m1->refCount == 1 && m2->refCount == 1 && unused->refCount == 1);
    RpMaterialDestroy(m1); RpMaterialDestroy(m2); RpMaterialDestroy(unused);
    CHECK(rw::Geometry::numAllocated == baseG && rw::Material::numAllocated == baseM);
}

static void StripTests() {
    Section("tristrip builder (grid, random, degenerate input)");
    const int baseG = rw::Geometry::numAllocated, baseM = rw::Material::numAllocated;
    const int N = 40; // N x N quads = 2*N*N triangles
    RpGeometry* g = RpGeometryCreate((N + 1) * (N + 1), 2 * N * N, rpGEOMETRYTRISTRIP | rpGEOMETRYTEXTURED);
    RpMaterial* mat[3] = {RpMaterialCreate(), RpMaterialCreate(), RpMaterialCreate()};
    RpGeometryLock(g, rpGEOMETRYLOCKALL);
    RpTriangle* t = RpGeometryGetTriangles(g);
    int k = 0;
    unsigned seed = 12345;
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        const uint16_t i0 = uint16_t(y * (N + 1) + x), i1 = i0 + 1, i2 = uint16_t(i0 + N + 1), i3 = i2 + 1;
        seed = seed * 1103515245u + 12345u;
        RpMaterial* m = mat[(seed >> 16) % 3];
        RpGeometryTriangleSetVertexIndices(g, &t[k], i0, i2, i1); RpGeometryTriangleSetMaterial(g, &t[k], m); k++;
        RpGeometryTriangleSetVertexIndices(g, &t[k], i1, i2, i3); RpGeometryTriangleSetMaterial(g, &t[k], m); k++;
    }
    const auto t0 = std::chrono::steady_clock::now();
    CHECK(RpGeometryUnlock(g) == g);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    RpMeshHeader* h = g->meshHeader;
    CHECK(h->flags == 1 && h->numMeshes == 3 && FromMeshes(g) == FromTriangles(g));
    size_t tris = 0; for (auto& kv : FromMeshes(g)) tris += kv.second.size();
    std::printf("  %d triangles, 3 materials: %u strip indices (%.2f per triangle, list would be 3.00), built in %.1f ms\n", 2 * N * N, h->totalIndices,
                double(h->totalIndices) / (2 * N * N), ms);
    CHECK(tris == size_t(2 * N * N) && h->totalIndices < 3u * 2 * N * N);
    // strips start at even indices => every mesh's first triangle is wound as in the list (checked by the equality above, which compares windings)
    RpGeometryDestroy(g);
    for (auto* m : mat) RpMaterialDestroy(m);

    // random soup incl. duplicated and degenerate triangles, single mesh
    RpGeometry* r = RpGeometryCreate(50, 400, rpGEOMETRYTRISTRIP);
    RpMaterial* rm = RpMaterialCreate();
    RpGeometryLock(r, rpGEOMETRYLOCKALL);
    for (int i = 0; i < 400; i++) {
        seed = seed * 1103515245u + 12345u; const uint16_t a = (seed >> 16) % 50;
        seed = seed * 1103515245u + 12345u; const uint16_t b = (seed >> 16) % 50;
        seed = seed * 1103515245u + 12345u; const uint16_t c = (seed >> 16) % 50;
        RpGeometryTriangleSetVertexIndices(r, &RpGeometryGetTriangles(r)[i], a, b, c);
        RpGeometryTriangleSetMaterial(r, &RpGeometryGetTriangles(r)[i], rm);
    }
    CHECK(RpGeometryUnlock(r) == r && r->meshHeader->numMeshes == 1);
    CHECK(FromMeshes(r) == FromTriangles(r));
    RpGeometryDestroy(r);
    RpMaterialDestroy(rm);
    CHECK(rw::Geometry::numAllocated == baseG && rw::Material::numAllocated == baseM);
}

static void MorphTargetTests() {
    Section("morph target bounding sphere");
    RpGeometry* g = RpGeometryCreate(4, 0, rpGEOMETRYPOSITIONS);
    RpMorphTarget* mt = RpGeometryGetMorphTarget(g, 0);
    RwV3d* v = RpMorphTargetGetVertices(mt);
    v[0] = {0, 0, 0}; v[1] = {4, 0, 0}; v[2] = {0, 2, 0}; v[3] = {4, 2, 6};
    RwSphere s{};
    CHECK(RpMorphTargetCalcBoundingSphere(mt, &s) == mt);
    // bbox [0..4]x[0..2]x[0..6] -> centre (2,1,3); farthest vertex (0,0,0)/(4,2,6): sqrt(4+1+9) = 3.7417; exe scales by 1.001
    CHECK(Near(s.center.x, 2) && Near(s.center.y, 1) && Near(s.center.z, 3));
    CHECK(Near(s.radius, std::sqrt(14.0f) * 1.001f, 1e-4f));
    const RwSphere set{{1, 2, 3}, 4};
    RpMorphTargetSetBoundingSphere(mt, &set);
    CHECK(RpMorphTargetGetBoundingSphere(mt)->radius == 4 && RpMorphTargetGetBoundingSphere(mt)->center.z == 3);
    // one vertex and zero vertices
    RpGeometry* one = RpGeometryCreate(1, 0, rpGEOMETRYPOSITIONS);
    RpMorphTargetGetVertices(RpGeometryGetMorphTarget(one, 0))[0] = {5, 6, 7};
    RpMorphTargetCalcBoundingSphere(RpGeometryGetMorphTarget(one, 0), &s);
    CHECK(Near(s.center.x, 5) && Near(s.radius, 0));
    CHECK(RpGeometryAddMorphTarget(one) == 1 && RpGeometryGetNumMorphTargets(one) == 2 && RpGeometryAddMorphTargets(one, 2) == 2 && one->numMorphTargets == 4);
    CHECK(RpGeometryGetMorphTarget(one, 3)->vertices != nullptr);
    RpGeometryDestroy(one); RpGeometryDestroy(g);
}

// ---------------------------------------------------------------------------------------------------------------------------------
static std::vector<uint8_t> ReadFile(const char* path) {
    std::vector<uint8_t> b;
    if (FILE* f = std::fopen(path, "rb")) {
        std::fseek(f, 0, SEEK_END); b.resize(std::ftell(f)); std::fseek(f, 0, SEEK_SET);
        b.resize(std::fread(b.data(), 1, b.size(), f)); std::fclose(f);
    }
    return b;
}

static void DffTest(const char* path) {
    Section(path);
    auto bytes = ReadFile(path);
    CHECK(!bytes.empty());
    rw::StreamMemory sm;
    sm.open(bytes.data(), (uint32_t)bytes.size());
    uint32_t len = 0, ver = 0;
    CHECK(rw::findChunk(&sm, rw::ID_CLUMP, &len, &ver));
    rw::Clump* clump = rw::Clump::streamRead(&sm);
    CHECK(clump != nullptr);
    if (!clump) return;
    int atomics = 0, geos = 0, meshes = 0, mats = 0, tris = 0, verts = 0, with2dfx = 0, withExtraCol = 0, withBreak = 0, envMats = 0, specMats = 0, native = 0;
    int meshMismatch = 0, rebuildMismatch = 0, badIdx = 0, emptyMesh = 0, tsRebuild = 0;
    uint32_t dxBytes = 0;
    std::set<RpGeometry*> seen;
    for (rw::LLLink* lnk = clump->atomics.link.next; lnk != &clump->atomics.link; lnk = lnk->next) {
        rw::Atomic* a = rw::Atomic::fromClump(lnk);
        atomics++;
        RpGeometry* g = a->geometry;
        if (!g || !seen.insert(g).second) continue;
        geos++;
        native += (g->flags & rpGEOMETRYNATIVE) != 0;
        verts += RpGeometryGetNumVertices(g); tris += RpGeometryGetNumTriangles(g);
        mats += RpGeometryGetNumMaterials(g);
        if (Raw* r = RawOf(g, g_off2dfx); r->len) { with2dfx++; dxBytes += r->len; }
        withExtraCol += RawOf(g, g_offExtraCol)->len != 0;
        withBreak += RawOf(g, g_offBreakable)->len != 0;
        for (int i = 0; i < RpGeometryGetNumMaterials(g); i++) {
            envMats += RawOf(RpGeometryGetMaterial(g, i), g_offEnv)->len != 0;
            specMats += RawOf(RpGeometryGetMaterial(g, i), g_offSpec)->len != 0;
        }
        if (!g->meshHeader) { emptyMesh++; continue; }
        // mesh iteration as the game does it (RpGeometryGetMesh / RwCompatMeshHeaderMeshes): every mesh has a listed material, valid indices
        RpMeshHeader* h = g->meshHeader;
        RpMesh* mesh = h->getMeshes();
        for (unsigned i = 0; i < h->numMeshes; i++, mesh++) {
            meshes++;
            if (_rpMaterialListFindMaterialIndex(&g->matList, mesh->material) < 0) badIdx++;
            for (uint32_t j = 0; j < mesh->numIndices; j++) if (mesh->indices[j] >= g->numVertices) { badIdx++; break; }
            if (mesh->numIndices == 0) emptyMesh++;
        }
        if (g->triangles) {
            const bool strip = (h->flags & 1) != 0;
            // file meshes (tristrips of the exporter) must draw the same triangles as the triangle array
            if (FromMeshes(g) != FromTriangles(g)) meshMismatch++;
            // rebuild through the shim exactly like RpAtomicConvertGeometryToTL / RpAtomicConvertGeometryToTS do
            for (int pass = 0; pass < 2; pass++) {
                RpGeometryLock(g, rpGEOMETRYLOCKALL);
                RpGeometrySetFlags(g, pass == 0 ? (g->flags & ~rpGEOMETRYTRISTRIP) : (g->flags | rpGEOMETRYTRISTRIP));
                if (RpGeometryUnlock(g) != g || FromMeshes(g) != FromTriangles(g)) rebuildMismatch++;
                if (pass == 1) tsRebuild += g->meshHeader->totalIndices;
            }
            (void)strip;
        }
    }
    std::printf("  %d atomics, %d geometries (%d native), %d verts, %d tris, %d materials, %d meshes (file meshes empty/missing: %d, bad refs: %d)\n",
                atomics, geos, native, verts, tris, mats, meshes, emptyMesh, badIdx);
    std::printf("  SA plugin payloads kept: 2dfx geometries=%d (%u bytes), extra colours=%d, breakable=%d, env mats=%d, spec mats=%d\n",
                with2dfx, dxBytes, withExtraCol, withBreak, envMats, specMats);
    std::printf("  file strips vs triangle array mismatches=%d, TL/TS rebuild mismatches=%d (strip indices after rebuild: %d)\n", meshMismatch, rebuildMismatch, tsRebuild);
    CHECK(atomics > 0 && geos > 0 && native == 0);
    CHECK(badIdx == 0 && emptyMesh == 0);
    CHECK(meshMismatch == 0 && rebuildMismatch == 0);
    // streams out and back in: the kept plugin data is written again (geometry + materials)
    const int nbefore = with2dfx + withExtraCol + withBreak + envMats + specMats;
    if (nbefore) {
        rw::Atomic* a0 = rw::Atomic::fromClump(clump->atomics.link.next);
        std::vector<uint8_t> buf(32u << 20);
        rw::StreamMemory out;
        out.open(buf.data(), 0, (uint32_t)buf.size());
        for (rw::LLLink* l = clump->atomics.link.next; l != &clump->atomics.link; l = l->next) {
            RpGeometry* g = rw::Atomic::fromClump(l)->geometry;
            if (RawOf(g, g_off2dfx)->len || RawOf(g, g_offExtraCol)->len || RawOf(g, g_offBreakable)->len) { a0 = rw::Atomic::fromClump(l); break; }
        }
        RpGeometry* g = a0->geometry;
        const uint32_t sz = RpGeometryStreamGetSize(g);
        CHECK(RpGeometryStreamWrite(g, &out) == g);
        const uint32_t written = out.tell();
        CHECK(written == sz + 12);
        rw::StreamMemory in;
        in.open(buf.data(), written);
        uint32_t l2 = 0, v2 = 0;
        CHECK(rw::findChunk(&in, rw::ID_GEOMETRY, &l2, &v2) && l2 == sz);
        RpGeometry* back = RpGeometryStreamRead(&in);
        CHECK(back != nullptr);
        if (back) {
            CHECK(back->numVertices == g->numVertices && back->numTriangles == g->numTriangles && back->matList.numMaterials == g->matList.numMaterials);
            CHECK(RawOf(back, g_off2dfx)->len == RawOf(g, g_off2dfx)->len && RawOf(back, g_offExtraCol)->len == RawOf(g, g_offExtraCol)->len &&
                  RawOf(back, g_offBreakable)->len == RawOf(g, g_offBreakable)->len);
            if (RawOf(g, g_off2dfx)->len) CHECK(std::memcmp(RawOf(back, g_off2dfx)->data, RawOf(g, g_off2dfx)->data, RawOf(g, g_off2dfx)->len) == 0);
            RpGeometryDestroy(back);
        }
    }
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    _set_error_mode(_OUT_TO_STDERR);                       // asserts print and abort instead of opening a message box
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    HWND wnd = CreateWindowA("STATIC", "rw_world_geometry_test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(wnd != nullptr);
    rw::MemoryFunctions mf{}; // plain CRT heap, same as the engine layer (engine.cpp)
    mf.rwmalloc  = [](size_t sz, unsigned) -> void* { return sz ? std::malloc(sz) : nullptr; };
    mf.rwrealloc = [](void* p, size_t sz, unsigned) -> void* { return std::realloc(p, sz); };
    mf.rwfree    = [](void* p) { std::free(p); };
    CHECK(rw::Engine::init(&mf));
    CHECK(RpWorldPluginAttach() == TRUE && RpWorldPluginAttach() == TRUE);   // mesh / native data / rights plugins (idempotent)
    // SA's private plugin chunks on geometry / material (04c registers the real ones; here they only keep the payload)
    g_off2dfx = RegRaw<rw::Geometry>(0x253F2F8);
    g_offExtraCol = RegRaw<rw::Geometry>(0x253F2F9);
    g_offBreakable = RegRaw<rw::Geometry>(0x253F2FD);
    g_offEnv = RegRaw<rw::Material>(0x253F2FC);
    g_offSpec = RegRaw<rw::Material>(0x253F2F6);
    CHECK(g_off2dfx > 0 && g_offExtraCol > 0 && g_offBreakable > 0 && g_offEnv > 0 && g_offSpec > 0);
    rw::EngineOpenParams params{};
    params.window = wnd;
    // Device-less engine: the objects under test never touch the D3D9 device, so the render device's system callback is replaced by a
    // stub (every request "succeeds") instead of going through Direct3DCreate9 (flaky / hanging under Wine). Engine::start is not called
    // (the D3D9 driver constructor creates shaders on the device); its engine-plugin constructors are run by hand below.
    rw::d3d::renderdevice.system = [](rw::DeviceReq, void*, int32_t) -> int32_t { return 1; };
    CHECK(rw::Engine::open(&params));
    rw::Engine::s_plglist.construct(rw::engine); // what Engine::start does for the engine plugins (texture globals, ...) minus the D3D9 driver ctor

    LightTests();
    WorldTests();
    MaterialTests();
    GeometryCreateTests();
    GeometryMeshTests();
    StripTests();
    MorphTargetTests();
    for (int i = 1; i < argc; i++) DffTest(argv[i]);

    rw::Engine::s_plglist.destruct(rw::engine);
    DestroyWindow(wnd);
    std::printf("\nrw_world_geometry_test: %d checks passed, %d failed\n%s\n", g_pass, g_fail, g_fail ? "FAILED" : "PASSED");
    return g_fail;
}
