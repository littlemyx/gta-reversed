// MAC-BOOT1 (W-3 stage 1): a SILENT software DirectSound device for the portable build.
//
// The audio hardware layer of the game (CAEAudioHardware / CAEAudioChannel / CAEStaticChannel / CAEStreamingChannel / CAESmoothFadeThread) is DirectSound code. Until the OpenAL
// version of that layer exists (stream W-3), DirectSoundCreate8 returns this device so that the whole audio ENGINE runs (banks, events, channels, radio streaming) and the game's
// start-up does not depend on audio failing half way (CAudioEngine::Initialise() == false leaves tables unset and the frontend dereferences them).
//
// What it models (everything the game observes through the interfaces of winshim/dsound.h):
//  * buffers own their memory (Lock/Unlock give real pointers, with the wrap-around split), format and frequency;
//  * a PLAYING buffer's play cursor advances with the steady clock at frequency * blockAlign bytes per second (looping wraps; a non-looping buffer stops at its end and the
//    cursor returns to 0), the write cursor leads the play cursor by 100 ms (0 while stopped): the radio stream thread and the static-channel timers see a believable device;
//  * volume / pan / frequency follow the DirectSound value ranges (out-of-range values are rejected with DSERR_INVALIDPARAM as dsound does);
//  * IDirectSound3DBuffer / IDirectSound3DListener exist (setters store, getters return what was stored); IKsPropertySet (EAX) and the FX chain (SetFX / GetObjectInPath) are
//    NOT available (E_NOINTERFACE / DSERR_UNSUPPORTED), exactly like a device without EAX/DMO effects.
// Nothing is ever sent to a sound card. The OpenAL layer will replace this file's role (same interfaces) by feeding these buffers to OpenAL sources.
#ifdef NOTSA_PORTABLE
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

#include "winshim/dsound.h"

