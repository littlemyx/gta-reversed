# ASM-S22c - CarCtrl.cpp slice, the 18 largest functions, asm sweep

Start 19:42, end ~19:55 (WEST 2026-10-26). Budget-limited (tokens), not time-limited.
Result file: `.notes/asm_sweep/S22c.result.tsv`. Counts: 3 MATCH, 0 FIXED, 0 UNSURE, 15 NOT_REACHED (slice is `partial`).

## MATCH (3)
GenerateCarCreationCoors2 (0x424210), UpdateCarOnRails (0x436540), SteerAICarWithPhysics (0x437C20). Checked: branch polarity of every
FCOMP / `test ah` mask (NaN paths), the mission and temp-action jump tables (decoded from the exe), constants (read from the exe, incl. the
doubles 0.05 / +-0.5), call argument order (CalcCurvePoint, DragCarToPoint, AreNodesLoadedForArea, Find2NodesForCarCreation, ThisNodeWillLeadIntoADeadEnd),
output slots of the steer/gas/brake/handbrake locals, bit offsets of the node flags (+0x18/+0x19), rand() order, float-vs-extended rounding points.

## Not reached (15)
GenerateOneRandomCar (2169 asm lines - needs its own agent), PickNextNodeRandomly (926), PickNextNodeToChaseCar, PickNextNodeToFollowPath, DealWithBend_Racing,
CreateConvoy, CreatePoliceChase, FlyAIPlaneInCertainDirection, FlyAIHeliToTarget_FixedOrientation, PossiblyRemoveVehicle, SlowCarDownForPedsSectorList,
FlyAIHeliInCertainDirection, DragCarToPoint, SteerAICarWithPhysicsFollowPreRecordedPath, SteerAICarWithPhysics_OnlyMission.

## Gate
No source change, so no build / oracle / soak was needed.
