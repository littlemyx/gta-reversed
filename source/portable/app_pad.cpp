// MAC-W2 (W-2): SDL3 gamepad feed (see app_pad.h). Portable-only TU.
#include "StdInc.h"

#include <SDL3/SDL.h>
#include <cstring>

#include "app_input_map.h"
#include "app_pad.h"
#include "ControllerConfigManager.h"
#include "MenuManager.h"
#include "Pad.h"
#include "platform.h"

namespace notsa::portable {
namespace {
constexpr int kPads = NUM_OF_GAME_JOYS; // 2

struct Slot {
    SDL_Gamepad*    gamepad = nullptr;
    SDL_JoystickID  id      = 0;
};
Slot s_Slots[kPads];

// Virtual gamepad of the injector (cmd:padattach)
SDL_Joystick*  s_VirtualJoy = nullptr;
SDL_JoystickID s_VirtualId  = 0;

void FillJoyInfo(int n, SDL_Gamepad* g) {
    auto& j               = AllValidWinJoys.JoyStickNum[n];
    j.wDeviceID           = (uint32)n;
    j.bJoyAttachedToPort  = g != nullptr;
    j.bZAxisPresent       = g != nullptr; // the right stick is read from Z / Z-rotation (CPad::ProcessPad)
    j.bZRotPresent        = g != nullptr;
    j.wVendorID           = g ? SDL_GetGamepadVendor(g) : 0;
    j.wProductID          = g ? SDL_GetGamepadProduct(g) : 0;
}

bool Open(SDL_JoystickID id) {
    for (const auto& s : s_Slots) {
        if (s.gamepad && s.id == id) {
            return true;
        }
    }
    for (int n = 0; n < kPads; n++) {
        if (s_Slots[n].gamepad) {
            continue;
        }
        SDL_Gamepad* g = SDL_OpenGamepad(id);
        if (!g) {
            NOTSA_LOG_WARN("[pad] SDL_OpenGamepad({}) failed: {}", (unsigned)id, SDL_GetError());
            return false;
        }
        s_Slots[n] = { g, id };
        FillJoyInfo(n, g);
        NOTSA_LOG_INFO("[pad] pad {} = \"{}\" (vendor {:04x} product {:04x}, SDL id {})", n + 1, SDL_GetGamepadName(g) ? SDL_GetGamepadName(g) : "?", (unsigned)SDL_GetGamepadVendor(g), (unsigned)SDL_GetGamepadProduct(g), (unsigned)id);
        // A pad that appears after the controls were set up (hot plug) gets the default joypad bindings when the loaded configuration has none
        if (n == 0 && ControlsManager.GetControllerKeyAssociatedWithAction(eControllerAction::PED_FIRE_WEAPON, eControllerType::JOY_STICK) == 0) {
            ControlsManager.InitDefaultControlConfigJoyPad(JOYBUTTON_COUNT - 1);
        }
        return true;
    }
    return false; // both pads taken
}

void Close(SDL_JoystickID id) {
    for (int n = 0; n < kPads; n++) {
        if (s_Slots[n].gamepad && s_Slots[n].id == id) {
            NOTSA_LOG_INFO("[pad] pad {} removed", n + 1);
            SDL_CloseGamepad(s_Slots[n].gamepad);
            s_Slots[n] = {};
            FillJoyInfo(n, nullptr);
            // all buttons up on the next poll
            ControlsManager.m_NewJoyState = {};
        }
    }
}

const char* const kBtn[] = { "a", "b", "x", "y", "back", "guide", "start", "ls", "rs", "lb", "rb", "du", "dd", "dl", "dr" }; // SDL_GamepadButton order (SOUTH, EAST, WEST, NORTH, BACK, GUIDE, START, ...)
const char* const kAxis[] = { "lx", "ly", "rx", "ry", "lt", "rt" };                                                         // SDL_GamepadAxis order
SDL_GamepadButton ButtonByName(const std::string& n) {
    // SDL order: SOUTH(a) EAST(b) WEST(x) NORTH(y) BACK GUIDE START LEFT_STICK RIGHT_STICK LEFT_SHOULDER RIGHT_SHOULDER DPAD_UP DPAD_DOWN DPAD_LEFT DPAD_RIGHT
    static const SDL_GamepadButton order[] = { SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH, SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE,
        SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT };
    for (size_t i = 0; i < std::size(kBtn); i++) {
        if (n == kBtn[i]) {
            return order[i];
        }
    }
    return SDL_GAMEPAD_BUTTON_INVALID;
}
} // namespace

void PadInit() {
    for (int n = 0; n < kPads; n++) {
        FillJoyInfo(n, nullptr);
    }
    int count = 0;
    if (SDL_JoystickID* ids = SDL_GetGamepads(&count)) {
        for (int i = 0; i < count; i++) {
            Open(ids[i]);
        }
        SDL_free(ids);
    }
}

bool PadOnEvent(const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_GAMEPAD_ADDED:   Open(e.gdevice.which); return true;
    case SDL_EVENT_GAMEPAD_REMOVED: Close(e.gdevice.which); return true;
    }
    return false;
}

