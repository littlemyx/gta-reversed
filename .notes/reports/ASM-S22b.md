# ASM-S22b - CarCtrl.cpp slice, second half (34 functions), asm sweep

Start 18:29, end ~18:45 (WEST 2026-10-26). Budget-limited (tokens), not time-limited.
Result file: `.notes/asm_sweep/S22b.result.tsv`. Counts: 16 MATCH, 0 FIXED, 0 UNSURE, 18 NOT_REACHED (slice is `partial`).

## MATCH (16)
GetAIHeliToAttackPlayer, GetAIPlaneToDoDogFight, FireHeliRocketsAtTarget, TriggerDogFightMoves, SteerAIHeliToLand,
FindLinksToGoWithTheseNodes, ReconsiderRoute, ScanForPedDanger, IsThisAnAppropriateNode, SteerAIHeliAsPoliceHeli,
GenerateEmergencyServicesCar, GenerateOneEmergencyServicesCar, GetNewVehicleDependingOnCarModel, CreateCarForScript,
SteerAIBoatWithPhysicsAttackingPlayer, SteerAIHeliToFollowEntity. Checked: branch polarity (FCOMP/test ah masks, NaN paths), jump
tables (decoded from the exe: mission, behaviour-type and model tables), constants (read from the exe), call argument order
(DoPathSearch, FindObjectsKindaColliding, ProcessVerticalLine, SlowCarDownForPedsSectorList), bit offsets of the flag fields, rand() order.

## Notes
- SteerAIHeliToFollowEntity / SteerAIHeliTowardsTargetCoors: the exe passes a 4th stack arg (1) to FlyAIHeliToTarget_FixedOrientation; the callee takes 3 (4th unread) - unobservable.
- No source change, so no build / oracle / soak was needed for this slice (nothing to gate).
- Not reached (18): PickNextNodeToChaseCar, PickNextNodeToFollowPath, DealWithBend_Racing, CreateConvoy, CreatePoliceChase, GenerateCarCreationCoors2,
  GenerateOneRandomCar (2169 asm lines), FlyAIPlaneInCertainDirection, FlyAIHeliToTarget_FixedOrientation, PossiblyRemoveVehicle,
  SlowCarDownForPedsSectorList, FlyAIHeliInCertainDirection, PickNextNodeRandomly, DragCarToPoint, SteerAICarWithPhysicsFollowPreRecordedPath,
  UpdateCarOnRails, SteerAICarWithPhysics_OnlyMission, SteerAICarWithPhysics.
