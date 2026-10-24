#pragma once
/*
 * MAC-W2 (W-2): SDL3 gamepad -> the game's pad layer (the role of DirectInput + CPad::ProcessPad in the Win32 build), plus the injector commands for input checks.
 * Implementation: app_pad.cpp. Used only from `#ifdef NOTSA_PORTABLE` blocks (WinMain.cpp, SDLWrapper.cpp, Pad.cpp). The pure tables are in app_input_map.h.
 */
#ifdef NOTSA_PORTABLE
#include <string>

union SDL_Event;

namespace notsa::portable {
//! Opens the gamepads that are already connected (up to two, pad 1 / pad 2) and fills AllValidWinJoys; call after SDL_Init and BEFORE ControlsManager.ReinitControls
void PadInit();
//! SDL_EVENT_GAMEPAD_ADDED / REMOVED (hot plug). true = consumed
bool PadOnEvent(const SDL_Event& e);
//! CPad::ProcessPad(padID): polls the gamepad, fills ControlsManager.m_New/OldJoyState, raises the pad button events and the stick axes of CPad::PCTempJoyState
void PadProcess(int padID);
void PadShutdown();
//! Injector commands (cmd:padattach | padbtn:NAME:0|1 | padaxis:NAME:VALUE | padstat | menustat); false = not mine
bool InputCmd(const char* name, const std::string& args);
}
#endif
