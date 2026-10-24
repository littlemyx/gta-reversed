// MAC-W2 host test: the REAL portable/app_input_map.h (keyboard table, gamepad -> DIJOYSTATE2 values, stick dead zone) and portable/app_present_pacer.h (vsync fallback decision)
// against the Win32 conventions of the game (app_input.cpp HandleKeyEvent, CPad::ProcessPad, ControllerConfigManager joypad table), plus a real SDL3 virtual gamepad
// (SDL events / polling the way app_pad.cpp does it). Build + run: tools/portable/run_input_test.sh [mutate]
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <SDL3/SDL.h>

#include "portable/app_input_map.h"
#include "portable/app_present_pacer.h"

using namespace notsa::portable;
using namespace notsa::portable::input;

static int g_fail = 0, g_checks = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static KeyTarget K(SDL_Scancode sc, SDL_Keycode key) { return MapKey(sc, key); }
static bool Is(KeyTarget t, KeySlot s, int idx = 0) { return t.slot == s && (t.index == idx || (s != KeySlot::FKey && s != KeySlot::Num && s != KeySlot::Standard)); }

static void TestKeys() {
    // keypad operators are told apart from the main block by SCANCODE (same keycode in SDL3)
    CHECK(Is(K(SDL_SCANCODE_KP_DIVIDE, SDLK_KP_DIVIDE), KeySlot::Div));
    CHECK(Is(K(SDL_SCANCODE_SLASH, SDLK_SLASH), KeySlot::Standard, '/'));
    CHECK(Is(K(SDL_SCANCODE_KP_MULTIPLY, SDLK_KP_MULTIPLY), KeySlot::Mul));
    CHECK(Is(K(SDL_SCANCODE_KP_MINUS, SDLK_KP_MINUS), KeySlot::Sub));
    CHECK(Is(K(SDL_SCANCODE_MINUS, SDLK_MINUS), KeySlot::Standard, '-'));
    CHECK(Is(K(SDL_SCANCODE_KP_PLUS, SDLK_KP_PLUS), KeySlot::Add));
    CHECK(Is(K(SDL_SCANCODE_EQUALS, SDLK_EQUALS), KeySlot::Standard, '='));
    CHECK(Is(K(SDL_SCANCODE_KP_PERIOD, SDLK_KP_PERIOD), KeySlot::Decimal));
    CHECK(Is(K(SDL_SCANCODE_PERIOD, SDLK_PERIOD), KeySlot::Standard, '.'));
    CHECK(Is(K(SDL_SCANCODE_KP_ENTER, SDLK_KP_ENTER), KeySlot::Enter));
    CHECK(Is(K(SDL_SCANCODE_RETURN, SDLK_RETURN), KeySlot::ExtEnter));
    for (int i = 0; i < 10; i++) {
        CHECK(Is(K((SDL_Scancode)(SDL_SCANCODE_KP_1 + (i == 0 ? 9 : i - 1)), SDLK_KP_1), KeySlot::Num, i));
    }
    // the rest of the Win32 slots
    CHECK(Is(K(SDL_SCANCODE_BACKSPACE, SDLK_BACKSPACE), KeySlot::Back));
    CHECK(Is(K(SDL_SCANCODE_TAB, SDLK_TAB), KeySlot::Tab));
    CHECK(Is(K(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE), KeySlot::Esc));
    CHECK(Is(K(SDL_SCANCODE_UP, SDLK_UP), KeySlot::Up));
    CHECK(Is(K(SDL_SCANCODE_DOWN, SDLK_DOWN), KeySlot::Down));
    CHECK(Is(K(SDL_SCANCODE_LEFT, SDLK_LEFT), KeySlot::Left));
    CHECK(Is(K(SDL_SCANCODE_RIGHT, SDLK_RIGHT), KeySlot::Right));
    CHECK(Is(K(SDL_SCANCODE_PAGEUP, SDLK_PAGEUP), KeySlot::PgUp));
    CHECK(Is(K(SDL_SCANCODE_PAGEDOWN, SDLK_PAGEDOWN), KeySlot::PgDn));
    CHECK(Is(K(SDL_SCANCODE_HOME, SDLK_HOME), KeySlot::Home));
    CHECK(Is(K(SDL_SCANCODE_END, SDLK_END), KeySlot::End));
    CHECK(Is(K(SDL_SCANCODE_DELETE, SDLK_DELETE), KeySlot::Del));
    CHECK(Is(K(SDL_SCANCODE_INSERT, SDLK_INSERT), KeySlot::Insert));
    CHECK(Is(K(SDL_SCANCODE_CAPSLOCK, SDLK_CAPSLOCK), KeySlot::CapsLock));
    CHECK(Is(K(SDL_SCANCODE_LSHIFT, SDLK_LSHIFT), KeySlot::LShift));
    CHECK(Is(K(SDL_SCANCODE_RSHIFT, SDLK_RSHIFT), KeySlot::RShift));
    CHECK(Is(K(SDL_SCANCODE_LCTRL, SDLK_LCTRL), KeySlot::LCtrl));
    CHECK(Is(K(SDL_SCANCODE_RCTRL, SDLK_RCTRL), KeySlot::RCtrl));
    CHECK(Is(K(SDL_SCANCODE_LALT, SDLK_LALT), KeySlot::LAlt));
    CHECK(Is(K(SDL_SCANCODE_RALT, SDLK_RALT), KeySlot::RAlt));
    CHECK(Is(K(SDL_SCANCODE_LGUI, SDLK_LGUI), KeySlot::LWin));
    CHECK(Is(K(SDL_SCANCODE_RGUI, SDLK_RGUI), KeySlot::RWin));
    for (int i = 0; i < 12; i++) {
        CHECK(Is(K((SDL_Scancode)(SDL_SCANCODE_F1 + i), SDLK_F1 + i), KeySlot::FKey, i));
    }
    CHECK(K(SDL_SCANCODE_F13, SDLK_F13).slot == KeySlot::None);
    // characters: lower case letters are the upper-case ASCII code (rsA .. rsZ), digits / punctuation / space unchanged
    for (char c = 'a'; c <= 'z'; c++) {
        CHECK(Is(K(SDL_SCANCODE_A, c), KeySlot::Standard, c - 'a' + 'A'));
    }
    CHECK(Is(K(SDL_SCANCODE_1, SDLK_1), KeySlot::Standard, '1'));
    CHECK(Is(K(SDL_SCANCODE_0, SDLK_0), KeySlot::Standard, '0'));
    CHECK(Is(K(SDL_SCANCODE_SPACE, SDLK_SPACE), KeySlot::Standard, ' '));
    CHECK(Is(K(SDL_SCANCODE_COMMA, SDLK_COMMA), KeySlot::Standard, ','));
    CHECK(Is(K(SDL_SCANCODE_SEMICOLON, SDLK_SEMICOLON), KeySlot::Standard, ';'));
    // keycodes above 0xFF never reach <cctype>: Unicode (euro sign), scancode-masked keys without a slot (media keys, volume)
    CHECK(K(SDL_SCANCODE_UNKNOWN, 0x20AC).slot == KeySlot::None);
    CHECK(K(SDL_SCANCODE_MUTE, SDLK_MUTE).slot == KeySlot::None);
    CHECK(K(SDL_SCANCODE_UNKNOWN, 0).slot == KeySlot::None);
    CHECK(K(SDL_SCANCODE_UNKNOWN, 0x7FFFFFFF).slot == KeySlot::None);
}

