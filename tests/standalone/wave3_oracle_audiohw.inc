// ORACLE-W3-AUDIO2: exe-oracle rows of the uncovered class A functions of game_sa/Audio/Hardware/AEAudioHardware.cpp (executable wave3_audiohw_oracle_test: W3_AUDIO_ONLY + W3_AUDIOHW, the real
// AEAudioHardware.cpp + AEMP3BankLoader.cpp linked, the CAEAudioHardware recording stubs of wave3_oracle_audio.inc left out). `this` = the exe's own global object AEAudioHardware (0xB5F8B8, the same
// address on both sides, part of the compared data image). The loaders the object points to are garbage objects of the fixture g_af (bank loader with 3 bank slots, track loader with the DVD fields).
// DirectSound / OpenAL / stream-thread calls are not reached by these functions (the closure of every row is pure), so no recording stubs are needed here; the stream-thread forwards run the
// exe's / port's real CAEStreamThread code on the object's embedded thread struct.
namespace w3au {
constexpr size_t kBankLoader = 0x1000, kBankSlots = 0x2000, kTrkLoader = 0x6000, kDvdPath = 0x6100, kOutBuf = 0x7000;
static CAEAudioHardware* HW() { return &AEAudioHardware; }
static void HwBuild(Rng& r) {
    BuildFx(r);
    auto* h = HW();
    FillGarbage(r, h, sizeof(CAEAudioHardware));
    auto* b = reinterpret_cast<uint8_t*>(h);
    auto W16 = [&](size_t o, uint16_t v) { std::memcpy(b + o, &v, 2); };
    auto W32 = [&](size_t o, uint32_t v) { std::memcpy(b + o, &v, 4); };
    for (size_t i = 0; i < 64; ++i) W16(0x90 + i * 2, (uint16_t)(r.below(6) == 0 ? r.below(70) : r.below(4)));   // m_anNumChannelsInSlot
    if (r.below(8) == 0) W16(0x90 + r.below(64) * 2, 0xFFFF);
    W16(0x8C, (uint16_t)(r.below(6) == 0 ? r.u32() : r.below(70)));                                              // m_nNumAvailableChannels
    W32(0xD98, (uint32_t)(uintptr_t)P(kBankLoader)); W32(0xD9C, (uint32_t)(uintptr_t)P(kTrkLoader));
    At<uint32_t>(kBankLoader + 0) = (uint32_t)(uintptr_t)P(kBankSlots); At<uint16_t>(kBankLoader + 0xC) = 3; At<uint8_t>(kBankLoader + 0x14) = (uint8_t)(r.below(4) != 0);   // m_BankSlots, m_BankSlotCnt, m_IsInitialised
    for (int s = 0; s < 3; ++s) for (int k = 0; k < 400; ++k) At<int16_t>(kBankSlots + (size_t)s * 0x12D4 + 0x14 + (size_t)k * 12 + 10) = (int16_t)(r.below(3) ? (int)r.below(2000) - 500 : (int)(r.u32() & 0xFFFF));   // Headroom
    At<uint32_t>(kTrkLoader + 0x18) = (uint32_t)(uintptr_t)P(kDvdPath);                                           // m_pszDvdDrivePath
}
static void HwZero(Rng& r) { BuildFx(r); std::memset(HW(), 0, sizeof(CAEAudioHardware)); }   // what the BSS object looks like before its constructor runs
static int16_t HwCh(Rng& r) { return (int16_t)(r.below(8) == 0 ? (int)r.below(8) - 4 : r.below(10) == 0 ? (int)r.below(70) : (int)r.below(64)); }
static uint16_t HwId(Rng& r) { return (uint16_t)(r.below(6) == 0 ? r.below(70) : r.below(8)); }
static float HwF(Rng& r) { return r.below(3) == 0 ? GenF(r, 2000.f) : r.below(2) ? -1000.f : GenF(r, 50.f, S_NOHUGE); }
} // namespace w3au

