# ASM-S22f - CarCtrl.cpp, leftovers of S22e (partial)

Result file: `.notes/asm_sweep/S22f.result.tsv`. Counts: 3 MATCH, 0 FIXED, 0 UNSURE, 3 NOT_REACHED (token budget). No code changed, so no oracle/gameplay run was needed.

## MATCH (3)
FindPlaneObstacleAltitude + ProbePlaneObstacle (0x422E10-0x422FF5), PickNextNodeRandomly (0x42DE80-0x42EC86), DealWithBend_Racing (0x428040-0x428981).
Checked: x87 rounding points (float stores vs extended temporaries), FCOM/test-ah mask polarity (incl. NaN paths), link lane nibbles (opposite = bits 0-2, same = bits 3-5), 3-pass node selection and sequence index formula, inlined SwitchVehicleToRealPhysics fields, ap field offsets (0x390..0x3e0), dir*0.01*sign order, 256/60/100/11/0.1 gates, CalcSpeedScaleFactor and FindIntersection2Lines argument order, rand order.

## Not reached (3)
FlyAIHeliInCertainDirection (0x429A70-0x42A621), SteerAICarWithPhysics_OnlyMission (0x436A90-0x437B22), GenerateOneRandomCar (0x430050-0x431F3A, 2169 asm lines). Disassemblies are in C:/gta-scratch/asm-s22e/{heli,onlymission,genrand}.asm.
