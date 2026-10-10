#include "StdInc.h"
#include "AEDirectSoundMute.h"

#include <extensions/utility.hpp>

#include "AEStreamingChannel.h"
#include "AEAudioUtility.h"

//! Stereo channel FX Param EQ presets [0x8CBA70]
static inline DSFXParamEq s_FXParamEqPresets[(+eBassSetting::NUM)][2]{
    {{ 0.0f, 0.0f,  0.0f }, { 0.f,   0.0f,  0.0f }}, // Preset NORMAL [Unused]
    {{ 80.f, 30.f,  4.0f }, { 180.f, 30.f,  1.5f }}, // Preset BOOST
    {{ 80.f, 36.f, -15.f }, { 80.f,  36.f, -15.f }}, // Preset CUT
};

// 0x4F2200
CAEStreamingChannel::~CAEStreamingChannel() {
    DirectSoundBufferFadeToSilence();

    m_nState              = StreamingChannelState::Stopping;
    m_lStoppingFrameCount = 0;

    if (m_pDirectSoundBuffer8) {
        m_pDirectSoundBuffer8->SetFX(0, nullptr, nullptr);
    }

    if (m_pSilenceBuffer) {
        std::exchange(m_pSilenceBuffer, nullptr)->Release();
    }

    if (m_pDirectSoundBuffer) {
        std::exchange(m_pDirectSoundBuffer, nullptr)->Release();
    }

    if (m_pStreamingDecoder) {
        delete std::exchange(m_pStreamingDecoder, nullptr);
    }

    if (m_pNextStreamingDecoder) {
        delete std::exchange(m_pNextStreamingDecoder, nullptr);
    }
}

// 0x4F22F0
void CAEStreamingChannel::Initialise() {
    VERIFY(SUCCEEDED(CoInitialize(nullptr)));

    DSBUFFERDESC bufferDesc{};
    bufferDesc.guid3DAlgorithm = GUID_NULL;
    bufferDesc.lpwfxFormat = &m_WaveFormat;
    bufferDesc.dwSize = 36;
    bufferDesc.dwBufferBytes = 0xC0000;
    bufferDesc.dwReserved = 0;
    bufferDesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLFX | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;

    m_WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
    m_WaveFormat.cbSize = 0;
    m_WaveFormat.nChannels = 2;
    m_WaveFormat.wBitsPerSample = 16;
    m_WaveFormat.nSamplesPerSec = 48000;
    m_WaveFormat.nAvgBytesPerSec = 192000;
    m_WaveFormat.nBlockAlign = 4;

    if (SUCCEEDED(m_pDirectSound->CreateSoundBuffer(
        &bufferDesc,
        &m_pDirectSoundBuffer,
        0
    ))) {
        NOTSA_AUDIO_MUTE_ATTACH(m_pDirectSoundBuffer); // NOTSA: NOTSA_STANDALONE_MUTE (run build only)
        m_bInitialized = true;
        SetOriginalFrequency(m_WaveFormat.nSamplesPerSec);
        m_pBuffer = m_aBuffer;
        m_bLooped = true;
    }
    InitialiseSilence();
}

// 0x4F1C70
void CAEStreamingChannel::InitialiseSilence() {
    DSBUFFERDESC bufferDesc{};
    bufferDesc.dwSize = 36;
    bufferDesc.lpwfxFormat = &m_WaveFormat;
    bufferDesc.dwBufferBytes = 0x8000;
    bufferDesc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_LOCSOFTWARE;

    if (SUCCEEDED(m_pDirectSound->CreateSoundBuffer(&bufferDesc, &m_pSilenceBuffer, 0))) {
        void *audioPtr;
        DWORD audioPtrBytes;

        m_pSilenceBuffer->Lock(0, 0, &audioPtr, &audioPtrBytes, nullptr, 0, DSBLOCK_ENTIREBUFFER);
        memset(audioPtr, 0, audioPtrBytes);
        m_pSilenceBuffer->Unlock(audioPtr, audioPtrBytes, nullptr, 0);
        m_pSilenceBuffer->Play(0, 0, DSBPLAY_LOOPING);

    } else {
        NOTSA_LOG_WARN("Creating silence buffer failed.");
    }
}

