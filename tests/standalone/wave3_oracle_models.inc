// ORACLE-W3-MODELS: exe-oracle rows for the uncovered class A/B functions of game_sa/Models on model-info fixtures.
// Fixture g_fx: a model-info object (CBase/Atomic/Clump/Vehicle/Ped/Time ... = the same 0x310 bytes of garbage with plausible fields), a vehicle structure, an upgrade list, a fake vehicle (render lights),
// materials / atomics, an array of small model infos behind the exe's CModelInfo::ms_modelInfoPtrs (0xA9B0C8), the 2dfx store (0xB4C2D8), the vehicle statics (colour, light, wheel upgrade tables).
// Included by wave2_oracle_all.inc when W3_MODELS_ONLY is defined (executable wave3_models_oracle_test = the W2 harness: deterministic arena, whole exe data image compared after every case).
// The fixture memory g_fx is compared as a whole after every call (vptr of the info objects is garbage on both sides and never read: qualified calls).
#include "Models/ModelInfo.h"
#include "Models/BaseModelInfo.h"
#include "Models/AtomicModelInfo.h"
#include "Models/ClumpModelInfo.h"
#include "Models/DamageAtomicModelInfo.h"
#include "Models/LodAtomicModelInfo.h"
#include "Models/LodTimeModelInfo.h"
#include "Models/TimeModelInfo.h"
#include "Models/PedModelInfo.h"
#include "Models/WeaponModelInfo.h"
#include "Models/VehicleModelInfo.h"
#include "cHandlingDataMgr.h"
#include "VisibilityPlugins.h"
#include "TxdStore.h"
#include "Plugins/NodeNamePlugin/NodeName.h"
#include "Cheat.h"
#include "World.h"
#include "Weather.h"
#include "Entity/Ped/PlayerPed.h"
#include "Entity/Vehicle/Vehicle.h"

// the file-static free functions of VehicleModelInfo.cpp, exported by tests/standalone/wave3_models_vehicle_tu.cpp
bool        W3M_IsValidCompRule(int32 rule);
int32       W3M_CountCompsInRule(int32 comps);
int32       W3M_GetListOfComponentsNotUsedByRules(uint32 compRules, int32 numExtras, int32* outList);
RpMaterial* W3M_RemoveWindowAlphaCB(RpMaterial* material, void* data);
RwObject*   W3M_GetOkAndDamagedAtomicCB(RwObject* object, void* data);

namespace w3m {
using namespace w2;
constexpr size_t kFxSize = 0x10000;
alignas(16) static uint8_t g_fx[kFxSize];
constexpr size_t kMi = 0x0000 /*0x310*/, kVS = 0x0320 /*0x320*/, kLU = 0x0640 /*0x80*/, kOut = 0x06C0 /*0x40*/, kStr = 0x0700 /*0x40*/, kMat = 0x0740 /*0x40*/, kAt = 0x0780 /*0x80*/,
                 kInfos = 0x0800 /*16*0x40*/, kVeh = 0x0C00 /*0x800*/, kCol = 0x1400 /*0x100*/, kMi2 = 0x1500 /*0x310*/, kRest = 0x1900 /*0x200*/, kEntries = 0x1B00 /*0x100*/, kPed = 0x2000 /*0x1000*/;
constexpr uint32_t kModelPtrs = 0xA9B0C8, kStore2dfx = 0xB4C2D8, kLodMul = 0xB6F118;
static uint8_t* P(size_t off) { return g_fx + off; }
template<class T> static T& At(size_t off) { return *reinterpret_cast<T*>(g_fx + off); }
static inline uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static std::function<void(Rng&)> g_tweak;

// fresh garbage everywhere, then the plausible fields every row can rely on
static void BuildMi(Rng& r) {
    FillGarbage(r, g_fx, kFxSize);
    At<void*>(kMi + offsetof(CVehicleModelInfo, m_pVehicleStruct)) = P(kVS);
    if (g_tweak) g_tweak(r);
}

static uint16_t FlagsVal(Rng& r) {
    uint32_t f = r.u32();
    if (r.below(3) == 0) f = 1u << r.below(16);
    if (r.below(5) == 0) f = 0;
    if (r.below(7) == 0) f = 0xFFFF;
    return (uint16_t)f;
}
static uint32_t ItemFlags(Rng& r) {   // sItemDefinitionFlags: single bits mixed with random words
    switch (r.below(5)) { case 0: return 0; case 1: return 1u << r.below(32); case 2: return r.u32(); case 3: return (1u << r.below(32)) | (1u << r.below(32)); default: return r.u32() & 0x0FFFFFFF; }
}

template<class PF, class EF>
static bool MCase(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&) = &BuildMi) {
    const uint32_t seed = r.u32() | 1;
    build(r);
    Reg fx{ (uintptr_t)g_fx, kFxSize };
    Reg img{ kImgLo, kImgHi - kImgLo };
    return BothA(d, seed, { fx, img }, std::forward<PF>(pf), std::forward<EF>(ef));
}
// `this` = the info object of the fixture, exe thiscall / static cdecl call helpers
template<class R, class... A> static R ExeT(uint32_t addr, void* self, A... a) { return oracle::Fn<R __fastcall(void*, int, A...)>(addr)(self, 0, a...); }
template<class R, class... A> static R ExeC(uint32_t addr, A... a) { return oracle::Fn<R __cdecl(A...)>(addr)(a...); }

