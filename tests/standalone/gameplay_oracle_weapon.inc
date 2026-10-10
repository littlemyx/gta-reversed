// (c) weapons: CWeapon / CWeaponInfo / CExplosion / CShotInfo / CBulletInfo / CProjectileInfo against the exe (see gameplay_oracle_test.cpp).
// Shared helpers (namespace wfx) are also used by gameplay_oracle_vehicle.inc.
#include <functional>
#include "Weapon.h"
#include "WeaponInfo.h"
#include "Explosion.h"
#include "ShotInfo.h"
#include "BulletInfo.h"
#include "ProjectileInfo.h"

namespace wfx {
// the port and the exe work on two copies of one random buffer: compares the returned value (kind 0 = integer (masked), 1 = float bits, 2 = ignored) and the whole buffer
template<size_t N, class Init, class FP, class FE>
static bool Obj(Rng& r, std::string& d, Init&& init, FP&& fp, FE&& fe, int kind = 0, uint32_t mask = 0xFFFFFFFFu, uintptr_t gva = 0, size_t gn = 0, const std::function<void(uint8*, bool)>& fix = {}) {
    alignas(16) uint8 A[N + 16], E[N + 16], O[N + 16];
    std::memset(A, 0, sizeof A);
    init(r, A);
    std::memcpy(E, A, sizeof A); std::memcpy(O, A, sizeof A);   // fix(b, true) wires the buffer-internal pointers before a call, fix(b, false) unwires them for the comparison
    std::vector<uint8> g0(gn), ga(gn), ge(gn);
    if (gn) std::memcpy(g0.data(), (void*)gva, gn);
    if (fix) fix(A, true);
    const uint32_t ra = fp(A);
    if (fix) fix(A, false);
    if (gn) { std::memcpy(ga.data(), (void*)gva, gn); std::memcpy((void*)gva, g0.data(), gn); }
    if (fix) fix(E, true);
    const uint32_t re = fe(E);
    if (fix) fix(E, false);
    if (gn) { std::memcpy(ge.data(), (void*)gva, gn); std::memcpy((void*)gva, g0.data(), gn); }
    bool ok = true;
    if (kind == 0) { if ((ra & mask) != (re & mask)) { g_hardDiff = true; ok = false; } }
    else if (kind == 1) ok &= SameBits(ra, re);
    ok &= SameBlob(A, E, N + 16);
    if (gn) ok &= SameBlob(ga.data(), ge.data(), gn);
    if (ok) return true;
    char b[96]; std::snprintf(b, sizeof b, "ret %08X/%08X ", ra, re);
    d = b + DiffDesc(A, E, N + 16, "buf") + (gn ? DiffDesc(ga.data(), ge.data(), gn, "glob") : "") + " in " + Hex(O, N < 160 ? N : 160);
    return false;
}
// the same for exe globals: `n` bytes at `va` are randomised (fill), saved after each side and compared
static bool Glob(Rng& r, std::string& d, uintptr_t va, size_t n, const std::function<void(Rng&, uint8*)>& fill, const std::function<uint32_t()>& fp, const std::function<uint32_t()>& fe, int kind = 0, uint32_t mask = 0xFFFFFFFFu) {
    Snap init(va, n), a(va, n), e(va, n);
    init.Save();
    { std::vector<uint8> tmp(n); std::memcpy(tmp.data(), (void*)va, n); fill(r, tmp.data()); std::memcpy((void*)va, tmp.data(), n); }
    Snap s0(va, n); s0.Save();
    const uint32_t ra = fp(); a.Save();
    s0.Restore();
    const uint32_t re = fe(); e.Save();
    init.Restore();
    bool ok = true;
    if (kind == 0) { if ((ra & mask) != (re & mask)) { g_hardDiff = true; ok = false; } }
    else if (kind == 1) ok &= SameBits(ra, re);
    ok &= a.SameAs(e);
    if (ok) return true;
    char b[64]; std::snprintf(b, sizeof b, "ret %08X/%08X ", ra, re);
    d = b + DiffDesc(a.b.data(), e.b.data(), n, "glob") + " in " + Hex(s0.b.data(), n < 96 ? n : 96);
    return false;
}
static void FillF(Rng& r, uint8* p, size_t n, float sc) { for (size_t i = 0; i + 4 <= n; i += 4) { const float v = GenF(r, sc); std::memcpy(p + i, &v, 4); } }
static void FillU(Rng& r, uint8* p, size_t n) { for (size_t i = 0; i + 4 <= n; i += 4) { const uint32_t v = r.u32() * 2654435761u; std::memcpy(p + i, &v, 4); } }
template<class T> static void Put(uint8* b, size_t off, T v) { std::memcpy(b + off, &v, sizeof v); }
template<class T> static T Get(const uint8* b, size_t off) { T v; std::memcpy(&v, b + off, sizeof v); return v; }
static uint32_t FB(float f) { return FBits(f); }
// overwrites a port function's entry with a JMP to a host function (same calling convention)
static void PatchFn(void* fn, void* host) {
    DWORD old = 0; VirtualProtect(fn, 16, PAGE_EXECUTE_READWRITE, &old);
    auto* p = reinterpret_cast<uint8_t*>(fn); p[0] = 0xE9; *reinterpret_cast<int32_t*>(p + 1) = (int32_t)((uintptr_t)host - ((uintptr_t)fn + 5));
    FlushInstructionCache(GetCurrentProcess(), fn, 16);
}
template<class PM> static void* PmAddr(PM pm) { void* p; static_assert(sizeof(pm) == sizeof(p)); std::memcpy(&p, &pm, sizeof p); return p; }
}

