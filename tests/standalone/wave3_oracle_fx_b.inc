// ORACLE-W3-EFFECTS rows B: Cranes / Explosion::Shutdown (bare stubs), Fire, FireManager, CreepingFire (see wave3_oracle_fx.inc).
namespace w3fb {
using namespace w3f; using namespace w2;
static CFire* FireAt(size_t off) { return reinterpret_cast<CFire*>(S(off)); }
// the global fire manager: every slot gets random flags / script ids / positions / strength, so the active / script / extinguishing combinations and the id lookups hit all branches
static void PutFires(Rng& r) {
    FillGarbage(r, &gFireManager, sizeof(gFireManager));
    auto* b = reinterpret_cast<uint8_t*>(&gFireManager);
    for (int i = 0; i < MAX_NUM_FIRES; ++i) {
        uint8_t& fl = b[i * 0x28];
        fl = (uint8_t)(r.below(4) == 0 ? r.u32() : (r.below(2) ? (fl & 0xE0) | (uint8_t)r.below(32) : 0x14));
        *reinterpret_cast<int16_t*>(b + i * 0x28 + 2) = (int16_t)(r.below(3) ? i : (int)r.below(1000));
        *reinterpret_cast<float*>(b + i * 0x28 + 4) = GF(r, 2000.f);
        *reinterpret_cast<float*>(b + i * 0x28 + 8) = GF(r, 2000.f);
        *reinterpret_cast<float*>(b + i * 0x28 + 12) = GF(r, 200.f);
    }
}
} // namespace w3fb
static void TestW3FxB() {
    using namespace w2; using namespace w3f; using namespace w3fb;
    ROW("CCranes::InitCranes", 0x6F3FC0, 20) {
        return UCase(r, d, [&]() -> uint64_t { CCranes::InitCranes(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x6F3FC0)(); return 0; });
    } ENDROW;
    ROW("CCranes::UpdateCranes", 0x6F3FE0, 20) {
        return UCase(r, d, [&]() -> uint64_t { CCranes::UpdateCranes(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x6F3FE0)(); return 0; });
    } ENDROW;
    ROW("CCranes::IsThisCarBeingCarriedByAnyCrane", 0x6F3FF0, 40) {
        auto* v = reinterpret_cast<CVehicle*>(S(0x1000));
        return UCase(r, d, [&]() -> uint64_t { return B8(CCranes::IsThisCarBeingCarriedByAnyCrane(v)); }, [&]() -> uint64_t { return oracle::Fn<bool __cdecl(CVehicle*)>(0x6F3FF0)(v) ? 1 : 0; });
    } ENDROW;
    ROW("CCranes::IsThisCarBeingTargettedByAnyCrane", 0x6F4000, 40) {
        auto* v = reinterpret_cast<CVehicle*>(S(0x1000));
        return UCase(r, d, [&]() -> uint64_t { return B8(CCranes::IsThisCarBeingTargettedByAnyCrane(v)); }, [&]() -> uint64_t { return oracle::Fn<bool __cdecl(CVehicle*)>(0x6F4000)(v) ? 1 : 0; });
    } ENDROW;
    ROW("CCranes::IsThisCarPickedUp", 0x6F3FD0, 40) {
        auto* v = reinterpret_cast<CVehicle*>(S(0x1000)); const float a = Fl(r, 100.f), b = Fl(r, 100.f);
        return UCase(r, d, [&]() -> uint64_t { return B8(CCranes::IsThisCarPickedUp(a, b, v)); }, [&]() -> uint64_t { return oracle::Fn<bool __cdecl(float, float, CVehicle*)>(0x6F3FD0)(a, b, v) ? 1 : 0; });
    } ENDROW;
    ROW("CExplosion::Shutdown", 0x7368F0, 20) {
        return UCase(r, d, [&]() -> uint64_t { CExplosion::Shutdown(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x7368F0)(); return 0; });
    } ENDROW;
    ROW("CFire::Constructor", 0x539D90, 200) {
        const size_t off = 0x100 * r.below(8); CFire* f = FireAt(off);
        return UCase(r, d, [&]() -> uint64_t { return U(f->Constructor()); }, [&]() -> uint64_t { return U(oracle::Fn<CFire* __fastcall(CFire*, int)>(0x539D90)(f, 0)); });
    } ENDROW;
    ROW("CFire::Initialise", 0x538B30, 200) {
        const size_t off = 0x100 * r.below(8); CFire* f = FireAt(off);
        return UCase(r, d, [&]() -> uint64_t { f->Initialise(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(CFire*, int)>(0x538B30)(f, 0); return 0; });
    } ENDROW;
    ROW("CFireManager::Init", 0x538BC0, 100) {
        Tu tu([](Rng& rr) { PutFires(rr); });
        return UCase(r, d, [&]() -> uint64_t { gFireManager.Init(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(CFireManager*, int)>(0x538BC0)(&gFireManager, 0); return 0; });
    } ENDROW;
    ROW("CFireManager::GetNumOfNonScriptFires", 0x538F10, 800) {
        Tu tu([](Rng& rr) { PutFires(rr); });
        return UCase(r, d, [&]() -> uint64_t { return gFireManager.GetNumOfNonScriptFires(); }, [&]() -> uint64_t { return oracle::Fn<uint32 __fastcall(CFireManager*, int)>(0x538F10)(&gFireManager, 0); });
    } ENDROW;
    ROW("CFireManager::PlentyFiresAvailable", 0x539340, 800) {
        Tu tu([](Rng& rr) { PutFires(rr);
            if (rr.below(3)) { auto* b = reinterpret_cast<uint8_t*>(&gFireManager); const int nInactive = (int)rr.below(9); for (int i = 0; i < MAX_NUM_FIRES; ++i) { if (i < nInactive) b[i * 0x28] &= ~1; else b[i * 0x28] |= 1; } } });
        return UCase(r, d, [&]() -> uint64_t { return B8(gFireManager.PlentyFiresAvailable()); }, [&]() -> uint64_t { return oracle::Fn<bool __fastcall(CFireManager*, int)>(0x539340)(&gFireManager, 0) ? 1 : 0; });
    } ENDROW;
    ROW("CFireManager::IsScriptFireExtinguished", 0x5396E0, 800) {
        const int16_t id = (int16_t)r.below(MAX_NUM_FIRES);
        Tu tu([](Rng& rr) { PutFires(rr); });
        return UCase(r, d, [&]() -> uint64_t { return B8(gFireManager.IsScriptFireExtinguished(id)); }, [&]() -> uint64_t { return oracle::Fn<bool __fastcall(CFireManager*, int, int)>(0x5396E0)(&gFireManager, 0, id) ? 1 : 0; });
    } ENDROW;
    ROW("CFireManager::SetScriptFireAudio", 0x5397B0, 800) {
        const int16_t id = (int16_t)r.below(MAX_NUM_FIRES); const bool flag = r.below(2);
        Tu tu([](Rng& rr) { PutFires(rr); });
        return UCase(r, d, [&]() -> uint64_t { gFireManager.SetScriptFireAudio(id, flag); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(CFireManager*, int, int, int)>(0x5397B0)(&gFireManager, 0, id, flag ? 1 : 0); return 0; });
    } ENDROW;
    ROW("CFireManager::GetScriptFireCoords", 0x5397E0, 800) {
        const int16_t id = (int16_t)r.below(MAX_NUM_FIRES);
        Tu tu([](Rng& rr) { PutFires(rr); });
        return UCase(r, d, [&]() -> uint64_t { return U(&gFireManager.GetScriptFireCoords(id)); }, [&]() -> uint64_t { return U(oracle::Fn<const CVector* __fastcall(CFireManager*, int, int)>(0x5397E0)(&gFireManager, 0, id)); });
    } ENDROW;
    ROW("CCreepingFire::SetReadyToBurn", 0x539CC0, 100) {
        return UCase(r, d, [&]() -> uint64_t { CCreepingFire::SetReadyToBurn(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x539CC0)(); return 0; });
    } ENDROW;
    ROW("CCreepingFire::Update", 0x539CE0, 1500) {
        Tu tu([](Rng& rr) { for (auto& row : CCreepingFire::m_aFireStatus) for (auto& c : row) c = (uint8_t)(rr.below(3) ? rr.below(9) : rr.u32()); CTimer::m_FrameCounter = rr.below(4) ? rr.below(1024) : rr.u32(); });
        return UCase(r, d, [&]() -> uint64_t { CCreepingFire::Update(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x539CE0)(); return 0; });
    } ENDROW;
}
