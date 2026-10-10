// (d) vehicles: CDamageManager, cTransmission, cHandlingDataMgr, CVehicle / CAutomobile / ... physics against the exe (see gameplay_oracle_test.cpp).
// Helpers: namespace wfx (gameplay_oracle_weapon.inc).
#include "DamageManager.h"
#include "cTransmission.h"
#include "CarEnterExit.h"
#include "cHandlingDataMgr.h"
#include "Entity/Vehicle/Plane.h"
#include "Entity/Vehicle/Heli.h"
#include "Entity/Vehicle/Boat.h"
#include "Entity/Vehicle/Bike.h"
#include "Entity/Vehicle/Bmx.h"
#include "Entity/Vehicle/MonsterTruck.h"
#include "Entity/Vehicle/QuadBike.h"
#include "Entity/Vehicle/Trailer.h"
#include "Entity/Vehicle/Train.h"
#include "Entity/Physical.h"
#include "Models/ModelInfo.h"
#include "Collision/ColModel.h"
#include "Weather.h"
#include "Cheat.h"
#include "Entity/Vehicle/Automobile.h"

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
        const bool e = oracle::Fn<bool __stdcall(int, uint32_t&, uint8&)>(0x6C2040)(c, ge, ie);
        if (a == e && (uint32_t)ga == ge && ia == ie) return true;
        d = "comp " + std::to_string(c) + " ret " + std::to_string(a) + "/" + std::to_string(e) + " group " + std::to_string((int)ga) + "/" + std::to_string(ge) + " idx " + std::to_string(ia) + "/" + std::to_string(ie); return false;
    });
}

static void GenTransmission(Rng& r, uint8* b) {   // cTransmission at b+0 (0x68 bytes); the gear table is the real InitGearRatios output, sometimes perturbed
    using namespace wfx;
    FillU(r, b, 0x68);
    auto& t = *reinterpret_cast<cTransmission*>(b);
    t.m_nNumberOfGears = (uint8)(1 + r.below(5));   // the gear array has 6 entries (0 = reverse): at most 5 forward gears
    t.m_MaxVelocity = r.below(6) ? 0.3f + r.f01() * 2.5f : GenF(r, 3.f);
    t.m_MaxReverseVelocity = r.below(6) ? -0.05f - r.f01() * 0.5f : GenF(r, 1.f);
    t.m_EngineAcceleration = r.below(6) ? 0.05f + r.f01() * 2.f : GenF(r, 3.f);
    t.m_EngineInertia = r.below(6) ? r.f01() * 40.f : GenF(r, 60.f);
    t.m_handlingFlags = r.below(3) == 0 ? r.u32() : (r.below(3) ? 0u : (r.below(2) ? 1u : 2u));
    if (std::isfinite(t.m_MaxVelocity) && t.m_MaxVelocity > 0.f) {
        notsa::GameSRand(1);
        t.InitGearRatios();
        if (r.below(4) == 0) { const float k = 0.5f + r.f01(); for (auto& g : t.m_aGears) { g.MaxVelocity *= k; g.ChangeUpVelocity *= k; g.ChangeDownVelocity *= k; } }   // uniform: keeps up(g) > down(g+1), so the shift loop cannot ping-pong
        t.m_aGears[t.m_nNumberOfGears].ChangeUpVelocity = t.m_MaxVelocity;   // keeps the shift-up loop finite
    } else {
        notsa::GameSRand(1);
        t.m_MaxVelocity = 1.f; t.InitGearRatios();   // non-finite top speed: keep a finite monotonic gear table, only the scalar fields are wild
        t.m_MaxVelocity = GenF(r, 3.f);
        t.m_aGears[t.m_nNumberOfGears].ChangeUpVelocity = t.m_MaxVelocity;
    }
}

static void TestTransmission() {
    using namespace wfx;
    Run("cTransmission::InitGearRatios 0x6D0460", [](Rng& r, std::string& d) {
        return Obj<0x68>(r, d, [](Rng& rr, uint8* b) { FillU(rr, b, 0x68); auto& t = *reinterpret_cast<cTransmission*>(b); t.m_nNumberOfGears = (uint8)(1 + rr.below(5));
                t.m_MaxVelocity = rr.below(5) ? 0.3f + rr.f01() * 2.5f : GenF(rr, 5.f); t.m_MaxReverseVelocity = rr.below(5) ? -rr.f01() : GenF(rr, 2.f); },
            [](uint8* b) { reinterpret_cast<cTransmission*>(b)->InitGearRatios(); return 0u; },
            [](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6D0460)(b, 0); return 0u; }, 2);   // (the globals at 0xC1CB30/34 hold pointers into the buffer: not compared)
    });
    Run("cTransmission::CalculateGearForSimpleCar 0x6D0530", [](Rng& r, std::string& d) {
        const float speed = r.below(4) ? GenF(r, 3.f) : GenF(r, PickScale(r));
        return Obj<0x70>(r, d, [&](Rng& rr, uint8* b) { GenTransmission(rr, b); Put<uint8>(b, 0x68, (uint8)rr.below(6)); },
            [&](uint8* b) { reinterpret_cast<cTransmission*>(b)->CalculateGearForSimpleCar(speed, b[0x68]); return 0u; },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, float, uint8*)>(0x6D0530)(b, 0, speed, b + 0x68); return 0u; }, 2);
    });
    Run("cTransmission::CalculateDriveAcceleration 0x6D05E0", [](Rng& r, std::string& d) {
        // buffer: transmission(0x68) | gas @0x68 | gear @0x6C | gearChange @0x70 | velocity @0x74 | a6 @0x78 | a7 @0x7C | flags: wheels @0x80, cheat @0x81, useA @0x82
        const float ts = r.below(6) ? 0.2f + r.f01() * 2.f : GenF(r, 3.f);
        const int cheat = r.below(3) == 0 ? r.below(4) : 0;
        return Obj<0x90>(r, d, [&](Rng& rr, uint8* b) {
                GenTransmission(rr, b);
                auto& t = *reinterpret_cast<cTransmission*>(b);
                Put<float>(b, 0x68, rr.below(5) ? rr.f01() * 2.f - 1.f : GenF(rr, 2.f));
                Put<uint32_t>(b, 0x6C, rr.below(t.m_nNumberOfGears + 1));
                Put<float>(b, 0x70, GenF(rr, 5.f));
                const float lo = std::isfinite(t.m_MaxReverseVelocity) ? t.m_MaxReverseVelocity : -1.f, hi = std::isfinite(t.m_MaxVelocity) ? t.m_MaxVelocity : 1.f;
                Put<float>(b, 0x74, rr.below(8) ? lo + (hi - lo) * rr.f01() : GenF(rr, 3.f));
                Put<float>(b, 0x78, rr.below(2) ? rr.f01() : GenF(rr, 2.f));
                Put<float>(b, 0x7C, rr.below(2) ? rr.f01() : GenF(rr, 2.f));
                Put<uint8>(b, 0x80, (uint8)rr.below(3));
                Put<uint8>(b, 0x82, (uint8)rr.below(3));
                CTimer::ms_fTimeStep = ts;
            },
            [&](uint8* b) { auto& t = *reinterpret_cast<cTransmission*>(b); const bool ab = b[0x82] != 0;
                return FB(t.CalculateDriveAcceleration(*(float*)(b + 0x68), b[0x6C], *(float*)(b + 0x70), *(float*)(b + 0x74), ab ? (float*)(b + 0x78) : nullptr, ab ? (float*)(b + 0x7C) : nullptr, b[0x80], (uint8)cheat)); },
            [&](uint8* b) { const bool ab = b[0x82] != 0;
                return FB(oracle::Fn<float __fastcall(void*, int, float*, uint8*, float*, float*, float*, float*, uint8, uint8)>(0x6D05E0)(b, 0, (float*)(b + 0x68), b + 0x6C, (float*)(b + 0x70), (float*)(b + 0x74), ab ? (float*)(b + 0x78) : nullptr, ab ? (float*)(b + 0x7C) : nullptr, b[0x80], (uint8)cheat)); },
            1, 0, 0xC1CB38, 0xC);
    });
}

