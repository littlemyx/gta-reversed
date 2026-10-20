// ORACLE-W3-GP2: exe-oracle rows for the A + B functions of game_sa/TrafficLights.cpp (light phases from the game clock), Restart.cpp (hospital / police restart points), Shopping.cpp (price table,
// owned items, stat modifiers), Attractors/PedAttractor.cpp (queues of peds at bus stops / ATMs / benches) and the vehicle-pool leftovers of World.cpp. Included by wave2_oracle_all.inc when W3_GP2_ONLY
// is defined (executable wave3_gp2_oracle_test = the W2 harness: deterministic arena, the whole exe data image + the fixture block compared after every case).
// Fixtures: the exe's globals at their exe addresses (clock, cheat flags, model indices of the traffic-light models, restart tables, price / item tables, ped-attractor task list), a fixture block
// holding raw CEntity / CMatrix / CPedAttractor blobs with MSVC6-vector members (SArray) over fake ped / task pointers, the entity reference world of the W2 harness for the vehicle pool.
// Callees that are not the function under test are the same host stubs on both sides (patched into the exe, defined as the port's symbol in this TU): CGameLogic::LaRiotsActiveHere (0x441C10),
// CTheZones::GetLevelFromPosition (0x572300), CStats::GetStatValue (0x558E40), CStats::ModifyStat (0x55D090).
#include "TrafficLights.h"
#include "Restart.h"
#include "Shopping.h"
#include "Attractors/PedAttractor.h"
#include "GameLogic.h"
#include "TheZones.h"
#include "Stats.h"
#include "Cheat.h"
#include "ModelIndices.h"
#include "World.h"

// steal the private statics: explicit-instantiation access trick
#define TL_STEAL(NAME, SIG) \
struct T_##NAME { using type = SIG; friend type tl_steal(T_##NAME); }; \
template<class Tag, typename Tag::type P> struct R_##NAME { friend typename Tag::type tl_steal(Tag) { return P; } }; \
template struct R_##NAME<T_##NAME, &CTrafficLights::NAME>;
TL_STEAL(LightForCars1, uint8 (*)())
TL_STEAL(LightForCars2, uint8 (*)())
TL_STEAL(LightForCars1_Visual, uint8 (*)())
TL_STEAL(LightForCars2_Visual, uint8 (*)())
TL_STEAL(FindTrafficLightType, int32 (*)(CEntity*))
TL_STEAL(FindOrientationForTrafficLightType, float (*)(CEntity*))
TL_STEAL(FindTrafficLightTypeFromOrientation, int32 (*)(float))

