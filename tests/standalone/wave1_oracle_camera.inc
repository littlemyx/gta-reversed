// ORACLE-W1 / camera: CCamera methods that only touch TheCamera (0xB6F028, 0xD78 bytes) and a few statics (fade, timer, collision flags, game logic), plus CamShakeNoPos.
// Per case TheCamera is filled with structured garbage (random dwords; the two CCam modes / looking directions, fade alpha / flag, active cam index are drawn from the interesting values),
// then both implementations run on the same object and the whole camera object + the touched statics are compared. Functions that would dereference entity pointers use zeroed pointers.
#include "Draw.h"
#include "GameLogic.h"
#include "wave1_oracle_bools.inc"

namespace w1cam {
constexpr uintptr_t kCam = 0xB6F028;
static_assert(sizeof(CCamera) == 0xD78);
constexpr Rg kCamRg { kCam, sizeof(CCamera) };
constexpr Rg kDraw  { 0xC3EFA8, 8 };            // CDraw::FadeRed..FadeValue and neighbours
constexpr Rg kTimer { 0xB7CB28, 0x68 };         // CTimer statics (ms_fTimeStep, time in ms, ...)
constexpr Rg kColl  { 0x8A5B14, 12 };           // CCollision::bCamCollideWith* / relVelCamCollisionVehiclesSqr
constexpr Rg kColVars { 0x8CCB80, 8 };          // gCurCamColVars, gCurDistForCam
constexpr Rg kColVarsP { 0xB6FE88, 4 };         // gpCamColVars
constexpr Rg kLogic { 0x8A5E50, 4 };            // CGameLogic::n2PlayerPedInFocus
constexpr Rg kCine  { 0xB6EC2C, 4 };            // bDidWeProcessAnyCinemaCam (+ neighbours)

inline void Setup(Rng& r) {
    FillGarbage(r, &TheCamera, sizeof(CCamera));
    FixBools(r, &TheCamera, kBools_CCamera);
    for (auto& c : TheCamera.m_aCams) FixBools(r, &c, kBools_CCam);
    FillGarbage(r, kDraw.va, kDraw.n); FillGarbage(r, kColl.va, kColl.n); FillGarbage(r, kColVars.va, kColVars.n); FillGarbage(r, kLogic.va, kLogic.n); FillGarbage(r, kCine.va, kCine.n);
    FillGarbage(r, kTimer.va, kTimer.n);
    CTimer::ms_fTimeStep = r.below(8) ? r.f01() * (r.below(4) ? 4.f : 50.f) : 0.f;   // never denormal: the port returns the step as a float, the exe keeps the product on the x87 stack (denormals are not game states)
    CTimer::m_snTimeInMilliseconds = r.below(5) ? r.u32() % 4000000u : (r.below(2) ? 0x7FFFF000u + r.below(0x2000) : r.u32());   // incl. values at / above 2^31 (signed read of the ms timer)
    TheCamera.m_nActiveCam = (uint8)r.below(2);
    static const int modes[] = { 4, 7, 8, 15, 16, 17, 18, 22, 34, 45, 46, 51, 65, 0, 3, 10 };
    for (auto& c : TheCamera.m_aCams) {
        c.m_nMode = (eCamMode)(r.below(5) ? modes[r.below(16)] : (int)r.u32());
        c.m_nDirectionWasLooking = r.below(5) ? r.below(4) : r.u32();
        c.m_pCamTargetEntity = nullptr;
    }
    TheCamera.m_nExtraEntitiesCount = r.below(8) ? r.below(2) : (int)r.u32() % 5 - 1;   // m_pExtraEntity[2]: writes at [count] stay inside TheCamera for 0..2
    if (TheCamera.m_nExtraEntitiesCount > 2) TheCamera.m_nExtraEntitiesCount = 2;
    for (auto& e : TheCamera.m_pExtraEntity) e = r.below(3) ? nullptr : reinterpret_cast<CEntity*>(0x1000u + r.below(4) * 0x10);
    switch (r.below(4)) { case 0: TheCamera.m_fFadeAlpha = 0.f; break; case 1: TheCamera.m_fFadeAlpha = 255.f; break; default: break; }
    TheCamera.m_nFadeInOutFlag = (eFadeFlag)r.below(2);            // eFadeFlag has two values; the port treats "not FADE_OUT" as FADE_IN, the exe checks both
    TheCamera.m_nMusicFadingDirection = (eFadeFlag)r.below(2);
    if (r.below(2)) TheCamera.m_fFadeDuration = 0.f;
    if (r.below(2)) TheCamera.m_fTimeToWaitToFadeMusic = r.below(2) ? 0.f : r.f01() * 2.f - 0.5f;
    if (r.below(3) == 0) TheCamera.m_fTimeToFadeMusic = 0.f;
    gCurCamColVars = (uint8)r.below(28);
    *reinterpret_cast<float*>(0x8CCB84) = r.f01() * 3.f;   // gCurDistForCam
    gpCamColVars = &gCamColVars[r.below(28)][0];
}
}