// ---- fake vehicle: raw buffer with the real CVehicle / CAutomobile layout (0x988 bytes), a CMatrix at +0xA00 and a tHandlingData at +0xA50 (wired by VehWire), shared fake peds
namespace vfx {
inline uint8 g_ped[10][0x600];
constexpr size_t SZ = 0x5A0, MAT = 0xA00, HND = 0xA50, ALL = 0xC00;   // CAutomobile is 0x988 bytes
inline void Wire(uint8* b, bool on) {
    using namespace wfx;
    Put<uint32_t>(b, 0x14, on ? (uint32_t)(uintptr_t)(b + MAT) : 0u);        // CPlaceable::m_matrix
    Put<uint32_t>(b, 0x384, on ? (uint32_t)(uintptr_t)(b + HND) : 0u);      // m_pHandlingData
}
inline void Gen(Rng& r, uint8* b) {
    using namespace wfx;
    FillU(r, b, ALL);
    for (int i = 0; i < 10; ++i) { std::memset(g_ped[i], 0, sizeof g_ped[i]); Put<int16_t>(g_ped[i], 0x22, (int16_t)(400 + r.below(220))); Put<uint32_t>(g_ped[i], 0x598, r.below(8)); Put<uint8>(g_ped[i], 0x484, (uint8)r.below(4)); Put<uint32_t>(g_ped[i], 0x530, r.below(3) ? (r.below(2) ? 0x32 : 0x37) : r.below(80)); }
    auto pedp = [&]() -> uint32_t { return r.below(2) ? 0u : (uint32_t)(uintptr_t)g_ped[r.below(10)]; };
    Put<uint32_t>(b, 0x460, pedp());
    for (int i = 0; i < 8; ++i) Put<uint32_t>(b, 0x464 + 4 * i, pedp());
    Put<int16_t>(b, 0x22, (int16_t)(r.below(3) ? 400 + r.below(220) : (int)(r.u32() & 0xFFFF)));
    Put<uint32_t>(b, 0x4F8, r.below(9));            // m_nDoorLock
    Put<uint32_t>(b, 0x590, r.below(12));           // m_nVehicleType
    Put<uint32_t>(b, 0x594, r.below(12));           // m_nVehicleSubType
    Put<uint8>(b, 0x484, (uint8)(r.below(3) ? 0 : r.below(5)));   // m_nNumPassengers
    float m[16]; FillF(r, (uint8*)m, 64, 1.f);
    if (r.below(3)) { const float h = r.f01() * 6.2831853f, p = (r.f01() - 0.5f) * 3.f; m[0] = std::cos(h); m[1] = std::sin(h); m[2] = 0; m[4] = -std::sin(h) * std::cos(p); m[5] = std::cos(h) * std::cos(p); m[6] = std::sin(p); m[8] = -m[1] * m[6]; m[9] = m[0] * m[6]; m[10] = std::cos(p); }
    std::memcpy(b + MAT, m, 64);
    Put<uint32_t>(b, MAT + 0x4C, 0);
    Wire(b, false);
}
inline void Fix(uint8* b, bool on) { Wire(b, on); }
}

static void TestVehicleSmall() {
    using namespace wfx;
    using V = CVehicle;
    auto veh = [](uint8* b) -> V& { return *reinterpret_cast<V*>(b); };
    auto init = [](Rng& rr, uint8* b) { vfx::Gen(rr, b); };
    auto RunV = [&](const char* name, auto&& fp, auto&& fe, int kind = 0, uint32_t mask = 0xFF) {
        Run(name, [&, kind, mask](Rng& r, std::string& d) { return Obj<vfx::ALL>(r, d, init, fp, fe, kind, mask, 0, 0, vfx::Fix); });
    };
    (void)RunV;
    Run("CVehicle::IsPassenger(CPed*) 0x6D1BD0", [&](Rng& r, std::string& d) {
        const int k = r.below(11);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::IsPassenger(k == 10 ? nullptr : (CPed*)vfx::g_ped[k]); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, void*)>(0x6D1BD0)(b, 0, k == 10 ? nullptr : vfx::g_ped[k]); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::IsPassenger(model) 0x6D1C00", [&](Rng& r, std::string& d) {
        const int m = 400 + r.below(220);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::IsPassenger((int32)m); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, int)>(0x6D1C00)(b, 0, m); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::IsDriver(CPed*) 0x6D1C40", [&](Rng& r, std::string& d) {
        const int k = r.below(11);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::IsDriver(k == 10 ? nullptr : (CPed*)vfx::g_ped[k]); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, void*)>(0x6D1C40)(b, 0, k == 10 ? nullptr : vfx::g_ped[k]); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::IsDriver(model) 0x6D1C60", [&](Rng& r, std::string& d) {
        const int m = 400 + r.below(220);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::IsDriver((int32)m); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, int)>(0x6D1C60)(b, 0, m); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::IsUpsideDown 0x6D1D90", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::IsUpsideDown(); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x6D1D90)(b, 0); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::IsOnItsSide 0x6D1DD0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::IsOnItsSide(); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x6D1DD0)(b, 0); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::CanDoorsBeDamaged 0x6D1E60", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::CanDoorsBeDamaged(); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x6D1E60)(b, 0); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::CanPedEnterCar 0x6D1E80", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::CanPedEnterCar(); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x6D1E80)(b, 0); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::IsLawEnforcementVehicle 0x6D2370", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::IsLawEnforcementVehicle(); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x6D2370)(b, 0); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::UsesSiren 0x6D8470", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::UsesSiren(); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x6D8470)(b, 0); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::CarHasRoof 0x6D25D0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::CarHasRoof(); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x6D25D0)(b, 0); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::HeightAboveCeiling 0x6D2600", [&](Rng& r, std::string& d) {
        static const float k[] = { 500.f, 950.f, 1500.f, 800.f, 1000.f };
        const float h = r.below(3) ? k[r.below(5)] + (r.below(3) - 1) * (r.below(2) ? 0.f : 1e-3f) : GenF(r, 2500.f);
        const int fm = r.below(4);
        return Obj<0x20>(r, d, [](Rng&, uint8*) {}, [&](uint8* b) { return FB(veh(b).CVehicle::HeightAboveCeiling(h, (eFlightModel)fm)); },
            [&](uint8* b) { return FB(oracle::Fn<float __fastcall(void*, int, float, int)>(0x6D2600)(b, 0, h, fm)); }, 1);
    });
    Run("CVehicle::UpdatePassengerList 0x6D29E0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); if (rr.below(2)) for (int i = 0; i < 8; ++i) wfx::Put<uint32_t>(b, 0x464 + 4 * i, 0); },
            [&](uint8* b) { veh(b).CVehicle::UpdatePassengerList(); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6D29E0)(b, 0); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CVehicle::SetGettingInFlags 0x6D3000", [&](Rng& r, std::string& d) {
        const uint8 f = (uint8)r.below(256);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { veh(b).CVehicle::SetGettingInFlags(f); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, uint8)>(0x6D3000)(b, 0, f); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CVehicle::SetGettingOutFlags 0x6D3020", [&](Rng& r, std::string& d) {
        const uint8 f = (uint8)r.below(256);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { veh(b).CVehicle::SetGettingOutFlags(f); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, uint8)>(0x6D3020)(b, 0, f); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CVehicle::ClearGettingInFlags 0x6D3040", [&](Rng& r, std::string& d) {
        const uint8 f = (uint8)r.below(256);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { veh(b).CVehicle::ClearGettingInFlags(f); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, uint8)>(0x6D3040)(b, 0, f); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CVehicle::ClearGettingOutFlags 0x6D3060", [&](Rng& r, std::string& d) {
        const uint8 f = (uint8)r.below(256);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { veh(b).CVehicle::ClearGettingOutFlags(f); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, uint8)>(0x6D3060)(b, 0, f); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CVehicle::GetSpareHasslePosId 0x6D3AE0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::GetSpareHasslePosId(); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<int __fastcall(void*, int)>(0x6D3AE0)(b, 0); }, 0, 0xFFFFFFFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::SetHasslePosId 0x6D3B30", [&](Rng& r, std::string& d) {
        const int pos = r.below(8); const bool en = r.below(2);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { veh(b).CVehicle::SetHasslePosId(pos, en); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, int, bool)>(0x6D3B30)(b, 0, pos, en); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CVehicle::InitWinch 0x6D3B60", [&](Rng& r, std::string& d) {
        const int t = r.below(4);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { veh(b).CVehicle::InitWinch(t); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, int)>(0x6D3B60)(b, 0, t); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CVehicle::GetPlaneGunsAutoAimAngle 0x6D3E00", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return FB(veh(b).CVehicle::GetPlaneGunsAutoAimAngle()); },
            [&](uint8* b) { return FB(oracle::Fn<float __fastcall(void*, int)>(0x6D3E00)(b, 0)); }, 1, 0, 0, 0, vfx::Fix);
    });
    Run("CVehicle::GetPlaneNumGuns 0x6D3F30", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::GetPlaneNumGuns(); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<int __fastcall(void*, int)>(0x6D3F30)(b, 0); }, 0, 0xFFFFFFFF, 0, 0, vfx::Fix);
    });
}

