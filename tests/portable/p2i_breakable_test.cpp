// P-2i host test (portable arm64): the REAL BreakablePlugin.cpp / BreakablePlugin.h / FileRecord.hpp / FileRecords/BreakFile.hpp, compiled on tests/portable/p2i_stubs/StdInc.h
// (a memory RwStream that counts consumed bytes, fake RwTextureRead) and driven with
//   1. golden vectors of the exe's BreakInfo_t record (gen_x86_golden.py, independent of the code under test),
//   2. synthetic chunks (all header/count corner cases, texture array alignment sweep),
//   3. REAL DATA: every 0xFD (Breakable) chunk of every DFF geometry extension in gta3.img (path: env GTA_IMG or the Steam copy; never copied or committed):
//      BreakableStreamRead consumes exactly the chunk length, the in-memory blob is consistent (pointers inside the allocation, ordered, aligned, contents == file bytes),
//      read -> BreakableStreamWrite -> read round trip, destructor balances the texture count. The totals line must equal tools/portable/p2i_scan_img.py's.
// Run: tools/portable/run_p2i_test.sh  (ASan + UBSan: misaligned loads / heap overruns of the blob are errors).
#include <Base.h>
#include "Plugins/BreakablePlugin/BreakablePlugin.cpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <sys/mman.h>
#include "p2i_golden.inc"

#ifndef NOTSA_PORTABLE
#error "this test is for the portable build"
#endif

static int fails = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); fails++; } } while (0)
#define CHECKM(x, ...) do { if (!(x)) { std::printf("FAIL line %d: %s  ", __LINE__, #x); std::printf(__VA_ARGS__); std::printf("\n"); fails++; } } while (0)
static bool eq(const void* a, const void* b, size_t n) { return n == 0 || std::memcmp(a, b, n) == 0; }

// ---- layout ---------------------------------------------------------------------------------------------------------------------------------------------
static_assert(sizeof(BreakInfo_File) == 0x34);
static_assert(sizeof(BreakInfo_t) == 0x60, "LP64: the 9 pointers widen BreakInfo_t (this is what the record hides)");
static_assert(notsa::HasFileRecord<BreakInfo_t> && notsa::FileSizeOf<BreakInfo_t> == 0x34 && std::is_same_v<notsa::FileRecord<BreakInfo_t>::Type, BreakInfo_File>);
static_assert(sizeof(BreakablePlugin) == 8, "LP64 plugin slot");
static_assert(sizeof(kGolden_BreakInfo_File_exe) == 0x34 && sizeof(kGolden_BreakInfo_File_port) == 0x34 && sizeof(kGolden_BreakInfo_File_max) == 0x34);
static_assert(std::is_trivially_copyable_v<BreakInfo_File>);

constexpr size_t kInfoFile = 0x34, kHeader = 4;
static size_t payloadSize(size_t nv, size_t nt, size_t nm) { return 24 * nv + 8 * nt + 76 * nm; } // 12+8+4 per vertex, 6+2 per triangle, 32+32+12 per material (independent of the code under test)

// a plugin host object: 16 bytes of other plugins' data, then the plugin slot
struct Host { alignas(16) uint8_t bytes[64]; };
static BreakablePlugin* slotOf(Host& h) { return RWPLUGINOFFSET(BreakablePlugin, h.bytes, g_BreakablePlugin); }
static void construct(Host& h) { std::memset(h.bytes, 0xCD, sizeof(h.bytes)); BreakableConstructor(h.bytes, 0, 0); }

