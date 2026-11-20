#pragma once
// SH18 RR debug logs (STATEHASH builds only, no behaviour change: no rand draws, only pure reads). Rows: `frame randCount stage TAG ...`.
//   NOTSA_SH18_POP=<file> NOTSA_SH18_POP_FRAMES (3094-3095): CPopulation::Update steps. U:start / U:gate rows = operands of every early return and of the GeneratePedsAtAttractors call condition;
//                         S:<step>:enter/exit scopes (r0 r1 draws ret callers) for ManagePopulation, RemovePedsIfThePoolGetsFull, UpdatePedCounts, AddToPopulation (its return value IS the gate), GeneratePedsAtAttractors;
//                         A:* rows inside AddToPopulation (reason of each return + operands: totals, targetNumPeds, createCop, pedType, ...)
//   NOTSA_SH18_EE=<file>  NOTSA_SH18_EE_FRAMES (3094): E = every CEntryExit constructor (ordinal, name, entrance centre, exit pos, flags, burglaryAccess, unknownBurglary, on/off, roll draw value, coin result, rc before/after, callers); I = pool index returned by CEntryExitManager::AddOne
#ifdef NOTSA_STATEHASH
#include "sh12log.h"
namespace notsa::rr {
inline FILE* Sh18PopLog() { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH18_POP", "NOTSA_SH18_POP_FRAMES", "3094-3095", "# frame randCount stage U:*|S:step:enter/exit|A:* ... (see sh18log.h)"); return x; }(); return l.Get(); }
inline FILE* Sh18EeLog()  { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH18_EE", "NOTSA_SH18_EE_FRAMES", "3094", "# frame randCount stage E|I ... (see sh18log.h)"); return x; }(); return l.Get(); }
struct Sh18Scope {
    FILE* f; unsigned long long r0; const char* tag; char st[200]; long long ret = 0; bool hasRet = false;
    Sh18Scope(const char* tag_, const char* args) : f(Sh18PopLog()), r0(0), tag(tag_) {
        st[0] = 0; if (!f) return; r0 = RandCountNow(); Sh12Stack(st, sizeof st, 1);
        std::fprintf(f, "%d %llu %s S:%s:enter %s r0=%llu callers=%s\n", LoadLogRecFrame(), r0, Sh12Stage(), tag, args ? args : "", r0, st); std::fflush(f);
    }
    void Ret(long long v) { ret = v; hasRet = true; }
    ~Sh18Scope() { if (!f) return; const unsigned long long r1 = RandCountNow(); std::fprintf(f, "%d %llu %s S:%s:exit r0=%llu r1=%llu draws=%llu ret=%lld\n", LoadLogRecFrame(), r1, Sh12Stage(), tag, r0, r1, r1 - r0, hasRet ? ret : -9999); std::fflush(f); }
    Sh18Scope(const Sh18Scope&) = delete;
};
} // namespace notsa::rr
#define SH18_ON() (notsa::rr::Sh18PopLog() != nullptr)
#define SH18_ROW(tag, ...) do { if (FILE* sh18f_ = notsa::rr::Sh18PopLog()) { std::fprintf(sh18f_, "%d %llu %s %s ", notsa::rr::LoadLogRecFrame(), (unsigned long long)notsa::rr::RandCountNow(), notsa::rr::Sh12Stage(), tag); std::fprintf(sh18f_, __VA_ARGS__); std::fprintf(sh18f_, "\n"); std::fflush(sh18f_); } } while (0)
#define SH18_SCOPE(var, tag, ...) char var##Args_[240]; if (notsa::rr::Sh18PopLog()) { std::snprintf(var##Args_, sizeof var##Args_, __VA_ARGS__); } else { var##Args_[0] = 0; } notsa::rr::Sh18Scope var(tag, var##Args_)
#define SH18_RET(var, v) var.Ret((long long)(v))
#define SH18_EE_ON() (notsa::rr::Sh18EeLog() != nullptr)
#define SH18_EE(tag, ...) do { if (FILE* sh18f_ = notsa::rr::Sh18EeLog()) { char st_[200]; notsa::rr::Sh12Stack(st_, sizeof st_, 0); std::fprintf(sh18f_, "%d %llu %s %s ", notsa::rr::LoadLogRecFrame(), (unsigned long long)notsa::rr::RandCountNow(), notsa::rr::Sh12Stage(), tag); std::fprintf(sh18f_, __VA_ARGS__); std::fprintf(sh18f_, " callers=%s\n", st_); std::fflush(sh18f_); } } while (0)
#define SH18_RC() ((unsigned long long)notsa::rr::RandCountNow())
#else
#define SH18_ON() false
#define SH18_ROW(...) ((void)0)
#define SH18_SCOPE(...) ((void)0)
#define SH18_RET(var, v) ((void)0)
#define SH18_EE_ON() false
#define SH18_EE(...) ((void)0)
#define SH18_RC() 0ull
#endif