static void TestAutomobileSmall() {
    using namespace wfx;
    using A = CAutomobile;
    auto au = [](uint8* b) -> A& { return *reinterpret_cast<A*>(b); };
    auto init = [](Rng& rr, uint8* b) { vfx::Gen(rr, b); if (rr.below(3) == 0) for (int i = 0; i < 4; ++i) Put<float>(b, 0x7D4 + 4 * i, rr.below(3) ? 1.f : GenF(rr, 1.5f)); };
#define AUTO_RUN(NAME, ADDR, KIND, MASK, EXPR_PORT, EXPR_EXE) \
    Run(NAME " " ADDR, [&](Rng& r, std::string& d) { return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)(EXPR_PORT); }, [&](uint8* b) { return (uint32_t)(EXPR_EXE); }, KIND, MASK, 0, 0, vfx::Fix); })
    Run("CAutomobile::IsComponentPresent 0x6A2250", [&](Rng& r, std::string& d) {
        const int c = r.below(25);
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); for (int i = 0; i < 25; ++i) if (rr.below(2)) Put<uint32_t>(b, 0x648 + 4 * i, 0); },
            [&](uint8* b) { return (uint32_t)au(b).CAutomobile::IsComponentPresent(c); }, [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, int)>(0x6A2250)(b, 0, c); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::GetDooorAngleOpenRatio 0x6A2270", [&](Rng& r, std::string& d) {
        const int c = r.below(6);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return FB(au(b).CAutomobile::GetDooorAngleOpenRatio((eDoors)c)); }, [&](uint8* b) { return FB(oracle::Fn<float __fastcall(void*, int, int)>(0x6A2270)(b, 0, c)); }, 1, 0, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::IsDoorClosed 0x6A2310", [&](Rng& r, std::string& d) {
        const int c = r.below(6);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)au(b).CAutomobile::IsDoorClosed((eDoors)c); }, [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, int)>(0x6A2310)(b, 0, c); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::IsDoorMissing 0x6A2330", [&](Rng& r, std::string& d) {
        const int c = r.below(6);
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); for (int i = 0; i < 6; ++i) Put<uint8>(b, 0x5A0 + 9 + i, (uint8)rr.below(5)); },
            [&](uint8* b) { return (uint32_t)au(b).CAutomobile::IsDoorMissing((eDoors)c); }, [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, int)>(0x6A2330)(b, 0, c); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::IsOpenTopCar 0x6A2350", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); if (rr.below(2)) Put<int16_t>(b, 0x22, (int16_t)(rr.below(2) ? 0x1e0 : 0x1b7)); Put<uint8>(b, 0x438, (uint8)rr.below(2)); Put<uint8>(b, 0x439, (uint8)rr.below(2)); },
            [&](uint8* b) { return (uint32_t)au(b).CAutomobile::IsOpenTopCar(); }, [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x6A2350)(b, 0); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::ReduceHornCounter 0x6A29A0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); if (rr.below(2)) Put<uint32_t>(b, 0x514, rr.below(4) ? rr.below(5) : rr.u32()); },
            [&](uint8* b) { au(b).CAutomobile::ReduceHornCounter(); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6A29A0)(b, 0); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::ResetSuspension 0x6A2AE0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { au(b).CAutomobile::ResetSuspension(); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6A2AE0)(b, 0); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::GetAllWheelsOffGround 0x6A2F70", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); if (rr.below(2)) Put<uint8>(b, 0x961, 0); },
            [&](uint8* b) { return (uint32_t)au(b).CAutomobile::GetAllWheelsOffGround(); }, [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x6A2F70)(b, 0); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::FixTyre 0x6A3580", [&](Rng& r, std::string& d) {
        const int w = r.below(4);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { au(b).CAutomobile::FixTyre((eWheels)w); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, int)>(0x6A3580)(b, 0, w); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::SetTaxiLight 0x6A3740", [&](Rng& r, std::string& d) {
        const bool e = r.below(2);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { au(b).CAutomobile::SetTaxiLight(e); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, bool)>(0x6A3740)(b, 0, e); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::IsInAir 0x6A6140", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); Put<uint32_t>(b, 0x40, rr.u32()); for (int i = 0; i < 4; ++i) Put<float>(b, 0x7D4 + 4 * i, rr.below(2) ? 1.f : (rr.below(5) ? 1.f + rr.f01() : GenF(rr, 2.f)));
                for (int i = 0; i < 3; ++i) if (rr.below(3) == 0) Put<float>(b, 0x44 + 4 * i, 0.f); },
            [&](uint8* b) { return (uint32_t)au(b).CAutomobile::IsInAir(); }, [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x6A6140)(b, 0); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::GetCarRoll 0x6A6010", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return FB(au(b).CAutomobile::GetCarRoll()); }, [&](uint8* b) { return FB(oracle::Fn<float __fastcall(void*, int)>(0x6A6010)(b, 0)); }, 1, 0, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::GetCarPitch 0x6A6050", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return FB(au(b).CAutomobile::GetCarPitch()); }, [&](uint8* b) { return FB(oracle::Fn<float __fastcall(void*, int)>(0x6A6050)(b, 0)); }, 1, 0, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::PopBootUsingPhysics 0x6A44D0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); Put<uint8>(b, 0x5A0 + 10, (uint8)rr.below(5)); },
            [&](uint8* b) { au(b).CAutomobile::PopBootUsingPhysics(); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6A44D0)(b, 0); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::GetNumContactWheels 0x6A62A0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)au(b).CAutomobile::GetNumContactWheels(); }, [&](uint8* b) { return oracle::Fn<uint32_t __fastcall(void*, int)>(0x6A62A0)(b, 0); }, 0, 0xFFFFFFFF, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::GetHeightAboveRoad 0x6A62B0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return FB(au(b).CAutomobile::GetHeightAboveRoad()); }, [&](uint8* b) { return FB(oracle::Fn<float __fastcall(void*, int)>(0x6A62B0)(b, 0)); }, 1, 0, 0, 0, vfx::Fix);
    });
#undef AUTO_RUN
}

static void TestCarEnterExit() {
    using namespace wfx;
    auto init = [](Rng& rr, uint8* b) { vfx::Gen(rr, b); if (rr.below(2)) Put<uint8>(b, vfx::HND + 0xDE, (uint8)(rr.below(2) ? 0x0F : 0x10)); if (rr.below(3) == 0) Put<uint8>(b, 0x429, (uint8)(rr.u32() | 2)); };
    const int doors[] = { 8, 9, 10, 11, 18 };
    auto veh = [](uint8* b) { return reinterpret_cast<const CVehicle*>(b); };
    Run("CCarEnterExit::ComputeDoorFlag 0x64E550", [&](Rng& r, std::string& d) {
        const int door = doors[r.below(5)]; const bool f = r.below(2);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)CCarEnterExit::ComputeDoorFlag(veh(b), door, f); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<int __cdecl(void*, int, bool)>(0x64E550)(b, door, f); }, 0, 0xFFFFFFFF, 0, 0, vfx::Fix);
    });
    Run("CCarEnterExit::ComputeOppositeDoorFlag 0x64E610", [&](Rng& r, std::string& d) {
        const int door = doors[r.below(5)]; const bool f = r.below(2);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)CCarEnterExit::ComputeOppositeDoorFlag(veh(b), door, f); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<int __cdecl(void*, int, bool)>(0x64E610)(b, door, f); }, 0, 0xFFFFFFFF, 0, 0, vfx::Fix);
    });
    Run("CCarEnterExit::ComputePassengerIndexFromCarDoor 0x64F1E0", [&](Rng& r, std::string& d) {
        const int door = r.below(4) == 0 ? (int)r.below(25) : doors[r.below(5)];
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)CCarEnterExit::ComputePassengerIndexFromCarDoor(veh(b), door); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<int __cdecl(void*, int)>(0x64F1E0)(b, door); }, 0, 0xFFFFFFFF, 0, 0, vfx::Fix);
    });
    Run("CCarEnterExit::ComputeSlowJackedPed 0x64F070", [&](Rng& r, std::string& d) {
        const int door = r.below(4) == 0 ? (int)r.below(25) : doors[r.below(5)];
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)(uintptr_t)CCarEnterExit::ComputeSlowJackedPed(veh(b), door); },
            [&](uint8* b) { return (uint32_t)(uintptr_t)oracle::Fn<void* __cdecl(void*, int)>(0x64F070)(b, door); }, 0, 0xFFFFFFFF, 0, 0, vfx::Fix);
    });
    Run("CCarEnterExit::ComputeTargetDoorToEnterAsPassenger 0x64F190", [&](Rng& r, std::string& d) {
        const int idx = r.below(5) - 1;
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(veh(b), idx); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<int __cdecl(void*, int)>(0x64F190)(b, idx); }, 0, 0xFFFFFFFF, 0, 0, vfx::Fix);
    });
    Run("CCarEnterExit::IsCarDoorInUse 0x64EC90", [&](Rng& r, std::string& d) {
        const int d1 = r.below(4) == 0 ? (int)r.below(25) : doors[r.below(5)], d2 = r.below(4) == 0 ? (int)r.below(25) : doors[r.below(5)];
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)CCarEnterExit::IsCarDoorInUse(veh(b), d1, d2); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __cdecl(void*, int, int)>(0x64EC90)(b, d1, d2); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CCarEnterExit::IsCarSlowJackRequired 0x64EF70", [&](Rng& r, std::string& d) {
        const int door = r.below(4) == 0 ? (int)r.below(25) : doors[r.below(5)];
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)CCarEnterExit::IsCarSlowJackRequired(veh(b), door); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __cdecl(void*, int)>(0x64EF70)(b, door); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CCarEnterExit::IsVehicleHealthy 0x64EEC0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)CCarEnterExit::IsVehicleHealthy(veh(b)); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __cdecl(void*)>(0x64EEC0)(b); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CCarEnterExit::IsPedHealthy 0x64EEE0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { wfx::FillU(rr, b, vfx::ALL); wfx::Put<float>(b, 0x540, rr.below(3) ? GenF(rr, 100.f) : 0.f); },
            [&](uint8* b) { return (uint32_t)CCarEnterExit::IsPedHealthy(reinterpret_cast<CPed*>(b)); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __cdecl(void*)>(0x64EEE0)(b); }, 0, 0xFF);
    });
}

