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
//    O(triangles^2) search, exit(1)s on a mismatch and underflows on empty materials. The shim builds one mesh per material that has
//    triangles (RW drops empty materials too), index order = triangle order; with rpGEOMETRYTRISTRIP a greedy strip builder joins strips
//    with degenerate triangles (even start parity), which is what RpBuildMeshGenerateTrivialTriStrip/DefaultTriStrip output looks like.
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

// Rebuilds g->meshHeader from g->triangles / g->matList. Returns false (header left empty) on invalid triangle material indices.
bool BuildMeshHeader(rw::Geometry* g) {
    const int32_t nMat = g->matList.numMaterials;
    for (int32_t i = 0; i < g->numTriangles; i++) {
        if (g->triangles[i].matId >= nMat) {
            return false;
        }
    }
    const bool strip = (g->flags & rw::Geometry::TRISTRIP) != 0;
    std::vector<std::vector<int>> ids(nMat);
    for (int32_t i = 0; i < g->numTriangles; i++) {
        ids[g->triangles[i].matId].push_back(i);
    }
    std::vector<std::vector<uint16_t>> idx(nMat);
    size_t total = 0;
    int32_t numMeshes = 0;
    for (int32_t m = 0; m < nMat; m++) {
        if (ids[m].empty()) {
            continue;
        }
        if (strip) {
            BuildStrips(g->triangles, ids[m], idx[m]);
        } else {
            idx[m].reserve(ids[m].size() * 3);
            for (int t : ids[m]) {
                idx[m].insert(idx[m].end(), g->triangles[t].v, g->triangles[t].v + 3);
            }
        }
        total += idx[m].size();
        numMeshes++;
    }

    rw::Engine::memfuncs.rwfree(g->meshHeader);
    g->meshHeader = nullptr;
    rw::MeshHeader* h = g->allocateMeshes(numMeshes, uint32_t(total), 0);
    h->flags          = strip ? rw::MeshHeader::TRISTRIP : 0;
    rw::Mesh* mesh    = h->getMeshes();
    uint16_t* dst     = reinterpret_cast<uint16_t*>(mesh + numMeshes);
    for (int32_t m = 0; m < nMat; m++) {
        if (idx[m].empty()) {
            continue;
        }
        mesh->material   = g->matList.materials[m];
        mesh->numIndices = uint32_t(idx[m].size());
        mesh->indices    = dst;
        std::memcpy(dst, idx[m].data(), idx[m].size() * sizeof(uint16_t));
        dst += idx[m].size();
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
