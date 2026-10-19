// ORACLE-W3-TASKS: exe-oracle rows for game_sa/Tasks/TaskTypes (classes A and B of .notes/ORACLE_COVERAGE.md). Included by wave2_oracle_all.inc when W3_TASKS_ONLY is defined
// (executable wave3_tasks_oracle_test = the W2 harness: deterministic arena, fake pools, entity reference world, whole exe data image compared after every case).
//
// Fixture (W3TaskCase): the ORACLE-W3-EVENTS fixture (wave3_oracle_ev.inc, fixture part only): the PED under test is the wired ped of wave3_oracle_pedfx.inc (g_src, also CWorld::Players[0], reachable as
// UnderTest()), the reference world's other peds / vehicles are wired as well (intelligence, ped type, drivers), the task allocator pool (0xB744BC) exists, so `new Task` in Clone / CreateSubTask works on
// both sides. `this` of a task row is the task object g_tk: garbage, entity-pointer members point at the wired peds / reference entities, plain bools 0 / 1, enums small, m_pSubTask a real sub task.
// Buffer arguments (g_ta), the struct return (g_tr) and the animation buffer (g_anim) are compared as well, together with the pools, the arena, the reference world and the whole exe data image.
#define W3_EV_FIXTURE_ONLY 1
#include "wave3_oracle_ev.inc"

