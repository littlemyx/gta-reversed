// MAC-W2: see app_present_mac.h. Portable-only TU; no game headers.
#include "app_present_mac.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

#ifdef __APPLE__
// The CoreGraphics / CoreVideo headers cannot be included here: the build force-includes the game's precompiled header and MacTypes.h (pulled in by both) redefines
// the game's FixedRect. The C ABI used is tiny and stable (CoreVideo/CVDisplayLink.h, CoreGraphics/CGDirectDisplay.h), so declare it; the frameworks are linked
// already (SDL3 Cocoa: CoreVideo, Cocoa -> CoreGraphics).
extern "C" {
typedef struct __CVDisplayLink* CVDisplayLinkRef;
typedef int32_t  CVReturn;
typedef uint64_t CVOptionFlags;
typedef CVReturn (*CVDisplayLinkOutputCallback)(CVDisplayLinkRef, const void* now, const void* outputTime, CVOptionFlags flagsIn, CVOptionFlags* flagsOut, void* ctx);
CVReturn CVDisplayLinkCreateWithActiveCGDisplays(CVDisplayLinkRef* out);
CVReturn CVDisplayLinkSetOutputCallback(CVDisplayLinkRef, CVDisplayLinkOutputCallback, void* ctx);
CVReturn CVDisplayLinkStart(CVDisplayLinkRef);
CVReturn CVDisplayLinkSetCurrentCGDisplay(CVDisplayLinkRef, uint32_t display);
uint32_t CGMainDisplayID(void);
int      CGDisplayIsAsleep(uint32_t display);
}
using CGDirectDisplayID = uint32_t;
constexpr CVReturn kCVReturnSuccess = 0;
#endif

namespace notsa::portable::mac {
int64_t SteadyNowNs() { return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

namespace {
struct Source {
    std::mutex              m;
    std::condition_variable cv;
    uint64_t                ticks = 0;
    std::atomic<int64_t>    lastTickNs{ 0 };
    std::atomic<uint64_t>   ticksAtomic{ 0 };
#ifdef __APPLE__
    CVDisplayLinkRef  link    = nullptr;
    CGDirectDisplayID display = 0;
#endif
};
Source& Src() { static Source* s = new Source; return *s; } // never destroyed: the display link thread may still fire while the process exits

#ifdef __APPLE__
CVReturn OnTick(CVDisplayLinkRef, const void*, const void*, CVOptionFlags, CVOptionFlags*, void* ctx) {
    auto* s = static_cast<Source*>(ctx);
    {
        const std::lock_guard lock{ s->m };
        s->ticks++;
        s->ticksAtomic.store(s->ticks);
        s->lastTickNs.store(SteadyNowNs());
    }
    s->cv.notify_all();
    return kCVReturnSuccess;
}
#endif
} // namespace

bool LinkEnsure(uint32_t displayId) {
#ifdef __APPLE__
    auto& s = Src();
    const CGDirectDisplayID display = displayId ? (CGDirectDisplayID)displayId : CGMainDisplayID();
    if (!s.link) {
        if (CVDisplayLinkCreateWithActiveCGDisplays(&s.link) != kCVReturnSuccess) {
            s.link = nullptr;
            return false;
        }
        CVDisplayLinkSetOutputCallback(s.link, &OnTick, &s);
        CVDisplayLinkStart(s.link);
    }
    if (s.display != display) {
        CVDisplayLinkSetCurrentCGDisplay(s.link, display);
        s.display = display;
    }
    return true;
#else
    (void)displayId;
    return false;
#endif
}

bool LinkExists() {
#ifdef __APPLE__
    return Src().link != nullptr;
#else
    return false;
#endif
}

uint64_t LinkTicks() { return Src().ticksAtomic.load(); }
int64_t  LinkLastTickNs() { return Src().lastTickNs.load(); }

bool LinkWaitTick(uint64_t seenTicks, int64_t timeoutNs) {
    auto& s = Src();
    std::unique_lock lock{ s.m };
    return s.cv.wait_for(lock, std::chrono::nanoseconds(timeoutNs), [&] { return s.ticks != seenTicks; });
}

bool DisplayAsleep(uint32_t displayId) {
#ifdef __APPLE__
    return CGDisplayIsAsleep(displayId ? (CGDirectDisplayID)displayId : CGMainDisplayID()) != 0;
#else
    (void)displayId;
    return false;
#endif
}
} // namespace notsa::portable::mac
