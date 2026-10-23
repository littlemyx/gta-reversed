// P-2h host test (portable arm64): CPathNode_File (0x1C), FileRecord<CPathNode>, notsa::LoadPathNodesFromFile and the portable block of CPathFind::LoadPathFindData.
//   tools/portable/run_p2h_test.sh   (goldens: tools/portable/gen_x86_golden.py, independent of the code under test -> $OUT/p2h_golden.inc;
//   $OUT/p2h_cut.h = `class CPathNode` .. `// P-2h END` cut verbatim from PathFind.h; $OUT/p2h_load_block.inc = the NOTSA_PORTABLE block of LoadPathFindData cut verbatim from PathFind.cpp)
// REAL DATA (env P2H_NODES_DIR = the game's data/paths, never copied/committed): all 64 NODES*.DAT -> STATS line that must equal tools/portable/p2h_scan_nodes.py (independent Python decode).
#include <Base.h>
#include "Vector.h"
#include "CompressedVector.h"
#include "NodeAddress.h"
#include "FileRecord.hpp"
#include "FileRecords/PathNodeFile.hpp"
#include "p2h_cut.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include "p2h_golden.inc"
#define P2H_STR2(x) #x
#define P2H_STR(x) P2H_STR2(x)

#ifndef NOTSA_PORTABLE
#error "this test is for the portable build"
#endif

static int fails = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); fails++; } } while (0)
static bool eq(const void* a, const void* b, size_t n) { return std::memcmp(a, b, n) == 0; }

// ---- layout -----------------------------------------------------------------------------------------------------------------------------------------------
static_assert(notsa::HasFileRecord<CPathNode> && notsa::FileSizeOf<CPathNode> == 0x1C);
static_assert(sizeof(CPathNode) == 40, "LP64: the two hash-chain pointers widen CPathNode (this is what the record hides)");
static_assert(std::is_same_v<notsa::FileRecord<CPathNode>::Type, CPathNode_File> && std::is_trivially_copyable_v<CPathNode_File>);
static_assert(sizeof(kGolden_CPathNode_File_basic) == 0x1C && sizeof(kGolden_CPathNode_File_extreme) == 0x1C && sizeof(kGolden_CPathNode_File_chain) == 0x1C);
// the tail of the in-memory node is NOT at the file offsets on LP64 (that is the bug the record fixes)
static_assert(offsetof(CPathNode, m_vPos) == 16 && offsetof(CPathNode_File, pos) == 8);

static CPathNode_File rec(const uint8_t* g) { CPathNode_File r; std::memcpy(&r, g, sizeof(r)); return r; }

// one accessor per bit-field member, in file bit order (independent of the trait: the table is written by hand from the exe's declaration order)
struct BitMember { const char* name; unsigned shift, width; uint32 (*get)(const CPathNode&); };
#define BM(nm, sh, w, expr) BitMember{ nm, sh, w, [](const CPathNode& n) -> uint32 { return (uint32)(expr); } }
static const BitMember kBits[] = {
    BM("numLinks", 0, 4, n.m_nNumLinks), BM("onDeadEnd", 4, 1, n.m_onDeadEnd), BM("isSwitchedOff", 5, 1, n.m_isSwitchedOff), BM("roadBlocks", 6, 1, n.m_bRoadBlocks),
    BM("waterNode", 7, 1, n.m_bWaterNode), BM("switchedOffOriginal", 8, 1, n.m_isSwitchedOffOriginal), BM("unk1", 9, 1, n.unk1), BM("dontWander", 10, 1, n.m_bDontWander),
    BM("unk2", 11, 1, n.unk2), BM("notHighway", 12, 1, n.m_bNotHighway), BM("highway", 13, 1, n.m_bHighway), BM("unk3", 14, 1, n.unk3), BM("unk4", 15, 1, n.unk4),
    BM("spawnProbability", 16, 4, n.m_nSpawnProbability), BM("behaviourType", 20, 4, n.m_nBehaviourType),
};
static std::string describe(const CPathNode& n) { std::string s; for (auto& b : kBits) if (b.get(n)) s += std::string(b.name) + "=" + std::to_string(b.get(n)) + " "; return s; }

static void fillGarbage(CPathNode& n) { std::memset((void*)&n, 0xA5, sizeof(n)); }

