// ORACLE-W3-GAMEPLAY: exe-oracle rows for the gameplay-rule root files of game_sa (Weapon*, Wanted, Pickup(s), Darkel, GangWars, PedGroup*, Garage(s), Stats, PlayerInfo, Clock, Weather, Population; classes A and B of
// .notes/ORACLE_COVERAGE.md). Included by wave2_oracle_all.inc when W3_GP_ONLY is defined (executable wave3_gameplay_oracle_test = the W2 harness: deterministic arena, fake pools, entity reference world,
// whole exe data image compared after every case, restored afterwards).
//
// The `this` object (CGarage, CPickup, CPedGroup, ...) is garbage with its entity-pointer members pointing at the reference-world entities (PutEnt); the static state of the subsystem (the exe globals the port
// keeps at the same addresses) is wired per class by GpFix() (wave3_oracle_gp_fix.inc).
#include "World.h"
#include "Collision/ColSphere.h"
#include "Garages.h"
#include "Pickups.h"
#include "Darkel.h"
#include "GangWars.h"
#include "PedGroups.h"
#include "PlayerInfo.h"
#include "Population.h"
#include "Stats.h"
#include "Weather.h"
#include "Clock.h"
#include "Wanted.h"
#include "WeaponEffects.h"
namespace w2 {   // arguments in game range (the port indexes its tables with them under an assert, the exe reads past the table end)
template<> eWeaponType GE<eWeaponType>(Rng& r) { return (eWeaponType)(r.below(8) ? r.below(47) : r.below(60)); }
template<> eGarageType GE<eGarageType>(Rng& r) { static constexpr uint8_t k[] = { 0,1,2,3,4,5,11,14,15,16,17,18,19,20,21,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45 }; return (eGarageType)k[r.below(sizeof k)]; }
}
namespace w3g {
using namespace w2;
static std::function<void(Rng&, uint8_t*)> g_post;   // hand rows: extra fixture work after the object is wired
template<class T> static void Put32(uint8_t* b, size_t off, T v) { std::memcpy(b + off, &v, sizeof v); }

alignas(16) static uint8_t g_gmat[40][0x60];   // matrices of the reference-world entities (WireWorld)
alignas(16) static uint8_t g_gmi[0x400];        // model info shared by the wired entities
alignas(16) static uint8_t g_gcol[0x80];        // its CColModel
alignas(16) static uint8_t g_gcd[0x60];         // CCollisionData
alignas(16) static CColSphere g_gsph[4];        // its spheres
alignas(16) static uint8_t g_gint[0x800];       // CPedIntelligence of a member ped
static int g_gmodel = 0; static bool g_gwired = false;
static void GpFix(const char* cls, Rng& r, uint8_t* v, size_t sz);   // defined in wave3_oracle_gp_fix.inc (after the generated rows)
static void UnwireWorld();

template<class PF, class EF>
static bool W3GpCase(Rng& r, std::string& d, const char* cls, size_t sz, std::initializer_list<EntOff> ents, PF&& portFn, EF&& exeFn, bool sub = false, bool dtor = false) {
    static std::vector<uint8_t> img0(kImgHi - kImgLo);
    const auto post = std::move(g_post); g_post = nullptr;
    std::memcpy(img0.data(), (void*)kImgLo, img0.size());
    const uint32_t seed = Prep(r, 0);
    FillGarbage(r, g_anim, sizeof g_anim);
    g_subTask = nullptr;
    if (sub) { g_subTask = g_tp.pool->New(); if (g_subTask) ::new (static_cast<void*>(g_subTask)) CTaskSimpleStandStill(50, false, false, 8.0f); }
    if (sz) { FillGarbage(r, g_src, (sz + 3) & ~size_t(3)); for (const auto& e : ents) PutEnt(r, e.off, e.kind, e.size); }
    GpFix(cls, r, g_src, sz);
    if (post) post(r, g_src);
    Reg thisReg{ (uintptr_t)g_src, sz ? sz : 4 };
    if (dtor) { thisReg.mask.assign(sz ? sz : 4, 0); for (int k = 0; k < 4; ++k) thisReg.mask[k] = 1; }
    const bool ok = BothA(d, seed, { thisReg, Reg{ (uintptr_t)g_anim, sizeof g_anim }, Reg{ (uintptr_t)g_gmat, sizeof g_gmat }, Reg{ (uintptr_t)g_gint, sizeof g_gint },
            g_tp.RegStruct(), g_tp.RegFlags(), g_tp.RegStore(), g_ep.RegStruct(), g_ep.RegFlags(), g_ep.RegStore(), Reg{ (uintptr_t)rf::pp, sizeof(CPedPool) }, Reg{ (uintptr_t)rf::pedSt, (size_t)rf::kP }, Reg{ kImgLo, kImgHi - kImgLo } },
        [&]() -> uint64_t { return portFn(static_cast<void*>(g_src)); }, [&]() -> uint64_t { return exeFn(static_cast<void*>(g_src)); }, kRf);
    Done();
    if (g_gwired) { UnwireWorld(); g_gwired = false; }
    std::memcpy((void*)kImgLo, img0.data(), img0.size());
    return ok;
}
} // namespace w3g
using w3g::W3GpCase;

#if __has_include("wave3_oracle_gen_gp.inc")
#include "wave3_oracle_gen_gp.inc"      // GENERATED (tools/gen_w3_gp.py): member / static functions of the gameplay root files
#define W3G_HAVE_GEN 1
#endif
#include "wave3_oracle_gp_fix.inc"      // per-class fixtures (w3g::GpFix) and the hand rows (TestW3GpHand)

static void TestW3Gp() {
    TestW3GpHand();
#ifdef W3G_HAVE_GEN
    TestW3GpFuncs();
#endif
}
