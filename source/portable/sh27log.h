#pragma once
// SH27 RR debug log (STATEHASH builds only, pure reads, no rand draws, no behaviour change).
//   NOTSA_SH27_RYDER=<file> NOTSA_SH27_RYDER_FRAMES (3000-3142): ped pool slot 3 (Ryder, model 290):
//   P row = ped state, A rows = every anim association (id group time/timeStep/speed/blend/blendDelta/flags), T row = task chain,
//   at frame end (where=endOfCGameProcess) and before/after every RpAnimBlendClumpUpdateAnimations of its clump (updPre/updPost: step, onScreen);
//   F rows = CPed::PlayFootSteps decision (walk assoc time/timeStep/min/max, branch, skip reason) and footstep audio / DoFootLanded calls (rand before/after).
#ifdef NOTSA_STATEHASH
#include "sh7log.h"
class CPed;
namespace notsa::rr {
inline FILE* Sh27Log() { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH27_RYDER", "NOTSA_SH27_RYDER_FRAMES", "3000-3142", "# frame randCount stage P|A|T|F where ... | P: ped state | A: anim id group blend delta time timeStep speed flags | T: task chains | F: footstep rows (see statehash.cpp Sh27*)"); return x; }(); return l.Get(); }
void Sh27Snap(const char* where, const char* extra);            // statehash.cpp
void Sh27Upd(void* clump, float step, bool onScreen, bool post);
void Sh27Foot(const class CPed* ped, const char* kind, float a, float b, float c, float d, int i, unsigned long long r0, unsigned long long r1);
bool Sh27IsRyder(const class CPed* ped);
struct Sh27UpdScope { void* c; float s; bool o; Sh27UpdScope(void* c_, float s_, bool o_) : c(c_), s(s_), o(o_) { if (Sh27Log()) Sh27Upd(c, s, o, false); } ~Sh27UpdScope() { if (Sh27Log()) Sh27Upd(c, s, o, true); } Sh27UpdScope(const Sh27UpdScope&) = delete; };
struct Sh27FootScope { const class CPed* p; const char* k; float a, b; int i; unsigned long long r0; bool on;
    Sh27FootScope(const class CPed* p_, const char* k_, float a_, float b_, int i_) : p(p_), k(k_), a(a_), b(b_), i(i_), r0(0), on(Sh27Log() && Sh27IsRyder(p_)) { if (on) r0 = RandCountNow(); }
    ~Sh27FootScope() { if (on) Sh27Foot(p, k, a, b, 0, 0, i, r0, RandCountNow()); } Sh27FootScope(const Sh27FootScope&) = delete; };
} // namespace notsa::rr
#define SH27_SNAP(w) do { if (notsa::rr::Sh27Log()) notsa::rr::Sh27Snap(w, ""); } while (0)
#define SH27_UPD(clump, step, os) notsa::rr::Sh27UpdScope sh27UpdScope_((void*)(clump), (step), (os))
#define SH27_FOOT(ped, kind, a, b, c, d, i) do { if (notsa::rr::Sh27Log() && notsa::rr::Sh27IsRyder(ped)) { const auto r_ = notsa::rr::RandCountNow(); notsa::rr::Sh27Foot(ped, kind, a, b, c, d, i, r_, r_); } } while (0)
#define SH27_FOOTSCOPE(ped, kind, a, b, i) notsa::rr::Sh27FootScope sh27Foot_##i(ped, kind, a, b, i)
#else
#define SH27_SNAP(w) ((void)0)
#define SH27_UPD(clump, step, os) ((void)0)
#define SH27_FOOT(ped, kind, a, b, c, d, i) ((void)0)
#define SH27_FOOTSCOPE(ped, kind, a, b, i) ((void)0)
#endif
