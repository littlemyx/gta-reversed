#pragma once

#include "dsound.h"
#include "AESmoothFadeThread.h"
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

//! NOTSA (run build only, env `NOTSA_AUDIO_TAP=<file>`): a SILENT diagnostic tap on every DirectSound secondary buffer the hardware layer creates
//! (static SFX channels + the streaming channel), meant to find crackle objectively without listening. Like AEDirectSoundMute.h it works by attaching
//! the buffer to a COPY of its own vtable (chained on top of the mute's, so both can be on at once); the game code is untouched.
//!
//! Logged (file `NOTSA_AUDIO_TAP`; `NOTSA_AUDIO_TAP_RAW=1` additionally logs every call, otherwise only anomalies + 30 s summaries + long frames):
//!   Lock / Unlock (offset, size, play+write cursor at Lock and Unlock, FNV hash, first/last sample frames), GetCurrentPosition (slot tracking),
//!   Play / Stop / SetCurrentPosition, SetVolume (thread, value, step, fade-thread range check), SetFrequency (value, step, rc), SetPan, Release.
//!   `Frame()` (called once per game frame from CAEAudioHardware::Service) logs the main-loop frame time; `StreamIter()` (CAEStreamThread::MainLoop)
//!   logs how long the stream thread needed per iteration.
//! Anomaly kinds (`!KIND` lines, each with `lf=` = ms since the end of the last frame longer than 50 ms and that frame's length, to correlate with stutters):
//!   STALE_SLOT      stream buffer: the play cursor entered a half that was not refilled since it last played (underrun: 2 s of old audio replayed)
//!   MISSED_SLOT     stream buffer: the play cursor stayed in one half for > 2.1 s between two observations (a whole half was skipped by the refill logic)
//!   WRITE_IN_PLAY   Unlock: the region just written contains the play cursor / the committed span [play, write) (data changed under the cursor)
//!   LATE_REFILL     stream half refilled less than 150 ms before the cursor reaches it
//!   SEAM_JUMP       first frame of a chunk differs from the last frame of the previous chunk by > max(10 x local mean |delta|, 4000)
//!   CLICK           stream chunks only: inside a chunk |delta| > max(12 x running mean |delta|, 8000) (counted per chunk; first 3 positions logged). SFX transients are only counted as info.
//!   LOOP_WRAP       static looped sound: last frame -> first frame jump (checked at Play with looping)
//!   FADE_RANGE      SetVolume from the fade thread outside [min(start,target), max(start,target)] of its active fade entry
//!   FADE_REVERSE    SetVolume from the fade thread moves against the direction of its fade by > 3 dB
//!   SETVOL_REJECTED SetVolume returned an error (value out of range, e.g. the old NaN -> INT_MIN fade bug)
//!   FREQ_REJECTED   SetFrequency returned an error
//!   STREAM_STALL    CAEStreamThread loop iteration (Service + FillBuffer) took > 300 ms or the gap between iterations was > 400 ms
//!   LONG_FRAME      (info, not an anomaly) main loop frame > 50 ms
#ifdef NOTSA_STANDALONE_RUN
namespace notsa::audio_tap {
namespace detail {
// IDirectSoundBuffer8 vtable slots (see AEDirectSoundMute.h)
constexpr size_t SLOT_RELEASE = 2, SLOT_GETCAPS = 3, SLOT_GETCURPOS = 4, SLOT_GETFORMAT = 5, SLOT_LOCK = 11, SLOT_PLAY = 12, SLOT_SETCURPOS = 13,
                 SLOT_SETVOLUME = 15, SLOT_SETPAN = 16, SLOT_SETFREQ = 17, SLOT_STOP = 18, SLOT_UNLOCK = 19, NUM_SLOTS = 24;
constexpr DWORD STREAM_BYTES = 0xC0000, STREAM_SLOT = 0x60000;

using ReleaseFn  = ULONG(STDMETHODCALLTYPE*)(IDirectSoundBuffer*);
using GetCapsFn  = HRESULT(STDMETHODCALLTYPE*)(IDirectSoundBuffer*, LPDSBCAPS);
using GetCurPosFn= HRESULT(STDMETHODCALLTYPE*)(IDirectSoundBuffer*, LPDWORD, LPDWORD);
using GetFormatFn= HRESULT(STDMETHODCALLTYPE*)(IDirectSoundBuffer*, LPWAVEFORMATEX, DWORD, LPDWORD);
using LockFn     = HRESULT(STDMETHODCALLTYPE*)(IDirectSoundBuffer*, DWORD, DWORD, LPVOID*, LPDWORD, LPVOID*, LPDWORD, DWORD);
using UnlockFn   = HRESULT(STDMETHODCALLTYPE*)(IDirectSoundBuffer*, LPVOID, DWORD, LPVOID, DWORD);
using PlayFn     = HRESULT(STDMETHODCALLTYPE*)(IDirectSoundBuffer*, DWORD, DWORD, DWORD);
using StopFn     = HRESULT(STDMETHODCALLTYPE*)(IDirectSoundBuffer*);
using SetPosFn   = HRESULT(STDMETHODCALLTYPE*)(IDirectSoundBuffer*, DWORD);
using SetLongFn  = HRESULT(STDMETHODCALLTYPE*)(IDirectSoundBuffer*, LONG);
using SetDwFn    = HRESULT(STDMETHODCALLTYPE*)(IDirectSoundBuffer*, DWORD);

enum Kind : unsigned {
    K_STALE_SLOT, K_MISSED_SLOT, K_WRITE_IN_PLAY, K_LATE_REFILL, K_SEAM_JUMP, K_CLICK, K_LOOP_WRAP, K_FADE_RANGE, K_FADE_REVERSE,
    K_SETVOL_REJECTED, K_FREQ_REJECTED, K_STREAM_STALL, K_LONG_FRAME, K_NUM
};
constexpr const char* KIND_NAME[K_NUM] = { "STALE_SLOT", "MISSED_SLOT", "WRITE_IN_PLAY", "LATE_REFILL", "SEAM_JUMP", "CLICK", "LOOP_WRAP", "FADE_RANGE",
                                           "FADE_REVERSE", "SETVOL_REJECTED", "FREQ_REJECTED", "STREAM_STALL", "LONG_FRAME" };

struct Frame2 { int16_t c[2]; };

struct Rec {
    unsigned id{};
    bool     stream{};
    DWORD    size{};
    WORD     ch{1};
    DWORD    freq{48000};            // current DSound frequency (last SetFrequency)
    // pending Lock
    DWORD    lockOff{}, lockBytes{}, lockFlags{}, lockPlay{}, lockWrite{};
    // cursor / play state
    bool     playing{}, looping{};
    int      playSlot{-1};
    uint64_t slotEnterT{};
    bool     slotFilled[2]{ true, true };
    bool     slotValid[2]{};
    Frame2   slotFirst[2]{}, slotLast[2]{};
    float    slotHeadRef[2]{}, slotTailRef[2]{};
    // static sound data (for the loop wrap check)
    Frame2   first{}, last{};
    float    headRef{}, tailRef{};
    bool     haveData{};
    // volume
    bool     haveVol{};
    LONG     lastVol{};
    uint64_t lastVolT{};
    DWORD    lastVolTid{};
    bool     haveFadeVol{};
    LONG     lastFadeVol{};
    uint32_t lastFadeStart{};
    // frequency
    uint64_t lastFreqT{};
    unsigned nLock{}, nVol{}, nFreq{};
};

struct State {
    std::mutex                       M;
    std::unordered_map<void*, Rec>   Bufs;
    std::vector<void**>              Tables;
    void*                            OrigVtbl{};
    void**                           Patched{};
    void*                            Orig[NUM_SLOTS]{};
    FILE*                            F{};
    bool                             On{}, Raw{};
    LARGE_INTEGER                    Freq{}, T0{};
    unsigned                         NextId{};
    unsigned                         Count[K_NUM]{};
    unsigned                         CountNearLongFrame[K_NUM]{};  // anomalies within [longframe start, longframe end + 300 ms]
    // frames
    uint64_t                         LastFrameT{};
    uint64_t                         LastLongEndT{};
    double                           LastLongDtMs{};
    uint64_t                         NearCoverageUs{};     // union of the [start, end + 300 ms] windows of long frames (approx., windows are merged on the fly)
    uint64_t                         NearWindowEnd{};
    unsigned                         Frames{};
    unsigned                         FrameHist[7]{};      // <17 <25 <34 <50 <100 <250 >=250 ms
    double                           MaxFrameMs{};
    // stream thread
    uint64_t                         LastStreamIterT{};
    unsigned                         StreamIters{};
    double                           MaxStreamWorkMs{}, MaxStreamGapMs{};
    // stats
    unsigned                         StreamChunks{}, StaticLocks{}, StreamRefills{};
    double                           MinLeadMs{1e9}, SumLeadMs{};
    double                           MaxSeamRatio{};
    unsigned                         SetVolMain{}, SetVolFade{}, StepsGt10dBMain{}, FreqCalls{}, FreqStepsGt50pc{};
    unsigned                         PlaySoundBig{}, PlaySoundAll{}, StaticRewriteUnderCursor{}, StaticClickSounds{};
    unsigned                         FadeStepsOld{}, FadeStepsOldBad{};
    unsigned                         FadeEv[12]{};
    uint64_t                         LastSummaryT{};
    DWORD                            FadeTid{};
};
inline State& S() { static State s; return s; }

inline uint64_t NowUs() {
    auto& s = S();
    LARGE_INTEGER n;
    QueryPerformanceCounter(&n);
    return (uint64_t)((n.QuadPart - s.T0.QuadPart) * 1000000ull / (uint64_t)s.Freq.QuadPart);
}

inline void Vlog(const char* fmt, va_list ap) {
    auto& s = S();
    char buf[640];
    const uint64_t t = NowUs();
    int n = snprintf(buf, sizeof(buf), "%10.3f t%04lx ", (double)t / 1000.0, (unsigned long)GetCurrentThreadId());
    n += vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
    if (n > (int)sizeof(buf) - 2) { n = sizeof(buf) - 2; }
    buf[n++] = '\n';
    fwrite(buf, 1, n, s.F);
}
// caller holds s.M
inline void Log(const char* fmt, ...) { va_list ap; va_start(ap, fmt); Vlog(fmt, ap); va_end(ap); }
inline void Raw(const char* fmt, ...) { if (!S().Raw) return; va_list ap; va_start(ap, fmt); Vlog(fmt, ap); va_end(ap); }

// caller holds s.M. Records an anomaly (or an info event for K_LONG_FRAME) with the long-frame correlation.
inline void Anom(Kind k, unsigned bufId, const char* fmt, ...) {
    auto& s = S();
    ++s.Count[k];
    const uint64_t now = NowUs();
    const double sinceLong = s.LastLongEndT ? (double)(now - s.LastLongEndT) / 1000.0 : -1.0;
    const bool nearLf = s.LastLongEndT && sinceLong >= 0.0 && sinceLong <= 300.0;
    if (nearLf) { ++s.CountNearLongFrame[k]; }
    char msg[480];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);
    Log("!%s buf=%u %s | lf=%.1fms lfdt=%.1fms%s", KIND_NAME[k], bufId, msg, sinceLong, s.LastLongDtMs, nearLf ? " NEAR" : "");
}

inline bool InSpan(DWORD x, DWORD a, DWORD len, DWORD size) { return size && len && ((x + size - a) % size) < len; }

struct Analysis { uint64_t hash{}; Frame2 first{}, last{}; unsigned clicks{}, firstClick[3]{}; unsigned maxJump{}, maxJumpAt{}; float headRef{}, tailRef{}; unsigned zeros{}, frames{}; };

inline unsigned AbsDelta(const Frame2& a, const Frame2& b, int ch) {
    unsigned m = 0;
    for (int c = 0; c < ch; c++) { const int d = std::abs((int)a.c[c] - (int)b.c[c]); if ((unsigned)d > m) m = (unsigned)d; }
    return m;
}

// data: interleaved int16, `ch` channels. Pure function (no locks).
inline Analysis Analyse(const void* data, DWORD bytes, int ch) {
    Analysis r;
    const int16_t* p = (const int16_t*)data;
    const size_t frames = bytes / (2 * ch);
    r.frames = (unsigned)frames;
    uint64_t h = 1469598103934665603ull;
    const uint8_t* b = (const uint8_t*)data;
    for (DWORD i = 0; i < bytes; i++) { h = (h ^ b[i]) * 1099511628211ull; }
    r.hash = h;
    if (!frames) return r;
    auto fr = [&](size_t i) { Frame2 f{}; for (int c = 0; c < ch; c++) f.c[c] = p[i * ch + c]; if (ch == 1) f.c[1] = f.c[0]; return f; };
    r.first = fr(0); r.last = fr(frames - 1);
    float ema = 0.f; double headSum = 0; unsigned headN = 0;
    Frame2 prev = r.first;
    for (size_t i = 1; i < frames; i++) {
        const Frame2 cur = fr(i);
        const unsigned d = AbsDelta(prev, cur, ch);
        if (d > r.maxJump) { r.maxJump = d; r.maxJumpAt = (unsigned)i; }
        if (i > 64 && d > 8000 && (float)d > 12.f * ema) { // ema warmed up
            if (r.clicks < 3) r.firstClick[r.clicks] = (unsigned)i;
            ++r.clicks;
        }
        ema += ((float)d - ema) * (1.f / 32.f);
        if (i <= 2048) { headSum += d; ++headN; }
        prev = cur;
    }
    for (size_t i = 0; i < frames; i++) { if (p[i * ch] == 0) ++r.zeros; }
    r.headRef = headN ? (float)(headSum / headN) : 0.f;
    // tail ref: mean |delta| over the last 2048 frames
    double tailSum = 0; unsigned tailN = 0;
    for (size_t i = frames > 2048 ? frames - 2048 : 1; i < frames; i++) { tailSum += AbsDelta(fr(i - 1), fr(i), ch); ++tailN; }
    r.tailRef = tailN ? (float)(tailSum / tailN) : 0.f;
    return r;
}

inline Rec* Find(void* self) { auto& s = S(); auto it = s.Bufs.find(self); return it == s.Bufs.end() ? nullptr : &it->second; }

inline void SummaryLocked() {
    auto& s = S();
    const double upS = (double)NowUs() / 1e6;
    Log("[tap-summary] up=%.1fs buffers=%zu streamRefills=%u staticLocks=%u setVol(main/fade)=%u/%u mainSteps>10dB=%u setFreq=%u freqSteps>50%%in100ms=%u playSound(all/>64KiB)=%u/%u oldFadeFormula(steps/bad)=%u/%u staticRewriteUnderCursor(exe-inherent)=%u sfxWithTransients(info)=%u",
        upS, s.Bufs.size(), s.StreamRefills, s.StaticLocks, s.SetVolMain, s.SetVolFade, s.StepsGt10dBMain, s.FreqCalls, s.FreqStepsGt50pc, s.PlaySoundAll, s.PlaySoundBig, s.FadeStepsOld, s.FadeStepsOldBad, s.StaticRewriteUnderCursor, s.StaticClickSounds);
    Log("[tap-summary] fade thread: RequestFade calls=%u ok(new/reuse)=%u/%u false(disabled/not-playing/nofree)=%u/%u/%u same-volume-ok=%u | Service: waiting=%u ramp-steps=%u ends=%u cancel-releases=%u",
        s.FadeEv[0], s.FadeEv[1], s.FadeEv[2], s.FadeEv[4], s.FadeEv[3], s.FadeEv[6], s.FadeEv[5], s.FadeEv[10], s.FadeEv[7], s.FadeEv[8], s.FadeEv[9]);
    Log("[tap-summary] frames=%u hist(<17,<25,<34,<50,<100,<250,>=250ms)=%u/%u/%u/%u/%u/%u/%u maxFrame=%.1fms streamIters=%u maxStreamWork=%.1fms maxStreamGap=%.1fms refillLead min/avg=%.0f/%.0f ms maxSeamRatio=%.1f",
        s.Frames, s.FrameHist[0], s.FrameHist[1], s.FrameHist[2], s.FrameHist[3], s.FrameHist[4], s.FrameHist[5], s.FrameHist[6], s.MaxFrameMs, s.StreamIters, s.MaxStreamWorkMs, s.MaxStreamGapMs,
        s.StreamRefills ? s.MinLeadMs : 0.0, s.StreamRefills ? s.SumLeadMs / s.StreamRefills : 0.0, s.MaxSeamRatio);
    const double covFrac = upS > 0 ? (double)s.NearCoverageUs / 1e6 / upS : 0.0;
    for (unsigned k = 0; k < K_NUM; k++) {
        if (k == K_LONG_FRAME) { Log("[tap-summary] %-16s %u (windows [start,end+300ms] cover %.1f%% of the run)", KIND_NAME[k], s.Count[k], covFrac * 100.0); continue; }
        Log("[tap-summary] %-16s %u (within 300ms after a long frame: %u)", KIND_NAME[k], s.Count[k], s.CountNearLongFrame[k]);
    }
    fflush(s.F);
}

inline void MaybeSummaryImpl() {
    auto& s = S();
    const uint64_t now = NowUs();
    if (now - s.LastSummaryT > 30000000ull) { s.LastSummaryT = now; SummaryLocked(); }
}
// ---------------------------------------------------------------------------------------------------------------------------- wrappers
inline ULONG STDMETHODCALLTYPE W_Release(IDirectSoundBuffer* self) {
    auto& s = S();
    const auto r = ((ReleaseFn)s.Orig[SLOT_RELEASE])(self);
    if (r == 0) {
        std::lock_guard g{ s.M };
        if (auto* rec = Find(self)) { Raw("buf=%u RELEASE (destroyed)", rec->id); s.Bufs.erase(self); }
    }
    return r;
}

inline HRESULT STDMETHODCALLTYPE W_GetCurrentPosition(IDirectSoundBuffer* self, LPDWORD play, LPDWORD write) {
    auto& s = S();
    DWORD pl = 0, wr = 0;
    const auto hr = ((GetCurPosFn)s.Orig[SLOT_GETCURPOS])(self, &pl, &wr);
    if (play) *play = pl;
    if (write) *write = wr;
    if (SUCCEEDED(hr)) {
        std::lock_guard g{ s.M };
        if (auto* r = Find(self); r && r->stream && r->playing) {
            const uint64_t now = NowUs();
            const int slot = (int)(pl / STREAM_SLOT) & 1;
            if (r->playSlot != slot) {
                if (r->playSlot >= 0 && !r->slotFilled[slot]) {
                    Anom(K_STALE_SLOT, r->id, "slot=%d play=%u (half was not refilled since it last played; left it %.0f ms ago)", slot, pl, (double)(now - r->slotEnterT) / 1000.0);
                }
                r->slotFilled[slot] = false; // consumed, must be refilled before the cursor comes back
                Raw("buf=%u CURSOR enters slot %d play=%u write=%u", r->id, slot, pl, wr);
                r->playSlot = slot; r->slotEnterT = now;
            } else if (r->slotEnterT && now - r->slotEnterT > 2100000ull) {
                // a half lasts size/bytesPerSec = 0xC0000/2 / (freq*4); at 48 kHz = 2.0 s. Only meaningful at the nominal rate.
                const double halfMs = 1000.0 * STREAM_SLOT / (4.0 * (r->freq ? r->freq : 48000));
                if ((double)(now - r->slotEnterT) / 1000.0 > halfMs * 1.05 + 100.0) {
                    Anom(K_MISSED_SLOT, r->id, "slot=%d play=%u stayed %.0f ms in one half (expected %.0f ms)", slot, pl, (double)(now - r->slotEnterT) / 1000.0, halfMs);
                    r->slotEnterT = now;
                }
            }
        }
        MaybeSummaryImpl();
    }
    return hr;
}

inline HRESULT STDMETHODCALLTYPE W_Lock(IDirectSoundBuffer* self, DWORD off, DWORD bytes, LPVOID* p1, LPDWORD b1, LPVOID* p2, LPDWORD b2, DWORD flags) {
    auto& s = S();
    DWORD pl = 0, wr = 0;
    ((GetCurPosFn)s.Orig[SLOT_GETCURPOS])(self, &pl, &wr);
    const auto hr = ((LockFn)s.Orig[SLOT_LOCK])(self, off, bytes, p1, b1, p2, b2, flags);
    std::lock_guard g{ s.M };
    if (auto* r = Find(self)) {
        r->lockOff = (flags & DSBLOCK_ENTIREBUFFER) ? 0 : off; r->lockBytes = (flags & DSBLOCK_ENTIREBUFFER) ? r->size : bytes; r->lockFlags = flags;
        r->lockPlay = pl; r->lockWrite = wr; ++r->nLock;
        Raw("buf=%u LOCK off=%u bytes=%u flags=%x play=%u write=%u hr=%08x", r->id, off, bytes, flags, pl, wr, (unsigned)hr);
        if (FAILED(hr)) { Log("!LOCK_FAIL buf=%u hr=%08x off=%u bytes=%u flags=%x", r->id, (unsigned)hr, off, bytes, flags); }
    }
    return hr;
}

inline void CheckSeam(Rec& r, int dstSlot, const Analysis& a) {
    auto& s = S();
    const int prev = dstSlot ^ 1;
    if (!r.slotValid[prev]) return;
    const unsigned jump = AbsDelta(r.slotLast[prev], a.first, 2);
    const float ref = std::fmax(a.headRef, r.slotTailRef[prev]);
    const double ratio = ref > 1.f ? jump / ref : jump;
    if (ratio > s.MaxSeamRatio && jump > 500) { s.MaxSeamRatio = ratio; }
    if ((float)jump > std::fmax(10.f * ref, 4000.f)) {
        Anom(K_SEAM_JUMP, r.id, "slot %d<-%d jump=%u (prev last L=%d R=%d, new first L=%d R=%d) localMean=%.0f", dstSlot, prev, jump, r.slotLast[prev].c[0], r.slotLast[prev].c[1], a.first.c[0], a.first.c[1], ref);
    }
}

inline HRESULT STDMETHODCALLTYPE W_Unlock(IDirectSoundBuffer* self, LPVOID p1, DWORD b1, LPVOID p2, DWORD b2) {
    auto& s = S();
    // copy what we need before the data becomes visible to the mixer
    Rec snap; bool have = false;
    { std::lock_guard g{ s.M }; if (auto* r = Find(self)) { snap = *r; have = true; } }
    Analysis a1{}, a2{}, h0{}, h1{};
    if (have && p1 && b1) {
        if (snap.stream && b1 == STREAM_BYTES && snap.lockOff == 0) { // ENTIREBUFFER fill (CAEStreamingChannel::PrepareStream): analyse both halves
            h0 = Analyse(p1, STREAM_SLOT, 2); h1 = Analyse((const uint8_t*)p1 + STREAM_SLOT, STREAM_SLOT, 2);
        } else {
            a1 = Analyse(p1, b1, snap.ch);
        }
        if (p2 && b2) a2 = Analyse(p2, b2, snap.ch);
    }
    const auto hr = ((UnlockFn)s.Orig[SLOT_UNLOCK])(self, p1, b1, p2, b2);
    DWORD pl = 0, wr = 0;
    ((GetCurPosFn)s.Orig[SLOT_GETCURPOS])(self, &pl, &wr);
    if (!have) return hr;
    std::lock_guard g{ s.M };
    auto* r = Find(self);
    if (!r) return hr;
    const DWORD total = b1 + b2;
    // ---- write vs cursors
    const bool inPlay = r->playing && (InSpan(r->lockPlay, r->lockOff, total, r->size) || InSpan(pl, r->lockOff, total, r->size));
    const DWORD span1 = r->size ? (r->lockWrite + r->size - r->lockPlay) % r->size : 0;
    const bool inSpan = r->playing && (InSpan(r->lockOff, r->lockPlay, span1, r->size) || (span1 && InSpan(r->lockOff + total - 1, r->lockPlay, span1, r->size)));
    if (r->stream) {
        if (b1 == STREAM_BYTES && r->lockOff == 0) { // whole-buffer fill
            for (int k = 0; k < 2; k++) {
                const Analysis& a = k ? h1 : h0;
                r->slotValid[k] = true; r->slotFilled[k] = true; r->slotFirst[k] = a.first; r->slotLast[k] = a.last; r->slotHeadRef[k] = a.headRef; r->slotTailRef[k] = a.tailRef;
                if (a.clicks) Anom(K_CLICK, r->id, "stream full-fill half %d: %u clicks, first at frames %u/%u/%u, maxJump=%u", k, a.clicks, a.firstClick[0], a.firstClick[1], a.firstClick[2], a.maxJump);
            }
            r->playSlot = -1;
            Raw("buf=%u UNLOCK full-fill play=%u write=%u hashes %016llx %016llx first L=%d R=%d", r->id, pl, wr, (unsigned long long)h0.hash, (unsigned long long)h1.hash, h0.first.c[0], h0.first.c[1]);
        } else if (b1 == STREAM_SLOT && r->lockOff % STREAM_SLOT == 0) {
            const int slot = (int)(r->lockOff / STREAM_SLOT) & 1;
            ++s.StreamRefills;
            const double bytesPerMs = (r->freq ? r->freq : 48000) * 4.0 / 1000.0;
            const DWORD lead = (r->lockOff + r->size - pl) % r->size;   // bytes until the cursor reaches the start of the refilled half
            const double leadMs = lead / bytesPerMs;
            if (leadMs < s.MinLeadMs) s.MinLeadMs = leadMs;
            s.SumLeadMs += leadMs;
            if (inPlay) {
                Anom(K_WRITE_IN_PLAY, r->id, "slot=%d off=%u len=%u playAtLock=%u writeAtLock=%u playAtUnlock=%u writeAtUnlock=%u", slot, r->lockOff, total, r->lockPlay, r->lockWrite, pl, wr);
            } else if (leadMs < 150.0 && r->playing) {
                Anom(K_LATE_REFILL, r->id, "slot=%d lead=%.0f ms (play=%u, half starts at %u)", slot, leadMs, pl, r->lockOff);
            }
            CheckSeam(*r, slot, a1);
            if (a1.clicks) Anom(K_CLICK, r->id, "stream slot %d: %u clicks, first at frames %u/%u/%u, maxJump=%u at %u", slot, a1.clicks, a1.firstClick[0], a1.firstClick[1], a1.firstClick[2], a1.maxJump, a1.maxJumpAt);
            r->slotValid[slot] = true; r->slotFilled[slot] = true; r->slotFirst[slot] = a1.first; r->slotLast[slot] = a1.last; r->slotHeadRef[slot] = a1.headRef; r->slotTailRef[slot] = a1.tailRef;
            ++s.StreamChunks;
            Raw("buf=%u UNLOCK refill slot=%d off=%u len=%u playAtLock=%u writeAtLock=%u playAtUnlock=%u lead=%.0fms hash=%016llx first L=%d R=%d last L=%d R=%d zeros=%u/%u",
                r->id, slot, r->lockOff, total, r->lockPlay, r->lockWrite, pl, leadMs, (unsigned long long)a1.hash, a1.first.c[0], a1.first.c[1], a1.last.c[0], a1.last.c[1], a1.zeros, a1.frames);
        } else {
            Raw("buf=%u UNLOCK (stream, odd) off=%u len=%u", r->id, r->lockOff, total);
        }
    } else {
        ++s.StaticLocks;
        if (r->playing && inPlay) {
            // exe-inherent for looped SFX: CAEStaticChannel::Service rewrites the intro area once `field_74` ms of GAME time passed after Play
            Raw("buf=%u UNLOCK static rewrite under cursor off=%u len=%u play=%u (exe-inherent timing)", r->id, r->lockOff, total, pl);
            ++s.StaticRewriteUnderCursor;
        }
        if (r->lockOff == 0 && !r->playing) { // initial fill
            r->first = a1.first; r->last = a1.last; r->headRef = a1.headRef; r->tailRef = a1.tailRef; r->haveData = true;
            // transients inside SFX data (gunshots, impacts, ...) are part of the recordings: counted as info, not as an anomaly
            if (a1.clicks) { ++s.StaticClickSounds; Raw("buf=%u static sound has %u transients (maxJump %u at frame %u of %u)", r->id, a1.clicks, a1.maxJump, a1.maxJumpAt, a1.frames); }
        }
        Raw("buf=%u UNLOCK static off=%u len=%u play=%u hash=%016llx first=%d last=%d frames=%u maxJump=%u clicks=%u", r->id, r->lockOff, total, pl, (unsigned long long)a1.hash, a1.first.c[0], a1.last.c[0], a1.frames, a1.maxJump, a1.clicks);
    }
    return hr;
}

inline HRESULT STDMETHODCALLTYPE W_Play(IDirectSoundBuffer* self, DWORD r1, DWORD prio, DWORD flags) {
    auto& s = S();
    const auto hr = ((PlayFn)s.Orig[SLOT_PLAY])(self, r1, prio, flags);
    std::lock_guard g{ s.M };
    if (auto* r = Find(self)) {
        r->playing = true; r->looping = (flags & DSBPLAY_LOOPING) != 0;
        if (r->stream) { r->playSlot = -1; r->slotEnterT = 0; }
        else if (r->looping && r->haveData) {
            const unsigned jump = AbsDelta(r->last, r->first, 1);
            const float ref = std::fmax(r->headRef, r->tailRef);
            if ((float)jump > std::fmax(10.f * ref, 4000.f)) {
                Anom(K_LOOP_WRAP, r->id, "looped static buffer wraps last=%d -> first=%d (jump %u, local mean %.0f)", r->last.c[0], r->first.c[0], jump, ref);
            }
        }
        Raw("buf=%u PLAY flags=%x hr=%08x", r->id, flags, (unsigned)hr);
    }
    return hr;
}

inline HRESULT STDMETHODCALLTYPE W_Stop(IDirectSoundBuffer* self) {
    auto& s = S();
    const auto hr = ((StopFn)s.Orig[SLOT_STOP])(self);
    std::lock_guard g{ s.M };
    if (auto* r = Find(self)) { r->playing = false; r->playSlot = -1; Raw("buf=%u STOP", r->id); }
    return hr;
}

inline HRESULT STDMETHODCALLTYPE W_SetCurrentPosition(IDirectSoundBuffer* self, DWORD pos) {
    auto& s = S();
    const auto hr = ((SetPosFn)s.Orig[SLOT_SETCURPOS])(self, pos);
    std::lock_guard g{ s.M };
    if (auto* r = Find(self)) { r->playSlot = -1; r->slotEnterT = 0; Raw("buf=%u SETPOS %u", r->id, pos); }
    return hr;
}

inline HRESULT STDMETHODCALLTYPE W_SetVolume(IDirectSoundBuffer* self, LONG vol) {
    auto& s = S();
    const auto hr = ((SetLongFn)s.Orig[SLOT_SETVOLUME])(self, vol);
    const DWORD tid = GetCurrentThreadId();
    const uint64_t now = NowUs();
    std::lock_guard g{ s.M };
    if (auto* r = Find(self)) {
        ++r->nVol;
        const bool fadeThread = s.FadeTid && tid == s.FadeTid;
        if (FAILED(hr)) { Anom(K_SETVOL_REJECTED, r->id, "vol=%d hr=%08x thread=%s", (int)vol, (unsigned)hr, fadeThread ? "fade" : "other"); }
        if (fadeThread) {
            ++s.SetVolFade;
            // find the fade entry of this buffer
            const auto& fade = AESmoothFadeThread;
            for (const auto& e : fade.m_aEntries) {
                if (e.m_nStatus == eSmoothFadeEntryStatus::STATE_ACTIVE && e.m_pSoundBuffer == self) {
                    const double a = e.m_fStartVolume * 100.0, b = e.m_fTargetVolume * 100.0;
                    const double lo = std::fmin(a, b) - 150.0, hi = std::fmax(a, b) + 150.0;
                    if (!(vol >= lo && vol <= hi)) {
                        Anom(K_FADE_RANGE, r->id, "vol=%d outside [%.0f, %.0f] (start %.1f dB, target %.1f dB, fadeTime %u ms)", (int)vol, lo, hi, e.m_fStartVolume, e.m_fTargetVolume, (unsigned)e.m_wFadeTime);
                    }
                    const int dir = b > a ? 1 : -1;
                    if (r->haveFadeVol && r->lastFadeStart == e.m_nStartTime && ((double)(vol - r->lastFadeVol)) * dir < -300.0) {
                        Anom(K_FADE_REVERSE, r->id, "vol=%d after %d (fade %.1f -> %.1f dB)", (int)vol, (int)r->lastFadeVol, e.m_fStartVolume, e.m_fTargetVolume);
                    }
                    r->lastFadeStart = e.m_nStartTime; r->lastFadeVol = vol; r->haveFadeVol = true;
                    break;
                }
            }
        } else {
            ++s.SetVolMain;
            if (r->haveVol && std::abs((int)vol - (int)r->lastVol) > 1000) ++s.StepsGt10dBMain;
            r->haveFadeVol = false;
        }
        Raw("buf=%u SETVOL %d (%s) step=%d dt=%.1fms", r->id, (int)vol, fadeThread ? "fade" : "main", r->haveVol ? (int)vol - (int)r->lastVol : 0, r->haveVol ? (double)(now - r->lastVolT) / 1000.0 : 0.0);
        r->haveVol = true; r->lastVol = vol; r->lastVolT = now; r->lastVolTid = tid;
    }
    return hr;
}

inline HRESULT STDMETHODCALLTYPE W_SetFrequency(IDirectSoundBuffer* self, DWORD f) {
    auto& s = S();
    const auto hr = ((SetDwFn)s.Orig[SLOT_SETFREQ])(self, f);
    const uint64_t now = NowUs();
    std::lock_guard g{ s.M };
    if (auto* r = Find(self)) {
        ++r->nFreq; ++s.FreqCalls;
        if (FAILED(hr)) { Anom(K_FREQ_REJECTED, r->id, "freq=%u hr=%08x (previous %u)", f, (unsigned)hr, r->freq); }
        else {
            if (r->freq && (f > r->freq * 3 / 2 || f < r->freq * 2 / 3) && now - r->lastFreqT < 100000) ++s.FreqStepsGt50pc;
            r->freq = f ? f : r->freq;
        }
        r->lastFreqT = now;
        Raw("buf=%u SETFREQ %u hr=%08x", r->id, f, (unsigned)hr);
    }
    return hr;
}

inline HRESULT STDMETHODCALLTYPE W_SetPan(IDirectSoundBuffer* self, LONG pan) {
    auto& s = S();
    const auto hr = ((SetLongFn)s.Orig[SLOT_SETPAN])(self, pan);
    std::lock_guard g{ s.M };
    if (auto* r = Find(self)) { Raw("buf=%u SETPAN %d hr=%08x", r->id, (int)pan, (unsigned)hr); }
    return hr;
}

struct Closer { ~Closer() { auto& s = S(); if (s.On && s.F) { std::lock_guard g{ s.M }; SummaryLocked(); fclose(s.F); s.F = nullptr; s.On = false; } } };
} // namespace detail