static void TestPlaneHelpers() {
    using namespace wfx;
    auto veh = [](uint8* b) -> CVehicle& { return *reinterpret_cast<CVehicle*>(b); };
    static const int models[] = { 447, 464, 476, 520, 548, 513, 425, 487, 497, 465, 460, 511, 512, 519, 553, 577, 592, 593 };
    auto init = [](Rng& rr, uint8* b) {
        vfx::Gen(rr, b);
        if (rr.below(5) < 4) Put<int16_t>(b, 0x22, (int16_t)models[rr.below(18)]);
        Put<uint32_t>(b, 0x594, rr.below(3) ? 3 + rr.below(2) : rr.below(12));
        Put<uint8>(b, 0x9D8, (uint8)(rr.below(3) ? 1 + rr.below(255) : 0)); Put<uint8>(b, 0xA10, (uint8)(rr.below(3) ? 1 + rr.below(255) : 0));
    };
    Run("CVehicle::GetFiringRateMultiplier 0x6D4090", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return FB(veh(b).CVehicle::GetFiringRateMultiplier()); }, [&](uint8* b) { return FB(oracle::Fn<float __fastcall(void*, int)>(0x6D4090)(b, 0)); }, 1, 0, 0, 0, vfx::Fix);
    });
    Run("CVehicle::SetFiringRateMultiplier 0x6D4010", [&](Rng& r, std::string& d) {
        const float m = r.below(2) ? GenF(r, 20.f) : GenF(r, 3.f);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { veh(b).CVehicle::SetFiringRateMultiplier(m); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, float)>(0x6D4010)(b, 0, m); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CVehicle::GetPlaneGunsRateOfFire 0x6D40E0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::GetPlaneGunsRateOfFire(); }, [&](uint8* b) { return oracle::Fn<uint32_t __fastcall(void*, int)>(0x6D40E0)(b, 0); }, 0, 0xFFFFFFFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::GetPlaneOrdnanceRateOfFire 0x6D4590", [&](Rng& r, std::string& d) {
        const int t = r.below(3);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::GetPlaneOrdnanceRateOfFire((eOrdnanceType)t); }, [&](uint8* b) { return oracle::Fn<uint32_t __fastcall(void*, int, int)>(0x6D4590)(b, 0, t); }, 0, 0xFFFFFFFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::SelectPlaneWeapon 0x6D4900", [&](Rng& r, std::string& d) {
        const bool ch = r.below(2); const int t = r.below(3);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { veh(b).CVehicle::SelectPlaneWeapon(ch, (eOrdnanceType)t); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, bool, int)>(0x6D4900)(b, 0, ch, t); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
}

static void TestVehicleMisc() {
    using namespace wfx;
    auto veh = [](uint8* b) -> CVehicle& { return *reinterpret_cast<CVehicle*>(b); };
    auto init = [](Rng& rr, uint8* b) { vfx::Gen(rr, b); Put<uint8>(b, 0x4A4, (uint8)rr.below(7)); Put<uint8>(b, 0x485, (uint8)(rr.below(4) ? 0 : rr.below(3))); Put<uint8>(b, 0x487, (uint8)(rr.below(4) ? 0 : 1)); };
    Run("CVehicle::CanBeDeleted 0x6D1180", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { Hit(true); return (uint32_t)veh(b).CVehicle::CanBeDeleted(); }, [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x6D1180)(b, 0); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::GetVehicleAppearance 0x6D1080", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); Put<uint32_t>(b, vfx::HND + 0xCC, rr.u32() & (rr.below(2) ? 0xFFFFFFFFu : 0xF000000u)); },
            [&](uint8* b) { return (uint32_t)veh(b).CVehicle::GetVehicleAppearance(); }, [&](uint8* b) { return oracle::Fn<uint32_t __fastcall(void*, int)>(0x6D1080)(b, 0); }, 0, 0xFFFFFFFF, 0, 0, vfx::Fix);
    });
    Run("CVehicle::FindTyreNearestPoint 0x6D7BC0", [&](Rng& r, std::string& d) {
        const float sc = r.below(2) ? 20.f : PickScale(r); const CVector2D p{ GenF(r, sc), GenF(r, sc) };
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)veh(b).CVehicle::FindTyreNearestPoint(p); },
            [&](uint8* b) { return oracle::Fn<uint32_t __fastcall(void*, int, float, float)>(0x6D7BC0)(b, 0, p.x, p.y); }, 0, 0xFFFFFFFF, 0, 0, vfx::Fix);
    });
}

static void GenHandling(Rng& rr, uint8* b) {   // tHandlingData (0xE0 bytes): realistic ranges so the exe's max-velocity search loop stays short
    using namespace wfx;
    FillU(rr, b, 0xE0);
    const bool wild = rr.below(10) == 0;
    Put<int32_t>(b, 0, rr.below(3) == 0 ? (rr.below(2) ? 0x26 : 0xA2 + rr.below(14)) : (int)rr.below(0xBE));          // vehicle id (0x26 = RC bandit, 0xA2..0xAE bikes)
    Put<float>(b, 4, wild ? GenF(rr, 3.f, S_NOHUGE) : 50.f + rr.f01() * 30000.f);                                                // mass
    Put<float>(b, 0x10, rr.below(4) == 0 ? rr.f01() * 0.02f : rr.f01() * 3.f);                                         // drag mult
    Put<uint8>(b, 0x20, (uint8)(rr.below(30) == 0 ? 0 : 1 + rr.below(120)));                                           // percent submerged
    Put<float>(b, 0x7C, wild ? GenF(rr, 5.f, S_NOHUGE) : 0.1f + rr.f01() * 40.f);                                                // engine acceleration (file units)
    Put<float>(b, 0x84, wild ? GenF(rr, 5.f, S_NOHUGE) :   // (no huge max velocity: the exe's search loop steps by 0.01 and would run for ever)
         5.f + rr.f01() * 250.f);                                                // max velocity (file units)
    Put<float>(b, 0x94, rr.f01() * 30.f);                                                                              // brake deceleration
    Put<float>(b, 0xC8, GenF(rr, 5.f));                                                                                // collision damage mult
    Put<uint8>(b, 0x74, (uint8)(rr.below(3) == 0 ? '4' : (rr.below(2) ? 'F' : 'R')));
    Put<uint8>(b, 0x76, (uint8)(1 + rr.below(5)));                                                                      // number of gears (transmission +0x4A)
    Put<uint32_t>(b, 0xD0, rr.below(3) == 0 ? 0x1000000u | (rr.u32() & 0xFFFFFF) : (rr.u32() & ~0x1000000u));
}

