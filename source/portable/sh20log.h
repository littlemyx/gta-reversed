#pragma once
// SH20 RR debug logs (STATEHASH builds only, no behaviour change: pure reads, no rand draws).
//   R1: extra rows (P pad, H hold-entity, E event group, S put-down start) written into the SH13 PUTDOWN log (NOTSA_SH13_PUTDOWN / _FRAMES), next to its A/T rows.
//   R2: NOTSA_SH20_BMX=<file> NOTSA_SH20_BMX_FRAMES (0): bike slot 2 (wheel ratios, ratio history, fake physics, status, move speed) and the matrix of vehicle slot 4, at the start and end of the frame.
#ifdef NOTSA_STATEHASH
#include "sh13log.h"
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
} // namespace notsa::rr
#define SH20_PAD(w) do { if (notsa::rr::Sh13PutLog()) notsa::rr::Sh20Pad(w); } while (0)
#define SH20_HOLD(t, p, ph) do { if (notsa::rr::Sh13PutLog()) notsa::rr::Sh20Hold(t, p, ph); } while (0)
#define SH20_EVENT(p) do { if (notsa::rr::Sh13PutLog()) notsa::rr::Sh20Event(p); } while (0)
#define SH20_BMX(w) do { if (notsa::rr::Sh20BmxLog()) notsa::rr::Sh20Bmx(w); } while (0)
#define SH21_Z(self, w, x) do { if (notsa::rr::Sh21BmxPcLog()) notsa::rr::Sh21Bmx(self, w, x); } while (0)
#define SH21_SCOPE(self, w) notsa::rr::Sh21Scope sh21Scope_(self, w)
#define SH21_VALS(self, w, a, b, c, d) do { if (notsa::rr::Sh21BmxPcLog()) notsa::rr::Sh21Vals(self, w, a, b, c, d); } while (0)
#else
#define SH20_PAD(w) ((void)0)
#define SH20_HOLD(t, p, ph) ((void)0)
#define SH20_EVENT(p) ((void)0)
#define SH20_BMX(w) ((void)0)
#define SH21_Z(self, w, x) ((void)0)
#define SH21_SCOPE(self, w) ((void)0)
#define SH21_VALS(self, w, a, b, c, d) ((void)0)
#endif
