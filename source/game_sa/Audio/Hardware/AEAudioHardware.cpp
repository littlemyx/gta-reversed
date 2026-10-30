#include "StdInc.h"

#include "AEAudioHardware.h"
#include "AEMP3BankLoader.h"
#include "AEMP3TrackLoader.h"
#include "AEAudioEnvironment.h"
#include "AEAudioUtility.h"
#include "AEStaticChannel.h"
#include "AEUserRadioTrackManager.h"
#include "AESmoothFadeThread.h"
#include "AEAudioTap.h"
#include "AEAudioStats.h"

NOTSA_GLOBAL(AEAudioHardware, 0xB5F8B8, (CAEAudioHardware), {}); VALIDATE_SIZE(AEAudioHardware, 4116);

void CAEAudioHardware::InjectHooks() {
    RH_ScopedClass(CAEAudioHardware);
    RH_ScopedCategory("Audio/Hardware");

    RH_ScopedInstall(Constructor, 0x4D83E0);
    RH_ScopedInstall(Destructor, 0x4D83A0);

    RH_ScopedInstall(PlaySound, 0x4D86B0);
    RH_ScopedInstall(AllocateChannels, 0x5B9340);
    RH_ScopedInstall(RequestVirtualChannelSoundInfo, 0x4D8E60);
    RH_ScopedInstall(Query3DSoundEffects, 0x4D8490);
    RH_ScopedInstall(GetNumAvailableChannels, 0x4D8810);
    RH_ScopedInstall(GetChannelPlayTimes, 0x4D8820);
    RH_ScopedInstall(SetChannelVolume, 0x4D8870);
    RH_ScopedInstall(LoadSoundBank, 0x4D88A0);
    RH_ScopedInstall(IsSoundBankLoaded, 0x4D88C0);
    RH_ScopedInstall(GetSoundBankLoadingStatus, 0x4D88D0);
    RH_ScopedInstall(LoadSound, 0x4D8ED0);
    RH_ScopedInstall(IsSoundLoaded, 0x4D8EF0);
    RH_ScopedInstall(GetSoundLoadingStatus, 0x4D8F00);
    RH_ScopedInstall(StopSound, 0x4D88E0);
    RH_ScopedInstall(SetChannelPosition, 0x4D8920);
    RH_ScopedInstall(SetChannelFrequencyScalingFactor, 0x4D8960);
    RH_ScopedInstall(RescaleChannelVolumes, 0x4D8990);
    RH_ScopedInstall(UpdateReverbEnvironment, 0x4D8DA0);
    RH_ScopedInstall(GetSoundHeadroom, 0x4D8E30);
    RH_ScopedInstall(EnableEffectsLoading, 0x4D8E40);
    RH_ScopedInstall(DisableEffectsLoading, 0x4D8E50);
    RH_ScopedInstall(GetVirtualChannelSoundLengths, 0x4D8E90);
    RH_ScopedInstall(GetVirtualChannelSoundLoopStartTimes, 0x4D8EB0);
    RH_ScopedInstall(PlayTrack, 0x4D8F10);
    RH_ScopedInstall(StartTrackPlayback, 0x4D8F30);
    RH_ScopedInstall(StopTrack, 0x4D8F50);
    RH_ScopedInstall(GetTrackPlayTime, 0x4D8F60);
    RH_ScopedInstall(GetTrackLengthMs, 0x4D8F70);
    RH_ScopedInstall(GetActiveTrackID, 0x4D8F80);
    RH_ScopedInstall(GetPlayingTrackID, 0x4D8F90);
    RH_ScopedInstall(GetBeatInfo, 0x4D8FA0);
    RH_ScopedInstall(SetBassSetting, 0x4D94A0);
    RH_ScopedInstall(DisableBassEq, 0x4D94D0);
    RH_ScopedInstall(EnableBassEq, 0x4D94E0);
    RH_ScopedInstall(SetChannelFlags, 0x4D9500);
    RH_ScopedInstall(SetMusicMasterScalingFactor, 0x4D9530);
    RH_ScopedInstall(SetEffectsMasterScalingFactor, 0x4D9540);
    RH_ScopedInstall(SetMusicFaderScalingFactor, 0x4D9550);
    RH_ScopedInstall(SetEffectsFaderScalingFactor, 0x4D9560);
    RH_ScopedInstall(GetEffectsMasterScalingFactor, 0x4D9570);
    RH_ScopedInstall(GetMusicMasterScalingFactor, 0x4D9580);
    RH_ScopedInstall(GetEffectsFaderScalingFactor, 0x4D9590);
    RH_ScopedInstall(SetNonStreamFaderScalingFactor, 0x4D95A0);
    RH_ScopedInstall(SetStreamFaderScalingFactor, 0x4D95B0);
    RH_ScopedInstall(IsStreamingFromDVD, 0x4D95C0);
    RH_ScopedInstall(GetDVDDriveLetter, 0x4D95D0);
    RH_ScopedInstall(CheckDVD, 0x4D95E0);
    RH_ScopedInstall(PauseAllSounds, 0x4D95F0);
    RH_ScopedInstall(ResumeAllSounds, 0x4D9630);
    RH_ScopedInstall(InitDirectSoundListener, 0x4D9640);
    RH_ScopedInstall(Terminate, 0x4D97A0);
    RH_ScopedInstall(Service, 0x4D9870);
    RH_ScopedInstall(Initialise, 0x4D9930);
}