static void TestW3AudioHw() {
    using namespace w2; using namespace w3au;
#define HWR(NAME, ADDR, CASES, PCALL, ECALL) \
    Run(NAME " " #ADDR, [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { return Cv(PCALL); }, [&]() -> uint64_t { return Cv(ECALL); }, &HwBuild); }, CASES)
#define HWV(NAME, ADDR, CASES, PCALL, ECALL) \
    Run(NAME " " #ADDR, [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { PCALL; return 0; }, [&]() -> uint64_t { ECALL; return 0; }, &HwBuild); }, CASES)

    // constructor: on the zeroed object (BSS) - the exe sets only the fields below, the port also writes the other defaults of the class (all zero in the exe's constructed object)
    Run("CAEAudioHardware::Constructor 0x4D83E0", [&](Rng& r, std::string& d) { return ACase(r, d, [&]() -> uint64_t { new (HW()) CAEAudioHardware(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4D83E0, HW()); return 0; }, &HwZero); }, 20);
    Run("CAEAudioHardware::AllocateChannels 0x5B9340", [&](Rng& r, std::string& d) {
        static const uint16_t edge[] = { 0, 1, 2, 63, 64, 65, 0xFFFF, 0x8000 };
        const uint16_t n = r.below(5) == 0 ? edge[r.below(8)] : (uint16_t)r.below(12);
        return ACase(r, d, [&]() -> uint64_t { return Cv((int)HW()->AllocateChannels(n)); }, [&]() -> uint64_t { return Cv((int)(int16_t)ExeT<int32_t>(0x5B9340, HW(), (int)n)); }, &HwBuild);
    }, 800);
    // the exe ignores vch >= 300 (`jae`), the port asserts: no caller passes one
    Run("CAEAudioHardware::RequestVirtualChannelSoundInfo 0x4D8E60", [&](Rng& r, std::string& d) {
        const uint16_t vch = (uint16_t)r.below(300); const int16_t sfx = (int16_t)(r.below(4) == 0 ? (int)r.below(65536) - 32768 : (int)r.below(400) - 1), slot = (int16_t)(r.below(4) == 0 ? (int)r.below(65536) - 32768 : (int)r.below(60) - 1);
        return ACase(r, d, [&]() -> uint64_t { HW()->RequestVirtualChannelSoundInfo(vch, (eSoundID)sfx, (eSoundBankSlot)slot); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4D8E60, HW(), (int)vch, (int)sfx, (int)slot); return 0; }, &HwBuild);
    }, 600);
    HWR("CAEAudioHardware::GetNumAvailableChannels", 0x4D8810, 200, (uint16_t)HW()->GetNumAvailableChannels(), (uint16_t)ExeT<uint32_t>(0x4D8810, HW()));
    Run("CAEAudioHardware::SetChannelVolume 0x4D8870", [&](Rng& r, std::string& d) {
        const int16_t ch = HwCh(r); const uint16_t id = HwId(r); const float v = HwF(r); const uint8_t a4 = (uint8_t)r.u32();
        return ACase(r, d, [&]() -> uint64_t { HW()->SetChannelVolume(ch, id, v, a4); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4D8870, HW(), (int)ch, (int)id, v, (int)a4); return 0; }, &HwBuild);
    }, 1500);
    Run("CAEAudioHardware::SetChannelFrequencyScalingFactor 0x4D8960", [&](Rng& r, std::string& d) {
        const int16_t ch = HwCh(r); const uint16_t id = HwId(r); const float v = HwF(r);
        return ACase(r, d, [&]() -> uint64_t { HW()->SetChannelFrequencyScalingFactor(ch, id, v); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4D8960, HW(), (int)ch, (int)id, v); return 0; }, &HwBuild);
    }, 1500);
    Run("CAEAudioHardware::SetChannelFlags 0x4D9500", [&](Rng& r, std::string& d) {
        const int16_t ch = HwCh(r); const uint16_t id = HwId(r); const int16_t f = (int16_t)(r.below(2) ? r.below(256) : r.u32());
        return ACase(r, d, [&]() -> uint64_t { HW()->SetChannelFlags(ch, id, f); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4D9500, HW(), (int)ch, (int)id, (int)f); return 0; }, &HwBuild);
    }, 1500);
    Run("CAEAudioHardware::GetSoundHeadroom 0x4D8E30", [&](Rng& r, std::string& d) {
        const int16_t sfx = (int16_t)((int)r.below(401) - 1), slot = (int16_t)r.below(3);
        return ACase(r, d, [&]() -> uint64_t { return Cv(HW()->GetSoundHeadroom((eSoundID)sfx, (eSoundBankSlot)slot)); }, [&]() -> uint64_t { return Cv(ExeT<float>(0x4D8E30, HW(), (int)sfx, (int)slot)); }, &HwBuild);
    }, 800);
    HWV("CAEAudioHardware::GetVirtualChannelSoundLengths", 0x4D8E90, 40, HW()->GetVirtualChannelSoundLengths(reinterpret_cast<int16*>(P(kOutBuf))), ExeT<void>(0x4D8E90, HW(), P(kOutBuf)));
    HWV("CAEAudioHardware::GetVirtualChannelSoundLoopStartTimes", 0x4D8EB0, 40, HW()->GetVirtualChannelSoundLoopStartTimes(reinterpret_cast<int16*>(P(kOutBuf))), ExeT<void>(0x4D8EB0, HW(), P(kOutBuf)));
    HWV("CAEAudioHardware::StopTrack", 0x4D8F50, 60, HW()->StopTrack(), ExeT<void>(0x4D8F50, HW()));
    HWR("CAEAudioHardware::GetTrackLengthMs", 0x4D8F70, 60, HW()->GetTrackLengthMs(), ExeT<int32_t>(0x4D8F70, HW()));
    HWR("CAEAudioHardware::GetActiveTrackID", 0x4D8F80, 60, HW()->GetActiveTrackID(), ExeT<int32_t>(0x4D8F80, HW()));
    HWR("CAEAudioHardware::GetPlayingTrackID", 0x4D8F90, 60, HW()->GetPlayingTrackID(), ExeT<int32_t>(0x4D8F90, HW()));
    HWR("CAEAudioHardware::GetEffectsMasterScalingFactor", 0x4D9570, 200, HW()->GetEffectsMasterScalingFactor(), ExeT<float>(0x4D9570, HW()));
    HWR("CAEAudioHardware::GetMusicMasterScalingFactor", 0x4D9580, 200, HW()->GetMusicMasterScalingFactor(), ExeT<float>(0x4D9580, HW()));
    HWR("CAEAudioHardware::GetEffectsFaderScalingFactor", 0x4D9590, 200, HW()->GetEffectsFaderScalingFactor(), ExeT<float>(0x4D9590, HW()));
    HWR("CAEAudioHardware::IsStreamingFromDVD", 0x4D95C0, 100, HW()->IsStreamingFromDVD() ? 1 : 0, ExeT<uint8_t>(0x4D95C0, HW()) != 0 ? 1 : 0);
    HWR("CAEAudioHardware::GetDVDDriveLetter", 0x4D95D0, 100, (int)(uint8_t)HW()->GetDVDDriveLetter(), (int)ExeT<uint8_t>(0x4D95D0, HW()));
#undef HWR
#undef HWV
}
