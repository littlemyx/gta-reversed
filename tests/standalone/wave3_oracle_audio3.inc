// ORACLE-W3-AUDIO part 3 (included by wave3_oracle_audio.inc): the small audio entities (door, explosion, fire, frontend, weather, water cannon, weapon, ped, twin loop, collision list, pedless speech).
// `this` = the garbage fixture g_af (qualified calls: no vptr is read); the CAESound objects the entities point to live at kSnd (6 x 0x80, m_PhysicalEntity null).
namespace w3au {
constexpr size_t kSnd = 0x6000;
static CAESound* Snd(int i) { return reinterpret_cast<CAESound*>(P(kSnd + (size_t)i * 0x80)); }
static CAESound* SndP(Rng& r) { return r.below(3) == 0 ? nullptr : Snd((int)r.below(6)); }
static void BuildEnt(Rng& r) {
    BuildFx(r);
    for (int i = 0; i < 6; ++i) { auto* s = Snd(i); At<uint32_t>(kSnd + (size_t)i * 0x80 + 8) = 0; s->m_Volume = r.below(2) ? GenF(r, 50.f, S_NOHUGE) : s->m_Volume; }
}
// a CAETwinLoopSoundEntity at `off` of the fixture (layout from the header: m_IsInUse +0x88, swap time +0x94, playing-first +0x98, sounds +0xA0)
static void TwinPlausible(Rng& r, size_t off) {
    At<int16_t>(off + 0x88) = (int16_t)(r.below(3) ? r.below(2) : r.u32());
    At<uint32_t>(off + 0x94) = r.below(2) ? r.below(100000) : r.u32();
    At<uint8_t>(off + 0x98) = (uint8_t)r.below(2);
    At<CAESound*>(off + 0xA0) = SndP(r); At<CAESound*>(off + 0xA4) = SndP(r);
}
} // namespace w3au

