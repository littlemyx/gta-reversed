// ORACLE-W3-STREAM rows, part A: IplStore.cpp + TxdStore.cpp (included at the end of wave3_oracle_stream.inc, namespace w3s helpers in scope)

// the three quad-tree callbacks of IplStore.cpp (free functions, not in the header)
void SetIfIplIsRequired(const CVector2D& posn, IplDef* def);
void SetIfIplIsRequiredReducedBB(const CVector2D& posn, IplDef* def);
void SetIfInteriorIplIsRequired(const CVector2D& posn, IplDef* def);

static void TestW3sIpl() {
    using namespace w2; using namespace w3s;
    using CI = CIplStore;
    // AddIplSlot: the exe writes the whole def except the 2 padding bytes (0x32 / 0x33) and the name tail after the NUL; the port value-initialises all of it: the name areas start zeroed and the padding is masked
    SROW("CIplStore::AddIplSlot", 0x405AC0)
        const uint32_t len = r.below(18); char nm[24] = {}; for (uint32_t i = 0; i < len; ++i) nm[i] = (char)(' ' + 1 + r.below(94));
        std::string nms(nm);
        g_post = [nms](Rng& rr) {
            bool fr = false; for (uint32_t i = 0; i < g_iplCap; ++i) fr |= (SAt<uint8_t>(kIplFl + i) & 0x80) != 0;
            if (!fr) SAt<uint8_t>(kIplFl + (rr.below(g_iplCap))) |= 0x80;
            for (uint32_t i = 0; i < kNPoolSlots; ++i) std::memset(S(kIplSt + 0x34 * i + 0x10), 0, 0x12);
            std::memcpy(S(kStr), nms.c_str(), nms.size() + 1);
        };
        std::vector<uint8_t> mask(kNPoolSlots * 0x34, 0);       // the strict compare indexes the mask by the absolute byte
        for (uint32_t sl = 0; sl < kNPoolSlots; ++sl) {
            mask[sl * 0x34 + 0x32] = mask[sl * 0x34 + 0x33] = 1;
            for (uint32_t i = len + 1; i < 18; ++i) mask[sl * 0x34 + 0x10 + i] = 1;   // the name tail after the NUL: strcpy_s of the port fills it with 0xFE, the exe leaves it (nothing reads it)
        }
        std::vector<Reg> rg = { Reg{ (uintptr_t)g_sc, kIplSt }, Reg{ (uintptr_t)S(kIplSt), kNPoolSlots * 0x34, false, ~size_t(0), 0, 0x34, mask }, Reg{ (uintptr_t)S(kIplSt + kNPoolSlots * 0x34), kScSize - kIplSt - kNPoolSlots * 0x34 } };
        const char* nmp = (const char*)S(kStr);
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)CI::AddIplSlot(nmp); }, [&]() -> uint64_t { return ExeC<uint32_t, const char*>(0x405AC0, nmp); }, rg);
    SEND(600)
    SROW("CIplStore::AddIplsNeededAtPosn", 0x4045B0)
        g_post = nullptr;
        return VCase(r, d, [&]() -> uint64_t { CI::AddIplsNeededAtPosn(SAt<CVector>(kVec)); return 0; }, [&]() -> uint64_t { ExeC<void, const void*>(0x4045B0, S(kVec)); return 0; });
    SEND(200)
    SROW("CIplStore::ClearIplsNeededAtPosn", 0x4045E0)
        g_post = nullptr;
        return VCase(r, d, [&]() -> uint64_t { CI::ClearIplsNeededAtPosn(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x4045E0); return 0; });
    SEND(60)
    SROW("CIplStore::EnableDynamicStreaming", 0x404D30)
        const uint32_t en = r.below(2); int32_t slot = -1;
        g_post = [&slot](Rng& rr) { slot = UsedSlot(rr, kIplFl, g_iplCap); };
        return VCase(r, d, [&]() -> uint64_t { CI::EnableDynamicStreaming(slot, en != 0); return 0; }, [&]() -> uint64_t { ExeC<void, int32_t, uint32_t>(0x404D30, slot, en); return 0; });
    SEND(600)
    SROW("CIplStore::SetIsInterior", 0x404A90)
        const uint32_t v = r.below(2); int32_t slot = -1;
        g_post = [&slot](Rng& rr) { slot = UsedSlot(rr, kIplFl, g_iplCap); };
        return VCase(r, d, [&]() -> uint64_t { CI::SetIsInterior(slot, v != 0); return 0; }, [&]() -> uint64_t { ExeC<void, int32_t, uint32_t>(0x404A90, slot, v); return 0; });
    SEND(600)
    SROW("CIplStore::GetBoundingBox", 0x404C70)
        int32_t slot = 0; g_post = [&slot](Rng& rr) { slot = AnySlot(rr, g_iplCap); };
        return VCase(r, d, [&]() -> uint64_t { return U(CI::GetBoundingBox(slot)); }, [&]() -> uint64_t { return U(ExeC<void*, int32_t>(0x404C70, slot)); });
    SEND(600)
    SROW("CIplStore::GetIplName", 0x404A60)
        int32_t slot = 0; g_post = [&slot](Rng& rr) { slot = AnySlot(rr, g_iplCap); };
        return VCase(r, d, [&]() -> uint64_t { return U(CI::GetIplName(slot)); }, [&]() -> uint64_t { return U(ExeC<const char*, int32_t>(0x404A60, slot)); });
    SEND(600)
    SROW("CIplStore::GetIplEntityIndexArray", 0x4047B0)
        g_post = nullptr; const int32_t idx = (int32_t)r.below(40);
        return VCase(r, d, [&]() -> uint64_t { return U(CI::GetIplEntityIndexArray(idx)); }, [&]() -> uint64_t { return U(ExeC<void*, int32_t>(0x4047B0, idx)); });
    SEND(120)
    // IncludeEntity: building (type 1) / dummy (type 5) entity whose pool index (the exe: (entity - storage) / 0x38 through the pool header) covers 0..14000 and the int16 edge; the def's counters
    // start at the extremes, at plausible values or garbage
    SROW("CIplStore::IncludeEntity", 0x404C90)
        const bool bld = r.below(2); const uint32_t k = r.below(4);
        int32_t slot = -1; uint32_t idx = 0;
        g_post = [&, bld, k](Rng& rr) {
            slot = UsedSlot(rr, kIplFl, g_iplCap);
            const uint32_t m = rr.below(8); idx = m == 0 ? 0 : m == 1 ? 32767 : m == 2 ? rr.below(40) : rr.below(14000);
            SAt<uint8_t>(kEnt + 0x40 * k + 0x36) = (uint8_t)((rr.u32() & 0xF8) | (bld ? 1 : 5));
            const size_t hdr = bld ? kBHdr : kDHdr;
            SAt<uint32_t>(hdr + 0) = P(kEnt + 0x40 * k) - 0x38 * idx; SAt<uint32_t>(hdr + 4) = P(kIplFl); SAt<uint32_t>(hdr + 8) = 14000; SAt<int32_t>(hdr + 0xC) = -1;
            StaticRef<uint32_t>(bld ? 0xB74498 : 0xB744A0) = P(hdr);
            const size_t df = kIplSt + 0x34 * slot;
            const uint32_t mode = rr.below(3);
            for (int j = 0; j < 4; ++j) SAt<int16_t>(df + 0x22 + 2 * j) = mode == 0 ? (int16_t)((j & 1) ? -32768 : 32767) : mode == 1 ? (int16_t)rr.below(14000) : (int16_t)rr.u32();
        };
        CEntity* ent = reinterpret_cast<CEntity*>(S(kEnt + 0x40 * k));
        return VCase(r, d, [&]() -> uint64_t { CI::IncludeEntity(slot, ent); return 0; }, [&]() -> uint64_t { ExeC<void, int32_t, CEntity*>(0x404C90, slot, ent); return 0; });
    SEND(1200)
    // the quad-tree callbacks: a def with a random bounding box (edge values, NaN, inf) and flags, a position around its borders (+-140 / 160 tolerance edges)
    static const auto Cb = [](Rng& r, std::string& d, uint32_t addr, void (*port)(const CVector2D&, IplDef*)) -> bool {
        g_post = [](Rng& rr) {
            FillIplDef(rr, kDef);
            float c[4]; const float x0 = Fe(rr, 3000.f, 20), y0 = Fe(rr, 3000.f, 20), w = std::fabs(Fe(rr, 2000.f, 20)), h = std::fabs(Fe(rr, 2000.f, 20));
            c[0] = x0; c[1] = y0; c[2] = x0 + w; c[3] = y0 + h;
            for (int j = 0; j < 4; ++j) SAt<float>(kDef + 4 * j) = c[j];
            static const float kOff[] = { 0.f, 140.f, -140.f, 160.f, -160.f, 139.99f, 140.01f, 159.99f, 160.01f, -139.99f, -159.99f, 1e-3f, -1e-3f };
            for (int j = 0; j < 2; ++j) {
                const float e = (j == 0) ? c[rr.below(2) ? 0 : 2] : c[rr.below(2) ? 1 : 3];
                SAt<float>(kPos + 4 * j) = rr.below(8) == 0 ? Fe(rr, 3000.f, 3) : e + kOff[rr.below(13)] * (rr.below(2) ? 1.f : 0.f);
            }
        };
        return VCase(r, d, [&]() -> uint64_t { port(SAt<CVector2D>(kPos), reinterpret_cast<IplDef*>(S(kDef))); return 0; }, [&]() -> uint64_t { ExeC<void, const void*, void*>(addr, S(kPos), S(kDef)); return 0; });
    };
    SROW("SetIfIplIsRequired", 0x404660) return Cb(r, d, 0x404660, &SetIfIplIsRequired); SEND(1500)
    SROW("SetIfInteriorIplIsRequired", 0x4045F0) return Cb(r, d, 0x4045F0, &SetIfInteriorIplIsRequired); SEND(1500)
    SROW("SetIfIplIsRequiredReducedBB", 0x404690) return Cb(r, d, 0x404690, &SetIfIplIsRequiredReducedBB); SEND(1500)
}

