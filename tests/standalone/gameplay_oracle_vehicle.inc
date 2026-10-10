// (d) vehicles: CDamageManager, cTransmission, cHandlingDataMgr, CVehicle / CAutomobile / ... physics against the exe (see gameplay_oracle_test.cpp).
// Helpers: namespace wfx (gameplay_oracle_weapon.inc).
#include "DamageManager.h"
#include "cTransmission.h"

static void TestDamageManager() {
    using namespace wfx;
    constexpr size_t N = 0x18;
    auto fillDm = [](Rng& rr, uint8* b) { FillU(rr, b, N); if (rr.below(3) == 0) std::memset(b + 5, 0, 12); };
    using DM = CDamageManager;
    auto dm = [](uint8* b) -> DM& { return *reinterpret_cast<DM*>(b); };
    Run("CDamageManager::Init 0x6A0520", [&](Rng& r, std::string& d) {
        return Obj<N>(r, d, fillDm, [&](uint8* b) { dm(b).Init(); return 0u; }, [](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6A0520)(b, 0); return 0u; }, 2);
    });
    Run("CDamageManager::ResetDamageStatusAndWheelDamage 0x6A04E0", [&](Rng& r, std::string& d) {
        return Obj<N>(r, d, fillDm, [&](uint8* b) { dm(b).ResetDamageStatusAndWheelDamage(); return 0u; }, [](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6A04E0)(b, 0); return 0u; }, 2);
    });
    Run("CDamageManager::ResetDamageStatus 0x6C20E0", [&](Rng& r, std::string& d) {
        return Obj<N>(r, d, fillDm, [&](uint8* b) { dm(b).ResetDamageStatus(); return 0u; }, [](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6C20E0)(b, 0); return 0u; }, 2);
    });
    Run("CDamageManager::SetLightStatus/GetLightStatus 0x6C2100/0x6C2130", [&](Rng& r, std::string& d) {
        const int l = r.below(4), s = r.below(4);
        return Obj<N>(r, d, fillDm, [&](uint8* b) { dm(b).SetLightStatus((eLights)l, (eLightsState)s); return (uint32_t)dm(b).GetLightStatus((eLights)(3 - l)); },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, int, int)>(0x6C2100)(b, 0, l, s); return (uint32_t)oracle::Fn<int __fastcall(void*, int, int)>(0x6C2130)(b, 0, 3 - l); });
    });
    Run("CDamageManager::SetPanelStatus/GetPanelStatus 0x6C2150/0x6C2180", [&](Rng& r, std::string& d) {
        const int p = r.below(7), s = r.below(16);
        return Obj<N>(r, d, fillDm, [&](uint8* b) { dm(b).SetPanelStatus((ePanels)p, (ePanelDamageState)s); return (uint32_t)dm(b).GetPanelStatus((ePanels)(6 - p)); },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, int, int)>(0x6C2150)(b, 0, p, s); return (uint32_t)oracle::Fn<int __fastcall(void*, int, int)>(0x6C2180)(b, 0, 6 - p); });
    });
    Run("CDamageManager::SetWheelStatus/GetWheelStatus 0x6C21A0/0x6C21B0", [&](Rng& r, std::string& d) {
        const int w = r.below(4), s = r.below(256);
        return Obj<N>(r, d, fillDm, [&](uint8* b) { dm(b).SetWheelStatus((eCarWheel)w, (eCarWheelStatus)s); return (uint32_t)dm(b).GetWheelStatus((eCarWheel)(3 - w)); },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, int, int)>(0x6C21A0)(b, 0, w, s); return (uint32_t)oracle::Fn<int __fastcall(void*, int, int)>(0x6C21B0)(b, 0, 3 - w); });
    });
    Run("CDamageManager::SetDoorStatus/GetDoorStatus 0x6C21C0/0x6C2230", [&](Rng& r, std::string& d) {
        const int w = r.below(6), s = r.below(256);
        return Obj<N>(r, d, fillDm, [&](uint8* b) { dm(b).SetDoorStatus((eDoors)w, (eDoorStatus)s); return (uint32_t)dm(b).GetDoorStatus((eDoors)(5 - w)); },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, int, int)>(0x6C21C0)(b, 0, w, s); return (uint32_t)oracle::Fn<int __fastcall(void*, int, int)>(0x6C2230)(b, 0, 5 - w); });
    });
    Run("CDamageManager::SetDoorStatus_Component 0x6C21E0", [&](Rng& r, std::string& d) {
        const int c = r.below(18), s = r.below(256);
        return Obj<N>(r, d, fillDm, [&](uint8* b) { dm(b).SetDoorStatus_Component((tComponent)c, (eDoorStatus)s); return 0u; },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, int, int)>(0x6C21E0)(b, 0, c, s); return 0u; }, 2);
    });
    Run("CDamageManager::GetDoorStatus_Component 0x6C2250", [&](Rng& r, std::string& d) {
        const int c = r.below(18);
        return Obj<N>(r, d, fillDm, [&](uint8* b) { return (uint32_t)dm(b).GetDoorStatus_Component((tComponent)c); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<int __fastcall(void*, int, int)>(0x6C2250)(b, 0, c); });
    });
    Run("CDamageManager::SetEngineStatus/GetEngineStatus 0x6C22A0/0x6C22C0", [&](Rng& r, std::string& d) {
        const uint32_t s = r.below(3) ? r.below(300) : r.u32();
        return Obj<N>(r, d, fillDm, [&](uint8* b) { dm(b).SetEngineStatus(s); return dm(b).GetEngineStatus(); },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, uint32_t)>(0x6C22A0)(b, 0, s); return oracle::Fn<uint32_t __fastcall(void*, int)>(0x6C22C0)(b, 0); });
    });
    Run("CDamageManager::Set/GetAeroplaneCompStatus 0x6C22D0/0x6C2300", [&](Rng& r, std::string& d) {
        const int f = 12 + r.below(16), s = r.below(4), f2 = 12 + r.below(16);
        return Obj<N>(r, d, fillDm, [&](uint8* b) { dm(b).SetAeroplaneCompStatus((uint8)f, (ePanelDamageState)s); return (uint32_t)dm(b).GetAeroplaneCompStatus((uint8)f2); },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, int, int)>(0x6C22D0)(b, 0, f, s); return (uint32_t)oracle::Fn<uint32_t __fastcall(void*, int, int)>(0x6C2300)(b, 0, f2) & 0xFF; }, 0, 0xFF);
    });
    Run("CDamageManager::ProgressWheelDamage 0x6C2440", [&](Rng& r, std::string& d) {
        const int w = r.below(4);
        return Obj<N>(r, d, fillDm, [&](uint8* b) { return (uint32_t)dm(b).ProgressWheelDamage((eCarWheel)w); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, int)>(0x6C2440)(b, 0, w); }, 0, 0xFF);
    });
    Run("CDamageManager::ProgressAeroplaneDamage 0x6C2460", [&](Rng& r, std::string& d) {
        const int f = 12 + r.below(16);
        return Obj<N>(r, d, fillDm, [&](uint8* b) { return (uint32_t)dm(b).ProgressAeroplaneDamage((uint8)f); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, int)>(0x6C2460)(b, 0, f); }, 0, 0xFF);
    });
    Run("CDamageManager::ProgressPanelDamage 0x6C23C0", [&](Rng& r, std::string& d) {
        const int p = r.below(7); const unsigned seed = r.u32();
        return Obj<N>(r, d, fillDm, [&](uint8* b) { SeedBoth(seed); return (uint32_t)dm(b).ProgressPanelDamage((ePanels)p); },
            [&](uint8* b) { SeedBoth(seed); return (uint32_t)oracle::Fn<bool __fastcall(void*, int, int)>(0x6C23C0)(b, 0, p); }, 0, 0xFF);
    });
    Run("CDamageManager::FuckCarCompletely 0x6C25D0", [&](Rng& r, std::string& d) {
        const int dont = r.below(2); const unsigned seed = r.u32();
        return Obj<N>(r, d, fillDm, [&](uint8* b) { SeedBoth(seed); dm(b).FuckCarCompletely(dont); return 0u; },
            [&](uint8* b) { SeedBoth(seed); oracle::Fn<void __fastcall(void*, int, int)>(0x6C25D0)(b, 0, dont); return 0u; }, 2);
    });
    Run("CDamageManager::GetCarNodeIndexFromPanel 0x6C26A0", [](Rng& r, std::string& d) {
        const int p = r.below(7);
        const int a = (int)CDamageManager::GetCarNodeIndexFromPanel((ePanels)p), e = oracle::Fn<int __cdecl(int)>(0x6C26A0)(p);
        if (a == e) return true; d = "panel " + std::to_string(p) + " got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    });
    Run("CDamageManager::GetCarNodeIndexFromDoor 0x6C26F0", [](Rng& r, std::string& d) {
        const int p = r.below(6);
        const int a = (int)CDamageManager::GetCarNodeIndexFromDoor((eDoors)p), e = oracle::Fn<int __cdecl(int)>(0x6C26F0)(p);
        if (a == e) return true; d = "door " + std::to_string(p) + " got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    });
    Run("CDamageManager::GetComponentGroup 0x6C2040", [](Rng& r, std::string& d) {
        const int c = r.below(18);
        tComponentGroup ga{}; uint8 ia = 0x77; uint32_t ge = 0; uint8 ie = 0x77;
        const bool a = CDamageManager::GetComponentGroup((tComponent)c, ga, ia);
        const bool e = oracle::Fn<bool __cdecl(int, uint32_t&, uint8&)>(0x6C2040)(c, ge, ie);
        if (a == e && (uint32_t)ga == ge && ia == ie) return true;
        d = "comp " + std::to_string(c) + " ret " + std::to_string(a) + "/" + std::to_string(e) + " group " + std::to_string((int)ga) + "/" + std::to_string(ge) + " idx " + std::to_string(ia) + "/" + std::to_string(ie); return false;
    });
}

static void TestVehicles() {
    TestDamageManager();
}
