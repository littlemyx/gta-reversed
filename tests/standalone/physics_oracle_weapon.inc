// CWeapon::FireM16_1stPerson (0x741C00) against the exe with the world / audio / bullet-impact callees replaced by host stubs on BOTH sides:
// the exe's callees are redirected with oracle::Patch, the port's (Weapon.cpp members) are redirected by overwriting their entry with a JMP; the symbols Weapon.cpp needs
// from TUs this test does not link (CWorld::ProcessLineOfSight, FindPlayerPed, CPad::StartShake, CWeaponInfo::GetWeaponInfo, CBirds::HandleGunShot, CPed::IsPlayer / GetPadNumber)
// are defined here. Compared: return value, the camera angles, the recorded pad shake and DoBulletImpact victim.
#include "Weapon.h"
#include "WeaponInfo.h"
#include "Pad.h"
#include "Birds.h"
#include "World.h"

namespace fm16 {
alignas(16) static uint8 g_weaponInfo[sizeof(CWeaponInfo)];
alignas(16) static uint8 g_victim[0x40];
static CEntity* g_hitEntity = nullptr;
static CVector  g_hitPoint{};
static float    g_rangeMult = 1.f;
struct Rec { int shakeCount = 0; int time = 0, freq = 0, delay = 0; int bulletCount = 0; void* victim = nullptr; int losCount = 0; };
static Rec g_rec;
}

// --- symbols of unlinked TUs
bool CWorld::ProcessLineOfSight(const CVector&, const CVector&, CColPoint& outColPoint, CEntity*& outEntity, bool, bool, bool, bool, bool, bool, bool, bool) {
    ++fm16::g_rec.losCount; outEntity = fm16::g_hitEntity; outColPoint.m_vecPoint = fm16::g_hitPoint; return false;
}
void CPad::StartShake(int16 time, uint8 freq, uint32 delay) { ++fm16::g_rec.shakeCount; fm16::g_rec.time = time; fm16::g_rec.freq = freq; fm16::g_rec.delay = (int)delay; }
CWeaponInfo* CWeaponInfo::GetWeaponInfo(eWeaponType, eWeaponSkill) { return reinterpret_cast<CWeaponInfo*>(fm16::g_weaponInfo); }
void CBirds::HandleGunShot(const CVector*, const CVector*) {}
bool CPed::IsPlayer() const { return m_nPedType == PED_TYPE_PLAYER1 || m_nPedType == PED_TYPE_PLAYER2; }
int32 CPed::GetPadNumber() const { return m_nPedType == PED_TYPE_PLAYER2 ? 1 : 0; }

// --- host stubs (port: cdecl / thiscall as the real functions; exe: the exe's conventions)
static float __cdecl StubRangeMult(CEntity*, CEntity*) { return fm16::g_rangeMult; }
static bool  __cdecl StubCheckVehOcc(CEntity**, CColPoint*, eWeaponType, const CVector&, const CVector&) { return false; }
static void __fastcall StubDoBulletImpact(CWeapon*, int, CEntity*, CEntity* victim, const CVector&, const CVector&, const CColPoint&, int32) { ++fm16::g_rec.bulletCount; fm16::g_rec.victim = victim; }
static void __fastcall ExeStartShake(CPad*, int, int time, int freq, int delay) { ++fm16::g_rec.shakeCount; fm16::g_rec.time = (int16)time; fm16::g_rec.freq = (uint8)freq; fm16::g_rec.delay = delay; }
static void* __cdecl ExeGetWeaponInfo(int, int) { return fm16::g_weaponInfo; }
static void* __cdecl ExeFindPlayerPed(int) { return fm16::g_player; }
static void __cdecl ExeBirds(const void*, const void*) {}
static void __cdecl ExeOil(const void*, const void*) {}
static void __cdecl PortOil(const CVector&, const CVector&) {}
static bool __cdecl ExeLOS(const CVector&, const CVector&, CColPoint& cp, CEntity*& e, bool, bool, bool, bool, bool, bool, bool, bool) { ++fm16::g_rec.losCount; e = fm16::g_hitEntity; cp.m_vecPoint = fm16::g_hitPoint; return false; }

static void PatchHostFn(void* fn, void* host) {
    DWORD old = 0; VirtualProtect(fn, 16, PAGE_EXECUTE_READWRITE, &old);
    auto* p = reinterpret_cast<uint8_t*>(fn); p[0] = 0xE9; *reinterpret_cast<int32_t*>(p + 1) = (int32_t)((uintptr_t)host - ((uintptr_t)fn + 5));
    FlushInstructionCache(GetCurrentProcess(), fn, 16);
}
template<class PM> static void* PmAddr(PM pm) { void* p; static_assert(sizeof(pm) == sizeof(p)); std::memcpy(&p, &pm, sizeof p); return p; }

