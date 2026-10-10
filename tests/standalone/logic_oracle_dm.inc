// (b) decision makers: CDecision (SetDefault / Set / Add / From / MakeDecision with the RNG), CDecisionSimple::MakeDecision, CDecisionMakerTypes (Add/Remove/Flush/HasAny/AddEventResponse, both MakeDecision overloads),
// CDecisionMakerTypesFileLoader name tables / (Re)Unload, CPedIntelligence::Set/RestorePedDecisionMakerType. The singleton CDecisionMakerTypes is a zeroed heap object wired into the exe's pointer (0xC0B030).
// Not oracle-able: LoadDecisionMaker / LoadEventIndicesFromFile (CFileMgr + CRT stdio + sscanf of the exe: no imports / locks here).
#include "DecisionMakers/DecisionMakerTypes.h"
#include "DecisionMakers/DecisionMakerTypesFileLoader.h"
#include "DecisionMakers/DecisionSimple.h"
#include "PedGroup.h"
#include "PedGroups.h"
#include "PedGroupIntelligence.h"
#include "PedStats.h"
#include "PedIntelligence.h"

LO_STEAL(T_PedCreatedBy, ePedCreatedBy CPed::*, &CPed::m_nCreatedBy)
LO_STEAL(T_GrpDM, eDecisionMakerType CPedGroupIntelligence::*, &CPedGroupIntelligence::m_DecisionMakerType)

namespace ld {
static CDecisionMakerTypes* T = nullptr;
static CPedStat*            Stat = nullptr;
static CPedGroup*           Grp = nullptr;      // a stand-alone fake group (for MakeDecision(group))
static uint8*               Intel = nullptr;    // one CPedIntelligence per pool ped
static bool                 inited = false;
constexpr int               kEvents = DM_TOTAL_NUM_EVENTS;

static void Init() {
    if (inited) return;
    inited = true;
    lw::Init();
    T = (CDecisionMakerTypes*)lw::Z(sizeof(CDecisionMakerTypes));
    StaticRef<CDecisionMakerTypes*>(0xC0B030) = T;
    Stat = (CPedStat*)lw::Z(sizeof(CPedStat));
    Grp = (CPedGroup*)lw::Z(sizeof(CPedGroup));
    Intel = (uint8*)lw::Z(lw::kPeds * sizeof(CPedIntelligence));
}
static CPedIntelligence* IntelOf(int i) { return (CPedIntelligence*)(Intel + i * sizeof(CPedIntelligence)); }
static void WirePeds() { for (int i = 0; i < lw::kPeds; ++i) { lw::PedObj[i]->*Get(T_PedInt{}) = IntelOf(i); lw::PedObj[i]->m_pStats = Stat; } }

static int32 GenTask(Rng& r) {
    switch (r.below(6)) { case 0: return -1; case 1: return 200; case 2: return (int32)r.below(10); default: return (int32)r.below(600); }
}
static void GenDecision(CDecision& d, Rng& r, int fill = -1) {   // fill: number of used choices (-1 = random)
    const int n = fill >= 0 ? fill : (r.below(3) == 0 ? 0 : (int)r.below(7));
    std::memset(&d, 0, sizeof d);
    for (int i = 0; i < CDecision::MAX_NUM_CHOICES; ++i) {
        if (i < n) {
            d.m_Tasks[i] = (eTaskType)GenTask(r);
            for (int j = 0; j < 4; ++j) d.m_Probs[i][j] = r.below(3) == 0 ? 0 : (uint8)r.u32();
            d.m_Bools[i][0] = r.below(2); d.m_Bools[i][1] = r.below(2);
        } else {
            d.m_Tasks[i] = (eTaskType)-1;   // SetDefault state
        }
    }
}
static void GenDM(CDecisionMaker& dm, Rng& r) { for (auto& d : dm.m_aDecisions) GenDecision(d, r); }
static void GenTypes(Rng& r) {
    std::memset(T, 0, sizeof *T);
    T->m_NoOfDecisionMakers = r.below(21);
    for (auto& dm : T->m_DecisionMakers) GenDM(dm, r);
    GenDM(T->m_DefaultRandomPedDecisionMaker, r); GenDM(T->m_DefaultMissionPedDecisionMaker, r); GenDM(T->m_DefaultPlayerPedDecisionMaker, r);
    GenDM(T->m_DefaultRandomPedGroupDecisionMaker, r); GenDM(T->m_DefaultMissionPedGroupDecisionMaker, r);
    for (auto& e : T->m_EventIndices) e = r.below(kEvents);
    for (int i = 0; i < 20; ++i) {
        CDecisionMakerTypes::m_IsActive[i] = r.below(2); CDecisionMakerTypes::m_IsGroupDM[i] = r.below(2);
        CDecisionMakerTypes::ScriptReferenceIndex[i] = (uint16)r.below(5);
    }
}
static int DmFromRng(Rng& r) { switch (r.below(8)) { case 0: return -2; case 1: return -1; default: return (int)r.below(20); } }

struct Mem {   // everything the decision maker functions may write
    std::vector<uint8> t; uint8 st[0x60];
    void Save() { t.resize(sizeof(CDecisionMakerTypes)); std::memcpy(t.data(), T, sizeof(CDecisionMakerTypes)); std::memcpy(st, (void*)0xC0AFE0, sizeof st); }
    bool Same(const Mem& o) const { return SameBlob(t.data(), o.t.data(), t.size()) && SameBlob(st, o.st, sizeof st); }
    void Restore() const { std::memcpy(T, t.data(), t.size()); std::memcpy((void*)0xC0AFE0, st, sizeof st); }
};
// types-level case: random singleton; port fn; exe fn on identical memory (+ rng seed), compare the singleton, the statics and `ret`
template<class PortFn, class ExeFn>
static bool TCase(Rng& r, std::string& d, PortFn&& port, ExeFn&& exe, const char* what) {
    GenTypes(r);
    const int seed = r.u32();
    static Mem m0, m1, m2;
    m0.Save();
    SeedBoth(seed);
    const long long a = port();
    m1.Save(); const uint32 s1 = notsa::g_RandSeed;
    m0.Restore();
    SeedBoth(seed);
    const long long e = exe();
    m2.Save(); const uint32 s2 = g_fakePtd[5];
    const bool ok = m1.Same(m2) && a == e && s1 == s2;
    if (!ok) d = std::string(what) + " ret got " + std::to_string(a) + " exe " + std::to_string(e) + (s1 != s2 ? " rng differs" : "") + DiffDesc(m1.t.data(), m2.t.data(), m1.t.size(), "t");
    return ok;
}
} // namespace ld

