#pragma once
/*
 * MAC-W2 (W-1 / W-2 / W-8): window, HiDPI, mouse mode and present pacing of the native build. Implementation: app_window.cpp.
 * Used only from `#ifdef NOTSA_PORTABLE` blocks (WinMain.cpp, WinPs.cpp, VideoMode.cpp, SDLWrapper.cpp).
 */
#ifdef NOTSA_PORTABLE
#include <cstdint>
#include <string>

#include <SDL3/SDL_video.h>

namespace notsa::portable {
// ---- HiDPI ----
//! NOTSA_STANDALONE_HIDPI=0 keeps the old 1x backing store; anything else (default) creates the window high-pixel-density: the drawable is in pixels and the game
//! resolution (video mode, camera raster, mouse coordinates) is in drawable pixels.
bool HiDpiEnabled();
//! SDL_WINDOW_HIGH_PIXEL_DENSITY or 0
SDL_WindowFlags HiDpiWindowFlag();
//! Pixels per point of the game window (1.0 without HiDPI / without a window)
float PixelDensity();
//! Window size in points whose drawable is `pixels` wide/high
int PointsFromPixels(int pixels);
//! Right after SDL_CreateWindow: hides the OS cursor over the window (the game draws its own) and, when NOTSA_STANDALONE_WINDOWED is unset, defaults the windowed mode to 1280x720
//! POINTS (= 1280*density x 720*density pixels)
void OnWindowCreated(SDL_Window* window);

// ---- mouse ----
//! Relative mode (raw deltas, camera) while the game runs and the window has the focus; absolute pointer (menu cursor) while the front-end menu is open
void UpdateMouseMode();
//! window-point -> game-pixel conversion of an SDL mouse position
float MouseToGame(float v);
//! game pixels -> window points (for SDL_WarpMouseInWindow and the injector)
float MouseFromGame(float v);

// ---- present pacing ----
//! Called by psCameraShowRaster with the flags the game wants: waits (display link tick with a deadline, or a timed limiter) and returns the flags to hand to the
//! swap: WAITVSYNC is always removed (SDL's own wait is unbounded). NOTSA_STANDALONE_NOVSYNC=1: no pacing at all.
uint32_t PresentPace(uint32_t rwFlags);

//! Injector commands: cmd:presentstat | windowstat | fakeocclude:0|1 (forces the "window not visible" input of the pacer, for the hang proof) ; false = not mine
bool WindowCmd(const char* name, const std::string& args);
}
#endif
