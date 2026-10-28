# ASM-S19b (Trailer.cpp, Train.cpp, Vehicle.cpp; 105 rows)

Partial. Compared 1 (CTrain::ProcessControl, FIXED), NOT_REACHED 104 (budget). Result table: S19b.result.tsv. Continue in S19c from CTrain::DoTrainGenerationAndRemoval (0x6F7900), then TrainHitStuff (0x6F5CF0), CreateMissionTrain, then the CVehicle per-frame functions in the S19 order.

## Task 0: portable spelling (commit c7d163b5)
13 added lines with a bare `double` in PlayerPed.cpp (a comment), TaskComplexEnterCarAsPassengerWait.cpp, TaskComplexEvasiveDiveAndGetUp.cpp, TaskSimpleSwim.cpp re-spelled with x87d / X87R / X87W the way 9fb52939 did for CWorld::Process (pow results `const auto`, 53-bit, kept for the multiplies). Win32 build bit-identical: `dumpbin /disasm` of the four StandaloneRelease .obj files before and after is identical (no diff). The four objs also compile in PortableWin64 (preset configured in the worktree by copying build/PortableWin64/generators from the main checkout); the full PortableWin64 link was not run. Pre-commit grep on the diff: clean.

## CTrain::ProcessControl 0x6F86A0 (whole function; the node search 0x6F9235-0x6F95D4 only read)
| Deviation | Exe | Fix |
|---|---|---|
| station stop/leave flag loops wrote this->trainFlags in every iteration (bStoppedAtStation, bPassengersCanEnterAndLeave: 3 loops) | 0x6F88D0, 0x6F8910, 0x6F8940: flags of the carriage walked | trainCarriage->trainFlags |
| derail loop set bNotOnARailRoad on this only | 0x6F8DF0: every carriage gets bNotOnARailRoad, force/simple-collision cleared, SetIsStatic(false) | theTrainCarriage->trainFlags |
| speed *= pow(0.99, ts) and *= pow(0.99975, ts) used float pow (rounded before the multiply) | 0x6F8E49, 0x6F8BF2: CRT pow stays 53 bit, one rounding after the multiply | X87W/X87R spelling |
| derail check dot product summed x,y,z (DotProduct is z,y,x) | 0x6F8D93: (z*z + x*x) + y*y | explicit sum |
| m_fMovingSpeed = DistanceBetweenPoints (x,y,z sum) | 0x6F990C: (dz^2 + dy^2) + dx^2, one root | explicit |
| in-water block read vecOldTrainPosition for the speed factor and ApplyMoveForce | 0x6F9CF5-0x6F9DA2: the buoyancy force (4th ProcessBuoyancy out parameter, vecTurnForce in the port) and an unrounded pow for the six speed multiplies | fixed |
Checked and matching: heading/old-position capture, track arrays, both speed branches (player pad gas/brake, AI station timer with 20000/28000/32000 and 10000/18000/22000 limits, FindMaximumSpeedToStopAtStations), forbidden-crossing slowdown, speed integration with the 1/256, 0.002f, 0.006f constants, brake, rail-distance wrap, node walk with idiv, interpolation with *0.125 and *(1/3) constants, z offset, forward/right/up, lighting lerp, move/turn speed, heading wrap, clamps, UpdateRwMatrix/Frame/RemoveAndAdd flags, collision marking, ScanForPedDanger, the non-rail physics branch (force/torque thresholds, fake-physics counter; the `0x424100` call is `xor al,al; ret`, i.e. always false), passenger state machine (nibble counts, rand&3+1). Only NaN-edge compare polarity differences remain (not changed).

Tests: StandaloneRelease gta_reversed.exe builds (1181/1181). wave2_oracle_test (RW_EXE_ORACLE): 1142 functions, 0 mismatches PC24 and PC53. gameplay_regress combat: PASS=14. PortableWin64: the four Task 0 objs and Train.cpp.obj compile.
