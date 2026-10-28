# ASM-S19h (Trailer.cpp, Vehicle.cpp; 63 rows)

Partial. 10 rows touched (4 FIXED, 6 MATCH); 53 rows NOT_REACHED (budget). Result table: S19h.result.tsv. Continue in S19i with the CVehicle rows in S19h.tsv order that are not in the result table as MATCH/FIXED (next: ProcessBoatControl 0x6DBCE0, DoBladeCollision 0x6E2E50, FireFixedMachineGuns 0x73DF00, AddDamagedVehicleParticles 0x6D2A80, AddWaterSplashParticles 0x6DDF60, AddSingleWheelParticles 0x6DE880, SetDriver/RemoveDriver 0x6D16A0/0x6D1950, then the ctor/dtor/upgrade/render rows).

| Function | Verdict |
|---|---|
| CVehicle::AddExhaustParticles 0x6DE240 (rest of the function) | FIXED |
| CTrailer::SetTowLink 0x6CFDF0 | FIXED |
| CVehicle::ApplyBoatWaterResistance 0x6D2740 | FIXED |
| CVehicle::SpecialEntityCalcCollisionSteps 0x6D0E90 | FIXED |
| CTrailer::SetupSuspensionLines 0x6CF1A0, CVehicle::Constructor 0x6D5F10, ProcessDrivingAnims 0x6DF4A0, Shutdown 0x6D0B40, GetSpecialColModel 0x6DF3D0, PreRenderDriverAndPassengers 0x6D3DB0 | MATCH |

## Deviations fixed
1. AddExhaustParticles: the exe stores `z = 0` once and writes only x/y of the particle velocity (`moveSpeed * 30` or `randomFactor * forward`); the port copied all three components, so the exhaust puff on a pitched / airborne vehicle drifted vertically. Everything after the velocity (water test per exhaust, rand() gate with `(gas + 1.1)`, alpha/life clamps, the 2-iteration particle loop with the persistent alpha/size changes, `gas > 0.5 && gear < 3` secondary puff with `rand() & 1`, AddParticle args incl. lighting limit 0.6) matches, stack offsets of every position/velocity/fxPrt argument checked.
2. CTrailer::SetTowLink (no `setMyPosToTowBar`): exe calls `UpdateTractorLink(true, false)` on the towing vehicle (`mov ecx, edi`); the port called it on the trailer (this), whose `m_pVehicleBeingTowed` is null, so the tractor never pulled the freshly attached trailer. The `setMyPosToTowBar` branch (ratio gate -1000, heading of the tractor, hitch/bar delta order, PlaceOnRoadProperly, audio 0x71) matches.
3. ApplyBoatWaterResistance: immersion factor is `((depth * suspensionForceLevel) * depth) * mass * 0.001`; the port computed `depth^2 * ...` (different float rounding).
4. SpecialEntityCalcCollisionSteps: `(|v|^2 * dt) * dt`, the port used `|v|^2 * (dt * dt)`.

## Notes (not changed)
- NaN-only polarity differences remain in a few compares (e.g. `<= 2.5` gas gate, `0.25 - speed >= 0` alpha clamp); unreachable for finite game state.
- CVehicle::Constructor: the exe also clears `m_nOrdnanceCycleIndex` (bits of the 0x4A9 byte) and does not touch 0x51C/0x51D; the port relies on pool memory for the former. No behaviour change observed.

## Tests
StandaloneRelease gta_reversed.exe rebuilt in the worktree. wave2_oracle_test (RW_EXE_ORACLE and GTA_EXE = gta_sa_compact.exe): 1142 functions, 0 mismatches PC24 and PC53. gameplay_regress combat (-b worktree StandaloneRelease): PASS=14. Not run: soak, PortableWin64 (pre-commit grep clean).
