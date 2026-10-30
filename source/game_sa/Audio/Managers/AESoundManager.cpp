#include "StdInc.h"

#include "AESoundManager.h"
#include "DbgDangling.h"

#include "AEAudioEnvironment.h"
#include "AEAudioHardware.h"
#include "AEAudioStats.h"
#include "AudioEngine.h"
#include "Camera.h"
#include "AERadioTrackManager.h"

#include <DebugModules/Audio/SoundManagerDebugModule.hpp>
#include <UIRenderer.h>

NOTSA_GLOBAL(AESoundManager, 0xB62CB0, (CAESoundManager), {}); VALIDATE_SIZE(AESoundManager, 36028);

void CAESoundManager::InjectHooks() {
    RH_ScopedClass(CAESoundManager);
    RH_ScopedCategory("Audio/Managers");

    RH_ScopedInstall(Service, 0x4F0000);
    RH_ScopedInstall(Initialise, 0x5B9690);
    RH_ScopedInstall(Terminate, 0x4EFAA0);
    RH_ScopedInstall(Reset, 0x4EF4D0);
    RH_ScopedInstall(PauseManually, 0x4EF510);
    RH_ScopedInstall(RequestNewSound, 0x4EFB10);
    RH_ScopedInstall(AreSoundsPlayingInBankSlot, 0x4EF520);
    RH_ScopedInstall(AreSoundsOfThisEventPlayingForThisEntity, 0x4EF570);
    RH_ScopedInstall(AreSoundsOfThisEventPlayingForThisEntityAndPhysical, 0x4EF5D0);
    RH_ScopedInstall(GetVirtualChannelForPhysicalChannel, 0x4EF630);
    RH_ScopedInstall(CancelSoundsInBankSlot, 0x4EFC60);
    RH_ScopedInstall(CancelSoundsOwnedByAudioEntity, 0x4EFCD0);
    RH_ScopedInstall(CancelSoundsOfThisEventPlayingForThisEntity, 0x4EFB90);
    RH_ScopedInstall(CancelSoundsOfThisEventPlayingForThisEntityAndPhysical, 0x4EFBF0);
}

// 0x5B9690
bool CAESoundManager::Initialise() {
    const auto availChannels = AEAudioHardware.GetNumAvailableChannels();
    if (availChannels <= 10) {
        return false;
    }

    m_NumAllocatedPhysicalChannels = availChannels >= MAX_NUM_SOUNDS ? MAX_NUM_SOUNDS : availChannels;
    m_AudioHardwareHandle       = AEAudioHardware.AllocateChannels(m_NumAllocatedPhysicalChannels);
    if (m_AudioHardwareHandle == -1) {
        return false;
    }

    // BUG? There's some int16 weird logic in the original code, I simplified it to what's actually used i think
    m_PhysicallyPlayingSoundList = new tSoundReference[m_NumAllocatedPhysicalChannels];
    m_ChannelPosition            = new int16[m_NumAllocatedPhysicalChannels];
    m_PrioritisedSoundList       = new tSoundReference[m_NumAllocatedPhysicalChannels];

    for (CAESound& sound : m_VirtuallyPlayingSoundList) {
        sound.m_IsInUse             = false;
        sound.m_IsPhysicallyPlaying = false;
    }

    std::fill_n(m_PhysicallyPlayingSoundList, m_NumAllocatedPhysicalChannels, -1);

    m_TimeLastCalled         = CTimer::GetTimeInMS();
    m_WasGamePausedLastFrame = false;
    m_TimeLastCalledUnpaused = 0;
    m_IsManuallyPaused       = false;

    NOTSA_LOG_DEBUG("Initialised SoundManager");
    return true;
}

// 0x4EFAA0
void CAESoundManager::Terminate() {
    delete[] m_PhysicallyPlayingSoundList;
    delete[] m_ChannelPosition;
    delete[] m_PrioritisedSoundList;

    m_PhysicallyPlayingSoundList = nullptr;
    m_ChannelPosition            = nullptr;
    m_PrioritisedSoundList       = nullptr;
}

// 0x4EF4D0
void CAESoundManager::Reset() {
    for (CAESound& sound : m_VirtuallyPlayingSoundList) {
        if (!sound.IsActive()) {
            continue;
        }

        sound.StopSound();
    }
}

// 0x4EF510
void CAESoundManager::PauseManually(uint8 bPause) {
    m_IsManuallyPaused = bPause;
}