// ---- build a chunk body the way the exe's writer does -------------------------------------------------------------------------------------------------------
struct Chunk { std::vector<uint8_t> bytes; size_t nv, nt, nm; };
static Chunk makeChunk(uint32_t hdr, uint32_t rule, size_t nv, size_t nt, size_t nm, uint32_t seed, bool garbagePtrs = true) {
    Chunk c; c.nv = nv; c.nt = nt; c.nm = nm;
    auto& b = c.bytes; b.resize(kHeader + (hdr ? kInfoFile + payloadSize(nv, nt, nm) : 0));
    std::memcpy(b.data(), &hdr, 4);
    if (!hdr) return c;
    uint8_t* r = b.data() + 4;
    auto u32 = [&](size_t o, uint32_t v) { std::memcpy(r + o, &v, 4); }; auto u16 = [&](size_t o, uint16_t v) { std::memcpy(r + o, &v, 2); };
    u32(0, rule); u16(4, (uint16_t)nv); u16(0x14, (uint16_t)nt); u16(0x20, (uint16_t)nm);
    if (garbagePtrs) { for (size_t o : {0x08, 0x0C, 0x10, 0x18, 0x1C, 0x24, 0x28, 0x2C, 0x30}) u32(o, 0x0B1E0000u + (uint32_t)o * 7 + seed); u16(6, 0xBEEF); u16(0x16, 0xFEED); u16(0x22, 0xF00D); }
    uint32_t x = seed * 2654435761u + 1;
    for (size_t i = kHeader + kInfoFile; i < b.size(); i++) { x = x * 1664525u + 1013904223u; b[i] = uint8_t(x >> 24); }
    // names / masks: NUL-terminated, mask 0 empty (=> null mask) for the even materials
    uint8_t* names = b.data() + 4 + kInfoFile + 24 * nv + 8 * nt; uint8_t* masks = names + 32 * nm;
    for (size_t m = 0; m < nm; m++) {
        std::snprintf((char*)names + 32 * m, 32, "tex_%u_%zu", seed, m);
        if (m % 2 == 0) std::memset(masks + 32 * m, 0, 32); else std::snprintf((char*)masks + 32 * m, 32, "tex_%u_%zum", seed, m);
    }
    return c;
}

struct Stats { size_t dffs = 0, chunks = 0, withData = 0, verts = 0, tris = 0, mats = 0, payloadBytes = 0, chunkBytes = 0, lenMismatch = 0; std::set<uint32_t> hdrs;
               size_t unterminatedNames = 0, badTriIndex = 0, badMatIndex = 0, nonOrigin = 0, emptyMask = 0; };

