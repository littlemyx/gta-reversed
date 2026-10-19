// ORACLE-W3-CAMERA rows A: CCamera member functions (Camera.cpp) on TheCamera. ROW / CC / Tw are defined in wave3_oracle_camera.inc.
#include "Audio/AudioEngine.h"
// CAudioEngine mock for ProcessMusicFade: AudioEngine.cpp is not linked into this test. The port's calls land in these definitions, the exe's calls are Patch()ed to host stubs that do the same:
// IsLoadingTuneActive answers a flag of the scratch block, the two fader setters append (kind, float bits) to a log in the scratch block (compared).
namespace w3c {
constexpr size_t oAud = oOut + 0x200;   // [0] loading-tune flag, [1] entry count, [2..] (kind, bits) pairs
static void AudLog(uint32_t kind, float f) {
    uint32_t* a = reinterpret_cast<uint32_t*>(S(oAud)); uint32_t bits; std::memcpy(&bits, &f, 4);
    const uint32_t n = a[1] < 20 ? a[1] : 0;
    a[2 + 2 * n] = kind; a[3 + 2 * n] = bits; a[1] = n + 1;
}
static bool __fastcall HostLoadingTune(void*, void*) { return reinterpret_cast<uint32_t*>(S(oAud))[0] != 0; }
static void __fastcall HostSetMusic(void*, void*, float f) { AudLog(1, f); }
static void __fastcall HostSetEffects(void*, void*, float f) { AudLog(2, f); }
} // namespace w3c
bool CAudioEngine::IsLoadingTuneActive() { return w3c::HostLoadingTune(nullptr, nullptr); }
void CAudioEngine::SetMusicFaderScalingFactor(float f) { w3c::AudLog(1, f); }
void CAudioEngine::SetEffectsFaderScalingFactor(float f) { w3c::AudLog(2, f); }
static void TestW3CameraA() {
    using namespace w2; using namespace w3c;
    CCamera* const T = reinterpret_cast<CCamera*>(CAM);
    ROW("CCamera::InitialiseCameraForDebugMode", 0x50AF90, 100) { CC(T->InitialiseCameraForDebugMode(); return 0;, ExeT<void>(0x50AF90, T); return 0;); } ENDROW;
    ROW("CCamera::ProcessObbeCinemaCameraPed", 0x50B880, 100) { CC(T->ProcessObbeCinemaCameraPed(); return 0;, ExeT<void>(0x50B880, T); return 0;); } ENDROW;
    ROW("CCamera::RenderMotionBlur", 0x50B8F0, 100) { CC(T->RenderMotionBlur(); return 0;, ExeT<void>(0x50B8F0, T); return 0;); } ENDROW;
    ROW("CCamera::SetCameraDirectlyBehindForFollowPed_CamOnAString", 0x50BD40, 400) {
        Tw tw([](Rng& rr) { if (rr.below(6) == 0) CWorld::Players[CWorld::PlayerInFocus].m_pPed = nullptr; });
        CC(T->SetCameraDirectlyBehindForFollowPed_CamOnAString(); return 0;, ExeT<void>(0x50BD40, T); return 0;);
    } ENDROW;
    ROW("CCamera::SetCameraDirectlyInFrontForFollowPed_CamOnAString", 0x50BD70, 400) {
        Tw tw([](Rng& rr) { if (rr.below(6) == 0) CWorld::Players[CWorld::PlayerInFocus].m_pPed = nullptr; });
        CC(T->SetCameraDirectlyInFrontForFollowPed_CamOnAString(); return 0;, ExeT<void>(0x50BD70, T); return 0;);
    } ENDROW;
    ROW("CCamera::SetCameraDirectlyBehindForFollowPed_ForAPed_CamOnAString", 0x50BDA0, 500) {
        const int k = r.below(7);   // 6 = null
        CPed* p = k == 6 ? nullptr : PedF(k & 1);
        CC(T->SetCameraDirectlyBehindForFollowPed_ForAPed_CamOnAString(p); return 0;, ExeT<void>(0x50BDA0, T, p); return 0;);
    } ENDROW;
    ROW("CCamera::SetCameraDirectlyInFrontForFollowPed_ForAPed_CamOnAString", 0x50BE30, 500) {
        const int k = r.below(7);
        CPed* p = k == 6 ? nullptr : PedF(k & 1);
        CC(T->SetCameraDirectlyInFrontForFollowPed_ForAPed_CamOnAString(p); return 0;, ExeT<void>(0x50BE30, T, p); return 0;);
    } ENDROW;
    ROW("CCamera::ProcessMusicFade", 0x50B6D0, 3000) {
        Tw tw([](Rng& rr) {
            static bool patched = false;
            if (!patched) { patched = true; oracle::Patch(0x506D90, (void*)&HostLoadingTune); oracle::Patch(0x506E40, (void*)&HostSetMusic); oracle::Patch(0x506E50, (void*)&HostSetEffects); }
            uint32_t* a = reinterpret_cast<uint32_t*>(S(oAud)); a[0] = rr.below(4) == 0; a[1] = 0;
            TheCamera.m_bMusicFading = rr.below(5) != 0;
            TheCamera.m_nMusicFadingDirection = (eFadeFlag)(rr.below(8) == 0 ? (int)rr.below(4) : (int)rr.below(2));
            TheCamera.m_nFadeInOutFlag = (eFadeFlag)(rr.below(8) == 0 ? (int)rr.below(4) : (int)rr.below(2));
            if (rr.below(12) == 0) TheCamera.m_fTimeToWaitToFadeMusic = std::numeric_limits<float>::quiet_NaN();
            TheCamera.m_fTimeToWaitToFadeMusic = rr.below(2) ? 0.f : (rr.below(8) ? rr.f01() * 3.f : Fl(rr, 3.f, 3));
            TheCamera.m_fTimeToFadeMusic = rr.below(10) ? 0.1f + rr.f01() * 10.f : (rr.below(2) ? 0.f : Fl(rr, 5.f, 2));
            TheCamera.m_fEffectsFaderScalingFactor = rr.below(8) ? rr.f01() : (rr.below(2) ? 0.f : (rr.below(2) ? 1.f : Fl(rr, 3.f, 3)));
        });
        CC(T->ProcessMusicFade(); return 0;, ExeT<void>(0x50B6D0, T); return 0;);
    } ENDROW;
    // CCamera::SetNearClipBasedOnPedCollision 0x50CB90: not a row - it ends in RwCameraSetNearClipPlane (exe RW core 0x7EE1D0 on an exe-layout RwCamera vs librw; the port side is not linked here, the exe side reads frame-buffer fields)
    ROW("CCamera::ProcessScriptedCommands", 0x516AE0, 600) { CC(T->ProcessScriptedCommands(); return 0;, ExeT<void>(0x516AE0, T); return 0;); } ENDROW;
    ROW("CCamera::Restore", 0x50B930, 800) { CC(T->Restore(); return 0;, ExeT<void>(0x50B930, T); return 0;); } ENDROW;
    ROW("CCamera::RestoreWithJumpCut", 0x50BAB0, 1200) { CC(T->RestoreWithJumpCut(); return 0;, ExeT<void>(0x50BAB0, T); return 0;); } ENDROW;
    ROW("CCamera::SetZoomValueCamStringScript", 0x50C1B0, 800) {
        const int zm = r.below(6) == 0 ? (int)r.below(70000) - 33000 : (int)r.below(3);
        CC(T->SetZoomValueCamStringScript((int16)zm); return 0;, ExeT<void>(0x50C1B0, T, (int)zm); return 0;);
    } ENDROW;
    ROW("CCamera::TakeControl", 0x50C7C0, 1500) {
        const int k = r.below(7), mode = r.below(4) == 0 ? (int)MODE_NONE : (int)r.below(60), sw = r.below(3), who = r.below(4);
        CEntity* e = k == 6 ? nullptr : Ent(k);
        Tw tw([](Rng& rr) { TheCamera.m_bCinemaCamera = rr.below(2); TheCamera.m_nWhoIsInControlOfTheCamera = rr.below(3); });
        CC(T->TakeControl(e, (eCamMode)mode, (eSwitchType)sw, who); return 0;, ExeT<void>(0x50C7C0, T, e, mode, sw, who); return 0;);
    } ENDROW;
    ROW("CCamera::TakeControlAttachToEntity", 0x50C910, 1500) {
        const int kt = r.below(7), ka = r.below(7), sw = r.below(3), who = r.below(4);
        CEntity* tg = kt == 6 ? nullptr : Ent(kt); CEntity* at = ka == 6 ? nullptr : Ent(ka);
        const float tilt = Fl(r, 3.f, 10);
        Tw tw([](Rng& rr) {
            TheCamera.m_nWhoIsInControlOfTheCamera = rr.below(3);
            CVector* off = reinterpret_cast<CVector*>(S(oOut)); CVector* la = reinterpret_cast<CVector*>(S(oOut + 0x10));
            *off = rr.below(3) == 0 ? CVector(0.f, rr.below(2) ? 0.f : -0.f, 0.f) : V3(rr, 5.f, 12);
            *la = rr.below(3) == 0 ? *off : V3(rr, 5.f, 12);
            if (rr.below(8) == 0) la->y = off->y;
        });
        CVector* off = reinterpret_cast<CVector*>(S(oOut)); CVector* la = reinterpret_cast<CVector*>(S(oOut + 0x10));
        CC(T->TakeControlAttachToEntity(tg, at, off, la, tilt, (eSwitchType)sw, who); return 0;,
           ExeT<void>(0x50C910, T, tg, at, off, la, tilt, sw, who); return 0;);
    } ENDROW;
    ROW("CCamera::StartTransitionWhenNotFinishedInter", 0x515BC0, 400) {
        const int m = r.below(60);
        CC(T->StartTransitionWhenNotFinishedInter((eCamMode)m); return 0;, ExeT<void>(0x515BC0, T, m); return 0;);
    } ENDROW;
    ROW("CCamera::StartTransition", 0x515200, 6000) {
        const int m = r.below(8) == 0 ? (int)r.below(256) : (int)r.below(60);
        Tw tw([](Rng& rr) {
            // aim the active cam's mode at the interesting sets
            static const int kModes[] = { MODE_SNIPER, MODE_ROCKETLAUNCHER, MODE_FOLLOWPED, MODE_CAM_ON_A_STRING, MODE_SYPHON, MODE_SYPHON_CRIM_IN_FRONT, MODE_SPECIAL_FIXED_FOR_SYPHON, MODE_FIXED, MODE_AIMWEAPON, MODE_M16_1STPERSON, MODE_BEHINDCAR, MODE_BEHINDBOAT, MODE_CAMERA, MODE_1STPERSON_RUNABOUT, MODE_FIGHT_CAM_RUNABOUT, MODE_HELICANNON_1STPERSON, MODE_PED_DEAD_BABY, MODE_SNIPER_RUNABOUT, MODE_ROCKETLAUNCHER_HS };
            TheCamera.m_nActiveCam = (uint8)(rr.below(12) == 0 ? 2 : rr.below(2));
            if (rr.below(4)) TheCamera.m_aCams[TheCamera.m_nActiveCam].m_nMode = (eCamMode)kModes[rr.below((uint32)(sizeof kModes / sizeof *kModes))];
            if (rr.below(4)) TheCamera.m_pTargetEntity = rr.below(3) ? static_cast<CEntity*>(PedF(rr.below(2))) : Ent((int)rr.below(6));
            if (rr.below(2)) TheCamera.m_aCams[TheCamera.m_nActiveCam].m_vecFront = rr.below(10) ? V3P(rr, 1.f) : V3(rr, 1.f, 2);
        });
        CC(T->StartTransition((eCamMode)m); return 0;, ExeT<void>(0x515200, T, m); return 0;);
    } ENDROW;
}
