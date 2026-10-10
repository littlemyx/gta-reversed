// (b) camera: CCamera / CCam helpers on the exe's TheCamera global (both implementations work on the same statics; snapshot / restore around each call)
#include "Camera.h"
#include "Cam.h"
#include "Draw.h"
#include "Entity/Vehicle/Vehicle.h"

static constexpr uintptr_t CAM_VA = 0xB6F028;
struct CamRun {
    Snap in{ CAM_VA, sizeof(CCamera) }, pa{ CAM_VA, sizeof(CCamera) }, pe{ CAM_VA, sizeof(CCamera) };
    uint32_t ts = 0;
    void Begin(Rng& r, float scale = 0.f) {   // random floats everywhere (pointers become garbage: only call helpers that do not dereference them)
        if (scale == 0.f) scale = r.below(3) ? 100.f : PickScale(r);
        auto* w = reinterpret_cast<uint32_t*>(CAM_VA);
        for (size_t i = 0; i < sizeof(CCamera) / 4; ++i) { const float v = GenF(r, scale); std::memcpy(&w[i], &v, 4); }
        TheCamera.m_nActiveCam = (uint8)r.below(2);
        ts = CTimer::m_snTimeInMilliseconds = r.below(4) ? r.u32() % 2000000 : r.u32();
    }
    void Mark()     { in.Save(); }
    void PortDone() { pa.Save(); in.Restore(); }
    void ExeDone()  { pe.Save(); in.Restore(); }
    bool Same()     { return pa.SameAs(pe); }
    std::string Diff() { return DiffDesc(pa.b.data(), pe.b.data(), pa.b.size(), "cam"); }
};

static void TestCamera() {
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
        for (auto& cam : TheCamera.m_aCams) { cam.m_nMode = (eCamMode)(r.below(3) ? 29 : r.below(60)); cam.m_nFinishTime = r.u32() % 100000; }   // 29 = MODE_FLYBY
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
        for (auto& cam : TheCamera.m_aCams) { cam.m_nMode = (eCamMode)(r.below(3) ? 29 : r.below(60)); cam.m_nFinishTime = r.u32() % 100000; }
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