// ---- in-memory view of a decoded node, stats ---------------------------------------------------------------------------------------------------------------
struct Stats {
    long long files = 0, nodes = 0, veh = 0, ped = 0, carLinks = 0, addrs = 0, bytes = 0, countBad = 0, sizeBad = 0, nextNonZero = 0, prevNonZero = 0, byte3NonZero = 0, areaBad = 0, nodeBad = 0,
              sumX = 0, sumY = 0, sumZ = 0, sumDist = 0, sumBase = 0, sumWidth = 0, sumFlood = 0, flood0 = 0, maxNumLinks = 0, sumNumLinks = 0, sumSpawn = 0, sumBehaviour = 0,
              linkRangeBad = 0, linkAreaBad = 0, linkNodeBad = 0, linkInvalid = 0, emptyAreas = 0;
    long long bits[16] = {};
};
static std::string statsLine(const Stats& s) {
    std::ostringstream o;
    o << "STATS files=" << s.files << " nodes=" << s.nodes << " veh=" << s.veh << " ped=" << s.ped << " carLinks=" << s.carLinks << " addrs=" << s.addrs << " bytes=" << s.bytes
      << " countBad=" << s.countBad << " sizeBad=" << s.sizeBad << " nextNonZero=" << s.nextNonZero << " prevNonZero=" << s.prevNonZero << " byte3NonZero=" << s.byte3NonZero
      << " areaBad=" << s.areaBad << " nodeBad=" << s.nodeBad << " sumX=" << s.sumX << " sumY=" << s.sumY << " sumZ=" << s.sumZ << " sumDist=" << s.sumDist << " sumBase=" << s.sumBase
      << " sumWidth=" << s.sumWidth << " sumFlood=" << s.sumFlood << " flood0=" << s.flood0 << " maxNumLinks=" << s.maxNumLinks << " sumNumLinks=" << s.sumNumLinks
      << " sumSpawn=" << s.sumSpawn << " sumBehaviour=" << s.sumBehaviour << " linkRangeBad=" << s.linkRangeBad << " linkAreaBad=" << s.linkAreaBad << " linkNodeBad=" << s.linkNodeBad
      << " linkInvalid=" << s.linkInvalid << " emptyAreas=" << s.emptyAreas << " bits4_15=";
    for (int i = 4; i < 16; i++) o << (i > 4 ? "," : "") << s.bits[i];
    return o.str();
}

// ---- the portable block of LoadPathFindData, cut verbatim from PathFind.cpp, around a memory RwStream -------------------------------------------------------------------------
struct RwStream { const uint8* p; size_t n; size_t pos; size_t reads; size_t overrun; };
static void RwStreamRead(RwStream* s, void* dst, uint32 bytes) {
    s->reads++;
    if (s->pos + bytes > s->n) { s->overrun++; std::memset(dst, 0, bytes); return; }
    std::memcpy(dst, s->p + s->pos, bytes); s->pos += bytes;
}
struct FakePathFind {
    CPathNode* m_pPathNodes[1];
    void LoadNodes(RwStream* stream, uint32 numNodes, int areaId = 0) {
        m_pPathNodes[areaId] = new CPathNode[numNodes ? numNodes : 1];
        if (numNodes) {
#include "p2h_load_block.inc"
        }
    }
};

static std::vector<uint8> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary); if (!f) return {};
    return std::vector<uint8>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
template<typename T> static T rd(const std::vector<uint8>& b, size_t off) { T v; std::memcpy(&v, b.data() + off, sizeof(T)); return v; }

