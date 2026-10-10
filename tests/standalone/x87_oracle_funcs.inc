// ported game functions whose exe code uses INLINE x87 transcendental instructions: port vs exe machine code, bit-exact
static void SetV(RwV3d& d, const CVector& v) { d.x = v.x; d.y = v.y; d.z = v.z; }
struct MatBuf {
    alignas(16) uint8 b[sizeof(CMatrix)];
    CMatrix& m() { return *reinterpret_cast<CMatrix*>(b); }
};
static void FillMat(Rng& r, MatBuf& mb, float scale, unsigned mask = S_ALL) {
    std::memset(mb.b, 0, sizeof mb.b);
    auto& m = mb.m();
    SetV(m.GetRight(), GenV(r, scale, mask)); SetV(m.GetForward(), GenV(r, scale, mask)); SetV(m.GetUp(), GenV(r, scale, mask)); SetV(m.GetPosition(), GenV(r, scale, mask));
}
// rotation matrix from three double angles (so the euler functions see mostly valid rotations)
static void FillRotMat(Rng& r, MatBuf& mb) {
    FillMat(r, mb, 1.f, 0);
    const double a = r.f01() * 6.4 - 3.2, b = r.f01() * 3.2 - 1.6, c = r.f01() * 6.4 - 3.2;
    const double sa = std::sin(a), ca = std::cos(a), sb = std::sin(b), cb = std::cos(b), sc = std::sin(c), cc = std::cos(c);
    auto& m = mb.m();
    SetV(m.GetRight(),   { (float)(ca * cc - sa * sb * sc), (float)(sa * cc + ca * sb * sc), (float)(-cb * sc) });
    SetV(m.GetForward(), { (float)(-sa * cb), (float)(ca * cb), (float)sb });
    SetV(m.GetUp(),      { (float)(ca * sc + sa * sb * cc), (float)(sa * sc - ca * sb * cc), (float)(cb * cc) });
}
static std::string M(const CMatrix& m) { return "R" + V(m.GetRight()) + " F" + V(m.GetForward()) + " U" + V(m.GetUp()) + " P" + V(m.GetPosition()); }
static uint32 EulerFlags(Rng& r) { if (const char* e = std::getenv("EULER_FLAGS")) return (uint32)std::atoi(e); return r.below(2) | (r.below(2) << 1) | (r.below(6) << 2); }
// the exe leaves part of its stack-local matrix uninitialised for some flag combinations (garbage in the output): such cases are not deterministic, skip them
__declspec(noinline) static void DirtyStack(uint32 pattern) { volatile uint32 junk[2048]; for (auto& j : junk) j = pattern; }
// g_huge: 0 = angles in the range the game produces (including NaN / 0 / denormal specials), 1 = some arguments beyond 2^63 (fsin / fcos leave them unchanged, the CRT reduces them)
static int g_huge = 0;
template<class Fn> static void RunH(const std::string& name, Fn&& fn) { if (g_huge && name.find("[args]") == std::string::npos) {} Run(g_huge ? name + " [huge]" : name, fn); }
static float Ang(Rng& r) { switch (r.below(g_huge ? 3 : 5)) { case 0: return GenF(r, 3.2f); case 1: return GenF(r, 400.f); case 2: return g_huge ? GenF(r, 1e30f, S_HUGE | S_INF) : GenF(r, 1e5f, S_NOHUGE); default: return GenF(r, 7.f, S_NOHUGE); } }

