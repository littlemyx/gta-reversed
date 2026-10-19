// ORACLE-W3-AUDIO part 5 (included by wave3_oracle_audio.inc): CAEVehicleAudioEntity (bank-slot table, inhibit timers, volume / frequency / skid maths, siren state, engine sound cancel).
// `this` = the garbage fixture (qualified calls); tVehicleParams at kVP, a garbage CVehicle at kVeh, a transmission at kTr.
namespace w3au {
constexpr size_t kVP = 0x8000, kTr = 0x8800, kVeh = 0x9000, kWs = 0x8F00;
static void VehBuild(Rng& r) {
    BuildEnt(r);
    auto* ae = reinterpret_cast<CAEVehicleAudioEntity*>(P(0));
    auto* veh = reinterpret_cast<CVehicle*>(P(kVeh));
    auto* vp = reinterpret_cast<CAEVehicleAudioEntity::tVehicleParams*>(P(kVP));
    ae->m_Entity = veh;
    ae->m_IsInitialized = r.below(5) != 0; ae->m_IsSingleGear = r.below(2); ae->m_AuGear = (uint8_t)r.below(6);
    ae->m_CrzCount = r.below(3) ? r.f01() * 60.f : GenF(r, 500.f, S_NOHUGE); ae->m_bNitroOnLastFrame = r.below(2); ae->m_CurrentNitroRatio = r.below(3) ? r.f01() : GenF(r, 1.5f, S_NOHUGE);
    ae->m_AuSettings.EngineVolumeOffset = r.below(4) ? (r.below(2) ? 0.f : GenF(r, 20.f, S_NOHUGE)) : GenF(r, 30.f, S_NOHUGE);
    ae->m_AuSettings.VehicleAudioType = r.below(3) ? eAEVehicleSoundType::AE_CAR : eAEVehicleSoundType::AE_BMX;
    ae->m_AuSettings.VehicleAudioTypeForName = (eAEVehicleAudioTypeForName)r.below(24);
    ae->m_TimeBeforeAllowAccelerate = r.below(2) ? r.below(100000) : r.u32(); ae->m_TimeBeforeAllowCruise = r.below(2) ? r.below(100000) : r.u32();
    ae->m_FramesAgoACLooped = (int16_t)r.below(14); ae->m_ACPlayPositionThisFrame = (int16_t)(r.below(4) == 0 ? -1 : (int)r.below(1000)); ae->m_ACPlayPositionLastFrame = (int16_t)(r.below(4) == 0 ? -1 : (int)r.below(1000));
    ae->m_HasSiren = r.below(2); ae->m_IsWreckedVehicle = r.below(4) == 0; ae->m_PlayerEngineBank = (eSoundBank)r.below(300);
    for (auto& es : ae->m_EngineSounds) { es.Sound = r.below(3) ? nullptr : Snd((int)r.below(6)); es.EngineSoundType = r.below(12); }
    veh->m_nModelIndex = (uint16_t)(r.below(3) ? 400 + r.below(212) : (r.below(2) ? 457 /*caddy*/ : 497 /*maverick*/)); veh->vehicleFlags.bIsDrowning = r.below(4) == 0; veh->m_pVehicleBeingTowed = r.below(3) == 0 ? veh : nullptr;
    veh->m_bUnderwater = r.below(4) == 0; veh->vehicleFlags.bSirenOrAlarm = r.below(2); veh->m_HornCounter = (uint16_t)r.below(3);
    veh->SetStatus((eEntityStatus)(r.below(3) ? (int)r.below(10) : (int)STATUS_ABANDONED));
    *reinterpret_cast<float*>(P(kTr + 0x2C)) = 0.f;
    vp->Vehicle = veh; vp->Transmission = reinterpret_cast<cTransmission*>(P(kTr)); vp->WheelState = reinterpret_cast<tWheelState*>(P(kWs));
    vp->Speed = r.below(5) ? GenF(r, 1.5f, S_NOHUGE) : GenF(r, 5.f); vp->AbsSpeed = std::fabs(vp->Speed); vp->ZOverSpeed = GenF(r, 0.6f, S_NOHUGE); vp->SpeedRatio = r.below(3) ? r.f01() : GenF(r, 2.f, S_NOHUGE);
    vp->RealRevsRatio = r.below(3) ? r.f01() : GenF(r, 2.f, S_NOHUGE); vp->WheelSpin = r.below(3) ? r.f01() * 12.f : GenF(r, 20.f, S_NOHUGE); vp->ThisAccel = decltype(vp->ThisAccel)((int16_t)r.u32()); vp->ThisBrake = decltype(vp->ThisBrake)((int16_t)(r.below(2) ? 0 : r.u32()));
    vp->IsHandbrakeOn = r.below(4) == 0; vp->NumDriveWheelsOnGround = (uint8_t)r.below(4); vp->RealGear = (uint8_t)r.below(6);
    for (int i = 0; i < 4; ++i) At<int32_t>(kWs + 4 * i) = (int32_t)(r.below(6) == 0 ? (int)r.u32() : (int)r.below(4));
}
} // namespace w3au