// read one chunk through the real BreakableStreamRead, check everything; returns the info pointer (null if header 0). `expectBytesOfRecord` = original record bytes for the content compare.
static BreakInfo_t* readChecked(Host& h, const std::vector<uint8_t>& chunk, RwStream& rs, const char* what, bool& ok) {
    ok = true;
    construct(h);
    rs = RwStream{}; rs.data = chunk; rs.pos = 0;
    const long liveBefore = g_liveTextures; g_textureLog.clear();
    RwStream* ret = BreakableStreamRead(&rs, (int)chunk.size(), h.bytes, 0, 0);
    CHECKM(ret == &rs, "%s: returned another stream", what);
    CHECKM(rs.pos == chunk.size(), "%s: consumed %zu of %zu bytes", what, rs.pos, chunk.size()); ok &= rs.pos == chunk.size();
    uint32_t hdr; std::memcpy(&hdr, chunk.data(), 4);
    auto* info = slotOf(h)->m_pBreakableInfo;
    if (!hdr) { CHECKM(info == nullptr, "%s: header 0 but info set", what); CHECK(g_liveTextures == liveBefore); ok &= info == nullptr; return nullptr; }
    CHECKM(info != nullptr, "%s: header != 0 but no info", what); if (!info) { ok = false; return nullptr; }

    uint16_t nv, nt, nm; uint32_t rule; std::memcpy(&rule, chunk.data() + 4, 4); std::memcpy(&nv, chunk.data() + 4 + 4, 2); std::memcpy(&nt, chunk.data() + 4 + 0x14, 2); std::memcpy(&nm, chunk.data() + 4 + 0x20, 2);
    CHECK(info->m_uiPosRule == (eBreakablePluginPositionRule)rule && info->m_usNumVertices == nv && info->m_usNumTriangles == nt && info->m_usNumMaterials == nm);
    const auto* base = (const uint8_t*)info; const size_t alloc = BreakablePluginData::GetRequiredAllocationSize(*info);
    auto off = [&](const void* p) { return (size_t)((const uint8_t*)p - base); };
    // layout of the blob: contiguous in the documented order, texture array aligned and ending exactly at the allocation end
    size_t o = sizeof(BreakInfo_t);
    CHECK(off(info->m_pVertexPos) == o);                  o += 12 * nv;
    CHECK(off(info->m_pTexCoors) == o);                   o += 8 * nv;
    CHECK(off(info->m_pVertexColors) == o);               o += 4 * nv;
    CHECK(off(info->m_pTriangles) == o);                  o += 6 * nt;
    CHECK(off(info->m_pTrianglesMaterialIndices) == o);   o += 2 * nt;
    CHECK(off(info->m_pTextureNames) == o);               o += 32 * nm;
    CHECK(off(info->m_pMaskNames) == o);                  o += 32 * nm;
    CHECK(off(info->m_pMaterialProperties) == o);         o += 12 * nm;
    const size_t texOff = off(info->m_pTextures);
    CHECKM(texOff >= o && texOff - o < alignof(RwTexture*), "%s: texture array at %zu, props end %zu", what, texOff, o);
    CHECKM(texOff % alignof(RwTexture*) == 0, "%s: texture array misaligned (%zu)", what, texOff);
    CHECKM(texOff + sizeof(RwTexture*) * nm == alloc, "%s: texture array end %zu != allocation %zu", what, texOff + sizeof(RwTexture*) * nm, alloc);
    CHECK((uintptr_t)info->m_pVertexPos % alignof(RwV3d) == 0 && (uintptr_t)info->m_pTexCoors % alignof(RwTexCoords) == 0 && (uintptr_t)info->m_pTriangles % alignof(uint16) == 0);
    // contents == the file bytes
    const uint8_t* p = chunk.data() + 4 + kInfoFile;
    CHECK(eq(info->m_pVertexPos, p, 12 * nv));              p += 12 * nv;
    CHECK(eq(info->m_pTexCoors, p, 8 * nv));                p += 8 * nv;
    CHECK(eq(info->m_pVertexColors, p, 4 * nv));            p += 4 * nv;
    CHECK(eq(info->m_pTriangles, p, 6 * nt));               p += 6 * nt;
    CHECK(eq(info->m_pTrianglesMaterialIndices, p, 2 * nt)); p += 2 * nt;
    CHECK(eq(info->m_pTextureNames, p, 32 * nm));           p += 32 * nm;
    CHECK(eq(info->m_pMaskNames, p, 32 * nm));              p += 32 * nm;
    CHECK(eq(info->m_pMaterialProperties, p, 12 * nm));     p += 12 * nm;
    CHECK(p == chunk.data() + chunk.size());
    // textures: one RwTextureRead per material with the name / (mask or null), pointer stored in the array
    CHECKM(g_textureLog.size() == nm && g_liveTextures == liveBefore + nm, "%s: %zu texture reads for %u materials", what, g_textureLog.size(), nm);
    for (size_t m = 0; m < nm && m < g_textureLog.size(); m++) {
        const char* name = info->m_pTextureNames + 32 * m; const char* mask = info->m_pMaskNames + 32 * m;
        CHECK(g_textureLog[m].first == std::string(name, strnlen(name, 32)).substr(0, 31 + 1) || strnlen(name, 32) == 32);
        CHECK(g_textureLog[m].second == (mask[0] ? std::string(mask, strnlen(mask, 32)) : std::string("<null>")) || strnlen(mask, 32) == 32);
        CHECK(info->m_pTextures[m] != nullptr);
    }
    return info;
}

