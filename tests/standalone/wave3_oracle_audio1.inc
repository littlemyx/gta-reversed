// ORACLE-W3-AUDIO part 1 (included by wave3_oracle_audio.inc): AEAudioEnvironment, AEStreamThread, AESmoothFadeThread, CAudioEngine (the engine rows use the exe's own AudioEngine object 0xB6BC90 as `this`).
namespace w3au {
static void BuildEng(Rng& r) { BuildFx(r); FillGarbage(r, &AudioEngine, sizeof(AudioEngine)); }
static void* Eng() { return &AudioEngine; }
} // namespace w3au

static void TestW3Audio1() {
    using namespace w2; using namespace w3au;
#define ENG(NAME, ADDR, CASES, PCALL, ECALL) \
    Run(NAME " " #ADDR, [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv(PCALL); }, [&]() -> uint64_t { return Cv(ECALL); }, &BuildEng); }, CASES)
#define ENGV(NAME, ADDR, CASES, PCALL, ECALL) \
    Run(NAME " " #ADDR, [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { PCALL; return 0; }, [&]() -> uint64_t { ECALL; return 0; }, &BuildEng); }, CASES)

    Run("CAEAudioEnvironment::GetDopplerRelativeFrequency 0x4D7E40", [&](Rng& r, std::string& d) {
        const float pd = GenF(r, 200.f), cd = r.below(6) == 0 ? pd : GenF(r, 200.f), sc = r.below(6) == 0 ? 0.f : GenF(r, 3.f);
        const uint32_t pt = r.below(5) ? r.u32() % 100000 : r.u32(); uint32_t ct = r.below(5) ? pt + (r.u32() % 2000) - 100 : r.u32();
        if (r.below(8) == 0) ct = pt;
        g_atweak = [&](Rng& rr) { TheCamera.m_bJust_Switched = rr.below(5) == 0; };
        const bool ok = ACase(r, d, [&]() -> uint64_t { return FB(CAEAudioEnvironment::GetDopplerRelativeFrequency(pd, cd, pt, ct, sc)); },
            [&]() -> uint64_t { return FB(ExeC<float>(0x4D7E40, pd, cd, pt, ct, sc)); });
        g_atweak = nullptr; return ok;
    }, 800);
    Run("CAEAudioEnvironment::GetDirectionalMikeAttenuation 0x4D7F60", [&](Rng& r, std::string& d) {
        CVector v = GenV(r, r.below(2) ? 1.f : 100.f);
        if (r.below(6) == 0) v = CVector(0.f, r.below(2) ? 1.f : -1.f, 0.f);
        if (r.below(8) == 0) v = CVector(0.f, 0.f, 0.f);
        return ACase(r, d, [&]() -> uint64_t { return FB(CAEAudioEnvironment::GetDirectionalMikeAttenuation(v)); }, [&]() -> uint64_t { return FB(ExeC<float>(0x4D7F60, &v)); });
    }, 800);

    // ---- CAEStreamThread: plain getters / the stop flag on a garbage object
    auto ST = [] { return reinterpret_cast<CAEStreamThread*>(P(0)); };
    Run("CAEStreamThread::GetTrackLengthMs 0x4F1550", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv(ST()->CAEStreamThread::GetTrackLengthMs()); }, [&]() -> uint64_t { return Cv(ExeT<int32_t>(0x4F1550, P(0))); }); }, 40);
    Run("CAEStreamThread::GetActiveTrackID 0x4F1560", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv(ST()->CAEStreamThread::GetActiveTrackID()); }, [&]() -> uint64_t { return Cv(ExeT<int32_t>(0x4F1560, P(0))); }); }, 40);
    Run("CAEStreamThread::GetPlayingTrackID 0x4F1570", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv(ST()->CAEStreamThread::GetPlayingTrackID()); }, [&]() -> uint64_t { return Cv(ExeT<int32_t>(0x4F1570, P(0))); }); }, 40);
    Run("CAEStreamThread::StopTrack 0x4F1580", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { ST()->CAEStreamThread::StopTrack(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4F1580, P(0)); return 0; }); }, 40);

    // ---- CAESmoothFadeThread: the entry table (64 x 0x20) with random status words and a few distinct buffer pointers
    auto FT = [] { return reinterpret_cast<CAESmoothFadeThread*>(P(0)); };
    auto fadeBuild = [](Rng& r) {
        BuildFx(r);
        auto* t = reinterpret_cast<CAESmoothFadeThread*>(P(0));
        for (auto& e : t->m_aEntries) { e.m_nStatus = (eSmoothFadeEntryStatus)(r.below(5) == 0 ? r.u32() : r.below(3)); e.m_pSoundBuffer = reinterpret_cast<IDirectSoundBuffer*>((uintptr_t)(0x1000 + 0x10 * r.below(4))); }
    };
    Run("CAESmoothFadeThread::InitialiseRequestSlots 0x4EE9F0", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { FT()->CAESmoothFadeThread::InitialiseRequestSlots(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4EE9F0, P(0)); return 0; }, fadeBuild); }, 60);
    Run("CAESmoothFadeThread::Stop 0x4EEA30", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { FT()->CAESmoothFadeThread::Stop(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4EEA30, P(0)); return 0; }, fadeBuild); }, 40);
    Run("CAESmoothFadeThread::CancelFade 0x4EEA50", [&](Rng& r, std::string& d) {
        auto* buf = reinterpret_cast<IDirectSoundBuffer*>((uintptr_t)(0x1000 + 0x10 * r.below(6)));
        return ACase(r, d, [&]() -> uint64_t { FT()->CAESmoothFadeThread::CancelFade(buf); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4EEA50, P(0), buf); return 0; }, fadeBuild);
    }, 300);

    // ---- CAudioEngine: trampolines to the managers / hardware (recorded) and the engine's own sub-entities
    // the exe returns the count of playing sounds (ax) / the raw byte of the twin-loop entity; every caller tests al != 0: compared as truth values
    ENG("CAudioEngine::IsLoadingTuneActive", 0x506D90, 100, AudioEngine.IsLoadingTuneActive() ? 1 : 0, ExeT<uint8_t>(0x506D90, Eng()) != 0 ? 1 : 0);
    ENGV("CAudioEngine::ResetStatistics", 0x506DA0, 40, AudioEngine.ResetStatistics(), ExeT<void>(0x506DA0, Eng()));
    ENGV("CAudioEngine::Restart", 0x506DB0, 40, AudioEngine.Restart(), ExeT<void>(0x506DB0, Eng()));
    for (int k = 0; k < 2; ++k) {
        const uint32_t a = k ? 0x506E10 : 0x506DE0;
        Run(std::string(k ? "CAudioEngine::SetEffectsMasterVolume " : "CAudioEngine::SetMusicMasterVolume ") + (k ? "0x506E10" : "0x506DE0"), [&, k, a](Rng& r, std::string& d) {
            const int8_t v = (int8_t)(r.below(3) ? r.below(128) : r.u32());
            return ACase(r, d, [&]() -> uint64_t { if (k) AudioEngine.SetEffectsMasterVolume(v); else AudioEngine.SetMusicMasterVolume(v); return 0; }, [&]() -> uint64_t { ExeT<void>(a, Eng(), (int)v); return 0; }, &BuildEng);
        }, 200);
    }
    {
        struct Row { const char* n; uint32_t a; int which; };
        static const Row rows[] = { { "SetMusicFaderScalingFactor 0x506E40", 0x506E40, 0 }, { "SetEffectsFaderScalingFactor 0x506E50", 0x506E50, 1 }, { "SetNonStreamFaderScalingFactor 0x506E60", 0x506E60, 2 }, { "SetStreamFaderScalingFactor 0x506E70", 0x506E70, 3 } };
        for (const auto& row : rows)
            Run(std::string("CAudioEngine::") + row.n, [&, row](Rng& r, std::string& d) {
                const float f = GenF(r, 4.f);
                return ACase(r, d, [&]() -> uint64_t {
                    switch (row.which) { case 0: AudioEngine.SetMusicFaderScalingFactor(f); break; case 1: AudioEngine.SetEffectsFaderScalingFactor(f); break; case 2: AudioEngine.SetNonStreamFaderScalingFactor(f); break; default: AudioEngine.SetStreamFaderScalingFactor(f); }
                    return 0; }, [&]() -> uint64_t { ExeT<void>(row.a, Eng(), f); return 0; }, &BuildEng);
            }, 200);
    }
    ENGV("CAudioEngine::DisableEffectsLoading", 0x506E80, 40, AudioEngine.DisableEffectsLoading(), ExeT<void>(0x506E80, Eng()));
    ENGV("CAudioEngine::EnableEffectsLoading", 0x506E90, 40, AudioEngine.EnableEffectsLoading(), ExeT<void>(0x506E90, Eng()));
    Run("CAudioEngine::SetRadioAutoRetuneOnOff 0x506F80", [&](Rng& r, std::string& d) {
        const bool b = r.below(2);
        return ACase(r, d, [&]() -> uint64_t { AudioEngine.SetRadioAutoRetuneOnOff(b); return 0; }, [&]() -> uint64_t { ExeT<void>(0x506F80, Eng(), (int)b); return 0; }, &BuildEng);
    }, 40);
    Run("CAudioEngine::InitialiseRadioStationID 0x506FC0", [&](Rng& r, std::string& d) {
        const int id = (int)r.below(14) - 2;
        return ACase(r, d, [&]() -> uint64_t { AudioEngine.InitialiseRadioStationID((eRadioID)id); return 0; }, [&]() -> uint64_t { ExeT<void>(0x506FC0, Eng(), id); return 0; }, &BuildEng);
    }, 40);
    ENG("CAudioEngine::IsRadioOn", 0x506FD0, 60, AudioEngine.IsRadioOn(), ExeT<uint8_t>(0x506FD0, Eng()));
    ENG("CAudioEngine::HasRadioRetuneJustStarted", 0x506FE0, 60, AudioEngine.HasRadioRetuneJustStarted(), ExeT<uint8_t>(0x506FE0, Eng()));
    ENG("CAudioEngine::IsRadioRetuneInProgress", 0x506FF0, 200, AudioEngine.IsRadioRetuneInProgress() ? 1 : 0, ExeT<uint8_t>(0x506FF0, Eng()) != 0 ? 1 : 0);
    ENG("CAudioEngine::GetRadioStationListenTimes", 0x507020, 40, AudioEngine.GetRadioStationListenTimes(), ExeT<int32_t*>(0x507020, Eng()));
    ENG("CAudioEngine::GetCurrentRadioStationID", 0x507040, 60, AudioEngine.GetCurrentRadioStationID(), ExeT<int32_t>(0x507040, Eng()));
    ENG("CAudioEngine::IsVehicleRadioActive", 0x507050, 60, AudioEngine.IsVehicleRadioActive(), ExeT<uint8_t>(0x507050, Eng()));
    ENGV("CAudioEngine::PlayPreloadedCutsceneTrack", 0x507070, 40, AudioEngine.PlayPreloadedCutsceneTrack(), ExeT<void>(0x507070, Eng()));
    ENG("CAudioEngine::IsCutsceneTrackActive", 0x507150, 60, AudioEngine.IsCutsceneTrackActive(), ExeT<uint8_t>(0x507150, Eng()));
    ENG("CAudioEngine::GetCutsceneTrackStatus", 0x507160, 60, AudioEngine.GetCutsceneTrackStatus(), ExeT<int8_t>(0x507160, Eng()));
    ENG("CAudioEngine::IsAmbienceTrackActive", 0x507210, 60, AudioEngine.IsAmbienceTrackActive(), ExeT<uint8_t>(0x507210, Eng()));
    ENG("CAudioEngine::DoesAmbienceTrackOverrideRadio", 0x507270, 60, AudioEngine.DoesAmbienceTrackOverrideRadio(), ExeT<uint8_t>(0x507270, Eng()));
    ENG("CAudioEngine::IsAmbienceRadioActive", 0x507280, 60, AudioEngine.IsAmbienceRadioActive(), ExeT<uint8_t>(0x507280, Eng()));
    Run("CAudioEngine::GetMissionAudioEvent 0x5072D0", [&](Rng& r, std::string& d) {
        const uint8_t id = (uint8_t)r.below(4);
        return ACase(r, d, [&]() -> uint64_t { return Cv(AudioEngine.GetMissionAudioEvent(id)); }, [&]() -> uint64_t { return Cv(ExeT<int32_t>(0x5072D0, Eng(), (int)id)); }, &BuildEng);
    }, 100);
    Run("CAudioEngine::GetMissionAudioPosition 0x5072E0", [&](Rng& r, std::string& d) {
        const uint8_t id = (uint8_t)r.below(4);
        g_atweak = [&](Rng& rr) { auto& l = AudioEngine.m_ScriptAE.wavLinks[id]; const auto m = rr.below(4); if (m == 0) l.m_pEntity = nullptr, l.m_vPosition = CVector(-1000.f, -1000.f, -1000.f); else if (m == 1) l.m_pEntity = nullptr, l.m_vPosition = CVector(0.f, 0.f, 0.f); else if (m == 2) l.m_pEntity = nullptr; else l.m_pEntity = reinterpret_cast<CEntity*>(P(0x400)); };
        const bool ok = ACase(r, d, [&]() -> uint64_t { return Cv(AudioEngine.GetMissionAudioPosition(id)); }, [&]() -> uint64_t { return Cv(ExeT<void*>(0x5072E0, Eng(), (int)id)); }, +[](Rng& rr) { BuildFx(rr); FillGarbage(rr, &AudioEngine, sizeof(AudioEngine)); if (g_atweak) g_atweak(rr); });
        g_atweak = nullptr; return ok;
    }, 200);
    Run("CAudioEngine::AttachMissionAudioToPhysical 0x507330", [&](Rng& r, std::string& d) {
        const uint8_t id = (uint8_t)r.below(4);
        auto* phys = reinterpret_cast<CPhysical*>(P(0x400));
        return ACase(r, d, [&]() -> uint64_t { return Cv(AudioEngine.AttachMissionAudioToPhysical(id, phys)); }, [&]() -> uint64_t { return Cv(ExeT<void*>(0x507330, Eng(), (int)id, phys)); }, &BuildEng);
    }, 100);
#undef ENG
#undef ENGV
}
