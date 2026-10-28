# ASM-S19f (Trailer.cpp, Train.cpp, Vehicle.cpp; 85 rows)

Partial. 8 rows touched (3 FIXED, 5 MATCH); 77 rows NOT_REACHED (budget). Result table: S19f.result.tsv. Continue in S19g from the CVehicle rows in S19f.tsv order after SetupPassenger (Constructor 0x6D5F10 ... FireUnguidedMissile, FireHeatSeakingMissile, GetPlaneWeaponFiringStatus 0x6E3440, DoHeadLightReflectionSingle/Twin 0x6E1440/0x6E1600), then Trailer.

| Function | Verdict |
|---|---|
| CVehicle::DoVehicleLights 0x6E1A60 | FIXED 5fa0c8dd (see below) |
| CVehicle::DoHeadLightReflection 0x6E1720 | FIXED 5fa0c8dd: without TWIN the exe draws the combine's twin shadow, otherwise the single RIGHT shadow unconditionally (stale `al` is not read: the jne at 0x6E1766 jumps past the test); the port required an include flag and picked the left side for bikes |
| CVehicle::FirePlaneGuns 0x6D4D30 | FIXED 35e50167 (see below) |
| CVehicle::DoPlaneGunFireFX 0x6D4AD0 | MATCH (plane 0x9D4 / heli 0xA0C array alloc+null fill, gunflash fx, gunshell dir (0,0.1) size 0.5, light 3.0 / 0.5,0.4,0) |
| CVehicle::DoDriveByShootings 0x741FD0 | MATCH (BMX ==1 fire test, bike/low anim ids 202/203/204, 74/75 or 76/77, 191, cam modes 1/49 + obbe flag, clip reset with movsx, fade -8, blend 16, flags&1 and blend>0.99 returns, +70 ms) |
| CVehicle::SetupPassenger 0x6D1AA0 | MATCH (tail: types 18/19/6/7..16 and criminal with gang type 14..23 skip the model check; driver then seats < seatIdx; port lacks the unreachable `occupant != psgr` guard) |
| CVehicle::PossiblyDropFreeFallBombForPlayer 0x6E07E0 | MATCH |
| CTrain::MarkSurroundingEntitiesForCollisionWithTrain 0x6F6640 | MATCH in PC24: the exe floors the unrounded `v*0.02+60` for the clamp and the float-stored copy for the result; with 24-bit precision both are the same value (sub-ulp only at PC53/64), nothing to change |

## DoVehicleLights deviations fixed
1. The four `m_renderLights` bits are cleared at 0x6E1CCE on every call (also with the engine off); the port left stale bits when it returned early.
2. Alarm dark phase (0x6E1C13) stores forceOn = false as well (override-ON + alarm dark phase drops the force); the zr350 pop-up only runs with the engine on and before the clear.
3. Per-side light state (0x6E1CF1-0x6E1DB6): DISABLE_FRONT/REAR beat the damage state and the left lights need the TWIN flag; the port's `a && b || c && d` kept a disabled side lit on an undamaged automobile and lit the left side without TWIN (affects the directional-light gate).
4. Front order is right main, left main, right secondary, left secondary (the secondaries are inlined copies); the rear stays main/secondary per side.
5. The inlined front-secondary copies (0x6E21D0, 0x6E2569) compute the corona size as `((1-d/150)*angle)*0.4`, the call version (S19e) as `((1-d/150)*0.4)*angle` (1 ulp); new `inlinedSecondary` parameter on DoLightEffectImpl.
6. Front directional light: position is the vehicle's OWN matrix position (not `vehicleMatrix`), fog flag is 1 below speed^2 0.2025 and 0 at/above/NaN (the port had it inverted). Brake directional light: the 4*forward offset uses `vehicleMatrix` forward (the port used the entity's).
7. forceOff paths (0x6E2711-0x6E2739): reflection + point lights are skipped, front/rear run with everything disabled (identical effects once the flags are cleared).

## FirePlaneGuns deviations fixed
1. The exe fires the instant hit first and the FX after it (port: FX first -> rand order of gunshell vs shot swapped).
2. Twin-gun planes (rustler, hydra, tornado): the second gun is `GetPlaneGunsPosition` with x negated, transformed and velocity-offset again (the port comment suspected an original bug but reused the first position); FX ids cycle*2 and cycle*2+1.
3. Pad shake frequency: rustler 224 (port 225), hydra/tornado 128 (port 123).

## Tests
StandaloneRelease gta_reversed.exe rebuilt (Vehicle.cpp). wave2_oracle_test (RW_EXE_ORACLE and GTA_EXE = gta_sa_compact.exe): 1142 functions, 0 mismatches PC24 and PC53. gameplay_regress combat (-b worktree StandaloneRelease): PASS=14. Not run: soak, PortableWin64 (no new double/ftol spellings; pre-commit grep clean).
