#pragma once
// SH7 RR debug logs for the TS port (STATEHASH builds only, no behaviour change). Every row: `frame randCount TAG ...`, frame = recorded frame.
//   NOTSA_SH_FRUSTLOG=<file>  NOTSA_SH_FRUSTLOG_FRAMES ("2700-2725" default; SH7B run uses 2712-2724): G row per FxManager_c::CalcFrustumInfo (camera + the frustum FxSystem_c::IsVisible reads),
//                             I row per FxSystem_c::IsVisible call (system name, world bounding sphere, result, culling reason)
//   NOTSA_SH_FLYBYLOG=<file>  NOTSA_SH_FLYBYLOG_FRAMES ("50-62,3075-3090" default): B row per CCam::Process_FlyBy call (spline state), E = early return (cutscene finished)
// Floats are %.9g and raw bits (0x........).
#ifdef NOTSA_STATEHASH
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
namespace notsa::rr {
uint64_t RandCountNow(); // statehash.cpp
int      LoadLogRecFrame();

struct Sh7Log {
    FILE* f = nullptr; std::vector<std::pair<int, int>> r;
    void Init(const char* envName, const char* framesEnv, const char* defFrames, const char* hdr) {
        const char* p = std::getenv(envName);
        if (p && p[0]) { f = std::fopen(p, "w"); if (f) { std::fprintf(f, "%s\n", hdr); std::fflush(f); } }
        const char* e = std::getenv(framesEnv); std::string s = e && e[0] ? e : defFrames;
        size_t b = 0; while (b <= s.size()) { size_t c = s.find(',', b); if (c == std::string::npos) c = s.size(); const std::string t = s.substr(b, c - b); b = c + 1; if (t.empty()) continue; int lo = 0, hi = 0; if (std::sscanf(t.c_str(), "%d-%d", &lo, &hi) == 2) r.push_back({ lo, hi }); else if (std::sscanf(t.c_str(), "%d", &lo) == 1) r.push_back({ lo, lo }); }
    }
    FILE* Get() const { const int fr = LoadLogRecFrame(); if (!f || fr < 0) return nullptr; for (auto& p : r) { if (fr >= p.first && fr <= p.second) return f; } return nullptr; }
};
inline FILE* FrustLog() { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH_FRUSTLOG", "NOTSA_SH_FRUSTLOG_FRAMES", "2700-2725", "# frame randCount G|I ... | G: G camId view=x,y near far proj aspect(CDraw) sphere=cx,cy,cz,r frustplanes[0..3 = camera planes 2..5]=nx,ny,nz,d(+bits) camplanes[0..5] | I: I name key sysptr hasSphere sphere=cx,cy,cz,r result reason(S sphere-vs-frustum-sphere; P<i> plane index; N none; X no bounding sphere) planedist[0..3]=double,numPlanesPassed"); return x; }(); return l.Get(); }
inline FILE* FlyByLog() { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH_FLYBYLOG", "NOTSA_SH_FLYBYLOG_FRAMES", "50-62,3075-3090", "# frame randCount B|E|Q|F ... | Q: script read of CCamera::GetPositionAlongSpline (value, finished flag, time ms); F: CCamera::FinishCutscene call | B also has time (truncated ms) endtest (finishTime<=time) | cam mode, bStartingSpline, bCutsceneFinished, timeStep, timeStepNonClipped, timeElapsedFloat, finishTime, posAlongSpline, seg idx src/look/roll/fov, fov, vecSource, vecFront"); return x; }(); return l.Get(); }
inline void FlyByNote(const char* tag, double v, int flag, unsigned ms) { if (FILE* lf = FlyByLog()) { std::fprintf(lf, "%d %llu %s v=%.9g finished=%d ms=%u\n", LoadLogRecFrame(), (unsigned long long)RandCountNow(), tag, v, flag, ms); std::fflush(lf); } }
inline std::unordered_map<uint32_t, std::string>& FxNameMap() { static std::unordered_map<uint32_t, std::string> m; return m; }
inline uint32_t Sh7Bits(float v) { uint32_t u; std::memcpy(&u, &v, 4); return u; }
// SH11 NOTSA_SH_VEHMTX=<file> (+ NOTSA_SH_VEHMTX_FRAMES default "0", NOTSA_SH_VEHMTX_SLOTS default "2,3"): M row = all 16 floats of the vehicle matrix (hex bits / %.9g), T row = turn speed, before/after a call
void VehMtxLog(const char* tag, const void* entity); // statehash.cpp
struct VehMtxScope { const char* b; const void* e; VehMtxScope(const char* a, const char* b_, const void* e_) : b(b_), e(e_) { VehMtxLog(a, e); } ~VehMtxScope() { VehMtxLog(b, e); } VehMtxScope(const VehMtxScope&) = delete; };
} // namespace notsa::rr
#define SH7F(v) (double)(v), notsa::rr::Sh7Bits(v)
#define SH_VMX(a, b, e) notsa::rr::VehMtxScope shVmxScope_(a, b, e)
#else
#define SH_VMX(a, b, e) ((void)0)
#endif