namespace w3g {
using namespace w2;
constexpr size_t kScSize = 0x4000;
alignas(16) static uint8_t g_sc[kScSize];
constexpr int kNEnt = 8;
// fixture block layout
constexpr size_t kEnt = 0x000 /*8 x 0x40*/, kMat = 0x200 /*8 x 0x50*/, kRec = 0x500 /*0x80*/, kOutV = 0x580, kOutA = 0x590, kStat = 0x5A0 /*float: STAT_CITY_UNLOCKED*/, kVecs = 0x600 /*0x200*/,
                 kAttr = 0x800 /*0x100*/, kPeds = 0x900 /*0x100*/, kPairs = 0xA00 /*8 x 0x14*/, kTasks = 0xB00 /*0x40*/, kMatV = 0xC00 /*8 x 0x50*/;
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
static uint32_t P(size_t off) { return (uint32_t)(uintptr_t)(g_sc + off); }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static uint64_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
template<class R, class... A> static R ExeC(uint32_t addr, A... a) { return oracle::Fn<R __cdecl(A...)>(addr)(a...); }
template<class R, class... A> static R ExeT(uint32_t addr, void* self, A... a) { return oracle::Fn<R __fastcall(void*, int, A...)>(addr)(self, 0, a...); }   // thiscall
static float Fe(Rng& r, float scale, int special = 10) {
    if (special && r.below((uint32_t)special) == 0) return GenF(r, scale, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF | S_HUGE);
    return GenF(r, scale, S_ZERO);
}
static CEntity* Ent(int i) { return reinterpret_cast<CEntity*>(S(kEnt + 0x40 * i)); }

// ---- host stubs: the same bodies behind the exe address and the port's symbol ----
static bool g_riots = false;
static bool StubRiots() { return g_riots; }
static int StubLevel(float x, float y) { return x < -1200.f ? 2 : y > 900.f ? 3 : (x > 600.f || y < -900.f) ? 1 : 0; }
static int32_t __cdecl ExeLevel(const CVector* p) { return StubLevel(p->x, p->y); }
static float __cdecl ExeStat(int) { return SAt<float>(kStat); }
static void StubModify(int stat, float v) {
    uint32_t& n = SAt<uint32_t>(kRec);
    if (n < 6) { SAt<uint32_t>(kRec + 4 + 8 * n) = (uint32_t)stat; SAt<float>(kRec + 8 + 8 * n) = v; }
    ++n;
}
static void __cdecl ExeModify(int stat, float v) { StubModify(stat, v); }
static bool __cdecl ExeRiots() { return StubRiots(); }
static void InstallStubs() {
    static bool done = false; if (done) return; done = true;
    oracle::Patch(0x441C10, (void*)&ExeRiots);
    oracle::Patch(0x572300, (void*)&ExeLevel);
    oracle::Patch(0x558E40, (void*)&ExeStat);
    oracle::Patch(0x55D090, (void*)&ExeModify);
}

static void BuildAll(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    SAt<uint32_t>(kRec) = 0;
    g_riots = r.below(5) == 0;
    // entities: placement + matrix (forward vector in the matrix), raw blobs
    for (int i = 0; i < kNEnt; ++i) {
        uint8_t* e = S(kEnt + 0x40 * i);
        SAt<uint32_t>(kEnt + 0x40 * i + 0x14) = (uint32_t)(uintptr_t)S(kMat + 0x50 * i);
        for (int k = 0; k < 3; ++k) { const float q = Fe(r, 3000.f, 14); std::memcpy(S(kMat + 0x50 * i) + 0x30 + 4 * k, &q, 4); }
        (void)e;
    }
}

static std::function<void(Rng&)> g_post;
static void BuildPost(Rng& rr) { BuildAll(rr); if (g_post) g_post(rr); }
template<class PF, class EF>
static bool VCase(Rng& r, std::string& d, PF&& pf, EF&& ef, std::vector<Reg> extra = {}) {
    const uint32_t seed = r.u32() | 1;
    BuildPost(r); g_post = nullptr;
    Reg sc{ (uintptr_t)g_sc, kScSize };
    Reg img{ kImgLo, kImgHi - kImgLo };
    std::vector<Reg> regs{ sc, img };
    for (auto& e : extra) regs.push_back(e);
    return BothA(d, seed, std::initializer_list<Reg>(regs.data(), regs.data() + regs.size()), std::forward<PF>(pf), std::forward<EF>(ef));
}
} // namespace w3g

bool CGameLogic::LaRiotsActiveHere() { return w3g::StubRiots(); }
eLevelName CTheZones::GetLevelFromPosition(const CVector& p) { return (eLevelName)w3g::StubLevel(p.x, p.y); }
float CStats::GetStatValue(eStats) { return w3g::SAt<float>(w3g::kStat); }
void CStats::ModifyStat(eStats stat, float v) { w3g::StubModify((int)stat, v); }

