#pragma once
/*
 * MAC-W0: DirectSound as TYPES for non-Windows builds, so that the audio hardware layer (CAEAudioHardware / CAEAudioChannel / CAEStaticChannel /
 * CAEStreamingChannel / CAESmoothFadeThread) compiles unchanged. The interfaces are abstract: nothing in the portable build can create an
 * implementation, because the only factory functions (DirectSoundCreate / DirectSoundCreate8 / EAXDirectSoundCreate) log and FAIL with
 * DSERR_NODRIVER. CAEAudioHardware::Initialise then returns false before any channel exists (audio disabled).
 * Real audio on this platform is the OpenAL port of the hardware layer (stream W-3/W-4, game_sa/Audio/OpenAL).
 *
 * Constant values are the Windows SDK's; GUID values are placeholders that are never interpreted (no QueryInterface can succeed).
 */
#include "objidl.h"
#include "mmsystem.h"

#include <cstdio>

// ---- results ----
#define DS_OK 0
#define DSERR_GENERIC E_FAIL
#define DSERR_INVALIDPARAM E_INVALIDARG
#define DSERR_NODRIVER ((HRESULT)0x88780078)
#define DSERR_BUFFERLOST ((HRESULT)0x88780096)
#define DSERR_UNSUPPORTED E_NOTIMPL

// ---- flags / constants ----
#define DSSCL_NORMAL 1
#define DSSCL_PRIORITY 2
#define DSBCAPS_PRIMARYBUFFER 0x00000001
#define DSBCAPS_STATIC 0x00000002
#define DSBCAPS_LOCHARDWARE 0x00000004
#define DSBCAPS_LOCSOFTWARE 0x00000008
#define DSBCAPS_CTRL3D 0x00000010
#define DSBCAPS_CTRLFREQUENCY 0x00000020
#define DSBCAPS_CTRLPAN 0x00000040
#define DSBCAPS_CTRLVOLUME 0x00000080
#define DSBCAPS_CTRLPOSITIONNOTIFY 0x00000100
#define DSBCAPS_CTRLFX 0x00000200
#define DSBCAPS_STICKYFOCUS 0x00004000
#define DSBCAPS_GLOBALFOCUS 0x00008000
#define DSBCAPS_GETCURRENTPOSITION2 0x00010000
#define DSBCAPS_MUTE3DATMAXDISTANCE 0x00020000
#define DSBPLAY_LOOPING 0x00000001
#define DSBLOCK_FROMWRITECURSOR 0x00000001
#define DSBLOCK_ENTIREBUFFER 0x00000002
#define DSBSTATUS_PLAYING 0x00000001
#define DSBSTATUS_BUFFERLOST 0x00000002
#define DSBSTATUS_LOOPING 0x00000004
#define DSBSTATUS_LOCHARDWARE 0x00000008
#define DSBSTATUS_LOCSOFTWARE 0x00000010
#define DSBSTATUS_TERMINATED 0x00000020
#define DSBVOLUME_MIN (-10000)
#define DSBVOLUME_MAX 0
#define DS3D_IMMEDIATE 0x00000000
#define DS3D_DEFERRED 0x00000001

struct DSCAPS {
    DWORD dwSize, dwFlags, dwMinSecondarySampleRate, dwMaxSecondarySampleRate, dwPrimaryBuffers, dwMaxHwMixingAllBuffers, dwMaxHwMixingStaticBuffers,
        dwMaxHwMixingStreamingBuffers, dwFreeHwMixingAllBuffers, dwFreeHwMixingStaticBuffers, dwFreeHwMixingStreamingBuffers, dwMaxHw3DAllBuffers,
        dwMaxHw3DStaticBuffers, dwMaxHw3DStreamingBuffers, dwFreeHw3DAllBuffers, dwFreeHw3DStaticBuffers, dwFreeHw3DStreamingBuffers, dwTotalHwMemBytes,
        dwFreeHwMemBytes, dwMaxContigFreeHwMemBytes, dwUnlockTransferRateHwBuffers, dwPlayCpuOverheadSwBuffers, dwReserved1, dwReserved2;
};
using LPDSCAPS = DSCAPS*;
struct DSBCAPS {
    DWORD dwSize, dwFlags, dwBufferBytes, dwUnlockTransferRate, dwPlayCpuOverhead;
};
using LPDSBCAPS = DSBCAPS*;
struct DSBUFFERDESC {
    DWORD         dwSize;
    DWORD         dwFlags;
    DWORD         dwBufferBytes;
    DWORD         dwReserved;
    LPWAVEFORMATEX lpwfxFormat;
    GUID          guid3DAlgorithm;
};
using LPDSBUFFERDESC  = DSBUFFERDESC*;
using LPCDSBUFFERDESC = const DSBUFFERDESC*;
struct DSEFFECTDESC {
    DWORD dwSize, dwFlags;
    GUID  guidDSFXClass;
    DWORD_PTR dwReserved1, dwReserved2;
};
struct DSFXParamEq {
    FLOAT fCenter, fBandwidth, fGain;
};

