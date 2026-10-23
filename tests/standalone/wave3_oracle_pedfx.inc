// ORACLE-W3-PED fixture (wired ped, W3PedCase), split out of wave3_oracle_ped.inc so that wave3_oracle_ev.inc (ORACLE-W3-EVENTS) can reuse it.
#pragma once
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
// the speech entity at CPed+0x294 (protected members): offsets via the access trick, same as above
struct W3POffSpeech_T { using type = CAEPedSpeechAudioEntity CPed::*; friend type W2Get(W3POffSpeech_T); }; template struct W2Rob<W3POffSpeech_T, &CPed::m_pedSpeech>;
struct W3SpInit_T { using type = bool CAEPedSpeechAudioEntity::*; friend type W2Get(W3SpInit_T); }; template struct W2Rob<W3SpInit_T, &CAEPedSpeechAudioEntity::m_IsInitialized>;
struct W3SpType_T { using type = eAudioPedType CAEPedSpeechAudioEntity::*; friend type W2Get(W3SpType_T); }; template struct W2Rob<W3SpType_T, &CAEPedSpeechAudioEntity::m_PedAudioType>;
struct W3SpVoice_T { using type = ePedSpeechVoiceS16 CAEPedSpeechAudioEntity::*; friend type W2Get(W3SpVoice_T); }; template struct W2Rob<W3SpVoice_T, &CAEPedSpeechAudioEntity::m_VoiceID>;
struct W3SpOff_T { using type = bool CAEPedSpeechAudioEntity::*; friend type W2Get(W3SpOff_T); }; template struct W2Rob<W3SpOff_T, &CAEPedSpeechAudioEntity::m_IsSpeechDisabled>;
struct W3SpScr_T { using type = bool CAEPedSpeechAudioEntity::*; friend type W2Get(W3SpScr_T); }; template struct W2Rob<W3SpScr_T, &CAEPedSpeechAudioEntity::m_IsSpeechForScriptsDisabled>;
template<class C, class M> static size_t W3POffM(M C::* pm) { uint32_t o = 0; static_assert(sizeof pm == 4); std::memcpy(&o, &pm, 4); return o; }
namespace w2 {   // ped arguments in their game range: the port indexes its tables with them (assert), the exe reads past the table end
template<> eWeaponType GE<eWeaponType>(Rng& r) { return (eWeaponType)(r.below(8) ? r.below(47) : r.below(60)); }
template<> eSprintType GE<eSprintType>(Rng& r) { return (eSprintType)r.below(4); }
template<> ePedState GE<ePedState>(Rng& r) { return (ePedState)r.below(60); }
template<> eWantedLevel GE<eWantedLevel>(Rng& r) { return (eWantedLevel)r.below(7); }   // the port asserts level <= 6; the exe skips the chaos assignment above 6 (never reached from the game)
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
// a real CWanted (the player's m_pWanted): zeroed, with the fields the wanted functions read in their game range. The exe statics (maximum wanted / chaos level, the "I do as I please" cheat byte) are
// set per case through the exe data image (the port's globals sit at the same addresses and the image is restored after the case)
static void WireWanted(Rng& r) {
    std::memset(g_wanted, 0, sizeof g_wanted);
    auto& w = *reinterpret_cast<CWanted*>(g_wanted);
    w.m_ChaosLevel = (int32)(r.below(3) ? r.below(5200) : r.below(10000));
    w.m_ChaosLevelBeforeParole = (int32)r.below(5200);
    w.m_WantedLevel = (eWantedLevel)r.below(7);
    w.m_WantedLevelBeforeParole = (eWantedLevel)r.below(7);
    w.m_Multiplier = 1.f;
    w.m_LastTimeWantedLevelChanged = r.below(100000);
    w.m_Flags = (uint8_t)(r.below(6) ? 0 : r.below(64));   // police back-off / swat / fbi / army required bits
    for (auto& c : w.m_CrimesBeingQd) { c.m_nCrimeType = (eCrimeType)(r.below(3) ? 0 : 1 + r.below(21)); c.m_nStartTime = r.below(100000); }
    *reinterpret_cast<volatile int32_t*>(0x8CDEE4) = (int32_t)(r.below(4) ? 6 : r.below(7));        // CWanted::MaximumWantedLevel
    *reinterpret_cast<volatile int32_t*>(0x8CDEE8) = (int32_t)(r.below(4) ? 9200 : 200 + r.below(9000));   // CWanted::MaximumChaosLevel
    *reinterpret_cast<volatile uint8_t*>(0x969171) = (uint8_t)(r.below(8) == 0);                   // CCheat: I do as I please
    *reinterpret_cast<volatile uint8_t*>(0x8CDE56) = 0;   // CStats::CheckForStatsMessage (via UpdateWantedLevel -> IncrementStat) is a no-op: the stat help message needs CHud / CText, which the harness does not have
}
// the ped's CAEPedSpeechAudioEntity (CPed+0x294) as Initialise leaves it: initialised flag, audio type and voice in the range of the speech tables (the port indexes them with an assert), disable flags
static void WireSpeech(Rng& r, uint8_t* v) {
    uint8_t* s = v + W3POffM(W2Get(W3POffSpeech_T{}));
    const int16_t pt = (int16_t)r.below(6);   // GEN EMG PLAYER GANG GFD SPC
    static constexpr uint32_t kVoices[6] = { 209, 46, 20, 52, 18, 1 };
    const int16_t voice = (int16_t)r.below(kVoices[pt]);
    s[W3POffM(W2Get(W3SpInit_T{}))] = (uint8_t)(r.below(4) != 0);
    std::memcpy(s + W3POffM(W2Get(W3SpType_T{})), &pt, 2);
    std::memcpy(s + W3POffM(W2Get(W3SpVoice_T{})), &voice, 2);
    s[W3POffM(W2Get(W3SpOff_T{}))] = (uint8_t)r.below(2);
    s[W3POffM(W2Get(W3SpScr_T{}))] = (uint8_t)r.below(2);
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
    // a player ped is PLAYER1 / PLAYER2 (CPad::GetPad(0/1) in ControlButtonSprint, the port and the exe pass a null pad for any other type: 95% of the cases ended as agreed faults)
    if (sz == sizeof(CPlayerPed) && r.below(8) != 0) { Put32(v, offsetof(CPed, m_nPedType), (int32_t)r.below(2)); }
    else { Put32(v, offsetof(CPed, m_nPedType), (int32_t)r.below(32)); }
    if (sz == sizeof(CPlayerPed)) {   // the two pads: the button / stick state the sprint, targeting and weapon code reads
        for (int k = 0; k < 2; ++k) { FillGarbage(r, &CPad::Pads[k], sizeof(CPad)); }
    }
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
    {   // CPhysical lighting terms (IsHidden -> GetLightingTotal): finite, as the game keeps them (garbage floats trap in the FP unit and ended 88 of 120 cases)
        auto* phys = reinterpret_cast<CPhysical*>(v);
        phys->m_fContactSurfaceBrightness = r.f01() * 2.f - 0.5f;
        phys->m_fDynamicLighting = r.below(4) ? 0.f : r.f01() * 0.2f;
    }
    // player ped: the player data of the Players[0] slot
    // the live camera's placeable matrix points at its own camera matrix (FindPedToAttack reads TheCamera.GetForward(); the exe reads the camera matrix global directly)
    TheCamera.m_matrix = reinterpret_cast<CMatrixLink*>(&TheCamera.m_mCameraMatrix);
    CWorld::PlayerInFocus = 0;
    auto& pi = CWorld::Players[0];
    FillGarbage(r, &pi, sizeof pi);
    pi.m_pPed = reinterpret_cast<CPlayerPed*>(v);
    FillGarbage(r, g_clo, sizeof g_clo);
    WireWanted(r);
    WireSpeech(r, v);
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

// ORACLE-C-EVENTS: class C rows of game_sa/Entity/Ped (virtual calls in the exe closure): W3PedCase(..., vt) writes the EXE's vftable of the ped class into word 0 of the ped `this` before the exe call and the PORT's
// vftable (the compiler's ??_7 symbol through /alternatename, no object needed) before the port call, and clears it afterwards so the compare ignores it (same scheme as wave3_oracle_tasks.inc).
#define W3_PORT_VT(C) extern "C" const uint32_t w3vt_##C[]; __pragma(comment(linker, "/alternatename:_w3vt_" #C "=??_7" #C "@@6B@"))
struct PVtEnt { const char* name; uint32_t exeVt; const uint32_t* portVt; };
static void SetPedVt(const PVtEnt* vt, int side) { if (vt) Put32(g_src, 0, side == 1 ? (uint32_t)(uintptr_t)vt->portVt : side == 2 ? vt->exeVt : 0); }

template<class PF, class EF>
static bool W3PedCase(Rng& r, std::string& d, size_t sz, std::initializer_list<EntOff> ents, PF&& portFn, EF&& exeFn, bool sub = false, bool dtor = false, const PVtEnt* vt = nullptr) {
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
        [&]() -> uint64_t { SetPedVt(vt, 1); const uint64_t rv = portFn(static_cast<void*>(g_src)); SetPedVt(vt, 0); return rv; }, [&]() -> uint64_t { SetPedVt(vt, 2); const uint64_t rv = exeFn(static_cast<void*>(g_src)); SetPedVt(vt, 0); return rv; }, kRf);
    if (!ok) { char t[96]; std::snprintf(t, sizeof t, " [model %d state %d]", g_model, *reinterpret_cast<int*>(g_src + offsetof(CPed, m_nPedState))); d += t; }
    Done();
    UnwireModel();
    std::memcpy((void*)kImgLo, img0.data(), img0.size());
    return ok;
}
} // namespace w3p
using w3p::W3PedCase;
