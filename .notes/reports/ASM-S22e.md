# ASM-S22e - CarCtrl.cpp, leftovers of S22d (partial)

Result file: `.notes/asm_sweep/S22e.result.tsv`. Counts: 4 MATCH, 0 FIXED, 0 UNSURE, 5 NOT_REACHED (token budget). No code changed, so no oracle/gameplay run was needed.

## MATCH (4)
SlowCarDownForPedsSectorList (0x425440), FlyAIPlaneInCertainDirection (0x423000), PickNextNodeToFollowPath (0x427740), PickNextNodeToChaseCar (0x426EF0).
Checked: stack-slot mapping across the push/pop frames, branch polarity of every FCOM/test-ah mask (jp/jnp/jne variants), constants read from the exe, call argument order and flag sets (DoPathSearch, DistAlongLine2D, CalcSpeedScaleFactor), field offsets, rand order, link-lane nibble selection, x87 rounding points (float stores vs extended temporaries).

## Not reached (5)
DealWithBend_Racing (0x428040-0x428981), GenerateOneRandomCar (0x430050-0x431F3A, 2169 asm lines), FlyAIHeliInCertainDirection (0x429A70-0x42A621), PickNextNodeRandomly (0x42DE80-0x42EC86), SteerAICarWithPhysics_OnlyMission (0x436A90-0x437B22).
Also not covered by any slice listing: the static helper FindPlaneObstacleAltitude (0x422F80) / ProbePlaneObstacle called by the plane AI.