// 0x4F1FF0
void CAEStreamingChannel::SetReady() {
    switch (m_nState) {
    case StreamingChannelState::Stopping:
        AESmoothFadeThread.CancelFade(m_pDirectSoundBuffer);
        m_pDirectSoundBuffer->Stop();
        m_nState = StreamingChannelState::Stopped;

        break;
    case StreamingChannelState::Stopped:
        if (m_pStreamingDecoder)
            m_nState = StreamingChannelState::UNK_MINUS_2;
        break;
    }
}

// 0x4F1F30
void CAEStreamingChannel::SetBassEQ(eBassSetting mode, float gain) {
    if (mode == eBassSetting::NORMAL) {
        if (m_bEQEnabled) {
            RemoveFX();
        }
        return;
    }

    if (!m_bEQEnabled && !AddFX()) {
        return;
    }

    for (auto ch = 0; ch < 2; ch++) {
        IDirectSoundFXParamEq* fxEqParam;
        if (FAILED(m_pDirectSoundBuffer8->GetObjectInPath(
            GUID_All_Objects,
            ch,
            IID_IDirectSoundFXParamEq,
            reinterpret_cast<LPVOID*>(&fxEqParam)
        ))) {
            continue;
        }

        DSFXParamEq savedFxExParam{ s_FXParamEqPresets[(+mode)][ch] };
        savedFxExParam.fGain *= gain;
        VERIFY(SUCCEEDED(fxEqParam->SetAllParameters(&savedFxExParam)));
        fxEqParam->Release();
    }
}

