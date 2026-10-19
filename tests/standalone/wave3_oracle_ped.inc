// ORACLE-W3-PED: exe-oracle rows for game_sa/Entity/Ped (classes A and B of .notes/ORACLE_COVERAGE.md). Included by wave2_oracle_all.inc when W3_PED_ONLY is defined
// (executable wave3_ped_oracle_test = the W2 harness: deterministic arena, fake pools, entity reference world, whole exe data image compared after every case).
//
// Like the vehicle wave, the `this` object is a wired PED: a garbage ped faults in nearly every function, so W3PedCase wires the pointers a real ped always has (matrix, model info with a collision
// model, the intelligence object with an empty task manager, the player data / wanted / clothes of CWorld::Players[0] for a player ped, the stats) and keeps the state enums, the weapon slots
// and the counters in their game range; everything else stays garbage. The exe data image is restored after every case.
#include "Models/ModelInfo.h"
#include "Models/PedModelInfo.h"
#include "Collision/ColModel.h"
#include "PedIntelligence.h"
#include "PlayerInfo.h"
#include "Wanted.h"
#include "World.h"
#include "Camera.h"

struct W3POffIntel_T { using type = CPedIntelligence* CPed::*; friend type W2Get(W3POffIntel_T); }; template struct W2Rob<W3POffIntel_T, &CPed::m_pIntelligence>;
struct W3POffPData_T { using type = CPlayerPedData* CPed::*; friend type W2Get(W3POffPData_T); }; template struct W2Rob<W3POffPData_T, &CPed::m_pPlayerData>;
static size_t W3POffOf(CPedIntelligence* CPed::* pm) { uint32_t o = 0; std::memcpy(&o, &pm, 4); return o; }
static size_t W3POffOf(CPlayerPedData* CPed::* pm) { uint32_t o = 0; std::memcpy(&o, &pm, 4); return o; }
namespace w2 {   // ped arguments in their game range: the port indexes its tables with them (assert), the exe reads past the table end
template<> eWeaponType GE<eWeaponType>(Rng& r) { return (eWeaponType)(r.below(8) ? r.below(47) : r.below(60)); }
template<> eSprintType GE<eSprintType>(Rng& r) { return (eSprintType)r.below(4); }
template<> ePedState GE<ePedState>(Rng& r) { return (ePedState)r.below(60); }
}
namespace w3p {
using namespace w2;
alignas(16) static uint8_t g_mat[0x60];     // the ped's CMatrixLink
alignas(16) static uint8_t g_mi[0x400];     // CPedModelInfo
alignas(16) static uint8_t g_col[0x80];     // CColModel
alignas(16) static uint8_t g_intel[0x800];  // CPedIntelligence (empty task manager)
alignas(16) static uint8_t g_stat[0x40];    // CPedStat
alignas(16) static uint8_t g_wanted[0x800]; // CWanted
alignas(16) static uint8_t g_clo[0x200];    // CPedClothesDesc
alignas(16) static uint8_t g_out[0x40];     // out parameters
alignas(16) static uint8_t g_clump[0x100];  // fake weapon / goggles object (non-null pointers only)
static int g_model = 0;
static std::function<void(Rng&, uint8_t*)> g_post;   // hand rows: extra fixture work after the ped is wired
template<class T> static void Put32(uint8_t* b, size_t off, T v) { std::memcpy(b + off, &v, sizeof v); }
static_assert(sizeof(CPedIntelligence) <= sizeof(g_intel) && sizeof(CWanted) <= sizeof(g_wanted));

static void WireMatrix(Rng& r, uint8_t* m) {
    FillGarbage(r, m, 0x50);
    auto& M = *reinterpret_cast<CMatrix*>(m);
    if (r.below(4)) {   // mostly a proper rotation
        const float h = r.f01() * 6.2831853f;
        M.GetRight() = CVector(std::cos(h), std::sin(h), 0.f);
        M.GetForward() = CVector(-std::sin(h), std::cos(h), 0.f);
        M.GetUp() = CVector(0.f, 0.f, 1.f);
    }
    M.GetPosition() = GV(r);
    std::memset(m + 0x40, 0, 0x10);
}
static void WirePed(Rng& r, uint8_t* v, size_t sz) {
    WireMatrix(r, g_mat);
    Put32(v, offsetof(CPlaceable, m_matrix), (uint32_t)(uintptr_t)g_mat);
    // model info + collision model
    g_model = (int)r.below(300);
    Put32(v, offsetof(CEntity, m_nModelIndex), (int16_t)g_model);
    FillGarbage(r, g_mi, sizeof g_mi); FillGarbage(r, g_col, sizeof g_col);
    Put32(g_mi, offsetof(CBaseModelInfo, m_pColModel), (uint32_t)(uintptr_t)g_col);
    CModelInfo::ms_modelInfoPtrs[g_model] = reinterpret_cast<CBaseModelInfo*>(g_mi);
    // intelligence: empty task manager, event handler / group cleared; back pointer to this ped
    std::memset(g_intel, 0, sizeof g_intel);
    reinterpret_cast<CPedIntelligence*>(g_intel)->m_pPed = reinterpret_cast<CPed*>(v);
    Put32(v, W3POffOf(W2Get(W3POffIntel_T{})), (uint32_t)(uintptr_t)g_intel);
    FillGarbage(r, g_stat, sizeof g_stat);
    Put32(v, offsetof(CPed, m_pStats), (uint32_t)(uintptr_t)g_stat);
    FillGarbage(r, g_out, sizeof g_out); FillGarbage(r, g_clump, sizeof g_clump);
    // states and counters in their game ranges
    Put32(v, offsetof(CPed, m_nPedState), (int32_t)(r.below(3) ? r.below(60) : 0));
    Put32(v, offsetof(CPed, m_nMoveState), (int32_t)r.below(8));
    Put32(v, offsetof(CPed, m_nPedType), (int32_t)r.below(32));
    Put32(v, offsetof(CPed, m_nWeaponSkill), (int32_t)r.below(4));
    v[offsetof(CPed, m_nActiveWeaponSlot)] = (uint8_t)(r.below(13));
    {   // weapon slots
        auto* ped = reinterpret_cast<CPed*>(v);
        for (auto& w : ped->m_aWeapons) {
            w.m_Type = (eWeaponType)(r.below(3) ? 0 : r.below(48));
            w.m_State = (eWeaponState)r.below(5);
            w.m_TotalAmmo = r.below(3) ? r.below(500) : r.u32() >> r.below(32);
            w.m_AmmoInClip = r.below(40);
        }
        Put32(v, offsetof(CPed, m_nSavedWeapon), (int32_t)(r.below(3) ? 0 : r.below(48)));
        Put32(v, offsetof(CPed, m_nDelayedWeapon), (int32_t)(r.below(3) ? 0 : r.below(48)));
    }
    if (r.below(2)) { Put32(v, offsetof(CPed, m_pWeaponObject), (uint32_t)(uintptr_t)g_clump); }
    // player ped: the player data of the Players[0] slot
    // the live camera's placeable matrix points at its own camera matrix (FindPedToAttack reads TheCamera.GetForward(); the exe reads the camera matrix global directly)
    TheCamera.m_matrix = reinterpret_cast<CMatrixLink*>(&TheCamera.m_mCameraMatrix);
    CWorld::PlayerInFocus = 0;
    auto& pi = CWorld::Players[0];
    FillGarbage(r, &pi, sizeof pi);
    pi.m_pPed = reinterpret_cast<CPlayerPed*>(v);
    FillGarbage(r, g_wanted, sizeof g_wanted); FillGarbage(r, g_clo, sizeof g_clo);
    pi.m_PlayerData.m_pWanted = reinterpret_cast<CWanted*>(g_wanted);
    pi.m_PlayerData.m_pPedClothesDesc = reinterpret_cast<CPedClothesDesc*>(g_clo);
    pi.m_PlayerData.m_nChosenWeapon = (uint8)r.below(13);
    pi.m_PlayerData.m_nPlayerGroup = r.below(8);
    pi.m_PlayerData.m_nDrugLevel = (uint8)r.below(4);
    pi.m_PlayerData.m_pArrestingCop = nullptr;
    pi.m_PlayerData.m_pDangerCar = nullptr;
    pi.m_PlayerData.m_pSpecialAtomic = nullptr;
    pi.m_PlayerData.m_LastHSMissileTarget = nullptr;
    Put32(v, W3POffOf(W2Get(W3POffPData_T{})), (uint32_t)(uintptr_t)&pi.m_PlayerData);
    (void)sz;
}
static void UnwireModel() { CModelInfo::ms_modelInfoPtrs[g_model] = nullptr; }

template<class PF, class EF>
static bool W3PedCase(Rng& r, std::string& d, size_t sz, std::initializer_list<EntOff> ents, PF&& portFn, EF&& exeFn, bool sub = false, bool dtor = false) {
    static std::vector<uint8_t> img0(kImgHi - kImgLo);
    const auto post = std::move(g_post); g_post = nullptr;
    std::memcpy(img0.data(), (void*)kImgLo, img0.size());
    const uint32_t seed = Prep(r, 0);
    FillGarbage(r, g_anim, sizeof g_anim);
    g_subTask = nullptr;
    if (sub) { g_subTask = g_tp.pool->New(); if (g_subTask) ::new (static_cast<void*>(g_subTask)) CTaskSimpleStandStill(50, false, false, 8.0f); }
    if (sz) { FillGarbage(r, g_src, (sz + 3) & ~size_t(3)); for (const auto& e : ents) PutEnt(r, e.off, e.kind, e.size); WirePed(r, g_src, sz); }
    if (post) post(r, g_src);
    Reg thisReg{ (uintptr_t)g_src, sz ? sz : 4 };
    if (dtor) { thisReg.mask.assign(sz ? sz : 4, 0); for (int k = 0; k < 4; ++k) thisReg.mask[k] = 1; }
    const bool ok = BothA(d, seed, { thisReg, Reg{ (uintptr_t)g_anim, sizeof g_anim }, Reg{ (uintptr_t)g_mat, sizeof g_mat }, Reg{ (uintptr_t)g_mi, sizeof g_mi }, Reg{ (uintptr_t)g_col, sizeof g_col },
            Reg{ (uintptr_t)g_intel, sizeof g_intel }, Reg{ (uintptr_t)g_stat, sizeof g_stat }, Reg{ (uintptr_t)g_wanted, sizeof g_wanted }, Reg{ (uintptr_t)g_clo, sizeof g_clo }, Reg{ (uintptr_t)g_out, sizeof g_out }, Reg{ (uintptr_t)g_clump, sizeof g_clump },
            g_tp.RegStruct(), g_tp.RegFlags(), g_tp.RegStore(), g_ep.RegStruct(), g_ep.RegFlags(), g_ep.RegStore(), Reg{ (uintptr_t)rf::pp, sizeof(CPedPool) }, Reg{ (uintptr_t)rf::pedSt, (size_t)rf::kP }, Reg{ kImgLo, kImgHi - kImgLo } },
        [&]() -> uint64_t { return portFn(static_cast<void*>(g_src)); }, [&]() -> uint64_t { return exeFn(static_cast<void*>(g_src)); }, kRf);
    if (!ok) { char t[96]; std::snprintf(t, sizeof t, " [model %d state %d]", g_model, *reinterpret_cast<int*>(g_src + offsetof(CPed, m_nPedState))); d += t; }
    Done();
    UnwireModel();
    std::memcpy((void*)kImgLo, img0.data(), img0.size());
    return ok;
}
} // namespace w3p
using w3p::W3PedCase;

