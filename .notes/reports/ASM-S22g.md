# ASM-S22g - CarCtrl.cpp: FlyAIHeliInCertainDirection, SteerAICarWithPhysics_OnlyMission (GenerateOneRandomCar not reached)

Result file: `.notes/asm_sweep/S22g.result.tsv`. Counts: 1 DEVIATION (fixed), 1 MATCH, 0 UNSURE, 1 NOT_REACHED (token budget).

## FlyAIHeliInCertainDirection (0x429A70-0x42A621) - 1 fix (105f2782)
Whole function compared. Only deviation: `(1.0f - factor) * steeringUpDown` in the "climbing quickly" block; the exe computes `1 - factor` in extended precision (fld 1.0; fsub st1; fmul [0x990]) while the port rounded it to float first. Fixed with `(x87d)1.0f`.
Everything else matches: news-heli slow check, 500 ms timer compare (CTimer prev at 0xB7CB78 vs now at 0xB7CB84), 3 probes (down-ahead, level, side R/L with distR rounded to float / distL kept extended), field_9AC update polarity, throttle noise and clamp, skid wrap loops (2pi), pitch to destination, 2.12207 level-out factor, forward-speed limit, speed boost, brake when field_9B8.

## SteerAICarWithPhysics_OnlyMission (0x436A90-0x437B22) - MATCH
Jump table decoded from the exe (index bytes at 0x437BD4, targets at 0x437B24) and compared with the port's switch for all 69 mission ids (NONE / EMERGENCY_STOP / STOP_FOREVER share the same stop values, WAITFORDELETION / PROTECTION_* and ids > 0x44 return). Per-case bodies compared: RAMPLAYER_CLOSE, RAMCAR/KILLPED_CLOSE, FOLLOWCAR_CLOSE, APPROACHPLAYER_CLOSE, BLOCK* (arg order), STRAIGHTLINE / SLOWLY_DRIVE, ASTHECROWSWIMS, ESCORT_* offsets, DO_DRIVEBY_CLOSE and all pure dispatch cases.

## Not reached
GenerateOneRandomCar (0x430050-0x431F3A, 2169 asm lines; disassembly in C:/gta-scratch/asm-s22e/genrand.asm and .raw): no instruction compared, whole range open.
