// MAC-W2 (vsync): the pure decision part of the present pacing (header only, std only; tests/portable/input_test.cpp compiles it).
//
// Problem: SDL3's Cocoa swap with swap interval 1 waits on a condition that only a CVDisplayLink callback signals, UNBOUNDED (SDL_cocoaopengl.m
// Cocoa_GL_SwapWindow). When the display sleeps or the window is not on screen the callback never comes and the game thread hangs forever (MAC-BOOT1 r19-r21).
// Fix: the game never asks SDL to wait. When the game wants vsync (frame limiter option, loading screens) the portable layer waits for ITS OWN display link tick with
// a deadline; when nothing can tick (window hidden / minimised / occluded, display asleep, link silent) it paces with a timed limiter at the display's refresh period
// instead (the same frame time the vsync wait would give), and vsync resumes by itself as soon as ticks arrive again.
#pragma once

#include <algorithm>
#include <cstdint>

namespace notsa::portable {

enum class PaceAction : uint8_t {
    None,      // the game did not ask for vsync: present at once (the game's software limiter and the menu's "frame limiter off" semantics are untouched)
    WaitTick,  // wait for the next display link tick, but not beyond `untilNs`
    SleepUntil // timed limiter: sleep until `untilNs` (monotonic ns; already past = no sleep)
};

enum class PaceReason : uint8_t { Vsync, NotRequested, WindowNotVisible, DisplayAsleep, LinkSilent, NoLink };

struct PaceInput {
    bool    vsyncRequested = false;
    int64_t nowNs          = 0;
    int64_t lastPresentNs  = 0; // monotonic time of the previous present (0 = none yet)
    int64_t periodNs       = 16'666'667;
    int64_t lastTickNs     = 0; // monotonic time of the latest display link tick (0 = none seen / no link)
    bool    linkExists     = false;
    bool    windowVisible  = true;
    bool    displayAwake   = true;
};

struct PaceDecision {
    PaceAction action  = PaceAction::None;
    int64_t    untilNs = 0;
    PaceReason reason  = PaceReason::NotRequested;
};

// A tick is "recent" when it is not older than 4 periods (>= 100 ms): a healthy link ticks every period.
inline int64_t TickFreshnessNs(int64_t periodNs) { return std::max<int64_t>(4 * periodNs, 100'000'000); }
// The wait for a tick never lasts longer than 3 periods (>= 50 ms): a link that dies between the check and the wait costs one such frame, never a hang.
inline int64_t TickTimeoutNs(int64_t periodNs) { return std::max<int64_t>(3 * periodNs, 50'000'000); }

inline PaceDecision DecidePacing(const PaceInput& in) {
    PaceDecision d;
    if (!in.vsyncRequested) {
        return d; // None / NotRequested
    }
    const int64_t period = std::max<int64_t>(in.periodNs, 1'000'000);
    // Timed limiter by default: the frame ends one period after the previous present (a first frame, or one that took longer than a period, does not wait)
    d.action  = PaceAction::SleepUntil;
    d.untilNs = in.lastPresentNs ? in.lastPresentNs + period : in.nowNs;
    if (!in.windowVisible) {
        d.reason = PaceReason::WindowNotVisible;
    } else if (!in.displayAwake) {
        d.reason = PaceReason::DisplayAsleep;
    } else if (!in.linkExists) {
        d.reason = PaceReason::NoLink;
    } else if (in.lastTickNs == 0 || in.nowNs - in.lastTickNs > TickFreshnessNs(period)) {
        d.reason = PaceReason::LinkSilent;
    } else {
        d.action  = PaceAction::WaitTick;
        d.untilNs = in.nowNs + TickTimeoutNs(period);
        d.reason  = PaceReason::Vsync;
    }
    return d;
}

inline const char* PaceReasonName(PaceReason r) {
    switch (r) {
    case PaceReason::Vsync:            return "vsync";
    case PaceReason::NotRequested:     return "not requested";
    case PaceReason::WindowNotVisible: return "window hidden/minimised/occluded";
    case PaceReason::DisplayAsleep:    return "display asleep";
    case PaceReason::LinkSilent:       return "no display link tick";
    case PaceReason::NoLink:           return "no display link";
    }
    return "?";
}

} // namespace notsa::portable
