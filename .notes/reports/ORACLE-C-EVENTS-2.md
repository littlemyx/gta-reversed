# ORACLE-C-EVENTS-2 report

Agent ORACLE-C-EVENTS-2 (rules v4), worktree C:/gta-wt/oracle-c-events-2, start 2026-10-23 23:30 WEST. Base origin/reverse/interior_c 8cbce9cf (rebased on fa56297e before landing).

| function | exe | bug | pre-fix failures |
|---|---|---|---:|
| `CEventHandler::ComputeGunAimedAtResponse` | 0x4C2840 | (1) port derefs a null tactive and also asks tsimplest, the exe tests only tactive (0x4C2872); (2) exe types 0x38C/0x38D -> CTaskComplexFleeEntity (0x4C2E09), 0x38E/0x38F -> SmartFleeEntity (0x4C28F5), port had only the smart ones; (3) SMART_FLEE_ENTITY (0x4C2B94) and CAR_DRIVE_MISSION_FLEE_SCENE (0x4C2CB8) fall back to SmartFleeEntity, port built FleeEntity; (4) KILL_CRIMINAL (0x4C2D7C): nullptr for a player target, KillCriminal only if IsKillTaskAppropriate, else FleeEntity - port always KillCriminal; (5) IsPlayer exit (0x4C286C) and an existing duck task (0x4C2A1F) leave m_EventResponseTask untouched, port stored nullptr / the duck task | 15/150 (old row); the extended row (fake active task, 16 ids) showed 7, then 1, before the last two fixes |
| `CEventHandler::ComputeVehiclePotentialPassiveCollisionResponse` | 0x4B96D0 | condition inverted: the exe skips when the ped IS in a vehicle (0x4B9703 test ch,1 / jne), port required it; null tsimplest checked before IsGoToTask (0x4B970D); the skipped move state is the event's (e+0x18 == STILL, 0x4B9729), port used the go-to task's; early exits leave the response task untouched | 62/150 |
| `CEventHandler::ComputeDeadPedResponse` | 0x4B9470 | fcomp/test ah,5/jp (0x4B94E4) continues only for dist2 < 0.5625 - port `>=` let a NaN distance through; flee safe distance is [0xC18CEC] = 60.0f (0x4B96A3), port passed 100000; exits do not store (only TASK_NONE stores nullptr) | 1/150 (real bug, not a fixture artefact) |
| `CPed::DisablePedSpeech` / `...ForScriptSpeech` | 0x5EFF60 / 0x5EFF80 | trampolines to CAEPedSpeechAudioEntity, which compares the whole word (`cmp word ptr [esp+4], 0`, 0x4E56DA / 0x4E570A); CPed took a bool (low byte). Now int16. Unreachable from real callers (they pass true/false), but exe-faithful | 1/60 |
| `CEventVehicleHitAndRun::ReportCriminalEvent` | 0x4B27D0 | **artefact**: exe and port code agree instruction for instruction (IsCriminalEvent 0x4B27B0, RegisterCrime 0xA/0xB); the AV came from the garbage `this` of the generated row. Hand row with a real event: 150 cases, 0 mismatches | n/a |

Fixes are one commit each (633f57e9-style subjects name function and address); the tests commit removes the W3_KNOWN_MISMATCH guards and the skip rows.

## Rows / coverage
| | before | after |
|---|---:|---:|
| `wave3_events_oracle_test` | 95 rows, 3 findings disabled | **99 rows**, PC24 0, PC53 0, exit 0, ~1 min (GunAimedAt 300 cases with a fake active task, HitAndRun hand row new) |
| `wave3_ped_oracle_test` | 92 rows | **94 rows** (DisablePedSpeech x2 as hand rows, int16 values 0/1/0x100/-1/0x7FFF), PC24 0, PC53 0 |
| coverage tool, game_sa/Events | 391 / 480 | 392 |
| coverage tool, game_sa/Entity/Ped | 122 / 232 | 124 |
| whole game (tool) | 4988 | 5028 (includes work landed by others) |

Harness: `FakeTact` in wave3_oracle_ev_c.inc (fake task, vftable slot 4 = GetTaskType, same table on both sides); 0xC18CE4 / 0xC18CE8 / 0xC18CEC are all initialised from [0x86F648] by static initialisers (0x852680 / 0x852690), the harness now sets 0xC18CE4 = 60.0f as well as 0xC18CEC.

## Consumer audit
Only response-task values changed, read by CEventHandler::ComputeEventResponseTask callers (dispatch table); no shared state layout, index or flag meaning changed. CPed::DisablePedSpeech* callers (CharacterCommands.cpp, PedToPlayerConversations.cpp, TaskSimpleBeHit.cpp) pass bool -> int16.

## Not done
Item 5 (real constructed events as `this` for the generated rows with 60-100% agreed-faulting cases: CEventDamage::AffectsPed / ComputeDeathAnim, CEventDanger::AffectsPedGroup, CEventEditableResponse::Clone / TriggerLookAt, CEventFireNearby::AffectsPed) - token budget. The extended GunAimedAt row still has 10 of 600 cases faulting identically in port and exe (unresolved callee on both sides), the DisablePedSpeech rows 52 of 120 (StopCurrentSpeech fixture).

## Verification
* wave3_events_oracle_test 99 rows PC24 0 / PC53 0 (exit 0); wave3_ped_oracle_test 94 rows PC24 0 / PC53 0 (exit 0), both on the tree rebased on fa56297e.
* Soak: StandaloneRelease built from this tree, `soak.sh build/StandaloneRelease 3` with NOTSA_STANDALONE_NOVSYNC=1, private user-files dir: RESULT OK, 0 errors, 40700 frames, 24.9 fps (route 393 s). Output C:/gta-soak-out/20261023-235612.
* Not run here: the full run_all_tests (rebase brought only tests/.notes files from ORACLE-C-MODELS); the orchestrator gate covers it.
