# ASM-S59b - 2026-10-30

15 rows (S59b.tsv, CStats display / persistence): 13 MATCH, 2 DEVIATION (fixed), 0 UNSURE. Table: S59b.result.tsv.

## Fixes
- CStats::Save (0x5D3B40, 7f7ba80c2): the exe calls PopulateFavoriteRadioStationList (0x558EC0) between the LastMissionPassedName block and the favourite-radio-station block, so the saved listen times are the current ones from the audio engine; the port saved the copy from the last Init / menu refresh. Block sizes and order (0x148, 0x37C, 0x80, 8, 0x38, 0x190, then 128 displayed bytes at msg+2) were already identical; Load has no populate and matches.
- CStats::DisplayScriptStatUpdateMessage (0x55B980, 1dd063d5c): for the skill / ability stats (fat, stamina, muscle, max health, sex appeal, weapon skills, gambling, driving, armour, energy; flying, lung, bike, cycling, luck) the exe skips when `test ah,0x41` + jnp says value <= 1, so a NaN value shows the message; the port used `value > 1` (NaN skipped). TOTAL_RESPECT, PIMPING (10), the girlfriend progress stats (100) and gang strength (min(FindMaxNumberOfGroupMembers, player limit)) are unconditional as in the port; the stat-group jump tables were decoded from the exe.

## Verified MATCH
Stat message logic (CheckForStatsMessage: morethan stat >= v, lessthan stat <= v, NaN skips; LoadStatUpdateConditions), BuildStatLine (all 10 metric formats, argument order, signed zero-pad test), ConstructStatLine (all 8 page tables, German skip list, the 0x00-0xF3 / 0xFC-0x101 / 0x140-0x151 special stats, weapon / skill / flight rank / gang / radio lines), FindCriminalRatingString (every threshold and key), UpdateSexAppealStat (clamps, class table, 35.0 gate), Init, SetNewRecordStat, GetStatID, Populate / GetFull favourite list, LoadActionReactionStats, Load.

## Gate
StandaloneRelease (gta_reversed.exe) and StandaloneRW (wave2_oracle_test) built after the last edit. wave2_oracle_test (RW_EXE_ORACLE = GTA_EXE = the compact exe): 1142 functions, PC24 0 / 0, PC53 0 / 0. Regress `combat` (NOTSA_STANDALONE_NOVSYNC=1, -b the worktree StandaloneRelease): 14 PASS, 0 FAIL. PortableWin64 compile of Stats.cpp (main checkout compile_commands redirected to the worktree): 0 errors. The two edits add no double / ftol constructs.
