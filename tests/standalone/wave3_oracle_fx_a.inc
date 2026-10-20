// ORACLE-W3-EFFECTS rows A: Bridge / Cranes / Explosion::Shutdown (bare stubs), Fire, FireManager, CreepingFire (see wave3_oracle_fx.inc).
namespace w3fa { using namespace w3f; using namespace w2; }
static void TestW3FxA() {
    using namespace w2; using namespace w3f; using namespace w3fa;
    ROW("CBridge::Init", 0x41BC70, 20) {
        return UCase(r, d, [&]() -> uint64_t { CBridge::Init(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x41BC70)(); return 0; });
    } ENDROW;
    ROW("CBridge::Update", 0x41BC80, 20) {
        return UCase(r, d, [&]() -> uint64_t { CBridge::Update(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x41BC80)(); return 0; });
    } ENDROW;
    ROW("CBridge::FindBridgeEntities", 0x41BCA0, 20) {
        return UCase(r, d, [&]() -> uint64_t { CBridge::FindBridgeEntities(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x41BCA0)(); return 0; });
    } ENDROW;
    ROW("CBridge::ShouldLightsBeFlashing", 0x41BC90, 20) {
        return UCase(r, d, [&]() -> uint64_t { return CBridge::ShouldLightsBeFlashing() ? 1 : 0; }, [&]() -> uint64_t { return oracle::Fn<bool __cdecl()>(0x41BC90)() ? 1 : 0; });
    } ENDROW;
}
