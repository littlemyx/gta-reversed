// (h) VT-OVERRIDES: the individual cheat functions (the table entries of CCheat::m_aCheatFunctions) that need neither streaming nor the clothes system, port vs the exe's machine code.
// Why: the port's Funhouse / Ninja / VillagePeople cheats had their ON and OFF branches swapped, Dusk set the clock when switched OFF, Predator spawned a vehicle where the exe's function is a bare `ret`,
// NotWanted stored an extra flag - none of it was covered (the oracle only drove the dispatch table through recorders). Fixture: the wanted world of logic_oracle_wanted.inc (fake player ped / CPlayerInfo / pools,
// random CWanted, random statistics, random cheat flags); each case: port -> snapshot -> restore -> exe -> snapshot -> compare (CWanted, statistics, CCheat flags, clock, time scale, weather statics, post-effect flag,
// CPopulation flag, the player's CPlayerInfo / ped / ped data). Cheats whose ON branch streams models or rebuilds the player (weapon sets, outfits, ped waves, vehicles) are covered by their OFF branches only.
#include "PostEffects.h"
#include "Population.h"

namespace cf {
struct Extra {
    Snap cheat{ 0x969110, 0x80 }, clock{ 0xB70150, 4 }, ts{ 0xB7CB64, 4 }, wthr{ 0xC812F0, 0x48 }, haze{ 0xC402BA, 1 }, pop{ 0xC0FCB3, 1 };
    std::unique_ptr<Snap> ped, pdata;
    void Init() { if (!ped) { ped = std::make_unique<Snap>((uintptr_t)lw::Pl, sizeof(CPlayerPed)); pdata = std::make_unique<Snap>((uintptr_t)&CWorld::Players[0].m_PlayerData, sizeof(CPlayerPedData)); } }
    void Save()          { Init(); cheat.Save(); clock.Save(); ts.Save(); wthr.Save(); haze.Save(); pop.Save(); ped->Save(); pdata->Save(); }
    void Restore() const { cheat.Restore(); clock.Restore(); ts.Restore(); wthr.Restore(); haze.Restore(); pop.Restore(); ped->Restore(); pdata->Restore(); }
    bool Same(const Extra& o) const { return cheat.SameAs(o.cheat) && clock.SameAs(o.clock) && ts.SameAs(o.ts) && wthr.SameAs(o.wthr) && haze.SameAs(o.haze) && pop.SameAs(o.pop) && ped->SameAs(*o.ped) && pdata->SameAs(*o.pdata); }
    std::string Diff(const Extra& o) const {
        return DiffDesc(cheat.b.data(), o.cheat.b.data(), 0x80, "cheat") + DiffDesc(clock.b.data(), o.clock.b.data(), 4, "clock") + DiffDesc(ts.b.data(), o.ts.b.data(), 4, "ts") + DiffDesc(wthr.b.data(), o.wthr.b.data(), 0x48, "wthr")
             + DiffDesc(haze.b.data(), o.haze.b.data(), 1, "haze") + DiffDesc(pop.b.data(), o.pop.b.data(), 1, "pop") + DiffDesc(ped->b.data(), o.ped->b.data(), ped->b.size(), "ped") + DiffDesc(pdata->b.data(), o.pdata->b.data(), pdata->b.size(), "pdata");
    }
};
}   // namespace cf

