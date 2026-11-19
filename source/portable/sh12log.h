#pragma once
// SH12 RR debug logs (STATEHASH builds only, no behaviour change: no rand draws, only pure reads). Every row: `frame randCount stage TAG ...`, frame = recorded frame, stage = SH10 stage mark (NOTSA_SH10_STAGES must be on for the frame).
//   NOTSA_SH12_STREAM=<file>  NOTSA_SH12_STREAM_FRAMES (3080-3100): streaming log. Q = CStreaming::RequestModel call (id, name, kind, img, flags, state, 8 caller RVAs), R = channel read issued (chIdx, models, pos, sectors,
//                             big / retry), D = channel read finished (READING_SUCCESS), L = model converted (part 1; started=1 means a large model that needs a second part), F = FinishLoadingLargeFile (part 2),
//                             V = CStreaming::IsVeryBusy call (inputs, result, caller), S = per-frame state (begin / end of CStreaming::Update), I = CIplStore::RequestIplAndIgnore, A = LoadAllRequestedModels call
//   NOTSA_SH12_SAFEPOS=<file> NOTSA_SH12_SAFEPOS_FRAMES (0) NOTSA_SH12_SAFEPOS_SLOTS (3): vehicle probe in CWorld::Process: P = per pass / point, E = per entity call of the collision pass (before/after ProcessCollision)
#ifdef NOTSA_STATEHASH
#include "sh7log.h"
#include "sh10log.h"
namespace notsa::rr {
inline FILE* Sh12StreamLog() { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH12_STREAM", "NOTSA_SH12_STREAM_FRAMES", "3080-3100", "# frame randCount stage Q|R|D|L|F|V|S|I|A ... (see sh12log.h)"); return x; }(); return l.Get(); }
inline FILE* Sh12ScmLog() { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH12_SCM", "NOTSA_SH12_SCM_FRAMES", "3094", "# frame randCount T name base ip wake active mission ms | C name ip opcode (the command about to run, per thread, in CTheScripts::Process order)"); return x; }(); return l.Get(); }
template<class S> inline int Sh12ScmBegin(S* s) { if (FILE* f = Sh12ScmLog()) { std::fprintf(f, "%d %llu T %.8s base=%p ip=%#x wake=%d active=%d cleanup=%d ms=%u\n", LoadLogRecFrame(), (unsigned long long)RandCountNow(), (const char*)s->m_szName, (void*)s->m_BaseIP, (unsigned)(s->m_IP - s->m_BaseIP), (int)s->m_WakeTime, (int)s->m_IsActive, (int)s->m_UsesMissionCleanup, (unsigned)CTimer::GetTimeInMS()); } return 0; }
template<class S> inline int Sh12ScmCmd(S* s) { if (FILE* f = Sh12ScmLog()) { std::fprintf(f, "%d %llu C %.8s ip=%#x op=%#x\n", LoadLogRecFrame(), (unsigned long long)RandCountNow(), (const char*)s->m_szName, (unsigned)(s->m_IP - s->m_BaseIP), (unsigned)*(const uint16_t*)s->m_IP); } return 0; }
void Sh12Name(int id, char* buf, size_t n);          // statehash.cpp: `kind#rel name`
void Sh12Stack(char* buf, size_t n, int skip);       // statehash.cpp: RVAs (hex, ',' separated) of the caller chain
void Sh12StreamState(const char* when);              // statehash.cpp: CStreaming counters + channels
void Sh12SafePos(const char* tag, const void* ent);  // statehash.cpp: entity probe (slots from env)
void Sh12SafePosAll(const char* tag);                // statehash.cpp: all slots from env
void Sh12SetPass(const char* pass);
inline const char* Sh12Stage() { const char* s = Sh10Cur().name; return s ? s : "-"; }
} // namespace notsa::rr
#define SH12_ROW(...) do { if (FILE* sh12f_ = notsa::rr::Sh12StreamLog()) { std::fprintf(sh12f_, "%d %llu %s ", notsa::rr::LoadLogRecFrame(), (unsigned long long)notsa::rr::RandCountNow(), notsa::rr::Sh12Stage()); std::fprintf(sh12f_, __VA_ARGS__); std::fputc(10, sh12f_); std::fflush(sh12f_); } } while (0)
#define SH12_SCM_BEGIN(s) notsa::rr::Sh12ScmBegin(s)
#define SH12_SCM_CMD(s) notsa::rr::Sh12ScmCmd(s)
#define SH12_ON() (notsa::rr::Sh12StreamLog() != nullptr)
#define SH12_STATE(w) do { if (SH12_ON()) notsa::rr::Sh12StreamState(w); } while (0)
#define SH12_SAFE(tag, e) notsa::rr::Sh12SafePos(tag, e)
#define SH12_SAFEALL(tag) notsa::rr::Sh12SafePosAll(tag)
#define SH12_PASS(p) notsa::rr::Sh12SetPass(p)
#else
#define SH12_ROW(...) ((void)0)
#define SH12_SCM_BEGIN(s) 0
#define SH12_SCM_CMD(s) 0
#define SH12_ON() (false)
#define SH12_STATE(w) ((void)0)
#define SH12_SAFE(tag, e) ((void)0)
#define SH12_SAFEALL(tag) ((void)0)
#define SH12_PASS(p) ((void)0)
#endif
