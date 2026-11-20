#pragma once
// SH20 RR debug logs (STATEHASH builds only, no behaviour change: pure reads, no rand draws).
//   R1: extra rows (P pad, H hold-entity, E event group, S put-down start) written into the SH13 PUTDOWN log (NOTSA_SH13_PUTDOWN / _FRAMES), next to its A/T rows.
//   R2: NOTSA_SH20_BMX=<file> NOTSA_SH20_BMX_FRAMES (0): bike slot 2 (wheel ratios, ratio history, fake physics, status, move speed) and the matrix of vehicle slot 4, at the start and end of the frame.
#ifdef NOTSA_STATEHASH
#include "sh13log.h"
class CTaskComplexGotoDoorAndOpen; class CTaskSimpleGoToPoint; // SH24
namespace notsa::rr {
inline FILE* Sh20BmxLog() { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH20_BMX", "NOTSA_SH20_BMX_FRAMES", "0", "# frame randCount stage B|M where ... (see statehash.cpp Sh20Bmx)"); return x; }(); return l.Get(); }
void Sh20Pad(const char* where);                              // statehash.cpp
void Sh20Hold(const class CTaskSimpleHoldEntity* t, const class CPed* ped, int phase);
void Sh20Event(const class CPed* ped);
void Sh20Bmx(const char* where);
// SH21 R2b: NOTSA_SH21_BMXPC=<file> NOTSA_SH21_BMXPC_FRAMES (0-1): Z rows = moveSpeed/turnSpeed (+bits), fakePhysics, colProcessed, status of vehicle pool slot 2 at marked points of CBike::ProcessControl / CPhysical::ProcessCollision / ApplyMoveSpeed / ApplyTurnSpeed
inline FILE* Sh21BmxPcLog() { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH21_BMXPC", "NOTSA_SH21_BMXPC_FRAMES", "0-1", "# frame randCount stage Z where | slot2 move= turn= (hex/%.9g) fake col status"); return x; }(); return l.Get(); }
void Sh21Bmx(const class CPhysical* self, const char* where, int extra);
void Sh21Vals(const class CPhysical* self, const char* where, float a, float b, float c, float d);
struct Sh21Scope { const class CPhysical* s; const char* w; Sh21Scope(const class CPhysical* s_, const char* w_) : s(s_), w(w_) { if (Sh21BmxPcLog()) Sh21Bmx(s, w, 0); } ~Sh21Scope() { if (Sh21BmxPcLog()) Sh21Bmx(s, w, 1); } Sh21Scope(const Sh21Scope&) = delete; };
// SH22 (STATEHASH-only, pure reads): R3 NOTSA_SH22_PICK=<file>: every pickup object create (C) / free (F) / unhooked change seen by a scan (X) from boot (frame -1 rows too) to frame 4100, + the pickup list at the first CPickups::Update of frame 0 (L rows)
//   R4 NOTSA_SH22_AVGN=<file>: bike slot 2 m_vecAveGroundNormal at marked points of CBike::ProcessControl, frames <= 2 (boot rows have frame -1)
//   R5 NOTSA_SH22_IDLE=<file>: CJ anim (group 69, id 3) per-frame row (I) in 20000..22550 and event rows (E: Start/SetBlend/SetBlendTo/dtor of any assoc of that group+id, with its pointer) from frame 0
inline FILE* Sh22Open(const char* env) { const char* p = std::getenv(env); FILE* f = p && p[0] ? std::fopen(p, "w") : nullptr; if (f) { std::fprintf(f, "# SH22 %s frame fc randCount ...\n", env); std::fflush(f); } return f; }
inline FILE* Sh22PickLog() { static FILE* f = Sh22Open("NOTSA_SH22_PICK"); const int fr = LoadLogRecFrame(); return (f && fr <= 4100) ? f : nullptr; }
inline FILE* Sh22AvgnLog() { static FILE* f = Sh22Open("NOTSA_SH22_AVGN"); const int fr = LoadLogRecFrame(); return (f && fr <= 2) ? f : nullptr; }
inline FILE* Sh22IdleFile() { static FILE* f = Sh22Open("NOTSA_SH22_IDLE"); return f; }
void Sh22Pick(const char* ev, const class CPickup* p, const class CObject* obj);
void Sh22PickScan(const char* where);
void Sh22Avgn(const class CBike* b, const char* where);
void Sh22IdleFrame();
void Sh22IdleEvent(const char* ev, const class CAnimBlendAssociation* a);
struct Sh22PickScope { const class CPickup* p; class CObject*& o; Sh22PickScope(const class CPickup* p_, class CObject*& o_) : p(p_), o(o_) {} ~Sh22PickScope() { if (Sh22PickLog()) Sh22Pick("C", p, o); } Sh22PickScope(const Sh22PickScope&) = delete; };
// SH23 (STATEHASH-only, pure reads): R6 NOTSA_SH23_BOOTPICK=<file>: every CPickups::GenerateNewOne (script CREATE_PICKUP/_WITH_AMMO/CREATE_MONEY_PICKUP, IPL, save, weapon drops...) at fc <= 300 or frame 0: model/type/slot/pos/ammo, script thread+ip when from a script, else C++ caller stack
//   R7 NOTSA_SH23_CJPOS=<file>: CJ position (+bits), heading, move state, primary task, move speed per frame 19313..19454
inline FILE* Sh23Open(const char* env) { const char* p = std::getenv(env); FILE* f = p && p[0] ? std::fopen(p, "w") : nullptr; if (f) { std::fprintf(f, "# SH23 %s frame fc randCount ...\n", env); std::fflush(f); } return f; }
inline FILE* Sh23PickLog() { static FILE* f = Sh23Open("NOTSA_SH23_BOOTPICK"); return f; }
inline FILE* Sh23CjLog() { static FILE* f = Sh23Open("NOTSA_SH23_CJPOS"); const int fr = LoadLogRecFrame(); return (f && fr >= 19313 && fr <= 19454) ? f : nullptr; }
inline const void*& Sh23ScriptHint() { static const void* s = nullptr; return s; }
void Sh23Pick(unsigned modelId, int type, unsigned ammo, unsigned moneyPerDay, bool isEmpty, float x, float y, float z, int slot, unsigned ret);
void Sh23Cj();
// SH24 (STATEHASH-only, pure reads): R8 NOTSA_SH24_COUNTDOWN=<file>: every write of CPopulation::m_CountDownToPedsAtStart boot..3140 with caller stack, V row per frame, GeneratePedsAtStartOfGame call
//   R9 NOTSA_SH24_DOOR=<file>: enex door task (FindNearestDoor candidates, ctor, CreateFirstSubTask, GoToPoint aim intermediates, per-frame CJ task chain) frames 19313..19360
inline FILE* Sh24Open(const char* env) { const char* p = std::getenv(env); FILE* f = p && p[0] ? std::fopen(p, "w") : nullptr; if (f) { std::fprintf(f, "# SH24 %s frame fc randCount stage ...\n", env); std::fflush(f); } return f; }
inline FILE* Sh24CdLog() { static FILE* f = Sh24Open("NOTSA_SH24_COUNTDOWN"); return (f && LoadLogRecFrame() <= 3140) ? f : nullptr; }
inline FILE* Sh24DoorLog() { static FILE* f = Sh24Open("NOTSA_SH24_DOOR"); const int fr = LoadLogRecFrame(); return (f && fr >= 19313 && fr <= 19360) ? f : nullptr; }
void Sh24Cd(const char* where, int oldv, int newv);
void Sh24CdFrame();
void Sh24ObjDesc(char* b, size_t n, const class CObject* o);
void Sh24Door(const char* ev, const class CTaskComplexGotoDoorAndOpen* t, const class CPed* ped);
void Sh24DoorFrame();
void Sh24Goto(const class CTaskSimpleGoToPoint* t, const class CPed* ped, float dx, float dy, float nx, float ny, bool aimSet);
} // namespace notsa::rr
#define SH24_DROW(...) do { if (FILE* sh24f_ = notsa::rr::Sh24DoorLog()) { std::fprintf(sh24f_, "%d %u %llu %s ", notsa::rr::LoadLogRecFrame(), (unsigned)CTimer::GetFrameCounter(), (unsigned long long)notsa::rr::RandCountNow(), notsa::rr::Sh12Stage()); std::fprintf(sh24f_, __VA_ARGS__); std::fputc(10, sh24f_); std::fflush(sh24f_); } } while (0)
#define SH24_CD(w, o, n) do { if (notsa::rr::Sh24CdLog()) notsa::rr::Sh24Cd(w, o, n); } while (0)
#define SH24_CDFRAME() do { if (notsa::rr::Sh24CdLog()) notsa::rr::Sh24CdFrame(); } while (0)
#define SH24_DOOR(ev, t, ped) do { if (notsa::rr::Sh24DoorLog()) notsa::rr::Sh24Door(ev, t, ped); } while (0)
#define SH24_DOORFRAME() do { if (notsa::rr::Sh24DoorLog()) notsa::rr::Sh24DoorFrame(); } while (0)
#define SH24_GOTO(t, ped, dx, dy, nx, ny, a) do { if (notsa::rr::Sh24DoorLog()) notsa::rr::Sh24Goto(t, ped, dx, dy, nx, ny, a); } while (0)
#define SH20_PAD(w) do { if (notsa::rr::Sh13PutLog()) notsa::rr::Sh20Pad(w); } while (0)
#define SH20_HOLD(t, p, ph) do { if (notsa::rr::Sh13PutLog()) notsa::rr::Sh20Hold(t, p, ph); } while (0)
#define SH20_EVENT(p) do { if (notsa::rr::Sh13PutLog()) notsa::rr::Sh20Event(p); } while (0)
#define SH20_BMX(w) do { if (notsa::rr::Sh20BmxLog()) notsa::rr::Sh20Bmx(w); } while (0)
#define SH21_Z(self, w, x) do { if (notsa::rr::Sh21BmxPcLog()) notsa::rr::Sh21Bmx(self, w, x); } while (0)
#define SH21_SCOPE(self, w) notsa::rr::Sh21Scope sh21Scope_(self, w)
#define SH21_VALS(self, w, a, b, c, d) do { if (notsa::rr::Sh21BmxPcLog()) notsa::rr::Sh21Vals(self, w, a, b, c, d); } while (0)
#define SH22_PICK(ev, p, o) do { if (notsa::rr::Sh22PickLog()) notsa::rr::Sh22Pick(ev, p, o); } while (0)
#define SH22_PICKSCOPE(p, o) notsa::rr::Sh22PickScope sh22PickScope_(p, o)
#define SH22_PICKSCAN(w) do { if (notsa::rr::Sh22PickLog()) notsa::rr::Sh22PickScan(w); } while (0)
#define SH22_AVGN(b, w) do { if (notsa::rr::Sh22AvgnLog()) notsa::rr::Sh22Avgn(b, w); } while (0)
#define SH22_IDLEFRAME() do { if (notsa::rr::Sh22IdleFile()) notsa::rr::Sh22IdleFrame(); } while (0)
#define SH22_IDLEEVENT(ev, a) do { if (notsa::rr::Sh22IdleFile()) notsa::rr::Sh22IdleEvent(ev, a); } while (0)
#define SH23_SCRIPT(S) do { if (notsa::rr::Sh23PickLog()) notsa::rr::Sh23ScriptHint() = &(S); } while (0)
#define SH23_PICK(m, t, a, mpd, e, pos, slot, ret) do { if (notsa::rr::Sh23PickLog()) notsa::rr::Sh23Pick(m, t, a, mpd, e, (pos).x, (pos).y, (pos).z, slot, ret); } while (0)
#define SH23_CJ() do { if (notsa::rr::Sh23CjLog()) notsa::rr::Sh23Cj(); } while (0)
#else
#define SH24_DROW(...) ((void)0)
#define SH24_CD(w, o, n) ((void)0)
#define SH24_CDFRAME() ((void)0)
#define SH24_DOOR(ev, t, ped) ((void)0)
#define SH24_DOORFRAME() ((void)0)
#define SH24_GOTO(t, ped, dx, dy, nx, ny, a) ((void)0)
#define SH20_PAD(w) ((void)0)
#define SH20_HOLD(t, p, ph) ((void)0)
#define SH20_EVENT(p) ((void)0)
#define SH20_BMX(w) ((void)0)
#define SH21_Z(self, w, x) ((void)0)
#define SH21_SCOPE(self, w) ((void)0)
#define SH21_VALS(self, w, a, b, c, d) ((void)0)
#define SH22_PICK(ev, p, o) ((void)0)
#define SH22_PICKSCOPE(p, o) ((void)0)
#define SH22_PICKSCAN(w) ((void)0)
#define SH22_AVGN(b, w) ((void)0)
#define SH22_IDLEFRAME() ((void)0)
#define SH22_IDLEEVENT(ev, a) ((void)0)
#define SH23_SCRIPT(S) ((void)0)
#define SH23_PICK(m, t, a, mpd, e, pos, slot, ret) ((void)0)
#define SH23_CJ() ((void)0)
#endif