int main() {
    // ---- bit order: each file bit alone -> exactly one member, and back ----------------------------------------------------------------------------------------------------
    for (unsigned bit = 0; bit < 24; ++bit) {
        CPathNode_File f{}; f.flags = 1u << bit;
        CPathNode n; fillGarbage(n);
        notsa::FileRecord<CPathNode>::FromFile(n, f);
        unsigned seen = 0;
        for (auto& b : kBits) { const uint32 v = b.get(n); if (v) { seen++; CHECK(v == (1u << (bit - b.shift)) && bit >= b.shift && bit < b.shift + b.width); } }
        CHECK(seen == 1);
        CPathNode_File back = notsa::FileRecord<CPathNode>::ToFile(n);
        CHECK(back.flags == f.flags);
    }
    // all member bits at once and the padding byte
    { CPathNode_File f{}; f.flags = 0xFFFFFFFFu; CPathNode n; notsa::FileRecord<CPathNode>::FromFile(n, f); CHECK(notsa::FileRecord<CPathNode>::ToFile(n).flags == kCPathNodeFlagsUsedMask); }
    // ---- golden vectors: exe bytes -> node -> file bytes ---------------------------------------------------------------------------------------------------------------------
    {
        CPathNode n; fillGarbage(n);
        notsa::FileRecord<CPathNode>::FromFile(n, rec(kGolden_CPathNode_File_basic));
        CHECK(n.m_next == nullptr && n.m_prev == nullptr);
        CHECK(n.m_vPos.x.Raw() == 100 && n.m_vPos.y.Raw() == -200 && n.m_vPos.z.Raw() == 300 && n.m_vPos.x == 12.5f && n.m_vPos.y == -25.f && n.m_vPos.z == 37.5f);
        CHECK(n.m_totalDistFromOrigin == 32766 && n.m_wBaseLinkId == 57 && n.m_wAreaId == 12 && n.m_wNodeId == 34 && n.m_nPathWidth == 48 && n.m_nFloodFill == 3);
        CHECK(n.m_nNumLinks == 3 && n.m_isSwitchedOff && n.m_bDontWander && n.m_bHighway && n.m_nSpawnProbability == 5 && n.m_nBehaviourType == 2);
        CHECK(!n.m_onDeadEnd && !n.m_bRoadBlocks && !n.m_bWaterNode && !n.m_isSwitchedOffOriginal && !n.unk1 && !n.unk2 && !n.m_bNotHighway && !n.unk3 && !n.unk4);
        const auto g = notsa::FileRecord<CPathNode>::ToFile(n); CHECK(eq(&g, kGolden_CPathNode_File_basic, 0x1C));
    }
    {
        CPathNode n; fillGarbage(n);
        notsa::FileRecord<CPathNode>::FromFile(n, rec(kGolden_CPathNode_File_extreme));
        CHECK(n.m_vPos.x.Raw() == -32768 && n.m_vPos.y.Raw() == 32767 && n.m_vPos.z.Raw() == 0 && n.m_totalDistFromOrigin == -1 && n.m_wBaseLinkId == -2);
        CHECK(n.m_wAreaId == 65535 && n.m_wNodeId == 65535 && n.m_nPathWidth == 255 && n.m_nFloodFill == 255 && n.m_nNumLinks == 15 && n.m_nSpawnProbability == 15 && n.m_nBehaviourType == 15);
        const auto g = notsa::FileRecord<CPathNode>::ToFile(n); CHECK(eq(&g, kGolden_CPathNode_File_extreme, 0x1C));
    }
    { // 'chain': the exe stored stale pointers in the link slots; they must not survive, and ToFile writes zeros there
        CPathNode n; fillGarbage(n);
        notsa::FileRecord<CPathNode>::FromFile(n, rec(kGolden_CPathNode_File_chain));
        CHECK(n.m_next == nullptr && n.m_prev == nullptr && n.m_nNumLinks == 0 && n.m_onDeadEnd && n.m_bWaterNode && n.m_isSwitchedOffOriginal && n.unk2 && n.m_bNotHighway && n.unk3 && n.unk4
              && n.m_nSpawnProbability == 15 && n.m_nBehaviourType == 0 && !n.m_isSwitchedOff && !n.m_bRoadBlocks && !n.unk1 && !n.m_bDontWander && !n.m_bHighway);
        uint8_t want[0x1C]; std::memcpy(want, kGolden_CPathNode_File_chain, 0x1C); std::memset(want, 0, 8);
        const auto g = notsa::FileRecord<CPathNode>::ToFile(n); CHECK(eq(&g, want, 0x1C));
    }
    // ---- random round trip: file -> node -> file is the identity on everything but the link slots / padding byte -------------------------------------------------------------------
    {
        std::mt19937 rng(0x2468);
        for (int i = 0; i < 20000; ++i) {
            uint8_t b[0x1C]; for (auto& x : b) x = (uint8_t)rng();
            if (i & 1) std::memset(b, 0, 8);
            CPathNode_File f = rec(b); f.flags &= kCPathNodeFlagsUsedMask;
            CPathNode n; fillGarbage(n);
            notsa::FileRecord<CPathNode>::FromFile(n, f);
            CPathNode_File g = notsa::FileRecord<CPathNode>::ToFile(n);
            CPathNode_File want = f; want.links[0] = want.links[1] = 0;
            CHECK(eq(&g, &want, sizeof(g)));
            CHECK(n.m_next == nullptr && n.m_prev == nullptr);
            // decoded members == the by-hand shift/mask of the word
            for (auto& m : kBits) CHECK(m.get(n) == ((f.flags >> m.shift) & ((1u << m.width) - 1)));
        }
    }
    // ---- loader: batch boundaries (256), stride 0x1C, exact byte consumption, ASan on the destination array ----------------------------------------------------------------------------
    {
        std::mt19937 rng(77);
        for (uint32 count : {0u, 1u, 2u, 255u, 256u, 257u, 511u, 512u, 513u, 1000u}) {
            std::vector<CPathNode_File> recs(count + 1); // +1 sentinel record that must stay unread
            for (auto& r : recs) { std::memset(&r, 0, sizeof(r)); for (auto* p = (uint8_t*)&r; p < (uint8_t*)(&r + 1); ++p) *p = (uint8_t)rng(); r.flags &= kCPathNodeFlagsUsedMask; }
            std::vector<uint8> bytes(sizeof(CPathNode_File) * (count + 1)); std::memcpy(bytes.data(), recs.data(), bytes.size());
            RwStream s{ bytes.data(), bytes.size(), 0, 0, 0 };
            FakePathFind pf{};
            pf.LoadNodes(&s, count);
            CHECK(s.pos == sizeof(CPathNode_File) * count && s.overrun == 0);
            CHECK(s.reads == (count + 255) / 256);
            for (uint32 i = 0; i < count; ++i) {
                CPathNode_File g = notsa::FileRecord<CPathNode>::ToFile(pf.m_pPathNodes[0][i]);
                CPathNode_File want = recs[i]; want.links[0] = want.links[1] = 0;
                if (!eq(&g, &want, sizeof(g))) { CHECK(false); break; }
                CHECK(pf.m_pPathNodes[0][i].m_next == nullptr && pf.m_pPathNodes[0][i].m_prev == nullptr);
            }
            delete[] pf.m_pPathNodes[0];
        }
    }
    // ---- source scan: PathFind.cpp keeps the raw read (MSVC) in #else and uses the loader under NOTSA_PORTABLE ----------------------------------------------------------------------------
    {
        std::ifstream f(std::string(P2H_STR(REPO_DIR)) + "/source/game_sa/PathFind.cpp"); std::stringstream ss; ss << f.rdbuf(); const std::string src = ss.str();
        const auto p0 = src.find("#ifdef NOTSA_PORTABLE\n        // P-2h"), p1 = src.find("#else\n", p0), p2 = src.find("#endif", p1);
        CHECK(p0 != std::string::npos && p1 != std::string::npos && p2 != std::string::npos);
        CHECK(src.find("LoadPathNodesFromFile(m_pPathNodes[areaId], numNodes", p0) < p1);
        const auto raw = src.find("RwStreamRead(stream, m_pPathNodes[areaId], sizeof(CPathNode) * numNodes);");
        CHECK(raw != std::string::npos && raw > p1 && raw < p2);
        // no other reader of the nodes file bytes
        size_t cnt = 0, pos = 0; while ((pos = src.find("sizeof(CPathNode)", pos)) != std::string::npos) { cnt++; pos++; }
        CHECK(cnt == 2); // the raw read above + the interior-node Malloc (in-memory array, not file bytes)
    }
    // ---- REAL DATA: all 64 NODES*.DAT ---------------------------------------------------------------------------------------------------------------------------------------------
    if (const char* dir = std::getenv("P2H_NODES_DIR")) {
        Stats st;
        std::vector<std::vector<uint8>> files(64); std::vector<uint32> counts(64);
        for (int k = 0; k < 64; ++k) { files[k] = readFile(std::string(dir) + "/NODES" + std::to_string(k) + ".DAT"); CHECK(files[k].size() >= 20); counts[k] = files[k].size() >= 20 ? rd<uint32>(files[k], 0) : 0; }
        constexpr uint32 EXTRA = 16 * 12; // NUM_DYNAMIC_LINKS_PER_AREA * 12
        for (int k = 0; k < 64 && files[k].size() >= 20; ++k) {
            const auto& b = files[k];
            const uint32 n = rd<uint32>(b, 0), veh = rd<uint32>(b, 4), ped = rd<uint32>(b, 8), links = rd<uint32>(b, 12), addrs = rd<uint32>(b, 16);
            st.files++; st.nodes += n; st.veh += veh; st.ped += ped; st.carLinks += links; st.addrs += addrs; st.bytes += (long long)b.size();
            st.countBad += n != veh + ped;
            st.sizeBad += b.size() != 20 + 28ull * n + 14ull * links + 8ull * addrs + 6ull * EXTRA;
            st.emptyAreas += n == 0;
            // the real loader, via the cut block, over exactly the node region
            RwStream s{ b.data() + 20, 28ull * n, 0, 0, 0 };
            FakePathFind pf{}; pf.LoadNodes(&s, n);
            CHECK(s.pos == 28ull * n && s.overrun == 0);
            const size_t nlOff = 20 + 28ull * n + 14ull * links;
            for (uint32 i = 0; i < n; ++i) {
                const CPathNode& nd = pf.m_pPathNodes[0][i];
                const size_t o = 20 + 28ull * i;
                st.nextNonZero += rd<uint32>(b, o) != 0; st.prevNonZero += rd<uint32>(b, o + 4) != 0; st.byte3NonZero += (rd<uint32>(b, o + 24) >> 24) != 0;
                CHECK(nd.m_next == nullptr && nd.m_prev == nullptr);
                st.areaBad += nd.m_wAreaId != k; st.nodeBad += nd.m_wNodeId != i;
                st.sumX += nd.m_vPos.x.Raw(); st.sumY += nd.m_vPos.y.Raw(); st.sumZ += nd.m_vPos.z.Raw(); st.sumDist += nd.m_totalDistFromOrigin; st.sumBase += nd.m_wBaseLinkId;
                st.sumWidth += nd.m_nPathWidth; st.sumFlood += nd.m_nFloodFill; st.flood0 += nd.m_nFloodFill == 0;
                st.maxNumLinks = std::max<long long>(st.maxNumLinks, nd.m_nNumLinks); st.sumNumLinks += nd.m_nNumLinks;
                st.sumSpawn += nd.m_nSpawnProbability; st.sumBehaviour += nd.m_nBehaviourType;
                const uint32 mem[16] = { 0, 0, 0, 0, nd.m_onDeadEnd, nd.m_isSwitchedOff, nd.m_bRoadBlocks, nd.m_bWaterNode, nd.m_isSwitchedOffOriginal, nd.unk1, nd.m_bDontWander, nd.unk2,
                                         nd.m_bNotHighway, nd.m_bHighway, nd.unk3, nd.unk4 };
                for (int j = 4; j < 16; ++j) st.bits[j] += mem[j];
                // file -> node -> file is the identity (link slots aside)
                CPathNode_File orig; std::memcpy(&orig, b.data() + o, 28); orig.links[0] = orig.links[1] = 0;
                CPathNode_File back = notsa::FileRecord<CPathNode>::ToFile(nd);
                if (!eq(&back, &orig, 28)) { CHECK(false); }
                // link range / targets, from the decoded base link id and link count
                if (nd.m_wBaseLinkId < 0 || (uint32)nd.m_wBaseLinkId + nd.m_nNumLinks > addrs + EXTRA) { st.linkRangeBad++; continue; }
                for (uint32 j = 0; j < nd.m_nNumLinks; ++j) {
                    const uint16 ta = rd<uint16>(b, nlOff + 4ull * (nd.m_wBaseLinkId + j)), tn = rd<uint16>(b, nlOff + 4ull * (nd.m_wBaseLinkId + j) + 2);
                    if (ta == 0xFFFF || tn == 0xFFFF) st.linkInvalid++; else if (ta >= 64) st.linkAreaBad++; else if (tn >= counts[ta]) st.linkNodeBad++;
                }
            }
            delete[] pf.m_pPathNodes[0];
        }
        std::printf("%s\n", statsLine(st).c_str());
        // sanity gates (the numbers the plan demands)
        CHECK(st.files == 64 && st.countBad == 0 && st.sizeBad == 0 && st.areaBad == 0 && st.nodeBad == 0 && st.byte3NonZero == 0 && st.linkRangeBad == 0 && st.linkAreaBad == 0 && st.linkNodeBad == 0);
        CHECK(st.sumNumLinks == st.addrs); // every address slot belongs to exactly one node link
    } else {
        std::printf("NOTE: P2H_NODES_DIR not set: real-data part skipped\n");
    }
    std::printf(fails ? "P2H FAIL (%d)\n" : "P2H ALL OK\n", fails);
    return fails ? 1 : 0;
}
