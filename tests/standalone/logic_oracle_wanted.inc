// (a) wanted / crime / police: CWanted (all members), CGameLogic forbidden territories (IsPointWithinLineArea, SetPlayerWantedLevelForForbiddenTerritories), CPedType flags/acquaintances.
// Fixture: a heap CWanted (+ zeroed fake player ped / CPlayerInfo wired into CWorld::Players, fake ped / vehicle pools at the exe's pool pointers); the port and the exe run on the
// SAME memory (the port's statics are StaticRef<> into the mapped exe image), so each case is: snapshot -> port -> snapshot -> restore -> exe -> snapshot -> compare.
#include "Wanted.h"
#include "Stats.h"
#include "GameLogic.h"
#include "Cheat.h"
#include "Darkel.h"
#include "GangWars.h"
#include "Game.h"
#include "Weather.h"
#include "World.h"
#include "PlayerInfo.h"
#include "Pools/Pools.h"
#include "Pools/PedPool.h"
#include "Pools/VehiclePool.h"
#include "Entity/Ped/CopPed.h"
#include "Entity/Ped/PlayerPed.h"
#include "Entity/Vehicle/Vehicle.h"
#include "PedType.h"
#include "Crime.h"
#include "Messages.h"
#include "Text/Text.h"
#include "Localisation.h"
#include "eModelID.h"

// private / protected members via explicit instantiation (access checks do not apply to the template arguments of an explicit instantiation)
#define LO_STEAL(TAG, TYPE, MEMBER) \
    struct TAG { using type = TYPE; friend type Get(TAG); }; \
    template<class T, typename T::type M> struct Rob_##TAG { friend typename T::type Get(T) { return M; } }; \
    template struct Rob_##TAG<TAG, MEMBER>;
LO_STEAL(T_Displace, CCopPed* (*)(CCopPed*, CCopPed**), &CWanted::ComputePursuitCopToDisplace)
LO_STEAL(T_JoinStatic, bool (*)(CCopPed*, uint8, CCopPed**, uint8&), &CWanted::CanCopJoinPursuit)
LO_STEAL(T_RemoveStatic, void (*)(CCopPed*, CCopPed**, uint8&), &CWanted::RemovePursuitCop)
LO_STEAL(T_Stored, bool CWanted::*, &CWanted::m_StoredPoliceBackOff)
LO_STEAL(T_PedPD, CPlayerPedData* CPed::*, &CPed::m_pPlayerData)
LO_STEAL(T_PedInt, CPedIntelligence* CPed::*, &CPed::m_pIntelligence)

