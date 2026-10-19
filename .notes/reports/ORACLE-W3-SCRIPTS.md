# ORACLE-W3-SCRIPTS report

Agent ORACLE-W3-SCRIPTS (rules v3), start 23:31, end see the hand-back (2026-10-19 / 20), worktree C:/gta-wt/oracle-w3-scripts (branch oracle-w3-scripts, base 44060285).
New target `wave3_scripts_oracle_test` (W2 harness: deterministic arena, whole exe data image + the scratch block compared after every case, entity reference world compared through the Ext hooks), rows in
`tests/standalone/wave3_oracle_scripts*.inc` (`.inc` = fixture + helpers + parameter-stream emitter, `_a` = CRunningScript, `_b` = CTheScripts, `_c` = CScriptsForBrains / CScripted2dEffects / tUserList).
52 rows (+52 covered sites; every A/B function of the brief except ScriptDebugLine3D), PC24 0 mismatches (exit 0). PC53 (CRT default precision, informational) differs in ProcessAllSearchLights (x87 summation order) only.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 3905 | **3957** |
| game_sa/Scripts covered | 23 | **72** |
| uncovered A / B in game_sa/Scripts | A 21, B 29 | **A 1, B 0** |
| port bugs fixed (one commit each, asm address + pre-fix failing count in the subject) | | **8 commits, 11 functions** |

