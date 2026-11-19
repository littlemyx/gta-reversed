#pragma once
// SH6 NOTSA_SH_ATTRLOG=<file> / NOTSA_SH_PICKCARLOG=<file> (RR debug logs for the TS port, STATEHASH builds only). Same path in both = one shared file.
// NOTSA_SH_ATTRLOG_FRAMES = recorded-frame ranges ("0-2" default, "0-1,180-205") for both logs. Every row: `frame randCount TAG ...`
//   A rows (CPopulation::GeneratePedsAtAttractors): C call header, E entity in range, V visit (+ skip token), D GetRandom2dEffect draw
//   K rows (CLoadedCarGroup::PickRandomCar / PickLeastUsedModel): call header with caller address, M candidate, W draw, T try result, R result
#ifdef NOTSA_STATEHASH
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
namespace notsa::rr {
uint64_t RandCountNow(); // statehash.cpp
int      LoadLogRecFrame();
inline FILE* AttrOpenShared(const char* envName) {
    static std::string s_path[2]; static FILE* s_f[2] = {};
    const char* p = std::getenv(envName);
    if (!p || !p[0]) return nullptr;
    for (int i = 0; i < 2; i++) { if (s_f[i] && s_path[i] == p) return s_f[i]; }
    for (int i = 0; i < 2; i++) {
        if (!s_f[i]) {
            s_f[i] = std::fopen(p, "w"); s_path[i] = p;
            if (s_f[i]) { std::fprintf(s_f[i], "# frame randCount TAG ... | A: C call pos n | E idx model name n2dfx nattr rw | V call pass idx model name rw n2dfx nattr result | D call pass idx model drawnfx rand0 rand1 ; K: K call fn caller group nmodels args | M model frq cls ref used | W draw sum | T try model result | R result\n"); std::fflush(s_f[i]); }
            return s_f[i];
        }
    }
    return nullptr;
}
inline bool AttrFrameWanted(int frame) {
    static std::vector<std::pair<int, int>> r = [] {
        std::vector<std::pair<int, int>> v; const char* e = std::getenv("NOTSA_SH_ATTRLOG_FRAMES"); std::string s = e && e[0] ? e : "0-2";
        size_t b = 0; while (b <= s.size()) { size_t c = s.find(',', b); if (c == std::string::npos) c = s.size(); const std::string t = s.substr(b, c - b); b = c + 1; if (t.empty()) continue; int lo = 0, hi = 0; if (std::sscanf(t.c_str(), "%d-%d", &lo, &hi) == 2) v.push_back({ lo, hi }); else if (std::sscanf(t.c_str(), "%d", &lo) == 1) v.push_back({ lo, lo }); }
        return v; }();
    for (auto& p : r) { if (frame >= p.first && frame <= p.second) return true; }
    return false;
}
inline FILE* AttrLog() { static FILE* f = AttrOpenShared("NOTSA_SH_ATTRLOG"); const int fr = LoadLogRecFrame(); return (f && fr >= 0 && AttrFrameWanted(fr)) ? f : nullptr; }
inline FILE* PickCarLog() { static FILE* f = AttrOpenShared("NOTSA_SH_PICKCARLOG"); const int fr = LoadLogRecFrame(); return (f && fr >= 0 && AttrFrameWanted(fr)) ? f : nullptr; }
} // namespace notsa::rr
#endif
