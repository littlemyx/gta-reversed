// (b) camera: CCamera / CCam helpers on the exe's TheCamera global (both implementations work on the same statics; snapshot / restore around each call)
#include "Camera.h"
#include "Cam.h"
#include "Draw.h"
#include "Entity/Vehicle/Vehicle.h"

static constexpr uintptr_t CAM_VA = 0xB6F028;
static_assert(offsetof(CCamera, m_bUseSpecialFovTrain) == 0x36 && offsetof(CCamera, m_fFovForTrain) == 0x130);
struct CamRun {
    Snap in{ CAM_VA, sizeof(CCamera) }, pa{ CAM_VA, sizeof(CCamera) }, pe{ CAM_VA, sizeof(CCamera) };
    Snap xin, xpa, xpe;   // optional extra exe globals (e.g. the function-local statics of CCam::KeepTrackOfTheSpeed)
    uint32_t ts = 0;
    CamRun() : xin(0, 0), xpa(0, 0), xpe(0, 0) {}
    CamRun(uintptr_t va, size_t n) : xin(va, n), xpa(va, n), xpe(va, n) {}
    void FillExtra(Rng& r) { if (xin.b.empty()) return; auto* w = reinterpret_cast<uint32_t*>(xin.va); for (size_t i = 0; i < xin.b.size() / 4; ++i) { const float v = GenF(r, 100.f, S_NOHUGE); std::memcpy(&w[i], &v, 4); } }
    void Begin(Rng& r, float scale = 0.f) {   // random floats everywhere (pointers become garbage: only call helpers that do not dereference them)
        if (scale == 0.f) scale = r.below(3) ? 100.f : PickScale(r);
        auto* w = reinterpret_cast<uint32_t*>(CAM_VA);
        for (size_t i = 0; i < sizeof(CCamera) / 4; ++i) { const float v = GenF(r, scale); std::memcpy(&w[i], &v, 4); }
        TheCamera.m_nActiveCam = (uint8)r.below(2);
        ts = CTimer::m_snTimeInMilliseconds = r.below(4) ? r.u32() % 2000000 : r.u32();
    }
    void Mark()     { in.Save(); if (!xin.b.empty()) xin.Save(); }
    void PortDone() { pa.Save(); in.Restore(); if (!xin.b.empty()) { xpa.Save(); xin.Restore(); } }
    void ExeDone()  { pe.Save(); in.Restore(); if (!xin.b.empty()) { xpe.Save(); xin.Restore(); } }
    bool Same()     { return pa.SameAs(pe) && (xin.b.empty() || xpa.SameAs(xpe)); }
    std::string Diff() { return DiffDesc(pa.b.data(), pe.b.data(), pa.b.size(), "cam") + (xin.b.empty() ? "" : DiffDesc(xpa.b.data(), xpe.b.data(), xpa.b.size(), "xtra")); }
};