// 0x4F2060
void CAEStreamingChannel::SetFrequencyScalingFactor(float factor) {
    if (factor == 0.0f) {
        if (!m_pDirectSoundBuffer || m_nState == StreamingChannelState::Paused || !IsBufferPlaying())
            return;

        DirectSoundBufferFadeToSilence();
        m_nState = StreamingChannelState::Paused;
    } else {
        SetFrequency(static_cast<uint32>((float)m_nOriginalFrequency * factor));

        if (m_nState != StreamingChannelState::Paused)
            return;

        if (!m_pDirectSoundBuffer)
            return;

        m_pDirectSoundBuffer->SetVolume(-10'000);
        m_pDirectSoundBuffer->Play(0, 0, m_bLooped ? DSBPLAY_LOOPING : 0);

        if (!AESmoothFadeThread.RequestFade(m_pDirectSoundBuffer, m_Volume, 35, true))
            m_pDirectSoundBuffer->SetVolume(static_cast<int32>(m_Volume * 100.0f));

        m_nState = StreamingChannelState::Started;
    }
}

// 0x4F1870
void CAEStreamingChannel::SynchPlayback() {
    // NOP
}

// 0x4F1E20
uint32 CAEStreamingChannel::FillBuffer(void* buffer, uint32 size) {
    auto filled = m_pStreamingDecoder->FillBuffer(buffer, size);
    if (filled < size) {
        if (m_bLoopTrack) {
            while (filled < size) {
                m_pStreamingDecoder->SetCursor(1);
                filled += m_pStreamingDecoder->FillBuffer((uint8*)buffer + filled, size - filled);
            }
        } else {
            if (m_pNextStreamingDecoder) {
                if (m_pNextStreamingDecoder->GetSampleRate() == m_pStreamingDecoder->GetSampleRate()) {
                    if (m_pStreamingDecoder)
                        delete m_pStreamingDecoder;

                    m_pStreamingDecoder = m_pNextStreamingDecoder;
                    m_pNextStreamingDecoder = nullptr;

                    filled += m_pStreamingDecoder->FillBuffer((uint8*)buffer + filled, size - filled);
                } else {
                    memset((uint8*)buffer + filled, 0, size - filled);
                    filled = size;
                    m_bSilenced = true;
                }
            } else {
                m_nState = StreamingChannelState::Finished;
            }
        }
    }

    m_nStreamPlayTimeMs = m_pStreamingDecoder->GetStreamLengthMs();
    return filled;
}

// 0x4F23D0
int32 CAEStreamingChannel::PrepareStream(CAEStreamingDecoder* decoder, int8 soundFlags, bool resetState) {
    if (!decoder || !m_pDirectSoundBuffer) {
        return -1;
    }

    if (resetState) {
        switch (m_nState) {
        case StreamingChannelState::UNK_MINUS_3:
        case StreamingChannelState::Finished:
        case StreamingChannelState::Started:
            DirectSoundBufferFadeToSilence();
            break;
        default:
            break;
        }
        m_nState = StreamingChannelState::Stopped;
    }

    if (m_pStreamingDecoder) {
        delete std::exchange(m_pStreamingDecoder, nullptr);
    }

    m_bLoopTrack        = (soundFlags & 1) != 0;
    m_pStreamingDecoder = decoder;

    void*  audioPtr{};
    DWORD  audioBytes{};
    if (SUCCEEDED(m_pDirectSoundBuffer->Lock(0, 0, &audioPtr, &audioBytes, nullptr, nullptr, DSBLOCK_ENTIREBUFFER))) {
        const auto filled = FillBuffer(audioPtr, audioBytes);
        if (filled == audioBytes) {
            m_bNeedToFinish = false;
        } else {
            std::memset((uint8*)audioPtr + filled, 0, audioBytes - filled);
            m_bNeedToFinish = true;
        }

        // Make sure there's no complete silence (Sets the lowest bit of both channels)
        for (auto i = 0u; i < audioBytes / sizeof(uint32); i++) {
            ((uint32*)audioPtr)[i] |= 0x10001u;
        }

        m_pDirectSoundBuffer->Unlock(audioPtr, audioBytes, nullptr, 0);
    }

    switch (m_nState) {
    case StreamingChannelState::UNK_MINUS_3:
    case StreamingChannelState::Started:
    case StreamingChannelState::Paused:
        break;
    default:
        m_nState = StreamingChannelState::UNK_MINUS_2;
        break;
    }

    m_lastSlot = 0;
    SetOriginalFrequency(m_pStreamingDecoder->GetSampleRate());
    m_bSilenced   = false;
    m_bNeedSwitch = false;
    m_bShouldStop = false;
    m_pDirectSoundBuffer->SetCurrentPosition(0);

    return 0;
}

// 0x4F1DE0
void CAEStreamingChannel::SetNextStream(CAEStreamingDecoder* decoder) {
    if (m_pNextStreamingDecoder) {
        delete m_pNextStreamingDecoder;
    }
    m_pNextStreamingDecoder = decoder;
}

// 0x4F1A60
int32 CAEStreamingChannel::GetPlayingTrackID() {
    switch (m_nState) {
    case StreamingChannelState::UNK_MINUS_3:
    case StreamingChannelState::Finished:
    case StreamingChannelState::Started:
    case StreamingChannelState::Stopping:
        if (m_pStreamingDecoder)
            return m_pStreamingDecoder->GetStreamID();

        [[fallthrough]];
    default:
        return -1;
    }
}

// 0x4F1A40
int32 CAEStreamingChannel::GetActiveTrackID() {
    return m_pStreamingDecoder ? m_pStreamingDecoder->GetStreamID() : -1;
}

// 0x4F18A0
int32 CAEStreamingChannel::UpdatePlayTime() {
    if (m_nState != StreamingChannelState::Started && m_nState != StreamingChannelState::UNK_MINUS_3) {
        m_nPlayTime = static_cast<int32>(m_nState);
        return m_nPlayTime;
    }

    if (CAEAudioUtility::GetCurrentTimeInMS() - m_nLastUpdateTime < 1'000) {
        return GetPlayTime();
    }

    DWORD currentPlayCursor;
    m_pDirectSoundBuffer->GetCurrentPosition(&currentPlayCursor, nullptr);
    auto freqFactor = m_nFrequency * ExeRecip(250.f);
    auto upperLimit = 393216.f / freqFactor; //TODO: Magic number (393.216 khz can be found often with oscilators / clock generators, also that's 0x60000 in hex)
    auto curStep    = currentPlayCursor / freqFactor;
    if ((uint32(freqFactor) / 0x60000) > 0) {
        if (m_lastWrittenSlot == 1) {
            m_nPlayTime = static_cast<int32>(m_nStreamPlayTimeMs + curStep - (2 * upperLimit));
        } else {
            m_nPlayTime = static_cast<int32>(m_nStreamPlayTimeMs + curStep - upperLimit);
        }
    } else if (m_lastWrittenSlot == 0) {
        m_nPlayTime = static_cast<int32>(m_nStreamPlayTimeMs + curStep - upperLimit);
    } else {
        m_nPlayTime = static_cast<int32>(m_nStreamPlayTimeMs + curStep);
    }

    if (m_nPlayTime >= upperLimit) {
        m_nPlayTime -= static_cast<int32>(upperLimit);
    }

    m_nLastUpdateTime = CAEAudioUtility::GetCurrentTimeInMS();
    m_nPlayTime       = std::max(0, m_nPlayTime); // Originally a bitwise hack: x &= (x <= 0) - 1
    return m_nPlayTime;
}

// 0x4F1AE0
bool CAEStreamingChannel::AddFX() {
    if (m_bEQEnabled)
        return true;

    DSEFFECTDESC effectDesc[2]{};
    for (auto& desc : effectDesc) {
        desc.guidDSFXClass = GUID_DSFX_STANDARD_PARAMEQ;
        desc.dwFlags = 0;
        desc.dwReserved1 = desc.dwReserved2 = 0;
        desc.dwSize = 32;
    }

    bool wasStopped = false;
    if (bufferStatus.Bit0x1) {
        m_pDirectSoundBuffer->Stop();
        wasStopped = true;
    }

    if (FAILED(m_pDirectSoundBuffer8->SetFX(
        std::size(effectDesc),
        effectDesc,
        nullptr
    )))
        return false;

    for (auto i = 0; i < 2; i++) { // Set both channels
        IDirectSoundFXParamEq* fxParamEq{};
        m_pDirectSoundBuffer8->GetObjectInPath(
            GUID_All_Objects,
            i,
            IID_IDirectSoundFXParamEq,
            reinterpret_cast<LPVOID*>(&fxParamEq)
        );
        fxParamEq->SetAllParameters(&s_FXParamEqPresets[0][i]);
        fxParamEq->Release();
    }

    if (wasStopped)
        m_pDirectSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);

    m_bEQEnabled = true;
    return true;
}

