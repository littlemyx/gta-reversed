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
} // namespace notsa::rr
#define SH20_PAD(w) do { if (notsa::rr::Sh13PutLog()) notsa::rr::Sh20Pad(w); } while (0)
#define SH20_HOLD(t, p, ph) do { if (notsa::rr::Sh13PutLog()) notsa::rr::Sh20Hold(t, p, ph); } while (0)
#define SH20_EVENT(p) do { if (notsa::rr::Sh13PutLog()) notsa::rr::Sh20Event(p); } while (0)
#define SH20_BMX(w) do { if (notsa::rr::Sh20BmxLog()) notsa::rr::Sh20Bmx(w); } while (0)
#else
#define SH20_PAD(w) ((void)0)
#define SH20_HOLD(t, p, ph) ((void)0)
#define SH20_EVENT(p) ((void)0)
#define SH20_BMX(w) ((void)0)
#endif
