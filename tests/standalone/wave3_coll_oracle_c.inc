// ORACLE-W3-COLL part C: class B functions (touch exe globals only) of game_sa/Collision and game_sa/Core: CColAccel, CColModel pool new/delete, CColStore,
// ResetMadeInvisibleObjects, CEntryInfoList::Flush, CEntryInfoNode pool new/delete, CKeyGen::AppendStringToKey. The test is compiled with NOTSA_ORACLE_ADDRESS_GLOBALS: the port's
// NOTSA_GLOBALs live at the exe's addresses, so port and exe read/write the same memory; each row snapshots the touched regions, runs the port, snapshots again, restores, runs the exe,
// and compares (return value + every region).
#include "ColStore.h"
#include "ColAccel.h"
#include "ColModel.h"
#include "EntryInfoList.h"
#include "EntryInfoNode.h"
#include "KeyGen.h"
#include "References.h"
#include "Pools/ColModelPool.h"
#include "Pools/EntryInfoNodePool.h"
#include "Pools/Pools.h"
#include "Entity/Entity.h"

// the pool getters live in Pools.cpp (not linked into this test): same bodies, reading the pool globals at their exe addresses
CColModelPool*      GetColModelPool() { return StaticRef<CColModelPool*>(0xB744A4); }
CEntryInfoNodePool* GetEntryInfoNodePool() { return StaticRef<CEntryInfoNodePool*>(0xB7448C); }
// Entity.cpp is not linked: the body of CEntity::CleanUpOldReference (0x571A00, verified against the exe by the ResetMadeInvisibleObjects row below)
void CEntity::CleanUpOldReference(CEntity** entity) {
    auto lastnextp = &m_pReferences;
    for (auto ref = m_pReferences; ref; ref = ref->m_pNext) {
        if (ref->m_ppEntity == entity) { *lastnextp = ref->m_pNext; ref->m_pNext = CReferences::pEmptyList; CReferences::pEmptyList = ref; ref->m_ppEntity = nullptr; break; }
        lastnextp = &ref->m_pNext;
    }
}
void SetIfCollisionIsRequired(const CVector2D& vecPos, ColDef* def);
void SetIfCollisionIsRequiredReducedBB(const CVector2D& vecPos, ColDef* def);

struct W3Reg { void* p; size_t n; };
// port on the live state, snapshot, restore, exe, compare. The return values are compared as 32-bit values (`retMask`)
template<class PF, class EF>
static bool W3Both(std::string& d, std::initializer_list<W3Reg> regs, PF&& pf, EF&& ef, const char* what, uintptr_t retMask = ~uintptr_t(0)) {
    std::vector<std::vector<uint8>> s0, s1;
    for (auto& g : regs) { s0.emplace_back((uint8*)g.p, (uint8*)g.p + g.n); }
    const uintptr_t ra = pf();
    for (auto& g : regs) { s1.emplace_back((uint8*)g.p, (uint8*)g.p + g.n); }
    { size_t i = 0; for (auto& g : regs) std::memcpy(g.p, s0[i++].data(), g.n); }
    const uintptr_t re = ef();
    bool ok = (ra & retMask) == (re & retMask);
    if (!ok) g_hardDiff = true;
    std::string dd; size_t i = 0;
    for (auto& g : regs) {
        // dword-wise: a NaN-vs-NaN difference (quiet bit / payload: the exe copies bits, the port may load / store floats) is classified by SameBits as payload-only, anything else is a real difference
        bool diff = false; size_t first = 0;
        for (size_t o = 0; o + 4 <= g.n; o += 4) { uint32 x, y; std::memcpy(&x, s1[i].data() + o, 4); std::memcpy(&y, (uint8*)g.p + o, 4); if (x != y) { if (!diff) first = o; diff = true; SameBits(x, y); } }
        for (size_t o = g.n & ~size_t(3); o < g.n; ++o) if (s1[i][o] != ((uint8*)g.p)[o]) { if (!diff) first = o & ~size_t(3); diff = true; g_hardDiff = true; }
        if (diff) { ok = false; dd += " reg" + std::to_string(i) + "@" + std::to_string(first) + " got " + Hex(s1[i].data() + first, g.n - first < 8 ? g.n - first : 8) + " exe " + Hex((uint8*)g.p + first, g.n - first < 8 ? g.n - first : 8) + ";"; }
        ++i;
    }
    if (!ok) d = std::string(what) + " ret got " + std::to_string(ra) + " exe " + std::to_string(re) + ";" + dd;
    return ok;
}