static void TestJoy() {
    // axis scaling: exact at both ends and symmetric, 0 stays 0
    CHECK(ScaleAxis(0) == 0);
    CHECK(ScaleAxis(32767) == 2000);
    CHECK(ScaleAxis(-32768) == -2000);
    CHECK(ScaleAxis(-32767) == -2000);
    CHECK(ScaleAxis(16384) == 1000);
    CHECK(ScaleAxis(-16384) == -1000);

    // each button on its own lands on exactly the rgbButtons index of the PS2-style default configuration and nothing else
    struct B { bool PadSnapshot::* f; int idx; const char* n; };
    const B bs[] = { { &PadSnapshot::b, 0, "B/Circle=1" }, { &PadSnapshot::a, 1, "A/Cross=2" }, { &PadSnapshot::x, 2, "X/Square=3" }, { &PadSnapshot::y, 3, "Y/Triangle=4" },
        { &PadSnapshot::lb, 6, "LB=7" }, { &PadSnapshot::rb, 7, "RB=8" }, { &PadSnapshot::back, 8, "Back=9" }, { &PadSnapshot::lstick, 9, "L3=10" }, { &PadSnapshot::rstick, 10, "R3=11" },
        { &PadSnapshot::start, 11, "Start=12" }, { &PadSnapshot::dup, 12, "DUp=13" }, { &PadSnapshot::dright, 13, "DRight=14" }, { &PadSnapshot::ddown, 14, "DDown=15" }, { &PadSnapshot::dleft, 15, "DLeft=16" } };
    for (const auto& b : bs) {
        PadSnapshot p;
        p.*(b.f) = true;
        const auto j = BuildJoy(p);
        int n = 0;
        for (int i = 0; i < 16; i++) {
            n += j.button[i];
        }
        if (!(n == 1 && j.button[b.idx])) { std::printf("button %s\n", b.n); }
        CHECK(n == 1 && j.button[b.idx]);
    }
    { PadSnapshot p; p.lt = 32767; const auto j = BuildJoy(p); CHECK(j.button[4] && !j.button[5]); }
    { PadSnapshot p; p.rt = 32767; const auto j = BuildJoy(p); CHECK(j.button[5] && !j.button[4]); }
    { PadSnapshot p; p.lt = 16384; const auto j = BuildJoy(p); CHECK(!j.button[4]); }  // exactly one half: not pressed
    { PadSnapshot p; p.lt = 16385; const auto j = BuildJoy(p); CHECK(j.button[4]); }
    { PadSnapshot p; const auto j = BuildJoy(p); bool any = false; for (bool b : j.button) any |= b; CHECK(!any && j.lX == 0 && j.lY == 0 && j.lZ == 0 && j.lRz == 0); }
    { PadSnapshot p; p.lx = 32767; p.ly = -32768; p.rx = -32768; p.ry = 32767; const auto j = BuildJoy(p); CHECK(j.lX == 2000 && j.lY == -2000 && j.lZ == -2000 && j.lRz == 2000); }

    // the stick -> game value of CPad::ProcessPad: |pos| <= 0.3 is a dead zone (axis untouched), else trunc(pos * 128), optional invert
    int32_t out = 777;
    CHECK(!StickToGame(0, false, out) && out == 777);
    CHECK(!StickToGame(600, false, out) && out == 777);   // exactly 0.3
    CHECK(!StickToGame(-600, false, out) && out == 777);
    CHECK(StickToGame(601, false, out) && out == 38);     // 0.3005 * 128
    CHECK(StickToGame(2000, false, out) && out == 128);
    CHECK(StickToGame(-2000, false, out) && out == -128);
    CHECK(StickToGame(2000, true, out) && out == -128);
    CHECK(StickToGame(-1000, true, out) && out == 64);
    CHECK(StickToGame(1000, false, out) && out == 64);
}