static void TestHandlingMgr() {
    using namespace wfx;
    // the exe keeps the unit-conversion constants in two runtime-initialised globals (static initialisers at 0x854350 / 0x854390: 1/(50*50) and 0.277778/50, single precision); the port uses constexpr floats
    constexpr uintptr_t kAccel = 0xC2B9B4, kVel = 0xC2B9BC;
    const float accelC = 1.f / (50.f * 50.f), velC = 0.277778f / 50.f;
    std::memcpy((void*)kAccel, &accelC, 4); std::memcpy((void*)kVel, &velC, 4);
    auto mgr = [](uint8* b) -> cHandlingDataMgr& { return *reinterpret_cast<cHandlingDataMgr*>(b); };
    Run("cHandlingDataMgr::ConvertDataToGameUnits 0x6F5080", [&](Rng& r, std::string& d) {
        alignas(16) uint8 dummy[0x20] {};
        return Obj<0xE0>(r, d, GenHandling, [&](uint8* b) { mgr(dummy).ConvertDataToGameUnits(reinterpret_cast<tHandlingData*>(b)); return 0u; },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, void*)>(0x6F5080)(dummy, 0, b); return 0u; }, 2);
    });
    Run("cHandlingDataMgr::ConvertDataToWorldUnits 0x6F5010", [&](Rng& r, std::string& d) {
        alignas(16) uint8 dummy[0x20] {};
        return Obj<0xE0>(r, d, GenHandling, [&](uint8* b) { mgr(dummy).ConvertDataToWorldUnits(reinterpret_cast<tHandlingData*>(b)); return 0u; },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, void*)>(0x6F5010)(dummy, 0, b); return 0u; }, 2);
    });
    // (no huge / inf angles in the fixture: FSIN leaves an operand >= 2^63 unchanged, the UCRT sin does not - an x87 transcendental difference, not a port logic bug)
    Run("cHandlingDataMgr::ConvertBikeDataToGameUnits 0x6F5290", [&](Rng& r, std::string& d) {
        alignas(16) uint8 dummy[0x20] {};
        return Obj<0x40>(r, d, [](Rng& rr, uint8* b) { FillU(rr, b, 0x40); for (int o : { 0x14, 0x18, 0x2C, 0x30 }) Put<float>(b, o, rr.below(4) ? GenF(rr, 90.f, S_NOHUGE) : GenF(rr, 1e5f, S_NOHUGE)); },
            [&](uint8* b) { mgr(dummy).ConvertBikeDataToGameUnits(reinterpret_cast<tBikeHandlingData*>(b)); return 0u; },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, void*)>(0x6F5290)(dummy, 0, b); return 0u; }, 2);
    });
    Run("cHandlingDataMgr::HasFrontWheelDrive 0x6A0480", [&](Rng& r, std::string& d) {
        const int id = r.below(4);
        return Obj<0x1000>(r, d, [](Rng& rr, uint8* b) { FillU(rr, b, 0x1000); for (int i = 0; i < 4; ++i) Put<uint8>(b, 0x14 + i * 0xE0 + 0x2C + 0x48, (uint8)(rr.below(2) ? 'R' : rr.below(256))); },
            [&](uint8* b) { return (uint32_t)mgr(b).HasFrontWheelDrive((uint8)id); }, [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, uint8)>(0x6A0480)(b, 0, (uint8)id); }, 0, 0xFF);
    });
    Run("cHandlingDataMgr::HasRearWheelDrive 0x6A04B0", [&](Rng& r, std::string& d) {
        const int id = r.below(4);
        return Obj<0x1000>(r, d, [](Rng& rr, uint8* b) { FillU(rr, b, 0x1000); for (int i = 0; i < 4; ++i) Put<uint8>(b, 0x14 + i * 0xE0 + 0x2C + 0x48, (uint8)(rr.below(2) ? 'F' : rr.below(256))); },
            [&](uint8* b) { return (uint32_t)mgr(b).HasRearWheelDrive((uint8)id); }, [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, uint8)>(0x6A04B0)(b, 0, (uint8)id); }, 0, 0xFF);
    });
    Run("cHandlingDataMgr::GetFlyingPointer 0x6F52D0", [&](Rng& r, std::string& d) {
        const int id = r.below(256);
        return Obj<0x40>(r, d, [](Rng&, uint8*) {}, [&](uint8* b) { return (uint32_t)((uintptr_t)mgr(b).GetFlyingPointer((uint8)id) - (uintptr_t)b); },
            [&](uint8* b) { return (uint32_t)((uintptr_t)oracle::Fn<void* __fastcall(void*, int, uint8)>(0x6F52D0)(b, 0, (uint8)id) - (uintptr_t)b); });
    });
    Run("cHandlingDataMgr::GetBoatPointer 0x6F5300", [&](Rng& r, std::string& d) {
        const int id = r.below(256);
        return Obj<0x40>(r, d, [](Rng&, uint8*) {}, [&](uint8* b) { return (uint32_t)((uintptr_t)mgr(b).GetBoatPointer((uint8)id) - (uintptr_t)b); },
            [&](uint8* b) { return (uint32_t)((uintptr_t)oracle::Fn<void* __fastcall(void*, int, uint8)>(0x6F5300)(b, 0, (uint8)id) - (uintptr_t)b); });
    });
    Run("cHandlingDataMgr::GetHandlingId 0x6F4FD0", [&](Rng& r, std::string& d) {
        const char* name = reinterpret_cast<const char*>(0x8D3978 + 14 * r.below(210));
        return Obj<0x40>(r, d, [](Rng&, uint8*) {}, [&](uint8* b) { return (uint32_t)mgr(b).GetHandlingId(name); }, [&](uint8* b) { return oracle::Fn<uint32_t __fastcall(void*, int, const char*)>(0x6F4FD0)(b, 0, name); });
    });
}

static void TestVehicleTypes() {
    using namespace wfx;
    auto init = [](Rng& rr, uint8* b) { vfx::Gen(rr, b); };
    Run("CHeli::FindSwatPositionRelativeToHeli 0x6C4760", [&](Rng& r, std::string& d) {
        const int n = r.below(6) - 1;
        return Obj<0x40>(r, d, [](Rng&, uint8*) {}, [&](uint8* b) { CVector v = reinterpret_cast<CHeli*>(b)->CHeli::FindSwatPositionRelativeToHeli(n); std::memcpy(b + 0x20, &v, 12); return 0u; },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, void*, int)>(0x6C4760)(b, 0, b + 0x20, n); return 0u; }, 2);
    });
    Run("CPlane::SetGearUp 0x6CAC20", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); Put<float>(b, vfx::HND + 0x10, GenF(rr, rr.below(2) ? 3.f : PickScale(rr))); Put<uint32_t>(b, 0x388, (uint32_t)(uintptr_t)(b + vfx::HND + 0x80)); Put<float>(b, vfx::HND + 0x80 + 0x30, GenF(rr, 2.f)); },
            [&](uint8* b) { reinterpret_cast<CPlane*>(b)->CPlane::SetGearUp(); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6CAC20)(b, 0); return 0u; }, 2, 0, 0, 0,
            [](uint8* b, bool on) { vfx::Wire(b, on); wfx::Put<uint32_t>(b, 0x388, on ? (uint32_t)(uintptr_t)(b + vfx::HND + 0x80) : 0u); });
    });
    Run("CPlane::SetGearDown 0x6CAC70", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); Put<float>(b, vfx::HND + 0x10, GenF(rr, rr.below(2) ? 3.f : PickScale(rr))); },
            [&](uint8* b) { reinterpret_cast<CPlane*>(b)->CPlane::SetGearDown(); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6CAC70)(b, 0); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CBoat::ModifyHandlingValue 0x6F0DE0", [&](Rng& r, std::string& d) {
        const bool plus = r.below(2);
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); Put<uint8>(b, 0x63C, (uint8)(rr.below(2) ? 4 : rr.below(8))); Put<float>(b, vfx::HND + 0xA0, GenF(rr, 100.f)); },
            [&](uint8* b) { reinterpret_cast<CBoat*>(b)->CBoat::ModifyHandlingValue(plus); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, const bool*)>(0x6F0DE0)(b, 0, &plus); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CBoat::CheckForSkippingCalculations 0x6F10C0", [&](Rng& r, std::string& d) {
        return Glob(r, d, 0xC279A4, 0x10, [&](Rng& rr, uint8* g) { for (int i = 0; i < 8; ++i) Put<int16_t>(g, i * 2, (int16_t)(rr.below(3) ? rr.below(12) - 2 : (int)rr.u32())); },
            [&] { CBoat::CheckForSkippingCalculations(); return 0u; }, [&] { oracle::Fn<void __cdecl()>(0x6F10C0)(); return 0u; }, 2);
    });
    Run("CBike::ResetSuspension 0x6B6740", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { reinterpret_cast<CBike*>(b)->CBike::ResetSuspension(); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6B6740)(b, 0); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CMonsterTruck::ResetSuspension 0x6C7D40", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { reinterpret_cast<CMonsterTruck*>(b)->CMonsterTruck::ResetSuspension(); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6C7D40)(b, 0); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CQuadBike::ResetSuspension 0x6CDCB0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { reinterpret_cast<CQuadBike*>(b)->CQuadBike::ResetSuspension(); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6CDCB0)(b, 0); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CTrailer::ResetSuspension 0x6CEE50", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); Put<float>(b, 0x9EC, rr.below(2) ? rr.f01() : GenF(rr, 2.f)); if (rr.below(2)) Put<uint32_t>(b, 0x4C4, 0); },
            [&](uint8* b) { reinterpret_cast<CTrailer*>(b)->CTrailer::ResetSuspension(); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6CEE50)(b, 0); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    // ---- trains: a chain of up to 6 carriages in one buffer (next @0x5D4, prev @0x5D0)
    constexpr size_t TS = 0x600;
    auto genTrains = [](Rng& rr, uint8* b) {
        FillU(rr, b, 6 * TS);
        for (int i = 0; i < 6; ++i) { Put<uint32_t>(b, i * TS + 0x5D4, 0); Put<uint32_t>(b, i * TS + 0x5D0, 0); }
        const int len = 1 + rr.below(6);
        for (int i = 0; i < len; ++i) { Put<uint32_t>(b, i * TS + 0x5D4, i + 1 < len ? (uint32_t)(i + 1) : 0u); Put<uint32_t>(b, i * TS + 0x5D0, i > 0 ? (uint32_t)(i) : 0u); }   // stored as index + 1 (wired by trainFix)
    };
    auto trainFix = [](uint8* b, bool on) {
        for (int i = 0; i < 6; ++i) for (size_t off : { 0x5D4u, 0x5D0u }) {
            uint32_t v = Get<uint32_t>(b, i * TS + off);
            if (on) v = v ? (uint32_t)(uintptr_t)(b + (v - (off == 0x5D4 ? 0 : 1) ) * TS) : 0u;   // placeholder, replaced below
            Put<uint32_t>(b, i * TS + off, v);
        }
    };
    (void)trainFix;
    // simpler wiring: next = chain[i+1] / prev = chain[i-1] recomputed from a stored length byte at +0x5C0 of carriage 0
    auto genChain = [](Rng& rr, uint8* b) { FillU(rr, b, 6 * TS); Put<uint8>(b, 0x5C0, (uint8)(1 + rr.below(6))); };
    auto wireChain = [](uint8* b, bool on) {
        const int len = Get<uint8>(b, 0x5C0);
        for (int i = 0; i < 6; ++i) {
            Put<uint32_t>(b, i * TS + 0x5D4, on && i + 1 < len ? (uint32_t)(uintptr_t)(b + (i + 1) * TS) : 0u);
            Put<uint32_t>(b, i * TS + 0x5D0, on && i > 0 && i < len ? (uint32_t)(uintptr_t)(b + (i - 1) * TS) : 0u);
        }
    };
    (void)genTrains;
    Run("CTrain::FindNumCarriagesPulled 0x6F5CD0", [&](Rng& r, std::string& d) {
        const int from = r.below(6);
        return Obj<6 * TS>(r, d, genChain, [&](uint8* b) { return (uint32_t)reinterpret_cast<CTrain*>(b + from * TS)->CTrain::FindNumCarriagesPulled(); },
            [&](uint8* b) { return oracle::Fn<uint32_t __fastcall(void*, int)>(0x6F5CD0)(b + from * TS, 0); }, 0, 0xFFFFFFFF, 0, 0, wireChain);
    });
    Run("CTrain::FindCaboose 0x6F5E70", [&](Rng& r, std::string& d) {
        const int from = r.below(6);
        return Obj<6 * TS>(r, d, genChain, [&](uint8* b) { return (uint32_t)((uintptr_t)CTrain::FindCaboose(reinterpret_cast<CTrain*>(b + from * TS)) - (uintptr_t)b); },
            [&](uint8* b) { return (uint32_t)((uintptr_t)oracle::Fn<void* __cdecl(void*)>(0x6F5E70)(b + from * TS) - (uintptr_t)b); }, 0, 0xFFFFFFFF, 0, 0, wireChain);
    });
    Run("CTrain::FindEngine 0x6F5E90", [&](Rng& r, std::string& d) {
        const int from = r.below(6);
        return Obj<6 * TS>(r, d, genChain, [&](uint8* b) { return (uint32_t)((uintptr_t)CTrain::FindEngine(reinterpret_cast<CTrain*>(b + from * TS)) - (uintptr_t)b); },
            [&](uint8* b) { return (uint32_t)((uintptr_t)oracle::Fn<void* __cdecl(void*)>(0x6F5E90)(b + from * TS) - (uintptr_t)b); }, 0, 0xFFFFFFFF, 0, 0, wireChain);
    });
    Run("CTrain::FindCarriage 0x6F5EB0", [&](Rng& r, std::string& d) {
        const int from = r.below(6), n = r.below(8);
        return Obj<6 * TS>(r, d, genChain, [&](uint8* b) { auto* c = CTrain::FindCarriage(reinterpret_cast<CTrain*>(b + from * TS), (uint8)n); return c ? (uint32_t)((uintptr_t)c - (uintptr_t)b) : 0xFFFFFFFFu; },
            [&](uint8* b) { auto* c = oracle::Fn<void* __cdecl(void*, uint8)>(0x6F5EB0)(b + from * TS, (uint8)n); return c ? (uint32_t)((uintptr_t)c - (uintptr_t)b) : 0xFFFFFFFFu; }, 0, 0xFFFFFFFF, 0, 0, wireChain);
    });
    Run("CTrain::ReleaseOneMissionTrain 0x6F5DF0", [&](Rng& r, std::string& d) {
        const int from = r.below(7);
        return Obj<6 * TS>(r, d, genChain, [&](uint8* b) { CTrain::ReleaseOneMissionTrain(from == 6 ? nullptr : reinterpret_cast<CTrain*>(b + from * TS)); return 0u; },
            [&](uint8* b) { oracle::Fn<void __cdecl(void*)>(0x6F5DF0)(from == 6 ? nullptr : b + from * TS); return 0u; }, 2, 0, 0, 0, wireChain);
    });
    Run("CTrain::SetTrainSpeed 0x6F5E20", [&](Rng& r, std::string& d) {
        const float sp = r.below(2) ? GenF(r, 100.f) : GenF(r, PickScale(r));
        return Obj<TS>(r, d, [](Rng& rr, uint8* b) { FillU(rr, b, TS); }, [&](uint8* b) { CTrain::SetTrainSpeed(reinterpret_cast<CTrain*>(b), sp); return 0u; },
            [&](uint8* b) { oracle::Fn<void __cdecl(void*, float)>(0x6F5E20)(b, sp); return 0u; }, 2);
    });
    Run("CTrain::SetTrainCruiseSpeed 0x6F5E50", [&](Rng& r, std::string& d) {
        const float sp = r.below(2) ? GenF(r, 255.f) : GenF(r, 1000.f, S_NAN | S_ZERO | S_DEN | S_TINY);
        return Obj<TS>(r, d, [](Rng& rr, uint8* b) { FillU(rr, b, TS); }, [&](uint8* b) { CTrain::SetTrainCruiseSpeed(reinterpret_cast<CTrain*>(b), sp); return 0u; },
            [&](uint8* b) { oracle::Fn<void __cdecl(void*, float)>(0x6F5E50)(b, sp); return 0u; }, 2);
    });
    Run("CTrain::FindSideStationIsOn 0x6F5EF0", [&](Rng& r, std::string& d) {
        return Obj<TS>(r, d, [](Rng& rr, uint8* b) { FillU(rr, b, TS); }, [&](uint8* b) { return (uint32_t)reinterpret_cast<CTrain*>(b)->CTrain::FindSideStationIsOn(); },
            [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x6F5EF0)(b, 0); }, 0, 0xFF);
    });
}