#define GROW(NAME, ADDR) Run(NAME " " #ADDR, [&](Rng& r, std::string& d) -> bool {
#define GEND(CASES) }, std::max(10, (CASES) / w3g_div()));
static int w3g_div() { static const int v = std::getenv("W3G_DIV") ? std::max(1, std::atoi(std::getenv("W3G_DIV"))) : 2; return v; }

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void TestW3Gp2TrafficLights() {
    using namespace w2; using namespace w3g;
    using TL = CTrafficLights;
    // game clock: game range, phase boundaries of the 16384-tick cycle (time / 2 & 16383: 5000, 6000, 11000, 12000, 15383), the wrap and huge values
    static const auto Clock = [](Rng& rr) {
        BuildAll(rr);
        static const uint32_t bounds[] = { 0, 4999, 5000, 5001, 5999, 6000, 6001, 10999, 11000, 11001, 11999, 12000, 12001, 15381, 15382, 15383, 15384, 16382, 16383 };
        uint32_t t;
        const uint32_t k = rr.below(6);
        if (k == 0) t = rr.u32();
        else if (k < 4) t = bounds[rr.below(sizeof bounds / 4)] * 2 + rr.below(2) + 32768u * rr.below(8000);
        else if (k == 4) t = rr.below(10'000'000);
        else t = 0xFFFFFFFFu - rr.below(70000);
        StaticRef<uint32_t>(0xB7CB84) = t;
        StaticRef<bool>(0x969130 + CHEAT_BGREEN_LIGHTS_CHEAT) = rr.below(4) == 0;
        StaticRef<uint8_t>(0x969130 + CHEAT_RIOT_MODE) = (uint8_t)rr.below(2);
    };
#define TLROW(NAME, ADDR, CALL) \
    GROW("CTrafficLights::" NAME, ADDR) \
        g_post = Clock; \
        return VCase(r, d, [&]() -> uint64_t { return (uint8_t)CALL; }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t>(ADDR); }); \
    GEND(3000)
    TLROW("LightForPeds", 0x49D400, TL::LightForPeds())
    TLROW("LightForCars1", 0x49D2D0, tl_steal(T_LightForCars1{})())
    TLROW("LightForCars2", 0x49D310, tl_steal(T_LightForCars2{})())
    TLROW("LightForCars1_Visual", 0x49D350, tl_steal(T_LightForCars1_Visual{})())
    TLROW("LightForCars2_Visual", 0x49D3A0, tl_steal(T_LightForCars2_Visual{})())
