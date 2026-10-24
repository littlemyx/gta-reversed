// MAC-W2 (W-2): the pure part of the portable SDL3 -> game input feed. Header only, std + SDL3 constants only (no game types), so tests/portable/input_test.cpp
// compiles the REAL tables. Used by game_sa/Pad_SDL.cpp (keyboard) and portable/app_pad.cpp (gamepad).
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>

namespace notsa::portable::input {

//--------------------------------------------------------------------------------------------------
// Keyboard: SDL key event -> the slot of CKeyboardState that the Win32 build fills (app_input.cpp HandleKeyEvent, RsKeyCodes)
//--------------------------------------------------------------------------------------------------
enum class KeySlot : uint8_t {
    None, // the key is not fed to the game
    FKey, // index 0..11
    Esc, Insert, Del, Home, End, PgUp, PgDn, Up, Down, Left, Right, Scroll, Pause, NumLock,
    Div, Mul, Sub, Add, Enter /*numpad Enter, rsPADENTER*/, Decimal /*numpad ., rsPADDEL*/,
    Num, // numpad digit, index 0..9 (num0..num9)
    Back /*rsBACKSP*/, Tab, CapsLock, ExtEnter /*main Return, rsENTER*/,
    LShift, RShift, LCtrl, RCtrl, LAlt, RAlt, LWin, RWin, Apps,
    Standard, // CKeyboardState::standardKeys[index]: the upper-case ASCII code of the key (rsA..rsZ, '0'..'9', rsSPACE, ',', '.', ';' ...)
};

struct KeyTarget {
    KeySlot slot  = KeySlot::None;
    uint8_t index = 0; // FKey / Num / Standard
    bool operator==(const KeyTarget&) const = default;
};

// `sc` = e.key.scancode, `key` = e.key.key (SDL3: the layout dependent, UNSHIFTED key, like a Win32 virtual key).
inline KeyTarget MapKey(SDL_Scancode sc, SDL_Keycode key) {
    // The keypad first: SDL3 gives it the same keycodes as the main block for / * - + . (SDLK_SLASH ...), only the scancode tells them apart. The Win32 build
    // reports the numpad operators as VK_DIVIDE / VK_MULTIPLY / VK_SUBTRACT / VK_ADD / VK_DECIMAL (= div mul sub add decimal) and the main-block keys as plain
    // characters (standardKeys['/'], ['-'], ['.'] ...).
    switch (sc) {
    case SDL_SCANCODE_KP_0: return { KeySlot::Num, 0 };
    case SDL_SCANCODE_KP_1: return { KeySlot::Num, 1 };
    case SDL_SCANCODE_KP_2: return { KeySlot::Num, 2 };
    case SDL_SCANCODE_KP_3: return { KeySlot::Num, 3 };
    case SDL_SCANCODE_KP_4: return { KeySlot::Num, 4 };
    case SDL_SCANCODE_KP_5: return { KeySlot::Num, 5 };
    case SDL_SCANCODE_KP_6: return { KeySlot::Num, 6 };
    case SDL_SCANCODE_KP_7: return { KeySlot::Num, 7 };
    case SDL_SCANCODE_KP_8: return { KeySlot::Num, 8 };
    case SDL_SCANCODE_KP_9: return { KeySlot::Num, 9 };
    case SDL_SCANCODE_KP_DIVIDE:   return { KeySlot::Div };
    case SDL_SCANCODE_KP_MULTIPLY: return { KeySlot::Mul };
    case SDL_SCANCODE_KP_MINUS:    return { KeySlot::Sub };
    case SDL_SCANCODE_KP_PLUS:     return { KeySlot::Add };
    case SDL_SCANCODE_KP_PERIOD:   return { KeySlot::Decimal };
    case SDL_SCANCODE_KP_ENTER:    return { KeySlot::Enter };
    default: break;
    }
    if (key >= SDLK_F1 && key <= SDLK_F12) {
        return { KeySlot::FKey, (uint8_t)(key - SDLK_F1) };
    }
    switch (key) {
    case SDLK_ESCAPE:       return { KeySlot::Esc };
    case SDLK_INSERT:       return { KeySlot::Insert };
    case SDLK_DELETE:       return { KeySlot::Del };
    case SDLK_HOME:         return { KeySlot::Home };
    case SDLK_END:          return { KeySlot::End };
    case SDLK_PAGEUP:       return { KeySlot::PgUp };
    case SDLK_PAGEDOWN:     return { KeySlot::PgDn };
    case SDLK_UP:           return { KeySlot::Up };
    case SDLK_DOWN:         return { KeySlot::Down };
    case SDLK_LEFT:         return { KeySlot::Left };
    case SDLK_RIGHT:        return { KeySlot::Right };
    case SDLK_SCROLLLOCK:   return { KeySlot::Scroll };
    case SDLK_PAUSE:        return { KeySlot::Pause };
    case SDLK_NUMLOCKCLEAR: return { KeySlot::NumLock };
    case SDLK_BACKSPACE:    return { KeySlot::Back };
    case SDLK_TAB:          return { KeySlot::Tab };
    case SDLK_CAPSLOCK:     return { KeySlot::CapsLock };
    case SDLK_RETURN:       return { KeySlot::ExtEnter }; // main Return = rsENTER = `extenter` (default 'Enter/Exit vehicle', menu confirm)
    case SDLK_LSHIFT:       return { KeySlot::LShift };
    case SDLK_RSHIFT:       return { KeySlot::RShift };
    case SDLK_LCTRL:        return { KeySlot::LCtrl };
    case SDLK_RCTRL:        return { KeySlot::RCtrl };
    case SDLK_LALT:         return { KeySlot::LAlt };   // macOS: Option
    case SDLK_RALT:         return { KeySlot::RAlt };
    case SDLK_LGUI:         return { KeySlot::LWin };   // macOS: Command
    case SDLK_RGUI:         return { KeySlot::RWin };
    case SDLK_APPLICATION:  return { KeySlot::Apps };
    default: break;
    }
    // Everything else with a character code: standardKeys[upper-case ASCII]. Codes above 0xFF (SDLK_SCANCODE_MASK keys, Unicode) are not fed; the range test
    // comes BEFORE any <cctype> call (toupper of an out-of-range int is undefined).
    if (key > 0 && key < 0x100) {
        const int c = (key >= 'a' && key <= 'z') ? key - 'a' + 'A' : (int)key;
        return { KeySlot::Standard, (uint8_t)c };
    }
    return {};
}

//--------------------------------------------------------------------------------------------------
// Gamepad: SDL gamepad state -> the DIJOYSTATE2 values the Win32 build reads from DirectInput (CPad::ProcessPad, ControllerConfigManager)
//--------------------------------------------------------------------------------------------------
// Layout (docs: .notes/reports/MAC-W2.md): the game's default joypad configuration (InitDefaultControlConfigJoyPad, "standard" table) is the PS2 pad one, button
// number n = rgbButtons[n-1]: 1 Circle, 2 Cross, 3 Square, 4 Triangle, 5 L2, 6 R2, 7 L1, 8 R1, 9 Select, 10 L3, 11 R3, 12 Start, 13..16 D-pad up/right/down/left.
// An Xbox pad sits on those positions by shape (A = Cross = south, B = Circle = east, X = Square = west, Y = Triangle = north, LB/RB = L1/R1, the triggers = L2/R2).
constexpr int kDeviceAxisMax = 2000; // CPad::ProcessPad: deviceAxisMin/Max = -2000/2000
constexpr int kTriggerPressed = 16384; // SDL trigger axes 0..32767: pressed above one half

struct PadSnapshot {
    int16_t lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0; // SDL_GAMEPAD_AXIS_* (-32768..32767; triggers 0..32767)
    bool a = false, b = false, x = false, y = false, back = false, start = false, lstick = false, rstick = false, lb = false, rb = false;
    bool dup = false, ddown = false, dleft = false, dright = false;
};

struct JoyState {
    int32_t lX = 0, lY = 0; // left stick,  -2000..2000, +Y = down (DirectInput and SDL agree)
    int32_t lZ = 0, lRz = 0; // right stick X, Y: the game reads the right stick from the Z and Z-rotation axes (CPad::ProcessPad)
    bool    button[16] = {}; // rgbButtons[0..15] pressed
    bool operator==(const JoyState&) const = default;
};

inline int32_t ScaleAxis(int16_t v) { // -32768..32767 -> -2000..2000 (exact at both ends, symmetric)
    const int32_t s = v < 0 ? std::max<int32_t>(v, -32767) : v;
    return (int32_t)((int64_t)s * kDeviceAxisMax / 32767);
}

inline JoyState BuildJoy(const PadSnapshot& p) {
    JoyState j;
    j.lX = ScaleAxis(p.lx);
    j.lY = ScaleAxis(p.ly);
    j.lZ = ScaleAxis(p.rx);
    j.lRz = ScaleAxis(p.ry);
    j.button[0]  = p.b;
    j.button[1]  = p.a;
    j.button[2]  = p.x;
    j.button[3]  = p.y;
    j.button[4]  = p.lt > kTriggerPressed;
    j.button[5]  = p.rt > kTriggerPressed;
    j.button[6]  = p.lb;
    j.button[7]  = p.rb;
    j.button[8]  = p.back;
    j.button[9]  = p.lstick;
    j.button[10] = p.rstick;
    j.button[11] = p.start;
    j.button[12] = p.dup;
    j.button[13] = p.dright;
    j.button[14] = p.ddown;
    j.button[15] = p.dleft;
    return j;
}

inline PadSnapshot ReadSnapshot(SDL_Gamepad* g) {
    PadSnapshot p;
    if (!g) {
        return p;
    }
    p.lx = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX);
    p.ly = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY);
    p.rx = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTX);
    p.ry = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTY);
    p.lt = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    p.rt = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    p.a = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_SOUTH);
    p.b = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_EAST);
    p.x = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_WEST);
    p.y = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_NORTH);
    p.back = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_BACK);
    p.start = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_START);
    p.lstick = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_LEFT_STICK);
    p.rstick = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    p.lb = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    p.rb = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    p.dup = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_UP);
    p.ddown = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    p.dleft = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    p.dright = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    return p;
}

//--------------------------------------------------------------------------------------------------
// Stick -> game axis (CPad::ProcessPad): |pos| <= 0.3 is a dead zone (the axis stays 0), else pos * 128 (-128..128), optionally inverted.
//--------------------------------------------------------------------------------------------------
inline bool StickToGame(int32_t deviceValue, bool inverted, int32_t& out) {
    float pos = (float)deviceValue / (float)kDeviceAxisMax;
    if (std::abs(pos) <= 0.3f) {
        return false;
    }
    if (inverted) {
        pos = -pos;
    }
    out = (int32_t)(pos * 128.0f);
    return true;
}

} // namespace notsa::portable::input