// CEntity::GetColModel (Entity.cpp is not linked; same logic as the port / 0x535300 - the fixtures keep m_vehicleSpecialColIndex = -1)
CColModel* CEntity::GetColModel() const {
    if (GetIsTypeVehicle()) {
        const auto veh = AsVehicle();
        if (veh->m_vehicleSpecialColIndex > -1) return &CVehicle::m_aSpecialColModel[veh->m_vehicleSpecialColIndex];
    }
    return GetModelInfo()->GetColModel();
}

// ---- CVehicle::FlyingControl: ApplyMoveForce / ApplyTurnForce are recorded on both sides (CPhysical's own code is covered by the physics test)
namespace fcx {
struct Call { int kind; CVector a, b; };
inline std::vector<Call> g_calls;
inline std::string Dump(const std::vector<Call>& c) { std::string s = std::to_string(c.size()) + " calls"; for (auto& x : c) s += (x.kind ? " T" : " M") + V(x.a) + (x.kind ? V(x.b) : std::string()); return s; }
static void __fastcall StubMove(void*, int, CVector f) { g_calls.push_back({ 0, f, {} }); }
static void __fastcall StubTurn(void*, int, CVector f, CVector p) { g_calls.push_back({ 1, f, p }); }
alignas(16) inline uint8 g_info[0x40];
alignas(16) inline uint8 g_col[0x30];
inline void Wire(uint8* b, bool on) {
    vfx::Wire(b, on);
    wfx::Put<uint32_t>(b, 0x388, on ? (uint32_t)(uintptr_t)(b + vfx::HND + 0xE0) : 0u);   // m_pFlyingHandlingData
}
}