// 0x4F1C20
void CAEStreamingChannel::RemoveFX() {
    bool stopped{false};
    if (bufferStatus.Bit0x1) {
        m_pDirectSoundBuffer->Stop();
        stopped = true;
    }

    m_pDirectSoundBuffer8->SetFX(0, nullptr, nullptr);

    if (stopped)
        m_pDirectSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);

    m_bEQEnabled = false;
}

// 0x4F2040
bool CAEStreamingChannel::IsSoundPlaying() {
    switch (m_nState) {
    case StreamingChannelState::UNK_MINUS_3:
    case StreamingChannelState::Finished:
    case StreamingChannelState::Stopping:
    case StreamingChannelState::Started:
        return true;
    default:
        return false;
    }
}

// 0x4F19E0
int16 CAEStreamingChannel::GetPlayTime() {
    switch (m_nState) {
    case StreamingChannelState::Started:
    case StreamingChannelState::UNK_MINUS_3:
        if (CAEAudioUtility::GetCurrentTimeInMS() - m_nLastUpdateTime >= 1000u)
            return UpdatePlayTime();
        else
            return static_cast<int16>(m_nPlayTime - static_cast<uint32>(m_nLastUpdateTime) + CAEAudioUtility::GetCurrentTimeInMS());
        break;
    default:
        break;
    }

    return static_cast<int16>(m_nState); // possibly not intended.
}

