// RR-1: deterministic record/replay state-hash harness (see statehash.h and .notes/RR_PLAN.md). Compiled only with -DGTASA_STATEHASH=ON.
#include "StdInc.h"

#ifdef NOTSA_STATEHASH
#include "statehash.h"
#include "fakerw.h"
#ifdef NOTSA_PORTABLE // RR-3: this file also builds into the Win32 StandaloneRelease variant (-DGTASA_STATEHASH=ON); the portable-only parts are marked
#include <src/gl/rwgl3impl.h>
#ifndef _WIN32 // SH2: the portable build on Windows (PortableWin64) has the real DirectSound and no mach
#include "dsound_al.h"
#include <mach/mach.h>
#endif
#endif

#include <atomic>
#include <new>
#include <thread>
#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <set>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Pools/Pools.h"
#include "Tasks/TaskTypes/TaskComplexCarDrive.h"
#include "Tasks/TaskTypes/TaskComplexDriveWander.h"
#include "Tasks/TaskTypes/TaskSimpleCarDrive.h"
#include "Tasks/TaskTypes/TaskSimpleCarDriveTimed.h"
#include "Tasks/TaskManager.h"
#include <format>
#include "Wanted.h"
#include "Bike.h"
#include "Automobile.h"
#include "PedIntelligence.h"
#include "Population.h"
#include "CarCtrl.h"
#include "Streaming.h"
#include "Weather.h"
#include "Clock.h"
#include "MenuManager.h"
#include "AnimBlendAssociation.h"
#include "Plugins/RpAnimBlendPlugin/RpAnimBlend.h"

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