// 0x4F0000
void CAESoundManager::Service() {
    // Clear sounds uncancellable status for this frame
    std::fill_n(m_PrioritisedSoundList, m_NumAllocatedPhysicalChannels, -1);

    // Calculate time diff from last update
    uint32 timeSinceLastUpdate;
    if (CAESoundManager::IsPaused()) {
        if (m_WasGamePausedLastFrame) {
            timeSinceLastUpdate = CTimer::GetTimeInMSPauseMode() - m_TimeLastCalled;
        } else {
            m_TimeLastCalledUnpaused = m_TimeLastCalled;
            timeSinceLastUpdate      = 0;
        }

        m_TimeLastCalled         = CTimer::GetTimeInMSPauseMode();
        m_WasGamePausedLastFrame = true;
    } else {
        timeSinceLastUpdate = CTimer::GetTimeInMS() - (m_WasGamePausedLastFrame ? m_TimeLastCalledUnpaused : m_TimeLastCalled);

        m_TimeLastCalled         = CTimer::GetTimeInMS();
        m_WasGamePausedLastFrame = false;
    }

    // Get current frame sounds infos
    AEAudioHardware.GetChannelPlayTimes(m_AudioHardwareHandle, m_ChannelPosition);
    AEAudioHardware.GetVirtualChannelSoundLengths(m_VirtualChannelSoundLengths);
    AEAudioHardware.GetVirtualChannelSoundLoopStartTimes(m_VirtualChannelSoundLoopStartTimes);

    // 0x4F011C - Initialize sounds that are using percentage specified start positions
    for (auto&& [i, sound] : rngv::enumerate(m_VirtuallyPlayingSoundList)) {
        if (!sound.IsActive() || !sound.IsAudioHardwareAware() || !sound.GetPlayTimeIsPercentage()) {
            continue;
        }

        sound.SetFlags(eSoundEnvironment::SOUND_START_PERCENTAGE, false);
        if (sound.m_IsPhysicallyPlaying) {
            continue;
        }

        //sound.m_PlayTime *= uint16(static_cast<float>(m_aSoundLengths[i]) / 100.0F);
        sound.m_PlayTime = NOTSA_FTOL(uint16, (float)(sound.m_PlayTime * m_VirtualChannelSoundLengths[i]) * ExeRecip(100.0f));
    }

    // 0x4F016D - Stop sounds that turned inactive
    for (auto&& [i, ref] : rngv::enumerate(GetPhysicallyPlayingSoundList())) {
        if (ref == -1) {
            continue;
        }
        auto& sound                  = m_VirtuallyPlayingSoundList[ref];
        sound.m_PlayTime = m_ChannelPosition[i];
        if (sound.m_HasRequestedStopped) {
            AEAudioHardware.StopSound(m_AudioHardwareHandle, i);
        }
    }

    // 0x4F01E4 - Update sounds playtime
    for (auto&& [i, sound] : rngv::enumerate(m_VirtuallyPlayingSoundList)) {
        if (!sound.IsActive() || !sound.IsAudioHardwareAware() || sound.m_FrameDelay) {
            continue;
        }
        auto usedProgress = CAESoundManager::IsSoundPaused(sound) ? 0 : timeSinceLastUpdate;
        sound.UpdatePlayTime(m_VirtualChannelSoundLengths[i], m_VirtualChannelSoundLoopStartTimes[i], usedProgress);
    }

    // 0x4F02FB - Remove sounds that ended from sounds table
    for (auto&& [chN, ref] : rngv::enumerate(GetChannelPositions())) {
        if (ref == -1) {
            GetPhysicallyPlayingSoundList()[chN] = -1;
        }
    }

    // 0x4F0329 - Mark sounds that ended as finished
    for (CAESound& sound : m_VirtuallyPlayingSoundList) {
        // 0x4F0330: InUse, FrameDelay == 0 (NOT the hardware-aware flag), PlayTime == -1; SoundHasFinished is inlined by the exe
        if (!sound.IsActive() || sound.m_FrameDelay != 0 || sound.m_PlayTime != -1) {
            continue;
        }
        sound.SoundHasFinished();
    }

    // 0x4F03E5, 0x4F040D - Update sounds positions and volumes
    for (CAESound& sound : m_VirtuallyPlayingSoundList) {
        if (!sound.IsActive()) {
            continue;
        }
        sound.UpdateParameters(sound.m_PlayTime);
        sound.CalculateVolume();
    }

    // 0x4F042C - Find prioritized sounds
    auto numPrioritisedSounds = 0;
    for (auto&& [i, ref] : rngv::enumerate(GetPhysicallyPlayingSoundList())) {
        if (ref == -1) {
            continue;
        }
        if (!m_VirtuallyPlayingSoundList[ref].IsUnancellable()) {
            continue;
        }
        m_PrioritisedSoundList[numPrioritisedSounds++] = ref;
    }

    // Check if we need to insert any of the already playing sounds into the list
    for (auto&& [i, sound] : rngv::enumerate(m_VirtuallyPlayingSoundList)) {
        if (!sound.IsActive()) {
            continue;
        }
        if (sound.m_IsPhysicallyPlaying && sound.IsUnancellable()) {
            continue;
        }
        if (sound.m_FrameDelay != 0) {
            continue;
        }

        int16 chN = m_NumAllocatedPhysicalChannels - 1;

        // 0x4F04CE - Find last slot in use
        for (; chN >= numPrioritisedSounds; chN--) { // exe: do-while down to and INCLUDING index numPrioritisedSounds (0x4F04D6)
            if (GetPrioritisedSoundList()[chN] != -1) {
                break;
            }
        }

        // 0x4F04EB - Find where to insert
        for (; chN >= numPrioritisedSounds; chN--) {
            const auto& soundB = m_VirtuallyPlayingSoundList[m_PrioritisedSoundList[chN]];
            if (sound.m_ListenerVolume >= soundB.m_ListenerVolume) {
                continue;
            }
            // 0x4F051D: `cmp bl, cl; jbe` - the insert point is found when the new sound is quieter and NOT more physical than B
            if (sound.GetPlayPhysically() > soundB.GetPlayPhysically()) {
                continue;
            }
            break;
        }

        // 0x4F0529 - Insert at given index
        if (chN != m_NumAllocatedPhysicalChannels - 1) {
            // Shift to right
            for (auto i = m_NumAllocatedPhysicalChannels - 1; i > chN + 1; --i) {
                m_PrioritisedSoundList[i] = m_PrioritisedSoundList[i - 1];
            }

            // Insert
            m_PrioritisedSoundList[chN + 1] = i;
        }
    }

    // 0x4F0585 - Stop songs that aren't marked as uncancellable in this frame
    for (auto i = 0; i < m_NumAllocatedPhysicalChannels; ++i) {
        const auto ref = m_PhysicallyPlayingSoundList[i];
        if (ref == -1) {
            continue;
        }

        int32 chN;
        for (chN = 0; chN < m_NumAllocatedPhysicalChannels; ++chN) {
            if (ref == m_PrioritisedSoundList[chN]) {
                break;
            }
        }

        if (chN == m_NumAllocatedPhysicalChannels) {
            m_VirtuallyPlayingSoundList[ref].m_IsPhysicallyPlaying = false;
            m_PhysicallyPlayingSoundList[i]                        = -1;
            AEAudioHardware.StopSound(m_AudioHardwareHandle, i);
        } else {
            m_PrioritisedSoundList[chN] = -1;
        }
    }

    // 0x4F0628 - Play sounds that require that
    for (auto i = 0, chN = 0; i < m_NumAllocatedPhysicalChannels; ++i) {
        const auto ref = m_PrioritisedSoundList[i];
        if (ref == -1) {
            continue;
        }

        // Find a free channel to use
        while (chN < m_NumAllocatedPhysicalChannels && m_PhysicallyPlayingSoundList[chN] != -1) {
            chN++;
        }
        if (chN >= m_NumAllocatedPhysicalChannels) {
            break;
        }

        m_PhysicallyPlayingSoundList[chN] = ref;
        auto& sound                       = m_VirtuallyPlayingSoundList[ref];
        sound.m_IsPhysicallyPlaying               = true;

        const auto freqFactor = sound.GetRelativePlaybackFrequencyWithDoppler() * sound.GetSlowMoFrequencyScalingFactor();

        CAEAudioHardwarePlayFlags flags{};
        flags.CopyFromAESound(sound);

        AEAudioHardware.PlaySound(m_AudioHardwareHandle, chN, sound.m_SoundID, sound.m_BankSlot, sound.m_PlayTime, flags.m_nFlags, sound.GetSpeed());
        AEAudioHardware.SetChannelVolume(m_AudioHardwareHandle, chN, sound.m_ListenerVolume, 0);
        AEAudioHardware.SetChannelPosition(m_AudioHardwareHandle, chN, sound.GetRelativePosition(), 0);
        AEAudioHardware.SetChannelFrequencyScalingFactor(m_AudioHardwareHandle, chN, freqFactor);
    }

    // 0x4F0894
    for (auto i = 0; i < m_NumAllocatedPhysicalChannels; ++i) {
        const auto ref = m_PhysicallyPlayingSoundList[i];
        if (ref == -1) {
            continue;
        }

        auto& sound = m_VirtuallyPlayingSoundList[ref];
        if (!sound.IsActive()) {
            continue;
        }

        if (!CAESoundManager::IsSoundPaused(sound)) {
            AEAudioHardware.SetChannelVolume(m_AudioHardwareHandle, i, sound.m_ListenerVolume, 0);
            auto freq        = sound.GetRelativePlaybackFrequencyWithDoppler();
            auto slomoFactor = sound.GetSlowMoFrequencyScalingFactor();
            AEAudioHardware.SetChannelFrequencyScalingFactor(m_AudioHardwareHandle, i, freq * slomoFactor);
        } else {
            AEAudioHardware.SetChannelVolume(m_AudioHardwareHandle, i, -100.0F, 0);
            AEAudioHardware.SetChannelFrequencyScalingFactor(m_AudioHardwareHandle, i, 0.0F);
        }
        AEAudioHardware.SetChannelPosition(m_AudioHardwareHandle, i, sound.GetRelativePosition(), 0);
    }

    // 0x4F0AB8
    AEAudioHardware.Service();

    // 0x4F0B05
    for (CAESound& sound : m_VirtuallyPlayingSoundList) {
        if (!sound.IsActive()) {
            continue;
        }

        sound.m_IsAudioHardwareAware = true;
        if ((int8)sound.m_FrameDelay > 0 && !CAESoundManager::IsSoundPaused(sound)) { // exe: signed byte `jle` (0x4F0AD6)
            --sound.m_FrameDelay;
        }
    }

#ifdef NOTSA_STANDALONE_RUN
    notsa::audio_stats::Service(); // NOTSA: NOTSA_AUDIO_STATS
#endif
}

