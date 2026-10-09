// P2B-04b: RpGeometry* / RpMorphTarget* on top of librw (rw::Geometry / rw::MorphTarget).
//   RpGeometry{Create,Destroy,Lock,Unlock,ForAllMaterials,TriangleGetMaterial,TriangleSetMaterial,TriangleSetVertexIndices},
//   RpD3D9GeometrySetUsageFlags, RpMorphTargetCalcBoundingSphere (+ extras: AddMorphTargets, ForAllMeshes, StreamRead/Write/GetSize).
//   RpGeometryGet{Flags,NumVertices,NumTriangles,Triangles,Material,NumMaterials,MorphTarget,NumMorphTargets,PreLightColors,
//   VertexTexCoords,NumTexCoordSets}, RpGeometrySetFlags, RpMorphTargetGet*/Set* are accessor macros in fakerw/rwaccessors.h;
//   the mesh header is reached through RwCompatGeometryMeshHeader / RwCompatMeshHeaderMeshes (RenderWare/RwCompat.h): rw::MeshHeader
//   {flags,numMeshes,serialNum,totalIndices,pad} + rw::Mesh{indices,numIndices,material} (RpMesh) are layout-compatible for what the game reads.
//   NOT here (batch 04c): RpGeometryRegisterPlugin / RpGeometryRegisterPluginStream. The plugin data of a streamed geometry (2DFX 0x253F2F8,
//   extra colours 0x253F2F9, breakable 0x253F2FD) is kept by librw as soon as those plugins are registered on rw::Geometry.
//
// W-adapters (librw differs from the exe; exe addresses in brackets):
//  * RpGeometryCreate [0x74CA90]: range checks (numVert < 0x10000), texture-set count/flag normalisation (TEXTURED -> 1 set, TEXTURED2 -> 2,
//    explicit count in bits 16..23; flags keep only bits 24..27 above the low byte).
//  * RpGeometryUnlock [0x74C800]: the mesh header is rebuilt here, NOT with rw::Geometry::buildMeshes: librw's tristripper verifies with an
//    O(triangles^2) search, exit(1)s on a mismatch and underflows on empty materials. The shim ports the exe's mesh ORDER (qsort comparator
//    0x759640 over triangles tagged with first-seen texture/raster/pipeline indices, one mesh per run of equal material: opaque before
//    transparent, then raster / pipeline / texture / material index); with rpGEOMETRYTRISTRIP a greedy strip builder (own, not the exe's
//    tunnelling stripper) joins strips with degenerate triangles (even start parity).
//    Invalid input (a triangle with a material index >= numMaterials, e.g. 0xFFFF) makes Unlock fail with NULL instead of asserting.
//  * RpMorphTargetCalcBoundingSphere [0x74C200]: centre of the vertex bounding box, radius = farthest vertex distance * 1.001
//    (librw: half-diagonal of the box, no margin).
//  * RpGeometryLock/Unlock instancing invalidation: Lock ORs the mode into rw::Geometry::lockedSinceInst (the D3D9 pipeline re-instances
//    exactly the locked streams, or everything when the mesh header's serial number changed) and drops the mesh header on LOCKPOLYGONS.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace {
// RpD3D9GeometrySetUsageFlags has no librw counterpart; the flags live in a side table keyed by the geometry (erased by RpGeometryDestroy).
// A geometry freed through librw directly (atomic / clump destruction) leaves its entry behind until the address is reused -> the flags are
// only a usage HINT (dynamic vertex buffers), see RpD3D9GeometryGetUsageFlags in rwextra.h.
std::unordered_map<const rw::Geometry*, RwUInt32>& UsageFlags() {
    static std::unordered_map<const rw::Geometry*, RwUInt32> s;
    return s;
}