static void TestW3sTxd() {
    using namespace w2; using namespace w3s;
    using CT = CTxdStore;
    SROW("CTxdStore::PushCurrentTxd", 0x7316A0)
        g_post = nullptr;
        return VCase(r, d, [&]() -> uint64_t { CT::PushCurrentTxd(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x7316A0); return 0; });
    SEND(120)
    SROW("CTxdStore::PopCurrentTxd", 0x7316B0)
        g_post = nullptr;
        return VCase(r, d, [&]() -> uint64_t { CT::PopCurrentTxd(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x7316B0); return 0; });
    SEND(120)
    SROW("CTxdStore::FindTxdSlot(hash)", 0x7318E0)
        uint32_t hash = 0;
        g_post = [&hash](Rng& rr) {
            const uint32_t m = rr.below(4);
            hash = m == 0 ? SAt<uint32_t>(kTxdSt + 12 * rr.below(kNPoolSlots) + 8) : m == 1 ? 0x1000u + rr.below(6) : m == 2 ? rr.u32() : 0;
        };
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)CT::FindTxdSlot((uint32)hash); }, [&]() -> uint64_t { return ExeC<uint32_t, uint32_t>(0x7318E0, hash); });
    SEND(600)
    static const auto ExeVI = [](uint32_t a, int32_t s) -> uint64_t { ExeC<void, int32_t>(a, s); return 0; };
    static const auto ExeRI = [](uint32_t a, int32_t s) -> uint64_t { return ExeC<uint32_t, int32_t>(a, s); };
    static const auto ExePI = [](uint32_t a, int32_t s) -> uint64_t { return U(ExeC<void*, int32_t>(a, s)); };