// ---- GUIDs (placeholders, see the header comment) ----
inline constexpr GUID IID_IDirectSound3DBuffer    = {0x279AFA86, 0x4981, 0x11CE, {0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60}};
inline constexpr GUID IID_IDirectSound3DListener  = {0x279AFA84, 0x4981, 0x11CE, {0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60}};
inline constexpr GUID IID_IKsPropertySet          = {0x31EFAC30, 0x515C, 0x11D0, {0xA9, 0xAA, 0x00, 0xAA, 0x00, 0x61, 0xBE, 0x93}};
inline constexpr GUID IID_IDirectSoundFXParamEq   = {0xC03CA9FE, 0xFE90, 0x4204, {0x80, 0x78, 0x82, 0x33, 0x4C, 0xD1, 0x77, 0xDA}};
inline constexpr GUID GUID_DSFX_STANDARD_PARAMEQ  = {0x120CED87, 0x6DA7, 0x4B16, {0x9E, 0x4E, 0xD8, 0x3C, 0x7A, 0x2D, 0xFF, 0x13}};
inline constexpr GUID DSDEVID_DefaultPlayback     = {0xDEF00000, 0x9C6D, 0x47ED, {0xAA, 0xF1, 0x4D, 0xDA, 0x8F, 0x2B, 0x5C, 0x03}};
inline constexpr GUID GUID_All_Objects            = {0xAA114DE5, 0xC262, 0x4169, {0xA1, 0xC8, 0x23, 0xD6, 0x98, 0xCC, 0x73, 0xB5}};