// ---- pool fixture (pre-allocated storage + states, installed at an exe pool global) ----
template<class PoolT, class T, size_t N>
struct W3PoolFx {
    alignas(16) uint8 store[N * sizeof(T)];
    alignas(16) uint8 flags[N + 4];
    alignas(16) uint8 poolMem[sizeof(PoolT)];
    PoolT* pool = nullptr;
    void Init() { if (!pool) { std::memset(flags, 0, sizeof flags); pool = new (poolMem) PoolT(N, store, flags); } }
    // random occupancy (at least one used and one free slot), random ref counters, random first-free hint, garbage objects
    void Prepare(Rng& r, bool needUsed = true) {
        Init(); W3Garbage(r, store, sizeof store);
        int nused = 0;
        for (size_t i = 0; i < N; ++i) { const bool used = r.below(3) != 0; flags[i] = (uint8)(r.below(0x7F) | (used ? 0 : 0x80)); nused += used; }
        if (needUsed && !nused) flags[r.below((int)N)] &= 0x7F;
        if (nused == (int)N) flags[r.below((int)N)] |= 0x80;
        const int32 hint = (int32)r.below((int)N + 1) - 1; std::memcpy(poolMem + 0x0C, &hint, 4);
    }
    int PickUsed(Rng& r) { for (int t = 0; t < 64; ++t) { const int i = r.below((int)N); if (!(flags[i] & 0x80)) return i; } for (size_t i = 0; i < N; ++i) if (!(flags[i] & 0x80)) return (int)i; return 0; }
    W3Reg RStruct() { return { poolMem, 0x14 }; } W3Reg RFlags() { return { flags, N }; } W3Reg RStore() { return { store, N * sizeof(T) }; }
};

