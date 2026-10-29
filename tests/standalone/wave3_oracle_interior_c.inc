// ORACLE-W3-INTERIOR rows C: Interior_c (see wave3_oracle_interior.inc). ROW / IC / Tw are defined there.
static int g_ti;   // the interior under test
static uint32_t TileHash(int i) { const uint8_t* t = reinterpret_cast<const uint8_t*>(w3i::Int(i)) + 0x68; uint32_t h = 0; for (int k = 0; k < 900; ++k) h = h * 31 + t[k]; return h; }
static void ClearTileRect(Rng& rr, int x, int y, int w, int d) {   // zero a rectangle of the grid so that the "empty" paths are reached
    using namespace w3i;
    uint8_t* t = reinterpret_cast<uint8_t*>(Int(g_ti)) + 0x68;
    for (int i = 0; i < w; ++i) for (int j = 0; j < d; ++j) if (x + i >= 0 && x + i < 30 && y + j >= 0 && y + j < 30) t[(x + i) * 30 + y + j] = 0;
    (void)rr;
}
static void TestW3InteriorC() {
    using namespace w2; using namespace w3i;
    ROW("Interior_c::GetFurnitureEntity", 0x5913B0, 400) {
        const int i = r.below(8), k = r.below(10);
        IC(return U(Int(i)->GetFurnitureEntity(k >= 8 ? nullptr : Ent(k)));, return (uint64_t)ExeT<uint32_t>(0x5913B0, Int(i), (void*)(k >= 8 ? nullptr : Ent(k))););
    } ENDROW;
    ROW("Interior_c::IsPtInside", 0x5913E0, 3000) {
        const int i = r.below(8), bm = r.below(4);
        Tw tw([i, bm](Rng& rr) {
            PickPoint(rr, i);
            if (rr.below(4) == 0) { // near the faces: scale a face distance
                const auto* b = At<tEffectInterior*>((uintptr_t)Int(i) + 0x14);
                const CVector c = InteriorLocal(rr, i, 0.f); (void)b; g_pt = c;
            }
            std::memcpy(S(oOut + 0x20), &g_pt, 12);
            g_bx = bm == 0 ? 0.f : 5.f; g_by = bm == 0 ? 0.f : 5.f; g_bz = bm == 3 ? Fl(rr, 4.f, 3) : 0.f;
            if (bm == 2) { g_bx = Fl(rr, 3.f, 6); g_by = Fl(rr, 3.f, 6); }
        });
        IC(return B8(Int(i)->IsPtInside(*reinterpret_cast<CVector*>(S(oOut + 0x20)), CVector(g_bx, g_by, g_bz)));,
           return (uint64_t)ExeT<uint8_t>(0x5913E0, Int(i), (void*)S(oOut + 0x20), g_bx, g_by, g_bz););
    } ENDROW;
    ROW("Interior_c::CheckTilesEmpty", 0x591680, 3000) {
        const int i = r.below(8), x = r.below(6) == 0 ? (int)r.below(70) - 20 : r.below(34), y = r.below(6) == 0 ? (int)r.below(70) - 20 : r.below(34), w = r.below(8) == 0 ? (int)r.below(60) - 10 : r.below(7), dp = r.below(8) == 0 ? (int)r.below(60) - 10 : r.below(7), a9 = r.below(2);
        Tw tw([i, x, y, w, dp](Rng& rr) { g_ti = i; if (rr.below(2)) ClearTileRect(rr, x, y, w, dp); else if (rr.below(2)) { ClearTileRect(rr, x, y, w, dp); if (w > 0 && dp > 0 && x >= 0 && y >= 0 && x < 30 && y < 30) reinterpret_cast<uint8_t*>(Int(i))[0x68 + x * 30 + y] = rr.below(2) ? 9 : (uint8_t)rr.below(12); } });
        IC(return (uint64_t)(uint8_t)Int(i)->CheckTilesEmpty(x, y, w, dp, (uint8)a9);, return (uint64_t)ExeT<uint8_t>(0x591680, Int(i), x, y, w, dp, a9););
    } ENDROW;
    ROW("Interior_c::SetTilesStatus", 0x591700, 3000) {
        const int i = r.below(8), x = r.below(8) == 0 ? (int)r.below(70) - 20 : r.below(32), y = r.below(8) == 0 ? (int)r.below(70) - 20 : r.below(32), w = r.below(8) == 0 ? (int)r.below(60) - 10 : r.below(7), dp = r.below(8) == 0 ? (int)r.below(60) - 10 : r.below(7), st = r.below(14), ov = r.below(2);
        Tw tw([i, x, y, w, dp](Rng& rr) {
            g_ti = i;
            { auto* b = At<tEffectInterior*>((uintptr_t)Int(i) + 0x14); if (rr.below(4)) { if (x >= 0 && w > 0 && x + w <= 40 && b->m_width < x + w) b->m_width = (uint8_t)(x + w + rr.below(3)); if (y >= 0 && dp > 0 && y + dp <= 40 && b->m_depth < y + dp) b->m_depth = (uint8_t)(y + dp + rr.below(3)); } }   // mostly inside the grid so that the tile rules run
            if (rr.below(3) == 0) { uint8_t* t = reinterpret_cast<uint8_t*>(Int(i)) + 0x68; for (int k = 0; k < 900; ++k) if (t[k] == 0 && rr.below(2)) t[k] = (uint8_t)(rr.below(2) ? 3 : 9); } });
        IC(const uint32_t h0 = TileHash(i); Int(i)->SetTilesStatus(x, y, w, dp, st, (int8)ov); return (uint64_t)(TileHash(i) ^ h0);, const uint32_t h0 = TileHash(i); ExeT<void>(0x591700, Int(i), x, y, w, dp, st, ov); return (uint64_t)(TileHash(i) ^ h0););
    } ENDROW;
    ROW("Interior_c::SetCornerTiles", 0x5917C0, 2000) {
        const int i = r.below(8), cn = r.below(8) == 0 ? (int)r.below(8) - 2 : r.below(4), sz = r.below(8) == 0 ? (int)r.below(40) : 1 + r.below(7), st = r.below(14), ov = r.below(2);
        Tw tw([i](Rng& rr) { (void)rr; g_ti = i; });
        IC(const uint32_t h0 = TileHash(i); Int(i)->SetCornerTiles(cn, sz, st, (uint8)ov); return (uint64_t)(TileHash(i) ^ h0);, const uint32_t h0 = TileHash(i); ExeT<void>(0x5917C0, Int(i), cn, sz, st, ov); return (uint64_t)(TileHash(i) ^ h0););
    } ENDROW;
    ROW("Interior_c::GetTileStatus", 0x5918E0, 2000) {
        const int i = r.below(8), x = r.below(5) == 0 ? (int)r.below(80) - 25 : r.below(32), y = r.below(5) == 0 ? (int)r.below(80) - 25 : r.below(32);
        IC(return (uint64_t)(uint32_t)Int(i)->GetTileStatus(x, y);, return (uint64_t)ExeT<uint32_t>(0x5918E0, Int(i), x, y););
    } ENDROW;
    ROW("Interior_c::GetNumEmptyTiles", 0x591920, 3000) {
        const int i = r.below(8), x = r.below(6) == 0 ? (int)r.below(70) - 20 : r.below(32), y = r.below(6) == 0 ? (int)r.below(70) - 20 : r.below(32), dir = r.below(10) == 0 ? (int)r.below(8) - 2 : r.below(4), len = 1 + r.below(r.below(6) ? 4 : 12);
        Tw tw([i, x, y](Rng& rr) { g_ti = i; if (rr.below(2)) ClearTileRect(rr, x - rr.below(30), y - rr.below(30), 30, 30); });
        IC(return (uint64_t)(uint32_t)Int(i)->GetNumEmptyTiles(x, y, dir, len);, return (uint64_t)ExeT<uint32_t>(0x591920, Int(i), x, y, dir, len););
    } ENDROW;
    ROW("Interior_c::FindBoundingBox", 0x5922C0, 3000) {
        const int i = r.below(8), x = r.below(30), y = r.below(30);
        Tw tw([i, x, y](Rng& rr) {   // a connected blob of "5" tiles (furniture footprints) around the start tile; visited cleared except the start
            g_ti = i;
            uint8_t* t = reinterpret_cast<uint8_t*>(Int(i)) + 0x68;
            const int nb = rr.below(4);
            for (int b = 0; b < nb; ++b) { const int bx = x - 2 + rr.below(5), by = y - 2 + rr.below(5), bw = 1 + rr.below(6), bd = 1 + rr.below(6); for (int a = 0; a < bw; ++a) for (int c = 0; c < bd; ++c) if (bx + a >= 0 && bx + a < 30 && by + c >= 0 && by + c < 30) t[(bx + a) * 30 + by + c] = 5; }
            if (rr.below(2)) t[x * 30 + y] = 5;
            std::memset(S(oVis), 0, 900 * 4);
            if (rr.below(8) != 0) SAt<int32_t>(oVis + 4 * (size_t)(x * 30 + y)) = 1;
            if (rr.below(10) == 0) for (int k = 0; k < 60; ++k) SAt<int32_t>(oVis + 4 * (size_t)rr.below(900)) = 1;
            SAt<int32_t>(oOut) = x; SAt<int32_t>(oOut + 4) = x; SAt<int32_t>(oOut + 8) = y; SAt<int32_t>(oOut + 12) = y;
        });
        IC(Int(i)->FindBoundingBox(x, y, &SAt<int32_t>(oOut), &SAt<int32_t>(oOut + 4), &SAt<int32_t>(oOut + 8), &SAt<int32_t>(oOut + 12), &SAt<int32_t>(oVis)); return (uint64_t)((SAt<int32_t>(oOut) != x) | ((SAt<int32_t>(oOut + 4) != x) << 1) | ((SAt<int32_t>(oOut + 8) != y) << 2) | ((SAt<int32_t>(oOut + 12) != y) << 3));,
           ExeT<void>(0x5922C0, Int(i), x, y, (void*)S(oOut), (void*)S(oOut + 4), (void*)S(oOut + 8), (void*)S(oOut + 12), (void*)S(oVis)); return (uint64_t)((SAt<int32_t>(oOut) != x) | ((SAt<int32_t>(oOut + 4) != x) << 1) | ((SAt<int32_t>(oOut + 8) != y) << 2) | ((SAt<int32_t>(oOut + 12) != y) << 3)););
    } ENDROW;
    ROW("Interior_c::ResetTiles", 0x593910, 4000) {
        const int i = r.below(8);
        IC(const uint32_t h0 = TileHash(i); Int(i)->ResetTiles(); return (uint64_t)(TileHash(i) ^ h0);, const uint32_t h0 = TileHash(i); ExeT<void>(0x593910, Int(i)); return (uint64_t)(TileHash(i) ^ h0););
    } ENDROW;
    ROW("Interior_c::Shop_FurnishCeiling", 0x59A130, 20) {
        const int i = r.below(8);
        IC(Int(i)->Shop_FurnishCeiling(); return 0;, ExeT<void>(0x59A130, Int(i)); return 0;);
    } ENDROW;
    ROW("Interior_c::IsVisible", 0x5929F0, 3000) {
        const int i = r.below(8);
        Tw tw([i](Rng& rr) {   // camera inside the interior, inside the 5-unit margin, near the door circle (10 units around m_position) or far away
            const int m = rr.below(4);
            if (m == 0) TheCamera.m_placement.m_vPosn = InteriorLocal(rr, i, 0.f);
            else if (m == 1) TheCamera.m_placement.m_vPosn = InteriorLocal(rr, i, 7.f);
            else if (m == 2) { CVector p = At<CVector>((uintptr_t)Int(i) + 0x400); p.x += (rr.f01() - 0.5f) * 24.f; p.y += (rr.f01() - 0.5f) * 24.f; TheCamera.m_placement.m_vPosn = p; }
            if (rr.below(30) == 0) { float v = std::numeric_limits<float>::quiet_NaN(); std::memcpy(&TheCamera.m_placement.m_vPosn.x + rr.below(3), &v, 4); }
        });
        IC(return B8(Int(i)->IsVisible());, return (uint64_t)ExeT<uint8_t>(0x5929F0, Int(i)););
    } ENDROW;

    // ---- ASM-S34 rows: rand()-driven / pure helpers of Interior_c (outs + the whole exe image incl. the interior struct are compared) ----
    ROW("Interior_c::GetRandomTile", 0x591B20, 1500) {
        const int i = r.below(8), st = r.below(12);
        Tw tw([i, st](Rng& rr) {   // a box with a real grid and at least one tile of the wanted status (else both loop for ever)
            g_ti = i;
            auto* b = At<tEffectInterior*>((uintptr_t)Int(i) + 0x14);
            b->m_width = (uint8_t)(2 + rr.below(14)); b->m_depth = (uint8_t)(2 + rr.below(14));
            uint8_t* t = reinterpret_cast<uint8_t*>(Int(i)) + 0x68;
            const int n = 1 + rr.below(4);
            for (int k = 0; k < n; ++k) t[rr.below(b->m_width) * 30 + rr.below(b->m_depth)] = (uint8_t)st;
            SAt<int32_t>(oOut) = -7; SAt<int32_t>(oOut + 4) = -7;
        });
        IC(Int(i)->GetRandomTile(st, &SAt<int32_t>(oOut), &SAt<int32_t>(oOut + 4)); return (uint64_t)(uint32_t)SAt<int32_t>(oOut) | ((uint64_t)(uint32_t)SAt<int32_t>(oOut + 4) << 32);,
           ExeT<void>(0x591B20, Int(i), st, (void*)S(oOut), (void*)S(oOut + 4)); return (uint64_t)(uint32_t)SAt<int32_t>(oOut) | ((uint64_t)(uint32_t)SAt<int32_t>(oOut + 4) << 32););
    } ENDROW;
    ROW("Interior_c::FindEmptyTiles", 0x591C50, 2500) {
        const int i = r.below(8), w = r.below(8) == 0 ? (int)r.below(40) - 3 : 1 + r.below(6), dp = r.below(8) == 0 ? (int)r.below(40) - 3 : 1 + r.below(6);
        Tw tw([i](Rng& rr) {
            g_ti = i;
            auto* b = At<tEffectInterior*>((uintptr_t)Int(i) + 0x14);
            if (rr.below(3)) { b->m_width = (uint8_t)(4 + rr.below(20)); b->m_depth = (uint8_t)(4 + rr.below(20)); }
            if (rr.below(3)) ClearTileRect(rr, 0, 0, 30, 30);
            SAt<int32_t>(oOut) = -7; SAt<int32_t>(oOut + 4) = -7;
        });
        IC(const bool b = Int(i)->FindEmptyTiles(w, dp, &SAt<int32_t>(oOut), &SAt<int32_t>(oOut + 4)); return (uint64_t)b | ((uint64_t)(uint32_t)SAt<int32_t>(oOut) << 8) | ((uint64_t)(uint32_t)SAt<int32_t>(oOut + 4) << 32);,
           const bool b = ExeT<uint8_t>(0x591C50, Int(i), w, dp, (void*)S(oOut), (void*)S(oOut + 4)) != 0; return (uint64_t)b | ((uint64_t)(uint32_t)SAt<int32_t>(oOut) << 8) | ((uint64_t)(uint32_t)SAt<int32_t>(oOut + 4) << 32););
    } ENDROW;
    ROW("Interior_c::AddGotoPt", 0x591D20, 3000) {
        const int i = r.below(8), tx = r.below(6) == 0 ? (int)r.below(70) - 20 : r.below(32), ty = r.below(6) == 0 ? (int)r.below(70) - 20 : r.below(32);
        const float ox = r.below(2) ? 0.f : (r.below(3) ? 0.5f : -0.5f), oy = r.below(2) ? 0.f : (r.below(3) ? 0.5f : -0.5f);
        Tw tw([i, tx, ty](Rng& rr) {
            g_ti = i;
            auto* b = At<tEffectInterior*>((uintptr_t)Int(i) + 0x14);
            if (rr.below(4)) { if (tx >= 0 && b->m_width < tx + 2) b->m_width = (uint8_t)(tx + 2); if (ty >= 0 && b->m_depth < ty + 2) b->m_depth = (uint8_t)(ty + 2); }
            if (tx >= 0 && ty >= 0 && tx < 30 && ty < 30) { static const uint8_t sts[] = { 0, 3, 7, 4, 9 }; reinterpret_cast<uint8_t*>(Int(i))[0x68 + tx * 30 + ty] = sts[rr.below(5)]; }
        });
        IC(Int(i)->AddGotoPt(tx, ty, ox, oy); return 0;, ExeT<void>(0x591D20, Int(i), tx, ty, ox, oy); return 0;);
    } ENDROW;
    ROW("Interior_c::AddInteriorInfo", 0x591E40, 3000) {
        const int i = r.below(8), act = r.below(8) == 0 ? (int)r.below(300) : 1 + r.below(10), dir = r.below(8) == 0 ? (int)r.below(8) - 2 : r.below(4) - (r.below(5) == 0 ? 1 : 0);
        const float ox = Fl(r, 30.f, 12), oy = Fl(r, 30.f, 12);
        const bool ent = r.below(2);
        Tw tw([i](Rng& rr) { g_ti = i; (void)rr; });
        IC(return B8(Int(i)->AddInteriorInfo((int32)act, ox, oy, dir, ent ? Ent(1) : nullptr));, return (uint64_t)ExeT<uint8_t>(0x591E40, Int(i), act, ox, oy, dir, (void*)(ent ? Ent(1) : nullptr)););
    } ENDROW;
}