void PadShutdown() {
    for (auto& s : s_Slots) {
        if (s.gamepad) {
            SDL_CloseGamepad(s.gamepad);
        }
        s = {};
    }
    if (s_VirtualJoy) {
        SDL_CloseJoystick(s_VirtualJoy);
        SDL_DetachVirtualJoystick(s_VirtualId);
        s_VirtualJoy = nullptr;
    }
}

// The body of CPad::ProcessPad (Pad.cpp, DirectInput) with the device state coming from SDL.
void PadProcess(int padIDi) {
    if (padIDi < 0 || padIDi >= kPads || !s_Slots[padIDi].gamepad) {
        return;
    }
    auto padID = (ePadID)padIDi;
    const auto j = input::BuildJoy(input::ReadSnapshot(s_Slots[padIDi].gamepad));

    DIJOYSTATE2 joyState{};
    joyState.lX = j.lX;
    joyState.lY = j.lY;
    joyState.lZ = j.lZ;
    joyState.lRz = j.lRz;
    for (int i = 0; i < 16; i++) {
        joyState.rgbButtons[i] = j.button[i] ? 0x80 : 0;
    }

    // Update NewJoyState with the current joyState and get the previous value
    const auto previousNewJoyState = std::exchange(ControlsManager.m_NewJoyState, joyState);
    if (ControlsManager.m_bJoyJustInitialised) {
        ControlsManager.m_OldJoyState         = ControlsManager.m_NewJoyState;
        ControlsManager.m_bJoyJustInitialised = false;
    } else {
        ControlsManager.m_OldJoyState = previousNewJoyState;
    }

    RsPadEventHandler(RsEvent::rsPADBUTTONUP, &padID);

    RsPadEventHandler(RsEvent::rsPADBUTTONUP, &padID);
    RsPadEventHandler(RsEvent::rsPADBUTTONDOWN, &padID);
    CPad& pad = *CPad::GetPad(padID);

    // Sticks: dead zone 0.3, pos * 128, per-pad invert / swap options of the controller menu
    const auto Stick = [&](int32 device, int16& outSwapped, int16& outNormal, bool inverted, bool swapped) {
        int32 v = 0;
        if (input::StickToGame(device, inverted, v)) {
            (swapped ? outSwapped : outNormal) = (int16)v;
        }
    };
    auto& fm = FrontEndMenuManager;
    // left stick
    Stick(j.lX, pad.PCTempJoyState.LeftStickY, pad.PCTempJoyState.LeftStickX, fm.m_bInvertPadX1, fm.m_bSwapPadAxis1);
    Stick(j.lY, pad.PCTempJoyState.LeftStickX, pad.PCTempJoyState.LeftStickY, fm.m_bInvertPadY1, fm.m_bSwapPadAxis1);
    // right stick
    Stick(j.lZ, pad.PCTempJoyState.RightStickY, pad.PCTempJoyState.RightStickX, fm.m_bInvertPadX2, fm.m_bSwapPadAxis2);
    Stick(j.lRz, pad.PCTempJoyState.RightStickX, pad.PCTempJoyState.RightStickY, fm.m_bInvertPadY2, fm.m_bSwapPadAxis2);
}

