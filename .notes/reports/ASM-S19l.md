# ASM-S19l (Vehicle.cpp rows; 29 rows)

Partial. 15 rows compared: 8 DEVIATION fixed, 6 MATCH, 1 UNSURE (DoHeadLightBeam, render-only); 14 NOT_REACHED (budget). Result table: S19l.result.tsv.
Not reached: Add/RemoveReplacementUpgrade, RemoveWinch, RemoveVehicleUpgrade, AddUpgrade, AddVehicleUpgrade, SetupUpgradesAfterLoad, the six upgrade/atomic callbacks, ProcessBoatControl preamble 0x6DBCE0-0x6DBF0A.

| Function | Result |
|---|---|
| BladeColSectorList 0x6DAF00 | Ped hit: direction is 2D (`CVector2D::Normalise` of this-ped x,y; port used a 3D unit vector); `bIsStanding` is cleared before `ApplyMoveForce`; blood spawns at the PED position (port used the vehicle) with velocity (dir*0.01, 0.01) (port: 3D dir*0.01). `damageMult` was never used: `SetDamagedPieceRecord` intensity = max(100*mass/3000, intensity) * damageMult. The plane-rotor sound / `planeRotorDmgTimeMS` block was gated on the entity being a ped (dead code in the non-ped branch); the exe runs it for non-peds when not in [dmg-2000, dmg] (inclusive). Heli rotor-speed `<= 0.15` NaN polarity, turn-force factor (-0.0005*turnMass) unrounded, rotor-plane dots (x+z+y, z+y+x) and `point - up*dot` in the x87/float mix of the exe. |
| ~CVehicle 0x6E2B40 | Rope lookup key was `this+1`; exe uses `this+0x1D` (`GetRopeID()`), so the winch rope was never removed. The fx-system loop nulled a copy, not the members. |
| SetModelIndex 0x6D6A40 | Jump tables decoded (RC set, weapon models equal). Non-RC models only clear bIsRCVehicle; the port also cleared bCreatedAsPoliceVehicle. |
| ClearVehicleUpgradeFlags 0x6D3210 | Stereo branch needs `RadioType == CIVILIAN` (`test al,al; jne`); port had `!=`, so the bass/stereo upgrade was never removed. |
| CustomCarPlate_TextureCreate 0x6D10E0 | `SetCustomCarPlateText(0)` after creating the plate texture; port set the text from itself (kept it for the next vehicle, overlapping strcpy_s). |
| CreateReplacementAtomic 0x6D3700 | Wheel matrix: exe ORs 0x20003 then `RwMatrixUpdate` clears them; port set them after the update. |
| DoBoatSplashes 0x6DD130 | NaN polarity of the intensity / cruise / fx clamps (`fcom; test ah,0x41; jne` vs `test ah,5; jp`). Everything else (x87 sums, rand order, AddParticle args) equal. |
| DoSunGlare 0x6DD6F0 | Corona id now `NOTSA_PTR_ID32(this)`; arithmetic compared fully, equal. |
| MATCH | Save, Load (+ CVehicleSaveStructure::Construct/Extract offsets), KillPedsGettingInVehicle, SetVehicleUpgradeFlags, CreateUpgradeAtomic, PickUpEntityWithWinch. |
| UNSURE | DoHeadLightBeam 0x6E0E20: structure, render states, indices equal; the exe keeps some products unrounded in the FPU (2x*right.x, the z,y,x alpha dot, lightNormal.z, lightPos x/y, right*0.05 y/z) so vertices differ by sub-ulp. Render only, not reproduced. |

## Tests
StandaloneRelease gta_reversed.exe rebuilt (0 errors). Pre-commit grep clean. wave2_oracle_test (RW_EXE_ORACLE and GTA_EXE = gta_sa_compact.exe): 1142 functions, 0 mismatches PC24 and PC53. gameplay_regress combat (-b worktree StandaloneRelease): PASS=14. Not run: soak, PortableWin64.
