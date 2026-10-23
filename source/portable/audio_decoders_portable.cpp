// Portable (non-Windows) CAEMFDecoder / CAEWMADecoder, replacing game_sa/Audio/Loaders/AEMFDecoder.cpp and AEWMADecoder.cpp (MAC-W0).
// Built ONLY with GTASA_PORTABLE=ON.
//
// Both decoders exist for user radio tracks (Media Foundation: mp3/aac/..., WMA via the Windows Media Format SDK): Windows-only codecs behind COM.
// W-B5: there is no decoder for user tracks on this platform yet. InitLibrary()/Initialise() log once and FAIL, so CAEUserRadioTrackManager /
// CAEStreamThread treat the track as unplayable (the same path as a missing Windows codec). Replacement: libav/AudioToolbox decoder behind the
// same CAEStreamingDecoder interface (later slice, with the OpenAL hardware layer).
#include "StdInc.h"

#include "AEMFDecoder.h"
#include "AEWMADecoder.h"

namespace {
void W_B5_NoUserTrackDecoder(const char* who) {
    static bool logged = false;
    if (!std::exchange(logged, true)) {
        NOTSA_LOG_ERR("W-B5: {}: user-track decoding (Media Foundation / WMA) is not available on this platform, user tracks cannot be played", who);
    }
}
} // namespace

// ---- CAEMFDecoder ----
HMODULE CAEMFDecoder::mfPlatModule      = nullptr;
HMODULE CAEMFDecoder::mfReadWriteModule = nullptr;
HRESULT(__stdcall* CAEMFDecoder::MFCreateSourceReaderFromURL)(LPCWSTR, IMFAttributes*, IMFSourceReader**) = nullptr;
HRESULT(__stdcall* CAEMFDecoder::MFCreateMediaType)(IMFMediaType**) = nullptr;

CAEMFDecoder::CAEMFDecoder(char* filename, int32 trackID)
    : CAEStreamingDecoder(nullptr), m_pFilename(filename), m_bInitialized(false), m_pSourceReader(nullptr), m_pPcmAudio(nullptr), m_wpTempBuffer(nullptr), m_nTempBufferUsed(0), m_nPlayTime(0),
      m_nLength(0), m_nTrackID(trackID), m_nLengthMs(0), m_nSampleRate(0), m_fTimeDivider(0.0f) {}

CAEMFDecoder::~CAEMFDecoder() {
    delete[] m_wpTempBuffer;
    delete[] m_pFilename;
}

bool CAEMFDecoder::Initialise() {
    W_B5_NoUserTrackDecoder("CAEMFDecoder::Initialise");
    return false;
}
size_t CAEMFDecoder::FillBuffer(void*, size_t) { return 0; }
long   CAEMFDecoder::GetStreamLengthMs() { return 0; }
long   CAEMFDecoder::GetStreamPlayTimeMs() { return 0; }
void   CAEMFDecoder::SetCursor(unsigned long) {}
int32  CAEMFDecoder::GetSampleRate() { return 0; }
int32  CAEMFDecoder::GetStreamID() { return m_nTrackID; }

bool CAEMFDecoder::InitLibrary() {
    W_B5_NoUserTrackDecoder("CAEMFDecoder::InitLibrary");
    return false;
}
void CAEMFDecoder::Shutdown() {}
void CAEMFDecoder::InjectHooks() {} // no hook registry in the portable build

// ---- CAEWMADecoder ----
namespace {
HRESULT(__stdcall* s_WMCreateSyncReader)(IUnknown*, DWORD, IWMSyncReader**) = nullptr;
}
HRESULT(__stdcall*& CAEWMADecoder::WMCreateSyncReader)(IUnknown*, DWORD, IWMSyncReader**) = s_WMCreateSyncReader;

CAEWMADecoder::CAEWMADecoder(CAEDataStream* dataStream) : CAEStreamingDecoder(dataStream) {
    m_bInitialized    = false;
    m_nTempBufferUsed = 0;
    syncReader        = nullptr;
}

CAEWMADecoder::~CAEWMADecoder() = default; // nothing was ever opened (Initialise fails)

bool CAEWMADecoder::Initialise() {
    W_B5_NoUserTrackDecoder("CAEWMADecoder::Initialise");
    return false;
}
size_t CAEWMADecoder::FillBuffer(void*, size_t) { return 0; }
long   CAEWMADecoder::GetStreamLengthMs() { return 0; }
long   CAEWMADecoder::GetStreamPlayTimeMs() { return 0; }
void   CAEWMADecoder::SetCursor(unsigned long) {}
int32  CAEWMADecoder::GetSampleRate() { return 0; }
int32  CAEWMADecoder::GetStreamID() { return m_dataStream ? (int32)m_dataStream->m_nTrackId : 0; }

bool CAEWMADecoder::InitLibrary() {
    W_B5_NoUserTrackDecoder("CAEWMADecoder::InitLibrary");
    return false;
}
void    CAEWMADecoder::Shutdown() {}
HRESULT CAEWMADecoder::SelectStreamIndex(IWMProfile*) { return E_NOTIMPL; }
void    CAEWMADecoder::InjectHooks() {}
