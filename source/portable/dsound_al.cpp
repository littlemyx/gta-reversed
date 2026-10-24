// MAC-W3 (W-3): DirectSound for the portable build, realised as a SOFTWARE MIXER whose output goes through OpenAL.
//
// The audio hardware layer of the game (CAEAudioHardware / CAEAudioChannel / CAEStaticChannel / CAEStreamingChannel / CAESmoothFadeThread) is DirectSound code and stays
// UNCHANGED. It relies on DirectSound's buffer model: a secondary buffer is a RING of PCM bytes that the game keeps rewriting while it plays (CAEStaticChannel::Service
// re-locks the loop region of a playing buffer, CAEStreamingChannel refills 0xC0000 bytes of radio stream behind the play cursor and reads GetCurrentPosition to find out
// how far the device has got), with volume / pan / frequency / 3D position set at any time. Mapping each buffer to an AL source would need the very same cursor and refill
// emulation PLUS a per-buffer queue pump, so this file does the one thing DirectSound itself does: mixing.
//
//   game -> IDirectSoundBuffer8 (ring bytes, cursors, volume, pan, frequency, 3D) -> Core::Mix() -> interleaved s16 stereo chunks -> ONE OpenAL streaming source
//        -> ALC device (the default playback device) | ALC_SOFT_loopback (nothing audible; tests and agent runs) | no OpenAL (steady clock, output discarded)
//
// Mixer model (everything the game observes through winshim/dsound.h):
//  * 8/16 bit PCM, mono/stereo, any frequency 100..200000 (linear interpolation to the mix rate); a buffer plays once or loops; a stopped end resets the cursor to 0;
//  * SetVolume in hundredths of dB: gain = 10^(v/2000), -10000 = silence; SetPan: the OPPOSITE channel is attenuated by |pan| hundredths of dB; gain changes are ramped linearly
//    over the chunk (no zipper clicks, still exact for a constant gain);
//  * the play cursor is the mixer's read position (it is therefore up to the OpenAL queue depth (~43 ms) ahead of what is audible), the write cursor leads it by 50 ms of
//    audio while playing, Lock/Unlock give real pointers with the ring wrap split;
//  * 3D buffers / listener: deferred (DS3D_DEFERRED) settings are applied by CommitDeferredSettings(). A buffer in DS3DMODE_NORMAL is positioned relative to the listener:
//    azimuth -> pan (the far ear is scaled by cos(|pan| * pi/2)), distance -> inverse-distance rolloff (the game sets the listener rolloff to 0, so no attenuation); no doppler,
//    no cones;
//  * NOT available (reported like a card without them): EAX (IKsPropertySet -> E_NOINTERFACE, logged once), the FX chain (SetFX with effects -> DSERR_UNSUPPORTED: the radio bass
//    EQ is off), hardware mixing (caps report 0 hardware buffers so the game picks the 48 software channels).
// Debug knobs (environment): NOTSA_AUDIO=device|loopback|null (default device, falls back to null), NOTSA_AUDIO_WAV=<file> (everything that was output as 16-bit stereo WAV),
// NOTSA_AUDIO_RATE=<hz> (loopback / null mix rate, default 48000).
#ifdef NOTSA_PORTABLE
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "dsound_al.h"

namespace notsa::portable {
namespace dsal {
namespace {
using Clock = std::chrono::steady_clock;

constexpr DWORD  kMinFreq = 100, kMaxFreq = 200000; // DSBFREQUENCY_MIN / MAX
constexpr DWORD  kDsSpeakerStereo = 3;               // DSSPEAKER_STEREO
constexpr int    kQueueTarget = 4;                   // OpenAL buffers kept queued (4 * 512 frames = ~43 ms at 48 kHz)
constexpr int    kAlBuffers = 8;
constexpr std::size_t kLoopbackPad = 128; // see AlOut::Queue
constexpr double kWriteLeadSec = 0.05;
constexpr double kPi = 3.14159265358979323846;

template<class... A>
void Log(const char* fmt, A... a) {
    std::fprintf(stderr, "[audio] ");
    std::fprintf(stderr, fmt, a...);
    std::fputc('\n', stderr);
}

template<class T>
struct Unk : T { // reference counting, shared by every COM-style object of this file
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
Vec3  operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
FLOAT Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3  Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
FLOAT Len(Vec3 a) { return std::sqrt(Dot(a, a)); }

constexpr DWORD kDS3DMODE_NORMAL = 0, kDS3DMODE_HEADRELATIVE = 1, kDS3DMODE_DISABLE = 2;

struct ListenerParams {
    Vec3  pos, vel, front{0, 0, 1}, top{0, 1, 0};
    FLOAT distanceFactor{1.f}, dopplerFactor{1.f}, rolloffFactor{1.f};
};
struct Params3D {
    FLOAT minDist{1.f}, maxDist{1e9f};
    DWORD mode{kDS3DMODE_NORMAL}, coneIn{360}, coneOut{360};
    LONG  coneVol{0};
    Vec3  pos, vel, cone;
};

struct BufState;

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------- mixer core
struct Core {
    std::mutex                mu;           // guards EVERYTHING below and every BufState (one lock: a mix chunk is ~50 us)
    std::uint32_t             rate{48000};
    ListenerParams            lis, lisPending;
    bool                      lisDirty{false};
    std::vector<BufState*>    bufs;
    std::vector<float>        acc;
    std::atomic<int>          playLogs{0};
    std::atomic<std::uint64_t> framesMixed{0}, playCalls{0};

