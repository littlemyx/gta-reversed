# ASM-S97 (+ S96b) - CMenuManager / CMenuSystem, 2026-10-30
S97: 34 rows: 5 FIXED (4 commits), 5 MATCH, 24 NOT_REACHED (sweep budget exhausted). S96b: 8 rows left UNSURE (not read, same reason). Tables: S97.result.tsv, S96b.result.tsv.
Fixes (exe address, one line each):
- 7215374ad CMenuManager::CheckSliderMovement (0x573440-0x57363C): brightness step is ftol(LR*24.19f + b) (0x865258), so one step down is 25 not 24; draw-distance step is the float 0x3D5FFFFF (exe 0x865250), one ulp under 7/128.
- 8a23886aa CMenuManager::CheckMissionPackValidMenu (0x57D720-0x57D85E): the TEXT.GXT handle is closed on the success path too.
- 16424838d CMenuManager::CheckCodesForControls (0x57DB20-0x57DDD8): invalid-key list is F1-F3, F9, PAD5, RALT, LWIN, RWIN (port lacked F9/PAD5/RALT); an invalid key returns without resetting the redefine state; m_MenuIsAbleToQuit (joystick combo action) returns early without binding.
- b8360c953 CMenuSystem::InputStandardMenu / InputGridMenu (0x580800, 0x580BD0): the paused accept key is only the Return key state (0xB733EA/0xB7317A), the port also accepted the pad-enter key.
Matched: CheckForMenuClosing, DisplaySlider (rect corner order not decoded), CheckRedefineControlInput, ProcessUserInput, CMenuSystem::Input.
Open (not compared): all other S97 rows (UserInput 0x57FD70 is hand-written in the port and needs a full pass, ProcessMenuOptions/ProcessPCMenuOptions, Print*, MenuSystem Display*/Process/ActivateItems) and all of S96b (CLoadingScreen::Update has no caller; the CMenuManager Draw* functions).
Notes: CheckForMenuClosing keeps the deliberate FastLoader guard around AudioEngine.Service. CheckCodesForControls with type OPTIONAL_EXTRA_KEY (1) binds like keyboard in the port, the exe binds nothing; no caller passes it.
Tests: StandaloneRelease builds; combat+render_vmode regress NOT run (waited 30 min behind the gate gpu.lock reservation, killed my queued run: regress left to the gate). wave2_oracle_test and PortableWin64 compile not run (no x87d/signature change; frontend not oracle-covered).
Tools: C:/gta-scratch/asm-s97/ann.py annotates a disassembly with CMenuManager field names, string and float constants.