#define TXD_SLOT_ROW(NAME, ADDR, PORTEXPR, EXEFN, N)     SROW("CTxdStore::" NAME, ADDR)         int32_t slot = 0; g_post = [&slot](Rng& rr) { slot = UsedSlot(rr, kTxdFl, g_txdCap); };         return VCase(r, d, [&]() -> uint64_t { return PORTEXPR; }, [&]() -> uint64_t { return EXEFN(ADDR, slot); });     SEND(N)
    TXD_SLOT_ROW("SetCurrentTxd", 0x7319C0, (CT::SetCurrentTxd(slot), 0), ExeVI, 600)
    TXD_SLOT_ROW("AddRef", 0x731A00, (CT::AddRef(slot), 0), ExeVI, 600)
    TXD_SLOT_ROW("RemoveRefWithoutDelete", 0x731A70, (CT::RemoveRefWithoutDelete(slot), 0), ExeVI, 600)
    TXD_SLOT_ROW("GetNumRefs", 0x731AA0, (uint32_t)CT::GetNumRefs(slot), ExeRI, 600)
    TXD_SLOT_ROW("GetParentTxdSlot", 0x408370, (uint32_t)CT::GetParentTxdSlot(slot), ExeRI, 600)
    TXD_SLOT_ROW("GetTxd", 0x408340, U(CT::GetTxd(slot)), ExePI, 600)
    // SetupTxdParent: a slot with a parent slot that is used (the exe dereferences a free parent slot's null), a dictionary on both
    SROW("CTxdStore::SetupTxdParent", 0x731D50)
        int32_t slot = 0;
        g_post = [&slot](Rng& rr) {
            slot = UsedSlot(rr, kTxdFl, g_txdCap);
            const int32_t par = UsedSlot(rr, kTxdFl, g_txdCap);
            SAt<int16_t>(kTxdSt + 12 * slot + 6) = rr.below(5) != 0 ? (int16_t)par : (int16_t)-1;
            SAt<uint32_t>(kTxdSt + 12 * slot) = P(kDict + 0x40 * rr.below(8));       // the exe writes the parent plugin of this dictionary
        };
        return VCase(r, d, [&]() -> uint64_t { CT::SetupTxdParent(slot); return 0; }, [&]() -> uint64_t { ExeC<void, int32_t>(0x731D50, slot); return 0; });
    SEND(800)
    SROW("CTxdStore::TxdStoreFindCB", 0x731720)
        g_post = [](Rng& rr) { SAt<uint32_t>(kCur) = rr.below(6) ? P(kDict + 0x40 * rr.below(8)) : 0; };      // the current dictionary is a blob or null (the stub reads through it)
        return VCase(r, d, [&]() -> uint64_t { return U(CT::TxdStoreFindCB((const char*)S(kStr))); }, [&]() -> uint64_t { return U(ExeC<void*, const char*>(0x731720, (const char*)S(kStr))); });
    SEND(800)
    SROW("CTxdStore::TxdStoreLoadCB", 0x731710)
        g_post = nullptr;
        return VCase(r, d, [&]() -> uint64_t { return U(CT::TxdStoreLoadCB((const char*)S(kStr), (const char*)S(kStr + 0x20))); }, [&]() -> uint64_t { return U(ExeC<void*, const char*, const char*>(0x731710, (const char*)S(kStr), (const char*)S(kStr + 0x20))); });
    SEND(30)
}