namespace w1cam { inline std::string Ctx() { return "fading " + std::to_string(TheCamera.m_bFading) + " flag " + std::to_string((int)TheCamera.m_nFadeInOutFlag) + " alpha " + F(TheCamera.m_fFadeAlpha) + " dur " + F(TheCamera.m_fFadeDuration) + " ts " + F(CTimer::ms_fTimeStep); } }
#define W1_CAM_RGS w1cam::kCamRg, w1cam::kDraw, w1cam::kTimer, w1cam::kColl, w1cam::kColVars, w1cam::kColVarsP, w1cam::kLogic, w1cam::kCine
#define W1_CAM_VOID(name, va) Run("CCamera::" #name " " #va, [](Rng& r, std::string& d) { w1cam::Setup(r); \
    return Both(d, { W1_CAM_RGS }, [&] { TheCamera.name(); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int)>(va)(&TheCamera, 0); return 0ull; }, w1cam::Ctx()); });
#define W1_CAM_RET(name, va, RT, wrap, FL) Run("CCamera::" #name " " #va, [](Rng& r, std::string& d) { w1cam::Setup(r); \
    return Both<FL>(d, { W1_CAM_RGS }, [&] { return wrap(TheCamera.name()); }, [&] { return wrap(oracle::Fn<RT __fastcall(CCamera*, int)>(va)(&TheCamera, 0)); }); });

