# ORACLE-W3-INPUT report

Agent ORACLE-W3-INPUT (rules v3), start 06:01, end ~07:20 (2026-10-20), worktree C:/gta-wt/oracle-w3-input (branch oracle-w3-input, base 47482960). New target `wave3_input_oracle_test` (W2 harness: deterministic arena, whole exe data image + a 128 KB scratch block compared after every case), rows in `tests/standalone/wave3_oracle_input{,_a,_b}.inc` (a = Pad.cpp, b = ControllerConfigManager.cpp), skip list `tests/standalone/tools/w3_input_skip.tsv`. Development switches: `W3I_DIV=n` divides the case counts, `W3I_SCALE=n` multiplies them; `./wave3_input_oracle_test.exe <substring>...` runs matching rows only (`W2_FAULTLOG=1` prints the fault records).
**67 functions (all 47 + 18 A/B functions of the two files, plus AffectPadFromKeyBoard and ReinitControls which are class C), PC24 0 mismatches.**

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 4206 | **4275** (67 new rows; +2 are the host stubs `Patch`ed at 0x53F2D0 / 0x747CD0, which the address search counts) |
| ControllerConfigManager.cpp uncovered A / B | 47 | **0** |
| Pad.cpp uncovered A / B | 18 | **0** |
| port bugs fixed (one commit each) | | **13 commits** |

## Harness
- `ControlsManager` is the exe's global (0xB70198, NOTSA_ORACLE_ADDRESS_GLOBALS) and is refilled for every case: 59 actions x 4 controller types with keys from the real value sets (keyboard: blank 1056, special keys 1000..1055, ASCII, raw bytes; mouse 0..7; joystick 0..16), set orders 0..4 (a few 5/6), clean rows, press-both-down flags, joystick button arrays (`DIJOYSTATE2`), `m_ButtonStates`.
- Pad world: both pads (`NewState` / `OldState` / `PCTemp*State` with real axis values 0, +-127/128, 255 and garbage), the keyboard state copies (int16 words 0 / 255, half the keys down), the mouse state copies, the function-local statics around them (0xB72CB0 .. 0xB73700), `CTimer` time, the three camera / look flags (0xB6EC2E, 0x8CD782, 0xB73403).
- Player world for the driving / first person dispatch: a fake `CPlayerPed` (bInVehicle, m_pVehicle, m_nPedState, m_pAttachedTo) + fake vehicle (driver, type) in the scratch block through `CWorld::Players[0]`; camera mode / active cam, `FrontEndMenuManager` (control method, menu active, text language), `AllValidWinJoys` (including the 0x3427 / 0x1190 gamepad).
- `GTATranslateShiftKey` (0x747CD0, GetKeyState through the IAT) and `WinInput::GetMouseSetUp` (0x53F2D0, DirectInput) are patched on the exe side with host stubs (nothing / all seven mouse buttons present: the SDL3 port always binds every button, `isForcedMouseBlinding`). Linked besides the two files: ControllerState / KeyboardState / MouseControllerState, GxtChar, MenuManager (the global `FrontEndMenuManager` must be the exe one).
- Calls use the exe's ABI as the asm shows it (`ret N` checked for each function): an ABI mismatch of the port shows up as an ESP check failure.

## Port bugs fixed (rows failed pre-fix; counts = failing PC24 cases of the first run on the unfixed code at half the default case count, logs C:/gta-scratch/w3input/pre1.log)
| function | asm | bug | pre-fix failing |
|---|---|---|---|
| CControllerConfigManager::DeleteMatchingVehicle_3rdPersonControls | 0x531BC0 | the exe pops three arguments (unused action, button, type: `ret 0xC`), the port two; callers with the exe's argument list read the action as the button | 185/750 |
| CKeyboardState::Clear (CPad::ClearKeyBoardHistory / Clear / Initialise) | 0x53F090 | the exe has no store for the `enter` word (numpad ENTER, +0x23C), the port zeroed the whole object | 27/30, 45/50, 143/150 |
| CPad::ReconcileTwoControllersInput | 0x53F530 | the result state is a function-local static at 0xB736C0; the port's lived elsewhere (parity of the data image) | 1500/1500 |
| CPad::Update | 0x541C40 | the steering buffer is shifted right (`buf[i] = buf[i-1]`, i = 9..1); the port rotated it left | 750/750 (with the previous) |
| CPad::StartShake_Train | 0x53FA70 | three bugs: takes the point by value (`ret 8`; the port a reference); returns when the player IS in a train (the port: when not); distance in x/y only (the port added the camera z) | 98/1000 (all three) |
| CControllerConfigManager::SetMouseButtonAssociatedWithAction | 0x52F590 | no ResetSettingOrder call in the exe (the key variant 0x530490 has it): the orders of the mouse bindings (InitDefaultControlConfigMouse, ReinitControls) differed | 534/750; InitDefaultControlConfigMouse 400/400 |
| CControllerConfigManager::GetIsMouseButtonJustUp | 0x52F110 | 'just released' is `!new && old` (the port: `new && !old`); the exe's table maps key 2 (middle) to the RIGHT byte and key 3 to the MIDDLE byte | 308/750 |
| CControllerConfigManager::UpdateJoyInConfigMenus_ButtonDown / Up | 0x52DAB0 / 0x52DC20 | START (button 12) is written only for pad 2, every other button for both pads; the port's condition skipped pad 2 entirely and START nowhere | 209/750, 168/750 |
| CControllerConfigManager Driving / FirstAndThirdPersonOnly (UpdateJoy_ButtonDown) | 0x52F7B0 / 0x52FAB0 | left-stick actions (GO_*, VEHICLE_STEER*) test only the both-down flag; the port also tested the opposite stick value (new CheckAndSetStickLeft) | 63/1500, 75/1500 (8/1500) |
| CControllerConfigManager::AffectPadFromMouse | 0x5314A0 | release handlers only for a non-blank button and a closed menu | 99/750 (with the left-stick commit) |
| CControllerConfigManager::AffectPadFromKeyBoard | 0x531140 | the port ran the release handler inside the press branch (resetting NETWORK_TALK / VEHICLE_MOUSELOOK right after setting them); the exe runs press(key 0), press(key 1), then `if (!down(0)) release(0) else if (!down(1)) release(1)` | 326/750 (with the left-stick commit) |

