// ORACLE-W3-UI rows F: draw / debug calls recorded by stubs on both sides: CRadar::ShowRadarMarker (CTheScripts::ScriptDebugLine3D, 0x485DE0).
// The exe address is oracle::Patch()ed to a host stub and the port method (TheScripts.cpp is not linked) is defined here with the same body; the call log lives in the scratch block and is compared.
namespace w3uf {
using namespace w3u; using namespace w2;
static float NoDen(float v) { return (std::fabs(v) < 1e-12f) ? 0.f : v; }   // denormal / tiny operands: x87 extended range vs float rounding (no game values)
constexpr size_t oRec = 0x7000;   // u32 count, then 40 byte entries {start xyz, end xyz, c1, c2}
static void RecLine(const CVector& a, const CVector& b, uint32_t c1, uint32_t c2) {
    uint32_t& n = SAt<uint32_t>(oRec);
    if (n < 64) { float* e = reinterpret_cast<float*>(S(oRec + 4 + n * 40)); e[0] = a.x; e[1] = a.y; e[2] = a.z; e[3] = b.x; e[4] = b.y; e[5] = b.z; std::memcpy(&e[6], &c1, 4); std::memcpy(&e[7], &c2, 4); }
    ++n;
}
static void __cdecl X_Dbg3D(float x1, float y1, float z1, float x2, float y2, float z2, uint32_t c1, uint32_t c2) { RecLine(CVector(x1, y1, z1), CVector(x2, y2, z2), c1, c2); }   // (the exe passes the vectors by value; its own body is a bare ret)
static void InstallStubsF() { static bool done = false; if (done) return; done = true; oracle::Patch(0x485DE0, (void*)&X_Dbg3D); }
} // namespace w3uf
void CTheScripts::ScriptDebugLine3D(const CVector& start, const CVector& end, uint32 color1, uint32 color2) { w3uf::RecLine(start, end, color1, color2); }

static void TestW3UiF() {
    using namespace w2; using namespace w3u; using namespace w3uf;
    InstallStubsF();
    ROW("CRadar::ShowRadarMarker", 0x584480, 3000) {
        // no denormal / tiny specials: the exe keeps the intermediates in extended range, the port's float math rounds a denormal result differently (no game values)
        const auto Fn = [&](float sc) { const float v = r.below(10) == 0 ? GenF(r, sc, S_NAN | S_ZERO | S_INF) : GF(r, sc); return (std::fabs(v) < 1e-12f) ? 0.f : v; };
        const CVector p(Fn(3000.f), Fn(3000.f), Fn(100.f)); const uint32_t col = r.u32(); const float rad = Fn(50.f);
        Tu tu([](Rng& rr) { SAt<uint32_t>(oRec) = 0; for (int i = 0; i < 16; ++i) reinterpret_cast<float*>(&TheCamera.m_mCameraMatrix)[i] = NoDen(rr.below(10) == 0 ? GenF(rr, 2.f, S_NAN | S_ZERO | S_INF) : GF(rr, i % 4 == 3 ? 1.f : 2.f)); });
        return UCase(r, d, [&]() -> uint64_t { CRadar::ShowRadarMarker(p, col, rad); return SAt<uint32_t>(oRec); }, [&]() -> uint64_t { oracle::Fn<void __cdecl(float, float, float, uint32_t, float)>(0x584480)(p.x, p.y, p.z, col, rad); return SAt<uint32_t>(oRec); });
    } ENDROW;
}
