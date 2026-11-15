# ASM-P4S1 (C++ suspects from the TS port) - 2026-11-15
Result: 4 TS suspect rows + the other conditional / argument NOP handlers (~40 ids) checked; 2 commits FIXED, 3 rows MATCH, 1 UNSURE (ADD_CONTINUOUS_SOUND, unused). Table in P4S1.result.tsv.
Fixes:
- e249d05f6 script NOP handlers: IS_PS2_KEYBOARD_KEY_PRESSED / _JUST_PRESSED (exe 0x46DDCA / 0x46DE0E), IS_PLAYER_IN_SHORTCUT_TAXI (0x48EE39), IS_RECORDING_GOING_ON_FOR_CAR (0x4971B8): CollectParameters(1) + compare flag false (the exe keyboard stubs 0x571980/0x571970 return 0); IS_AUDIO_BUILD (0x474315): flag false; WRITE_DEBUG*, VIEW_/WATCH_*_VARIABLE, SAVE_*_TO_DEBUG_FILE: arguments consumed as the exe does (text label / raw 128 bytes / CollectParameters) instead of uint32 reads.
- 9306ec878 CShopping::RestoreClothesState (0x49B240): copy the stored state into the player clothes desc (the port only assigned a local pointer).
MATCH: CTaskSimpleSay::ProcessPed 0x48E440 (exe starts the timer unguarded = StartIfNotAlready), CShadows::StoreShadowToBeRendered(type) 0x707930 (exe literally passes 1/2 as the type), the other conditional NOPs (jump table default, or equal arg counts).
TS action per row:
- IS_PS2_KEYBOARD_KEY_PRESSED / _JUST_PRESSED, IS_PLAYER_IN_SHORTCUT_TAXI, IS_RECORDING_GOING_ON_FOR_CAR: none (C++ now equals the exe and the TS); EXE_NOP_ARGS 'f' entries and the N3 normalisation rule become redundant after the next snapshot.
- IS_AUDIO_BUILD: TS must follow the new C++/exe: read no args, UpdateCompareFlag(false).
- WRITE_DEBUG*, VIEW_/WATCH_*, SAVE_*_TO_DEBUG_FILE: none (TS already follows the exe).
- CShopping::RestoreClothesState: none (TS follows the exe; C++ now copies the clothes).
- CTaskSimpleSay::ProcessPed (StartIfNotAlready): none (C++ = exe).
- CShadows::StoreShadowToBeRendered(type): none (C++ = exe; the 1/2 type arguments are what the exe passes, not a bug).
Gate: see the report of the agent.
