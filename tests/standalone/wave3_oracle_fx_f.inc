// ORACLE-W3-EFFECTS rows F: class C rows of the fire manager that stay pure state when the fires hold no entity / FX system pointers (CFire::Extinguish and its callers).
// (The entity and FX pointers of every fire are null: Extinguish then only rewrites the flag byte and the burn time on both sides.)
struct W3fCtorTag { using type = CFireManager* (CFireManager::*)(); friend type W2Get(W3fCtorTag); }; template struct W2Rob<W3fCtorTag, &CFireManager::Constructor>;   // private in the port
struct W3fDtorTag { using type = CFireManager* (CFireManager::*)(); friend type W2Get(W3fDtorTag); }; template struct W2Rob<W3fDtorTag, &CFireManager::Destructor>;
namespace w3ff {
using namespace w3f; using namespace w2; using namespace w3fb;
static void PutFiresNoPtr(Rng& r) {
    PutFires(r);
    auto* b = reinterpret_cast<uint8_t*>(&gFireManager);
    for (int i = 0; i < MAX_NUM_FIRES; ++i) { *reinterpret_cast<uint32_t*>(b + i * 0x28 + 0x10) = 0; *reinterpret_cast<uint32_t*>(b + i * 0x28 + 0x14) = 0; *reinterpret_cast<uint32_t*>(b + i * 0x28 + 0x24) = 0; }
}
} // namespace w3ff
static void TestW3FxF() {
    using namespace w2; using namespace w3f; using namespace w3fb; using namespace w3ff;
    ROW("CFire::Extinguish", 0x5393F0, 600) {
        const size_t off = 0x100 * r.below(4); CFire* f = FireAt(off);
        Tu tu([off](Rng& rr) { uint8_t* p = S(off); p[0] = (uint8_t)rr.u32(); *reinterpret_cast<uint32_t*>(p + 0x10) = 0; *reinterpret_cast<uint32_t*>(p + 0x14) = 0; *reinterpret_cast<uint32_t*>(p + 0x24) = 0; });
        return UCase(r, d, [&]() -> uint64_t { f->Extinguish(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(CFire*, int)>(0x5393F0)(f, 0); return 0; });
    } ENDROW;
    ROW("CFireManager::GetNextFreeFire", 0x539E50, 2000) {
        const bool may = r.below(2);
        Tu tu([](Rng& rr) { PutFiresNoPtr(rr);
            if (rr.below(2)) { auto* b = reinterpret_cast<uint8_t*>(&gFireManager); const int nFree = (int)rr.below(4); for (int i = 0; i < MAX_NUM_FIRES; ++i) { if (i >= 60 - nFree) b[i * 0x28] &= ~3; else b[i * 0x28] |= 1; } } });
        return UCase(r, d, [&]() -> uint64_t { return U(gFireManager.GetNextFreeFire(may)); }, [&]() -> uint64_t { return U(oracle::Fn<CFire* __fastcall(CFireManager*, int, int)>(0x539E50)(&gFireManager, 0, may ? 1 : 0)); });
    } ENDROW;
    ROW("CFireManager::RemoveScriptFire", 0x539700, 1500) {
        const uint16_t id = (uint16_t)r.below(MAX_NUM_FIRES);
        Tu tu([](Rng& rr) { PutFiresNoPtr(rr); });
        return UCase(r, d, [&]() -> uint64_t { gFireManager.RemoveScriptFire(id); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(CFireManager*, int, int)>(0x539700)(&gFireManager, 0, id); return 0; });
    } ENDROW;
    ROW("CFireManager::RemoveAllScriptFires", 0x539720, 800) {
        Tu tu([](Rng& rr) { PutFiresNoPtr(rr); });
        return UCase(r, d, [&]() -> uint64_t { gFireManager.RemoveAllScriptFires(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(CFireManager*, int)>(0x539720)(&gFireManager, 0); return 0; });
    } ENDROW;
    ROW("CFireManager::Shutdown", 0x539DD0, 800) {
        Tu tu([](Rng& rr) { PutFiresNoPtr(rr); });
        return UCase(r, d, [&]() -> uint64_t { gFireManager.Shutdown(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(CFireManager*, int)>(0x539DD0)(&gFireManager, 0); return 0; });
    } ENDROW;
    ROW("CFireManager::Constructor", 0x539DA0, 100) {
        const size_t off = 0x1000;
        return UCase(r, d, [&]() -> uint64_t { return U((reinterpret_cast<CFireManager*>(S(off))->*W2Get(W3fCtorTag{}))()); }, [&]() -> uint64_t { return U(oracle::Fn<CFireManager* __fastcall(CFireManager*, int)>(0x539DA0)(reinterpret_cast<CFireManager*>(S(off)), 0)); });
    } ENDROW;
    ROW("CFireManager::Destructor", 0x538BB0, 100) {
        const size_t off = 0x1000;
        return UCase(r, d, [&]() -> uint64_t { (reinterpret_cast<CFireManager*>(S(off))->*W2Get(W3fDtorTag{}))(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(CFireManager*, int)>(0x538BB0)(reinterpret_cast<CFireManager*>(S(off)), 0); return 0; });   // (the exe destructor is a bare ret: the return register is not compared)
    } ENDROW;
}
