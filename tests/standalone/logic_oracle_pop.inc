// (c) population / car-model choice / pop cycle: CLoadedCarGroup, CPopulation (occupation choice, ped-stat / model-info predicates, multipliers), CPopCycle (zone percentages, group / ped picking).
// Fixture: fake model infos (zeroed, reinterpret-cast like the port does) wired into CModelInfo::ms_modelInfoPtrs, streaming load states, a fake CZoneInfo as the current pop-cycle zone;
// port and exe run on the same exe-image statics: snapshot -> port -> snapshot -> restore -> exe -> snapshot -> compare (return value + all touched statics).
#include "Population.h"
#include "Scripts/TheScripts.h"
#include "CarCtrl.h"
#include "Gangs.h"
#include "GangInfo.h"
#include "Wanted.h"
#include "TheZones.h"
#include "GangWars.h"
#include "GameLogic.h"
#include "Darkel.h"
#include "Timer.h"
#include "TimeCycle.h"
#include "Pipelines/CustomBuilding/CustomBuildingDNPipeline.h"
#include "World.h"
#include "Entity/Ped/PlayerPed.h"
#include "PopCycle.h"
#include "LoadedCarGroup.h"
#include "Clock.h"
#include "Weather.h"
#include "Cheat.h"
#include "Game.h"
#include "Streaming.h"
#include "ZoneInfo.h"
#include "CullZones.h"
#include "ModelIndices.h"
#include "Models/ModelInfo.h"
#include "Models/PedModelInfo.h"
#include "Models/VehicleModelInfo.h"
#include "Plugins/TwoDEffectPlugin/2dEffect.h"