inline bool Enabled() {
    static const bool v = [] {
        const char* e = std::getenv("NOTSA_AUDIO_TAP");
        if (!e || !*e) return false;
        auto& s = detail::S();
        s.F = std::fopen(e, "wb");
        if (!s.F) return false;
        static char iobuf[1 << 16];
        std::setvbuf(s.F, iobuf, _IOFBF, sizeof(iobuf));
        QueryPerformanceFrequency(&s.Freq);
        QueryPerformanceCounter(&s.T0);
        const char* raw = std::getenv("NOTSA_AUDIO_TAP_RAW");
        s.Raw = raw && *raw && *raw != '0';
        s.On = true;
        static detail::Closer closer;
        std::lock_guard g{ s.M };
        detail::Log("[tap] NOTSA_AUDIO_TAP on (raw=%d)", (int)s.Raw);
        return true;
    }();
    return v;
}

//! Attach the tap to a buffer. `fadeThreadId` = AESmoothFadeThread.m_dwThreadId (0 before it exists: re-read lazily by the caller via SetFadeThread).
inline void Attach(IDirectSoundBuffer* buffer) {
    using namespace detail;
    if (!buffer || !Enabled()) return;
    auto& s = S();
    void** vtbl = *reinterpret_cast<void***>(buffer);
    DSBCAPS caps{ sizeof(DSBCAPS) };
    WAVEFORMATEX wf{};
    const bool haveCaps = SUCCEEDED(buffer->GetCaps(&caps));
    DWORD wfSize = 0;
    const bool haveFmt = SUCCEEDED(buffer->GetFormat(&wf, sizeof(wf), &wfSize));
    std::lock_guard g{ s.M };
    if (s.OrigVtbl != vtbl) {
        if (s.OrigVtbl) { Log("[tap] WARNING: second distinct buffer vtable %p (first %p): chaining the new one", vtbl, s.OrigVtbl); }
        auto* copy = new void*[NUM_SLOTS];
        for (size_t i = 0; i < NUM_SLOTS; i++) { copy[i] = vtbl[i]; s.Orig[i] = vtbl[i]; }
        copy[SLOT_RELEASE] = (void*)&W_Release;
        copy[SLOT_GETCURPOS] = (void*)&W_GetCurrentPosition;
        copy[SLOT_LOCK] = (void*)&W_Lock;
        copy[SLOT_UNLOCK] = (void*)&W_Unlock;
        copy[SLOT_PLAY] = (void*)&W_Play;
        copy[SLOT_STOP] = (void*)&W_Stop;
        copy[SLOT_SETCURPOS] = (void*)&W_SetCurrentPosition;
        copy[SLOT_SETVOLUME] = (void*)&W_SetVolume;
        copy[SLOT_SETPAN] = (void*)&W_SetPan;
        copy[SLOT_SETFREQ] = (void*)&W_SetFrequency;
        s.Tables.push_back(copy);
        s.OrigVtbl = vtbl; s.Patched = copy;
    }
    *reinterpret_cast<void***>(buffer) = s.Patched;
    Rec r;
    r.id = ++s.NextId;
    r.size = haveCaps ? caps.dwBufferBytes : 0;
    r.ch = haveFmt && wf.nChannels ? wf.nChannels : 1;
    r.freq = haveFmt && wf.nSamplesPerSec ? wf.nSamplesPerSec : 48000;
    r.stream = r.size == STREAM_BYTES && r.ch == 2;
    s.Bufs[buffer] = r;
    s.FadeTid = AESmoothFadeThread.m_dwThreadId;
    Raw("buf=%u ATTACH %s size=%u ch=%u freq=%u flags=%x", r.id, r.stream ? "STREAM" : "static", r.size, (unsigned)r.ch, r.freq, haveCaps ? caps.dwFlags : 0);
}

