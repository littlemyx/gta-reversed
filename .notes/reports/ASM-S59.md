# ASM-S59 - 2026-10-30

40 rows (S59.tsv): 20 MATCH, 5 DEVIATION (fixed; plus the task-0 AddScriptSphere fix outside the table), 0 UNSURE, 15 NOT_REACHED. Table: S59.result.tsv.

## Task 0 - script-thing arrays (TheScripts.cpp)
Checked the Add / Remove of every array against the exe: AddScriptSphere 0x483B30, RemoveScriptSphere 0x483BA0, AddScriptEffectSystem 0x492F90, RemoveScriptEffectSystem 0x492FD0 (searchlight / checkpoint were fixed in S58b), GetNewUniqueScriptThingIndex 0x483720 / GetActualScriptThingIndex 0x4839A0 (sphere, effect, searchlight, checkpoint, sequence cases).
- RemoveScriptSphere / RemoveScriptEffectSystem: MATCH (the exe clears the used byte and the dword at +4 only; the unique-id word at +2 is kept; the port does the same).
- AddScriptSphere: DEVIATION, fixed (8a433e6b3). The exe pushes thing type 0 (sphere) for GetNewUniqueScriptThingIndex; the port passed SCRIPT_THING_EFFECT_SYSTEM, so the handle carried (and bumped) the id of effect slot `idx` and the sphere's own m_nUniqueId never advanced; the returned handle could not be resolved by GetActualScriptThingIndex(SPHERE).
- Sequence tasks / fires / 2d effects / decision makers / ped groups: Add / Remove live outside TheScripts.cpp (Group13_15.cpp, Fire.cpp, Scripted2dEffects.cpp, DecisionMakerTypes.cpp, PedGroups.cpp); only the id lookups in TheScripts.cpp were compared (match).

## Fixes
- CStuckCarCheck::Process (0x465680, 2426d51b4): stuck distance is dx*dx + (dy*dy + dz*dz) (not CVector::Magnitude's (x*x + y*y) + z*z); the warp-node search FindNthNodeClosestToCoors uses the vehicle's CURRENT position (exe 0x46585C), the port used the record's LastPos (stale between check intervals). AttemptToWarpVehicle (0x463A60): MATCH.
- CStreamedScripts::Initialise (0x470660, 51c35d0a6): also zeroes m_nLargestExternalSize (+0xA40) and the script count (+0xA44). The other 10 functions MATCH.
- CShopping::LoadPrices (0x49B8D0, a6a55d4be): ms_numPrices was incremented by the for-loop increment expression, which also ran on every skipped comment / empty line (shopping.dat has comment lines inside the sections): price slots were wasted and the table shifted. nameTag is a plain zero-padded strncpy(,8) (strncpy_s rejects an 8-char tag). LoadShop, IncrementStat, Init, Save, Load: MATCH.
- CStats::UpdateStatsWhenOnMotorBike (0x55CD60, 49f673927): the time step is the _ftol-truncated ms integer (the port used the float step), the skill-up test is strict `limit*1000 < counter` (NaN goes on), speed is the x87 extended sqrt.
- CStats::UpdateRespectStat (0x55BC50, b5a99235a): territory clamp `!(t >= 0)`; the rest (both threshold arms, -99 handling, sum order) matches.
- MATCH, no change: UpdateStatsWhenCycling / Driving / Flying / WeaponHit.

## Not reached
Stats: GetStatID, PopulateFavoriteRadioStationList, GetFullFavoriteRadioStationList, BuildStatLine, CheckForStatsMessage, LoadStatUpdateConditions, LoadActionReactionStats, FindCriminalRatingString, ConstructStatLine, DisplayScriptStatUpdateMessage, UpdateSexAppealStat, Init, SetNewRecordStat, Save, Load.

## Gate
StandaloneRelease (gta_reversed.exe) and StandaloneRW (wave2_oracle_test) built after the last edit. wave2_oracle_test (RW_EXE_ORACLE = GTA_EXE = the compact exe): 1142 functions, PC24 0 / 0, PC53 0 / 0. Regress `missions combat` (NOTSA_STANDALONE_NOVSYNC=1, -b the worktree StandaloneRelease): 25 PASS, 0 FAIL. PortableWin64 compile (main checkout's compile_commands with sources / PCH redirected to the worktree, output in scratch) of TheScripts.cpp, StuckCarCheck.cpp, StreamedScripts.cpp, Shopping.cpp, Stats.cpp: 0 errors. v6 pre-commit grep: only the `(double)` float-constant compares already used by the neighbouring Driving / Flying code.
