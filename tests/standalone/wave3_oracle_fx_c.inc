// ORACLE-W3-EFFECTS rows C: Glass, Birds, Skidmarks, ProjectileInfo, WaterCannon(s), Ropes / Rope (see wave3_oracle_fx.inc).
// Audio callees are recorded by stubs on both sides: the exe addresses are oracle::Patch()ed to host stubs, the port methods (their translation units are not linked) are defined here with the same body;
// the call log lives in the scratch block and is part of the compared memory.
#include "Audio/AudioEngine.h"
#include "Audio/Entities/AEWaterCannonAudioEntity.h"
namespace w3fc {
using namespace w3f; using namespace w2;
constexpr size_t oRec = 0x1F000;   // u32 count, then 16 byte entries {tag, a, b, c}
static void Rec(uint32_t tag, uint32_t a, uint32_t b, uint32_t c) {
    uint32_t& n = SAt<uint32_t>(oRec);
    if (n < 64) { uint32_t* e = reinterpret_cast<uint32_t*>(S(oRec + 4 + n * 16)); e[0] = tag; e[1] = a; e[2] = b; e[3] = c; }
    ++n;
}
static uint32_t FBits(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
// CAudioEngine::ReportGlassCollisionEvent(this, eAudioEvents, const CVector&) 0x506EE0 (thiscall, 2 stack args)
static void __fastcall X_Glass(void* self, int, int ev, const CVector* pos) { Rec(1, (uint32_t)(uintptr_t)self, (uint32_t)ev, FBits(pos->x) ^ FBits(pos->y) * 3u ^ FBits(pos->z) * 7u); }
// CAEWaterCannonAudioEntity::Initialise(this, CWaterCannon*) 0x503060
static void __fastcall X_WcInit(void* self, int, void* wc) { Rec(2, (uint32_t)(uintptr_t)self, (uint32_t)(uintptr_t)wc, 0); }
static void InstallStubs() { static bool done = false; if (done) return; done = true; oracle::Patch(0x506EE0, (void*)&X_Glass); oracle::Patch(0x503060, (void*)&X_WcInit); }
static void NoRec() { SAt<uint32_t>(oRec) = 0; }
// rope fixtures: the rope table at the exe address with random types / ids / segment positions; attached objects are blocks in the scratch block
constexpr size_t oEnt = 0x10000;   // 8 fake entities x 0x400
static float GN(Rng& r, float scale) { float v = GenF(r, scale, S_ZERO); if (v != 0.f && std::fabs(v) < 1e-4f) v = 0.5f; if (std::fabs(v) > 1e7f) v = v < 0.f ? -1e7f : 1e7f; return v; }   // game-range magnitudes: beyond ~1e19 the squared distances overflow float but not the x87 stack   // no denormal / tiny values (x87 vs float exponent range)
static void PutRope(Rng& r, CRope& rp, bool nearOrigin) {
    FillGarbage(r, &rp, sizeof(rp));
    for (auto& s : rp.m_aSegments) s = CVector(GN(r, nearOrigin ? 30.f : 2000.f), GN(r, nearOrigin ? 30.f : 2000.f), GN(r, nearOrigin ? 30.f : 100.f));
    for (auto& s : rp.m_aSpeed) s = CVector(GN(r, 1.f), GN(r, 1.f), GN(r, 1.f));
    rp.m_nType = (eRopeType)(r.below(4) == 0 ? 0 : r.below(9));
    rp.m_nId = r.below(3) ? r.below(12) : r.u32();
    rp.m_pRopeAttachObject = r.below(3) ? nullptr : reinterpret_cast<CEntity*>(S(oEnt + 0x400 * r.below(8)));
    rp.m_pAttachedEntity = reinterpret_cast<CEntity*>(S(oEnt + 0x400 * r.below(8)));
}
static void PutRopes(Rng& r) {
    FillGarbage(r, &CRopes::aRopes, sizeof(CRopes::aRopes));
    for (auto& rp : CRopes::aRopes) PutRope(r, rp, false);
    CRopes::PlayerControlsCrane = (eControlledCrane)r.below(6);
}
static void PutCannons(Rng& r) {
    FillGarbage(r, &CWaterCannons::aCannons, sizeof(CWaterCannons::aCannons));
    for (auto& c : CWaterCannons::aCannons) {
        c.m_nId = r.below(3) == 0 ? 0 : (r.below(2) ? r.below(8) : r.u32());
        c.m_nSectionsCount = (int32)(r.below(5) == 0 ? r.u32() : r.below(32));
        for (auto& u : c.m_abUsed) u = r.below(2);
    }
    CTimer::m_snTimeInMilliseconds = r.below(4) ? r.below(100000) : r.u32();
}
} // namespace w3fc
void CAudioEngine::ReportGlassCollisionEvent(eAudioEvents ev, Const CVector& pos) { w3fc::Rec(1, 0xB6BC90u, (uint32_t)ev, w3fc::FBits(pos.x) ^ w3fc::FBits(pos.y) * 3u ^ w3fc::FBits(pos.z) * 7u); }
void CAEWaterCannonAudioEntity::Initialise(CWaterCannon* wc) { w3fc::Rec(2, (uint32_t)(uintptr_t)this, (uint32_t)(uintptr_t)wc, 0); }

static void TestW3FxC() {
    using namespace w2; using namespace w3f; using namespace w3fc;
    InstallStubs();
    // ---- Glass ----
    ROW("CGlass::Init", 0x71A8D0, 200) {
        Tu tu([](Rng& rr) { NoRec(); });
        return UCase(r, d, [&]() -> uint64_t { CGlass::Init(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x71A8D0)(); return 0; });
    } ENDROW;
    ROW("CGlass::FindFreePane", 0x71ACA0, 600) {
        Tu tu([](Rng& rr) { FillGarbage(rr, &CGlass::aGlassPanes, sizeof(CGlass::aGlassPanes)); const int nFree = (int)rr.below(50); for (int i = 0; i < 45; ++i) CGlass::aGlassPanes[i].m_bExist = i < nFree ? (rr.below(8) == 0) : true; });
        return UCase(r, d, [&]() -> uint64_t { return U(CGlass::FindFreePane()); }, [&]() -> uint64_t { return U(oracle::Fn<CFallingGlassPane* __cdecl()>(0x71ACA0)()); });
    } ENDROW;
    ROW("CGlass::AskForObjectToBeRenderedInGlass", 0x71ACD0, 600) {
        auto* e = reinterpret_cast<CEntity*>(S(0x1000 + 0x400 * r.below(4)));
        Tu tu([](Rng& rr) { FillGarbage(rr, &CGlass::apEntitiesToBeRendered, sizeof(CGlass::apEntitiesToBeRendered)); CGlass::NumGlassEntities = rr.below(10) == 0 ? rr.u32() : rr.below(36); });
        return UCase(r, d, [&]() -> uint64_t { CGlass::AskForObjectToBeRenderedInGlass(e); return CGlass::NumGlassEntities; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(CEntity*)>(0x71ACD0)(e); return CGlass::NumGlassEntities; });
    } ENDROW;
    ROW("CGlass::CalcAlphaWithNormal", 0x71ACF0, 3000) {
        const float sc = r.below(4) ? 1.f : 3.f;
        CVector n(Fl(r, sc, 12), Fl(r, sc, 12), Fl(r, sc, 12)); if (r.below(2)) { const float m = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z); if (m > 1e-6f && std::isfinite(m)) { n.x /= m; n.y /= m; n.z /= m; } }
        std::memcpy(S(0x100), &n, 12);
        Tu tu([](Rng& rr) { TheCamera.m_matrix = reinterpret_cast<CMatrixLink*>(&TheCamera.m_mCameraMatrix); for (int i = 0; i < 16; ++i) reinterpret_cast<float*>(&TheCamera.m_mCameraMatrix)[i] = rr.below(30) == 0 ? GenF(rr, 2.f, S_NAN | S_ZERO | S_INF) : GF(rr, 1.5f);
            if (rr.below(2)) { CVector f(GF(rr, 1.f), GF(rr, 1.f), GF(rr, 1.f)); const float m = std::sqrt(f.x * f.x + f.y * f.y + f.z * f.z); if (m > 1e-3f) { f.x /= m; f.y /= m; f.z /= m; } TheCamera.m_mCameraMatrix.GetForward() = f; } });
        return UCase(r, d, [&]() -> uint64_t { return CGlass::CalcAlphaWithNormal(SAt<CVector>(0x100)); }, [&]() -> uint64_t { return oracle::Fn<uint8 __cdecl(const CVector*)>(0x71ACF0)(&SAt<CVector>(0x100)) & 0xFF; });
    } ENDROW;
    ROW("CGlass::WindowRespondsToSoftCollision", 0x71AF70, 1000) {
        auto* e = reinterpret_cast<CEntity*>(S(0x1000 + 0x400 * r.below(4))); const float dmg = r.below(5) ? (float)r.below(120) : Fl(r, 200.f, 4);
        Tu tu([](Rng& rr) { NoRec(); });
        return UCase(r, d, [&]() -> uint64_t { CGlass::WindowRespondsToSoftCollision(e, dmg); return SAt<uint32_t>(oRec); }, [&]() -> uint64_t { oracle::Fn<void __cdecl(CEntity*, float)>(0x71AF70)(e, dmg); return SAt<uint32_t>(oRec); });
    } ENDROW;
    // ---- Birds / Skidmarks / ProjectileInfo ----
    ROW("CBirds::Init", 0x711EC0, 100) {
        Tu tu([](Rng& rr) { FillGarbage(rr, &CBirds::aBirds, sizeof(CBirds::aBirds)); CBirds::uiNumberOfBirds = rr.u32(); CBirds::bHasBirdBeenShot = rr.below(2); });
        return UCase(r, d, [&]() -> uint64_t { CBirds::Init(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x711EC0)(); return 0; });
    } ENDROW;
    ROW("CBirds::Shutdown", 0x712300, 100) {
        Tu tu([](Rng& rr) { FillGarbage(rr, &CBirds::aBirds, sizeof(CBirds::aBirds)); CBirds::uiNumberOfBirds = rr.u32(); CBirds::bHasBirdBeenShot = rr.below(2); });
        return UCase(r, d, [&]() -> uint64_t { CBirds::Shutdown(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x712300)(); return 0; });
    } ENDROW;
    ROW("CSkidmarks::Clear", 0x720590, 100) {
        Tu tu([](Rng& rr) { FillGarbage(rr, &CSkidmarks::m_aSkidmarks, sizeof(CSkidmarks::m_aSkidmarks)); });
        return UCase(r, d, [&]() -> uint64_t { CSkidmarks::Clear(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x720590)(); return 0; });
    } ENDROW;
    ROW("CProjectileInfo::GetProjectileInfo", 0x737BF0, 200) {
        const int id = (int)r.below(MAX_PROJECTILE_INFOS);
        return UCase(r, d, [&]() -> uint64_t { return U(CProjectileInfo::GetProjectileInfo(id)); }, [&]() -> uint64_t { return U(oracle::Fn<CProjectileInfo* __cdecl(int)>(0x737BF0)(id)); });
    } ENDROW;
    // ---- WaterCannon(s) ----
    ROW("CWaterCannon::Init", 0x728B40, 400) {
        const int k = (int)r.below(3);
        Tu tu([](Rng& rr) { PutCannons(rr); NoRec(); });
        return UCase(r, d, [&]() -> uint64_t { CWaterCannons::aCannons[k].Init(); return SAt<uint32_t>(oRec); }, [&]() -> uint64_t { oracle::Fn<void __fastcall(CWaterCannon*, int)>(0x728B40)(&CWaterCannons::aCannons[k], 0); return SAt<uint32_t>(oRec); });
    } ENDROW;
    ROW("CWaterCannon::Update_NewInput", 0x728C20, 600) {
        const int k = (int)r.below(3); CVector a(GF(r, 2000.f), GF(r, 2000.f), GF(r, 100.f)), b(Fl(r, 5.f), Fl(r, 5.f), Fl(r, 5.f));
        std::memcpy(S(0x200), &a, 12); std::memcpy(S(0x210), &b, 12);
        Tu tu([](Rng& rr) { PutCannons(rr); });
        return UCase(r, d, [&]() -> uint64_t { CWaterCannons::aCannons[k].Update_NewInput(&SAt<CVector>(0x200), &SAt<CVector>(0x210)); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(CWaterCannon*, int, const CVector*, const CVector*)>(0x728C20)(&CWaterCannons::aCannons[k], 0, &SAt<CVector>(0x200), &SAt<CVector>(0x210)); return 0; });
    } ENDROW;
    ROW("CWaterCannons::Init", 0x728C80, 200) {
        Tu tu([](Rng& rr) { PutCannons(rr); NoRec(); });
        return UCase(r, d, [&]() -> uint64_t { CWaterCannons::Init(); return SAt<uint32_t>(oRec); }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x728C80)(); return SAt<uint32_t>(oRec); });
    } ENDROW;
    ROW("CWaterCannons::UpdateOne", 0x728CB0, 2000) {
        uint32_t veh = r.below(3) ? r.below(8) : r.u32(); CVector a(GF(r, 2000.f), GF(r, 2000.f), GF(r, 100.f)), b(Fl(r, 5.f), Fl(r, 5.f), Fl(r, 5.f));
        std::memcpy(S(0x200), &a, 12); std::memcpy(S(0x210), &b, 12);
        Tu tu([&veh](Rng& rr) { PutCannons(rr); NoRec(); if (rr.below(2)) CWaterCannons::aCannons[rr.below(3)].m_nId = veh; });
        return UCase(r, d, [&]() -> uint64_t { CWaterCannons::UpdateOne(veh, &SAt<CVector>(0x200), &SAt<CVector>(0x210)); return SAt<uint32_t>(oRec); },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(uint32_t, CVector*, CVector*)>(0x728CB0)(veh, &SAt<CVector>(0x200), &SAt<CVector>(0x210)); return SAt<uint32_t>(oRec); });
    } ENDROW;
    // ---- Ropes / Rope ----
    ROW("CRopes::Init", 0x555DC0, 100) {
        Tu tu([](Rng& rr) { PutRopes(rr); });
        return UCase(r, d, [&]() -> uint64_t { CRopes::Init(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x555DC0)(); return 0; });
    } ENDROW;
    ROW("CRopes::FindRope", 0x556000, 1500) {
        Tu tu([](Rng& rr) { PutRopes(rr); });
        uint32_t id = r.below(4) ? r.below(12) : r.u32();
        return UCase(r, d, [&]() -> uint64_t { return (uint32_t)CRopes::FindRope(id); }, [&]() -> uint64_t { return (uint32_t)oracle::Fn<int __cdecl(uint32_t)>(0x556000)(id); });
    } ENDROW;
    ROW("CRopes::IsCarriedByRope", 0x555F80, 1500) {
        CPhysical* ph = nullptr; const unsigned k = r.below(10);
        Tu tu([&](Rng& rr) { PutRopes(rr); });
        const int pick = (int)r.below(4); const int which = (int)r.below(8);
        return UCase(r, d, [&]() -> uint64_t { ph = pick == 0 ? nullptr : (pick == 1 ? reinterpret_cast<CPhysical*>(CRopes::aRopes[which].m_pRopeAttachObject) : reinterpret_cast<CPhysical*>(S(oEnt + 0x400 * ((which + k) & 7)))); return B8(CRopes::IsCarriedByRope(ph)); },
            [&]() -> uint64_t { return oracle::Fn<bool __cdecl(CPhysical*)>(0x555F80)(ph) ? 1 : 0; });
    } ENDROW;
    ROW("CRopes::SetSpeedOfTopNode", 0x555DF0, 1500) {
        Tu tu([](Rng& rr) { PutRopes(rr); });
        uint32_t id = r.below(4) ? r.below(12) : r.u32(); const CVector sp(Fl(r, 10.f), Fl(r, 10.f), Fl(r, 10.f));
        return UCase(r, d, [&]() -> uint64_t { CRopes::SetSpeedOfTopNode(id, sp); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(uint32_t, float, float, float)>(0x555DF0)(id, sp.x, sp.y, sp.z); return 0; });
    } ENDROW;
    ROW("CRopes::FindCoorsAlongRope", 0x555E40, 3000) {
        Tu tu([](Rng& rr) { PutRopes(rr); });
        uint32_t id = r.below(4) ? r.below(12) : r.u32(); const float dist = r.below(6) == 0 ? Fl(r, 3.f, 2) : r.f01() * 1.2f - 0.1f; const bool wantSpeed = r.below(3) != 0;
        return UCase(r, d, [&]() -> uint64_t { return B8(CRopes::FindCoorsAlongRope(id, dist, &SAt<CVector>(0x300), wantSpeed ? &SAt<CVector>(0x310) : nullptr)); },
            [&]() -> uint64_t { return oracle::Fn<bool __cdecl(uint32_t, float, CVector*, CVector*)>(0x555E40)(id, dist, &SAt<CVector>(0x300), wantSpeed ? &SAt<CVector>(0x310) : nullptr) ? 1 : 0; });
    } ENDROW;
    ROW("CRopes::FindPickupHeight", 0x556760, 600) {
        const int mi = (int)r.below(20000);
        Tu tu([mi](Rng& rr) { SAt<int16_t>(oEnt + 0x22) = (int16_t)mi; CModelInfo::ms_modelInfoPtrs[mi] = reinterpret_cast<CBaseModelInfo*>(S(0x14000)); *reinterpret_cast<CColModel**>(S(0x14000 + 20)) = reinterpret_cast<CColModel*>(S(0x14100)); });
        return UCase(r, d, [&]() -> uint64_t { return FB(CRopes::FindPickupHeight(reinterpret_cast<CEntity*>(S(oEnt)))); }, [&]() -> uint64_t { return FB(oracle::Fn<float __cdecl(CEntity*)>(0x556760)(reinterpret_cast<CEntity*>(S(oEnt)))); });
    } ENDROW;
    ROW("CRope::ReleasePickedUpObject", 0x556030, 1500) {
        Tu tu([](Rng& rr) { PutRope(rr, SAt<CRope>(0x4000), true); });
        return UCase(r, d, [&]() -> uint64_t { SAt<CRope>(0x4000).ReleasePickedUpObject(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(CRope*, int)>(0x556030)(&SAt<CRope>(0x4000), 0); return 0; });
    } ENDROW;
    ROW("CRope::UpdateWeightInRope", 0x5561B0, 4000) {
        const int fixedHow = (int)r.below(4);
        Tu tu([fixedHow](Rng& rr) {
            auto& rp = SAt<CRope>(0x4000); PutRope(rr, rp, true);
            rp.m_nSegments = (uint8_t)(rr.below(12) == 0 ? rr.u32() & 31 : rr.below(32)); rp.m_fTotalLength = rr.below(10) == 0 ? FlN(rr, 3.f, 3) : 0.1f + rr.f01() * 3.f;
            CTimer::ms_fTimeStep = rr.below(10) == 0 ? FlN(rr, 3.f, 3) : 0.2f + rr.f01() * 2.f;
            (void)fixedHow; });
        // the target: near the anchor so that both the stretched and the slack branch happen
        float x, y, z; { const float sp = (r.below(2) ? 3.f : 40.f); x = GN(r, sp); y = GN(r, sp); z = GN(r, sp); if (r.below(25) == 0) { x = FlN(r, 40.f, 2); y = FlN(r, 40.f, 2); z = FlN(r, 40.f, 2); } }
        const int a5 = (int)r.u32();
        return UCase(r, d, [&]() -> uint64_t { return B8(SAt<CRope>(0x4000).UpdateWeightInRope(x, y, z, a5, reinterpret_cast<float*>(S(0x5000)))); },
            [&]() -> uint64_t { return oracle::Fn<bool __fastcall(CRope*, int, float, float, float, int, float*)>(0x5561B0)(&SAt<CRope>(0x4000), 0, x, y, z, a5, reinterpret_cast<float*>(S(0x5000))) ? 1 : 0; });
    } ENDROW;
}
