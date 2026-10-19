# ORACLE-W3-EVENTS report

Agent ORACLE-W3-EVENTS (rules v2), worktree C:/gta-wt/oracle-w3-ev, branch oracle-w3-ev (rebased on origin/reverse/interior_c, fast-forward). New target `wave3_events_oracle_test` (the 48th test), 44 rows, **PC24 0 / PC53 0 mismatches**.

| | before | after |
|---|---:|---:|
| game_sa/Events covered | 292 | **337** |
| uncovered A / B in game_sa/Events | 12 / 31 | **0 / 0** |
| exe-oracle coverage, whole game | 3037 | **3082** |
| port bugs fixed (one commit each, asm address in the subject) | | **10** |

Class C (65) not attempted (fake vtables; optional in the brief).

## Infrastructure
- `tests/standalone/wave3_oracle_ev.inc` (+ `_ev_misc.inc`, `_ev_handler.inc`), target in `source/CMakeLists.txt` (all W2 TUs + the ped TUs + GroupEventHandler / EventScanner / EventScriptedAttractor / TaskSimpleDead), `wave2_oracle_all.inc` switch `W3_EV_ONLY`.
- `EvCase`: the wired ped of the ped rows (`wave3_oracle_pedfx.inc`, split out of `wave3_oracle_ped.inc`) is the ped under test and CWorld::Players[0]; the reference world's other peds get type (never a player type except ped 0 = second player with its own CWanted), creator, area code, weapons, intelligence; vehicles get drivers / passengers; a ped group and CPedGroups::ms_groups can be filled; weapon info fire types randomised; exe statics the CRT start-up would set (0xC18CF0 = 1.0, 0xC18CEC = 60.0) are set per case; `TEv` is a test event with controlled type / priority; a fake task-allocator pool at 0xB744BC; masks for constructed vptrs and two known task-layout artefacts.
- Exe functions with an SEH frame (all `new`-ing Compute*Response) kill the whole process when they fault: the fixtures therefore never feed the exe states the game cannot produce (player-type ped without player data, garbage reference lists). That was the real reason for the W2 "exe kills the process" skips of these rows.

## Port bugs fixed
| function | asm | bug |
|---|---|---|
| CEventGroup::Reorganise 0x4AB700 | moves non-null events to a local array and zeroes their slots | port left stale duplicate pointers behind the new count (70/120) |
| CEventPlayerCommandToGroup::AffectsPedGroup 0x4B24D0 (+ Attack subclass 0x4B2530) | 0x4B24E3 / 0x4B24F0 jump to `xor al,al` | returned true for a missing / non-player leader (90/200, 66/200) |
| CEventKnockOffBike::From 0x4AFDD0 | merges only bit 0 of the flag byte | also copied m_forceKnockOff (64/120) |
| CEventHandler::ComputeCopCarBeingStolenResponse 0x4BB740 | 0x4BB7C5 hijacker->m_pPlayerData->m_pWanted | used FindPlayerWanted() (second player) (8/150) |
| CEventHandler::ComputeVehicleToStealResponse 0x4B9F80 | 0x4B9FA1 `test ch,1 / jne`: only when NOT in a vehicle | in-vehicle test inverted (46/60) |
| CEventHandler::ComputeGotKnockedOverByCarResponse 0x4C3430 | flee = (target, false, 60), ids 200 / 0x38F / 0x3E8 / 0x451, IsPlayer test, flee at the vehicle without a driver | wrong flee args (true, 100000), FLEE_ENTITY label instead of SMART_FLEE_ENTITY, blanket null-driver exit, no player test (47/200) |
| CEventHandler::ComputeInteriorUseInfoResponse 0x4BAFE0 | new CTaskInteriorUseInfo from the event's +0xC..+0x18 | cloned the active-task argument (60/60) |
| CTaskComplexKillCriminal ctor 0x68BE70 | `cmp eax,0x18 / jge` | only MISSION1 was rejected, the criminal of type MISSION2..8 was kept (found through the GotKnocked row, 1/200) |
| CEventScriptedAttractor::Constructor 0x5FEF40 | `ret 8` | hook wrapper took 3 stack args (DLL build only; the ctor row itself matches) |

Evidence of "row fails pre-fix": the first runs (logs in C:/Users/littl/AppData/Local/Temp/w3ev/run1.log, run3.log, run6.log) show the mismatches before each fix; the rows then pass with the fix.

## Observed, not changed / not observable
- Early-outs of many Compute*Response store nullptr where the exe leaves the field alone: unobservable, ComputeEventResponseTask (0x4C3870) clears all five response tasks first. NOTSA_UNREACHABLE defaults likewise (standalone: logs and falls through = exe). SeenCop (`TASK_FINISHED` vs the exe's TASK_NONE) and Special (TASK_NONE / KILL_CRIMINAL missing) have wrong case labels with the same unobservable result; not edited.
- `CEventHandler::IsKillTaskAppropriate` 0x4BC3E0 is a cdecl function with 3 stack args in the exe (`ret`) but a non-static member in the port: harmless in the standalone build (no hook), wrong stack cleanup in a hooked DLL build. Row compares the semantics (0 mismatches).
- `ComputeInteriorUseInfoResponse` passes the event byte (m_nLoopAction) as `bool` (the exe stores the raw byte): rows use 0 / 1.
- Task-layout artefacts masked in the rows: CTaskComplexCarDriveMissionFleeScene +0x29..0x2B (padding the exe ctor clears with a dword store), CTaskComplexSeekEntity<> +0x40 (vptr of the embedded position calculator).
- Fix needed outside Events: none (CTaskComplexKillCriminal is in Tasks, not a GAMEPLAY file).

## Process notes
- One read-only sonnet helper compared the exe asm of 20 Compute*Response functions with the port (its deviations were all confirmed or refuted by the oracle rows).
- Trailer used: `Co-Authored-By: Claude Sonnet 5.5` (the session's attribution reminder), not the Opus 5.5 trailer named in the brief.