static void TestCheatFns() {
    using namespace cf;
    // `onFlag` >= 0: that cheat flag is forced ON first, so that the toggle switches it OFF (the branches that need no streaming)
    const auto Row = [&](const char* name, void (*port)(), uint32 exeAddr, int onFlag = -1, int onFlag2 = -1) {
        Run(name, [=](Rng& r, std::string& d) {
            lw::Init();
            lw::GenWorld(r);
            lw::RandomStats(r);
            lw::GenPools(r, lw::Pl->m_placement.m_vPosn);
            lw::GenWanted(*lw::W, r);
            lw::Pl->bInVehicle = false; lw::Pl->m_pVehicle = nullptr;   // the fake vehicles have no vtable: Health / Fix would call through a null vptr in both implementations
            for (auto& b : CCheat::m_aCheatsActive) b = r.below(3) == 0;
            if (onFlag >= 0) CCheat::m_aCheatsActive[onFlag] = true;
            if (onFlag2 >= 0) CCheat::m_aCheatsActive[onFlag2] = true;
            CCheat::m_bHasPlayerCheated = r.below(2);
            CClock::ms_nGameClockHours = (uint8)r.below(24); CClock::ms_nGameClockMinutes = (uint8)r.below(60); CClock::ms_nGameClockSeconds = (uint16)r.below(60);
            static const float tss[] = { 0.125f, 0.25f, 0.5f, 1.f, 2.f, 4.f, 8.f };
            CTimer::ms_fTimeScale = tss[r.below(7)];
            CPostEffects::m_bHeatHazeFX = r.below(2);
            CPopulation::m_bOnlyCreateRandomGangMembers = r.below(2);
            CWorld::Players[0].m_nMoney = (int32)r.u32() % 100000; CWorld::Players[0].m_nMaxHealth = (uint8)(100 + r.below(100)); CWorld::Players[0].m_nMaxArmour = (uint8)r.below(100);
            lw::Pl->m_fHealth = GenF(r, 200.f); lw::Pl->m_fArmour = GenF(r, 100.f);
            const int seed = (int)r.u32();
            static std::vector<uint8> w0(sizeof(CWanted)), w1(sizeof(CWanted)), w2(sizeof(CWanted));
            static lw::Statics s0, s1, s2;
            static Extra x0, x1, x2;
            std::memcpy(w0.data(), lw::W, sizeof(CWanted)); s0.Save(); x0.Save();
            SeedBoth(seed); g_audioEv.clear(); port();
            const auto ev1 = g_audioEv;
            std::memcpy(w1.data(), lw::W, sizeof(CWanted)); s1.Save(); x1.Save();
            std::memcpy(lw::W, w0.data(), sizeof(CWanted)); s0.Restore(); x0.Restore();
            SeedBoth(seed); g_audioEv.clear(); oracle::Fn<void __cdecl()>(exeAddr)();
            const auto ev2 = g_audioEv;
            std::memcpy(w2.data(), lw::W, sizeof(CWanted)); s2.Save(); x2.Save();
            const bool ok = SameBlob(w1.data(), w2.data(), sizeof(CWanted)) && s1.Same(s2) && x1.Same(x2) && ev1 == ev2;
            if (!ok) { d = std::string(name) + DiffDesc(w1.data(), w2.data(), sizeof(CWanted), "w") + x1.Diff(x2) + DiffDesc(s1.stats.b.data(), s2.stats.b.data(), s1.stats.b.size(), "stats") + " events " + std::to_string(ev1.size()) + "/" + std::to_string(ev2.size()); }
            return ok;
        }, 400);
    };
    Row("CCheat::DuskCheat 0x439540",               &CCheat::DuskCheat,               0x439540);
    Row("CCheat::MidnightCheat 0x439510",           &CCheat::MidnightCheat,           0x439510);
    Row("CCheat::FastTimeCheat 0x438F90",           &CCheat::FastTimeCheat,           0x438F90);
    Row("CCheat::SlowTimeCheat 0x438FC0",           &CCheat::SlowTimeCheat,           0x438FC0);
    Row("CCheat::NotWantedCheat 0x4396C0",          &CCheat::NotWantedCheat,          0x4396C0);
    Row("CCheat::WantedCheat 0x4396F0",             &CCheat::WantedCheat,             0x4396F0);
    Row("CCheat::WantedLevelUpCheat 0x438E90",      &CCheat::WantedLevelUpCheat,      0x438E90);
    Row("CCheat::WantedLevelDownCheat 0x438F20",    &CCheat::WantedLevelDownCheat,    0x438F20);
    Row("CCheat::RiotCheat 0x439710",               &CCheat::RiotCheat,               0x439710);
    Row("CCheat::PinkCarsCheat 0x4390D0",           &CCheat::PinkCarsCheat,           0x4390D0);
    Row("CCheat::BlackCarsCheat 0x4390F0",          &CCheat::BlackCarsCheat,          0x4390F0);
    Row("CCheat::PredatorCheat 0x4395A0",           &CCheat::PredatorCheat,           0x4395A0);
    Row("CCheat::StaminaCheat 0x439930",            &CCheat::StaminaCheat,            0x439930);
    Row("CCheat::WeaponSkillsCheat 0x439940",       &CCheat::WeaponSkillsCheat,       0x439940);
    Row("CCheat::VehicleSkillsCheat 0x4399D0",      &CCheat::VehicleSkillsCheat,      0x4399D0);
    Row("CCheat::GangLandCheat 0x4393D0",           &CCheat::GangLandCheat,           0x4393D0);
    Row("CCheat::HealthCheat 0x438D60",             &CCheat::HealthCheat,             0x438D60);
    Row("CCheat::MoneyArmourHealthCheat 0x438E40",  &CCheat::MoneyArmourHealthCheat,  0x438E40);
    Row("CCheat::SunnyWeatherCheat 0x438F40",       &CCheat::SunnyWeatherCheat,       0x438F40);
    Row("CCheat::ExtraSunnyWeatherCheat 0x438F50",  &CCheat::ExtraSunnyWeatherCheat,  0x438F50);
    Row("CCheat::CloudyWeatherCheat 0x438F60",      &CCheat::CloudyWeatherCheat,      0x438F60);
    Row("CCheat::RainyWeatherCheat 0x438F70",       &CCheat::RainyWeatherCheat,       0x438F70);
    Row("CCheat::FoggyWeatherCheat 0x438F80",       &CCheat::FoggyWeatherCheat,       0x438F80);
    Row("CCheat::StormCheat 0x439570",              &CCheat::StormCheat,              0x439570);
    Row("CCheat::SandstormCheat 0x439590",          &CCheat::SandstormCheat,          0x439590);
    // OFF branches (the flag is ON, so the toggle switches it off): Funhouse / Ninja / VillagePeople swapped them with the streaming ON branches
    Row("CCheat::FunhouseCheat 0x439720 (switch OFF)",           &CCheat::FunhouseCheat,           0x439720, CHEAT_FUNHOUSE_THEME);
    Row("CCheat::NinjaCheat 0x439E50 (switch OFF)",              &CCheat::NinjaCheat,              0x439E50, CHEAT_NINJA_THEME);
    Row("CCheat::AdrenalineCheat 0x439880 (switch OFF)",         &CCheat::AdrenalineCheat,         0x439880, CHEAT_ADRENALINE_MODE);
    Row("CCheat::VillagePeopleCheat 0x439DD0 (switch OFF)",      &CCheat::VillagePeopleCheat,      0x439DD0, CHEAT_PEDS_ATTACK_YOU_WITH_ROCKETS, CHEAT_HAVE_ABOUNTY_ON_YOUR_HEAD);
    Row("CCheat::EverybodyAttacksPlayerCheat 0x439C70 (switch OFF)", &CCheat::EverybodyAttacksPlayerCheat, 0x439C70, CHEAT_HAVE_ABOUNTY_ON_YOUR_HEAD);
}
