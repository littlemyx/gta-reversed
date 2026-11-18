# ASM-P4S8c
Part A (review, all CONFIRMED against the 1.0 exe, no changes): ProcessPlayerWeapon 6fae2687b; TASK_KILL_CHAR_ON_FOOT/_WHILE_DUCKING + GivePedScriptedTask(int32) b4f4a81ef; IS_CHAR_IN_AREA_ON_FOOT_3D/IN_CAR_3D; PRINT_STRING_IN_STRING_NOW; COMMAND_LINE; ADD_CONTINUOUS_SOUND; SWITCH_RUBBISH.
Part B (CVehicle::FlyingControl 0x6D85F0): Model1345 (0x6D8B1D-0x6D9869) and Drag/clamp/epilogue (0x6DA900-0x6DAEE1) MATCH; no code changes. FlyingControlModel268 (0x6D986A-0x6DA8FF) NOT_REACHED (budget).
No source changes, no build needed.