static void TestPacer() {
    constexpr int64_t P = 16'666'667, ms = 1'000'000;
    PaceInput in;
    in.vsyncRequested = true; in.nowNs = 10'000 * ms; in.lastPresentNs = in.nowNs - 5 * ms; in.periodNs = P; in.lastTickNs = in.nowNs - 3 * ms; in.linkExists = true;
    // healthy: wait for a tick, bounded
    auto d = DecidePacing(in);
    CHECK(d.action == PaceAction::WaitTick && d.reason == PaceReason::Vsync && d.untilNs > in.nowNs && d.untilNs - in.nowNs <= TickTimeoutNs(P));
    CHECK(TickTimeoutNs(P) >= 50 * ms && TickTimeoutNs(P) <= 100 * ms);
    // the game did not ask for vsync: untouched
    { auto i2 = in; i2.vsyncRequested = false; CHECK(DecidePacing(i2).action == PaceAction::None); }
    // window hidden / minimised / occluded -> timed limiter, one period after the previous present
    { auto i2 = in; i2.windowVisible = false; auto d2 = DecidePacing(i2); CHECK(d2.action == PaceAction::SleepUntil && d2.reason == PaceReason::WindowNotVisible && d2.untilNs == in.lastPresentNs + P); }
    // display asleep
    { auto i2 = in; i2.displayAwake = false; auto d2 = DecidePacing(i2); CHECK(d2.action == PaceAction::SleepUntil && d2.reason == PaceReason::DisplayAsleep && d2.untilNs == in.lastPresentNs + P); }
    // the link is silent: never ticked / last tick older than max(4 periods, 100 ms)
    { auto i2 = in; i2.lastTickNs = 0; CHECK(DecidePacing(i2).action == PaceAction::SleepUntil && DecidePacing(i2).reason == PaceReason::LinkSilent); }
    { auto i2 = in; i2.lastTickNs = in.nowNs - 101 * ms; CHECK(DecidePacing(i2).action == PaceAction::SleepUntil); }
    { auto i2 = in; i2.lastTickNs = in.nowNs - 99 * ms; CHECK(DecidePacing(i2).action == PaceAction::WaitTick); }
    { auto i2 = in; i2.linkExists = false; CHECK(DecidePacing(i2).action == PaceAction::SleepUntil && DecidePacing(i2).reason == PaceReason::NoLink); }
    // first frame: no wait; a frame that already took longer than a period: deadline in the past (no sleep)
    { auto i2 = in; i2.windowVisible = false; i2.lastPresentNs = 0; CHECK(DecidePacing(i2).untilNs == i2.nowNs); }
    { auto i2 = in; i2.windowVisible = false; i2.lastPresentNs = in.nowNs - 50 * ms; CHECK(DecidePacing(i2).untilNs < i2.nowNs); }
    // a 120 Hz ProMotion display: the period follows the display
    { auto i2 = in; i2.windowVisible = false; i2.periodNs = 8'333'333; CHECK(DecidePacing(i2).untilNs == in.lastPresentNs + 8'333'333); }
    // resume: the same inputs with the window visible again wait for the tick again
    { auto i2 = in; i2.windowVisible = false; CHECK(DecidePacing(i2).action == PaceAction::SleepUntil); i2.windowVisible = true; CHECK(DecidePacing(i2).action == PaceAction::WaitTick); }
    // property: whatever the inputs, a wait is never unbounded and only happens when everything is healthy; a sleep never exceeds one period (+ the previous present's slack)
    int waits = 0;
    for (int vis = 0; vis < 2; vis++) for (int awake = 0; awake < 2; awake++) for (int link = 0; link < 2; link++) for (int req = 0; req < 2; req++)
    for (int64_t tick : { (int64_t)0, in.nowNs - 1 * ms, in.nowNs - 90 * ms, in.nowNs - 5000 * ms })
    for (int64_t per : { (int64_t)4'000'000, (int64_t)8'333'333, P, (int64_t)33'333'333 })
    for (int64_t last : { (int64_t)0, in.nowNs - 1 * ms, in.nowNs - 400 * ms }) {
        PaceInput i2 = in; i2.windowVisible = vis; i2.displayAwake = awake; i2.linkExists = link; i2.vsyncRequested = req; i2.lastTickNs = tick; i2.periodNs = per; i2.lastPresentNs = last;
        const auto r = DecidePacing(i2);
        if (r.action == PaceAction::WaitTick) {
            waits++;
            CHECK(req && vis && awake && link && tick != 0 && i2.nowNs - tick <= TickFreshnessNs(per));
            CHECK(r.untilNs - i2.nowNs <= TickTimeoutNs(per) && r.untilNs > i2.nowNs);
            CHECK(TickTimeoutNs(per) < 250 * ms);
        } else if (r.action == PaceAction::SleepUntil) {
            CHECK(req);
            CHECK(r.untilNs - i2.nowNs <= std::max<int64_t>(per, 1 * ms)); // never longer than a period
        } else {
            CHECK(!req);
        }
    }
    CHECK(waits > 0);
}

