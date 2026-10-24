// MAC-W2: window / HiDPI / mouse mode / present pacing (see app_window.h). Portable-only TU.
#include "StdInc.h"

#include <SDL3/SDL.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

#include "app_present_mac.h"
#include "app_present_pacer.h"
#include "app_window.h"
#include "MenuManager.h"

namespace notsa::portable {
namespace {
int64_t NowNs() { return mac::SteadyNowNs(); }

bool EnvOff(const char* name) {
    const char* v = std::getenv(name);
    return v && (!std::strcmp(v, "0") || !strcasecmp(v, "off") || !strcasecmp(v, "no") || !strcasecmp(v, "false"));
}
bool EnvOn(const char* name) {
    const char* v = std::getenv(name);
    return v && *v && !EnvOff(name);
}
SDL_Window* Window() { return PSGLOBAL(sdlWindow); }
} // namespace

//--------------------------------------------------------------------------------------------------
// HiDPI
//--------------------------------------------------------------------------------------------------
bool HiDpiEnabled() {
    static const bool s_On = !EnvOff("NOTSA_STANDALONE_HIDPI");
    return s_On;
}

SDL_WindowFlags HiDpiWindowFlag() { return HiDpiEnabled() ? SDL_WINDOW_HIGH_PIXEL_DENSITY : 0; }

float PixelDensity() {
    const float d = Window() ? SDL_GetWindowPixelDensity(Window()) : 1.f;
    return d > 0.f ? d : 1.f;
}

int PointsFromPixels(int pixels) { return (int)std::lround((double)pixels / PixelDensity()); }

void OnWindowCreated(SDL_Window* window) {
    SDL_HideCursor(); // the game draws its own pointer in the menu (and hides the system one with WM_SETCURSOR on Windows)
    int pw = 0, ph = 0, w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &pw, &ph);
    SDL_GetWindowSize(window, &w, &h);
    const float d = SDL_GetWindowPixelDensity(window);
    NOTSA_LOG_INFO("[window] created: {}x{} points, {}x{} pixels, pixel density {:.2f} (HiDPI {})", w, h, pw, ph, d, HiDpiEnabled() ? "on" : "off (NOTSA_STANDALONE_HIDPI=0)");
    if (!std::getenv("NOTSA_STANDALONE_WINDOWED") && d > 1.01f) {
        // engine_gl3.cpp reads NOTSA_STANDALONE_WINDOWED (pixels, default 1280x720). On a 2x display that would be a 640x360 point window: keep the on-screen size and
        // render in native pixels instead.
        char buf[32];
        std::snprintf(buf, sizeof buf, "%dx%d", (int)std::lround(1280 * d), (int)std::lround(720 * d));
        setenv("NOTSA_STANDALONE_WINDOWED", buf, 0);
        NOTSA_LOG_INFO("[window] windowed mode defaults to {} pixels (1280x720 points)", buf);
    }
}

//--------------------------------------------------------------------------------------------------
// Mouse
//--------------------------------------------------------------------------------------------------
float MouseToGame(float v) { return v * PixelDensity(); }
float MouseFromGame(float v) { return v / PixelDensity(); }

void UpdateMouseMode() {
    SDL_Window* const w = Window();
    if (!w) {
        return;
    }
    // Absolute pointer in the front-end menu (the pointer's real position is the menu cursor), raw relative deltas otherwise. A window without the input focus is never
    // captured (a focus-less test run keeps the pointer free).
    const bool focus = (SDL_GetWindowFlags(w) & SDL_WINDOW_INPUT_FOCUS) != 0;
    const bool want  = focus && !FrontEndMenuManager.m_bMenuActive;
    const bool have  = SDL_GetWindowRelativeMouseMode(w);
    if (want == have) {
        return;
    }
    if (!want && FrontEndMenuManager.m_bMenuActive) {
        // entering the menu: put the system pointer where the menu cursor is, so that both agree
        SDL_SetWindowRelativeMouseMode(w, false);
        SDL_WarpMouseInWindow(w, MouseFromGame((float)FrontEndMenuManager.m_nMousePosWinX), MouseFromGame((float)FrontEndMenuManager.m_nMousePosWinY));
        return;
    }
    SDL_SetWindowRelativeMouseMode(w, want);
}

//--------------------------------------------------------------------------------------------------
// Present pacing
//--------------------------------------------------------------------------------------------------
namespace {
struct PaceStats {
    uint64_t vsyncFrames = 0, timedFrames = 0, tickTimeouts = 0, unpaced = 0;
    PaceReason lastReason = PaceReason::NotRequested;
    int64_t   lastPresentNs = 0, maxWaitNs = 0;
    bool      fakeOcclude = false;
    int64_t   periodNs = 0;
} s_Stats;

int64_t PeriodNs(SDL_Window* w) {
    float hz = 0.f;
    if (w) {
        if (const auto* m = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(w))) {
            hz = m->refresh_rate;
        }
    }
    if (!(hz >= 24.f && hz <= 500.f)) {
        hz = 60.f;
    }
    return (int64_t)(1e9 / hz);
}