// Greedy triangle-strip builder for the triangles `ids` (indices into g->triangles) of one material. A triangle k of a strip s is
// (s[k], s[k+1], s[k+2]) for even k and (s[k+1], s[k], s[k+2]) for odd k, so the strip continues over the directed edge (s[k+2] -> s[k+1])
// when k+1 is odd and (s[k+1] -> s[k+2]) when k+1 is even. Strips are joined with duplicated vertices; the next strip always starts at an even index.
void BuildStrips(const rw::Triangle* tris, const std::vector<int>& ids, std::vector<uint16_t>& out) {
    const size_t n = ids.size();
    auto key = [](uint16_t p, uint16_t q) { return (uint32_t(p) << 16) | q; };
    std::unordered_multimap<uint32_t, uint32_t> edges; // directed edge -> index into `ids`
    edges.reserve(n * 3);
    for (uint32_t i = 0; i < n; i++) {
        const rw::Triangle& t = tris[ids[i]];
        edges.emplace(key(t.v[0], t.v[1]), i);
        edges.emplace(key(t.v[1], t.v[2]), i);
        edges.emplace(key(t.v[2], t.v[0]), i);
    }
    std::vector<char> used(n, 0);
    auto thirdOf = [&](uint32_t i, uint16_t p, uint16_t q) -> uint16_t {
        const rw::Triangle& t = tris[ids[i]];
        for (int e = 0; e < 3; e++) {
            if (t.v[e] == p && t.v[(e + 1) % 3] == q) {
                return t.v[(e + 2) % 3];
            }
        }
        return t.v[2];
    };
    auto next = [&](uint16_t p, uint16_t q, uint16_t& w) -> int {
        auto range = edges.equal_range(key(p, q));
        for (auto it = range.first; it != range.second; ++it) {
            if (!used[it->second]) {
                w = thirdOf(it->second, p, q);
                return int(it->second);
            }
        }
        return -1;
    };

    std::vector<uint16_t> s;
    for (uint32_t start = 0; start < n; start++) {
        if (used[start]) {
            continue;
        }
        used[start] = 1;
        const rw::Triangle& t = tris[ids[start]];
        uint16_t a = t.v[0], b = t.v[1], c = t.v[2];
        // prefer a rotation that can be continued over its (c -> b) edge
        for (int r = 0; r < 3; r++) {
            uint16_t w;
            if (next(c, b, w) >= 0) {
                break;
            }
            const uint16_t na = b, nb = c, nc = a;
            a = na; b = nb; c = nc;
        }
        s.assign({a, b, c});
        for (size_t k = 0;; k++) { // k = index of the last triangle in the strip
            const uint16_t y = s[k + 1], z = s[k + 2];
            uint16_t w;
            const int i = ((k + 1) % 2 == 0) ? next(y, z, w) : next(z, y, w);
            if (i < 0) {
                break;
            }
            used[i] = 1;
            s.push_back(w);
        }
        if (!out.empty()) {
            out.push_back(out.back());
            out.push_back(s[0]);
            if (out.size() % 2) {
                out.push_back(s[0]);
            }
        }
        out.insert(out.end(), s.begin(), s.end());
    }
}

// One entry per triangle of the exe's mesh builder (_rpBuildMeshAddTriangle 0x758C00, 20 bytes): the material and four sort keys.
struct BuildEntry {
    int32_t  tri;      // index into g->triangles
    RpMaterial* mat;
    uint16_t matId;    // +0xC
    uint16_t texIdx;   // +0xE  first-seen index of the material's texture (over the triangles in file order)
    uint16_t rasIdx;   // +0x10 same for the texture's raster
    uint16_t pipeIdx;  // +0x12 same for the material's pipeline
};

// Sort flag of the comparator 0x759640: 0x10 for a material whose raster has an alpha pixel format (1555/4444/8888) or whose colour alpha != 0xFF.
unsigned BuildTransparency(const RpMaterial* m) {
    unsigned f = 0;
    if (m) {
        if (m->texture && m->texture->raster) {
            const unsigned fmt = (m->texture->raster->format & 0xF00);
            if (fmt == 0x100 || fmt == 0x300 || fmt == 0x500) {
                f = 0x10;
            }
        }
        if (m->color.alpha != 0xFF) {
            f |= 0x10;
        }
    }
    return f;
}