static void TestFlyingControl() {
    using namespace wfx;
    if (!Wanted("FlyingControl")) return;
    static const int models[] = { 0x21B, 0x1CC, 0x208, 0x1F0, 0x1D7, 0x1E5, 0x1B7, 0x200, 0x1A6 };
    *reinterpret_cast<void**>(fcx::g_info + 0x14) = fcx::g_col;
    for (int m : models) CModelInfo::ms_modelInfoPtrs[m] = reinterpret_cast<CBaseModelInfo*>(fcx::g_info);
    PatchFn(PmAddr(static_cast<void (CPhysical::*)(CVector)>(&CPhysical::ApplyMoveForce)), (void*)&fcx::StubMove);
    PatchFn(PmAddr(&CPhysical::ApplyTurnForce), (void*)&fcx::StubTurn);
    oracle::Patch(0x5429F0, (void*)&fcx::StubMove);
    oracle::Patch(0x542A50, (void*)&fcx::StubTurn);
    Run("CVehicle::FlyingControl 0x6D85F0", [&](Rng& r, std::string& d) {
        const int model = r.below(10);
        auto in = [&]() { return r.below(3) == 0 ? -9999.99f : (r.below(6) ? r.f01() * 2.f - 1.f : GenF(r, 2.f)); };
        const float skid = in(), upDown = in(), leftRight = in(), accel = in();
        std::vector<fcx::Call> ca, ce;
        std::string da, de;
        const bool ok = Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) {
                vfx::Gen(rr, b);
                auto& v = *reinterpret_cast<CVehicle*>(b);
                auto& au = *reinterpret_cast<CAutomobile*>(b);
                const float sc = rr.below(2) ? 0.6f : 3.f;
                v.m_vecMoveSpeed = GenV(rr, sc); v.m_vecTurnSpeed = GenV(rr, 0.2f);
                v.m_fMass = rr.below(8) ? 300.f + rr.f01() * 5000.f : GenF(rr, 5000.f); v.m_fTurnMass = rr.below(8) ? 500.f + rr.f01() * 30000.f : GenF(rr, 30000.f);
                v.m_vecCentreOfMass = GenV(rr, 0.7f);
                v.m_fSteerAngle = GenF(rr, 0.6f);
                v.SetStatus((eEntityStatus)(1 + rr.below(7)));
                v.SetType(ENTITY_TYPE_VEHICLE); v.m_vehicleSpecialColIndex = -1;
                v.m_nCreatedBy = (eVehicleCreatedBy)(1 + rr.below(4));
                v.m_nModelIndex = (int16)models[rr.below(9)];
                v.m_nVehicleType = (eVehicleType)(rr.below(3) == 0 ? rr.below(10) : (rr.below(2) ? 0 : 3 + rr.below(2)));
                v.m_nVehicleSubType = (eVehicleType)(rr.below(3) ? 4 : (int)rr.below(10));
                v.physicalFlags.bSubmergedInWater = rr.below(2);
                v.vehicleFlags.bHeliMinimumTilt = rr.below(3) == 0;
                for (int i = 0; i < 4; ++i) au.m_fWheelsSuspensionCompression[i] = rr.below(2) ? 1.f : rr.f01();
                au.m_wMiscComponentAngle = (uint16)(rr.below(2) ? rr.below(300) : rr.u32());
                au.m_fHeliRotorSpeed = rr.f01() * 0.5f;
                Put<float>(b, 0x9CC, rr.below(2) ? 1.f : rr.f01());                       // CPlane::m_fLandingGearStatus
                Put<int32_t>(b, 0x9A0, (int32_t)rr.u32());                                 // CPlane::field_9A0
                auto* fh = reinterpret_cast<tFlyingHandlingData*>(b + vfx::HND + 0xE0);
                float* f = reinterpret_cast<float*>(fh);
                for (int i = 1; i < 22; ++i) f[i] = rr.below(10) ? (rr.below(2) ? rr.f01() * 2.f : rr.f01() * 0.1f) : GenF(rr, 2.f);
                for (int i = 0; i < 3; ++i) { (&fh->m_vecTurnRes.x)[i] = rr.below(10) ? 0.95f + rr.f01() * 0.05f : GenF(rr, 1.5f); (&fh->m_vecSpeedRes.x)[i] = rr.below(3) ? 0.f : rr.f01(); }
                CTimer::ms_fTimeStep = rr.below(12) ? 0.3f + rr.f01() * 2.5f : GenF(rr, 3.f);
                CWeather::WindDir = GenV(rr, 0.8f);
                CHeli::bHeliControlsCheat = rr.below(3) == 0;
                // fake col model: bounding box
                std::memset(fcx::g_col, 0, sizeof fcx::g_col);
                auto& cm = *reinterpret_cast<CColModel*>(fcx::g_col);
                cm.m_boundBox.m_vecMin = CVector{ -1.f - rr.f01(), -2.f - rr.f01() * 4.f, -1.f }; cm.m_boundBox.m_vecMax = CVector{ 1.f + rr.f01(), 2.f + rr.f01() * 4.f, 1.5f };
                if (rr.below(10) == 0) { cm.m_boundBox.m_vecMin = GenV(rr, 4.f); }
            },
            [&](uint8* b) { fcx::g_calls.clear(); reinterpret_cast<CVehicle*>(b)->CVehicle::FlyingControl((eFlightModel)model, skid, upDown, leftRight, accel); ca = fcx::g_calls; return 0u; },
            [&](uint8* b) { fcx::g_calls.clear(); oracle::Fn<void __fastcall(void*, int, int, float, float, float, float)>(0x6D85F0)(b, 0, model, skid, upDown, leftRight, accel); ce = fcx::g_calls; return 0u; },
            2, 0, 0, 0, fcx::Wire);
        bool same = ca.size() == ce.size();
        for (size_t i = 0; same && i < ca.size(); ++i) same = ca[i].kind == ce[i].kind && SameV(ca[i].a, ce[i].a) && SameV(ca[i].b, ce[i].b);
        if (ok && same) return true;
        d += " model " + std::to_string(model) + " in " + F(skid) + F(upDown) + F(leftRight) + F(accel) + " port " + fcx::Dump(ca) + " exe " + fcx::Dump(ce);
        return false;
    });
}

static void TestAircraftOrders() {
    using namespace wfx;
    auto au = [](uint8* b) -> CAutomobile& { return *reinterpret_cast<CAutomobile*>(b); };
    auto init = [](Rng& rr, uint8* b) { vfx::Gen(rr, b); if (rr.below(2)) Put<float>(b, 0x8A8, 0.f); else if (rr.below(3) == 0) Put<float>(b, 0x8A8, GenF(rr, 10.f)); };
    Run("CAutomobile::TellHeliToGoToCoors 0x6A2390", [&](Rng& r, std::string& d) {
        const float sc = r.below(2) ? 100.f : PickScale(r);
        const float x = GenF(r, sc), y = GenF(r, sc), z = GenF(r, sc), lo = GenF(r, sc), hi = GenF(r, sc);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { au(b).CAutomobile::TellHeliToGoToCoors(x, y, z, lo, hi); return 0u; },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, float, float, float, float, float)>(0x6A2390)(b, 0, x, y, z, lo, hi); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CAutomobile::TellPlaneToGoToCoors 0x6A2470", [&](Rng& r, std::string& d) {
        const float sc = r.below(2) ? 100.f : PickScale(r);
        const float x = GenF(r, sc), y = GenF(r, sc), z = GenF(r, sc), lo = GenF(r, sc), hi = GenF(r, sc);
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { au(b).CAutomobile::TellPlaneToGoToCoors(x, y, z, lo, hi); return 0u; },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, float, float, float, float, float)>(0x6A2470)(b, 0, x, y, z, lo, hi); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
}

static void TestBikeBmxBoat() {
    using namespace wfx;
    auto bike = [](uint8* b) -> CBike& { return *reinterpret_cast<CBike*>(b); };
    auto init = [](Rng& rr, uint8* b) { vfx::Gen(rr, b); };
    Run("CBike::GetAllWheelsOffGround 0x6B6790", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); Put<uint8>(b, 0x804, (uint8)rr.below(3)); Put<uint8>(b, 0x805, (uint8)rr.below(3)); },
            [&](uint8* b) { return (uint32_t)bike(b).CBike::GetAllWheelsOffGround(); }, [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x6B6790)(b, 0); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CBike::GetNumContactWheels 0x6B58A0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return (uint32_t)bike(b).CBike::GetNumContactWheels(); }, [&](uint8* b) { return oracle::Fn<uint32_t __fastcall(void*, int)>(0x6B58A0)(b, 0) & 0xFF; }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CBike::GetHeightAboveRoad 0x6B58B0", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { return FB(bike(b).CBike::GetHeightAboveRoad()); }, [&](uint8* b) { return FB(oracle::Fn<float __fastcall(void*, int)>(0x6B58B0)(b, 0)); }, 1, 0, 0, 0, vfx::Fix);
    });
    Run("CBike::IsComponentPresent 0x6B59E0", [&](Rng& r, std::string& d) {
        const int c = r.below(BIKE_NUM_NODES);
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); for (int i = 0; i < BIKE_NUM_NODES; ++i) if (rr.below(2)) Put<uint32_t>(b, 0x5A0 + 4 * i, 0); },
            [&](uint8* b) { return (uint32_t)bike(b).CBike::IsComponentPresent(c); }, [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, int)>(0x6B59E0)(b, 0, c); }, 0, 0xFF, 0, 0, vfx::Fix);
    });
    Run("CBike::Fix 0x6B7050", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, init, [&](uint8* b) { bike(b).CBike::Fix(); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6B7050)(b, 0); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CBike::ReduceHornCounter 0x6B5F90", [&](Rng& r, std::string& d) {
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); if (rr.below(2)) Put<uint32_t>(b, 0x514, rr.below(4) ? rr.below(4) : rr.u32()); },
            [&](uint8* b) { bike(b).CBike::ReduceHornCounter(); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x6B5F90)(b, 0); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CBike::RemoveRefsToVehicle 0x6B67B0", [&](Rng& r, std::string& d) {
        const uint32_t ent = 0x1000 + 4 * r.below(3);
        return Obj<vfx::ALL>(r, d, [&](Rng& rr, uint8* b) { vfx::Gen(rr, b); for (int i = 0; i < 4; ++i) Put<uint32_t>(b, 0x7C0 + 4 * i, 0x1000 + 4 * rr.below(3)); },
            [&](uint8* b) { bike(b).CBike::RemoveRefsToVehicle((CEntity*)(uintptr_t)ent); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, uint32_t)>(0x6B67B0)(b, 0, ent); return 0u; }, 2, 0, 0, 0, vfx::Fix);
    });
    Run("CBike::FindWheelWidth 0x6B8940", [&](Rng& r, std::string& d) {
        const bool rear = r.below(2);
        return Obj<0x40>(r, d, [](Rng&, uint8*) {}, [&](uint8* b) { return FB(bike(b).CBike::FindWheelWidth(rear)); }, [&](uint8* b) { return FB(oracle::Fn<float __fastcall(void*, int, bool)>(0x6B8940)(b, 0, rear)); }, 1);
    });
    Run("CBmx::FindWheelWidth 0x6C0550", [&](Rng& r, std::string& d) {
        const bool rear = r.below(2);
        return Obj<0x40>(r, d, [](Rng&, uint8*) {}, [&](uint8* b) { return FB(reinterpret_cast<CBmx*>(b)->CBmx::FindWheelWidth(rear)); }, [&](uint8* b) { return FB(oracle::Fn<float __fastcall(void*, int, bool)>(0x6C0550)(b, 0, rear)); }, 1);
    });
    Run("CBmx::BurstTyre 0x6BF9C0", [&](Rng& r, std::string& d) {
        const int c = r.below(20); const bool ph = r.below(2);
        return Obj<0x40>(r, d, [](Rng&, uint8*) {}, [&](uint8* b) { return (uint32_t)reinterpret_cast<CBmx*>(b)->CBmx::BurstTyre((uint8)c, ph); }, [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, int, bool)>(0x6BF9C0)(b, 0, c, ph); }, 0, 0xFF);
    });
    Run("CBmx::SetUpWheelColModel 0x6BF9B0", [&](Rng& r, std::string& d) {
        return Obj<0x40>(r, d, [](Rng&, uint8*) {}, [&](uint8* b) { return (uint32_t)reinterpret_cast<CBmx*>(b)->CBmx::SetUpWheelColModel(nullptr); }, [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, void*)>(0x6BF9B0)(b, 0, nullptr); }, 0, 0xFF);
    });
    Run("CBoat::IsComponentPresent 0x6F0200", [&](Rng& r, std::string& d) {
        const int c = r.below(30);
        return Obj<0x40>(r, d, [](Rng&, uint8*) {}, [&](uint8* b) { return (uint32_t)reinterpret_cast<CBoat*>(b)->CBoat::IsComponentPresent(c); }, [&](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int, int)>(0x6F0200)(b, 0, c); }, 0, 0xFF);
    });
}