// 0x4EFB10
CAESound* CAESoundManager::RequestNewSound(CAESound* pSound) {
    size_t sidx;
    auto   s = GetFreeSound(&sidx);
    NOTSA_AUDIO_STAT(reqOk); // NOTSA: NOTSA_AUDIO_STATS (reqOk counts all requests, reqRefused those that found the table full)
    if (!s) { NOTSA_AUDIO_STAT(reqRefused); }
    if (s) {
        *s = *pSound;
        pSound->UnregisterWithPhysicalEntity();
        s->NewVPSLEntry();
#ifdef NOTSA_INPUT_INJECT
        if (s->m_AudioEntity) { auto& sh = dbg3::g_sndShadow[sidx]; sh.owner = s->m_AudioEntity; sh.vtbl = *(const uint32*)s->m_AudioEntity; sh.cls = dbg3::Classify(s->m_AudioEntity); sh.frame = CTimer::GetFrameCounter(); sh.ms = CTimer::GetTimeInMS(); }
#endif
        AEAudioHardware.RequestVirtualChannelSoundInfo((uint16)sidx, s->m_SoundID, s->m_BankSlot);
    }

#ifdef NOTSA_DEBUG
    notsa::ui::UIRenderer::GetInstance().GetDebugModules().GetModule<notsa::debugmodules::SoundManagerDebugModule>()->SetSoundInfo(sidx, {
        .StackTrace = notsa::StackTrace::current()
    });
#endif

    return s;
}