// info -> write -> bytes ; canonical form of the original chunk: pointer slots + padding zeroed, header 1
static std::vector<uint8_t> canonical(const std::vector<uint8_t>& chunk) {
    auto c = chunk; uint32_t hdr; std::memcpy(&hdr, c.data(), 4); if (!hdr) return c;
    uint32_t one = 1; std::memcpy(c.data(), &one, 4);
    uint8_t* r = c.data() + 4;
    for (size_t o : {0x06, 0x16, 0x22}) std::memset(r + o, 0, 2);
    for (size_t o : {0x08, 0x0C, 0x10, 0x18, 0x1C, 0x24, 0x28, 0x2C, 0x30}) std::memset(r + o, 0, 4);
    return c;
}

static void destroy(Host& h) { BreakableDestructor(h.bytes, 0, 0); }

// full cycle on one chunk: read, write, compare with canonical, read again (structurally equal), destroy
static bool cycle(const std::vector<uint8_t>& chunk, const char* what) {
    const int f0 = fails; const long live0 = g_liveTextures;
    Host h1, h2; RwStream rs1, rs2, out;
    bool ok; auto* i1 = readChecked(h1, chunk, rs1, what, ok);
    out = RwStream{}; out.writing = true;
    BreakableStreamWrite(&out, (int)chunk.size(), h1.bytes, 0, 0);
    const auto want = canonical(chunk);
    CHECKM(out.data.size() == want.size() && eq(out.data.data(), want.data(), std::min(out.data.size(), want.size())), "%s: write != canonical original (%zu vs %zu bytes)", what, out.data.size(), want.size());
    if (i1) {
        bool ok2; auto* i2 = readChecked(h2, out.data, rs2, what, ok2);
        CHECK(i2 && i2->m_usNumVertices == i1->m_usNumVertices && i2->m_usNumMaterials == i1->m_usNumMaterials && i2->m_uiPosRule == i1->m_uiPosRule);
        // second write is byte-identical to the first (stable)
        RwStream out2; BreakableStreamWrite(&out2, (int)out.data.size(), h2.bytes, 0, 0); CHECK(out2.data == out.data);
        destroy(h2);
    }
    destroy(h1);
    CHECKM(g_liveTextures == live0, "%s: texture leak %ld", what, g_liveTextures - live0);
    return fails == f0;
}

// ---- real data --------------------------------------------------------------------------------------------------------------------------------------------
static bool loadFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary); if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()); return true;
}
static uint32_t rd32(const std::vector<uint8_t>& b, size_t o) { uint32_t v; std::memcpy(&v, b.data() + o, 4); return v; }
constexpr uint32_t kBreakChunk = MAKECHUNKID(rwVENDORID_DEVELOPER, 0xFD);

static void walk(const std::vector<uint8_t>& b, size_t pos, size_t end, std::vector<uint32_t>& path, std::vector<std::pair<size_t, size_t>>& found) {
    while (pos + 12 <= end) {
        const uint32_t t = rd32(b, pos), s = rd32(b, pos + 4); const size_t body = pos + 12;
        const bool inGeomExt = path.size() == 4 && path[0] == 0x10 && path[1] == 0x1A && path[2] == 0x0F && path[3] == 0x03;
        if (t == kBreakChunk && inGeomExt) found.emplace_back(body, s);
        else if ((t == 0x10 || t == 0x1A || t == 0x0F || t == 0x03) && (path.empty() || path.back() != t)) { path.push_back(t); walk(b, body, std::min(body + s, end), path, found); path.pop_back(); }
        pos = body + s;
    }
}