// ProcessFlyingCarStuff of CAutomobile / CHeli with CVehicle::FlyingControl replaced by a recorder on both sides (restored afterwards; the heli's rotor node is null: the blade / wind part is not run)
namespace fcx {
struct FcCall { int model; float a, b, c, d; };
inline std::vector<FcCall> g_fc;
static void __fastcall StubFlying(void*, int, int model, float a, float b, float c, float d) { g_fc.push_back({ model, a, b, c, d }); }
struct Patched { void* addr = nullptr; uint8 saved[16] {}; void Set(void* a, void* host) { addr = a; std::memcpy(saved, a, 16); wfx::PatchFn(a, host); } void Restore() { if (addr) { DWORD o; VirtualProtect(addr, 16, PAGE_EXECUTE_READWRITE, &o); std::memcpy(addr, saved, 16); FlushInstructionCache(GetCurrentProcess(), addr, 16); addr = nullptr; } } };
}

static void TestProcessFlyingCarStuff() {
    using namespace wfx;
    auto fcInit = [](Rng& rr, uint8* b) {
        vfx::Gen(rr, b);
        static const int st[] = { 0, 3, 8, 1, 2, 4, 5, 6, 7 };
        Put<uint8>(b, 0x36, (uint8)((rr.below(10) < 7 ? st[rr.below(3)] : st[rr.below(9)]) << 3 | 2));
        auto& v = *reinterpret_cast<CVehicle*>(b);
        v.m_vecMoveSpeed = rr.below(4) ? GenV(rr, 0.1f) : CVector{ 0.f, 0.f, 0.f };
        if (rr.below(5) == 0) v.m_vecMoveSpeed = GenV(rr, 0.03f);
        CTimer::ms_fTimeStep = rr.below(6) ? 0.2f + rr.f01() * 2.f : GenF(rr, 2.f);
    };
    Run("CAutomobile::ProcessFlyingCarStuff 0x6A8500", [&](Rng& r, std::string& d) {
        const bool cheat = r.below(3) != 0;
        std::vector<fcx::FcCall> ca, ce;
        fcx::Patched pp, pe;
        pp.Set(PmAddr(&CVehicle::FlyingControl), (void*)&fcx::StubFlying); pe.Set((void*)0x6D85F0, (void*)&fcx::StubFlying);
        const bool ok = Obj<vfx::ALL>(r, d, fcInit,
            [&](uint8* b) { CCheat::m_aCheatsActive[CHEAT_CARS_FLY] = cheat; fcx::g_fc.clear(); reinterpret_cast<CAutomobile*>(b)->CAutomobile::ProcessFlyingCarStuff(); ca = fcx::g_fc; return 0u; },
            [&](uint8* b) { CCheat::m_aCheatsActive[CHEAT_CARS_FLY] = cheat; fcx::g_fc.clear(); oracle::Fn<void __fastcall(void*, int)>(0x6A8500)(b, 0); ce = fcx::g_fc; return 0u; }, 2, 0, 0, 0, vfx::Fix);
        pp.Restore(); pe.Restore();
        bool same = ca.size() == ce.size();
        for (size_t i = 0; same && i < ca.size(); ++i) same = ca[i].model == ce[i].model && SameF(ca[i].a, ce[i].a) && SameF(ca[i].b, ce[i].b) && SameF(ca[i].c, ce[i].c) && SameF(ca[i].d, ce[i].d);
        Hit(!ca.empty());
        if (ok && same) return true;
        d += " port calls " + std::to_string(ca.size()) + " exe calls " + std::to_string(ce.size()); return false;
    });
    Run("CHeli::ProcessFlyingCarStuff (no rotor node) 0x6C4E60", [&](Rng& r, std::string& d) {
        std::vector<fcx::FcCall> ca, ce;
        fcx::Patched pp, pe;
        pp.Set(PmAddr(&CVehicle::FlyingControl), (void*)&fcx::StubFlying); pe.Set((void*)0x6D85F0, (void*)&fcx::StubFlying);
        auto heliInit = [&](Rng& rr, uint8* b) {
            fcInit(rr, b);
            auto& h = *reinterpret_cast<CHeli*>(b);
            h.m_aCarNodes[HELI_STATIC_ROTOR] = nullptr;
            h.m_fHeliRotorSpeed = rr.below(3) ? rr.f01() * 0.3f : GenF(rr, 0.3f);
            h.m_fLeftRightSkid = GenF(rr, 1.f); h.m_fSteeringUpDown = GenF(rr, 1.f); h.m_fSteeringLeftRight = GenF(rr, 1.f); h.m_fAccelerationBreakStatus = rr.below(3) ? GenF(rr, 1.f) : 0.f;
            h.m_nNumContactWheels = (uint8)rr.below(6);
            h.physicalFlags.bSubmergedInWater = rr.below(3) == 0; h.physicalFlags.bTouchingWater = rr.below(3) == 0; h.vehicleFlags.bIsRCVehicle = rr.below(4) == 0;
            Put<int16_t>(b, 0x22, (int16_t)(rr.below(3) ? rr.below(2) ? 0x1d1 : 0x1f5 : 400 + (int)rr.below(200)));
            Put<uint32_t>(b, vfx::HND + 0xCC, rr.u32());
        };
        const bool ok = Obj<vfx::ALL>(r, d, heliInit,
            [&](uint8* b) { fcx::g_fc.clear(); reinterpret_cast<CHeli*>(b)->CHeli::ProcessFlyingCarStuff(); ca = fcx::g_fc; return 0u; },
            [&](uint8* b) { fcx::g_fc.clear(); oracle::Fn<void __fastcall(void*, int)>(0x6C4E60)(b, 0); ce = fcx::g_fc; return 0u; }, 2, 0, 0, 0, vfx::Fix);
        pp.Restore(); pe.Restore();
        bool same = ca.size() == ce.size();
        for (size_t i = 0; same && i < ca.size(); ++i) same = ca[i].model == ce[i].model && SameF(ca[i].a, ce[i].a) && SameF(ca[i].b, ce[i].b) && SameF(ca[i].c, ce[i].c) && SameF(ca[i].d, ce[i].d);
        Hit(!ca.empty());
        if (ok && same) return true;
        d += " port calls " + std::to_string(ca.size()) + " exe calls " + std::to_string(ce.size()); return false;
    });
    Run("CVehicle::ProcessWheelRotation 0x6D1230", [&](Rng& r, std::string& d) {
        const int ws = r.below(4);
        const CVector a = GenV(r, 3.f), c = GenV(r, 3.f); const float f = r.below(3) ? 0.1f + r.f01() * 2.f : GenF(r, 2.f);
        return Obj<0x40>(r, d, [](Rng&, uint8*) {}, [&](uint8* b) { return FB(reinterpret_cast<CVehicle*>(b)->CVehicle::ProcessWheelRotation((tWheelState)ws, a, c, f)); },
            [&](uint8* b) { return FB(oracle::Fn<float __fastcall(void*, int, int, const CVector*, const CVector*, float)>(0x6D1230)(b, 0, ws, &a, &c, f)); }, 1);
    });
}

static void TestVehicles() {
    TestDamageManager();
    TestTransmission();
    TestVehicleSmall();
    TestAutomobileSmall();
    TestCarEnterExit();
    TestPlaneHelpers();
    TestVehicleMisc();
    TestHandlingMgr();
    TestVehicleTypes();
    TestFlyingControl();
    TestAircraftOrders();
    TestBikeBmxBoat();
    TestProcessFlyingCarStuff();
}