static void TestW3Audio3() {
    using namespace w2; using namespace w3au;
#define OBJ(C) reinterpret_cast<C*>(P(0))
#define EV(NAME, ADDR, CASES, PCALL, ECALL) \
    Run(NAME " " #ADDR, [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { PCALL; return 0; }, [&]() -> uint64_t { ECALL; return 0; }, &BuildEnt); }, CASES)
#define ER(NAME, ADDR, CASES, PCALL, ECALL) \
    Run(NAME " " #ADDR, [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv(PCALL); }, [&]() -> uint64_t { return Cv(ECALL); }, &BuildEnt); }, CASES)

    // ---- door / explosion / fire / weather / water cannon / weapon: bank loads, cancel-by-owner, field resets
    EV("CAEDoorAudioEntity::Reset", 0x4DC6B0, 60, OBJ(CAEDoorAudioEntity)->CAEDoorAudioEntity::Reset(), ExeT<void>(0x4DC6B0, P(0)));
    EV("CAEDoorAudioEntity::StaticInitialise", 0x5B9A80, 20, CAEDoorAudioEntity::StaticInitialise(), ExeC<void>(0x5B9A80));
    EV("CAEExplosionAudioEntity::StaticInitialise", 0x5B9A60, 20, CAEExplosionAudioEntity::StaticInitialise(), ExeC<void>(0x5B9A60));
    Run("CAEExplosionAudioEntity::UpdateParameters 0x4DCB90", [&](Rng& r, std::string& d) {
        const int16_t pos = (int16_t)(r.below(4) == 0 ? -1 : r.below(3) == 0 ? 0 : (int)r.below(5000));
        return ACase(r, d, [&]() -> uint64_t { OBJ(CAEExplosionAudioEntity)->CAEExplosionAudioEntity::UpdateParameters(Snd(0), pos); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4DCB90, P(0), Snd(0), (int)pos); return 0; }, &BuildEnt);
    }, 300);
    Run("CAEFireAudioEntity::Initialise 0x4DCF20", [&](Rng& r, std::string& d) {
        auto* fx = reinterpret_cast<FxSystem_c*>((uintptr_t)(r.below(2) ? 0 : r.u32()));
        return ACase(r, d, [&]() -> uint64_t { OBJ(CAEFireAudioEntity)->CAEFireAudioEntity::Initialise(fx); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4DCF20, P(0), fx); return 0; }, &BuildEnt);
    }, 60);
    EV("CAEFireAudioEntity::Terminate", 0x4DCF40, 60, OBJ(CAEFireAudioEntity)->CAEFireAudioEntity::Terminate(), ExeT<void>(0x4DCF40, P(0)));
    EV("CAEFireAudioEntity::StaticInitialise", 0x5B9A90, 20, CAEFireAudioEntity::StaticInitialise(), ExeC<void>(0x5B9A90));
    EV("CAEWeatherAudioEntity::StaticInitialise", 0x5B9A70, 20, CAEWeatherAudioEntity::StaticInitialise(), ExeC<void>(0x5B9A70));
    Run("CAEWeatherAudioEntity::StaticReset 0x5052B0", [&](Rng& r, std::string& d) {
        g_atweak = [&](Rng& rr) { for (uintptr_t a : { 0xB6BB18u, 0xB6BBC0u }) { StaticRef<int16_t>(a + 0x88) = (int16_t)(rr.below(3) ? rr.below(2) : rr.u32()); StaticRef<CAESound*>(a + 0xA0) = SndP(rr); StaticRef<CAESound*>(a + 0xA4) = SndP(rr); StaticRef<float>(0x8CC30C) = GenF(rr, 100.f); } };
        const bool ok = ACase(r, d, [&]() -> uint64_t { CAEWeatherAudioEntity::StaticReset(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5052B0); return 0; }, &BuildEnt);
        g_atweak = nullptr; return ok;
    }, 200);
    Run("CAEWaterCannonAudioEntity::UpdateParameters 0x502EF0", [&](Rng& r, std::string& d) {
        const int16_t pos = (int16_t)(r.below(3) == 0 ? -1 : (int)r.below(1000)); const int pick = (int)r.below(6);
        g_atweak = [&](Rng& rr) { for (int i = 0; i < 6; ++i) At<CAESound*>(0x7C + 4 + (size_t)i * 0) = nullptr; (void)rr; };
        g_atweak = nullptr;
        return ACase(r, d, [&]() -> uint64_t { OBJ(CAEWaterCannonAudioEntity)->CAEWaterCannonAudioEntity::UpdateParameters(Snd(pick), pos); return 0; }, [&]() -> uint64_t { ExeT<void>(0x502EF0, P(0), Snd(pick), (int)pos); return 0; },
            +[](Rng& rr) { BuildEnt(rr); for (size_t o = 0x7C; o < 0xB0; o += 4) At<CAESound*>(o) = SndP(rr); });
    }, 200);
    EV("CAEWaterCannonAudioEntity::ClearSplashInfo", 0x502F20, 60, OBJ(CAEWaterCannonAudioEntity)->CAEWaterCannonAudioEntity::ClearSplashInfo(), ExeT<void>(0x502F20, P(0)));
    Run("CAEWaterCannonAudioEntity::SetSplashInfo 0x502F50", [&](Rng& r, std::string& d) {
        const CVector p(GenF(r, 100.f), GenF(r, 100.f), GenF(r, 100.f)); const float m = GenF(r, 10.f);
        return ACase(r, d, [&]() -> uint64_t { OBJ(CAEWaterCannonAudioEntity)->CAEWaterCannonAudioEntity::SetSplashInfo(p, m); return 0; }, [&]() -> uint64_t { ExeT<void>(0x502F50, P(0), p.x, p.y, p.z, m); return 0; }, &BuildEnt);
    }, 200);
    Run("CAEWaterCannonAudioEntity::Initialise 0x503060", [&](Rng& r, std::string& d) {
        auto* wc = reinterpret_cast<CWaterCannon*>((uintptr_t)r.u32());
        return ACase(r, d, [&]() -> uint64_t { OBJ(CAEWaterCannonAudioEntity)->CAEWaterCannonAudioEntity::Initialise(wc); return 0; }, [&]() -> uint64_t { ExeT<void>(0x503060, P(0), wc); return 0; }, &BuildEnt);
    }, 100);
    Run("CAEWaterCannonAudioEntity::Terminate 0x5030A0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { OBJ(CAEWaterCannonAudioEntity)->CAEWaterCannonAudioEntity::Terminate(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x5030A0, P(0)); return 0; },
            +[](Rng& rr) { BuildEnt(rr); for (size_t o = 0x7C; o < 0xB0; o += 4) At<CAESound*>(o) = SndP(rr); });
    }, 200);
    Run("CAEWeaponAudioEntity::Initialise 0x503450", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { OBJ(CAEWeaponAudioEntity)->CAEWeaponAudioEntity::Initialise(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x503450, P(0)); return 0; }, &BuildEng);
    }, 100);
    EV("CAEWeaponAudioEntity::Terminate", 0x503480, 60, OBJ(CAEWeaponAudioEntity)->CAEWeaponAudioEntity::Terminate(), ExeT<void>(0x503480, P(0)));
    EV("CAEWeaponAudioEntity::Reset", 0x503490, 100, OBJ(CAEWeaponAudioEntity)->CAEWeaponAudioEntity::Reset(), ExeT<void>(0x503490, P(0)));
    Run("CAEWeaponAudioEntity::StopFlameThrowerIdleGasLoop 0x5034E0", [&](Rng& r, std::string& d) {
        const size_t off = 0x9C;   // m_FlameThrowerIdleGasLoopSound (private; offset from 0x5034E0)
        g_atweak = [off](Rng& rr) { At<CAESound*>(off) = SndP(rr); };
        const bool ok = ACase(r, d, [&]() -> uint64_t { OBJ(CAEWeaponAudioEntity)->CAEWeaponAudioEntity::StopFlameThrowerIdleGasLoop(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x5034E0, P(0)); return 0; }, &BuildEnt);
        g_atweak = nullptr; return ok;
    }, 100);

    // ---- frontend
    EV("CAEFrontendAudioEntity::Reset", 0x4DD440, 60, OBJ(CAEFrontendAudioEntity)->CAEFrontendAudioEntity::Reset(), ExeT<void>(0x4DD440, P(0)));
    ER("CAEFrontendAudioEntity::IsLoadingTuneActive", 0x4DD470, 60, OBJ(CAEFrontendAudioEntity)->CAEFrontendAudioEntity::IsLoadingTuneActive() ? 1 : 0, ExeT<uint8_t>(0x4DD470, P(0)) != 0 ? 1 : 0);
    Run("CAEFrontendAudioEntity::IsRadioTuneSoundActive 0x4DD480", [&](Rng& r, std::string& d) {
        const size_t o1 = 0x1EC - 2 * 0xA8, o2 = 0x1EC - 0xA8;   // the two twin-loop entities close the object (members are protected)
        g_atweak = [=](Rng& rr) { TwinPlausible(rr, o1); TwinPlausible(rr, o2); CTimer::m_UserPause = rr.below(3) == 0; CTimer::m_CodePause = rr.below(4) == 0; };
        const bool ok = ACase(r, d, [&]() -> uint64_t { return OBJ(CAEFrontendAudioEntity)->CAEFrontendAudioEntity::IsRadioTuneSoundActive() ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4DD480, P(0)) != 0 ? 1 : 0; }, &BuildEnt);
        g_atweak = nullptr; return ok;
    }, 200);

    // ---- twin loop sound entity
    Run("CAETwinLoopSoundEntity::Initialise 0x4F28A0", [&](Rng& r, std::string& d) {
        const auto bank = (eSoundBankSlot)r.below(40); const auto a = (eSoundID)r.below(300), b = (eSoundID)r.below(300);
        auto* parent = reinterpret_cast<CAEAudioEntity*>(P(0x2000)); const uint16_t mn = (uint16_t)r.below(3000), mx = (uint16_t)(mn + r.below(3000));
        const int16_t pa = (int16_t)(r.below(3) == 0 ? -1 : (int)r.below(100)), pb = (int16_t)(r.below(3) == 0 ? -1 : (int)r.below(100));
        g_atweak = [](Rng& rr) { At<int16_t>(0x88) = 0; (void)rr; };   // the port asserts !m_IsInUse
        const bool ok = ACase(r, d, [&]() -> uint64_t { OBJ(CAETwinLoopSoundEntity)->CAETwinLoopSoundEntity::Initialise(bank, a, b, parent, mn, mx, pa, pb); return 0; },
            [&]() -> uint64_t { ExeT<void>(0x4F28A0, P(0), (int)bank, (int)a, (int)b, parent, (int)mn, (int)mx, (int)pa, (int)pb); return 0; }, &BuildEnt);
        g_atweak = nullptr; return ok;
    }, 300);
    Run("CAETwinLoopSoundEntity::StopSoundAndForget 0x4F2960", [&](Rng& r, std::string& d) {
        g_atweak = [](Rng& rr) { TwinPlausible(rr, 0); };
        const bool ok = ACase(r, d, [&]() -> uint64_t { OBJ(CAETwinLoopSoundEntity)->CAETwinLoopSoundEntity::StopSoundAndForget(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4F2960, P(0)); return 0; }, &BuildEnt);
        g_atweak = nullptr; return ok;
    }, 200);
    Run("CAETwinLoopSoundEntity::UpdateTwinLoopSound 0x4F29A0", [&](Rng& r, std::string& d) {
        const CVector p(GenF(r, 100.f, S_NOHUGE), GenF(r, 100.f, S_NOHUGE), GenF(r, 100.f, S_NOHUGE)); const float vol = GenF(r, 40.f), spd = GenF(r, 3.f);
        g_atweak = [](Rng& rr) { TwinPlausible(rr, 0); CamSetup(rr); TimerSetup(rr); };
        const bool ok = ACase(r, d, [&]() -> uint64_t { OBJ(CAETwinLoopSoundEntity)->CAETwinLoopSoundEntity::UpdateTwinLoopSound(p, vol, spd); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4F29A0, P(0), p.x, p.y, p.z, vol, spd); return 0; }, &BuildEnt);
        g_atweak = nullptr; return ok;
    }, 300);
    Run("CAETwinLoopSoundEntity::DoSoundsSwitchThisFrame 0x4F2CA0", [&](Rng& r, std::string& d) {
        g_atweak = [](Rng& rr) { TwinPlausible(rr, 0); CTimer::m_snTimeInMillisecondsPauseMode = rr.below(2) ? At<uint32_t>(0x94) + (uint32_t)((int)rr.below(3) - 1) : rr.u32(); };
        const bool ok = ACase(r, d, [&]() -> uint64_t { return OBJ(CAETwinLoopSoundEntity)->CAETwinLoopSoundEntity::DoSoundsSwitchThisFrame() ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4F2CA0, P(0)) != 0 ? 1 : 0; }, &BuildEnt);
        g_atweak = nullptr; return ok;
    }, 300);

    // ---- collision entity: the 300-entry list
    auto collBuild = +[](Rng& r) {
        BuildEnt(r);
        auto* c = reinterpret_cast<CAECollisionAudioEntity*>(P(0));
        c->m_NumActiveCollisionSounds = (int32_t)r.below(300);
        for (auto& e : c->m_CollisionSoundList) {
            e.EntityA = reinterpret_cast<CEntity*>((uintptr_t)(0x1000 + 0x10 * r.below(6))); e.EntityB = reinterpret_cast<CEntity*>((uintptr_t)(0x1000 + 0x10 * r.below(6)));
            e.Sound = r.below(3) ? nullptr : Snd((int)r.below(6)); e.Status = (eCollisionSoundStatus)r.below(3); e.LoopStopTimeMs = r.below(2) ? r.below(100000) : r.u32();
        }
        CTimer::m_snTimeInMilliseconds = r.below(2) ? r.below(100000) : r.u32();
    };
    Run("CAECollisionAudioEntity::InitialisePostLoading 0x4DA050", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { CAECollisionAudioEntity::InitialisePostLoading(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x4DA050); return 0; }, &BuildEnt); }, 20);
    Run("CAECollisionAudioEntity::Initialise 0x5B9BD0", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { OBJ(CAECollisionAudioEntity)->CAECollisionAudioEntity::Initialise(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x5B9BD0, P(0)); return 0; }, collBuild); }, 60);
    Run("CAECollisionAudioEntity::Service 0x4DA2C0", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { OBJ(CAECollisionAudioEntity)->CAECollisionAudioEntity::Service(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4DA2C0, P(0)); return 0; }, collBuild); }, 200);
    Run("CAECollisionAudioEntity::Reset 0x4DA320", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { OBJ(CAECollisionAudioEntity)->CAECollisionAudioEntity::Reset(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4DA320, P(0)); return 0; }, collBuild); }, 200);
    Run("CAECollisionAudioEntity::GetCollisionSoundStatus 0x4DA830", [&](Rng& r, std::string& d) {
        auto* a = reinterpret_cast<CEntity*>((uintptr_t)(0x1000 + 0x10 * r.below(6))); auto* b = reinterpret_cast<CEntity*>((uintptr_t)(0x1000 + 0x10 * r.below(6)));
        const auto sa = (eSurfaceType)r.below(180), sb = (eSurfaceType)r.below(180);
        return ACase(r, d, [&]() -> uint64_t { int32_t idx = 0x55; const auto st = OBJ(CAECollisionAudioEntity)->CAECollisionAudioEntity::GetCollisionSoundStatus(a, b, sa, sb, idx); return Cv((int)st) * 0x10001ull + (uint32_t)idx; },
            [&]() -> uint64_t { int32_t idx = 0x55; const auto st = ExeT<int32_t>(0x4DA830, P(0), a, b, (int)sa, (int)sb, &idx); return Cv(st) * 0x10001ull + (uint32_t)idx; }, collBuild);
    }, 300);
    Run("CAECollisionAudioEntity::AddCollisionSoundToList 0x4DAAC0", [&](Rng& r, std::string& d) {
        auto* a = reinterpret_cast<CEntity*>((uintptr_t)(0x1000 + 0x10 * r.below(6))); auto* b = reinterpret_cast<CEntity*>((uintptr_t)(0x1000 + 0x10 * r.below(6)));
        const auto sa = (eSurfaceType)r.below(180), sb = (eSurfaceType)r.below(180); CAESound* snd = Snd((int)r.below(6)); const auto st = (eCollisionSoundStatus)r.below(3);
        return ACase(r, d, [&]() -> uint64_t { OBJ(CAECollisionAudioEntity)->CAECollisionAudioEntity::AddCollisionSoundToList(a, b, sa, sb, snd, st); return 0; },
            [&]() -> uint64_t { ExeT<void>(0x4DAAC0, P(0), a, b, (int)sa, (int)sb, snd, (int)st); return 0; }, collBuild);
    }, 300);
#undef OBJ
#undef EV
#undef ER
}
