// ORACLE-W3-AUDIO: exe-oracle rows for the uncovered class A/B functions of game_sa/Audio/Entities and the game_sa/Audio root.
// Harness = the W2 one (deterministic arena, whole exe data image + the fixture g_af compared after every case; included by wave2_oracle_all.inc when W3_AUDIO_ONLY is defined,
// executable wave3_audio_oracle_test). `this` of an entity row is the garbage fixture g_af (qualified calls, no vptr read) or, for the engine / manager rows, the exe's own
// global object (the same address on both sides: the object lives in the exe data image, which is compared as a whole).
// Sound-hardware calls (CAEAudioHardware::*, the sound manager's cancel / query calls, the radio / cutscene / ambience managers) are recorded by stubs on both sides: the exe addresses are
// oracle::Patch()ed to host stubs and the port's functions of the same names (their TUs are not linked) are defined in this file with the same bodies; the recorded call log
// (kind, this, arguments) lives in the fixture and is compared with it, so the rows compare which sound / bank / slot / volume the function asked the hardware for.
#include "Audio/AEAudioEnvironment.h"
#include "Audio/AEAudioUtility.h"
#include "Audio/AESmoothFadeThread.h"
#include "Audio/AESound.h"
#include "Audio/AEStreamThread.h"
#include "Audio/AudioEngine.h"
#include "Audio/AudioZones.h"
#include "Audio/Entities/AEAudioEntity.h"
#include "Camera.h"
#include "Weather.h"
#include "World.h"
#include "Audio/Hardware/AEAudioHardware.h"
#include "Audio/Managers/AESoundManager.h"
#include "Audio/Managers/AERadioTrackManager.h"
#include "Audio/Managers/AECutsceneTrackManager.h"
#include "Audio/Managers/AEAmbienceTrackManager.h"
#include "Audio/Entities/AEScriptAudioEntity.h"
#include "Audio/Entities/AEFrontendAudioEntity.h"
#include "Audio/Entities/AECollisionAudioEntity.h"
#include "Audio/Entities/AEGlobalWeaponAudioEntity.h"
#include "Audio/Entities/AEDoorAudioEntity.h"
#include "Audio/Entities/AEPedlessSpeechAudioEntity.h"
#include "Audio/Entities/AEExplosionAudioEntity.h"
#include "Audio/Entities/AEFireAudioEntity.h"
#include "Audio/Entities/AEWeatherAudioEntity.h"
#include "Audio/Entities/AEWaterCannonAudioEntity.h"
#include "Audio/Entities/AEWeaponAudioEntity.h"
#include "Audio/Entities/AEPedAudioEntity.h"
#include "Audio/Entities/AETwinLoopSoundEntity.h"
#include "Audio/Entities/AEPoliceScannerAudioEntity.h"
#include "Audio/Entities/AEVehicleAudioEntity.h"
#include "Audio/Entities/AEPedSpeechAudioEntity.h"

// globals whose translation units are not linked into this test (their methods are recording stubs below): the same exe addresses the port's NOTSA_GLOBAL definitions use
NOTSA_GLOBAL(AESoundManager, 0xB62CB0, (CAESoundManager), {});
NOTSA_GLOBAL(AEAudioHardware, 0xB5F8B8, (CAEAudioHardware), {});
NOTSA_GLOBAL(AERadioTrackManager, 0x8CB6F8, (CAERadioTrackManager), {});
NOTSA_GLOBAL(AECutsceneTrackManager, 0x8AE554, (CAECutsceneTrackManager), {});
NOTSA_GLOBAL(AEAmbienceTrackManager, 0x8AC15C, (CAEAmbienceTrackManager), {});