// 0x4D83E0
CAEAudioHardware::CAEAudioHardware() {
    rng::fill(m_afChannelVolumes, -1000.0f);
}

// 0x4D9930
bool CAEAudioHardware::Initialise() {
    m_pMP3BankLoader  = new CAEMP3BankLoader;
    m_pMP3TrackLoader = new CAEMP3TrackLoader;
    if (!m_pMP3BankLoader->Initialise() || !m_pMP3TrackLoader->Initialise()) {
        NOTSA_LOG_ERR("Failed initializing MP3 module!");
        return false;
    }

    VERIFY(SUCCEEDED(CoInitialize(nullptr)));

#if 0 // No EAX
    if (FAILED(EAXDirectSoundCreate(
        const_cast<GUID*>(&DSDEVID_DefaultPlayback),
        reinterpret_cast<IDirectSound**>(&m_pDSDevice),
        0
    ))) {
        return false;
    }
#else
    // TODO: Use DirectSoundCreate.
    if (FAILED(DirectSoundCreate8(&DSDEVID_DefaultPlayback, &m_pDSDevice, 0))) {
        return false;
    }
#endif

    m_dsCaps.dwSize = sizeof(DSCAPS);
    m_pDSDevice->GetCaps(&m_dsCaps);

    if (FAILED(m_pDSDevice->SetCooperativeLevel(PSGLOBAL(window), DSSCL_PRIORITY))
        || !InitDirectSoundListener(2, 48'000, 16))
        return false;

    m_pDSDevice->GetSpeakerConfig((LPDWORD)&m_nSpeakerConfig);

    m_pStreamingChannel = new CAEStreamingChannel(m_pDSDevice, 0);
    m_pStreamingChannel->Initialise();

    const uint32 numFree3dBuffers = m_dsCaps.dwFreeHw3DAllBuffers;
    if (numFree3dBuffers < 24) {
        m_nNumChannels = 48;
        AESmoothFadeThread.m_nNumAvailableBuffers = 48;
        m_IsHardwareMixAvailable = false;
    } else {
        m_nNumChannels = (uint16)std::min(numFree3dBuffers, 64u) - 7;
        m_IsHardwareMixAvailable = true;
        AESmoothFadeThread.m_nNumAvailableBuffers = 7;
    }

    m_aChannels[0] = m_pStreamingChannel;
    for (auto i = 1u; i < m_nNumChannels; i++) {
        m_aChannels[i] = new CAEStaticChannel(m_pDSDevice, i, m_IsHardwareMixAvailable, 44'100, 16);
    }

    m_pStreamingChannel->SetVolume(-100.0f);
    m_awChannelFlags[0] = 55; // todo: flag

    if (FrontEndMenuManager.m_bTracksAutoScan) {
        AEUserRadioTrackManager.ScanUserTracks();

        // Wait other threads until scanning is done.
        auto& state = AEUserRadioTrackManager.m_nUserTracksScanState;
        // 0x4D9B90-0x4D9BAE: the state is read once per pass; COMPLETE/ERROR are reset to OFF (also when the scan had already ended before the first read),
        //                    the loop only sleeps and goes round again while the read value was IN_PROGRESS. The port's `while (state == IN_PROGRESS)` never reached its switch.
        for (;;) {
            const auto scanState = state;
            if (scanState == USER_TRACK_SCAN_COMPLETE || scanState == USER_TRACK_SCAN_ERROR) {
                state = USER_TRACK_SCAN_OFF; // Set off after ended up complete or failed.
            }
            if (scanState != USER_TRACK_SCAN_IN_PROGRESS) {
                break;
            }
            Sleep(100);
        }
    }

    AEUserRadioTrackManager.Initialise();
    AESmoothFadeThread.Initialise();
    AESmoothFadeThread.Start();
    m_pStreamThread.Initialise(m_pStreamingChannel);
    m_pStreamThread.Start();

    m_nNumAvailableChannels = m_nNumChannels;
    m_bInitialised = true;

    return true;
}

// 0x4D9640
bool CAEAudioHardware::InitDirectSoundListener(uint32 numChannels, uint32 samplesPerSec, uint32 bitsPerSample) {
    if (!m_pDSDevice)
        return false;

    DSBUFFERDESC dsBuffDsc{
        sizeof(DSBUFFERDESC),
        DSBCAPS_CTRL3D | DSBCAPS_PRIMARYBUFFER,
        0,
        0,
        0,
    };
    LPDIRECTSOUNDBUFFER soundbuf;
    if (FAILED(m_pDSDevice->CreateSoundBuffer(&dsBuffDsc, &soundbuf, NULL))) {
        return false;
    }

    const auto nBlockAlign = numChannels * bitsPerSample / 8;
    WAVEFORMATEX wavFmt{
        .wFormatTag      = 1,
        .nChannels       = (WORD)numChannels,
        .nSamplesPerSec  = samplesPerSec,
        .nAvgBytesPerSec = samplesPerSec * nBlockAlign,
        .nBlockAlign     = (WORD)nBlockAlign,
        .wBitsPerSample  = (WORD)bitsPerSample,
        .cbSize          = 0
    };
    if (FAILED(soundbuf->SetFormat(&wavFmt))) {
#ifdef FIX_BUGS
        soundbuf->Release();
#endif
        return false;
    }

    auto& listener = m_pDirectSound3dListener;
    if (FAILED(soundbuf->QueryInterface(
        IID_IDirectSound3DListener,
        (LPVOID*)&listener
    ))) {
        soundbuf->Release();
        return false;
    }

    listener->SetPosition(0.f, 0.f, 0.f, TRUE);
    listener->SetOrientation(
        0.f, 1.f, 0.f,  // In Front
        0.f, 0.f, -1.f, // Bottom
        TRUE
    );
    listener->SetRolloffFactor(0.f, TRUE);
    listener->SetDopplerFactor(0.f, TRUE);
    listener->CommitDeferredSettings();

    soundbuf->Release();

    Query3DSoundEffects();

    return true;
}

// 0x4D97A0
void CAEAudioHardware::Terminate() {
    m_bInitialised = false;
    m_pStreamThread.Stop();
    AESmoothFadeThread.Stop();
    m_pStreamThread.WaitForExit();
    AESmoothFadeThread.WaitForExit(); // 0x4D97C8: the port never waited for the fade thread, which kept calling Service() on buffers while the channels / device below were released
    for (auto& ch : m_aChannels) { // 0x4D97D2-0x4D97F2: all 64 slots, each deleted through its virtual destructor and set to null (the port left the pointers dangling)
        delete std::exchange(ch, nullptr);
    }
    delete std::exchange(m_pMP3BankLoader, nullptr);
    delete std::exchange(m_pMP3TrackLoader, nullptr);
    SAFE_RELEASE(m_pDirectSound3dListener);
    SAFE_RELEASE(m_pDSDevice);
}

// 0x4D86B0
void CAEAudioHardware::PlaySound(int16 channel, uint16 channelSlot, uint16 soundIdInSlot, uint16 bankSlot, int16 playPosition, int16 flags, float speed) {
    NOTSA_AUDIO_STAT(hwPlayCalls); // NOTSA: NOTSA_AUDIO_STATS
    if (channel < 0 || channelSlot >= m_anNumChannelsInSlot[channel]) {
        NOTSA_AUDIO_STAT(hwBadChannel);
        return;
    }

    uint32 bufferSize{};
    uint16 sampleRate{};
    const auto buffer = m_pMP3BankLoader->GetSoundBuffer((eSoundID)soundIdInSlot, (eSoundBankSlot)bankSlot, bufferSize, sampleRate);
    if (!buffer) {
        NOTSA_AUDIO_STAT(hwNoBuffer);
        return;
    }

#if defined(NOTSA_STANDALONE_RUN) && !defined(NOTSA_PORTABLE) // MAC-W0: DirectSound vtable tap/mute are Windows debug tools
    notsa::audio_tap::NotePlaySound(bufferSize); // NOTSA: NOTSA_AUDIO_TAP
#endif
    const auto loopOffset = m_pMP3BankLoader->GetLoopOffset((eSoundID)soundIdInSlot, (eSoundBankSlot)bankSlot);

    // BUG: Off by one, `m_aChannels` has `MAX_NUM_AUDIO_CHANNELS` entries, but index == MAX_NUM_AUDIO_CHANNELS passes this check
    const auto chIdx = (uint16)(channelSlot + channel);
    if (notsa::IsFixBugs() ? chIdx >= MAX_NUM_AUDIO_CHANNELS : chIdx > MAX_NUM_AUDIO_CHANNELS) {
        NOTSA_AUDIO_STAT(hwBadChannel);
        return;
    }

    auto* const ch = static_cast<CAEStaticChannel*>(m_aChannels[chIdx]);
    if (!ch->SetAudioBuffer(buffer, bufferSize, (int16)soundIdInSlot, (int16)bankSlot, (int16)loopOffset, sampleRate)) {
        NOTSA_AUDIO_STAT(hwSetBufFail);
        return;
    }

    int32 startPos = playPosition < 0 ? 0 : playPosition;

    const int32 length = ch->GetLength();
    if ((flags & 0x8) && (int16)startPos > 0) { // Play position is a percentage
        const auto  scaled = (float)((x87d)(int16)startPos * (x87d)0.01f * (x87d)length);
        startPos = NOTSA_FTOL(int32, std::floor((x87d)scaled)); // 0x406CE0 = `floor`, then `_ftol`
    }
    if ((int16)startPos > length) {
        startPos = length;
    }

    if (m_n3dEffectsQueryResult) {
        ch->SetNotInRoom((flags & 0x200) == 0);
    }

    NOTSA_AUDIO_STAT(hwPlayOk);
    ch->Play((int16)startPos, (int8)flags, speed);

    m_awChannelFlags[(int32)channelSlot + channel] = flags;
    ch->m_nFlags = (int32)flags; // +0x28
}

// 0x5B9340
int16 CAEAudioHardware::AllocateChannels(uint16 numChannels) {
    if (!numChannels || numChannels > m_nNumAvailableChannels) {
        return -1;
    }

    size_t slot = 0;
    while (slot < MAX_NUM_AUDIO_CHANNELS && m_anNumChannelsInSlot[slot]) {
        slot += m_anNumChannelsInSlot[slot];
    }

    if (slot >= MAX_NUM_AUDIO_CHANNELS) {
        return -1;
    }

    m_anNumChannelsInSlot[slot] = numChannels;
    m_nNumAvailableChannels    -= numChannels;

    return (int16)slot;
}

// 0x4D94A0
void CAEAudioHardware::SetBassSetting(eBassSetting bassSetting, float bassGain) {
    m_BassSetting = bassSetting;
    m_BassGain    = bassGain;
    m_pStreamingChannel->SetBassEQ(bassSetting, bassGain);
}

// 0x4D8810
uint16 CAEAudioHardware::GetNumAvailableChannels() const {
    return m_nNumAvailableChannels;
}

// 0x4D8820
void CAEAudioHardware::GetChannelPlayTimes(int16 channel, int16* playTimes) {
    if (!playTimes)
        return;

    // The exe loops `numInSlot - 1` times, ascending, and skips the loop when `numInSlot <= 1` (0x4D8820: `lea edx,[num+ch-1]; cmp ch,edx; jae`;
    // `ebx = edx - ch`), i.e. the position of the LAST channel of the slot is never written (the caller's `new int16[]` keeps its garbage there).
    const uint32 ch  = (uint32)(int32)channel;
    const uint32 end = (uint32)m_anNumChannelsInSlot[channel] + ch - 1;
    if (ch >= end) {
        return;
    }
    auto* out = playTimes;
    auto* src = &m_aChannels[channel];
    for (uint32 n = end - ch; n != 0; --n) {
        *out++ = (*src++)->GetPlayTime();
    }
}

// 0x4D8870
void CAEAudioHardware::SetChannelVolume(int16 channel, uint16 channelId, float volume, uint8 a4) {
    if (channel >= 0 && channelId < m_anNumChannelsInSlot[channel])
        m_afChannelVolumes[channel + channelId] = volume;
}

// 0x4D88A0
void CAEAudioHardware::LoadSoundBank(eSoundBank bank, eSoundBankSlot slot) {
    if (!m_bDisableEffectsLoading) {
        m_pMP3BankLoader->LoadSoundBank(bank, slot);
    }
}

// 0x4D88C0
bool CAEAudioHardware::IsSoundBankLoaded(eSoundBank bank, eSoundBankSlot slot) {
    return m_pMP3BankLoader->IsSoundBankLoaded(bank, slot);
}

// 0x4D88D0
int8 CAEAudioHardware::GetSoundBankLoadingStatus(eSoundBank bank, eSoundBankSlot slot) {
    return m_pMP3BankLoader->GetSoundBankLoadingStatus(bank, slot);
}

// notsa
bool CAEAudioHardware::EnsureSoundBankIsLoaded(eSoundBank bank, eSoundBankSlot slot, bool checkLoadingTune, bool cancelSoundsInSlot) {
    if (AEAudioHardware.IsSoundBankLoaded(bank, slot)) {
        return true;
    }
    if (cancelSoundsInSlot) {
        if (AESoundManager.AreSoundsPlayingInBankSlot(slot)) {
            AESoundManager.CancelSoundsInBankSlot(slot, false);
        }
    }
    if (checkLoadingTune && AudioEngine.IsLoadingTuneActive()) {
        return false;
    }
    LoadSoundBank(bank, slot);
    return false;
}

// 0x4D8ED0
void CAEAudioHardware::LoadSound(eSoundBank bank, eSoundID sound, eSoundBankSlot slot) {
    if (!m_bDisableEffectsLoading) {
        m_pMP3BankLoader->LoadSound(bank, sound, slot);
    }
}

// 0x4D8EF0
bool CAEAudioHardware::IsSoundLoaded(eSoundBank bank, eSoundID sfx, eSoundBankSlot slot) {
    return m_pMP3BankLoader->IsSoundLoaded(bank, sfx, slot);
}

// 0x4D8F00
bool CAEAudioHardware::GetSoundLoadingStatus(eSoundBank bank, eSoundID sfx, eSoundBankSlot slot) {
    return m_pMP3BankLoader->GetSoundLoadingStatus(bank, sfx, slot);
}

// 0x4D88E0
void CAEAudioHardware::StopSound(int16 channel, uint16 channelId) const {
    if (channel >= 0 && channelId < m_anNumChannelsInSlot[channel]) {
        const auto ch = m_aChannels[channel + channelId];
        if (ch) {
            ch->Stop();
        }
    }
}

// 0x4D8920
void CAEAudioHardware::SetChannelPosition(int16 slotId, uint16 channelId, const CVector& posn, uint8 unused) const {
    if (slotId >= 0 && channelId < m_anNumChannelsInSlot[slotId]) {
        const auto ch = m_aChannels[slotId + channelId];
        if (ch) {
            ch->SetPosition(posn);
        }
    }
}

// 0x4D8960
void CAEAudioHardware::SetChannelFrequencyScalingFactor(int16 channel, uint16 channelId, float factor) {
    if (channel >= 0 && channelId < m_anNumChannelsInSlot[channel]) {
        m_afChannelsFrqScalingFactor[channel + channelId] = factor;
    }
}

// 0x4D8990
// Recompute all channel gains and push them to the hardware. Runs in 3 passes:
//  1. Mark idle channels as silent (-100.0f) and find the loudest active volume
//     per group (global + secondary).
//  2. Normalize each channel's volume by its group max (or clamp it to <= 0),
//     convert it to a linear gain (10 ^ (dB / 20)) and accumulate the sums used
//     for headroom control (channels near their end only count their remaining play time).
//  3. Scale the gains by the master scale and category faders, convert back to
//     dB (20 * log10) and push volume + frequency scaling to the hardware.
// Between passes 1 and 2 each group max is released toward the previous frame's
// value (0.5 step if that frame was slow-fadeout, else 1.2), so ducked volumes
// fade back in instead of popping.
void CAEAudioHardware::RescaleChannelVolumes() {
    float maxVolumeGlobal           = 0.0f;
    float maxVolumeSecondary        = 0.0f;
    float scaleSum                  = 0.0f;
    float fullSum                   = 0.0f;
    bool  isMaxGlobalSlowFadeout    = false;
    bool  isMaxSecondarySlowFadeout = false;

    for (auto&& [i, ch] : rngv::enumerate(GetChannels())) {
        if (!ch || ch->GetPlayTime() == -1) {
            m_afChannelVolumes[i] = -100.0f;
        } else {
            if (maxVolumeGlobal < m_afChannelVolumes[i]) {
                maxVolumeGlobal        = m_afChannelVolumes[i];
                isMaxGlobalSlowFadeout = (m_awChannelFlags[i] & FLAG_SLOW_FADEOUT) != 0;
            }
            if ((m_awChannelFlags[i] & FLAG_SECONDARY_GROUP) && maxVolumeSecondary < m_afChannelVolumes[i]) {
                maxVolumeSecondary        = m_afChannelVolumes[i];
                isMaxSecondarySlowFadeout = (m_awChannelFlags[i] & FLAG_SLOW_FADEOUT) != 0;
            }
        }
    }

    // Release the group maxima toward the previous frame's values (ducking fade-out)
    // TODO: FPS dependent logic
    if (maxVolumeGlobal < m_PrevMaxVolumeGlobal) {
        if (m_PrevMaxGlobalSlowFadeout) {
            const float vol = m_PrevMaxVolumeGlobal - 0.5f;
            if (vol > maxVolumeGlobal) {
                maxVolumeGlobal        = vol;
                isMaxGlobalSlowFadeout = true;
            }
        } else {
            const float vol = m_PrevMaxVolumeGlobal - 1.2f;
            if (vol > maxVolumeGlobal) {
                maxVolumeGlobal = vol;
            }
        }
    }

    // TODO: FPS dependent logic
    if (maxVolumeSecondary < m_PrevMaxVolumeSecondary) {
        if (m_PrevMaxSecondarySlowFadeout) {
            const float vol = m_PrevMaxVolumeSecondary - 0.5f;
            if (vol > maxVolumeSecondary) {
                maxVolumeSecondary        = vol;
                isMaxSecondarySlowFadeout = true;
            }
        } else {
            const float vol = m_PrevMaxVolumeSecondary - 1.2f;
            if (vol > maxVolumeSecondary) {
                maxVolumeSecondary = vol;
            }
        }
    }

    for (auto&& [i, ch] : rngv::enumerate(GetChannels())) {
        const auto flags = m_awChannelFlags[i];
        auto&      vol   = m_afChannelVolumes[i];

        if (flags & FLAG_CLAMP_VOL_TO_NEG) {
            vol = vol < 0.0f ? vol : 0.0f; // 0x4D8B2E: fcomp 0.0 / test ah,5 / jp -> `fld 0.0`: also for NaN (std::min keeps it)
        } else if (flags & FLAG_SECONDARY_GROUP) {
            vol -= maxVolumeSecondary;
        } else {
            vol -= maxVolumeGlobal;
        }

        // NOTE: `pow` in double precision on purpose - the original computes it on the
        // x87 FPU in temp-real precision (`FLD qword [10.0]` @ 0x4D8B58).
        const auto   gainPow = x87::pow(10.0, vol * 0.05f);
        m_ChannelGains[i]    = static_cast<float>(gainPow); // 0x4D8B6B: `fst dword` - st0 keeps the unrounded pow result

        if (flags & FLAG_UNDUCKABLE) {
            // 0x4D8B76: `fadd dword [fullSum]` adds the UNROUNDED pow result still on the x87 stack (one rounding to 24 bits), not the float stored above
            fullSum = (float)X87R((x87d)gainPow + (x87d)fullSum);
        } else if ((flags & FLAG_FADE_NEAR_END) && ch && ch->GetPlayTime() >= 0) {
            // NOTE: `GetPlayTime()` is called twice on purpose (0x4D8B9A / 0x4D8BBB) -
            // it reads live playback state, so caching it would deviate from the original.
            // NOTE: `static_cast<int16>` reproduces the original's MOVSX sign-extension (0x4D8BB2).
            const auto  length    = static_cast<int16>(ch->GetLength());
            const auto  playTime  = ch->GetPlayTime();
            const float remaining = static_cast<float>(length - playTime) * m_ChannelGains[i] / static_cast<float>(length);
            scaleSum += remaining < 0.0f ? 0.0f : std::min(remaining, m_ChannelGains[i]);
        } else {
            scaleSum += m_ChannelGains[i];
        }
    }

    m_PrevMaxVolumeGlobal         = maxVolumeGlobal;
    m_PrevMaxVolumeSecondary      = maxVolumeSecondary;
    m_PrevMaxSecondarySlowFadeout = isMaxSecondarySlowFadeout;
    m_PrevMaxGlobalSlowFadeout    = isMaxGlobalSlowFadeout;

    // NOTE: Operation order matters - the original does FSUBR -> FMUL -> FDIV, so
    // reassociating `(6.4f - fullSum * 0.8f) * 16383.0f / scaleSum` changes the result.
    // NOTE: Exact `== 0.0f` compare on purpose - matches the original's FCOMP against 0.0.
    const float masterScale = scaleSum == 0.0f
        ? 0.0f
        : std::min((6.4f - fullSum * 0.8f) * 16383.0f / scaleSum, 16383.0f);

    for (auto&& [i, ch] : rngv::enumerate(GetChannels())) {
        const auto flags = m_awChannelFlags[i];
        auto&      gain  = m_ChannelGains[i];

        gain *= (flags & FLAG_UNDUCKABLE) ? 16383.0f : masterScale;

        if (flags & FLAG_IS_MUSIC) {
            gain *= m_fMusicMasterScalingFactor * m_fMusicFaderScalingFactor;
        } else {
            gain *= m_fEffectMasterScalingFactor * m_fEffectsFaderScalingFactor;
        }

        gain *= (flags & FLAG_IS_NOT_STREAM) ? m_fNonStreamFaderScalingFactor : m_fStreamFaderScalingFactor;

        if (ch) {
            // NOTE: Exact `== 0.0f` compare on purpose - the original does FCOMP against
            // 0.0 and sends NaN gains down the log path.
            if (gain == 0.0f) {
                ch->SetVolume(-100.0f);
            } else {
                ch->SetVolume(20.0f * CAEAudioUtility::AudioLog10(gain * ExeRecip(16383.0f)));
            }
            ch->SetFrequencyScalingFactor(m_afChannelsFrqScalingFactor[i]);
        }
    }
}

// 0x4D8DA0
void CAEAudioHardware::UpdateReverbEnvironment() {
    int8 reverb;
    int32 depth;
    CAEAudioEnvironment::GetReverbEnvironmentAndDepth(&reverb, &depth);
    if ((uint8)reverb == m_nReverbEnvironment && depth == m_nReverbDepth) { // Nothing is changing? (0x4D8DBF: byte compare)
        return;
    }

    // 0x4D8DE2: the new environment is only remembered once a channel accepted it (`test al,al / jne 0x4D8E11`); if none did (or there are no channels) it is
    // retried on the next call.
    if (rng::none_of(GetChannels(), [=](CAEAudioChannel* ch) {
        return ch && ch->SetReverbAndDepth(reverb, depth); }
    )) {
        return;
    }

    m_nReverbEnvironment = reverb;
    m_nReverbDepth       = depth;
}

// 0x4D8E30
float CAEAudioHardware::GetSoundHeadroom(eSoundID sfx, eSoundBankSlot slot) {
    return m_pMP3BankLoader->GetSoundHeadroom(sfx, slot);
}

// 0x4D8E40
void CAEAudioHardware::EnableEffectsLoading() {
    m_bDisableEffectsLoading = false; // weird name
}

// 0x4D8E50
void CAEAudioHardware::DisableEffectsLoading() {
    m_bDisableEffectsLoading = true; // weird name
}

// 0x4D8E60
void CAEAudioHardware::RequestVirtualChannelSoundInfo(uint16 vch, eSoundID sfx, eSoundBankSlot slot) {
    assert(vch < MAX_NUM_SOUNDS);

    m_VirtualChannelSettings.BankSlotIDs[vch] = slot;
    m_VirtualChannelSettings.SoundIDs[vch]    = sfx;
}

// 0x4D8E90
void CAEAudioHardware::GetVirtualChannelSoundLengths(int16* soundLengths) const {
    memcpy(soundLengths, m_VirtualChannelSoundLengths, sizeof(m_VirtualChannelSoundLengths));
}

// 0x4D8EB0
void CAEAudioHardware::GetVirtualChannelSoundLoopStartTimes(int16* soundLoopStartTimes) const {
    memcpy(soundLoopStartTimes, m_VirtualChannelLoopTimes, sizeof(m_VirtualChannelLoopTimes)); // size is 600, replaced by loop?
}

// 0x4D8F10
void CAEAudioHardware::PlayTrack(uint32 trackID, int nextTrackID, uint32 startOffsetMs, uint8 trackFlags, bool bUserTrack, bool bUserNextTrack) {
    m_PlayingTrackFlags = trackFlags;
    m_pStreamThread.PlayTrack(trackID, nextTrackID, startOffsetMs, trackFlags, bUserTrack, bUserNextTrack);
}

// 0x4D8F30
void CAEAudioHardware::StartTrackPlayback() const {
    m_pStreamingChannel->Play(0, m_PlayingTrackFlags, 1.0f);
}

// 0x4D8F50
void CAEAudioHardware::StopTrack() {
    m_pStreamThread.StopTrack();
}

// 0x4D8F60
int32 CAEAudioHardware::GetTrackPlayTime() const {
    return m_pStreamThread.GetTrackPlayTime();
}

// 0x4D8F70
int32 CAEAudioHardware::GetTrackLengthMs() const {
    return m_pStreamThread.GetTrackLengthMs();
}

// 0x4D8F80
int32 CAEAudioHardware::GetActiveTrackID() const {
    return m_pStreamThread.GetActiveTrackID();
}

// 0x4D8F90
int32 CAEAudioHardware::GetPlayingTrackID() const {
    return m_pStreamThread.GetPlayingTrackID();
}

// 0x4D8FA0
void CAEAudioHardware::GetBeatInfo(tBeatInfo* beatInfo) {
    auto& bi = gBeatInfo;
    const auto ClearWindow = [&] { rng::fill(bi.BeatWindow, tTrackInfo::tBeat{}); };

    // 0x4D8FB2
    if (m_pStreamThread.GetActiveTrackID() == -1) {
        bi.IsBeatInfoPresent = 0;
        ClearWindow();
        return; // NOTE: the exe returns here WITHOUT copying `gBeatInfo` into the caller's struct (the copy is at 0x4D946F)
    }

    const auto trkInfo     = std::unique_ptr<tTrackInfo>{ m_pMP3TrackLoader->GetTrackInfo(m_pStreamThread.GetActiveTrackID()) }; // freed at 0x4D9481
    if (!trkInfo) {
        return;
    }
    int32      trkPlayTime = m_pStreamThread.GetTrackPlayTime();

    // The exe reads the 1000 beats + (up to) the 8 following 8-byte units of the struct (`field_1F40`...) with the same stride, so index like it does
    static_assert(offsetof(tTrackInfo, m_aBeats) == 0);
    const auto* const beats = reinterpret_cast<const tTrackInfo::tBeat*>(trkInfo.get());
    const auto Time = [&](int32 idx) { return (int32)beats[idx].m_nTime; };
    const auto Key  = [&](int32 idx) { return (int32)beats[idx].m_nKey; };
    const auto NumBeats = (int32)std::size(trkInfo->m_aBeats);

    if (trkPlayTime < 0 || Time(0) < 0) { // 0x4D9455 (all of these are signed compares)
        if (trkPlayTime != -7) {
            bi.IsBeatInfoPresent = 0;
            ClearWindow();
        }
    } else {
        const auto prevBeatNumber = bi.BeatNumber; // 0x4D9008
        if (prevBeatNumber >= 0) {
            trkPlayTime = std::max(trkPlayTime - 50, 0);
        }

        bi.IsBeatInfoPresent = 1;
        bi.BeatTypeThisFrame = 0;
        bi.BeatNumber        = -1;
        ClearWindow();

        // 0x4D904C - index of the first beat that is not before the (shifted) play time, stops at the first non-positive key
        int32 i = 0;
        while (i < NumBeats && Time(i) < trkPlayTime && Key(i) > 0) {
            i++;
        }

        // 0x4D9078 / 0x4D9332 - the (up to) 10 beats before `i` go to BeatWindow[9..0]
        const auto FillBeatsBefore = [&] {
            for (int32 k = 1; k <= 10; k++) {
                if (const auto idx = i - k; idx >= 0) {
                    bi.BeatWindow[10 - k] = { (uint32)(Time(idx) - trkPlayTime), beats[idx].m_nKey };
                }
            }
        };

        if (Key(i) > 0) {
            if (i != prevBeatNumber && i > 0) {
                bi.BeatTypeThisFrame = beats[i - 1].m_nKey;
            }
            FillBeatsBefore();
            for (int32 k = 0; k < 10; k++) { // 0x4D91A1: BeatWindow[10..19] = beats `i`..`i+9` (only those with a positive key, and inside the 1000 beats)
                if (i + k < NumBeats && Key(i + k) > 0) {
                    bi.BeatWindow[10 + k] = { (uint32)(Time(i + k) - trkPlayTime), beats[i + k].m_nKey };
                }
            }
            bi.BeatNumber = i;
        } else if (i > 0) {
            FillBeatsBefore();
        }
    }

    *beatInfo = bi; // 0x4D946F: rep movsd x 0x2B
}

// 0x4D94E0
void CAEAudioHardware::EnableBassEq() {
    m_pStreamingChannel->SetBassEQ(m_BassSetting, m_BassGain);
}

// 0x4D94D0
void CAEAudioHardware::DisableBassEq() {
    m_pStreamingChannel->SetBassEQ(eBassSetting::NORMAL, 0.f);
}

// 0x4D9500
void CAEAudioHardware::SetChannelFlags(int16 channel, uint16 channelId, int16 flags) {
    if (channel >= 0 && channelId < m_anNumChannelsInSlot[channel])
        m_awChannelFlags[channel + channelId] = flags;
}

// 0x4D9530
void CAEAudioHardware::SetMusicMasterScalingFactor(float factor) {
    m_fMusicMasterScalingFactor = factor;
}

// 0x4D9540
void CAEAudioHardware::SetEffectsMasterScalingFactor(float factor) {
    m_fEffectMasterScalingFactor = factor;
}

// 0x4D9550
void CAEAudioHardware::SetMusicFaderScalingFactor(float factor) {
    m_fMusicFaderScalingFactor = factor;
}

// 0x4D9560
void CAEAudioHardware::SetEffectsFaderScalingFactor(float factor) {
    m_fEffectsFaderScalingFactor = factor;
}

// 0x4D9570
float CAEAudioHardware::GetEffectsMasterScalingFactor() const {
    return m_fEffectMasterScalingFactor;
}

// 0x4D9580
float CAEAudioHardware::GetMusicMasterScalingFactor() const {
    return m_fMusicMasterScalingFactor;
}

// 0x4D9590
float CAEAudioHardware::GetEffectsFaderScalingFactor() const {
    return m_fEffectsFaderScalingFactor;
}

// 0x4D95A0
void CAEAudioHardware::SetNonStreamFaderScalingFactor(float factor) {
    m_fNonStreamFaderScalingFactor = factor;
}

// 0x4D95B0
void CAEAudioHardware::SetStreamFaderScalingFactor(float factor) {
    m_fStreamFaderScalingFactor = factor;
}

// 0x4D95C0
bool CAEAudioHardware::IsStreamingFromDVD() {
    return m_pMP3TrackLoader->m_bStreamingFromDVD;
}

// 0x4D95D0
char CAEAudioHardware::GetDVDDriveLetter() {
    return m_pMP3TrackLoader->m_pszDvdDrivePath[0];
}

// 0x4D95E0
bool CAEAudioHardware::CheckDVD() {
    return m_pMP3TrackLoader->IsCurrentAudioStreamAvailable();
}

// 0x4D95F0
void CAEAudioHardware::PauseAllSounds() {
    if (!m_bInitialised) {
        return;
    }
    for (const auto ch : GetChannels()) {
        ch->SetFrequencyScalingFactor(0.f);
    }
}

// 0x4D9630
void CAEAudioHardware::ResumeAllSounds() {
    if (!m_bInitialised) {
        return;
    }
    RescaleChannelVolumes();
}

// 0x4D8490
void CAEAudioHardware::Query3DSoundEffects() {
    DSBUFFERDESC bufferDesc;
    bufferDesc.guid3DAlgorithm = GUID_NULL;
    bufferDesc.lpwfxFormat = nullptr;
    bufferDesc.dwFlags = DSBCAPS_CTRL3D;
    bufferDesc.dwReserved = 0;
    bufferDesc.dwSize = sizeof(DSBUFFERDESC);
    bufferDesc.dwBufferBytes = 1024;

    IDirectSoundBuffer* ppDSBuffer{};
    if (FAILED(m_pDSDevice->CreateSoundBuffer(&bufferDesc, &ppDSBuffer, 0)) || !ppDSBuffer)
        return;

    IDirectSound3DBuffer* ppDS3DBuffer{};
    if (FAILED(ppDSBuffer->QueryInterface(IID_IDirectSound3DBuffer, (LPVOID*)&ppDS3DBuffer)) || !ppDS3DBuffer) {
        ppDSBuffer->Release(); // 0x4D854D: the exe returns here (without releasing, and with the result left at 0); the port used to carry on and dereference the null 3D buffer
        return;
    }

    IKsPropertySet* ppIKsPropertySet{};
    if (ppDS3DBuffer->QueryInterface(IID_IKsPropertySet, (LPVOID*)&ppIKsPropertySet) == S_OK) {
#if 0 // No EAX
        auto fxSlot0 = EAXPROPERTYID_EAX_FXSlot0;
        auto envRoom = EAX_ENVIRONMENT_ROOM;
        auto defRoom = EAXREVERB_DEFAULTROOM;
        if (
            SUCCEEDED(ppIKsPropertySet->Set(EAXPROPERTYID_EAX_Context, EAXCONTEXT_PRIMARYFXSLOTID, nullptr, 0, &fxSlot0, sizeof(fxSlot0))) &&
            SUCCEEDED(ppIKsPropertySet->Set(EAXPROPERTYID_EAX_FXSlot0, EAXREVERB_ENVIRONMENT, nullptr, 0, &envRoom, sizeof(envRoom))) &&
            SUCCEEDED(ppIKsPropertySet->Set(EAXPROPERTYID_EAX_FXSlot0, EAXREVERB_ROOM, nullptr, 0, &defRoom, sizeof(defRoom)))
        ) {
            NOTSA_LOG_DEBUG("EAX properties set successfully!");
            m_n3dEffectsQueryResult = 1;
        } else {
            NOTSA_LOG_DEBUG("EAX properties couldn't be set!");
        }
#endif
        SAFE_RELEASE(ppIKsPropertySet);
    }
    if (!m_n3dEffectsQueryResult)
        m_n3dEffectsQueryResult = 2;

    SAFE_RELEASE(ppDS3DBuffer);
    SAFE_RELEASE(ppDSBuffer);
}

// 0x4D9870
void CAEAudioHardware::Service() {
    NOTSA_AUDIO_TAP_FRAME(); // NOTSA: NOTSA_AUDIO_TAP frame time (run build only)
    VERIFY(SUCCEEDED(m_pDirectSound3dListener->CommitDeferredSettings()));
    RescaleChannelVolumes();
    if (m_n3dEffectsQueryResult) {
        UpdateReverbEnvironment();
    }
    for (const auto ch : GetChannels() | rng::views::drop(1)) {
        ch->SynchPlayback();
    }
    for (const auto ch : GetChannels() | rng::views::drop(1)) {
        ch->Service();
    }
    m_pMP3BankLoader->UpdateVirtualChannels(
        &m_VirtualChannelSettings,
        m_VirtualChannelSoundLengths,
        m_VirtualChannelLoopTimes
    );
    m_pMP3BankLoader->Service();
}

const CAEBankSlot& CAEAudioHardware::GetBankSlot(eSoundBankSlot slot) const {
    return m_pMP3BankLoader->GetBankSlot(slot);
}