//--------------------------------------------------------------------------------------------------
// Injector commands
//--------------------------------------------------------------------------------------------------
bool InputCmd(const char* name, const std::string& args) {
    if (!std::strcmp(name, "padattach")) { // a virtual Xbox-style gamepad: the whole SDL gamepad path (hot plug event, mapping, polling) is the one a real pad takes
        if (s_VirtualJoy) {
            return true;
        }
        // SDL drops joystick input while its window has no keyboard focus (an agent / test run has none): the virtual pad is a test device, let it through
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type     = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes    = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name     = "NOTSA virtual gamepad";
        s_VirtualId   = SDL_AttachVirtualJoystick(&desc);
        s_VirtualJoy  = s_VirtualId ? SDL_OpenJoystick(s_VirtualId) : nullptr;
        NOTSA_LOG_INFO("[pad-cmd] padattach: id {} joystick {} ({})", (unsigned)s_VirtualId, (void*)s_VirtualJoy, s_VirtualId ? "ok" : SDL_GetError());
        return true;
    }
    if (!std::strcmp(name, "padbtn")) { // padbtn:NAME:1|0
        char n[16] = {};
        int  down  = 1;
        std::sscanf(args.c_str(), "%15[^:]:%d", n, &down);
        const auto b = ButtonByName(n);
        if (s_VirtualJoy && b != SDL_GAMEPAD_BUTTON_INVALID) {
            SDL_SetJoystickVirtualButton(s_VirtualJoy, (int)b, down != 0);
        } else {
            NOTSA_LOG_WARN("[pad-cmd] padbtn {}: no virtual pad or unknown button", args);
        }
        return true;
    }
    if (!std::strcmp(name, "padaxis")) { // padaxis:NAME:-32768..32767
        char n[16] = {};
        int  v     = 0;
        std::sscanf(args.c_str(), "%15[^:]:%d", n, &v);
        for (size_t i = 0; i < std::size(kAxis); i++) {
            if (!std::strcmp(n, kAxis[i]) && s_VirtualJoy) {
                SDL_SetJoystickVirtualAxis(s_VirtualJoy, (int)i, (Sint16)std::clamp(v, -32768, 32767));
                return true;
            }
        }
        NOTSA_LOG_WARN("[pad-cmd] padaxis {}: no virtual pad or unknown axis", args);
        return true;
    }
    if (!std::strcmp(name, "padstat")) {
        const auto* p = CPad::GetPad(0);
        const auto& s = p->NewState;
        NOTSA_LOG_INFO("[pad-cmd] padstat attached={} state: cross={} circle={} square={} triangle={} start={} select={} L1={} R1={} L2={} R2={} dpad u/d/l/r={}/{}/{}/{} lstick x/y={}/{} rstick x/y={}/{} menu={} screen={} item={}",
            (int)AllValidWinJoys.JoyStickNum[0].bJoyAttachedToPort, s.ButtonCross, s.ButtonCircle, s.ButtonSquare, s.ButtonTriangle, s.Start, s.Select, s.LeftShoulder1, s.RightShoulder1, s.LeftShoulder2, s.RightShoulder2,
            s.DPadUp, s.DPadDown, s.DPadLeft, s.DPadRight, s.LeftStickX, s.LeftStickY, s.RightStickX, s.RightStickY,
            (int)FrontEndMenuManager.m_bMenuActive, (int)FrontEndMenuManager.m_nCurrentScreen, (int)FrontEndMenuManager.m_nCurrentScreenItem);
        return true;
    }
    if (!std::strcmp(name, "menustat")) {
        NOTSA_LOG_INFO("[pad-cmd] menustat active={} screen={} item={} mouse={},{}", (int)FrontEndMenuManager.m_bMenuActive, (int)FrontEndMenuManager.m_nCurrentScreen, (int)FrontEndMenuManager.m_nCurrentScreenItem,
            (int)FrontEndMenuManager.m_nMousePosWinX, (int)FrontEndMenuManager.m_nMousePosWinY);
        return true;
    }
    return false;
}
} // namespace notsa::portable