namespace w3au {
using namespace w2;
constexpr size_t kFxSize = 0x20000, kRecOff = 0x1E000, kModeOff = 0x1DFF0;
alignas(16) static uint8_t g_af[kFxSize];
static uint8_t* P(size_t off) { return g_af + off; }
template<class T> static T& At(size_t off) { return *reinterpret_cast<T*>(g_af + off); }
static inline uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static std::function<void(Rng&)> g_atweak;
static void BuildFx(Rng& r) { FillGarbage(r, g_af, kFxSize); At<uint32_t>(kRecOff) = 0; At<uint32_t>(kModeOff) = r.u32(); if (g_atweak) g_atweak(r); }

template<class PF, class EF>
static bool ACase(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&) = &BuildFx) {
    const uint32_t seed = r.u32() | 1;
    build(r);
    Reg fx{ (uintptr_t)g_af, kFxSize };
    Reg img{ kImgLo, kImgHi - kImgLo };
    return BothA(d, seed, { fx, img }, std::forward<PF>(pf), std::forward<EF>(ef));
}
// like ACase, but the byte range [skipVa, skipVa + skipLen) of the exe data image is not compared (the caller compares the meaningful fields through the return value: the entry holds
// debug-CRT strcpy_s fill bytes (0xFE) and uninitialised stack bytes of the port's temporary on one side)
template<class PF, class EF>
static bool ACaseX(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&), uintptr_t skipVa, size_t skipLen) {
    const uint32_t seed = r.u32() | 1;
    build(r);
    Reg fx{ (uintptr_t)g_af, kFxSize };
    Reg i1{ kImgLo, skipVa - kImgLo };
    Reg i2{ skipVa + skipLen, kImgHi - (skipVa + skipLen) };
    return BothA(d, seed, { fx, i1, i2 }, std::forward<PF>(pf), std::forward<EF>(ef));
}
template<class R, class... A> static R ExeT(uint32_t addr, void* self, A... a) { return oracle::Fn<R __fastcall(void*, int, A...)>(addr)(self, 0, a...); }
template<class R, class... A> static R ExeC(uint32_t addr, A... a) { return oracle::Fn<R __cdecl(A...)>(addr)(a...); }
static uint64_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
template<class T> static uint64_t Cv(T v) {
    if constexpr (std::is_same_v<T, float>) return FB(v);
    else if constexpr (std::is_pointer_v<T>) return (uint64_t)(uintptr_t)v;
    else if constexpr (std::is_same_v<T, bool>) return v ? 1 : 0;
    else return (uint64_t)(uint32_t)(int32_t)v;
}

// ---- recording stubs: the call log (kind, this, a, b, c) lives in the fixture and is compared with it -----------------------------------------------------------------------------------
static void Rec(uint32_t kind, const void* self, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0) {
    uint32_t& n = At<uint32_t>(kRecOff);
    if (n < 200) { uint32_t* e = &At<uint32_t>(kRecOff + 4 + n * 20); e[0] = kind; e[1] = (uint32_t)(uintptr_t)self; e[2] = a; e[3] = b; e[4] = c; }
    ++n;
}
static uint32_t RetV(uint32_t kind, uint32_t a, uint32_t b) {   // deterministic "answer" of a stub: a function of the call and the per-case mode word
    uint32_t h = (kind * 0x9E3779B1u) ^ (a * 0x85EBCA6Bu) ^ (b * 0xC2B2AE35u) ^ At<uint32_t>(kModeOff);
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; return h;
}
enum K : uint32_t { K_LoadSoundBank = 1, K_IsSoundBankLoaded, K_GetBankStatus, K_LoadSound, K_IsSoundLoaded, K_GetSoundStatus, K_MusicMaster, K_EffectsMaster, K_MusicFader, K_EffectsFader, K_NonStreamFader, K_StreamFader,
    K_EnableFx, K_DisableFx, K_BankSlotPlaying, K_EventPlaying, K_CancelBankSlot, K_CancelEvent, K_CancelOwned, K_Pause, K_ResetStats, K_AutoRetune, K_InitStation, K_IsRadioOn, K_RetuneStarted, K_ListenTimes, K_CurStation,
    K_VehRadio, K_PlayCutscene, K_CutsceneActive, K_CutsceneStatus, K_AmbActive, K_AmbRadio };
} // namespace w3au

