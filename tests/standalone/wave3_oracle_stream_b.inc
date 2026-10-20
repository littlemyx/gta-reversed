// ORACLE-W3-STREAM rows, part B: Streaming.cpp (included at the end of wave3_oracle_stream.inc; the w3s fixture helpers are in scope)
#include "Streaming.h"
#include "Population.h"
#include "Models/VehicleModelInfo.h"
#include "LoadedCarGroup.h"

// CVehicleModelInfo::ms_linkedUpgrades.FindOtherUpgrade (exe 0x4C74D0, __thiscall) is Patch()ed to the same host stub as the port's member: answers from a table in the fixture block
namespace w3s {
constexpr size_t kLink = 0xC00 /*12 x 0x10*/, kMiB = 0xD00 /*24 x 0x60*/, kZone = 0x1600 /*0x40*/, kUpg = 0x1640 /*256 x 2*/, kMiV = 0x1900 /*24 x 0x60*/;
static int16_t UpgBody(int32_t id) { return SAt<int16_t>(kUpg + 2 * (id & 0xFF)); }
static int16_t __fastcall StubUpg(void*, void*, int32_t id) { return UpgBody(id); }
static void InstallStubsB() {
    static bool done = false; if (done) return; done = true;
    oracle::Patch(0x4C74D0, (void*)&StubUpg);
}
static uint8_t* Inf(int m) { return reinterpret_cast<uint8_t*>(0x8E4CC0 + 20u * (uint32_t)m); }
constexpr int kHead = 26312, kTail = 26313;
static int g_cand[64]; static int g_nCand = 0, g_nDff = 0;
static int g_listN = 0;
// request list, channels, model infos (DFF candidates), txd parents: the streaming world around the TXD pool fixture of BuildAll
static void BuildStreamFx(Rng& rr) {
    StaticRef<uint32_t>(0x9654B4) = 0x8E4CC0;
    g_nCand = 0; g_nDff = 0;
    for (int j = 0; j < 20; ++j) {                                        // DFF models with fake model infos (m_nTxdIndex = +0xA)
        int id; bool dup;
        do { id = rr.below(3) ? (int)rr.below(400) : (int)rr.below(20000); dup = false; for (int k = 0; k < g_nCand; ++k) dup |= g_cand[k] == id; } while (dup);
        g_cand[g_nCand++] = id; ++g_nDff;
        uint8_t* mi = S(kMiB + 0x60 * j);
        FillGarbage(rr, mi, 0x60);
        SAt<int16_t>(kMiB + 0x60 * j + 0xA) = rr.below(4) ? (int16_t)rr.below(g_txdCap) : (int16_t)rr.u32();
        StaticRef<uint32_t>(0xA9B0C8 + 4 * id) = P(kMiB + 0x60 * j);
    }
    for (int j = 0; j < 8; ++j) {                                         // TXD models (20000 + slot) and others (25000 +)
        const int id = j < 5 ? 20000 + (int)rr.below(g_txdCap) : 25000 + (int)rr.below(1000);
        if (j < 5) SAt<uint8_t>(kTxdFl + (id - 20000)) &= 0x7F;
        bool dup = false; for (int k = 0; k < g_nCand; ++k) dup |= g_cand[k] == id;
        if (!dup) g_cand[g_nCand++] = id;
    }
    // request list head / tail sentinels and n members in random order
    int perm[64]; for (int i = 0; i < g_nCand; ++i) perm[i] = g_cand[i];
    for (int i = g_nCand - 1; i > 0; --i) std::swap(perm[i], perm[rr.below(i + 1)]);
    g_listN = (int)rr.below(13); if (g_listN > g_nCand) g_listN = g_nCand;
    int prev = kHead;
    *reinterpret_cast<int16_t*>(Inf(kHead) + 2) = -1;
    for (int i = 0; i < g_listN; ++i) {
        const int m = perm[i];
        *reinterpret_cast<int16_t*>(Inf(prev) + 0) = (int16_t)m;
        *reinterpret_cast<int16_t*>(Inf(m) + 2) = (int16_t)prev;
        prev = m;
    }
    *reinterpret_cast<int16_t*>(Inf(prev) + 0) = (int16_t)kTail;
    *reinterpret_cast<int16_t*>(Inf(kTail) + 2) = (int16_t)prev;
    *reinterpret_cast<int16_t*>(Inf(kTail) + 0) = -1;
    StaticRef<uint32_t>(0x8E4C58) = 0x8E4CC0 + 20u * kHead;
    StaticRef<uint32_t>(0x8E4C54) = 0x8E4CC0 + 20u * kTail;
    for (int c = 0; c < 2; ++c) for (int k = 0; k < 16; ++k)              // loading channels
        StaticRef<int32_t>(0x8E4A60 + 0x98 * c + 4 * k) = rr.below(2) ? -1 : g_cand[rr.below(g_nCand)];
}
static int RandCand(Rng& rr) { return g_cand[rr.below(g_nCand)]; }
// FindOtherUpgrade table
static void BuildUpg(Rng& rr) {
    for (int i = 0; i < 256; ++i) SAt<int16_t>(kUpg + 2 * i) = rr.below(3) == 0 ? (int16_t)-1 : rr.below(2) ? (int16_t)rr.below(300) : (int16_t)rr.below(26316);
}
static void SetLoadState(Rng& rr, int m) { Inf(m)[0x10] = rr.below(3) ? (uint8_t)(rr.below(2) ? 1 : rr.below(5)) : (uint8_t)rr.below(256); }
// the population world: zone info, perc table, car groups, loaded groups, cheats
static void BuildPopFx(Rng& rr) {
    StaticRef<uint32_t>(0xC0BC68) = rr.below(8) == 0 ? 0 : P(kZone);
    FillGarbage(rr, S(kZone), 0x40);
    SAt<uint8_t>(kZone + 0xF) = (uint8_t)((rr.u32() & ~0x1Fu) | rr.below(20));
    for (int g = 0; g < 10; ++g) SAt<uint8_t>(kZone + g) = rr.below(2) ? 0 : (uint8_t)rr.below(256);
    StaticRef<int32_t>(0xC0BC70) = (int32_t)rr.below(2); StaticRef<int32_t>(0xC0BC74) = (int32_t)rr.below(12);
    for (int i = 0; i < 8640; ++i) StaticRef<uint8_t>(0xC0BC78 + i) = rr.below(3) ? 0 : (uint8_t)rr.below(256);
    for (int g = 0; g < 36; ++g) {                                         // car groups: counts and models (the model ids come from one small set so the lookups hit)
        StaticRef<uint16_t>(0xC0EC78 + 2 * g) = rr.below(5) ? (uint16_t)rr.below(8) : (uint16_t)rr.below(24);
        for (int k = 0; k < 23; ++k) StaticRef<int16_t>(0xC0ED38 + 46 * g + 2 * k) = (int16_t)(400 + rr.below(24));
    }
    static const uint32_t kCheat[6] = { 0x969159, 0x96917B, 0x96915E, 0x96915F, 0x96915C, 0x969176 };
    for (auto a : kCheat) StaticRef<uint8_t>(a) = 0;
    if (rr.below(3) == 0) StaticRef<uint8_t>(kCheat[rr.below(6)]) = (uint8_t)(1 + rr.below(2) * 0);
    // loaded groups: boats, inappropriate, appropriate, 11 gang groups (0xC0EA28 + 46 g), 0x8E4C24
    static const uint32_t kGrp0[4] = { 0xC0E998, 0xC0E9C8, 0xC0E9F8, 0x8E4C24 };
    for (int g = 0; g < 4 + 11; ++g) {
        const uint32_t a = g < 4 ? kGrp0[g] : 0xC0EA28 + 46 * (g - 4);
        const uint32_t k = rr.below(4) == 0 ? rr.below(24) : rr.below(8);
        for (uint32_t i = 0; i < 23; ++i) StaticRef<int16_t>(a + 2 * i) = i < k ? (int16_t)(400 + rr.below(24)) : (int16_t)-1;
    }
    for (int j = 0; j < 24; ++j) {                                          // vehicle model infos 400..423: m_nTimesUsed at +0x50
        FillGarbage(rr, S(kMiV + 0x60 * j), 0x60);
        StaticRef<uint32_t>(0xA9B0C8 + 4 * (400 + j)) = P(kMiV + 0x60 * j);
    }
}
static int PopModel(Rng& rr) { return rr.below(8) == 0 ? (int)rr.below(500) : (int)(400 + rr.below(24)); }
} // namespace w3s

