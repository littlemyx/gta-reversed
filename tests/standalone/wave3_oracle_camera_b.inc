// ORACLE-W3-CAMERA rows B: CCam member functions (Cam.cpp) and the free functions of that file. ROW / CC / Tw are defined in wave3_oracle_camera.inc.
void OracleFlyBySplineVec3(CVector* out, float* data, float time, int32* idx);                       // Cam.cpp (NOTSA_ORACLE_ADDRESS_GLOBALS): the anonymous-namespace functions
void OracleFlyBySplineFloat(float* out, float* data, float time, int32* idx);
bool OracleGetArrestCamPosBesideCop(CEntity* target, CPed* cop, const CVector* targetPos, CVector* outPos);
void WellBufferMe(float target, float& valueToChange, float& speedSoFar, float topSpeed, float speedStep, bool isAnAngle);

namespace w3c {
// a spline block: count, then `stride` floats per entry (time in seconds ascending, then 3 / 1 values + control values); times sometimes equal / close (the short-segment skip), the controls sometimes equal to the point (straight line)
static void BuildSpline(Rng& r, float* d, int stride, int maxCount = 7) {
    const int count = r.below(8) == 0 ? 1 + r.below(2) : 2 + r.below(maxCount - 1);
    d[0] = (float)count;
    float t = r.below(4) ? 0.f : FlP(r, 2.f);
    for (int e = 0; e < count; ++e) {
        float* p = d + 1 + e * stride;
        p[0] = t;
        t += r.below(10) == 0 ? r.f01() * 0.04f : 0.2f + r.f01() * 3.f;
        const int vals = (stride - 1) / 3;   // 3 (vec3: stride 10) or 1 (float: stride 4)
        for (int k = 1; k < stride; ++k) p[k] = vals == 3 ? FlP(r, 100.f) : FlP(r, 90.f);
        if (r.below(3) == 0) for (int k = 0; k < vals; ++k) p[1 + vals + k] = p[1 + k];   // incoming control = point
        if (r.below(3) == 0) for (int k = 0; k < vals; ++k) p[1 + 2 * vals + k] = p[1 + k]; // outgoing control = point
    }
}
static void PickCamForRow(Rng& rr, int k, bool vehTarget) {   // make cam k the active one (mostly) and give it a vehicle / any target
    if (rr.below(4)) TheCamera.m_nActiveCam = (uint8)k;
    if (vehTarget && rr.below(8)) Cam(k)->m_pCamTargetEntity = Veh(rr.below(2));
}
} // namespace w3c