// port side: the methods of the unlinked TUs, defined with the same recording bodies
using namespace w3au;
void CAEAudioHardware::LoadSoundBank(eSoundBank b, eSoundBankSlot s) { Rec(K_LoadSoundBank, this, (uint16_t)b, (uint16_t)s); }
bool CAEAudioHardware::IsSoundBankLoaded(eSoundBank b, eSoundBankSlot s) { Rec(K_IsSoundBankLoaded, this, (uint16_t)b, (uint16_t)s); return RetV(K_IsSoundBankLoaded, (uint16_t)b, (uint16_t)s) & 1; }
int8 CAEAudioHardware::GetSoundBankLoadingStatus(eSoundBank b, eSoundBankSlot s) { Rec(K_GetBankStatus, this, (uint16_t)b, (uint16_t)s); return (int8)(RetV(K_GetBankStatus, (uint16_t)b, (uint16_t)s) % 3); }
void CAEAudioHardware::LoadSound(eSoundBank b, eSoundID f, eSoundBankSlot s) { Rec(K_LoadSound, this, (uint16_t)b, (uint16_t)f, (uint16_t)s); }
bool CAEAudioHardware::IsSoundLoaded(eSoundBank b, eSoundID f, eSoundBankSlot s) { Rec(K_IsSoundLoaded, this, (uint16_t)b, (uint16_t)f, (uint16_t)s); return RetV(K_IsSoundLoaded, (uint16_t)b, (uint16_t)f) & 1; }
bool CAEAudioHardware::GetSoundLoadingStatus(eSoundBank b, eSoundID f, eSoundBankSlot s) { Rec(K_GetSoundStatus, this, (uint16_t)b, (uint16_t)f, (uint16_t)s); return RetV(K_GetSoundStatus, (uint16_t)b, (uint16_t)f) & 1; }
void CAEAudioHardware::SetMusicMasterScalingFactor(float f) { Rec(K_MusicMaster, this, (uint32_t)FB(f)); }
void CAEAudioHardware::SetEffectsMasterScalingFactor(float f) { Rec(K_EffectsMaster, this, (uint32_t)FB(f)); }
void CAEAudioHardware::SetMusicFaderScalingFactor(float f) { Rec(K_MusicFader, this, (uint32_t)FB(f)); }
void CAEAudioHardware::SetEffectsFaderScalingFactor(float f) { Rec(K_EffectsFader, this, (uint32_t)FB(f)); }
void CAEAudioHardware::SetNonStreamFaderScalingFactor(float f) { Rec(K_NonStreamFader, this, (uint32_t)FB(f)); }
void CAEAudioHardware::SetStreamFaderScalingFactor(float f) { Rec(K_StreamFader, this, (uint32_t)FB(f)); }
void CAEAudioHardware::EnableEffectsLoading() { Rec(K_EnableFx, this); }
void CAEAudioHardware::DisableEffectsLoading() { Rec(K_DisableFx, this); }
int16 CAESoundManager::AreSoundsPlayingInBankSlot(int16 s) { Rec(K_BankSlotPlaying, this, (uint16_t)s); return (int16)(RetV(K_BankSlotPlaying, (uint16_t)s, 0) % 3); }
int16 CAESoundManager::AreSoundsOfThisEventPlayingForThisEntity(int16 e, CAEAudioEntity* a) { Rec(K_EventPlaying, this, (uint16_t)e, (uint32_t)(uintptr_t)a); return (int16)(RetV(K_EventPlaying, (uint16_t)e, (uint32_t)(uintptr_t)a) % 3); }
void CAESoundManager::CancelSoundsInBankSlot(int16 s, bool f) { Rec(K_CancelBankSlot, this, (uint16_t)s, f ? 1 : 0); }
void CAESoundManager::CancelSoundsOfThisEventPlayingForThisEntity(int16 e, CAEAudioEntity* a) { Rec(K_CancelEvent, this, (uint16_t)e, (uint32_t)(uintptr_t)a); }
void CAESoundManager::CancelSoundsOwnedByAudioEntity(CAEAudioEntity* a, bool f) { Rec(K_CancelOwned, this, (uint32_t)(uintptr_t)a, f ? 1 : 0); }
void CAESoundManager::PauseManually(uint8 p) { Rec(K_Pause, this, p); }
void CAERadioTrackManager::ResetStatistics() { Rec(K_ResetStats, nullptr); }
void CAERadioTrackManager::SetRadioAutoRetuneOnOff(bool e) { Rec(K_AutoRetune, this, e ? 1 : 0); }
void CAERadioTrackManager::InitialiseRadioStationID(eRadioID id) { Rec(K_InitStation, this, (uint32_t)(int32_t)id); }
bool CAERadioTrackManager::IsRadioOn() const { Rec(K_IsRadioOn, this); return RetV(K_IsRadioOn, 0, 0) & 1; }
bool CAERadioTrackManager::HasRadioRetuneJustStarted() const { Rec(K_RetuneStarted, this); return RetV(K_RetuneStarted, 0, 0) & 1; }
int32* CAERadioTrackManager::GetRadioStationListenTimes() { Rec(K_ListenTimes, this); return reinterpret_cast<int32*>(g_af + 0x1C000); }
eRadioID CAERadioTrackManager::GetCurrentRadioStationID() const { Rec(K_CurStation, this); return (eRadioID)(int32_t)(RetV(K_CurStation, 0, 0) % 12 - 1); }
bool CAERadioTrackManager::IsVehicleRadioActive() { Rec(K_VehRadio, nullptr); return RetV(K_VehRadio, 0, 0) & 1; }
void CAECutsceneTrackManager::PlayPreloadedCutsceneTrack() { Rec(K_PlayCutscene, this); }
bool CAECutsceneTrackManager::IsCutsceneTrackActive() const { Rec(K_CutsceneActive, this); return RetV(K_CutsceneActive, 0, 0) & 1; }
uint8 CAECutsceneTrackManager::GetCutsceneTrackStatus() const { Rec(K_CutsceneStatus, this); return (uint8)(RetV(K_CutsceneStatus, 0, 0) % 4); }
bool CAEAmbienceTrackManager::IsAmbienceTrackActive() const { Rec(K_AmbActive, this); return RetV(K_AmbActive, 0, 0) & 1; }
bool CAEAmbienceTrackManager::IsAmbienceRadioActive() const { Rec(K_AmbRadio, this); return RetV(K_AmbRadio, 0, 0) & 1; }

