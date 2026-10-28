# ASM-S19i (Vehicle.cpp rows of S19h/S19i; 53 rows)

Partial. Task 0 (NaN forms of the S19h functions, Constructor ordnance index) done; 1 row compared (ProcessBoatControl, FIXED); 52 rows NOT_REACHED (budget). Result table: S19i.result.tsv. Continue with DoBladeCollision 0x6E2E50, FireFixedMachineGuns 0x73DF00, AddDamagedVehicleParticles 0x6D2A80, AddWaterSplashParticles 0x6DDF60, AddSingleWheelParticles 0x6DE880, SetDriver/RemoveDriver 0x6D16A0/0x6D1950, then the ctor/dtor/upgrade/render rows. The first part of ProcessBoatControl (0x6DBCE0-0x6DBF0A buoyancy preamble, ApplyMoveForce scaling, ApplyBoatWaterResistance call, turn-res block) was only checked at the compare level.

## Task 0 (commit f5e8f752)
| Function | Change |
|---|---|
| AddExhaustParticles 0x6DE240 | `!(rand*(gas+1.1) > 2.5)` returns (NaN returns); alpha = `raw < 0 ? 0 : raw` (NaN passes) |
| ApplyBoatWaterResistance 0x6D2740 | fchs only for an ordered negative; `!(z > 0)` takes the blended z factor (NaN too) |
| SpecialEntityCalcCollisionSteps 0x6D0E90 | `!(sq >= 0.16)` returns 1, `!(sq > 0.32)` takes the 2.5 factor, longest direction = `a > b ? a : b` (NaN -> right operand) |
| ProcessDrivingAnims 0x6DF4A0 | skill ladder `>=100`, `>=50`, else bad (NaN skill = bad); slow = `!(speed > 0.4)`; steer branch `!(steer > 0)` with factor -1/0.61 (0xBFD1D608) instead of fabs |
| SetupSuspensionLines 0x6CF1A0 | bound radius max keeps the min magnitude only if strictly greater |
| CVehicle::Constructor 0x6D5F10 | explicit `m_nOrdnanceCycleIndex = 0` (exe `and byte [0x4A9], 0xF0`) |
Checked and equal to the exe incl. NaN: SetTowLink ratio gate, AddExhaustParticles distance/dot/water/gas>0.5 compares, SetupSuspensionLines min.z and radius max, ProcessDrivingAnims blend/gas/lookback/steer==0 compares, the clamp.

## ProcessBoatControl 0x6DBCE0 (commit c17aeea5)
1. `|gas| <= 0.01` (and NaN) takes the slowing-down path (`jne 0x6DC940`); the port used `< 0.01`, so exactly 0.01 skipped the traction-loss block.
2. Steer force: 5 successive vector*float products (depth, gas, 40, engine acceleration, mass), each component rounded after every factor; the port multiplied by one scalar chain. Traction-loss force `(T * depth) * dt` (port: `T * (depth * dt)`). Turn force: scalar `dt/traction*loss*sin*-0.75` first, then `right * scalar` (port: `right*dt/traction...` per component).
3. Skimmer 2nd-steer timer: reset only for `time > angle` (port `angle <= time` also at equality), time converted by fild (exact integer), `time + 300` with one rounding.
4. NaN forms: submerged gate (>= and NaN), gas <= 0.05 force zero, move-speed gate (`je` only for ordered |gas| > 0.05), AddWakePoint speed vs the 64-bit 0.05 (float == 0.05f now counts), engine-in-water compare, thrust depth `depth > 1 ? 1 : depth^2`, fabs(steer) as `fchs` only for an ordered negative, `max(0,z)` keeps z for >= 0/NaN.

## Tests
StandaloneRelease gta_reversed.exe rebuilt (0 errors). Pre-commit grep clean. wave2_oracle_test (RW_EXE_ORACLE and GTA_EXE = gta_sa_compact.exe): 1142 functions, 0 mismatches PC24 and PC53. gameplay_regress combat (-b worktree StandaloneRelease): PASS=14. Not run: soak, PortableWin64.