uint32_t DisplayOf(SDL_Window* w) { return w ? (uint32_t)SDL_GetDisplayForWindow(w) : 0; } // SDL's Cocoa display id IS the CGDirectDisplayID
bool DisplayAwake(SDL_Window* w) { return !mac::DisplayAsleep(DisplayOf(w)); }
} // namespace

uint32_t PresentPace(uint32_t rwFlags) {
    constexpr uint32_t kWaitVsync = 1; // rwRASTERFLIPWAITVSYNC == rw::Raster::FLIPWAITVSYNCH
    static const bool s_NoVsync = EnvOn("NOTSA_STANDALONE_NOVSYNC");
    const bool requested        = (rwFlags & kWaitVsync) != 0;
    const uint32_t out          = rwFlags & ~kWaitVsync; // the swap itself never waits (SDL's wait has no timeout)
    if (!requested || s_NoVsync) {
        s_Stats.unpaced++;
        return out;
    }

    SDL_Window* const w = Window();
    mac::LinkEnsure(DisplayOf(w));

    PaceInput in;
    in.vsyncRequested = true;
    in.nowNs          = NowNs();
    in.lastPresentNs  = s_Stats.lastPresentNs;
    in.periodNs       = PeriodNs(w);
    in.lastTickNs     = mac::LinkLastTickNs();
    in.linkExists     = mac::LinkExists();
    in.windowVisible  = !s_Stats.fakeOcclude && (!w || !(SDL_GetWindowFlags(w) & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_OCCLUDED)));
    in.displayAwake   = DisplayAwake(w);
    s_Stats.periodNs  = in.periodNs;

    const auto d = DecidePacing(in);
    if (d.reason != s_Stats.lastReason) {
        if (d.reason == PaceReason::Vsync) {
            NOTSA_LOG_INFO("[present] vsync resumed ({:.1f} Hz display link)", 1e9 / (double)in.periodNs);
        } else {
            NOTSA_LOG_INFO("[present] vsync unavailable ({}): timed frame limiter at {:.1f} Hz", PaceReasonName(d.reason), 1e9 / (double)in.periodNs);
        }
        s_Stats.lastReason = d.reason;
    }

    const int64_t waitStart = NowNs();
    if (d.action == PaceAction::WaitTick) {
        const bool ticked = mac::LinkWaitTick(mac::LinkTicks(), d.untilNs - in.nowNs);
        if (ticked) {
            s_Stats.vsyncFrames++;
        } else {
            s_Stats.tickTimeouts++;
            s_Stats.timedFrames++;
        }
    } else if (d.action == PaceAction::SleepUntil) {
        const int64_t now = NowNs();
        if (d.untilNs > now) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(d.untilNs - now));
        }
        s_Stats.timedFrames++;
    }
    const int64_t end = NowNs();
    s_Stats.maxWaitNs = std::max(s_Stats.maxWaitNs, end - waitStart);
    s_Stats.lastPresentNs = end;
    return out;
}

bool WindowCmd(const char* name, const std::string& args) {
    if (!std::strcmp(name, "presentstat")) {
        NOTSA_LOG_INFO("[window-cmd] presentstat vsyncFrames={} timedFrames={} tickTimeouts={} unpaced={} maxWait={:.1f}ms reason=\"{}\" period={:.2f}ms ticks={} displayAwake={} relativeMouse={}",
            s_Stats.vsyncFrames, s_Stats.timedFrames, s_Stats.tickTimeouts, s_Stats.unpaced, (double)s_Stats.maxWaitNs / 1e6, PaceReasonName(s_Stats.lastReason), (double)s_Stats.periodNs / 1e6,
            (unsigned long long)mac::LinkTicks(), DisplayAwake(Window()) ? 1 : 0, Window() ? (int)SDL_GetWindowRelativeMouseMode(Window()) : -1);
        return true;
    }
    if (!std::strcmp(name, "windowstat")) {
        int w = 0, h = 0, pw = 0, ph = 0;
        if (Window()) {
            SDL_GetWindowSize(Window(), &w, &h);
            SDL_GetWindowSizeInPixels(Window(), &pw, &ph);
        }
        NOTSA_LOG_INFO("[window-cmd] windowstat points={}x{} pixels={}x{} density={:.2f} flags=0x{:x} game={}x{} camera={}x{}", w, h, pw, ph, PixelDensity(), Window() ? (unsigned long long)SDL_GetWindowFlags(Window()) : 0ull,
            RsGlobal.maximumWidth, RsGlobal.maximumHeight, SCREEN_WIDTH, SCREEN_HEIGHT);
        return true;
    }
    if (!std::strcmp(name, "fakeocclude")) {
        s_Stats.fakeOcclude = std::atoi(args.c_str()) != 0;
        NOTSA_LOG_INFO("[window-cmd] fakeocclude={}", (int)s_Stats.fakeOcclude);
        return true;
    }
    return false;
}
} // namespace notsa::portable
