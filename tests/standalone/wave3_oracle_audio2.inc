// ORACLE-W3-AUDIO part 2 (included by wave3_oracle_audio.inc): CAESound, CAudioZones, CAEAudioUtility::FindVehicleOfPlayer.
namespace w3au {
// like w2::FuncCase, plus a hook that runs after the fixture is filled (extra setup of the source object / globals)
template<class PF, class EF>
static bool SCase(Rng& r, std::string& d, size_t sz, std::initializer_list<EntOff> ents, PF&& portFn, EF&& exeFn, const std::function<void(Rng&)>& hook = {}, const std::vector<uint8_t>& thisMask = {}) {
    const uint32_t seed = Prep(r, 0);
    FillGarbage(r, g_anim, sizeof g_anim);
    g_subTask = nullptr;
    if (sz) { FillGarbage(r, g_src, (sz + 3) & ~size_t(3)); for (const auto& e : ents) PutEnt(r, e.off, e.kind, e.size); }
    if (hook) hook(r);
    Reg thisReg{ (uintptr_t)g_src, sz ? sz : 4 };
    thisReg.mask = thisMask;
    const bool ok = BothA(d, seed, { thisReg, Reg{ (uintptr_t)g_anim, sizeof g_anim }, g_tp.RegStruct(), g_tp.RegFlags(), g_tp.RegStore(), g_ep.RegStruct(), g_ep.RegFlags(), g_ep.RegStore(), Reg{ kImgLo, kImgHi - kImgLo } },
        [&]() -> uint64_t { return portFn(static_cast<void*>(g_src)); }, [&]() -> uint64_t { return exeFn(static_cast<void*>(g_src)); }, kRf);
    Done();
    return ok;
}
static void CamSetup(Rng& r) {   // the camera fields the sound code reads: position (no matrix), active cam + its mode, switch flag
    TheCamera.m_matrix = nullptr;
    TheCamera.m_placement.m_vPosn = CVector(GenF(r, 300.f, S_NOHUGE), GenF(r, 300.f, S_NOHUGE), GenF(r, 300.f, S_NOHUGE));
    TheCamera.m_nActiveCam = (uint8_t)r.below(2);
    TheCamera.m_aCams[TheCamera.m_nActiveCam].m_nMode = (eCamMode)(r.below(3) == 0 ? (int)MODE_CAMERA : (int)r.below(60));
    TheCamera.m_bJust_Switched = r.below(5) == 0;
}
static void TimerSetup(Rng& r) {
    CTimer::m_FrameCounter = r.below(3) ? r.below(40) : r.u32();
    CTimer::m_snTimeInMilliseconds = r.below(3) ? r.below(100000) : r.u32();
    CTimer::ms_fTimeScale = r.below(2) ? 1.f : r.f01() * 1.5f;
}
} // namespace w3au