namespace notsa::portable {
namespace {
using Clock = std::chrono::steady_clock;

constexpr DWORD kMinFreq = 100, kMaxFreq = 200000; // DSBFREQUENCY_MIN / MAX
constexpr DWORD kDsSpeakerStereo = 3;               // DSSPEAKER_STEREO

template<class T>
struct Unk : T { // reference counting, shared by every object of this file
    std::atomic<ULONG> refs{1};
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto r = --refs;
        if (r == 0) { delete this; }
        return r;
    }
    virtual ~Unk() = default;
};

struct Vec3 { FLOAT x{}, y{}, z{}; };

struct Null3DBuffer final : Unk<IDirectSound3DBuffer> {
    FLOAT minDist{1.f}, maxDist{1e9f};
    DWORD mode{0}, coneIn{360}, coneOut{360};
    LONG  coneVol{0};
    Vec3  pos, vel, cone;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** out) override { *out = nullptr; return E_NOINTERFACE; } // no IKsPropertySet (EAX)
    HRESULT STDMETHODCALLTYPE GetAllParameters(void*) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetConeAngles(DWORD* i, DWORD* o) override { *i = coneIn; *o = coneOut; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetConeOrientation(void* p) override { *(Vec3*)p = cone; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetConeOutsideVolume(LONG* p) override { *p = coneVol; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetMaxDistance(FLOAT* p) override { *p = maxDist; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetMinDistance(FLOAT* p) override { *p = minDist; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetMode(DWORD* p) override { *p = mode; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetPosition(void* p) override { *(Vec3*)p = pos; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetVelocity(void* p) override { *(Vec3*)p = vel; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetAllParameters(const void*, DWORD) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetConeAngles(DWORD i, DWORD o, DWORD) override { coneIn = i; coneOut = o; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetConeOrientation(FLOAT x, FLOAT y, FLOAT z, DWORD) override { cone = {x, y, z}; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetConeOutsideVolume(LONG v, DWORD) override { coneVol = v; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetMaxDistance(FLOAT v, DWORD) override { maxDist = v; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetMinDistance(FLOAT v, DWORD) override { minDist = v; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetMode(DWORD m, DWORD) override { mode = m; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetPosition(FLOAT x, FLOAT y, FLOAT z, DWORD) override { pos = {x, y, z}; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetVelocity(FLOAT x, FLOAT y, FLOAT z, DWORD) override { vel = {x, y, z}; return DS_OK; }
};

struct NullListener final : Unk<IDirectSound3DListener> {
    FLOAT distanceFactor{1.f}, dopplerFactor{1.f}, rolloffFactor{1.f};
    Vec3  pos, vel, front{0, 0, 1}, top{0, 1, 0};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** out) override { *out = nullptr; return E_NOINTERFACE; }
    HRESULT STDMETHODCALLTYPE GetAllParameters(void*) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetDistanceFactor(FLOAT* p) override { *p = distanceFactor; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetDopplerFactor(FLOAT* p) override { *p = dopplerFactor; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetOrientation(void* f, void* t) override { *(Vec3*)f = front; *(Vec3*)t = top; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetPosition(void* p) override { *(Vec3*)p = pos; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetRolloffFactor(FLOAT* p) override { *p = rolloffFactor; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetVelocity(void* p) override { *(Vec3*)p = vel; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetAllParameters(const void*, DWORD) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetDistanceFactor(FLOAT v, DWORD) override { distanceFactor = v; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetDopplerFactor(FLOAT v, DWORD) override { dopplerFactor = v; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetOrientation(FLOAT xf, FLOAT yf, FLOAT zf, FLOAT xt, FLOAT yt, FLOAT zt, DWORD) override { front = {xf, yf, zf}; top = {xt, yt, zt}; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetPosition(FLOAT x, FLOAT y, FLOAT z, DWORD) override { pos = {x, y, z}; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetRolloffFactor(FLOAT v, DWORD) override { rolloffFactor = v; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetVelocity(FLOAT x, FLOAT y, FLOAT z, DWORD) override { vel = {x, y, z}; return DS_OK; }
    HRESULT STDMETHODCALLTYPE CommitDeferredSettings() override { return DS_OK; }
};

struct NullBuffer final : Unk<IDirectSoundBuffer8> {
    std::mutex          m;
    DWORD               flags{};
    std::vector<uint8>  data;
    WAVEFORMATEX        fmt{};
    DWORD               freq{};      // current frequency (Hz)
    LONG                volume{0}, pan{0};
    bool                playing{false}, looping{false};
    double              pos{0.0};    // play cursor in bytes, valid at `last`
    Clock::time_point   last{};

    explicit NullBuffer(const DSBUFFERDESC* d) : flags(d->dwFlags), data(d->dwBufferBytes) {
        if (d->lpwfxFormat) { fmt = *d->lpwfxFormat; }
        freq = fmt.nSamplesPerSec;
    }
    DWORD BlockAlign() const { return fmt.nBlockAlign ? fmt.nBlockAlign : 1; }
    double BytesPerSec() const { return (double)(freq ? freq : fmt.nSamplesPerSec) * BlockAlign(); }
    // advances the play cursor to "now" (caller holds m)
    void Advance() {
        const auto now = Clock::now();
        if (playing && !data.empty()) {
            const double dt = std::chrono::duration<double>(now - last).count();
            pos += dt * BytesPerSec();
            const double size = (double)data.size();
            if (looping) {
                pos = std::fmod(pos, size);
            } else if (pos >= size) {
                playing = false;
                pos     = 0.0;
            }
        }
        last = now;
    }
    DWORD PlayCursor() const { return (DWORD)pos / BlockAlign() * BlockAlign(); }
    DWORD WriteCursor() const {
        if (data.empty()) { return 0; }
        const DWORD lead = playing ? (DWORD)(BytesPerSec() * 0.1) / BlockAlign() * BlockAlign() : 0;
        return (PlayCursor() + lead) % (DWORD)data.size();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) { return E_INVALIDARG; }
        if (iid == IID_IDirectSound3DBuffer) {
            if (!(flags & DSBCAPS_CTRL3D) || (flags & DSBCAPS_PRIMARYBUFFER)) { *out = nullptr; return E_NOINTERFACE; }
            *out = new Null3DBuffer;
            return DS_OK;
        }
        if (iid == IID_IDirectSound3DListener) {
            if (!(flags & DSBCAPS_PRIMARYBUFFER)) { *out = nullptr; return E_NOINTERFACE; }
            *out = new NullListener;
            return DS_OK;
        }
        *out = nullptr; // IKsPropertySet, IDirectSoundBuffer8 queried by iid (the game casts instead), anything else
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetCaps(LPDSBCAPS caps) override {
        if (!caps) { return DSERR_INVALIDPARAM; }
        *caps = {sizeof(DSBCAPS), flags, (DWORD)data.size(), 0, 0};
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentPosition(LPDWORD play, LPDWORD write) override {
        std::lock_guard lk{m};
        Advance();
        if (play) { *play = PlayCursor(); }
        if (write) { *write = WriteCursor(); }
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE GetFormat(LPWAVEFORMATEX f, DWORD size, LPDWORD written) override {
        std::lock_guard lk{m};
        const DWORD n = std::min<DWORD>(size, sizeof(WAVEFORMATEX));
        if (f) { std::memcpy(f, &fmt, n); }
        if (written) { *written = n; }
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE GetVolume(LPLONG v) override { std::lock_guard lk{m}; *v = volume; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetPan(LPLONG p) override { std::lock_guard lk{m}; *p = pan; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetFrequency(LPDWORD f) override { std::lock_guard lk{m}; *f = freq; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetStatus(LPDWORD s) override {
        std::lock_guard lk{m};
        Advance();
        *s = DSBSTATUS_LOCSOFTWARE | (playing ? DSBSTATUS_PLAYING : 0) | (playing && looping ? DSBSTATUS_LOOPING : 0);
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE Initialize(IDirectSound*, LPCDSBUFFERDESC) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE Lock(DWORD offset, DWORD bytes, LPVOID* p1, LPDWORD b1, LPVOID* p2, LPDWORD b2, DWORD lockFlags) override {
        std::lock_guard lk{m};
        if (!p1 || !b1 || data.empty()) { return DSERR_INVALIDPARAM; }
        const DWORD size = (DWORD)data.size();
        if (lockFlags & DSBLOCK_FROMWRITECURSOR) { Advance(); offset = WriteCursor(); }
        if (lockFlags & DSBLOCK_ENTIREBUFFER) { offset = 0; bytes = size; }
        if (offset >= size || bytes > size) { return DSERR_INVALIDPARAM; }
        const DWORD first = std::min(bytes, size - offset);
        *p1 = data.data() + offset;
        *b1 = first;
        if (p2) { *p2 = first < bytes ? data.data() : nullptr; }
        if (b2) { *b2 = bytes - first; }
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE Play(DWORD, DWORD, DWORD playFlags) override {
        std::lock_guard lk{m};
        if (!playing) { last = Clock::now(); }
        else { Advance(); }
        playing = true;
        looping = (playFlags & DSBPLAY_LOOPING) != 0;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE SetCurrentPosition(DWORD p) override {
        std::lock_guard lk{m};
        if (p >= data.size() && !data.empty()) { return DSERR_INVALIDPARAM; }
        pos  = p;
        last = Clock::now();
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE SetFormat(const WAVEFORMATEX* f) override {
        std::lock_guard lk{m};
        if (!f) { return DSERR_INVALIDPARAM; }
        fmt  = *f;
        freq = fmt.nSamplesPerSec;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE SetVolume(LONG v) override {
        if (v < DSBVOLUME_MIN || v > DSBVOLUME_MAX) { return DSERR_INVALIDPARAM; }
        std::lock_guard lk{m};
        volume = v;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE SetPan(LONG p) override {
        if (p < -10000 || p > 10000) { return DSERR_INVALIDPARAM; }
        std::lock_guard lk{m};
        pan = p;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE SetFrequency(DWORD f) override {
        std::lock_guard lk{m};
        if (f == 0) { f = fmt.nSamplesPerSec; } // DSBFREQUENCY_ORIGINAL
        if (f < kMinFreq || f > kMaxFreq) { return DSERR_INVALIDPARAM; }
        Advance();
        freq = f;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE Stop() override {
        std::lock_guard lk{m};
        Advance();
        playing = false;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE Unlock(LPVOID, DWORD, LPVOID, DWORD) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE Restore() override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetFX(DWORD n, DSEFFECTDESC*, DWORD*) override { return n == 0 ? DS_OK : DSERR_UNSUPPORTED; }
    HRESULT STDMETHODCALLTYPE AcquireResources(DWORD, DWORD, DWORD*) override { return DSERR_UNSUPPORTED; }
    HRESULT STDMETHODCALLTYPE GetObjectInPath(REFGUID, DWORD, REFGUID, LPVOID* o) override { if (o) { *o = nullptr; } return DSERR_UNSUPPORTED; }
};

struct NullDevice final : Unk<IDirectSound8> {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** out) override { *out = nullptr; return E_NOINTERFACE; }
    HRESULT STDMETHODCALLTYPE CreateSoundBuffer(LPCDSBUFFERDESC d, IDirectSoundBuffer** out, IUnknown*) override {
        if (!d || !out || d->dwSize < sizeof(DSBUFFERDESC) - sizeof(GUID)) { return DSERR_INVALIDPARAM; }
        const bool primary = (d->dwFlags & DSBCAPS_PRIMARYBUFFER) != 0;
        if (!primary && (d->dwBufferBytes == 0 || !d->lpwfxFormat) && !(d->dwFlags & DSBCAPS_CTRL3D)) { return DSERR_INVALIDPARAM; }
        *out = new NullBuffer(d);
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCaps(LPDSCAPS caps) override {
        if (!caps) { return DSERR_INVALIDPARAM; }
        *caps = {};
        caps->dwSize = sizeof(DSCAPS);
        caps->dwFlags = 0x10 | 0x20 | 0x2 | 0x1; // DSCAPS_CONTINUOUSRATE | EMULDRIVER | SECONDARYSTEREO | PRIMARYMONO (informational)
        caps->dwMinSecondarySampleRate = kMinFreq;
        caps->dwMaxSecondarySampleRate = kMaxFreq;
        caps->dwPrimaryBuffers = 1;
        return DS_OK; // no hardware mixing / 3D buffers: dwFree* stay 0 (software device)
    }
    HRESULT STDMETHODCALLTYPE DuplicateSoundBuffer(IDirectSoundBuffer* original, IDirectSoundBuffer** dup) override {
        auto* src = dynamic_cast<NullBuffer*>(original);
        if (!src || !dup) { return DSERR_INVALIDPARAM; }
        std::lock_guard lk{src->m};
        DSBUFFERDESC d{sizeof(DSBUFFERDESC), src->flags, (DWORD)src->data.size(), 0, &src->fmt, {}};
        auto* b = new NullBuffer(&d);
        b->data = src->data;
        b->freq = src->freq;
        *dup    = b;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND, DWORD) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE Compact() override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetSpeakerConfig(LPDWORD c) override { *c = kDsSpeakerStereo; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetSpeakerConfig(DWORD) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE Initialize(const GUID*) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE VerifyCertification(LPDWORD c) override { if (c) { *c = 0; } return DS_OK; }
};
} // namespace

HRESULT CreateNullDirectSound(IDirectSound8** out) {
    static std::atomic<bool> logged{false};
    if (!logged.exchange(true)) {
        std::fprintf(stderr, "[portable] W-3 stage 1: DirectSoundCreate8 returns a SILENT software device (no sound output until the OpenAL layer exists)\n");
    }
    *out = new NullDevice;
    return DS_OK;
}
} // namespace notsa::portable
#endif
