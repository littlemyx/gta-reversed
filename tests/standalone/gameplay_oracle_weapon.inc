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
static void InstallPortFxPatches();
namespace sfx {
struct Rec { int kind; CVector pos; float a; const void* ent; float b = 0.f, c = 0.f; };
inline std::vector<Rec> g_rec;
inline int      g_spray = 0;
inline CVector  g_sprayDir{};
inline bool     g_extinguish = false;
inline bool     g_pvl = false;
inline CVector  g_pvlPoint{};
}
void CWorld::SetCarsOnFire(CVector pos, float radius, CEntity* c) { sfx::g_rec.push_back({ 0, pos, radius, c }); }
void CWorld::SetWorldOnFire(CVector pos, float radius, CEntity* c) { sfx::g_rec.push_back({ 1, pos, radius, c }); }
eSprayPaintState CWorld::SprayPaintWorld(CVector& posn, CVector& outDir, float radius, bool f) { sfx::g_rec.push_back({ 2 + (int)f * 10, posn, radius, nullptr }); outDir = sfx::g_sprayDir; return (eSprayPaintState)sfx::g_spray; }
static bool __fastcall HostExtinguish(void*, int, CVector point, float r, float sz) { sfx::g_rec.push_back({ 3, point, r + sz * 1000.f, nullptr }); return sfx::g_extinguish; }
namespace sfx {
static void __cdecl ExeCarsOnFire(CVector pos, float radius, CEntity* c) { g_rec.push_back({ 0, pos, radius, c }); }
static void __cdecl ExeWorldOnFire(CVector pos, float radius, CEntity* c) { g_rec.push_back({ 1, pos, radius, c }); }
static int  __cdecl ExeSpray(CVector* posn, CVector* outDir, float radius, bool f) { g_rec.push_back({ 2 + (int)f * 10, *posn, radius, nullptr }); *outDir = g_sprayDir; return g_spray; }
static bool __fastcall ExeExtinguish(void*, int, CVector point, float r, float sz) { g_rec.push_back({ 3, point, r + sz * 1000.f, nullptr }); return g_extinguish; }
inline std::string Dump(const std::vector<Rec>& v) { std::string s = std::to_string(v.size()) + " effects"; for (size_t i = 0; i < v.size() && i < 6; ++i) s += " [" + std::to_string(v[i].kind) + V(v[i].pos) + F(v[i].a) + (v[i].b != 0.f || v[i].c != 0.f ? F(v[i].b) + F(v[i].c) : std::string()) + "]"; return s; }
}

