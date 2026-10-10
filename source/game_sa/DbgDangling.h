#pragma once
// Wine/DLL-run diagnostics for dangling-pointer hunts (NOTSA_INPUT_INJECT only; compiled out otherwise).
#ifdef NOTSA_INPUT_INJECT
#include <array>
#include <cstdint>
#include <string>

namespace dbg3 {
inline constexpr size_t kRing = 512;
struct Resolved { const void* p; uint32 frame; int type; int model; };
inline std::array<Resolved, kRing> g_resolved{};
inline size_t g_resolvedN = 0;
inline void NoteResolved(const CEntity* e) {
    g_resolved[g_resolvedN++ % kRing] = { e, CTimer::GetFrameCounter(), (int)e->GetType(), (int)e->m_nModelIndex };
}
inline const Resolved* FindResolved(const void* p) {
    for (size_t i = 0; i < std::min(g_resolvedN, kRing); ++i) {
        const auto& r = g_resolved[(g_resolvedN - 1 - i) % kRing];
        if (r.p == p) return &r;
    }
    return nullptr;
}

struct SndShadow { const void* owner; uint32 vtbl; std::string cls; uint32 frame; uint32 ms; };
inline std::array<SndShadow, 300> g_sndShadow{};
struct EvShadow { const CEntity* e; int type; int etype; int model; uint32 frame; uint32 timeMs; };
inline std::array<EvShadow, 8> g_evShadow{};

// Which pool/entity contains `p` (and at which offset)? Returns a description.
std::string Classify(const void* p);
void PeriodicReport(); // CReferences / enex usage, once per ~10 s of game time
void CheckEvents();    // every frame: report an interesting event whose entity is already freed
}
#endif