// qsort comparator of the exe's mesh builder (0x759640, used by _rpTriListMeshGenerate 0x75D530 and the tristrip generators 0x7591D0/0x75C380):
// equal materials compare 0; otherwise opaque before transparent, then ascending raster, pipeline, texture, material index.
int BuildCompare(const void* pa, const void* pb) {
    const BuildEntry* a = *static_cast<const BuildEntry* const*>(pa);
    const BuildEntry* b = *static_cast<const BuildEntry* const*>(pb);
    if (a->mat == b->mat) {
        return 0;
    }
    const int fa = (int)BuildTransparency(a->mat), fb = (int)BuildTransparency(b->mat);
    const int aLess = ((a->rasIdx < b->rasIdx) ? 8 : 0) | ((a->pipeIdx < b->pipeIdx) ? 4 : 0) | ((a->texIdx < b->texIdx) ? 2 : 0) | ((a->matId < b->matId) ? 1 : 0);
    const int bLess = ((b->rasIdx < a->rasIdx) ? 8 : 0) | ((b->pipeIdx < a->pipeIdx) ? 4 : 0) | ((b->texIdx < a->texIdx) ? 2 : 0) | ((b->matId < a->matId) ? 1 : 0);
    return (bLess | fa) - (aLess | fb);
}

// The exe's CRT qsort (0x8247E0, MSVC quicksort: median-of-three of lo/mid/hi, insertion-by-selection `shortsort` 0x824770 for <= 8 elements,
// explicit stack) ported 1:1. It is NOT stable, and BuildCompare returns 0 for all triangles of one material, so the triangle order inside
// a rebuilt mesh is whatever this algorithm produces; using the same algorithm reproduces the exe's index order (the platform qsort differs).
using QsortCmp = int (*)(const void*, const void*);
void QSwap(char* a, char* b, size_t w) {
    if (a != b) {
        while (w--) {
            const char t = *a;
            *a++ = *b;
            *b++ = t;
        }
    }
}
void QShortSort(char* lo, char* hi, size_t w, QsortCmp cmp) {
    while (hi > lo) {
        char* mx = lo;
        for (char* p = lo + w; p <= hi; p += w) {
            if (cmp(p, mx) > 0) {
                mx = p;
            }
        }
        QSwap(mx, hi, w);
        hi -= w;
    }
}
void MsvcQsort(void* base, size_t num, size_t w, QsortCmp cmp) {
    if (num < 2 || w == 0) {
        return;
    }
    char* lostk[30];
    char* histk[30];
    int stkptr = 0;
    char* lo = static_cast<char*>(base);
    char* hi = lo + w * (num - 1);
    for (;;) {
        const size_t size = (size_t)(hi - lo) / w + 1;
        if (size <= 8) {
            QShortSort(lo, hi, w, cmp);
        } else {
            char* mid = lo + (size / 2) * w;
            if (cmp(lo, mid) > 0) QSwap(lo, mid, w);
            if (cmp(lo, hi) > 0) QSwap(lo, hi, w);
            if (cmp(mid, hi) > 0) QSwap(mid, hi, w);
            char* loguy = lo;
            char* higuy = hi;
            for (;;) {
                if (mid > loguy) {
                    do { loguy += w; } while (loguy < mid && cmp(loguy, mid) <= 0);
                }
                if (mid <= loguy) {
                    do { loguy += w; } while (loguy <= hi && cmp(loguy, mid) <= 0);
                }
                do { higuy -= w; } while (higuy > mid && cmp(higuy, mid) > 0);
                if (higuy < loguy) {
                    break;
                }
                QSwap(loguy, higuy, w);
                if (mid == higuy) {
                    mid = loguy;
                }
            }
            higuy += w;
            if (mid < higuy) {
                do { higuy -= w; } while (higuy > mid && cmp(higuy, mid) == 0);
            }
            if (mid >= higuy) {
                do { higuy -= w; } while (higuy > lo && cmp(higuy, mid) == 0);
            }
            if (higuy - lo >= hi - loguy) {
                if (lo < higuy) { lostk[stkptr] = lo; histk[stkptr] = higuy; ++stkptr; }
                if (loguy < hi) { lo = loguy; continue; }
            } else {
                if (loguy < hi) { lostk[stkptr] = loguy; histk[stkptr] = hi; ++stkptr; }
                if (lo < higuy) { hi = higuy; continue; }
            }
        }
        if (--stkptr < 0) {
            break;
        }
        lo = lostk[stkptr];
        hi = histk[stkptr];
    }
}