static void TestCamera() {
    // ---- zero-argument queries
    W1_CAM_RET(FindCamFOV, 0x50AD20, float, RF, true)
    W1_CAM_RET(GetLookDirection, 0x50AE90, int, RI, false)
    W1_CAM_RET(GetLookingForwardFirstPerson, 0x50AED0, bool, RB, false)
    W1_CAM_RET(GetLookingLRBFirstPerson, 0x50AE60, bool, RB, false)
    W1_CAM_RET(GetScreenFadeStatus, 0x50AE20, int, RI, false)
    W1_CAM_RET(GetFadingDirection, 0x50ADF0, int, RI, false)
    W1_CAM_RET(GetFading, 0x50ADE0, bool, RB, false)
    W1_CAM_RET(GetCutSceneFinishTime, 0x50AD90, uint32, RI, false)
    W1_CAM_RET(GetPositionAlongSpline, 0x50AF80, float, RF, true)
    W1_CAM_RET(Get_Just_Switched_Status, 0x50AE10, bool, RB, false)
    W1_CAM_RET(Using1stPersonWeaponMode, 0x50BFF0, bool, RB, false)
    // GetGameCamPosition returns a pointer into TheCamera
    Run("CCamera::GetGameCamPosition 0x50AE50", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        return Both(d, { W1_CAM_RGS }, [&] { return (uint64_t)(uintptr_t)TheCamera.GetGameCamPosition(); }, [&] { return (uint64_t)(uintptr_t)oracle::Fn<CVector* __fastcall(CCamera*, int)>(0x50AE50)(&TheCamera, 0); });
    });
    // ---- zero-argument state changes
    W1_CAM_VOID(ClearPlayerWeaponMode, 0x50AB10)
    W1_CAM_VOID(Enable1rstPersonCamCntrlsScript, 0x50AC00)
    W1_CAM_VOID(Enable1rstPersonWeaponsCamera, 0x50AC10)
    W1_CAM_VOID(InitialiseScriptableComponents, 0x50D2D0)
    W1_CAM_VOID(ProcessWideScreenOn, 0x50B890)
    W1_CAM_VOID(SetWideScreenOff, 0x50C150)
    W1_CAM_VOID(SetWideScreenOn, 0x50C140)
    W1_CAM_VOID(StartCooperativeCamMode, 0x50C260)
    W1_CAM_VOID(StopCooperativeCamMode, 0x50C270)
    W1_CAM_VOID(ProcessFade, 0x50B5D0)
    W1_CAM_VOID(ProcessFOVLerp, 0x516500)
    W1_CAM_VOID(ProcessVectorMoveLinear, 0x5164A0)
    W1_CAM_VOID(ProcessVectorTrackLinear, 0x516440)
    // (the exe's 0x50CD80 takes one stack argument that it ignores - `ret 4`; the port's member has none: harmless with every caller ported)
    Run("CCamera::CameraPedModeSpecialCases 0x50CD80", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.CameraPedModeSpecialCases(); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, void*)>(0x50CD80)(&TheCamera, 0, nullptr); return 0ull; });
    });
    Run("CCamera::DontProcessObbeCinemaCamera 0x50AB40", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        return Both(d, { W1_CAM_RGS }, [&] { CCamera::DontProcessObbeCinemaCamera(); return 0ull; }, [&] { oracle::Fn<void __cdecl()>(0x50AB40)(); return 0ull; });
    });
    // ---- with arguments
    Run("CCamera::IsExtraEntityToIgnore 0x50CE80", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        CEntity* e = reinterpret_cast<CEntity*>(0x1000u + r.below(4) * 0x10);
        return Both(d, { W1_CAM_RGS }, [&] { return RB(TheCamera.IsExtraEntityToIgnore(e)); }, [&] { return RB(oracle::Fn<bool __fastcall(CCamera*, int, CEntity*)>(0x50CE80)(&TheCamera, 0, e)); }, "count " + std::to_string(TheCamera.m_nExtraEntitiesCount));
    });
    Run("CCamera::SetNewPlayerWeaponMode 0x50BFB0", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const int mode = r.u32() & 0xFF; const int a = (int16)r.u32(), b = (int16)r.u32();
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.SetNewPlayerWeaponMode((eCamMode)mode, (int16)a, (int16)b); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, int, int, int)>(0x50BFB0)(&TheCamera, 0, mode, a, b); return 0ull; },
                    "mode " + std::to_string(mode) + " a " + std::to_string(a) + " b " + std::to_string(b));
    });
    Run("CCamera::TakeControlNoEntity 0x50C8B0", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const CVector v = GVf(r, 100.f); const int sw = r.below(3), who = r.below(4);
        if (r.below(2)) TheCamera.m_nWhoIsInControlOfTheCamera = r.below(3);
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.TakeControlNoEntity(v, (eSwitchType)sw, who); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, const CVector*, int, int)>(0x50C8B0)(&TheCamera, 0, &v, sw, who); return 0ull; },
                    "who " + std::to_string(who) + " cur " + std::to_string(TheCamera.m_nWhoIsInControlOfTheCamera));
    });
    Run("CCamera::TakeControlWithSpline 0x50CAE0", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const int sw = r.below(3);
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.TakeControlWithSpline((eSwitchType)sw); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, int)>(0x50CAE0)(&TheCamera, 0, sw); return 0ull; });
    });
    Run("CCamera::UpdateAimingCoors 0x50CB10", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const CVector v = GVf(r, 100.f);
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.UpdateAimingCoors(v); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, const CVector*)>(0x50CB10)(&TheCamera, 0, &v); return 0ull; });
    });
    Run("CCamera::SetCamCutSceneOffSet 0x50BD20", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const CVector v = GVf(r, 100.f);
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.SetCamCutSceneOffSet(v); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, const CVector*)>(0x50BD20)(&TheCamera, 0, &v); return 0ull; });
    });
    Run("CCamera::SetCamPositionForFixedMode 0x50BEC0", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const CVector a = GVf(r, 100.f), b = GVf(r, 10.f);
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.SetCamPositionForFixedMode(a, b); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, const CVector*, const CVector*)>(0x50BEC0)(&TheCamera, 0, &a, &b); return 0ull; });
    });
    Run("CCamera::SetZoomValueFollowPedScript 0x50C160", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const int z = r.below(5) ? r.below(4) : (int16)r.u32();
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.SetZoomValueFollowPedScript((int16)z); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, int)>(0x50C160)(&TheCamera, 0, z); return 0ull; }, "zoom " + std::to_string(z));
    });
    Run("CCamera::VectorMoveLinear 0x50D160", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        CVector to = GVf(r, 100.f), from = GVf(r, 100.f); const float dur = GF(r, 5000.f); const int ease = r.below(2);
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.VectorMoveLinear(&to, &from, dur, ease != 0); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, CVector*, CVector*, float, int)>(0x50D160)(&TheCamera, 0, &to, &from, dur, ease); return 0ull; },
                    "now " + std::to_string(CTimer::m_snTimeInMilliseconds) + " dur " + F(dur));
    });
    Run("CCamera::VectorTrackLinear 0x50D1D0", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        CVector to = GVf(r, 100.f), from = GVf(r, 100.f); const float dur = GF(r, 5000.f); const int ease = r.below(2);
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.VectorTrackLinear(&to, &from, dur, ease != 0); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, CVector*, CVector*, float, int)>(0x50D1D0)(&TheCamera, 0, &to, &from, dur, ease); return 0ull; },
                    "now " + std::to_string(CTimer::m_snTimeInMilliseconds) + " dur " + F(dur));
    });
    Run("CCamera::Fade 0x50AC20", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const float dur = r.below(4) ? r.f01() * 3.f : GF(r, 10.f); const int dir = r.below(2);
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.Fade(dur, (eFadeFlag)dir); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, float, int)>(0x50AC20)(&TheCamera, 0, dur, dir); return 0ull; },
                    "dur " + F(dur) + " dir " + std::to_string(dir) + " ignoreMusic " + std::to_string(TheCamera.m_bIgnoreFadingStuffForMusic));
    });
    Run("CCamera::SetFadeColour 0x50BF00", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const int c[3] = { r.below(3) == 0 ? 2 : (int)(r.u32() & 0xFF), r.below(3) == 0 ? 2 : (int)(r.u32() & 0xFF), r.below(3) == 0 ? 2 : (int)(r.u32() & 0xFF) };
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.SetFadeColour((uint8)c[0], (uint8)c[1], (uint8)c[2]); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, int, int, int)>(0x50BF00)(&TheCamera, 0, c[0], c[1], c[2]); return 0ull; });
    });
    Run("CCamera::LerpFOV 0x50D280", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const float a = GF(r, 100.f), b = GF(r, 100.f), t = GF(r, 5000.f); const int ease = r.below(2);
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.LerpFOV(a, b, t, ease != 0); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, float, float, float, int)>(0x50D280)(&TheCamera, 0, a, b, t, ease); return 0ull; });
    });
    Run("CCamera::SetMotionBlur 0x50BF40", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const int c[3] = { (int)(r.u32() & 0xFF), (int)(r.u32() & 0xFF), (int)(r.u32() & 0xFF) }; const int val = (int)r.u32(), type = r.below(4);
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.SetMotionBlur((uint8)c[0], (uint8)c[1], (uint8)c[2], val, (eMotionBlurType)type); return 0ull; },
                    [&] { oracle::Fn<void __fastcall(CCamera*, int, int, int, int, int, int)>(0x50BF40)(&TheCamera, 0, c[0], c[1], c[2], val, type); return 0ull; });
    });
    Run("CCamera::SetMotionBlurAlpha 0x50BF80", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const int a = (int)r.u32();
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.SetMotionBlurAlpha(a); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, int)>(0x50BF80)(&TheCamera, 0, a); return 0ull; });
    });
    Run("CCamera::AllowShootingWith2PlayersInCar 0x50C280", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const int a = r.below(2);
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.AllowShootingWith2PlayersInCar(a != 0); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, int)>(0x50C280)(&TheCamera, 0, a); return 0ull; });
    });
    Run("CCamera::AddShakeSimple 0x50D240", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const float dur = GF(r, 5000.f), inten = GF(r, 10.f); const int type = (int)r.u32();
        return Both(d, { W1_CAM_RGS }, [&] { TheCamera.AddShakeSimple(dur, type, inten); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, float, int, float)>(0x50D240)(&TheCamera, 0, dur, type, inten); return 0ull; });
    });
    Run("CamShakeNoPos 0x50A970", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const float s = r.below(4) ? r.f01() * 3.f : GF(r, 5.f);
        return Both(d, { W1_CAM_RGS }, [&] { CamShakeNoPos(&TheCamera, s); return 0ull; }, [&] { oracle::Fn<void __cdecl(CCamera*, float)>(0x50A970)(&TheCamera, s); return 0ull; }, "strength " + F(s) + " force " + F(TheCamera.m_fCamShakeForce));
    });
    Run("CCamera::SetCamCollisionVarDataSet 0x50CB60", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const int idx = r.below(28);
        if (r.below(3) == 0) gCurCamColVars = (uint8)idx;
        return Both(d, { W1_CAM_RGS }, [&] { CCamera::SetCamCollisionVarDataSet(idx); return 0ull; }, [&] { oracle::Fn<void __cdecl(int)>(0x50CB60)(idx); return 0ull; }, "idx " + std::to_string(idx));
    });
    Run("CCamera::SetColVarsAimWeapon 0x50CBF0", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const int t = r.below(7) - 1;
        return Both(d, { W1_CAM_RGS }, [&] { CCamera::SetColVarsAimWeapon(t); return 0ull; }, [&] { oracle::Fn<void __cdecl(int)>(0x50CBF0)(t); return 0ull; }, "type " + std::to_string(t));
    });
    Run("CCamera::SetColVarsPed 0x50CC50", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const int ped = r.below(5) ? r.below(3) : r.below(30), zoom = r.below(3);
        return Both(d, { W1_CAM_RGS }, [&] { CCamera::SetColVarsPed((ePedType)ped, zoom); return 0ull; }, [&] { oracle::Fn<void __cdecl(int, int)>(0x50CC50)(ped, zoom); return 0ull; }, "ped " + std::to_string(ped) + " zoom " + std::to_string(zoom));
    });
    Run("CCamera::SetColVarsVehicle 0x50CCA0", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        const int vt = r.below(11), zoom = r.below(3);
        return Both(d, { W1_CAM_RGS }, [&] { CCamera::SetColVarsVehicle((eVehicleType)vt, zoom); return 0ull; }, [&] { oracle::Fn<void __cdecl(int, int)>(0x50CCA0)(vt, zoom); return 0ull; }, "type " + std::to_string(vt) + " zoom " + std::to_string(zoom));
    });
    // CameraVehicleModeSpecialCases: a raw vehicle buffer (move speed at +0x44, towed vehicle pointer)
    Run("CCamera::CameraVehicleModeSpecialCases 0x50CDE0", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        alignas(16) static uint8 veh[0x800];
        FillGarbage(r, veh, sizeof veh);
        auto* v = reinterpret_cast<CVehicle*>(veh);
        v->m_vecMoveSpeed = r.below(3) ? GVf(r, 0.3f) : GVf(r, 1.f);
        v->m_pVehicleBeingTowed = r.below(2) ? nullptr : reinterpret_cast<CVehicle*>(0x2000u + r.below(4) * 0x10);
        return Both(d, { W1_CAM_RGS, Rg{ (uintptr_t)veh, 0x100 } }, [&] { TheCamera.CameraVehicleModeSpecialCases(v); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, CVehicle*)>(0x50CDE0)(&TheCamera, 0, v); return 0ull; });
    });
    Run("CCamera::CameraPedAimModeSpecialCases 0x50CDA0", [](Rng& r, std::string& d) {
        w1cam::Setup(r);
        alignas(16) static uint8 ped[0x800];
        FillGarbage(r, ped, sizeof ped);
        auto* p = reinterpret_cast<CPed*>(ped);
        p->bInVehicle = r.below(2);
        p->m_pVehicle = r.below(3) ? reinterpret_cast<CVehicle*>(0x3000u + r.below(4) * 0x10) : nullptr;
        return Both(d, { W1_CAM_RGS, Rg{ (uintptr_t)ped, 0x100 } }, [&] { TheCamera.CameraPedAimModeSpecialCases(p); return 0ull; }, [&] { oracle::Fn<void __fastcall(CCamera*, int, CPed*)>(0x50CDA0)(&TheCamera, 0, p); return 0ull; });
    });
}
