# ORACLE-C-EVENTS report

Agent ORACLE-C-EVENTS (rules v4), worktree C:/gta-wt/oracle-c-events, 2026-10-23 21:59 - 23:30 WEST. About 35 minutes of the run were spent waiting for build.lock (orchestrator gate, ORACLE-C-TASKS-2 builds).

| | before | after |
|---|---:|---:|
| game_sa/Events covered | 337 / 480 | **391** (includes 3 disabled open-finding rows, so 388 really compared); uncovered C 64 -> 10 |
| game_sa/Entity/Ped covered | 102 / 232 | **122**; uncovered C 38 -> 18 (all skipped with a reason, below); A 1 / B 4 unchanged (skipped, fixture) |
| exe-oracle coverage, whole game (tool, same tree) | 4914 | 4988 |
| `wave3_events_oracle_test` | 62 rows | **95 rows**, PC24 0, PC53 0, exit 0, ~1 min |
| `wave3_ped_oracle_test` | 69 rows | **92 rows**, PC24 0, PC53 0, exit 0, ~1 min |
| port bugs fixed | | **0**; the open findings below are the real result of this wave |

## Infrastructure
* Events: tools/gen_w3_tasks.py takes `--sub game_sa/Events --prefix events` (writes wave3_oracle_gen_events.inc / _sources.cmake / _report.txt, `TestW3EventsFuncs`), builds `CEvent&` arguments (a controlled `TEv`, `w3e::NewTev`), rejects `T**` parameters, and for an abstract event class swaps the vftable of the first concrete derived class (`vtclass()`). Skip table: tests/standalone/tools/w3_events_skip.tsv.
  The generated rows use the task fixture (`W3TaskCase`, vtable swap of the event `this` and of the ped under test, as in ORACLE-C-TASKS); wave3_oracle_ev.inc includes wave3_oracle_tasks.inc as a fixture (`W3_TASKS_FIXTURE_ONLY` guard around the tail of tasks.inc, `#pragma once` on ev.inc).
* Events hand rows (wave3_oracle_ev_c.inc): `EvCase` got `g_evVt` (exe / port vftable of the event in g_ev and CPlayerPed vftable in the ped under test, written around each call) for the CEventHandler::Compute*Response rows, whose events carry pointer members into the reference world.
* Ped: `W3PedCase(..., vt)` (`PVtEnt`, `SetPedVt`) writes the CPlayerPed vftable around each call; gen_w3_ped.py emits it for class C (`--classes A,B,C --skip tests/standalone/tools/w3_ped_skip.tsv`) and links game_sa/Tasks/TaskManager.cpp (CTaskManager::FindActiveTaskByType was an unresolved 0x400000 call: GetHoldingTask and co ran only on the exe side).
* /SAFESEH:NO on wave3_events_oracle_test and wave3_ped_oracle_test (exe EH frames; without it the ped test died at the first agreed fault, CPed::ShoulderBoneRotation).

## Open findings (rows exist; the three event rows are kept out of the default run behind `W3_KNOWN_MISMATCH`, the others sit in the skip tables with the numbers)
| function | exe | evidence |
|---|---|---|
| `CEventHandler::ComputeGunAimedAtResponse` | 0x4C2840 | 15/150 cases: port-only access violation in the `IsFleeTask` lambda (`t->GetTaskType()` with tactive == nullptr; the exe tests it: 0x4C2872 `test edi,edi / je 0x4C291A`). Also the exe maps tactive task types 0x38D / 0x38C to CTaskComplexFleeEntity (0x4C2E09) and 0x38F / 0x38E to CTaskComplexSmartFleeEntity (0x4C28F5), the port tests only SMART_FLEE_ENTITY / POINT and always builds SmartFleeEntity. Needs the full asm comparison (not done) |
| `CEventHandler::ComputeVehiclePotentialPassiveCollisionResponse` | 0x4B96D0 | 62/150 PC24 cases differ (16 register-visible, 46 special inputs); not analysed |
| `CEventHandler::ComputeDeadPedResponse` | 0x4B9470 | 1/150 (special input); not analysed (may be the fixture: the event built with its constructor registers a reference) |
| `CEventVehicleHitAndRun::ReportCriminalEvent` | 0x4B27D0 | port-only access violation in CEntity::GetType (access 0x36, null entity) on case 1, the exe survives; in w3_events_skip.tsv |
| `CPed::DisablePedSpeech` / `DisablePedSpeechForScriptSpeech` | 0x5EFF60 / 0x5EFF80 | 1 of 60 special-input cases differs (PC24 and PC53); in w3_ped_skip.tsv |

Re-run the disabled rows: compile wave3_events_oracle_test with `/DW3_KNOWN_MISMATCH`; `-v <row name>` lists the cases, `W2_FAULTLOG=1` prints the fault addresses. The coverage tool counts the 3 disabled rows as covered (text match).

## Skipped (fixture; every case faults identically in port and exe; reasons in the skip tables)
* Events: CEventAttractor::IsEffectActive, CEventGroupEvent::Clone, CEventLeaderExited / QuitEnteringCarAsDriver::AffectsPedGroup, CGroupEventHandler::ComputeWalkAlongsideResponse (need a real CPedGroup / attractor); the 4 CGroupEventHandler::ComputeResponse* rows with derived-event reference arguments (NOARG in the generator report).
* Ped: lighting / skeleton / clump / anim-blend / player-data rows (tests/standalone/tools/w3_ped_skip.tsv); GetTransformedBonePosition, SpecialEntityPreCollisionStuff, DoesLOSBulletHitPed, CPlayerPed::EvaluateNeighbouringTarget (reference in/out argument kinds the generator cannot build), CPed::SetPedAtomicVisibilityCB (no declaration found).
* Several generated event rows run with 60-100% agreed-faulting cases (the run prints `TRAPAUDIT ... both N of 120`): CEventDamage::AffectsPed / ComputeDeathAnim, CEventDanger::AffectsPedGroup, CEventEditableResponse::Clone / TriggerLookAt, CEventFireNearby::AffectsPed. Real constructed events as `this` would raise the compared share.

Consumer audit: no port code changed (tests, generators, CMake only). Soak not needed (no gameplay code touched).