// Rebuilds g->meshHeader from g->triangles / g->matList like the exe's RpGeometryUnlock (0x74C800): triangles are tagged with first-seen
// texture / raster / pipeline indices, sorted with BuildCompare and cut into one mesh per run of equal material (so the MESH ORDER is the
// exe's: opaque before transparent, then by raster / pipeline / texture / material index, NOT material-list order). Tristrip geometry
// gets the same mesh order; the strips themselves come from our own greedy builder (the exe's RpBuildMeshGenerateDefaultTriStrip
// 0x7591D0 is a ~6 KB tunnelling/cost stripper; its output only changes the index sequence inside a mesh, not what is drawn).
// Returns false (header left empty) on invalid triangle material indices.
bool BuildMeshHeader(rw::Geometry* g) {
    const int32_t nMat = g->matList.numMaterials;
    for (int32_t i = 0; i < g->numTriangles; i++) {
        if (g->triangles[i].matId >= nMat) {
            return false;
        }
    }
    const bool strip = (g->flags & rw::Geometry::TRISTRIP) != 0;

    std::vector<BuildEntry> entries(g->numTriangles);
    std::vector<const void*> texs, rass, pipes;
    auto indexOf = [](std::vector<const void*>& v, const void* key) -> uint16_t {
        for (size_t i = 0; i < v.size(); i++) {
            if (v[i] == key) {
                return (uint16_t)i;
            }
        }
        v.push_back(key);
        return (uint16_t)(v.size() - 1);
    };
    for (int32_t i = 0; i < g->numTriangles; i++) {
        const uint16_t id = g->triangles[i].matId;
        RpMaterial* m = g->matList.materials[id];
        BuildEntry& e = entries[i];
        e.tri     = i;
        e.mat     = m;
        e.matId   = id;
        e.texIdx  = indexOf(texs, m ? m->texture : nullptr);
        e.rasIdx  = indexOf(rass, (m && m->texture) ? m->texture->raster : nullptr);
        e.pipeIdx = indexOf(pipes, m ? m->pipeline : nullptr);
    }
    std::vector<const BuildEntry*> order(g->numTriangles);
    for (int32_t i = 0; i < g->numTriangles; i++) {
        order[i] = &entries[i];
    }
    if (!order.empty()) {
        MsvcQsort(order.data(), order.size(), sizeof(order[0]), BuildCompare);
    }

    struct Run { RpMaterial* mat; std::vector<uint16_t> idx; };
    std::vector<Run> runs;
    size_t total = 0;
    for (size_t k = 0; k < order.size();) {
        size_t e = k;
        std::vector<int> ids;
        while (e < order.size() && order[e]->mat == order[k]->mat) {
            ids.push_back(order[e]->tri);
            e++;
        }
        Run r{order[k]->mat, {}};
        if (strip) {
            BuildStrips(g->triangles, ids, r.idx);
        } else {
            r.idx.reserve(ids.size() * 3);
            for (int t : ids) {
                r.idx.insert(r.idx.end(), g->triangles[t].v, g->triangles[t].v + 3);
            }
        }
        total += r.idx.size();
        runs.push_back(std::move(r));
        k = e;
    }

    rw::Engine::memfuncs.rwfree(g->meshHeader);
    g->meshHeader = nullptr;
    rw::MeshHeader* h = g->allocateMeshes((int32_t)runs.size(), uint32_t(total), 0);
    h->flags          = strip ? rw::MeshHeader::TRISTRIP : 0;
    rw::Mesh* mesh    = h->getMeshes();
    uint16_t* dst     = reinterpret_cast<uint16_t*>(mesh + runs.size());
    for (const Run& r : runs) {
        mesh->material   = r.mat;
        mesh->numIndices = uint32_t(r.idx.size());
        mesh->indices    = dst;
        std::memcpy(dst, r.idx.data(), r.idx.size() * sizeof(uint16_t));
        dst += r.idx.size();
        mesh++;
    }
    return true;
}
} // namespace