static void TestDm() {
    using namespace ld;
    Init();
    auto sub = [](Rng& r) { return Rng(r.u32() * 2654435761ull + 99); };
    static std::array<CDecision, 2> dec;

    // ---- CDecision
    Run("CDecision::SetDefault 0x600530", [&](Rng& r, std::string& d) {
        GenDecision(dec[0], r); std::memcpy(&dec[1], &dec[0], sizeof(CDecision));
        dec[0].SetDefault(); oracle::Fn<void __fastcall(CDecision*, int)>(0x600530)(&dec[1], 0);
        if (SameBlob(&dec[0], &dec[1], sizeof(CDecision))) return true;
        d = "got " + Hex(&dec[0], sizeof(CDecision)) + " exe " + Hex(&dec[1], sizeof(CDecision)); return false;
    }, 500);
    Run("CDecision::Set 0x600570", [&](Rng& r, std::string& d) {
        GenDecision(dec[0], r); std::memcpy(&dec[1], &dec[0], sizeof(CDecision));
        notsa::mdarray<int32, 6> tasks; notsa::mdarray<float, 6, 4> probs; notsa::mdarray<int32, 6, 2> bools; notsa::mdarray<float, 6, 6> facial;
        for (int i = 0; i < 6; ++i) {
            tasks[i] = GenTask(r);
            for (int j = 0; j < 4; ++j) probs[i][j] = r.below(2) ? (float)(int)r.below(256) : GenF(r, r.below(2) ? 300.f : 1e12f);
            bools[i][0] = r.below(3) == 0 ? 0 : (int32)r.u32(); bools[i][1] = r.below(3) == 0 ? 0 : (int32)r.u32();
            for (int j = 0; j < 6; ++j) facial[i][j] = r.f01();
        }
        dec[0].Set(tasks, probs, bools, facial);
        oracle::Fn<void __fastcall(CDecision*, int, int*, float*, int*, float*)>(0x600570)(&dec[1], 0, &tasks[0], &probs[0][0], &bools[0][0], &facial[0][0]);
        if (SameBlob(&dec[0], &dec[1], sizeof(CDecision))) return true;
        d = "got " + Hex(&dec[0], sizeof(CDecision)) + " exe " + Hex(&dec[1], sizeof(CDecision)); return false;
    }, 2500);
    Run("CDecision::Add 0x600600", [&](Rng& r, std::string& d) {
        GenDecision(dec[0], r); if (r.below(3) == 0) { auto& c = dec[0]; for (int i = 0; i < 6; ++i) if (r.below(2)) { c.m_Tasks[i] = (eTaskType)-1; for (auto& p : c.m_Probs[i]) p = 0; c.m_Bools[i][0] = c.m_Bools[i][1] = false; } }
        std::memcpy(&dec[1], &dec[0], sizeof(CDecision));
        float ch[4]; int32 fl[2]; for (auto& c : ch) c = r.below(2) ? (float)(int)r.below(256) : GenF(r, 400.f); fl[0] = r.below(2) ? 0 : (int32)r.u32(); fl[1] = r.below(2) ? 0 : (int32)r.u32();
        const eTaskType t = (eTaskType)GenTask(r);
        dec[0].Add(t, ch, fl);
        oracle::Fn<void __fastcall(CDecision*, int, int, float*, int*)>(0x600600)(&dec[1], 0, (int)t, ch, fl);
        if (SameBlob(&dec[0], &dec[1], sizeof(CDecision))) return true;
        d = "got " + Hex(&dec[0], sizeof(CDecision)) + " exe " + Hex(&dec[1], sizeof(CDecision)); return false;
    }, 2500);
    Run("CDecision::From 0x6006B0", [&](Rng& r, std::string& d) {
        static CDecision src; GenDecision(src, r); GenDecision(dec[0], r); std::memcpy(&dec[1], &dec[0], sizeof(CDecision));
        dec[0].From(src);
        oracle::Fn<void __fastcall(CDecision*, int, const CDecision*)>(0x6006B0)(&dec[1], 0, &src);
        if (SameBlob(&dec[0], &dec[1], sizeof(CDecision))) return true;
        d = "got " + Hex(&dec[0], sizeof(CDecision)) + " exe " + Hex(&dec[1], sizeof(CDecision)); return false;
    }, 500);
    Run("CDecision::MakeDecision 0x6040D0", [&](Rng& r, std::string& d) {
        GenDecision(dec[0], r, r.below(2) ? -1 : 6);
        Rng a = sub(r);
        const int32 src = a.below(4); const bool veh = a.below(2);
        auto pick = [&]() -> eTaskType { return (eTaskType)(a.below(3) == 0 ? -1 : (int)dec[0].m_Tasks[a.below(6)]); };
        const eTaskType av1 = pick(), av2 = pick(), av3 = pick(); const eTaskType seek = a.below(2) ? (eTaskType)-1 : pick();
        int16 t1 = 77, f1 = 77, t2 = 77, f2 = 77;
        const int seed = r.u32();
        SeedBoth(seed); dec[0].MakeDecision(src, veh, av1, av2, av3, seek, t1, f1); const uint32 s1 = notsa::g_RandSeed;
        SeedBoth(seed); oracle::Fn<void __fastcall(CDecision*, int, int32, bool, eTaskType, eTaskType, eTaskType, eTaskType, int16*, int16*)>(0x6040D0)(&dec[0], 0, src, veh, av1, av2, av3, seek, &t2, &f2); const uint32 s2 = g_fakePtd[5];
        Hit(t1 != 200);
        if (t1 == t2 && f1 == f2 && s1 == s2) return true;
        d = "dec " + Hex(&dec[0], sizeof(CDecision)) + " src " + std::to_string(src) + " veh " + std::to_string(veh) + " got " + std::to_string(t1) + "/" + std::to_string(f1) + " exe " + std::to_string(t2) + "/" + std::to_string(f2) + (s1 != s2 ? " rng differs" : ""); return false;
    }, 8000);
    Run("CDecisionSimple::MakeDecision 0x6007A0", [&](Rng& r, std::string& d) {
        CDecisionSimple s{}; s.m_nCount = r.below(7);
        float cum = 0.f;
        for (int i = 0; i < 6; ++i) { s.m_anTasks[i] = (eTaskType)GenTask(r); cum += r.f01() * 0.4f; s.m_afChances[i] = r.below(8) ? std::min(cum, 1.f) : GenF(r, 2.f); }
        const int32 seek = r.below(3) ? (int32)s.m_anTasks[r.below(6)] : (int32)GenTask(r);
        int16 o1 = 5, o2 = 5; int32 i1 = 5, i2 = 5; const int seed = r.u32();
        SeedBoth(seed); s.MakeDecision(seek, o1, i1); const uint32 s1 = notsa::g_RandSeed;
        SeedBoth(seed); oracle::Fn<void __fastcall(CDecisionSimple*, int, int32, int16*, int32*)>(0x6007A0)(&s, 0, seek, &o2, &i2); const uint32 s2 = g_fakePtd[5];
        if (o1 == o2 && i1 == i2 && s1 == s2) return true;
        d = "seek " + std::to_string(seek) + " got " + std::to_string(o1) + "/" + std::to_string(i1) + " exe " + std::to_string(o2) + "/" + std::to_string(i2) + (s1 != s2 ? " rng differs" : ""); return false;
    }, 4000);

    // ---- CDecisionMakerTypes
    Run("CDecisionMakerTypes::GetInstance 0x4684F0", [&](Rng& r, std::string& d) {
        const auto a = CDecisionMakerTypes::GetInstance(); const auto e = oracle::Fn<CDecisionMakerTypes* __cdecl()>(0x4684F0)();
        if (a == e && a == T) return true; d = "differs"; return false;
    }, 50);
    Run("CDecisionMakerTypes::AddDecisionMaker 0x607050", [&](Rng& r, std::string& d) {
        Rng a = sub(r); static CDecisionMaker dm; GenDM(dm, a); const eDecisionTypes ty = (eDecisionTypes)a.below(2); const bool mis = a.below(2);
        return TCase(r, d, [&] { return (long long)T->AddDecisionMaker(&dm, ty, mis); },
                     [&] { return (long long)oracle::Fn<int32 __fastcall(CDecisionMakerTypes*, int, CDecisionMaker*, int, bool)>(0x607050)(T, 0, &dm, (int)ty, mis); }, "Add");
    }, 1500);
    Run("CDecisionMakerTypes::AddEventResponse 0x6044C0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const int idx = a.below(20); const eEventType ev = (eEventType)a.below(96); const eTaskType task = (eTaskType)GenTask(a);
        float ch[4]; int32 fl[2]; for (auto& c : ch) c = (float)(int)a.below(300); fl[0] = a.below(2); fl[1] = a.below(2);
        return TCase(r, d, [&] { T->AddEventResponse(idx, ev, task, ch, fl); return 0LL; },
                     [&] { oracle::Fn<void __fastcall(CDecisionMakerTypes*, int, int32, int, int, float*, int*)>(0x6044C0)(T, 0, idx, (int)ev, (int)task, ch, fl); return 0LL; }, "AddEventResponse");
    }, 1500);
    Run("CDecisionMakerTypes::FlushDecisionMakerEventResponse 0x604490", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const int idx = a.below(20); const eEventType ev = (eEventType)a.below(96);
        return TCase(r, d, [&] { T->FlushDecisionMakerEventResponse(idx, ev); return 0LL; },
                     [&] { oracle::Fn<void __fastcall(CDecisionMakerTypes*, int, int32, int)>(0x604490)(T, 0, idx, (int)ev); return 0LL; }, "Flush");
    }, 1000);
    // ped based: the fake ped pool objects with an intelligence block each
    const auto PrepPed = [](Rng& a) -> CPed* {
        lw::GenPools(a, CVector{});
        WirePeds();
        const int k = a.below(lw::kPeds);
        CPed* ped = lw::PedObj[k];
        auto* in = IntelOf(k);
        in->m_nDecisionMakerType = DmFromRng(a); in->m_nDecisionMakerTypeInGroup = DmFromRng(a);
        ped->*Get(T_PedCreatedBy{}) = a.below(3) == 0 ? PED_MISSION : (ePedCreatedBy)a.below(4);
        return ped;
    };
    Run("CDecisionMakerTypes::MakeDecision(ped) 0x606E70", [&](Rng& r, std::string& d) {
        Rng a = sub(r); CPed* ped = PrepPed(a);
        const eEventType ev = (eEventType)a.below(96); const int32 src = a.below(4); const bool veh = a.below(2), grp = a.below(2);
        const eTaskType av[3] = { (eTaskType)GenTask(a), (eTaskType)GenTask(a), (eTaskType)GenTask(a) }; const eTaskType seek = (eTaskType)(a.below(2) ? -1 : GenTask(a));
        int16 t1 = 1, f1 = 1, t2 = 1, f2 = 1;
        const bool ok = TCase(r, d, [&] { T->MakeDecision(ped, ev, src, veh, av[0], av[1], av[2], seek, grp, t1, f1); return (long long)t1 * 65536 + f1; },
            [&] { oracle::Fn<void __fastcall(CDecisionMakerTypes*, int, CPed*, int, int32, bool, int, int, int, int, bool, int16*, int16*)>(0x606E70)(T, 0, ped, (int)ev, src, veh, (int)av[0], (int)av[1], (int)av[2], (int)seek, grp, &t2, &f2); return (long long)t2 * 65536 + f2; }, "MakeDecision(ped)");
        Hit(t1 != 200);
        return ok;
    }, 6000);
    Run("CDecisionMakerTypes::MakeDecision(group) 0x606F80", [&](Rng& r, std::string& d) {
        Rng a = sub(r);
        Grp->m_bIsMissionGroup = a.below(2); Grp->m_groupIntelligence.*Get(T_GrpDM{}) = (eDecisionMakerType)(a.below(3) == 0 ? -1 : (int)a.below(20));
        const eEventType ev = (eEventType)a.below(96); const int32 src = a.below(4); const bool veh = a.below(2);
        const eTaskType t[4] = { (eTaskType)GenTask(a), (eTaskType)GenTask(a), (eTaskType)GenTask(a), (eTaskType)(a.below(2) ? -1 : GenTask(a)) };
        return TCase(r, d, [&] { return (long long)T->MakeDecision(Grp, ev, src, veh, t[0], t[1], t[2], t[3]); },
            [&] { return (long long)oracle::Fn<int __fastcall(CDecisionMakerTypes*, int, CPedGroup*, int, int32, bool, int, int, int, int)>(0x606F80)(T, 0, Grp, (int)ev, src, veh, (int)t[0], (int)t[1], (int)t[2], (int)t[3]); }, "MakeDecision(group)");
    }, 4000);
    Run("CDecisionMakerTypes::HasAnyEventResponse 0x6042B0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); CPed* ped = PrepPed(a);
        int32 evs[8]; const int n = a.below(9); for (auto& e : evs) e = a.below(96);
        return TCase(r, d, [&] { return (long long)T->HasAnyEventResponse(ped, evs, n); },
                     [&] { return (long long)(oracle::Fn<bool __fastcall(CDecisionMakerTypes*, int, CPed*, const int32*, int32)>(0x6042B0)(T, 0, ped, evs, n) & 1); }, "HasAny");
    }, 3000);
    Run("CDecisionMakerTypes::RemoveDecisionMaker 0x6043A0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); lw::GenPools(a, CVector{}); WirePeds();
        for (int i = 0; i < lw::kPeds; ++i) {
            auto* in = IntelOf(i); in->m_nDecisionMakerType = a.below(3) ? (int)a.below(20) : DmFromRng(a); in->m_nDecisionMakerTypeInGroup = DmFromRng(a);
        }
        Stat->m_nDefaultDecisionMaker = (char)a.below(20);
        for (int i = 0; i < 8; ++i) {
            CPedGroups::ms_activeGroups[i] = a.below(2);
            CPedGroups::ms_groups[i].m_groupIntelligence.*Get(T_GrpDM{}) = (eDecisionMakerType)(a.below(3) == 0 ? -1 : (int)a.below(20));
        }
        const eDecisionTypes dm = (eDecisionTypes)a.below(20);
        constexpr size_t isz = lw::kPeds * sizeof(CPedIntelligence), gsz = 8 * sizeof(CPedGroup);
        static std::vector<uint8> in0(isz), in1(isz), in2(isz), g0(gsz), g1(gsz), g2(gsz);
        std::memcpy(in0.data(), Intel, isz); std::memcpy(g0.data(), &CPedGroups::ms_groups[0], gsz);
        bool ok = TCase(r, d, [&] { T->RemoveDecisionMaker(dm); std::memcpy(in1.data(), Intel, isz); std::memcpy(g1.data(), &CPedGroups::ms_groups[0], gsz);
                                    std::memcpy(Intel, in0.data(), isz); std::memcpy(&CPedGroups::ms_groups[0], g0.data(), gsz); return 0LL; },
                        [&] { oracle::Fn<void __fastcall(CDecisionMakerTypes*, int, int)>(0x6043A0)(T, 0, (int)dm); std::memcpy(in2.data(), Intel, isz); std::memcpy(g2.data(), &CPedGroups::ms_groups[0], gsz); return 0LL; }, "Remove");
        ok = ok && SameBlob(in1.data(), in2.data(), isz) && SameBlob(g1.data(), g2.data(), gsz);
        if (!ok && d.empty()) d = "ped intelligence / group blobs differ (dm " + std::to_string((int)dm) + ")";
        return ok;
    }, 3000);
    Run("CDecisionMakerTypesFileLoader::GetPedDMName 0x600860", [&](Rng& r, std::string& d) {
        char a[32]{}, e[32]{}; const int idx = r.below(6);
        CDecisionMakerTypesFileLoader::GetPedDMName(idx, a); oracle::Fn<void __cdecl(int, char*)>(0x600860)(idx, e);
        if (!std::strcmp(a, e)) return true; d = std::string("idx ") + std::to_string(idx) + " got " + a + " exe " + e; return false;
    }, 50);
    Run("CDecisionMakerTypesFileLoader::GetGrpDMName 0x600880", [&](Rng& r, std::string& d) {
        char a[32]{}, e[32]{};
        CDecisionMakerTypesFileLoader::GetGrpDMName(0, a); oracle::Fn<void __cdecl(int, char*)>(0x600880)(0, e);
        if (!std::strcmp(a, e)) return true; d = std::string("got ") + a + " exe " + e; return false;
    }, 20);
    Run("CDecisionMakerTypesFileLoader::UnloadDecisionMaker 0x607A70", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const eDecisionTypes dm = (eDecisionTypes)a.below(20);
        lw::GenPools(a, CVector{}); WirePeds();
        return TCase(r, d, [&] { CDecisionMakerTypesFileLoader::UnloadDecisionMaker(dm); return 0LL; }, [&] { oracle::Fn<void __cdecl(int)>(0x607A70)((int)dm); return 0LL; }, "Unload");
    }, 1500);
    Run("CDecisionMakerTypesFileLoader::ReStart 0x607D00", [&](Rng& r, std::string& d) {
        Rng a = sub(r); lw::GenPools(a, CVector{}); WirePeds();
        return TCase(r, d, [&] { CDecisionMakerTypesFileLoader::ReStart(); return 0LL; }, [&] { oracle::Fn<void __cdecl()>(0x607D00)(); return 0LL; }, "ReStart");
    }, 600);

    // ---- CPedIntelligence decision maker setters
    {
        struct G { const char* n; unsigned va; int kind; };
        static const G gs[] = { { "SetPedDecisionMakerType", 0x600B50, 0 }, { "SetPedDecisionMakerTypeInGroup", 0x600BB0, 1 }, { "RestorePedDecisionMakerType", 0x600BC0, 2 } };
        for (auto& g : gs)
            Run(std::string("CPedIntelligence::") + g.n + " 0x" + HexVA(g.va), [&](Rng& r, std::string& d) {
                Rng a = sub(r);
                static CPedIntelligence* in = (CPedIntelligence*)lw::Z(sizeof(CPedIntelligence) * 2);
                auto* p = in; auto* q = in + 1;
                std::memset(p, 0, sizeof *p); p->m_nDecisionMakerType = DmFromRng(a); p->m_nDecisionMakerTypeInGroup = DmFromRng(a);
                if (a.below(4) == 0) p->m_nDecisionMakerType = 0;
                p->m_fDmRadius = a.f01() * 20.f; p->m_nDmNumPedsToScan = (uint8)a.u32();
                std::memcpy(q, p, sizeof *p);
                const int32 nt = a.below(4) == 0 ? 0 : DmFromRng(a);
                switch (g.kind) { case 0: p->SetPedDecisionMakerType(nt); break; case 1: p->SetPedDecisionMakerTypeInGroup(nt); break; default: p->RestorePedDecisionMakerType(); }
                if (g.kind == 2) oracle::Fn<void __fastcall(CPedIntelligence*, int)>(g.va)(q, 0); else oracle::Fn<void __fastcall(CPedIntelligence*, int, int32)>(g.va)(q, 0, nt);
                if (SameBlob(p, q, sizeof *p)) return true;
                d = "got " + Hex(p, 0x80) + " exe " + Hex(q, 0x80); return false;
            }, 1500);
    }
}
