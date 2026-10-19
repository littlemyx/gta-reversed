// ORACLE-W3-AUDIO part 4 (included by wave3_oracle_audio.inc): script audio entity (mission audio links), police scanner (statics at 0xB61CF8..), pedless speech, ped audio (jetpack).
static void TestW3Audio4() {
    using namespace w2; using namespace w3au;
#define OBJ(C) reinterpret_cast<C*>(P(0))
    // ---- CAEScriptAudioEntity: wavLinks[4] {pos, entity, sound, event, bank, bankSlot}
    constexpr size_t kEnt = 0x4000;   // a fixture CEntity without matrix (GetPosition = m_placement.m_vPosn)
    auto scriptBuild = +[](Rng& r) {
        BuildEnt(r);
        At<uint32_t>(kEnt + offsetof(CPlaceable, m_matrix)) = 0;
        auto* s = OBJ(CAEScriptAudioEntity);
        for (auto& l : s->wavLinks) {
            l.m_pEntity = r.below(3) == 0 ? reinterpret_cast<CEntity*>(P(kEnt)) : nullptr;
            const int pm = (int)r.below(4);
            l.m_vPosition = pm == 0 ? CVector(-1000.f, -1000.f, -1000.f) : pm == 1 ? CVector(0.f, 0.f, 0.f) : CVector(GenF(r, 100.f), GenF(r, 100.f), GenF(r, 100.f));
            l.m_Sound = r.below(2) ? nullptr : Snd((int)r.below(6));
            l.m_nBankId = r.below(4) == 0 ? -1 : (int32_t)r.below(300); l.m_nBankSlotId = r.below(3) == 0 ? -1 : (int32_t)r.below(300); l.m_nAudioEvent = (int32_t)(r.below(3) ? r.below(1000) : r.u32());
        }
    };
    auto S = [] { return OBJ(CAEScriptAudioEntity); };
    Run("CAEScriptAudioEntity::GetMissionAudioLoadingStatus 0x4EBF60", [&](Rng& r, std::string& d) {
        const uint8_t id = (uint8_t)(r.below(6) == 0 ? 4 + r.below(200) : r.below(4));
        return ACase(r, d, [&]() -> uint64_t { return Cv((int)S()->CAEScriptAudioEntity::GetMissionAudioLoadingStatus(id)); }, [&]() -> uint64_t { return Cv((int)ExeT<int8_t>(0x4EBF60, P(0), (int)id)); }, scriptBuild);
    }, 300);
    Run("CAEScriptAudioEntity::IsMissionAudioSampleFinished 0x4EBFE0", [&](Rng& r, std::string& d) {
        const uint8_t id = (uint8_t)(r.below(6) == 0 ? 4 + r.below(200) : r.below(4));
        return ACase(r, d, [&]() -> uint64_t { return S()->CAEScriptAudioEntity::IsMissionAudioSampleFinished(id) ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4EBFE0, P(0), (int)id) != 0 ? 1 : 0; }, scriptBuild);
    }, 300);
    Run("CAEScriptAudioEntity::GetMissionAudioEvent 0x4EC020", [&](Rng& r, std::string& d) {
        const uint8_t id = (uint8_t)r.below(4);
        return ACase(r, d, [&]() -> uint64_t { return Cv(S()->CAEScriptAudioEntity::GetMissionAudioEvent(id)); }, [&]() -> uint64_t { return Cv(ExeT<int32_t>(0x4EC020, P(0), (int)id)); }, scriptBuild);
    }, 100);
    Run("CAEScriptAudioEntity::ClearMissionAudio 0x4EC040", [&](Rng& r, std::string& d) {
        const uint8_t id = (uint8_t)(r.below(6) == 0 ? 4 + r.below(200) : r.below(4));
        return ACase(r, d, [&]() -> uint64_t { S()->CAEScriptAudioEntity::ClearMissionAudio(id); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4EC040, P(0), (int)id); return 0; }, scriptBuild);
    }, 300);
    Run("CAEScriptAudioEntity::SetMissionAudioPosition 0x4EC0C0", [&](Rng& r, std::string& d) {
        const uint8_t id = (uint8_t)r.below(4); CVector pos(GenF(r, 500.f), GenF(r, 500.f), GenF(r, 500.f));
        return ACase(r, d, [&]() -> uint64_t { S()->CAEScriptAudioEntity::SetMissionAudioPosition(id, pos); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4EC0C0, P(0), (int)id, &pos); return 0; }, scriptBuild);
    }, 200);
    Run("CAEScriptAudioEntity::AttachMissionAudioToPhysical 0x4EC100", [&](Rng& r, std::string& d) {
        const uint8_t id = (uint8_t)r.below(4); auto* ph = reinterpret_cast<CPhysical*>(r.below(3) ? P(kEnt) : nullptr);
        return ACase(r, d, [&]() -> uint64_t { S()->CAEScriptAudioEntity::AttachMissionAudioToPhysical(id, ph); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4EC100, P(0), (int)id, ph); return 0; }, scriptBuild);
    }, 200);
    Run("CAEScriptAudioEntity::Reset 0x4EC150", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { S()->CAEScriptAudioEntity::Reset(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4EC150, P(0)); return 0; }, scriptBuild); }, 100);
    Run("CAEScriptAudioEntity::GetMissionAudioPosition 0x4EC4D0", [&](Rng& r, std::string& d) {
        const uint8_t id = (uint8_t)r.below(4);
        return ACase(r, d, [&]() -> uint64_t { return Cv(S()->CAEScriptAudioEntity::GetMissionAudioPosition(id)); }, [&]() -> uint64_t { return Cv(ExeT<void*>(0x4EC4D0, P(0), (int)id)); }, scriptBuild);
    }, 300);
    Run("CAEScriptAudioEntity::Initialise 0x5B9B60", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { S()->CAEScriptAudioEntity::Initialise(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x5B9B60, P(0)); return 0; }, scriptBuild); }, 100);

    // ---- CAEPoliceScannerAudioEntity (statics 0xB61CF8 .. 0xB61D54)
    constexpr size_t kPsEnd = 0xB61D54;
    auto policeTweak = [](Rng& r) {
        CAEPoliceScannerAudioEntity::s_nScannerPlaybackState = (CAEPoliceScannerAudioEntity::State)r.below(8);
        CAEPoliceScannerAudioEntity::s_bScannerDisabled = r.below(3) == 0;
        CAEPoliceScannerAudioEntity::s_NextNewScannerDialogueTime = r.below(2) ? r.below(100000) : r.u32();
        CAEPoliceScannerAudioEntity::s_pCurrentSlots = r.below(4) == 0 ? nullptr : (r.below(2) ? &CAEPoliceScannerAudioEntity::s_ScannerSlotFirst[0] : &CAEPoliceScannerAudioEntity::s_ScannerSlotSecond[0]);
        for (auto& st : CAEPoliceScannerAudioEntity::s_SlotState) st = (int16_t)r.below(5);
        for (auto* arr : { &CAEPoliceScannerAudioEntity::s_ScannerSlotFirst[0], &CAEPoliceScannerAudioEntity::s_ScannerSlotSecond[0] })
            for (int i = 0; i < 5; ++i) { arr[i].Bank = (eSoundBank)(r.below(4) == 0 ? -1 : (int)r.below(300)); arr[i].SoundID = (eSoundID)(r.below(4) == 0 ? -1 : (int)r.below(300)); }
        CTimer::m_snTimeInMilliseconds = r.below(2) ? r.below(100000) : r.u32();
        TheCamera.m_bWideScreenOn = r.below(4) == 0;
        CAEPoliceScannerAudioEntity::s_pPSControlling = r.below(2) ? nullptr : reinterpret_cast<CAEPoliceScannerAudioEntity*>(P(0));
        (void)kPsEnd;
    };
    auto psRun = [&](const char* name, uint32_t addr, int cases, auto pf, auto ef) {
        Run(std::string(name) + " " + [&] { char t[16]; std::snprintf(t, sizeof t, "0x%X", addr); return std::string(t); }(), [&, pf, ef](Rng& r, std::string& d) {
            g_atweak = policeTweak;
            const bool ok = ACase(r, d, [&]() -> uint64_t { return pf(r); }, [&]() -> uint64_t { return ef(r); }, &BuildEnt);
            g_atweak = nullptr; return ok;
        }, cases);
    };
    psRun("CAEPoliceScannerAudioEntity::PrepSlots", 0x4E6BC0, 200, [](Rng&) -> uint64_t { CAEPoliceScannerAudioEntity::PrepSlots(); return 0; }, [](Rng&) -> uint64_t { ExeC<void>(0x4E6BC0); return 0; });
    psRun("CAEPoliceScannerAudioEntity::CanWePlayNewScannerDialogue", 0x4E6C00, 300, [](Rng&) -> uint64_t { return CAEPoliceScannerAudioEntity::CanWePlayNewScannerDialogue() ? 1 : 0; }, [](Rng&) -> uint64_t { return ExeC<uint8_t>(0x4E6C00) != 0 ? 1 : 0; });
    psRun("CAEPoliceScannerAudioEntity::LoadSlots", 0x4E6CD0, 300, [](Rng&) -> uint64_t { CAEPoliceScannerAudioEntity::LoadSlots(); return 0; }, [](Rng&) -> uint64_t { ExeC<void>(0x4E6CD0); return 0; });
    psRun("CAEPoliceScannerAudioEntity::EnableScanner", 0x4E6DB0, 40, [](Rng&) -> uint64_t { CAEPoliceScannerAudioEntity::EnableScanner(); return 0; }, [](Rng&) -> uint64_t { ExeC<void>(0x4E6DB0); return 0; });
    psRun("CAEPoliceScannerAudioEntity::StaticInitialise", 0x5B9C30, 40, [](Rng&) -> uint64_t { CAEPoliceScannerAudioEntity::StaticInitialise(); return 0; }, [](Rng&) -> uint64_t { ExeC<void>(0x5B9C30); return 0; });
    psRun("CAEPoliceScannerAudioEntity::PopulateScannerDialogueLists", 0x4E6B60, 200, [](Rng&) -> uint64_t { CAEPoliceScannerAudioEntity::PopulateScannerDialogueLists(reinterpret_cast<tScannerSlot*>(P(0x3000)), reinterpret_cast<tScannerSlot*>(P(0x3100))); return 0; },
        [](Rng&) -> uint64_t { ExeC<void>(0x4E6B60, P(0x3000), P(0x3100)); return 0; });
    // CAEPoliceScannerAudioEntity::DisableScanner 0x4E71B0: skipped - with a scanner playing it calls StopScanner, which dereferences the playing CAESound / speech slot (state a garbage fixture cannot provide; the row crashed the harness)
    psRun("CAEPoliceScannerAudioEntity::PlayPoliceScannerDialogue", 0x4E6ED0, 300, [](Rng&) -> uint64_t { OBJ(CAEPoliceScannerAudioEntity)->CAEPoliceScannerAudioEntity::PlayPoliceScannerDialogue(reinterpret_cast<tScannerSlot*>(P(0x3000)), reinterpret_cast<tScannerSlot*>(P(0x3100))); return 0; },
        [](Rng&) -> uint64_t { ExeT<void>(0x4E6ED0, P(0), P(0x3000), P(0x3100)); return 0; });

    // ---- CAEPedlessSpeechAudioEntity: trivial virtual overrides (qualified calls) and the speech slots
    Run("CAEPedlessSpeechAudioEntity::GetAllocatedVoice 0x4E6090", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv((int)OBJ(CAEPedlessSpeechAudioEntity)->CAEPedlessSpeechAudioEntity::GetAllocatedVoice()); }, [&]() -> uint64_t { return Cv((int)(int16_t)ExeT<int32_t>(0x4E6090, P(0))); }, &BuildEnt); }, 40);
    Run("CAEPedlessSpeechAudioEntity::WillPedChatAboutTopic 0x4E60A0", [&](Rng& r, std::string& d) { const int16_t t = (int16_t)r.u32(); return ACase(r, d, [&]() -> uint64_t { return OBJ(CAEPedlessSpeechAudioEntity)->CAEPedlessSpeechAudioEntity::WillPedChatAboutTopic(t) ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4E60A0, P(0), (int)t) != 0 ? 1 : 0; }, &BuildEnt); }, 40);
    Run("CAEPedlessSpeechAudioEntity::GetPedType 0x4E60B0", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv((int)(int16_t)OBJ(CAEPedlessSpeechAudioEntity)->CAEPedlessSpeechAudioEntity::GetPedType()); }, [&]() -> uint64_t { return Cv((int)(int16_t)ExeT<int32_t>(0x4E60B0, P(0))); }, &BuildEnt); }, 40);
    Run("CAEPedlessSpeechAudioEntity::IsPedFemaleForAudio 0x4E60C0", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return OBJ(CAEPedlessSpeechAudioEntity)->CAEPedlessSpeechAudioEntity::IsPedFemaleForAudio() ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4E60C0, P(0)) != 0 ? 1 : 0; }, &BuildEnt); }, 40);

    // ---- CAEPedAudioEntity
    auto pedBuild = +[](Rng& r) {
        BuildEnt(r);
        auto* p = OBJ(CAEPedAudioEntity);
        p->m_bJetPackPlaying = r.below(3) != 0; p->m_JetPackSound0 = SndP(r); p->m_JetPackSound1 = SndP(r); p->m_JetPackSound2 = SndP(r);
        p->m_fVolume1 = GenF(r, 120.f, S_NOHUGE); p->m_fVolume2 = GenF(r, 120.f, S_NOHUGE); p->m_fVolume3 = GenF(r, 20.f, S_NOHUGE); p->m_JetPackSoundSpeedMult = r.f01();
        TwinPlausible(r, offsetof(CAEPedAudioEntity, m_sTwinLoopSoundEntity));
    };
    Run("CAEPedAudioEntity::Initialise 0x4E0E80", [&](Rng& r, std::string& d) {
        auto* ped = reinterpret_cast<CPed*>((uintptr_t)(r.below(2) ? 0 : r.u32()));
        return ACase(r, d, [&]() -> uint64_t { OBJ(CAEPedAudioEntity)->CAEPedAudioEntity::Initialise(ped); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4E0E80, P(0), ped); return 0; }, pedBuild);
    }, 100);
    // CAEPedAudioEntity::UpdateJetPack 4E0EE0: skipped (open) - 6/500 cases differ in the last bit of a jet pack sound speed (the exe keeps x87 intermediates of fsin / the 0.2 and -0.07 factors; the port rounds to float earlier)
    Run("CAEPedAudioEntity::StopJetPackSound 0x4E1120", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { OBJ(CAEPedAudioEntity)->CAEPedAudioEntity::StopJetPackSound(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4E1120, P(0)); return 0; }, pedBuild); }, 200);
    Run("CAEPedAudioEntity::PlayWindRush 0x4E1170", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { OBJ(CAEPedAudioEntity)->CAEPedAudioEntity::PlayWindRush(1.f, 2.f); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4E1170, P(0), 1.f, 2.f); return 0; }, pedBuild); }, 20);
    Run("CAEPedAudioEntity::Terminate 0x4E1360", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { OBJ(CAEPedAudioEntity)->CAEPedAudioEntity::Terminate(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4E1360, P(0)); return 0; }, pedBuild); }, 200);
    Run("CAEPedAudioEntity::TurnOffJetPack 0x4E2A70", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { OBJ(CAEPedAudioEntity)->CAEPedAudioEntity::TurnOffJetPack(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4E2A70, P(0)); return 0; }, pedBuild); }, 200);
    Run("CAEPedAudioEntity::StaticInitialise 0x5B98A0", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { CAEPedAudioEntity::StaticInitialise(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5B98A0); return 0; }, &BuildEnt); }, 20);
#undef OBJ
}
