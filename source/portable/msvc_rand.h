#pragma once
/*
 * MSVC CRT `rand` / `srand` / `RAND_MAX` for NOTSA_PORTABLE builds (P4-S1).
 *
 * The exe uses the MSVC CRT (VS2003-2008 static CRT, `_rand` @ 0x821B1E, `_srand` @ 0x821B11): 15-bit LCG, RAND_MAX 0x7FFF.
 *     holdrand = holdrand * 214013 + 2531011;  return (holdrand >> 16) & 0x7FFF;      srand(s): holdrand = s;
 * Any other `rand()` (macOS: 31 bit, RAND_MAX 0x7FFFFFFF; glibc: 31 bit) silently breaks the game: `CGeneral::GetRandomNumber()` returns uint16,
 * `RAND_MAX_FLOAT_RECIPROCAL = 1/RAND_MAX` (General.h) is ~0 for ~35 float rolls, `GetRandomNumber() >= RAND_MAX / 2` never holds, ...
 *
 * State = PER THREAD, exactly like the exe: the CRT keeps `_holdrand` in the per-thread block `_tiddata` (+0x14; `_rand` starts with `__getptd()`),
 * and `_initptd` (0x827D25 `_mtinit`/`_initptd`) sets it to 1 for every new thread (.notes/INITTERM_AUDIT.md). So a worker thread that calls rand() sees
 * 41, 18467, 6334, ... regardless of what the main thread did. The main thread additionally had 19 `rand()` calls in the C++ static initialisers before
 * `main` (CHandShaker x6, CMenuManager; INITTERM_AUDIT.md), so its first game-visible seed is LCG^19(1) (same as the Windows standalone `GameRand.h`).
 * "Main thread" = the thread that ran the static initialisers (the first thread that touches this header's state; `g_captureMainThread` below makes that
 * happen before main()).
 *
 * Mechanism: `RAND_MAX` is redefined and `rand()` / `srand(x)` are function-like macros (the game never takes their address, never calls `std::rand`; checked
 * by tests/portable/msvc_rand_test.cpp `scan`). A function-like macro can only be applied to calls, so declarations in system headers stay intact.
 * Include this AFTER <cstdlib>/<stdlib.h>: StdInc.h -> standalone/GameRand.h, General.h, portable/crt.h all include it (idempotent).
 *
 * MSVC x86 builds never see this file (gated on NOTSA_PORTABLE; crt.h is !_WIN32 only).
 */
#if defined(NOTSA_PORTABLE) || defined(NOTSA_MSVC_RAND_HOSTTEST)

#include <cstdint>
#include <cstdlib> // must precede the macros below (declares ::rand(void) / ::srand(unsigned) and the host RAND_MAX)
#include <stdlib.h>
#include <thread>

namespace notsa::msvc_crt {
constexpr uint32_t RandStep(uint32_t holdrand) { return holdrand * 214013u + 2531011u; }

//! Seed of the main thread when the game starts: LCG^19(1) (the 19 static-initialiser `rand()` calls of the exe)
constexpr uint32_t MainThreadInitialSeed() {
    uint32_t s = 1;
    for (int i = 0; i < 19; i++) {
        s = RandStep(s);
    }
    return s;
}

//! Thread that ran the static initialisers (captured the first time anybody asks)
inline std::thread::id MainThreadId() {
    static const std::thread::id id = std::this_thread::get_id();
    return id;
}
[[maybe_unused]] inline const bool g_captureMainThread = (MainThreadId(), true); // runs in the static-init thread, before main()

//! `_tiddata::_holdrand`
inline uint32_t& HoldRand() {
    thread_local uint32_t holdrand = (std::this_thread::get_id() == MainThreadId()) ? MainThreadInitialSeed() : 1u; // _initptd: 1
    return holdrand;
}

inline int Rand() {
    auto& h = HoldRand();
    h       = RandStep(h);
    return (int)((h >> 16) & 0x7FFF);
}
inline void SRand(unsigned seed) { HoldRand() = seed; }

//! Test hooks: pure functions on an explicit state (no thread-local), used by the golden test
inline int  RandOn(uint32_t& holdrand) { holdrand = RandStep(holdrand); return (int)((holdrand >> 16) & 0x7FFF); }
} // namespace notsa::msvc_crt

#undef  RAND_MAX
#define RAND_MAX     0x7FFF
#undef  rand
#undef  srand
#define rand()       (::notsa::msvc_crt::Rand())
#define srand(seed)  (::notsa::msvc_crt::SRand((unsigned)(seed)))

#endif