CAESound* CAESoundManager::PlaySound(tSoundPlayParams p) {
    if (p.RegisterWithEntity) {
        p.Flags |= SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY;
    }
    CAESound s;
    s.Initialise(
        p.BankSlotID,
        p.SoundID,
        p.AudioEntity,
        p.Pos,
        p.Volume,
        p.RollOffFactor,
        p.Speed,
        p.Doppler,
        p.FrameDelay,
        p.Flags,
        p.FrequencyVariance,
        p.PlayTime
    );
    s.m_Event          = p.EventID;
    s.m_ClientVariable = p.ClientVariable;
    if (p.Flags & SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY) {
        s.RegisterWithPhysicalEntity(p.RegisterWithEntity);
    }
    return RequestNewSound(&s);
}

// 0x4EF520
int16 CAESoundManager::AreSoundsPlayingInBankSlot(int16 bankSlot) {
    auto nPlaying = eSoundPlayingStatus::SOUND_NOT_PLAYING;
    for (CAESound& sound : m_VirtuallyPlayingSoundList) {
        if (!sound.IsActive() || sound.m_BankSlot != bankSlot) {
            continue;
        }
        if (sound.m_IsPhysicallyPlaying) {
            return eSoundPlayingStatus::SOUND_HAS_STARTED;
        }
        nPlaying = eSoundPlayingStatus::SOUND_PLAYING;
    }
    return nPlaying;
}