// ---- interfaces (abstract; method order = the DirectSound vtable order) ----
struct IKsPropertySet : IUnknown {
    STDMETHOD(Get)(REFGUID set, ULONG id, LPVOID instance, ULONG instanceLen, LPVOID data, ULONG dataLen, ULONG* returned) = 0;
    STDMETHOD(Set)(REFGUID set, ULONG id, LPVOID instance, ULONG instanceLen, LPVOID data, ULONG dataLen) = 0;
    STDMETHOD(QuerySupport)(REFGUID set, ULONG id, ULONG* support) = 0;
};
struct IDirectSoundFXParamEq : IUnknown {
    STDMETHOD(SetAllParameters)(const DSFXParamEq* params) = 0;
    STDMETHOD(GetAllParameters)(DSFXParamEq* params) = 0;
};
struct IDirectSound3DListener : IUnknown {
    STDMETHOD(GetAllParameters)(void* p) = 0;
    STDMETHOD(GetDistanceFactor)(FLOAT* p) = 0;
    STDMETHOD(GetDopplerFactor)(FLOAT* p) = 0;
    STDMETHOD(GetOrientation)(void* front, void* top) = 0;
    STDMETHOD(GetPosition)(void* p) = 0;
    STDMETHOD(GetRolloffFactor)(FLOAT* p) = 0;
    STDMETHOD(GetVelocity)(void* p) = 0;
    STDMETHOD(SetAllParameters)(const void* p, DWORD apply) = 0;
    STDMETHOD(SetDistanceFactor)(FLOAT v, DWORD apply) = 0;
    STDMETHOD(SetDopplerFactor)(FLOAT v, DWORD apply) = 0;
    STDMETHOD(SetOrientation)(FLOAT xFront, FLOAT yFront, FLOAT zFront, FLOAT xTop, FLOAT yTop, FLOAT zTop, DWORD apply) = 0;
    STDMETHOD(SetPosition)(FLOAT x, FLOAT y, FLOAT z, DWORD apply) = 0;
    STDMETHOD(SetRolloffFactor)(FLOAT v, DWORD apply) = 0;
    STDMETHOD(SetVelocity)(FLOAT x, FLOAT y, FLOAT z, DWORD apply) = 0;
    STDMETHOD(CommitDeferredSettings)() = 0;
};
struct IDirectSound3DBuffer : IUnknown {
    STDMETHOD(GetAllParameters)(void* p) = 0;
    STDMETHOD(GetConeAngles)(DWORD* inside, DWORD* outside) = 0;
    STDMETHOD(GetConeOrientation)(void* p) = 0;
    STDMETHOD(GetConeOutsideVolume)(LONG* p) = 0;
    STDMETHOD(GetMaxDistance)(FLOAT* p) = 0;
    STDMETHOD(GetMinDistance)(FLOAT* p) = 0;
    STDMETHOD(GetMode)(DWORD* p) = 0;
    STDMETHOD(GetPosition)(void* p) = 0;
    STDMETHOD(GetVelocity)(void* p) = 0;
    STDMETHOD(SetAllParameters)(const void* p, DWORD apply) = 0;
    STDMETHOD(SetConeAngles)(DWORD inside, DWORD outside, DWORD apply) = 0;
    STDMETHOD(SetConeOrientation)(FLOAT x, FLOAT y, FLOAT z, DWORD apply) = 0;
    STDMETHOD(SetConeOutsideVolume)(LONG v, DWORD apply) = 0;
    STDMETHOD(SetMaxDistance)(FLOAT v, DWORD apply) = 0;
    STDMETHOD(SetMinDistance)(FLOAT v, DWORD apply) = 0;
    STDMETHOD(SetMode)(DWORD mode, DWORD apply) = 0;
    STDMETHOD(SetPosition)(FLOAT x, FLOAT y, FLOAT z, DWORD apply) = 0;
    STDMETHOD(SetVelocity)(FLOAT x, FLOAT y, FLOAT z, DWORD apply) = 0;
};
struct IDirectSoundBuffer : IUnknown {
    STDMETHOD(GetCaps)(LPDSBCAPS caps) = 0;
    STDMETHOD(GetCurrentPosition)(LPDWORD play, LPDWORD write) = 0;
    STDMETHOD(GetFormat)(LPWAVEFORMATEX fmt, DWORD size, LPDWORD written) = 0;
    STDMETHOD(GetVolume)(LPLONG volume) = 0;
    STDMETHOD(GetPan)(LPLONG pan) = 0;
    STDMETHOD(GetFrequency)(LPDWORD freq) = 0;
    STDMETHOD(GetStatus)(LPDWORD status) = 0;
    STDMETHOD(Initialize)(struct IDirectSound* ds, LPCDSBUFFERDESC desc) = 0;
    STDMETHOD(Lock)(DWORD offset, DWORD bytes, LPVOID* ptr1, LPDWORD bytes1, LPVOID* ptr2, LPDWORD bytes2, DWORD flags) = 0;
    STDMETHOD(Play)(DWORD reserved1, DWORD priority, DWORD flags) = 0;
    STDMETHOD(SetCurrentPosition)(DWORD pos) = 0;
    STDMETHOD(SetFormat)(const WAVEFORMATEX* fmt) = 0;
    STDMETHOD(SetVolume)(LONG volume) = 0;
    STDMETHOD(SetPan)(LONG pan) = 0;
    STDMETHOD(SetFrequency)(DWORD freq) = 0;
    STDMETHOD(Stop)() = 0;
    STDMETHOD(Unlock)(LPVOID ptr1, DWORD bytes1, LPVOID ptr2, DWORD bytes2) = 0;
    STDMETHOD(Restore)() = 0;
};
struct IDirectSoundBuffer8 : IDirectSoundBuffer {
    STDMETHOD(SetFX)(DWORD numEffects, DSEFFECTDESC* effects, DWORD* resultCodes) = 0;
    STDMETHOD(AcquireResources)(DWORD flags, DWORD numEffects, DWORD* resultCodes) = 0;
    STDMETHOD(GetObjectInPath)(REFGUID objectClass, DWORD index, REFGUID iid, LPVOID* obj) = 0;
};
struct IDirectSound : IUnknown {
    STDMETHOD(CreateSoundBuffer)(LPCDSBUFFERDESC desc, IDirectSoundBuffer** buffer, IUnknown* outer) = 0;
    STDMETHOD(GetCaps)(LPDSCAPS caps) = 0;
    STDMETHOD(DuplicateSoundBuffer)(IDirectSoundBuffer* original, IDirectSoundBuffer** duplicate) = 0;
    STDMETHOD(SetCooperativeLevel)(HWND window, DWORD level) = 0;
    STDMETHOD(Compact)() = 0;
    STDMETHOD(GetSpeakerConfig)(LPDWORD config) = 0;
    STDMETHOD(SetSpeakerConfig)(DWORD config) = 0;
    STDMETHOD(Initialize)(const GUID* device) = 0;
};
struct IDirectSound8 : IDirectSound {
    STDMETHOD(VerifyCertification)(LPDWORD certified) = 0;
};
using LPDIRECTSOUND       = IDirectSound*;
using LPDIRECTSOUND8      = IDirectSound8*;
using LPDIRECTSOUNDBUFFER = IDirectSoundBuffer*;

// ---- factories: log and FAIL (W-B4: no DirectSound on this platform; the OpenAL hardware layer replaces them) ----
namespace notsa::portable {
inline HRESULT DirectSoundUnavailable(const char* fn) {
    static bool logged = false;
    if (!logged) {
        logged = true;
        std::fprintf(stderr, "W-B4: %s: DirectSound does not exist on this platform, audio hardware is disabled (OpenAL port pending)\n", fn);
    }
    return DSERR_NODRIVER;
}
} // namespace notsa::portable
inline HRESULT DirectSoundCreate(const GUID*, IDirectSound** ds, IUnknown*) {
    *ds = nullptr;
    return notsa::portable::DirectSoundUnavailable("DirectSoundCreate");
}
inline HRESULT DirectSoundCreate8(const GUID*, IDirectSound8** ds, IUnknown*) {
    *ds = nullptr;
    return notsa::portable::DirectSoundUnavailable("DirectSoundCreate8");
}
