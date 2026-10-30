# ASM-S96 (CLoadingScreen + first part of CMenuManager) - 2026-10-30
Result: 45 rows: 8 FIXED (6 commits), 27 MATCH, 1 NOT_REACHED (CLoadingScreen::Update, no caller), 8 UNSURE (Draw* functions not compared: budget). Table in S96.result.tsv.
Fixes (exe address, one line each):
- 0cbe76735 CLoadingScreen::LoadSplashes (0x5900B0-0x590214): the splash order only shuffled loadsc1..6 with std::mt19937; the exe picks 6 of loadsc1..loadsc14 without replacement via CRT rand() (14 calls, index = rand*n>>15).
- b878b0507 CLoadingScreen::RenderLoadingBar (0x590370): m_TimeBarAppeared is stamped only after the bar was drawn, with GetClockTime(false); DoPCScreenChange amp tests only the FadeOut flag.
- 32889b5ce CMenuManager::SwitchToNewScreen (0x573680): SwitchToNewScreen(0) goes to the target of the currently selected entry (port reset the selection and used entry 0).
- 554a3e4fe CMenuManager::Initialise (0x5744D0): map-origin clamp computed x - x - K (= -K) and re-associated sums; exe subtracts the overshoot, bounds from the entry origin.
- 9f9ce03c3 CMenuManager::LoadAllTextures/UnloadTextures (0x572EC0, 0x574630): exe order FRONTEN1 (0x21000, TidyUp), FRONTEN_pc (0x3000, TidyUp), FRONTEN2 (0x4C000, no TidyUp); the port loaded an extra frontend3 TXD and had the 0x3000/0x4C000 reservations swapped.
- 756c21bad CMenuManager::LoadSettings (0x57C8F0): invalid/old settings file -> exe clears m_nCurrentRwSubsystem (port cleared m_nPrefsVideoMode) and keeps the previous language (port forced American).
Notes: GetClockTime differs by 1ulp only above 2^24 ms uptime (exe rounds once); NewChunkLoaded differs only for the FastLoader NoCopyright path (cur==-1, exe would index splash[-1]); JumpToGenericMessageScreen uses snprintf(8) vs exe strncpy(8), equal for keys up to 7 chars.
Open (UNSURE, not read): DrawBackground 0x57B750, DrawStandardMenus 0x5794A0, DrawWindow 0x573EE0, DrawWindowedText 0x578F50, DrawControllerScreenExtraText 0x57D8D0, DrawControllerBound 0x57E6E0, DrawControllerSetupScreen 0x57F300 (all in Frontend/MenuManager_Draw.cpp).
Tools: C:/gta-scratch/asm-s96/annot.py annotates a disassembly with CMenuManager field names (layout from cl /d1reportSingleClassLayoutCMenuManager).