#undef TLROW
    // model indices: the nine light models get random (possibly repeated) values, the query is one of them or a neighbour / random
    GROW("CTrafficLights::IsMITrafficLight", 0x49D5A0)
        int16_t mi[9]; for (auto& m : mi) m = (int16_t)(r.below(4) ? r.below(20000) : (uint32_t)(int32_t)(int16_t)r.u32());
        int32_t q = r.below(3) ? mi[r.below(9)] : r.below(2) ? (int32_t)r.below(20000) : (int32_t)r.u32();
        if (r.below(4) == 0) q += (int32_t)r.below(3) - 1;
        g_post = [&](Rng&) {
            *reinterpret_cast<int16_t*>(&ModelIndices::MI_TRAFFICLIGHTS) = mi[0]; *reinterpret_cast<int16_t*>(&ModelIndices::MI_TRAFFICLIGHTS_VERTICAL) = mi[1]; *reinterpret_cast<int16_t*>(&ModelIndices::MI_TRAFFICLIGHTS_MIAMI) = mi[2]; *reinterpret_cast<int16_t*>(&ModelIndices::MI_TRAFFICLIGHTS_VEGAS) = mi[3];
            *reinterpret_cast<int16_t*>(&ModelIndices::MI_TRAFFICLIGHTS_TWOVERTICAL) = mi[4]; *reinterpret_cast<int16_t*>(&ModelIndices::MI_TRAFFICLIGHTS_3) = mi[5]; *reinterpret_cast<int16_t*>(&ModelIndices::MI_TRAFFICLIGHTS_4) = mi[6]; *reinterpret_cast<int16_t*>(&ModelIndices::MI_TRAFFICLIGHTS_5) = mi[7];
            *reinterpret_cast<int16_t*>(&ModelIndices::MI_TRAFFICLIGHTS_GAY) = mi[8];
        };
        return VCase(r, d, [&]() -> uint64_t { return (uint8_t)TL::IsMITrafficLight(q); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, int32_t>(0x49D5A0, q); });
    GEND(2000)
    // orientation of the light model from the entity's forward vector (matrix +0x10 / +0x14); lights are placed at all headings
    static const auto Fwd = [](Rng& rr) {
        BuildAll(rr);
        for (int i = 0; i < kNEnt; ++i) {
            float f[2];
            const uint32_t k = rr.below(6);
            if (k == 0) { f[0] = Fe(rr, 1.f, 1); f[1] = Fe(rr, 1.f, 1); }
            else if (k == 1) { f[0] = 0.f; f[1] = rr.below(2) ? 1.f : -1.f; if (rr.below(2)) std::swap(f[0], f[1]); }
            else { const double a = (rr.below(1 << 20) / double(1 << 20)) * 6.283185307179586; f[0] = (float)std::sin(a); f[1] = (float)std::cos(a); }
            if (k == 2) { static const double hd[] = { 60., 150., 240., 330., 0., 90., 180., 270. }; const double a = hd[rr.below(8)] * 0.017453292519943295; f[0] = (float)-std::sin(a); f[1] = (float)std::cos(a); }
            std::memcpy(S(kMat + 0x50 * i) + 0x10, f, 8);
        }
    };
    GROW("CTrafficLights::FindOrientationForTrafficLightType", 0x49D520)
        g_post = Fwd; const int e = (int)r.below(kNEnt);
        return VCase(r, d, [&]() -> uint64_t { return FB(tl_steal(T_FindOrientationForTrafficLightType{})(Ent(e))); }, [&]() -> uint64_t { return FB(ExeC<float, CEntity*>(0x49D520, Ent(e))); });
    GEND(3000)
    GROW("CTrafficLights::FindTrafficLightType", 0x49D580)
        g_post = Fwd; const int e = (int)r.below(kNEnt);
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)tl_steal(T_FindTrafficLightType{})(Ent(e)); }, [&]() -> uint64_t { return ExeC<uint32_t, CEntity*>(0x49D580, Ent(e)); });
    GEND(3000)
    GROW("CTrafficLights::FindTrafficLightTypeFromOrientation", 0x49D4D0)
        static const float edges[] = { 60.f, 150.f, 240.f, 330.f };
        float o;
        const uint32_t k = r.below(5);
        if (k == 0) o = Fe(r, 400.f, 1);
        else if (k < 3) { o = edges[r.below(4)]; const uint32_t s = r.below(3); if (s == 1) o = std::nextafter(o, 1e9f); else if (s == 2) o = std::nextafter(o, -1e9f); }
        else o = (float)(r.below(36000) / 100.0);
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)tl_steal(T_FindTrafficLightTypeFromOrientation{})(o); }, [&]() -> uint64_t { return ExeC<uint32_t, float>(0x49D4D0, o); });
    GEND(2500)
