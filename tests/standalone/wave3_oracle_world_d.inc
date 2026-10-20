// ORACLE-W3-PATHS rows D: route finding (CPathFind::DoPathSearch / ComputeRoute / FindNextNodeWandering) on the ORACLE-W3-WORLD node fixture (see wave3_oracle_world.inc / _b.inc).
// Route rows rebuild the links of the 64 map areas as a SYMMETRIC graph (a->b and b->a with the same length, up to 4 links per node at slots 4n..4n+3): the exe walks back from the origin along the links whose
// distance decreases and loops forever when none matches, which a real (two-way) road network cannot do. All nodes start with m_totalDistFromOrigin = 32766 (SHRT_MAX - 1: the search resets them).
using namespace w3w;
static unsigned g_pathCases[3] = {}, g_pathFound[3] = {}, g_pathLong[3] = {};   // per row (0 DoPathSearch, 1 ComputeRoute, 2 FindNextNodeWandering): exe-side cases, results with >= 1 node, results with >= 3 nodes
static void SymmetricLinks(Rng& r, bool sameFlood) {
    int cnt[64][NNODE] = {};
    for (int a = 0; a < NAREA; ++a) for (int n = 0; n < NNODE; ++n) {
        uint8_t* raw = Node(a, n);
        PutI16(raw, 0xE, 32766);
        PutI16(raw, 0x10, 4 * n);
        if (sameFlood) raw[0x17] = 0;
        raw[0x18] = (uint8_t)(raw[0x18] & 0xF0);
    }
    for (int a = 0; a < NAREA; ++a) for (int l = 0; l < NLINK; ++l) g_pf.links[a][l] = 0x0000FFFFu;
    for (int a = 0; a < 64; ++a) for (int n = 0; n < NNODE; ++n) {
        const int want = (int)r.below(5);
        for (int t = 0; t < want && cnt[a][n] < 4; ++t) {
            const int a2 = r.below(3) ? (a + (int)r.below(3) + 63) % 64 : (int)r.below(64);
            const int n2 = (int)r.below(NNODE);
            if ((a2 == a && n2 == n) || cnt[a2][n2] >= 4) continue;
            const int k = cnt[a][n]++, k2 = cnt[a2][n2]++;
            const uint32_t len = r.below(12) == 0 ? r.below(256) : 1 + r.below(60);
            g_pf.links[a][4 * n + k] = NA(a2, n2); g_pf.links[a2][4 * n2 + k2] = NA(a, n);
            g_pf.linkLen[a][4 * n + k] = (uint8_t)len; g_pf.linkLen[a2][4 * n2 + k2] = (uint8_t)len;
        }
    }
    for (int a = 0; a < 64; ++a) {
        for (int n = 0; n < NNODE; ++n) Node(a, n)[0x18] |= (uint8_t)cnt[a][n];
        for (int l = 0; l < NLINK; ++l) {   // navi link of slot l -> car path link (own area mostly) whose attachedTo is the linked node (2/3) or a random node
            const int ar = r.below(10) ? a : (int)r.below(64);
            g_pf.naviLinks[a][l] = (uint16_t)((ar & 0x3F) << 10 | l);
            uint32_t att = g_pf.links[a][l] != 0xFFFFu ? g_pf.links[a][l] : NA((int)r.below(NAREA), (int)r.below(NNODE));
            if (r.below(3) == 0) att = NA((int)r.below(NAREA), (int)r.below(NNODE));
            std::memcpy(g_pf.navi[ar][l] + 4, &att, 4);
        }
    }
}
static CNodeAddress* NaP(size_t off) { return reinterpret_cast<CNodeAddress*>(S(off)); }
static CNodeAddress NaOf(uint32_t v) { return CNodeAddress{ (uint16)(v & 0xFFFF), (uint16)(v >> 16) }; }

