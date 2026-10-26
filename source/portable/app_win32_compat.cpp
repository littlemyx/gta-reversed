// MAC-W0a: implementations of app_win32_compat.h (portable build only; not part of the Windows targets)
#include "StdInc.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <mutex>
#include <set>
#include <string>

#include "app_win32_compat.h"

namespace notsa::portable {
void Unsupported(const char* what) {
    static std::mutex         s_Mtx;
    static std::set<std::string> s_Seen;
    std::scoped_lock lock{ s_Mtx };
    if (s_Seen.insert(what).second) {
        std::fprintf(stderr, "[portable] not implemented on this platform: %s\n", what);
    }
}
} // namespace notsa::portable

#ifndef _WIN32 // MAC-WIN64: the Win32 API twins below are the real Windows API on Windows
// VK code -> SDL scancode (0 = SDL_SCANCODE_UNKNOWN: not mapped, key reported as up)
static SDL_Scancode VkToScancode(int vk) {
    if (vk >= 'A' && vk <= 'Z') { return (SDL_Scancode)(SDL_SCANCODE_A + (vk - 'A')); }
    if (vk >= '1' && vk <= '9') { return (SDL_Scancode)(SDL_SCANCODE_1 + (vk - '1')); }
    if (vk == '0')              { return SDL_SCANCODE_0; }
    if (vk >= VK_F1 && vk <= VK_F12) { return (SDL_Scancode)(SDL_SCANCODE_F1 + (vk - VK_F1)); }
    if (vk >= VK_NUMPAD1 && vk <= VK_NUMPAD9) { return (SDL_Scancode)(SDL_SCANCODE_KP_1 + (vk - VK_NUMPAD1)); }
    switch (vk) {
    case VK_BACK:     return SDL_SCANCODE_BACKSPACE;
    case VK_TAB:      return SDL_SCANCODE_TAB;
    case VK_RETURN:   return SDL_SCANCODE_RETURN;
    case VK_PAUSE:    return SDL_SCANCODE_PAUSE;
    case VK_CAPITAL:  return SDL_SCANCODE_CAPSLOCK;
    case VK_ESCAPE:   return SDL_SCANCODE_ESCAPE;
    case VK_SPACE:    return SDL_SCANCODE_SPACE;
    case VK_PRIOR:    return SDL_SCANCODE_PAGEUP;
    case VK_NEXT:     return SDL_SCANCODE_PAGEDOWN;
    case VK_END:      return SDL_SCANCODE_END;
    case VK_HOME:     return SDL_SCANCODE_HOME;
    case VK_LEFT:     return SDL_SCANCODE_LEFT;
    case VK_UP:       return SDL_SCANCODE_UP;
    case VK_RIGHT:    return SDL_SCANCODE_RIGHT;
    case VK_DOWN:     return SDL_SCANCODE_DOWN;
    case VK_INSERT:   return SDL_SCANCODE_INSERT;
    case VK_DELETE:   return SDL_SCANCODE_DELETE;
    case VK_LWIN:     return SDL_SCANCODE_LGUI;
    case VK_RWIN:     return SDL_SCANCODE_RGUI;
    case VK_APPS:     return SDL_SCANCODE_APPLICATION;
    case VK_NUMPAD0:  return SDL_SCANCODE_KP_0;
    case VK_MULTIPLY: return SDL_SCANCODE_KP_MULTIPLY;
    case VK_ADD:      return SDL_SCANCODE_KP_PLUS;
    case VK_SUBTRACT: return SDL_SCANCODE_KP_MINUS;
    case VK_DECIMAL:  return SDL_SCANCODE_KP_PERIOD;
    case VK_DIVIDE:   return SDL_SCANCODE_KP_DIVIDE;
    case VK_NUMLOCK:  return SDL_SCANCODE_NUMLOCKCLEAR;
    case VK_SCROLL:   return SDL_SCANCODE_SCROLLLOCK;
    case VK_LSHIFT:   return SDL_SCANCODE_LSHIFT;
    case VK_RSHIFT:   return SDL_SCANCODE_RSHIFT;
    case VK_LCONTROL: return SDL_SCANCODE_LCTRL;
    case VK_RCONTROL: return SDL_SCANCODE_RCTRL;
    case VK_LMENU:    return SDL_SCANCODE_LALT;
    case VK_RMENU:    return SDL_SCANCODE_RALT;
    default:          return SDL_SCANCODE_UNKNOWN;
    }
}

static bool IsScancodeDown(SDL_Scancode sc) {
    int         n  = 0;
    const bool* ks = SDL_GetKeyboardState(&n);
    return ks && sc > SDL_SCANCODE_UNKNOWN && (int)sc < n && ks[sc];
}

SHORT GetKeyState(int vk) {
    bool down = false;
    switch (vk) { // "either side" codes
    case VK_SHIFT:   down = IsScancodeDown(SDL_SCANCODE_LSHIFT) || IsScancodeDown(SDL_SCANCODE_RSHIFT); break;
    case VK_CONTROL: down = IsScancodeDown(SDL_SCANCODE_LCTRL)  || IsScancodeDown(SDL_SCANCODE_RCTRL);  break;
    case VK_MENU:    down = IsScancodeDown(SDL_SCANCODE_LALT)   || IsScancodeDown(SDL_SCANCODE_RALT);   break;
    default:         down = IsScancodeDown(VkToScancode(vk)); break;
    }
    return down ? (SHORT)0x8000 : (SHORT)0; // toggle state (low bit) is not tracked
}

SHORT GetAsyncKeyState(int vk) {
    return GetKeyState(vk); // SDL's keyboard state is updated by SDL_PumpEvents on the game thread; there is no separate "async" state
}

UINT MapVirtualKey(UINT, UINT) {
    notsa::portable::Unsupported("MapVirtualKey (Win32 window procedure key translation; SDL3 events carry keycodes)"); // W-A6
    return 0;
}

BOOL GetClientRect(HWND hwnd, RECT* rect) {
    int w = 0, h = 0;
    if (!hwnd || !SDL_GetWindowSizeInPixels(reinterpret_cast<SDL_Window*>(hwnd), &w, &h)) {
        return FALSE;
    }
    *rect = RECT{ 0, 0, w, h };
    return TRUE;
}
#endif // !_WIN32