// ---- builders for the table rows ---------------------------------------------------------------------------------------------------------------------------------------------------
// 16 small infos behind ms_modelInfoPtrs; every entry points at one of them (or is null when `nulls`)
static void FillInfoTable(Rng& r, bool nulls) {
    static const uint32_t keys[] = { 0x11111111u, 0x22222222u, 0xDEADBEEFu, 0, 0x7FFFFFFFu, 0xFFFFFFFFu };
    for (int i = 0; i < 16; ++i) {
        At<uint32_t>(kInfos + i * 0x40 + 4) = r.below(4) ? keys[r.below(6)] : r.u32();
    }
    auto* tab = reinterpret_cast<void**>(kModelPtrs);
    for (int i = 0; i < CModelInfo::NUM_MODEL_INFOS; ++i) tab[i] = (nulls && r.below(3)) ? nullptr : static_cast<void*>(P(kInfos + r.below(16) * 0x40));
    if (nulls && r.below(3) == 0) { const int k = r.below(CModelInfo::NUM_MODEL_INFOS); tab[k] = P(kInfos + r.below(16) * 0x40); }
}
static void BuildTableReInit(Rng& r) { BuildMi(r); FillInfoTable(r, false); StaticRef<uint32_t>(kStore2dfx) = r.u32(); }
static void BuildTableHash(Rng& r) {
    BuildMi(r);
    FillInfoTable(r, true);
    // make hits likely: some entries hold exactly the key searched later
}
static void BuildVehicleStatics(Rng& r) {
    BuildMi(r);
    for (int i = 0; i < 4; ++i) { StaticRef<int16_t>(0xB4E470 + 2 * i) = (int16_t)r.below(15); }   // ms_numWheelUpgrades: room for one more
    if (r.below(8) == 0) StaticRef<int16_t>(0xB4E470 + 2 * r.below(4)) = 0;
}
// ms_modelInfoPtrs entries and the vehicle info fixture for the passenger-count function
static int g_modelId = 0;
static void BuildPassengers(Rng& r) {
    BuildMi(r);
    static const int ids[] = { 407 /*firetruk*/, 508 /*journey*/, 425 /*hunter*/, 441 /*rcbandit*/, 564 /*rctiger*/, 437 /*coach*/, 431 /*bus*/, 400, 411, 509, 522, 471, 595, 590, 560, 448, 481 };
    g_modelId = r.below(3) ? ids[r.below(17)] : 400 + r.below(212);
    auto* tab = reinterpret_cast<void**>(kModelPtrs);
    tab[g_modelId] = P(kMi);
    // vehicle struct with dummy positions (seat rear dummy is the 4th? vector: random zero / non-zero)
    auto* vi = reinterpret_cast<CVehicleModelInfo*>(P(kMi));
    vi->m_nVehicleType = (eVehicleType)r.below(12);
    vi->m_nNumDoors = (uint8)(r.below(3) ? r.below(6) : 0);
    vi->m_nHandlingId = (uint16)r.below(210);
    for (int i = 0; i < 15; ++i) { if (r.below(2)) { At<float>(kVS + i * 12) = 0.f; At<float>(kVS + i * 12 + 4) = 0.f; At<float>(kVS + i * 12 + 8) = 0.f; } }
    for (int i = 0; i < 210; ++i) gHandlingDataMgr.m_aVehicleHandling[i].m_bTandemSeats = r.below(4) == 0;   // the exe reads the same bytes of its data image
}
} // namespace w3m