// ---- stubs for the world-facing callees of the camera modes: scripted + recorded, installed on BOTH sides
#include "WaterLevel.h"
#include "TimeCycle.h"
namespace cwf {
struct Rec {
    int cone = 0; CVector conePos{}, coneLook{}; float coneRad = 0, coneMin = 0; CEntity* coneIgnore = nullptr;
    int blur = 0; int blurR = 0, blurG = 0, blurB = 0, blurV = 0, blurType = 0;
    int water = 0; float wx = 0, wy = 0, wz = 0; std::vector<CVector> waterCalls;
    int playerVeh = 0;
};
inline Rec g_rec;
inline bool coneRet = false; inline CVector coneOut{}; inline float coneDist = 0;
inline bool waterRet = false; inline float waterLevel = 0;
struct WaterEntry { bool ret = false; float level = 0; CVector normal{ 0, 0, 1 }; };
inline WaterEntry waterTab[16]; inline size_t waterIdx = 0;   // the k-th call of a run uses entry k % 16 (entry 0 = waterRet / waterLevel)
inline void Reset() { g_rec = Rec{}; waterIdx = 0; }
inline void* playerVeh = nullptr;
inline bool Cone(const CVector& pos, const CVector& look, CVector& out, float rad, float mn, float& dist) {
    ++g_rec.cone; g_rec.conePos = pos; g_rec.coneLook = look; g_rec.coneRad = rad; g_rec.coneMin = mn; g_rec.coneIgnore = CWorld::pIgnoreEntity;
    if (coneRet) { out = coneOut; dist = coneDist; }
    return coneRet;
}
inline void Blur(int r, int g, int b, int v, int type) { ++g_rec.blur; g_rec.blurR = r; g_rec.blurG = g; g_rec.blurB = b; g_rec.blurV = v; g_rec.blurType = type; }
inline bool Water(float x, float y, float z, float& out, CVector* normals = nullptr) {
    ++g_rec.water; g_rec.wx = x; g_rec.wy = y; g_rec.wz = z; g_rec.waterCalls.push_back({ x, y, z });
    const WaterEntry& e = waterTab[waterIdx++ % 16];
    if (e.ret) out = e.level;
    if (normals) *normals = e.normal;
    return e.ret;
}
inline bool SameRec(const Rec& a, const Rec& b) {
    return a.cone == b.cone && SameV(a.conePos, b.conePos) && SameV(a.coneLook, b.coneLook) && SameF(a.coneRad, b.coneRad) && SameF(a.coneMin, b.coneMin) && a.coneIgnore == b.coneIgnore
        && a.blur == b.blur && a.blurR == b.blurR && a.blurG == b.blurG && a.blurB == b.blurB && a.blurV == b.blurV && a.blurType == b.blurType
        && a.water == b.water && (a.water == 0 || (SameF(a.wx, b.wx) && SameF(a.wy, b.wy) && SameF(a.wz, b.wz))) && a.playerVeh == b.playerVeh && SameV3Arr(a.waterCalls.data(), b.waterCalls.data(), (int)std::min(a.waterCalls.size(), b.waterCalls.size())) && a.waterCalls.size() == b.waterCalls.size();
}
inline std::string Desc(const Rec& a) {
    return "cone " + std::to_string(a.cone) + " blur " + std::to_string(a.blur) + "(" + std::to_string(a.blurR) + "," + std::to_string(a.blurG) + "," + std::to_string(a.blurB) + "," + std::to_string(a.blurV) + "," + std::to_string(a.blurType) + ") water " + std::to_string(a.water) + " pveh " + std::to_string(a.playerVeh);
}
}
bool CWaterLevel::GetWaterLevel(float x, float y, float z, float& out, uint8, CVector* n) { return cwf::Water(x, y, z, out, n); }
bool CWaterLevel::GetWaterLevel(const CVector& pos, float& out, bool, CVector* n) { return cwf::Water(pos.x, pos.y, pos.z, out, n); }
CVehicle* FindPlayerVehicle(int32, bool) { ++cwf::g_rec.playerVeh; return reinterpret_cast<CVehicle*>(cwf::playerVeh); }
static bool __fastcall HostCone(CCamera*, int, const CVector& pos, const CVector& look, CVector& out, float rad, float mn, float& dist) { return cwf::Cone(pos, look, out, rad, mn, dist); }
static void* __cdecl ExeFindPlayerVehicle(int, bool) { ++cwf::g_rec.playerVeh; return cwf::playerVeh; }
static bool __cdecl ExeWater(float x, float y, float z, float* out, bool, CVector* n) { return cwf::Water(x, y, z, *out, n); }
static bool g_camStubsInstalled = false;
static void InstallCamStubs() {
    if (g_camStubsInstalled) return;
    g_camStubsInstalled = true;
    PatchHostFn(PmAddr(&CCamera::ConeCastCollisionResolve), (void*)&HostCone);
    oracle::Patch(0x51A5D0, (void*)&HostCone);
    oracle::Patch(0x56E0D0, (void*)&ExeFindPlayerVehicle);
    oracle::Patch(0x6EB690, (void*)&ExeWater);
}
struct CamWorldScript {   // random script for the stubs of one case + the camera-related exe globals (water colours, allow-collision flag)
    static void Roll(Rng& r) {
        cwf::coneRet = r.below(2); cwf::coneOut = GenV(r, 100.f); cwf::coneDist = GenF(r, 5.f);
        cwf::waterRet = r.below(2); cwf::waterLevel = GenF(r, 100.f);
        for (auto& e : cwf::waterTab) { e.ret = r.below(2); e.level = GenF(r, 100.f, S_NOHUGE); e.normal = r.below(2) ? CVector{ 0, 0, 1 } : GenDir(r); }
        cwf::waterTab[0].ret = cwf::waterRet; cwf::waterTab[0].level = cwf::waterLevel;
        static alignas(16) uint8 veh[16]; cwf::playerVeh = r.below(2) ? veh : nullptr;
        auto* w = reinterpret_cast<uint32_t*>(0xB7C500);   // CTimeCycle current colours: water rgb at 0xB7C508..0xB7C510
        for (int i = 0; i < 8; ++i) { const float v = r.below(3) ? r.f01() * 300.f : std::fabs(GenF(r, 300.f, S_NOHUGE)); std::memcpy(&w[i], &v, 4); /* water colours >= 0 (the exe inlines SetMotionBlur and stores the raw int) */ }
        *reinterpret_cast<uint8_t*>(0xB6EC2C) = r.below(2);    // gAllowScriptedFixedCameraCollision (exe global)
        cwf::Reset();
    }
};