static void TestCover() {
    RunH("CCover::FindDirFromVector 0x698D40", [](Rng& r, std::string& d) {
        const float s = PickScale(r); const CVector v{ GenF(r, s), GenF(r, s), GenF(r, s) };
        const auto dir = CCover::FindDirFromVector(v);
        uint8 a; std::memcpy(&a, &dir, 1);
        const uint8 b = (uint8)oracle::Fn<int __cdecl(float, float, float)>(0x698D40)(v.x, v.y, v.z);
        if (a == b) return true;
        char t[64]; std::snprintf(t, sizeof t, " got %u exe %u", a, b); d = "in " + V(v) + t; return false; });
    RunH("CCover::FindVectorFromDir 0x698D60", [](Rng& r, std::string& d) {
        const uint8 raw = (uint8)r.u32(); CVector b{ 9, 9, 9 };
        const CVector a = CCover::FindVectorFromDir(CCoverPoint::Dir{ raw });
        oracle::Fn<void __cdecl(CVector*, uint32)>(0x698D60)(&b, raw);
        if (SameV(a, b)) return true; d = "raw " + std::to_string(raw) + " got " + V(a) + " exe " + V(b); return false; });
}
static void TestCurvesHandling() {
    RunH("CCurves::CalcCorrectedDist 0x43C880", [](Rng& r, std::string& d) {
        const float s = PickScale(r), cur = GenF(r, s), tot = r.below(8) ? std::fabs(GenF(r, s)) : GenF(r, s), sv = GenF(r, 3.f);
        float ta = -1, tb = -1;
        const float a = CCurves::CalcCorrectedDist(cur, tot, sv, &ta), b = oracle::Fn<float __cdecl(float, float, float, float*)>(0x43C880)(cur, tot, sv, &tb);
        if (SameF(a, b) && SameF(ta, tb)) return true;
        d = "in " + F(cur) + "," + F(tot) + "," + F(sv) + " got " + F(a) + "/" + F(ta) + " exe " + F(b) + "/" + F(tb); return false; });
    RunH("cHandlingDataMgr::ConvertBikeDataToGameUnits 0x6F5290", [](Rng& r, std::string& d) {
        float a[0x20], b[0x20];
        for (auto& f : a) f = GenF(r, Ang(r) * 20.f);
        std::memcpy(b, a, sizeof a);
        static uint8 mgrStore[16]{};   // (the method does not touch the manager; no constructor run)
        auto* mgr = reinterpret_cast<cHandlingDataMgr*>(mgrStore);
        mgr->ConvertBikeDataToGameUnits(reinterpret_cast<tBikeHandlingData*>(a));
        oracle::Fn<void __fastcall(void*, int, void*)>(0x6F5290)(mgr, 0, b);
        if (SameBlob(a, b, sizeof a)) return true; d = "in max " + F(a[5]) + " got " + F(a[5]) + " exe " + F(b[5]); return false; });
}
static void TestMatrixEuler() {
    RunH("CMatrix::ConvertToEulerAngles 0x59A840", [](Rng& r, std::string& d) {
        MatBuf mb; if (r.below(4)) FillRotMat(r, mb); else FillMat(r, mb, PickScale(r));
        const uint32 fl = EulerFlags(r);
        float a[3] = { -1, -1, -1 }, b[3] = { -1, -1, -1 }, c[3] = { -1, -1, -1 };
        DirtyStack(0x7F7F7F7F); oracle::Fn<void __fastcall(CMatrix*, int, float*, float*, float*, uint32)>(0x59A840)(&mb.m(), 0, &c[0], &c[1], &c[2], fl);
        DirtyStack(0x11223344);
        oracle::Fn<void __fastcall(CMatrix*, int, float*, float*, float*, uint32)>(0x59A840)(&mb.m(), 0, &b[0], &b[1], &b[2], fl);
        if (!SameBlob(b, c, 12)) { g_special = true; return true; }   // exe nondeterministic (uninitialised stack read) for this flag combination
        mb.m().ConvertToEulerAngles(&a[0], &a[1], &a[2], fl);
        if (SameF(a[0], b[0]) && SameF(a[1], b[1]) && SameF(a[2], b[2])) return true;
        d = "flags " + std::to_string(fl) + " " + M(mb.m()) + " got " + F(a[0]) + "," + F(a[1]) + "," + F(a[2]) + " exe " + F(b[0]) + "," + F(b[1]) + "," + F(b[2]); return false; });
    RunH("CMatrix::ConvertFromEulerAngles 0x59AA40", [](Rng& r, std::string& d) {
        MatBuf ma, mb, mc; FillMat(r, ma, 1.f); std::memcpy(mb.b, ma.b, sizeof mb.b); std::memcpy(mc.b, ma.b, sizeof mc.b);
        const float x = Ang(r), y = Ang(r), z = Ang(r); const uint32 fl = EulerFlags(r);
        DirtyStack(0x7F7F7F7F); oracle::Fn<void __fastcall(CMatrix*, int, float, float, float, uint32)>(0x59AA40)(&mc.m(), 0, x, y, z, fl);
        DirtyStack(0x11223344); oracle::Fn<void __fastcall(CMatrix*, int, float, float, float, uint32)>(0x59AA40)(&mb.m(), 0, x, y, z, fl);
        if (!SameBlob(mb.b, mc.b, 0x40)) return true;   // exe nondeterministic (uninitialised stack read) for this flag combination
        ma.m().ConvertFromEulerAngles(x, y, z, fl);
        if (SameBlob(ma.b, mb.b, 0x40)) return true;
        d = "flags " + std::to_string(fl) + " in " + F(x) + "," + F(y) + "," + F(z) + " got " + M(ma.m()) + " exe " + M(mb.m()); return false; });
    RunH("CMatrix::SetRotate(x,y,z) 0x59B120", [](Rng& r, std::string& d) {
        MatBuf ma, mb; FillMat(r, ma, 1.f); std::memcpy(mb.b, ma.b, sizeof mb.b);
        const float x = Ang(r), y = Ang(r), z = Ang(r);
        ma.m().SetRotate(x, y, z);
        oracle::Fn<void __fastcall(CMatrix*, int, float, float, float)>(0x59B120)(&mb.m(), 0, x, y, z);
        if (SameBlob(ma.b, mb.b, 0x40)) return true; d = "in " + F(x) + "," + F(y) + "," + F(z) + " got " + M(ma.m()) + " exe " + M(mb.m()); return false; });
    struct Ax { const char* name; unsigned va; void (CMatrix::*fn)(float); };
    static const Ax axes[] = {
        { "SetRotateX 0x59B060", 0x59B060, &CMatrix::SetRotateX }, { "SetRotateY 0x59B0A0", 0x59B0A0, &CMatrix::SetRotateY }, { "SetRotateZ 0x59B0E0", 0x59B0E0, &CMatrix::SetRotateZ },
        { "SetRotateXOnly 0x59AFA0", 0x59AFA0, &CMatrix::SetRotateXOnly }, { "SetRotateYOnly 0x59AFE0", 0x59AFE0, &CMatrix::SetRotateYOnly }, { "SetRotateZOnly 0x59B020", 0x59B020, &CMatrix::SetRotateZOnly },
    };
    for (const auto& ax : axes) {
        RunH(std::string("CMatrix::") + ax.name, [&ax](Rng& r, std::string& d) {
            MatBuf ma, mb; FillMat(r, ma, 1.f); std::memcpy(mb.b, ma.b, sizeof mb.b);
            const float x = Ang(r);
            (ma.m().*ax.fn)(x);
            oracle::Fn<void __fastcall(CMatrix*, int, float)>(ax.va)(&mb.m(), 0, x);
            if (SameBlob(ma.b, mb.b, 0x40)) return true; d = "in " + F(x) + " got " + M(ma.m()) + " exe " + M(mb.m()); return false; });
    }
}
static void TestQuat() {
    RunH("CQuaternion::Get(x,y,z) [Get_Euler] 0x59C160", [](Rng& r, std::string& d) {
        CQuaternion q{ GenF(r, 1.f), GenF(r, 1.f), GenF(r, 1.f), GenF(r, 1.f) };
        if (r.below(3)) { const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w); if (n > 0.f) { q.x /= n; q.y /= n; q.z /= n; q.w /= n; } }
        float a[3]{ -1, -1, -1 }, b[3]{ -1, -1, -1 };
        q.Get(&a[0], &a[1], &a[2]);
        oracle::Fn<void __fastcall(CQuaternion*, int, float*, float*, float*)>(0x59C160)(&q, 0, &b[0], &b[1], &b[2]);
        if (SameF(a[0], b[0]) && SameF(a[1], b[1]) && SameF(a[2], b[2])) return true;
        d = "q " + Q(q) + " got " + F(a[0]) + "," + F(a[1]) + "," + F(a[2]) + " exe " + F(b[0]) + "," + F(b[1]) + "," + F(b[2]); return false; });
    RunH("CQuaternion::Set(x,y,z) [Set_Euler] 0x59C530", [](Rng& r, std::string& d) {
        CQuaternion a{ 9, 9, 9, 9 }, b = a; const float x = Ang(r), y = Ang(r), z = Ang(r);
        a.Set(x, y, z);
        oracle::Fn<void __fastcall(CQuaternion*, int, float, float, float)>(0x59C530)(&b, 0, x, y, z);
        if (SameQ(a, b)) return true; d = "in " + F(x) + "," + F(y) + "," + F(z) + " got " + Q(a) + " exe " + Q(b); return false; });
    RunH("CQuaternion::Get(axis,angle) [Get_AxisAngle] 0x59C230", [](Rng& r, std::string& d) {
        CQuaternion q{ GenF(r, 1.f), GenF(r, 1.f), GenF(r, 1.f), r.below(4) ? GenF(r, 1.1f) : GenF(r, 5.f) };
        CVector ax1{ -1, -1, -1 }, ax2 = ax1; float an1 = -1, an2 = -1;
        q.Get((RwV3d*)&ax1, &an1);
        oracle::Fn<void __fastcall(CQuaternion*, int, CVector*, float*)>(0x59C230)(&q, 0, &ax2, &an2);
        if (SameV(ax1, ax2) && SameF(an1, an2)) return true;
        d = "q " + Q(q) + " got " + V(ax1) + "," + F(an1) + " exe " + V(ax2) + "," + F(an2); return false; });
    RunH("CQuaternion::Set(axis,angle) [Set_AxisAngle] 0x59C600", [](Rng& r, std::string& d) {
        CQuaternion a{ 9, 9, 9, 9 }, b = a; CVector ax = GenV(r, 1.f); const float an = Ang(r);
        a.Set((RwV3d*)&ax, an);
        oracle::Fn<void __fastcall(CQuaternion*, int, CVector*, float)>(0x59C600)(&b, 0, &ax, an);
        if (SameQ(a, b)) return true; d = "axis " + V(ax) + " angle " + F(an) + " got " + Q(a) + " exe " + Q(b); return false; });
}
static void TestPlaceable() {
    RunH("CPlaceable::GetHeading 0x441DB0", [](Rng& r, std::string& d) {
        alignas(16) uint8 pl[sizeof(CPlaceable)]{}; MatBuf mb; FillMat(r, mb, PickScale(r));
        auto* p = reinterpret_cast<CPlaceable*>(pl);
        const bool hasMat = r.below(4) != 0; p->m_matrix = hasMat ? reinterpret_cast<CMatrixLink*>(&mb.m()) : nullptr; p->m_placement.m_fHeading = Ang(r);
        const float a = p->GetHeading(), b = oracle::Fn<float __fastcall(CPlaceable*, int)>(0x441DB0)(p, 0);
        if (SameF(a, b)) return true; d = "mat " + std::to_string(hasMat) + " " + M(mb.m()) + " heading " + F(p->m_placement.m_fHeading) + " got " + F(a) + " exe " + F(b); return false; });
    RunH("CPlaceable::GetRoll 0x420B30", [](Rng& r, std::string& d) {
        alignas(16) uint8 pl[sizeof(CPlaceable)]{}; MatBuf mb; FillMat(r, mb, PickScale(r));
        auto* p = reinterpret_cast<CPlaceable*>(pl); p->m_matrix = reinterpret_cast<CMatrixLink*>(&mb.m());
        const float a = p->GetRoll(), b = oracle::Fn<float __fastcall(CPlaceable*, int)>(0x420B30)(p, 0);
        if (SameF(a, b)) return true; d = M(mb.m()) + " got " + F(a) + " exe " + F(b); return false; });
}
static void TestCamera() {
    // CCamera member functions on the shared TheCamera object (0xB6F028): the object is saved, randomised, run through the port, restored, run through the exe, and the whole object compared
    constexpr unsigned CAM = 0xB6F028;
    auto camRun = [](Rng& r, std::string& d, auto&& setup, auto&& portCall, unsigned va, auto&& exeCall, const char* what) {
        const auto saved = Snap(CAM, sizeof(CCamera));
        setup(r);
        const auto init = Snap(CAM, sizeof(CCamera));
        portCall();
        const auto afterPort = Snap(CAM, sizeof(CCamera));
        Put(CAM, init);
        exeCall(va);
        const auto afterExe = Snap(CAM, sizeof(CCamera));
        Put(CAM, saved);
        return SameMem(afterPort, afterExe, what, d);
    };
    RunH("CCamera::ProcessVectorTrackLinear(float) 0x50D350", [&](Rng& r, std::string& d) {
        const float ratio = r.below(3) ? r.f01() : GenF(r, 3.f);
        return camRun(r, d, [&](Rng& r) { TheCamera.m_bTrackLinearWithEase = r.below(2); TheCamera.m_vecTrackLinearStartPoint = GenV(r, PickScale(r)); TheCamera.m_vecTrackLinearEndPoint = GenV(r, PickScale(r)); },
            [&] { TheCamera.ProcessVectorTrackLinear(ratio); }, 0x50D350, [&](unsigned va) { oracle::Fn<void __fastcall(CCamera*, int, float)>(va)(&TheCamera, 0, ratio); }, "TheCamera"); });
    RunH("CCamera::ProcessVectorMoveLinear(float) 0x50D430", [&](Rng& r, std::string& d) {
        const float ratio = r.below(3) ? r.f01() : GenF(r, 3.f);
        return camRun(r, d, [&](Rng& r) { TheCamera.m_bMoveLinearWithEase = r.below(2); TheCamera.m_vecMoveLinearPosnStart = GenV(r, PickScale(r)); TheCamera.m_vecMoveLinearPosnEnd = GenV(r, PickScale(r)); },
            [&] { TheCamera.ProcessVectorMoveLinear(ratio); }, 0x50D430, [&](unsigned va) { oracle::Fn<void __fastcall(CCamera*, int, float)>(va)(&TheCamera, 0, ratio); }, "TheCamera"); });
    RunH("CCamera::ProcessFOVLerp(float) 0x50D510", [&](Rng& r, std::string& d) {
        const float ratio = r.below(3) ? r.f01() : GenF(r, 3.f);
        return camRun(r, d, [&](Rng& r) { TheCamera.m_nZoomMode = (uint8)r.below(2); TheCamera.m_fZoomInFactor = GenF(r, 90.f); TheCamera.m_fZoomOutFactor = GenF(r, 90.f); },
            [&] { TheCamera.ProcessFOVLerp(ratio); }, 0x50D510, [&](unsigned va) { oracle::Fn<void __fastcall(CCamera*, int, float)>(va)(&TheCamera, 0, ratio); }, "TheCamera"); });
    RunH("CCamera::Find3rdPersonQuickAimPitch 0x50AD40", [&](Rng& r, std::string& d) {
        const auto saved = Snap(CAM, sizeof(CCamera));
        TheCamera.m_nActiveCam = (uint8)r.below(2);
        auto& cam = TheCamera.m_aCams[TheCamera.m_nActiveCam];
        float* f = reinterpret_cast<float*>(&cam);
        for (unsigned i = 0; i < sizeof(CCam) / 4; ++i) f[i] = GenF(r, 5.f);
        const float a = TheCamera.Find3rdPersonQuickAimPitch(), b = oracle::Fn<float __fastcall(CCamera*, int)>(0x50AD40)(&TheCamera, 0);
        Put(CAM, saved);
        if (SameF(a, b)) return true; d = "got " + F(a) + " exe " + F(b); return false; });
}
static void TestMisc() {
    RunH("CCheckpoints::SetHeading 0x722970", [](Rng& r, std::string& d) {
        const unsigned arr = reinterpret_cast<uintptr_t>(&CCheckpoints::m_aCheckPtArray);
        const auto saved = Snap(arr, sizeof(CCheckpoints::m_aCheckPtArray));
        auto bytes = saved;
        for (auto& b : bytes) b = (uint8)r.u32();
        auto* cps = reinterpret_cast<CCheckpoint*>(bytes.data());
        for (int i = 0; i < 32; ++i) { float* fp = reinterpret_cast<float*>(&cps[i].m_Fwd); for (int k = 0; k < 3; ++k) fp[k] = GenF(r, 3.f, S_ZERO | S_TINY); }
        const uint32 id = cps[r.below(32)].m_ID; const float h = Ang(r);
        Put(arr, bytes);
        CCheckpoints::SetHeading(id, h);
        const auto a = Snap(arr, bytes.size());
        Put(arr, bytes);
        oracle::Fn<void __cdecl(uint32, float)>(0x722970)(id, h);
        const auto b = Snap(arr, bytes.size());
        Put(arr, saved);
        return SameMem(a, b, "checkpoint array", d); });
}
static void TestFuncs() {
  for (g_huge = 0; g_huge < 2; ++g_huge) {
    TestCover();
    TestCurvesHandling();
    TestMatrixEuler();
    TestQuat();
    TestPlaceable();
    TestCamera();
    TestMisc();
  }
}