// recorders for the police scanner audio entity (port: defined here instead of AEPoliceScannerAudioEntity.cpp; exe: patched to the host function)
static std::vector<std::array<uint32, 5>> g_audioEv;
namespace lw {
static void* Z(size_t n) { return std::calloc(1, (n + 63) & ~size_t(63)); }
constexpr int kPeds = 8, kVehs = 6;
static CWanted*    W;          // the CWanted under test (heap)
static CPlayerPed* Pl;         // fake player ped (Players[0].m_pPed)
static CCopPed*    PedObj[kPeds];
static CVehicle*   VehObj[kVehs];
static uint8*      PedState; static uint8* VehState;   // pool slot-state bytes (bit 7 = free)
static bool        inited = false;
static void      (*g_prep)(CWanted&) = nullptr;
static void      (*g_prepWorld)(Rng&) = nullptr;   // optional: reshapes the generated world (player position, statics) after GenWorld/RandomStats/GenPools   // optional fix-up of the generated CWanted (e.g. to keep a known endless loop of the original out of the test)

static void Init() {
    if (inited) return;
    inited = true;
    W  = (CWanted*)Z(sizeof(CWanted));
    Pl = (CPlayerPed*)Z(sizeof(CPlayerPed));
    auto* pedStore = (uint8*)Z(kPeds * sizeof(CCopPed));
    auto* vehStore = (uint8*)Z(kVehs * sizeof(CHeli));
    PedState = (uint8*)Z(kPeds); VehState = (uint8*)Z(kVehs);
    auto* pp = new (Z(sizeof(CPedPool))) CPedPool(kPeds, pedStore, PedState);
    auto* vp = new (Z(sizeof(CVehiclePool))) CVehiclePool(kVehs, vehStore, VehState);
    for (int i = 0; i < kPeds; ++i)  PedObj[i] = (CCopPed*)(pedStore + i * sizeof(CCopPed));
    for (int i = 0; i < kVehs; ++i)  VehObj[i] = (CVehicle*)(vehStore + i * sizeof(CHeli));
    StaticRef<CPedPool*>(0xB74490) = pp;
    StaticRef<CVehiclePool*>(0xB74494) = vp;
    CWorld::PlayerInFocus = 0;
    CWorld::Players[0].m_pPed = Pl;
    CWorld::Players[0].m_PlayerData.m_pWanted = W;
    Pl->*Get(T_PedPD{}) = &CWorld::Players[0].m_PlayerData;
    Pl->*Get(T_PedInt{}) = (CPedIntelligence*)Z(sizeof(CPedIntelligence));
}

static eWantedLevel GenLevel(Rng& r) { return (eWantedLevel)r.below(7); }
static uint32 GenChaos(Rng& r) {
    static const uint32 th[] = { 0, 49, 50, 179, 180, 549, 550, 1199, 1200, 2399, 2400, 4599, 4600, 9199, 9200, 9201, 115, 365, 875, 1800, 3500, 6900 };
    switch (r.below(6)) {
    case 0: return th[r.below(22)];
    case 1: return r.u32() % 10000;
    case 2: return r.u32() % 100;
    case 3: return r.below(2) ? 0xFFFFFFFFu - r.below(4) : r.u32();
    case 4: return (r.u32() % 10000) | 0x80000000u;
    default: return th[r.below(22)] + (int)r.below(3) - 1;
    }
}
static float GenMult(Rng& r) {
    switch (r.below(8)) {
    case 0: return 1.f;
    case 1: return r.f01() * 3.f;
    case 2: return GenF(r, 10.f);
    case 3: return GenF(r, 1e10f);
    case 4: return 0.f;
    case 5: return GenF(r, PickScale(r));
    default: return 1.f + r.f01();
    }
}
static void GenCrime(CCrimeBeingQd& c, Rng& r, uint32 now) {
    c.m_nCrimeType = (eCrimeType)(r.below(4) ? r.below(MAX_CRIMES) : r.below(0x20));
    if (r.below(3) == 0) c.m_nCrimeType = CRIME_NONE;
    c.m_nCrimeId = r.below(3) ? r.below(6) : r.u32();
    c.m_nStartTime = r.below(3) ? now - r.below(12000) : r.u32();
    c.m_vecCoors = GenV(r, 3000.f);
    c.m_bAlreadyReported = r.below(2); c.m_bPoliceDontReallyCare = r.below(2);
}
// random world around the wanted state: time, cheats, gang war / darkel statics, game area, weather region, player position / vehicle, pools content, cops in pursuit
static void GenWorld(Rng& r) {
    CTimer::m_snTimeInMilliseconds = r.below(5) ? r.u32() % 3000000 : r.u32();
    CTimer::m_FrameCounter = r.below(4) ? r.u32() % 100000 : r.u32();
    std::memset(CCheat::m_aCheatsActive, 0, sizeof CCheat::m_aCheatsActive);
    CCheat::m_aCheatsActive[CHEAT_I_DO_AS_I_PLEASE] = r.below(12) == 0;
    CDarkel::Status = r.below(6) == 0 ? eDarkelStatus::FRENZY_ON_GOING : (eDarkelStatus)r.below(6);
    CGangWars::State = (eGangWarState)(r.below(3) == 0 ? r.below(7) : 0);
    CGangWars::State2 = (eGangAttackState)(r.below(3) == 0 ? r.below(4) : 0);
    CGame::currArea = (eAreaCodes)(r.below(3) ? 0 : r.below(5));
    CWeather::WeatherRegion = (eWeatherRegion)r.below(5);
    // player: ped state / position / vehicle
    Pl->m_matrix = nullptr;
    Pl->m_placement.m_vPosn = GenV(r, 3000.f);
    Pl->bInVehicle = false; Pl->m_pVehicle = nullptr;
    Pl->m_nPedState = (ePedState)(r.below(4) ? 1 : r.below(60));
    Pl->m_fHealth = GenF(r, 200.f); Pl->m_nPedType = PED_TYPE_PLAYER1;
}
static void GenPools(Rng& r, const CVector& nearp) {
    for (int i = 0; i < kPeds; ++i) {
        std::memset(PedObj[i], 0, sizeof(CCopPed));
        PedState[i] = r.below(4) ? (uint8)(r.below(0x7F)) : 0x80;   // occupied (ref byte) or free
        PedObj[i]->m_nPedType = (ePedType)(r.below(2) ? PED_TYPE_COP : r.below(32));
        PedObj[i]->m_nPedState = (ePedState)(r.below(4) ? 1 : r.below(60));
        PedObj[i]->m_placement.m_vPosn = r.below(2) ? nearp + GenV(r, 30.f, S_NOHUGE) : GenV(r, 3000.f);
    }
    for (int i = 0; i < kVehs; ++i) {
        std::memset(VehObj[i], 0, sizeof(CHeli));
        VehState[i] = r.below(3) ? (uint8)(r.below(0x7F)) : 0x80;
        static const int mi[] = { MODEL_POLMAV, MODEL_COPCARLA, MODEL_ENFORCER, MODEL_FBIRANCH, MODEL_INFERNUS, MODEL_RHINO, 0, 596, 597, 598, 599 };
        VehObj[i]->m_nModelIndex = (int16)(r.below(4) ? mi[r.below(11)] : r.below(700));
        VehObj[i]->vehicleFlags.bIsLawEnforcer = r.below(2);
        VehObj[i]->SetStatus((eEntityStatus)(r.below(4) ? r.below(8) : r.below(3)));
        VehObj[i]->m_placement.m_vPosn = r.below(2) ? nearp + GenV(r, 30.f, S_NOHUGE) : GenV(r, 3000.f);
        VehObj[i]->m_nVehicleSubType = (eVehicleType)r.below(8);
    }
    if (r.below(3) == 0) { Pl->bInVehicle = true; Pl->m_pVehicle = VehObj[r.below(kVehs)]; }
    else if (r.below(3) == 0) { Pl->bInVehicle = false; Pl->m_pVehicle = VehObj[r.below(kVehs)]; }   // stale m_pVehicle
}
static void GenWanted(CWanted& w, Rng& r) {
    std::memset(&w, 0, sizeof w);
    const uint32 now = CTimer::m_snTimeInMilliseconds;
    w.m_ChaosLevel = GenChaos(r);
    w.m_ChaosLevelBeforeParole = r.below(3) ? 0 : GenChaos(r);
    w.m_LastTimeWantedDecreased = r.below(3) ? now - r.below(3000) : r.u32();
    w.m_LastTimeWantedLevelChanged = r.u32();
    w.m_TimeOfParole = r.below(3) ? now - r.below(30000) : r.u32();
    w.m_Multiplier = GenMult(r);
    w.m_NumCopsInPursuit = r.below(3) ? r.below(11) : (uint8)r.u32();
    w.m_MaxCopsInPursuit = r.below(3) ? r.below(11) : (uint8)r.u32();
    w.m_MaxCopCarsInPursuit = r.below(4); w.m_CopsBeatingSuspect = r.below(4);
    w.m_ChanceOnRoadBlock = (uint16)(r.below(3) ? r.below(40) : r.u32());
    w.m_Flags = r.below(3) ? 0 : (uint8)r.u32();
    w.m_CurrentChaseTime = r.below(3) ? r.below(300) : r.u32();
    w.m_CurrentChaseTimeCounter = r.below(3) ? now - r.below(3000) : r.u32();
    w.m_TimeCounting = r.below(2);
    w.m_WantedLevel = GenLevel(r); w.m_WantedLevelBeforeParole = GenLevel(r);
    for (auto& c : w.m_CrimesBeingQd) GenCrime(c, r, now);
    for (auto& c : w.m_CopsInPursuit) c = nullptr;
    if (r.below(2)) { const int n = r.below(CWanted::MAX_COPS_IN_PURSUIT + 1); for (int i = 0; i < n; ++i) w.m_CopsInPursuit[i] = PedObj[r.below(kPeds)]; }
    else for (auto& c : w.m_CopsInPursuit) c = r.below(2) ? PedObj[r.below(kPeds)] : nullptr;
    // StoredPoliceBackOff (private): raw byte at the end of the object
    reinterpret_cast<uint8*>(&w)[sizeof(CWanted) - 4] = (uint8)r.below(2);
}
// statics a CWanted function may change
struct Statics {
    Snap stats{ 0xB79000, 223 * 4 + 82 * 4 + 0x80 }, wl{ 0x8CDEE4, 8 }, news{ 0xB7CB8C, 4 };
    Snap statmsg{ 0xB794CC, 0x40 }, players{ 0xB7CD98, 0x190 };
    void Save()            { stats.Save(); wl.Save(); news.Save(); statmsg.Save(); players.Save(); }
    void Restore() const   { stats.Restore(); wl.Restore(); news.Restore(); statmsg.Restore(); players.Restore(); }
    bool Same(const Statics& o) const { return stats.SameAs(o.stats) && wl.SameAs(o.wl) && news.SameAs(o.news) && statmsg.SameAs(o.statmsg) && players.SameAs(o.players); }
};
static void RandomStats(Rng& r) {
    for (auto& v : CStats::StatTypesFloat) v = r.below(4) ? r.f01() * 100.f : GenF(r, 1e5f);
    for (auto& v : CStats::StatTypesInt) v = r.below(4) ? (int32)(r.u32() % 100) : (int32)r.u32();
    CStats::TotalNumStatMessages = 0; CStats::bShowUpdateStats = false;
    for (auto& v : CStats::StatReactionValue) v = 0.f;
}

// the generic runner: gen(world+wanted) -> port fn -> snapshot -> restore -> exe fn -> snapshot -> compare (wanted struct, statics, recorded events, return value)
template<class RetT, class PortFn, class ExeFn>
static bool Case(Rng& r, std::string& d, PortFn&& port, ExeFn&& exe, bool needsPools = false, const char* what = "") {
    Init();
    GenWorld(r);
    RandomStats(r);
    GenPools(r, Pl->m_placement.m_vPosn);
    GenWanted(*W, r);
    if (g_prep) g_prep(*W);
    if (g_prepWorld) g_prepWorld(r);
    const int seed = r.u32();
    static std::vector<uint8> w0(sizeof(CWanted)), w1(sizeof(CWanted)), w2(sizeof(CWanted));
    static Statics s0, s1, s2;
    std::memcpy(w0.data(), W, sizeof(CWanted)); s0.Save();
    static std::vector<uint8> p0(sizeof(CCopPed) * kPeds), p1(sizeof(CCopPed) * kPeds);
    SeedBoth(seed);
    g_audioEv.clear();
    RetT a = port(*W);
    const auto ev1 = g_audioEv;
    std::memcpy(w1.data(), W, sizeof(CWanted)); s1.Save();
    std::memcpy(W, w0.data(), sizeof(CWanted)); s0.Restore();
    SeedBoth(seed);
    g_audioEv.clear();
    RetT e = exe(*W);
    const auto ev2 = g_audioEv;
    std::memcpy(w2.data(), W, sizeof(CWanted)); s2.Save();
    bool ok = SameBlob(w1.data(), w2.data(), sizeof(CWanted)) && s1.Same(s2) && ev1 == ev2;
    if constexpr (!std::is_void_v<RetT>) { ok = ok && (std::memcmp(&a, &e, sizeof(RetT)) == 0); }
    if (!ok) {
        d = std::string(what) + " W0 " + Hex(w0.data(), 0x40) + DiffDesc(w1.data(), w2.data(), sizeof(CWanted), "w");
        if constexpr (!std::is_void_v<RetT>) d += " ret got " + std::to_string((long long)(a)) + " exe " + std::to_string((long long)(e));
        if (!s1.Same(s2)) { d += " statics differ:" + DiffDesc(s1.stats.b.data(), s2.stats.b.data(), s1.stats.b.size(), "stats") + DiffDesc(s1.wl.b.data(), s2.wl.b.data(), 8, "wl") + DiffDesc(s1.news.b.data(), s2.news.b.data(), 4, "news"); }
        if (ev1 != ev2) d += " audio events " + std::to_string(ev1.size()) + " vs exe " + std::to_string(ev2.size());
    }
    return ok;
}
} // namespace lw


