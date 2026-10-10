// (e) closed gameplay helpers outside the big areas: particle key-curve interpolation (Fx), radar polygon clipping, garage containment, compressed matrices, fire queries
#include "Fx/Blueprints/Interp/FxInterpInfoFloat.h"
#include "Fx/Blueprints/Interp/FxInterpInfo32.h"
#include "Fx/Blueprints/Interp/FxInterpInfo255.h"
#include "Fx/Blueprints/Interp/FxInterpInfoU255.h"
#include "Radar.h"
#include "Garage.h"
#include "CompressedMatrixNotAligned.h"
#include "FireManager.h"
#include "Fire.h"
#include "VehicleRecording.h"
#include "Pad.h"

namespace mfx {
// key curves: up to 6 keys, up to 4 attributes; the layout is that of FxInterpInfo_c (+ m_Keys at +0xC) for all four key types
struct Curve {
    alignas(16) uint8 obj[0x20] {};
    uint16 times[8] {};
    alignas(16) uint8 keys[4][8 * 4] {};   // per attribute: up to 8 keys of float / int16 / uint16 (sized for float)
    void* keyPtrs[4] {};
};
inline FxInterpInfo_c& Base(Curve& c) { return *reinterpret_cast<FxInterpInfo_c*>(c.obj); }
inline void Build(Rng& r, Curve& c, int keyType /*0 float, 1 int16 (32 / 255), 2 uint16 (U255)*/) {
    auto& b = Base(c);
    std::memset(c.obj, 0, sizeof c.obj);
    b.m_bLooped = r.below(3) == 0; b.m_nNumKeys = (int8)(1 + r.below(6)); b.m_nCount = (int16)(1 + r.below(4));
    uint16 t = 0;
    for (int i = 0; i < 8; ++i) { t = (uint16)(t + (r.below(4) ? 1 + r.below(300) : r.below(2000))); c.times[i] = i == 0 && r.below(3) ? 0 : t; }
    if (r.below(10) == 0) c.times[r.below(8)] = (uint16)r.u32();
    b.m_pTimes = c.times;
    for (int a = 0; a < 4; ++a) {
        c.keyPtrs[a] = c.keys[a];
        for (int k = 0; k < 8; ++k) {
            if (keyType == 0) reinterpret_cast<float*>(c.keys[a])[k] = r.below(8) ? GenF(r, 2.f, S_NOHUGE) : GenF(r, 1e5f, S_NOHUGE);
            else if (keyType == 1) reinterpret_cast<int16_t*>(c.keys[a])[k] = (int16_t)r.u32();
            else reinterpret_cast<uint16_t*>(c.keys[a])[k] = (uint16_t)r.u32();
        }
    }
    *reinterpret_cast<void**>(c.obj + 0xC) = c.keyPtrs;
}
}

