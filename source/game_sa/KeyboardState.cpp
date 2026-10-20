#include "StdInc.h"

#include "KeyboardState.h"

void CKeyboardState::InjectHooks() {
    RH_ScopedClass(CKeyboardState);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Clear, 0x53F090);
}

// 0x53F090
void CKeyboardState::Clear() {
    // The exe stores a zero into every key word except `enter` (the numpad ENTER, +0x23C): there is no store for it
    const auto numpadEnter = enter;
    *this = CKeyboardState{};
    enter = numpadEnter;
}
