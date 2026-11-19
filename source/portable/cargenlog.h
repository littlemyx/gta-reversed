#pragma once
// SH3 NOTSA_SH_CARGENLOG=<file> (RR debug log for the TS port, STATEHASH builds only): car creation + parked car generators of the first recorded frames (rec 0-5).
// Row types (every row starts with `rec randCount`; randCount = the rand log's callidx at that moment, so rows can be placed in the rand stream):
//   R  random car attempt (CCarCtrl::GenerateOneRandomCar), rand0 = randCount at entry, rand_end = at exit, outcome = one token per return site or `created <handle> <model>`
//   G  parked car generator that was processed (CCarGenerator::Process): index model pos, outcome = reason or `created <handle>`
//   (rec -1 = before cmd:rr_start: the initial population while loading; R rows only, first 2000 attempts + G rows of generators that ran DoInternalProcessing, first 1500)
//   L  CStreaming::ms_vehiclesLoaded / CPopulation::m_AppropriateLoadedCars (once)    P  popcycle values used by ChooseModel (once)
#ifdef NOTSA_STATEHASH
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <string>
#include <utility>
namespace notsa::rr {
uint64_t RandCountNow(); // statehash.cpp
int      LoadLogRecFrame();
void     CarGenLogOnce(); // CarCtrl.cpp: writes the L and P rows the first time it is called with logging active, and again at rec 0
// SH11 NOTSA_SH_CARGENLOG_FRAMES (ranges like "3085-3088,3090"): when set, only these recorded frames are logged (R rows + E counter rows + per-frame L/P rows)
inline const std::vector<std::pair<int, int>>& CarGenRanges() {
    static const std::vector<std::pair<int, int>> r = [] { std::vector<std::pair<int, int>> v; const char* e = std::getenv("NOTSA_SH_CARGENLOG_FRAMES"); if (!e || !e[0]) return v; std::string s = e; size_t b = 0; while (b <= s.size()) { size_t c = s.find(',', b); if (c == std::string::npos) c = s.size(); const std::string t = s.substr(b, c - b); b = c + 1; int lo = 0, hi = 0; if (std::sscanf(t.c_str(), "%d-%d", &lo, &hi) == 2) v.push_back({ lo, hi }); else if (std::sscanf(t.c_str(), "%d", &lo) == 1) v.push_back({ lo, lo }); } return v; }();
    return r;
}
inline bool CarGenFramesSet() { return !CarGenRanges().empty(); }
inline bool CarGenFrameWanted(int rec) { for (auto& p : CarGenRanges()) { if (rec >= p.first && rec <= p.second) return true; } return false; }
inline FILE* CarGenLog(bool pre = false, bool onlyOpen = false) {
    static FILE* f = [] {
        const char* p = std::getenv("NOTSA_SH_CARGENLOG");
        FILE* h = p && p[0] ? std::fopen(p, "w") : nullptr;
        if (h) { std::fprintf(h, "# rec randCount | R attempt model name ctype chosen_by cnt(other/cops/gangs/dealers) wanted coors(ok|fail|-) pos node_a node_b frac rand_end outcome | G idx model pos outcome | L list ids | P values\n"); std::fflush(h); }
        return h;
    }();
    const int rec = LoadLogRecFrame();
    if (onlyOpen) return f; // raw handle (the caller applies its own gating)
    if (CarGenFramesSet()) return (f && rec >= 0 && CarGenFrameWanted(rec)) ? f : nullptr; // SH11: NOTSA_SH_CARGENLOG_FRAMES ("3085-3088") replaces the default rec 0-5 and the pre-recording rows
    static int s_pre = 0; // pre-recording R attempts (rec -1: the initial population during the load) are logged too, capped
    if (f && pre && rec < 0 && s_pre < 4000) { s_pre++; return f; }
    return (f && rec >= 0 && rec <= 5) ? f : nullptr;
}
} // namespace notsa::rr
#endif
