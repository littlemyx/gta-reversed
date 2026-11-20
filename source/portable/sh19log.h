#pragma once
// SH19 RR debug log (STATEHASH builds only, no behaviour change: no rand draws, only pure reads). Rows: `frame randCount stage TAG ...`.
//   NOTSA_SH19_FNP=<file> NOTSA_SH19_FNP_FRAMES (3094-3105): every CPopCycle::FindNewPedType / CPopulation::ChooseCivilianOccupation / ChooseGangOccupation call:
//   F:* rows (inputs, chances before/after the rand scaling, each loop round, result), C:* rows (inputs, pass, every candidate model in ms_pedsLoaded order with its reason),
//   G:* rows (ChooseGangOccupation), rand counts before/after each draw.
#ifdef NOTSA_STATEHASH
#include "sh12log.h"
namespace notsa::rr {
inline FILE* Sh19Log() { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH19_FNP", "NOTSA_SH19_FNP_FRAMES", "3094-3105", "# frame randCount stage F:*|C:*|G:* ... (see sh19log.h)"); return x; }(); return l.Get(); }
} // namespace notsa::rr
#define SH19_ON() (notsa::rr::Sh19Log() != nullptr)
#define SH19_ROW(tag, ...) do { if (FILE* sh19f_ = notsa::rr::Sh19Log()) { std::fprintf(sh19f_, "%d %llu %s %s ", notsa::rr::LoadLogRecFrame(), (unsigned long long)notsa::rr::RandCountNow(), notsa::rr::Sh12Stage(), tag); std::fprintf(sh19f_, __VA_ARGS__); std::fprintf(sh19f_, "\n"); std::fflush(sh19f_); } } while (0)
#define SH19_RC() ((unsigned long long)notsa::rr::RandCountNow())
#else
#define SH19_ON() false
#define SH19_ROW(...) ((void)0)
#define SH19_RC() 0ull
#endif