static void realData(Stats& st, const std::string& imgPath) {
    std::vector<uint8_t> img;
    CHECK(loadFile(imgPath, img)); if (img.size() < 8) return;
    CHECK(std::memcmp(img.data(), "VER2", 4) == 0);
    const uint32_t n = rd32(img, 4);
    Host h;
    for (uint32_t i = 0; i < n; i++) {
        const size_t e = 8 + (size_t)i * 32; uint16_t sa, sb; std::memcpy(&sa, &img[e + 4], 2); std::memcpy(&sb, &img[e + 6], 2);
        std::string name((const char*)&img[e + 8], strnlen((const char*)&img[e + 8], 24)); for (auto& ch : name) ch = (char)std::tolower((unsigned char)ch);
        if (name.size() < 4 || name.substr(name.size() - 4) != ".dff") continue;
        const size_t off = (size_t)rd32(img, e) * 2048, sz = (size_t)std::max(sa, sb) * 2048;
        if (off + sz > img.size() || sz < 12) continue;
        std::vector<uint8_t> dff(img.begin() + off, img.begin() + off + sz);
        if (rd32(dff, 0) != 0x10) continue;
        st.dffs++;
        std::vector<std::pair<size_t, size_t>> found; std::vector<uint32_t> path{0x10};
        walk(dff, 12, std::min<size_t>(12 + rd32(dff, 4), dff.size()), path, found);
        for (auto [body, size] : found) {
            st.chunks++; st.chunkBytes += size;
            std::vector<uint8_t> chunk(dff.begin() + body, dff.begin() + body + size);
            const uint32_t hdr = rd32(chunk, 0);
            size_t nv = 0, nt = 0, nm = 0;
            if (hdr) {
                st.hdrs.insert(hdr); st.withData++;
                uint16_t v, t, m; std::memcpy(&v, &chunk[8], 2); std::memcpy(&t, &chunk[4 + 0x14], 2); std::memcpy(&m, &chunk[4 + 0x20], 2); nv = v; nt = t; nm = m;
                st.verts += nv; st.tris += nt; st.mats += nm; st.payloadBytes += payloadSize(nv, nt, nm);
                st.lenMismatch += size != kHeader + kInfoFile + payloadSize(nv, nt, nm);
            } else st.lenMismatch += size != 4;
            if (!cycle(chunk, name.c_str())) { std::printf("  (chunk of %s at 0x%zx, %zu bytes)\n", name.c_str(), body, size); if (fails > 40) return; }
            if (hdr) { // data sanity (informational): indices inside the arrays, names terminated
                const uint8_t* tri = &chunk[4 + kInfoFile + 24 * nv]; const uint8_t* mi = tri + 6 * nt; const uint8_t* names = mi + 2 * nt; const uint8_t* masks = names + 32 * nm;
                for (size_t k = 0; k < nt; k++) { for (int c = 0; c < 3; c++) { uint16_t vi; std::memcpy(&vi, tri + 6 * k + 2 * c, 2); st.badTriIndex += vi >= nv; } uint16_t m; std::memcpy(&m, mi + 2 * k, 2); st.badMatIndex += m >= nm; }
                for (size_t k = 0; k < nm; k++) { st.unterminatedNames += strnlen((const char*)names + 32 * k, 32) == 32; st.emptyMask += masks[32 * k] == 0; }
                if (rd32(chunk, 4) != 0) st.nonOrigin++;
            }
        }
    }
}