static void TestCamCam() {
    // CCam helpers: `this` is one of TheCamera's two active cams
    InstallCamStubs();
    Run("CCam::Process_Fixed 0x51D470", [](Rng& r, std::string& d) {
        CamRun c(0xB7CD68, 4); c.Begin(r); c.FillExtra(r);   // extra: CWorld::pIgnoreEntity
        auto& cam = TheCamera.m_aCams[r.below(2)];
        cam.m_vecCamFixedModeSource = GenV(r, 100.f); cam.m_vecCamFixedModeUpOffSet = r.below(3) ? GenV(r, 1.f) : GenV(r, 5.f);
        cam.m_nDirectionWasLooking = (uint8)r.below(5);
        TheCamera.m_bUseSpecialFovTrain = r.below(2); TheCamera.m_fFovForTrain = r.below(2) ? 50.f + r.f01() * 40.f : GenF(r, 120.f);
        CamWorldScript::Roll(r);
        const CVector target = GenV(r, 100.f); const float ori = GenF(r, 6.f), sv = GenF(r, 1.f), svw = GenF(r, 1.f);
        c.Mark();
        cwf::Reset();
        cam.Process_Fixed(target, ori, sv, svw); const auto recA = cwf::g_rec; c.PortDone();
        cwf::Reset();
        oracle::Fn<void __fastcall(CCam*, int, const CVector*, float, float, float)>(0x51D470)(&cam, 0, &target, ori, sv, svw); const auto recE = cwf::g_rec; c.ExeDone();
        if (c.Same() && cwf::SameRec(recA, recE)) return true;
        d = "target " + V(target) + " fixedsrc " + V(cam.m_vecCamFixedModeSource) + " " + cwf::Desc(recA) + " / exe " + cwf::Desc(recE) + c.Diff();
        return false;
    });
    Run("CCam::GetVectorsReadyForRW 0x509CE0", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r);
        auto& cam = TheCamera.m_aCams[r.below(2)];
        cam.m_vecFront = r.below(4) ? GenDir(r) : GenV(r, 2.f);
        if (r.below(8) == 0) { cam.m_vecFront.x = r.below(2) ? 0.f : -0.f; cam.m_vecFront.y = r.below(2) ? 0.f : -0.f; }
        c.Mark();
        cam.GetVectorsReadyForRW(); c.PortDone();
        oracle::Fn<void __fastcall(CCam*, int)>(0x509CE0)(&cam, 0); c.ExeDone();
        if (c.Same()) return true;
        d = "front " + V(cam.m_vecFront) + c.Diff();
        return false;
    });
    Run("CCam::ClipBeta 0x509C50", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r);
        auto& cam = TheCamera.m_aCams[r.below(2)];
        cam.m_fHorizontalAngle = r.below(3) ? (r.f01() - 0.5f) * 14.f : GenF(r, 20.f, S_NOHUGE);
        c.Mark();
        cam.ClipBeta(); c.PortDone();
        oracle::Fn<void __fastcall(CCam*, int)>(0x509C50)(&cam, 0); c.ExeDone();
        if (c.Same()) return true;
        d = "beta " + F(cam.m_fHorizontalAngle) + c.Diff();
        return false;
    });
    Run("CCam::Using3rdPersonMouseCam 0x50A850", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r);
        auto& cam = TheCamera.m_aCams[r.below(2)];
        cam.m_nMode = (eCamMode)(r.below(2) ? 4 : r.below(60));   // 4 = MODE_FOLLOWPED
        CCamera::m_bUseMouse3rdPerson = r.below(2);
        c.Mark();
        const bool ra = cam.Using3rdPersonMouseCam(); c.PortDone();
        const bool re = oracle::Fn<bool __fastcall(CCam*, int)>(0x50A850)(&cam, 0); c.ExeDone();
        Hit(ra);
        if (ra == re && c.Same()) return true;
        d = "mode " + std::to_string((int)cam.m_nMode) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
        return false;
    });
    Run("CCam::KeepTrackOfTheSpeed 0x509DF0", [](Rng& r, std::string& d) {
        CamRun c(0xB6FF5C, 0x34); c.Begin(r); c.FillExtra(r);
        *reinterpret_cast<uint32_t*>(0xB6FF8C) = r.u32() & 0x3F;   // init guard bits
        auto& cam = TheCamera.m_aCams[r.below(2)];
        TheCamera.m_bJust_Switched = r.below(4) == 0;
        const CVector s = GenV(r, 100.f), tg = GenV(r, 100.f), up = GenV(r, 1.f);
        const float al = r.below(3) ? (r.f01() - 0.5f) * 3.f : GenF(r, 8.f, S_NOHUGE), be = r.below(3) ? (r.f01() - 0.5f) * 14.f : GenF(r, 20.f, S_NOHUGE), fov = GenF(r, 100.f);
        c.Mark();
        cam.KeepTrackOfTheSpeed(s, tg, up, al, be, fov); c.PortDone();
        oracle::Fn<void __fastcall(CCam*, int, const CVector*, const CVector*, const CVector*, const float*, const float*, const float*)>(0x509DF0)(&cam, 0, &s, &tg, &up, &al, &be, &fov); c.ExeDone();
        if (c.Same()) return true;
        d = "al " + F(al) + " be " + F(be) + " fov " + F(fov) + c.Diff();
        return false;
    });
    Run("CCamera::Find3rdPersonCamTargetVector 0x514970", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r);
        CDraw::ms_fAspectRatio = r.below(4) ? 1.f + r.f01() * 1.5f : GenF(r, 3.f);
        TheCamera.m_f3rdPersonCHairMultY = r.below(4) ? r.f01() : GenF(r, 2.f);
        TheCamera.m_f3rdPersonCHairMultX = r.below(4) ? r.f01() : GenF(r, 2.f);
        auto& cam = TheCamera.m_aCams[TheCamera.m_nActiveCam];
        cam.m_nMode = (eCamMode)(r.below(8) ? 4 : 56);   // not MODE_TWOPLAYER_IN_CAR_AND_SHOOTING (reads the players' pads)
        cam.m_fFOV = r.below(4) ? 20.f + r.f01() * 80.f : GenF(r, 200.f);
        cam.m_vecFront = r.below(2) ? GenDir(r) : GenV(r, 2.f); cam.m_vecUp = r.below(2) ? GenV(r, 1.f) : CVector{ 0, 0, 1 };
        const float range = GenF(r, 100.f); const CVector muzzle = GenV(r, 100.f);
        CVector sa{ 7, 8, 9 }, ta{ 7, 8, 9 }, se{ 7, 8, 9 }, te{ 7, 8, 9 };
        c.Mark();
        TheCamera.Find3rdPersonCamTargetVector(range, muzzle, sa, ta); c.PortDone();
        oracle::Fn<void __fastcall(CCamera*, int, float, CVector, CVector*, CVector*)>(0x514970)(&TheCamera, 0, range, muzzle, &se, &te); c.ExeDone();
        if (c.Same() && SameV(sa, se) && SameV(ta, te)) return true;
        d = "range " + F(range) + " muzzle " + V(muzzle) + " front " + V(cam.m_vecFront) + " up " + V(cam.m_vecUp) + " fov " + F(cam.m_fFOV) + " got " + V(sa) + V(ta) + " exe " + V(se) + V(te) + c.Diff();
        return false;
    });
}