## Fixture
`CTheScripts::ScriptSpace` (0xA49960), `LocalVariablesForCurrentMission` (0xA48960) and all other CTheScripts statics are at their exe addresses on both sides. `FillVars` fills every variable space with half small values (valid array indices) and half full-range garbage.
A scratch block holds four `CRunningScript`s (the `this` / `newScript` / list members), a script buffer with SCM-encoded parameter streams (`Em`: static int32 / int8 / int16 / float, global / local variable, global / local array = ArrayAccess records with global or local index variables, unknown types, terminator),
a `CScriptsForBrains` copy, a fake ped / vehicle / object, entities with matrices (and no RW object, so the exe's UpdateRwMatrix / UpdateRwFrame return before any RenderWare call) and out parameters.
Entities / buildings / objects come from `logic_oracle_refs.inc` (fake pools at the exe's pool globals); ScriptsArray (0xA8B430, 70 scripts) gets random idle / active double-linked lists for the StartNewScript rows; the SCM head chunks (global-var, used-objects, script-file-info) are written for ReadObjectNames / ReadMultiScriptFileOffsets.
Unlike earlier waves the port's STL is hardened in the oracle build: an out-of-range `std::array` index asserts. Rows therefore use in-range indices only (what a running game uses); where the exe's out-of-range behaviour is a deliberate part of a function (full tables) the port is compared on its Release behaviour (see AddToListOfConnectedLodObjects).

## Port bugs fixed (rows failed pre-fix; counts are PC24 failing cases of the first clean run on the unfixed code)
| function | asm | bug | pre-fix failing |
|---|---|---|---|
| CTheScripts::ProcessAllSearchLights | 0x4939F0 | every state used PathCoord2 as the destination (exe: STATE_1/4 PathCoord1, STATE_2 PathCoord2, STATE_3 followed entity), `m_Target *= dir * speed` instead of `+=`, NOTSA_UNREACHABLE for STATE_0 (exe: no-op), bulb / housing RotateX (exe: SetRotateX 0x59B060, so the port turned the already rotated matrix again every frame) | 2463/2500 |
| CTheScripts::AddToListOfConnectedLodObjects | 0x470980 | exe takes the LAST slot with a == -1 (its free index is overwritten per free slot) and does nothing on a full table; port: first free slot, store through end() when full | 2030/3000 |
| CTheScripts::AddToWaitingForScriptBrainArray | 0x46AB60 | exe takes the LAST empty slot of the 150, port the first | 169/3000 |
| CTheScripts::AddToInvisibilitySwapArray | 0x481200 | exe returns when `visible` and the entity is not in the array; port added it | 100/3000 |
| CScripted2dEffects::GetIndex / GetEffectPairs | 0x6F9F60 / 0x6FA840 | exe matches the element address exactly; port's range test + pointer difference accepted a pointer into the middle of an effect | 104/600, 181/600 |
| CTheScripts::InitialiseSpecialAnimGroupsAttachedToCharModels / InitialiseSpecialAnimGroup | 0x474730 / 0x474710 | exe stores only m_nModelID = -1 and m_IfpName[0] = 0; port copied a temporary's uninitialised name[1..15] | 100/100, 200/200 |
| CRunningScript::Init | 0x4648E0 | exe copies 7 bytes "noname\0"; the port's strcpy_s filled m_szName[7] (debug CRT: 0xFE). Also failed every StartNewScript row through Init | 200/200 (+ 1499/1500, 1784/2500, 199/200) |
| CTheScripts::IsVehicleStopped | 0x4861F0 | step = `ts < old ? ts : old` (jp on the fcomp: NaN -> old), std::min picks ts for a NaN old step | 1/1500 |
| CRunningScript::ReadParametersForNewlyStartedScript | 0x464500 | float arguments are a raw dword copy in the exe; the port's float return quieted a signalling NaN payload (NaN payload only) | 6/2500 |

## Rows that pass without a change (exe-exact already)
CRunningScript::GetPointerToLocalVariable / GetPointerToLocalArrayElement / ReadArrayInformation / GetIndexOfGlobalVariable / AddScriptToList / RemoveScriptFromList / UpdateCompareFlag / UpdatePC / GetPadState / GetCorrectPedModelIndexForEmergencyServiceType,
CTheScripts::InitialiseAllConnectLodObjects / InitialiseConnectLodObjects / ReadObjectNamesFromScript / ReadMultiScriptFileOffsetsFromScript / AddToBuildingSwapArray / AddToSwitchJumpTable / AddToVehicleModelsBlockedByScript / RemoveFromWaitingForScriptBrainArray / CleanUpThisObject /
ClearAllVehicleModelsBlockedByScript / ClearAllSuppressedCarModels / WipeLocalVariableMemoryForMissionScript / ReinitialiseSwitchStatementData / GetScriptIndexFromPointer / GetUniqueScriptThingIndex / StartNewScript x2 / StartTestScript / PrintListSizes / IsPlayerOnAMission /
HasCarModelBeenSuppressed / HasVehicleModelBeenBlockedByScript / UndoEntityInvisibilitySettings / IsPedStopped (non-player peds) / CheckStreamedScriptVersion / UseSwitchJumpTable,
CScriptsForBrains::Init (padding bytes masked) / AddNewStreamedScriptBrainForCodeUse / IsObjectWithinBrainActivationRange, tUserList::IsPedTypeInList, CScripted2dEffects::Init.

## Not covered / limits
- CTheScripts::ScriptDebugLine3D (0x485DE0, the only remaining A): the exe body is a single `ret`; the port appends to a TU-static line buffer outside the exe image that only RenderTheScriptDebugLines (not called by the port's frame either) reads. Nothing observable to compare.
- IsPedStopped: players are excluded (the player branch queries animation associations of the ped's clump; no clump in the fixture). The in-vehicle / on-foot logic is covered.
- CScriptsForBrains::Init: the 3 padding bytes after m_bBrainActive of every entry are masked (the port zero-fills a value-initialised temporary, the exe leaves them) - harmless.
- Out-of-range inputs not exercised (hardened std::array asserts in the port, plain pointer arithmetic / memory access in the exe): local indices >= 34 (>= 1024 for a mission script), ScriptSpace offsets >= 269000, ReadObjectNames `usedObjects` count >= 395, a 76th AddToSwitchJumpTable (overwrites NumberOfEntriesInSwitchTable itself), a full VehicleModelsBlockedByScript (writes SuppressedVehicleModels[0]), brain index -1 in IsObjectWithinBrainActivationRange, and an unknown parameter type in GetIndexOfGlobalVariable (UB in both).
- The port's `GetLocal(VarLoc)` truncates `base + idx * size` to 16 bits in GetPointerToLocalArrayElement where the exe adds in 32 bits; only differs for sums >= 65536, impossible for the 34 / 1024 local slots, not changed.

## Gates
New target `wave3_scripts_oracle_test` 52 rows PC24 0 mismatches, exit 0. Targets linking the changed TUs: logic_oracle_test (TheScripts.cpp) and the script_oracle_* family (they carry their own stubs of CRunningScript / CTheScripts members and do not link the changed TUs): see the hand-back.