// 0x4EF570
int16 CAESoundManager::AreSoundsOfThisEventPlayingForThisEntity(int16 eventId, CAEAudioEntity* audioEntity) {
    auto nPlaying = eSoundPlayingStatus::SOUND_NOT_PLAYING;
    for (CAESound& sound : m_VirtuallyPlayingSoundList) {
        if (!sound.IsActive() || sound.m_Event != eventId || sound.m_AudioEntity != audioEntity) {
            continue;
        }
        if (sound.m_IsPhysicallyPlaying) {
            return eSoundPlayingStatus::SOUND_HAS_STARTED;
        }
        nPlaying = eSoundPlayingStatus::SOUND_PLAYING;
    }

    return nPlaying;
}

// 0x4EF5D0
int16 CAESoundManager::AreSoundsOfThisEventPlayingForThisEntityAndPhysical(int16 eventId, CAEAudioEntity* audioEntity, CPhysical* physical) {
    bool nPlaying = eSoundPlayingStatus::SOUND_NOT_PLAYING;
    for (CAESound& sound : m_VirtuallyPlayingSoundList) {
        if (!sound.IsActive() || sound.m_Event != eventId || sound.m_AudioEntity != audioEntity || sound.m_PhysicalEntity != physical) {
            continue;
        }
        if (sound.m_IsPhysicallyPlaying) {
            return eSoundPlayingStatus::SOUND_HAS_STARTED;
        }
        nPlaying = eSoundPlayingStatus::SOUND_PLAYING;
    }

    return nPlaying;
}

// 0x4EFB90
void CAESoundManager::CancelSoundsOfThisEventPlayingForThisEntity(int16 eventId, CAEAudioEntity* audioEntity) {
    for (CAESound& sound : m_VirtuallyPlayingSoundList) {
        if (!sound.IsActive() || sound.m_Event != eventId || sound.m_AudioEntity != audioEntity) {
            continue;
        }
        sound.StopSoundAndForget();
    }
}

// 0x4EFBF0
void CAESoundManager::CancelSoundsOfThisEventPlayingForThisEntityAndPhysical(int16 eventId, CAEAudioEntity* audioEntity, CPhysical* physical) {
    for (CAESound& sound : m_VirtuallyPlayingSoundList) {
        if (!sound.IsActive() || sound.m_Event != eventId || sound.m_AudioEntity != audioEntity || sound.m_PhysicalEntity != physical) {
            continue;
        }
        sound.StopSoundAndForget();
    }
}

// 0x4EFC60
void CAESoundManager::CancelSoundsInBankSlot(int16 bankSlot, bool bFullStop) {
    for (CAESound& sound : m_VirtuallyPlayingSoundList) {
        if (!sound.IsActive() || sound.m_BankSlot != bankSlot) {
            continue;
        }
        if (bFullStop) {
            sound.StopSoundAndForget();
        } else {
            sound.StopSound();
        }
    }
}

// 0x4EFCD0
void CAESoundManager::CancelSoundsOwnedByAudioEntity(CAEAudioEntity* audioEntity, bool bFullStop) {
    for (CAESound& sound : m_VirtuallyPlayingSoundList) {
        if (!sound.IsActive() || sound.m_AudioEntity != audioEntity) {
            continue;
        }
        if (bFullStop) {
            sound.StopSoundAndForget();
        } else {
            sound.StopSound();
        }
    }
}