namespace w3t {
using namespace w2; using namespace w3e; using namespace w3p;
#include "Pools/PointRoutePool.h"
#include "Pools/NodeRoutePool.h"
static PoolFx<CPointRoutePool, 0x64, 0xB744B0, 16> g_prp;   // the point route pool (CPointRoute::operator new), larger than the default so Clone / ctor rows can allocate several routes
static PoolFx<CNodeRoutePool, 0x24, 0xB744B8, 16> g_nrp;    // the node route pool
alignas(16) static uint8_t g_tk[0x800];      // the task object under test
alignas(16) static uint8_t g_ta[8][0x80];    // buffer arguments / in-out parameters (CVector&, CColSphere&, CNodeAddress&)
alignas(16) static uint8_t g_tr[0x80];       // struct return value (hidden pointer)
alignas(16) static uint8_t g_tk2[0x100];     // a second task object (argument)
static std::vector<std::pair<size_t, size_t>> g_tmask;   // byte ranges of the task object the exe leaves uninitialised (bit-field padding, half-initialised CNodeAddress): not compared
static std::vector<std::pair<size_t, size_t>> g_pmask;   // the same, inside every slot of the task pool store (tasks created by the function under test)
static bool g_tzero = false;   // constructor rows whose bit-fields / padding the exe leaves untouched: start from an all-zero object instead of garbage
static std::function<void(Rng&)> g_tpost;    // hand rows: extra fixture work (exe globals) after everything is wired

static CPed* TPedR(Rng& r) { return r.below(3) == 0 ? UnderTest() : RPedR(r); }
static CPed* TPedN(Rng& r) { const uint32 k = r.below(9); return k < 3 ? UnderTest() : (k == 8 ? nullptr : RPedR(r)); }
template<class T> static uint64_t StoreRet(const T& v) { std::memcpy(g_tr, &v, sizeof v); return 0; }

// a valid route object from the fake pool: 0..8 entries (the game never stores more), garbage-but-finite coordinates
static void* NewPointRoute(Rng& r) {
    auto* p = g_prp.pool->New(); if (!p) return nullptr;
    auto* b = reinterpret_cast<uint8_t*>(p); std::memset(b, 0, 0x64);
    const uint32_t n = 5 + r.below(4); std::memcpy(b, &n, 4);   // 5..8 entries: the index members (kind 'x', 0..4) stay inside the route, the port asserts on an out-of-range subscript
    for (uint32_t i = 0; i < 8; ++i) { const CVector v = GV(r); std::memcpy(b + 4 + i * 12, &v, 12); }
    return p;
}
static void* NewNodeRoute(Rng& r) {
    auto* p = g_nrp.pool->New(); if (!p) return nullptr;
    auto* b = reinterpret_cast<uint8_t*>(p); std::memset(b, 0, 0x24);
    const uint32_t n = 5 + r.below(4); std::memcpy(b, &n, 4);
    for (uint32_t i = 0; i < 8; ++i) { const uint16_t a = (uint16_t)r.below(64), nd = (uint16_t)r.below(256); std::memcpy(b + 4 + i * 4, &a, 2); std::memcpy(b + 6 + i * 4, &nd, 2); }
    return p;
}

static void PutEntTo(Rng& r, uint8_t* b, size_t off, char kind, size_t size) {
    if (kind == 'R' || kind == 'Q') { void* v = kind == 'R' ? NewPointRoute(r) : NewNodeRoute(r); std::memcpy(b + off, &v, 4); return; }
    if (kind == 'x') { const uint32_t v = r.below(5); std::memcpy(b + off, &v, 4); return; }
    if (kind == 'u') { const uint32_t v = r.below(5); std::memcpy(b + off, &v, size < 4 ? size : 4); if (size < 4) { std::memset(b + off + size, 0, 4 - size); } return; }
    if (kind == 'T') { std::memcpy(b + off, &g_subTask, 4); return; }
    if (kind == 'b') { b[off] = (uint8_t)r.below(2); return; }
    if (kind == 'n') { const uint32_t z = 0; std::memcpy(b + off, &z, 4); return; }
    CEntity* e = kind == 'p' ? (r.below(3) == 0 ? static_cast<CEntity*>(UnderTest()) : GPedE(r)) : kind == 'v' ? GVehE(r) : kind == 'o' ? GObjE(r) : GEnt(r);
    std::memcpy(b + off, &e, 4);
}

template<class T, class... A> static bool Build(A&&... a) { if constexpr (!std::is_abstract_v<T> && std::is_constructible_v<T, A&&...>) { ::new (static_cast<void*>(g_tk)) T(std::forward<A>(a)...); return true; } else return false; }

// one case: fresh worlds, the wired ped under test, the reference world; then the task object `this` (sz > 0); compared after the call
template<class PF, class EF>
static bool W3TaskCase(Rng& r, std::string& d, size_t sz, std::initializer_list<EntOff> ents, PF&& portFn, EF&& exeFn, bool maskVptr = false, std::function<bool()> build = nullptr) {
    static std::vector<uint8_t> img0(kImgHi - kImgLo);
    std::memcpy(img0.data(), (void*)kImgLo, img0.size());
    const uint32_t seed = Prep(r, 0);
    g_tap.Prepare(r, true); g_tap.Enter();
    g_prp.Prepare(r, true); g_prp.Enter(); g_nrp.Prepare(r, true); g_nrp.Enter();
    FillGarbage(r, g_anim, sizeof g_anim);
    g_subTask = nullptr; g_tact = g_tsimp = nullptr;
    FillGarbage(r, g_src, (sizeof(CCopPed) + 3) & ~size_t(3));
    WirePed(r, g_src, sizeof(CPlayerPed));
    *reinterpret_cast<volatile float*>(0xC18CF0) = 1.0f; *reinterpret_cast<volatile float*>(0xC18CEC) = 60.0f; *reinterpret_cast<volatile float*>(0xC18CE8) = 60.0f;   // 0xC18CE8: safe distance read by CTaskComplexLeaveCarAndFlee::CreateSubTask (the port passes the literal 60)
    WireRef(r); WireUnderTest(r); WireWeaponInfos(r);
    FillGarbage(r, g_tk, sizeof g_tk); FillGarbage(r, g_tk2, sizeof g_tk2); FillGarbage(r, g_tr, sizeof g_tr);
    for (auto& a : g_ta) FillGarbage(r, a, sizeof a);
    if (sz) {
        g_subTask = g_tp.pool->New(); if (g_subTask) ::new (static_cast<void*>(g_subTask)) CTaskSimpleStandStill(50, false, false, 8.0f);
        NoNegInts(r, g_tk, (sz + 3) & ~size_t(3));
        if (g_tzero) std::memset(g_tk, 0, sizeof g_tk);
        if (build && build()) { for (const auto& e : ents) if (e.kind == 'R' || e.kind == 'Q' || e.kind == 'x') PutEntTo(r, g_tk, e.off, e.kind, e.size); }   // routes with 5..8 entries and index members in range replace the empty routes of the constructed object   // a real object built with the port's constructor (valid members); otherwise garbage with the pointer members wired
        else for (const auto& e : ents) PutEntTo(r, g_tk, e.off, e.kind, e.size);
    }
    if (g_tpost) { auto f = std::move(g_tpost); g_tpost = nullptr; f(r); }
    Reg tk{ (uintptr_t)g_tk, sizeof g_tk };
    if (maskVptr || !g_tmask.empty()) { tk.mask.assign(sizeof g_tk, 0); if (maskVptr) for (int k = 0; k < 4; ++k) tk.mask[k] = 1; for (const auto& m : g_tmask) for (size_t k = 0; k < m.second; ++k) tk.mask[m.first + k] = 1; }
    Reg tps = g_tp.RegStore();
    if (!g_pmask.empty()) { tps.mask.assign(tps.n, 0); for (size_t o = 0; o + 0x80 <= tps.n; o += 0x80) for (const auto& m : g_pmask) for (size_t k = 0; k < m.second; ++k) tps.mask[o + m.first + k] = 1; }
    const bool ok = BothA(d, seed, { Reg{ (uintptr_t)g_src, sizeof(CCopPed) }, tk, Reg{ (uintptr_t)g_ta, sizeof g_ta }, Reg{ (uintptr_t)g_tr, sizeof g_tr }, Reg{ (uintptr_t)g_anim, sizeof g_anim },
            Reg{ (uintptr_t)g_ints, sizeof g_ints }, Reg{ (uintptr_t)g_mat, sizeof g_mat }, Reg{ (uintptr_t)g_mi, sizeof g_mi }, Reg{ (uintptr_t)g_col, sizeof g_col }, Reg{ (uintptr_t)g_intel, sizeof g_intel },
            Reg{ (uintptr_t)g_stat, sizeof g_stat }, Reg{ (uintptr_t)g_wanted, sizeof g_wanted }, Reg{ (uintptr_t)g_wanted2, sizeof g_wanted2 }, Reg{ (uintptr_t)g_clo, sizeof g_clo },
            g_tp.RegStruct(), g_tp.RegFlags(), tps, g_ep.RegStruct(), g_ep.RegFlags(), g_ep.RegStore(), g_tap.RegStruct(), g_tap.RegFlags(), g_tap.RegStore(),
            g_prp.RegStruct(), g_prp.RegFlags(), g_prp.RegStore(), g_nrp.RegStruct(), g_nrp.RegFlags(), g_nrp.RegStore(), Reg{ (uintptr_t)rf::pp, sizeof(CPedPool) }, Reg{ (uintptr_t)rf::pedSt, (size_t)rf::kP }, Reg{ kImgLo, kImgHi - kImgLo } },
        [&]() -> uint64_t { return portFn(static_cast<void*>(g_tk)); }, [&]() -> uint64_t { return exeFn(static_cast<void*>(g_tk)); }, kRf);
    g_tap.Leave(); g_prp.Leave(); g_nrp.Leave();
    g_tmask.clear(); g_pmask.clear(); g_tzero = false;
    Done();
    UnwireModel();
    std::memcpy((void*)kImgLo, img0.data(), img0.size());
    return ok;
}
} // namespace w3t
using w3t::W3TaskCase;

#if __has_include("wave3_oracle_gen_tasks.inc")
#include "wave3_oracle_gen_tasks.inc"     // GENERATED (tools/gen_w3_tasks.py)
#define W3_HAVE_TASKGEN 1
#endif

#include "wave3_oracle_tasks_hand.inc"

static void TestW3Tasks() {
    TestW3TasksHand();
#ifdef W3_HAVE_TASKGEN
    TestW3TaskFuncs();
#endif
}