static void TestWeapon() {
    if (!Wanted("FireM16")) return;
    fm16::g_playerActive = true;
    // redirect the callees
    PatchHostFn(PmAddr(&CWeapon::DoBulletImpact), (void*)&StubDoBulletImpact);
    PatchHostFn(PmAddr(&CWeapon::TargetWeaponRangeMultiplier), (void*)&StubRangeMult);
    PatchHostFn(PmAddr(&CWeapon::CheckForShootingVehicleOccupant), (void*)&StubCheckVehOcc);
    PatchHostFn((void*)&CShadows::GunShotSetsOilOnFire, (void*)&PortOil);
    oracle::Patch(0x707550, (void*)&ExeOil);
    oracle::Patch(0x73B550, (void*)&StubDoBulletImpact);   // thiscall, ret 0x18: same layout as the port stub
    oracle::Patch(0x73B380, (void*)&StubRangeMult);
    oracle::Patch(0x73F480, (void*)&StubCheckVehOcc);
    oracle::Patch(0x53F920, (void*)&ExeStartShake);
    oracle::Patch(0x743C60, (void*)&ExeGetWeaponInfo);
    oracle::Patch(0x56E210, (void*)&ExeFindPlayerPed);
    oracle::Patch(0x712E40, (void*)&ExeBirds);
    oracle::Patch(0x56BA00, (void*)&ExeLOS);

    Run("CWeapon::FireM16_1stPerson 0x741C00", [](Rng& r, std::string& d) {
        using namespace fm16;
        // camera: three random active-cam blocks, valid mode, random active index
        Snap camSnap(0xB6F028, 0x700 + 0x238 * 3); camSnap.FillRandom(r); camSnap.Restore();
        TheCamera.m_nActiveCam = (uint8)r.below(3);
        static const int16 modes[] = { 34, 7, 46, 8, 51, 42, 39, 40, 52, 45, 4 };
        auto& cam = TheCamera.GetActiveCam();
        cam.m_nMode = (eCamMode)modes[r.below(11)];
        cam.m_vecSource = GenV(r, 100.f); cam.m_vecFront = GenDir(r);
        cam.m_fHorizontalAngle = GenF(r, 6.f); cam.m_fVerticalAngle = GenF(r, 1.5f);
        // weapon info
        auto* wi = reinterpret_cast<CWeaponInfo*>(g_weaponInfo);
        std::memset(g_weaponInfo, 0, sizeof g_weaponInfo);
        wi->m_fWeaponRange = r.below(8) ? 5.f + r.f01() * 100.f : GenF(r, 200.f);
        wi->m_fAnimLoopStart = r.below(4) ? r.f01() * 0.5f : GenF(r, 1.f);
        wi->m_fAnimLoopEnd = wi->m_fAnimLoopStart + (r.below(4) ? r.f01() * 0.5f : GenF(r, 1.f));
        // players / owner
        std::memset(g_player, 0, sizeof g_player);
        *reinterpret_cast<uint32_t*>(g_player + 0x46c) = r.below(3) == 0 ? 0x4000000u : 0u;     // CPed ducking bit
        *reinterpret_cast<uint32_t*>(g_player + 0xfc)  = r.below(4) == 0 ? 1u : 0u;              // m_pAttachedTo
        alignas(16) uint8 owner[0x800] {};
        *reinterpret_cast<uint32_t*>(owner + 0x598) = r.below(6) ? (uint32_t)r.below(2) : 5u;     // ped type (0 / 1 = player)
        alignas(16) uint8 wbuf[sizeof(CWeapon)] {};
        auto* weapon = reinterpret_cast<CWeapon*>(wbuf);
        static const int types[] = { 30, 31, 31, 30, 29, 24, 22, 38 };
        *reinterpret_cast<int32_t*>(wbuf) = types[r.below(8)];
        g_hitEntity = r.below(3) ? reinterpret_cast<CEntity*>(g_victim) : nullptr;
        g_hitPoint = GenV(r, 100.f);
        g_rangeMult = r.below(4) ? 0.5f + r.f01() : GenF(r, 3.f);
        CTimer::ms_fTimeStep = r.below(8) ? r.f01() * 3.f : GenF(r, 4.f);
        const unsigned seed = r.u32();
        Snap cs(0xB6F028, 0x700 + 0x238 * 3), ca(0xB6F028, 0x700 + 0x238 * 3), ce(0xB6F028, 0x700 + 0x238 * 3);
        cs.Save();
        g_rec = Rec{}; SeedBoth(seed);
        const bool ra = weapon->FireM16_1stPerson(reinterpret_cast<CPed*>(owner)); ca.Save(); const Rec pa = g_rec;
        cs.Restore();
        g_rec = Rec{}; SeedBoth(seed);
        const bool re = oracle::Fn<bool __fastcall(CWeapon*, int, CPed*)>(0x741C00)(weapon, 0, reinterpret_cast<CPed*>(owner)); ce.Save(); const Rec pe = g_rec;
        if (ra == re && ca.SameAs(ce) && pa.shakeCount == pe.shakeCount && pa.time == pe.time && pa.freq == pe.freq && pa.delay == pe.delay && pa.bulletCount == pe.bulletCount && pa.victim == pe.victim) return true;
        d = "type " + std::to_string(*reinterpret_cast<int32_t*>(wbuf)) + " ret " + std::to_string(ra) + "/" + std::to_string(re) + " shake " + std::to_string(pa.shakeCount) + "/" + std::to_string(pe.shakeCount)
          + " t " + std::to_string(pa.time) + "/" + std::to_string(pe.time) + " f " + std::to_string(pa.freq) + "/" + std::to_string(pe.freq) + " bullets " + std::to_string(pa.bulletCount) + "/" + std::to_string(pe.bulletCount)
          + " victim " + std::to_string(pa.victim != nullptr) + "/" + std::to_string(pe.victim != nullptr) + " range " + F(wi->m_fWeaponRange) + " mult " + F(g_rangeMult) + " ts " + F(CTimer::ms_fTimeStep) + " loop " + F(wi->m_fAnimLoopStart) + "," + F(wi->m_fAnimLoopEnd)
          + DiffDesc(ca.b.data(), ce.b.data(), ca.b.size(), "cam");
        return false;
    });
}
