#pragma once
// SH13 RR debug logs (STATEHASH builds only, no behaviour change: no rand draws, only pure reads). Every row: `frame randCount stage TAG ...`, floats = raw bits / %.9g.
//   NOTSA_SH13_PED=<file>      NOTSA_SH13_PED_FRAMES (3084-3086): CJ matrix/rotations/heading at stage boundaries (after CWorld::Process, after TheCamera.Process, end of CGame::Process)
//   NOTSA_SH13_PUTDOWN=<file>  NOTSA_SH13_PUTDOWN_FRAMES (23005-23025): CJ anim associations (A rows) + task tree (T row: primary / secondary tasks with sub-task chains) at the end of CGame::Process
//   NOTSA_SH13_POP3094=<file>  NOTSA_SH13_POP3094_FRAMES (3094): C = every CPathFind::GeneratePedCreationCoors call from CPopulation (inputs, rand before/after, result, coords),
//                              G = CPopulation::GeneratePedsAtAttractors call (entry / exit rand count, caller RVAs), D = a CPedAttractor / CPedShelterAttractor draw site (rand before/after, caller RVAs)
//   NOTSA_SH13_VEHDUMP=<file>  NOTSA_SH13_VEHDUMP_FRAMES (0): every vehicle pool slot after CWorld::Process
//   NOTSA_SH13_POPCOUNT=<file> NOTSA_SH13_POPCOUNT_FRAMES (0): all CPopulation counters at each stage boundary where they differ from the previous row (+ first row of the frame), and at the function that changed them (caller RVAs)
#ifdef NOTSA_STATEHASH
#include "sh12log.h"
namespace notsa::rr {
inline FILE* Sh13PedLog()    { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH13_PED", "NOTSA_SH13_PED_FRAMES", "3084-3086", "# frame randCount stage H where | R right F fwd U up P pos (hex/%.9g) | rot=m_fCurrentRotation aim=m_fAimingRotation heading=GetHeading() headingExt=GetHeadingExt()"); return x; }(); return l.Get(); }
inline FILE* Sh13PutLog()    { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH13_PUTDOWN", "NOTSA_SH13_PUTDOWN_FRAMES", "23005-23025", "# frame randCount stage A|T where ... | A: animId group blend delta time speed flags hier | T: primary / secondary task chains (type ids)"); return x; }(); return l.Get(); }
inline FILE* Sh13PopLog()    { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH13_POP3094", "NOTSA_SH13_POP3094_FRAMES", "3094", "# frame randCount stage C|G|D ... (see sh13log.h)"); return x; }(); return l.Get(); }
inline FILE* Sh13VehLog()    { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH13_VEHDUMP", "NOTSA_SH13_VEHDUMP_FRAMES", "0", "# frame randCount stage V where slot model name | pos/move/turn (hex/%.9g) right fwd up | status"); return x; }(); return l.Get(); }
inline FILE* Sh13PopCntLog() { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH13_POPCOUNT", "NOTSA_SH13_POPCOUNT_FRAMES", "0", "# frame randCount stage N where changed=0|1 | civMale civFemale cop emergency dealers gang[0..9] totalPeds totalGang totalCiv totalCarPass totalMission countDownAtStart | callers"); return x; }(); return l.Get(); }
void Sh13Ped(const char* where);       // statehash.cpp
void Sh13Put(const char* where);
void Sh13VehDump(const char* where);
void Sh13PopPoll(const char* where, bool stack);
struct Sh13PopScope { const char* w; Sh13PopScope(const char* w_) : w(w_) { if (Sh13PopCntLog()) Sh13PopPoll(w_, false); } ~Sh13PopScope() { if (Sh13PopCntLog()) Sh13PopPoll(w, true); } Sh13PopScope(const Sh13PopScope&) = delete; };
struct Sh13AttrScope {
    bool on; unsigned long long r0; char tag[96]; char st[200];
    Sh13AttrScope(const char* what, const char* args) : on(Sh13PopLog() != nullptr), r0(0) {
        if (!on) return; r0 = RandCountNow(); std::snprintf(tag, sizeof tag, "%s", what); Sh12Stack(st, sizeof st, 1);
        if (FILE* f = Sh13PopLog()) { std::fprintf(f, "%d %llu %s %s %s r0=%llu callers=%s\n", LoadLogRecFrame(), r0, Sh12Stage(), tag[0] == 'G' ? "G:enter" : "D:enter", args, r0, st); std::fflush(f); }
    }
    ~Sh13AttrScope() { if (!on) return; if (FILE* f = Sh13PopLog()) { const unsigned long long r1 = RandCountNow(); std::fprintf(f, "%d %llu %s %s %s r0=%llu r1=%llu draws=%llu callers=%s\n", LoadLogRecFrame(), r1, Sh12Stage(), tag[0] == 'G' ? "G:exit" : "D:exit", tag, r0, r1, r1 - r0, st); std::fflush(f); } }
    Sh13AttrScope(const Sh13AttrScope&) = delete;
};
} // namespace notsa::rr
#define SH13_PED(w) do { if (notsa::rr::Sh13PedLog()) notsa::rr::Sh13Ped(w); } while (0)
#define SH13_PUT(w) do { if (notsa::rr::Sh13PutLog()) notsa::rr::Sh13Put(w); } while (0)
#define SH13_VEHDUMP(w) do { if (notsa::rr::Sh13VehLog()) notsa::rr::Sh13VehDump(w); } while (0)
#define SH13_POP(w) do { if (notsa::rr::Sh13PopCntLog()) notsa::rr::Sh13PopPoll(w, false); } while (0)
#define SH13_POPSCOPE(w) notsa::rr::Sh13PopScope sh13PopScope_(w)
#define SH13_ATTR(what, args) notsa::rr::Sh13AttrScope sh13AttrScope_(what, args)
#define SH13_RC() ((unsigned long long)notsa::rr::RandCountNow())
#define SH13_PCC_ON() (notsa::rr::Sh13PopLog() != nullptr)
#define SH13_PCC(r0, ok, ax, ay, d1, d2, d3, d4, apos, a1, a2, orient, low) do { if (FILE* sh13f_ = notsa::rr::Sh13PopLog()) { const auto r1_ = notsa::rr::RandCountNow(); char st_[200]; notsa::rr::Sh12Stack(st_, sizeof st_, 0); \
    std::fprintf(sh13f_, "%d %llu %s C in=%.9g,%.9g min1=%.9g max1=%.9g min2=%.9g max2=%.9g lowTraffic=%d r0=%llu r1=%llu draws=%llu ret=%d pos=%08x/%.9g,%08x/%.9g,%08x/%.9g orient=%.9g node1=%d/%d node2=%d/%d callers=%s\n", notsa::rr::LoadLogRecFrame(), (unsigned long long)r1_, notsa::rr::Sh12Stage(), \
    (double)(ax), (double)(ay), (double)(d1), (double)(d2), (double)(d3), (double)(d4), (int)(low), (unsigned long long)(r0), (unsigned long long)r1_, (unsigned long long)r1_ - (unsigned long long)(r0), (int)(ok), \
    notsa::rr::Sh7Bits((apos).x), (double)(apos).x, notsa::rr::Sh7Bits((apos).y), (double)(apos).y, notsa::rr::Sh7Bits((apos).z), (double)(apos).z, (double)(orient), (int)(a1).m_wAreaId, (int)(a1).m_wNodeId, (int)(a2).m_wAreaId, (int)(a2).m_wNodeId, st_); std::fflush(sh13f_); } } while (0)
#else
#define SH13_PED(w) ((void)0)
#define SH13_PUT(w) ((void)0)
#define SH13_VEHDUMP(w) ((void)0)
#define SH13_POP(w) ((void)0)
#define SH13_POPSCOPE(w) ((void)0)
#define SH13_ATTR(what, args) ((void)0)
#define SH13_RC() 0ull
#define SH13_PCC(...) ((void)0)
#endif
