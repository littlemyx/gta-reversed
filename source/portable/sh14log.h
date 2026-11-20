#pragma once
// SH14 RR debug logs (STATEHASH builds only, no behaviour change: no rand draws, only pure reads). Rows: `frame randCount stage TAG:enter|exit ...`, stacks = Sh12Stack RVAs (resolve offline with sh12_sym.py).
//   NOTSA_SH14_CARGEN=<file>    NOTSA_SH14_CARGEN_FRAMES (3100-3103): enter/exit scope rows (r0 r1 draws ret callers) of CCarCtrl::GenerateCarCreationCoors2 (inputs; hops/reason are the NOTSA_SH_WALKLOG X rows),
//                               ChooseModel / ChooseCarModelToLoad / ChoosePoliceCarModel / PickGangCar / ChooseGangOccupation / ChooseCivilianOccupation / ChooseCivilianOccupationForVehicle /
//                               ChoosePolicePedOccupation / CStreaming::StreamVehiclesAndPeds (+_Always, StreamZoneModels, StreamZoneModels_Gangs, StreamPedsIntoRandomSlots); GenerateOneRandomCar R/E rows come from NOTSA_SH_CARGENLOG
//   NOTSA_SH14_LOADSCENE=<file> NOTSA_SH14_LOADSCENE_FRAMES (3094): P = CVehicleModelInfo::SetCarCustomPlate (model, key, plate text, rand before/after, callers), X = CEntryExit::CEntryExit (name, flags, timeOn/off, roll draw, callers)
#ifdef NOTSA_STATEHASH
#include "sh12log.h"
#include "cargenlog.h"
namespace notsa::rr {
inline FILE* Sh14CarLog() {
    static Sh7Log l = [] { Sh7Log x; const char* p = std::getenv("NOTSA_SH14_CARGEN"); const char* cg = std::getenv("NOTSA_SH_CARGENLOG");
        if (p && p[0] && cg && !std::strcmp(p, cg)) { // same file as the SH3/SH11 car generation log: rows interleave in call order
            x.f = CarGenLog(false, true);
            const char* fr = std::getenv("NOTSA_SH14_CARGEN_FRAMES"); const std::string str = fr && fr[0] ? fr : "3100-3103"; int lo = 0, hi = 0; if (std::sscanf(str.c_str(), "%d-%d", &lo, &hi) == 2) x.r.push_back({ lo, hi });
            if (x.f) { std::fprintf(x.f, "# SH14: TAG:enter/TAG:exit rows (frame randCount stage TAG:enter args r0 callers | TAG:exit r0 r1 draws ret callers)\n"); std::fflush(x.f); }
        } else x.Init("NOTSA_SH14_CARGEN", "NOTSA_SH14_CARGEN_FRAMES", "3100-3103", "# frame randCount stage TAG:enter args r0 callers | TAG:exit r0 r1 draws ret callers");
        return x; }();
    return l.Get();
}
inline FILE* Sh14LsLog()  { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH14_LOADSCENE", "NOTSA_SH14_LOADSCENE_FRAMES", "3094", "# frame randCount stage P|X ... (see sh14log.h)"); return x; }(); return l.Get(); }
struct Sh14Scope {
    bool car; FILE* f; unsigned long long r0; const char* tag; char st[200]; long long ret = 0; bool hasRet = false;
    Sh14Scope(bool car_, const char* tag_, const char* args) : car(car_), f(car_ ? Sh14CarLog() : Sh14LsLog()), r0(0), tag(tag_) {
        st[0] = 0; if (!f) return; r0 = RandCountNow(); Sh12Stack(st, sizeof st, 1);
        std::fprintf(f, "%d %llu %s %s:enter %s r0=%llu callers=%s\n", LoadLogRecFrame(), r0, Sh12Stage(), tag, args ? args : "", r0, st); std::fflush(f);
    }
    void Ret(long long v) { ret = v; hasRet = true; }
    ~Sh14Scope() { if (!f) return; const unsigned long long r1 = RandCountNow(); std::fprintf(f, "%d %llu %s %s:exit r0=%llu r1=%llu draws=%llu ret=%lld callers=%s\n", LoadLogRecFrame(), r1, Sh12Stage(), tag, r0, r1, r1 - r0, hasRet ? ret : -9999, st); std::fflush(f); }
    Sh14Scope(const Sh14Scope&) = delete;
};
} // namespace notsa::rr
#define SH14_CAR(tag, ...) char sh14Args_[240]; if (notsa::rr::Sh14CarLog()) { std::snprintf(sh14Args_, sizeof sh14Args_, __VA_ARGS__); } else { sh14Args_[0] = 0; } notsa::rr::Sh14Scope sh14Scope_(true, tag, sh14Args_)
#define SH14_RET(v) sh14Scope_.Ret((long long)(v))
#define SH_RC14() ((unsigned long long)notsa::rr::RandCountNow())
#define SH14_LS_ON() (notsa::rr::Sh14LsLog() != nullptr)
#define SH14_LS(tag, ...) do { if (FILE* sh14f_ = notsa::rr::Sh14LsLog()) { char st_[200]; notsa::rr::Sh12Stack(st_, sizeof st_, 0); std::fprintf(sh14f_, "%d %llu %s %s ", notsa::rr::LoadLogRecFrame(), (unsigned long long)notsa::rr::RandCountNow(), notsa::rr::Sh12Stage(), tag); std::fprintf(sh14f_, __VA_ARGS__); std::fprintf(sh14f_, " callers=%s\n", st_); std::fflush(sh14f_); } } while (0)
#else
#define SH14_CAR(tag, ...) ((void)0)
#define SH14_RET(v) ((void)0)
#define SH_RC14() 0ull
#define SH14_LS_ON() false
#define SH14_LS(...) ((void)0)
#endif
