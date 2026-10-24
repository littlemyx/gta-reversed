// RR-1: deterministic record/replay state-hash harness (see statehash.h and .notes/RR_PLAN.md). Compiled only with -DGTASA_STATEHASH=ON.
#include "StdInc.h"

#ifdef NOTSA_STATEHASH
#include "statehash.h"
#include "fakerw.h"
#ifdef NOTSA_PORTABLE // RR-3: this file also builds into the Win32 StandaloneRelease variant (-DGTASA_STATEHASH=ON); the portable-only parts are marked
#include "dsound_al.h"
#include <src/gl/rwgl3impl.h>
#include <mach/mach.h>
#endif

#include <atomic>
#include <new>
#include <thread>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Pools/Pools.h"
#include "Wanted.h"
#include "Bike.h"
#include "Automobile.h"
#include "PedIntelligence.h"
#include "Population.h"
#include "CarCtrl.h"
#include "Streaming.h"
#include "Weather.h"
#include "Clock.h"

// RR: every C++ `new` returns zeroed memory too (the default operator delete is free()). Only the two plain forms are replaced; aligned / nothrow forms keep the library versions.
// (MAC-RR2: must not call notsa::rr::Active(): its Cfg() static holds std::strings, whose first allocation lands back here -> __cxa_guard_acquire "recursive initialization"
// abort at startup. A constant-initialised tri-state read straight from the environment has no guard.)
static int g_rrNewMode = -1;
void* operator new(std::size_t n) {
    if (g_rrNewMode < 0) {
        const char* e = std::getenv("NOTSA_RR");
        g_rrNewMode = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    void* p = g_rrNewMode ? std::calloc(1, n ? n : 1) : std::malloc(n ? n : 1);
    if (!p) { throw std::bad_alloc(); }
    return p;
}
void* operator new[](std::size_t n) { return ::operator new(n); }

namespace notsa::rr {
namespace {
// ---------------------------------------------------------------- configuration
struct Config {
    bool        active   = false;
    uint32_t    seed     = 12345;
    uint32_t    stepMs   = 20;
    std::string trace, tag;
    uint32_t    maxFrames = 0, detailFrom = 1, detailTo = 0;
    bool        exitAtMax = false;
};
Config& Cfg() {
    static Config c = [] {
        Config c;
        if (const char* e = std::getenv("NOTSA_RR")) { c.active = e[0] && std::strcmp(e, "0") != 0; }
        if (const char* e = std::getenv("NOTSA_RR_STEP_MS")) { c.stepMs = std::max(14, std::atoi(e)); }
        if (const char* e = std::getenv("NOTSA_RR_SEED")) { c.seed = (uint32_t)std::strtoul(e, nullptr, 0); }
        if (const char* e = std::getenv("NOTSA_RR_TRACE")) { c.trace = e; }
        if (const char* e = std::getenv("NOTSA_RR_TAG")) { c.tag = e; }
        if (const char* e = std::getenv("NOTSA_RR_MAXFRAMES")) { c.maxFrames = (uint32_t)std::strtoul(e, nullptr, 0); }
        if (const char* e = std::getenv("NOTSA_RR_EXIT")) { c.exitAtMax = e[0] && std::strcmp(e, "0") != 0; }
        if (const char* e = std::getenv("NOTSA_RR_DETAIL")) { unsigned a = 1, b = 0; if (std::sscanf(e, "%u-%u", &a, &b) >= 1) { if (!b) b = a; c.detailFrom = a; c.detailTo = b; } }
        return c;
    }();
    return c;
}

// virtual clock: starts at 1000 s so no code ever sees a tiny / negative time
std::atomic<uint64_t> g_vns{ 1'000'000'000'000ull };
uint64_t              g_vStartNs = 1'000'000'000'000ull;

// ---------------------------------------------------------------- hashing
// FNV-1a 64 over explicit little-endian bytes (endianness independent). Two accumulators: `e` = exact (floats by bit pattern), `q` = quantised (floats rounded to 1/256, so
// rounding-level drift between x87 and SSE/NEON float shows up in `e` only and a real behavioural divergence in both).
constexpr uint64_t kFnvOff = 0xcbf29ce484222325ull, kFnvPrime = 0x100000001b3ull;

struct Acc {
    uint64_t e = kFnvOff, q = kFnvOff;
    static void Feed(uint64_t& h, uint32_t v) { for (int i = 0; i < 4; i++) { h ^= (v >> (8 * i)) & 0xFF; h *= kFnvPrime; } }
    void U(uint32_t v) { Feed(e, v); Feed(q, v); }
    void F(float f) {
        Feed(e, std::bit_cast<uint32_t>(f));
        Feed(q, std::isfinite(f) && std::fabs(f) < 8e6f ? (uint32_t)(int32_t)std::lround(f * 256.0f) : 0x7FFFFFFFu);
    }
    void Mix(const Acc& o) { Feed(e, (uint32_t)o.e); Feed(e, (uint32_t)(o.e >> 32)); Feed(q, (uint32_t)o.q); Feed(q, (uint32_t)(o.q >> 32)); }
};

// one field sink for the hash and the detail text, so the two can never disagree
struct Sink {
    Acc          acc;
    std::string* det = nullptr; // detail text of the current entity (nullptr: hash only)
    void U(const char* n, uint32_t v) { acc.U(v); if (det) { char b[48]; std::snprintf(b, sizeof b, " %s=%x", n, v); *det += b; } }
    void I(const char* n, int32_t v) { U(n, (uint32_t)v); }
    void F(const char* n, float v) { acc.F(v); if (det) { char b[48]; std::snprintf(b, sizeof b, " f.%s=%08x", n, std::bit_cast<uint32_t>(v)); *det += b; } }
    void V(const char* n, const CVector& v) { char b[16]; std::snprintf(b, sizeof b, "%sx", n); F(b, v.x); std::snprintf(b, sizeof b, "%sy", n); F(b, v.y); std::snprintf(b, sizeof b, "%sz", n); F(b, v.z); }
};

// ---------------------------------------------------------------- state
FILE*    g_f          = nullptr;
bool     g_recording  = false;
uint32_t g_frame      = 0;   // recorded frames so far (the number of the NEXT frame - 1)
uint32_t g_idleFrames = 0;   // all Idle frames since process start (diagnostics)
bool     g_atexit     = false;

uint32_t RngState() {
#if defined(NOTSA_PORTABLE)
    return ::notsa::msvc_crt::HoldRand();
#elif defined(NOTSA_STANDALONE_RUN)
    return ::notsa::g_RandSeed;
#else
    return 0;
#endif
}
void RngSeed(uint32_t s) { srand(s); }

void CloseTrace() {
    if (g_f) { std::fflush(g_f); std::fclose(g_f); g_f = nullptr; }
    g_recording = false;
}

// ---------------------------------------------------------------- per-entity field lists
void PhysicalFields(Sink& s, CPhysical& p) {
    s.U("model", p.m_nModelIndex);
    s.U("status", (uint32_t)p.GetStatus());
    s.V("p", p.GetPosition());
    if (p.m_matrix) { // orientation: the 3 axes of the live matrix
        s.V("r", p.GetRight()); s.V("f", p.GetForward()); s.V("u", p.GetUp());
    } else {
        s.F("hd", p.m_placement.m_fHeading);
    }
    s.V("mv", p.m_vecMoveSpeed);
    s.V("tv", p.m_vecTurnSpeed);
}

void PedFields(Sink& s, CPed& ped) {
    PhysicalFields(s, ped);
    s.U("ptype", (uint32_t)ped.m_nPedType);
    s.U("pstate", (uint32_t)ped.m_nPedState);
    s.U("mstate", (uint32_t)ped.m_nMoveState);
    s.F("hp", ped.m_fHealth);
    s.F("arm", ped.m_fArmour);
    s.F("rot", ped.m_fCurrentRotation);
    s.F("aim", ped.m_fAimingRotation);
    s.I("veh", ped.m_pVehicle ? (int32_t)GetVehiclePool()->GetIndex(ped.m_pVehicle) : -1);
    s.U("invh", ped.bInVehicle ? 1u : 0u);
    auto& w = ped.GetActiveWeapon();
    s.U("wtype", (uint32_t)w.GetType());
    s.U("ammo", (uint32_t)w.GetTotalAmmo());
    uint32_t chain = 0, n = 0; // task type chain (primary tree), 6 deep
    for (auto* t = ped.GetIntelligence()->GetTaskManager().GetActiveTask(); t && n < 6; t = t->GetSubTask(), n++) { chain = chain * 131u + (uint32_t)t->GetTaskType() + 1u; }
    s.U("tasks", chain);
}

void VehicleFields(Sink& s, CVehicle& v) {
    PhysicalFields(s, v);
    s.F("hp", v.m_fHealth);
    s.F("gas", v.m_GasPedal);
    s.F("brk", v.m_BrakePedal);
    s.F("steer", v.m_fSteerAngle);
    s.I("drv", v.m_pDriver ? (int32_t)GetPedPool()->GetIndex(v.m_pDriver) : -1);
    s.U("mission", (uint32_t)v.m_autoPilot.m_nCarMission);
    s.U("tempact", (uint32_t)v.m_autoPilot.m_nTempAction);
    s.U("col", (uint32_t)v.m_nPrimaryColor | ((uint32_t)v.m_nSecondaryColor << 8));
}

void ObjectFields(Sink& s, CObject& o) { PhysicalFields(s, o); }

struct Result { uint32_t n = 0; Acc acc; };

template<typename Pool, typename Fn>
Result HashPool(Pool* pool, const char* kind, bool detail, uint32_t frame, std::string* out, Fn&& fields) {
    Result r;
    if (!pool) return r;
    Acc total;
    for (size_t i = 0, sz = (size_t)pool->GetSize(); i < sz; i++) { // slot order = pool order (never pointer order)
        auto* obj = pool->GetAt(i);
        if (!obj) continue;
        std::string det;
        Sink s;
        s.det = detail ? &det : nullptr;
        s.U("slot", (uint32_t)i);
        fields(s, *obj);
        total.Mix(s.acc);
        r.n++;
        if (detail && out) {
            char b[40];
            std::snprintf(b, sizeof b, "D %u %s %zu", frame, kind, i);
            *out += b; *out += det; *out += '\n';
        }
    }
    r.acc = total;
    return r;
}

std::string Hex(uint64_t v) { char b[20]; std::snprintf(b, sizeof b, "%016llx", (unsigned long long)v); return b; }
std::string HQ(const Acc& a) { return Hex(a.e) + "/" + Hex(a.q); }
std::string HE(const Acc& a) { return Hex(a.e); }

void WriteHeader(uint32_t seed) {
    std::fprintf(g_f, "#RR 1 build=%s seed=%u step_ms=%u float=%s\n", Cfg().tag.empty() ? "unset" : Cfg().tag.c_str(), seed, Cfg().stepMs,
#if defined(NOTSA_PORTABLE)
        "portable"
#else
        "win32"
#endif
    );
    std::fflush(g_f);
}

// ---------------------------------------------------------------- the per-frame record
void Record() {
    const bool detail = (g_frame >= Cfg().detailFrom && g_frame <= Cfg().detailTo);
    std::string det;
    std::string* d = detail ? &det : nullptr;
    char line[64];

    // timer + clock + rng (integers only)
    Acc tm;
    { Sink s; s.det = d; if (d) *d += "D " + std::to_string(g_frame) + " timer";
      s.U("fc", CTimer::GetFrameCounter()); s.U("ms", CTimer::GetTimeInMS()); s.U("msnc", CTimer::GetTimeInMSNonClipped()); s.U("mspause", CTimer::GetTimeInMSPauseMode());
      s.F("step", CTimer::GetTimeStep()); s.F("scale", CTimer::GetTimeScale()); s.U("paused", CTimer::GetIsPaused() ? 1u : 0u);
      if (d) *d += '\n'; tm = s.acc; }
    Acc ck;
    { Sink s; s.det = d; if (d) *d += "D " + std::to_string(g_frame) + " clock";
      s.U("h", CClock::GetGameClockHours()); s.U("m", CClock::GetGameClockMinutes()); s.U("s", CClock::GetGameClockSeconds()); s.U("d", CClock::GetGameClockDays());
      s.U("wnew", (uint32_t)CWeather::NewWeatherType); s.U("wold", (uint32_t)CWeather::OldWeatherType);
      if (d) *d += '\n'; ck = s.acc; }
    const uint32_t rng = RngState();
    if (d) { std::snprintf(line, sizeof line, "D %u rng state=%x\n", g_frame, rng); *d += line; }

    // world counters (population / traffic / streaming)
    Acc pop;
    { Sink s; s.det = d; if (d) *d += "D " + std::to_string(g_frame) + " pop";
      s.I("peds", CPopulation::ms_nTotalPeds); s.I("civ", CPopulation::ms_nTotalCivPeds); s.I("gang", CPopulation::ms_nTotalGangPeds); s.I("cop", CPopulation::ms_nNumCop);
      s.I("rcars", CCarCtrl::NumRandomCars); s.I("law", CCarCtrl::NumLawEnforcerCars); s.I("mcars", CCarCtrl::NumMissionCars); s.I("parked", CCarCtrl::NumParkedCars);
      s.U("stmem", CStreaming::ms_memoryUsedBytes); s.I("streq", CStreaming::ms_numModelsRequested);
      s.U("npedpool", GetPedPool() ? (uint32_t)GetPedPool()->GetNoOfUsedSpaces() : 0u); s.U("nvehpool", GetVehiclePool() ? (uint32_t)GetVehiclePool()->GetNoOfUsedSpaces() : 0u);
      if (d) *d += '\n'; pop = s.acc; }

    // player (ped 0 of CWorld::Players[0]), wanted level, money; camera
    Acc pl;
    if (auto* p = FindPlayerPed()) {
        Sink s; s.det = d; if (d) *d += "D " + std::to_string(g_frame) + " player";
        PedFields(s, *p);
        auto* w = FindPlayerWanted();
        s.I("wl", w ? (int32_t)w->GetWantedLevel() : -1); s.I("chaos", w ? w->m_ChaosLevel : -1);
        s.I("money", FindPlayerInfo().m_nMoney);
        if (d) *d += '\n'; pl = s.acc;
    }
    Acc cam;
    { Sink s; s.det = d; if (d) *d += "D " + std::to_string(g_frame) + " camera";
      s.V("p", TheCamera.GetPosition()); s.V("f", TheCamera.GetForward());
      if (d) *d += '\n'; cam = s.acc; }

    std::string ents;
    const auto peds = HashPool(GetPedPool(), "ped", detail, g_frame, &ents, PedFields);
    const auto vehs = HashPool(GetVehiclePool(), "veh", detail, g_frame, &ents, VehicleFields);
    const auto objs = HashPool(GetObjectPool(), "obj", detail, g_frame, &ents, ObjectFields);

    Acc all;
    Acc rngA; rngA.U(rng);
    const Acc* const parts[] = { &tm, &ck, &rngA, &pop, &pl, &cam, &peds.acc, &vehs.acc, &objs.acc };
    for (const Acc* a : parts) { all.Mix(*a); }

    std::string out;
    out.reserve(512);
    out += "F " + std::to_string(g_frame);
    out += " gf=" + std::to_string(CTimer::GetFrameCounter());
    std::snprintf(line, sizeof line, " rng=%08x", rng); out += line;
    out += " tm=" + HE(tm) + " ck=" + HE(ck) + " pop=" + HE(pop);
    out += " pl=" + HQ(pl) + " cam=" + HQ(cam);
    out += " pd=" + std::to_string(peds.n) + ":" + HQ(peds.acc);
    out += " vh=" + std::to_string(vehs.n) + ":" + HQ(vehs.acc);
    out += " ob=" + std::to_string(objs.n) + ":" + HQ(objs.acc);
    out += " all=" + HQ(all) + "\n";
    std::fwrite(out.data(), 1, out.size(), g_f);
    if (detail) { std::fwrite(det.data(), 1, det.size(), g_f); std::fwrite(ents.data(), 1, ents.size(), g_f); }
    std::fflush(g_f);
}

void StartRecording(uint32_t seed) {
    if (g_recording) CloseTrace();
    const auto& path = Cfg().trace;
    if (path.empty()) { NOTSA_LOG_WARN("rr_start: NOTSA_RR_TRACE is not set, nothing is recorded"); return; }
    g_f = std::fopen(path.c_str(), "w");
    if (!g_f) { NOTSA_LOG_ERR("rr_start: cannot open '{}'", path); return; }
    if (!g_atexit) { g_atexit = true; std::atexit([] { CloseTrace(); }); }
    RngSeed(seed);
    g_frame     = 0;
    g_recording = true;
    WriteHeader(seed);
    NOTSA_LOG_INFO("rr_start: recording to '{}' (seed {}, game frame {})", path, seed, CTimer::GetFrameCounter());
}
} // namespace

// ---------------------------------------------------------------- public API
bool     Active() { return Cfg().active; }
uint32_t Seed() { return Cfg().seed; }
uint64_t VirtualNs() { return g_vns.load(std::memory_order_relaxed); }
uint64_t VirtualMs() { return (VirtualNs() - g_vStartNs) / 1'000'000ull; }
bool     Recording() { return g_recording; }
bool     NoAudio() { static const bool v = Active() && std::getenv("NOTSA_RR_NOAUDIO") && std::getenv("NOTSA_RR_NOAUDIO")[0] == '1'; return v; }

uint32_t StableId(const void* p) {
    if (!p) return 0;
    const auto id = [p](auto* pool, uint32_t tag) -> int64_t { return pool && pool->IsPtrFromPool((const typename std::remove_pointer_t<decltype(pool->GetAt(0))>*)p) ? (int64_t)((tag << 24) | (uint32_t)pool->GetIndex((const typename std::remove_pointer_t<decltype(pool->GetAt(0))>*)p)) : -1; };
    if (const auto v = id(GetPedPool(), 1); v >= 0) return (uint32_t)v;
    if (const auto v = id(GetVehiclePool(), 2); v >= 0) return (uint32_t)v;
    if (const auto v = id(GetObjectPool(), 3); v >= 0) return (uint32_t)v;
    if (const auto v = id(GetBuildingPool(), 4); v >= 0) return (uint32_t)v;
    if (const auto v = id(GetDummyPool(), 5); v >= 0) return (uint32_t)v;
    static unsigned s_Unknown = 0;
    if (s_Unknown++ < 8) { NOTSA_LOG_WARN("rr::StableId: {} is not in a known pool (static data? the offset from the module is used)", p); }
    return 0xF0000000u | ((uint32_t)((uintptr_t)p - (uintptr_t)&StableId) & 0x0FFFFFFFu);
}

#ifdef NOTSA_PORTABLE
// ---- rand() draw log (NOTSA_RR_RANDLOG=<file>, NOTSA_RR_RANDLOG_FRAMES=A-B in Idle frames): for bisecting a divergence of the RNG state down to the call site of the first differing draw
namespace {
struct RandLog {
    FILE*    f = nullptr;
    uint32_t from = 0, to = 0;
    uint32_t idx = 0;
    std::thread::id main;
    RandLog() {
        if (const char* p = std::getenv("NOTSA_RR_RANDLOG")) {
            f = std::fopen(p, "w");
            unsigned a = 0, b = 0xFFFFFFFFu;
            if (const char* r = std::getenv("NOTSA_RR_RANDLOG_FRAMES")) { std::sscanf(r, "%u-%u", &a, &b); }
            from = a; to = b;
            main = ::notsa::msvc_crt::MainThreadId();
        }
    }
};
RandLog& RL() { static RandLog r; return r; }
} // namespace
__attribute__((noinline)) void RandHook(uint32_t stateBefore) {
    auto& r = RL();
    if (!r.f || g_idleFrames < r.from || g_idleFrames > r.to || std::this_thread::get_id() != r.main) return;
    static const uintptr_t base = (uintptr_t)&StableId; // offsets relative to a function of this module: stable across runs (ASLR), resolve with `nm` / `atos`
    uintptr_t ra[5] = {};
    void**    fp    = (void**)__builtin_frame_address(0); // arm64 / x64 clang keep the frame-pointer chain: [fp] = caller's fp, [fp+1] = return address
    for (int i = 0; i < 5 && fp && ((uintptr_t)fp & 7) == 0; i++) { ra[i] = (uintptr_t)fp[1] - base; fp = (void**)fp[0]; }
    std::fprintf(r.f, "%u %u %08x %llx %llx %llx %llx %llx\n", g_idleFrames, r.idx++, stateBefore, (unsigned long long)ra[0], (unsigned long long)ra[1], (unsigned long long)ra[2], (unsigned long long)ra[3], (unsigned long long)ra[4]);
}
#endif // NOTSA_PORTABLE (rand() draw log: the RandHook call sits in portable/msvc_rand.h)

void RngMark(const char* name) {
    static const bool on = Active() && std::getenv("NOTSA_RR_RNGMARKS");
    if (on && g_idleFrames <= 2) { NOTSA_LOG_INFO("rrm {} rng={:08x} idle={} gf={}", name, RngState(), g_idleFrames, CTimer::GetFrameCounter()); }
}

void Probe(const char* tag) {
    static const std::pair<uint32_t, uint32_t> rg = [] { unsigned a = 1, b = 0; if (const char* e = std::getenv("NOTSA_RR_PROBE")) { std::sscanf(e, "%u-%u", &a, &b); if (!b) b = a; } else { a = 1; b = 0; } return std::pair<uint32_t, uint32_t>{ a, b }; }();
    static const uint32_t slot = [] { const char* e = std::getenv("NOTSA_RR_PROBE_SLOT"); return e ? (uint32_t)std::strtoul(e, nullptr, 0) : 3u; }();
    if (g_idleFrames < rg.first || g_idleFrames > rg.second) return;
    auto* vp = GetVehiclePool();
    if (!vp) return;
    Acc all;
    for (size_t i = 0; i < (size_t)vp->GetSize(); i++) { if (auto* v = vp->GetAt(i)) { Sink s; s.U("slot", (uint32_t)i); VehicleFields(s, *v); all.Mix(s.acc); } }
    std::string l;
    if (auto* v = vp->GetAt(slot)) {
        char b[220];
        std::snprintf(b, sizeof b, " pos=%08x,%08x,%08x mv=%08x,%08x,%08x tv=%08x,%08x,%08x ft=%08x,%08x,%08x force=%08x,%08x,%08x tq=%08x,%08x,%08x dmg=%08x", std::bit_cast<uint32_t>(v->GetPosition().x), std::bit_cast<uint32_t>(v->GetPosition().y), std::bit_cast<uint32_t>(v->GetPosition().z),
            std::bit_cast<uint32_t>(v->m_vecMoveSpeed.x), std::bit_cast<uint32_t>(v->m_vecMoveSpeed.y), std::bit_cast<uint32_t>(v->m_vecMoveSpeed.z), std::bit_cast<uint32_t>(v->m_vecTurnSpeed.x), std::bit_cast<uint32_t>(v->m_vecTurnSpeed.y), std::bit_cast<uint32_t>(v->m_vecTurnSpeed.z),
            std::bit_cast<uint32_t>(v->m_vecFrictionTurnSpeed.x), std::bit_cast<uint32_t>(v->m_vecFrictionTurnSpeed.y), std::bit_cast<uint32_t>(v->m_vecFrictionTurnSpeed.z), std::bit_cast<uint32_t>(v->m_vecForce.x), std::bit_cast<uint32_t>(v->m_vecForce.y), std::bit_cast<uint32_t>(v->m_vecForce.z),
            std::bit_cast<uint32_t>(v->m_vecTorque.x), std::bit_cast<uint32_t>(v->m_vecTorque.y), std::bit_cast<uint32_t>(v->m_vecTorque.z), std::bit_cast<uint32_t>(v->m_fDamageIntensity));
        l = b;
        if (v->IsBike()) { std::snprintf(b, sizeof b, " avgn=%08x,%08x,%08x", std::bit_cast<uint32_t>(((CBike*)v)->m_vecAveGroundNormal.x), std::bit_cast<uint32_t>(((CBike*)v)->m_vecAveGroundNormal.y), std::bit_cast<uint32_t>(((CBike*)v)->m_vecAveGroundNormal.z)); l += b; }
    }
    NOTSA_LOG_INFO("rrp {} {} all={:016x}{}", g_idleFrames, tag, all.e, l);
}

namespace {
// RR: every RenderWare allocation (clump hierarchies, animation interpolators, keyframe buffers, ...) comes back zeroed. Without it a held prop (CJ_CIGGY) occasionally reads a NaN bone matrix
// (u6 in RR_PLAN section 9) and CPhysical::RemoveAndAdd then crashes on the NaN position: librw's HAnimHierarchy matrices / interpolator buffers are malloc'd and read before the first update.
void* RrRwMalloc(size_t sz, uint32_t) { return sz ? std::calloc(1, sz) : nullptr; }
void PinRwHeap() {
    static bool s_Done = false;
    if (s_Done || !rw::Engine::memfuncs.rwmalloc) return;
    s_Done = true;
    rw::Engine::memfuncs.rwmalloc = RrRwMalloc;
    NOTSA_LOG_INFO("rr: RenderWare allocations are zeroed from now on");
}
} // namespace

void PinHeap() { PinRwHeap(); }

void AdvanceClock() {
    if (!Active()) return;
    PinRwHeap();
    g_vns.fetch_add((uint64_t)Cfg().stepMs * 1'000'000ull, std::memory_order_relaxed);
#ifdef NOTSA_PORTABLE
    notsa::portable::dsal::RrPump(Cfg().stepMs); // the software DirectSound mixer: cursors / playing state advance with game time (the audio entities read them and draw random numbers on that basis)
#endif
}

void OnFrame() {
    g_idleFrames++;
    static const uint32_t s_LogFrames = [] { const char* e = std::getenv("NOTSA_RR_LOG_FRAMES"); return e ? (uint32_t)std::strtoul(e, nullptr, 0) : 0u; }();
    if (g_idleFrames <= s_LogFrames) { // bisecting a pre-record divergence: one line per frame with the cheap global state words (compare two runs with diff)
        NOTSA_LOG_INFO("rrf {} gf={} tms={} rng={:08x} peds={} vehs={} objs={} stmem={} streq={}", g_idleFrames, CTimer::GetFrameCounter(), CTimer::GetTimeInMS(), RngState(),
            GetPedPool() ? GetPedPool()->GetNoOfUsedSpaces() : 0, GetVehiclePool() ? GetVehiclePool()->GetNoOfUsedSpaces() : 0, GetObjectPool() ? GetObjectPool()->GetNoOfUsedSpaces() : 0, CStreaming::ms_memoryUsedBytes, CStreaming::ms_numModelsRequested);
    }
    if (g_idleFrames <= s_LogFrames && std::getenv("NOTSA_RR_LOG_ENTS")) { // per-entity positions (bit patterns) in the same bisecting log
        std::string l = "rrfe " + std::to_string(g_idleFrames);
        char b[96];
        if (auto* vp = GetVehiclePool()) { for (size_t i = 0; i < (size_t)vp->GetSize(); i++) { if (auto* v = vp->GetAt(i)) { std::snprintf(b, sizeof b, " v%zu:%u,%08x,%08x,%08x,mv%08x,%08x,%08x,tv%08x,%08x,%08x,f%08x,%08x", i, (unsigned)v->m_nModelIndex, std::bit_cast<uint32_t>(v->GetPosition().x), std::bit_cast<uint32_t>(v->GetPosition().y), std::bit_cast<uint32_t>(v->GetPosition().z),
        std::bit_cast<uint32_t>(v->m_vecMoveSpeed.x), std::bit_cast<uint32_t>(v->m_vecMoveSpeed.y), std::bit_cast<uint32_t>(v->m_vecMoveSpeed.z), std::bit_cast<uint32_t>(v->m_vecTurnSpeed.x), std::bit_cast<uint32_t>(v->m_vecTurnSpeed.y), std::bit_cast<uint32_t>(v->m_vecTurnSpeed.z),
        std::bit_cast<uint32_t>(v->m_vecFrictionMoveSpeed.x), std::bit_cast<uint32_t>(v->m_vecFrictionMoveSpeed.y)); l += b; } } }
        if (auto* pp = GetPedPool()) { for (size_t i = 0; i < (size_t)pp->GetSize(); i++) { if (auto* v = pp->GetAt(i)) { std::snprintf(b, sizeof b, " p%zu:%u,%08x,%08x,%08x", i, (unsigned)v->m_nModelIndex, std::bit_cast<uint32_t>(v->GetPosition().x), std::bit_cast<uint32_t>(v->GetPosition().y), std::bit_cast<uint32_t>(v->GetPosition().z)); l += b; } } }
        NOTSA_LOG_INFO("{}", l);
    }
    if (const char* dv = std::getenv("NOTSA_RR_DUMP_VEH"); dv && g_idleFrames == (uint32_t)std::strtoul(dv, nullptr, 0)) { // raw bytes of every vehicle at that Idle frame (find uninitialised / racy members by diffing two runs)
        if (auto* vp = GetVehiclePool()) {
            for (size_t i = 0; i < (size_t)vp->GetSize(); i++) {
                if (auto* v = vp->GetAt(i)) {
                    const std::string path = std::string(std::getenv("NOTSA_RR_DUMP_DIR") ? std::getenv("NOTSA_RR_DUMP_DIR") : ".") + "/veh" + std::to_string(i) + "_m" + std::to_string(v->m_nModelIndex) + ".bin";
                    if (FILE* f = std::fopen(path.c_str(), "wb")) { std::fwrite(v, 1, v->IsBike() ? sizeof(CBike) : sizeof(CAutomobile), f); std::fclose(f); }
                }
            }
        }
    }
#ifdef NOTSA_PORTABLE
    if (Active() && (g_idleFrames == 100 || g_idleFrames == 200)) { // which threads exist (any thread that touches game state is a suspect for a nondeterminism)
        thread_act_array_t th; mach_msg_type_number_t n = 0;
        if (task_threads(mach_task_self(), &th, &n) == KERN_SUCCESS) {
            std::string l = "rr: threads=" + std::to_string(n) + " cpu%:";
            for (mach_msg_type_number_t i = 0; i < n; i++) { thread_basic_info_data_t bi; mach_msg_type_number_t c = THREAD_BASIC_INFO_COUNT; if (thread_info(th[i], THREAD_BASIC_INFO, (thread_info_t)&bi, &c) == KERN_SUCCESS) { l += " " + std::to_string(bi.cpu_usage / 10) + (bi.run_state == TH_STATE_RUNNING ? "r" : "s"); } }
            NOTSA_LOG_INFO("{}", l);
        }
    }
#endif
    if (Active() && g_idleFrames % 250 == 0) { NOTSA_LOG_INFO("rr: idle frame {} (virtual {} ms, game frame {}, recording {})", g_idleFrames, VirtualMs(), CTimer::GetFrameCounter(), g_recording); }
    if (!g_recording || !g_f) return;
    Record();
    g_frame++;
    if (Cfg().maxFrames && g_frame >= Cfg().maxFrames) {
        NOTSA_LOG_INFO("rr: {} frames recorded, stopping", g_frame);
        CloseTrace();
        if (Cfg().exitAtMax) { RsGlobal.quit = true; }
    }
}

bool Cmd(const char* name, const std::string& args) {
    if (!std::strcmp(name, "rr_start")) {
        StartRecording(args.empty() ? Cfg().seed : (uint32_t)std::strtoul(args.c_str(), nullptr, 0));
        return true;
    }
    if (!std::strcmp(name, "rr_stop")) {
        NOTSA_LOG_INFO("rr_stop: {} frames", g_frame);
        CloseTrace();
        return true;
    }
    if (!std::strcmp(name, "rr_offsets")) { // layout of the fields a raw byte diff of two runs points at
        NOTSA_LOG_INFO("rr_offsets: moveSpeed={:#x} turnSpeed={:#x} frictionMove={:#x} frictionTurn={:#x} force={:#x} torque={:#x} mass={:#x} damageIntensity={:#x} collidedCnt={:#x} CBike={:#x}",
            offsetof(CPhysical, m_vecMoveSpeed), offsetof(CPhysical, m_vecTurnSpeed), offsetof(CPhysical, m_vecFrictionMoveSpeed), offsetof(CPhysical, m_vecFrictionTurnSpeed), offsetof(CPhysical, m_vecForce), offsetof(CPhysical, m_vecTorque),
            offsetof(CPhysical, m_fMass), offsetof(CPhysical, m_fDamageIntensity), offsetof(CPhysical, m_nNumEntitiesCollided), sizeof(CBike));
        return true;
    }
    if (!std::strcmp(name, "rr_stat")) {
        NOTSA_LOG_INFO("rr_stat {}: idle={} vms={} gf={} tms={} rng={:08x} peds={} vehs={} objs={} bld={} stmem={}", args, g_idleFrames, VirtualMs(), CTimer::GetFrameCounter(), CTimer::GetTimeInMS(), RngState(),
            GetPedPool() ? GetPedPool()->GetNoOfUsedSpaces() : 0, GetVehiclePool() ? GetVehiclePool()->GetNoOfUsedSpaces() : 0, GetObjectPool() ? GetObjectPool()->GetNoOfUsedSpaces() : 0, GetBuildingPool() ? GetBuildingPool()->GetNoOfUsedSpaces() : 0, CStreaming::ms_memoryUsedBytes);
        return true;
    }
    return false;
}
} // namespace notsa::rr
#endif // NOTSA_STATEHASH