#if __has_include("wave3_oracle_gen_ped.inc")
#include "wave3_oracle_gen_ped.inc"      // GENERATED (tools/gen_w3_ped.py): ped member / static functions on a wired ped
#define W3P_HAVE_GEN 1
#endif

// ---- hand rows ------------------------------------------------------------------------------------------------------------------------------
static void TestW3PedHand() {
    using namespace w2; using namespace w3p;
    // CPed::operator new(size, poolRef) 0x5E4730: slot = poolRef >> 8, the low byte becomes the slot's flag byte; a ref outside the pool writes at an arbitrary address (the generated row used a random int)
    Run("CPed::operator new-poolIndexed 0x5E4730", [&](Rng& r, std::string& d) {
        const int32 ref = (int32)((r.below((uint32)rf::kP) << 8) | r.below(128));
        return W3PedCase(r, d, 0, {}, [&](void*) -> uint64_t { return (uint64_t)(uintptr_t)CPed::operator new(sizeof(CCopPed), ref); },
            [&](void*) -> uint64_t { return (uint64_t)(uintptr_t)oracle::Fn<void* __cdecl(unsigned, int32)>(0x5E4730)(sizeof(CCopPed), ref); });
    }, 60);
}

static void TestW3Ped() {
    TestW3PedHand();
#ifdef W3P_HAVE_GEN
    TestW3PedFuncs();
#endif
}