namespace lp {
constexpr int    kNMI = 700, kPedMI = 300, kVehLo = 400, kVehHi = 612;
constexpr size_t kMISz = (sizeof(CVehicleModelInfo) + 63) & ~size_t(63);
static uint8*     MI;
static CZoneInfo* Zone;
static bool       inited = false;
static bool       g_zoneNullOk = false;   // tests of functions that tolerate (or are fed) a null current zone set this
static float GenPopF(Rng& r) { return r.below(8) == 0 ? 0.f : (r.below(4) ? r.f01() * 30.f : (r.f01() * 2.f - 1.f) * 1000.f); }   // no NaN / inf: the exe's chance logic has no defined answer for them
static CPedModelInfo*     PM(int i) { return reinterpret_cast<CPedModelInfo*>(MI + (size_t)i * kMISz); }
static CVehicleModelInfo* VM(int i) { return reinterpret_cast<CVehicleModelInfo*>(MI + (size_t)i * kMISz); }

static void Init() {
    if (inited) return;
    inited = true;
    MI   = (uint8*)std::calloc(kNMI, kMISz);
    Zone = (CZoneInfo*)std::calloc(1, 64);
    for (int i = 0; i < kNMI; ++i) CModelInfo::ms_modelInfoPtrs[i] = reinterpret_cast<CBaseModelInfo*>(MI + (size_t)i * kMISz);
    CPopCycle::m_pCurrZoneInfo = Zone;
    // ms_modelInfoPtrs[-1] (0xA9B0C4): PickRiotRoadBlockCar's second phase walks all 23 slots of a group and reads the model info of the unused slots (-1) = this word,
    // which is a null pointer in the exe (null deref => the original crashes there). A BIG-class info makes those slots 'skip', which is what the port does by stopping at the last member.
    auto* dummy = reinterpret_cast<CVehicleModelInfo*>(std::calloc(1, kMISz));
    dummy->m_nVehicleClass = VEHICLE_CLASS_BIG;
    *reinterpret_cast<CBaseModelInfo**>(0xA9B0C4) = reinterpret_cast<CBaseModelInfo*>(dummy);
}

static int GenPedId(Rng& r) { return r.below(12) == 0 ? -1 : (int)r.below(kPedMI); }
static int GenVehId(Rng& r) { return kVehLo + (int)r.below(kVehHi - kVehLo); }

static void GenGroup(CLoadedCarGroup& g, Rng& r, int maxN = 23) {
    g.Clear();
    const int n = r.below(5) == 0 ? 0 : (int)r.below(maxN + 1);
    for (int i = 0; i < n; ++i) g.AddMember((eModelID)GenVehId(r));
}
// random world around the population functions
static void GenWorld(Rng& r) {
    Init();
    lw::Init();
    for (int i = 0; i < kPedMI; ++i) {
        auto* p = PM(i);
        p->m_nRefCount = (uint16)(r.below(3) ? r.below(8) : r.below(0x10000));
        p->m_nAnimType = (AssocGroupId)(r.below(4) ? r.below(6) : r.below(40));
        p->m_nPedType = (ePedType)(r.below(3) == 0 ? r.below(32) : (r.below(2) ? PED_TYPE_CIVMALE : PED_TYPE_CIVFEMALE));
        p->m_nStatType = (ePedStats)(r.below(10) == 0 ? (int)r.below(60) - 1 : (int)r.below(48) - 1);
        p->m_nCarsCanDriveMask = (uint16)(r.below(3) ? 0x1000 | r.below(0x1000) : r.below(0x10000));
        p->m_nRace = (uint8)(r.below(3) ? 0 : r.below(5));
    }
    for (int i = kVehLo; i < kVehHi; ++i) {
        auto* v = VM(i);
        v->m_nRefCount = (uint16)(r.below(3) ? r.below(5) : r.below(0x10000));
        v->m_nFrq = (uint16)(r.below(8) ? r.below(25) : r.below(0x8000));   // the exe reads it as a signed word
        v->m_nTimesUsed = (uint8)(r.below(3) ? r.below(10) : r.below(256));
        v->m_nVehicleClass = (eVehicleClass)(r.below(4) ? r.below(13) : r.below(16));
    }
    for (int i = 0; i < kNMI; ++i) {
        auto& si = CStreaming::ms_aInfoForModel[i];
        si.m_LoadState = (eStreamingLoadState)(r.below(3) ? 1 : r.below(5));
        si.m_NextIndex = si.m_PrevIndex = (int16)(r.below(2) ? -1 : (int)r.below(kNMI));   // in a list (both links) or not
    }
    for (auto& m : CTheScripts::SuppressedVehicleModels) m = (eModelID)(r.below(3) ? -1 : GenVehId(r));
    for (auto& m : CTheScripts::VehicleModelsBlockedByScript) m = (eModelID)(r.below(3) ? -1 : GenVehId(r));
    for (auto& m : CStreaming::ms_pedsLoaded) m = (eModelID)GenPedId(r);
    for (size_t i = 0; i < CStreaming::ms_pedsLoaded.size(); ++i) if (CStreaming::ms_pedsLoaded[i] >= 0 && r.below(5)) PM(CStreaming::ms_pedsLoaded[i])->m_nRefCount = (uint16)i;
    CStreaming::ms_numPedsLoaded = r.below(9);
    std::memset(CCheat::m_aCheatsActive, 0, sizeof CCheat::m_aCheatsActive);
    if (r.below(6) == 0) CCheat::m_aCheatsActive[r.below(3) ? CHEAT_ELVIS_IS_EVERYWHERE : r.below(TOTAL_CHEATS)] = true;
    CGame::currArea = (eAreaCodes)(r.below(3) ? 0 : r.below(5));
    CWeather::Rain = r.below(2) ? 0.f : r.f01();
    CWeather::WeatherRegion = (eWeatherRegion)r.below(5);
    CClock::ms_nGameClockHours = (uint8)r.below(24);
    CClock::CurrentDay = (uint8)r.below(9);
    CCullZones::CurrentFlags_Player = r.below(3) ? 0 : (int32)r.u32();
    CPopulation::NumberOfPedsInUseInterior = r.below(40);
    CPopulation::MaxNumberOfPedsInUse = r.below(60);
    CPopulation::m_bMoreCarsAndFewerPeds = r.below(2);
    CPopulation::bInPoliceStation = r.below(3) == 0;
    CPopulation::m_bDontCreateRandomGangMembers = r.below(6) == 0;
    CPopulation::m_bOnlyCreateRandomGangMembers = r.below(6) == 0;
    CPopulation::m_bDontCreateRandomCops = r.below(6) == 0;
    CPopulation::ms_nNumDealers = r.below(8); CPopulation::ms_nNumCop = r.below(20);
    CPopulation::ms_nNumCivMale = r.below(60); CPopulation::ms_nNumCivFemale = r.below(60);
    for (auto& g : CPopulation::ms_nNumGang) g = r.below(20);
    CPopulation::CurrentWorldZone = r.below(3);
    GenGroup(CPopulation::m_AppropriateLoadedCars, r); GenGroup(CPopulation::m_InAppropriateLoadedCars, r); GenGroup(CPopulation::m_LoadedBoats, r);
    for (auto& g : CPopulation::m_LoadedGangCars) GenGroup(g, r, r.below(2) ? 3 : 23);
    GenGroup(CStreaming::ms_vehiclesLoaded, r);
    // zone
    for (auto& g : Zone->GangStrength) g = (uint8)(r.below(3) ? r.below(40) : r.below(256));
    Zone->DealerStrength = (uint8)(r.below(3) ? r.below(16) : r.below(256));
    Zone->PopType = r.below(+eZonePopulationType::COUNT);
    Zone->PopRaces = r.below(16);
    Zone->IsNoCops = r.below(4) == 0;
    CPopCycle::m_nCurrentZoneType = Zone->PopType;
    CPopCycle::m_nCurrentTimeOfWeek = r.below(2);
    CPopCycle::m_nCurrentTimeIndex = r.below(12);
    CPopCycle::m_pCurrZoneInfo = (g_zoneNullOk && r.below(10) == 0) ? nullptr : Zone;
    // pop-cycle tables / floats
    const auto GenPerc = [&](auto& arr) { auto* b = reinterpret_cast<uint8*>(&arr); for (size_t i = 0; i < sizeof(arr); ++i) b[i] = (uint8)(r.below(3) ? r.below(30) : r.below(101)); };
    GenPerc(CPopCycle::m_nPercOther); GenPerc(CPopCycle::m_nPercCops); GenPerc(CPopCycle::m_nPercGang); GenPerc(CPopCycle::m_nPercDealers);
    GenPerc(CPopCycle::m_nMaxNumCars); GenPerc(CPopCycle::m_nMaxNumPeds);
    {   // group percentage rows: sum >= 100 (the game's own rescale keeps them there; below that the exe walks out of the row)
        auto* rows = reinterpret_cast<uint8(*)[POPCYCLE_TOTAL_GROUP_PERCS]>(&CPopCycle::m_nPercTypeGroup);
        const size_t nRows = sizeof(CPopCycle::m_nPercTypeGroup) / POPCYCLE_TOTAL_GROUP_PERCS;
        for (size_t i = 0; i < nRows; ++i) {
            int sum = 0;
            for (int g = 0; g < POPCYCLE_TOTAL_GROUP_PERCS; ++g) { rows[i][g] = (uint8)(r.below(3) ? 0 : r.below(40)); sum += rows[i][g]; }
            if (sum < 100) rows[i][r.below(POPCYCLE_TOTAL_GROUP_PERCS)] += (uint8)(100 - sum);
            if (r.below(8) == 0) { const int k = r.below(POPCYCLE_TOTAL_GROUP_PERCS); const int j = (k + 1) % POPCYCLE_TOTAL_GROUP_PERCS; rows[i][j] = (uint8)(rows[i][j] + rows[i][k]); rows[i][k] = 0; }   // a zero entry in the walk, sum unchanged
        }
    }
    for (auto& g : CPopulation::m_nNumPedsInGroup) g = (uint16)(r.below(8) == 0 ? 0 : r.below(22));
    for (size_t g = 0; g < CPopulation::m_PedGroups.size(); ++g) for (auto& m : CPopulation::m_PedGroups[g]) m = (int16)r.below(kPedMI);
    for (auto& t : CPopulation::m_TranslationArray) for (auto& g : t.pedGroupIds) g = (ePopcyclePedGroup)r.below(POPCYCLE_TOTAL_PEDGROUPS);
    for (int g = 0; g < 10; ++g) for (int z = 0; z < 3; ++z) { auto& n = CPopulation::m_nNumPedsInGroup[CPopulation::GetGangGroupId((eGangID)g, z)]; if (!n) n = 1 + r.below(21); }   // a gang ped group is never empty (the exe divides by its size: InitSequence)
    for (auto& v : CStreaming::ms_NextPedToLoadFromGroup) v = (int)r.below(21);
    CPopCycle::m_NumOther_Peds = GenPopF(r); CPopCycle::m_NumCops_Peds = GenPopF(r); CPopCycle::m_NumGangs_Peds = GenPopF(r); CPopCycle::m_NumDealers_Peds = GenPopF(r);
    CPopCycle::m_NumOther_Cars = GenPopF(r); CPopCycle::m_NumCops_Cars = GenPopF(r); CPopCycle::m_NumGangs_Cars = GenPopF(r); CPopCycle::m_NumDealers_Cars = GenPopF(r);
    CPopCycle::m_fPercOther = r.f01(); CPopCycle::m_fPercCops = r.f01(); CPopCycle::m_fPercGangs = r.f01(); CPopCycle::m_fPercDealers = r.f01();
    CPopCycle::m_fCurrentZoneDodgyness = r.f01();
    // zones (UpdateDealerStrengths walks CTheZones::ZoneInfoArray)
    CTheZones::TotalNumberOfMapZones = (int16)(r.below(6) == 0 ? 0 : r.below(40));
    for (auto& z : CTheZones::ZoneInfoArray) {
        for (auto& g : z.GangStrength) g = (uint8)(r.below(3) ? r.below(40) : r.below(256));
        z.DealerStrength = (uint8)(r.below(3) ? r.below(17) : r.below(256));
    }
    CGangWars::bGangWarsActive = r.below(4) != 0;
    CTimer::m_snTimeInMilliseconds = r.u32() % 1000000; CTimer::m_snPreviousTimeInMilliseconds = r.below(2) ? CTimer::m_snTimeInMilliseconds - r.below(60) : r.u32() % 1000000;
    gfLaRiotsLightMult = r.below(4) ? 0.6f + r.f01() * 0.4f : r.f01() * 2.f;
    CCustomBuildingDNPipeline::m_fDNBalanceParam = r.below(3) ? r.f01() : 1.f;
    CWorld::Players[0].m_pPed->m_placement.m_vPosn = r.below(3) == 0 ? CVector{ 1700.f + (float)r.below(300), -1500.f + (float)r.below(300), r.below(10) ? 10.f : 960.f } : CVector{ GenF(r, 3000.f), GenF(r, 3000.f), r.below(8) ? 10.f : 960.f };
    gbLARiots = r.below(2);
    CTheScripts::OnAMissionFlag = r.below(2) ? 0 : 16; { uint32 one = r.below(2); std::memcpy(&CTheScripts::ScriptSpace[16], &one, 4); }
    CDarkel::Status = r.below(6) == 0 ? eDarkelStatus::FRENZY_ON_GOING : (eDarkelStatus)r.below(6);
    CGangWars::State = (eGangWarState)(r.below(4) == 0 ? r.below(7) : 0);
    CGangWars::State2 = (eGangAttackState)(r.below(4) == 0 ? r.below(4) : 0);
    // car groups / police car choice
    for (auto& n : CPopulation::m_nNumCarsInGroup) n = (uint16)(r.below(6) == 0 ? 0 : r.below(24));
    for (size_t g = 0; g < CPopulation::m_CarGroups.size(); ++g) for (auto& m : CPopulation::m_CarGroups[g]) m = (int16)(r.below(4) == 0 ? CPopulation::m_DefaultModelIDForUnusedSlot : GenVehId(r));
    for (auto& m : CStreaming::ms_aDefaultCopCarModel) m = r.below(3) ? GenVehId(r) : (int)r.below(kNMI);
    for (auto& m : CStreaming::ms_aDefaultCopModel) m = (int)r.below(kPedMI);
    CStreaming::ms_DefaultCopBikeModel = GenVehId(r); CStreaming::ms_DefaultCopBikerModel = (int)r.below(kPedMI); CStreaming::m_bCopBikeLoaded = r.below(2);
    CTheZones::m_CurrLevel = (eLevelName)r.below(4);
    CTheScripts::ForceRandomCarModel = r.below(5) == 0 ? GenVehId(r) : -1;
    CCarCtrl::SequenceElements = r.below(30); CCarCtrl::SequenceRandomOffset = r.below(30); CCarCtrl::bSequenceOtherWay = r.below(2);
    for (auto& g : CGangs::Gang) g.m_nPedModelOverride = (int8)(r.below(4) == 0 ? (int)r.below(10) : -1);
    lw::W->m_WantedLevel = (eWantedLevel)r.below(7); lw::W->m_Flags = (uint8)(r.below(2) ? 0 : r.u32());
    CCheat::m_aCheatsActive[CHEAT_REDUCED_TRAFFIC] = r.below(8) == 0; CCheat::m_aCheatsActive[CHEAT_GANGS_CONTROLS_THE_STREETS] = r.below(8) == 0;
}

static std::vector<Snap> g_snaps;
static void MakeSnaps() {
    if (!g_snaps.empty()) return;
    g_snaps.emplace_back((uintptr_t)MI, (size_t)kNMI * kMISz);
    g_snaps.emplace_back((uintptr_t)Zone, 64);
    g_snaps.emplace_back(0x8E4CC0, (size_t)kNMI * 0x14);   // CStreaming::ms_aInfoForModel[0..kNMI)
    g_snaps.emplace_back(0x8E4BA0, 0x120);                 // CStreaming misc statics up to the info array
    g_snaps.emplace_back(0x8D2530, 0x10);                  // CPopulation float / counters
    g_snaps.emplace_back(0xC0BC30, 0xC0FCC0 - 0xC0BC30);   // CPopCycle + CPopulation statics
    g_snaps.emplace_back(0x969130, 0x200);                 // cheats
    g_snaps.emplace_back(0xB70140, 0x40);                  // clock
    g_snaps.emplace_back(0xC81300, 0x60);                  // weather
    g_snaps.emplace_back(0xC87AB8, 4);                     // cull zone flags
    g_snaps.emplace_back(0xBA1DF0, 380 * 0x11);            // CTheZones::ZoneInfoArray
    g_snaps.emplace_back(0x969000, 0x300);                 // cheat flags (REDUCED_TRAFFIC / GANGS_CONTROL ...) and gang wars flags
    g_snaps.emplace_back(0xBA1900, 2);                     // CTheZones::TotalNumberOfMapZones
    g_snaps.emplace_back(0xB7CB60, 0x20);                  // CTimer
    g_snaps.emplace_back(0x8CD060, 4);                     // gfLaRiotsLightMult
}
static std::string g_dMem;
static void SaveAll(std::vector<std::vector<uint8>>& o) { MakeSnaps(); o.resize(g_snaps.size()); for (size_t i = 0; i < g_snaps.size(); ++i) { g_snaps[i].Save(); o[i] = g_snaps[i].b; } }
static void RestoreAll(const std::vector<std::vector<uint8>>& o) { for (size_t i = 0; i < g_snaps.size(); ++i) { g_snaps[i].b = o[i]; g_snaps[i].Restore(); } }
static bool SameAll(const std::vector<std::vector<uint8>>& a, const std::vector<std::vector<uint8>>& b) {
    bool ok = true;
    for (size_t i = 0; i < a.size(); ++i) {
        if (!SameBlob(a[i].data(), b[i].data(), a[i].size())) {
            ok = false;
            g_dMem += " mem[" + std::to_string(i) + "]" + DiffDesc(a[i].data(), b[i].data(), a[i].size(), "m");
        }
    }
    return ok;
}

// generic runner: gen the world, `prep(r)` (per-case extras), port fn -> snapshot -> restore -> exe fn -> snapshot -> compare statics and return value
template<class RetT, class PortFn, class ExeFn, class PrepFn>
static bool Case(Rng& r, std::string& d, PrepFn&& prep, PortFn&& port, ExeFn&& exe, const char* what = "") {
    GenWorld(r);
    prep(r);
    const int seed = r.u32();
    static std::vector<std::vector<uint8>> w0, w1, w2;
    SaveAll(w0);
    SeedBoth(seed);
    RetT a = port();
    SaveAll(w1);
    RestoreAll(w0);
    SeedBoth(seed);
    RetT e = exe();
    SaveAll(w2);
    g_dMem.clear();
    bool ok = SameAll(w1, w2);
    ok = ok && (std::memcmp(&a, &e, sizeof(RetT)) == 0);
    if (!ok) {
        d = std::string(what) + g_dMem;
        if constexpr (std::is_arithmetic_v<RetT> || std::is_enum_v<RetT>) { d += " ret got " + std::to_string((double)(a)) + " exe " + std::to_string((double)(e)); }
        else { d += " ret got"; for (auto v : a) d += " " + std::to_string(v); d += " exe"; for (auto v : e) d += " " + std::to_string(v); }
    }
    return ok;
}
template<class RetT, class PortFn, class ExeFn>
static bool Case(Rng& r, std::string& d, PortFn&& port, ExeFn&& exe, const char* what = "") { return Case<RetT>(r, d, [](Rng&) {}, port, exe, what); }
} // namespace lp

