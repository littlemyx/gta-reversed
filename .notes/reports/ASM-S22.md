# ASM-S22 - CarCtrl.cpp slice (55 functions), asm sweep

Start 18:05, end ~18:35 (WEST 2026-10-26). Budget-limited (tokens), not time-limited.
Result file: `.notes/asm_sweep/S22.result.tsv`. Counts: 17 MATCH, 4 FIXED, 0 UNSURE, 34 NOT_REACHED (slice is `partial`).

## Fixes (one commit each, asm range in the message)
- JoinCarWithRoadAccordingToMission 0x432CB0 (6d0f101c): the "target entity" missions (RAMCAR/BLOCKCAR/PROTECTION/ESCORT/FOLLOWCAR/KILLPED/DRIVEBY, 27 values) skipped the exe's `m_TargetEntity == nullptr` early exit (0x432D31 `je 0x432D63`); the port dereferenced null.
- RemoveCarsIfThePoolGetsFull 0x4322B0 (1b61d3c9): initial closest distance is 1e7 (0x4B189680), not FLT_MAX (unobservable in practice).
- SlowCarOnRailsDownForTrafficAndLights 0x434790 (eb37ef5e): when m_fMaxTrafficSpeed == m_speed the exe returns without CAutoPilot::ModifySpeed; the port called ModifySpeed(max) every update (it restarts the speed interpolation: field_C / m_nSpeedScaleFactor) - affects every rail-bound AI car cruising at the traffic speed (trains/planes-on-rails paths use this).
- PossiblyFireHSMissile 0x429600 (09f94990): the 0.8 cone test used a copy of the direction taken BEFORE Normalise (|dir| * cos > 0.8), the exe dots the normalised vector - HS missiles of AI planes/helis fired at far wider angles than in the exe.

## Notes
- Many CarCtrl functions already carry x87-order comments from earlier lockstep work; the logic-level bugs were in the less-audited ones.
- UNSURE-minor: PickNextNodeAccordingStrategy (0x432B10) passes FindPlayerVehicle(-1,0) / the target entity as a 5th argument to PickNextNodeToChaseCar (0x432BAE, 0x432C19); the port has a 4-arg signature and calls FindPlayerVehicle nowhere. The 5th arg looks unread in 0x426EF0 (its stack slots are reused as scratch) - not verified.
- Consumer audit: none of the fixes changes shared state layout/meaning.
- Not reached (34): PickNextNodeToChaseCar, PickNextNodeToFollowPath, DealWithBend_Racing, CreateConvoy, CreatePoliceChase, GenerateCarCreationCoors2, GenerateOneEmergencyServicesCar, IsThisAnAppropriateNode, CreateCarForScript, GetNewVehicleDependingOnCarModel, GenerateOneRandomCar (2169 asm lines), FlyAI* / SteerAIHeli* / SteerAIBoat*, TriggerDogFightMoves, GetAIPlaneToDoDogFight, GetAIHeliToAttackPlayer, GenerateEmergencyServicesCar, ReconsiderRoute, FindLinksToGoWithTheseNodes, ScanForPedDanger, PickNextNodeRandomly, DragCarToPoint, SlowCarDownForPedsSectorList, PossiblyRemoveVehicle, UpdateCarOnRails, SteerAICarWithPhysics*, ...; see the result tsv.

## Gate (rules v5)
- Release build of the worktree: OK (exit 0, 1185 steps).
- Oracle targets: no CarCtrl oracle test exists (tests/standalone has none) - nothing to run.
- soak.sh 3 (NOVSYNC, new game, route 406 s): RESULT OK, 0 errors, 24.9 fps steady.
