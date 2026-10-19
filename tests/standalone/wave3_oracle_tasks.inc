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
alignas(16) static uint8_t g_tk[0x800];      // the task object under test
alignas(16) static uint8_t g_ta[8][0x80];    // buffer arguments / in-out parameters (CVector&, CColSphere&, CNodeAddress&)
alignas(16) static uint8_t g_tr[0x80];       // struct return value (hidden pointer)
alignas(16) static uint8_t g_tk2[0x100];     // a second task object (argument)
static std::function<void(Rng&)> g_tpost;    // hand rows: extra fixture work (exe globals) after everything is wired

static CPed* TPedR(Rng& r) { return r.below(3) == 0 ? UnderTest() : RPedR(r); }
static CPed* TPedN(Rng& r) { const uint32 k = r.below(9); return k < 3 ? UnderTest() : (k == 8 ? nullptr : RPedR(r)); }
template<class T> static uint64_t StoreRet(const T& v) { std::memcpy(g_tr, &v, sizeof v); return 0; }

static void PutEntTo(Rng& r, uint8_t* b, size_t off, char kind, size_t size) {
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
        if (build && build()) {}   // a real object built with the port's constructor (valid members); otherwise garbage with the pointer members wired
        else for (const auto& e : ents) PutEntTo(r, g_tk, e.off, e.kind, e.size);
    }
    if (g_tpost) { auto f = std::move(g_tpost); g_tpost = nullptr; f(r); }
    Reg tk{ (uintptr_t)g_tk, sizeof g_tk };
    if (maskVptr) { tk.mask.assign(sizeof g_tk, 0); for (int k = 0; k < 4; ++k) tk.mask[k] = 1; }
    const bool ok = BothA(d, seed, { Reg{ (uintptr_t)g_src, sizeof(CCopPed) }, tk, Reg{ (uintptr_t)g_ta, sizeof g_ta }, Reg{ (uintptr_t)g_tr, sizeof g_tr }, Reg{ (uintptr_t)g_anim, sizeof g_anim },
            Reg{ (uintptr_t)g_ints, sizeof g_ints }, Reg{ (uintptr_t)g_mat, sizeof g_mat }, Reg{ (uintptr_t)g_mi, sizeof g_mi }, Reg{ (uintptr_t)g_col, sizeof g_col }, Reg{ (uintptr_t)g_intel, sizeof g_intel },
            Reg{ (uintptr_t)g_stat, sizeof g_stat }, Reg{ (uintptr_t)g_wanted, sizeof g_wanted }, Reg{ (uintptr_t)g_wanted2, sizeof g_wanted2 }, Reg{ (uintptr_t)g_clo, sizeof g_clo },
            g_tp.RegStruct(), g_tp.RegFlags(), g_tp.RegStore(), g_ep.RegStruct(), g_ep.RegFlags(), g_ep.RegStore(), g_tap.RegStruct(), g_tap.RegFlags(), g_tap.RegStore(),
            Reg{ (uintptr_t)rf::pp, sizeof(CPedPool) }, Reg{ (uintptr_t)rf::pedSt, (size_t)rf::kP }, Reg{ kImgLo, kImgHi - kImgLo } },
        [&]() -> uint64_t { return portFn(static_cast<void*>(g_tk)); }, [&]() -> uint64_t { return exeFn(static_cast<void*>(g_tk)); }, kRf);
    g_tap.Leave();
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
