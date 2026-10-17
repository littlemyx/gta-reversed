// ORACLE-W1 / peds: CPed state predicates and small mutators (switch tables over ePedState, flag helpers), on a raw 0x800-byte ped buffer filled with garbage (bit-fields random,
// ePedState / ePedType drawn from the real range most of the time). Pointer members the functions dereference (player data) point into a second garbage buffer or are null.
#include "Entity/Ped/Ped.h"
#include "PedStats.h"
#include "PlayerPedData.h"

namespace w1ped {
alignas(16) inline uint8 buf[0x800];
alignas(16) inline uint8 pdata[0x200];
static_assert(sizeof(CPed) <= sizeof buf);
struct Acc : CPed { using CPed::m_pPlayerData; using CPed::m_pIntelligence; using CPed::m_nCreatedBy; };   // protected members
inline Acc* A() { return reinterpret_cast<Acc*>(buf); }
inline CPed* P() { return reinterpret_cast<CPed*>(buf); }
inline CPed* Setup(Rng& r) {
    FillGarbage(r, buf, sizeof buf);
    FillGarbage(r, pdata, sizeof pdata);
    CPed* p = P();
    p->m_nPedState = (ePedState)r.below(80);   // only real states (range 0..79): the exe tables for CanPedReturnToState decode out-of-range values differently and no game code produces them
    p->m_nPedType  = (ePedType)(r.below(10) ? r.below(32) : r.u32());
    p->m_nMoveState = (eMoveState)(r.below(10) ? r.below(8) : r.u32());
    A()->m_pPlayerData = r.below(2) ? reinterpret_cast<CPlayerPedData*>(pdata) : nullptr;
    A()->m_pIntelligence = nullptr;
    p->m_nLookTime = r.below(3) ? r.u32() % 100000u : r.u32();
    p->m_pEntityIgnoredCollision = r.below(2) ? nullptr : reinterpret_cast<CEntity*>(0x1000);
    return p;
}
inline std::string Ctx(const CPed* p) { return "state " + std::to_string((int)p->m_nPedState) + " type " + std::to_string((int)p->m_nPedType) + " move " + std::to_string((int)p->m_nMoveState) + " pdata " + std::to_string(reinterpret_cast<const Acc*>(p)->m_pPlayerData != nullptr); }
}
#define W1_PED_RGS Rg{ (uintptr_t)w1ped::buf, sizeof(CPed) }, Rg{ (uintptr_t)w1ped::pdata, sizeof w1ped::pdata }

#define W1_PED_BOOL(name, va) Run("CPed::" #name " " #va, [](Rng& r, std::string& d) { CPed* p = w1ped::Setup(r); \
    return Both(d, { W1_PED_RGS }, [&] { return RB(p->name()); }, [&] { return RB(oracle::Fn<bool __fastcall(CPed*, int)>(va)(p, 0)); }, w1ped::Ctx(p)); });
#define W1_PED_VOID(name, va) Run("CPed::" #name " " #va, [](Rng& r, std::string& d) { CPed* p = w1ped::Setup(r); \
    return Both(d, { W1_PED_RGS }, [&] { p->name(); return 0ull; }, [&] { oracle::Fn<void __fastcall(CPed*, int)>(va)(p, 0); return 0ull; }, w1ped::Ctx(p)); });

static void TestPeds() {
    W1_PED_BOOL(IsPedInControl, 0x5E3960)
    W1_PED_BOOL(CanBeArrested, 0x5DF060)
    W1_PED_BOOL(CanSetPedState, 0x5DF030)
    W1_PED_BOOL(CanPedReturnToState, 0x5DF000)
    W1_PED_BOOL(CanStrafeOrMouseControl, 0x5DF090)
    W1_PED_BOOL(UseGroundColModel, 0x5DEFE0)
    W1_PED_BOOL(CanUseTorsoWhenLooking, 0x5DED90)
    W1_PED_BOOL(IsAlive, 0x5E0170)
    W1_PED_VOID(ClearAimFlag, 0x5DEF20)
    W1_PED_VOID(SetIdle, 0x5E7980)
    W1_PED_VOID(ClearAll, 0x5E5320)
    Run("CPed::SetWeaponSkill 0x5E3C10", [](Rng& r, std::string& d) {
        CPed* p = w1ped::Setup(r);
        const int wt = r.below(60), sk = r.below(5);
        return Both(d, { W1_PED_RGS }, [&] { p->SetWeaponSkill((eWeaponType)wt, (eWeaponSkill)sk); return 0ull; },
                    [&] { oracle::Fn<void __fastcall(CPed*, int, int, int)>(0x5E3C10)(p, 0, wt, sk); return 0ull; }, w1ped::Ctx(p));
    });
    Run("CPed::SetLookTimer 0x5DF8D0", [](Rng& r, std::string& d) {
        CPed* p = w1ped::Setup(r);
        CTimer::m_snTimeInMilliseconds = r.below(3) ? r.u32() % 100000u : r.u32();
        const uint32 t = r.below(3) ? r.u32() % 5000u : r.u32();
        return Both(d, { W1_PED_RGS, Rg{ 0xB7CB84, 4 } }, [&] { p->SetLookTimer(t); return 0ull; }, [&] { oracle::Fn<void __fastcall(CPed*, int, uint32)>(0x5DF8D0)(p, 0, t); return 0ull; },
                    "look " + std::to_string(p->m_nLookTime) + " now " + std::to_string(CTimer::m_snTimeInMilliseconds) + " t " + std::to_string(t));
    });
    Run("CPed::SetPedStats 0x5DEBC0", [](Rng& r, std::string& d) {
        CPed* p = w1ped::Setup(r);
        CPedStats::ms_apPedStats = reinterpret_cast<CPedStat*>(0x20000000u + (r.u32() & 0xFFFF0));
        const int t = r.below(60);
        return Both(d, { W1_PED_RGS, Rg{ 0xC0BBEC, 4 } }, [&] { p->SetPedStats((ePedStats)t); return 0ull; }, [&] { oracle::Fn<void __fastcall(CPed*, int, int)>(0x5DEBC0)(p, 0, t); return 0ull; }, "type " + std::to_string(t));
    });
}
