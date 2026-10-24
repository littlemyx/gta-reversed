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
#include <string_view>
#include <utility>

namespace notsa::lockstep {
namespace {
constexpr uint32_t kFcGranule = 0x100000; // pin bit 16: at the arm point the frame counter jumps FORWARD to the next multiple of this (row 5 of the plan; the same constant in rd_orig.c)
struct Cfg {
    FILE*    f{};
    unsigned pin{};
    unsigned step{16};
    unsigned maxf{20000};
    unsigned rlog{0}; // pin bit 32: log every rand() call (R fc file:line) in the first `rlog` traced frames
    Cfg() {
        if (const char* o = std::getenv("NOTSA_LOCKSTEP_OUT")) { f = std::fopen(o, "w"); }
        if (const char* e = std::getenv("NOTSA_LOCKSTEP_PIN")) { pin = (unsigned)std::strtoul(e, nullptr, 0); }
        if (const char* e = std::getenv("NOTSA_LOCKSTEP_STEP")) { step = (unsigned)std::atoi(e); }
        if (const char* e = std::getenv("NOTSA_LOCKSTEP_MAXF")) { maxf = (unsigned)std::atoi(e); }
        if (const char* e = std::getenv("NOTSA_LOCKSTEP_RLOG")) { rlog = (unsigned)std::atoi(e); }
        if (f) { g_SrandHook = &OnSrand; if (rlog && (pin & 32)) { g_RandHook = &OnRand; } } // inert (hooks stay null) unless a trace file was requested
    }
};
Cfg& C() { static Cfg c; return c; }
unsigned g_nf; // traced frames so far
uint64_t g_anchor; bool g_anchorSet; bool g_started; bool g_frameRead; // g_frameRead: the next timer-function call is CTimer::Update's / CTimer::Initialise's
uint32_t Fnv(uint32_t h, const void* p, size_t n) { auto b = (const uint8_t*)p; while (n--) { h ^= *b++; h *= 16777619u; } return h; }
uint64_t RealQ() { return GetOSWPerformanceTime(); }
std::string_view Base(const char* path) { std::string_view p{path}; const auto i = p.find_last_of("/\\"); return i == p.npos ? p : p.substr(i + 1); }
const struct Install { Install() { C(); } } s_install; // reads the env at static-init time so the srand hook is in place before main
} // namespace

void FrameRead() { g_frameRead = true; }

void Tick() {
    auto& c = C();
    g_frameRead = true;
    unsigned& nf = g_nf;
    if (!c.f || nf >= c.maxf) return;
    if (!g_started) { // trace from the first frame with a player ped (game started): the ARM POINT
        if (!CWorld::Players[0].m_pPed) return;
        g_started = true;
        if (c.pin & 8) { g_RandSeed = 0x2468ACE1; } // pin bit 8: the loading screen / menu consume a real-time dependent number of rand() calls -> re-seed here
        if (c.pin & 16) { // pin bit 16: frame counter normalisation. The menu length differs per run, game logic uses fc % n. Jump FORWARD (stamps stay in the past) and shift the
            const uint32_t fc = CTimer::m_FrameCounter, to = (fc / kFcGranule + 1) * kFcGranule; // pinned clock anchor back by the same number of slots so the clock is continuous
            if (c.pin & 2) { g_anchor -= (uint64_t)(to - fc) * c.step * CTimer::m_snTimerDivider; }
            CTimer::m_FrameCounter = to;
        }
        g_RandCalls = 0;
    }
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
    if (!g_started) { // before the arm point the anchor is re-taken at every fc == 0 (a new game session restarts the counter)
        if (fc) { g_anchorSet = false; } else if (!g_anchorSet) { g_anchor = RealQ(); g_anchorSet = true; }
    }
    return g_anchor + (uint64_t)(fc + (frameRead ? 0 : 1)) * c.step * div;
}

bool PsTimer(uint32_t& out) {
    auto& c = C();
    if (!c.f || !(c.pin & 4) || !CTimer::m_FrameCounter) return false;
    out = 1000 + (uint32_t)CTimer::m_FrameCounter * c.step;
    return true;
}

// rand hook (GameRand.h), pin bit 32: one "R fc file:line" line per call during the first NOTSA_LOCKSTEP_RLOG traced frames (counterpart: RD_RLOG in rd_orig.c logs the return address)
void OnRand(const char* file, unsigned line) {
    auto& c = C();
    if (!c.f || !g_started || g_nf > c.rlog) return;
    const auto f = Base(file);
    std::fprintf(c.f, "R %u %.*s:%u\n", (unsigned)CTimer::m_FrameCounter, (int)f.size(), f.data(), line);
}

// srand hook (GameRand.h): pin bit 1 + the "S fc file:line seed" trace line (the original's S line carries the return address instead of file:line; rd_diff.py ignores that column)
void OnSrand(unsigned& seed, const char* file, unsigned line) {
    auto& c = C();
    if (!c.f) return;
    const auto f = Base(file);
    if ((c.pin & 1) && (f == "app_game.cpp" || f == "LoadingScreen.cpp")) seed = 0x13572468;
    if (g_started) std::fprintf(c.f, "S %u %.*s:%u %08x\n", (unsigned)CTimer::m_FrameCounter, (int)f.size(), f.data(), line, seed);
}
} // namespace notsa::lockstep
#endif