namespace w3au {
// exe side: the same behaviour over the exe's own addresses (thiscall = __fastcall(this, edx, args...), callee cleans the stack)
#define EXE_SELF void* self, int
static void __fastcall X_LoadSoundBank(EXE_SELF, int b, int s) { Rec(K_LoadSoundBank, self, (uint16_t)b, (uint16_t)s); }
static uint32_t __fastcall X_IsSoundBankLoaded(EXE_SELF, int b, int s) { Rec(K_IsSoundBankLoaded, self, (uint16_t)b, (uint16_t)s); return RetV(K_IsSoundBankLoaded, (uint16_t)b, (uint16_t)s) & 1; }
static uint32_t __fastcall X_GetBankStatus(EXE_SELF, int b, int s) { Rec(K_GetBankStatus, self, (uint16_t)b, (uint16_t)s); return (uint32_t)(int8)(RetV(K_GetBankStatus, (uint16_t)b, (uint16_t)s) % 3); }
static void __fastcall X_LoadSound(EXE_SELF, int b, int f, int s) { Rec(K_LoadSound, self, (uint16_t)b, (uint16_t)f, (uint16_t)s); }
static uint32_t __fastcall X_IsSoundLoaded(EXE_SELF, int b, int f, int s) { Rec(K_IsSoundLoaded, self, (uint16_t)b, (uint16_t)f, (uint16_t)s); return RetV(K_IsSoundLoaded, (uint16_t)b, (uint16_t)f) & 1; }
static uint32_t __fastcall X_GetSoundStatus(EXE_SELF, int b, int f, int s) { Rec(K_GetSoundStatus, self, (uint16_t)b, (uint16_t)f, (uint16_t)s); return RetV(K_GetSoundStatus, (uint16_t)b, (uint16_t)f) & 1; }
static void __fastcall X_MusicMaster(EXE_SELF, float f) { Rec(K_MusicMaster, self, (uint32_t)FB(f)); }
static void __fastcall X_EffectsMaster(EXE_SELF, float f) { Rec(K_EffectsMaster, self, (uint32_t)FB(f)); }
static void __fastcall X_MusicFader(EXE_SELF, float f) { Rec(K_MusicFader, self, (uint32_t)FB(f)); }
static void __fastcall X_EffectsFader(EXE_SELF, float f) { Rec(K_EffectsFader, self, (uint32_t)FB(f)); }
static void __fastcall X_NonStreamFader(EXE_SELF, float f) { Rec(K_NonStreamFader, self, (uint32_t)FB(f)); }
static void __fastcall X_StreamFader(EXE_SELF, float f) { Rec(K_StreamFader, self, (uint32_t)FB(f)); }
static void __fastcall X_EnableFx(EXE_SELF) { Rec(K_EnableFx, self); }
static void __fastcall X_DisableFx(EXE_SELF) { Rec(K_DisableFx, self); }
static uint32_t __fastcall X_BankSlotPlaying(EXE_SELF, int s) { Rec(K_BankSlotPlaying, self, (uint16_t)s); return (uint32_t)(RetV(K_BankSlotPlaying, (uint16_t)s, 0) % 3); }
static uint32_t __fastcall X_EventPlaying(EXE_SELF, int e, void* a) { Rec(K_EventPlaying, self, (uint16_t)e, (uint32_t)(uintptr_t)a); return (uint32_t)(RetV(K_EventPlaying, (uint16_t)e, (uint32_t)(uintptr_t)a) % 3); }
static void __fastcall X_CancelBankSlot(EXE_SELF, int s, int f) { Rec(K_CancelBankSlot, self, (uint16_t)s, (f & 0xFF) ? 1 : 0); }
static void __fastcall X_CancelEvent(EXE_SELF, int e, void* a) { Rec(K_CancelEvent, self, (uint16_t)e, (uint32_t)(uintptr_t)a); }
static void __fastcall X_CancelOwned(EXE_SELF, void* a, int f) { Rec(K_CancelOwned, self, (uint32_t)(uintptr_t)a, (f & 0xFF) ? 1 : 0); }
static void __fastcall X_Pause(EXE_SELF, int p) { Rec(K_Pause, self, (uint8_t)p); }
static void __cdecl X_ResetStats() { Rec(K_ResetStats, nullptr); }
static void __fastcall X_AutoRetune(EXE_SELF, int e) { Rec(K_AutoRetune, self, (e & 0xFF) ? 1 : 0); }
static void __fastcall X_InitStation(EXE_SELF, int id) { Rec(K_InitStation, self, id); }
static uint32_t __fastcall X_IsRadioOn(EXE_SELF) { Rec(K_IsRadioOn, self); return RetV(K_IsRadioOn, 0, 0) & 1; }
static uint32_t __fastcall X_RetuneStarted(EXE_SELF) { Rec(K_RetuneStarted, self); return RetV(K_RetuneStarted, 0, 0) & 1; }
static int32_t* __fastcall X_ListenTimes(EXE_SELF) { Rec(K_ListenTimes, self); return reinterpret_cast<int32_t*>(g_af + 0x1C000); }
static int32_t __fastcall X_CurStation(EXE_SELF) { Rec(K_CurStation, self); return (int32_t)(RetV(K_CurStation, 0, 0) % 12 - 1); }
static uint32_t __cdecl X_VehRadio() { Rec(K_VehRadio, nullptr); return RetV(K_VehRadio, 0, 0) & 1; }
static void __fastcall X_PlayCutscene(EXE_SELF) { Rec(K_PlayCutscene, self); }
static uint32_t __fastcall X_CutsceneActive(EXE_SELF) { Rec(K_CutsceneActive, self); return RetV(K_CutsceneActive, 0, 0) & 1; }
static uint32_t __fastcall X_CutsceneStatus(EXE_SELF) { Rec(K_CutsceneStatus, self); return (uint32_t)(RetV(K_CutsceneStatus, 0, 0) % 4); }
static uint32_t __fastcall X_AmbActive(EXE_SELF) { Rec(K_AmbActive, self); return RetV(K_AmbActive, 0, 0) & 1; }
static uint32_t __fastcall X_AmbRadio(EXE_SELF) { Rec(K_AmbRadio, self); return RetV(K_AmbRadio, 0, 0) & 1; }
static void InstallStubs() {
    static bool done = false;
    if (done) return;
    done = true;
    auto pt = [](uint32_t a, void* f) { oracle::Patch(a, f); };
    pt(0x4D88A0, (void*)&X_LoadSoundBank); pt(0x4D88C0, (void*)&X_IsSoundBankLoaded); pt(0x4D88D0, (void*)&X_GetBankStatus); pt(0x4D8ED0, (void*)&X_LoadSound); pt(0x4D8EF0, (void*)&X_IsSoundLoaded);
    pt(0x4D8F00, (void*)&X_GetSoundStatus); pt(0x4D9530, (void*)&X_MusicMaster); pt(0x4D9540, (void*)&X_EffectsMaster); pt(0x4D9550, (void*)&X_MusicFader); pt(0x4D9560, (void*)&X_EffectsFader);
    pt(0x4D95A0, (void*)&X_NonStreamFader); pt(0x4D95B0, (void*)&X_StreamFader); pt(0x4D8E40, (void*)&X_EnableFx); pt(0x4D8E50, (void*)&X_DisableFx);
    pt(0x4EF520, (void*)&X_BankSlotPlaying); pt(0x4EF570, (void*)&X_EventPlaying); pt(0x4EFC60, (void*)&X_CancelBankSlot); pt(0x4EFB90, (void*)&X_CancelEvent); pt(0x4EFCD0, (void*)&X_CancelOwned); pt(0x4EF510, (void*)&X_Pause);
    pt(0x4E8200, (void*)&X_ResetStats); pt(0x4E82E0, (void*)&X_AutoRetune); pt(0x4E8330, (void*)&X_InitStation); pt(0x4E8350, (void*)&X_IsRadioOn); pt(0x4E8370, (void*)&X_RetuneStarted); pt(0x4E83E0, (void*)&X_ListenTimes);
    pt(0x4E83F0, (void*)&X_CurStation); pt(0x4E9800, (void*)&X_VehRadio); pt(0x4DBE70, (void*)&X_PlayCutscene); pt(0x4DBEB0, (void*)&X_CutsceneActive); pt(0x4DBEC0, (void*)&X_CutsceneStatus);
    pt(0x4D6CF0, (void*)&X_AmbActive); pt(0x4D6D40, (void*)&X_AmbRadio);
}
} // namespace w3au

#include "wave3_oracle_audio1.inc"
#include "wave3_oracle_audio2.inc"
#include "wave3_oracle_audio3.inc"
#include "wave3_oracle_audio4.inc"
#include "wave3_oracle_audio5.inc"
#include "wave3_oracle_audio6.inc"

static void TestW3AudioAll() {
    w3au::InstallStubs();
    TestW3Audio1();
    rf::Init(); g_tp.Init(); g_ep.Init(); g_onFault = &w2::OnFault;
    TestW3Audio2();
    TestW3Audio3();
    TestW3Audio4();
    TestW3Audio5();
    TestW3Audio6();
}