static void TestShotInfoUpdate() {
    using namespace wfx;
    if (!Wanted("CShotInfo::Update")) return;
    oracle::Patch(0x5659F0, (void*)&sfx::ExeCarsOnFire);
    oracle::Patch(0x56B910, (void*)&sfx::ExeWorldOnFire);
    oracle::Patch(0x565B70, (void*)&sfx::ExeSpray);
    oracle::Patch(0x5394C0, (void*)&sfx::ExeExtinguish);
    InstallPortFxPatches();
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


// CExplosion::Update (creator-less / victim-less explosions): lights, fire / explosion world calls and fx creation are recorded on both sides
#include "PointLights.h"
#include "CreepingFire.h"
#include "Fx/FxManager.h"
void CPointLights::AddLight(uint8 lightType, CVector point, CVector direction, float radius, float red, float green, float blue, uint8 fogType, bool generateExtraShadows, CEntity* entityAffected) {
    sfx::g_rec.push_back({ 4 + lightType * 100, point, radius, entityAffected, red + 2.f * green + 4.f * blue, (float)fogType + 2.f * generateExtraShadows + direction.x + direction.y + direction.z });
}
void CWorld::TriggerExplosion(const CVector& point, float radius, float visibleDistance, CEntity* victim, CEntity* creator, bool processVehicleBombTimer, float damage) {
    sfx::g_rec.push_back({ 5, point, radius, creator, visibleDistance + 2.f * processVehicleBombTimer, damage + (victim ? 1000.f : 0.f) });
}
void CWorld::SetPedsOnFire(float x, float y, float z, float radius, CEntity* fireCreator) { sfx::g_rec.push_back({ 6, { x, y, z }, radius, fireCreator }); }
bool CWorld::ProcessVerticalLine(const CVector& origin, float distance, CColPoint& outColPoint, CEntity*& outEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, CStoredCollPoly* outCollPoly) {
    sfx::g_rec.push_back({ 7, origin, distance, nullptr, (float)(buildings + 2 * vehicles + 4 * peds + 8 * objects + 16 * dummies + 32 * doSeeThroughCheck), outCollPoly ? 1.f : 0.f });
    outEntity = nullptr; outColPoint.m_vecPoint = sfx::g_pvlPoint; return sfx::g_pvl;
}
bool CCreepingFire::TryToStartFireAtCoors(CVector pos, uint8 nGenerations, bool a5, bool bScriptFire, float fDistance) { sfx::g_rec.push_back({ 8, pos, fDistance, nullptr, (float)(nGenerations + 2 * a5 + 4 * bScriptFire) }); return false; }
FxSystem_c* FxManager_c::CreateFxSystem(const char* name, const CVector& point, RwMatrix* objectMatrix, bool ignoreBoundingChecks) { sfx::g_rec.push_back({ 9, point, (float)(unsigned char)name[10], objectMatrix, (float)ignoreBoundingChecks }); return nullptr; }
namespace sfx {
static void __cdecl ExeAddLight(uint8 lightType, CVector point, CVector direction, float radius, float red, float green, float blue, uint8 fogType, bool genShadows, CEntity* e) {
    g_rec.push_back({ 4 + lightType * 100, point, radius, e, red + 2.f * green + 4.f * blue, (float)fogType + 2.f * genShadows + direction.x + direction.y + direction.z });
}
static void __cdecl ExeTrigger(const CVector* point, float radius, float vis, CEntity* victim, CEntity* creator, bool bomb, float damage) { g_rec.push_back({ 5, *point, radius, creator, vis + 2.f * bomb, damage + (victim ? 1000.f : 0.f) }); }
static void __cdecl ExePeds(float x, float y, float z, float radius, CEntity* c) { g_rec.push_back({ 6, { x, y, z }, radius, c }); }
static bool __cdecl ExeVertical(const CVector* origin, float distance, CColPoint* cp, CEntity** outEntity, bool b, bool v, bool p, bool o, bool dm, bool st, void* poly) {
    g_rec.push_back({ 7, *origin, distance, nullptr, (float)(b + 2 * v + 4 * p + 8 * o + 16 * dm + 32 * st), poly ? 1.f : 0.f });
    *outEntity = nullptr; cp->m_vecPoint = g_pvlPoint; return g_pvl;
}
static bool __cdecl ExeCreeping(CVector pos, uint8 gen, bool a5, bool script, float dist) { g_rec.push_back({ 8, pos, dist, nullptr, (float)(gen + 2 * a5 + 4 * script) }); return false; }
static void* __fastcall ExeCreateFx(void*, int, const char* name, const CVector* point, void* mat, bool ign) { g_rec.push_back({ 9, *point, (float)(unsigned char)name[10], mat, (float)ign }); return nullptr; }
}

static void TestExplosionUpdate() {
    using namespace wfx;
    if (!Wanted("CExplosion::Update")) return;
    oracle::Patch(0x7000E0, (void*)&sfx::ExeAddLight);
    oracle::Patch(0x56B790, (void*)&sfx::ExeTrigger);
    oracle::Patch(0x565610, (void*)&sfx::ExePeds);
    oracle::Patch(0x5674E0, (void*)&sfx::ExeVertical);
    oracle::Patch(0x53A450, (void*)&sfx::ExeCreeping);
    oracle::Patch(0x4A9BE0, (void*)&sfx::ExeCreateFx);
    oracle::Patch(0x5659F0, (void*)&sfx::ExeCarsOnFire);
    oracle::Patch(0x56B910, (void*)&sfx::ExeWorldOnFire);
    Run("CExplosion::Update (no victim / creator) 0x737620", [&](Rng& r, std::string& d) {
        constexpr size_t N = 16 * 0x7C;
        std::vector<sfx::Rec> ra, re;
        sfx::g_pvl = r.below(2); sfx::g_pvlPoint = CVector{ GenF(r, 50.f), GenF(r, 50.f), GenF(r, 50.f) };
        const unsigned seed = r.u32();
        const bool ok = Glob(r, d, 0xC88950, N, [&](Rng& rr, uint8* g) {
                FillU(rr, g, N);
                CTimer::m_snTimeInMilliseconds = rr.u32() & 0xFFFFF; CTimer::m_FrameCounter = rr.u32() & 0xFFFF;
                CTimer::ms_fTimeStep = rr.below(6) ? 0.2f + rr.f01() * 2.f : GenF(rr, 3.f, S_NAN | S_ZERO | S_TINY | S_DEN);
                for (int i = 0; i < 16; ++i) {
                    uint8* e = g + i * 0x7C;
                    Put<int32_t>(e, 0, rr.below(4) ? (int)rr.below(14) : (int)rr.below(30));
                    for (int k = 0; k < 3; ++k) Put<float>(e, 4 + 4 * k, GenF(rr, 100.f));
                    Put<float>(e, 0x10, rr.below(2) ? rr.f01() * 30.f : GenF(rr, 30.f));                                // radius
                    Put<float>(e, 0x14, rr.below(2) ? 0.5f : GenF(rr, 2.f));                                           // propagation rate
                    Put<uint32_t>(e, 0x18, 0); Put<uint32_t>(e, 0x1C, 0);                                              // creator / victim
                    Put<float>(e, 0x20, rr.below(2) ? (float)CTimer::GetTimeInMS() + GenF(rr, 3000.f) : GenF(rr, 1e6f));       // expire time
                    Put<float>(e, 0x24, rr.below(2) ? 1.f : rr.f01());
                    Put<uint8>(e, 0x28, (uint8)(rr.below(3) ? rr.below(4) : rr.below(256))); Put<uint8>(e, 0x29, (uint8)rr.below(2)); Put<uint8>(e, 0x2A, (uint8)rr.below(2));
                    Put<uint32_t>(e, 0x30, rr.below(3) ? 0 : (uint32_t)(CTimer::GetTimeInMS() + (int)rr.below(1000) - 500));      // particles expire time
                    Put<float>(e, 0x34, rr.below(3) ? 0.f : 300.f * rr.f01());                                         // visible distance
                    Put<int32_t>(e, 0x3C, (int)rr.below(400) - 100);                                                   // fuel timer
                    for (int k = 0; k < 3; ++k) { Put<float>(e, 0x64 + 4 * k, rr.below(3) ? 0.f : rr.f01() * 2.f); Put<float>(e, 0x70 + 4 * k, 20.f + rr.f01() * 10.f); for (int c = 0; c < 3; ++c) Put<float>(e, 0x40 + 12 * k + 4 * c, GenF(rr, 1.f)); }
                }
            },
            [&] { SeedBoth(seed); sfx::g_rec.clear(); CExplosion::Update(); ra = sfx::g_rec; return 0u; },
            [&] { SeedBoth(seed); sfx::g_rec.clear(); oracle::Fn<void __cdecl()>(0x737620)(); re = sfx::g_rec; return 0u; }, 2);
        bool same = ra.size() == re.size();
        for (size_t i = 0; same && i < ra.size(); ++i) same = ra[i].kind == re[i].kind && SameV(ra[i].pos, re[i].pos) && SameF(ra[i].a, re[i].a) && SameF(ra[i].b, re[i].b) && SameF(ra[i].c, re[i].c) && ra[i].ent == re[i].ent;
        Hit(!ra.empty());
        if (ok && same) return true;
        d += " port " + sfx::Dump(ra) + " exe " + sfx::Dump(re);
        for (size_t i = 0; i < ra.size() && i < re.size(); ++i) if (ra[i].kind != re[i].kind || !SameV(ra[i].pos, re[i].pos) || !SameF(ra[i].a, re[i].a) || !SameF(ra[i].b, re[i].b) || !SameF(ra[i].c, re[i].c) || ra[i].ent != re[i].ent) { d += " FIRST DIFF #" + std::to_string(i) + " " + F(ra[i].b) + F(re[i].b) + F(ra[i].c) + F(re[i].c) + F(ra[i].a) + F(re[i].a); break; }
        return false;
    });
}


// CProjectileInfo::Update for grenades / tear gas / molotov / bombs / satchels / flares (no rocket types: their smoke needs an fx system); the callees are recorded on both sides
#include "Entity/Object/Projectile.h"
namespace pjx {
struct Rec { int kind; int idx; CVector a, b; float f; };
inline std::vector<Rec> g_rec;
inline uint8* g_base = nullptr;           // projectile buffers of the side that is running
constexpr size_t PS = 0x200;
inline int IdxOf(const void* p) { return p ? (int)(((const uint8*)p - g_base) / PS) : -1; }
inline int g_clearCounter = 0; inline unsigned g_clearMask = 0;
}
bool CWorld::GetIsLineOfSightClear(const CVector& origin, const CVector& target, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, bool doCameraIgnoreCheck) {
    const bool res = (pjx::g_clearMask >> (pjx::g_clearCounter++ & 31)) & 1;
    pjx::g_rec.push_back({ 1, res, origin, target, (float)(buildings + 2 * vehicles + 4 * peds + 8 * objects + 16 * dummies + 32 * doSeeThroughCheck + 64 * doCameraIgnoreCheck) });
    return res;
}
void CWorld::SetPedsChoking(float x, float y, float z, float radius, CEntity* gasCreator) { pjx::g_rec.push_back({ 2, 0, { x, y, z }, {}, radius }); }
namespace pjx {
static bool __cdecl ExeClear(const CVector* origin, const CVector* target, bool a, bool b, bool c, bool d, bool e, bool f, bool g) {
    const bool res = (g_clearMask >> (g_clearCounter++ & 31)) & 1;
    g_rec.push_back({ 1, res, *origin, *target, (float)(a + 2 * b + 4 * c + 8 * d + 16 * e + 32 * f + 64 * g) });
    return res;
}
static void __cdecl ExeChoking(float x, float y, float z, float radius, CEntity*) { g_rec.push_back({ 2, 0, { x, y, z }, {}, radius }); }
static void __cdecl ExeRemove(CProjectileInfo* info, CProjectile* proj) { g_rec.push_back({ 3, (int)(info - &gaProjectileInfo[0]), {}, {}, (float)IdxOf(proj) }); }
inline std::string Dump(const std::vector<Rec>& v) { std::string s = std::to_string(v.size()) + " calls"; for (size_t i = 0; i < v.size() && i < 6; ++i) s += " [" + std::to_string(v[i].kind) + "," + std::to_string(v[i].idx) + V(v[i].a) + V(v[i].b) + F(v[i].f) + "]"; return s; }
}

static void TestProjectileInfoUpdate() {
    using namespace wfx;
    if (!Wanted("CProjectileInfo::Update")) return;
    oracle::Patch(0x56A490, (void*)&pjx::ExeClear);
    oracle::Patch(0x565800, (void*)&pjx::ExeChoking);
    oracle::Patch(0x7388F0, (void*)&pjx::ExeRemove);
    PatchFn((void*)&CProjectileInfo::RemoveProjectile, (void*)&pjx::ExeRemove);
    Run("CProjectileInfo::Update (no rockets) 0x738B20", [&](Rng& r, std::string& d) {
        constexpr int NP = 32;
        alignas(16) static uint8 setA[NP][pjx::PS], setE[NP][pjx::PS], mats[2][NP][0x40];
        static const int types[] = { 16, 17, 18, 21, 39, 58, 58, 16, 17, 22 };
        pjx::g_clearMask = r.u32(); pjx::g_clearCounter = 0;
        const unsigned seed = r.u32();
        CTimer::m_snTimeInMilliseconds = r.u32() & 0xFFFFFF; CTimer::ms_fTimeStep = r.below(6) ? 0.2f + r.f01() * 2.f : GenF(r, 3.f, S_NAN | S_ZERO | S_TINY);
        // projectile entities (two identical copies, wired to their own matrices) and the info array
        for (int i = 0; i < NP; ++i) {
            FillU(r, setA[i], pjx::PS);
            auto& pr = *reinterpret_cast<CProjectile*>(setA[i]);
            pr.m_vecMoveSpeed = GenV(r, r.below(3) ? 0.03f : 1.f);
            pr.m_fElasticity = r.below(2) ? 0.5f : r.f01() * 0.2f;
            Put<uint32_t>(setA[i], 0x40, r.u32() & ~0x100u); if (r.below(8) == 0) Put<uint32_t>(setA[i], 0x40, Get<uint32_t>(setA[i], 0x40) | 0x100u);
            pr.m_nNumEntitiesCollided = (uint8)r.below(3); pr.m_apCollidedEntities[0] = nullptr; pr.m_pEntityIgnoredCollision = nullptr;
            pr.m_fDamageIntensity = 0.f; pr.m_pDamageEntity = nullptr; pr.m_pAttachedTo = nullptr;                // (satchel attach is excluded)
            FillF(r, mats[0][i], 0x40, 1.f);
            std::memcpy(mats[1][i], mats[0][i], 0x40);
        }
        std::memcpy(setE, setA, sizeof setE);
        Snap infoSave(0xC891A8, NP * 0x24), ptrSave(0xC89110, NP * 4);
        infoSave.Save(); ptrSave.Save();
        for (int i = 0; i < NP; ++i) {
            uint8* e = (uint8*)0xC891A8 + i * 0x24;
            Put<uint32_t>(e, 0, r.below(5) ? types[r.below(10)] : r.below(60)); Put<uint32_t>(e, 4, 0); Put<uint32_t>(e, 8, 0);
            Put<int32_t>(e, 0xC, r.below(3) == 0 ? 0 : r.below(2) ? (int)CTimer::GetTimeInMS() + (int)r.below(40000) - 20000 : (int)r.u32());
            if (Get<uint32_t>(e, 0) == 19 || Get<uint32_t>(e, 0) == 20) Put<uint32_t>(e, 0, 22);                  // (no rocket types: smoke particles need an fx system)
            if (Get<uint32_t>(e, 0) == 39) Put<int32_t>(e, 0xC, 0);                                               // (an expired satchel dereferences its creator: null crashes the exe as well)
            Put<uint8>(e, 0x10, (uint8)(r.below(4) != 0));
            for (int k = 0; k < 3; ++k) Put<float>(e, 0x14 + 4 * k, GenF(r, 50.f));
            Put<uint32_t>(e, 0x20, 0);
        }
        auto wire = [&](uint8 (*set)[pjx::PS], int side, bool on) { for (int i = 0; i < NP; ++i) Put<uint32_t>(set[i], 0x14, on ? (uint32_t)(uintptr_t)mats[side][i] : 0u); };
        Snap infoIn(0xC891A8, NP * 0x24); infoIn.Save();
        std::vector<pjx::Rec> ra, re;
        // port
        for (int i = 0; i < NP; ++i) { uint32_t p = (uint32_t)(uintptr_t)setA[i]; std::memcpy((uint8*)0xC89110 + 4 * i, &p, 4); }
        wire(setA, 0, true); pjx::g_base = &setA[0][0]; pjx::g_rec.clear(); pjx::g_clearCounter = 0; SeedBoth(seed);
        CProjectileInfo::Update(); ra = pjx::g_rec; wire(setA, 0, false);
        Snap infoPort(0xC891A8, NP * 0x24); infoPort.Save();
        // exe
        infoIn.Restore();
        for (int i = 0; i < NP; ++i) { uint32_t p = (uint32_t)(uintptr_t)setE[i]; std::memcpy((uint8*)0xC89110 + 4 * i, &p, 4); }
        wire(setE, 1, true); pjx::g_base = &setE[0][0]; pjx::g_rec.clear(); pjx::g_clearCounter = 0; SeedBoth(seed);
        oracle::Fn<void __cdecl()>(0x738B20)(); re = pjx::g_rec; wire(setE, 1, false);
        Snap infoExe(0xC891A8, NP * 0x24); infoExe.Save();
        infoSave.Restore(); ptrSave.Restore();
        bool ok = infoPort.SameAs(infoExe) && SameBlob(setA, setE, sizeof setA) && ra.size() == re.size();
        for (size_t i = 0; ok && i < ra.size(); ++i) ok = ra[i].kind == re[i].kind && ra[i].idx == re[i].idx && SameV(ra[i].a, re[i].a) && SameV(ra[i].b, re[i].b) && SameF(ra[i].f, re[i].f);
        Hit(!ra.empty());
        if (ok) return true;
        d = "info " + DiffDesc(infoPort.b.data(), infoExe.b.data(), infoPort.b.size(), "info") + " ents " + DiffDesc(setA, setE, sizeof setA, "ent") + " port " + pjx::Dump(ra) + " exe " + pjx::Dump(re);
        return false;
    });
}


// CExplosion::AddExplosion with creator = victim = null (FindPlayerPed() == null as well: the "havoc" counters of player 0 are updated); effects recorded on both sides
#include "Shadows.h"
#include "GameLogic.h"
#include "Camera.h"
#include "WaterLevel.h"
#include "Audio/Entities/AEExplosionAudioEntity.h"
namespace aex {
inline unsigned g_hitMask = 0, g_n = 0;
inline float    g_water = 0.f; inline bool g_hasWater = false, g_coop = false;
}
float CWorld::FindGroundZFor3DCoord(CVector coord, bool* outResult, CEntity** outEntity) {
    const bool hit = (aex::g_hitMask >> (aex::g_n++ & 31)) & 1;
    sfx::g_rec.push_back({ 10, coord, (float)hit });
    if (outResult) *outResult = hit;
    if (outEntity) *outEntity = nullptr;
    return coord.z - 3.0f + 0.25f * (float)(aex::g_n & 7);
}
bool CWaterLevel::GetWaterLevelNoWaves(CVector pos, float* out, float* a, float* b) { sfx::g_rec.push_back({ 11, pos, (float)aex::g_hasWater }); if (out) *out = aex::g_water; return aex::g_hasWater; }
void CAEExplosionAudioEntity::AddAudioEvent(eAudioEvents ev, CVector& posn, float vol) { sfx::g_rec.push_back({ 12, posn, vol, nullptr, (float)ev }); }
static CFire* __fastcall HostStartFire(void*, int, CVector pos, float size, uint8 unused, CEntity* creator, uint32 nTimeToBurn, int8 nGenerations, uint8 unused_) { sfx::g_rec.push_back({ 13, pos, size, creator, (float)nTimeToBurn, (float)(nGenerations + 16 * unused_) }); return nullptr; }
void CShadows::AddPermanentShadow(uint8 type, RwTexture* texture, CVector* posn, float topX, float topY, float rightX, float rightY, int16 intensity, uint8 red, uint8 green, uint8 blue, float drawDistance, uint32 time, float upDistance) {
    sfx::g_rec.push_back({ 14, *posn, topX + 2.f * topY + 3.f * rightX + 4.f * rightY, texture, (float)(intensity + red + green + blue + type) + drawDistance, (float)time + upDistance });
}
static void __fastcall HostCamShake(void*, int, float strength, CVector from) { sfx::g_rec.push_back({ 15, from, strength }); }
static void __fastcall HostShakeDist(CPad* self, int, int16 time, uint8 frequency, CVector pos) { sfx::g_rec.push_back({ 16, pos, (float)(time * 1000 + frequency), self }); }
bool CGameLogic::IsCoopGameGoingOn() { sfx::g_rec.push_back({ 17, {}, 0.f }); return aex::g_coop; }
static void InstallPortFxPatches() {
    PatchHostFn(PmAddr(&CFireManager::ExtinguishPointWithWater), (void*)&HostExtinguish);
    PatchHostFn(PmAddr(static_cast<CFire* (CFireManager::*)(CVector, float, uint8, CEntity*, uint32, int8, uint8)>(&CFireManager::StartFire)), (void*)&HostStartFire);
    PatchHostFn(PmAddr(&CCamera::CamShake), (void*)&HostCamShake);
    PatchHostFn(PmAddr(&CPad::StartShake_Distance), (void*)&HostShakeDist);
}
namespace aex {
static float __cdecl ExeGround(float x, float y, float z, bool* outResult, CEntity** outEntity) {
    const bool hit = (g_hitMask >> (g_n++ & 31)) & 1;
    sfx::g_rec.push_back({ 10, { x, y, z }, (float)hit });
    if (outResult) *outResult = hit;
    if (outEntity) *outEntity = nullptr;
    return z - 3.0f + 0.25f * (float)(g_n & 7);
}
static bool __cdecl ExeWater(float x, float y, float z, float* out, float*, float*) { sfx::g_rec.push_back({ 11, { x, y, z }, (float)g_hasWater }); if (out) *out = g_water; return g_hasWater; }
static void __fastcall ExeAudio(void*, int, int ev, CVector* posn, float vol) { sfx::g_rec.push_back({ 12, *posn, vol, nullptr, (float)ev }); }
static void* __fastcall ExeFire(void*, int, CVector pos, float size, uint8 unused, CEntity* creator, uint32 t, int8 gens, uint8 unused_) { sfx::g_rec.push_back({ 13, pos, size, creator, (float)t, (float)(gens + 16 * unused_) }); return nullptr; }
static void __cdecl ExeShadow(uint8 type, RwTexture* texture, CVector* posn, float topX, float topY, float rightX, float rightY, int16 intensity, uint8 red, uint8 green, uint8 blue, float drawDistance, uint32 time, float upDistance) {
    sfx::g_rec.push_back({ 14, *posn, topX + 2.f * topY + 3.f * rightX + 4.f * rightY, texture, (float)(intensity + red + green + blue + type) + drawDistance, (float)time + upDistance });
}
static void __fastcall ExeShake(void*, int, float strength, CVector from) { sfx::g_rec.push_back({ 15, from, strength }); }
static void __fastcall ExePadShake(void* self, int, int time, int freq, CVector pos) { sfx::g_rec.push_back({ 16, pos, (float)((int16)time * 1000 + (uint8)freq), self }); }
static bool __cdecl ExeCoop() { sfx::g_rec.push_back({ 17, {}, 0.f }); return g_coop; }
}

static void TestAddExplosion() {
    using namespace wfx;
    if (!Wanted("CExplosion::AddExplosion")) return;
    oracle::Patch(0x7000E0, (void*)&sfx::ExeAddLight);
    oracle::Patch(0x56B790, (void*)&sfx::ExeTrigger);
    oracle::Patch(0x4A9BE0, (void*)&sfx::ExeCreateFx);
    oracle::Patch(0x5696C0, (void*)&aex::ExeGround);
    oracle::Patch(0x6E8580, (void*)&aex::ExeWater);
    oracle::Patch(0x4DCBE0, (void*)&aex::ExeAudio);
    oracle::Patch(0x539F00, (void*)&aex::ExeFire);
    oracle::Patch(0x706F60, (void*)&aex::ExeShadow);
    oracle::Patch(0x50A9F0, (void*)&aex::ExeShake);
    oracle::Patch(0x53F9A0, (void*)&aex::ExePadShake);
    oracle::Patch(0x441390, (void*)&aex::ExeCoop);
    Run("CExplosion::AddExplosion (no creator / victim) 0x736A50", [&](Rng& r, std::string& d) {
        constexpr size_t N = 16 * 0x7C;
        std::vector<sfx::Rec> ra, re;
        const int type = (int)r.below(13);                                                       // (types > 12 are ignored by the exe's switch; the port's bInvisible pre-clear would differ)
        const CVector pos = GenV(r, r.below(2) ? 100.f : 3000.f);
        const uint32 lifetime = r.below(2) ? 0 : r.below(5000); const uint8 sound = (uint8)r.below(2); const uint8 inv = (uint8)r.below(3) == 0;
        const float shake = r.below(2) ? -1.f : GenF(r, 2.f);
        aex::g_hitMask = r.u32(); aex::g_n = 0; aex::g_water = GenF(r, 50.f); aex::g_hasWater = r.below(2); aex::g_coop = r.below(3) == 0;
        const unsigned seed = r.u32();
        Snap playersSave(0xB7CD98, 0x400); playersSave.Save();
        std::memset((void*)0xB7CD98, 0, 0x400);                                                   // player 0 ped = null
        const bool ok = Glob(r, d, 0xC88950, N, [&](Rng& rr, uint8* g) {
                FillU(rr, g, N);
                for (int i = 0; i < 16; ++i) { uint8* e = g + i * 0x7C; Put<uint8>(e, 0x28, (uint8)(rr.below(3) ? rr.below(3) : 0)); Put<uint32_t>(e, 0x18, 0); Put<uint32_t>(e, 0x1C, 0); }
                CTimer::m_snTimeInMilliseconds = rr.u32() & 0xFFFFF;
            },
            [&] { SeedBoth(seed); aex::g_n = 0; sfx::g_rec.clear(); CExplosion::AddExplosion(nullptr, nullptr, (eExplosionType)type, pos, lifetime, sound, shake, inv); ra = sfx::g_rec; return 0u; },
            [&] { SeedBoth(seed); aex::g_n = 0; sfx::g_rec.clear(); oracle::Fn<void __cdecl(void*, void*, int, float, float, float, uint32, uint8, float, uint8)>(0x736A50)(nullptr, nullptr, type, pos.x, pos.y, pos.z, lifetime, sound, shake, inv); re = sfx::g_rec; return 0u; }, 2);
        // havoc counters of the player 0 info (written when FindPlayerPed() == creator)
        const uint32_t havocA = Get<uint32_t>((uint8*)0xB7CD98, 0x140), havocBytes = 0; (void)havocA; (void)havocBytes;
        playersSave.Restore();
        bool same = ra.size() == re.size();
        for (size_t i = 0; same && i < ra.size(); ++i) same = ra[i].kind == re[i].kind && SameV(ra[i].pos, re[i].pos) && SameF(ra[i].a, re[i].a) && SameF(ra[i].b, re[i].b) && SameF(ra[i].c, re[i].c) && ra[i].ent == re[i].ent;
        Hit(!ra.empty());
        if (ok && same) return true;
        d += " type " + std::to_string(type) + " port " + sfx::Dump(ra) + " exe " + sfx::Dump(re);
        for (size_t i = 0; i < ra.size() && i < re.size(); ++i) if (ra[i].kind != re[i].kind || !SameV(ra[i].pos, re[i].pos) || !SameF(ra[i].a, re[i].a) || !SameF(ra[i].b, re[i].b) || !SameF(ra[i].c, re[i].c) || ra[i].ent != re[i].ent) { d += " FIRST DIFF #" + std::to_string(i) + " " + F(ra[i].a) + F(re[i].a) + F(ra[i].b) + F(re[i].b) + F(ra[i].c) + F(re[i].c); break; }
        return false;
    });
}

static void TestWeapons() {
    TestWeaponMisc();
    TestAddExplosion();
    TestProjectileInfoUpdate();
    TestExplosionUpdate();
    TestWeaponState();
    TestHeatSeeking();
    TestShotInfoUpdate();
    TestBulletInfoUpdate();
}