static void TestCamera() {
    TestCamCam();
    Run("CCamera::StoreValuesDuringInterPol 0x50C290", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r);
        CVector s = GenV(r, 100.f), t = GenV(r, 100.f), u = GenV(r, 1.f); float fov = GenF(r, 90.f);
        c.Mark();
        TheCamera.StoreValuesDuringInterPol(&s, &t, &u, &fov); c.PortDone();
        oracle::Fn<void __fastcall(CCamera*, int, CVector*, CVector*, CVector*, float*)>(0x50C290)(&TheCamera, 0, &s, &t, &u, &fov); c.ExeDone();
        if (c.Same()) return true;
        d = "s " + V(s) + " t " + V(t) + c.Diff();
        return false;
    });
    Run("CCamera::CalculateMirroredMatrix 0x50B380", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r);
        CMatrix in1, out1, out2; std::memset((void*)&in1, 0, sizeof in1); std::memset((void*)&out1, 0, sizeof out1); std::memset((void*)&out2, 0, sizeof out2);
        GenMatrix(r, reinterpret_cast<uint8*>(&in1), 50.f); std::memcpy((void*)&out2, (void*)&out1, sizeof out1);
        const CVector n = r.below(2) ? GenDir(r) : GenV(r, 2.f); const float mv = GenF(r, 50.f);
        c.Mark();
        TheCamera.CalculateMirroredMatrix(n, mv, &in1, &out1); c.PortDone();
        oracle::Fn<void __fastcall(CCamera*, int, CVector, float, CMatrix*, CMatrix*)>(0x50B380)(&TheCamera, 0, n, mv, &in1, &out2); c.ExeDone();
        if (c.Same() && SameBlob(&out1, &out2, 0x40)) return true;
        d = "n " + V(n) + " mv " + F(mv) + " in " + Hex(&in1, 0x40) + " got " + Hex(&out1, 0x40) + " exe " + Hex(&out2, 0x40) + c.Diff();
        return false;
    });
    Run("CCamera::ProcessVectorMoveLinear(float) 0x50D430", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r); TheCamera.m_bMoveLinearWithEase = r.below(2);
        const float ratio = r.below(4) ? r.f01() : GenF(r, 3.f);
        c.Mark();
        TheCamera.ProcessVectorMoveLinear(ratio); c.PortDone();
        oracle::Fn<void __fastcall(CCamera*, int, float)>(0x50D430)(&TheCamera, 0, ratio); c.ExeDone();
        if (c.Same()) return true;
        d = "ratio " + F(ratio) + c.Diff();
        return false;
    });
    Run("CCamera::ProcessVectorTrackLinear(float) 0x50D350", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r); TheCamera.m_bTrackLinearWithEase = r.below(2);
        const float ratio = r.below(4) ? r.f01() : GenF(r, 3.f);
        c.Mark();
        TheCamera.ProcessVectorTrackLinear(ratio); c.PortDone();
        oracle::Fn<void __fastcall(CCamera*, int, float)>(0x50D350)(&TheCamera, 0, ratio); c.ExeDone();
        if (c.Same()) return true;
        d = "ratio " + F(ratio) + c.Diff();
        return false;
    });
    Run("CCamera::ProcessFOVLerp(float) 0x50D510", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r); TheCamera.m_nZoomMode = (uint8)r.below(3);
        const float ratio = r.below(4) ? r.f01() : GenF(r, 3.f);
        c.Mark();
        TheCamera.ProcessFOVLerp(ratio); c.PortDone();
        oracle::Fn<void __fastcall(CCamera*, int, float)>(0x50D510)(&TheCamera, 0, ratio); c.ExeDone();
        if (c.Same()) return true;
        d = "ratio " + F(ratio) + c.Diff();
        return false;
    });
    Run("CCamera::SetPercentAlongCutScene 0x50C070", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r);
        for (auto& cam : TheCamera.m_aCams) { cam.m_nMode = (eCamMode)(r.below(3) ? 17 : r.below(60)); cam.m_nFinishTime = r.below(4) ? r.u32() % 100000 : r.u32(); }   // 17 = MODE_FLYBY
        const float pc = r.below(4) ? r.f01() * 100.f : GenF(r, 300.f);
        c.Mark();
        TheCamera.SetPercentAlongCutScene(pc); c.PortDone();
        oracle::Fn<void __fastcall(CCamera*, int, float)>(0x50C070)(&TheCamera, 0, pc); c.ExeDone();
        if (c.Same()) return true;
        d = "pc " + F(pc) + " active " + std::to_string(TheCamera.m_nActiveCam) + " modes " + std::to_string((int)TheCamera.m_aCams[0].m_nMode) + "," + std::to_string((int)TheCamera.m_aCams[1].m_nMode) + c.Diff();
        return false;
    });
    Run("CCamera::FinishCutscene 0x514950", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r);
        for (auto& cam : TheCamera.m_aCams) { cam.m_nMode = (eCamMode)(r.below(3) ? 17 : r.below(60)); cam.m_nFinishTime = r.below(4) ? r.u32() % 100000 : r.u32(); }
        c.Mark();
        TheCamera.FinishCutscene(); c.PortDone();
        oracle::Fn<void __fastcall(CCamera*, int)>(0x514950)(&TheCamera, 0); c.ExeDone();
        if (c.Same()) return true;
        d = c.Diff();
        return false;
    });
    Run("CCamera::InitCameraVehicleTweaks 0x50A3B0", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r); TheCamera.m_bCameraVehicleTweaksInitialized = r.below(2);
        c.Mark();
        TheCamera.InitCameraVehicleTweaks(); c.PortDone();
        oracle::Fn<void __fastcall(CCamera*, int)>(0x50A3B0)(&TheCamera, 0); c.ExeDone();
        if (c.Same()) return true;
        d = c.Diff();
        return false;
    });
    Run("CCamera::ApplyVehicleCameraTweaks 0x50A480", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r);
        alignas(16) uint8 veh[0x600] {};
        const int16 model = (int16)(r.below(3) ? 501 : (int)r.below(700));   // 501 = RCGOBLIN
        reinterpret_cast<CEntity*>(veh)->m_nModelIndex = model;
        TheCamera.m_bCameraVehicleTweaksInitialized = r.below(2);
        TheCamera.m_nCurrentTweakModelIndex = r.below(3) ? model : (int16)r.below(700);
        for (auto& t : TheCamera.m_aCamTweak) t.ModelID = (int16)(r.below(3) ? model : r.below(700) - 1);
        c.Mark();
        TheCamera.ApplyVehicleCameraTweaks(reinterpret_cast<CVehicle*>(veh)); c.PortDone();
        oracle::Fn<void __fastcall(CCamera*, int, CVehicle*)>(0x50A480)(&TheCamera, 0, reinterpret_cast<CVehicle*>(veh)); c.ExeDone();
        if (c.Same()) return true;
        d = "model " + std::to_string(model) + c.Diff();
        return false;
    });
    Run("CCamera::GetArrPosForVehicleType 0x50AF00", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r);
        const int type = r.below(3) ? r.below(12) : (int)r.u32();
        int pa = -77, pe = -77;
        c.Mark();
        const bool ra = TheCamera.GetArrPosForVehicleType((eVehicleType)type, pa); c.PortDone();
        const bool re = oracle::Fn<bool __fastcall(CCamera*, int, int, int*)>(0x50AF00)(&TheCamera, 0, type, &pe); c.ExeDone();
        Hit(ra);
        if (ra == re && pa == pe && c.Same()) return true;
        d = "type " + std::to_string(type) + " got " + std::to_string(ra) + "/" + std::to_string(pa) + " exe " + std::to_string(re) + "/" + std::to_string(pe);
        return false;
    });
    Run("CCamera::Find3rdPersonQuickAimPitch 0x50AD40", [](Rng& r, std::string& d) {
        CamRun c; c.Begin(r);
        CDraw::ms_fAspectRatio = r.below(4) ? 1.f + r.f01() * 1.5f : GenF(r, 3.f);
        TheCamera.m_f3rdPersonCHairMultY = r.below(4) ? r.f01() : GenF(r, 2.f);
        for (auto& cam : TheCamera.m_aCams) cam.m_fFOV = r.below(4) ? 20.f + r.f01() * 80.f : GenF(r, 200.f);
        c.Mark();
        const float ra = TheCamera.Find3rdPersonQuickAimPitch(); c.PortDone();
        const float re = oracle::Fn<float __fastcall(CCamera*, int)>(0x50AD40)(&TheCamera, 0); c.ExeDone();
        if (SameF(ra, re) && c.Same()) return true;
        d = "aspect " + F(CDraw::ms_fAspectRatio) + " chair " + F(TheCamera.m_f3rdPersonCHairMultY) + " got " + F(ra) + " exe " + F(re);
        return false;
    });
}
