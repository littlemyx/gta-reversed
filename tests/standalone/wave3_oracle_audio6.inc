// ORACLE-W3-AUDIO part 6 (included by wave3_oracle_audio.inc): CAEPedSpeechAudioEntity (context tables, voice / type lookups, speech slots, conversation state) and the player-conversation statics.
namespace w3au {
template<class R, class... A> static R ExeS(uint32_t addr, A... a) { return oracle::Fn<R __stdcall(A...)>(addr)(a...); }
static int GCtx(Rng& r) { return (int)r.below(360); }   // 0..0x167: the exe returns 0 / false for others, the port indexes its tables (asserts) - no caller passes one
static int GCtxWide(Rng& r) { static const int edge[] = { -1, 0, 338, 339, 358, 359, 360, 361, 32767, -32768 }; return r.below(6) == 0 ? edge[r.below(10)] : (int)r.below(383) - 2; }   // the exe takes a word
constexpr size_t kSpeechSlots = 0xB61C38, kSpeechSlotsEnd = 0xB61CF8;   // s_PedSpeechSlots (6 x 0x1C)
// access to the private statics through explicit instantiation (access checks do not apply to the template arguments of an explicit instantiation)
template<class Tag, class Tag::type Mem> struct Rob { friend typename Tag::type RobGet(Tag) { return Mem; } };
struct RobScripted { using type = tPedSpeechSlotID (*)(); friend type RobGet(RobScripted); };
struct RobReservePed { using type = bool (*)(); friend type RobGet(RobReservePed); };
struct RobReservePlayer { using type = bool (*)(); friend type RobGet(RobReservePlayer); };
template struct Rob<RobScripted, &CAEPedSpeechAudioEntity::CanWePlayScriptedSpeech>;
template struct Rob<RobReservePed, &CAEPedSpeechAudioEntity::ReservePedConversationSpeechSlots>;
template struct Rob<RobReservePlayer, &CAEPedSpeechAudioEntity::ReservePlayerConversationSpeechSlot>;
} // namespace w3au

