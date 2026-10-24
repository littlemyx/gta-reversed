// MAC-W3 host test: the REAL source/portable/dsound_al.cpp (DirectSound over a software mixer whose output goes through OpenAL) driven the way the game's audio hardware layer
// drives it, with the output captured from an ALC_SOFT_loopback device (nothing audible, no sound card needed).
//   - passthrough of a known PCM buffer (mono / stereo, 16 bit, exact samples) through the OpenAL source, silence after the end, status / cursors
//   - volume (hundredths of dB), pan (opposite channel attenuation), frequency (resampling ratio), looping, SetCurrentPosition, Stop / resume
//   - Lock / Unlock ring wrap, clipping of a two-buffer mix, DuplicateSoundBuffer
//   - 3D: deferred vs immediate settings, azimuth -> pan, rolloff, DS3DMODE_DISABLE
//   - the same scenario on the Null (no OpenAL) mixer equals the OpenAL loopback output
//   - the threaded device (real-time pump) with a WAV capture; the default playback device opens (silent buffer only)
// Build: tools/portable/run_dsound_al_test.sh
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "../../source/portable/dsound_al.h"

using namespace notsa::portable::dsal;

static int g_fail = 0, g_checks = 0;
#define CHECK(c)                                                                                         \
    do {                                                                                                 \
        ++g_checks;                                                                                      \
        if (!(c)) { ++g_fail; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); }                \
    } while (0)
