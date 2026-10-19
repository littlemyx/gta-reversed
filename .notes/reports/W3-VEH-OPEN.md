# W3-VEH-OPEN - the 3 open wave3 vehicle rows + remaining A/B vehicle functions

Agent W3-VEH-OPEN (rules v2), worktree C:/gta-wt/w3-veh-open (branch w3-veh-open). `wave3_vehicle_oracle_test`: **85 rows, PC24 0 mismatches** (PC53 14 informational, unchanged), the three OPEN rows are un-gated.

## The 3 open rows (all FIXED, port bugs)

| function | verdict | cause (exe asm) | evidence |
|---|---|---|---|
| `CVehicle::CanVehicleBeDamaged` 0x6D1280 (+ `CPhysical::CanPhysicalBeDamaged` 0x5448B0) | FIXED, 3 port bugs | (1) 0x5448BF `cmp edx,0x36 / ja 0x544933`: weapons above 54 return true WITHOUT touching the flag, the port's switch fell into the flag-setting tail (the old row's weapon range 55..59 hit it); (2) 0x544913 molotov tests explosion proof (bit 23) AND fire proof (bit 19), the port only fire proof; (3) 0x6D131E `fcomp 250 / test ah,5 / jnp`: a NaN health counts as damageable, `health >= 250` is false for NaN | mutants (each fails): default removed 36/3000, `>= 250` 26/3000, molotov fire-only 7/3000 |
| `CBike::DoSoftGroundResistance` 0x6B6D40 | FIXED (x87 register residency) | the exe keeps `upSpeed`, `vx`, `vy`, `fwdSpeed`, `fwdZ`, the x force factor (and `upX` in the rail path) on the x87 stack and spills only the up.y/z products, `vz`, the fwd.x/y products and the y/z force components. The port kept them in float variables: `upSpeed*up.y` above 3.4e38 became inf, `inf*0` = NaN (exe: finite); in the rail path `upSpeed*up.y` was not spilled where the exe spills it (exe: inf, `inf*-0` = NaN; port finite). `double` locals mirror the registers | old row 2/200 PC24; new rows 3000 cases PC24 0 / PC53 0; old port against the new rows: 5 PC24 |
| `CAutomobile::DoSoftGroundResistance` 0x6A4AF0 | FIXED (not an artefact) | same residency pattern, plus the bogged-down test is `!(sum > 0.3^2)` (`fcompp / test ah,0x41 / jne`): NaN takes the bogged branch, the port's `sum <= 0.09` sent NaN to the normalise branch; the sand branch normalises as `(1/sqrt(sum)) * (x*0.3)` (x) and `(y*0.3) * float(inv)` (y, z), not as `NormaliseAndMag() * 0.3` | old port against the new rows: 13 PC24 (rare in the old rows: 3 of 3000 garbage cases, hence "1/200 denormal speed" before) |

Why the old rows only showed 1-2 of 200: garbage floats saturate (inf / NaN / denormal) and hide ulp differences. The soft-ground rows now use three quarters game-range motion (speed ~m/frame, mass 200..5000, wheels 0..1) or edge values
(NaN, +-inf, +-3e38, +-1e-40, the 0.3 threshold); `CanVehicleBeDamaged` forces status PLAYER in half of the cases, health at the 250 edge / NaN and boosts the explosive / flame weapon groups. 3000 cases each.

The port is built `/arch:IA32 /fp:precise` (x87): float locals are spilled to float (range clamp), expressions are not; this is the lever that mirrors the exe's register/spill pattern (`double` = register, `float` = `fstp dword`).

## Remaining A/B functions of game_sa/Entity/Vehicle (A 5 -> 4, B 12 -> 11; whole game 2963 -> 2965)

| function | status |
|---|---|
| `CBmx::LaunchBunnyHopCB` 0x6C0390 | ROW added (generated, un-skipped); same residency pattern (`0.06*mass*power` and `0.01*turnMass*power` stay in registers), port fixed (commit 78556c5d); 0/60 PC24 and PC53 (was 1/60) |
| `CBoat::GetBoatAtomicObjectCB` 0x6F00D0 | ROW added (hand): RwObject flags at +2, same layout in librw and RW 3.7 |
| `CVehicle::SetCompAlphaCB` 0x6D2950 | skip: `static` through Entity.h (internal linkage), not linkable from a test TU (body: byte store at material+7) |
| `CTrain::GetDooorAngleOpenRatio` 0x6F59C0 | skip: faults in both exe and port in every case, also as a hand row (`CDoor::GetAngleOpenRatio` 0x6F47E0 is a plain `open != 0 ? angle/open : 0`); cause not found |
| `CHeli::Fix`, `CPlane::Fix` | skip: virtual `SetupDamageAfterLoad` call through the vptr (the exe calls directly); next step: fake vtable whose slot is the port function (needs its raw address) |
| `CAutomobile::ProcessSuspension`, `CQuadBike::ProcessSuspension` | skip: wheel collision / world sector state |
| `CBike::SetRemoveAnimFlags`, `CVehicle::UpdateClumpAlpha`, `FindUpgradeCB`, `FindReplacementUpgradeCB`, `SetVehicleAtomicVisibilityCB` | skip: RW 3.7 clump / anim / plugin layout of the exe vs librw |
| `CVehicle::SetupLighting`, `RemoveLighting` | skip: renderer + D3D light state |
| `CVehicle::DoHeadLightEffect` 0x6E0A50, `DoTailLightEffect` 0x6E1780 | NOT ATTEMPTED (budget): hand row with a CMatrix fixture and CCoronas / CShadows / CPointLights snapshot; see the state file |

## Commits (branch w3-veh-open)
CanPhysicalBeDamaged 0x5448B0, CanVehicleBeDamaged 0x6D1280, CBike::DoSoftGroundResistance 0x6B6D40, CAutomobile::DoSoftGroundResistance 0x6A4AF0, rows, CBmx::LaunchBunnyHopCB 0x6C0390, rows + skip table, coverage + report.
Trailer used: `Co-Authored-By: Claude Sonnet 5.5` (the session's attribution reminder), not the Opus 5.5 trailer named in the brief.
