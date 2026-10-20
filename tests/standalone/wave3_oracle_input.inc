// ORACLE-W3-INPUT: exe-oracle rows for ControllerConfigManager.cpp and Pad.cpp (classes A + B), on real fixtures: the exe's ControlsManager (action / key table with keys of all four controller types,
// set orders 0..4, joystick state, press checkers) and CPad::Pads with NewState / OldState / PCTemp*State, the keyboard / mouse state globals, a fake player ped / vehicle for the driving checks.
// Included by wave2_oracle_all.inc when W3_INPUT_ONLY is defined (executable wave3_input_oracle_test = the W2 harness: deterministic arena, whole exe data image + the scratch block compared after every case).
#include "ControllerConfigManager.h"
#include "Pad.h"
#include "MenuManager.h"
#include "Camera.h"
#include "Timer.h"
#include "World.h"
#include "CutsceneMgr.h"
#include "PlayerInfo.h"
#include "Entity/Ped/Ped.h"
#include "Entity/Ped/PlayerPed.h"
#include "Entity/Vehicle/Vehicle.h"

namespace w3i {
using namespace w2;
constexpr size_t kScSize = 0x20000;
alignas(16) static uint8_t g_sc[kScSize];
constexpr size_t oOut = 0x0, oStA = 0x400, oStB = 0x500, oPed = 0x2000, oVeh = 0x4000, oPed2 = 0x6000, oPadObj = 0x8000, oVeh2 = 0xA000;
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& At(uintptr_t va) { return *reinterpret_cast<T*>(va); }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
static uint32_t P32(const void* p) { return (uint32_t)(uintptr_t)p; }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static inline uint64_t B8(bool b) { return b ? 1 : 0; }
static std::function<void(Rng&)> g_tweak;     // row-specific fixture work after the standard build
template<class R, class... A> static R ExeThis(uint32_t addr, void* self, A... a) { return oracle::Fn<R __fastcall(void*, int, A...)>(addr)(self, 0, a...); }

// ---- pad world: both pads, the keyboard / mouse state globals and the function-local statics around them (0xB72CB0 .. 0xB73700)
constexpr uintptr_t kPadLo = 0xB72CB0, kPadHi = 0xB73700;
static int16_t Axis(Rng& r) {
    switch (r.below(8)) {
    case 0: case 1: case 2: return 0;
    case 3: return 127;
    case 4: return -128;
    case 5: return 255;
    case 6: return (int16_t)r.u32();
    default: return (int16_t)((int)r.below(256) - 128);
    }
}
static void FillState(Rng& r, CControllerState& s) {
    auto* w = reinterpret_cast<int16_t*>(&s);
    for (size_t i = 0; i < sizeof(CControllerState) / 2; ++i) w[i] = (i < 4) ? Axis(r) : (int16_t)(r.below(2) ? 0 : (r.below(3) ? 255 : (int16_t)r.u32()));
}
static void PadWorld(Rng& r) {
    auto* lo = reinterpret_cast<uint8_t*>(kPadLo);
    for (size_t i = 0; i < kPadHi - kPadLo; ++i) lo[i] = r.below(3) == 0 ? (uint8_t)r.u32() : 0;
    for (auto& p : CPad::Pads) {
        FillState(r, p.NewState); FillState(r, p.OldState); FillState(r, p.PCTempKeyState); FillState(r, p.PCTempJoyState); FillState(r, p.PCTempMouseState);
        p.Mode = (int16_t)(r.below(8) ? r.below(4) : r.below(6));
        p.DrunkDrivingBufferUsed = (int32_t)r.below(10);
        p.DisablePlayerControls = r.below(5) == 0 ? (uint16_t)r.u32() : 0;
        for (auto& v : p.SteeringLeftRightBuffer) v = (int16_t)r.u32();
        p.Phase = (char)r.below(14);
        p.ShakeDur = (int16_t)(r.below(3) ? r.below(300) : (int16_t)r.u32()); p.ShakeFreq = (char)r.u32();
        p.LastTimeTouched = r.below(3) ? r.below(1000000) : r.u32(); p.NoShakeBeforeThis = GF(r, 1e5f); p.NoShakeFreq = (char)r.u32();
        p.AverageWeapon = (int32_t)r.below(1000); p.AverageEntries = (int32_t)r.below(5);
        p.iCurrHornHistory = (char)r.below(5); p.JustOutOfFrontEnd = (char)r.below(4);
    }
    CPad::padNumber = (char)r.below(2); CPad::bInvertLook4Pad = r.below(2) != 0;
    At<uint8_t>(0xB73403) = (uint8_t)r.below(2); At<uint8_t>(0x8CD782) = (uint8_t)r.below(2); At<uint8_t>(0xB6EC2E) = (uint8_t)r.below(2);
    CTimer::m_snTimeInMilliseconds = r.below(3) ? r.below(2000000) : r.u32();
}
// fake player ped (+ vehicle) in the scratch block; FindPlayerPed / FindPlayerVehicle of both sides read them through CWorld::Players
static void PlayerWorld(Rng& r, bool inVehicle, bool driving, bool train) {
    FillGarbage(r, S(oPed), 0x1000); FillGarbage(r, S(oVeh), 0x1000); FillGarbage(r, S(oPed2), 0x1000); FillGarbage(r, S(oVeh2), 0x1000);
    auto* ped = reinterpret_cast<CPlayerPed*>(S(oPed)); auto* veh = reinterpret_cast<CVehicle*>(S(oVeh));
    ped->bInVehicle = inVehicle; ped->m_pVehicle = inVehicle ? veh : nullptr; ped->m_nPedState = driving ? PEDSTATE_DRIVING : (ePedState)r.below(60);
    veh->m_pDriver = r.below(2) ? ped : reinterpret_cast<CPed*>(S(oPed2)); veh->m_nVehicleType = train ? VEHICLE_TYPE_TRAIN : (eVehicleType)r.below(9);
    ped->m_pAttachedTo = r.below(3) == 0 ? reinterpret_cast<CPhysical*>(S(oVeh2)) : nullptr;
    CWorld::PlayerInFocus = 0;
    CWorld::Players[0].m_pPed = ped; CWorld::Players[0].m_pRemoteVehicle = nullptr;
}
static void BuildAll(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    if (g_tweak) g_tweak(r);
}
template<class PF, class EF>
static bool UCase(Rng& r, std::string& d, PF&& pf, EF&& ef) {
    const uint32_t seed = r.u32() | 1;
    BuildAll(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    Reg img{ kImgLo, kImgHi - kImgLo };
    return BothA(d, seed, { sc, img }, std::forward<PF>(pf), std::forward<EF>(ef));
}
} // namespace w3i

struct Tu { explicit Tu(std::function<void(Rng&)> f) { w3i::g_tweak = std::move(f); } ~Tu() { w3i::g_tweak = nullptr; } };
template<class Fn> static void RunScaledI(const std::string& name, Fn&& fn, int cases) {
    static const int scale = std::getenv("W3I_SCALE") ? std::max(1, std::atoi(std::getenv("W3I_SCALE"))) : 1;
    static const int div = std::getenv("W3I_DIV") ? std::max(1, std::atoi(std::getenv("W3I_DIV"))) : 1;   // quick runs while developing
    Run(name, std::forward<Fn>(fn), std::max(10, cases * scale / div));
}
#define ROW(NAME, ADDR, CASES) { const int _cs = (CASES); const std::string _nm = std::string(NAME " " #ADDR); RunScaledI(_nm, [&](Rng& r, std::string& d) -> bool
#define ENDROW , _cs); }
#include "wave3_oracle_input_a.inc"      // Pad.cpp
#include "wave3_oracle_input_b.inc"      // ControllerConfigManager.cpp
static void TestW3InputAll() {
    (void)spdlog::default_logger_raw();   // the default logger is created lazily by the first NOTSA_LOG_* of the port: do it outside the redirected allocator
    TestW3InputA();
    TestW3InputB();
}