static int __cdecl HostStricmp(const char* a, const char* b) { return _stricmp(a, b); }   // the exe's CRT _stricmp (0x8229B6) needs its locale / lock state

static void TestPop() {
    using namespace lp;
    Init();
    oracle::Patch(0x8229B6, (void*)&HostStricmp);
    auto sub = [](Rng& r) { return Rng(r.u32() * 2654435761ull + 77); };
    const auto ped = [](Rng& a) { return (eModelID)(a.below(10) == 0 ? (int)a.below(kNMI) : (int)a.below(kPedMI)); };
    const auto veh = [](Rng& a) { return (eModelID)(kVehLo + (int)a.below(kVehHi - kVehLo)); };

    // ---- CLoadedCarGroup (thiscall; instance = a heap group whose bytes are compared next to the statics)
    {
        using GT = CLoadedCarGroup;
        static GT* G = new (std::calloc(1, sizeof(GT))) GT();
        const auto GCase = [&](Rng& r, std::string& d, auto&& port, auto&& exe, const char* what) {
            static std::vector<uint8> g0(sizeof(GT)), g1(sizeof(GT)), g2(sizeof(GT));
            GenWorld(r);
            GenGroup(*G, r, r.below(2) ? 23 : 1 + r.below(6));
            if (r.below(8) == 0) reinterpret_cast<int16*>(G)[r.below(23)] = (int16)GenVehId(r);   // holes / garbage after the first sentinel
            const int seed = r.u32();
            std::memcpy(g0.data(), G, sizeof(GT));
            if (g_trace) std::printf("  %s group %s\n", what, Hex(g0.data(), sizeof(GT)).c_str());
            static std::vector<std::vector<uint8>> w0, w1, w2;
            SaveAll(w0);
            SeedBoth(seed); const int a = port(*G); std::memcpy(g1.data(), G, sizeof(GT)); SaveAll(w1);
            std::memcpy(G, g0.data(), sizeof(GT)); RestoreAll(w0);
            SeedBoth(seed); const int e = exe(*G); std::memcpy(g2.data(), G, sizeof(GT)); SaveAll(w2);
            g_dMem.clear();
            const bool ok = a == e && !std::memcmp(g1.data(), g2.data(), sizeof(GT)) && SameAll(w1, w2);
            if (!ok) { for (int i = 0; i < 23; ++i) { const int m = reinterpret_cast<int16*>(g0.data())[i]; if (m < 0) break; const auto* v = VM(m); const auto& si = CStreaming::ms_aInfoForModel[m]; char b[200]; std::snprintf(b, sizeof b, " [m%d frq %d cls %d ref %d used %d ld %d n %d p %d]", m, (int)(int16)v->m_nFrq, (int)v->m_nVehicleClass, (int)v->m_nRefCount, (int)v->m_nTimesUsed, (int)si.m_LoadState, (int)si.m_NextIndex, (int)si.m_PrevIndex); d += b; } }
            if (!ok) d += std::string(what) + " group0 " + Hex(g0.data(), sizeof(GT)) + " got " + std::to_string(a) + " exe " + std::to_string(e) + " grp got " + Hex(g1.data(), sizeof(GT)) + " exe " + Hex(g2.data(), sizeof(GT)) + g_dMem;
            return ok;
        };
        Run("CLoadedCarGroup::Clear 0x611B90", [&](Rng& r, std::string& d) {
            return GCase(r, d, [](GT& g) { g.Clear(); return 0; }, [](GT& g) { oracle::Fn<void __fastcall(GT*, int)>(0x611B90)(&g, 0); return 0; }, "Clear");
        }, 600);
        Run("CLoadedCarGroup::AddMember 0x611BB0", [&](Rng& r, std::string& d) {
            Rng a = sub(r); const eModelID m = a.below(8) == 0 ? (eModelID)(int)a.below(40000) : veh(a);
            return GCase(r, d, [&](GT& g) { g.AddMember(m); return 0; }, [&](GT& g) { oracle::Fn<void __fastcall(GT*, int, int)>(0x611BB0)(&g, 0, (int)m); return 0; }, "AddMember");
        }, 1500);
        Run("CLoadedCarGroup::RemoveMember 0x611BD0", [&](Rng& r, std::string& d) {
            Rng a = sub(r); const bool useMem = a.below(2); const uint32 pk = a.u32(); const eModelID rnd = veh(a);
            const auto pick = [&](GT& g) { return useMem && !g.Empty() ? g.GetMember(pk % g.CountMembers()) : rnd; };
            return GCase(r, d, [&](GT& g) { g.RemoveMember(pick(g)); return 0; }, [&](GT& g) { oracle::Fn<void __fastcall(GT*, int, int)>(0x611BD0)(&g, 0, (int)pick(g)); return 0; }, "RemoveMember");
        }, 1500);
        Run("CLoadedCarGroup::GetMember 0x611C20", [&](Rng& r, std::string& d) {
            Rng a = sub(r); const uint32 pk = a.u32();
            return GCase(r, d, [&](GT& g) { const auto n = g.CountMembers(); return n ? (int)g.GetMember(pk % n) : -2; }, [&](GT& g) { const auto n = g.CountMembers(); return n ? (int)oracle::Fn<int __fastcall(GT*, int, uint32)>(0x611C20)(&g, 0, pk % n) : -2; }, "GetMember");
        }, 1000);
        Run("CLoadedCarGroup::CountMembers 0x611C30", [&](Rng& r, std::string& d) {
            return GCase(r, d, [](GT& g) { return (int)g.CountMembers(); }, [](GT& g) { return (int)oracle::Fn<uint32 __fastcall(GT*, int)>(0x611C30)(&g, 0); }, "CountMembers");
        }, 1000);
        Run("CLoadedCarGroup::PickRandomCar 0x611C50", [&](Rng& r, std::string& d) {
            Rng a = sub(r); const bool nt = a.below(2), no = a.below(2);
            if (g_trace) std::printf("  nt %d no %d\n", nt, no);
            return GCase(r, d, [&](GT& g) { return (int)g.PickRandomCar(nt, no); }, [&](GT& g) { return (int)oracle::Fn<int __fastcall(GT*, int, bool, bool)>(0x611C50)(&g, 0, nt, no); }, (std::string("PickRandomCar nt ") + (nt ? "1" : "0") + " no " + (no ? "1" : "0")).c_str());
        }, 3000);
        Run("CLoadedCarGroup::SortBasedOnUsage 0x611E10", [&](Rng& r, std::string& d) {
            return GCase(r, d, [](GT& g) { g.SortBasedOnUsage(); return 0; }, [](GT& g) { oracle::Fn<void __fastcall(GT*, int)>(0x611E10)(&g, 0); return 0; }, "SortBasedOnUsage");
        }, 1500);
        Run("CLoadedCarGroup::PickLeastUsedModel 0x611E90", [&](Rng& r, std::string& d) {
            Rng a = sub(r); const int mx = a.below(4) ? (int)a.below(12) : (int)a.u32();
            return GCase(r, d, [&](GT& g) { return (int)g.PickLeastUsedModel(mx); }, [&](GT& g) { return (int)oracle::Fn<int __fastcall(GT*, int, int)>(0x611E90)(&g, 0, mx); }, "PickLeastUsedModel");
        }, 1500);
    }

    // ---- CPopulation: constants / predicates over model infos and statics
    Run("CPopulation::FindDummyDistForModel 0x610ED0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const eModelID m = a.below(3) == 0 ? (eModelID)(a.below(2) ? (int)ModelIndices::MI_SAMSITE : (int)ModelIndices::MI_SAMSITE2) : (eModelID)(int)a.below(kNMI);
        return Case<float>(r, d, [&] { return CPopulation::FindDummyDistForModel(m); }, [&] { return oracle::Fn<float __cdecl(int)>(0x610ED0)((int)m); }, "FindDummyDist");
    }, 600);
    Run("CPopulation::FindPedDensityMultiplierCullZone 0x610F00", [&](Rng& r, std::string& d) {
        return Case<float>(r, d, [&] { return CPopulation::FindPedDensityMultiplierCullZone(); }, [&] { return oracle::Fn<float __cdecl()>(0x610F00)(); }, "PedDensityCullZone");
    }, 600);
    Run("CPopulation::ChoosePolicePedOccupation 0x610F40", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [&] { return (int)CPopulation::ChoosePolicePedOccupation(); }, [&] { return oracle::Fn<int __cdecl()>(0x610F40)(); }, "ChoosePolicePedOccupation");
    }, 100);
    Run("CPopulation::ArePedStatsCompatible 0x610F50", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const ePedStats s1 = (ePedStats)((int)a.below(60) - 2), s2 = (ePedStats)((int)a.below(60) - 2);
        return Case<int>(r, d, [&] { const bool b = CPopulation::ArePedStatsCompatible(s1, s2); Hit(b); return (int)b; }, [&] { return (int)oracle::Fn<bool __cdecl(int, int)>(0x610F50)((int)s1, (int)s2); }, "ArePedStatsCompatible");
    }, 3000);
    Run("CPopulation::PedMICanBeCreatedAtAttractor 0x6110C0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const eModelID m = ped(a);
        return Case<int>(r, d, [&] { const bool b = CPopulation::PedMICanBeCreatedAtAttractor(m); Hit(b); return (int)b; }, [&] { return (int)oracle::Fn<bool __cdecl(int)>(0x6110C0)((int)m); }, "AtAttractor");
    }, 1500);
    Run("CPopulation::PedMICanBeCreatedAtThisAttractor 0x6110E0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); static const char* names[] = { nullptr, "COPSIT", "COPLOOK", "BROWSE", "DANCER", "BARGUY", "PEDROUL", "PEDCARD", "PEDSLOT", "STRIPW", "STRIPM", "copsit", "Dancer", "xx", "" };
        const eModelID m = (eModelID)(int)a.below(kPedMI); const char* nm = names[a.below(15)];
        return Case<int>(r, d, [&] { const bool b = CPopulation::PedMICanBeCreatedAtThisAttractor(m, nm); Hit(b); return (int)b; }, [&] { return (int)oracle::Fn<bool __cdecl(int, const char*)>(0x6110E0)((int)m, nm); }, "AtThisAttractor");
    }, 3000);
    Run("CPopulation::PedMICanBeCreatedInInterior 0x611450", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const eModelID m = ped(a);
        return Case<int>(r, d, [&] { const bool b = CPopulation::PedMICanBeCreatedInInterior(m); Hit(b); return (int)b; }, [&] { return (int)oracle::Fn<bool __cdecl(int)>(0x611450)((int)m); }, "InInterior");
    }, 1500);
    {
        struct B { const char* n; unsigned va; bool (*pf)(eModelID); };
        static const B bs[] = { { "IsMale", 0x611470, &CPopulation::IsMale }, { "IsFemale", 0x611490, &CPopulation::IsFemale }, { "CanSolicitPlayerInCar", 0x611790, &CPopulation::CanSolicitPlayerInCar },
                                { "CanJeerAtStripper", 0x6117B0, &CPopulation::CanJeerAtStripper }, { "CanSolicitPlayerOnFoot", 0x611780, &CPopulation::CanSolicitPlayerOnFoot },
                                { "IsSunbather", 0x611760, static_cast<bool (*)(eModelID)>(&CPopulation::IsSunbather) } };
        for (auto& b : bs)
            Run(std::string("CPopulation::") + b.n + " 0x" + HexVA(b.va), [&](Rng& r, std::string& d) {
                Rng a = sub(r); const eModelID m = ped(a);
                return Case<int>(r, d, [&] { const bool x = b.pf(m); Hit(x); return (int)x; }, [&] { return (int)oracle::Fn<bool __cdecl(int)>(b.va)((int)m); }, b.n);
            }, 1500);
    }
    Run("CPopulation::IsSecurityGuard 0x6114B0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const ePedType t = (ePedType)a.below(40);
        return Case<int>(r, d, [&] { return (int)CPopulation::IsSecurityGuard(t); }, [&] { return (int)oracle::Fn<bool __cdecl(int)>(0x6114B0)((int)t); }, "IsSecurityGuard");
    }, 200);
    Run("CPopulation::IsCorrectTimeOfDayForEffect 0x611B20", [&](Rng& r, std::string& d) {
        Rng a = sub(r);
        static C2dEffectPedAttractor fx;
        const int type = a.below(3) ? (int)a.below(14) : (int)a.below(256);
        return Case<int>(r, d, [&](Rng&) { std::memset(&fx, 0, sizeof fx); fx.m_nAttractorType = (ePedAttractorType)type; },
                         [&] { const bool b = CPopulation::IsCorrectTimeOfDayForEffect(fx); Hit(b); return (int)b; }, [&] { return (int)oracle::Fn<bool __cdecl(const C2dEffectPedAttractor*)>(0x611B20)(&fx); }, "TimeOfDayForEffect");
    }, 2000);
    Run("CPopulation::FindCarMultiplierMotorway 0x611B60", [&](Rng& r, std::string& d) {
        return Case<float>(r, d, [&] { return CPopulation::FindCarMultiplierMotorway(); }, [&] { return oracle::Fn<float __cdecl()>(0x611B60)(); }, "CarMotorway");
    }, 200);
    Run("CPopulation::FindPedMultiplierMotorway 0x611B80", [&](Rng& r, std::string& d) {
        return Case<float>(r, d, [&] { return CPopulation::FindPedMultiplierMotorway(); }, [&] { return oracle::Fn<float __cdecl()>(0x611B80)(); }, "PedMotorway");
    }, 200);
    Run("CPopulation::FindSpecificDriverModelForCar_ToUse 0x611900", [&](Rng& r, std::string& d) {
        Rng a = sub(r); static const int sp[] = { MODEL_STRETCH, MODEL_TAXI, MODEL_CABBIE, MODEL_MRWHOOP, MODEL_SECURICA, MODEL_PIZZABOY, MODEL_BMX, MODEL_FREEWAY };
        const eModelID m = (eModelID)(a.below(2) ? sp[a.below(8)] : (int)a.below(700));
        return Case<int>(r, d, [&] { return (int)CPopulation::FindSpecificDriverModelForCar_ToUse(m); }, [&] { return oracle::Fn<int __cdecl(int)>(0x611900)((int)m); }, "SpecificDriver");
    }, 1500);
    Run("CPopulation::PickGangCar 0x614490", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const eGangID g = (eGangID)a.below(10);
        return Case<int>(r, d, [&] { return (int)CPopulation::PickGangCar(g); }, [&] { return oracle::Fn<int __cdecl(int)>(0x614490)((int)g); }, "PickGangCar");
    }, 2000);
    Run("CPopulation::PickRiotRoadBlockCar 0x6144B0", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [&] { return (int)CPopulation::PickRiotRoadBlockCar(); }, [&] { return oracle::Fn<int __cdecl()>(0x6144B0)(); }, "PickRiotRoadBlockCar");
    }, 2000);

    // ---- CPopulation: occupation choice
    {
        static CVehicle* fv = (CVehicle*)std::calloc(1, sizeof(CHeli));
        Run("CPopulation::ChooseCivilianOccupationForVehicle 0x613260", [&](Rng& r, std::string& d) {
            Rng a = sub(r); const bool male = a.below(2); const int vm = (int)veh(a);
            return Case<int>(r, d, [&](Rng& rr) { std::memset(fv, 0, sizeof(CHeli)); fv->m_nModelIndex = (int16)vm;
                                                  fv->m_pDriver = nullptr;
                                                  if (rr.below(3) == 0) { auto* pd = (CPed*)std::calloc(1, 4096); pd->m_nModelIndex = (int16)GenPedId(rr) < 0 ? 5 : (int16)GenPedId(rr); fv->m_pDriver = pd; }
                                                  for (auto& p : fv->m_apPassengers) p = nullptr; },
                             [&] { return (int)CPopulation::ChooseCivilianOccupationForVehicle(male, fv); }, [&] { return oracle::Fn<int __cdecl(bool, CVehicle*)>(0x613260)(male, fv); }, "OccupationForVehicle");
        }, 3000);
    }
    Run("CPopulation::ChooseCivilianOccupation 0x612F90", [&](Rng& r, std::string& d) {
        Rng a = sub(r);
        const bool male = a.below(3) == 0, female = a.below(3) == 0, onFoot = a.below(3) == 0, tests = a.below(3) == 0, attr = a.below(3) == 0;
        const AssocGroupId anim = (AssocGroupId)(a.below(2) ? -1 : (int)a.below(6));
        const eModelID notm = (eModelID)(a.below(2) ? -1 : (int)a.below(kPedMI));
        const ePedStats st = (ePedStats)(a.below(2) ? -1 : (int)a.below(48));
        return Case<int>(r, d, [&] { return (int)CPopulation::ChooseCivilianOccupation(male, female, anim, notm, st, onFoot, tests, attr, nullptr); },
                         [&] { return oracle::Fn<int __cdecl(bool, bool, int, int, int, bool, bool, bool, const char*)>(0x612F90)(male, female, (int)anim, (int)notm, (int)st, onFoot, tests, attr, nullptr); }, "ChooseCivilianOccupation");
    }, 4000);
    Run("CPopulation::ChooseCivilianCoupleOccupations 0x613180", [&](Rng& r, std::string& d) {
        static eModelID hp, wp, he, we;
        const bool ok = Case<int>(r, d, [&] { hp = wp = (eModelID)-99; CPopulation::ChooseCivilianCoupleOccupations(hp, wp); return (int)hp * 1000 + (int)wp; },
                                  [&] { he = we = (eModelID)-99; oracle::Fn<void __cdecl(eModelID*, eModelID*)>(0x613180)(&he, &we); return (int)he * 1000 + (int)we; }, "Couple");
        return ok;
    }, 3000);

    Run("CPopulation::FindNumberOfPedsWeCanPlaceOnBenches 0x612240", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [&] { return CPopulation::FindNumberOfPedsWeCanPlaceOnBenches(); }, [&] { return oracle::Fn<int __cdecl()>(0x612240)(); }, "PedsOnBenches");
    }, 4000);
    Run("CPopulation::FindDistanceToNearestPedOfType 0x6143E0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const ePedType t = (ePedType)(a.below(4) ? (a.below(2) ? PED_TYPE_CIVMALE : PED_TYPE_COP) : a.below(32)); const CVector pos = GenV(a, a.below(2) ? 60.f : 3000.f, S_NOHUGE);
        return Case<float>(r, d, [&](Rng& rr) { for (int i = 0; i < lw::kPeds; ++i) { auto* p = lw::PedObj[i]; std::memset((void*)p, 0, 0x7C4); lw::PedState[i] = rr.below(4) ? (uint8)rr.below(0x7F) : 0x80;
                                                  p->m_nPedType = (ePedType)(rr.below(2) ? t : (ePedType)rr.below(32)); p->m_matrix = nullptr; p->m_placement.m_vPosn = rr.below(2) ? CVector{ pos.x + GenF(rr, 40.f), pos.y + GenF(rr, 40.f), pos.z + GenF(rr, 10.f) } : GenV(rr, 3000.f, S_NOHUGE); } },
                         [&] { return CPopulation::FindDistanceToNearestPedOfType(t, pos); }, [&] { return oracle::Fn<float __cdecl(int, float, float, float)>(0x6143E0)((int)t, pos.x, pos.y, pos.z); }, "NearestPedOfType");
    }, 3000);
    Run("CPopulation::PedCreationDistMultiplier 0x6116C0", [&](Rng& r, std::string& d) {
        Rng a = sub(r);
        return Case<float>(r, d, [&](Rng& rr) { auto* v = lw::VehObj[0]; std::memset((void*)v, 0, sizeof(CHeli)); v->m_vecMoveSpeed = GenV(rr, rr.below(2) ? 3.f : 0.5f, S_NOHUGE); lw::VehState[0] = 1;
                                                  lw::Pl->bInVehicle = rr.below(4) != 0; lw::Pl->m_pVehicle = rr.below(5) ? v : nullptr; },
                         [&] { return CPopulation::PedCreationDistMultiplier(); }, [&] { return oracle::Fn<float __cdecl()>(0x6116C0)(); }, "PedCreationDistMultiplier");
    }, 2500);

    // ---- CPopCycle
    Run("CPopCycle::GetCurrentPercOther_Peds 0x610310", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [&] { return CPopCycle::GetCurrentPercOther_Peds(); }, [&] { return oracle::Fn<int __cdecl()>(0x610310)(); }, "PercOther");
    }, 2500);
    Run("CPopCycle::IsPedAppropriateForCurrentZone 0x610150", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const int m = (int)ped(a);
        return Case<int>(r, d, [&] { const bool b = CPopCycle::IsPedAppropriateForCurrentZone(m); Hit(b); return (int)b; }, [&] { return (int)oracle::Fn<bool __cdecl(int)>(0x610150)(m); }, "IsPedAppropriate");
    }, 3000);
    Run("CPopCycle::IsPedInGroup 0x610210", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const int m = (int)ped(a); const ePopcycleGroup g = (ePopcycleGroup)a.below(POPCYCLE_TOTAL_GROUPS);
        return Case<int>(r, d, [&] { const bool b = CPopCycle::IsPedInGroup(m, g); Hit(b); return (int)b; }, [&] { return (int)oracle::Fn<bool __cdecl(int, int)>(0x610210)(m, (int)g); }, "IsPedInGroup");
    }, 3000);
    g_zoneNullOk = true;
    Run("CPopCycle::PedIsAcceptableInCurrentZone 0x610720", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const int m = (int)ped(a);
        return Case<int>(r, d, [&] { const bool b = CPopCycle::PedIsAcceptableInCurrentZone(m); Hit(b); return (int)b; }, [&] { return (int)oracle::Fn<bool __cdecl(int)>(0x610720)(m); }, "PedIsAcceptable");
    }, 2500);
    Run("CPopCycle::PlayerKilledADealer 0x610490", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [&] { CPopCycle::PlayerKilledADealer(); return 0; }, [&] { oracle::Fn<void __cdecl()>(0x610490)(); return 0; }, "PlayerKilledADealer");
    }, 600);
    Run("CPopCycle::FindNewPedType 0x60FBD0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const bool ng = a.below(4) == 0, nc = a.below(4) == 0;
        using R3 = std::array<int, 3>;
        // the outputs are only meaningful when the answer is true (the port additionally clears outPedMI on its early outs: NOTSA)
        return Case<R3>(r, d, [&] { ePedType t = (ePedType)0x7777; eModelID m = (eModelID)0x7777; const bool b = CPopCycle::FindNewPedType(t, m, ng, nc); Hit(b); return b ? R3{ 1, (int)t, (int)m } : R3{ 0, 0, 0 }; },
                        [&] { uint32 t = 0x7777; int m = 0x7777; const bool b = oracle::Fn<bool __cdecl(uint32*, int*, bool, bool)>(0x60FBD0)(&t, &m, ng, nc); return b ? R3{ 1, (int)t, (int)m } : R3{ 0, 0, 0 }; }, "FindNewPedType");
    }, 5000);
    g_zoneNullOk = false;
    Run("CPopCycle::PickARandomGroupOfOtherPeds 0x610420", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [&] { return (int)CPopCycle::PickARandomGroupOfOtherPeds(); }, [&] { return oracle::Fn<int __cdecl()>(0x610420)(); }, "PickARandomGroupOfOtherPeds");
    }, 3000);
    Run("CPopCycle::PickPedMIToStreamInForCurrentZone 0x60FFD0", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [&] { return (int)CPopCycle::PickPedMIToStreamInForCurrentZone(); }, [&] { return oracle::Fn<int __cdecl()>(0x60FFD0)(); }, "PickPedMIToStream");
    }, 3000);
    Run("CPopCycle::PickGangToCreateMembersOf 0x60F8D0", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [&] { return (int)CPopCycle::PickGangToCreateMembersOf(); }, [&] { return oracle::Fn<int __cdecl()>(0x60F8D0)(); }, "PickGang");
    }, 3000);
    Run("CPopCycle::UpdateDealerStrengths 0x6104B0", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [&] { CPopCycle::UpdateDealerStrengths(); return 0; }, [&] { oracle::Fn<void __cdecl()>(0x6104B0)(); return 0; }, "UpdateDealerStrengths");
    }, 2500);
    Run("CPopCycle::UpdateAreaDodgyness 0x610560", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [&] { CPopCycle::UpdateAreaDodgyness(); return 0; }, [&] { oracle::Fn<void __cdecl()>(0x610560)(); return 0; }, "UpdateAreaDodgyness");
    }, 2500);
    Run("CPopCycle::UpdateIsGangArea 0x6106D0", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [&] { CPopCycle::UpdateIsGangArea(); return 0; }, [&] { oracle::Fn<void __cdecl()>(0x6106D0)(); return 0; }, "UpdateIsGangArea");
    }, 1500);
    Run("CPopCycle::UpdatePercentages 0x610770", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [&] { CPopCycle::UpdatePercentages(); return 0; }, [&] { oracle::Fn<void __cdecl()>(0x610770)(); return 0; }, "UpdatePercentages");
    }, 4000);

    // ---- CCarCtrl: model choice
    Run("CCarCtrl::ChooseBoatModel 0x421970", [&](Rng& r, std::string& d) {
        return Case<int>(r, d, [&] { return CCarCtrl::ChooseBoatModel(); }, [&] { return oracle::Fn<int __cdecl()>(0x421970)(); }, "ChooseBoatModel");
    }, 1500);
    Run("CCarCtrl::ChooseCarModelToLoad 0x421900", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const int grp = a.below(POPCYCLE_TOTAL_CARGROUPS);
        return Case<int>(r, d, [&] { return CCarCtrl::ChooseCarModelToLoad(grp); }, [&] { return oracle::Fn<int __cdecl(int)>(0x421900)(grp); }, "ChooseCarModelToLoad");
    }, 2500);
    Run("CCarCtrl::ChooseGangCarModel 0x421A40", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const eGangID g = (eGangID)a.below(10);
        return Case<int>(r, d, [&] { return (int)CCarCtrl::ChooseGangCarModel(g); }, [&] { return oracle::Fn<int __cdecl(int)>(0x421A40)((int)g); }, "ChooseGangCarModel");
    }, 2500);
    Run("CCarCtrl::ChoosePoliceCarModel 0x421980", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const uint32 ig = a.below(3) ? a.below(2) : a.u32();
        return Case<int>(r, d, [&] { return CCarCtrl::ChoosePoliceCarModel(ig); }, [&] { return oracle::Fn<int __cdecl(uint32)>(0x421980)(ig); }, "ChoosePoliceCarModel");
    }, 4000);
    Run("CCarCtrl::ChooseModel 0x424CE0", [&](Rng& r, std::string& d) {
        using R2 = std::array<int, 2>;
        return Case<R2>(r, d, [&] { int ct = 0x7777; const int m = CCarCtrl::ChooseModel(&ct); return R2{ m, ct }; },
                        [&] { int ct = 0x7777; const int m = oracle::Fn<int __cdecl(int*)>(0x424CE0)(&ct); return R2{ m, ct }; }, "ChooseModel");
    }, 6000);
    Run("CCarCtrl::InitSequence 0x421740", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const int n = 1 + a.below(40);
        using R3 = std::array<int, 3>;
        return Case<R3>(r, d, [&] { CCarCtrl::InitSequence(n); return R3{ (int)CCarCtrl::SequenceElements, (int)CCarCtrl::SequenceRandomOffset, (int)CCarCtrl::bSequenceOtherWay }; },
                        [&] { oracle::Fn<void __cdecl(int)>(0x421740)(n); return R3{ (int)CCarCtrl::SequenceElements, (int)CCarCtrl::SequenceRandomOffset, (int)CCarCtrl::bSequenceOtherWay }; }, "InitSequence");
    }, 1500);
    Run("CCarCtrl::FindSequenceElement 0x421770", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const int i = a.below(40);
        return Case<int>(r, d, [&](Rng&) { if (!CCarCtrl::SequenceElements) CCarCtrl::SequenceElements = 7; }, [&] { return CCarCtrl::FindSequenceElement(i); }, [&] { return oracle::Fn<int __cdecl(int)>(0x421770)(i); }, "FindSequenceElement");
    }, 1500);
    Run("CGangs::ChooseGangPedModel 0x5DE5A0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const eGangID g = (eGangID)a.below(10);
        return Case<int>(r, d, [&] { return (int)CGangs::ChooseGangPedModel(g); }, [&] { return oracle::Fn<int __cdecl(int)>(0x5DE5A0)((int)g); }, "ChooseGangPedModel");
    }, 2500);
    Run("CStreaming::GetDefaultCopCarModel 0x407C50", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const bool ig = a.below(2);
        return Case<int>(r, d, [&] { return (int)CStreaming::GetDefaultCopCarModel(ig); }, [&] { return oracle::Fn<int __cdecl(bool)>(0x407C50)(ig); }, "GetDefaultCopCarModel");
    }, 3000);
    Run("CStreaming::WeAreTryingToPhaseVehicleOut 0x407F80", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const int m = a.below(kNMI);
        return Case<int>(r, d, [&] { const bool b = CStreaming::WeAreTryingToPhaseVehicleOut(m); Hit(b); return (int)b; }, [&] { return (int)oracle::Fn<bool __cdecl(int)>(0x407F80)(m); }, "PhaseOut");
    }, 2000);
    Run("CPopulation::DoesCarGroupHaveModelId 0x406F50", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const int g = a.below(POPCYCLE_TOTAL_CARGROUPS); const int m = a.below(2) ? (int)CPopulation::m_CarGroups[g][a.below(23)] : GenVehId(a);
        return Case<int>(r, d, [&] { const bool b = CPopulation::DoesCarGroupHaveModelId(g, m); Hit(b); return (int)b; }, [&] { return (int)oracle::Fn<bool __cdecl(int, int)>(0x406F50)(g, m); }, "DoesCarGroupHaveModelId");
    }, 1500);
}
