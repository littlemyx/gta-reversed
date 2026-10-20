// ORACLE-W3-PATHS rows E: WaterLevel.cpp (vertex / quad / triangle tables, block lists, nearest water, sea bed) and Shadows.cpp (stored / permanent / static shadow tables, lighting colour)
// ROW / Tw / w3w::WCase are defined in wave3_oracle_world.inc. Both sides share the water / shadow statics at their exe addresses (NOTSA_ORACLE_ADDRESS_GLOBALS); the whole data image is compared after every case.
// Included from wave3_oracle_world_d.inc.
namespace w3e {
using namespace w3w; using namespace w2;
static const int kGrid[] = { -3000, -2500, -2000, -1000, -500, 0, 500, 1000, 1500, 2500, 3000 };
static const float kZ[] = { 0.f, 1.5f, -2.f, 960.f, 3.25f };
static int GX(Rng& r) { return r.below(7) == 0 ? (int)r.below(7001) - 3500 : kGrid[r.below(11)]; }
// inside the world (-3000..3000): a quad / triangle whose corners collapse onto each other after the clamp of AddWaterLevelVertex leaves the 2+2 corner layout the sort of the exe assumes (it then reads uninitialised stack slots)
static int GXin(Rng& r) { return r.below(7) == 0 ? (int)r.below(6001) - 3000 : kGrid[r.below(11)]; }
static CRenPar RP(Rng& r, bool pool = true) {
    CRenPar p{};
    p.z = pool && r.below(3) ? kZ[r.below(5)] : GF(r, 40.f);
    p.bigWaves = r.below(3) ? 0.f : GF(r, 2.f); p.smallWaves = r.below(3) ? 0.f : GF(r, 1.f);
    p.flowX = (int8)r.u32(); p.flowY = (int8)r.u32();
    return p;
}
static void PutVertex(size_t idx, int x, int y, const CRenPar& p) {
    auto& v = CWaterLevel::m_aVertices[idx];
    v.x = (int16)x; v.y = (int16)y; v.rp = p;
}
// a rectangle in the sorted vertex order of AddWaterLevelQuad: (xa,ya) (xb,ya) (xa,yb) (xb,yb)
static void PutQuad(Rng& r, size_t q, size_t vtx, int xa, int xb, int ya, int yb, bool waves) {
    const int xs[4] = { xa, xb, xa, xb }, ys[4] = { ya, ya, yb, yb };
    for (int k = 0; k < 4; ++k) { CRenPar p = RP(r); if (!waves) { p.bigWaves = p.smallWaves = 0.f; } PutVertex(vtx + k, xs[k], ys[k], p); CWaterLevel::WaterQuads[q].verts[k] = (uint16)(vtx + k); }
}
static void FillWaterGarbage(Rng& r) {
    FillGarbage(r, (uintptr_t)&CWaterLevel::m_aVertices, sizeof(CWaterLevel::m_aVertices));
    FillGarbage(r, (uintptr_t)&CWaterLevel::WaterQuads, sizeof(CWaterLevel::WaterQuads));
    FillGarbage(r, (uintptr_t)&CWaterLevel::WaterTriangles, sizeof(CWaterLevel::WaterTriangles));
}
static void PutPoolVertices(Rng& r, int n) {
    FillWaterGarbage(r);
    CWaterLevel::NumWaterVertices = n;
    for (int i = 0; i < n; ++i) PutVertex(i, GX(r), GX(r), RP(r));
}
// the padding bytes of a stored vertex (+0x12/+0x13) and of a polygon (after the flag byte), and the unused bits of the flag byte, are caller stack garbage in the exe / temporary garbage in the port: not compared
static void NormWater() {
    for (auto& v : CWaterLevel::m_aVertices) { auto* b = reinterpret_cast<uint8_t*>(&v); b[0x12] = b[0x13] = 0; }
    for (auto& q : CWaterLevel::WaterQuads) { auto* b = reinterpret_cast<uint8_t*>(&q); b[8] &= 7; b[9] = 0; }
    for (auto& t : CWaterLevel::WaterTriangles) { auto* b = reinterpret_cast<uint8_t*>(&t); b[6] &= 7; b[7] = 0; }
}
static uint64_t Rp(uint32_t a, uint32_t b) { return (uint64_t)a << 32 | b; }
} // namespace w3e

