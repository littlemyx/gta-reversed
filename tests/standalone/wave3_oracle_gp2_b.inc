// ORACLE-W3-GP2 part b (included at the end of wave3_oracle_gp2.inc): Shopping, PedAttractor rows and the entry point.

static void TestW3Gp2Shopping() {
    using namespace w2; using namespace w3g;
    // price table (up to 300 buyable items; keys with repeats), item keys (560, with repeats), owned flags, price modifiers (up to 20), per-item stat modifiers
    constexpr uintptr_t kPrices = 0xA986F0, kNumPrices = 0xA9A7CC, kKeys = 0xA97D90, kBought = 0xA972A0, kMods = 0xA98650, kNumMods = 0xA9A7D0, kStatMods = 0xA974D0;
    static const auto Tables = [](Rng& rr) {
        BuildAll(rr);
        const uint32_t dupMode = rr.below(3);
        const uint32_t pool = dupMode == 0 ? 1000000u : dupMode == 1 ? 40u : 6u;     // key range: unique-ish / some repeats / many repeats
        for (int i = 0; i < 300; ++i) { StaticRef<uint32_t>(kPrices + 0x18 * i) = rr.below(pool) * 2654435761u; StaticRef<int32_t>(kPrices + 0x18 * i + 4) = (int32_t)rr.below(100000) - (rr.below(10) == 0 ? 50000 : 0); }
        for (int i = 0; i < 560; ++i) { StaticRef<uint32_t>(kKeys + 4 * i) = rr.below(pool) * 2654435761u; StaticRef<uint8_t>(kBought + i) = (uint8_t)rr.below(2); }
        StaticRef<int32_t>(kNumPrices) = rr.below(8) == 0 ? 300 : (int32_t)rr.below(301);
        StaticRef<int32_t>(kNumMods) = (int32_t)rr.below(21);
        for (int i = 0; i < 20; ++i) { StaticRef<uint32_t>(kMods + 8 * i) = rr.below(3) ? StaticRef<uint32_t>(kPrices + 0x18 * rr.below(300)) : rr.below(pool) * 2654435761u; StaticRef<int32_t>(kMods + 8 * i + 4) = (int32_t)rr.u32(); }
        for (int i = 0; i < 560; ++i) for (int k = 0; k < 2; ++k) {
            StaticRef<int8_t>(kStatMods + 4 * i + 2 * k) = rr.below(4) ? (int8_t)((int)rr.below(7) - 1) : (int8_t)-1;
            StaticRef<int8_t>(kStatMods + 4 * i + 2 * k + 1) = (int8_t)rr.u32();
        }
        StaticRef<uint32_t>(0xA97294) = rr.u32();
    };
    GROW("CShopping::FindItem", 0x49AD20)
        uint32_t key = 0;
        g_post = [&](Rng& rr) {
            Tables(rr); int32_t n = StaticRef<int32_t>(kNumPrices);
            if (rr.below(4) == 0) { key = rr.u32(); if (rr.below(3) == 0) key = 0; return; }     // absent key (exe: -1; n may be 0): the scan finds nothing, or a zero key in a zeroed row
            if (n < 1) { n = 1; StaticRef<int32_t>(kNumPrices) = 1; }
            key = StaticRef<uint32_t>(kPrices + 0x18 * rr.below((uint32_t)n)); };
        g_portFaultIsDiff = true;   // absent key: the exe returns -1, the port must not reach NOTSA_UNREACHABLE (unresolved symbol in this link = fault at 0x400000)
        const bool ok = VCase(r, d, [&]() -> uint64_t { return (uint32_t)CShopping::FindItem(key); }, [&]() -> uint64_t { return ExeC<uint32_t, uint32_t>(0x49AD20, key); });
        g_portFaultIsDiff = false; return ok;
    GEND(2000)
    GROW("CShopping::GetItemIndex", 0x49AB10)
        uint32_t key = 0;
        g_post = [&](Rng& rr) { Tables(rr); key = rr.below(4) == 0 ? rr.u32() : StaticRef<uint32_t>(kKeys + 4 * rr.below(560)); };     // 1 in 4: a random key, absent (exe: -1)
        g_portFaultIsDiff = true;   // absent key: the exe returns -1, the port must not reach NOTSA_UNREACHABLE (unresolved symbol in this link = fault at 0x400000)
        const bool ok = VCase(r, d, [&]() -> uint64_t { return (uint32_t)CShopping::GetItemIndex(key); }, [&]() -> uint64_t { return ExeC<uint32_t, uint32_t>(0x49AB10, key); });
        g_portFaultIsDiff = false; return ok;
    GEND(2000)
    GROW("CShopping::SetPlayerHasBought", 0x49B610)
        uint32_t key = 0;
        g_post = [&](Rng& rr) { Tables(rr); key = StaticRef<uint32_t>(kKeys + 4 * rr.below(560)); };
        return VCase(r, d, [&]() -> uint64_t { CShopping::SetPlayerHasBought(key); return 0; }, [&]() -> uint64_t { ExeC<void, uint32_t>(0x49B610, key); return 0; });
    GEND(1000)
    GROW("CShopping::HasPlayerBought", 0x49B5E0)
        uint32_t key = 0;
        g_post = [&](Rng& rr) { Tables(rr); key = StaticRef<uint32_t>(kKeys + 4 * rr.below(560)); };
        return VCase(r, d, [&]() -> uint64_t { return (uint8_t)CShopping::HasPlayerBought(key); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, uint32_t>(0x49B5E0, key); });
    GEND(1000)
    // modifiers: the key is an existing modifier, an existing price key or unknown; a full table (20) is not appended to (the exe has no bound check)
    static const auto Key = [](Rng& rr) -> uint32_t {
        const uint32_t k = rr.below(4);
        if (k == 0 && StaticRef<int32_t>(kNumMods) > 0) return StaticRef<uint32_t>(kMods + 8 * rr.below((uint32_t)StaticRef<int32_t>(kNumMods)));
        if (k == 1 && StaticRef<int32_t>(kNumPrices) > 0) return StaticRef<uint32_t>(kPrices + 0x18 * rr.below((uint32_t)StaticRef<int32_t>(kNumPrices)));
        return rr.u32();
    };
    GROW("CShopping::AddPriceModifier(key)", 0x49BDD0)
        uint32_t key = 0; const int32_t price = (int32_t)r.u32();
        g_post = [&](Rng& rr) { Tables(rr); key = Key(rr); if (StaticRef<int32_t>(kNumMods) == 20) { bool has = false; for (int i = 0; i < 20; ++i) has |= StaticRef<uint32_t>(kMods + 8 * i) == key; if (!has) StaticRef<int32_t>(kNumMods) = 19; } };
        return VCase(r, d, [&]() -> uint64_t { CShopping::AddPriceModifier(key, price); return 0; }, [&]() -> uint64_t { ExeC<void, uint32_t, int32_t>(0x49BDD0, key, price); return 0; });
    GEND(3000)
    GROW("CShopping::RemovePriceModifier(key)", 0x49ACD0)
        uint32_t key = 0;
        g_post = [&](Rng& rr) { Tables(rr); key = Key(rr); };
        return VCase(r, d, [&]() -> uint64_t { CShopping::RemovePriceModifier(key); return 0; }, [&]() -> uint64_t { ExeC<void, uint32_t>(0x49ACD0, key); return 0; });
    GEND(3000)
    GROW("CShopping::SetCurrentProperty", 0x49B1F0)
        // the cell 0xA97294: in this (address-globals) build the port's gpCurrentProperty is a TU-local variable (static inline), not the exe's cell: the port side only runs the call (it must not crash / touch the image) and
        // the exe's store is checked against the argument (the cell is restored after)
        g_post = Tables; const uint32_t p = r.u32();
        return VCase(r, d, [&]() -> uint64_t { CShopping::SetCurrentProperty((CMultiBuilding*)(uintptr_t)p); return p; },
            [&]() -> uint64_t { const uint32_t old = StaticRef<uint32_t>(0xA97294); ExeC<void, uint32_t>(0x49B1F0, p); const uint32_t v = StaticRef<uint32_t>(0xA97294); StaticRef<uint32_t>(0xA97294) = old; return v; });
    GEND(60)
    // stat modifiers: index 0..5 of the stat list or -1 (none), changes -128..127; ModifyStat is a recording stub (stat id, change as float) on both sides
    GROW("CShopping::UpdateStats", 0x49BEF0)
        g_post = Tables; const uint32_t idx = r.below(560); const bool inc = r.below(2);
        return VCase(r, d, [&]() -> uint64_t { CShopping::UpdateStats(idx, inc); return 0; }, [&]() -> uint64_t { ExeC<void, uint32_t, uint32_t>(0x49BEF0, idx, inc); return 0; });
    GEND(2000)
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// ped attractor: raw object (exe layout: vtable 0, effect 4, entity 8, attract peds vector 0xC/+4 first/+8 last, arrived peds 0x1C/+0x20/+0x24, task pairs 0x2C/+0x30/+0x34, max peds 0x3C)
static_assert(sizeof(CPedAttractor) <= 0x100 && sizeof(CPedTaskPair) == 0x14, "fixture blob sizes");
static void TestW3Gp2PedAttractor() {
    using namespace w2; using namespace w3g;
    static const auto A = []() { return reinterpret_cast<CPedAttractor*>(g_sc + kAttr); };
    static const auto Ped = [](uint32_t k) { return reinterpret_cast<CPed*>(0x7000u + 0x100u * k); };      // fake ped pointers: only compared
    // peds: kPeds+0..0x40 attract, +0x40..0x80 queue (each 16 slots of 4 bytes); vector capacity 16, `n` used; unused slots hold stale ped pointers
    static const auto Fill = [](Rng& rr) {
        BuildAll(rr);
        FillGarbage(rr, S(kAttr), 0x100);
        const uint32_t nPed = 2 + rr.below(6);
        auto* a = A();
        const auto SetVec = [&](SArray<CPed*>& v, size_t off, int n) {
            for (int i = 0; i < 16; ++i) SAt<uint32_t>(off + 4 * i) = (uint32_t)(uintptr_t)Ped(rr.below(nPed));
            v.allocator = nullptr; v._First = reinterpret_cast<CPed**>(S(off)); v._Last = v._First + n; v._End = v._First + 16;
        };
        SetVec(a->m_AttractPeds, kPeds, (int)rr.below(7)); SetVec(a->m_ArrivedPeds, kPeds + 0x40, (int)rr.below(7));
        if (rr.below(10) == 0) { a->m_AttractPeds._First = a->m_AttractPeds._Last = a->m_AttractPeds._End = nullptr; }              // never-allocated vector
        a->m_MaxNumPeds = rr.below(3) ? rr.below(14) : rr.below(100);
        a->m_AchieveQueueTime = Fe(rr, 10.f, 6); a->m_AchieveQueueShuffleTime = Fe(rr, 10.f, 6);
    };
    static const auto NullQueue = [](Rng& rr) { Fill(rr); if (rr.below(8) == 0) A()->m_ArrivedPeds._First = A()->m_ArrivedPeds._Last = A()->m_ArrivedPeds._End = nullptr; };
    GROW("CPedAttractor::IsRegisteredWithPed", 0x5EB4C0)
        g_post = Fill; const uint32_t p = r.below(8);
        return VCase(r, d, [&]() -> uint64_t { return (uint8_t)A()->IsRegisteredWithPed(Ped(p)); }, [&]() -> uint64_t { return (uint8_t)ExeT<uint32_t, void*>(0x5EB4C0, A(), Ped(p)); });
    GEND(3000)
    GROW("CPedAttractor::IsAtHeadOfQueue", 0x5EB530)
        const uint32_t p = r.below(8);
        g_post = [&](Rng& rr) { Fill(rr); if (rr.below(2)) A()->m_ArrivedPeds._Last = A()->m_ArrivedPeds._First; };      // empty queue: slot 0 still holds a stale ped
        return VCase(r, d, [&]() -> uint64_t { return (uint8_t)A()->CPedAttractor::IsAtHeadOfQueue(Ped(p)); }, [&]() -> uint64_t { return (uint8_t)ExeT<uint32_t, void*>(0x5EB530, A(), Ped(p)); });
    GEND(3000)
    GROW("CPedAttractor::GetQueueSlot", 0x5EB550)
        g_post = Fill; const uint32_t p = r.below(8);
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)A()->GetQueueSlot(Ped(p)); }, [&]() -> uint64_t { return ExeT<uint32_t, void*>(0x5EB550, A(), Ped(p)); });
    GEND(3000)
    GROW("CPedAttractor::GetHeadOfQueue", 0x5EB590)
        g_post = NullQueue;
        return VCase(r, d, [&]() -> uint64_t { return U(A()->GetHeadOfQueue()); }, [&]() -> uint64_t { return U(ExeT<void*>(0x5EB590, A())); });
    GEND(1500)
    GROW("CPedAttractor::GetTailOfQueue", 0x5EB5B0)
        g_post = NullQueue;
        return VCase(r, d, [&]() -> uint64_t { return U(A()->GetTailOfQueue()); }, [&]() -> uint64_t { return U(ExeT<void*>(0x5EB5B0, A())); });
    GEND(1500)
    GROW("CPedAttractor::HasEmptySlot", 0x5EAF10)
        g_post = NullQueue;
        return VCase(r, d, [&]() -> uint64_t { return (uint8_t)A()->HasEmptySlot(); }, [&]() -> uint64_t { return (uint8_t)ExeT<uint32_t>(0x5EAF10, A()); });
    GEND(2000)
    GROW("CPedAttractor::ComputeAttractTime", 0x5E95E0)
        g_post = Fill; const int32_t slot = (int32_t)r.u32(); const bool arrived = r.below(2);
        return VCase(r, d, [&]() -> uint64_t { A()->ComputeAttractTime(slot, arrived, SAt<float>(kOutA)); return 0; }, [&]() -> uint64_t { ExeT<void, int32_t, uint32_t, void*>(0x5E95E0, A(), slot, arrived, S(kOutA)); return 0; });
    GEND(300)
    // pending tasks of the peds: pair table (ped, task, slot, id, used) over the same fake peds; the task list ms_tasks (0xC0985C, an SArray of CTask*) holds every task of an unused pair
    GROW("CPedAttractor::GetTaskForPed", 0x5EC500)
        const uint32_t p = r.below(8);
        g_post = [&](Rng& rr) {
            Fill(rr);
            auto* a = A();
            const int n = (int)rr.below(8);
            for (int i = 0; i < 8; ++i) {
                auto* pr = reinterpret_cast<uint32_t*>(S(kPairs + 0x14 * i));
                pr[0] = (uint32_t)(uintptr_t)Ped(rr.below(8)); pr[1] = 0x9000u + 0x40u * i; pr[2] = rr.u32() & 3; pr[3] = rr.u32() & 0xFF; pr[4] = rr.below(2);
            }
            a->m_PedTaskPairs.allocator = nullptr; a->m_PedTaskPairs._First = reinterpret_cast<CPedTaskPair*>(S(kPairs)); a->m_PedTaskPairs._Last = a->m_PedTaskPairs._First + n; a->m_PedTaskPairs._End = a->m_PedTaskPairs._First + 8;
            // ms_tasks: the tasks of the unused pairs (in a random order) plus extras
            uint32_t* t = reinterpret_cast<uint32_t*>(S(kTasks)); int m = 0;
            for (int i = 0; i < n; ++i) if (!*reinterpret_cast<uint8_t*>(S(kPairs + 0x14 * i + 0x10))) t[m++] = 0x9000u + 0x40u * i;
            for (int i = m - 1; i > 0; --i) std::swap(t[i], t[rr.below((uint32_t)i + 1)]);
            const int extra = (int)rr.below(3); for (int i = 0; i < extra; ++i) t[m++] = 0xA000u + 0x40u * i;
            auto& ms = StaticRef<SArray<CTask*>>(0xC0985C);
            ms.allocator = nullptr; ms._First = reinterpret_cast<CTask**>(t); ms._Last = ms._First + m; ms._End = ms._First + 16;
        };
        return VCase(r, d, [&]() -> uint64_t { return U(A()->GetTaskForPed(Ped(p))); }, [&]() -> uint64_t { return U(ExeT<void*, void*>(0x5EC500, A(), Ped(p))); });
    GEND(3000)
}

static void TestW3Gp2All() {
    using namespace w3g;
    (void)spdlog::default_logger_raw();
    spdlog::set_level(spdlog::level::off);
    InstallStubs();
    TestW3Gp2TrafficLights();
    TestW3Gp2Restart();
    TestW3Gp2Shopping();
    TestW3Gp2PedAttractor();
}
