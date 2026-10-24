#pragma once

// NOTSA_STANDALONE_RUN: game-owned replacement of the CRT `rand`/`srand` of the original exe.
//
// The original CRT (0x821B1E rand / 0x821B11 srand) keeps the seed in the per-thread block (`_tiddata::_holdrand`, +0x14)
// and uses the MSVC LCG: `seed = seed * 214013 + 2531011; return (seed >> 16) & 0x7FFF`.
// The main thread's seed starts at 1, then the 19 `rand()` calls of the static initialisers (CHandShaker x6, CMenuManager)
// advance it (see .notes/INITTERM_AUDIT.md), so the value the game sees at `main` is LCG^19(1).
// The host CRT's rand() is per-thread (and not necessarily the same constants/state), so the standalone build routes
// every `rand()` / `srand()` of the game code through ONE state owned here.
#ifdef NOTSA_STANDALONE_RUN

#include <cstdlib> // must precede the macros below (declares ::rand(void) / ::srand(unsigned))
#include <stdlib.h>
#include <cstdint>
#include <source_location>

namespace notsa {
constexpr uint32_t RandStep(uint32_t seed) { return seed * 214013u + 2531011u; }
constexpr uint32_t InitialRandSeed() {
    uint32_t s = 1;
    for (int i = 0; i < 19; i++) { // initterm `rand()` calls, see above
        s = RandStep(s);
    }
    return s;
}

inline uint32_t g_RandSeed = InitialRandSeed();

inline uint32_t g_RandCalls = 0; // rand() calls since the last REPLAY-DIFF trace line (Lockstep.cpp)
// REPLAY-DIFF hook (Lockstep.cpp installs it only when NOTSA_LOCKSTEP_OUT is set; null = inert): may rewrite a srand() seed and logs the call with its source location
inline void (*g_SrandHook)(unsigned& seed, const char* file, unsigned line) = nullptr;

inline void (*g_RandHook)(const char* file, unsigned line) = nullptr; // REPLAY-DIFF pin bit 32 (per-call log "R fc file:line"); null = inert
inline int GameRand(const std::source_location loc = std::source_location::current()) {
    g_RandCalls++;
    if (g_RandHook) g_RandHook(loc.file_name(), loc.line());
    g_RandSeed = RandStep(g_RandSeed);
    return (int)((g_RandSeed >> 16) & 0x7FFF);
}
inline void GameSRand(unsigned seed, const std::source_location loc = std::source_location::current()) { if (g_SrandHook) g_SrandHook(seed, loc.file_name(), loc.line()); g_RandSeed = seed; }
} // namespace notsa

#define rand()       (::notsa::GameRand())
#define srand(seed)  (::notsa::GameSRand((unsigned)(seed)))

#endif