// W: see the file header. Fails (NULL) like RW for numVert >= 0x10000 / negative sizes. Triangle material indices start as 0xFFFF.
RpGeometry* RpGeometryCreate(RwInt32 numVert, RwInt32 numTriangles, RwUInt32 format) {
    if (numVert < 0 || numVert >= 0x10000 || numTriangles < 0) {
        return nullptr;
    }
    RwUInt32 numTex = (format & 0xFF0000) >> 16;
    if (numTex == 0) {
        numTex = (format & rpGEOMETRYTEXTURED2) ? 2 : ((format >> 2) & 1);
    }
    if (numTex > 8) {
        return nullptr;
    }
    RwUInt32 low = format & 0xFF & ~(rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2);
    low |= (numTex > 1) ? rpGEOMETRYTEXTURED2 : (numTex == 1 ? rpGEOMETRYTEXTURED : 0);
    return rw::Geometry::create(numVert, numTriangles, low | (format & 0x0F000000) | (numTex << 16));
}

// D: RW frees when the count would drop to 0 (a count of 1 or less); returns TRUE.
RwBool RpGeometryDestroy(RpGeometry* geometry) {
    if (!geometry) {
        return FALSE;
    }
    if (geometry->refCount <= 1) {
        UsageFlags().erase(geometry);
    }
    geometry->destroy();
    return TRUE;
}

// A: native (pre-instanced) geometry has no editable data -> NULL. Otherwise librw's lock: remember what was locked (instancing
// invalidation) and drop the mesh header when the polygons are locked (it is rebuilt by RpGeometryUnlock).
RpGeometry* RpGeometryLock(RpGeometry* geometry, RwInt32 lockMode) {
    if (!geometry || (geometry->flags & rpGEOMETRYNATIVE)) {
        return nullptr;
    }
    geometry->lock(lockMode);
    return geometry;
}

// W: see the file header. A geometry that still has its mesh header (not locked for polygons) is left untouched.
RpGeometry* RpGeometryUnlock(RpGeometry* geometry) {
    if (!geometry) {
        return nullptr;
    }
    if (geometry->meshHeader || (geometry->flags & rpGEOMETRYNATIVE)) {
        return geometry;
    }
    return BuildMeshHeader(geometry) ? geometry : nullptr;
}

// A: RW stops at the first callback that returns NULL.
RpGeometry* RpGeometryForAllMaterials(RpGeometry* geometry, RpMaterialCallBack fpCallBack, void* data) {
    if (!geometry || !fpCallBack) {
        return geometry;
    }
    for (RwInt32 i = 0; i < geometry->matList.numMaterials; i++) {
        if (!fpCallBack(geometry->matList.materials[i], data)) {
            break;
        }
    }
    return geometry;
}

// D: matId 0xFFFF = no material (RW does no further bounds check).
RpMaterial* RpGeometryTriangleGetMaterial(const RpGeometry* geometry, const RpTriangle* triangle) {
    if (triangle->matId == 0xFFFF) {
        return nullptr;
    }
    return geometry->matList.materials[triangle->matId];
}

// A: finds the material in the list (searching backwards like _rpMaterialListFindMaterialIndex) or appends it (taking a reference); NULL
// clears the index. Fails (NULL) when the list cannot grow.
RpGeometry* RpGeometryTriangleSetMaterial(RpGeometry* geometry, RpTriangle* triangle, RpMaterial* material) {
    if (!material) {
        triangle->matId = 0xFFFF;
        return geometry;
    }
    RwInt32 idx = _rpMaterialListFindMaterialIndex(&geometry->matList, material);
    if (idx < 0) {
        idx = _rpMaterialListAppendMaterial(&geometry->matList, material);
        if (idx < 0) {
            return nullptr;
        }
    }
    triangle->matId = static_cast<RwUInt16>(idx);
    return geometry;
}

// D
const RpGeometry* RpGeometryTriangleSetVertexIndices(const RpGeometry* geometry, RpTriangle* triangle, RwUInt16 vert1, RwUInt16 vert2, RwUInt16 vert3) {
    triangle->v[0] = vert1;
    triangle->v[1] = vert2;
    triangle->v[2] = vert3;
    return geometry;
}