// W3M_SCALE=<k>: every row runs k times its case count (deeper sweeps)
template<class Fn> static void RunScaled(const std::string& name, Fn&& fn, int cases) {
    static const int scale = std::getenv("W3M_SCALE") ? std::max(1, std::atoi(std::getenv("W3M_SCALE"))) : 1;
    Run(name, std::forward<Fn>(fn), cases * scale);
}
#define Run RunScaled

static void TestW3Models() {
    using namespace w2; using namespace w3m;
    auto BM = [] { return reinterpret_cast<CBaseModelInfo*>(P(kMi)); };
    auto AM = [] { return reinterpret_cast<CAtomicModelInfo*>(P(kMi)); };
    auto CM = [] { return reinterpret_cast<CClumpModelInfo*>(P(kMi)); };
    auto VM = [] { return reinterpret_cast<CVehicleModelInfo*>(P(kMi)); };
    auto PM = [] { return reinterpret_cast<CPedModelInfo*>(P(kMi)); };
#define M0(NAME, ADDR, RET, EXPR, CASES) \
    Run(NAME " " #ADDR, [&](Rng& r, std::string& d) { \
        return MCase(r, d, [&]() -> uint64_t { return (uint64_t)(uint32_t)(uintptr_t)(EXPR); }, [&]() -> uint64_t { return (uint64_t)(uint32_t)ExeT<RET>(ADDR, P(kMi)); }); }, CASES)