static void TestWeaponMisc() {
    using namespace wfx;
    // ---- CWeapon pure helpers (the weapon type is read from the buffer; CWeaponInfo table randomised where the exe reads it)
    Run("CWeapon::IsTypeMelee 0x73B1C0", [](Rng& r, std::string& d) {
        const int type = r.below(47);
        return Glob(r, d, 0xC8AAB8, 0x70 * 80, [&](Rng& rr, uint8* g) { FillU(rr, g, 0x70 * 80); for (int i = 0; i < 80; ++i) Put<uint32_t>(g, i * 0x70, rr.below(3) ? rr.below(6) : 0); },
            [&] { alignas(16) uint8 w[0x1C] {}; Put<int32_t>(w, 0, type); return (uint32_t)reinterpret_cast<CWeapon*>(w)->IsTypeMelee(); },
            [&] { alignas(16) uint8 w[0x1C] {}; Put<int32_t>(w, 0, type); return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x73B1C0)(w, 0); }, 0, 0xFF);
    });
    Run("CWeapon::IsType2Handed 0x73B1E0", [](Rng& r, std::string& d) {
        return Obj<0x1C>(r, d, [](Rng& rr, uint8* b) { Put<int32_t>(b, 0, rr.below(60)); },
            [](uint8* b) { return (uint32_t)reinterpret_cast<CWeapon*>(b)->IsType2Handed(); },
            [](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x73B1E0)(b, 0); }, 0, 0xFF);
    });
    Run("CWeapon::IsTypeProjectile 0x73B210", [](Rng& r, std::string& d) {
        return Obj<0x1C>(r, d, [](Rng& rr, uint8* b) { Put<int32_t>(b, 0, rr.below(60)); },
            [](uint8* b) { return (uint32_t)reinterpret_cast<CWeapon*>(b)->IsTypeProjectile(); },
            [](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x73B210)(b, 0); }, 0, 0xFF);
    });
    Run("CWeapon::CanBeUsedFor2Player(type) 0x73B240", [](Rng& r, std::string& d) {
        const int t = r.below(60) - 3;
        if (t < 0) return true;
        const bool a = CWeapon::CanBeUsedFor2Player((eWeaponType)t), e = oracle::Fn<bool __cdecl(int)>(0x73B240)(t);
        if (a == e) return true;
        d = "type " + std::to_string(t) + " got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    });
    Run("CWeapon::CanBeUsedFor2Player() 0x73DEF0", [](Rng& r, std::string& d) {
        return Obj<0x1C>(r, d, [](Rng& rr, uint8* b) { Put<int32_t>(b, 0, rr.below(60)); },
            [](uint8* b) { return (uint32_t)reinterpret_cast<CWeapon*>(b)->CanBeUsedFor2Player(); },
            [](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x73DEF0)(b, 0); }, 0, 0xFF);
    });
    Run("CWeapon::HasWeaponAmmoToBeUsed 0x73B2A0", [](Rng& r, std::string& d) {
        return Obj<0x1C>(r, d, [](Rng& rr, uint8* b) { FillU(rr, b, 0x1C); Put<int32_t>(b, 0, rr.below(52)); if (rr.below(3) == 0) Put<uint32_t>(b, 0xC, 0); },
            [](uint8* b) { return (uint32_t)reinterpret_cast<CWeapon*>(b)->HasWeaponAmmoToBeUsed(); },
            [](uint8* b) { return (uint32_t)oracle::Fn<bool __fastcall(void*, int)>(0x73B2A0)(b, 0); }, 0, 0xFF);
    });
    Run("CWeapon::CWeapon(type, ammo) 0x73B430", [](Rng& r, std::string& d) {
        const int type = r.below(47);
        const int ammo = r.below(4) == 0 ? (int)(r.u32() % 200000) : r.below(5) == 0 ? 0 : r.below(200);
        return Glob(r, d, 0xC8AAB8, 0x70 * 80, [&](Rng& rr, uint8* g) { FillU(rr, g, 0x70 * 80); for (int i = 0; i < 80; ++i) Put<int16_t>(g, i * 0x70 + 0x20, (int16_t)(rr.below(4) ? rr.below(100) : rr.u32())); },
            [&] { alignas(16) uint8 w[0x40]; std::memset(w, 0xA5, sizeof w); new (w) CWeapon((eWeaponType)type, (uint32)ammo); uint32_t h = 0; for (int i = 0; i < 0x20; ++i) h = h * 31 + w[i]; return h; },
            [&] { alignas(16) uint8 w[0x40]; std::memset(w, 0xA5, sizeof w); oracle::Fn<void* __fastcall(void*, int, int, int)>(0x73B430)(w, 0, type, ammo); uint32_t h = 0; for (int i = 0; i < 0x20; ++i) h = h * 31 + w[i]; return h; });
    });
    // ---- CWeaponInfo
    Run("CWeaponInfo::GetWeaponInfo 0x743C60", [](Rng& r, std::string& d) {
        const int t = r.below(47); const int sk = r.below(4);
        if (sk != 1 && !(t >= 22 && t <= 32)) return true;
        const void* a = CWeaponInfo::GetWeaponInfo((eWeaponType)t, (eWeaponSkill)sk);
        const void* e = oracle::Fn<void* __cdecl(int, int)>(0x743C60)(t, sk);
        if (a == e) return true;
        d = "type " + std::to_string(t) + " skill " + std::to_string(sk) + " got " + std::to_string((uintptr_t)a) + " exe " + std::to_string((uintptr_t)e); return false;
    });
    Run("CWeaponInfo::GetSkillStatIndex 0x743CD0", [](Rng& r, std::string& d) {
        const int t = 22 + r.below(11);   // exe: 22..32 (cmp 0x20; jg -> -1)
        const int a = (int)CWeaponInfo::GetSkillStatIndex((eWeaponType)t), e = oracle::Fn<int __cdecl(int)>(0x743CD0)(t);
        if (a == e) return true;
        d = "type " + std::to_string(t) + " got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    });
    // m_nSkillLevel is a byte in the port (always 0..3 in the data): the fixture keeps it below 128
    Run("CWeaponInfo::GetTargetHeadRange 0x743D50", [](Rng& r, std::string& d) {
        return Obj<0x70>(r, d, [](Rng& rr, uint8* b) { FillU(rr, b, 0x70); Put<float>(b, 8, GenF(rr, rr.below(2) ? 100.f : PickScale(rr))); Put<int32_t>(b, 0x30, rr.below(3) ? rr.below(5) : rr.below(128)); },
            [](uint8* b) { return FB(reinterpret_cast<CWeaponInfo*>(b)->GetTargetHeadRange()); },
            [](uint8* b) { return FB(oracle::Fn<float __fastcall(void*, int)>(0x743D50)(b, 0)); }, 1);
    });
    Run("CWeaponInfo::GetCrouchReloadAnimationID 0x685700", [](Rng& r, std::string& d) {
        return Obj<0x70>(r, d, [](Rng& rr, uint8* b) { FillU(rr, b, 0x70); },
            [](uint8* b) { return (uint32_t)reinterpret_cast<CWeaponInfo*>(b)->GetCrouchReloadAnimationID(); },
            [](uint8* b) { return (uint32_t)oracle::Fn<int __fastcall(void*, int)>(0x685700)(b, 0); });
    });
    Run("CWeaponInfo::GetWeaponReloadTime 0x743D70", [](Rng& r, std::string& d) {
        constexpr int AimN = 18;
        alignas(16) uint8 w[0x80];
        return Glob(r, d, 0xC8A8A8, 24 * AimN, [&](Rng& rr, uint8* g) {
                FillU(rr, g, 24 * AimN);
                for (int i = 0; i < AimN; ++i) for (int k = 0; k < 4; ++k) Put<uint16_t>(g, i * 24 + 16 + k * 2, (uint16_t)(rr.below(3) ? rr.below(600) : rr.below(65536)));
                FillU(rr, w, sizeof w); Put<int16_t>(w, 0x6C, (int16_t)(rr.below(AimN))); if (rr.below(3) == 0) Put<uint32_t>(w, 0x18, 0);
            },
            [&] { return (uint32_t)reinterpret_cast<CWeaponInfo*>(w)->GetWeaponReloadTime(); },
            [&] { return (uint32_t)oracle::Fn<uint32_t __fastcall(void*, int)>(0x743D70)(w, 0); });
    });
    Run("CWeaponInfo::FindWeaponFireType 0x5BCF30", [](Rng& r, std::string& d) {
        static const char* names[] = { "MELEE", "INSTANT_HIT", "PROJECTILE", "AREA_EFFECT", "CAMERA", "USE", "melee", "Use", "", "FOO", "AREA_EFFECT2", "INSTANT_HI", "PROJECTIL", "USE " };
        const char* s = names[r.below(14)];
        const int a = (int)CWeaponInfo::FindWeaponFireType(s), e = oracle::Fn<int __cdecl(const char*)>(0x5BCF30)(s);
        if (a == e) return true;
        d = std::string(s) + " got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    });
    // ---- CExplosion statics (16 x 0x7C bytes at 0xC88950)
    constexpr uintptr_t EX = 0xC88950; constexpr size_t EXN = 16 * 0x7C;
    // NOTE: the exe's ClearAllExplosions/Initialise clear the fuel arrays (+0x40..+0x7C) through a mis-strided loop (zero vectors at B_k+0x40+0xC*k ... and a few stray words outside the array,
    // 18 inner iterations for 16 explosions); the port clears them per explosion. The two agree whenever the fuel arrays are already zero (static zero-init, the only time the game calls it), which the fixture enforces.
    auto fillClear = [](Rng& rr, uint8* g) { FillU(rr, g, EXN); for (int i = 0; i < 16; ++i) std::memset(g + i * 0x7C + 0x40, 0, 0x7C - 0x40); };
    Run("CExplosion::ClearAllExplosions 0x736840", [&](Rng& r, std::string& d) {
        return Glob(r, d, EX, EXN, fillClear, [] { CExplosion::ClearAllExplosions(); return 0u; }, [] { oracle::Fn<void __cdecl()>(0x736840)(); return 0u; }, 2);
    });
    Run("CExplosion::Initialise 0x736A40", [&](Rng& r, std::string& d) {
        return Glob(r, d, EX, EXN, fillClear, [] { CExplosion::Initialise(); return 0u; }, [] { oracle::Fn<void __cdecl()>(0x736A40)(); return 0u; }, 2);
    });
    Run("CExplosion::GetExplosionActiveCounter 0x736900", [](Rng& r, std::string& d) {
        const uint8 id = (uint8)r.below(16);
        return Glob(r, d, EX, EXN, [](Rng& rr, uint8* g) { FillU(rr, g, EXN); }, [&] { return (uint32_t)CExplosion::GetExplosionActiveCounter(id); }, [&] { return (uint32_t)oracle::Fn<uint8 __cdecl(uint8)>(0x736900)(id); }, 0, 0xFF);
    });
    Run("CExplosion::ResetExplosionActiveCounter 0x736910", [](Rng& r, std::string& d) {
        const uint8 id = (uint8)r.below(16);
        return Glob(r, d, EX, EXN, [](Rng& rr, uint8* g) { FillU(rr, g, EXN); }, [&] { CExplosion::ResetExplosionActiveCounter(id); return 0u; }, [&] { oracle::Fn<void __cdecl(uint8)>(0x736910)(id); return 0u; }, 2);
    });
    Run("CExplosion::DoesExplosionMakeSound 0x736920", [](Rng& r, std::string& d) {
        const uint8 id = (uint8)r.below(16);
        return Glob(r, d, EX, EXN, [](Rng& rr, uint8* g) { FillU(rr, g, EXN); }, [&] { return (uint32_t)CExplosion::DoesExplosionMakeSound(id); }, [&] { return (uint32_t)oracle::Fn<bool __cdecl(uint8)>(0x736920)(id); }, 0, 0xFF);
    });
    Run("CExplosion::GetExplosionType 0x736930", [](Rng& r, std::string& d) {
        const uint8 id = (uint8)r.below(16);
        return Glob(r, d, EX, EXN, [](Rng& rr, uint8* g) { FillU(rr, g, EXN); }, [&] { return (uint32_t)CExplosion::GetExplosionType(id); }, [&] { return (uint32_t)oracle::Fn<int __cdecl(uint8)>(0x736930)(id); });
    });
    Run("CExplosion::GetExplosionPosition 0x736940", [](Rng& r, std::string& d) {
        const uint8 id = (uint8)r.below(16);
        return Glob(r, d, EX, EXN, [](Rng& rr, uint8* g) { FillU(rr, g, EXN); }, [&] { return (uint32_t)(uintptr_t)&CExplosion::GetExplosionPosition(id); }, [&] { return (uint32_t)(uintptr_t)oracle::Fn<const CVector* __cdecl(uint8)>(0x736940)(id); });
    });
    Run("CExplosion::TestForExplosionInArea 0x736950", [](Rng& r, std::string& d) {
        const int type = r.below(4) == 0 ? -1 : r.below(14);
        const float sc = r.below(2) ? 50.f : PickScale(r);
        float v[6]; for (auto& x : v) x = GenF(r, sc);
        if (r.below(3)) { if (v[0] > v[1]) std::swap(v[0], v[1]); if (v[2] > v[3]) std::swap(v[2], v[3]); if (v[4] > v[5]) std::swap(v[4], v[5]); }
        return Glob(r, d, EX, EXN, [&](Rng& rr, uint8* g) { FillU(rr, g, EXN);
                for (int i = 0; i < 16; ++i) { uint8* e = g + i * 0x7C; Put<int32_t>(e, 0, rr.below(14)); const float p[3] = { GenF(rr, sc), GenF(rr, sc), GenF(rr, sc) }; std::memcpy(e + 4, p, 12); Put<uint8>(e, 0x29, rr.below(3) ? rr.below(3) : 0); } },
            [&] { Hit(false); return (uint32_t)CExplosion::TestForExplosionInArea((eExplosionType)type, v[0], v[1], v[2], v[3], v[4], v[5]); },
            [&] { return (uint32_t)oracle::Fn<bool __cdecl(int, float, float, float, float, float, float)>(0x736950)(type, v[0], v[1], v[2], v[3], v[4], v[5]); }, 0, 0xFF);
    });
    Run("CExplosion::RemoveAllExplosionsInArea 0x7369E0", [](Rng& r, std::string& d) {
        const float sc = r.below(2) ? 50.f : PickScale(r);
        const CVector p = GenV(r, sc); const float rad = std::fabs(GenF(r, sc));
        return Glob(r, d, EX, EXN, [&](Rng& rr, uint8* g) { FillU(rr, g, EXN);
                for (int i = 0; i < 16; ++i) { uint8* e = g + i * 0x7C; const float q[3] = { GenF(rr, sc), GenF(rr, sc), GenF(rr, sc) }; std::memcpy(e + 4, q, 12); Put<uint8>(e, 0x29, rr.below(3) ? 1 : 0); } },
            [&] { CExplosion::RemoveAllExplosionsInArea(p, rad); return 0u; },
            [&] { oracle::Fn<void __cdecl(float, float, float, float)>(0x7369E0)(p.x, p.y, p.z, rad); return 0u; }, 2);
    });
    // ---- shot / bullet / projectile infos
    Run("CShotInfo::Initialise 0x739B60", [](Rng& r, std::string& d) {
        constexpr size_t n = 0xC8A7E9 - 0xC89628;
        return Glob(r, d, 0xC89628, n, [](Rng& rr, uint8* g) { FillU(rr, g, n); }, [] { CShotInfo::Initialise(); return 0u; }, [] { oracle::Fn<void __cdecl()>(0x739B60)(); return 0u; }, 2);
    });
    // (origins are finite: a float copy through the FPU would quiet a signalling NaN)
    Run("CShotInfo::GetFlameThrowerShotPosn 0x739DE0", [](Rng& r, std::string& d) {
        const uint8 id = (uint8)r.below(100);
        alignas(16) uint8 o[24];
        return Glob(r, d, 0xC89690, 100 * 0x2c, [&](Rng& rr, uint8* g) { FillU(rr, g, 100 * 0x2c); for (int i = 0; i < 100; ++i) { Put<int32_t>(g, i * 0x2c, rr.below(2) ? 0x25 : rr.below(60)); Put<uint8>(g, i * 0x2c + 0x28, rr.below(2)); for (int k = 0; k < 3; ++k) Put<float>(g, i * 0x2c + 4 + 4 * k, rr.f01() * 100.f - 50.f); } FillU(rr, o, sizeof o); },
            [&] { alignas(16) uint8 p[24]; std::memcpy(p, o, 24); const bool b = CShotInfo::GetFlameThrowerShotPosn(id, *reinterpret_cast<CVector*>(p + 4)); uint32_t h = b; for (int i = 0; i < 24; ++i) h = h * 31 + p[i]; return h; },
            [&] { alignas(16) uint8 p[24]; std::memcpy(p, o, 24); const bool b = oracle::Fn<bool __cdecl(uint8, CVector*)>(0x739DE0)(id, reinterpret_cast<CVector*>(p + 4)); uint32_t h = b; for (int i = 0; i < 24; ++i) h = h * 31 + p[i]; return h; });
    });
    Run("CBulletInfo::Initialise 0x735FD0", [](Rng& r, std::string& d) {
        return Glob(r, d, 0xC88740, 8 * 0x2C, [](Rng& rr, uint8* g) { FillU(rr, g, 8 * 0x2C); }, [] { CBulletInfo::Initialise(); return 0u; }, [] { oracle::Fn<void __cdecl()>(0x735FD0)(); return 0u; }, 2);
    });
    Run("CProjectileInfo::Initialise 0x737B40", [](Rng& r, std::string& d) {
        constexpr size_t n = 0xC8962C - 0xC89110;
        return Glob(r, d, 0xC89110, n, [](Rng& rr, uint8* g) { FillU(rr, g, n); }, [] { CProjectileInfo::Initialise(); return 0u; }, [] { oracle::Fn<void __cdecl()>(0x737B40)(); return 0u; }, 2);
    });
}

// CWeapon::EvaluateTargetForHeatSeekingMissile: the potential target is a raw entity buffer (type / sub type / position / matrix pointer read), CCollision::DistToLine runs on both sides
static void TestHeatSeeking() {
    using namespace wfx;
    Run("CWeapon::EvaluateTargetForHeatSeekingMissile 0x73E560", [](Rng& r, std::string& d) {
        const float sc = r.below(2) ? 100.f : PickScale(r);
        const CVector origin = GenV(r, sc), aim = r.below(2) ? GenDir(r) : GenV(r, 2.f);
        const float tol = r.below(4) ? 0.5f + r.f01() * 3.f : GenF(r, 5.f);
        const bool planes = r.below(2);
        const int prefMode = r.below(3);
        alignas(16) static uint8 other[0x40];
        return Obj<0x700>(r, d, [&](Rng& rr, uint8* b) {
                FillU(rr, b, 0x600);
                Put<uint8>(b, 0x36, (uint8)((rr.u32() & ~7u) | (rr.below(3) ? 2 : rr.below(8))));            // entity type bits (2 = vehicle)
                Put<uint32_t>(b, 0x594, rr.below(3) ? 3 + rr.below(2) : rr.below(10));                       // vehicle sub type
                const CVector p = GenV(rr, sc);
                std::memcpy(b + 4, &p, 12);                                                                   // placement
                float m[16]; FillF(rr, (uint8*)m, 64, 1.f); const CVector mp = rr.below(4) ? p + GenV(rr, 1.f) : GenV(rr, sc); std::memcpy(m + 12, &mp, 12);
                std::memcpy(b + 0x640, m, 64);                                                                 // (matrix lives inside the buffer, wired in fix)
                Put<uint8>(b, 0x6F8, (uint8)rr.below(2));
            },
            [&](uint8* b) { return FB(CWeapon::EvaluateTargetForHeatSeekingMissile(reinterpret_cast<CEntity*>(b), origin, aim, tol, planes, prefMode == 0 ? nullptr : prefMode == 1 ? reinterpret_cast<CEntity*>(b) : reinterpret_cast<CEntity*>(other))); },
            [&](uint8* b) { return FB(oracle::Fn<float __cdecl(void*, const CVector*, const CVector*, float, bool, void*)>(0x73E560)(b, &origin, &aim, tol, planes, prefMode == 0 ? nullptr : prefMode == 1 ? (void*)b : (void*)other)); },
            1, 0, 0, 0, [](uint8* b, bool on) { Put<uint32_t>(b, 0x14, on && Get<uint8>(b, 0x6F8) ? (uint32_t)(uintptr_t)(b + 0x640) : 0u); });
    });
}

// CShotInfo::Update with creator-less shots (no ped loop): the world effects it triggers are recorded on both sides. The port-side symbols of the (unlinked) World / FireManager TUs are defined here.
#include "World.h"
#include "FireManager.h"
namespace sfx {
struct Rec { int kind; CVector pos; float a; const void* ent; };
inline std::vector<Rec> g_rec;
inline int      g_spray = 0;
inline CVector  g_sprayDir{};
inline bool     g_extinguish = false;
}
void CWorld::SetCarsOnFire(CVector pos, float radius, CEntity* c) { sfx::g_rec.push_back({ 0, pos, radius, c }); }
void CWorld::SetWorldOnFire(CVector pos, float radius, CEntity* c) { sfx::g_rec.push_back({ 1, pos, radius, c }); }
eSprayPaintState CWorld::SprayPaintWorld(CVector& posn, CVector& outDir, float radius, bool f) { sfx::g_rec.push_back({ 2 + (int)f * 10, posn, radius, nullptr }); outDir = sfx::g_sprayDir; return (eSprayPaintState)sfx::g_spray; }
bool CFireManager::ExtinguishPointWithWater(CVector point, float r, float sz) { sfx::g_rec.push_back({ 3, point, r + sz * 1000.f, nullptr }); return sfx::g_extinguish; }
namespace sfx {
static void __cdecl ExeCarsOnFire(CVector pos, float radius, CEntity* c) { g_rec.push_back({ 0, pos, radius, c }); }
static void __cdecl ExeWorldOnFire(CVector pos, float radius, CEntity* c) { g_rec.push_back({ 1, pos, radius, c }); }
static int  __cdecl ExeSpray(CVector* posn, CVector* outDir, float radius, bool f) { g_rec.push_back({ 2 + (int)f * 10, *posn, radius, nullptr }); *outDir = g_sprayDir; return g_spray; }
static bool __fastcall ExeExtinguish(void*, int, CVector point, float r, float sz) { g_rec.push_back({ 3, point, r + sz * 1000.f, nullptr }); return g_extinguish; }
inline std::string Dump(const std::vector<Rec>& v) { std::string s = std::to_string(v.size()) + " effects"; for (size_t i = 0; i < v.size() && i < 4; ++i) s += " [" + std::to_string(v[i].kind) + V(v[i].pos) + F(v[i].a) + "]"; return s; }
}

static void TestShotInfoUpdate() {
    using namespace wfx;
    if (!Wanted("CShotInfo::Update")) return;
    oracle::Patch(0x5659F0, (void*)&sfx::ExeCarsOnFire);
    oracle::Patch(0x56B910, (void*)&sfx::ExeWorldOnFire);
    oracle::Patch(0x565B70, (void*)&sfx::ExeSpray);
    oracle::Patch(0x5394C0, (void*)&sfx::ExeExtinguish);
    Run("CShotInfo::Update (no creators) 0x739E60", [&](Rng& r, std::string& d) {
        constexpr size_t N = 100 * 0x2c;
        std::vector<sfx::Rec> ra, re;
        sfx::g_spray = r.below(3) == 0 ? 1 : 0; sfx::g_sprayDir = GenDir(r); sfx::g_extinguish = r.below(2);
        Snap wiSave(0xC8AAB8, 0x70 * 80); wiSave.Save();
        const bool ok = Glob(r, d, 0xC89690, N, [&](Rng& rr, uint8* g) {
                FillU(rr, g, N);
                for (int i = 0; i < 100; ++i) {
                    uint8* e = g + i * 0x2c;
                    static const int types[] = { 37, 37, 41, 42, 16, 22, 0, 38 };
                    Put<int32_t>(e, 0, rr.below(4) ? types[rr.below(8)] : rr.below(60));
                    for (int k = 0; k < 3; ++k) { Put<float>(e, 4 + 4 * k, GenF(rr, 50.f)); Put<float>(e, 0x10 + 4 * k, GenF(rr, 0.5f, S_NAN | S_ZERO | S_TINY)); }
                    Put<float>(e, 0x1c, rr.below(4) ? rr.f01() * 10.f : GenF(rr, 20.f));
                    Put<uint32_t>(e, 0x20, 0);                                                     // creator
                    Put<float>(e, 0x24, rr.below(2) ? (float)(CTimer::GetTimeInMS()) + GenF(rr, 5000.f) : GenF(rr, 1e6f));
                    Put<uint8>(e, 0x28, rr.below(3) != 0); Put<uint8>(e, 0x29, rr.below(2));
                }
                FillU(rr, (uint8*)0xC8AAB8, 0x70 * 80);                                          // weapon info flags (bSlowdown / bRangeIncreasesOverTime ...)
                CTimer::m_snTimeInMilliseconds = rr.u32() & 0xFFFFF; CTimer::m_FrameCounter = rr.u32() & 0xFFFF;
                CTimer::ms_fTimeStep = rr.below(6) ? 0.2f + rr.f01() * 2.f : GenF(rr, 3.f, S_NAN | S_ZERO | S_TINY);   // (no denormals / huge values: the pow underflow and the denormal float stores of the extended product are not what is tested)
            },
            [&] { sfx::g_rec.clear(); CShotInfo::Update(); ra = sfx::g_rec; return 0u; },
            [&] { sfx::g_rec.clear(); oracle::Fn<void __cdecl()>(0x739E60)(); re = sfx::g_rec; return 0u; }, 2);
        wiSave.Restore();
        bool same = ra.size() == re.size();
        for (size_t i = 0; same && i < ra.size(); ++i) same = ra[i].kind == re[i].kind && SameV(ra[i].pos, re[i].pos) && SameF(ra[i].a, re[i].a) && ra[i].ent == re[i].ent;
        Hit(!ra.empty());
        if (ok && same) return true;
        d += " port " + sfx::Dump(ra) + " exe " + sfx::Dump(re); return false;
    });
}

// CBulletInfo::Update, no hits (the scripted line of sight never hits) and no creators
static void TestBulletInfoUpdate() {
    using namespace wfx;
    Run("CBulletInfo::Update (no hits) 0x7360D0", [&](Rng& r, std::string& d) {
        constexpr size_t N = 0xC888B8 - 0xC88740;
        los::script.clear();
        std::vector<los::Call> ca, ce;
        const bool ok = Glob(r, d, 0xC88740, N, [&](Rng& rr, uint8* g) {
                FillU(rr, g, N);
                for (int i = 0; i < 8; ++i) {
                    uint8* e = g + i * 0x2c;
                    static const int types[] = { 22, 34, 34, 30, 31, 25, 38, 0 };
                    Put<int32_t>(e, 0, rr.below(3) ? types[rr.below(8)] : rr.below(60));
                    Put<uint32_t>(e, 4, 0);                                                           // creator
                    Put<float>(e, 8, rr.below(2) ? (float)CTimer::GetTimeInMS() + GenF(rr, 3000.f) : GenF(rr, 1e6f));
                    Put<uint8>(e, 0xC, rr.below(4) != 0);
                    const float sc = rr.below(4) ? 100.f : 21000.f;
                    for (int k = 0; k < 3; ++k) { Put<float>(e, 0x10 + 4 * k, GenF(rr, sc)); Put<float>(e, 0x1c + 4 * k, GenF(rr, rr.below(2) ? 3.f : 300.f)); }
                }
                CTimer::m_snTimeInMilliseconds = rr.u32() & 0xFFFFF;
                CTimer::ms_fTimeStep = rr.below(6) ? 0.2f + rr.f01() * 2.f : GenF(rr, 3.f);
            },
            [&] { los::Reset(); CBulletInfo::Update(); ca = los::calls; return 0u; },
            [&] { los::Reset(); oracle::Fn<void __cdecl()>(0x7360D0)(); ce = los::calls; return 0u; }, 2);
        Hit(!ca.empty());
        if (ok && los::SameCalls(ca, ce)) return true;
        d += " port " + los::Describe(ca) + " exe " + los::Describe(ce); return false;
    });
}


// CWeapon::Update / Reload / Shutdown / Initialise with a null owner (no ped, no audio) and no fx system; the CWeaponInfo table is randomised (models -1)
static void TestWeaponState() {
    using namespace wfx;
    auto fillInfos = [](Rng& rr) {
        FillU(rr, (uint8*)0xC8AAB8, 0x70 * 80);
        for (int i = 0; i < 80; ++i) {
            uint8* wi = (uint8*)0xC8AAB8 + i * 0x70;
            Put<int32_t>(wi, 0, rr.below(7)); Put<int32_t>(wi, 0xC, -1); Put<int32_t>(wi, 0x10, -1);
            Put<int16_t>(wi, 0x20, (int16_t)(rr.below(4) ? rr.below(100) : (int)rr.u32()));
            Put<int16_t>(wi, 0x6C, (int16_t)rr.below(18));                                      // aim offset index (std::array bounds)
        }
    };
    auto initW = [&](Rng& rr, uint8* b) {
        fillInfos(rr);
        FillU(rr, b, 0x1C);
        Put<int32_t>(b, 0, rr.below(47)); Put<int32_t>(b, 4, rr.below(7));
        Put<uint32_t>(b, 8, rr.below(3) ? rr.below(100) : rr.u32() & 0xFFFF); Put<uint32_t>(b, 0xC, rr.below(4) ? rr.below(200) : 0);
        Put<uint32_t>(b, 0x18, 0);                                                               // no fx system
        CTimer::m_snTimeInMilliseconds = rr.u32() & 0xFFFFFF; CTimer::m_snPreviousTimeInMilliseconds = CTimer::m_snTimeInMilliseconds - rr.below(100);
        Put<uint32_t>(b, 0x10, rr.below(2) ? CTimer::m_snTimeInMilliseconds + (int)rr.below(2000) - 1000 : rr.u32() & 0xFFFFFF);   // time for next shot
    };
    Snap wiSave(0xC8AAB8, 0x70 * 80);
    Run("CWeapon::Update(null owner) 0x73DB40", [&](Rng& r, std::string& d) {
        wiSave.Save();
        const bool ok = Obj<0x1C>(r, d, initW, [&](uint8* b) { reinterpret_cast<CWeapon*>(b)->Update(nullptr); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, void*)>(0x73DB40)(b, 0, nullptr); return 0u; }, 2);
        wiSave.Restore(); return ok;
    });
    Run("CWeapon::Reload(null owner) 0x73AEB0", [&](Rng& r, std::string& d) {
        wiSave.Save();
        const bool ok = Obj<0x1C>(r, d, initW, [&](uint8* b) { reinterpret_cast<CWeapon*>(b)->Reload(nullptr); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, void*)>(0x73AEB0)(b, 0, nullptr); return 0u; }, 2);
        wiSave.Restore(); return ok;
    });
    Run("CWeapon::Shutdown 0x73A380", [&](Rng& r, std::string& d) {
        wiSave.Save();
        const bool ok = Obj<0x1C>(r, d, initW, [&](uint8* b) { reinterpret_cast<CWeapon*>(b)->Shutdown(); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x73A380)(b, 0); return 0u; }, 2);
        wiSave.Restore(); return ok;
    });
    Run("CWeapon::Initialise(null owner) 0x73B4A0", [&](Rng& r, std::string& d) {
        wiSave.Save();
        const int ammo = r.below(4) == 0 ? (int)(r.u32() % 200000) - 1000 : r.below(200); const int type = r.below(47);
        const bool ok = Obj<0x1C>(r, d, initW, [&](uint8* b) { reinterpret_cast<CWeapon*>(b)->Initialise((eWeaponType)type, ammo, nullptr); return 0u; },
            [&](uint8* b) { oracle::Fn<void __fastcall(void*, int, int, int, void*)>(0x73B4A0)(b, 0, type, ammo, nullptr); return 0u; }, 2);
        wiSave.Restore(); return ok;
    });
    Run("CWeapon::StopWeaponEffect 0x73B360", [&](Rng& r, std::string& d) {
        return Obj<0x1C>(r, d, initW, [&](uint8* b) { reinterpret_cast<CWeapon*>(b)->StopWeaponEffect(); return 0u; }, [&](uint8* b) { oracle::Fn<void __fastcall(void*, int)>(0x73B360)(b, 0); return 0u; }, 2);
    });
}

static void TestWeapons() {
    TestWeaponMisc();
    TestWeaponState();
    TestHeatSeeking();
    TestShotInfoUpdate();
    TestBulletInfoUpdate();
}