//! Once per game frame (main thread): frame time.
inline void Frame() {
    using namespace detail;
    if (!Enabled()) return;
    auto& s = S();
    const uint64_t now = NowUs();
    std::lock_guard g{ s.M };
    if (!s.FadeTid) s.FadeTid = AESmoothFadeThread.m_dwThreadId;
    if (s.LastFrameT) {
        const double dt = (double)(now - s.LastFrameT) / 1000.0;
        ++s.Frames;
        s.FrameHist[dt < 17 ? 0 : dt < 25 ? 1 : dt < 34 ? 2 : dt < 50 ? 3 : dt < 100 ? 4 : dt < 250 ? 5 : 6]++;
        if (dt > s.MaxFrameMs) s.MaxFrameMs = dt;
        if (dt > 50.0) {
            ++s.Count[K_LONG_FRAME];
            s.LastLongEndT = now; s.LastLongDtMs = dt;
            // coverage of the window [start, end+300ms], merged with the previous window
            const uint64_t ws = now - (uint64_t)(dt * 1000.0), we = now + 300000ull;
            const uint64_t from = ws > s.NearWindowEnd ? ws : s.NearWindowEnd;
            if (we > from) s.NearCoverageUs += we - from;
            if (we > s.NearWindowEnd) s.NearWindowEnd = we;
            Log("F frame=%u dt=%.1fms", CTimer::GetFrameCounter(), dt);
        }
    }
    s.LastFrameT = now;
    MaybeSummaryImpl();
}