#undef TL_STEAL
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void TestW3Gp2Restart() {
    using namespace w2; using namespace w3g;
    using R = CRestart;
    // restart tables: up to 10 points per kind in the map range (several towns, the zone-level stub partitions the map), when-to-use 0..3, the city-unlocked statistic, the override / extra points
    static const auto Tables = [](Rng& rr) {
        BuildAll(rr);
        const bool police = rr.below(2);
        (void)police;
        R::NumberOfHospitalRestarts = (uint16_t)rr.below(11); R::NumberOfPoliceRestarts = (uint16_t)(1 + rr.below(10));
        for (int i = 0; i < 10; ++i) {
            R::HospitalRestartPoints[i] = CVector{ GenF(rr, 3000.f, S_ZERO), GenF(rr, 3000.f, S_ZERO), GenF(rr, 100.f, S_ZERO) };
            R::PoliceRestartPoints[i] = CVector{ GenF(rr, 3000.f, S_ZERO), GenF(rr, 3000.f, S_ZERO), GenF(rr, 100.f, S_ZERO) };
            R::HospitalRestartHeadings[i] = GenF(rr, 360.f, S_ZERO); R::PoliceRestartHeadings[i] = GenF(rr, 360.f, S_ZERO);
            R::HospitalRestartWhenToUse[i] = (int32_t)rr.below(4); R::PoliceRestartWhenToUse[i] = (int32_t)rr.below(4);
        }
        R::PoliceRestartWhenToUse[0] = 0;
        R::bOverrideRestart = rr.below(5) == 0; R::bOverrideRespawnBasePointForMission = rr.below(4) == 0;
        R::OverridePosition = CVector{ Fe(rr, 3000.f, 10), Fe(rr, 3000.f, 10), Fe(rr, 100.f, 10) };
        R::OverrideRespawnBasePointForMission = CVector{ GenF(rr, 3000.f, S_ZERO), GenF(rr, 3000.f, S_ZERO), GenF(rr, 100.f, S_ZERO) };
        R::OverrideHeading = Fe(rr, 360.f, 10);
        R::ExtraHospitalRestartCoors = CVector{ GenF(rr, 3000.f, S_ZERO), GenF(rr, 3000.f, S_ZERO), 10.f }; R::ExtraPoliceStationRestartCoors = CVector{ GenF(rr, 3000.f, S_ZERO), GenF(rr, 3000.f, S_ZERO), 10.f };
        R::ExtraHospitalRestartRadius = rr.below(3) ? 0.f : rr.below(4) ? GenF(rr, 1500.f, S_ZERO) : Fe(rr, 2000.f, 2);
        R::ExtraPoliceStationRestartRadius = rr.below(3) ? 0.f : rr.below(4) ? GenF(rr, 1500.f, S_ZERO) : std::fabs(Fe(rr, 2000.f, 8));
        if (std::isnan(R::ExtraPoliceStationRestartRadius)) R::ExtraPoliceStationRestartRadius = 0.f;
        R::ExtraHospitalRestartHeading = GenF(rr, 360.f, S_ZERO); R::ExtraPoliceStationRestartHeading = GenF(rr, 360.f, S_ZERO);
        SAt<float>(kStat) = rr.below(8) == 0 ? Fe(rr, 4.f, 2) : (float)rr.below(4) + (rr.below(4) == 0 ? 0.5f : 0.f);
        if (rr.below(30) == 0) { SAt<float>(kStat) = std::nanf(""); }
    };
    GROW("CRestart::Initialise", 0x460630)
        g_post = Tables;
        return VCase(r, d, [&]() -> uint64_t { R::Initialise(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x460630); return 0; });
    GEND(60)
    GROW("CRestart::AddHospitalRestartPoint", 0x460730)
        g_post = Tables; const CVector pt{ Fe(r, 3000.f, 10), Fe(r, 3000.f, 10), Fe(r, 100.f, 10) }; const float ang = Fe(r, 360.f, 10); const int32_t town = (int32_t)r.below(5);
        g_post = [](Rng& rr) { Tables(rr); R::NumberOfHospitalRestarts = (uint16_t)rr.below(10); };
        std::memcpy(S(kOutV), &pt, 12);
        return VCase(r, d, [&]() -> uint64_t { R::AddHospitalRestartPoint(SAt<CVector>(kOutV), ang, town); return 0; }, [&]() -> uint64_t { ExeC<void, const void*, float, int32_t>(0x460730, S(kOutV), ang, town); return 0; });
    GEND(300)
    GROW("CRestart::AddPoliceRestartPoint", 0x460780)
        const CVector pt{ Fe(r, 3000.f, 10), Fe(r, 3000.f, 10), Fe(r, 100.f, 10) }; const float ang = Fe(r, 360.f, 10); const int32_t town = (int32_t)r.below(5);
        g_post = [&](Rng& rr) { Tables(rr); R::NumberOfPoliceRestarts = (uint16_t)rr.below(10); std::memcpy(S(kOutV), &pt, 12); };
        return VCase(r, d, [&]() -> uint64_t { R::AddPoliceRestartPoint(SAt<CVector>(kOutV), ang, town); return 0; }, [&]() -> uint64_t { ExeC<void, const void*, float, int32_t>(0x460780, S(kOutV), ang, town); return 0; });
    GEND(300)
    GROW("CRestart::OverrideNextRestart", 0x4607D0)
        const CVector pt{ Fe(r, 3000.f, 10), Fe(r, 3000.f, 10), Fe(r, 100.f, 10) }; const float ang = Fe(r, 360.f, 10);
        g_post = [&](Rng& rr) { Tables(rr); std::memcpy(S(kOutV), &pt, 12); };
        return VCase(r, d, [&]() -> uint64_t { R::OverrideNextRestart(SAt<CVector>(kOutV), ang); return 0; }, [&]() -> uint64_t { ExeC<void, const void*, float>(0x4607D0, S(kOutV), ang); return 0; });
    GEND(100)
    GROW("CRestart::CancelOverrideRestart", 0x460800)
        g_post = Tables;
        return VCase(r, d, [&]() -> uint64_t { R::CancelOverrideRestart(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x460800); return 0; });
    GEND(40)
    GROW("CRestart::ClearRespawnPointForDurationOfMission", 0x460840)
        g_post = Tables;
        return VCase(r, d, [&]() -> uint64_t { R::ClearRespawnPointForDurationOfMission(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x460840); return 0; });
    GEND(40)
    GROW("CRestart::SetRespawnPointForDurationOfMission", 0x460810)
        const CVector pt{ Fe(r, 3000.f, 10), Fe(r, 3000.f, 10), Fe(r, 100.f, 10) };
        g_post = Tables;
        return VCase(r, d, [&]() -> uint64_t { R::SetRespawnPointForDurationOfMission(pt); return 0; }, [&]() -> uint64_t { ExeC<void, float, float, float>(0x460810, pt.x, pt.y, pt.z); return 0; });
    GEND(100)
    // closest restart: the query point near / far from the restart points; out parameters in the fixture block (compared)
    static const auto Query = [](Rng& rr, CVector& q) {
        q = CVector{ GenF(rr, 3000.f, S_ZERO), GenF(rr, 3000.f, S_ZERO), GenF(rr, 100.f, S_ZERO) };
        if (rr.below(3) == 0) { const int i = (int)rr.below(10); q = rr.below(2) ? R::HospitalRestartPoints[i] : R::PoliceRestartPoints[i]; q.x += GenF(rr, 5.f, S_ZERO); }
        if (rr.below(40) == 0) q = CVector{ Fe(rr, 3000.f, 1), Fe(rr, 3000.f, 1), 0.f };
    };
    GROW("CRestart::FindClosestHospitalRestartPoint", 0x460850)
        CVector q; g_post = [&](Rng& rr) { Tables(rr); Query(rr, q); if (rr.below(10) == 0) R::NumberOfHospitalRestarts = 0; };
        const uint32_t seed = r.u32() | 1; (void)seed;
        auto portFn = [&]() -> uint64_t { R::FindClosestHospitalRestartPoint(q, SAt<CVector>(kOutV), SAt<float>(kOutA)); return 0; };
        auto exeFn = [&]() -> uint64_t { ExeC<void, float, float, float, void*, void*>(0x460850, q.x, q.y, q.z, S(kOutV), S(kOutA)); return 0; };
        return VCase(r, d, portFn, exeFn);
    GEND(3000)
    GROW("CRestart::FindClosestPoliceRestartPoint", 0x460A50)
        CVector q; g_post = [&](Rng& rr) {
            Tables(rr);
            for (int i = 0; i < 10; ++i) for (int k = 0; k < 3; ++k) { auto& v = R::PoliceRestartPoints[i]; (k == 0 ? v.x : k == 1 ? v.y : v.z) = GenF(rr, k == 2 ? 100.f : 3000.f, S_ZERO); }
            if (std::isnan(SAt<float>(kStat)) || SAt<float>(kStat) < 0.f) SAt<float>(kStat) = 3.f;
            Query(rr, q); if (!std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.z) || std::fabs(q.x) > 1e6f || std::fabs(q.y) > 1e6f) q = CVector{ 100.f, 100.f, 10.f };
            R::PoliceRestartWhenToUse[0] = 0;
        };
        auto portFn = [&]() -> uint64_t { R::FindClosestPoliceRestartPoint(q, SAt<CVector>(kOutV), SAt<float>(kOutA)); return 0; };
        auto exeFn = [&]() -> uint64_t { ExeC<void, float, float, float, void*, void*>(0x460A50, q.x, q.y, q.z, S(kOutV), S(kOutA)); return 0; };
        return VCase(r, d, portFn, exeFn);
    GEND(3000)
}
#include "wave3_oracle_gp2_b.inc"