// 0x4F1880
uint16 CAEStreamingChannel::GetLength() {
    return static_cast<uint16>(m_pStreamingDecoder ? m_pStreamingDecoder->GetStreamLengthMs() : -1);
}

// 0x4F1D40
void CAEStreamingChannel::Play(int16 startOffsetMs, int8 soundFlags, float freqFac) {
    if (!m_pStreamingDecoder || !m_pDirectSoundBuffer)
        return;

    SetOriginalFrequency(m_pStreamingDecoder->GetSampleRate());

    if (m_nState != StreamingChannelState::Paused) {
        m_bLoopTrack = (soundFlags & 1) != 0;
    }

    if (m_nState == StreamingChannelState::Stopping) {
        AESmoothFadeThread.CancelFade(m_pDirectSoundBuffer);
        m_pDirectSoundBuffer->Stop();

        m_nState = StreamingChannelState::Stopped;
    }

    m_nState = StreamingChannelState::Started;
    m_pDirectSoundBuffer->SetVolume(static_cast<int32>(m_Volume * 100.0f));
    m_pDirectSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
}

// 0x4F2170
void CAEStreamingChannel::Pause() {
    if (!m_pDirectSoundBuffer)
        return;

    DirectSoundBufferFadeToSilence();

    m_nState = StreamingChannelState::Paused;
}

// 0x4F1A90
void CAEStreamingChannel::Stop(bool bUpdateState) {
    DirectSoundBufferFadeToSilence();

    if (bUpdateState) {
        m_nState = StreamingChannelState::Stopping;
        m_lStoppingFrameCount = 0;
    }
}

// unused
// 0x4F21C0
void CAEStreamingChannel::Stop() {
    DirectSoundBufferFadeToSilence();
    m_nState = StreamingChannelState::Stopping;
    m_lStoppingFrameCount = 0;
}

// 0x4F2550
void CAEStreamingChannel::Service() {
    constexpr uint32 SLOT_SIZE = 0x60000; // Size of one slot of the DirectSound buffer (which has 2 of them)

    UpdateStatus();

    if (IsBufferPlaying()) {
        switch (m_nState) {
        case StreamingChannelState::Started:
            m_nState = StreamingChannelState::UNK_MINUS_3;
            break;
        case StreamingChannelState::UNK_MINUS_3:
        case StreamingChannelState::Finished:
            break;
        case StreamingChannelState::Stopping:
            if (++m_lStoppingFrameCount > 6) {
                m_pDirectSoundBuffer->Stop();
            }
            break;
        default:
            m_lStoppingFrameCount = 0;
            if (m_nState == StreamingChannelState::Stopped) { // UNK_MINUS_2 is ignored
                m_pDirectSoundBuffer->Stop();
            }
            break;
        }
    } else if (m_nState == StreamingChannelState::Stopping) {
        m_nState = StreamingChannelState::Stopped;
    }

    if (m_nState == StreamingChannelState::UNK_MINUS_3 && m_nBufferStatus == 0) {
        m_nState = StreamingChannelState::Paused;
    }

    switch (m_nState) {
    case StreamingChannelState::UNK_MINUS_3:
    case StreamingChannelState::Finished:
    case StreamingChannelState::Paused:
        break;
    default:
        return;
    }

    if ((uint32)m_pStreamingDecoder->GetSampleRate() != m_nOriginalFrequency) {
        SetOriginalFrequency(m_pStreamingDecoder->GetSampleRate());
    }

    DWORD playCursor;
    m_pDirectSoundBuffer->GetCurrentPosition(&playCursor, nullptr);

    // Only do something when the play cursor has moved to a different slot
    const auto playingSlot = (uint8)(playCursor / SLOT_SIZE);
    if (playingSlot == m_lastSlot) {
        return;
    }

    uint32 filled = 0; // Number of bytes of the slot to be filled with data from `m_pBuffer`
    if (m_nState == StreamingChannelState::Finished) { // 0x4F266D
        if (m_bShouldStop) {
            m_pDirectSoundBuffer->Stop();
            m_nState      = StreamingChannelState::Stopped;
            m_bShouldStop = false;
            return;
        }
        m_bShouldStop = true; // Fill the slot with silence, and stop once we reach it
    } else if (m_bSilenced) { // 0x4F275B
        m_bSilenced   = false;
        m_bNeedSwitch = true;
    } else if (m_bNeedSwitch) { // 0x4F2770
        const auto next = std::exchange(m_pNextStreamingDecoder, nullptr);
        m_nState = StreamingChannelState::Paused;
        Stop(false);
        PrepareStream(next, 0, false);
        Play(0, 0, 1.0f);
        return;
    } else if (m_bNeedToFinish) { // 0x4F27BB
        m_nState = StreamingChannelState::Finished;
        return;
    } else {
        filled           = FillBuffer(m_pBuffer, SLOT_SIZE);
        m_lastWrittenSlot = playingSlot;
    }

    // 0x4F2695 - Copy the data into the slot
    void* audioPtr{};
    DWORD audioBytes{};
    if (SUCCEEDED(m_pDirectSoundBuffer->Lock(m_lastSlot * SLOT_SIZE, SLOT_SIZE, &audioPtr, &audioBytes, nullptr, nullptr, 0))) {
        if (filled > 0) {
            std::memcpy(audioPtr, m_pBuffer, std::min<uint32>(filled, audioBytes));
        }
        std::memset((uint8*)audioPtr + filled, 0, SLOT_SIZE - filled); // BUG: Should be limited by `audioBytes`

        // Make sure there's no complete silence (Sets the lowest bit of both channels)
        for (auto i = 0u; i < audioBytes / sizeof(uint32); i++) {
            ((uint32*)audioPtr)[i] |= 0x10001u;
        }

        m_pDirectSoundBuffer->Unlock(audioPtr, audioBytes, nullptr, 0);
    }
    m_lastSlot = playingSlot;
}

