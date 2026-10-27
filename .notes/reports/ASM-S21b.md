# ASM-S21b (PlayerPed.cpp, 9 functions left from S21) - 2026-10-27
Result: 5 compared (1 FIXED, 4 MATCH), 0 UNSURE, 4 NOT REACHED (token budget: ProcessAnimGroups, SetRealMoveAnim, FindWeaponLockOnTarget, ProcessControl; table in S21b.result.tsv). Slice stays PARTIAL.
Fix (exe address, commit):
- KeepAreaAroundPlayerClear (0x60C1E0): the CTaskComplexSmartFleeEntity position-change tolerance is `push [0xC18CF0]` (the runtime fEntityPosChangeThreshold, scripts can change it); the port passed a literal 1.0f. ebd91f73.
MATCH: MakeThisPedJoinOurGroup (0x60C840; blip colour bytes 46 C8 00 FF and separation 120.0 read from the exe data), Compute3rdPersonMouseTarget (0x60B650), ProcessWeaponSwitch (0x60D850; all cam-mode ids and slot loops), FindNextWeaponLockOnTarget (0x60E530; keeps the original's stale-register TWRM argument bug).
Consumer audit: the fix only changes one constructor argument inside the function; no layout/signature change.
Gate: Release build ok; logic_oracle_test (the only oracle target containing PlayerPed.cpp; 277 checks, 0 failures, 0 mismatches; no touched function is covered by it); soak.sh RESULT: OK.