    void Mix(std::int16_t* out, std::size_t frames);
    void Commit();
};

struct BufState {
    std::shared_ptr<Core> core;
    DWORD                 flags{};
    bool                  primary{false};
    std::vector<std::uint8_t> data;
    WAVEFORMATEX          fmt{};
    DWORD                 freq{};            // current frequency (Hz)
    LONG                  volume{0}, pan{0};
    bool                  playing{false}, looping{false};
    double                pos{0.0};          // play cursor in FRAMES (the mixer's read position)
    float                 prevGL{-1.f}, prevGR{-1.f};
    Params3D              cur, pending;
    bool                  dirty{false};

    BufState(std::shared_ptr<Core> c, const DSBUFFERDESC* d) : core(std::move(c)), flags(d->dwFlags), primary((d->dwFlags & DSBCAPS_PRIMARYBUFFER) != 0), data(d->dwBufferBytes) {
        if (d->lpwfxFormat) {
            fmt = *d->lpwfxFormat;
        } else { // DSBCAPS_CTRL3D buffer without a format (CAEAudioHardware::Query3DSoundEffects): the DirectSound default is the primary format; any PCM will do
            fmt = {WAVE_FORMAT_PCM, 1, 44100, 88200, 2, 16, 0};
        }
        freq = fmt.nSamplesPerSec;
        std::lock_guard lk{core->mu};
        core->bufs.push_back(this);
    }
    ~BufState() {
        std::lock_guard lk{core->mu};
        core->bufs.erase(std::remove(core->bufs.begin(), core->bufs.end(), this), core->bufs.end());
    }
    DWORD Block() const {
        if (fmt.nBlockAlign) { return fmt.nBlockAlign; }
        const DWORD b = std::max<DWORD>(1, fmt.nChannels) * std::max<DWORD>(8, fmt.wBitsPerSample) / 8;
        return b ? b : 1;
    }
    std::size_t Frames() const { return data.size() / Block(); }
    double BytesPerSec() const { return (double)(freq ? freq : fmt.nSamplesPerSec) * Block(); }
    DWORD PlayCursor() const { return (DWORD)((std::size_t)pos * Block()); }
    DWORD WriteCursor() const {
        if (data.empty()) { return 0; }
        const DWORD blk  = Block();
        const DWORD lead = playing ? (DWORD)(BytesPerSec() * kWriteLeadSec) / blk * blk : 0;
        return (PlayCursor() + lead) % (DWORD)data.size();
    }
    void Stopped() { playing = false; }

