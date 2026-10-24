#pragma once

// MAC-ANIM1 (.notes/reports/MAC-ANIM1.md): sector index helpers of CPhysical::Add / RemoveAndAdd. Kept in a header so that Physical.cpp keeps its line numbers (assert `__LINE__`
// immediates) and, without NOTSA_PORTABLE, its exact code: the Win32 branches are the previous expressions.
#ifdef NOTSA_PORTABLE
#include "Core/Fp.h"
#endif

namespace notsa {
//! `floor(v * 0.02f + 60.0f)` converted to an int (constants at 0x858B38 and 0x858B34).
inline int32 PhysicalSectorIdx(float v) {
#ifdef NOTSA_PORTABLE
    // The exe converts with floor (0x8219F0) + `_ftol2` (0x821B40): `fistp qword`, only the LOW dword is returned (see notsa::fp::Ftol). A plain `static_cast<int32>` is UB
    // for huge values and saturates on arm64 (INT_MIN..INT_MAX => ~2^32 loop iterations in Add / RemoveAndAdd). Huge values do occur, see PhysicalLimitSectorSpan.
    return notsa::fp::Ftol(std::floor(v * 0.02f + 60.0f));
#else
    return static_cast<int32>(std::floor(v * 0.02f + 60.0f));
#endif
}

//! Last sector index to visit for the span [minIdx, maxIdx] of `Add` / `RemoveAndAdd`. The repeat sectors are a 16x16 grid (`& 0xF`), so a span of more than 16 indices only
//! registers the same sectors again (every iteration allocates a CEntryInfoNode: pool of 500, and several cutscene objects are re-added per frame). No sane entity spans more than a
//! few sectors, but the cutscene IFPs (PROLOG1.IFP: csplay / csstew / cstaxi92 ...) keep uninitialised exporter memory in the LAST key frame of many sequences (translations of
//! 1e20..1e30, text like "bump_front_" / "D:\alienbrai"). That frame is evaluated when the anim reaches its end, UpdateCutsceneObjectBoundingBox gets a radius ~ sqrt(FLT_MAX), the bound
//! rect becomes +-1.8e19 and the loop allocates until the pool is empty (`new` => null => AV in RemoveAndAdd). The exe has the same data and the same loop (its result for such a rect
//! depends on the low dword of an x87 product), so this is a portable-only stability net: spans <= 16 sectors (everything real) are unchanged, a longer one collapses to its first sector.
inline int32 PhysicalLimitSectorSpan(int32 minIdx, int32 maxIdx) {
#ifdef NOTSA_PORTABLE
    return (int64)maxIdx - minIdx > 15 ? minIdx : maxIdx;
#else
    (void)minIdx;
    return maxIdx;
#endif
}
} // namespace notsa