// A real SDL3 virtual gamepad through the same calls app_pad.cpp makes (ReadSnapshot -> BuildJoy), and synthetic key events through SDL's event queue
static void TestSdl() {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_VIDEO)) {
        std::printf("NOTE: SDL_Init(GAMEPAD) failed (%s): SDL part skipped\n", SDL_GetError());
        return;
    }
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD; desc.naxes = SDL_GAMEPAD_AXIS_COUNT; desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT; desc.name = "test pad";
    const SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    CHECK(id != 0);
    SDL_Joystick* joy = SDL_OpenJoystick(id);
    SDL_PumpEvents();
    bool added = false;
    for (SDL_Event e; SDL_PollEvent(&e);) {
        added |= e.type == SDL_EVENT_GAMEPAD_ADDED && e.gdevice.which == id; // the hot plug event the game reacts to
    }
    CHECK(added);
    SDL_Gamepad* g = SDL_OpenGamepad(id);
    CHECK(joy && g);
    if (!joy || !g) {
        return;
    }
    const struct { const char* n; SDL_GamepadButton b; int idx; } map[] = {
        { "A", SDL_GAMEPAD_BUTTON_SOUTH, 1 }, { "B", SDL_GAMEPAD_BUTTON_EAST, 0 }, { "X", SDL_GAMEPAD_BUTTON_WEST, 2 }, { "Y", SDL_GAMEPAD_BUTTON_NORTH, 3 }, { "Back", SDL_GAMEPAD_BUTTON_BACK, 8 },
        { "Start", SDL_GAMEPAD_BUTTON_START, 11 }, { "L3", SDL_GAMEPAD_BUTTON_LEFT_STICK, 9 }, { "R3", SDL_GAMEPAD_BUTTON_RIGHT_STICK, 10 }, { "LB", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 6 },
        { "RB", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 7 }, { "DUp", SDL_GAMEPAD_BUTTON_DPAD_UP, 12 }, { "DDown", SDL_GAMEPAD_BUTTON_DPAD_DOWN, 14 }, { "DLeft", SDL_GAMEPAD_BUTTON_DPAD_LEFT, 15 }, { "DRight", SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 13 } };
    for (const auto& m : map) {
        SDL_SetJoystickVirtualButton(joy, (int)m.b, true);
        SDL_UpdateGamepads();
        const auto j = BuildJoy(ReadSnapshot(g));
        int n = 0;
        for (bool b : j.button) n += b;
        if (!(n == 1 && j.button[m.idx])) { std::printf("virtual pad button %s\n", m.n); }
        CHECK(n == 1 && j.button[m.idx]);
        SDL_SetJoystickVirtualButton(joy, (int)m.b, false);
    }
    SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_LEFTY, 32767);
    SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_RIGHTX, -32768);
    SDL_SetJoystickVirtualAxis(joy, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
    SDL_UpdateGamepads();
    const auto j = BuildJoy(ReadSnapshot(g));
    CHECK(j.lY == 2000 && j.lX == 0 && j.lZ == -2000 && j.lRz == 0 && j.button[5] && !j.button[4]);
    CHECK(BuildJoy(ReadSnapshot(nullptr)) == JoyState{});
    SDL_CloseGamepad(g);
    SDL_CloseJoystick(joy);
    SDL_DetachVirtualJoystick(id);

    // synthetic keyboard events through the queue: the fields MapKey reads survive SDL_PushEvent / SDL_PollEvent
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN; e.key.key = SDLK_DOWN; e.key.scancode = SDL_SCANCODE_DOWN; e.key.down = true;
    SDL_PushEvent(&e);
    e.key.key = SDLK_BACKSPACE; e.key.scancode = SDL_SCANCODE_BACKSPACE;
    SDL_PushEvent(&e);
    const auto NextKey = [](SDL_Event& out) { // skips the device events of the pad that was just detached
        while (SDL_PollEvent(&out)) {
            if (out.type == SDL_EVENT_KEY_DOWN) {
                return true;
            }
        }
        return false;
    };
    SDL_Event r;
    CHECK(NextKey(r) && Is(MapKey(r.key.scancode, r.key.key), KeySlot::Down));
    CHECK(NextKey(r) && Is(MapKey(r.key.scancode, r.key.key), KeySlot::Back));
    SDL_Quit();
}

int main() {
    TestKeys();
    TestJoy();
    TestPacer();
    TestSdl();
    std::printf("%d checks, %d failed\n", g_checks, g_fail);
    std::printf("RESULT INPUT %s\n", g_fail ? "FAILED" : "OK");
    return g_fail ? 1 : 0;
}