static void TestW3Audio5() {
    using namespace w2; using namespace w3au;
#define VA(C) reinterpret_cast<CAEVehicleAudioEntity*>(P(0))
    using VE = CAEVehicleAudioEntity;
    auto VPp = [] { return reinterpret_cast<VE::tVehicleParams*>(P(kVP)); };
    // dummy engine bank slots (statics): bank ids / refcounts
    auto slotBuild = +[](Rng& r) {
        VehBuild(r);
        for (auto& s : VE::s_DummyEngineSlots) { s.BankID = (eSoundBank)(r.below(3) == 0 ? -1 : (int)r.below(300)); s.RefCnt = (int16_t)(r.below(4) == 0 ? (int)(int16_t)r.u32() : (int)r.below(4)); }
        if (g_atweak) g_atweak(r);
    };
    Run("CAEVehicleAudioEntity::RequestBankSlot 0x4F4D10", [&](Rng& r, std::string& d) {
        const auto bank = (eSoundBank)r.below(300);
        g_atweak = [bank](Rng& rr) { if (rr.below(2)) VE::s_DummyEngineSlots[rr.below((uint32_t)VE::s_DummyEngineSlots.size())].BankID = bank; };
        const bool ok = ACase(r, d, [&]() -> uint64_t { return Cv((int)(int16_t)VE::RequestBankSlot(bank)); }, [&]() -> uint64_t { return Cv((int)(int16_t)ExeC<int32_t>(0x4F4D10, (int)bank)); }, slotBuild);
        g_atweak = nullptr; return ok;
    }, 400);
    Run("CAEVehicleAudioEntity::StoppedUsingBankSlot 0x4F4DF0", [&](Rng& r, std::string& d) {
        const auto slot = (eSoundBankSlot)(r.below(5) == 0 ? (int)r.below(80) : (int)(SND_BANK_SLOT_DUMMY_FIRST + (int)r.below(8)));
        return ACase(r, d, [&]() -> uint64_t { VE::StoppedUsingBankSlot(slot); return 0; }, [&]() -> uint64_t { ExeC<void>(0x4F4DF0, (int)slot); return 0; }, slotBuild);
    }, 400);
    Run("CAEVehicleAudioEntity::DoesBankSlotContainThisBank 0x4F4E30", [&](Rng& r, std::string& d) {
        const auto slot = (eSoundBankSlot)(r.below(5) == 0 ? (int)r.below(80) : (int)(SND_BANK_SLOT_DUMMY_FIRST + (int)r.below(8))); const auto bank = (eSoundBank)r.below(300);
        g_atweak = [=](Rng& rr) { if (rr.below(2)) if (auto* s = VE::GetDummyEngineSlot(slot)) s->BankID = bank; };
        const bool ok = ACase(r, d, [&]() -> uint64_t { return VE::DoesBankSlotContainThisBank(slot, bank) ? 1 : 0; }, [&]() -> uint64_t { return ExeC<uint8_t>(0x4F4E30, (int)slot, (int)bank) != 0 ? 1 : 0; }, slotBuild);
        g_atweak = nullptr; return ok;
    }, 400);
    Run("CAEVehicleAudioEntity::DemandBankSlot 0x4F4E60", [&](Rng& r, std::string& d) {
        const auto bank = (eSoundBank)r.below(300);
        return ACase(r, d, [&]() -> uint64_t { return Cv((int)(int16_t)VE::DemandBankSlot(bank)); }, [&]() -> uint64_t { return Cv((int)(int16_t)ExeC<int32_t>(0x4F4E60, (int)bank)); }, slotBuild);
    }, 400);
    Run("CAEVehicleAudioEntity::StaticService 0x4F4EC0", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { VE::StaticService(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x4F4EC0); return 0; }, &VehBuild); }, 20);
    Run("CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio 0x4F4ED0", [&](Rng& r, std::string& d) {
        g_atweak = [](Rng& rr) { VE::s_pVehicleAudioSettingsForRadio = reinterpret_cast<tVehicleAudioSettings*>((uintptr_t)(rr.below(2) ? 0 : rr.u32())); };
        const bool ok = ACase(r, d, [&]() -> uint64_t { return Cv(VE::StaticGetPlayerVehicleAudioSettingsForRadio()); }, [&]() -> uint64_t { return Cv(ExeC<void*>(0x4F4ED0)); }, &VehBuild);
        g_atweak = nullptr; return ok;
    }, 60);
    Run("CAEVehicleAudioEntity::DisableHelicoptors 0x4F4EE0", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { VE::DisableHelicoptors(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x4F4EE0); return 0; }, &VehBuild); }, 20);
    Run("CAEVehicleAudioEntity::EnableHelicoptors 0x4F4EF0", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { VE::EnableHelicoptors(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x4F4EF0); return 0; }, &VehBuild); }, 20);
    Run("CAEVehicleAudioEntity::DisableHelicoptor 0x4F5BF0", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { VA(0)->VE::DisableHelicoptor(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4F5BF0, P(0)); return 0; }, &VehBuild); }, 20);
    Run("CAEVehicleAudioEntity::EnableHelicoptor 0x4F5C00", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { VA(0)->VE::EnableHelicoptor(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4F5C00, P(0)); return 0; }, &VehBuild); }, 20);
    Run("CAEVehicleAudioEntity::GetVehicleTypeForAudio 0x4F4F00", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return Cv((int)(uint8_t)VA(0)->VE::GetVehicleTypeForAudio()); }, [&]() -> uint64_t { return Cv((int)(uint8_t)ExeT<int32_t>(0x4F4F00, P(0))); }, &VehBuild);
    }, 600);
    Run("CAEVehicleAudioEntity::IsAccInhibited 0x4F4F70", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return VA(0)->VE::IsAccInhibited(*VPp()) ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4F4F70, P(0), P(kVP)) != 0 ? 1 : 0; }, &VehBuild);
    }, 600);
    Run("CAEVehicleAudioEntity::IsAccInhibitedBackwards 0x4F4FC0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return VA(0)->VE::IsAccInhibitedBackwards(*VPp()) ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4F4FC0, P(0), P(kVP)) != 0 ? 1 : 0; }, &VehBuild);
    }, 400);
    Run("CAEVehicleAudioEntity::IsAccInhibitedForLowSpeed 0x4F4FF0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return VA(0)->VE::IsAccInhibitedForLowSpeed(*VPp()) ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4F4FF0, P(0), P(kVP)) != 0 ? 1 : 0; }, &VehBuild);
    }, 400);
    Run("CAEVehicleAudioEntity::IsAccInhibitedForTime 0x4F5020", [&](Rng& r, std::string& d) {
        g_atweak = [](Rng& rr) { CTimer::m_snTimeInMilliseconds = rr.below(2) ? At<uint32_t>(offsetof(CAEVehicleAudioEntity, m_TimeBeforeAllowAccelerate)) + (uint32_t)((int)rr.below(3) - 1) : rr.u32(); };
        const bool ok = ACase(r, d, [&]() -> uint64_t { return VA(0)->VE::IsAccInhibitedForTime() ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4F5020, P(0)) != 0 ? 1 : 0; }, &VehBuild);
        g_atweak = nullptr; return ok;
    }, 400);
    Run("CAEVehicleAudioEntity::InhibitAccForTime 0x4F5030", [&](Rng& r, std::string& d) {
        const uint32_t t = r.below(2) ? r.below(10000) : r.u32();
        return ACase(r, d, [&]() -> uint64_t { VA(0)->VE::InhibitAccForTime(t); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4F5030, P(0), t); return 0; }, &VehBuild);
    }, 200);
    Run("CAEVehicleAudioEntity::IsCrzInhibitedForTime 0x4F5050", [&](Rng& r, std::string& d) {
        g_atweak = [](Rng& rr) { CTimer::m_snTimeInMilliseconds = rr.below(2) ? At<uint32_t>(offsetof(CAEVehicleAudioEntity, m_TimeBeforeAllowCruise)) + (uint32_t)((int)rr.below(3) - 1) : rr.u32(); };
        const bool ok = ACase(r, d, [&]() -> uint64_t { return VA(0)->VE::IsCrzInhibitedForTime() ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4F5050, P(0)) != 0 ? 1 : 0; }, &VehBuild);
        g_atweak = nullptr; return ok;
    }, 400);
    Run("CAEVehicleAudioEntity::InhibitCrzForTime 0x4F5060", [&](Rng& r, std::string& d) {
        const uint32_t t = r.below(2) ? r.below(10000) : r.u32();
        return ACase(r, d, [&]() -> uint64_t { VA(0)->VE::InhibitCrzForTime(t); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4F5060, P(0), t); return 0; }, &VehBuild);
    }, 200);
    Run("CAEVehicleAudioEntity::CopHeli 0x4F5C40", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return VA(0)->VE::CopHeli() ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4F5C40, P(0)) != 0 ? 1 : 0; }, &VehBuild);
    }, 300);
    Run("CAEVehicleAudioEntity::JustFinishedAccelerationLoop 0x4F5E50", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return VA(0)->VE::JustFinishedAccelerationLoop() ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4F5E50, P(0)) != 0 ? 1 : 0; }, &VehBuild);
    }, 600);
    // GetVolForPlayerEngineSound 0x4F5D00 / GetFreqForPlayerEngineSound 0x4F8070 read factors (a span = max - min, a count divisor ...) that the exe's CRT start-up precomputes into 0xB6BA34..0xB6BA68 with
    // the small initialisers 0x84CED0 .. 0x84D050 (cdecl, void, no side effects other than those floats); the harness image does not run them, so a row seeds them the same way (at the CRT's PC53)
    auto seedPlayerEngine = +[] {
        unsigned cw = 0; _controlfp_s(&cw, 0, 0); const unsigned oldPc = cw & _MCW_PC; _controlfp_s(&cw, _PC_53, _MCW_PC);
        for (uint32_t a : { 0x84CED0u, 0x84CEF0u, 0x84CF10u, 0x84CF20u, 0x84CF40u, 0x84CF60u, 0x84CF80u, 0x84CFA0u, 0x84CFC0u, 0x84CFE0u, 0x84CFF0u, 0x84D010u, 0x84D030u, 0x84D050u }) ExeC<void>(a);
        _controlfp_s(&cw, oldPc, _MCW_PC);
    };
    static void (*seedFn)() = nullptr; seedFn = seedPlayerEngine;
    auto playerBuild = +[](Rng& r) {
        seedFn();
        VehBuild(r);
        auto* ae = reinterpret_cast<CAEVehicleAudioEntity*>(P(0));
        if (r.below(3) == 0) ae->m_AuSettings.VehicleAudioType = eAEVehicleSoundType::AE_BIKE;
        auto* veh = reinterpret_cast<CVehicle*>(P(kVeh));
        auto* vp = reinterpret_cast<CAEVehicleAudioEntity::tVehicleParams*>(P(kVP));
        *reinterpret_cast<float*>(P(kTr + 0x10)) = r.below(4) ? 0.1f + r.f01() * 1.2f : GenF(r, 2.f, S_NOHUGE);     // gear 1 change-up velocity
        *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(veh) + 0x648) = GenF(r, 1.2f, S_NOHUGE);                // bike lean angle
        (void)vp;
    };
    Run("CAEVehicleAudioEntity::GetVolForPlayerEngineSound 0x4F5D00", [&](Rng& r, std::string& d) {
        const int st = 1 + (int)r.below(5);
        return ACase(r, d, [&]() -> uint64_t { return FB(VA(0)->VE::GetVolForPlayerEngineSound(*VPp(), (VE::eVehicleEngineSoundType)st)); }, [&]() -> uint64_t { return FB(ExeT<float>(0x4F5D00, P(0), P(kVP), st)); }, playerBuild);
    }, 1500);
    Run("CAEVehicleAudioEntity::GetVehicleDriveWheelSkidValue 0x4F5F30", [&](Rng& r, std::string& d) {
        const auto ws = (tWheelState)r.below(4); const float gas = r.below(3) ? r.f01() : GenF(r, 2.f, S_ZERO | S_TINY), spd = GenF(r, 1.5f, S_ZERO | S_TINY);
        return ACase(r, d, [&]() -> uint64_t { return FB(VA(0)->VE::GetVehicleDriveWheelSkidValue(reinterpret_cast<CVehicle*>(P(kVeh)), ws, gas, *reinterpret_cast<cTransmission*>(P(kTr)), spd)); },
            [&]() -> uint64_t { return FB(ExeT<float>(0x4F5F30, P(0), P(kVeh), (int)ws, gas, P(kTr), spd)); }, &VehBuild);
    }, 800);
    Run("CAEVehicleAudioEntity::GetVehicleNonDriveWheelSkidValue 0x4F6000", [&](Rng& r, std::string& d) {
        const auto ws = (tWheelState)r.below(4); const float spd = GenF(r, 1.5f, S_ZERO | S_TINY);
        g_atweak = [](Rng& rr) { VA(0)->m_AuSettings.VehicleAudioType = rr.below(2) ? eAEVehicleSoundType::AE_BMX : eAEVehicleSoundType::AE_CAR; };
        const bool ok = ACase(r, d, [&]() -> uint64_t { return FB(VA(0)->VE::GetVehicleNonDriveWheelSkidValue(reinterpret_cast<CVehicle*>(P(kVeh)), ws, *reinterpret_cast<cTransmission*>(P(kTr)), spd)); },
            [&]() -> uint64_t { return FB(ExeT<float>(0x4F6000, P(0), P(kVeh), (int)ws, P(kTr), spd)); }, &VehBuild);
        g_atweak = nullptr; return ok;
    }, 800);
    Run("CAEVehicleAudioEntity::GetSirenState 0x4F62A0", [&](Rng& r, std::string& d) {
        const bool s0 = r.below(2), f0 = r.below(2);
        return ACase(r, d, [&]() -> uint64_t { bool s = s0, f = f0; VA(0)->VE::GetSirenState(s, f, *VPp()); return (s ? 1u : 0u) | (f ? 2u : 0u); },
            [&]() -> uint64_t { uint8_t s = s0, f = f0; ExeT<void>(0x4F62A0, P(0), &s, &f, P(kVP)); return (s ? 1u : 0u) | (f ? 2u : 0u); }, &VehBuild);
    }, 600);
    Run("CAEVehicleAudioEntity::CancelVehicleEngineSound 0x4F55C0", [&](Rng& r, std::string& d) {
        const int16_t st = (int16_t)(r.below(10) == 0 ? 13 + (int)r.below(3) : (int)r.below(12));
        g_atweak = [](Rng& rr) { CTimer::m_snTimeInMilliseconds = rr.below(2) ? rr.below(100000) : rr.u32(); };
        const bool ok = ACase(r, d, [&]() -> uint64_t { VA(0)->VE::CancelVehicleEngineSound(st); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4F55C0, P(0), (int)st); return 0; }, &VehBuild);
        g_atweak = nullptr; return ok;
    }, 600);
    Run("CAEVehicleAudioEntity::StopGenericEngineSound 0x4F6320", [&](Rng& r, std::string& d) {
        const int16_t st = (int16_t)r.below(12);
        return ACase(r, d, [&]() -> uint64_t { VA(0)->VE::StopGenericEngineSound(st); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4F6320, P(0), (int)st); return 0; }, &VehBuild);
    }, 300);
#undef VA
}