// ---- CPad: the zero-argument input queries on a randomised pad (both pads + the globals around them); return value and all pad memory are compared
#define PADFN(name, va) { #name " " #va, va, [](CPad* p, bool exe) -> int64_t { using R = decltype(p->name()); if (!exe) return (int64_t)p->name(); return (int64_t)oracle::Fn<R __fastcall(CPad*, int)>(va)(p, 0); } }
#define PADFNB(name, va, arg) { #name " " #va " " #arg, va, [](CPad* p, bool exe) -> int64_t { using R = decltype(p->name(arg)); if (!exe) return (int64_t)p->name(arg); return (int64_t)oracle::Fn<R __fastcall(CPad*, int, bool)>(va)(p, 0, arg); } }
#define PADFNP(name, va) { #name " " #va " (null ped)", va, [](CPad* p, bool exe) -> int64_t { using R = decltype(p->name(nullptr)); if (!exe) return (int64_t)p->name(nullptr); return (int64_t)oracle::Fn<R __fastcall(CPad*, int, CPed*)>(va)(p, 0, nullptr); } }
struct PadFn { const char* n; unsigned va; int64_t (*f)(CPad*, bool); };
static void TestPad() {
    if (std::getenv("PRINT_OFFS")) std::printf("CPad offsets: Mode %zX Disable %zX EnterCar %zX Duck %zX FireWeapon %zX FireL1 %zX CycleWeapon %zX Jump %zX VitalStats %zX NewState %zX OldState %zX\n", offsetof(CPad, Mode), offsetof(CPad, DisablePlayerControls), offsetof(CPad, bDisablePlayerEnterCar), offsetof(CPad, bDisablePlayerDuck), offsetof(CPad, bDisablePlayerFireWeapon), offsetof(CPad, bDisablePlayerFireWeaponWithL1), offsetof(CPad, bDisablePlayerCycleWeapon), offsetof(CPad, bDisablePlayerJump), offsetof(CPad, bDisablePlayerDisplayVitalStats), offsetof(CPad, NewState), offsetof(CPad, OldState));
    static const PadFn fns[] = {
        PADFN(GetSteeringLeftRight, 0x53FB80), PADFN(GetSteeringUpDown, 0x53FBD0), PADFN(GetCarGunLeftRight, 0x53FC50), PADFN(GetCarGunUpDown, 0x53FC10),
        PADFN(GetCarGunFired, 0x53FF90), PADFN(CarGunJustDown, 0x53FFE0), PADFN(GetPedWalkLeftRight, 0x53FC90), PADFN(GetPedWalkUpDown, 0x53FD30),
        PADFN(GetLookLeft, 0x53FDD0), PADFN(GetLookRight, 0x53FE10), PADFN(GetLookBehindForCar, 0x53FE70), PADFN(GetLookBehindForPed, 0x53FEC0),
        PADFN(GetHorn, 0x53FEE0), PADFN(HornJustDown, 0x53FF30), PADFN(GetHandBrake, 0x540040), PADFN(GetBrake, 0x540080), PADFN(GetExitVehicle, 0x5400D0),
        PADFN(ExitVehicleJustDown, 0x540120), PADFNB(GetMeleeAttack, 0x540340, true), PADFNB(GetMeleeAttack, 0x540340, false), PADFNB(MeleeAttackJustDown, 0x540390, true), PADFNB(MeleeAttackJustDown, 0x540390, false), PADFN(GetAccelerate, 0x5403F0),
        PADFN(GetAccelerateJustDown, 0x540440), PADFN(CycleWeaponLeftJustDown, 0x540610), PADFN(CycleWeaponRightJustDown, 0x540640), PADFN(GetTarget, 0x540670),
        PADFN(GetSprint, 0x5407A0), PADFN(SprintJustDown, 0x5407F0), PADFNP(GetDisplayVitalStats, 0x5408B0), PADFN(CollectPickupJustDown, 0x540A70),
        PADFN(GetForceCameraBehindPlayer, 0x540AE0), PADFN(SniperZoomIn, 0x540B30), PADFN(SniperZoomOut, 0x540B80), PADFNP(WeaponJustDown, 0x540250),
        PADFN(GetEnterTargeting, 0x5406B0), PADFNP(GetWeapon, 0x540180), PADFN(NextStationJustUp, 0x5405B0), PADFN(LastStationJustUp, 0x5405E0),
        PADFN(GetHydraulicJump, 0x53FF70), PADFN(GetDuck, 0x540700), PADFN(DuckJustDown, 0x540720), PADFN(GetJump, 0x540750), PADFN(JumpJustDown, 0x540770),
        PADFN(ShiftTargetLeftJustDown, 0x540850), PADFN(ShiftTargetRightJustDown, 0x540880), PADFN(GetGroupControlForward, 0x541190), PADFN(GetGroupControlBack, 0x5411B0),
        PADFN(ConversationYesJustDown, 0x5411D0), PADFN(ConversationNoJustDown, 0x541200), PADFN(GroupControlForwardJustDown, 0x541230), PADFN(GroupControlBackJustDown, 0x541260),
        PADFN(LookAroundLeftRight, 0x540E80), PADFN(LookAroundUpDown, 0x540F80), PADFN(GetAnaloguePadUp, 0x540950), PADFN(GetAnaloguePadLeft, 0x5409B0),
        PADFN(GetAnaloguePadRight, 0x5409E0), PADFN(GetAnaloguePadDown, 0x540980),
    };
    for (const auto& fn : fns) {
        Run(std::string("CPad::") + fn.n, [=](Rng& r, std::string& d) {
            constexpr uintptr_t BASE = 0xB73400, N = 0x300;   // both pads + the function-local statics up to 0xB73700 (oldfStickX / Y ...)
            Snap in(BASE, N), pa(BASE, N), pe(BASE, N);
            auto* b = reinterpret_cast<uint8_t*>(BASE);
            for (size_t i = 0; i < N; ++i) b[i] = (uint8_t)(r.below(3) ? r.u32() : (r.below(2) ? 0 : 0xFF));
            CPad& pad = CPad::Pads[r.below(2)];
            pad.Mode = (int16)(r.below(8) ? r.below(4) : r.below(6));
            pad.DrunkDrivingBufferUsed = r.below(10);   // index into SteeringLeftRightBuffer[10]
            pad.DisablePlayerControls = r.below(5) == 0;
            *reinterpret_cast<uint8_t*>(0x8CD782) = (uint8_t)r.below(2);
            CCamera::m_bUseMouse3rdPerson = r.below(2);
            in.Save();
            const int64_t ra = fn.f(&pad, false); pa.Save(); in.Restore();
            const int64_t re = fn.f(&pad, true);  pe.Save(); in.Restore();
            Hit(ra != 0);
            if (ra == re && pa.SameAs(pe)) return true;
            d = "mode " + std::to_string(pad.Mode) + " disabled " + std::to_string(pad.DisablePlayerControls) + " got " + std::to_string(ra) + " exe " + std::to_string(re) + DiffDesc(pa.b.data(), pe.b.data(), pa.b.size(), "pad");
            return false;
        });
    }
}