#define CHECK_NEAR(a, b, tol)                                                                            \
    do {                                                                                                 \
        ++g_checks;                                                                                      \
        const double va_ = (double)(a), vb_ = (double)(b);                                               \
        if (!(std::fabs(va_ - vb_) <= (double)(tol))) { ++g_fail; std::printf("  FAIL %s:%d  %s=%g vs %s=%g (tol %g)\n", __FILE__, __LINE__, #a, va_, #b, vb_, (double)(tol)); } \
    } while (0)

static IDirectSound8* MakeDev(Output o = Output::Loopback, bool manual = true, const char* wav = nullptr) {
    Options opt;
    opt.output  = o;
    opt.rate    = 48000;
    opt.manual  = manual;
    opt.wavPath = wav;
    IDirectSound8* d = nullptr;
    const HRESULT hr = Create(opt, &d);
    if (FAILED(hr) || !d) { std::printf("Create failed\n"); std::exit(2); }
    return d;
}
static IDirectSoundBuffer8* MakeBuf(IDirectSound8* ds, int ch, int bits, unsigned rate, size_t frames, DWORD extra = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLPAN) {
    WAVEFORMATEX f{WAVE_FORMAT_PCM, (WORD)ch, rate, (DWORD)(rate * ch * bits / 8), (WORD)(ch * bits / 8), (WORD)bits, 0};
    DSBUFFERDESC d{};
    d.dwSize        = sizeof(DSBUFFERDESC);
    d.dwFlags       = DSBCAPS_GLOBALFOCUS | DSBCAPS_LOCSOFTWARE | extra;
    d.dwBufferBytes = (DWORD)(frames * ch * bits / 8);
    d.lpwfxFormat   = &f;
    IDirectSoundBuffer* b = nullptr;
    const HRESULT hr = ds->CreateSoundBuffer(&d, &b, nullptr);
    if (FAILED(hr)) { std::printf("CreateSoundBuffer failed %x\n", (unsigned)hr); std::exit(2); }
    return static_cast<IDirectSoundBuffer8*>(b);
}
static void Fill(IDirectSoundBuffer8* b, const std::vector<std::int16_t>& pcm) {
    void* p1 = nullptr; DWORD n1 = 0;
    CHECK(SUCCEEDED(b->Lock(0, (DWORD)(pcm.size() * 2), &p1, &n1, nullptr, nullptr, 0)));
    std::memcpy(p1, pcm.data(), pcm.size() * 2);
    b->Unlock(p1, n1, nullptr, 0);
}
static std::vector<std::int16_t> Ramp(size_t n, int mul = 37, int mod = 20000) {
    std::vector<std::int16_t> v(n);
    for (size_t i = 0; i < n; ++i) { v[i] = (std::int16_t)((int)((i * (size_t)mul) % (size_t)mod) - mod / 2); }
    return v;
}
static std::vector<std::int16_t> Const(size_t n, int v) { return std::vector<std::int16_t>(n, (std::int16_t)v); }
static std::vector<std::int16_t> Rend(IDirectSound8* d, size_t frames) {
    std::vector<std::int16_t> o(frames * 2, 0x5555);
    CHECK(Render(d, o.data(), frames) == frames);
    return o;
}
static bool AllZero(const std::int16_t* p, size_t n) { for (size_t i = 0; i < n; ++i) { if (p[i]) return false; } return true; }
// max |out - expect|
static double MaxDiff(const std::vector<std::int16_t>& out, size_t from, size_t frames, const std::vector<double>& expL, const std::vector<double>& expR) {
    double m = 0;
    for (size_t i = 0; i < frames; ++i) {
        m = std::max(m, std::fabs(out[(from + i) * 2] - expL[i]));
        m = std::max(m, std::fabs(out[(from + i) * 2 + 1] - expR[i]));
    }
    return m;
}

static void TestDevice() {
    std::printf("device\n");
    IDirectSound8* d = MakeDev();
    CHECK(UsesOpenAL(d));
    CHECK(std::strcmp(OutputName(d), "loopback") == 0);
    CHECK(MixRate(d) == 48000);
    DSCAPS caps{}; caps.dwSize = sizeof(DSCAPS);
    CHECK(SUCCEEDED(d->GetCaps(&caps)));
    CHECK(caps.dwFreeHw3DAllBuffers == 0); // software device: the game selects its 48 software channels
    DWORD spk = 0;
    CHECK(SUCCEEDED(d->GetSpeakerConfig(&spk)) && spk == 3);
    auto* b = MakeBuf(d, 1, 16, 22050, 100, DSBCAPS_CTRL3D | DSBCAPS_CTRLVOLUME);
    IDirectSound3DBuffer* b3 = nullptr;
    CHECK(SUCCEEDED(b->QueryInterface(IID_IDirectSound3DBuffer, (void**)&b3)) && b3);
    IKsPropertySet* ks = nullptr;
    CHECK(FAILED(b3->QueryInterface(IID_IKsPropertySet, (void**)&ks)) && !ks); // no EAX
    CHECK(b->SetFX(0, nullptr, nullptr) == DS_OK);
    DSEFFECTDESC fx{};
    CHECK(b->SetFX(1, &fx, nullptr) == DSERR_UNSUPPORTED);
    // invalid parameters are rejected like dsound does
    CHECK(b->SetVolume(1) == DSERR_INVALIDPARAM);
    CHECK(b->SetVolume(-10001) == DSERR_INVALIDPARAM);
    CHECK(b->SetPan(10001) == DSERR_INVALIDPARAM);
    CHECK(b->SetPan(-10001) == DSERR_INVALIDPARAM);
    CHECK(b->SetFrequency(99) == DSERR_INVALIDPARAM);
    CHECK(b->SetFrequency(200001) == DSERR_INVALIDPARAM);
    CHECK(b->SetCurrentPosition(200) == DSERR_INVALIDPARAM);
    CHECK(b->SetFrequency(44100) == DS_OK);
    DWORD f = 0; CHECK(b->GetFrequency(&f) == DS_OK && f == 44100);
    CHECK(b->SetFrequency(0) == DS_OK); // DSBFREQUENCY_ORIGINAL
    CHECK(b->GetFrequency(&f) == DS_OK && f == 22050);
    b3->Release(); b->Release(); d->Release();
}

static void TestPassthrough() {
    std::printf("passthrough\n");
    IDirectSound8* d = MakeDev();
    // mono 16 bit at the mix rate: out L = R = source, exactly; silence after the end; the buffer stops itself
    const size_t N = 2000;
    auto src = Ramp(N);
    auto* b  = MakeBuf(d, 1, 16, 48000, N);
    Fill(b, src);
    CHECK(b->SetVolume(0) == DS_OK);
    DWORD st = 1;
    b->GetStatus(&st);
    CHECK(!(st & DSBSTATUS_PLAYING));
    auto idle = Rend(d, 600);
    CHECK(AllZero(idle.data(), idle.size())); // a stopped buffer is silent
    CHECK(b->Play(0, 0, 0) == DS_OK);
    b->GetStatus(&st);
    CHECK((st & DSBSTATUS_PLAYING) && !(st & DSBSTATUS_LOOPING));
    auto o = Rend(d, 3000);
    size_t bad = 0;
    for (size_t i = 0; i < N; ++i) { if (std::abs(o[2 * i] - src[i]) > 1 || std::abs(o[2 * i + 1] - src[i]) > 1) ++bad; }
    CHECK(bad == 0);
    CHECK(AllZero(o.data() + N * 2, (3000 - N) * 2));
    b->GetStatus(&st);
    CHECK(!(st & DSBSTATUS_PLAYING));
    DWORD pc = 99, wc = 99;
    b->GetCurrentPosition(&pc, &wc);
    CHECK(pc == 0);
    b->Release();

    // stereo 16 bit: L and R keep their own samples
    auto* s = MakeBuf(d, 2, 16, 48000, 700);
    std::vector<std::int16_t> st2(1400);
    for (size_t i = 0; i < 700; ++i) { st2[2 * i] = (std::int16_t)(i * 20 - 7000); st2[2 * i + 1] = (std::int16_t)(7000 - i * 11); }
    Fill(s, st2);
    s->Play(0, 0, 0);
    auto o2 = Rend(d, 800);
    bad = 0;
    for (size_t i = 0; i < 700; ++i) { if (std::abs(o2[2 * i] - st2[2 * i]) > 1 || std::abs(o2[2 * i + 1] - st2[2 * i + 1]) > 1) ++bad; }
    CHECK(bad == 0);
    s->Release();

    // 8 bit unsigned mono
    auto* e = MakeBuf(d, 1, 8, 48000, 256);
    void* p; DWORD n;
    e->Lock(0, 256, &p, &n, nullptr, nullptr, 0);
    for (int i = 0; i < 256; ++i) { ((std::uint8_t*)p)[i] = (std::uint8_t)i; }
    e->Unlock(p, n, nullptr, 0);
    e->Play(0, 0, 0);
    auto o3 = Rend(d, 300);
    bad = 0;
    for (int i = 0; i < 256; ++i) { if (std::abs(o3[2 * i] - (i - 128) * 256) > 2) ++bad; }
    CHECK(bad == 0);
    e->Release();
    d->Release();
}

static void TestVolumePan() {
    std::printf("volume / pan\n");
    IDirectSound8* d = MakeDev();
    const size_t N = 1500;
    auto src = Ramp(N, 41, 24000);
    for (LONG v : {-100, -600, -2000, -4000, -6000}) {
        auto* b = MakeBuf(d, 1, 16, 48000, N);
        Fill(b, src);
        b->SetVolume(v);
        b->Play(0, 0, 0);
        auto o = Rend(d, N + 100);
        const double g = std::pow(10.0, v / 2000.0);
        std::vector<double> e(N);
        for (size_t i = 0; i < N; ++i) { e[i] = src[i] * g; }
        CHECK(MaxDiff(o, 0, N, e, e) <= 2.0);
        LONG got = 0; b->GetVolume(&got); CHECK(got == v);
        b->Release();
    }
    { // -10000 = silence
        auto* b = MakeBuf(d, 1, 16, 48000, N);
        Fill(b, src); b->SetVolume(-10000); b->Play(0, 0, 0);
        auto o = Rend(d, N);
        CHECK(AllZero(o.data(), o.size()));
        b->Release();
    }
    struct P { LONG pan; };
    for (LONG pan : {-10000, -5000, -3000, 0, 1000, 3000, 7000, 10000}) {
        auto* b = MakeBuf(d, 1, 16, 48000, N);
        Fill(b, src); b->SetPan(pan); b->Play(0, 0, 0);
        auto o = Rend(d, N);
        const double gl = pan > 0 ? (pan >= 10000 ? 0.0 : std::pow(10.0, -pan / 2000.0)) : 1.0;
        const double gr = pan < 0 ? (pan <= -10000 ? 0.0 : std::pow(10.0, pan / 2000.0)) : 1.0;
        std::vector<double> eL(N), eR(N);
        for (size_t i = 0; i < N; ++i) { eL[i] = src[i] * gl; eR[i] = src[i] * gr; }
        CHECK(MaxDiff(o, 0, N, eL, eR) <= 2.0);
        b->Release();
    }
    d->Release();
}

static void TestFrequencyLoop() {
    std::printf("frequency / loop / cursors\n");
    IDirectSound8* d = MakeDev();
    const size_t N = 1200;
    auto src = Ramp(N, 13, 16000);
    { // 24 kHz buffer at its own rate -> 2x upsampling with linear interpolation
        auto* b = MakeBuf(d, 1, 16, 24000, N);
        Fill(b, src); b->Play(0, 0, 0);
        auto o = Rend(d, 2 * N);
        double m = 0;
        for (size_t k = 0; k + 1 < N; ++k) {
            m = std::max(m, (double)std::abs(o[2 * (2 * k)] - src[k]));
            m = std::max(m, std::fabs(o[2 * (2 * k + 1)] - (src[k] + src[k + 1]) / 2.0));
        }
        CHECK(m <= 2.0);
        b->Release();
    }
    { // SetFrequency(48000) on a 24 kHz buffer: 1:1; 96000: every second sample
        auto* b = MakeBuf(d, 1, 16, 24000, N);
        Fill(b, src); b->SetFrequency(48000); b->Play(0, 0, 0);
        auto o = Rend(d, N);
        double m = 0;
        for (size_t i = 0; i < N; ++i) { m = std::max(m, (double)std::abs(o[2 * i] - src[i])); }
        CHECK(m <= 1.0);
        b->Release();
        auto* c = MakeBuf(d, 1, 16, 24000, N);
        Fill(c, src); c->SetFrequency(96000); c->Play(0, 0, 0);
        auto o2 = Rend(d, N / 2 + 300);
        m = 0;
        for (size_t i = 0; i < N / 2; ++i) { m = std::max(m, (double)std::abs(o2[2 * i] - src[2 * i])); }
        CHECK(m <= 1.0);
        CHECK(AllZero(o2.data() + (N / 2) * 2, 200 * 2)); // ended after N/2 output frames
        c->Release();
    }
    { // looping: out[i] = src[i % 100] over 3.5 loops; status LOOPING; cursor wraps; Stop silences
        auto* b = MakeBuf(d, 1, 16, 48000, 100);
        auto s100 = Ramp(100, 97, 9000);
        Fill(b, s100); b->Play(0, 0, DSBPLAY_LOOPING);
        auto o = Rend(d, 350);
        size_t bad = 0;
        for (size_t i = 0; i < 350; ++i) { if (std::abs(o[2 * i] - s100[i % 100]) > 1) ++bad; }
        CHECK(bad == 0);
        DWORD st = 0; b->GetStatus(&st);
        CHECK((st & DSBSTATUS_PLAYING) && (st & DSBSTATUS_LOOPING));
        DWORD pc = 0, wc = 0; b->GetCurrentPosition(&pc, &wc);
        CHECK(pc == 50 * 2);
        b->Stop(); b->GetStatus(&st); CHECK(!(st & DSBSTATUS_PLAYING));
        auto o2 = Rend(d, 100); CHECK(AllZero(o2.data(), o2.size()));
        b->GetCurrentPosition(&pc, &wc); CHECK(pc == 100); // Stop keeps the cursor: 350 + 100 silent frames -> position 50 stays (frames * 2 bytes)
        b->Release();
    }
    { // cursors and SetCurrentPosition
        auto* b = MakeBuf(d, 1, 16, 48000, 4800);
        auto s = Ramp(4800, 29, 18000);
        Fill(b, s); b->Play(0, 0, 0);
        Rend(d, 1000);
        DWORD pc = 0, wc = 0; b->GetCurrentPosition(&pc, &wc);
        CHECK(pc == 2000);
        const DWORD lead = (wc + 9600 - pc) % 9600;
        CHECK(lead > 0 && lead <= 9600 / 2);
        b->Stop();
        CHECK(b->SetCurrentPosition(200) == DS_OK);
        b->GetCurrentPosition(&pc, &wc); CHECK(pc == 200);
        b->Play(0, 0, 0);
        auto o = Rend(d, 50);
        CHECK(std::abs(o[0] - s[100]) <= 1 && std::abs(o[2 * 49] - s[149]) <= 1);
        b->Stop();
        auto idle = Rend(d, 64); CHECK(AllZero(idle.data(), idle.size()));
        b->Play(0, 0, 0); // resumes where it stopped
        auto o2 = Rend(d, 10);
        CHECK(std::abs(o2[0] - s[150]) <= 1);
        b->Release();
    }
    d->Release();
}

static void TestLockMix() {
    std::printf("lock / mix / duplicate\n");
    IDirectSound8* d = MakeDev();
    {
        auto* b = MakeBuf(d, 1, 16, 48000, 1000); // 2000 bytes
        void *p1 = nullptr, *p2 = nullptr; DWORD n1 = 0, n2 = 0;
        CHECK(SUCCEEDED(b->Lock(1800, 600, &p1, &n1, &p2, &n2, 0)));
        CHECK(n1 == 200 && n2 == 400 && p2 != nullptr);
        for (DWORD i = 0; i < n1 / 2; ++i) { ((std::int16_t*)p1)[i] = (std::int16_t)(1000 + i); }
        for (DWORD i = 0; i < n2 / 2; ++i) { ((std::int16_t*)p2)[i] = (std::int16_t)(2000 + i); }
        b->Unlock(p1, n1, p2, n2);
        CHECK(SUCCEEDED(b->Lock(0, 0, &p1, &n1, nullptr, nullptr, DSBLOCK_ENTIREBUFFER)) && n1 == 2000);
        const auto* v = (const std::int16_t*)p1;
        CHECK(v[900] == 1000 && v[999] == 1099 && v[0] == 2000 && v[199] == 2199 && v[200] == 0);
        b->Unlock(p1, n1, nullptr, 0);
        CHECK(FAILED(b->Lock(2000, 4, &p1, &n1, &p2, &n2, 0)));
        CHECK(FAILED(b->Lock(0, 2002, &p1, &n1, &p2, &n2, 0)));
        // no wrap needed: p2 is null / 0 bytes
        CHECK(SUCCEEDED(b->Lock(100, 100, &p1, &n1, &p2, &n2, 0)) && n1 == 100 && n2 == 0 && p2 == nullptr);
        CHECK(SUCCEEDED(b->Lock(0, 64, &p1, &n1, nullptr, nullptr, DSBLOCK_FROMWRITECURSOR))); // stopped: write cursor 0
        b->Release();
    }
    { // two buffers sum, and clip
        auto* a = MakeBuf(d, 1, 16, 48000, 600);
        auto* b = MakeBuf(d, 1, 16, 48000, 600);
        auto* c = MakeBuf(d, 1, 16, 48000, 600);
        Fill(a, Const(600, 20000)); Fill(b, Const(600, 20000)); Fill(c, Const(600, -20000));
        a->Play(0, 0, 0); b->Play(0, 0, 0);
        auto o = Rend(d, 100);
        CHECK(o[0] == 32767 && o[1] == 32767 && o[198] == 32767); // 40000 clips
        c->Play(0, 0, 0);
        auto o2 = Rend(d, 100);
        CHECK(std::abs(o2[0] - 20000) <= 1); // 20000 + 20000 - 20000
        a->Stop(); b->Stop();
        auto o3 = Rend(d, 100);
        CHECK(std::abs(o3[0] + 20000) <= 1);
        c->Stop();
        Fill(a, Const(600, -20000)); Fill(b, Const(600, -20000));
        a->SetCurrentPosition(0); b->SetCurrentPosition(0); a->Play(0, 0, 0); b->Play(0, 0, 0);
        auto o4 = Rend(d, 20);
        CHECK(o4[0] == -32768);
        a->Release(); b->Release(); c->Release();
    }
    { // DuplicateSoundBuffer copies the data and the format
        auto* a = MakeBuf(d, 1, 16, 48000, 300);
        auto s = Ramp(300, 51, 12000);
        Fill(a, s);
        IDirectSoundBuffer* dup = nullptr;
        CHECK(SUCCEEDED(d->DuplicateSoundBuffer(a, &dup)) && dup);
        dup->Play(0, 0, 0);
        auto o = Rend(d, 300);
        size_t bad = 0;
        for (size_t i = 0; i < 300; ++i) { if (std::abs(o[2 * i] - s[i]) > 1) ++bad; }
        CHECK(bad == 0);
        dup->Release(); a->Release();
    }
    d->Release();
}

static void TestThreeD() {
    std::printf("3D\n");
    IDirectSound8* d = MakeDev();
    // the listener exactly as CAEAudioHardware::InitDirectSoundListener sets it up
    DSBUFFERDESC pd{}; pd.dwSize = sizeof(DSBUFFERDESC); pd.dwFlags = DSBCAPS_CTRL3D | DSBCAPS_PRIMARYBUFFER;
    IDirectSoundBuffer* prim = nullptr;
    CHECK(SUCCEEDED(d->CreateSoundBuffer(&pd, &prim, nullptr)));
    WAVEFORMATEX wf{1, 2, 48000, 192000, 4, 16, 0};
    CHECK(SUCCEEDED(prim->SetFormat(&wf)));
    IDirectSound3DListener* L = nullptr;
    CHECK(SUCCEEDED(prim->QueryInterface(IID_IDirectSound3DListener, (void**)&L)) && L);
    L->SetPosition(0.f, 0.f, 0.f, TRUE);
    L->SetOrientation(0.f, 1.f, 0.f, 0.f, 0.f, -1.f, TRUE);
    L->SetRolloffFactor(0.f, TRUE);
    L->SetDopplerFactor(0.f, TRUE);
    L->CommitDeferredSettings();
    const size_t N = 300;
    auto Play3D = [&](float x, float y, float z, DWORD apply, bool commit, int amp, auto&& cfg) {
        auto* b = MakeBuf(d, 1, 16, 48000, 600, DSBCAPS_CTRL3D | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY);
        Fill(b, Const(600, amp));
        IDirectSound3DBuffer* b3 = nullptr;
        CHECK(SUCCEEDED(b->QueryInterface(IID_IDirectSound3DBuffer, (void**)&b3)) && b3);
        cfg(b3);
        b3->SetPosition(x, y, z, apply);
        if (commit) { L->CommitDeferredSettings(); }
        b->Play(0, 0, DSBPLAY_LOOPING);
        auto o = Rend(d, N);
        b->Release(); b3->Release();
        return std::make_pair((double)o[2 * 100], (double)o[2 * 100 + 1]);
    };
    auto none = [](IDirectSound3DBuffer*) {};
    auto lr = Play3D(0, 10, 0, DS3D_DEFERRED, true, 20000, none); // straight ahead: both ears
    CHECK_NEAR(lr.first, 20000, 2); CHECK_NEAR(lr.second, 20000, 2);
    lr = Play3D(10, 0, 0, DS3D_DEFERRED, true, 20000, none); // right: the left ear is silent
    CHECK_NEAR(lr.first, 0, 2); CHECK_NEAR(lr.second, 20000, 2);
    lr = Play3D(-10, 0, 0, DS3D_DEFERRED, true, 20000, none); // left
    CHECK_NEAR(lr.first, 20000, 2); CHECK_NEAR(lr.second, 0, 2);
    lr = Play3D(10, 10, 0, DS3D_DEFERRED, true, 20000, none); // 45 degrees right: far ear = cos(sin45 * pi/2)
    CHECK_NEAR(lr.first, 20000 * std::cos(std::sqrt(0.5) * 3.14159265358979 / 2), 3); CHECK_NEAR(lr.second, 20000, 2);
    lr = Play3D(0, -10, 0, DS3D_DEFERRED, true, 20000, none); // behind: centre (no front/back cue)
    CHECK_NEAR(lr.first, 20000, 2); CHECK_NEAR(lr.second, 20000, 2);
    // a deferred setting is not applied before CommitDeferredSettings, an immediate one is
    lr = Play3D(10, 0, 0, DS3D_DEFERRED, false, 20000, none);
    CHECK_NEAR(lr.first, 20000, 2); CHECK_NEAR(lr.second, 20000, 2);
    lr = Play3D(10, 0, 0, DS3D_IMMEDIATE, false, 20000, none);
    CHECK_NEAR(lr.first, 0, 2); CHECK_NEAR(lr.second, 20000, 2);
    // DS3DMODE_DISABLE: no 3D processing
    lr = Play3D(10, 0, 0, DS3D_DEFERRED, true, 20000, [](IDirectSound3DBuffer* b3) { b3->SetMode(2, DS3D_IMMEDIATE); });
    CHECK_NEAR(lr.first, 20000, 2); CHECK_NEAR(lr.second, 20000, 2);
    // volume and 3D combine
    lr = Play3D(10, 0, 0, DS3D_DEFERRED, true, 20000, none);
    // rolloff: min 1, rolloff 1, distance 11 straight ahead -> 1 / 11; max distance 5 clamps to 1 / 5
    L->SetRolloffFactor(1.f, TRUE); L->CommitDeferredSettings();
    lr = Play3D(0, 11, 0, DS3D_DEFERRED, true, 22000, none);
    CHECK_NEAR(lr.first, 2000, 3); CHECK_NEAR(lr.second, 2000, 3);
    lr = Play3D(0, 11, 0, DS3D_DEFERRED, true, 22000, [](IDirectSound3DBuffer* b3) { b3->SetMaxDistance(5.f, DS3D_IMMEDIATE); });
    CHECK_NEAR(lr.first, 4400, 3);
    L->SetRolloffFactor(0.f, TRUE); L->CommitDeferredSettings();
    lr = Play3D(0, 500, 0, DS3D_DEFERRED, true, 20000, none); // the game's rolloff 0: distance changes nothing
    CHECK_NEAR(lr.first, 20000, 2);
    // getters return what was set
    float x = 0, y = 0, z = 0; struct V { float x, y, z; } v{};
    L->SetPosition(1.5f, 2.5f, 3.5f, TRUE);
    L->GetPosition(&v); x = v.x; y = v.y; z = v.z;
    CHECK(x == 1.5f && y == 2.5f && z == 3.5f);
    L->Release(); prim->Release(); d->Release();
}

// the same scenario on the Null mixer and on the OpenAL loopback: OpenAL must pass the mix through untouched
static void Scenario(IDirectSound8* d, std::vector<std::int16_t>& out) {
    auto* a = MakeBuf(d, 1, 16, 22050, 900);
    auto* b = MakeBuf(d, 2, 16, 44100, 700);
    Fill(a, Ramp(900, 31, 16000)); std::vector<std::int16_t> st(1400);
    for (size_t i = 0; i < 700; ++i) { st[2 * i] = (std::int16_t)(i * 30 - 9000); st[2 * i + 1] = (std::int16_t)(8000 - i * 17); }
    Fill(b, st);
    a->SetVolume(-700); a->SetPan(2500); a->Play(0, 0, DSBPLAY_LOOPING);
    b->SetFrequency(30000); b->Play(0, 0, 0);
    out = Rend(d, 5000);
    a->Release(); b->Release();
}
static void TestNullEqualsLoopback() {
    std::printf("null == loopback\n");
    IDirectSound8* dn = MakeDev(Output::Null);
    IDirectSound8* dl = MakeDev(Output::Loopback);
    CHECK(!UsesOpenAL(dn) && UsesOpenAL(dl));
    std::vector<std::int16_t> on, ol;
    Scenario(dn, on); Scenario(dl, ol);
    int worst = 0; bool nonzero = false;
    for (size_t i = 0; i < on.size(); ++i) { worst = std::max(worst, std::abs(on[i] - ol[i])); nonzero |= on[i] != 0; }
    CHECK(nonzero);
    CHECK(worst <= 1);
    dn->Release(); dl->Release();
}

static std::vector<std::int16_t> ReadWav(const char* path, std::uint32_t& rate, bool& ok) {
    std::vector<std::int16_t> pcm;
    ok = false;
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return pcm;
    unsigned char h[44];
    if (std::fread(h, 1, 44, f) != 44 || std::memcmp(h, "RIFF", 4) || std::memcmp(h + 8, "WAVEfmt ", 8) || std::memcmp(h + 36, "data", 4)) { std::fclose(f); return pcm; }
    std::uint32_t bytes; std::memcpy(&bytes, h + 40, 4); std::memcpy(&rate, h + 24, 4);
    pcm.resize(bytes / 2);
    const size_t got = std::fread(pcm.data(), 2, pcm.size(), f);
    std::fclose(f);
    ok = got == pcm.size() && h[22] == 2 && h[34] == 16;
    return pcm;
}
static void TestThreaded(const char* wavPath) {
    std::printf("threaded loopback + WAV capture\n");
    std::remove(wavPath);
    {
        IDirectSound8* d = MakeDev(Output::Loopback, false, wavPath);
        auto* b = MakeBuf(d, 1, 16, 48000, 4800);
        Fill(b, Const(4800, 10000));
        b->SetVolume(-600); // 0.5012
        b->Play(0, 0, DSBPLAY_LOOPING);
        std::this_thread::sleep_for(std::chrono::milliseconds(700));
        DWORD pc = 0, wc = 0; b->GetCurrentPosition(&pc, &wc);
        CHECK(pc < 9600);
        b->Stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        b->Release();
        d->Release();
    }
    bool ok; std::uint32_t rate = 0;
    auto pcm = ReadWav(wavPath, rate, ok);
    CHECK(ok && rate == 48000);
    const size_t frames = pcm.size() / 2;
    CHECK(frames >= 48000 * 0.6 && frames <= 48000 * 1.5);
    size_t loud = 0; double expect = 10000 * std::pow(10.0, -600 / 2000.0);
    bool levelOk = true;
    for (size_t i = 0; i < frames; ++i) {
        if (pcm[2 * i] != 0) { ++loud; if (std::fabs(pcm[2 * i] - expect) > 3 || pcm[2 * i] != pcm[2 * i + 1]) levelOk = false; }
    }
    CHECK(loud > 48000 * 0.3);   // played for ~0.7 s
    CHECK(loud < frames);        // and was silent before / after
    CHECK(levelOk);
}
static void TestDeviceSmoke() {
    std::printf("default playback device (silent buffer only)\n");
    Options opt; opt.output = Output::Device;
    IDirectSound8* d = nullptr;
    CHECK(SUCCEEDED(Create(opt, &d)) && d);
    if (UsesOpenAL(d)) {
        std::printf("  opened: output=%s rate=%u\n", OutputName(d), MixRate(d));
        auto* b = MakeBuf(d, 1, 16, 22050, 2205);
        b->Play(0, 0, DSBPLAY_LOOPING); // zeros: audibly nothing
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        DWORD pc = 0, wc = 0; b->GetCurrentPosition(&pc, &wc);
        CHECK(pc > 0);
        b->Release();
    } else {
        std::printf("  NOTE: no OpenAL playback device here, fell back to '%s'\n", OutputName(d));
        CHECK(std::strcmp(OutputName(d), "null") == 0);
    }
    d->Release();
}

int main(int argc, char** argv) {
    const char* wav = argc > 1 ? argv[1] : "/tmp/dsound_al_test.wav";
    TestDevice();
    TestPassthrough();
    TestVolumePan();
    TestFrequencyLoop();
    TestLockMix();
    TestThreeD();
    TestNullEqualsLoopback();
    TestThreaded(wav);
    TestDeviceSmoke();
    std::printf("%d checks, %d failed\n", g_checks, g_fail);
    std::printf(g_fail ? "RESULT DSOUND_AL FAILED\n" : "RESULT DSOUND_AL OK\n");
    return g_fail ? 1 : 0;
}