    float Sample(std::size_t frame, int chan) const { // normalised [-1, 1)
        const int ch = std::max<int>(1, fmt.nChannels);
        const std::size_t i = frame * (std::size_t)ch + (std::size_t)std::min(chan, ch - 1);
        if (fmt.wBitsPerSample == 16) {
            std::int16_t s;
            std::memcpy(&s, data.data() + i * 2, 2);
            return (float)s / 32768.f;
        }
        return ((float)data[i] - 128.f) / 128.f; // 8 bit PCM is unsigned
    }
    bool Mixable() const {
        return !primary && playing && !data.empty() && (fmt.wBitsPerSample == 16 || fmt.wBitsPerSample == 8) && fmt.nChannels >= 1 && Block() >= (DWORD)fmt.nChannels * fmt.wBitsPerSample / 8;
    }
    void MixInto(float* acc, std::size_t frames, const ListenerParams& L, std::uint32_t rate);
};

void BufState::MixInto(float* acc, std::size_t frames, const ListenerParams& L, std::uint32_t outRate) {
    const std::size_t nfr = Frames();
    if (!nfr) { playing = false; return; }

    // ---- target gains
    float g = volume <= DSBVOLUME_MIN ? 0.f : (float)std::pow(10.0, (double)volume / 2000.0);
    float gL = g, gR = g;
    if (pan > 0) {
        gL *= pan >= 10000 ? 0.f : (float)std::pow(10.0, -(double)pan / 2000.0);
    } else if (pan < 0) {
        gR *= pan <= -10000 ? 0.f : (float)std::pow(10.0, (double)pan / 2000.0);
    }
    if ((flags & DSBCAPS_CTRL3D) && cur.mode != kDS3DMODE_DISABLE) {
        const Vec3 rel  = cur.mode == kDS3DMODE_HEADRELATIVE ? cur.pos : cur.pos - L.pos;
        const FLOAT dist = Len(rel);
        FLOAT att = 1.f;
        if (L.rolloffFactor > 0.f) {
            const FLOAT d = std::min(dist, cur.maxDist);
            if (d > cur.minDist) { att = cur.minDist / (cur.minDist + L.rolloffFactor * (d - cur.minDist)); }
        }
        // DirectSound is left handed: right = top x front
        Vec3 right = Cross(L.top, L.front);
        const FLOAT rl = Len(right);
        FLOAT p = 0.f;
        if (rl > 1e-6f && dist > 1e-6f) { p = std::clamp(Dot(rel, right) / rl / dist, -1.f, 1.f); }
        const float far = (float)std::cos(std::fabs((double)p) * kPi / 2.0);
        if (p > 0.f) { gL *= far; } else if (p < 0.f) { gR *= far; }
        gL *= att;
        gR *= att;
    }
    if (prevGL < 0.f) { prevGL = gL; prevGR = gR; } // first chunk of this buffer: no ramp
    const float dL = (gL - prevGL) / (float)frames, dR = (gR - prevGR) / (float)frames;
    float       cL = prevGL, cR = prevGR;

    const double step = (double)(freq ? freq : fmt.nSamplesPerSec) / (double)outRate;
    for (std::size_t i = 0; i < frames; ++i) {
        if (pos >= (double)nfr) { pos = 0.0; playing = false; break; } // (a non-looping end is detected after the advance below; this only guards a position past the end)
        const std::size_t i0   = (std::size_t)pos;
        const float       frac = (float)(pos - (double)i0);
        std::size_t       i1   = i0 + 1;
        if (i1 >= nfr) { i1 = looping ? 0 : i0; }
        const float l0 = Sample(i0, 0), r0 = Sample(i0, 1), l1 = Sample(i1, 0), r1 = Sample(i1, 1);
        const float l  = frac == 0.f ? l0 : l0 + (l1 - l0) * frac;
        const float r  = frac == 0.f ? r0 : r0 + (r1 - r0) * frac;
        cL += dL;
        cR += dR;
        acc[2 * i]     += l * cL;
        acc[2 * i + 1] += r * cR;
        pos += step;
        if (pos >= (double)nfr) {
            if (looping) {
                pos = std::fmod(pos, (double)nfr);
            } else {
                pos     = 0.0;
                playing = false;
                break;
            }
        }
    }
    prevGL = gL;
    prevGR = gR;
}

void Core::Mix(std::int16_t* out, std::size_t frames) {
    std::lock_guard lk{mu};
    acc.assign(frames * 2, 0.f);
    for (auto* b : bufs) {
        if (b->Mixable()) { b->MixInto(acc.data(), frames, lis, rate); }
    }
    for (std::size_t i = 0; i < frames * 2; ++i) {
        const float v = acc[i] * 32768.f;
        out[i]        = (std::int16_t)std::clamp(std::lrintf(v), -32768L, 32767L);
    }
    framesMixed += frames;
}

void Core::Commit() {
    std::lock_guard lk{mu};
    if (lisDirty) { lis = lisPending; lisDirty = false; }
    for (auto* b : bufs) {
        if (b->dirty) { b->cur = b->pending; b->dirty = false; }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------- OpenAL output
struct WavWriter {
    std::FILE*    f{};
    std::uint64_t frames{0};
    std::uint32_t rate{48000};
    Clock::time_point lastFlush{};
    bool Open(const char* path, std::uint32_t r) {
        rate = r;
        f    = std::fopen(path, "wb");
        if (!f) { Log("cannot open WAV capture %s", path); return false; }
        WriteHeader();
        return true;
    }
    void WriteHeader() {
        const std::uint32_t dataBytes = (std::uint32_t)(frames * 4), riff = 36 + dataBytes, fmtLen = 16, byteRate = rate * 4;
        const std::uint16_t tag = 1, ch = 2, align = 4, bits = 16;
        const long pos = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
        std::fwrite(&fmtLen, 4, 1, f); std::fwrite(&tag, 2, 1, f); std::fwrite(&ch, 2, 1, f); std::fwrite(&rate, 4, 1, f);
        std::fwrite(&byteRate, 4, 1, f); std::fwrite(&align, 2, 1, f); std::fwrite(&bits, 2, 1, f);
        std::fwrite("data", 1, 4, f); std::fwrite(&dataBytes, 4, 1, f);
        if (pos > 44) { std::fseek(f, pos, SEEK_SET); }
    }
    void Append(const std::int16_t* s, std::size_t n) {
        if (!f) { return; }
        std::fwrite(s, 4, n, f);
        frames += n;
        const auto now = Clock::now();
        if (now - lastFlush > std::chrono::milliseconds(500)) { // keep the file valid if the process is killed
            lastFlush = now;
            WriteHeader();
            std::fflush(f);
        }
    }
    void Close() {
        if (!f) { return; }
        WriteHeader();
        std::fclose(f);
        f = nullptr;
    }
};

struct AlOut {
    ALCdevice*  dev{};
    ALCcontext* ctx{};
    ALuint      src{};
    std::vector<ALuint> freeBufs;
    std::vector<ALuint> allBufs;
    bool        loopback{false};
    std::uint32_t rate{48000};
    std::string name;
    LPALCRENDERSAMPLESSOFT renderFn{};
    int         underruns{0};
    std::vector<float> fbuf;
    // loopback: renders `n` frames (+ `pad` frames of the silent tail queued with them, see Queue) and returns the first `n` in `out` (s16, clamped)
    void RenderLoopback(std::int16_t* out, std::size_t n, std::size_t pad) {
        fbuf.resize((n + pad) * 2);
        renderFn(dev, fbuf.data(), (ALCsizei)(n + pad));
        for (std::size_t i = 0; i < n * 2; ++i) { out[i] = (std::int16_t)std::clamp(std::lrintf(fbuf[i] * 32768.f), -32768L, 32767L); }
    }

    bool Open(bool lb, std::uint32_t wantRate) {
        loopback = lb;
        if (lb) {
            if (!alcIsExtensionPresent(nullptr, "ALC_SOFT_loopback")) { Log("OpenAL: ALC_SOFT_loopback not available"); return false; }
            auto openFn = (LPALCLOOPBACKOPENDEVICESOFT)alcGetProcAddress(nullptr, "alcLoopbackOpenDeviceSOFT");
            renderFn    = (LPALCRENDERSAMPLESSOFT)alcGetProcAddress(nullptr, "alcRenderSamplesSOFT");
            if (!openFn || !renderFn) { Log("OpenAL: loopback entry points missing"); return false; }
            dev = openFn(nullptr);
            if (!dev) { Log("OpenAL: alcLoopbackOpenDeviceSOFT failed"); return false; }
            const ALCint attrs[] = {ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT, ALC_FORMAT_TYPE_SOFT, ALC_FLOAT_SOFT, ALC_FREQUENCY, (ALCint)wantRate, ALC_HRTF_SOFT, ALC_FALSE, ALC_OUTPUT_LIMITER_SOFT, ALC_FALSE, 0};
            // float output: no dithering, the limiter (1 ms look-ahead, a gain ramp on source start) is off: the loopback is a bit exact capture of the mix
            ctx = alcCreateContext(dev, attrs);
            rate = wantRate;
            name = "ALC_SOFT_loopback";
        } else {
            dev = alcOpenDevice(nullptr);
            if (!dev) { Log("OpenAL: no playback device (alcOpenDevice failed)"); return false; }
            const ALCint attrs[] = {ALC_OUTPUT_LIMITER_SOFT, ALC_FALSE, 0}; // the mixer clips itself; the limiter would only add its look-ahead delay
            ctx = alcCreateContext(dev, alcIsExtensionPresent(dev, "ALC_SOFT_output_limiter") ? attrs : nullptr);
            ALCint f = 48000;
            alcGetIntegerv(dev, ALC_FREQUENCY, 1, &f);
            rate = (std::uint32_t)f;
            const char* n = alcIsExtensionPresent(dev, "ALC_ENUMERATE_ALL_EXT") ? alcGetString(dev, ALC_ALL_DEVICES_SPECIFIER) : alcGetString(dev, ALC_DEVICE_SPECIFIER);
            name = n ? n : "default";
        }
        if (!ctx || !alcMakeContextCurrent(ctx)) {
            Log("OpenAL: cannot create / activate a context");
            Close();
            return false;
        }
        alGenSources(1, &src);
        alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcef(src, AL_ROLLOFF_FACTOR, 0.f);
        if (alIsExtensionPresent("AL_SOFT_direct_channels")) { alSourcei(src, AL_DIRECT_CHANNELS_SOFT, AL_TRUE); } // the stereo mix goes to L/R untouched (no HRTF, no panning)
        allBufs.resize(kAlBuffers);
        alGenBuffers(kAlBuffers, allBufs.data());
        freeBufs = allBufs;
        if (alGetError() != AL_NO_ERROR) { Log("OpenAL: source / buffer creation failed"); Close(); return false; }
        return true;
    }
    void Close() {
        if (ctx) { alcMakeContextCurrent(ctx); }
        if (src) { alSourceStop(src); alSourcei(src, AL_BUFFER, 0); alDeleteSources(1, &src); src = 0; }
        if (!allBufs.empty()) { alDeleteBuffers((ALsizei)allBufs.size(), allBufs.data()); allBufs.clear(); freeBufs.clear(); }
        alcMakeContextCurrent(nullptr);
        if (ctx) { alcDestroyContext(ctx); ctx = nullptr; }
        if (dev) { alcCloseDevice(dev); dev = nullptr; }
    }
    void Reclaim() {
        ALint n = 0;
        alGetSourcei(src, AL_BUFFERS_PROCESSED, &n);
        while (n-- > 0) {
            ALuint b = 0;
            alSourceUnqueueBuffers(src, 1, &b);
            if (b) { freeBufs.push_back(b); }
        }
    }
    int Queued() {
        ALint q = 0;
        alGetSourcei(src, AL_BUFFERS_QUEUED, &q);
        return q;
    }
    // `pad` > 0 (loopback only): that many silent frames are queued behind the chunk. When a voice runs dry, openal-soft fades the held LAST sample out over ~64 frames
    // (it would leak into the next chunk's first frames after a restart); a zero tail makes every loopback chunk independent and exact. A device stream never runs dry.
    bool Queue(const std::int16_t* pcm, std::size_t frames, std::size_t pad = 0) {
        if (freeBufs.empty()) { return false; }
        const ALuint b = freeBufs.back();
        freeBufs.pop_back();
        fbuf.assign((frames + pad) * 2, 0.f); // AL_EXT_FLOAT32: exact (s16 / 32768), and openal-soft does not dither float output
        for (std::size_t i = 0; i < frames * 2; ++i) { fbuf[i] = (float)pcm[i] / 32768.f; }
        alBufferData(b, AL_FORMAT_STEREO_FLOAT32, fbuf.data(), (ALsizei)((frames + pad) * 8), (ALsizei)rate);
        alSourceQueueBuffers(src, 1, &b);
        return alGetError() == AL_NO_ERROR;
    }
    void EnsurePlaying() {
        ALint st = 0;
        alGetSourcei(src, AL_SOURCE_STATE, &st);
        if (st != AL_PLAYING && Queued() > 0) {
            if (st == AL_STOPPED) { ++underruns; }
            alSourcePlay(src);
        }
    }
};

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------- interfaces
struct Buffer3D final : Unk<IDirectSound3DBuffer> {
    std::shared_ptr<BufState> s;
    explicit Buffer3D(std::shared_ptr<BufState> st) : s(std::move(st)) {}
    template<class F> void Set(DWORD apply, F&& f) {
        std::lock_guard lk{s->core->mu};
        f(s->pending);
        if (apply == DS3D_IMMEDIATE) { f(s->cur); } else { s->dirty = true; }
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) { return E_INVALIDARG; }
        *out = nullptr; // no IKsPropertySet (EAX)
        if (iid == IID_IKsPropertySet) {
            static std::atomic<bool> logged{false};
            if (!logged.exchange(true)) { Log("EAX (IKsPropertySet) is not supported: no environmental reverb"); }
        }
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetAllParameters(void*) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetConeAngles(DWORD* i, DWORD* o) override { std::lock_guard lk{s->core->mu}; *i = s->pending.coneIn; *o = s->pending.coneOut; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetConeOrientation(void* p) override { std::lock_guard lk{s->core->mu}; *(Vec3*)p = s->pending.cone; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetConeOutsideVolume(LONG* p) override { std::lock_guard lk{s->core->mu}; *p = s->pending.coneVol; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetMaxDistance(FLOAT* p) override { std::lock_guard lk{s->core->mu}; *p = s->pending.maxDist; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetMinDistance(FLOAT* p) override { std::lock_guard lk{s->core->mu}; *p = s->pending.minDist; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetMode(DWORD* p) override { std::lock_guard lk{s->core->mu}; *p = s->pending.mode; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetPosition(void* p) override { std::lock_guard lk{s->core->mu}; *(Vec3*)p = s->pending.pos; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetVelocity(void* p) override { std::lock_guard lk{s->core->mu}; *(Vec3*)p = s->pending.vel; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetAllParameters(const void*, DWORD) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetConeAngles(DWORD i, DWORD o, DWORD a) override { Set(a, [&](Params3D& p) { p.coneIn = i; p.coneOut = o; }); return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetConeOrientation(FLOAT x, FLOAT y, FLOAT z, DWORD a) override { Set(a, [&](Params3D& p) { p.cone = {x, y, z}; }); return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetConeOutsideVolume(LONG v, DWORD a) override { Set(a, [&](Params3D& p) { p.coneVol = v; }); return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetMaxDistance(FLOAT v, DWORD a) override { Set(a, [&](Params3D& p) { p.maxDist = v; }); return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetMinDistance(FLOAT v, DWORD a) override { Set(a, [&](Params3D& p) { p.minDist = v; }); return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetMode(DWORD m, DWORD a) override { Set(a, [&](Params3D& p) { p.mode = m; }); return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetPosition(FLOAT x, FLOAT y, FLOAT z, DWORD a) override { Set(a, [&](Params3D& p) { p.pos = {x, y, z}; }); return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetVelocity(FLOAT x, FLOAT y, FLOAT z, DWORD a) override { Set(a, [&](Params3D& p) { p.vel = {x, y, z}; }); return DS_OK; }
};

struct Listener final : Unk<IDirectSound3DListener> {
    std::shared_ptr<Core> c;
    explicit Listener(std::shared_ptr<Core> core) : c(std::move(core)) {}
    template<class F> void Set(DWORD apply, F&& f) {
        std::lock_guard lk{c->mu};
        f(c->lisPending);
        if (apply == DS3D_IMMEDIATE) { f(c->lis); } else { c->lisDirty = true; }
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** out) override { if (out) { *out = nullptr; } return E_NOINTERFACE; }
    HRESULT STDMETHODCALLTYPE GetAllParameters(void*) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetDistanceFactor(FLOAT* p) override { std::lock_guard lk{c->mu}; *p = c->lisPending.distanceFactor; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetDopplerFactor(FLOAT* p) override { std::lock_guard lk{c->mu}; *p = c->lisPending.dopplerFactor; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetOrientation(void* f, void* t) override { std::lock_guard lk{c->mu}; *(Vec3*)f = c->lisPending.front; *(Vec3*)t = c->lisPending.top; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetPosition(void* p) override { std::lock_guard lk{c->mu}; *(Vec3*)p = c->lisPending.pos; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetRolloffFactor(FLOAT* p) override { std::lock_guard lk{c->mu}; *p = c->lisPending.rolloffFactor; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetVelocity(void* p) override { std::lock_guard lk{c->mu}; *(Vec3*)p = c->lisPending.vel; return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetAllParameters(const void*, DWORD) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetDistanceFactor(FLOAT v, DWORD a) override { Set(a, [&](ListenerParams& p) { p.distanceFactor = v; }); return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetDopplerFactor(FLOAT v, DWORD a) override { Set(a, [&](ListenerParams& p) { p.dopplerFactor = v; }); return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetOrientation(FLOAT xf, FLOAT yf, FLOAT zf, FLOAT xt, FLOAT yt, FLOAT zt, DWORD a) override {
        Set(a, [&](ListenerParams& p) { p.front = {xf, yf, zf}; p.top = {xt, yt, zt}; });
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE SetPosition(FLOAT x, FLOAT y, FLOAT z, DWORD a) override { Set(a, [&](ListenerParams& p) { p.pos = {x, y, z}; }); return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetRolloffFactor(FLOAT v, DWORD a) override { Set(a, [&](ListenerParams& p) { p.rolloffFactor = v; }); return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetVelocity(FLOAT x, FLOAT y, FLOAT z, DWORD a) override { Set(a, [&](ListenerParams& p) { p.vel = {x, y, z}; }); return DS_OK; }
    HRESULT STDMETHODCALLTYPE CommitDeferredSettings() override { c->Commit(); return DS_OK; }
};

struct Buffer final : Unk<IDirectSoundBuffer8> {
    std::shared_ptr<BufState> s;
    explicit Buffer(std::shared_ptr<BufState> st) : s(std::move(st)) {}
    std::mutex& M() const { return s->core->mu; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) { return E_INVALIDARG; }
        *out = nullptr;
        if (iid == IID_IDirectSound3DBuffer) {
            if (!(s->flags & DSBCAPS_CTRL3D) || s->primary) { return E_NOINTERFACE; }
            *out = new Buffer3D(s);
            return DS_OK;
        }
        if (iid == IID_IDirectSound3DListener) {
            if (!s->primary) { return E_NOINTERFACE; }
            *out = new Listener(s->core);
            return DS_OK;
        }
        return E_NOINTERFACE; // IKsPropertySet, anything else
    }
    HRESULT STDMETHODCALLTYPE GetCaps(LPDSBCAPS caps) override {
        if (!caps) { return DSERR_INVALIDPARAM; }
        *caps = {sizeof(DSBCAPS), s->flags, (DWORD)s->data.size(), 0, 0};
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentPosition(LPDWORD play, LPDWORD write) override {
        std::lock_guard lk{M()};
        if (play) { *play = s->PlayCursor(); }
        if (write) { *write = s->WriteCursor(); }
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE GetFormat(LPWAVEFORMATEX f, DWORD size, LPDWORD written) override {
        std::lock_guard lk{M()};
        const DWORD n = std::min<DWORD>(size, sizeof(WAVEFORMATEX));
        if (f) { std::memcpy(f, &s->fmt, n); }
        if (written) { *written = n; }
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE GetVolume(LPLONG v) override { std::lock_guard lk{M()}; *v = s->volume; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetPan(LPLONG p) override { std::lock_guard lk{M()}; *p = s->pan; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetFrequency(LPDWORD f) override { std::lock_guard lk{M()}; *f = s->freq; return DS_OK; }
    HRESULT STDMETHODCALLTYPE GetStatus(LPDWORD st) override {
        std::lock_guard lk{M()};
        *st = DSBSTATUS_LOCSOFTWARE | (s->playing ? DSBSTATUS_PLAYING : 0) | (s->playing && s->looping ? DSBSTATUS_LOOPING : 0);
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE Initialize(IDirectSound*, LPCDSBUFFERDESC) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE Lock(DWORD offset, DWORD bytes, LPVOID* p1, LPDWORD b1, LPVOID* p2, LPDWORD b2, DWORD lockFlags) override {
        std::lock_guard lk{M()};
        if (!p1 || !b1 || s->data.empty()) { return DSERR_INVALIDPARAM; }
        const DWORD size = (DWORD)s->data.size();
        if (lockFlags & DSBLOCK_FROMWRITECURSOR) { offset = s->WriteCursor(); }
        if (lockFlags & DSBLOCK_ENTIREBUFFER) { offset = 0; bytes = size; }
        if (offset >= size || bytes > size) { return DSERR_INVALIDPARAM; }
        const DWORD first = std::min(bytes, size - offset);
        *p1 = s->data.data() + offset;
        *b1 = first;
        if (p2) { *p2 = first < bytes ? s->data.data() : nullptr; }
        if (b2) { *b2 = bytes - first; }
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE Play(DWORD, DWORD, DWORD playFlags) override {
        std::lock_guard lk{M()};
        if (s->primary) { return DS_OK; }
        s->playing = true;
        s->looping = (playFlags & DSBPLAY_LOOPING) != 0;
        ++s->core->playCalls;
        if (s->core->playLogs.fetch_add(1) < 24) {
            Log("Play buf=%p bytes=%zu %uHz %uch %ubit loop=%d vol=%ld pan=%ld%s", (void*)s.get(), s->data.size(), (unsigned)s->freq, (unsigned)s->fmt.nChannels,
                (unsigned)s->fmt.wBitsPerSample, (int)s->looping, (long)s->volume, (long)s->pan, (s->flags & DSBCAPS_CTRL3D) ? " 3D" : "");
        }
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE SetCurrentPosition(DWORD p) override {
        std::lock_guard lk{M()};
        if (!s->data.empty() && p >= s->data.size()) { return DSERR_INVALIDPARAM; }
        s->pos = (double)(p / s->Block());
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE SetFormat(const WAVEFORMATEX* f) override {
        std::lock_guard lk{M()};
        if (!f) { return DSERR_INVALIDPARAM; }
        s->fmt  = *f;
        s->freq = s->fmt.nSamplesPerSec;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE SetVolume(LONG v) override {
        if (v < DSBVOLUME_MIN || v > DSBVOLUME_MAX) { return DSERR_INVALIDPARAM; }
        std::lock_guard lk{M()};
        s->volume = v;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE SetPan(LONG p) override {
        if (p < -10000 || p > 10000) { return DSERR_INVALIDPARAM; }
        std::lock_guard lk{M()};
        s->pan = p;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE SetFrequency(DWORD f) override {
        std::lock_guard lk{M()};
        if (f == 0) { f = s->fmt.nSamplesPerSec; } // DSBFREQUENCY_ORIGINAL
        if (f < kMinFreq || f > kMaxFreq) { return DSERR_INVALIDPARAM; }
        s->freq = f;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE Stop() override {
        std::lock_guard lk{M()};
        s->playing = false;
        return DS_OK;
    }
    HRESULT STDMETHODCALLTYPE Unlock(LPVOID, DWORD, LPVOID, DWORD) override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE Restore() override { return DS_OK; }
    HRESULT STDMETHODCALLTYPE SetFX(DWORD n, DSEFFECTDESC*, DWORD*) override {
        if (n == 0) { return DS_OK; }
        static std::atomic<bool> logged{false};
        if (!logged.exchange(true)) { Log("DirectSound FX (radio bass EQ) is not supported"); }
        return DSERR_UNSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE AcquireResources(DWORD, DWORD, DWORD*) override { return DSERR_UNSUPPORTED; }
    HRESULT STDMETHODCALLTYPE GetObjectInPath(REFGUID, DWORD, REFGUID, LPVOID* o) override { if (o) { *o = nullptr; } return DSERR_UNSUPPORTED; }
};

// ---- the device: owns the mixer core, the OpenAL output and the pump thread
struct Engine final : Unk<IDirectSound8> {
    std::shared_ptr<Core> core = std::make_shared<Core>();
    Output                mode{Output::Null};
    bool                  manual{false};
    AlOut                 al;
    WavWriter             wav;
    std::thread           th;
    std::atomic<bool>     stop{false};
    // diagnostics
    std::atomic<int>      peak{0};
    std::atomic<bool>     announced{false};
    Clock::time_point     t0{Clock::now()};

    ~Engine() override {
        stop = true;
        if (th.joinable()) { th.join(); }
        const double sec = (double)core->framesMixed.load() / (double)core->rate;
        Log("device closed: mode=%s, %.1f s mixed, %llu Play() calls, peak %d/32767%s", ModeName(), sec, (unsigned long long)core->playCalls.load(), peak.load(),
            al.underruns ? " (OpenAL underruns occurred)" : "");
        wav.Close();
        if (al.dev) { al.Close(); }
    }
    const char* ModeName() const { return mode == Output::Device ? "device" : mode == Output::Loopback ? "loopback" : "null"; }

    bool Start(const Options& opt) {
        Output want = opt.output;
        std::uint32_t rate = opt.rate;
        const char* env = std::getenv("NOTSA_AUDIO");
        if (want == Output::Auto) {
            want = Output::Device;
            if (env) {
                const std::string e = env;
                if (e == "loopback") { want = Output::Loopback; } else if (e == "null" || e == "off" || e == "0") { want = Output::Null; }
            }
        }
        if (opt.output == Output::Auto) {
            if (const char* r = std::getenv("NOTSA_AUDIO_RATE")) { rate = (std::uint32_t)std::max(8000, std::atoi(r)); }
        }
        mode = want;
        if (mode == Output::Device && !al.Open(false, 0)) { mode = Output::Null; }
        if (mode == Output::Loopback && !al.Open(true, rate)) { mode = Output::Null; }
        core->rate = mode == Output::Null ? rate : al.rate;
        const char* wavPath = opt.wavPath ? opt.wavPath : (opt.output == Output::Auto ? std::getenv("NOTSA_AUDIO_WAV") : nullptr);
        if (wavPath && *wavPath && wav.Open(wavPath, core->rate)) { Log("capturing the output to %s", wavPath); }
        manual = opt.manual;
        Log("DirectSound over OpenAL: output=%s%s%s, mix rate %u Hz, chunk %zu frames%s", ModeName(), al.dev ? " device=" : "", al.dev ? al.name.c_str() : "", (unsigned)core->rate,
            kChunkFrames, manual ? ", manual pump" : "");
        if (mode == Output::Device) { alcMakeContextCurrent(al.ctx); }
        if (!manual) { th = std::thread([this] { Run(); }); }
        return true;
    }

    void Observe(const std::int16_t* pcm, std::size_t frames) {
        int pk = 0;
        for (std::size_t i = 0; i < frames * 2; ++i) { pk = std::max(pk, std::abs((int)pcm[i])); }
        if (pk > peak.load()) { peak = pk; }
        if (pk > 0 && !announced.exchange(true)) {
            Log("first non-silent output after %lld ms (peak %d)", (long long)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count(), pk);
        }
    }
    // one chunk of `n` frames into `out` (Loopback: through OpenAL; Device: queued for playback; Null: just mixed)
    void Chunk(std::int16_t* out, std::size_t n) {
        core->Mix(out, n);
        if (mode == Output::Loopback) {
            al.Reclaim();
            if (al.Queue(out, n, kLoopbackPad)) {
                al.EnsurePlaying();
                al.RenderLoopback(out, n, kLoopbackPad);
            }
        }
        Observe(out, n);
        wav.Append(out, n);
    }
    void Run() {
        std::vector<std::int16_t> chunk(kChunkFrames * 2);
        std::uint64_t rendered = 0;
        const auto start = Clock::now();
        while (!stop) {
            if (mode == Output::Device) {
                al.Reclaim();
                while (al.Queued() < kQueueTarget && !al.freeBufs.empty()) {
                    core->Mix(chunk.data(), kChunkFrames);
                    Observe(chunk.data(), kChunkFrames);
                    wav.Append(chunk.data(), kChunkFrames);
                    if (!al.Queue(chunk.data(), kChunkFrames)) { break; }
                }
                al.EnsurePlaying();
            } else { // Loopback / Null: paced by the steady clock
                const double el  = std::chrono::duration<double>(Clock::now() - start).count();
                const auto   due = (std::int64_t)(el * core->rate) - (std::int64_t)rendered;
                for (auto d = due; d >= (std::int64_t)kChunkFrames && !stop; d -= (std::int64_t)kChunkFrames) {
                    Chunk(chunk.data(), kChunkFrames);
                    rendered += kChunkFrames;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** out) override { if (out) { *out = nullptr; } return E_NOINTERFACE; }
    HRESULT STDMETHODCALLTYPE CreateSoundBuffer(LPCDSBUFFERDESC d, IDirectSoundBuffer** out, IUnknown*) override {
        if (!d || !out || d->dwSize < sizeof(DSBUFFERDESC) - sizeof(GUID)) { return DSERR_INVALIDPARAM; }
        const bool primary = (d->dwFlags & DSBCAPS_PRIMARYBUFFER) != 0;
        if (!primary && (d->dwBufferBytes == 0 || !d->lpwfxFormat) && !(d->dwFlags & DSBCAPS_CTRL3D)) { return DSERR_INVALIDPARAM; }
        *out = new Buffer(std::make_shared<BufState>(core, d));
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
        auto* src = dynamic_cast<Buffer*>(original);
        if (!src || !dup) { return DSERR_INVALIDPARAM; }
        DSBUFFERDESC d{};
        std::vector<std::uint8_t> bytes;
        WAVEFORMATEX fmt;
        {
            std::lock_guard lk{core->mu};
            d.dwSize = sizeof(DSBUFFERDESC);
            d.dwFlags = src->s->flags;
            d.dwBufferBytes = (DWORD)src->s->data.size();
            fmt = src->s->fmt;
            bytes = src->s->data;
        }
        d.lpwfxFormat = &fmt;
        auto st = std::make_shared<BufState>(core, &d);
        {
            std::lock_guard lk{core->mu};
            st->data = std::move(bytes);
        }
        *dup = new Buffer(std::move(st));
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

HRESULT Create(const Options& opt, IDirectSound8** out) {
    if (!out) { return DSERR_INVALIDPARAM; }
    auto* e = new Engine;
    if (!e->Start(opt)) {
        e->Release();
        return DSERR_NODRIVER;
    }
    *out = e;
    return DS_OK;
}

std::size_t Render(IDirectSound8* dev, std::int16_t* out, std::size_t frames) {
    auto* e = dynamic_cast<Engine*>(dev);
    if (!e || !e->manual) { return 0; }
    for (std::size_t done = 0; done < frames;) {
        const std::size_t n = std::min(kChunkFrames, frames - done);
        e->Chunk(out + done * 2, n);
        done += n;
    }
    return frames;
}
bool UsesOpenAL(IDirectSound8* dev) {
    auto* e = dynamic_cast<Engine*>(dev);
    return e && e->al.dev != nullptr;
}
const char* OutputName(IDirectSound8* dev) {
    auto* e = dynamic_cast<Engine*>(dev);
    return e ? e->ModeName() : "?";
}
std::uint32_t MixRate(IDirectSound8* dev) {
    auto* e = dynamic_cast<Engine*>(dev);
    return e ? e->core->rate : 0;
}
} // namespace dsal

HRESULT CreatePortableDirectSound(IDirectSound8** out) {
    return dsal::Create(dsal::Options{}, out);
}
} // namespace notsa::portable
#endif