//! Stream thread: one call per MainLoop iteration, `workMs` = time spent in CAEStreamThread::Service + CAEStreamingChannel::Service.
inline void StreamIter(double workMs) {
    using namespace detail;
    if (!Enabled()) return;
    auto& s = S();
    const uint64_t now = NowUs();
    std::lock_guard g{ s.M };
    ++s.StreamIters;
    if (workMs > s.MaxStreamWorkMs) s.MaxStreamWorkMs = workMs;
    double gap = 0;
    if (s.LastStreamIterT) { gap = (double)(now - s.LastStreamIterT) / 1000.0; if (gap > s.MaxStreamGapMs) s.MaxStreamGapMs = gap; }
    if (workMs > 300.0 || gap > 400.0) { Anom(K_STREAM_STALL, 0, "work=%.1fms gap=%.1fms (normal period is ~105 ms)", workMs, gap); }
    s.LastStreamIterT = now;
}

//! CAESmoothFadeThread counters: 0 RequestFade called, 1 ok new entry, 2 ok reused entry, 3 false: not playing, 4 false: disabled/uninitialised, 5 true: already at the volume, 6 false: no free entry,
//! 7 Service ramp step, 8 Service fade end, 9 Service released a cancelled entry, 10 Service skipped an entry that has not started yet.
inline void FadeEvent(unsigned k) {
    using namespace detail;
    if (!Enabled() || k >= 12) return;
    auto& s = S();
    std::lock_guard g{ s.M };
    ++s.FadeEv[k];
}

