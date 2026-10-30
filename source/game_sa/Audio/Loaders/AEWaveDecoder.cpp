#include "StdInc.h" // TODO: Remove

#include <cmath>
#include <cstdio>

#include "AEWaveDecoder.h"

// 0x503250
CAEWaveDecoder::CAEWaveDecoder(CAEDataStream* dataStream) : CAEStreamingDecoder(dataStream), m_bInitialized(false), _dataStreamCopy(dataStream) {
    // NOP
}

// 0x5032b0
bool CAEWaveDecoder::Initialise() {
#ifdef FIX_BUGS
    // Robust variant (not the exe): accepts mono (expanded to stereo in FillBuffer) and skips unknown chunks in front of "data"
    // RIFF<size>WAVEfmt<space><WAVEFORMAT><WORD>
    constexpr size_t HEADER_READ_SIZE = 36;
    size_t           readed = m_dataStream->FillBuffer(fileHeaderArray, HEADER_READ_SIZE);

    if (readed == HEADER_READ_SIZE && bitsPerSample == 16 && (channelCount == 1 || channelCount == 2)) {
        // Look for "data" tag
        char   tag[4] = {0, 0, 0, 0};
        uint32 size;

        while (true) {
            if (m_dataStream->FillBuffer(tag, sizeof(tag)) != sizeof(tag))
                // EOF
                return false;
            if (m_dataStream->FillBuffer(&size, sizeof(uint32)) != sizeof(uint32))
                return false;

            if (memcmp(tag, "data", sizeof(tag)) == 0)
                break;

            m_dataStream->Seek(size, SEEK_CUR);
        }

        dataPosition = m_dataStream->GetCurrentPosition();
        dataChunkSize = size;
        m_bInitialized = true;

        if (GetStreamLengthMs() < 7000)
            m_bInitialized = false;
    }

    return m_bInitialized;
#else
    // 0x5032B0: the exe reads a fixed 44 byte header and accepts 16 bit STEREO only; the data starts at byte 0x2C (see GetStreamPlayTimeMs / SetCursor)
    m_bInitialized = false;
    if (_dataStreamCopy->FillBuffer(fileHeaderArray, sizeof(fileHeaderArray)) == sizeof(fileHeaderArray) && bitsPerSample == 16 && channelCount == 2) {
        m_bInitialized = true;
        if (GetStreamLengthMs() < 7000) {
            m_bInitialized = false;
        }
    }
    return m_bInitialized;
#endif
}

// 0x5032a0
size_t CAEWaveDecoder::FillBuffer(void* dest, size_t size) {
#ifdef FIX_BUGS
    switch (channelCount) {
    case 1: {
        // Read half of it
        size_t  readed = m_dataStream->FillBuffer(dest, size / 2);
        int16*  byteStream = reinterpret_cast<int16*>(dest);

        // Duplicate channel data
        for (int32 i = static_cast<int32>(readed / sizeof(int16)) - 1; i >= 0; i--)
            byteStream[i * 2] = byteStream[i * 2 + 1] = byteStream[i];

        return readed * 2;
    }
    case 2:
        // Use fast path
        return m_dataStream->FillBuffer(dest, size);
    default:
        return 0;
    }
#else
    // 0x5032A0: a plain pass-through (only stereo files are accepted by Initialise)
    return _dataStreamCopy->FillBuffer(dest, size);
#endif
}

// 0x503310
long CAEWaveDecoder::GetStreamLengthMs() {
    if (m_bInitialized) {
        // 0x503310: data bytes / (sampleRate * 4) * 1000 in x87 arithmetic: the exe assumes 4 bytes per sample (16 bit stereo) and ignores blockAlign.
        // Both operands enter as signed fild + 2^32 for "negative" values (the add rounds to the current precision), the quotient is rounded once.
        const auto ToFp = [](uint32 v) { x87d f = (x87d)(int32)v; if ((int32)v < 0) f += 4294967296.0f; return f; };
        return NOTSA_FTOL(int32, (float)(X87R(ToFp(dataChunkSize) / ToFp(sampleRate << 2))) * 1000.0f);
    }

    return -1;
}

// 0x503360
long CAEWaveDecoder::GetStreamPlayTimeMs() {
    if (m_bInitialized) {
        uint32 posByte = m_dataStream->GetCurrentPosition() - dataPosition;
        uint32 posSamples = posByte / blockAlign;
        // More precise way would be involving using GCD
        return NOTSA_FTOL(int32, (float)posSamples * 1000.0f / (float)sampleRate);
    }

    return -1;
}

// 0x5033c0
void CAEWaveDecoder::SetCursor(unsigned long pos) {
    if (m_bInitialized) {
        // The calculation is exactly in this order!
        uint32 posByte = pos * sampleRate / 1000 * blockAlign;
        m_dataStream->Seek(static_cast<long>(posByte + dataPosition), SEEK_SET);
    }
}

// 0x503300
int32 CAEWaveDecoder::GetSampleRate() {
    return m_bInitialized ? static_cast<int32>(sampleRate) : -1;
}

// 0x503280
int32 CAEWaveDecoder::GetStreamID() {
    return m_dataStream->m_nTrackId;
}

CAEWaveDecoder* CAEWaveDecoder::Constructor(CAEDataStream* dataStream) {
    this->CAEWaveDecoder::CAEWaveDecoder(dataStream);
    return this;
}

void CAEWaveDecoder::Destructor() {
    this->CAEWaveDecoder::~CAEWaveDecoder();
}

void CAEWaveDecoder::InjectHooks() {
    RH_ScopedClass(CAEWaveDecoder);
    RH_ScopedCategory("Audio/Loaders");

    RH_ScopedInstall(Constructor, 0x503250);
    RH_ScopedInstall(Destructor, 0x503290);
    RH_ScopedInstall(Initialise, 0x5032b0);
    RH_ScopedInstall(FillBuffer, 0x5032A0);
    RH_ScopedInstall(GetStreamLengthMs, 0x503310);
    RH_ScopedInstall(GetStreamPlayTimeMs, 0x503360);
    RH_ScopedInstall(SetCursor, 0x5033c0);
    RH_ScopedInstall(GetSampleRate, 0x503300);
    RH_ScopedInstall(GetStreamID, 0x503280);
}