// 0x4EF630, unused
int16 CAESoundManager::GetVirtualChannelForPhysicalChannel(int16 physicalChannel) const {
    const auto chIdx = physicalChannel - m_AudioHardwareHandle;
    if (chIdx < 0 || chIdx >= m_NumAllocatedPhysicalChannels) {
        return -1;
    }
    return m_PhysicallyPlayingSoundList[chIdx];
}

// NOTSA
CAESound* CAESoundManager::GetFreeSound(size_t* outIdx) {
    for (auto&& [i, s] : rngv::enumerate(m_VirtuallyPlayingSoundList)) {
        if (!s.IsActive()) {
            if (outIdx) {
                *outIdx = (size_t)i;
            }
            return &s;
        }
    }
    return nullptr;
}

#ifdef NOTSA_STANDALONE_RUN
// NOTSA: NOTSA_AUDIO_STATS - see Hardware/AEAudioStats.h
namespace {
uint32 SafeVptrRva(const void* obj) {
    if (!obj) {
        return 0;
    }
#ifdef NOTSA_PORTABLE
    // W-B8: no SEH on this platform, the read is not fault-protected (objects come from the sound manager's own tables); 64-bit vptr minus the image start
    return (uint32)(*reinterpret_cast<const uintptr_t*>(obj) - notsa::portable::MainImageRange().first);
#else
    uint32 v = 0;
    __try {
        v = *reinterpret_cast<const uint32*>(obj);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0xDEAD;
    }
    const auto base = reinterpret_cast<uint32>(GetModuleHandleW(nullptr));
    return v - base;
#endif
}
} // namespace