// S (stored): the D3D9 usage hint for the vertex streams; honoured by the instance callbacks of the custom pipelines (P2B-07/08b) through
// RpD3D9GeometryGetUsageFlags.
void RpD3D9GeometrySetUsageFlags(RpGeometry* geometry, RpD3D9GeometryUsageFlag flags) {
    if (geometry) {
        UsageFlags()[geometry] = static_cast<RwUInt32>(flags);
    }
}

RwUInt32 RpD3D9GeometryGetUsageFlags(const RpGeometry* geometry) {
    auto it = UsageFlags().find(geometry);
    return it == UsageFlags().end() ? 0 : it->second;
}

// W: see the file header. Exe: bbox of the vertices, centre = (min+max)*0.5, radius = sqrt(max squared distance to the centre) * 1.001.
const RpMorphTarget* RpMorphTargetCalcBoundingSphere(const RpMorphTarget* morphTarget, RwSphere* boundingSphere) {
    const RwInt32 n = morphTarget->parent->numVertices;
    const RwV3d*  v = morphTarget->vertices;
    RwV3d lo{0, 0, 0}, hi{0, 0, 0};
    if (n > 0 && v) {
        lo = hi = v[0];
        for (RwInt32 i = 1; i < n; i++) {
            if (v[i].x < lo.x) lo.x = v[i].x;
            if (v[i].y < lo.y) lo.y = v[i].y;
            if (v[i].z < lo.z) lo.z = v[i].z;
            if (v[i].x > hi.x) hi.x = v[i].x;
            if (v[i].y > hi.y) hi.y = v[i].y;
            if (v[i].z > hi.z) hi.z = v[i].z;
        }
    }
    const RwV3d c{(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f};
    float maxSq = 0.0f;
    if (v) {
        for (RwInt32 i = 0; i < n; i++) {
            const float dx = v[i].x - c.x, dy = v[i].y - c.y, dz = v[i].z - c.z;
            const float d = (dx * dx + dy * dy) + dz * dz;
            if (d > maxSq) maxSq = d;
        }
    }
    float radius = 0.0f;
    if (maxSq > 0.0f) {
        radius = std::sqrt(maxSq);
    }
    boundingSphere->center = c;
    boundingSphere->radius = radius * 1.001f;
    return morphTarget;
}

// ---- 04ab extras (declared in rwextra.h) ----

// A: returns the index of the first new morph target, -1 for native geometry / bad count (RW: >= 0 on success).
RwInt32 RpGeometryAddMorphTargets(RpGeometry* geometry, RwInt32 mtcount) {
    if (!geometry || mtcount < 0 || (geometry->flags & rpGEOMETRYNATIVE)) {
        return -1;
    }
    const RwInt32 first = geometry->numMorphTargets;
    geometry->addMorphTargets(mtcount);
    return first;
}

RwInt32 RpGeometryAddMorphTarget(RpGeometry* geometry) {
    return RpGeometryAddMorphTargets(geometry, 1);
}

const RpGeometry* RpGeometryForAllMeshes(const RpGeometry* geometry, RpMeshCallBack fpCallBack, void* data) {
    RpMeshHeader* header = geometry->meshHeader;
    if (!header || !fpCallBack) {
        return geometry;
    }
    RpMesh* mesh = header->getMeshes();
    for (unsigned i = 0; i < header->numMeshes; i++, mesh++) {
        if (!fpCallBack(mesh, header, data)) {
            break;
        }
    }
    return geometry;
}

RpGeometry* RpGeometryStreamRead(RwStream* stream) {
    return rw::Geometry::streamRead(stream);
}

const RpGeometry* RpGeometryStreamWrite(const RpGeometry* geometry, RwStream* stream) {
    return const_cast<RpGeometry*>(geometry)->streamWrite(stream) ? geometry : nullptr;
}

RwUInt32 RpGeometryStreamGetSize(const RpGeometry* geometry) {
    return const_cast<RpGeometry*>(geometry)->streamGetSize();
}
#endif