Not changed on purpose: `ReinitControls` (SDL3 port: mouse always bound, joystick 16 buttons; the row compares with the exe under the stubs above), `InitDefaultControlConfigMouse` (isForcedMouseBlinding: the row uses a set-up with all buttons present).

## Rows that pass without a change
Joystick edges (GetJoyButtonJustUp / JustDown, UpdateJoyButtonState), InitialiseControllerActionNameArray, Constructor, ClearSimButtonPressCheckers, MakeControllerActionsBlank, InitDefaultControlConfiguration (all four language branches), InitDefaultControlConfigJoyPad, GetActionType, GetNumOfSettingsForAction, GetControllerKeyAssociatedWithAction, GetIsActionAButtonCombo, GetMouseButtonAssociatedWithAction, GetIsKeyBlank, GetIsKeyboardKeyDown / JustDown, GetIsMouseButtonDown / Up, StoreMouseButtonState, ResetSettingOrder, SetControllerKeyAssociatedWithAction, ClearSettingsAssociatedWithAction, DeleteMatching{Common, 3rdPerson, 1rst3rdPerson, Vehicle, 1rstPerson}Controls, DeleteMatchingActionInitiators, ThirdPersonOnly / FirstPersonOnly / AllStates / VehicleAndThirdPersonOnly / ButtonUp_All_Player_States, the two DebugStuff no-ops, UpdateJoy_ButtonDown, AffectControllerStateOn_ButtonUp; Pad: SetTouched, GetTouchedTimeDelta, StopShaking, ProcessPCSpecificStuff, ClearMouseHistory, GetPedWalkLeftRight / UpDown (ped), AimWeaponLeftRight / UpDown, LookAroundLeftRight / UpDown (ped), GetCurrentKeyPressed.

## Observed, not changed
- `WinInput::GetMouseSetUp` (0x53F2D0) takes the result pointer on the stack with `ret 4` in the exe (a trailing bool stays on the stack as the second argument of InitDefaultControlConfigMouse, `ret 8`); the port returns the struct by value with the cdecl convention. No exe caller is left in the port; WinInput is not part of this target.
- Port asserts / NOTSA_UNREACHABLE for inputs the exe tolerates: GetIsMouseButton* with a key above 7, GetIsKeyBlank / GetControllerState with a type above 3, GetControllerStateJoyStick with a button above 16, CheckMouseButton. The rows use the value sets the game produces.
- IsKeyboardKeyDownInState: the exe compares the key as a signed int (`key < 0xFF`), the port unsigned; keys come from a table of values below 0x10000, rows use those.
- `CPad::iCurrHornHistory` outside 0..4 (exe: unsigned compare after the increment, port: `% 5`); the game keeps it in range, rows use 0..4.

## Not covered (tests/standalone/tools/w3_input_skip.tsv)
Class D / C: SaveSettings / LoadSettings (file IO), the seven GXT text getters (TheText lookups), CPad::UpdateMouse / UpdatePads / ProcessPad / GetMouseState / DIReleaseMouse (DirectInput / SDL3 / ImGui). None is class A or B.

## Gates
`wave3_input_oracle_test`: 67 functions, PC24 0 mismatches (PC53 0), exit 0 (final tree, log C:/gta-scratch/w3input/full1.log; no case skipped as "faulted identically"). Targets linking the changed TUs (Pad.cpp / Pad.h), built and run in this worktree on the same tree: wave3_ui_oracle_test (77 functions, PC24 0), wave3_camera_oracle_test (34, PC24 0), wave3_scripts_oracle_test (52, PC24 0), gameplay_oracle_test (313, PC24 0 excluding NaN payloads, exit 0; 57 strict NaN-payload-only as before). ControllerConfigManager.cpp / KeyboardState.cpp are linked by no other test target. The orchestrator runs the full run_all_tests.