int main() {
    g_BreakablePlugin = 16; // pretend the plugin registered at offset 16 of the geometry
    // ---- 1. golden record ---------------------------------------------------------------------------------------------------------------------------------
    {
        BreakInfo_File f; std::memcpy(&f, kGolden_BreakInfo_File_exe, sizeof(f));
        BreakInfo_t i; std::memset(&i, 0xAB, sizeof(i)); // dirty on purpose: FromFile must clear every pointer
        notsa::FileRecord<BreakInfo_t>::FromFile(i, f);
        CHECK(i.m_uiPosRule == COLLISION_ORIGIN && i.m_usNumVertices == 7 && i.m_usNumTriangles == 5 && i.m_usNumMaterials == 3);
        CHECK(!i.m_pVertexPos && !i.m_pTexCoors && !i.m_pVertexColors && !i.m_pTriangles && !i.m_pTrianglesMaterialIndices && !i.m_pTextures && !i.m_pTextureNames && !i.m_pMaskNames && !i.m_pMaterialProperties);
        // ToFile of an object that holds live pointers writes exactly the 'port' golden (pointer slots + pads 0)
        int dummy; i.m_pVertexPos = (RwV3d*)&dummy; i.m_pTexCoors = (RwTexCoords*)&dummy; i.m_pVertexColors = (RwRGBA*)&dummy; i.m_pTriangles = (BreakInfoTriangle*)&dummy; i.m_pTrianglesMaterialIndices = (uint16*)&dummy;
        i.m_pTextures = (RwTexture**)&dummy; i.m_pTextureNames = (char*)&dummy; i.m_pMaskNames = (char*)&dummy; i.m_pMaterialProperties = (BreakInfoColor*)&dummy;
        const auto out = notsa::FileRecord<BreakInfo_t>::ToFile(i);
        CHECK(eq(&out, kGolden_BreakInfo_File_port, 0x34));
        // limits
        BreakInfo_File fm; std::memcpy(&fm, kGolden_BreakInfo_File_max, sizeof(fm)); BreakInfo_t im; notsa::FileRecord<BreakInfo_t>::FromFile(im, fm);
        CHECK(im.m_uiPosRule == OBJECT_ORIGIN && im.m_usNumVertices == 65535 && im.m_usNumTriangles == 65535 && im.m_usNumMaterials == 65535);
        const auto outm = notsa::FileRecord<BreakInfo_t>::ToFile(im); CHECK(eq(&outm, kGolden_BreakInfo_File_max, 0x34));
        CHECK(BreakablePluginData::GetRequiredAllocationSize(im) % 8 == 0 && BreakablePluginData::GetRequiredAllocationSize(im) > 65535ull * (24 + 8 + 76 + 8));
        CHECK(BreakableStreamGetSize(nullptr, 0, 0) == 0x34);
    }
    // ---- 2. texture-array alignment sweep (every count residue) ---------------------------------------------------------------------------------------------
    {
        size_t padded = 0;
        for (size_t nv = 0; nv <= 6; nv++) for (size_t nt = 0; nt <= 6; nt++) for (size_t nm = 0; nm <= 9; nm++) {
            BreakInfo_t info{}; info.m_usNumVertices = (uint16)nv; info.m_usNumTriangles = (uint16)nt; info.m_usNumMaterials = (uint16)nm;
            const size_t sz = BreakablePluginData::GetRequiredAllocationSize(info);
            auto* d = static_cast<BreakablePluginData*>(::operator new(sz)); d->m_Info = info;
            const size_t propsEnd = sizeof(BreakInfo_t) + 24 * nv + 8 * nt + 76 * nm - 0; // = props end (props are the last of the 12-byte-per-material arrays)
            const size_t t = d->GetTexturesOffset();
            CHECKM(t % alignof(RwTexture*) == 0 && t >= propsEnd && t - propsEnd < 8 && t + 8 * nm == sz, "nv=%zu nt=%zu nm=%zu: texOff=%zu propsEnd=%zu size=%zu", nv, nt, nm, t, propsEnd, sz);
            CHECK(((uintptr_t)d->GetTexturesPtr()) % alignof(RwTexture*) == 0);
            padded += t != propsEnd; ::operator delete(d);
        }
        CHECKM(padded > 100, "alignment pad never exercised (%zu)", padded);
    }
    // ---- 3. synthetic chunks ---------------------------------------------------------------------------------------------------------------------------------
    {
        CHECK(cycle(makeChunk(0, 0, 0, 0, 0, 1).bytes, "hdr0"));
        CHECK(cycle(makeChunk(0x0B1E0034, 1, 0, 0, 0, 2).bytes, "empty"));             // header != 0, no vertices / triangles / materials
        CHECK(cycle(makeChunk(0x80000000, 0, 3, 1, 1, 3).bytes, "hdr-high-bit"));      // low 32 bits of a pointer: top bit only
        CHECK(cycle(makeChunk(0x00000001, 1, 7, 5, 3, 4).bytes, "hdr-one"));
        CHECK(cycle(makeChunk(0xFFFFFFFF, 0, 1, 1, 8, 5).bytes, "hdr-ff"));
        CHECK(cycle(makeChunk(7, 1, 100, 99, 5, 6, false).bytes, "zero-ptr-slots"));  // what the port itself writes (pointer slots / pads 0)
        for (size_t nt = 0; nt < 5; nt++) for (size_t nm = 0; nm < 5; nm++) CHECK(cycle(makeChunk(0x0B000000u + (uint32_t)(nt * 5 + nm), nm & 1, nt + 1, nt, nm, 10 + (uint32_t)(nt * 5 + nm)).bytes, "sweep"));
        // exe garbage in the pointer slots / padding is ignored and normalised on write (covered by cycle's canonical compare); the plugin slot's upper half never leaks into the header
        Host h; construct(h); RwStream rs; rs.data = makeChunk(0, 0, 0, 0, 0, 0).bytes; BreakableStreamRead(&rs, 4, h.bytes, 0, 0); CHECK(slotOf(h)->m_pBreakableInfo == nullptr);
        // writer: a live info pointer whose low 32 bits are 0 (e.g. 0x1'0000'0000) still writes a non-zero header (the exe wrote the pointer's 4 bytes; a 64-bit low half can be 0)
        const size_t span = 3ull << 32; // reserve 12 GB of address space, take the first 4 GB-aligned page inside (low 32 bits == 0), make just that page accessible
        void* res = mmap(nullptr, span, PROT_NONE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
        void* hi = nullptr;
        if (res != MAP_FAILED) {
            hi = reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(res) + 0xFFFFFFFFull) & ~0xFFFFFFFFull);
            if (mprotect(hi, 4096, PROT_READ | PROT_WRITE) != 0) hi = nullptr;
        }
        if (hi) {
            CHECK((reinterpret_cast<uintptr_t>(hi) & 0xFFFFFFFFull) == 0 && reinterpret_cast<uintptr_t>(hi) != 0);
            Host hw; construct(hw); slotOf(hw)->m_pBreakableInfo = static_cast<BreakInfo_t*>(hi); RwStream o; BreakableStreamWrite(&o, 0, hw.bytes, 0, 0);
            uint32_t wh; std::memcpy(&wh, o.data.data(), 4); CHECKM(wh == 1 && o.data.size() == 4 + 0x34, "hdr=%08x size=%zu", wh, o.data.size());
        } else { std::printf("FAIL: could not set up the low-half-zero pointer case\n"); fails++; }
        if (res != MAP_FAILED) munmap(res, span);
    }
    // ---- 4. real data -----------------------------------------------------------------------------------------------------------------------------------------
    Stats st; const char* env = std::getenv("GTA_IMG");
    if (env && *env) {
        realData(st, env);
        std::printf("STATS dffs=%zu chunks=%zu withData=%zu verts=%zu tris=%zu mats=%zu payloadBytes=%zu chunkBytes=%zu lenMismatch=%zu hdrValues=%zu\n",
                    st.dffs, st.chunks, st.withData, st.verts, st.tris, st.mats, st.payloadBytes, st.chunkBytes, st.lenMismatch, st.hdrs.size());
        std::printf("INFO real-data sanity: nonOrigin=%zu emptyMasks=%zu unterminatedNames=%zu badTriVertexIndex=%zu badTriMaterialIndex=%zu\n", st.nonOrigin, st.emptyMask, st.unterminatedNames, st.badTriIndex, st.badMatIndex);
        CHECK(st.chunks > 0 && st.withData > 0 && st.lenMismatch == 0);
        CHECK(g_liveTextures == 0);
        std::printf("INFO textures created over the run: %ld\n", g_textureReads);
    } else std::printf("SKIPPED real data (GTA_IMG not set)\n");
    if (fails) { std::printf("FAILED: %d checks\n", fails); return 1; }
    std::printf("ALL OK\n");
    return 0;
}