#define M0B(NAME, ADDR, EXPR, CASES)     Run(NAME " " #ADDR, [&](Rng& r, std::string& d) {         return MCase(r, d, [&]() -> uint64_t { return (EXPR) ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(ADDR, P(kMi)) != 0 ? 1 : 0; }); }, CASES)   /* bool result: the exe returns the masked flag value in al (2, 64, 128 ...), callers only test it */
#define M0W(NAME, ADDR, EXPR, CASES)     Run(NAME " " #ADDR, [&](Rng& r, std::string& d) {         return MCase(r, d, [&]() -> uint64_t { return (EXPR) ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint32_t>(ADDR, P(kMi)) != 0 ? 1 : 0; }); }, CASES)   /* the exe returns `flags & mask` in the whole eax (0x200 ...), the port a bool: only "non-zero" is comparable (no caller left in the exe: all inlined) */
#define M0V(NAME, ADDR, EXPR, CASES) \
    Run(NAME " " #ADDR, [&](Rng& r, std::string& d) { \
        return MCase(r, d, [&]() -> uint64_t { EXPR; return 0; }, [&]() -> uint64_t { ExeT<void>(ADDR, P(kMi)); return 0; }); }, CASES)

    // ---- CBaseModelInfo: virtual defaults (called qualified: the base implementations), tex dictionary / 2dfx helpers, flag accessors
    M0("CBaseModelInfo::AsAtomicModelInfoPtr", 0x4C4A80, uint32_t, BM()->CBaseModelInfo::AsAtomicModelInfoPtr(), 40);
    M0("CBaseModelInfo::AsDamageAtomicModelInfoPtr", 0x4C4A90, uint32_t, BM()->CBaseModelInfo::AsDamageAtomicModelInfoPtr(), 40);
    M0("CBaseModelInfo::AsLodAtomicModelInfoPtr", 0x4C4AA0, uint32_t, BM()->CBaseModelInfo::AsLodAtomicModelInfoPtr(), 40);
    M0("CBaseModelInfo::GetTimeInfo", 0x4C4AB0, uint32_t, BM()->CBaseModelInfo::GetTimeInfo(), 40);
    M0V("CBaseModelInfo::Init", 0x4C4B10, BM()->CBaseModelInfo::Init(), 200);
    M0V("CBaseModelInfo::ConvertAnimFileIndex", 0x4C4AD0, BM()->CBaseModelInfo::ConvertAnimFileIndex(), 40);
    M0("CBaseModelInfo::GetAnimFileIndex", 0x4C4AE0, uint32_t, BM()->CBaseModelInfo::GetAnimFileIndex(), 40);
    Run("CBaseModelInfo::SetAnimFile 0x4C4AC0", [&](Rng& r, std::string& d) {
        const char* s = reinterpret_cast<const char*>(P(kStr));
        return MCase(r, d, [&]() -> uint64_t { BM()->CBaseModelInfo::SetAnimFile(s); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C4AC0, P(kMi), s); return 0; });
    }, 40);
    M0V("CBaseModelInfo::ClearTexDictionary", 0x4C4B70, BM()->ClearTexDictionary(), 100);
    M0V("CBaseModelInfo::Init2dEffects", 0x4C4C20, BM()->Init2dEffects(), 100);
    M0W("CBaseModelInfo::GetIsDrawLast", 0x5328C0, BM()->GetIsDrawLast(), 200);
    M0W("CBaseModelInfo::HasBeenPreRendered", 0x5328B0, BM()->HasBeenPreRendered(), 200);
    M0W("CBaseModelInfo::HasComplexHierarchy", 0x4C4E00, BM()->HasComplexHierarchy(), 200);
    M0W("CBaseModelInfo::IsBackfaceCulled", 0x5328F0, BM()->IsBackfaceCulled(), 200);
    M0W("CBaseModelInfo::IsLod", 0x4C4A00, BM()->IsLod(), 200);
    M0W("CBaseModelInfo::IsRoad", 0x4C4DF0, BM()->IsRoad(), 200);
    M0V("CBaseModelInfo::IncreaseAlpha", 0x532900, BM()->IncreaseAlpha(), 300);
    // special type (bits 11-14 of the flags): the searched type about a third of the time, else random
    for (const auto& row : { std::make_tuple("CBaseModelInfo::IsBreakableStatuePart 0x59F090", 0x59F090, 11), std::make_tuple("CBaseModelInfo::IsTagModel 0x49CC20", 0x49CC20, 6) }) {
        const uint32_t addr = (uint32_t)std::get<1>(row), want = (uint32_t)std::get<2>(row);
        Run(std::get<0>(row), [&](Rng& r, std::string& d) {
            g_tweak = [&](Rng& rr) { uint16_t f = At<uint16_t>(kMi + 0x12); const uint32_t t = rr.below(3) == 0 ? want : rr.below(16); At<uint16_t>(kMi + 0x12) = (uint16_t)((f & ~0x7800u) | (t << 11)); };
            const bool ok = MCase(r, d, [&]() -> uint64_t { return (want == 11 ? BM()->IsBreakableStatuePart() : BM()->IsTagModel()) ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(addr, P(kMi)) != 0 ? 1 : 0; });
            g_tweak = nullptr;
            return ok;
        }, 300);
    }
    Run("CBaseModelInfo::SwaysInWind_Hook 0x4212C0", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return (uint32_t)BM()->SwaysInWind(); }, [&]() -> uint64_t { return (uint32_t)ExeT<uint32_t>(0x4212C0, P(kMi)); });
    }, 400);
    Run("CBaseModelInfo::SetHasBeenPreRendered 0x4C42F0", [&](Rng& r, std::string& d) {
        const int32_t v = r.below(2) ? (int32_t)r.below(2) : (int32_t)r.u32();
        return MCase(r, d, [&]() -> uint64_t { BM()->SetHasBeenPreRendered(v); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C42F0, P(kMi), v); return 0; });
    }, 200);
    Run("CBaseModelInfo::SetIsLod 0x4C4A10", [&](Rng& r, std::string& d) {
        const int v = r.below(2);
        return MCase(r, d, [&]() -> uint64_t { BM()->SetIsLod(v != 0); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C4A10, P(kMi), v); return 0; });
    }, 100);
    Run("CBaseModelInfo::SetOwnsColModel 0x5328D0", [&](Rng& r, std::string& d) {
        const int v = r.below(2);
        return MCase(r, d, [&]() -> uint64_t { BM()->SetOwnsColModel(v != 0); return 0; }, [&]() -> uint64_t { ExeT<void>(0x5328D0, P(kMi), v); return 0; });
    }, 100);
    Run("CBaseModelInfo::Add2dEffect 0x4C4D20", [&](Rng& r, std::string& d) {
        const int idx = r.below(100);
        auto* eff = reinterpret_cast<C2dEffect*>(kStore2dfx + 4 + (uintptr_t)idx * sizeof(C2dEffect));
        return MCase(r, d, [&]() -> uint64_t { BM()->Add2dEffect(eff); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C4D20, P(kMi), eff); return 0; });
    }, 300);
    Run("::SetBaseModelInfoFlags 0x5B3AD0", [&](Rng& r, std::string& d) {
        const uint32_t f = ItemFlags(r);
        return MCase(r, d, [&]() -> uint64_t { ::SetBaseModelInfoFlags(BM(), f); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5B3AD0, P(kMi), f); return 0; });
    }, 400);
    // ---- B: txd refcount + LOD distance + 2dfx re-init + hash lookup
    Run("CBaseModelInfo::GetAtomicFromDistance (CAtomicModelInfo) 0x4C44B0", [&](Rng& r, std::string& d) {
        float dist; const float mul = 0.25f + r.f01() * 3.f;
        const float dd = 5.f + r.f01() * 400.f;
        switch (r.below(6)) { case 0: dist = mul * dd; break; case 1: dist = std::nextafter(mul * dd, 0.f); break; case 2: dist = std::nextafter(mul * dd, 1e9f); break; case 3: dist = r.below(2) ? NAN : INFINITY; break; default: dist = r.f01() * 800.f; }
        g_tweak = [&](Rng&) { At<float>(kMi + 0x18) = dd; StaticRef<float>(kLodMul) = mul; };
        const bool ok = MCase(r, d, [&]() -> uint64_t { return U(AM()->GetAtomicFromDistance(dist)); }, [&]() -> uint64_t { return (uint32_t)ExeT<uint32_t>(0x4C44B0, P(kMi), dist); });
        g_tweak = nullptr;
        return ok;
    }, 600);
    Run("CModelInfo::ReInit2dEffects 0x4C63B0", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { CModelInfo::ReInit2dEffects(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x4C63B0); return 0; }, &BuildTableReInit);
    }, 60);
    Run("CModelInfo::GetModelInfoFromHashKey 0x4C59B0", [&](Rng& r, std::string& d) {
        // the key searched: one of the infos' keys (hit), or a random one (miss)
        static const uint32_t keys[] = { 0x11111111u, 0x22222222u, 0xDEADBEEFu, 0, 0x7FFFFFFFu, 0xFFFFFFFFu };
        const uint32_t h = r.below(4) ? keys[r.below(6)] : r.u32();
        const bool wantIdx = r.below(4) != 0;
        return MCase(r, d,
            [&]() -> uint64_t { return U(CModelInfo::GetModelInfoFromHashKey(h, wantIdx ? &At<int32_t>(kOut) : nullptr)); },
            [&]() -> uint64_t { return (uint32_t)ExeC<uint32_t>(0x4C59B0, h, wantIdx ? &At<int32_t>(kOut) : nullptr); }, &BuildTableHash);
    }, 300);
    Run("CModelInfo::Get2dEffectStore 0x4C5A60", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return U(CModelInfo::Get2dEffectStore()); }, [&]() -> uint64_t { return (uint32_t)ExeC<uint32_t>(0x4C5A60); });
    }, 20);
    // ---- CAtomicModelInfo / Damage / Lod / Time / LodTime / Weapon / Clump
    M0("CAtomicModelInfo::AsAtomicModelInfoPtr", 0x4C5560, uint32_t, AM()->CAtomicModelInfo::AsAtomicModelInfoPtr(), 40);
    M0V("CAtomicModelInfo::Init", 0x4C4430, AM()->CAtomicModelInfo::Init(), 100);
    M0("CAtomicModelInfo::GetRwModelType", 0x4C5580, uint32_t, AM()->CAtomicModelInfo::GetRwModelType(), 40);
    M0("CDamageAtomicModelInfo::AsDamageAtomicModelInfoPtr", 0x4C55C0, uint32_t, reinterpret_cast<CDamageAtomicModelInfo*>(P(kMi))->CDamageAtomicModelInfo::AsDamageAtomicModelInfoPtr(), 40);
    M0V("CDamageAtomicModelInfo::Init", 0x4C48B0, reinterpret_cast<CDamageAtomicModelInfo*>(P(kMi))->CDamageAtomicModelInfo::Init(), 100);
    M0("CLodAtomicModelInfo::AsLodAtomicModelInfoPtr", 0x4C5610, uint32_t, reinterpret_cast<CLodAtomicModelInfo*>(P(kMi))->CLodAtomicModelInfo::AsLodAtomicModelInfoPtr(), 40);
    M0V("CLodAtomicModelInfo::Init", 0x4C54D0, reinterpret_cast<CLodAtomicModelInfo*>(P(kMi))->CLodAtomicModelInfo::Init(), 100);
    M0("CLodTimeModelInfo::GetTimeInfo", 0x4C56D0, uint32_t, reinterpret_cast<CLodTimeModelInfo*>(P(kMi))->CLodTimeModelInfo::GetTimeInfo(), 40);
    M0("CTimeModelInfo::GetTimeInfo", 0x4C5670, uint32_t, reinterpret_cast<CTimeModelInfo*>(P(kMi))->CTimeModelInfo::GetTimeInfo(), 40);
    M0V("CWeaponModelInfo::Init", 0x4C98F0, reinterpret_cast<CWeaponModelInfo*>(P(kMi))->CWeaponModelInfo::Init(), 100);
    M0V("CClumpModelInfo::Init", 0x4C4E40, CM()->CClumpModelInfo::Init(), 100);
    M0("CClumpModelInfo::GetRwModelType", 0x4C5730, uint32_t, CM()->CClumpModelInfo::GetRwModelType(), 40);
    M0("CClumpModelInfo::GetAnimFileIndex", 0x4C5740, uint32_t, CM()->CClumpModelInfo::GetAnimFileIndex(), 40);
    Run("CClumpModelInfo::GetBoundingBox 0x4C5710", [&](Rng& r, std::string& d) {
        g_tweak = [&](Rng&) { At<void*>(kMi + 0x14) = P(kCol); };
        const bool ok = MCase(r, d, [&]() -> uint64_t { return U(CM()->CClumpModelInfo::GetBoundingBox()); }, [&]() -> uint64_t { return (uint32_t)ExeT<uint32_t>(0x4C5710, P(kMi)); });
        g_tweak = nullptr;
        return ok;
    }, 40);
    // CClumpModelInfo::SetAtomicRendererCB 0x4C5280: row in wave3_oracle_models4.inc (recording stubs for the RW calls)
    Run("::SetAtomicModelInfoFlags 0x5B3B20", [&](Rng& r, std::string& d) {
        const uint32_t f = ItemFlags(r);
        return MCase(r, d, [&]() -> uint64_t { ::SetAtomicModelInfoFlags(AM(), f); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5B3B20, P(kMi), f); return 0; });
    }, 600);
    Run("::SetClumpModelInfoFlags 0x5B3C30", [&](Rng& r, std::string& d) {
        const uint32_t f = ItemFlags(r);
        return MCase(r, d, [&]() -> uint64_t { ::SetClumpModelInfoFlags(CM(), f); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5B3C30, P(kMi), f); return 0; });
    }, 600);
    Run("CAtomicModelInfo::SetupVehicleUpgradeFlags 0x4C4570", [&](Rng& r, std::string& d) {
        static const char* const pre[] = { "chss_", "wheel_", "exh_", "fbmp_", "rbmp_", "misc_a_", "misc_b_", "misc_c_", "bnt_", "bntl_", "bntr_", "spl_", "wg_l_", "wg_r", "fbb_", "bbb_", "lgt_", "rf_", "nto_", "hydralics", "stereo", "", "zz_", "wheel", "bn", "wg_" };
        std::string name = pre[r.below(26)];
        const int ex = r.below(4); for (int i = 0; i < ex; ++i) name += (char)('a' + r.below(26));
        g_tweak = [&](Rng&) { std::memset(P(kStr), 0, 0x40); std::strcpy(reinterpret_cast<char*>(P(kStr)), name.c_str()); };
        const char* s = reinterpret_cast<const char*>(P(kStr));
        const bool ok = MCase(r, d, [&]() -> uint64_t { AM()->SetupVehicleUpgradeFlags(s); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C4570, P(kMi), s); return 0; });
        g_tweak = nullptr;
        return ok;
    }, 800);
    // ---- CTimeInfo (4 bytes: on, off, other model)
    Run("CTimeInfo::GetOtherTimeModel 0x4C4A30", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return (uint32_t)reinterpret_cast<CTimeInfo*>(P(kMi))->GetOtherTimeModel(); }, [&]() -> uint64_t { return (uint32_t)ExeT<uint32_t>(0x4C4A30, P(kMi)); });
    }, 100);
    Run("CTimeInfo::GetTimeOff 0x407330", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return reinterpret_cast<CTimeInfo*>(P(kMi))->GetTimeOff(); }, [&]() -> uint64_t { return ExeT<uint8_t>(0x407330, P(kMi)); });
    }, 100);
    Run("CTimeInfo::GetTimeOn 0x407320", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return reinterpret_cast<CTimeInfo*>(P(kMi))->GetTimeOn(); }, [&]() -> uint64_t { return ExeT<uint8_t>(0x407320, P(kMi)); });
    }, 100);
    Run("CTimeInfo::SetOtherTimeModel 0x5B3440", [&](Rng& r, std::string& d) {
        const int16_t m = r.below(4) ? (int16_t)r.below(20000) : (int16_t)r.u32();
        return MCase(r, d, [&]() -> uint64_t { reinterpret_cast<CTimeInfo*>(P(kMi))->SetOtherTimeModel(m); return 0; }, [&]() -> uint64_t { ExeT<void>(0x5B3440, P(kMi), (int)m); return 0; });
    }, 100);
    Run("CTimeInfo::SetTimes 0x5B3430", [&](Rng& r, std::string& d) {
        const uint8_t a = (uint8_t)r.below(r.below(4) ? 25 : 256), b = (uint8_t)r.below(r.below(4) ? 25 : 256);
        return MCase(r, d, [&]() -> uint64_t { reinterpret_cast<CTimeInfo*>(P(kMi))->SetTimes(a, b); return 0; }, [&]() -> uint64_t { ExeT<void>(0x5B3430, P(kMi), (int)a, (int)b); return 0; });
    }, 100);
    // ---- CPedModelInfo
    Run("CPedModelInfo::AddXtraAtomics 0x4C6D40", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { PM()->AddXtraAtomics(reinterpret_cast<RpClump*>(P(kAt))); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C6D40, P(kMi), P(kAt)); return 0; });
    }, 20);
    Run("CPedModelInfo::SetFaceTexture 0x4C6D50", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { PM()->SetFaceTexture(reinterpret_cast<RwTexture*>(P(kAt))); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C6D50, P(kMi), P(kAt)); return 0; });
    }, 20);
    Run("CPedModelInfo::IncrementVoice 0x4C7300", [&](Rng& r, std::string& d) {
        g_tweak = [&](Rng& rr) {   // voice min / max / id: small values, negatives, equal, id past max
            int16_t p[3];
            for (int i = 0; i < 3; ++i) p[i] = (int16_t)(rr.below(5) == 0 ? -1 - (int)rr.below(3) : (int)rr.below(12));
            if (rr.below(3) == 0) p[2] = p[1];
            if (rr.below(5) == 0) p[2] = (int16_t)(p[1] + 1);
            std::memcpy(P(kMi + offsetof(CPedModelInfo, m_nVoiceMin)), &p[0], 2); std::memcpy(P(kMi + offsetof(CPedModelInfo, m_nVoiceMax)), &p[1], 2); std::memcpy(P(kMi + offsetof(CPedModelInfo, m_nVoiceId)), &p[2], 2);
        };
        const bool ok = MCase(r, d, [&]() -> uint64_t { PM()->IncrementVoice(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C7300, P(kMi)); return 0; });
        g_tweak = nullptr;
        return ok;
    }, 600);
}

#include "wave3_oracle_models2.inc"
#include "wave3_oracle_models3.inc"
#include "wave3_oracle_models4.inc"

static void TestW3ModelsAll() {
    TestW3Models();
    TestW3ModelsVeh();
    TestW3ModelsPart3();
    TestW3ModelsPart4();
}
#undef Run
