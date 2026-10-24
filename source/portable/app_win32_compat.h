#pragma once
/*
 * MAC-W0a (stream W, app/ layer): the Win32 CONSTANTS / plain-data structs / few functions that source/app/** and extensions/** use on the SDL3
 * path and that have a straight portable meaning. Included from app/platform/win/winincl.h when !_WIN32 (never on Windows).
 *
 * Convention of the portable build: the HWND stored in PSGLOBAL(window) IS the SDL_Window* (psGlobalType::sdlWindow holds the same pointer). There is no
 * Win32 window; the functions below that take an HWND treat it that way. Implementations: source/portable/app_win32_compat.cpp.
 * Everything that has no honest equivalent is NOT here: it is compiled out in its file under NOTSA_PORTABLE (W-A items, .notes/reports/MAC-W0a.md).
 */
#ifndef _WIN32

#include "wintypes.h"

namespace notsa::portable {
//! Logs ONCE per `what` to stderr: a Win32 facility without a portable implementation was reached (the call then reports failure / "nothing")
void Unsupported(const char* what);
}

// ---- virtual key codes (winuser.h) ----
#define VK_BACK     0x08
#define VK_TAB      0x09
#define VK_RETURN   0x0D
#define VK_SHIFT    0x10
#define VK_CONTROL  0x11
#define VK_MENU     0x12
#define VK_PAUSE    0x13
#define VK_CAPITAL  0x14
#define VK_ESCAPE   0x1B
#define VK_SPACE    0x20
#define VK_PRIOR    0x21
#define VK_NEXT     0x22
#define VK_END      0x23
#define VK_HOME     0x24
#define VK_LEFT     0x25
#define VK_UP       0x26
#define VK_RIGHT    0x27
#define VK_DOWN     0x28
#define VK_INSERT   0x2D
#define VK_DELETE   0x2E
#define VK_LWIN     0x5B
#define VK_RWIN     0x5C
#define VK_APPS     0x5D
#define VK_NUMPAD0  0x60
#define VK_NUMPAD1  0x61
#define VK_NUMPAD2  0x62
#define VK_NUMPAD3  0x63
#define VK_NUMPAD4  0x64
#define VK_NUMPAD5  0x65
#define VK_NUMPAD6  0x66
#define VK_NUMPAD7  0x67
#define VK_NUMPAD8  0x68
#define VK_NUMPAD9  0x69
#define VK_MULTIPLY 0x6A
#define VK_ADD      0x6B
#define VK_SUBTRACT 0x6D
#define VK_DECIMAL  0x6E
#define VK_DIVIDE   0x6F
#define VK_F1       0x70
#define VK_F2       0x71
#define VK_F3       0x72
#define VK_F4       0x73
#define VK_F5       0x74
#define VK_F6       0x75
#define VK_F7       0x76
#define VK_F8       0x77
#define VK_F9       0x78
#define VK_F10      0x79
#define VK_F11      0x7A
#define VK_F12      0x7B
#define VK_NUMLOCK  0x90
#define VK_SCROLL   0x91
#define VK_LSHIFT   0xA0
#define VK_RSHIFT   0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU    0xA4
#define VK_RMENU    0xA5

// ---- misc constants ----
#define KF_EXTENDED       0x0100
#define MAPVK_VK_TO_CHAR  2
#define SW_SHOWNORMAL     1
#define SW_SHOWMINIMIZED  2
#define LANG_ENGLISH      0x09
#define LANG_GERMAN       0x07
#define LANG_SPANISH      0x0A
#define LANG_FRENCH       0x0C
#define LANG_ITALIAN      0x10
#define PRIMARYLANGID(lgid) ((WORD)(lgid) & 0x3ff)

// ---- plain data used by game_sa headers (D3D9 gamma ramp, DirectInput joystick state) ----
struct D3DGAMMARAMP { WORD red[256]; WORD green[256]; WORD blue[256]; };
struct DIJOYSTATE2 { // dinput.h, 272 bytes
    LONG  lX, lY, lZ, lRx, lRy, lRz;
    LONG  rglSlider[2];
    DWORD rgdwPOV[4];
    BYTE  rgbButtons[128];
    LONG  lVX, lVY, lVZ, lVRx, lVRy, lVRz;
    LONG  rglVSlider[2];
    LONG  lAX, lAY, lAZ, lARx, lARy, lARz;
    LONG  rglASlider[2];
    LONG  lFX, lFY, lFZ, lFRx, lFRy, lFRz;
    LONG  rglFSlider[2];
};

// ---- process arguments (MSVC CRT globals); defined and filled by portable/app_main.cpp ----
extern int       __argc;
extern char**    __argv;
extern wchar_t** __wargv;

// ---- functions with a straight SDL3 equivalent (portable/app_win32_compat.cpp) ----
//! High bit (0x8000) set while the key is down (SDL keyboard state; VK codes map to SDL scancodes; unknown VK -> 0)
SHORT GetKeyState(int vk);
SHORT GetAsyncKeyState(int vk);
//! Only MAPVK_VK_TO_CHAR is meaningful for the Win32 window procedure, which does not exist here: logs once, returns 0 (= no mapping)
UINT MapVirtualKey(UINT code, UINT mapType);
//! `hwnd` is the SDL_Window* (see top). Client area in pixels, origin 0,0. FALSE if there is no window.
BOOL GetClientRect(HWND hwnd, RECT* rect);

#endif // !_WIN32
