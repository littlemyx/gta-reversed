# ASM-S22d - CarCtrl.cpp, the 15 left functions (partial)

Result file: `.notes/asm_sweep/S22d.result.tsv`. Counts: 5 MATCH, 1 FIXED, 0 UNSURE, 9 NOT_REACHED (smaller functions first, stopped at the token budget).

## Fixed (1)
- PossiblyRemoveVehicle (0x424F80, store at 0x42510B): the removal range when the camera looks (almost) straight down is 70.0 (0x428C0000); the port used 75.0. Commit 5d8be9cb.

## MATCH (5)
FlyAIHeliToTarget_FixedOrientation (0x423940), CreatePoliceChase (0x42C2B0), CreateConvoy (0x42C740), SteerAICarWithPhysicsFollowPreRecordedPath (0x432DD0), DragCarToPoint (0x42EC90). Checked: branch polarity of every FCOM/test-ah mask, constants read from the exe (floats and the qword doubles), call argument order and flag sets (ProcessVerticalLine, FindObjectsKindaColliding, SetUpDriverAndPassengersForVehicle, SetTask), field offsets, rand order, the signed `char field_51` compare.

## Not reached (9)
PickNextNodeToChaseCar, PickNextNodeToFollowPath, DealWithBend_Racing, GenerateOneRandomCar (2169 asm lines), FlyAIPlaneInCertainDirection, SlowCarDownForPedsSectorList, FlyAIHeliInCertainDirection, PickNextNodeRandomly (926), SteerAICarWithPhysics_OnlyMission (1357).

## Gate
Release build OK (1185/1185); no CarCtrl oracle target exists; soak.sh 3 min: RESULT: OK.
