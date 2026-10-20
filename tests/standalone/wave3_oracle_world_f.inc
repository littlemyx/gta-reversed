// ORACLE-W3-PATHS rows F: Shadows.cpp tables and colour (see wave3_oracle_world_e.inc). Included from wave3_oracle_world_d.inc.
static void TestW3WorldShadows() {
    using namespace w2; using namespace w3w; using namespace w3e;
    const auto FakeTex = [](Rng& rr) { return reinterpret_cast<RwTexture*>(S(oBuf + 0x100 * (1 + rr.below(12)))); };
    // ---- permanent shadows
    ROW("CShadows::TidyUpShadows", 0x707770, 100) {
        Tw tw([](Rng& rr) { FillGarbage(rr, (uintptr_t)&CShadows::aPermanentShadows, sizeof(CShadows::aPermanentShadows)); });
        return w3w::WCase(r, d, [&]() -> uint64_t { CShadows::TidyUpShadows(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x707770)(); return 0; });
    } ENDROW;
    ROW("CShadows::RemoveOilInArea", 0x7074F0, 1000) {
        const float minX = r.below(10) ? GF(r, 800.f) : Fl(r, 800.f, 1), maxX = r.below(10) ? minX + r.f01() * 1500.f : Fl(r, 800.f, 1), minY = r.below(10) ? GF(r, 800.f) : Fl(r, 800.f, 1), maxY = r.below(10) ? minY + r.f01() * 1500.f : Fl(r, 800.f, 1);
        Tw tw([minX, maxX, minY, maxY](Rng& rr) {
            FillGarbage(rr, (uintptr_t)&CShadows::aPermanentShadows, sizeof(CShadows::aPermanentShadows));
            for (auto& sh : CShadows::aPermanentShadows) {
                sh.m_nType = (eShadowType)(rr.below(10) == 0 ? rr.u32() : rr.below(9));
                const bool inside = rr.below(2);
                sh.m_vecPosn = inside ? CVector(minX + rr.f01() * (maxX - minX), minY + rr.f01() * (maxY - minY), GF(rr, 30.f)) : CVector(GF(rr, 1500.f), GF(rr, 1500.f), GF(rr, 30.f));
                if (rr.below(12) == 0) sh.m_vecPosn.x = rr.below(2) ? minX : maxX;
                if (rr.below(12) == 0) sh.m_vecPosn.y = rr.below(2) ? minY : maxY;
            }
        });
        return w3w::WCase(r, d, [&]() -> uint64_t { CShadows::RemoveOilInArea(minX, maxX, minY, maxY); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(float, float, float, float)>(0x7074F0)(minX, maxX, minY, maxY); return 0; });
    } ENDROW;
    ROW("CShadows::PrintDebugPoly", 0x7076B0, 20) {
        return w3w::WCase(r, d, [&]() -> uint64_t { CShadows::PrintDebugPoly(nullptr, nullptr, nullptr); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, void*, void*)>(0x7076B0)(nullptr, nullptr, nullptr); return 0; });
    } ENDROW;
    // ---- stored shadows
    ROW("CShadows::StoreShadowToBeRendered-Texture", 0x707390, 1500) {
        const uint32_t type = r.below(10), stored = r.below(8) == 0 ? 44 + r.below(8) : r.below(48);
        RwTexture* tex = FakeTex(r); const CVector pos(GF(r, 3000.f), GF(r, 3000.f), GF(r, 100.f));
        const float f[4] = { Fl(r, 9.f, 10), Fl(r, 9.f, 10), Fl(r, 9.f, 10), Fl(r, 9.f, 10) };
        const uint32_t inten = r.u32(), cr = r.u32(), cg = r.u32(), cb = r.u32();
        const float zd = Fl(r, 30.f, 10), sc = Fl(r, 3.f, 10);
        const uint32_t water = r.u32(), bld = r.u32();
        CRealTimeShadow* rts = reinterpret_cast<CRealTimeShadow*>(S(oBuf + 0x2000));
        Tw tw([stored](Rng& rr) { FillGarbage(rr, (uintptr_t)&CShadows::asShadowsStored, sizeof(CShadows::asShadowsStored)); CShadows::ShadowsStoredToBeRendered = (uint16)stored; });
        return w3w::WCase(r, d, [&]() -> uint64_t {
            CShadows::StoreShadowToBeRendered((uint8)type, tex, pos, f[0], f[1], f[2], f[3], (int16)inten, (uint8)cr, (uint8)cg, (uint8)cb, zd, (bool)(water & 1), sc, rts, (bool)(bld & 1));
            return CShadows::ShadowsStoredToBeRendered;
        }, [&]() -> uint64_t {
            using F = void __cdecl(uint32_t, RwTexture*, const CVector*, float, float, float, float, uint32_t, uint32_t, uint32_t, uint32_t, float, uint32_t, float, void*, uint32_t);
            oracle::Fn<F>(0x707390)((uint32_t)(uint8)type, tex, &pos, f[0], f[1], f[2], f[3], (uint32_t)(int32_t)(int16)inten, (uint32_t)(uint8)cr, (uint32_t)(uint8)cg, (uint32_t)(uint8)cb, zd, (uint32_t)(water & 1), sc, rts, (uint32_t)(bld & 1));
            return CShadows::ShadowsStoredToBeRendered;
        });
    } ENDROW;
    ROW("CShadows::StoreShadowToBeRendered-Type", 0x707930, 1500) {
        const uint32_t type = r.below(10), stored = r.below(8) == 0 ? 44 + r.below(8) : r.below(48);
        const CVector pos(GF(r, 3000.f), GF(r, 3000.f), GF(r, 100.f));
        const float f[4] = { Fl(r, 9.f, 10), Fl(r, 9.f, 10), Fl(r, 9.f, 10), Fl(r, 9.f, 10) };
        const uint32_t inten = r.u32(), cr = r.u32(), cg = r.u32(), cb = r.u32();
        Tw tw([stored](Rng& rr) {
            FillGarbage(rr, (uintptr_t)&CShadows::asShadowsStored, sizeof(CShadows::asShadowsStored)); CShadows::ShadowsStoredToBeRendered = (uint16)stored;
            RwTexture** texs[] = { &gpShadowCarTex, &gpShadowPedTex, &gpShadowHeliTex, &gpShadowBikeTex, &gpShadowBaronTex, &gpShadowExplosionTex, &gpShadowHeadLightsTex, &gpShadowHeadLightsTex2, &gpBloodPoolTex, &gpHandManTex, &gpCrackedGlassTex, &gpPostShadowTex };
            int k = 1; for (auto t : texs) *t = reinterpret_cast<RwTexture*>(S(oBuf + 0x100 * k++));
        });
        return w3w::WCase(r, d, [&]() -> uint64_t {
            CShadows::StoreShadowToBeRendered((uint8)type, pos, f[0], f[1], f[2], f[3], (int16)inten, (uint8)cr, (uint8)cg, (uint8)cb);
            return CShadows::ShadowsStoredToBeRendered;
        }, [&]() -> uint64_t {
            using F = void __cdecl(uint32_t, const CVector*, float, float, float, float, uint32_t, uint32_t, uint32_t, uint32_t);
            oracle::Fn<F>(0x707930)((uint32_t)(uint8)type, &pos, f[0], f[1], f[2], f[3], (uint32_t)(int32_t)(int16)inten, (uint32_t)(uint8)cr, (uint32_t)(uint8)cg, (uint32_t)(uint8)cb);
            return CShadows::ShadowsStoredToBeRendered;
        });
    } ENDROW;
    // ---- lighting colour
    ROW("CShadows::AffectColourWithLighting", 0x707850, 3000) {
        const uint32_t type = r.below(10), dn = r.u32(), cr = r.u32(), cg = r.u32(), cb = r.u32();
        const float bal = r.below(10) == 0 ? Fl(r, 2.f, 2) : r.f01();
        Tw tw([bal](Rng& rr) { CCustomBuildingDNPipeline::m_fDNBalanceParam = bal; });
        return w3w::WCase(r, d, [&]() -> uint64_t {
            uint8 o[3] = { 0xAA, 0xBB, 0xCC };
            CShadows::AffectColourWithLighting((eShadowType)(uint8)type, (uint8)dn, (uint8)cr, (uint8)cg, (uint8)cb, o[0], o[1], o[2]);
            return o[0] | o[1] << 8 | o[2] << 16;
        }, [&]() -> uint64_t {
            uint8 o[3] = { 0xAA, 0xBB, 0xCC };
            oracle::Fn<void __cdecl(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint8*, uint8*, uint8*)>(0x707850)((uint32_t)(uint8)type, (uint32_t)(uint8)dn, (uint32_t)(uint8)cr, (uint32_t)(uint8)cg, (uint32_t)(uint8)cb, &o[0], &o[1], &o[2]);
            return o[0] | o[1] << 8 | o[2] << 16;
        });
    } ENDROW;
    // ---- static shadows: each owns a disjoint chain of poly bunches, the empty list holds the rest
    const auto FixStatic = [](Rng& rr) {
        FillGarbage(rr, (uintptr_t)&CShadows::aStaticShadows, sizeof(CShadows::aStaticShadows));
        FillGarbage(rr, (uintptr_t)&CShadows::aPolyBunches, sizeof(CShadows::aPolyBunches));
        auto& pool = CShadows::aPolyBunches;
        size_t next = 0;
        const auto Chain = [&](size_t len) -> CPolyBunch* {
            if (!len || next + len > pool.size()) return nullptr;
            for (size_t i = 0; i < len; ++i) pool[next + i].m_pNext = i + 1 < len ? &pool[next + i + 1] : nullptr;
            CPolyBunch* head = &pool[next]; next += len; return head;
        };
        for (auto& sh : CShadows::aStaticShadows) sh.m_pPolyBunch = rr.below(4) == 0 ? nullptr : Chain(1 + rr.below(6));
        CShadows::pEmptyBunchList = rr.below(5) == 0 ? nullptr : Chain(1 + rr.below(30));
    };
    ROW("CStaticShadow::Free", 0x707670, 600) {
        const int k = r.below(48);
        Tw tw(FixStatic);
        return w3w::WCase(r, d, [&]() -> uint64_t { CShadows::aStaticShadows[k].Free(); return U(CShadows::pEmptyBunchList); },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(CStaticShadow*, int)>(0x707670)(&CShadows::aStaticShadows[k], 0); return U(CShadows::pEmptyBunchList); });
    } ENDROW;
    ROW("CShadows::UpdateStaticShadows", 0x707F40, 600) {
        const uint32_t now = r.u32() >> 4;
        Tw tw([now, FixStatic](Rng& rr) {
            FixStatic(rr);
            CTimer::m_snTimeInMilliseconds = now;
            for (auto& sh : CShadows::aStaticShadows) {
                sh.m_bJustCreated = rr.below(3) == 0; sh.m_bTemporaryShadow = rr.below(2);
                sh.m_nTimeCreated = rr.below(3) == 0 ? rr.u32() >> 4 : now - 6000u + rr.below(12000u);
            }
        });
        return w3w::WCase(r, d, [&]() -> uint64_t { CShadows::UpdateStaticShadows(); return U(CShadows::pEmptyBunchList); },
            [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x707F40)(); return U(CShadows::pEmptyBunchList); });
    } ENDROW;
}
