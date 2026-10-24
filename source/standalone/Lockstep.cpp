#include "StdInc.h"
#ifdef NOTSA_STANDALONE_RUN
#include "Lockstep.h"
#include "GameRand.h"
#include "Timer.h"
#include "World.h"
#include "Pools/Pools.h"
#include "Scripts/TheScripts.h"
#include "CarCtrl.h"
#include "PopCycle.h"
#include "CutsceneMgr.h"
#include "Pickups.h"
#include "Radar.h"
#include "Streaming.h"
#include "PathFind.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <utility>
#include <vector>

bool CdStreamAllIdle(); // CdStreamInfo.cpp
namespace notsa::lockstep { const void* g_Outer; } // set by CGeneral::RandomBool / DoCoinFlip (General.cpp): their caller, printed as "o+rva" in the R line

namespace notsa::lockstep {
namespace {
constexpr uint32_t kFcGranule = 0x100000; // pin bit 16: at the arm point the frame counter jumps FORWARD to the next multiple of this (row 5 of the plan; the same constant in rd_orig.c)
struct Cfg {
    FILE*    f{};
    unsigned pin{};
    unsigned step{16};
    unsigned maxf{20000};
    unsigned rstack{0};
    unsigned rfrom{0}; // NOTSA_LOCKSTEP_RFROM=<n>: R lines only from the n-th traced frame on (with RLOG = the last frame)
    std::vector<std::pair<uintptr_t, uintptr_t>> audioRva;
    unsigned rlog{0}; // pin bit 32: log every rand() call (R fc file:line) in the first `rlog` traced frames
    Cfg() {
        if (const char* o = std::getenv("NOTSA_LOCKSTEP_OUT")) { f = std::fopen(o, "w"); }
        if (const char* e = std::getenv("NOTSA_LOCKSTEP_PIN")) { pin = (unsigned)std::strtoul(e, nullptr, 0); }
        if (const char* e = std::getenv("NOTSA_LOCKSTEP_STEP")) { step = (unsigned)std::atoi(e); }
        if (const char* e = std::getenv("NOTSA_LOCKSTEP_MAXF")) { maxf = (unsigned)std::atoi(e); }
        if (const char* e = std::getenv("NOTSA_LOCKSTEP_RSTACK")) { rstack = (unsigned)std::atoi(e); }
        if (const char* e = std::getenv("NOTSA_LOCKSTEP_RFROM")) { rfrom = (unsigned)std::atoi(e); }
        if (const char* e = std::getenv("NOTSA_LOCKSTEP_RLOG")) { rlog = (unsigned)std::atoi(e); }
        if (const char* e = std::getenv("NOTSA_LOCKSTEP_AUDIO_RVA")) { // "lo-hi,lo-hi" hex image offsets of the audio code (tools/standalone/replay_diff/audio_rva.py)
            for (const char* q = e; *q;) { char* end; const auto lo = std::strtoull(q, &end, 16); if (*end != '-') break; const auto hi = std::strtoull(end + 1, &end, 16); audioRva.emplace_back(lo, hi); q = (*end == ',') ? end + 1 : end; if (!*q) break; }
        }
        if (f) { g_SrandHook = &OnSrand; if ((rlog && (pin & 32)) || (pin & 128)) { g_RandHook = &OnRand; } } // inert (hooks stay null) unless a trace file was requested
    }
};
Cfg& C() { static Cfg c; return c; }
unsigned g_nf; // traced frames so far
uint32_t g_AudioSeed = 0x5EED5EED; unsigned g_AudioCalls; // pin bit 128: the audio stream
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
        g_AudioSeed = 0x5EED5EED; g_AudioCalls = 0;
    }
    if (c.pin & 256) { // pin bit 256: CdStream tick barrier (plan row 7). The CdStream thread finishes a read at a real-time dependent moment, and the streaming code (spawning, model-loaded checks) sees the
        // completion at whichever frame comes next; waiting here until every channel is idle makes a read requested during frame N complete before the tick of frame N+1 in both exes (rd_orig.c does the same).
        const auto t0 = GetTickCount64();
        while (!CdStreamAllIdle() && GetTickCount64() - t0 < 10000) { Sleep(0); }
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
    if (const char* dp = std::getenv("NOTSA_LOCKSTEP_DUMPSCR"); dp && nf <= 4) { // raw script space at traced frames 1..4 (A-B hash hunt; counterpart RD_DUMPSCR in rd_orig.c)
        if (FILE* df = std::fopen((std::string{dp} + "." + std::to_string(nf)).c_str(), "wb")) { std::fwrite(CTheScripts::ScriptSpace.data(), 1, CTheScripts::ScriptSpace.size(), df); std::fclose(df); }
    }
    if (const char* dp = std::getenv("NOTSA_LOCKSTEP_DUMPSTR"); dp && nf <= 6) { // per model id (load state, flags) of CStreamingInfo at traced frames 1..6 (A-B streaming hunt; counterpart RD_DUMPSTR in rd_orig.c)
        if (FILE* df = std::fopen((std::string{dp} + "." + std::to_string(nf)).c_str(), "wb")) {
            std::vector<unsigned char> tab;
            for (const auto& si : CStreaming::ms_aInfoForModel) { tab.push_back((unsigned char)si.m_LoadState); tab.push_back((unsigned char)si.m_Flags); }
            std::fwrite(tab.data(), 1, tab.size(), df); std::fclose(df);
        }
    }
    uint32_t px, py, pz; std::memcpy(&px, &pos[0], 4); std::memcpy(&py, &pos[1], 4); std::memcpy(&pz, &pos[2], 4);
    if (c.pin & 64) { // pin bit 64: "D fc ..." line of debugging probes (ignored by rd_diff.py); rd_orig.c reads the same values from their original addresses
        auto fb = [](float v) { uint32_t u; std::memcpy(&u, &v, 4); return u; };
        uint32_t pn_lo = 0, pn_hi = 0; // loaded path-node areas 0..63 (A-B hunt: GenerateCarCreationCoors2 needs the area of the nodes near the player)
        int rw_b = 0, rw_d = 0, rw_o = 0; // entities of the building / dummy / object pools that have an RwObject (A-B hunt: ConvertToRealObject steals the dummy's RwObject)
        if (auto* pl = GetBuildingPool()) for (auto& e : pl->GetAllValid()) rw_b += e.GetRwObject() != nullptr;
        if (auto* pl = GetDummyPool()) for (auto& e : pl->GetAllValid()) rw_d += e.GetRwObject() != nullptr;
        if (auto* pl = GetObjectPool()) for (auto& e : pl->GetAllValid()) rw_o += e.GetRwObject() != nullptr;
        for (int a = 0; a < 64; a++) if (ThePaths.m_pPathNodes[a]) (a < 32 ? pn_lo : pn_hi) |= 1u << (a & 31);
        std::fprintf(c.f, "D %u run=%d cd=%d cars=%08x,%08x,%08x,%08x perc=%08x,%08x,%08x,%08x zt=%d tw=%d ti=%d pk=%d,%d,%d bc=%d,%d,%d pn=%08x%08x rw=%d,%d,%d\n", (unsigned)CTimer::m_FrameCounter, (int)CCutsceneMgr::ms_running, (int)CCarCtrl::CountDownToCarsAtStart,
                     fb(CPopCycle::m_NumOther_Cars), fb(CPopCycle::m_NumCops_Cars), fb(CPopCycle::m_NumGangs_Cars), fb(CPopCycle::m_NumDealers_Cars),
                     fb(CPopCycle::m_fPercOther), fb(CPopCycle::m_fPercCops), fb(CPopCycle::m_fPercGangs), fb(CPopCycle::m_fPercDealers),
                     (int)CPopCycle::m_nCurrentZoneType, (int)CPopCycle::m_nCurrentTimeOfWeek, (int)CPopCycle::m_nCurrentTimeIndex,
                     (int)CPickups::aPickUps[0].m_nReferenceIndex, (int)CPickups::aPickUps[3].m_nReferenceIndex, (int)CPickups::aPickUps[0x23].m_nReferenceIndex,
                     (int)CRadar::ms_RadarTrace[0].m_nCounter, (int)CRadar::ms_RadarTrace[3].m_nCounter, (int)CRadar::ms_RadarTrace[0x23].m_nCounter, pn_hi, pn_lo, rw_b, rw_d, rw_o);
    }
    std::fprintf(c.f, "F %u %u %08x %u %08x %08x %08x %u %u %u %08x\n", (unsigned)CTimer::m_FrameCounter, (unsigned)CTimer::m_snTimeInMilliseconds, (unsigned)g_RandSeed, (unsigned)g_RandCalls,
                 px, py, pz, hp, np, nv, sh);
    g_RandCalls = 0;
    if ((nf & 63) == 0) std::fflush(c.f);
}

void Note(const char* what) { auto& c = C(); if (c.f && g_started && g_nf < c.maxf) { std::fprintf(c.f, "X %u %s\n", (unsigned)CTimer::m_FrameCounter, what); } }

bool NoAudio() { return C().f && (C().pin & 512); }

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

// rand hook (GameRand.h). Pin bit 32: one "R fc file:line [+caller-rva ...]" line per call during the first NOTSA_LOCKSTEP_RLOG traced frames (counterpart: RD_RLOG in rd_orig.c logs the return address).
// Pin bit 128 (audio RNG partition, plan row 6): rand() calls made from audio code draw from a separate stream (g_AudioSeed), so the real-time dependent number of audio rand() calls (DirectSound
// playback state) cannot move the gameplay stream. Audio code = a source file under .../Audio/ or a CGeneral::GetRandomNumber call whose return address is in NOTSA_LOCKSTEP_AUDIO_RVA=lo-hi[,lo-hi..] (hex image offsets).
uint32_t* OnRand(const char* file, unsigned line) {
    auto& c = C();
    if (!c.f) return nullptr;
    const auto f = Base(file);
    const void* caller = (f == "General.cpp") ? g_RandCaller : nullptr;
    const uintptr_t crva = caller ? (uintptr_t)caller - (uintptr_t)GetModuleHandleA(nullptr) : 0;
    uint32_t* alt = nullptr;
    if ((c.pin & 128) && g_started) {
        bool audio = std::string_view{file}.find("\\Audio\\") != std::string_view::npos || std::string_view{file}.find("/Audio/") != std::string_view::npos;
        if (!audio && caller) for (const auto& r : c.audioRva) audio |= crva >= r.first && crva < r.second;
        if (audio) { alt = &g_AudioSeed; g_AudioCalls++; }
    }
    if (!(c.pin & 32) || !g_started || g_nf > c.rlog || g_nf < c.rfrom) return alt;
    std::fprintf(c.f, "R %u %.*s:%u%s", (unsigned)CTimer::m_FrameCounter, (int)f.size(), f.data(), line, alt ? "A" : "");
    if (caller) std::fprintf(c.f, " +%x", (unsigned)crva);
    if (g_Outer) { std::fprintf(c.f, " +%x", (unsigned)((uintptr_t)g_Outer - (uintptr_t)GetModuleHandleA(nullptr))); g_Outer = nullptr; }
    if (c.rstack) { // NOTSA_LOCKSTEP_RSTACK=<n>: the first n caller return addresses as image-relative offsets (resolved by tools/standalone/replay_diff/resolve_stack.py against the pdb)
        void* bt[16]{};
        const auto n = CaptureStackBackTrace(1, std::min<unsigned>(c.rstack, 16), bt, nullptr);
        const auto base = (uintptr_t)GetModuleHandleA(nullptr);
        for (USHORT i = 0; i < n; i++) std::fprintf(c.f, " +%x", (unsigned)((uintptr_t)bt[i] - base));
    }
    std::fputc('\n', c.f);
    return alt;
}

// srand hook (GameRand.h): pin bit 1 + the "S fc file:line seed" trace line (the original's S line carries the return address instead of file:line; rd_diff.py ignores that column)
void OnSrand(unsigned& seed, const char* file, unsigned line) {
    auto& c = C();
    if (!c.f) return;
    const auto f = Base(file);
    // pin bit 1: time-/pointer-derived seeds -> a constant: GameInit, LoadSplashes (clock), CEntity::CreateEffects (this + days: a heap address, plan row 8), CPostEffects::Grain (OS_TimeMS, row 3)
    if ((c.pin & 1) && (f == "app_game.cpp" || f == "LoadingScreen.cpp" || f == "Entity.cpp" || f == "PostEffects.cpp")) seed = 0x13572468;
    if (g_started) std::fprintf(c.f, "S %u %.*s:%u %08x\n", (unsigned)CTimer::m_FrameCounter, (int)f.size(), f.data(), line, seed);
}
} // namespace notsa::lockstep
#endif