void CAEStreamingChannel::InjectHooks() {
    RH_ScopedVirtualClass(CAEStreamingChannel, 0x85F3F0, 9);
    RH_ScopedCategory("Audio/Hardware");

    RH_ScopedInstall(Constructor, 0x4F1800);
    RH_ScopedInstall(Destructor, 0x4F2200);
    RH_ScopedInstall(SynchPlayback, 0x4F1870);
    RH_ScopedInstall(PrepareStream, 0x4F23D0);
    RH_ScopedInstall(Initialise, 0x4F22F0);
    RH_ScopedInstall(Pause, 0x4F2170);
    RH_ScopedInstall(SetReady, 0x4F1FF0);
    RH_ScopedInstall(SetBassEQ, 0x4F1F30);
    RH_ScopedInstall(FillBuffer, 0x4F1E20);
    RH_ScopedInstall(InitialiseSilence, 0x4F1C70);
    RH_ScopedInstall(SetNextStream, 0x4F1DE0);
    RH_ScopedInstall(AddFX, 0x4F1AE0);
    RH_ScopedOverloadedInstall(Stop, "class", 0x4F1A90, void(CAEStreamingChannel::*)(bool));
    RH_ScopedInstall(GetPlayingTrackID, 0x4F1A60);
    RH_ScopedInstall(GetActiveTrackID, 0x4F1A40);
    RH_ScopedInstall(UpdatePlayTime, 0x4F18A0);
    RH_ScopedInstall(RemoveFX, 0x4F1C20);
    RH_ScopedVMTInstall(Service, 0x4F2550);
    RH_ScopedVMTInstall(IsSoundPlaying, 0x4F2040);
    RH_ScopedVMTInstall(GetPlayTime, 0x4F19E0);
    RH_ScopedVMTInstall(GetLength, 0x4F1880);
    RH_ScopedVMTInstall(Play, 0x4F1D40);
    RH_ScopedVMTOverloadedInstall(Stop, "virtual", 0x4F21C0, void(CAEStreamingChannel::*)());
    RH_ScopedVMTInstall(SetFrequencyScalingFactor, 0x4F2060);
}
