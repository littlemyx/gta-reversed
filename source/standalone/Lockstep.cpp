#include "StdInc.h"
#ifdef NOTSA_STANDALONE_RUN
#include "Lockstep.h"
#include "GameRand.h"
#include "Timer.h"
#include "World.h"
#include "Pools/Pools.h"
#include "Scripts/TheScripts.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace notsa::lockstep {
namespace {
struct Cfg {
    FILE*    f{};
    unsigned pin{};
    unsigned step{16};
    unsigned maxf{20000};
    Cfg() {
        if (const char* o = std::getenv("NOTSA_LOCKSTEP_OUT")) { f = std::fopen(o, "w"); }
        if (const char* e = std::getenv("NOTSA_LOCKSTEP_PIN")) { pin = (unsigned)std::strtoul(e, nullptr, 0); }
        if (const char* e = std::getenv("NOTSA_LOCKSTEP_STEP")) { step = (unsigned)std::atoi(e); }
        if (const char* e = std::getenv("NOTSA_LOCKSTEP_MAXF")) { maxf = (unsigned)std::atoi(e); }
    }
};
Cfg& C() { static Cfg c; return c; }
uint64_t g_anchor; bool g_anchorSet; bool g_started; bool g_frameRead; // g_frameRead: the next timer-function call is CTimer::Update's / CTimer::Initialise's
uint32_t Fnv(uint32_t h, const void* p, size_t n) { auto b = (const uint8_t*)p; while (n--) { h ^= *b++; h *= 16777619u; } return h; }
uint64_t RealQ() { return GetOSWPerformanceTime(); }
} // namespace

void FrameRead() { g_frameRead = true; }

void Tick() {
    auto& c = C();
    g_frameRead = true;
    static unsigned nf;
    if (!c.f || nf >= c.maxf) return;
    if (!g_started) { if (!CWorld::Players[0].m_pPed) return; g_started = true; if (c.pin & 8) { g_RandSeed = 0x2468ACE1; } } // pin bit 8: RNG re-seeded at the first traced frame // trace from the first frame with a player ped (game started)
    nf++;
    float pos[3]{}; uint32_t hp = 0, np = 0, nv = 0;
    if (auto* ped = CWorld::Players[0].m_pPed) {
        const auto p = ped->GetPosition();
        pos[0] = p.x; pos[1] = p.y; pos[2] = p.z;
        std::memcpy(&hp, &ped->m_fHealth, 4);
    }
    if (auto* pool = GetPedPool()) np = (uint32_t)pool->GetNoOfUsedSpaces();
    if (auto* pool = GetVehiclePool()) nv = (uint32_t)pool->GetNoOfUsedSpaces();
    const uint32_t sh = Fnv(2166136261u, CTheScripts::ScriptSpace.data(), CTheScripts::ScriptSpace.size());
    uint32_t px, py, pz; std::memcpy(&px, &pos[0], 4); std::memcpy(&py, &pos[1], 4); std::memcpy(&pz, &pos[2], 4);
    std::fprintf(c.f, "F %u %u %08x %u %08x %08x %08x %u %u %u %08x\n", (unsigned)CTimer::m_FrameCounter, (unsigned)CTimer::m_snTimeInMilliseconds, (unsigned)g_RandSeed, (unsigned)g_RandCalls,
                 px, py, pz, hp, np, nv, sh);
    g_RandCalls = 0;
    if ((nf & 63) == 0) std::fflush(c.f);
}

bool PinTimer() { return C().f && (C().pin & 2); }

uint64_t TimerFn() { // same rules as my_timer() in tools/standalone/replay_diff/rd_orig.c
    auto& c = C();
    const uint32_t fc = CTimer::m_FrameCounter, div = CTimer::m_snTimerDivider;
    const bool frameRead = std::exchange(g_frameRead, false);
    if (fc) { g_anchorSet = false; } else if (!g_anchorSet) { g_anchor = RealQ(); g_anchorSet = true; }
    return g_anchor + (uint64_t)(fc + (frameRead ? 0 : 1)) * c.step * div;
}

bool PsTimer(uint32_t& out) {
    auto& c = C();
    if (!c.f || !(c.pin & 4) || !CTimer::m_FrameCounter) return false;
    out = 1000 + (uint32_t)CTimer::m_FrameCounter * c.step;
    return true;
}

void OnSrand(unsigned& seed, const char* file) {
    auto& c = C();
    if (!c.f) return;
    const std::string_view f{file};
    if ((c.pin & 1) && (f.ends_with("app_game.cpp") || f.ends_with("LoadingScreen.cpp"))) seed = 0x13572468;
    if (g_started) std::fprintf(c.f, "S %u %08x %08x\n", (unsigned)CTimer::m_FrameCounter, 0u, seed);
}
} // namespace notsa::lockstep
#endif