static void TestW3CameraB() {
    using namespace w2; using namespace w3c;
    ROW("CCam::Constructor", 0x517730, 100) {
        const int k = r.below(3);
        CC(new (Cam(k)) CCam(); return U(Cam(k));, return (uint64_t)ExeT<uint32_t>(0x517730, Cam(k)););
    } ENDROW;
    ROW("CCam::Init", 0x50E490, 150) {
        const int k = r.below(3);
        CC(Cam(k)->Init(); return 0;, ExeT<void>(0x50E490, Cam(k)); return 0;);
    } ENDROW;
    ROW("CCam::DoCamBump", 0x50CB30, 200) {
        const int k = r.below(3); const float h = Fl(r, 3.f, 8), v = Fl(r, 3.f, 8);
        CC(Cam(k)->DoCamBump(h, v); return 0;, ExeT<void>(0x50CB30, Cam(k), h, v); return 0;);
    } ENDROW;
    ROW("CCam::CacheLastSettingsDWCineyCam", 0x50D7A0, 400) {
        const int k = r.below(3);
        Tw tw([](Rng& rr) {
            uint8_t* rw = S(oRw); FillGarbage(rr, rw, 0x300);
            const float nc = rr.below(10) ? 0.1f + rr.f01() * 2.f : Fl(rr, 3.f, 2);
            std::memcpy(rw + 0x80, &nc, 4);                                  // the exe's RwCamera::nearPlane
            std::memcpy(rw + offsetof(RwCamera, nearPlane), &nc, 4);         // librw's
            Scene.m_pRwCamera = reinterpret_cast<RwCamera*>(rw);
        });
        CC(Cam(k)->CacheLastSettingsDWCineyCam(); return 0;, ExeT<void>(0x50D7A0, Cam(k)); return 0;);
    } ENDROW;
    ROW("CCam::GetCoreDataForDWCineyCamMode", 0x517130, 600) {
        const int k = r.below(3);
        Tw tw([k](Rng& rr) { Cam(k)->m_pCamTargetEntity = Ent((int)rr.below(4)); });
        CEntity** ent = reinterpret_cast<CEntity**>(S(oOut)); CVehicle** veh = reinterpret_cast<CVehicle**>(S(oOut + 4));
        CVector* v[7]; for (int i = 0; i < 7; ++i) v[i] = reinterpret_cast<CVector*>(S(oOut + 0x10 + (size_t)i * 0x10));   // dest, src, up, right, fwd, vel, angVel
        float* spd = reinterpret_cast<float*>(S(oOut + 0x80)); float* aspd = reinterpret_cast<float*>(S(oOut + 0x84));
        CColSphere* sp = reinterpret_cast<CColSphere*>(S(oOut + 0x90));
        CC(Cam(k)->GetCoreDataForDWCineyCamMode(*ent, *veh, *v[0], *v[1], *v[2], *v[3], *v[4], *v[5], *spd, *v[6], *aspd, *sp); return 0;,
           ExeT<void>(0x517130, Cam(k), ent, veh, v[0], v[1], v[2], v[3], v[4], v[5], spd, v[6], aspd, sp); return 0;);
    } ENDROW;
    ROW("CCam::RotCamIfInFrontCar", 0x50A4F0, 3000) {
        const int k = r.below(3);
        const float orient = r.below(8) ? (r.f01() - 0.5f) * 14.f : Fl(r, 10.f, 2);
        Tw tw([k](Rng& rr) {
            PickCamForRow(rr, k, true);
            CCam& c = *Cam(k);
            c.m_nDirectionWasLooking = rr.below(3) ? 3 : (int32)rr.below(4);
            c.m_fHorizontalAngle = rr.below(10) ? (rr.f01() - 0.5f) * 14.f : Fl(rr, 10.f, 2);
            if (rr.below(2)) c.m_fHorizontalAngle = (rr.f01() - 0.5f) * 3.f;
            c.m_vecSource = V3P(rr, 50.f);
            if (rr.below(3) == 0) { CVehicle* v = static_cast<CVehicle*>(c.m_pCamTargetEntity); if (v) v->m_vecMoveSpeed = CVector((rr.f01() - 0.5f) * 0.2f, (rr.f01() - 0.5f) * 0.2f, 0.f); }
            *reinterpret_cast<CVector*>(S(oOut)) = rr.below(8) ? V3P(rr, 50.f) : V3(rr, 50.f, 2);
        });
        CC(Cam(k)->RotCamIfInFrontCar(*reinterpret_cast<CVector*>(S(oOut)), orient); return 0;, ExeT<void>(0x50A4F0, Cam(k), S(oOut), orient); return 0;);
    } ENDROW;
    ROW("CCam::Process_Cam_TwoPlayer_Separate_Cars", 0x513510, 2500) {
        const int k = r.below(3);
        Tw tw([k](Rng& rr) {
            CWorld::Players[1].m_pPed = reinterpret_cast<CPlayerPed*>(PedF(1));
            PedF(0)->m_pVehicle = Veh(rr.below(2)); PedF(1)->m_pVehicle = Veh(rr.below(2));
            if (rr.below(3) == 0) PedF(1)->m_pVehicle = PedF(0)->m_pVehicle;
            Cam(k)->m_nTwoPlayerFocusedPlayer = (int32)rr.below(2);
            Cam(k)->m_fTwoPlayerFocusBlend = rr.below(6) ? rr.f01() : (rr.below(2) ? 0.f : (rr.below(2) ? 1.f : Fl(rr, 2.f, 3)));
        });
        CC(Cam(k)->Process_Cam_TwoPlayer_Separate_Cars(); return 0;, ExeT<void>(0x513510, Cam(k)); return 0;);
    } ENDROW;
    ROW("CCam::Process_Cam_TwoPlayer_Separate_Cars_TopDown", 0x513BE0, 1500) {
        const int k = r.below(3);
        Tw tw([k](Rng& rr) {
            CWorld::Players[1].m_pPed = reinterpret_cast<CPlayerPed*>(PedF(1));
            for (int i = 0; i < 2; ++i) { PedF(i)->m_pVehicle = Veh(rr.below(2)); PedF(i)->bInVehicle = rr.below(2); }
            Cam(k)->m_fCameraHeightMultiplier = rr.below(4) ? 0.3f + rr.f01() * 0.8f : Fl(rr, 2.f, 3);
        });
        CC(Cam(k)->Process_Cam_TwoPlayer_Separate_Cars_TopDown(); return 0;, ExeT<void>(0x513BE0, Cam(k)); return 0;);
    } ENDROW;
    ROW("CCam::Process_FlyBy", 0x5B25F0, 3000) {
        const int k = r.below(3);
        const float orient = Fl(r, 3.f, 6), sv = Fl(r, 1.f, 6), svw = Fl(r, 1.f, 6);
        Tw tw([](Rng& rr) {
            float* sp[4] = { reinterpret_cast<float*>(S(oSpl + 0xC00)) /*fov*/, reinterpret_cast<float*>(S(oSpl + 0x800)) /*roll*/, reinterpret_cast<float*>(S(oSpl)) /*src*/, reinterpret_cast<float*>(S(oSpl + 0x400)) /*look*/ };
            BuildSpline(rr, sp[0], 4); BuildSpline(rr, sp[1], 4); BuildSpline(rr, sp[2], 10); BuildSpline(rr, sp[3], 10);
            for (int i = 0; i < 4; ++i) TheCamera.m_aPathArray[i].m_pArrPathData = sp[i];
            TheCamera.m_bStartingSpline = rr.below(4) != 0;
            TheCamera.m_bCutsceneFinished = rr.below(10) == 0;
            // the segment indices of a running fly-by (exe image globals)
            At<int32_t>(0xBC407C) = 11 + 10 * (int32_t)rr.below(4); At<int32_t>(0xBC4080) = 11 + 10 * (int32_t)rr.below(4);
            At<int32_t>(0xBC4084) = 5 + 4 * (int32_t)rr.below(4); At<int32_t>(0xBC4088) = 5 + 4 * (int32_t)rr.below(4);
            CTimer::ms_fTimeStepNonClipped = rr.below(8) ? 0.5f + rr.f01() * 3.f : Fl(rr, 5.f, 2);
            // an elapsed time inside / near the end of the spline
            const float* src = sp[2]; const float tEnd = src[1 + ((int)src[0] - 1) * 10] * 1000.f;
            Cam(0)->m_fTimeElapsedFloat = rr.below(10) == 0 ? tEnd + rr.f01() * 500.f : rr.f01() * tEnd;
            Cam(1)->m_fTimeElapsedFloat = Cam(0)->m_fTimeElapsedFloat; Cam(2)->m_fTimeElapsedFloat = Cam(0)->m_fTimeElapsedFloat;
        });
        CVector* tg = reinterpret_cast<CVector*>(S(oOut));
        CC(Cam(k)->Process_FlyBy(*tg, orient, sv, svw); return 0;, ExeT<void>(0x5B25F0, Cam(k), tg, orient, sv, svw); return 0;);
    } ENDROW;
    ROW("WellBufferMe", 0x509AE0, 5000) {
        const bool ang = r.below(2);
        const float tgt = ang ? (r.below(10) ? (r.f01() - 0.5f) * 14.f : Fl(r, 20.f, 2)) : Fl(r, 20.f, 12);
        const float top = r.below(8) ? 0.05f + r.f01() * 0.3f : Fl(r, 1.f, 2), step = r.below(8) ? 0.001f + r.f01() * 0.1f : Fl(r, 1.f, 2);
        Tw tw([ang](Rng& rr) {
            float* v = reinterpret_cast<float*>(S(oOut)); float* sp = reinterpret_cast<float*>(S(oOut + 4));
            *v = ang && rr.below(10) ? (rr.f01() - 0.5f) * 14.f : Fl(rr, 20.f, 12); *sp = rr.below(6) ? (rr.f01() - 0.5f) * 0.5f : Fl(rr, 2.f, 6);
        });
        float* v = reinterpret_cast<float*>(S(oOut)); float* sp = reinterpret_cast<float*>(S(oOut + 4));
        CC(WellBufferMe(tgt, *v, *sp, top, step, ang); return 0;, ExeC<void>(0x509AE0, tgt, v, sp, top, step, (uint8_t)ang); return 0;);
    } ENDROW;
    ROW("FlyBySplineVec3", 0x5B2090, 4000) {
        const float tm = r.below(8) ? r.f01() * 30000.f : Fl(r, 30000.f, 3);
        Tw tw([](Rng& rr) {
            float* d = reinterpret_cast<float*>(S(oSpl)); BuildSpline(rr, d, 10);
            const int cnt = (int)d[0];
            int32_t idx = rr.below(8) ? 11 + 10 * (int32_t)rr.below((uint32)cnt) : (int32_t)(10 + rr.below((uint32)(cnt * 10 + 12)));
            *reinterpret_cast<int32_t*>(S(oOut + 0x20)) = idx;
            *reinterpret_cast<CVector*>(S(oOut)) = V3P(rr, 10.f);
            TheCamera.m_vecCutSceneOffset = rr.below(3) ? CVector(0.f, 0.f, 0.f) : V3P(rr, 50.f);
        });
        CVector* o = reinterpret_cast<CVector*>(S(oOut)); float* dat = reinterpret_cast<float*>(S(oSpl)); int32_t* ix = reinterpret_cast<int32_t*>(S(oOut + 0x20));
        CC(OracleFlyBySplineVec3(o, dat, tm, ix); return 0;, ExeC<void>(0x5B2090, o, dat, tm, ix); return 0;);
    } ENDROW;
    ROW("FlyBySplineFloat", 0x5B2330, 4000) {
        const float tm = r.below(8) ? r.f01() * 30000.f : Fl(r, 30000.f, 3);
        Tw tw([](Rng& rr) {
            float* d = reinterpret_cast<float*>(S(oSpl)); BuildSpline(rr, d, 4);
            const int cnt = (int)d[0];
            int32_t idx = rr.below(8) ? 5 + 4 * (int32_t)rr.below((uint32)cnt) : (int32_t)(4 + rr.below((uint32)(cnt * 4 + 6)));
            *reinterpret_cast<int32_t*>(S(oOut + 0x20)) = idx;
            *reinterpret_cast<float*>(S(oOut)) = FlP(rr, 10.f);
        });
        float* o = reinterpret_cast<float*>(S(oOut)); float* dat = reinterpret_cast<float*>(S(oSpl)); int32_t* ix = reinterpret_cast<int32_t*>(S(oOut + 0x20));
        CC(OracleFlyBySplineFloat(o, dat, tm, ix); return 0;, ExeC<void>(0x5B2330, o, dat, tm, ix); return 0;);
    } ENDROW;
    ROW("CCam::GetArrestCamPosBesideCop", 0x515D80, 3000) {
        const int kt = r.below(7), kc = r.below(7);
        CEntity* tg = kt == 6 ? nullptr : Ent(kt); CPed* cop = kc == 6 ? nullptr : PedF(kc & 1);
        Tw tw([](Rng& rr) {
            *reinterpret_cast<CVector*>(S(oOut)) = rr.below(10) ? V3P(rr, 40.f) : V3(rr, 40.f, 2);
            *reinterpret_cast<CVector*>(S(oOut + 0x10)) = V3P(rr, 5.f);
            if (rr.below(4) == 0) *reinterpret_cast<CVector*>(S(oOut)) = reinterpret_cast<CEntity*>(PedF(rr.below(2)))->GetPosition() + CVector(rr.below(2) ? 0.f : (rr.f01() - 0.5f) * 6.f, (rr.f01() - 0.5f) * 6.f, (rr.f01() - 0.5f) * 3.f);
        });
        const CVector* tp = reinterpret_cast<CVector*>(S(oOut)); CVector* out = reinterpret_cast<CVector*>(S(oOut + 0x10));
        CC(return B8(OracleGetArrestCamPosBesideCop(tg, cop, tp, out));, return (uint64_t)ExeT<uint8_t>(0x515D80, nullptr, tg, cop, tp, out););
    } ENDROW;
}