void notsa::audio_stats::Service() {
    if (!Enabled()) {
        return;
    }
    auto&       sm   = AESoundManager;
    auto&       hw   = AEAudioHardware;
    auto&       cn   = C();
    static uint32 s_calls = 0, s_peakAct = 0, s_peakPhys = 0, s_peakWant = 0, s_lastRefused = 0, s_lastDump = 0;
    static uint32 s_birth[MAX_NUM_SOUNDS]{};
    const uint32 now = CTimer::GetTimeInMS();
    ++s_calls;

    uint32 act = 0, phys = 0, stopReq = 0, want = 0, looped = 0, unanc = 0, oldest = 0, over30 = 0, virtOnlyLoop = 0;
    for (auto&& [i, snd] : rngv::enumerate(sm.m_VirtuallyPlayingSoundList)) {
        if (!snd.IsActive()) {
            s_birth[i] = 0;
            continue;
        }
        if (!s_birth[i]) {
            s_birth[i] = now ? now : 1;
        }
        const uint32 age = now - s_birth[i];
        oldest = std::max(oldest, age);
        over30 += age > 30000;
        ++act;
        phys += snd.m_IsPhysicallyPlaying != 0;
        stopReq += snd.m_HasRequestedStopped != 0;
        unanc += snd.IsUnancellable();
        const bool isLoop = sm.m_VirtualChannelSoundLoopStartTimes[i] != -1;
        looped += isLoop;
        if (isLoop && !snd.m_IsPhysicallyPlaying) {
            ++virtOnlyLoop;
        }
        if (!snd.m_IsPhysicallyPlaying && !snd.m_HasRequestedStopped && snd.m_FrameDelay == 0 && snd.m_ListenerVolume > -60.f) {
            ++want; // audible, but without a hardware channel
        }
    }
    uint32 collUsed = 0, collOne = 0, collLoop = 0;
    for (const auto& e : AudioEngine.m_CollisionAE.m_CollisionSoundList) {
        collUsed += e.Sound != nullptr;
        collOne += e.Status == COLLISION_SOUND_ONE_SHOT;
        collLoop += e.Status == COLLISION_SOUND_LOOPING;
    }
    uint32 chUsed = 0;
    for (const auto ref : sm.GetPhysicallyPlayingSoundList()) {
        chUsed += ref != -1;
    }
    s_peakAct  = std::max(s_peakAct, act);
    s_peakPhys = std::max(s_peakPhys, phys);
    s_peakWant = std::max(s_peakWant, want);

    auto& st = S();
    const bool refusedNow = cn.reqRefused > s_lastRefused;
    if (s_calls % st.Every == 0 || (refusedNow && now - s_lastDump > 2000)) {
        const auto& cam = TheCamera;
        Log("t=%u f=%u coll=%u(one %u loop %u n=%d add=%u) act=%u(pk %u) phys=%u(pk %u) chUsed=%u/%u want=%u(pk %u) stopReq=%u loop=%u(virt %u) unanc=%u oldest=%ums >30s=%u dsBufs=%u | req=%u refused=%u collFull=%u | hwCalls=%u ok=%u badCh=%u noBuf=%u setBufFail=%u createFail=%u(%08x) lockFail=%u(%08x) | dsPlayFail=%u(%08x) | "
            "paused=%d man=%d timerPause=%d loadTune=%d | effM=%.3f effF=%.3f musM=%.3f musF=%.3f nonStrF=%.3f strF=%.3f camFade=%d/%d camF=%.3f radio=%d lastChPos=%d",
            now, CTimer::GetFrameCounter(), collUsed, collOne, collLoop, AudioEngine.m_CollisionAE.m_NumActiveCollisionSounds, (uint32)cn.collAdd, act, s_peakAct, phys, s_peakPhys, chUsed, (uint32)sm.m_NumAllocatedPhysicalChannels, want, s_peakWant, stopReq, looped, virtOnlyLoop, unanc, oldest, over30, (uint32)g_numSoundChannelsUsed,
            (uint32)cn.reqOk, (uint32)cn.reqRefused, (uint32)cn.collFull,
            (uint32)cn.hwPlayCalls, (uint32)cn.hwPlayOk, (uint32)cn.hwBadChannel, (uint32)cn.hwNoBuffer, (uint32)cn.hwSetBufFail, (uint32)cn.hwCreateFail, (uint32)cn.createHr.load(), (uint32)cn.hwLockFail, (uint32)cn.lockHr.load(), (uint32)cn.dsPlayFail, (uint32)cn.playHr.load(),
            (int)sm.IsPaused(), (int)sm.m_IsManuallyPaused, (int)CTimer::GetIsPaused(), (int)AudioEngine.IsLoadingTuneActive(),
            hw.m_fEffectMasterScalingFactor, hw.m_fEffectsFaderScalingFactor, hw.m_fMusicMasterScalingFactor, hw.m_fMusicFaderScalingFactor, hw.m_fNonStreamFaderScalingFactor, hw.m_fStreamFaderScalingFactor,
            (int)cam.m_bMusicFading, (int)cam.m_bMusicFadedOut, cam.m_fEffectsFaderScalingFactor, (int)AERadioTrackManager.IsRadioOn(), sm.m_NumAllocatedPhysicalChannels ? (int)sm.m_ChannelPosition[sm.m_NumAllocatedPhysicalChannels - 1] : 0);
        s_peakAct = s_peakPhys = s_peakWant = 0;
    }

    if (refusedNow && now - s_lastDump > 2000) {
        s_lastDump = now;
        // breakdown of the active sounds: (bank slot, sound id, event, owner vtable rva) -> count, oldest age
        struct Key { int bank, snd, ev; uint32 owner; bool operator==(const Key&) const = default; };
        struct Row { Key k; uint32 n; uint32 oldest; int phys; };
        std::vector<Row> rows;
        for (auto&& [i, snd] : rngv::enumerate(sm.m_VirtuallyPlayingSoundList)) {
            if (!snd.IsActive()) {
                continue;
            }
            const Key k{ (int)snd.m_BankSlot, (int)snd.m_SoundID, (int)snd.m_Event, SafeVptrRva(snd.m_AudioEntity) };
            const uint32 age = now - s_birth[i];
            auto it = std::find_if(rows.begin(), rows.end(), [&](const Row& r) { return r.k == k; });
            if (it == rows.end()) {
                rows.push_back({ k, 1, age, snd.m_IsPhysicallyPlaying != 0 });
            } else {
                ++it->n;
                it->oldest = std::max(it->oldest, age);
                it->phys += snd.m_IsPhysicallyPlaying != 0;
            }
        }
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.n > b.n; });
        Log("  [table full] %zu distinct (bank,sound,event,ownerVtblRva): ", rows.size());
        for (size_t r = 0; r < rows.size() && r < 14; r++) {
            Log("    bank=%d snd=%d ev=%d owner=%#x  n=%u phys=%d oldest=%ums", rows[r].k.bank, rows[r].k.snd, rows[r].k.ev, rows[r].k.owner, rows[r].n, rows[r].phys, rows[r].oldest);
        }
    }
    s_lastRefused = cn.reqRefused;
}
#endif