static void TestW3Audio6() {
    using namespace w2; using namespace w3au;
    using PS = CAEPedSpeechAudioEntity;
#define OBJ(C) reinterpret_cast<C*>(P(0))
    auto psBuild = +[](Rng& r) {
        BuildEnt(r);
        auto* p = OBJ(CAEPedSpeechAudioEntity);
        // protected members by their offsets (0x7C: m_Sounds[5]; the layout is confirmed by 0x4E4130 / 0x4E49E0)
        At<uint8_t>(0x90) = r.below(4) != 0; At<int16_t>(0x92) = (int16_t)(r.below(10) == 0 ? (int)r.below(8) - 1 : (int)r.below(6)); At<int16_t>(0x94) = 0;   // voice 0 exists for every ped type (the port asserts on an unknown voice id, the exe reads on)
        At<int16_t>(0x96) = (int16_t)r.below(2); At<uint8_t>(0x98) = r.below(2); At<uint8_t>(0x99) = r.below(4) == 0; At<uint8_t>(0x9A) = r.below(4) == 0;
        for (size_t o = 0xB4; o < 0x100; o += 4) At<uint32_t>(o) = r.below(2) ? r.below(100000) : r.u32();
        for (auto& t : PS::gGlobalSpeechContextNextPlayTime) t = r.below(2) ? r.below(100000) : r.u32();
        for (auto& s : PS::s_PedSpeechSlots) { s.Status = (CAEPedSpeechSlot::eStatus)r.below(6); reinterpret_cast<int16_t&>(s.GCtx) = (int16_t)(r.below(3) == 0 ? (int)r.below(362) : (int)r.below(4)); s.AudioEntity = r.below(2) ? nullptr : p; s.Sound = SndP(r); }
        CTimer::m_snTimeInMilliseconds = r.below(2) ? r.below(100000) : r.u32();
        PS::s_bPlayerConversationHappening = r.below(2); PS::s_pConversationPedSlot1 = (int16_t)((int)r.below(9) - 2); PS::s_pPlayerConversationPed = reinterpret_cast<CPed*>((uintptr_t)(r.below(2) ? 0 : r.u32())); PS::s_NextSpeechSlot = (int16_t)r.below(7);
        if (g_atweak) g_atweak(r);
    };
#define PSR(NAME, ADDR, CASES, PCALL, ECALL) \
    Run(NAME " " #ADDR, [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv(PCALL); }, [&]() -> uint64_t { return Cv(ECALL); }, psBuild); }, CASES)
#define PSV(NAME, ADDR, CASES, PCALL, ECALL) \
    Run(NAME " " #ADDR, [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { PCALL; return 0; }, [&]() -> uint64_t { ECALL; return 0; }, psBuild); }, CASES)
    auto P0 = [] { return OBJ(CAEPedSpeechAudioEntity); };

    // ---- statics (stdcall in the exe, no `this`)
    Run("CAEPedSpeechAudioEntity::IsGlobalContextPain 0x4E44F0", [&](Rng& r, std::string& d) { const int c = GCtx(r); return ACase(r, d, [&]() -> uint64_t { return PS::IsGlobalContextPain((eGlobalSpeechContext)c) ? 1 : 0; }, [&]() -> uint64_t { return ExeS<uint8_t>(0x4E44F0, c) != 0 ? 1 : 0; }, psBuild); }, 1500);
    Run("CAEPedSpeechAudioEntity::IsGlobalContextImportantForInterupting 0x4E4600", [&](Rng& r, std::string& d) { const int c = GCtx(r); return ACase(r, d, [&]() -> uint64_t { return PS::IsGlobalContextImportantForInterupting((eGlobalSpeechContext)c) ? 1 : 0; }, [&]() -> uint64_t { return ExeS<uint8_t>(0x4E4600, c) != 0 ? 1 : 0; }, psBuild); }, 1500);
    Run("CAEPedSpeechAudioEntity::IsGlobalContextUberImportant 0x4E46F0", [&](Rng& r, std::string& d) { const int16_t c = (int16_t)GCtx(r); return ACase(r, d, [&]() -> uint64_t { return PS::IsGlobalContextUberImportant(c) ? 1 : 0; }, [&]() -> uint64_t { return ExeS<uint8_t>(0x4E46F0, (int)c) != 0 ? 1 : 0; }, psBuild); }, 100);
    Run("CAEPedSpeechAudioEntity::GetNextMoodToUse 0x4E4700", [&](Rng& r, std::string& d) { const int c = (int)r.below(14) - 2; return ACase(r, d, [&]() -> uint64_t { return Cv((int)PS::GetNextMoodToUse((eCJMood)c)); }, [&]() -> uint64_t { return Cv((int)(int16_t)ExeS<int32_t>(0x4E4700, c)); }, psBuild); }, 300);
    Run("CAEPedSpeechAudioEntity::SetCJMood 0x4E3ED0", [&](Rng& r, std::string& d) {
        const auto mood = (eCJMood)((int)r.below(12) - 1); const uint32_t t = r.below(2) ? r.below(100000) : r.u32(); const int16_t gb = (int16_t)(r.below(3) == 0 ? -1 : (int)r.below(3)), fat = (int16_t)(r.below(3) == 0 ? -1 : (int)r.below(3)), wd = (int16_t)(r.below(3) == 0 ? -1 : (int)r.below(3));
        return ACase(r, d, [&]() -> uint64_t { PS::SetCJMood(mood, t, gb, fat, wd); return 0; }, [&]() -> uint64_t { ExeC<void>(0x4E3ED0, (int)mood, t, (int)gb, (int)fat, (int)wd); return 0; }, psBuild);
    }, 600);
    PSV("CAEPedSpeechAudioEntity::DisableAllPedSpeech", 0x4E3EB0, 20, PS::DisableAllPedSpeech(), ExeC<void>(0x4E3EB0));
    PSV("CAEPedSpeechAudioEntity::EnableAllPedSpeech", 0x4E3EC0, 20, PS::EnableAllPedSpeech(), ExeC<void>(0x4E3EC0));
    Run("CAEPedSpeechAudioEntity::GetAudioPedType 0x4E3C60", [&](Rng& r, std::string& d) {
        static const char* const names[] = { "PED_TYPE_GEN", "PED_TYPE_EMG", "PED_TYPE_PLAYER", "PED_TYPE_GANG", "PED_TYPE_GFD", "PED_TYPE_SPC", "PED_TYPE_XXX", "ped_type_gen", "", "PED_TYPE_GEN2", "PED_TYPE_GE" };
        const char* n = names[r.below(11)];
        return ACase(r, d, [&]() -> uint64_t { return Cv((int)PS::GetAudioPedType(n)); }, [&]() -> uint64_t { return Cv((int)(int16_t)ExeC<int32_t>(0x4E3C60, n)); }, psBuild);
    }, 300);
    Run("CAEPedSpeechAudioEntity::GetVoice 0x4E3CD0", [&](Rng& r, std::string& d) {
        static const char* const names[] = { "VOICE_GEN_BMOCD", "VOICE_GEN_WFYST", "VOICE_EMG_ARMY1", "VOICE_PLY_CJ", "VOICE_GNG_BALLAS1", "VOICE_GFD_BARBARA", "VOICE_GEN_BFYRI", "NONE", "VOICE_PLY_MAC", "VOICE_GNG_GROVE1", "VOICE_EMG_LAPD1", "VOICE_GFD_DENISE" };
        const char* n = names[r.below(12)]; const int tp = r.below(10) == 0 ? (int)r.below(8) : (int)r.below(5);
        return ACase(r, d, [&]() -> uint64_t { return Cv((int)PS::GetVoice(n, (eAudioPedType)tp)); }, [&]() -> uint64_t { return Cv((int)(int16_t)ExeC<int32_t>(0x4E3CD0, n, tp)); }, psBuild);
    }, 600);

    // wide range of global contexts (-2..380): the exe accepts 0..0x167 (a word), anything else is -1 / 0; the table at 8C6A68 has no entry for 339 (PAIN_START) and 359, an unreachable one for 360
    Run("CAEPedSpeechAudioEntity::GetSpecificSpeechContext 0x4E4470", [&](Rng& r, std::string& d) {
        const int c = GCtxWide(r); const int tp = r.below(4) == 0 ? 5 : (int)r.below(10) - 2;
        return ACase(r, d, [&]() -> uint64_t { return Cv((int)(int16_t)PS::GetSpecificSpeechContext((eGlobalSpeechContext)c, (eAudioPedType)tp)); }, [&]() -> uint64_t { return Cv((int)(int16_t)ExeS<int32_t>(0x4E4470, c, tp)); }, psBuild);
    }, 3000);
    Run("CAEPedSpeechAudioEntity::GetRepeatTime 0x4E47E0", [&](Rng& r, std::string& d) { const int c = GCtxWide(r); return ACase(r, d, [&]() -> uint64_t { return Cv((int)(int16_t)P0()->PS::GetRepeatTime((eGlobalSpeechContext)c)); }, [&]() -> uint64_t { return Cv((int)(int16_t)ExeT<int32_t>(0x4E47E0, P(0), c)); }, psBuild); }, 1500);

    // ---- members (this = the fixture)
    PSR("CAEPedSpeechAudioEntity::GetPedTalking", 0x4E3F50, 200, P0()->PS::GetPedTalking() ? 1 : 0, ExeT<uint8_t>(0x4E3F50, P(0)) != 0 ? 1 : 0);
    PSV("CAEPedSpeechAudioEntity::EnablePedSpeech", 0x4E3F70, 200, P0()->PS::EnablePedSpeech(), ExeT<void>(0x4E3F70, P(0)));
    PSV("CAEPedSpeechAudioEntity::EnablePedSpeechForScriptSpeech", 0x4E3F90, 200, P0()->PS::EnablePedSpeechForScriptSpeech(), ExeT<void>(0x4E3F90, P(0)));
    PSR("CAEPedSpeechAudioEntity::GetAllocatedVoice", 0x4E4120, 200, (int)P0()->PS::GetAllocatedVoice(), (int)(int16_t)ExeT<int32_t>(0x4E4120, P(0)));
    PSR("CAEPedSpeechAudioEntity::GetPedType", 0x4E4130, 200, (int)P0()->PS::GetPedType(), (int)(int16_t)ExeT<int32_t>(0x4E4130, P(0)));
    PSR("CAEPedSpeechAudioEntity::IsPedFemaleForAudio", 0x4E4150, 200, P0()->PS::IsPedFemaleForAudio() ? 1 : 0, ExeT<uint8_t>(0x4E4150, P(0)) != 0 ? 1 : 0);
    Run("CAEPedSpeechAudioEntity::GetSexForSpecialPed 0x4E4260", [&](Rng& r, std::string& d) { const uint32_t h = r.u32(); return ACase(r, d, [&]() -> uint64_t { return P0()->PS::GetSexForSpecialPed(h) ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4E4260, P(0), h) != 0 ? 1 : 0; }, psBuild); }, 40);
    Run("CAEPedSpeechAudioEntity::IsGlobalContextImportantForStreaming 0x4E4510", [&](Rng& r, std::string& d) { const int c = GCtx(r); return ACase(r, d, [&]() -> uint64_t { return P0()->PS::IsGlobalContextImportantForStreaming((eGlobalSpeechContext)c) ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4E4510, P(0), c) != 0 ? 1 : 0; }, psBuild); }, 1500);
    Run("CAEPedSpeechAudioEntity::IsGlobalContextImportantForWidescreen 0x4E46B0", [&](Rng& r, std::string& d) { const int c = GCtx(r); return ACase(r, d, [&]() -> uint64_t { return P0()->PS::IsGlobalContextImportantForWidescreen((eGlobalSpeechContext)c) ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4E46B0, P(0), c) != 0 ? 1 : 0; }, psBuild); }, 1500);
    Run("CAEPedSpeechAudioEntity::GetNextPlayTime 0x4E49E0", [&](Rng& r, std::string& d) { const int c = GCtx(r); return ACase(r, d, [&]() -> uint64_t { return Cv(P0()->PS::GetNextPlayTime((eGlobalSpeechContext)c)); }, [&]() -> uint64_t { return Cv(ExeT<uint32_t>(0x4E49E0, P(0), c)); }, psBuild); }, 1500);
    Run("CAEPedSpeechAudioEntity::GetNumSlotsPlayingContext 0x4E49B0", [&](Rng& r, std::string& d) { const int c = r.below(2) ? (int)r.below(4) : GCtx(r); return ACase(r, d, [&]() -> uint64_t { return Cv(P0()->PS::GetNumSlotsPlayingContext((eGlobalSpeechContext)c)); }, [&]() -> uint64_t { return Cv(ExeT<int32_t>(0x4E49B0, P(0), c)); }, psBuild); }, 500);
    Run("CAEPedSpeechAudioEntity::CanPedSayGlobalContext 0x4E5730", [&](Rng& r, std::string& d) { const int c = GCtx(r); return ACase(r, d, [&]() -> uint64_t { return P0()->PS::CanPedSayGlobalContext((eGlobalSpeechContext)c) ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4E5730, P(0), c) != 0 ? 1 : 0; }, psBuild); }, 3000);
    Run("CAEPedSpeechAudioEntity::WillPedChatAboutTopic 0x4E5800", [&](Rng& r, std::string& d) { const int16_t t = (int16_t)((int)r.below(14) - 2); return ACase(r, d, [&]() -> uint64_t { return P0()->PS::WillPedChatAboutTopic(t) ? 1 : 0; }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4E5800, P(0), (int)t) != 0 ? 1 : 0; }, psBuild); }, 1000);
    PSR("CAEPedSpeechAudioEntity::CanPedHoldConversation", 0x4E69E0, 400, P0()->PS::CanPedHoldConversation() ? 1 : 0, ExeT<uint8_t>(0x4E69E0, P(0)) != 0 ? 1 : 0);
    // ReleasePlayerConversation 4E3960: skipped - the slot reset leaves different padding bytes (the port copies an uninitialised temporary), fields are equal
    // ---- ORACLE-W3-AUDIO2: slot reservation, scripted-speech slot, constructor, vehicle statics (private statics through the Rob accessors above)
    PSR("CAEPedSpeechAudioEntity::CanWePlayScriptedSpeech", 0x4E4950, 600, (int)RobGet(RobScripted{})(), (int)(int16_t)ExeC<int32_t>(0x4E4950));
    PSR("CAEPedSpeechAudioEntity::ReservePedConversationSpeechSlots", 0x4E37F0, 800, RobGet(RobReservePed{})() ? 1 : 0, ExeC<uint8_t>(0x4E37F0) != 0 ? 1 : 0);
    PSR("CAEPedSpeechAudioEntity::ReservePlayerConversationSpeechSlot", 0x4E3870, 800, RobGet(RobReservePlayer{})() ? 1 : 0, ExeC<uint8_t>(0x4E3870) != 0 ? 1 : 0);
    Run("CAEPedSpeechAudioEntity::Constructor 0x4E4F10", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { new (P(0)) CAEPedSpeechAudioEntity(); At<uint32_t>(0) = 0; return 0; }, [&]() -> uint64_t { ExeT<void>(0x4E4F10, P(0)); At<uint32_t>(0) = 0; return 0; },
            +[](Rng& r) { BuildEnt(r); std::memset(P(0), 0, sizeof(CAEPedSpeechAudioEntity)); });   // vptr (port vtable vs exe 0x85F310) cleared after the call on both sides; zeroed object (fresh pool memory)
    }, 20);
    // vehicle: the table lookup takes the model id (400..611; `sub eax, 0x190`), the output struct is the hidden first argument
    Run("CAEVehicleAudioEntity::GetVehicleAudioSettings 0x4F5C10", [&](Rng& r, std::string& d) {
        const int16_t m = (int16_t)(400 + r.below(212)); auto* o1 = P(0x10000); auto* o2 = P(0x10100);
        return ACase(r, d, [&]() -> uint64_t { const auto s = CAEVehicleAudioEntity::GetVehicleAudioSettings(m); std::memcpy(o1, &s, sizeof s); std::memcpy(o2, &s, sizeof s); return 0; }, [&]() -> uint64_t { ExeS<void>(0x4F5C10, (void*)o2, (int)m); std::memcpy(o1, o2, 0x24); return 0; }, psBuild);
    }, 424);
    PSV("CAEVehicleAudioEntity::StaticInitialise", 0x5B99F0, 20, CAEVehicleAudioEntity::StaticInitialise(), ExeC<void>(0x5B99F0));
    Run("CAEVehicleAudioEntity::UpdateGasPedalAudio 0x4F5EB0", [&](Rng& r, std::string& d) {
        auto* veh = P(0x10000); const int type = r.below(2) ? 0 : 9;   // the exe handles type 0 (automobile) and 9 (bike) only
        return ACase(r, d, [&]() -> uint64_t { OBJ(CAEVehicleAudioEntity)->CAEVehicleAudioEntity::UpdateGasPedalAudio(reinterpret_cast<CVehicle*>(veh), type); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4F5EB0, P(0), (void*)veh, type); return 0; },
            +[](Rng& r) { BuildEnt(r); At<float>(0x10000 + 0x49C) = GenF(r, 2.f); At<float>(0x10000 + 0x808) = GenF(r, 2.f); At<float>(0x10000 + 0x964) = GenF(r, 2.f); if (r.below(3) == 0) At<float>(0x10000 + 0x964) = At<float>(0x10000 + 0x808) = std::fabs(At<float>(0x10000 + 0x49C)); });
    }, 1500);
#undef OBJ
#undef PSR
#undef PSV
}