//! PlaySound: bytes of the sound handed to CAEStaticChannel::SetAudioBuffer (counts the sounds that do not fit 16 bits).
inline void NotePlaySound(uint32_t bufferBytes) {
    using namespace detail;
    if (!Enabled()) return;
    auto& s = S();
    std::lock_guard g{ s.M };
    ++s.PlaySoundAll;
    if (bufferBytes > 0xFFFF) ++s.PlaySoundBig;
}

//! CAESmoothFadeThread::Service: compares the new fade value with the formula the port used before the fix (log2(step*progress) without the 1 +).
inline void NoteFadeStep(double newVolDb, double oldVolDb) {
    using namespace detail;
    if (!Enabled()) return;
    auto& s = S();
    std::lock_guard g{ s.M };
    ++s.FadeStepsOld;
    if (!(std::fabs(newVolDb - oldVolDb) < 1.0)) ++s.FadeStepsOldBad;
}

#define NOTSA_AUDIO_TAP_ATTACH(buf) ::notsa::audio_tap::Attach(buf)
#define NOTSA_AUDIO_TAP_FRAME() ::notsa::audio_tap::Frame()
} // namespace notsa::audio_tap
#else
#define NOTSA_AUDIO_TAP_ATTACH(buf) ((void)0)
#define NOTSA_AUDIO_TAP_FRAME() ((void)0)
#endif