static void TestW3WorldD() {
    using namespace w2; using namespace w3w;
    // a node address for a hint: a map-area node (loaded or not), an interior node, invalid (area 0xFFFF)
    const auto Hint = [](Rng& rr) -> uint32_t {
        const uint32_t k = rr.below(10);
        if (k == 0) return 0x0000FFFFu | (rr.below(NNODE) << 16);
        if (k == 1) return NA(64 + (int)rr.below(8), (int)rr.below(NNODE));
        return NA((int)rr.below(64), (int)rr.below(NNODE));
    };
    const auto PosFor = [](Rng& rr, uint32_t addr) -> CVector {
        const int a = addr & 0xFFFF, n = addr >> 16;
        CVector p = (a < NAREA && n < NNODE) ? NodeP(a, n)->GetPosition() : CVector(0, 0, 0);
        const uint32_t k = rr.below(6);
        if (k == 0) return CVector(FlP(rr, 3500.f), FlP(rr, 3500.f), FlP(rr, 100.f));
        if (k == 1) return CVector(Fl(rr, 3500.f, 8), Fl(rr, 3500.f, 8), Fl(rr, 100.f, 8));
        p.x += (rr.f01() * 2 - 1) * 30.f; p.y += (rr.f01() * 2 - 1) * 30.f; p.z += (rr.f01() * 2 - 1) * 6.f;
        return p;
    };
    // ---- DoPathSearch
    ROW("CPathFind::DoPathSearch", 0x4515D0, 1500) {
        const bool sameFlood = r.below(10) != 0;
        const uint32_t oH = Hint(r), tH = Hint(r);
        const bool useT = r.below(5) != 0;     // pass the target hint (else the closest node to targetPos)
        const CVector oPos = PosFor(r, oH), tPos = PosFor(r, tH);
        const int ptype = r.below(8) ? 0 : (r.below(2) ? 1 : 2), maxN = r.below(14) == 0 ? (int)r.below(3) : 1 + (int)r.below(20);
        const float maxDist = r.below(3) ? 999999.875f : (r.below(8) == 0 ? Fl(r, 3000.f, 1) : 50.f + r.f01() * 3000.f);
        const float maxDepth = r.below(3) ? 999999.875f : (r.below(8) == 0 ? Fl(r, 3000.f, 1) : (float)r.below(400));
        const bool sameLane = r.below(4) == 0, water = r.below(3) == 0, boats = r.below(6) == 0, wantDist = r.below(4) != 0;
        const uint32_t forb = r.below(3) == 0 ? NA((int)r.below(64), (int)r.below(NNODE)) : (r.below(2) ? 0x0000FFFFu : Hint(r));
        Tw tw([sameFlood](Rng& rr) { SymmetricLinks(rr, sameFlood); });
        const auto reset = [&] {
            std::memset(S(oOut), 0xCD, 0x400);
            SAt<uint32_t>(oOut + 0x300) = tH; SAt<int16_t>(oOut + 0x308) = 0x5A5A; SAt<float>(oOut + 0x30C) = -1.f;
        };
        return w3w::WCase(r, d, [&]() -> uint64_t {
            reset();
            int16 cnt = 0x5A5A; float dist = -1.f; CNodeAddress th = NaOf(tH);
            ThePaths.DoPathSearch((ePathType)ptype, oPos, NaOf(oH), tPos, NaP(oOut), cnt, maxN, wantDist ? &dist : nullptr, maxDist, useT ? &th : nullptr, maxDepth, sameLane, NaOf(forb), water, boats);
            SAt<int16_t>(oOut + 0x308) = cnt; SAt<float>(oOut + 0x30C) = dist;
            return ((uint64_t)(uint16_t)cnt << 32) ^ FB(wantDist ? dist : 0.f);
        }, [&]() -> uint64_t {
            reset();
            int16_t cnt = 0x5A5A; float dist = -1.f;
            using DPS = void __fastcall(CPathFind*, int, uint32_t, float, float, float, uint32_t, float, float, float, CNodeAddress*, int16_t*, int, float*, float, uint32_t*, float, uint32_t, uint32_t, uint32_t, uint32_t);
            oracle::Fn<DPS>(0x4515D0)(&ThePaths, 0, (uint32_t)ptype, oPos.x, oPos.y, oPos.z, oH, tPos.x, tPos.y, tPos.z, NaP(oOut), &cnt, maxN, wantDist ? &dist : nullptr, maxDist,
                useT ? reinterpret_cast<uint32_t*>(S(oOut + 0x300)) : nullptr, maxDepth, (uint32_t)sameLane, forb, (uint32_t)water, (uint32_t)boats);
            SAt<int16_t>(oOut + 0x308) = cnt; SAt<float>(oOut + 0x30C) = dist;
            ++g_pathCases[0]; if (cnt > 0) ++g_pathFound[0]; if (cnt >= 3) ++g_pathLong[0];
            return ((uint64_t)(uint16_t)cnt << 32) ^ FB(wantDist ? dist : 0.f);
        });
    } ENDROW;
    // ---- ComputeRoute
    ROW("CPathFind::ComputeRoute", 0x452760, 800) {
        const uint32_t oH = Hint(r);
        const uint32_t tH = Hint(r);
        const CVector sPos = PosFor(r, oH), ePos = PosFor(r, tH);
        const int ptype = r.below(8) ? 0 : 1;
        const bool sameFlood = r.below(10) != 0;
        Tw tw([sameFlood](Rng& rr) { SymmetricLinks(rr, sameFlood); });
        const auto reset = [&] { std::memset(S(oOut), 0xCD, 0x400); SAt<CVector>(oOut + 0x300) = sPos; SAt<CVector>(oOut + 0x30C) = ePos; SAt<uint32_t>(oOut + 0x318) = oH; };
        return w3w::WCase(r, d, [&]() -> uint64_t {
            reset();
            ThePaths.ComputeRoute((uint8)ptype, SAt<CVector>(oOut + 0x300), SAt<CVector>(oOut + 0x30C), SAt<CNodeAddress>(oOut + 0x318), reinterpret_cast<CNodeRoute*>(S(oOut)));
            return (uint64_t)SAt<uint32_t>(oOut);
        }, [&]() -> uint64_t {
            reset();
            using CR = void __fastcall(CPathFind*, int, uint32_t, void*, void*, void*, void*);
            oracle::Fn<CR>(0x452760)(&ThePaths, 0, (uint32_t)ptype, S(oOut + 0x300), S(oOut + 0x30C), S(oOut + 0x318), S(oOut));
            ++g_pathCases[1]; if (SAt<uint32_t>(oOut) > 0 && SAt<uint32_t>(oOut) < 9) ++g_pathFound[1]; if (SAt<uint32_t>(oOut) >= 3 && SAt<uint32_t>(oOut) < 9) ++g_pathLong[1];
            return (uint64_t)SAt<uint32_t>(oOut);
        });
    } ENDROW;
    // ---- FindNextNodeWandering (standard fixture: random directed links, 85% of the areas loaded)
    ROW("CPathFind::FindNextNodeWandering", 0x451B70, 2500) {
        const uint32_t oH = Hint(r);
        const int ptype = r.below(6) ? 0 : 1;
        const uint32_t dirK = r.below(12);
        const uint8_t dir = (uint8_t)(dirK == 0 ? r.u32() : r.below(8));
        const CVector pos = PosFor(r, oH);
        const bool flat = r.below(3) == 0;   // sometimes two nodes share x/y (zero length link vector)
        Tw tw([flat](Rng& rr) {
            for (int a = 0; a < 64; ++a) for (int n = 0; n < NNODE; ++n) {
                uint8_t* raw = Node(a, n);
                raw[0x16] = (uint8_t)(rr.below(3) ? rr.below(96) : rr.u32());                  // path width
                if (flat && rr.below(6) == 0) { std::memcpy(raw + 8, Node(a, (n + 1) % NNODE) + 8, 4); }
            }
        });
        // the node id of an address whose area is 0xFFFF is stale stack in the exe (the hidden result slot of FindNodeClosestToCoors is the caller's uninitialised local): not compared
        const auto NormInvalid = [&] { for (size_t o : { (size_t)0, (size_t)4 }) if ((SAt<uint32_t>(oOut + o) & 0xFFFF) == 0xFFFF) SAt<uint32_t>(oOut + o) = 0xFFFF; };
        const auto reset = [&] {
            std::memset(S(oOut), 0xCD, 0x100);
            SAt<uint32_t>(oOut) = oH; SAt<uint32_t>(oOut + 4) = 0x00FF1234u; SAt<uint8_t>(oOut + 8) = 0xAA;
        };
        return w3w::WCase(r, d, [&]() -> uint64_t {
            reset();
            ThePaths.FindNextNodeWandering((uint8)ptype, pos, NaP(oOut), NaP(oOut + 4), dir, S(oOut + 8));
            NormInvalid();
            return ((uint64_t)SAt<uint32_t>(oOut + 4) << 8) ^ SAt<uint8_t>(oOut + 8);
        }, [&]() -> uint64_t {
            reset();
            using FW = void __fastcall(CPathFind*, int, uint32_t, float, float, float, CNodeAddress*, CNodeAddress*, uint32_t, uint8_t*);
            oracle::Fn<FW>(0x451B70)(&ThePaths, 0, (uint32_t)ptype, pos.x, pos.y, pos.z, NaP(oOut), NaP(oOut + 4), (uint32_t)dir, S(oOut + 8));
            NormInvalid();
            ++g_pathCases[2]; if ((SAt<uint32_t>(oOut + 4) & 0xFFFF) != 0xFFFF) ++g_pathFound[2];
            return ((uint64_t)SAt<uint32_t>(oOut + 4) << 8) ^ SAt<uint8_t>(oOut + 8);
        });
    } ENDROW;
    static const char* nm[3] = { "DoPathSearch", "ComputeRoute", "FindNextNodeWandering" };
    for (int i = 0; i < 3; ++i) if (g_pathCases[i]) std::printf("[paths] %s: route found in %u / %u cases = %.1f%% (>= 3 nodes: %.1f%%), exe side, both precision passes\n", nm[i], g_pathFound[i], g_pathCases[i], 100.0 * g_pathFound[i] / g_pathCases[i], 100.0 * g_pathLong[i] / g_pathCases[i]);
}