#include "loadlog.h"
namespace notsa::rr {
uint32_t LoadLogFrameCounter() { return CTimer::GetFrameCounter(); }
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
    std::string* dump = nullptr; // SH1 verbose dump rows (NOTSA_SH_DUMP): one `frame subsys key field value` row per hashed word, in feed order
    std::string  pfx;            // "frame subsys key" prefix of the dump rows
    void Row(const char* n, const char* val) { *dump += pfx; *dump += ' '; *dump += n; *dump += ' '; *dump += val; *dump += '\n'; }
    void U(const char* n, uint32_t v) { acc.U(v); if (det) { char b[48]; std::snprintf(b, sizeof b, " %s=%x", n, v); *det += b; } if (dump) { char b[24]; std::snprintf(b, sizeof b, "u:%x", v); Row(n, b); } }
    void I(const char* n, int32_t v) { U(n, (uint32_t)v); }
    void F(const char* n, float v) { acc.F(v); if (det) { char b[48]; std::snprintf(b, sizeof b, " f.%s=%08x", n, std::bit_cast<uint32_t>(v)); *det += b; } if (dump) { char b[48]; std::snprintf(b, sizeof b, "f:%08x:%.9g", std::bit_cast<uint32_t>(v), (double)v); Row(n, b); } }
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

// SH1: NOTSA_SH_FILE (per-frame hash file), NOTSA_SH_DUMP / NOTSA_SH_DUMP_SUBSYS / NOTSA_SH_DUMP_FILE (verbose dump), see .notes/STATEHASH_SPEC.md
struct ShConfig {
    std::string                 file, dumpFile = "sh_dump.txt";
    std::set<uint32_t>          dumpFrames;
    std::set<std::string>       dumpSubsys;
    bool                        dumpAll = false;
};
const ShConfig& ShCfg() {
    static const ShConfig c = [] {
        ShConfig c;
        if (const char* e = std::getenv("NOTSA_SH_FILE")) { c.file = e; }
        if (const char* e = std::getenv("NOTSA_SH_DUMP_FILE")) { c.dumpFile = e; }
        if (const char* e = std::getenv("NOTSA_SH_DUMP")) { for (const char* p = e; *p;) { char* end; const auto v = std::strtoul(p, &end, 0); if (end == p) break; c.dumpFrames.insert((uint32_t)v); p = *end ? end + 1 : end; } }
        if (const char* e = std::getenv("NOTSA_SH_DUMP_SUBSYS")) { std::string t = e; for (size_t a = 0; a <= t.size();) { size_t b = t.find(',', a); if (b == std::string::npos) b = t.size(); const auto n = t.substr(a, b - a); if (n == "all") c.dumpAll = true; else if (!n.empty()) c.dumpSubsys.insert(n); a = b + 1; } }
        return c;
    }();
    return c;
}
int                   g_isoDepth = 0;            // >0 inside a RandIsolate scope: rand() calls are neither counted nor logged
bool GpRand() { static const bool v = Active() && std::getenv("NOTSA_RR_GAMEPLAY_RAND") && std::getenv("NOTSA_RR_GAMEPLAY_RAND")[0] == '1'; return v; }
FILE*                 g_sh    = nullptr; // per-frame hash file
FILE*                 g_dumpF = nullptr;
std::atomic<uint64_t> g_randCount{ 0 };  // cumulative rand() calls (main thread on portable builds; every call on the single-state Win32 build); = the rand log's callidx
uint64_t RandCount() { return g_randCount.load(std::memory_order_relaxed); }

void CloseTrace() {
    if (g_sh) { std::fclose(g_sh); g_sh = nullptr; }
    if (g_dumpF) { std::fclose(g_dumpF); g_dumpF = nullptr; }
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
Result HashPool(Pool* pool, const char* kind, bool detail, uint32_t frame, std::string* out, Fn&& fields, const char* sub = nullptr, std::string* dump = nullptr) {
    Result r;
    if (!pool) return r;
    Acc total;
    for (size_t i = 0, sz = (size_t)pool->GetSize(); i < sz; i++) { // slot order = pool order (never pointer order)
        auto* obj = pool->GetAt(i);
        if (!obj) continue;
        std::string det;
        Sink s;
        s.det = detail ? &det : nullptr;
        if (dump && sub) { s.dump = dump; s.pfx = std::to_string(frame) + ' ' + sub + ' ' + std::to_string(i) + ':' + std::to_string((unsigned)obj->m_nModelIndex); }
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

// ---------------------------------------------------------------- SH4 NOTSA_SH_VEHLOG=<file>: unhashed vehicle + occupant dump at recorded frames 0 and 1 (see veh_f0.README.md next to the log)
// Rows: `frame randCount V ...` per vehicle (pool slot order), `frame randCount O ...` per occupant (driver, then passengers). key=value tokens, floats %.9g, ints decimal.
FILE* VehLogFile() {
    static FILE* f = [] {
        const char* p = std::getenv("NOTSA_SH_VEHLOG");
        FILE* h = p && p[0] ? std::fopen(p, "w") : nullptr;
        if (h) { std::fprintf(h, "# frame randCount V|O key=value... (see veh_f0.README.md); written after the statehash line of the logged frames (NOTSA_SH_VEHLOG_FRAMES, default 0-1); P rows = CCarCtrl::PossiblyRemoveVehicle calls during the frame\n"); }
        return h;
    }();
    return f;
}
// NOTSA_SH_VEHLOG_FRAMES ranges like "0-1,180-205" (default "0-1"); NOTSA_SH_VEHLOG_SLOT (optional) only that vehicle pool slot
bool VehLogFrameWanted(uint32_t frame) {
    static std::vector<std::pair<uint32_t, uint32_t>> r = [] {
        std::vector<std::pair<uint32_t, uint32_t>> v; const char* e = std::getenv("NOTSA_SH_VEHLOG_FRAMES"); std::string s = e && e[0] ? e : "0-1";
        size_t b = 0; while (b <= s.size()) { size_t c = s.find(',', b); if (c == std::string::npos) c = s.size(); const std::string t = s.substr(b, c - b); b = c + 1; if (t.empty()) continue; unsigned lo = 0, hi = 0; if (std::sscanf(t.c_str(), "%u-%u", &lo, &hi) == 2) v.push_back({ lo, hi }); else if (std::sscanf(t.c_str(), "%u", &lo) == 1) v.push_back({ lo, lo }); }
        return v; }();
    for (auto& p : r) if (frame >= p.first && frame <= p.second) return true;
    return false;
}
int VehLogSlot() { static const int s = [] { const char* e = std::getenv("NOTSA_SH_VEHLOG_SLOT"); return e && e[0] ? std::atoi(e) : -1; }(); return s; }
// SH7 NOTSA_SH_PEDMODELLOG=<file> (frames NOTSA_SH_PEDMODELLOG_FRAMES, default "0"): CStreaming::ms_pedsLoaded after the statehash line of the frame
void PedModelRows(uint32_t frame) {
    static FILE* f = [] { const char* p = std::getenv("NOTSA_SH_PEDMODELLOG"); FILE* h = p && p[0] ? std::fopen(p, "w") : nullptr;
        if (h) { std::fprintf(h, "# frame randCount N|M|Z ... | N: n=ms_numPedsLoaded loadedGangs gangCars zoneType popZoneType timeIdx timeOfWeek | M: slot model name loadState groups=popcycle ped groups that contain the model | Z: nextPedToLoadFromGroup[18]\n"); }
        return h; }();
    static int s_lo = -1, s_hi = -1;
    if (!f) return;
    if (s_lo < 0) { const char* e = std::getenv("NOTSA_SH_PEDMODELLOG_FRAMES"); unsigned a = 0, b = 0; if (e && std::sscanf(e, "%u-%u", &a, &b) == 2) { s_lo = (int)a; s_hi = (int)b; } else if (e && std::sscanf(e, "%u", &a) == 1) { s_lo = s_hi = (int)a; } else { s_lo = s_hi = 0; } }
    if ((int)frame < s_lo || (int)frame > s_hi) return;
    const unsigned long long rc = (unsigned long long)RandCount();
    std::fprintf(f, "%u %llu N n=%u loadedGangs=0x%x gangCars=0x%x zoneType=%d popZoneType=%d timeIdx=%d timeOfWeek=%d\n", frame, rc, (unsigned)CStreaming::ms_numPedsLoaded, (unsigned)CStreaming::ms_loadedGangs, (unsigned)CStreaming::ms_loadedGangCars,
        (int)CStreaming::ms_currentZoneType, (int)CPopCycle::m_nCurrentZoneType, (int)CPopCycle::m_nCurrentTimeIndex, (int)CPopCycle::m_nCurrentTimeOfWeek);
    for (unsigned i = 0; i < (unsigned)CStreaming::ms_pedsLoaded.size(); i++) {
        const int id = (int)CStreaming::ms_pedsLoaded[i];
        auto* mi = id >= 0 ? CModelInfo::GetModelInfo(id) : nullptr;
        std::string groups;
        for (int g = 0; g < (int)POPCYCLE_TOTAL_PEDGROUPS; g++) { for (auto m : CPopulation::GetModelsInPedGroup((ePopcyclePedGroup)g)) { if ((int)m == id) groups += std::to_string(g) + ","; } }
        std::fprintf(f, "%u %llu M slot=%u model=%d name=%s%s loadState=%d groups=%s\n", frame, rc, i, id, mi ? mi->GetModelNameAsString().c_str() : "-", i < CStreaming::ms_numPedsLoaded ? "" : " (inactive)", id >= 0 ? (int)CStreaming::GetInfo(id).m_LoadState : -1, groups.empty() ? "-" : groups.c_str());
    }
    std::fprintf(f, "%u %llu Z", frame, rc); for (auto v : CStreaming::ms_NextPedToLoadFromGroup) std::fprintf(f, " %d", (int)v); std::fprintf(f, "\n");
    std::fflush(f);
}
void VehLogRows(uint32_t frame) {
    static FILE* f = VehLogFile();
    if (!f || !VehLogFrameWanted(frame)) return;
    const unsigned long long rc = (unsigned long long)RandCount();
    const auto NA = [](const CNodeAddress& a) { return std::format("{}.{}", (int)a.m_wAreaId, (int)a.m_wNodeId); };
    const auto LA = [](const CCarPathLinkAddress& a) { return std::format("{}.{}", (int)a.m_wAreaId, (int)a.m_wCarPathLinkId); };
    const auto Name = [](int id) { auto* mi = id >= 0 ? CModelInfo::GetModelInfo(id) : nullptr; return mi ? mi->GetModelNameAsString() : std::string("-"); };
    const auto Vec = [](const CVector& v) { return std::format("{:.9g},{:.9g},{:.9g}", v.x, v.y, v.z); };
    const auto Slot = [](CPed* p) { return p ? (int)GetPedPool()->GetIndex(p) : -1; };
    auto* pool = GetVehiclePool();
    if (!pool) return;
    for (size_t i = 0, sz = (size_t)pool->GetSize(); i < sz; i++) {
        auto* v = pool->GetAt(i);
        if (!v || (VehLogSlot() >= 0 && (int)i != VehLogSlot())) continue;
        const auto& ap = v->m_autoPilot;
        std::string pn;
        for (auto& n : ap.m_aPathFindNodesInfo) { pn += NA(n) + ","; }
        std::string s = std::format("{} {} V slot={} sid={} model={} name={} r={} f={} u={} p={} createdBy={} status={} randomSeed={} forcedRandomRouteSeed={} col={},{},{},{} doorLock={} hp={:.9g} creationTime={} "
            "mv={} tv={} gas={:.9g} brk={:.9g} steer={:.9g} drv={} npass={} ",
            frame, rc, i, StableId(v), (int)v->m_nModelIndex, Name(v->m_nModelIndex), Vec(v->GetRight()), Vec(v->GetForward()), Vec(v->GetUp()), Vec(v->GetPosition()), (int)v->m_nCreatedBy, (int)v->GetStatus(), (unsigned)v->m_nRandomSeed, (int)v->m_nForcedRandomRouteSeed,
            (int)v->m_nPrimaryColor, (int)v->m_nSecondaryColor, (int)v->m_nTertiaryColor, (int)v->m_nQuaternaryColor, (int)v->m_nDoorLock, v->m_fHealth, (unsigned)v->m_nCreationTime,
            Vec(v->m_vecMoveSpeed), Vec(v->m_vecTurnSpeed), v->m_GasPedal, v->m_BrakePedal, v->m_fSteerAngle, Slot(v->m_pDriver), (int)v->m_nNumPassengers);
        s += std::format("ap.currentAddress={} ap.startingRouteNode={} ap.endingRouteNode={} ap.field_C={} ap.speedScaleFactor={} ap.curLink={} ap.nextLink={} ap.prevLink={} ap.smthPrev={} ap.smthCurr={} ap.smthNext={} ap.curLane={} ap.nextLane={} ",
            NA(ap.m_currentAddress), NA(ap.m_startingRouteNode), NA(ap.m_endingRouteNode), (int)ap.field_C, (unsigned)ap.m_nSpeedScaleFactor, LA(ap.m_nCurrentPathNodeInfo), LA(ap.m_nNextPathNodeInfo), LA(ap.m_nPreviousPathNodeInfo),
            (int)ap._smthPrev, (int)ap._smthCurr, (int)ap._smthNext, (int)ap.m_nCurrentLane, (int)ap.m_nNextLane);
        s += std::format("ap.drivingStyle={} ap.carMission={} ap.tempAction={} ap.tempActionTime={} ap.lastUpdateTimeMs={} ap.tempActionMode={} ap.carMissionModeCounter={} ap.timeToStartMission={} ap.timeSwitchedToRealPhysics={} ",
            (int)ap.m_nCarDrivingStyle, (int)ap.m_nCarMission, (int)ap.m_nTempAction, (unsigned)ap.m_nTempActionTime, (unsigned)ap.m_LastUpdateTimeMs, (int)ap.m_ucTempActionMode, (int)ap.m_ucCarMissionModeCounter, (unsigned)ap.m_nTimeToStartMission, (unsigned)ap.m_nTimeSwitchedToRealPhysics);
        s += std::format("ap.speed={:.9g} ap.maxTrafficSpeed={:.9g} ap.cruiseSpeed={} ap.speedMult={:.9g} ap.heliTargetDist={} ap.heliSpeedMult={} ap.carCtrlFlags={} ap.movementFlags={} ap.straightLineDistance={} ap.carFollowDist={} ap.heliTargetDist2={} ",
            ap.m_speed, ap.m_fMaxTrafficSpeed, (int)ap.m_nCruiseSpeed, ap.m_SpeedMult, (int)ap.m_ucHeliTargetDist, (int)ap.m_ucHeliSpeedMult, (int)ap.m_nCarCtrlFlags, (int)ap.m_nMovementFlags, (int)ap.m_nStraightLineDistance, (int)ap.m_ucCarFollowDist, (int)ap.m_ucHeliTargetDist2);
        s += std::format("ap.dest={} ap.pathNodesCount={} ap.pathNodes={} ap.targetEntity={} ap.obstructingEntity={} ap.recordingId={} ap.planeDogfight={} ap.field_96={} ap.field_50={}\n",
            Vec(ap.m_vecDestinationCoors), (int)ap.m_nPathFindNodesCount, pn, ap.m_TargetEntity ? (int)GetVehiclePool()->GetIndex(ap.m_TargetEntity) : -1, ap.m_ObstructingEntity ? (int)StableId(ap.m_ObstructingEntity) : -1,
            (int)ap.m_vehicleRecordingId, (int)ap.m_bPlaneDogfightSomething, (int)ap.field_96, (int)ap.field_50);
        std::fputs(s.c_str(), f);
        const auto Occ = [&](CPed* ped, const std::string& seat) {
            if (!ped) return;
            auto* im = ped->GetIntelligence();
            std::string o = std::format("{} {} O veh={} seat={} slot={} sid={} model={} name={} pedType={} createdBy={} randomSeed={} dmType={} dmTypeInGroup={} hp={:.9g} pos={} bInVehicle={} ", frame, rc, i, seat, Slot(ped), StableId(ped), (int)ped->m_nModelIndex, Name(ped->m_nModelIndex),
                (int)ped->m_nPedType, (int)ped->GetCreatedBy(), (unsigned)ped->m_nRandomSeed, im->m_nDecisionMakerType, im->m_nDecisionMakerTypeInGroup, ped->m_fHealth, Vec(ped->GetPosition()), (int)ped->bInVehicle);
            auto& tm = im->GetTaskManager();
            o += "primary=";
            for (int slotI = 0; slotI < TASK_PRIMARY_MAX; slotI++) {
                o += std::format("{}:", slotI);
                int n = 0;
                for (auto* t = tm.GetTaskPrimary(slotI); t && n < 12; t = t->GetSubTask(), n++) { o += std::format("{}{}", n ? ">" : "", (int)t->GetTaskType()); }
                if (!tm.GetTaskPrimary(slotI)) { o += "-"; }
                o += slotI + 1 < TASK_PRIMARY_MAX ? "," : "";
            }
            o += " secondary=";
            for (int slotI = 0; slotI < TASK_SECONDARY_MAX; slotI++) {
                auto* t = tm.GetTaskSecondary((eSecondaryTask)slotI);
                o += std::format("{}:{}{}", slotI, t ? (int)t->GetTaskType() : -1, slotI + 1 < TASK_SECONDARY_MAX ? "," : "");
            }
            // every field of the car-drive task classes found anywhere in the primary chains
            for (int slotI = 0; slotI < TASK_PRIMARY_MAX; slotI++) {
                for (auto* t = tm.GetTaskPrimary(slotI); t; t = t->GetSubTask()) {
                    const auto ty = t->GetTaskType();
                    const auto Drive = [&](CTaskComplexCarDrive* d, const char* nm) {
                        o += std::format(" [{} veh={} cruiseSpeed={:.9g} desiredModel={} drivingStyle={} asDriver={} origStyle={} origMission={} origSpeed={} carSetUp={}]", nm, d->m_Veh ? (int)GetVehiclePool()->GetIndex(d->m_Veh) : -1, d->m_CruiseSpeed, (int)d->m_DesiredCarModel,
                            (int)d->m_CarDrivingStyle, (int)d->m_bAsDriver, (int)d->m_OriginalDrivingStyle, (int)d->m_OriginalMission, (int)d->m_OriginalSpeed, (int)d->m_bIsCarSetUp);
                    };
                    if (ty == TASK_COMPLEX_CAR_DRIVE) { Drive(static_cast<CTaskComplexCarDrive*>(t), "CarDrive"); }
                    else if (ty == TASK_COMPLEX_CAR_DRIVE_WANDER) { Drive(static_cast<CTaskComplexCarDriveWander*>(t), "CarDriveWander"); }
                    else if (ty == TASK_SIMPLE_CAR_DRIVE || ty == TASK_SIMPLE_CAR_DRIVE_TIMED) {
                        auto* d = static_cast<CTaskSimpleCarDrive*>(t);
                        const auto Tm = [](const CTaskTimer& x) { return std::format("{}/{}/{}/{}", (unsigned)x.m_nStartTime, (int)x.m_nInterval, (int)x.m_bStarted, (int)x.m_bStopped); };
                        o += std::format(" [SimpleCarDrive{} veh={} noDriverTimer={} boppingStartTime={} field_24={} boppingEndTime={} boppingProgress={:.9g} boppingCompletedTimes={} headBoppingStartTime={} headBoppingDirection={} headBoppingOrientation={:.9g} randomHeadBoppingMultiplier={:.9g} headBoppingFactor={:.9g} armBoppingStartTime={} timePassedSinceCarUpSideDown={} copCarStolenTimer={} flags={}",
                            ty == TASK_SIMPLE_CAR_DRIVE_TIMED ? "Timed" : "", d->m_pVehicle ? (int)GetVehiclePool()->GetIndex(d->m_pVehicle) : -1, Tm(d->m_NoDriverTimer), d->m_nBoppingStartTime, d->field_24, d->m_nBoppingEndTime, d->m_fBoppingProgress, d->m_nBoppingCompletedTimes,
                            d->m_nHeadBoppingStartTime, d->m_nHeadBoppingDirection, d->m_fHeadBoppingOrientation, d->m_fRandomHeadBoppingMultiplier, d->m_fHeadBoppingFactor, d->m_nArmBoppingStartTime, d->m_nTimePassedSinceCarUpSideDown, Tm(d->m_copCarStolenTimer),
                            (int)d->m_bHeadBopping | ((int)d->m_bArmBopping << 1) | ((int)d->m_bUpdateCurrentVehicle << 2) | ((int)d->m_b08 << 3) | ((int)d->m_bPassengerAnimBlended << 4) | ((int)d->m_bClosingDoor << 5));
                        if (ty == TASK_SIMPLE_CAR_DRIVE_TIMED) { auto* td = static_cast<CTaskSimpleCarDriveTimed*>(t); o += std::format(" time={} timer={}", td->m_nTime, Tm(td->m_nTimer)); }
                        o += "]";
                    }
                }
            }
            o += "\n";
            std::fputs(o.c_str(), f);
        };
        Occ(v->m_pDriver, "driver");
        for (int p = 0; p < (int)v->m_apPassengers.size(); p++) { Occ(v->m_apPassengers[p], std::to_string(p)); }
    }
    std::fflush(f);
}

// ---------------------------------------------------------------- the per-frame record
void Record() {
    const bool detail = (g_frame >= Cfg().detailFrom && g_frame <= Cfg().detailTo);
    std::string det;
    std::string* d = detail ? &det : nullptr;
    char line[64];
    // SH1 verbose dump (NOTSA_SH_DUMP / NOTSA_SH_DUMP_SUBSYS): rows of every hashed word of this frame
    const auto& sh = ShCfg();
    const bool dumpFrame = sh.dumpFrames.count(g_frame) != 0;
    std::string dumpRows;
    const auto DumpSel = [&](const char* sub) { return dumpFrame && (sh.dumpAll || sh.dumpSubsys.count(sub)); };
    const auto Setup = [&](Sink& s, const char* sub) {
        s.det = d;
        if (DumpSel(sub)) { s.dump = &dumpRows; s.pfx = std::to_string(g_frame) + ' ' + sub + " -"; }
    };

    // timer + clock + rng (integers only)
    Acc tm;
    { Sink s; Setup(s, "tm"); if (d) *d += "D " + std::to_string(g_frame) + " timer";
      s.U("fc", CTimer::GetFrameCounter()); s.U("ms", CTimer::GetTimeInMS()); s.U("msnc", CTimer::GetTimeInMSNonClipped()); s.U("mspause", CTimer::GetTimeInMSPauseMode());
      s.F("step", CTimer::GetTimeStep()); s.F("scale", CTimer::GetTimeScale()); s.U("paused", CTimer::GetIsPaused() ? 1u : 0u);
      if (d) *d += '\n'; tm = s.acc; }
    Acc ck;
    { Sink s; Setup(s, "ck"); if (d) *d += "D " + std::to_string(g_frame) + " clock";
      s.U("h", CClock::GetGameClockHours()); s.U("m", CClock::GetGameClockMinutes()); s.U("s", CClock::GetGameClockSeconds()); s.U("d", CClock::GetGameClockDays());
      s.U("wnew", (uint32_t)CWeather::NewWeatherType); s.U("wold", (uint32_t)CWeather::OldWeatherType);
      if (d) *d += '\n'; ck = s.acc; }
    const uint32_t rng = RngState();
    if (d) { std::snprintf(line, sizeof line, "D %u rng state=%x\n", g_frame, rng); *d += line; }

    // world counters (population / traffic / streaming)
    Acc pop;
    { Sink s; Setup(s, "pop"); if (d) *d += "D " + std::to_string(g_frame) + " pop";
      s.I("peds", CPopulation::ms_nTotalPeds); s.I("civ", CPopulation::ms_nTotalCivPeds); s.I("gang", CPopulation::ms_nTotalGangPeds); s.I("cop", CPopulation::ms_nNumCop);
      s.I("rcars", CCarCtrl::NumRandomCars); s.I("law", CCarCtrl::NumLawEnforcerCars); s.I("mcars", CCarCtrl::NumMissionCars); s.I("parked", CCarCtrl::NumParkedCars);
      s.U("stmem", CStreaming::ms_memoryUsedBytes); s.I("streq", CStreaming::ms_numModelsRequested);
      s.U("npedpool", GetPedPool() ? (uint32_t)GetPedPool()->GetNoOfUsedSpaces() : 0u); s.U("nvehpool", GetVehiclePool() ? (uint32_t)GetVehiclePool()->GetNoOfUsedSpaces() : 0u);
      if (d) *d += '\n'; pop = s.acc; }

    // player (ped 0 of CWorld::Players[0]), wanted level, money; camera
    Acc pl;
    if (auto* p = FindPlayerPed()) {
        Sink s; Setup(s, "pl"); if (d) *d += "D " + std::to_string(g_frame) + " player";
        PedFields(s, *p);
        auto* w = FindPlayerWanted();
        s.I("wl", w ? (int32_t)w->GetWantedLevel() : -1); s.I("chaos", w ? w->m_ChaosLevel : -1);
        s.I("money", FindPlayerInfo().m_nMoney);
        if (d) *d += '\n'; pl = s.acc;
    }
    Acc cam;
    { Sink s; Setup(s, "cam"); if (d) *d += "D " + std::to_string(g_frame) + " camera";
      s.V("p", TheCamera.GetPosition()); s.V("f", TheCamera.GetForward());
      if (d) *d += '\n'; cam = s.acc; }

    std::string ents;
    const auto peds = HashPool(GetPedPool(), "ped", detail, g_frame, &ents, PedFields, "pd", DumpSel("pd") ? &dumpRows : nullptr);
    const auto vehs = HashPool(GetVehiclePool(), "veh", detail, g_frame, &ents, VehicleFields, "vh", DumpSel("vh") ? &dumpRows : nullptr);
    const auto objs = HashPool(GetObjectPool(), "obj", detail, g_frame, &ents, ObjectFields, "ob", DumpSel("ob") ? &dumpRows : nullptr);

    Acc all;
    Acc rngA;
    { Sink s; Setup(s, "rng"); s.det = nullptr; s.U("state", rng); rngA = s.acc; }
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
    if (g_sh) { // SH1 per-frame hash file: frame randCount h_tm h_ck h_rng h_pop h_pl h_cam h_pd h_vh h_ob h_all (e hashes)
        std::fprintf(g_sh, "%u %llu %s %s %s %s %s %s %s %s %s %s\n", g_frame, (unsigned long long)RandCount(), HE(tm).c_str(), HE(ck).c_str(), HE(rngA).c_str(), HE(pop).c_str(), HE(pl).c_str(), HE(cam).c_str(),
            HE(peds.acc).c_str(), HE(vehs.acc).c_str(), HE(objs.acc).c_str(), HE(all).c_str());
        std::fflush(g_sh);
    }
    PedModelRows(g_frame); // SH7 NOTSA_SH_PEDMODELLOG
    VehLogRows(g_frame); // SH4 NOTSA_SH_VEHLOG: after the statehash line of frames 0/1
    if (dumpFrame && !dumpRows.empty()) {
        if (!g_dumpF) { g_dumpF = std::fopen(sh.dumpFile.c_str(), "w"); }
        if (g_dumpF) { std::fwrite(dumpRows.data(), 1, dumpRows.size(), g_dumpF); std::fflush(g_dumpF); }
    }
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
    if (!ShCfg().file.empty()) {
        g_sh = std::fopen(ShCfg().file.c_str(), "w");
        if (g_sh) { std::fprintf(g_sh, "#SH 1 build=%s seed=%u gprand=%d subsys=tm,ck,rng,pop,pl,cam,pd,vh,ob,all\n", Cfg().tag.empty() ? "unset" : Cfg().tag.c_str(), seed, GpRand() ? 1 : 0); std::fflush(g_sh); }
        else { NOTSA_LOG_ERR("rr_start: cannot open NOTSA_SH_FILE '{}'", ShCfg().file); }
    }
    BootLogRow('S'); // SH5 NOTSA_SH_BOOTLOG: the state at the rr_start pump
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
bool VehLogWants(const void* veh) { // SH4: CCarCtrl::PossiblyRemoveVehicle rows: a recording frame in NOTSA_SH_VEHLOG_FRAMES and the slot (if NOTSA_SH_VEHLOG_SLOT is set)
    if (!VehLogFile() || !g_recording || !VehLogFrameWanted(g_frame)) return false;
    return VehLogSlot() < 0 || (veh && (int)GetVehiclePool()->GetIndex(const_cast<CVehicle*>(static_cast<const CVehicle*>(veh))) == VehLogSlot());
}
void VehLogPrv(const void* veh, const std::string& kv) {
    if (!VehLogWants(veh)) return;
    std::fprintf(VehLogFile(), "%u %llu P slot=%d %s\n", g_frame, (unsigned long long)RandCount(), (int)GetVehiclePool()->GetIndex(const_cast<CVehicle*>(static_cast<const CVehicle*>(veh))), kv.c_str()); std::fflush(VehLogFile());
}
int      LoadLogRecFrame() { return g_recording ? (int)g_frame : -1; }
uint64_t RandCountNow() { return RandCount(); }
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

// ---------------------------------------------------------------- SH5 debug logs (STATEHASH builds only)
// NOTSA_SH_AUDIOLOG=<file> [NOTSA_SH_AUDIOLOG_FRAMES=54 | a-b,c ...]: audio events and sound requests of the given RECORDED frames (frame column = the rand log's frame column).
//   Row (written when the scope EXITS, so an event row follows the rows of its own sounds; `seq` is the entry order, `depth` the nesting):
//   `frame seq depth kind event owner bank sound x rc0 rc1 draws callers`
//   kind: V CAEVehicleAudioEntity::AddAudioEvent(event,float) | P ...(event,CPhysical*) | D CAEPedAudioEntity::AddAudioEvent | R CAESoundManager::RequestNewSound (event = CAESound::m_Event) | S CAESoundManager::PlaySound (event = EventID)
//   owner = `<pool kind>:<slot>:<model>` of the audio entity's m_Entity (v = vehicle, p = ped, o = object, -:-:- none), bank = bank slot, sound = sound id (R / S rows; -1 for the events), x = the float p1 / volume (events) or the sound position x (R/S)
//   rc0 / rc1 = RandCountNow() at entry / exit, draws = rc1-rc0; callers = return addresses (offsets from StableId, hex) of 4 frames: the first is inside the logged function itself, then its callers
// NOTSA_SH_BOOTLOG=<file>: one row per game frame from the first frame the `until:control` condition holds (kind F, written at OnFrame = end of the simulation part) up to cmd:rr_start (kind S, written when the command runs, i.e. before the Idle of that frame):
//   `kind fc x y z xbits ybits zbits ts tsbits ms msNC rc vms` (CJ position / ms_fTimeStep as %.9g and raw uint32 bits, CTimer ms and NonClipped ms, rand count, virtual ms)
namespace {
struct AudioLogCfg {
    FILE* f = nullptr; std::set<int> frames; bool init = false;
    void Init() {
        if (init) return; init = true;
        const char* p = std::getenv("NOTSA_SH_AUDIOLOG");
        if (!p || !p[0]) return;
        f = std::fopen(p, "w");
        if (!f) return;
        std::fprintf(f, "# frame seq depth kind(V|P|D|R|S) event owner(kind:slot:model) bank sound x rc0 rc1 draws callers(offsets from StableId, hex) | rows are written at scope exit; see sh5.README.md\n");
        const char* fr = std::getenv("NOTSA_SH_AUDIOLOG_FRAMES"); std::string str = fr && fr[0] ? fr : "54";
        size_t i = 0;
        while (i < str.size()) {
            size_t j = str.find(',', i); if (j == std::string::npos) j = str.size();
            const std::string t = str.substr(i, j - i); i = j + 1;
            const size_t d = t.find('-');
            const int a = std::atoi(t.c_str()), b = d == std::string::npos ? a : std::atoi(t.c_str() + d + 1);
            for (int k = a; k <= b && k - a < 100000; k++) frames.insert(k);
        }
    }
};
AudioLogCfg& ALC() { static AudioLogCfg c; c.Init(); return c; }
int g_alDepth = 0, g_alSeq = 0;
}
#ifdef _WIN32
__declspec(noinline)
#endif
AudioLogScope::AudioLogScope(char kind, int event, const void* ae, int bank, int sound, float x) {
    auto& c = ALC();
    if (!c.f || !g_recording || !c.frames.count((int)g_frame) || std::this_thread::get_id() != ::notsa::msvc_crt::MainThreadId()) return;
    m_on = true; m_kind = kind; m_event = event; m_ae = ae; m_bank = bank; m_sound = sound; m_x = x; m_seq = g_alSeq++; m_depth = g_alDepth++; m_rc0 = RandCount();
#ifdef _WIN32
    void* bt[6] = {}; const USHORT n = RtlCaptureStackBackTrace(1, 4, bt, nullptr); // skip = this constructor: bt[0] = return address inside the function that declared the scope
    static const uintptr_t base = (uintptr_t)&StableId;
    for (USHORT i = 0; i < n && i < 4; i++) m_ra[i] = (uintptr_t)bt[i] - base;
#endif
}
AudioLogScope::~AudioLogScope() {
    if (!m_on) return;
    g_alDepth--;
    auto& c = ALC();
    char owner[48] = "-:-:-";
    if (m_ae) {
        if (const CEntity* e = static_cast<const CAEAudioEntity*>(m_ae)->m_Entity) {
            const uint32_t id = StableId(e);
            const char k = (id >> 24) == 1 ? 'p' : (id >> 24) == 2 ? 'v' : (id >> 24) == 3 ? 'o' : '?';
            std::snprintf(owner, sizeof owner, "%c:%u:%d", k, id & 0xFFFFFFu, (int)e->m_nModelIndex);
        }
    }
    const uint64_t rc1 = RandCount();
    std::fprintf(c.f, "%u %d %d %c %d %s %d %d %.9g %llu %llu %llu %llx,%llx,%llx,%llx\n", g_frame, m_seq, m_depth, m_kind, m_event, owner, m_bank, m_sound, (double)m_x,
        (unsigned long long)m_rc0, (unsigned long long)rc1, (unsigned long long)(rc1 - m_rc0), (unsigned long long)m_ra[0], (unsigned long long)m_ra[1], (unsigned long long)m_ra[2], (unsigned long long)m_ra[3]);
    std::fflush(c.f);
}

FILE* WalkLogFile() {
    static FILE* f = nullptr; static std::set<int> frames; static bool init = false;
    if (!init) {
        init = true;
        const char* p = std::getenv("NOTSA_SH_WALKLOG");
        if (p && p[0] && (f = std::fopen(p, "w"))) {
            std::fprintf(f, "# frame randCount W posn low=(area,node)(area,node) normal=... arg12 arg13 ... | A loaded path areas (once per frame) | S start node | H hop# cur=(a,n) numLinks link next=(a,n) accum(after hop) | X hops accum reason (reason: start_unloaded cur_unloaded dist_230 dead_end dead_end_lead wrong_side 30_nodes ok)\n");
            const char* fr = std::getenv("NOTSA_SH_WALKLOG_FRAMES"); std::string str = fr && fr[0] ? fr : "54";
            size_t i = 0;
            while (i < str.size()) { size_t j = str.find(',', i); if (j == std::string::npos) j = str.size(); const std::string t = str.substr(i, j - i); i = j + 1; const size_t d = t.find('-'); const int a = std::atoi(t.c_str()), b = d == std::string::npos ? a : std::atoi(t.c_str() + d + 1); for (int k = a; k <= b && k - a < 100000; k++) frames.insert(k); }
        }
    }
    return (f && g_recording && frames.count((int)g_frame) && std::this_thread::get_id() == ::notsa::msvc_crt::MainThreadId()) ? f : nullptr;
}

void BootLogRow(char kind) {
    static FILE* f = [] { const char* p = std::getenv("NOTSA_SH_BOOTLOG"); FILE* h = p && p[0] ? std::fopen(p, "w") : nullptr; if (h) { std::fprintf(h, "# kind(F frame end|S rr_start) fc x y z xbits ybits zbits ts tsbits ms msNonClipped randCount virtualMs | F rows start at the first OnFrame where `until:control` holds\n"); } return h; }();
    static bool s_latched = false;
    if (!f || g_recording) return;
    auto* ped = FindPlayerPed();
    if (!s_latched) {
        auto* pad = CPad::GetPad(0);
        s_latched = ped && pad && !FrontEndMenuManager.m_bMenuActive && !CCutsceneMgr::IsRunning() && !pad->bPlayerSafe && !pad->ArePlayerControlsDisabled() && !TheCamera.GetFading();
    }
    if (!s_latched && kind != 'S') return;
    const CVector p = ped ? ped->GetPosition() : CVector{};
    const float ts = CTimer::GetTimeStep();
    std::fprintf(f, "%c %u %.9g %.9g %.9g %08x %08x %08x %.9g %08x %u %u %llu %llu\n", kind, CTimer::GetFrameCounter(), p.x, p.y, p.z, std::bit_cast<uint32_t>(p.x), std::bit_cast<uint32_t>(p.y), std::bit_cast<uint32_t>(p.z),
        ts, std::bit_cast<uint32_t>(ts), CTimer::GetTimeInMS(), CTimer::GetTimeInMSNonClipped(), (unsigned long long)RandCount(), (unsigned long long)VirtualMs());
    std::fflush(f);
}

// ---- SH1 rand() call log (NOTSA_SH_RANDLOG=<file>; NOTSA_RR_RANDLOG is accepted as an alias): one row per rand() call, `callidx frame value caller`
//   callidx = cumulative call number since process start (0-based; same counter as `randCount` of the per-frame hash file), frame = recorded frame number (-1 before cmd:rr_start / after stop),
//   value = the value rand() returned (0..32767), caller = see below. All rand() calls of the game go through the `rand()` macro (GameRand.h / msvc_rand.h) and so through this hook.
namespace {
struct RandLog {
    FILE*          f = nullptr;
    std::thread::id main;
    RandLog() {
        const char* p = std::getenv("NOTSA_SH_RANDLOG");
        if (!p) { p = std::getenv("NOTSA_RR_RANDLOG"); }
        if (p) { f = std::fopen(p, "w"); }
#ifdef NOTSA_PORTABLE
        main = ::notsa::msvc_crt::MainThreadId();
#endif
    }
    ~RandLog() { if (f) { std::fclose(f); f = nullptr; } }
};
RandLog& RL() { static RandLog r; return r; }
inline uint32_t RandVal(uint32_t stateBefore) { return ((stateBefore * 214013u + 2531011u) >> 16) & 0x7FFF; }
} // namespace

#ifdef NOTSA_PORTABLE
// caller = up to 5 return addresses (frame-pointer chain) as offsets from `StableId` (stable across ASLR; resolve offline with nm / atos), hex, comma separated
#ifdef _WIN32
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void RandHook(uint32_t stateBefore) {
    if (g_isoDepth > 0) return;
    auto& r = RL();
    if (std::this_thread::get_id() != ::notsa::msvc_crt::MainThreadId()) return; // worker threads have their own LCG state and are not part of the game stream
    const uint64_t idx = g_randCount.fetch_add(1, std::memory_order_relaxed);
    if (!r.f) return;
    static const uintptr_t base = (uintptr_t)&StableId;
    uintptr_t ra[5] = {};
#ifdef _WIN32 // SH2 PortableWin64: x64 unwind tables (RtlCaptureStackBackTrace), frame 0 = the caller of RandHook
    { void* bt[6] = {}; const USHORT n = RtlCaptureStackBackTrace(1, 5, bt, nullptr); for (USHORT i = 0; i < n && i < 5; i++) { ra[i] = (uintptr_t)bt[i] - base; } }
#else
    void**    fp    = (void**)__builtin_frame_address(0); // arm64 / x64 clang keep the frame-pointer chain: [fp] = caller's fp, [fp+1] = return address
    for (int i = 0; i < 5 && fp && ((uintptr_t)fp & 7) == 0; i++) { ra[i] = (uintptr_t)fp[1] - base; fp = (void**)fp[0]; }
#endif
    std::fprintf(r.f, "%llu %d %u %llx,%llx,%llx,%llx,%llx\n", (unsigned long long)idx, g_recording ? (int)g_frame : -1, RandVal(stateBefore), (unsigned long long)ra[0], (unsigned long long)ra[1], (unsigned long long)ra[2], (unsigned long long)ra[3], (unsigned long long)ra[4]);
}
#else
// Win32 (single global LCG state, GameRand.h): caller = `file:line@rva[+wrap]`: source location of the `rand()` macro use, rva = return address of the hook (module-relative; the exe is linked /BASE:0x400000, resolve
// with tools/standalone/statehash/randlog_resolve.py + the linker .map), `+wrap` = module-relative return address of CGeneral::GetRandomNumber (the wrapper hides its caller, General.cpp), if the call came through it.
const void* g_wrapCaller = nullptr;
__declspec(noinline) void RandHookLoc(uint32_t stateBefore, const char* file, unsigned line) {
    if (g_isoDepth > 0) return;
    auto& r = RL();
    const uint64_t idx = g_randCount.fetch_add(1, std::memory_order_relaxed);
    if (!r.f) return;
    static const uintptr_t modBase = (uintptr_t)GetModuleHandleW(nullptr);
    const char* slash = std::strrchr(file, '\\');
    const char* fn    = slash ? slash + 1 : file;
    const uintptr_t ra = (uintptr_t)_ReturnAddress() - modBase;
    if (!std::strcmp(fn, "General.cpp") && g_wrapCaller) { // came through CGeneral::GetRandomNumber: log the wrapper's caller too
        std::fprintf(r.f, "%llu %d %u %s:%u@%x+%x\n", (unsigned long long)idx, g_recording ? (int)g_frame : -1, RandVal(stateBefore), fn, line, (unsigned)ra, (unsigned)((uintptr_t)g_wrapCaller - modBase));
    } else {
        std::fprintf(r.f, "%llu %d %u %s:%u@%x\n", (unsigned long long)idx, g_recording ? (int)g_frame : -1, RandVal(stateBefore), fn, line, (unsigned)ra);
    }
}
void SetWrapCaller(const void* ra) { g_wrapCaller = ra; }
#endif

#if defined(_WIN32) && defined(NOTSA_PORTABLE)
// P4C1: NOTSA_CRASHLOG=<file>: a vectored exception handler writing the exception code, the faulting address and an x64-unwound stack (RtlVirtualUnwind from the fault context) as offsets from `StableId` (resolve with the .map)
namespace {
LONG WINAPI CrashVeh(PEXCEPTION_POINTERS ep) {
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_ARRAY_BOUNDS_EXCEEDED && code != EXCEPTION_PRIV_INSTRUCTION) return EXCEPTION_CONTINUE_SEARCH;
    static volatile LONG once = 0;
    if (InterlockedExchange(&once, 1)) return EXCEPTION_CONTINUE_SEARCH;
    const char* path = std::getenv("NOTSA_CRASHLOG");
    FILE* f = path ? std::fopen(path, "w") : nullptr;
    if (!f) return EXCEPTION_CONTINUE_SEARCH;
    const uintptr_t base = (uintptr_t)&StableId;
    std::fprintf(f, "code=%08lx fault_ip=%llx", (unsigned long)code, (unsigned long long)((uintptr_t)ep->ExceptionRecord->ExceptionAddress - base));
    if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) std::fprintf(f, " av_kind=%llu av_addr=%llx", (unsigned long long)ep->ExceptionRecord->ExceptionInformation[0], (unsigned long long)ep->ExceptionRecord->ExceptionInformation[1]);
    std::fprintf(f, "\nframes:");
    CONTEXT ctx = *ep->ContextRecord;
    for (int i = 0; i < 24 && ctx.Rip; i++) {
        std::fprintf(f, " %llx", (unsigned long long)(ctx.Rip - base));
        DWORD64 imageBase = 0;
        auto* rf = RtlLookupFunctionEntry(ctx.Rip, &imageBase, nullptr);
        if (rf) { PVOID hd = nullptr; DWORD64 ef = 0; RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, ctx.Rip, rf, &ctx, &hd, &ef, nullptr); }
        else { ctx.Rip = *(DWORD64*)ctx.Rsp; ctx.Rsp += 8; }
    }
    std::fprintf(f, "\n");
    std::fclose(f);
    return EXCEPTION_CONTINUE_SEARCH;
}
struct CrashVehInit { CrashVehInit() { if (std::getenv("NOTSA_CRASHLOG")) AddVectoredExceptionHandler(1, CrashVeh); } } g_crashVehInit;
} // namespace
#endif

// NOTSA_RR_GAMEPLAY_RAND=1: everything inside a RandIsolate scope (the render part of Idle, AudioEngine.Service) runs with the rand state saved on entry and restored on exit, and is not counted / logged
RandIsolate::RandIsolate() { if (GpRand()) { m_on = true; m_saved = RngState(); g_isoDepth++; } }
RandIsolate::~RandIsolate() { if (m_on) { g_isoDepth--; RngSeed(m_saved); } }

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
    { // the screen size is part of the simulation (CCamera::CalculateFrustumPlanes uses its aspect ratio -> which entities are visible -> CAutomobile::Render -> when a car may be removed): pin it on both sides.
      // NOTSA_RR_SCREEN=WxH (default 1280x720); the native window / the Wine desktop would otherwise decide (FLOOP3: 2560x1440 vs 1512x982 removed a car one frame apart).
        static const std::pair<int, int> scr = [] { int w = 1280, h = 720; if (const char* e = std::getenv("NOTSA_RR_SCREEN")) { std::sscanf(e, "%dx%d", &w, &h); } return std::pair<int, int>{ w, h }; }();
        if (!FrontEndMenuManager.m_bMenuActive && CTimer::GetFrameCounter() > 0) { // not in the menus: the front end hit-tests the pointer against the real size
            RsGlobal.maximumWidth  = scr.first;
            RsGlobal.maximumHeight = scr.second;
        }
    }
    g_vns.fetch_add((uint64_t)Cfg().stepMs * 1'000'000ull, std::memory_order_relaxed);
#if defined(NOTSA_PORTABLE) && !defined(_WIN32)
    notsa::portable::dsal::RrPump(Cfg().stepMs); // the software DirectSound mixer: cursors / playing state advance with game time (the audio entities read them and draw random numbers on that basis)
#endif
}

void OnFrame() {
    BootLogRow('F'); // SH5 NOTSA_SH_BOOTLOG
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
#if defined(NOTSA_PORTABLE) && !defined(_WIN32)
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
    if (g_recording && g_f) { // NOTSA_RR_LOG_PLAYER=A-B (recorded frames): the player's anim-driven movement state + every animation association, bit patterns (diff the Win32 and the native log)
        static const std::pair<uint32_t, uint32_t> pr = [] { unsigned a = 1, b = 0; if (const char* e = std::getenv("NOTSA_RR_LOG_PLAYER")) { std::sscanf(e, "%u-%u", &a, &b); if (!b) b = a; } else { a = 1; b = 0; } return std::pair<uint32_t, uint32_t>{ a, b }; }();
        if (g_frame >= pr.first && g_frame <= pr.second) {
            if (auto* pl = FindPlayerPed()) {
                auto B = [](float f) { return std::bit_cast<uint32_t>(f); };
                std::string l = std::format("rrpl {} shl={:08x},{:08x} sh={:08x},{:08x} mv={:08x},{:08x},{:08x} rot={:08x}", g_frame, B(pl->m_vecAnimMovingShiftLocal.x), B(pl->m_vecAnimMovingShiftLocal.y), B(pl->m_vecAnimMovingShift.x), B(pl->m_vecAnimMovingShift.y),
                    B(pl->m_vecMoveSpeed.x), B(pl->m_vecMoveSpeed.y), B(pl->m_vecMoveSpeed.z), B(pl->m_fCurrentRotation));
                if (pl->GetRpClump()) {
                    for (auto* a = RpAnimBlendClumpGetFirstAssociation(pl->GetRpClump()); a; a = RpAnimBlendGetNextAssociation(a)) {
                        l += std::format(" [id{} t={:08x} bl={:08x} bd={:08x} sp={:08x} ts={:08x} fl={:x}]", (int)a->m_AnimId, B(a->m_CurrentTime), B(a->m_BlendAmount), B(a->m_BlendDelta), B(a->m_Speed), B(a->m_TimeStep), (unsigned)a->m_Flags);
                    }
                }
                NOTSA_LOG_INFO("{}", l);
            }
        }
    }
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