void CAEPoliceScannerAudioEntity::AddAudioEvent(eAudioEvents event, eCrimeType crimeType, const CVector& point) {
    g_audioEv.push_back({ (uint32)event, (uint32)crimeType, FBits(point.x), FBits(point.y), FBits(point.z) });
}
static void __fastcall HostAddAudioEvent(void*, int, int event, int crimeType, const CVector* point) {
    g_audioEv.push_back({ (uint32)event, (uint32)crimeType, FBits(point->x), FBits(point->y), FBits(point->z) });
}

static const GxtChar kGoodBoy[] = { 'G','O','O','D','B','O','Y',0 };
const GxtChar* CText::Get(const char*) { return kGoodBoy; }
static const GxtChar* __fastcall HostTextGet(void*, int, const char*) { return kGoodBoy; }
void CMessages::AddBigMessage(const GxtChar* text, uint32 time, eMessageStyle style) { g_audioEv.push_back({ 0xB16, (uint32)(uintptr_t)text, time, (uint32)style, 0 }); }
static void __cdecl HostBigMessage(const GxtChar* text, uint32 time, int style) { g_audioEv.push_back({ 0xB16, (uint32)(uintptr_t)text, time, (uint32)style, 0 }); }
static bool g_german = false;
bool CLocalisation::GermanGame() { return g_german; }
static bool __cdecl HostGerman() { return g_german; }
static int __cdecl HostPrintf(const char*, ...) { return 0; }   // the exe's leftover debug printf (CWanted::Update "CopPursuit ... messed up"): its CRT stdout state does not exist here