int16 CVehicleModelInfo::CLinkedUpgradeList::FindOtherUpgrade(int16 upgrade) { return w3s::UpgBody(upgrade); }

static void TestW3sStreaming() {
    using namespace w2; using namespace w3s;
    using CS = CStreaming;
    SROW("CStreaming::AreTexturesUsedByRequestedModels", 0x409A90)
        int32_t arg = 0;
        g_post = [&arg](Rng& rr) {
            BuildStreamFx(rr);
            const uint32_t m = rr.below(10);
            if (m < 5) arg = SAt<int16_t>(kMiB + 0x60 * rr.below(g_nDff) + 0xA);                         // the txd index of a DFF model (maybe in the list)
            else if (m < 8) arg = SAt<int16_t>(kTxdSt + 12 * rr.below(kNPoolSlots) + 6);                // the parent of a txd slot
            else arg = (int32_t)rr.below(g_txdCap + 2);
        };
        return VCase(r, d, [&]() -> uint64_t { return CS::AreTexturesUsedByRequestedModels(arg); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, int32_t>(0x409A90, arg); });
    SEND(3000)
    SROW("CStreaming::ClearFlagForAll", 0x407A40)
        uint32_t fl = r.below(3) == 0 ? r.u32() : (1u << r.below(8));
        g_post = [](Rng& rr) { for (int m = 0; m < 26316; ++m) Inf(m)[6] = (uint8_t)rr.u32(); };
        return VCase(r, d, [&]() -> uint64_t { CS::ClearFlagForAll(fl); return 0; }, [&]() -> uint64_t { ExeC<void, uint32_t>(0x407A40, fl); return 0; });
    SEND(60)
    SROW("CStreaming::HasSpecialCharLoaded", 0x407F00)
        int32_t slot = 0;
        g_post = [&slot](Rng& rr) { slot = (int32_t)rr.below(60); for (int k = 0; k < 60; ++k) SetLoadState(rr, 290 + k); };
        return VCase(r, d, [&]() -> uint64_t { return CS::HasSpecialCharLoaded(slot); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, int32_t>(0x407F00, slot); });
    SEND(600)
    SROW("CStreaming::HasVehicleUpgradeLoaded", 0x407820)
        int32_t id = 0;
        g_post = [&id](Rng& rr) {
            BuildUpg(rr);
            id = rr.below(2) ? (int32_t)rr.below(300) : (int32_t)rr.below(26316);
            SetLoadState(rr, id); if (rr.below(2)) SetLoadState(rr, id);
            for (int i = 0; i < 256; ++i) { const int o = UpgBody(i); if (o >= 0) { if (rr.below(3)) SetLoadState(rr, o); } }
            Inf(id)[0x10] = rr.below(3) ? 1 : Inf(id)[0x10];
        };
        return VCase(r, d, [&]() -> uint64_t { return CS::HasVehicleUpgradeLoaded(id); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, int32_t>(0x407820, id); });
    SEND(1000)
    SROW("CStreaming::FindMIPedSlotForInterior", 0x407FB0)
        int32_t rf = 0;
        g_post = [&rf](Rng& rr) {
            rf = rr.below(4) == 0 ? (int32_t)(rr.u32() & 0x7FFFFFF0u) : (int32_t)rr.below(30);   // >= 0 (a negative sum indexes before the array: std::array asserts)
            for (int k = 0; k < 8; ++k) {
                const int32_t m = rr.below(3) == 0 ? -1 : rr.below(30) == 0 ? (int32_t)rr.below(26316) : (int32_t)rr.below(300);
                StaticRef<int32_t>(0x8E4C00 + 4 * k) = m;
                if (m >= 0) SetLoadState(rr, m);
            }
        };
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)CS::FindMIPedSlotForInterior(rf); }, [&]() -> uint64_t { return ExeC<uint32_t, int32_t>(0x407FB0, rf); });
    SEND(800)
    // RenderEntity / RemoveEntity: the used list of the RW object instances (sentinels in the fixture block, items kLink + 0x10 * i, CLink = { data, prev (+4), next (+8) }), the free list sentinel
    // at the exe's ms_rwObjectInstances + 0x18 (0x965508), ms_renderEntityLink (0x8E48A0) = an item of the used list
    static const auto BuildLinks = [](Rng& rr, int& link, bool wantItem) {
        constexpr int kN = 6;
        auto L = [](int i) -> uint8_t* { return S(kLink + 0x10 * i); };           // 0 = head sentinel, 1..kN = items, kN+1 = tail sentinel, kN+2, kN+3 = free items
        auto Link = [&](uint8_t* a, uint8_t* b) { *reinterpret_cast<uint32_t*>(a + 8) = (uint32_t)(uintptr_t)b; *reinterpret_cast<uint32_t*>(b + 4) = (uint32_t)(uintptr_t)a; };
        for (int i = 0; i <= kN; ++i) Link(L(i), L(i + 1));
        // free list: sentinel head 0x965508 (next at +8 = 0x965510), a free item or two, tail sentinel 0x965514 (prev at +4 = 0x965518)
        uint8_t* fh = reinterpret_cast<uint8_t*>(0x965508); uint8_t* ft = reinterpret_cast<uint8_t*>(0x965514);
        const int nf = (int)rr.below(3);
        uint8_t* cur = fh;
        for (int i = 0; i < nf; ++i) { Link(cur, L(kN + 2 + i)); cur = L(kN + 2 + i); }
        Link(cur, ft);
        StaticRef<uint32_t>(0x8E48A0) = (uint32_t)(uintptr_t)L(rr.below(5) == 0 ? 0 : 1 + rr.below(kN));   // render link: usually an item
        const uint32_t m = rr.below(8);
        link = m == 0 ? 0 : m == 1 ? (int)StaticRef<uint32_t>(0x8E48A0) : (int)(uintptr_t)L(1 + rr.below(kN));
        (void)wantItem;
    };
    SROW("CStreaming::RenderEntity", 0x4096D0)
        int link = 0;
        g_post = [&link](Rng& rr) { BuildLinks(rr, link, true); };
        return VCase(r, d, [&]() -> uint64_t { CS::RenderEntity(reinterpret_cast<CLink<CEntity*>*>(link)); return 0; }, [&]() -> uint64_t { ExeC<void, int>(0x4096D0, link); return 0; });
    SEND(800)
    SROW("CStreaming::RemoveEntity", 0x409710)
        int link = 0;
        g_post = [&link](Rng& rr) { BuildLinks(rr, link, true); };
        return VCase(r, d, [&]() -> uint64_t { CS::RemoveEntity(reinterpret_cast<CLink<CEntity*>*>(link)); return 0; }, [&]() -> uint64_t { ExeC<void, int>(0x409710, link); return 0; });
    SEND(800)
    SROW("CStreaming::IsCarModelNeededInCurrentZone", 0x407DD0)
        int32_t id = 0;
        g_post = [&id](Rng& rr) { BuildPopFx(rr); id = PopModel(rr); };
        return VCase(r, d, [&]() -> uint64_t { return CS::IsCarModelNeededInCurrentZone(id); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, int32_t>(0x407DD0, id); });
    SEND(2500)
    SROW("CStreaming::AddToLoadedVehiclesList", 0x408000)
        int32_t id = 0;
        g_post = [&id](Rng& rr) { BuildPopFx(rr); id = (int32_t)(400 + rr.below(24)); };
        return VCase(r, d, [&]() -> uint64_t { return CS::AddToLoadedVehiclesList(id); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, int32_t>(0x408000, id); });
    SEND(2000)
    SROW("CStreaming::ReclassifyLoadedCars", 0x40AFA0)
        g_post = [](Rng& rr) { BuildPopFx(rr); };
        return VCase(r, d, [&]() -> uint64_t { CS::ReclassifyLoadedCars(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x40AFA0); return 0; });
    SEND(1000)
}