static_assert(sizeof(CFire) == 0x28);

static void TestMisc() {
    TestPad();
    // ---- Fx interpolation
    struct FxVar { const char* n; unsigned va; int type; };
    static const FxVar vs[] = { { "FxInterpInfoFloat_c::GetVal(out) 0x4A8470", 0x4A8470, 0 }, { "FxInterpInfo32_c::GetVal(out) 0x4A89C0", 0x4A89C0, 1 }, { "FxInterpInfo255_c::GetVal(out) 0x4A8B80", 0x4A8B80, 1 }, { "FxInterpInfoU255_c::GetVal(out) 0x4A8800", 0x4A8800, 2 } };
    for (const auto& v : vs) {
        Run(v.n, [=](Rng& r, std::string& d) {
            mfx::Curve c; mfx::Build(r, c, v.type);
            const float delta = r.below(4) ? r.f01() * 4.f : GenF(r, 40.f, S_NOHUGE);
            float oa[4] = { 11, 12, 13, 14 }, oe[4] = { 11, 12, 13, 14 };
            switch (v.va) {
            case 0x4A8470: reinterpret_cast<FxInterpInfoFloat_c*>(c.obj)->GetVal(oa, delta); break;
            case 0x4A89C0: reinterpret_cast<FxInterpInfo32_c*>(c.obj)->GetVal(oa, delta); break;
            case 0x4A8B80: reinterpret_cast<FxInterpInfo255_c*>(c.obj)->GetVal(oa, delta); break;
            default:       reinterpret_cast<FxInterpInfoU255_c*>(c.obj)->GetVal(oa, delta); break;
            }
            oracle::Fn<void __fastcall(void*, int, float*, float)>(v.va)(c.obj, 0, oe, delta);
            if (SameBlob(oa, oe, 16)) return true;
            d = "keys " + std::to_string(mfx::Base(c).m_nNumKeys) + " attrs " + std::to_string(mfx::Base(c).m_nCount) + " loop " + std::to_string(mfx::Base(c).m_bLooped) + " delta " + F(delta) + " got " + F(oa[0]) + F(oa[1]) + F(oa[2]) + F(oa[3]) + " exe " + F(oe[0]) + F(oe[1]) + F(oe[2]) + F(oe[3]) + " times " + Hex(c.times, 16);
            return false;
        });
    }
    Run("FxInterpInfoFloat_c::GetVal(attrib) 0x4A85C0", [](Rng& r, std::string& d) {
        mfx::Curve c; mfx::Build(r, c, 0);
        const int attrib = r.below(4);
        const float time = r.below(4) ? r.f01() * 4.f : GenF(r, 40.f, S_NOHUGE), delta = r.below(4) ? r.f01() * 2.f : GenF(r, 5.f, S_NOHUGE);
        const float ra = reinterpret_cast<FxInterpInfoFloat_c*>(c.obj)->GetVal(attrib, time, delta);
        const float re = oracle::Fn<float __fastcall(void*, int, int, float, float)>(0x4A85C0)(c.obj, 0, attrib, time, delta);
        if (SameF(ra, re)) return true;
        d = "keys " + std::to_string(mfx::Base(c).m_nNumKeys) + " attrib " + std::to_string(attrib) + " time " + F(time) + " delta " + F(delta) + " got " + F(ra) + " exe " + F(re) + " times " + Hex(c.times, 16);
        return false;
    });
    // ---- radar
    Run("LineRadarBoxCollision 0x584E00", [](Rng& r, std::string& d) {
        const CVector2D a{ GenF(r, 3.f), GenF(r, 3.f) }, b{ GenF(r, 3.f), GenF(r, 3.f) };
        CVector2D oa{ 7, 8 }, oe{ 7, 8 };
        const int ra = LineRadarBoxCollision(oa, a, b);
        const int re = oracle::Fn<int __cdecl(CVector2D*, const CVector2D*, const CVector2D*)>(0x584E00)(&oe, &a, &b);
        Hit(ra != -1);
        if (ra == re && SameF(oa.x, oe.x) && SameF(oa.y, oe.y)) return true;
        d = "a " + F(a.x) + F(a.y) + " b " + F(b.x) + F(b.y) + " got " + std::to_string(ra) + F(oa.x) + F(oa.y) + " exe " + std::to_string(re) + F(oe.x) + F(oe.y);
        return false;
    });
    Run("CRadar::ClipRadarPoly 0x585040", [](Rng& r, std::string& d) {
        CVector2D in[4];
        const float sc = r.below(3) ? 2.f : 6.f;
        for (auto& p : in) p = { GenF(r, sc), GenF(r, sc) };
        if (r.below(3) == 0) { const float h = r.f01() * 6.28f, w = r.f01() * 3.f + 0.1f, l = r.f01() * 3.f + 0.1f; const CVector2D c{ GenF(r, 1.5f), GenF(r, 1.5f) }; in[0] = { c.x + std::cos(h) * w, c.y + std::sin(h) * w }; in[1] = { c.x - std::sin(h) * l, c.y + std::cos(h) * l }; in[2] = { c.x - std::cos(h) * w, c.y - std::sin(h) * w }; in[3] = { c.x + std::sin(h) * l, c.y - std::cos(h) * l }; }
        CVector2D oa[16], oe[16]; for (int i = 0; i < 16; ++i) oa[i] = oe[i] = { 7, 8 };
        const int ra = CRadar::ClipRadarPoly(oa, in);
        const int re = oracle::Fn<int __cdecl(CVector2D*, const CVector2D*)>(0x585040)(oe, in);
        Hit(ra > 0);
        if (ra == re && SameBlob(oa, oe, sizeof oa)) return true;
        d = "in " + F(in[0].x) + F(in[0].y) + F(in[1].x) + F(in[1].y) + F(in[2].x) + F(in[2].y) + F(in[3].x) + F(in[3].y) + " got " + std::to_string(ra) + " exe " + std::to_string(re) + DiffDesc(oa, oe, sizeof oa, "out");
        return false;
    });
    // ---- garage containment
    for (int rad = 0; rad < 2; ++rad) {
        Run(rad ? "CGarage::IsPointInsideGarage(radius) 0x4487D0" : "CGarage::IsPointInsideGarage 0x448740", [=](Rng& r, std::string& d) {
            alignas(16) uint8 gb[sizeof(CGarage)] {};
            auto& g = *reinterpret_cast<CGarage*>(gb);
            g.m_vPosn = GenV(r, 100.f); g.m_fTopZ = g.m_vPosn.z + (r.below(4) ? r.f01() * 10.f : GenF(r, 10.f));
            const float h = r.f01() * 6.2831853f;
            g.m_vDirectionA = { std::cos(h), std::sin(h) }; g.m_vDirectionB = { -std::sin(h), std::cos(h) };
            if (r.below(8) == 0) { g.m_vDirectionA = { GenF(r, 1.f), GenF(r, 1.f) }; g.m_vDirectionB = { GenF(r, 1.f), GenF(r, 1.f) }; }
            g.m_fWidth = r.below(4) ? r.f01() * 20.f : GenF(r, 40.f); g.m_fHeight = r.below(4) ? r.f01() * 20.f : GenF(r, 40.f);
            const CVector p = r.below(2) ? g.m_vPosn + CVector{ GenF(r, 15.f), GenF(r, 15.f), GenF(r, 8.f) } : GenV(r, 100.f);
            const float radius = r.below(4) ? r.f01() * 4.f : GenF(r, 8.f);
            bool ra, re;
            if (rad) { ra = g.IsPointInsideGarage(p, radius); re = oracle::Fn<bool __fastcall(CGarage*, int, CVector, float)>(0x4487D0)(&g, 0, p, radius); }
            else     { ra = g.IsPointInsideGarage(p);         re = oracle::Fn<bool __fastcall(CGarage*, int, CVector)>(0x448740)(&g, 0, p); }
            Hit(ra);
            if (ra == re) return true;
            d = "garage " + V(g.m_vPosn) + " topz " + F(g.m_fTopZ) + " A " + F(g.m_vDirectionA.x) + F(g.m_vDirectionA.y) + " B " + F(g.m_vDirectionB.x) + F(g.m_vDirectionB.y) + " w/h " + F(g.m_fWidth) + F(g.m_fHeight) + " p " + V(p) + " r " + F(radius) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
            return false;
        });
    }
    Run("CMatrix::Reorthogonalise 0x59B6A0", [](Rng& r, std::string& d) {
        alignas(16) uint8 ma[0x48] {}, me[0x48] {};
        GenMatrix(r, ma, 10.f);
        float* f = reinterpret_cast<float*>(ma);   // perturb a clean rotation slightly (the usual input) or keep the random one
        if (r.below(3)) for (int i : { 0, 1, 2, 4, 5, 6, 8, 9, 10 }) f[i] += GenF(r, 0.05f, S_ZERO | S_TINY | S_DEN);
        std::memcpy(me, ma, sizeof ma);
        reinterpret_cast<CMatrix*>(ma)->Reorthogonalise();
        oracle::Fn<void __fastcall(CMatrix*, int)>(0x59B6A0)(reinterpret_cast<CMatrix*>(me), 0);
        if (SameBlob(ma, me, 0x40)) return true;
        d = "got " + Hex(ma, 0x40) + " exe " + Hex(me, 0x40);
        return false;
    });
    {   // CMatrix ops not covered by game_oracle_test (the trig ones are dominated by the x87 transcendental audit)
        auto genM = [](Rng& r, uint8* m) { std::memset(m, 0, 0x48); GenMatrix(r, m, 10.f); };
        Run("CMatrix::ForceUpVector 0x59B7E0", [=](Rng& r, std::string& d) {
            alignas(16) uint8 ma[0x48], me[0x48]; genM(r, ma); std::memcpy(me, ma, sizeof ma);
            const CVector up = r.below(2) ? GenDir(r) : GenV(r, 2.f);
            reinterpret_cast<CMatrix*>(ma)->ForceUpVector(up);
            oracle::Fn<void __fastcall(CMatrix*, int, CVector)>(0x59B7E0)(reinterpret_cast<CMatrix*>(me), 0, up);
            if (SameBlob(ma, me, 0x40)) return true;
            d = "up " + V(up) + " got " + Hex(ma, 0x40) + " exe " + Hex(me, 0x40);
            return false;
        });
        Run("CMatrix::SetRotate(x, y, z) 0x59B120", [=](Rng& r, std::string& d) {
            alignas(16) uint8 ma[0x48], me[0x48]; genM(r, ma); std::memcpy(me, ma, sizeof ma);
            const float x = GenF(r, 7.f, S_NOHUGE), y = GenF(r, 7.f, S_NOHUGE), z = GenF(r, 7.f, S_NOHUGE);
            reinterpret_cast<CMatrix*>(ma)->SetRotate(x, y, z);
            oracle::Fn<void __fastcall(CMatrix*, int, float, float, float)>(0x59B120)(reinterpret_cast<CMatrix*>(me), 0, x, y, z);
            if (SameBlob(ma, me, 0x40)) return true;
            d = "angles " + F(x) + F(y) + F(z) + " got " + Hex(ma, 0x40) + " exe " + Hex(me, 0x40);
            return false;
        });
        struct AxisFn { const char* n; unsigned va; int axis; };
        static const AxisFn axes[] = { { "CMatrix::SetRotateX 0x59B060", 0x59B060, 0 }, { "CMatrix::SetRotateY 0x59B0A0", 0x59B0A0, 1 }, { "CMatrix::SetRotateZ 0x59B0E0", 0x59B0E0, 2 } };
        for (const auto& ax : axes) {
            Run(ax.n, [=](Rng& r, std::string& d) {
                alignas(16) uint8 ma[0x48], me[0x48]; genM(r, ma); std::memcpy(me, ma, sizeof ma);
                const float a = GenF(r, 7.f, S_NOHUGE);
                auto* m = reinterpret_cast<CMatrix*>(ma);
                if (ax.axis == 0) m->SetRotateX(a); else if (ax.axis == 1) m->SetRotateY(a); else m->SetRotateZ(a);
                oracle::Fn<void __fastcall(CMatrix*, int, float)>(ax.va)(reinterpret_cast<CMatrix*>(me), 0, a);
                if (SameBlob(ma, me, 0x40)) return true;
                d = "angle " + F(a) + " got " + Hex(ma, 0x40) + " exe " + Hex(me, 0x40);
                return false;
            });
        }
        Run("CMatrix::ConvertToEulerAngles 0x59A840", [=](Rng& r, std::string& d) {
            alignas(16) uint8 ma[0x48]; genM(r, ma);
            if (r.below(2)) { const float h = r.f01() * 6.28f, p = (r.f01() - 0.5f) * 3.f; reinterpret_cast<CMatrix*>(ma)->SetRotate(p, 0.f, h); }
            const uint32 flags = r.below(3) ? (uint32)(r.below(2) | (r.below(6) << 2) | (r.below(2) << 1)) : r.u32();
            float xa = 1, ya = 2, za = 3, xe = 1, ye = 2, ze = 3;
            reinterpret_cast<CMatrix*>(ma)->ConvertToEulerAngles(&xa, &ya, &za, flags);
            oracle::Fn<void __fastcall(CMatrix*, int, float*, float*, float*, uint32)>(0x59A840)(reinterpret_cast<CMatrix*>(ma), 0, &xe, &ye, &ze, flags);
            if (SameF(xa, xe) && SameF(ya, ye) && SameF(za, ze)) return true;
            d = "flags " + std::to_string(flags) + " m " + Hex(ma, 0x40) + " got " + F(xa) + F(ya) + F(za) + " exe " + F(xe) + F(ye) + F(ze);
            return false;
        });
        Run("CMatrix::ConvertFromEulerAngles 0x59AA40", [=](Rng& r, std::string& d) {
            alignas(16) uint8 ma[0x48], me[0x48]; genM(r, ma); std::memcpy(me, ma, sizeof ma);
            const float x = GenF(r, 4.f, S_NOHUGE), y = GenF(r, 4.f, S_NOHUGE), z = GenF(r, 7.f, S_NOHUGE);
            const uint32 flags = (uint32)(r.below(2) | (r.below(6) << 2) | (r.below(2) << 1));
            reinterpret_cast<CMatrix*>(ma)->ConvertFromEulerAngles(x, y, z, flags);
            oracle::Fn<void __fastcall(CMatrix*, int, float, float, float, uint32)>(0x59AA40)(reinterpret_cast<CMatrix*>(me), 0, x, y, z, flags);
            if (SameBlob(ma, me, 0x40)) return true;
            d = "angles " + F(x) + F(y) + F(z) + " flags " + std::to_string(flags) + " got " + Hex(ma, 0x40) + " exe " + Hex(me, 0x40);
            return false;
        });
    }
    // ---- vehicle recordings (replay paths)
    {
        auto genFrame = [](Rng& r, CVehicleStateEachFrame& f) {
            auto* b = reinterpret_cast<uint8_t*>(&f);
            for (size_t i = 0; i < sizeof f; ++i) b[i] = (uint8_t)r.u32();
            const CVector p = GenV(r, 500.f); std::memcpy(b + 0x14, &p, 12);   // m_vecPosn
            b[0x13] = (uint8_t)r.below(2);                                      // m_bHandbrakeUsed
            if (r.below(2)) { for (int i = 0; i < 6; ++i) b[10 + i] = (uint8_t)(int8_t)(int)(r.below(255) - 127); }
        };
        Run("CVehicleRecording::RestoreInfoForMatrix 0x459960", [=](Rng& r, std::string& d) {
            CVehicleStateEachFrame f; genFrame(r, f);
            alignas(16) uint8 ma[0x48] {}, me[0x48] {}; GenMatrix(r, ma, 0.f); std::memcpy(me, ma, sizeof ma);
            CVehicleRecording::RestoreInfoForMatrix(*reinterpret_cast<CMatrix*>(ma), f);
            oracle::Fn<void __cdecl(CMatrix*, const CVehicleStateEachFrame*)>(0x459960)(reinterpret_cast<CMatrix*>(me), &f);
            if (SameBlob(ma, me, 0x40)) return true;
            d = "frame " + Hex(&f, 0x20) + " got " + Hex(ma, 0x40) + " exe " + Hex(me, 0x40);
            return false;
        });
        Run("CVehicleRecording::RestoreInfoForCar 0x459A30", [=](Rng& r, std::string& d) {
            CVehicleStateEachFrame f; genFrame(r, f);
            PedFx a; BuildPedFx(r, a, 50.f); a.Rewire(true); PedFx e(a);
            const bool pause = r.below(3) == 0;
            CVehicleRecording::RestoreInfoForCar(reinterpret_cast<CVehicle*>(a.e), f, pause);
            oracle::Fn<void __cdecl(CVehicle*, const CVehicleStateEachFrame*, bool)>(0x459A30)(reinterpret_cast<CVehicle*>(e.e), &f, pause);
            if (SamePedFx(a, e)) return true;
            d = "frame " + Hex(&f, 0x20) + " pause " + std::to_string(pause) + PedDiff(a, e);
            return false;
        });
        Run("CVehicleRecording::InterpolateInfoForCar 0x459B30", [=](Rng& r, std::string& d) {
            CVehicleStateEachFrame f; genFrame(r, f);
            PedFx a; BuildPedFx(r, a, 50.f); a.Rewire(true); PedFx e(a);
            const float t = r.below(4) ? r.f01() : GenF(r, 2.f, S_NOHUGE);
            CVehicleRecording::InterpolateInfoForCar(reinterpret_cast<CVehicle*>(a.e), f, t);
            oracle::Fn<void __cdecl(CVehicle*, const CVehicleStateEachFrame*, float)>(0x459B30)(reinterpret_cast<CVehicle*>(e.e), &f, t);
            for (int off : { 0x0C, 0x1C, 0x2C, 0x3C }) { *reinterpret_cast<uint32_t*>(a.m + off) = 0; *reinterpret_cast<uint32_t*>(e.m + off) = 0; }   // matrix padding is stack garbage on both sides
            if (SamePedFx(a, e)) return true;
            d = "frame " + Hex(&f, 0x20) + " t " + F(t) + PedDiff(a, e);
            return false;
        });
        Run("CVehicleRecording::SetRecordingToPointClosestToCoors 0x45A1E0", [=](Rng& r, std::string& d) {
            const int id = r.below(16);
            constexpr int N = 10;
            static CVehicleStateEachFrame fr[N];
            const int n = 1 + r.below(N);
            for (int i = 0; i < n; ++i) genFrame(r, fr[i]);
            CVehicleRecording::pPlaybackBuffer[id] = fr; CVehicleRecording::PlaybackBufferSize[id] = n * (int)sizeof(CVehicleStateEachFrame);
            const int32 start = (int32)r.below(50);
            const CVector p = GenV(r, 600.f);
            CVehicleRecording::PlaybackIndex[id] = start;
            CVehicleRecording::SetRecordingToPointClosestToCoors(id, p);
            const int32 ra = CVehicleRecording::PlaybackIndex[id];
            CVehicleRecording::PlaybackIndex[id] = start;
            oracle::Fn<void __cdecl(int, CVector)>(0x45A1E0)(id, p);
            const int32 re = CVehicleRecording::PlaybackIndex[id];
            if (ra == re) return true;
            d = "n " + std::to_string(n) + " p " + V(p) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
            return false;
        });
        Run("CVehicleRecording::SmoothRecording 0x45A0F0", [=](Rng& r, std::string& d) {
            const int id = r.below(8);
            constexpr int N = 12;
            static CVehicleStateEachFrame fa[N], fe[N];
            const int n = 1 + r.below(N);   // n = 0 would wrap the unsigned size - 0x20 test (in the exe, too)
            for (int i = 0; i < n; ++i) { genFrame(r, fa[i]); fa[i].m_nTime = r.below(3) ? r.below(100000) : r.u32(); fe[i] = fa[i]; }
            auto& slot = CVehicleRecording::StreamingArray[id];
            slot.m_pData = fa; slot.m_nSize = n * (int)sizeof(CVehicleStateEachFrame);
            CVehicleRecording::SmoothRecording(id);
            slot.m_pData = fe;
            oracle::Fn<void __cdecl(int)>(0x45A0F0)(id);
            if (SameBlob(fa, fe, n * sizeof(CVehicleStateEachFrame))) return true;
            d = "n " + std::to_string(n) + DiffDesc(fa, fe, n * sizeof(CVehicleStateEachFrame), "frm");
            return false;
        });
    }
    // ---- compressed matrix
    Run("CCompressedMatrixNotAligned::DecompressIntoFullMatrix 0x59B9F0", [](Rng& r, std::string& d) {
        CCompressedMatrixNotAligned cm{};
        std::memset(&cm, 0, sizeof cm);
        for (int i = 0; i < 3; ++i) reinterpret_cast<float*>(&cm)[i] = GenF(r, 100.f);
        for (int i = 0; i < 6; ++i) reinterpret_cast<int8_t*>(&cm)[12 + i] = (int8_t)(r.below(3) ? (int)r.below(255) - 127 : (int)r.u32());
        alignas(16) uint8 ma[0x48] {}, me[0x48] {};
        CMatrix& A = *reinterpret_cast<CMatrix*>(ma);
        CPedGeometryAnalyser* unused = nullptr; (void)unused;
        cm.DecompressIntoFullMatrix(A);
        oracle::Fn<void __fastcall(CCompressedMatrixNotAligned*, int, CMatrix*)>(0x59B9F0)(&cm, 0, reinterpret_cast<CMatrix*>(me));
        if (SameBlob(ma, me, 0x40)) return true;
        d = "cm " + Hex(&cm, 18) + " got " + Hex(ma, 0x40) + " exe " + Hex(me, 0x40);
        return false;
    });
    // ---- fires (gFireManager lives in the exe's .bss; both implementations read it)
    {
        struct FS { Snap in{ 0, 0 }, pa{ 0, 0 }, pe{ 0, 0 }; };
        auto fillFires = [](Rng& r) {
            auto& fm = gFireManager;
            auto* bytes = reinterpret_cast<uint8_t*>(&fm);
            for (size_t i = 0; i < sizeof fm; ++i) bytes[i] = (uint8_t)r.u32();
            for (auto& f : fm.m_aFires) {
                auto* fb = reinterpret_cast<uint8_t*>(&f);
                const CVector pos = GenV(r, 100.f); std::memcpy(fb + 4, &pos, 12);   // m_Position
                std::memset(fb + 0x10, 0, 8); std::memset(fb + 0x24, 0, 4);          // m_EntityOnFire, m_EntityStartedFire, m_FxSystem
            }
        };
        Run("CFireManager::FindNearestFire 0x538F40", [=](Rng& r, std::string& d) {
            fillFires(r);
            const CVector p = GenV(r, 100.f); const bool c1 = r.below(2), c2 = r.below(2);
            CFire* ra = gFireManager.FindNearestFire(p, c1, c2);
            CFire* re = oracle::Fn<CFire* __fastcall(CFireManager*, int, const CVector*, bool, bool)>(0x538F40)(&gFireManager, 0, &p, c1, c2);
            Hit(ra != nullptr);
            if (ra == re) return true;
            d = "p " + V(p) + " flags " + std::to_string(c1) + std::to_string(c2) + " got " + std::to_string((int)(ra ? ra - gFireManager.m_aFires.data() : -1)) + " exe " + std::to_string((int)(re ? re - gFireManager.m_aFires.data() : -1));
            return false;
        });
        Run("CFireManager::GetNumFiresInRange 0x5397F0", [=](Rng& r, std::string& d) {
            fillFires(r);
            const CVector p = GenV(r, 100.f); const float rad = r.below(4) ? r.f01() * 80.f : GenF(r, 200.f);
            const uint32 ra = gFireManager.GetNumFiresInRange(p, rad);
            const uint32 re = oracle::Fn<uint32 __fastcall(CFireManager*, int, const CVector*, float)>(0x5397F0)(&gFireManager, 0, &p, rad);
            Hit(ra != 0);
            if (ra == re) return true;
            d = "p " + V(p) + " r " + F(rad) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
            return false;
        });
        Run("CFireManager::GetNumFiresInArea 0x539860", [=](Rng& r, std::string& d) {
            fillFires(r);
            const float v[6] = { GenF(r, 120.f), GenF(r, 120.f), GenF(r, 120.f), GenF(r, 120.f), GenF(r, 120.f), GenF(r, 120.f) };
            const uint32 ra = gFireManager.GetNumFiresInArea(v[0], v[1], v[2], v[3], v[4], v[5]);
            const uint32 re = oracle::Fn<uint32 __fastcall(CFireManager*, int, float, float, float, float, float, float)>(0x539860)(&gFireManager, 0, v[0], v[1], v[2], v[3], v[4], v[5]);
            Hit(ra != 0);
            if (ra == re) return true;
            d = "box " + F(v[0]) + F(v[1]) + F(v[2]) + F(v[3]) + F(v[4]) + F(v[5]) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
            return false;
        });
    }
}