static void TestWanted() {
    using namespace lw;
    oracle::Patch(0x4E71E0, (void*)&HostAddAudioEvent);
    oracle::Patch(0x821982, (void*)&HostPrintf);
    oracle::Patch(0x6A0050, (void*)&HostTextGet);
    oracle::Patch(0x69F2B0, (void*)&HostBigMessage);
    oracle::Patch(0x56D200, (void*)&HostGerman);
    using VoidThis = void __fastcall(CWanted*, int);
    auto sub = [](Rng& r) { return Rng(r.u32() * 2654435761ull + 77); };
    const auto Cop = [](Rng& a) -> CCopPed* { return a.below(5) == 0 ? nullptr : PedObj[a.below(kPeds)]; };

    Run("CWanted::UpdateWantedLevel 0x561C90", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [](CWanted& w) { w.UpdateWantedLevel(); return 0; }, [](CWanted& w) { oracle::Fn<VoidThis>(0x561C90)(&w, 0); return 0; });
    });
    Run("CWanted::SetMaximumWantedLevel 0x561E70", [&](Rng& r, std::string& d) {
        const eWantedLevel lv = (eWantedLevel)r.below(7);
        return Case<int>(r, d, [&](CWanted&) { CWanted::SetMaximumWantedLevel(lv); return 0; }, [&](CWanted&) { oracle::Fn<void __cdecl(int)>(0x561E70)((int)lv); return 0; }, false, "lv");
    }, 600);
    Run("CWanted::InitialiseStaticVariables 0x561C70", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [](CWanted&) { CWanted::InitialiseStaticVariables(); return 0; }, [](CWanted&) { oracle::Fn<void __cdecl()>(0x561C70)(); return 0; });
    }, 200);
    {
        struct G { const char* n; unsigned va; bool (CWanted::*pf)() const; };
        static const G gs[] = { { "AreMiamiViceRequired", 0x561F30, &CWanted::AreMiamiViceRequired }, { "AreSwatRequired", 0x561F40, &CWanted::AreSwatRequired },
                                { "AreFbiRequired", 0x561F60, &CWanted::AreFbiRequired }, { "AreArmyRequired", 0x561F80, &CWanted::AreArmyRequired } };
        for (auto& g : gs)
            Run(std::string("CWanted::") + g.n + " 0x" + HexVA(g.va), [&](Rng& r, std::string& d) {
                return Case<int>(r, d, [&](CWanted& w) { const bool b = (w.*g.pf)(); Hit(b); return (int)b; }, [&](CWanted& w) { return (int)(oracle::Fn<bool __fastcall(CWanted*, int)>(g.va)(&w, 0)); });
            }, 1500);
    }
    Run("CWanted::NumOfHelisRequired 0x561FA0", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [](CWanted& w) { return w.NumOfHelisRequired(); }, [](CWanted& w) { return oracle::Fn<int __fastcall(CWanted*, int)>(0x561FA0)(&w, 0); });
    }, 1500);
    Run("CWanted::ClearQdCrimes 0x561FE0", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [](CWanted& w) { w.ClearQdCrimes(); return 0; }, [](CWanted& w) { oracle::Fn<VoidThis>(0x561FE0)(&w, 0); return 0; });
    }, 500);
    Run("CWanted::ResetPolicePursuit 0x561FD0", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [](CWanted& w) { w.ResetPolicePursuit(); return 0; }, [](CWanted& w) { oracle::Fn<VoidThis>(0x561FD0)(&w, 0); return 0; });
    }, 200);
    Run("CWanted::AddCrimeToQ 0x562000", [&](Rng& r, std::string& d) {
        Rng a = sub(r);
        const eCrimeType t = (eCrimeType)(1 + a.below(a.below(5) ? 22 : 40)); const uint32 id = a.below(3) ? a.below(6) : a.u32();
        const CVector pos = GenV(a, 3000.f); const bool rep = a.below(2), care = a.below(2);
        bool ret = false;
        const bool ok = Case<int>(r, d, [&](CWanted& w) { const bool b = w.AddCrimeToQ(t, id, pos, rep, care); Hit(b); return (int)b; },
            [&](CWanted& w) { return (int)(oracle::Fn<bool __fastcall(CWanted*, int, int, uint32, const CVector*, bool, bool)>(0x562000)(&w, 0, (int)t, id, &pos, rep, care)); }, false, "AddCrimeToQ");
        (void)ret; return ok;
    });
    Run("CWanted::ReportCrimeNow 0x562120", [&](Rng& r, std::string& d) {
        Rng a = sub(r);
        const eCrimeType t = (eCrimeType)(a.below(5) ? a.below(MAX_CRIMES) : a.below(40)); const CVector pos = GenV(a, 3000.f); const bool care = a.below(2);
        return Case<int>(r, d, [&](CWanted& w) { w.ReportCrimeNow(t, pos, care); return 0; },
            [&](CWanted& w) { oracle::Fn<void __fastcall(CWanted*, int, int, const CVector*, bool)>(0x562120)(&w, 0, (int)t, &pos, care); return 0; }, false, "ReportCrimeNow");
    });
    Run("CWanted::RegisterCrime 0x562410", [&](Rng& r, std::string& d) {
        Rng a = sub(r);
        const eCrimeType t = (eCrimeType)(1 + a.below(22)); const uint32 id = a.below(3) ? a.below(6) : a.u32(); const CVector pos = GenV(a, 3000.f); const bool care = a.below(2);
        return Case<int>(r, d, [&](CWanted& w) { w.RegisterCrime(t, pos, id, care); return 0; },
            [&](CWanted& w) { oracle::Fn<void __fastcall(CWanted*, int, int, const CVector*, uint32, bool)>(0x562410)(&w, 0, (int)t, &pos, id, care); return 0; });
    });
    Run("CWanted::RegisterCrime_Immediately 0x562430", [&](Rng& r, std::string& d) {
        Rng a = sub(r);
        const eCrimeType t = (eCrimeType)(1 + a.below(22)); const uint32 id = a.below(3) ? a.below(6) : a.u32(); const CVector pos = GenV(a, 3000.f); const bool care = a.below(2);
        return Case<int>(r, d, [&](CWanted& w) { w.RegisterCrime_Immediately(t, pos, id, care); return 0; },
            [&](CWanted& w) { oracle::Fn<void __fastcall(CWanted*, int, int, const CVector*, uint32, bool)>(0x562430)(&w, 0, (int)t, &pos, id, care); return 0; });
    });
    Run("CWanted::UpdateCrimesQ 0x562760", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [](CWanted& w) { w.UpdateCrimesQ(); return 0; }, [](CWanted& w) { oracle::Fn<VoidThis>(0x562760)(&w, 0); return 0; });
    });
    Run("CWanted::IsInPursuit 0x562330", [&](Rng& r, std::string& d) {
        Rng a = sub(r); CCopPed* c = Cop(a);
        return Case<int>(r, d, [&](CWanted& w) { const bool b = w.IsInPursuit(c); Hit(b); return (int)b; }, [&](CWanted& w) { return (int)oracle::Fn<bool __fastcall(CWanted*, int, CCopPed*)>(0x562330)(&w, 0, c); });
    }, 1500);
    Run("CWanted::UpdateEachFrame 0x562360", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [](CWanted&) { CWanted::UpdateEachFrame(); return 0; }, [](CWanted&) { oracle::Fn<void __cdecl()>(0x562360)(); return 0; });
    }, 1500);
    Run("CWanted::SetWantedLevel 0x562470", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const eWantedLevel lv = (eWantedLevel)a.below(7);
        return Case<int>(r, d, [&](CWanted& w) { w.SetWantedLevel(lv); return 0; }, [&](CWanted& w) { oracle::Fn<void __fastcall(CWanted*, int, int)>(0x562470)(&w, 0, (int)lv); return 0; });
    });
    Run("CWanted::CheatWantedLevel 0x562540", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const eWantedLevel lv = (eWantedLevel)a.below(7);
        return Case<int>(r, d, [&](CWanted& w) { w.CheatWantedLevel(lv); return 0; }, [&](CWanted& w) { oracle::Fn<void __fastcall(CWanted*, int, int)>(0x562540)(&w, 0, (int)lv); return 0; });
    });
    Run("CWanted::SetWantedLevelNoDrop 0x562570", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const eWantedLevel lv = (eWantedLevel)a.below(7);
        return Case<int>(r, d, [&](CWanted& w) { w.SetWantedLevelNoDrop(lv); return 0; }, [&](CWanted& w) { oracle::Fn<void __fastcall(CWanted*, int, int)>(0x562570)(&w, 0, (int)lv); return 0; });
    });
    Run("CWanted::ClearWantedLevelAndGoOnParole 0x5625A0", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [](CWanted& w) { w.ClearWantedLevelAndGoOnParole(); return 0; }, [](CWanted& w) { oracle::Fn<VoidThis>(0x5625A0)(&w, 0); return 0; });
    });
    Run("CWanted::Initialise 0x562390", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [](CWanted& w) { w.Initialise(); return 0; }, [](CWanted& w) { oracle::Fn<VoidThis>(0x562390)(&w, 0); return 0; });
    }, 500);
    Run("CWanted::Reset 0x562400", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [](CWanted& w) { w.Reset(); return 0; }, [](CWanted& w) { oracle::Fn<VoidThis>(0x562400)(&w, 0); return 0; });
    }, 500);
    Run("CWanted::WorkOutPolicePresence 0x5625F0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const CVector pos = r.below(2) ? Pl->m_placement.m_vPosn : GenV(a, 3000.f); const float rad = a.below(3) ? a.f01() * 60.f : GenF(a, 1000.f);
        return Case<int>(r, d, [&](CWanted&) { return CWanted::WorkOutPolicePresence(pos, rad); },
            [&](CWanted&) { return oracle::Fn<int __cdecl(float, float, float, float)>(0x5625F0)(pos.x, pos.y, pos.z, rad); }, true, "WorkOutPolicePresence");
    });
    Run("CWanted::IsClosestCop 0x5627D0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); CCopPed* c = PedObj[a.below(kPeds)]; const int n = a.below(3) ? 1 + a.below(10) : (int)a.below(11);   // > 10 overflows the exe's stack buffer (undefined in the original)
        return Case<int>(r, d, [&](CWanted& w) { const bool b = w.IsClosestCop(c, n); Hit(b); return (int)b; },
            [&](CWanted& w) { return (int)oracle::Fn<bool __fastcall(CWanted*, int, CPed*, int)>(0x5627D0)(&w, 0, c, n); });
    });
    Run("CWanted::ComputePursuitCopToDisplace 0x562B00", [&](Rng& r, std::string& d) {
        Rng a = sub(r); CCopPed* c = Cop(a);
        return Case<int>(r, d, [&](CWanted& w) { return (int)(intptr_t)Get(T_Displace{})(c, w.m_CopsInPursuit); },
            [&](CWanted& w) { return (int)(intptr_t)oracle::Fn<CCopPed* __cdecl(CCopPed*, CCopPed**)>(0x562B00)(c, w.m_CopsInPursuit); });
    });
    Run("CWanted::RemovePursuitCop(func) 0x562300", [&](Rng& r, std::string& d) {
        Rng a = sub(r); CCopPed* c = Cop(a);
        return Case<int>(r, d, [&](CWanted& w) { Get(T_RemoveStatic{})(c, w.m_CopsInPursuit, w.m_NumCopsInPursuit); return 0; },
            [&](CWanted& w) { oracle::Fn<void __cdecl(CCopPed*, CCopPed**, uint8*)>(0x562300)(c, w.m_CopsInPursuit, &w.m_NumCopsInPursuit); return 0; });
    });
    Run("CWanted::RemovePursuitCop(method) 0x562C10", [&](Rng& r, std::string& d) {
        Rng a = sub(r); CCopPed* c = Cop(a);
        return Case<int>(r, d, [&](CWanted& w) { w.RemovePursuitCop(c); return 0; }, [&](CWanted& w) { oracle::Fn<void __fastcall(CWanted*, int, CCopPed*)>(0x562C10)(&w, 0, c); return 0; });
    });
    g_prep = [](CWanted& w) { if (w.m_NumCopsInPursuit > w.m_MaxCopsInPursuit) w.m_CopsInPursuit[9] = nullptr; };   // with all 10 slots used and none displaceable the exe's loop is endless too
    Run("CWanted::RemoveExcessPursuitCops 0x562C40", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [](CWanted& w) { w.RemoveExcessPursuitCops(); return 0; }, [](CWanted& w) { oracle::Fn<VoidThis>(0x562C40)(&w, 0); return 0; });
    }, 1500);
    g_prep = nullptr;
    Run("CWanted::CanCopJoinPursuit(func) 0x562F60", [&](Rng& r, std::string& d) {
        Rng a = sub(r); CCopPed* c = Cop(a); const uint8 mx = a.below(3) ? a.below(11) : (uint8)a.u32();
        return Case<int>(r, d, [&](CWanted& w) { const bool b = Get(T_JoinStatic{})(c, mx, w.m_CopsInPursuit, w.m_NumCopsInPursuit); Hit(b); return (int)b; },
            [&](CWanted& w) { return (int)oracle::Fn<bool __cdecl(CCopPed*, uint8, CCopPed**, uint8*)>(0x562F60)(c, mx, w.m_CopsInPursuit, &w.m_NumCopsInPursuit); });
    }, 1500);
    Run("CWanted::CanCopJoinPursuit(method) 0x562FB0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); CCopPed* c = Cop(a);
        return Case<int>(r, d, [&](CWanted& w) { const bool b = w.CanCopJoinPursuit(c); Hit(b); return (int)b; }, [&](CWanted& w) { return (int)oracle::Fn<bool __fastcall(CWanted*, int, CCopPed*)>(0x562FB0)(&w, 0, c); });
    }, 1500);
    Run("CWanted::SetPursuitCop 0x563060", [&](Rng& r, std::string& d) {
        Rng a = sub(r); CCopPed* c = Cop(a);
        return Case<int>(r, d, [&](CWanted& w) { const bool b = w.SetPursuitCop(c); Hit(b); return (int)b; }, [&](CWanted& w) { return (int)oracle::Fn<bool __fastcall(CWanted*, int, CCopPed*)>(0x563060)(&w, 0, c); });
    }, 1500);
    Run("CWanted::Update 0x562C90", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [](CWanted& w) { w.Update(); return 0; }, [](CWanted& w) { oracle::Fn<VoidThis>(0x562C90)(&w, 0); return 0; }, true, "Update");
    }, 6000);

    // ---- CGameLogic: the forbidden-territory police level
    static const CVector kTown1[] = { {3000.f,535.f,0.f},{1759.f,576.f,0.f},{989.f,693.f,0.f},{-128.f,490.f,0.f},{-845.f,707.f,0.f},{-1477.f,1677.f,0.f},{-2154.f,2497.f,0.f},{-2971.f,2180.f,0.f},{-3000.f,3000.f,0.f},{3000.f,3000.f,0.f} };
    static const CVector kTown2[] = { {28.f,-3000.f,0.f},{-30.f,-1280.f,0.f},{-148.f,-911.f,0.f},{-487.f,-372.f,0.f},{-1028.f,-424.f,0.f},{-1145.f,479.f,0.f},{-1461.f,1488.f,0.f},{-3000.f,1668.f,0.f},{-3000.f,-3000.f,0.f} };
    Run("CGameLogic::IsPointWithinLineArea 0x4416E0", [&](Rng& r, std::string& d) {
        Init();
        CVector poly[12]; uint32 n = 0; const CVector* pts = poly;
        switch (r.below(4)) {
        case 0: pts = kTown1; n = 10; break;
        case 1: pts = kTown2; n = 9; break;
        default: n = r.below(13); for (uint32 i = 0; i < n; ++i) poly[i] = GenV(r, 3000.f, S_NOHUGE); break;
        }
        float x = GenF(r, 3500.f), y = GenF(r, 3500.f);
        if (n && r.below(3) == 0) { const auto& v = pts[r.below(n)]; x = v.x + (float)((int)r.below(5) - 2); y = v.y + (float)((int)r.below(5) - 2); }   // on / next to a vertex
        const bool a = CGameLogic::IsPointWithinLineArea(pts, n, x, y);
        const bool e = oracle::Fn<bool __cdecl(const CVector*, uint32, float, float, float)>(0x4416E0)(pts, n, x, y, 0.f) & 1;
        Hit(a);
        if (a == e) return true;
        d = "n " + std::to_string(n) + " p {" + F(x) + "," + F(y) + "} got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    }, 4000);
    g_prepWorld = [](Rng& r) {
        const CVector p = r.below(3) ? GenV(r, 3300.f, S_NOHUGE) : (r.below(2) ? CVector{ kTown1[r.below(10)].x + (float)((int)r.below(7) - 3), kTown1[r.below(10)].y + (float)((int)r.below(7) - 3), GenF(r, 100.f) } : GenV(r, 3300.f));
        Pl->m_placement.m_vPosn = r.below(8) ? p : GenV(r, 3300.f);
        if (r.below(10) == 0) Pl->m_placement.m_vPosn.z = r.below(2) ? 951.f : GenF(r, 2000.f);
        CStats::StatTypesInt[181 - CStats::FIRST_INT_STAT] = r.below(5) ? (int32)r.below(4) : (int32)r.u32();
        CTimer::m_FrameCounter = (r.u32() & ~0x1Fu) | (r.below(2) ? 0x12u : (r.u32() & 0x1F));
    };
    Run("CGameLogic::SetPlayerWantedLevelForForbiddenTerritories 0x441770", [&](Rng& r, std::string& d) {
        const bool imm = r.below(2);
        return Case<int>(r, d, [&](CWanted&) { CGameLogic::SetPlayerWantedLevelForForbiddenTerritories(imm); return 0; },
                         [&](CWanted&) { oracle::Fn<void __cdecl(bool)>(0x441770)(imm); return 0; }, false, "Forbidden");
    }, 6000);
    // ---- CCrime
    Run("CCrime::FindImmediateDetectionRange 0x531FC0", [&](Rng& r, std::string& d) {
        const eCrimeType t = (eCrimeType)(r.below(3) ? r.below(MAX_CRIMES) : r.u32() % 64);
        const float a = CCrime::FindImmediateDetectionRange(t); const float e = oracle::Fn<float __cdecl(int)>(0x531FC0)((int)t);
        if (SameF(a, e)) return true; d = "type " + std::to_string((int)t) + " got " + F(a) + " exe " + F(e); return false;
    }, 600);
    struct CrimeCfg { int victimKind = 0, victimIdx = 0, byKind = 0, byIdx = 0; bool german = false; };
    static CrimeCfg cc;
    g_prepWorld = [](Rng& r) {
        auto setType = [](void* e, int t) { auto& b = *((uint8*)e + 0x36); b = (uint8)((b & ~7) | t); };
        for (int i = 0; i < kPeds; ++i) {
            setType(PedObj[i], 3);
            reinterpret_cast<uint32&>(*((uint8*)PedObj[i] + 0x46C)) = r.below(2) ? 0x800000u : (uint32)r.u32();   // bBeingChasedByPolice is bit 23
            PedObj[i]->m_nPedType = (ePedType)(r.below(3) ? 2 + r.below(30) : (int)PED_TYPE_GANG1 + (int)r.below(12));   // pool peds are never the player (no player data)
            PedObj[i]->m_pCollisionList = {}; if (r.below(2)) *reinterpret_cast<uint32*>(&PedObj[i]->m_pCollisionList) = 0x1000;
        }
        for (int i = 0; i < kVehs; ++i) setType(VehObj[i], 2);
        Pl->m_nPedType = (ePedType)(r.below(4) ? PED_TYPE_PLAYER1 : (int)r.below(6));
        setType(Pl, 3);
        W->m_Multiplier = r.below(8) ? W->m_Multiplier : (r.below(2) ? -r.f01() : GenF(r, 2.f));
        g_german = cc.german;
        CWorld::Players[0].m_nMoney = (int32)r.u32();
    };
    Run("CCrime::ReportCrime 0x532010", [&](Rng& r, std::string& d) {
        Rng a = sub(r);
        cc.victimKind = a.below(5); cc.victimIdx = a.below(kPeds); cc.byKind = a.below(6); cc.byIdx = a.below(kPeds); cc.german = a.below(4) == 0;
        const eCrimeType t = (eCrimeType)(a.below(3) == 0 ? 2 : (a.below(8) ? a.below(MAX_CRIMES) : a.u32() % 40));
        if (t == CRIME_DAMAGED_PED && cc.victimKind == 0) cc.victimKind = 1;   // the exe dereferences the victim unconditionally for DAMAGED_PED
        const int vk = cc.victimKind, vi = cc.victimIdx, bk = cc.byKind, bi = cc.byIdx;
        const auto pickVictim = [&]() -> CEntity* { switch (vk) { case 0: return nullptr; case 1: case 2: case 3: return PedObj[vi]; default: return VehObj[vi % kVehs]; } };
        const auto pickBy = [&]() -> CPed* { switch (bk) { case 0: return nullptr; case 1: case 2: case 3: return Pl; default: return PedObj[bi]; } };
        return Case<int>(r, d, [&](CWanted&) { CCrime::ReportCrime(t, pickVictim(), pickBy()); return 0; },
                         [&](CWanted&) { oracle::Fn<void __cdecl(int, CEntity*, CPed*)>(0x532010)((int)t, pickVictim(), pickBy()); return 0; }, false, "ReportCrime");
    }, 8000);
    g_prepWorld = nullptr;
}