static void TestW3WorldE() {
    using namespace w2; using namespace w3w; using namespace w3e;
    // ---- WaterLevel tables
    ROW("CWaterLevel::AddWaterLevelVertex", 0x6E5A40, 1500) {
        const int n = r.below(8) == 0 ? 0 : (int)r.below(60);
        const int X = GX(r), Y = GX(r);
        const CRenPar P = RP(r);
        Tw tw([n](Rng& rr) { PutPoolVertices(rr, n); });
        return w3w::WCase(r, d, [&]() -> uint64_t {
            const uint64_t rv = CWaterLevel::AddWaterLevelVertex(X, Y, P); NormWater(); return rv;
        }, [&]() -> uint64_t {
            const uint64_t rv = oracle::Fn<uint32_t __cdecl(int32_t, int32_t, CRenPar)>(0x6E5A40)(X, Y, P); NormWater(); return rv;
        });
    } ENDROW;
    ROW("CWaterLevel::AddWaterLevelQuad", 0x6E7EF0, 1500) {
        const int nv = r.below(60), nq = r.below(10) == 0 ? 290 + (int)r.below(10) : (int)r.below(40);
        int xa = GXin(r), xb = GXin(r), ya = GXin(r), yb = GXin(r);
        if (r.below(10) != 0) { if (xa == xb) xb = xa < 2500 ? xa + 500 : xa - 500; if (ya == yb) yb = ya < 2500 ? ya + 500 : ya - 500; }
        const int px[4] = { xa, xb, xa, xb }, py[4] = { ya, ya, yb, yb };
        int ord[4] = { 0, 1, 2, 3 };
        for (int i = 3; i > 0; --i) std::swap(ord[i], ord[r.below(i + 1)]);
        CRenPar rp[4]; for (auto& p : rp) p = RP(r);
        const uint32_t flags = r.below(4);
        Tw tw([nv, nq](Rng& rr) { PutPoolVertices(rr, nv); CWaterLevel::NumWaterQuads = nq; });
        using Q = void __cdecl(int32_t, int32_t, CRenPar, int32_t, int32_t, CRenPar, int32_t, int32_t, CRenPar, int32_t, int32_t, CRenPar, uint32_t);
        return w3w::WCase(r, d, [&]() -> uint64_t {
            CWaterLevel::AddWaterLevelQuad(px[ord[0]], py[ord[0]], rp[0], px[ord[1]], py[ord[1]], rp[1], px[ord[2]], py[ord[2]], rp[2], px[ord[3]], py[ord[3]], rp[3], flags);
            NormWater(); return CWaterLevel::NumWaterQuads;
        }, [&]() -> uint64_t {
            oracle::Fn<Q>(0x6E7EF0)(px[ord[0]], py[ord[0]], rp[0], px[ord[1]], py[ord[1]], rp[1], px[ord[2]], py[ord[2]], rp[2], px[ord[3]], py[ord[3]], rp[3], flags);
            NormWater(); return CWaterLevel::NumWaterQuads;
        });
    } ENDROW;
    ROW("CWaterLevel::AddWaterLevelTriangle", 0x6E7D40, 1500) {
        const int nv = r.below(60), nt = (int)r.below(5);
        int xa = GXin(r), xb = GXin(r), ya = GXin(r), yb = GXin(r);
        if (r.below(10) != 0) { if (xa == xb) xb = xa < 2500 ? xa + 500 : xa - 500; if (ya == yb) yb = ya < 2500 ? ya + 500 : ya - 500; }
        // right triangle: two vertices share y (A, B), two share x (A or B with C)
        const bool cAtA = r.below(2);
        const int px[3] = { xa, xb, cAtA ? xa : xb }, py[3] = { ya, ya, yb };
        int ord[3] = { 0, 1, 2 };
        for (int i = 2; i > 0; --i) std::swap(ord[i], ord[r.below(i + 1)]);
        CRenPar rp[3]; for (auto& p : rp) p = RP(r);
        const uint32_t flags = r.below(4);
        Tw tw([nv, nt](Rng& rr) { PutPoolVertices(rr, nv); CWaterLevel::NumWaterTriangles = nt; });
        using T = void __cdecl(int32_t, int32_t, CRenPar, int32_t, int32_t, CRenPar, int32_t, int32_t, CRenPar, uint32_t);
        return w3w::WCase(r, d, [&]() -> uint64_t {
            CWaterLevel::AddWaterLevelTriangle(px[ord[0]], py[ord[0]], rp[0], px[ord[1]], py[ord[1]], rp[1], px[ord[2]], py[ord[2]], rp[2], flags);
            NormWater(); return CWaterLevel::NumWaterTriangles;
        }, [&]() -> uint64_t {
            oracle::Fn<T>(0x6E7D40)(px[ord[0]], py[ord[0]], rp[0], px[ord[1]], py[ord[1]], rp[1], px[ord[2]], py[ord[2]], rp[2], flags);
            NormWater(); return CWaterLevel::NumWaterTriangles;
        });
    } ENDROW;
    // ---- block lists
    ROW("CWaterLevel::BlockHit", 0x6E6CA0, 1500) {
        const int bx = (int)r.below(16) - 2, by = (int)r.below(16) - 2;
        const int area = r.below(3) ? 0 : (int)r.below(20);
        Tw tw([area](Rng& rr) {
            FillWaterGarbage(rr);
            for (size_t i = 0; i < CWaterLevel::m_aVertices.size(); ++i) CWaterLevel::m_aVertices[i].rp.z = kZ[rr.below(5)];
            for (auto& q : CWaterLevel::WaterQuads) for (auto& v : q.verts) v = (uint16)rr.below(1021);
            for (auto& t : CWaterLevel::WaterTriangles) for (auto& v : t.verts) v = (uint16)rr.below(1021);
            const auto pack = [&](bool comboOk) -> uint16_t {
                const uint32_t ty = rr.below(comboOk ? 4 : 3);
                const uint32_t id = ty == 0 ? rr.below(16384) : ty == 1 ? rr.below(301) : ty == 2 ? rr.below(6) : rr.below(690);
                return (uint16_t)(id | ty << 14);
            };
            for (int x = 0; x < 12; ++x) for (int y = 0; y < 12; ++y) { const uint16_t v = pack(true); std::memcpy(&CWaterLevel::m_BlockPolyInfo[x][y], &v, 2); }
            for (auto& c : CWaterLevel::m_PolyCombos) { const uint16_t v = rr.below(5) == 0 ? 0 : pack(false); std::memcpy(&c, &v, 2); }
            { const uint16_t z = 0; std::memcpy(&CWaterLevel::m_PolyCombos[699], &z, 2); }
            CGame::currArea = (eAreaCodes)area;
            CWaterLevel::m_NumBlocksOutsideWorldToBeRendered = rr.below(4) ? rr.below(60) : 60 + rr.below(20);
        });
        return w3w::WCase(r, d, [&]() -> uint64_t {
            CWaterLevel::BlockHit(bx, by); return CWaterLevel::m_NumBlocksOutsideWorldToBeRendered;
        }, [&]() -> uint64_t {
            oracle::Fn<void __cdecl(int32_t, int32_t)>(0x6E6CA0)(bx, by); return CWaterLevel::m_NumBlocksOutsideWorldToBeRendered;
        });
    } ENDROW;
    ROW("CWaterLevel::FillQuadsAndTrianglesList", 0x6E7B30, 200) {
        const int nq = r.below(8) == 0 ? 0 : (int)r.below(30), nt = (int)r.below(7), zone = r.below(2) ? 0 : (int)r.below(100);
        Tw tw([nq, nt, zone](Rng& rr) {
            FillWaterGarbage(rr);
            CWaterLevel::NumWaterQuads = nq; CWaterLevel::NumWaterTriangles = nt; CWaterLevel::NumWaterZonePolys = zone;
            std::memset(&CWaterLevel::m_BlockPolyInfo, 0, sizeof(CWaterLevel::m_BlockPolyInfo));
            for (int q = 0; q < nq; ++q) {
                const int xa = kGrid[rr.below(11)] + (rr.below(3) == 0 ? (int)rr.below(500) : 0), ya = kGrid[rr.below(11)] + (rr.below(3) == 0 ? (int)rr.below(500) : 0);
                PutQuad(rr, q, 4 * q, xa, xa + 100 + (int)rr.below(900), ya, ya + 100 + (int)rr.below(900), true);
            }
            for (int t = 0; t < nt; ++t) {
                const int xa = kGrid[rr.below(11)] + (rr.below(3) == 0 ? (int)rr.below(500) : 0), ya = kGrid[rr.below(11)] + (rr.below(3) == 0 ? (int)rr.below(500) : 0);
                const int xb = xa + 100 + (int)rr.below(900), yb = ya + 100 + (int)rr.below(900);
                const bool cAtA = rr.below(2);
                PutVertex(600 + 3 * t, xa, ya, RP(rr)); PutVertex(601 + 3 * t, xb, ya, RP(rr)); PutVertex(602 + 3 * t, cAtA ? xa : xb, yb, RP(rr));
                for (int k = 0; k < 3; ++k) CWaterLevel::WaterTriangles[t].verts[k] = (uint16)(600 + 3 * t + k);
            }
        });
        return w3w::WCase(r, d, [&]() -> uint64_t {
            CWaterLevel::FillQuadsAndTrianglesList(); return CWaterLevel::NumWaterZonePolys;
        }, [&]() -> uint64_t {
            oracle::Fn<void __cdecl()>(0x6E7B30)(); return CWaterLevel::NumWaterZonePolys;
        });
    } ENDROW;
    ROW("CWaterLevel::FindNearestWaterAndItsFlow", 0x6E9D70, 1200) {
        const int nq = r.below(10) == 0 ? 0 : (int)r.below(40);
        const bool inWorld = r.below(8) != 0;
        const float cx = inWorld ? (float)((int)r.below(6001) - 3000) + r.f01() : Fl(r, 3600.f, 4), cy = inWorld ? (float)((int)r.below(6001) - 3000) + r.f01() : Fl(r, 3600.f, 4);
        Tw tw([nq, cx, cy](Rng& rr) {
            FillWaterGarbage(rr);
            CWaterLevel::NumWaterQuads = nq;
            for (int q = 0; q < nq; ++q) {
                const int xa = GX(rr), ya = GX(rr);
                PutQuad(rr, q, 4 * q, xa, xa + 100 + (int)rr.below(900), ya, ya + 100 + (int)rr.below(900), rr.below(3) != 0);
            }
            TheCamera.m_matrix = nullptr;
            TheCamera.m_placement.m_vPosn = CVector(cx, cy, 50.f);
            TheCamera.m_fDistanceToWater = GF(rr, 9.f); TheCamera.m_fHeightOfNearestWater = GF(rr, 9.f);
            CWaterLevel::m_CurrentDesiredFlow = CVector2D(GF(rr, 2.f), GF(rr, 2.f));
        });
        return w3w::WCase(r, d, [&]() -> uint64_t {
            CWaterLevel::FindNearestWaterAndItsFlow(); return FB(TheCamera.m_fDistanceToWater) ^ FB(TheCamera.m_fHeightOfNearestWater) << 1;
        }, [&]() -> uint64_t {
            oracle::Fn<void __cdecl()>(0x6E9D70)(); return FB(TheCamera.m_fDistanceToWater) ^ FB(TheCamera.m_fHeightOfNearestWater) << 1;
        });
    } ENDROW;
    // ---- sea bed (writes the im3d render buffer)
    ROW("CWaterLevel::RenderSeaBedSegment", 0x6E6870, 800) {
        const int bx = (int)r.below(12), by = (int)r.below(12);
        const float mnx = r.below(8) ? r.f01() : Fl(r, 2.f, 1), mxx = r.below(8) ? r.f01() : Fl(r, 2.f, 1), mny = r.below(8) ? r.f01() : Fl(r, 2.f, 1), mxy = r.below(8) ? r.f01() : Fl(r, 2.f, 1);
        Tw tw([](Rng& rr) { uiTempBufferIndicesStored = (uint16)rr.below(300); uiTempBufferVerticesStored = (uint16)rr.below(300); });
        return w3w::WCase(r, d, [&]() -> uint64_t {
            CWaterLevel::RenderSeaBedSegment(bx, by, mnx, mxx, mny, mxy); return Rp(uiTempBufferIndicesStored, uiTempBufferVerticesStored);
        }, [&]() -> uint64_t {
            oracle::Fn<void __cdecl(int32_t, int32_t, float, float, float, float)>(0x6E6870)(bx, by, mnx, mxx, mny, mxy); return Rp(uiTempBufferIndicesStored, uiTempBufferVerticesStored);
        });
    } ENDROW;
    ROW("CWaterLevel::RenderDetailedSeaBedSegment", 0x6E6A10, 500) {
        const int bx = (int)r.below(12), by = (int)r.below(12);
        const float mnx = r.below(8) ? r.f01() * 0.5f : Fl(r, 2.f, 1), mny = r.below(8) ? r.f01() * 0.5f : Fl(r, 2.f, 1);
        const float mxx = r.below(8) ? mnx + r.f01() * 1.2f : Fl(r, 2.f, 1), mxy = r.below(8) ? mny + r.f01() * 1.2f : Fl(r, 2.f, 1);
        Tw tw([](Rng& rr) { uiTempBufferIndicesStored = (uint16)rr.below(300); uiTempBufferVerticesStored = (uint16)rr.below(300); });
        return w3w::WCase(r, d, [&]() -> uint64_t {
            CWaterLevel::RenderDetailedSeaBedSegment(bx, by, mnx, mxx, mny, mxy); return Rp(uiTempBufferIndicesStored, uiTempBufferVerticesStored);
        }, [&]() -> uint64_t {
            oracle::Fn<void __cdecl(int32_t, int32_t, float, float, float, float)>(0x6E6A10)(bx, by, mnx, mxx, mny, mxy); return Rp(uiTempBufferIndicesStored, uiTempBufferVerticesStored);
        });
    } ENDROW;
}
