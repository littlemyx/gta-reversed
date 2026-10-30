# ASM-S64b - 2026-10-30
S64b: 31 rows: 3 MATCH (FireInstantHitFromCar, FireInstantHitFromCar2, DoWeaponEffect), 8 DEVIATION (fixed; FireInstantHit only a NaN-only compare), 20 NOT_REACHED (17 VehicleRecording rows, ComputeAnimDoorOffsets, VehicleSaveStructure::Extract, TakePhotograph which already carried a NOTSA-AUDIT, LaserScopeDot which has no exe callers and is a port stub). Table: S64b.result.tsv. All fixes are in source/game_sa/Weapon.cpp.

Fixes:
- FireFromCar 0x73FA20: the exe accepts weapon state READY or FIRING (port: only READY), so a car weapon could not fire again while still FIRING.
- DoDoomAiming 0x73CDC0: candidates are the entities the shooter CAN see (port skipped exactly those: inverted CanSeeEntity result); FindObjectsInRange max 15 (port 16); 3D distance sqrt(dz^2 + dist2D^2) (port std::hypot); radius sums z,y,x.
- DoDriveByAutoAiming 0x73D720: all five distances sum the squares z,y,x (radius, plane/heli and owner score terms, aim scale); the plane/heli distance clamp keeps NaN.
- DoTankDoomAiming 0x73D1E0: search radius and slope divisor sum z,y,x.
- FireAreaEffect 0x73E800: flamethrower fire start is `rand() & 3 == 2` (1 in 4; port 33% RandomBool) with ONE scalar rand for the distance (port: CVector::Random = 3 rands); the no-target direction goes through degrees and the float constant 0.017453216 (0x872C60); a player ped's pitch direction is normalised.
- GenerateDamageEvent 0x73A530: CEventDamage::m_bAnimAdded (+0x25 bit 2) is set by the shot-partial and the default anim arms (the port never set it); the shot arm only SetCurrentTime(0) (port Start()); the default arm does not touch the playing flag; null-safe task checks; NaN health still adds the event.
- SetUpPelletCol 0x73C710: pellet radius is rand in [0.2 depth, depth] (port [0, 0.8 depth]); the building tmp axis tests |normal.x| (port z); right = hitDir x tmp for non-building victims (port had tmp x hitDir = opposite sign); length is a plain sqrt (port NormaliseAndMag = 1/recip); position offset (-depth * fwd) * dot with dot summed z,y,x.
- FireInstantHit 0x73FB10: only a NaN-only compare (fWeaponSpreadRate NaN takes the single-impact path). The rest of the ~1700 instructions (every compare, the muzzle-flash jump table, rand order, pellet loop, water test, event construction) matches.

Open note from S64 (FireProjectile adds the forward vector to the CALLER's origin in place): all callers checked - Vehicle fire ordnance (local const origin, unused afterwards), Fire() (shotOrigin is not read after FireProjectile; fxPos is used only before the switch), TaskSimpleUseGun (by-value lambda params), TaskSimpleThrowProjectile (firePos unused after Fire), TaskSimpleGangDriveBy (guns go through FireInstantHit). The in-place write is not observable: no change made.

Open (NOT_REACHED): VehicleRecording (17 rows), VehicleAnimGroupData::ComputeAnimDoorOffsets, VehicleSaveStructure::Extract, TakePhotograph, LaserScopeDot.
Tests: Release + StandaloneRW rebuilt after the last edit; wave2_oracle_test (1142 functions): PC24 0/0, PC53 0/0; combat regress PASS=14 (first run, no flake); v6 grep clean on every commit; PortableWin64 compile of Weapon.cpp (x64 cl, flags from the earlier S64 pw.bat): 0 errors.
