// MAC-W2: the macOS-only part of the present pacing (CVDisplayLink + CGDisplay). Separate TU WITHOUT the game headers: CoreGraphics' MacTypes.h collides with the
// game's types (FixedRect ...). Every function is callable on other platforms too (no link, awake display).
#pragma once
#include <cstdint>

namespace notsa::portable::mac {
//! Creates (once) / retargets the display link for `displayId` (SDL's Cocoa display id is a CGDirectDisplayID; 0 = main display). false = no link available.
bool LinkEnsure(uint32_t displayId);
bool LinkExists();
uint64_t LinkTicks();
int64_t  LinkLastTickNs(); // steady_clock ns of the latest tick, 0 = none yet
//! Waits for a tick after `seenTicks`, at most `timeoutNs`. true = a tick arrived.
bool LinkWaitTick(uint64_t seenTicks, int64_t timeoutNs);
bool DisplayAsleep(uint32_t displayId);
int64_t SteadyNowNs();
}