static void TestW3Audio2() {
    using namespace w2; using namespace w3au;
    constexpr size_t kPhys = offsetof(CAESound, m_PhysicalEntity);
    auto SO = [] { return reinterpret_cast<CAESound*>(g_src); };
    auto sfx = [](Rng& r) { const auto h = [&](Rng& rr) { CamSetup(rr); TimerSetup(rr); }; (void)r; return std::function<void(Rng&)>(h); };
#define SND_V(NAME, ADDR, CASES, PCALL, ...) \
    Run(NAME " " #ADDR, [&](Rng& r, std::string& d) { return SCase(r, d, sizeof(CAESound), { EntOff{ kPhys, 'e' } }, \
        [&](void*) -> uint64_t { PCALL; return 0; }, [&](void*) -> uint64_t { ExeT<void>(ADDR, g_src __VA_OPT__(,) __VA_ARGS__); return 0; }, sfx(r)); }, CASES)

    SND_V("CAESound::UnregisterWithPhysicalEntity", 0x4EF1A0, 200, SO()->CAESound::UnregisterWithPhysicalEntity());
    SND_V("CAESound::StopSound", 0x4EF1C0, 200, SO()->CAESound::StopSound());
    SND_V("CAESound::StopSoundAndForget", 0x4EF850, 200, SO()->CAESound::StopSoundAndForget());
    SND_V("CAESound::UpdateFrequency", 0x4EF3E0, 100, SO()->CAESound::UpdateFrequency());
    Run("CAESound::SetFlags 0x4EF2B0", [&](Rng& r, std::string& d) {
        const uint16_t env = (uint16_t)(r.below(2) ? 1u << r.below(13) : r.u32()), en = (uint16_t)(r.below(2) ? r.below(2) : r.u32());
        return SCase(r, d, sizeof(CAESound), {}, [&](void*) -> uint64_t { SO()->CAESound::SetFlags(env, en); return 0; }, [&](void*) -> uint64_t { ExeT<void>(0x4EF2B0, g_src, (int)env, (int)en); return 0; });
    }, 300);
    Run("CAESound::UpdatePlayTime 0x4EF2E0", [&](Rng& r, std::string& d) {
        const int16_t len = (int16_t)(r.below(5) == 0 ? r.below(3) : r.below(3) ? (int)r.below(5000) : (int)(int16_t)r.u32()), loop = (int16_t)(r.below(3) == 0 ? -1 : (int)r.below(2000)), prog = (int16_t)(r.below(3) ? (int)r.below(500) : (int)(int16_t)r.u32());
        return SCase(r, d, sizeof(CAESound), {}, [&](void*) -> uint64_t { SO()->CAESound::UpdatePlayTime(len, loop, prog); return 0; }, [&](void*) -> uint64_t { ExeT<void>(0x4EF2E0, g_src, (int)len, (int)loop, (int)prog); return 0; },
            [&](Rng& rr) { auto* s = SO(); s->m_ListenerSpeed = rr.below(4) ? rr.f01() * 3.f : GenF(rr, 4.f); s->m_IsPhysicallyPlaying = rr.below(3) == 0; s->m_HasRequestedStopped = rr.below(4) == 0; s->m_PlayTime = (int16_t)(rr.below(3) ? (int)rr.below(3000) : (int)(int16_t)rr.u32()); });
    }, 800);
    Run("CAESound::GetRelativePlaybackFrequencyWithDoppler 0x4EF400", [&](Rng& r, std::string& d) {
        return SCase(r, d, sizeof(CAESound), {}, [&](void*) -> uint64_t { return FB(SO()->CAESound::GetRelativePlaybackFrequencyWithDoppler()); }, [&](void*) -> uint64_t { return FB(ExeT<float>(0x4EF400, g_src)); },
            [&](Rng& rr) { CamSetup(rr); auto* s = SO(); s->m_Flags = (uint16_t)(rr.below(2) ? (s->m_Flags | 1) : (s->m_Flags & ~1)); s->m_ListenerSpeed = rr.f01() * 3.f; s->m_PrevCamDist = GenF(rr, 100.f); s->m_CurrCamDist = rr.below(5) == 0 ? s->m_PrevCamDist : GenF(rr, 100.f);
                s->m_PrevTimeUpdateMs = (int32_t)rr.below(100000); s->m_CurrTimeUpdateMs = (int32_t)(s->m_PrevTimeUpdateMs + (int)rr.below(3000) - 100); s->m_Doppler = rr.below(5) == 0 ? 0.f : GenF(rr, 3.f); });
    }, 600);
    Run("CAESound::GetSlowMoFrequencyScalingFactor 0x4EF440", [&](Rng& r, std::string& d) {
        return SCase(r, d, sizeof(CAESound), {}, [&](void*) -> uint64_t { return FB(SO()->CAESound::GetSlowMoFrequencyScalingFactor()); }, [&](void*) -> uint64_t { return FB(ExeT<float>(0x4EF440, g_src)); },
            [&](Rng& rr) { CamSetup(rr); TimerSetup(rr); });
    }, 400);
    Run("CAESound::operator= 0x4EF680", [&](Rng& r, std::string& d) {
        return SCase(r, d, sizeof(CAESound), { EntOff{ kPhys, 'e' } }, [&](void*) -> uint64_t { return Cv(&SO()->CAESound::operator=(*reinterpret_cast<const CAESound*>(g_anim))); },
            [&](void*) -> uint64_t { return Cv(ExeT<void*>(0x4EF680, g_src, g_anim)); },
            [&](Rng& rr) { CEntity* e = GEnt(rr); std::memcpy(g_anim + kPhys, &e, 4); },
            // observed, not changed: the exe's copy skips the padding byte 0x55 and m_IsAudioHardwareAware (0x5A..0x5B) - the sound manager resets that flag in NewVPSLEntry right after every copy
            [] { std::vector<uint8_t> m(0x74, 0); m[0x55] = m[0x5A] = m[0x5B] = 1; return m; }());
    }, 300);
    Run("CAESound::RegisterWithPhysicalEntity 0x4EF820", [&](Rng& r, std::string& d) {
        CEntity* e = GEnt(r);
        return SCase(r, d, sizeof(CAESound), { EntOff{ kPhys, 'e' } }, [&](void*) -> uint64_t { SO()->CAESound::RegisterWithPhysicalEntity(e); return 0; }, [&](void*) -> uint64_t { ExeT<void>(0x4EF820, g_src, e); return 0; });
    }, 300);
    Run("CAESound::SetPosition 0x4EF880", [&](Rng& r, std::string& d) {
        const CVector pos(GenF(r, 300.f, S_NOHUGE), GenF(r, 300.f, S_NOHUGE), GenF(r, 300.f, S_NOHUGE));
        return SCase(r, d, sizeof(CAESound), {}, [&](void*) -> uint64_t { SO()->CAESound::SetPosition(pos); return 0; }, [&](void*) -> uint64_t { ExeT<void>(0x4EF880, g_src, pos.x, pos.y, pos.z); return 0; },
            [&](Rng& rr) { CamSetup(rr); TimerSetup(rr); if (rr.below(3) == 0) SO()->m_LastFrameUpdatedAt = 0; else if (rr.below(2)) SO()->m_LastFrameUpdatedAt = (int32_t)CTimer::m_FrameCounter; });
    }, 500);
    Run("CAESound::Initialise 0x4EFE50", [&](Rng& r, std::string& d) {
        const auto bank = (eSoundBankSlot)r.below(40); const auto sfxId = (eSoundID)r.below(300);
        CAEAudioEntity* ae = r.below(3) ? nullptr : reinterpret_cast<CAEAudioEntity*>(g_anim + 0x40);
        const CVector pos(GenF(r, 300.f, S_NOHUGE), GenF(r, 300.f, S_NOHUGE), GenF(r, 300.f, S_NOHUGE));
        const float vol = GenF(r, 50.f), roll = GenF(r, 5.f), spd = GenF(r, 3.f), dop = GenF(r, 3.f), sv = GenF(r, 1.f);
        const uint8_t fd = (uint8_t)r.below(8); uint16_t fl = (uint16_t)(r.below(2) ? r.u32() & 0x1FFF : 1u << r.below(13)); if (!ae) fl &= ~SOUND_REQUEST_UPDATES;
        const int16_t pt = (int16_t)r.below(2000);
        return SCase(r, d, sizeof(CAESound), { EntOff{ kPhys, 'e' } }, [&](void*) -> uint64_t { SO()->CAESound::Initialise(bank, sfxId, ae, pos, vol, roll, spd, dop, fd, fl, sv, pt); return 0; },
            [&](void*) -> uint64_t { ExeT<void>(0x4EFE50, g_src, (int)bank, (int)sfxId, ae, pos.x, pos.y, pos.z, vol, roll, spd, dop, (int)fd, (int)fl, sv, (int)pt); return 0; },
            [&](Rng& rr) { CamSetup(rr); TimerSetup(rr); });
    }, 400);

    // ---- CAudioZones (statics at 0xB6DC6C .. 0xB6EC08, all in the compared image)
    constexpr uint32_t kZoneLast = 0xB6EBFC;   // the exe's `LastUpdateCoors` (the port keeps a function-local static: tracked below)
    static CVector s_portLast(0.f, 0.f, 0.f);
    auto zoneBuild = [](Rng& r, bool fillArrays) {
        BuildFx(r);
        CAudioZones::m_NumSpheres = r.below(3); CAudioZones::m_NumBoxes = r.below(3) ? r.below(50) : r.below(158);
        if (fillArrays) {
            CAudioZones::m_NumSpheres = r.below(4); CAudioZones::m_NumBoxes = r.below(30);
            for (auto& z : CAudioZones::m_aSpheres) { std::memset(&z, 0, sizeof z); for (int i = 0; i < 7; ++i) z.m_szName[i] = (char)('A' + r.below(3)); z.m_IsActive = r.below(4) != 0; z.m_nAudioZone = (int16_t)r.below(70); z.m_Sphere.m_vecCenter = CVector(GenF(r, 100.f), GenF(r, 100.f), GenF(r, 100.f)); z.m_Sphere.m_fRadius = r.f01() * 80.f; }
            for (auto& z : CAudioZones::m_aBoxes) { std::memset(&z, 0, sizeof z); for (int i = 0; i < 7; ++i) z.m_szName[i] = (char)('A' + r.below(3)); z.m_IsActive = r.below(4) != 0; z.m_nAudioZone = (int16_t)r.below(70);
                const CVector a(GenF(r, 100.f), GenF(r, 100.f), GenF(r, 100.f)); const CVector b = a + CVector(r.f01() * 80.f, r.f01() * 80.f, r.f01() * 80.f); z.m_Box = CompressedBox{ .m_vecMin = a, .m_vecMax = b }; }
            for (auto& v : CAudioZones::m_aActiveBoxes) v = (int32_t)r.below(200); for (auto& v : CAudioZones::m_aActiveSpheres) v = (int32_t)r.below(200);
            CAudioZones::m_NumActiveBoxes = r.below(11); CAudioZones::m_NumActiveSpheres = r.below(11);
        }
    };
    Run("CAudioZones::Init 0x5081A0", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { CAudioZones::Init(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5081A0); return 0; }, +[](Rng& rr) { BuildFx(rr); }); }, 40);
    // the table entry written by Register*: compared through a hash of its fields (name up to the terminator, zone, active bit, geometry)
    auto zoneHash = [](uintptr_t va, bool sphere) -> uint64_t {
        const auto* b = reinterpret_cast<const uint8_t*>(va);
        uint64_t h = 1469598103934665603ull; auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
        for (int i = 0; i < 8 && b[i]; ++i) mix(b[i]);
        mix(*reinterpret_cast<const int16_t*>(b + 8)); mix(b[10] & 1);
        const size_t n = sphere ? 16 : 12; for (size_t i = 0; i < n; ++i) mix(b[12 + i]);
        return h;
    };
    Run("CAudioZones::RegisterAudioSphere 0x5081C0", [&](Rng& r, std::string& d) {
        char name[8]; const int n = r.below(8); for (int i = 0; i < n; ++i) name[i] = (char)('A' + r.below(26)); name[n] = 0;
        const int id = r.below(5) ? (int)r.below(70) : (int)r.u32(); const bool act = r.below(2); const CVector p(GenF(r, 500.f, S_NOHUGE), GenF(r, 500.f, S_NOHUGE), GenF(r, 500.f, S_NOHUGE)); const float rad = GenF(r, 300.f);
        const uint32_t idx = r.below(3); const uintptr_t ent = 0xB6EBA8 + idx * 0x1C;
        g_atweak = [idx](Rng&) { CAudioZones::m_NumSpheres = idx; };
        const bool ok = ACaseX(r, d, [&]() -> uint64_t { CAudioZones::RegisterAudioSphere(name, id, act, p, rad); return zoneHash(ent, true); }, [&]() -> uint64_t { ExeC<void>(0x5081C0, (char*)name, id, (int)act, p.x, p.y, p.z, rad); return zoneHash(ent, true); },
            [](Rng& rr) { BuildFx(rr); }, ent, 0x1C);
        g_atweak = nullptr; return ok;
    }, 400);
    Run("CAudioZones::RegisterAudioBox 0x508240", [&](Rng& r, std::string& d) {
        char name[8]; const int n = r.below(8); for (int i = 0; i < n; ++i) name[i] = (char)('A' + r.below(26)); name[n] = 0;
        const int id = r.below(5) ? (int)r.below(70) : (int)r.u32(); const bool act = r.below(2);
        const float sc = r.below(3) ? 500.f : 5000.f; const CVector mn(GenF(r, sc, S_NOHUGE), GenF(r, sc, S_NOHUGE), GenF(r, sc, S_NOHUGE)), mx(GenF(r, sc, S_NOHUGE), GenF(r, sc, S_NOHUGE), GenF(r, sc, S_NOHUGE));
        const uint32_t idx = r.below(158); const uintptr_t ent = 0xB6DCD0 + idx * 0x18;
        g_atweak = [idx](Rng&) { CAudioZones::m_NumBoxes = idx; };
        const bool ok = ACaseX(r, d, [&]() -> uint64_t { CAudioZones::RegisterAudioBox(name, id, act, mn, mx); return zoneHash(ent, false); }, [&]() -> uint64_t { ExeC<void>(0x508240, (char*)name, id, (int)act, mn.x, mn.y, mn.z, mx.x, mx.y, mx.z); return zoneHash(ent, false); },
            [](Rng& rr) { BuildFx(rr); }, ent, 0x18);
        g_atweak = nullptr; return ok;
    }, 600);
    Run("CAudioZones::Update 0x5083C0", [&](Rng& r, std::string& d) {
        const bool force = r.below(3) == 0; const CVector p(r.f01() * 200.f - 100.f, r.f01() * 200.f - 100.f, r.f01() * 200.f - 100.f);
        g_atweak = [&](Rng& rr) { const CVector base = rr.below(2) ? s_portLast : p; (void)base; StaticRef<CVector>(kZoneLast) = s_portLast; CTimer::m_FrameCounter = rr.below(3) ? 6 + 16 * rr.below(5) : rr.u32(); };
        const bool ok = ACaseX(r, d, [&]() -> uint64_t { CAudioZones::Update(force, p); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5083C0, (int)force, p.x, p.y, p.z); return 0; }, +[](Rng& rr) { BuildFx(rr);
            CAudioZones::m_NumSpheres = rr.below(4); CAudioZones::m_NumBoxes = rr.below(30);
            for (auto& z : CAudioZones::m_aSpheres) { std::memset(&z, 0, sizeof z); z.m_IsActive = rr.below(4) != 0; z.m_Sphere.m_vecCenter = CVector(GenF(rr, 100.f), GenF(rr, 100.f), GenF(rr, 100.f)); z.m_Sphere.m_fRadius = rr.f01() * 120.f; }
            for (auto& z : CAudioZones::m_aBoxes) { std::memset(&z, 0, sizeof z); z.m_IsActive = rr.below(4) != 0; const CVector a(GenF(rr, 100.f), GenF(rr, 100.f), GenF(rr, 100.f)); z.m_Box = CompressedBox{ .m_vecMin = a, .m_vecMax = a + CVector(rr.f01() * 120.f, rr.f01() * 120.f, rr.f01() * 120.f) }; }
            if (g_atweak) g_atweak(rr); }, kZoneLast, 12);
        g_atweak = nullptr;
        // the port's function-local LastUpdateCoors follows the exe's: after the call the exe's static holds the new position when the update ran
        if (ok) { const CVector now = StaticRef<CVector>(kZoneLast); s_portLast = now; }
        return ok;
    }, 700);
    (void)zoneBuild;
    // ---- CAEAudioEnvironment::GetReverbEnvironmentAndDepth (underwater / active audio zone / interior area)
    Run("CAEAudioEnvironment::GetReverbEnvironmentAndDepth 0x4D8010", [&](Rng& r, std::string& d) {
        g_atweak = [](Rng& rr) {
            CWeather::UnderWaterness = rr.below(3) == 0 ? 0.5f + rr.f01() : rr.f01() * 0.6f;
            CAudioZones::m_NumActiveSpheres = rr.below(3); CAudioZones::m_NumActiveBoxes = rr.below(3);
            CAudioZones::m_aActiveSpheres[0] = (int32_t)rr.below(3); CAudioZones::m_aActiveBoxes[0] = (int32_t)rr.below(158);
            for (auto& z : CAudioZones::m_aSpheres) z.m_nAudioZone = (int16_t)(rr.below(5) == 0 ? -1 : (int)rr.below(68));
            for (auto& z : CAudioZones::m_aBoxes) z.m_nAudioZone = (int16_t)(rr.below(5) == 0 ? -1 : (int)rr.below(68));
            StaticRef<int32_t>(0xB72914) = rr.below(3) == 0 ? 0 : (int)rr.below(8);
        };
        const bool ok = ACase(r, d, [&]() -> uint64_t { int8_t env = 0x55; int32_t depth = 0x5555; CAEAudioEnvironment::GetReverbEnvironmentAndDepth(&env, &depth); return (uint64_t)(uint8_t)env | ((uint64_t)(uint32_t)depth << 8); },
            [&]() -> uint64_t { int8_t env = 0x55; int32_t depth = 0x5555; ExeC<void>(0x4D8010, &env, &depth); return (uint64_t)(uint8_t)env | ((uint64_t)(uint32_t)depth << 8); }, +[](Rng& rr) { BuildFx(rr); });
        g_atweak = nullptr; return ok;
    }, 600);
#undef SND_V
}