static void TestW3CollC() {
    // ---- CColAccel ----
    Run("CColAccel::isCacheLoading 0x5B2AC0", [](Rng& r, std::string& d) {
        const int states[] = { 0, 1, 2, 3, -1, 0x10002 }; CColAccel::m_iCacheState = (eColAccelState)states[r.below(6)];
        const bool x = CColAccel::isCacheLoading(), y = oracle::Fn<bool __cdecl()>(0x5B2AC0)();
        Hit(x); if (x == y) return true;
        d = "state " + std::to_string((int)CColAccel::m_iCacheState); return false;
    });
    Run("CColAccel::addCacheCol 0x5B2C20", [](Rng& r, std::string& d) {
        alignas(16) static uint8 items[8][0x30]; W3Garbage(r, items, sizeof items);
        alignas(16) uint8 cm[sizeof(CColModel) + 16]; W3Garbage(r, cm, sizeof cm);
        CColAccel::mp_caccColItems = reinterpret_cast<CColAccelColEntry*>(items); CColAccel::m_iNumColItems = r.below(7);
        CColAccel::m_iCacheState = r.below(5) ? COLACCEL_STARTED : (eColAccelState)r.below(3);
        PackedModelStartEnd se; se.modelId = (int32)r.u32();
        return W3Both(d, { { items, sizeof items }, { &CColAccel::m_iNumColItems, 4 }, { &CColAccel::m_iCacheState, 4 } },
            [&]() -> uintptr_t { CColAccel::addCacheCol(se, *reinterpret_cast<const CColModel*>(cm)); return 0; },
            [&]() -> uintptr_t { oracle::Fn<void __cdecl(int, const void*)>(0x5B2C20)(se.modelId, cm); return 0; }, "addCacheCol");
    });
    Run("CColAccel::addColDef 0x5B2DD0", [](Rng& r, std::string& d) {
        alignas(16) static uint8 bounds[8][0x18]; W3Garbage(r, bounds, sizeof bounds);
        ColDef def; W3Garbage(r, &def, sizeof def); def.m_bProcedural = r.below(2); def.m_bInterior = r.below(2);
        CColAccel::m_colBounds = reinterpret_cast<CColAccelColBound*>(bounds); CColAccel::m_iNumColBounds = r.below(7);
        return W3Both(d, { { bounds, sizeof bounds }, { &CColAccel::m_iNumColBounds, 4 } },
            [&]() -> uintptr_t { CColAccel::addColDef(def); return 0; },
            [&]() -> uintptr_t { oracle::Fn<void __cdecl(ColDef)>(0x5B2DD0)(def); return 0; }, "addColDef");
    });
    Run("CColAccel::getColDef 0x5B2E60", [](Rng& r, std::string& d) {
        alignas(16) static uint8 bounds[8][0x18]; W3Garbage(r, bounds, sizeof bounds);
        alignas(16) uint8 def[sizeof(ColDef)]; W3Garbage(r, def, sizeof def);
        CColAccel::m_colBounds = reinterpret_cast<CColAccelColBound*>(bounds); CColAccel::m_iNumColBounds = r.below(7);
        return W3Both(d, { { bounds, sizeof bounds }, { &CColAccel::m_iNumColBounds, 4 }, { def, sizeof def } },
            [&]() -> uintptr_t { CColAccel::getColDef(*reinterpret_cast<ColDef*>(def)); return 0; },
            [&]() -> uintptr_t { oracle::Fn<void __cdecl(void*)>(0x5B2E60)(def); return 0; }, "getColDef");
    });
    Run("CColAccel::setIplDef 0x5B2ED0", [](Rng& r, std::string& d) {
        alignas(16) static uint8 defs[8][sizeof(IplDef)]; W3Garbage(r, defs, sizeof defs);
        IplDef id; W3Garbage(r, &id, sizeof id); const int idx = r.below(8);
        CColAccel::m_iplDefs = reinterpret_cast<IplDef*>(defs);
        return W3Both(d, { { defs, sizeof defs } },
            [&]() -> uintptr_t { CColAccel::setIplDef(idx, id); return 0; },
            [&]() -> uintptr_t { oracle::Fn<void __cdecl(int, IplDef)>(0x5B2ED0)(idx, id); return 0; }, "setIplDef");
    });
    Run("CColAccel::getIplDef 0x5B2EF0", [](Rng& r, std::string& d) {
        alignas(16) static uint8 defs[8][sizeof(IplDef)]; W3Garbage(r, defs, sizeof defs);
        alignas(16) uint8 out[sizeof(IplDef)]; W3Garbage(r, out, sizeof out); const int idx = r.below(8);
        CColAccel::m_iplDefs = reinterpret_cast<IplDef*>(defs);
        return W3Both(d, { { defs, sizeof defs }, { out, sizeof out } },
            [&]() -> uintptr_t { const IplDef x = CColAccel::getIplDef(idx); std::memcpy(out, &x, sizeof out); return 0; },
            [&]() -> uintptr_t { oracle::Fn<void* __cdecl(void*, int)>(0x5B2EF0)(out, idx); return 0; }, "getIplDef");
    });

    // ---- CColModel / CEntryInfoNode pool new + delete ----
    static W3PoolFx<CColModelPool, CColModel, 8> cmFx;
    static W3PoolFx<CEntryInfoNodePool, CEntryInfoNode, 8> enFx;
    Run("CColModel::operator new 0x40FC30", [](Rng& r, std::string& d) {
        cmFx.Prepare(r); StaticRef<CColModelPool*>(0xB744A4) = cmFx.pool;
        return W3Both(d, { cmFx.RStruct(), cmFx.RFlags(), cmFx.RStore() },
            [&]() -> uintptr_t { void* p = CColModel::operator new(sizeof(CColModel)); Hit(p != nullptr); return (uintptr_t)p; },
            [&]() -> uintptr_t { return (uintptr_t)oracle::Fn<void* __cdecl(unsigned)>(0x40FC30)(sizeof(CColModel)); }, "new");
    });
    Run("CColModel::operator delete 0x40FC40", [](Rng& r, std::string& d) {
        cmFx.Prepare(r); StaticRef<CColModelPool*>(0xB744A4) = cmFx.pool; void* obj = cmFx.store + cmFx.PickUsed(r) * sizeof(CColModel);
        return W3Both(d, { cmFx.RStruct(), cmFx.RFlags(), cmFx.RStore() },
            [&]() -> uintptr_t { CColModel::operator delete(obj); return 0; },
            [&]() -> uintptr_t { oracle::Fn<void __cdecl(void*)>(0x40FC40)(obj); return 0; }, "delete");
    });
    Run("CEntryInfoNode::operator new 0x536DC0", [](Rng& r, std::string& d) {
        enFx.Prepare(r); StaticRef<CEntryInfoNodePool*>(0xB7448C) = enFx.pool;
        return W3Both(d, { enFx.RStruct(), enFx.RFlags(), enFx.RStore() },
            [&]() -> uintptr_t { void* p = CEntryInfoNode::operator new(sizeof(CEntryInfoNode)); Hit(p != nullptr); return (uintptr_t)p; },
            [&]() -> uintptr_t { return (uintptr_t)oracle::Fn<void* __cdecl(unsigned)>(0x536DC0)(sizeof(CEntryInfoNode)); }, "new");
    });
    Run("CEntryInfoNode::operator delete 0x536DD0", [](Rng& r, std::string& d) {
        enFx.Prepare(r); StaticRef<CEntryInfoNodePool*>(0xB7448C) = enFx.pool; void* obj = enFx.store + enFx.PickUsed(r) * sizeof(CEntryInfoNode);
        return W3Both(d, { enFx.RStruct(), enFx.RFlags(), enFx.RStore() },
            [&]() -> uintptr_t { CEntryInfoNode::operator delete(obj, sizeof(CEntryInfoNode)); return 0; },
            [&]() -> uintptr_t { oracle::Fn<void __cdecl(void*)>(0x536DD0)(obj); return 0; }, "delete");
    });
    Run("CEntryInfoList::Flush 0x536E10", [](Rng& r, std::string& d) {
        enFx.Prepare(r, true); StaticRef<CEntryInfoNodePool*>(0xB7448C) = enFx.pool;
        // a doubly linked list over a random subset of the used slots, in random order
        auto* nodes = reinterpret_cast<CEntryInfoNode*>(enFx.store); int order[8], n = 0;
        for (int i = 0; i < 8; ++i) if (!(enFx.flags[i] & 0x80)) order[n++] = i;
        for (int i = n - 1; i > 0; --i) std::swap(order[i], order[r.below(i + 1)]);
        n = r.below(n + 1);
        CEntryInfoList list{}; list.m_node = n ? &nodes[order[0]] : nullptr;
        for (int i = 0; i < n; ++i) { nodes[order[i]].m_previous = i ? &nodes[order[i - 1]] : nullptr; nodes[order[i]].m_next = i + 1 < n ? &nodes[order[i + 1]] : nullptr; }
        Hit(n > 0);
        return W3Both(d, { enFx.RStruct(), enFx.RFlags(), enFx.RStore(), { &list, sizeof list } },
            [&]() -> uintptr_t { list.Flush(); return 0; },
            [&]() -> uintptr_t { oracle::Fn<void __fastcall(void*)>(0x536E10)(&list); return 0; }, "Flush");
    });

    // ---- CKeyGen ----
    Run("CKeyGen::AppendStringToKey 0x53CF70", [](Rng& r, std::string& d) {
        char s[40]; const int len = r.below(30);
        for (int i = 0; i < len; ++i) s[i] = r.below(8) ? (char)(32 + r.below(95)) : (char)(1 + r.below(255));
        s[len] = 0; const uint32 key = r.below(4) ? r.u32() : 0xFFFFFFFFu;
        const uint32 x = CKeyGen::AppendStringToKey(key, s), y = oracle::Fn<uint32 __cdecl(uint32, const char*)>(0x53CF70)(key, s);
        Hit(len > 0); if (x == y) return true;
        d = "key " + Hex(&key, 4) + " str " + Hex(s, len) + " got " + Hex(&x, 4) + " exe " + Hex(&y, 4); return false;
    });

    // ---- CColStore ----
    static W3PoolFx<CColPool, ColDef, 8> cdFx;
    auto fixDef = [&](Rng& r) { cdFx.Prepare(r); StaticRef<CColPool*>(0x965560) = cdFx.pool; };
    Run("CColStore::AddCollisionNeededAtPosn 0x4103A0", [](Rng& r, std::string& d) {
        const CVector p = GenV(r, 3000.f); W3Garbage(r, &CColStore::ms_vecCollisionNeeded, 12); CColStore::ms_bCollisionNeeded = r.below(2);
        return W3Both(d, { { &CColStore::ms_vecCollisionNeeded, 12 }, { &CColStore::ms_bCollisionNeeded, 1 } },
            [&]() -> uintptr_t { CColStore::AddCollisionNeededAtPosn(p); return 0; },
            [&]() -> uintptr_t { oracle::Fn<void __cdecl(const CVector*)>(0x4103A0)(&p); return 0; }, "AddCollisionNeededAtPosn");
    });
    auto reqRow = [&](const char* name, unsigned va, bool reduced) {
        Run(name, [=](Rng& r, std::string& d) {
            alignas(16) uint8 buf[sizeof(ColDef)]; W3Garbage(r, buf, sizeof buf); auto* def = reinterpret_cast<ColDef*>(buf);
            const float S = std::fmin(W3S(r), 3000.f);
            float a = GenF(r, S), b = GenF(r, S), c = GenF(r, S), e = GenF(r, S);
            float v[4] = { std::fmin(a, c), std::fmax(b, e), std::fmax(a, c), std::fmin(b, e) };   // memory order left, +4 (upper y), right, +0xC (lower y)
            if (r.below(8) == 0) std::swap(v[0], v[2]);
            std::memcpy(&def->m_Area, v, 16); def->m_bCollisionIsRequired = r.below(2); def->m_bInterior = r.below(2);
            CVector2D p{ v[0] + (v[2] - v[0]) * (r.f01() * 1.3f - 0.15f), v[3] + (v[1] - v[3]) * (r.f01() * 1.3f - 0.15f) };
            if (reduced && r.below(2)) { const float k = r.below(2) ? 80.f : -80.f; p.x = v[0] + k + (r.below(3) == 0 ? 0.f : r.f01()); }
            if (r.below(7) == 0) p.y = GenF(r, S);
            const int codes[] = { 0, 1, 2, 3, 5, 18 }; StaticRef<int32>(0x965554) = codes[r.below(6)];
            const uint8 before = def->m_bCollisionIsRequired;
            const bool ok = W3Both(d, { { buf, sizeof buf } },
                [&]() -> uintptr_t { reduced ? SetIfCollisionIsRequiredReducedBB(p, def) : SetIfCollisionIsRequired(p, def); return 0; },
                [&]() -> uintptr_t { oracle::Fn<void __cdecl(const CVector2D*, ColDef*)>(va)(&p, def); return 0; }, name);
            Hit(def->m_bCollisionIsRequired && !before);
            return ok;
        });
    };
    reqRow("::SetIfCollisionIsRequired 0x4103D0", 0x4103D0, false);
    reqRow("::SetIfCollisionIsRequiredReducedBB 0x410470", 0x410470, true);
    auto refRow = [&](const char* name, unsigned va, int kind) {
        Run(name, [=](Rng& r, std::string& d) {
            fixDef(r); const int slot = cdFx.PickUsed(r); const int mid = (int)(r.u32() & 0xFFFF) - (r.below(2) ? 0 : 0x8000);
            return W3Both(d, { cdFx.RStruct(), cdFx.RFlags(), cdFx.RStore() },
                [&]() -> uintptr_t { switch (kind) { case 0: CColStore::AddRef(slot); return 0; case 1: CColStore::RemoveRef(slot); return 0; case 2: return (uintptr_t)CColStore::GetBoundingBox(slot); default: CColStore::IncludeModelIndex(slot, mid); return 0; } },
                [&]() -> uintptr_t { switch (kind) { case 0: case 1: oracle::Fn<void __cdecl(int)>(va)(slot); return 0; case 2: return (uintptr_t)oracle::Fn<void* __cdecl(int)>(va)(slot); default: oracle::Fn<void __cdecl(int, int)>(va)(slot, mid); return 0; } }, name);
        });
    };
    refRow("CColStore::AddRef 0x4107A0", 0x4107A0, 0);
    refRow("CColStore::RemoveRef 0x4107D0", 0x4107D0, 1);
    refRow("CColStore::GetBoundingBox 0x410800", 0x410800, 2);
    refRow("CColStore::IncludeModelIndex 0x410820", 0x410820, 3);

    // ---- ResetMadeInvisibleObjects ----
    Run("::ResetMadeInvisibleObjects 0x415540", [](Rng& r, std::string& d) {
        alignas(16) static uint8 ents[4][0x200]; alignas(16) static uint8 refs[8][8];
        W3Garbage(r, ents, sizeof ents); W3Garbage(r, refs, sizeof refs);
        CEntity** slots = reinterpret_cast<CEntity**>(0x9655A0); uint32& num = StaticRef<uint32>(0x9655DC);
        for (auto& e : ents) { auto* en = reinterpret_cast<CEntity*>(e); en->m_pReferences = nullptr; }
        num = r.below(11); for (int i = 0; i < 10; ++i) slots[i] = r.below(4) == 0 ? nullptr : reinterpret_cast<CEntity*>(ents[r.below(4)]);
        // reference chains: nodes {next, ppEntity}; most nodes point at a real slot address
        int used = 0; auto** nodes = reinterpret_cast<void**>(refs);
        for (auto& e : ents) { auto* en = reinterpret_cast<CEntity*>(e); void** prev = nullptr;
            for (int k = r.below(3); k > 0 && used < 8; --k, ++used) { void** n = nodes + 2 * used; n[0] = nullptr; n[1] = r.below(5) ? (void*)&slots[r.below(10)] : (void*)(slots + 10 + r.below(3)); if (prev) prev[0] = n; else en->m_pReferences = reinterpret_cast<CReference*>(n); prev = n; } }
        CReferences::pEmptyList = used < 8 && r.below(2) ? reinterpret_cast<CReference*>(nodes + 2 * used) : nullptr;
        if (CReferences::pEmptyList) { void** n = reinterpret_cast<void**>(CReferences::pEmptyList); n[0] = nullptr; }
        Hit(num > 0);
        return W3Both(d, { { ents, sizeof ents }, { refs, sizeof refs }, { slots, 0x28 }, { &num, 4 }, { &CReferences::pEmptyList, 4 } },
            [&]() -> uintptr_t { ResetMadeInvisibleObjects(); return 0; },
            [&]() -> uintptr_t { oracle::Fn<void __cdecl()>(0x415540)(); return 0; }, "Reset");
    });
}
