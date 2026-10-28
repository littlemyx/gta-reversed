# ASM-S19k (Vehicle.cpp rows; 48 rows)

Partial. 19 rows compared (3 DEVIATION fixed, 16 MATCH), 29 NOT_REACHED (budget). Result table: S19k.result.tsv.
Not reached: CVehicle ctor/dtor, SetModelIndex, Save/Load, DoHeadLightBeam, CustomCarPlate_TextureCreate, upgrade family (Set/Clear flags, CreateUpgradeAtomic, CreateReplacementAtomic, Add/Remove*Upgrade, AddUpgrade, AddVehicleUpgrade, SetupUpgradesAfterLoad, callbacks), winch pickup, DoBoatSplashes, DoSunGlare, BladeColSectorList, ProcessBoatControl preamble 0x6DBCE0-0x6DBF0A.

| Function | Result |
|---|---|
| RemoveDriver 0x6D1950 | Cop cars: the chromegun model is released (`SetModelIsDeletable`) whenever it is loaded; the port skipped it when `bFreebies` was already cleared (exe jumps to 0x6D19FD). Rest (caddy golf club, engine-off gate, ClearReference) equal. |
| AddSingleWheelParticles 0x6DE880 | Now fully compared: spinning (gas*speed*-30 random pairs, rand%10000*0.0001*0.8, colour from one random), skidding (flags&4, arg3>0.03), fixed, smoke-trail loop (count=ftol(|step|*scale)>=1, `1-i/count` x87), quad skid offset, RegisterOne args: all equal. |
| AddWheelDirtAndWater 0x6D2D50 | Every surface test reads CColPoint+0x23 (surfaceB); the port used pieceB (+0x24) for IsSand and CreatesWheelSpray. The wet-road random roll `Random(WetRoads, 1.01) > 0.5` is only made for dust / sand surfaces (WetRoads > 0, NaN skips) and returns false at once; the port rolled before the surface test (extra rand() for other surfaces) and fell through to the spray. Roll kept unrounded (x87d). |
| UpdateWinch 0x6D3B80 | Rope hook z = `mass*segLen - count*totalLen + base` summed unrounded in the x87 stack (port rounded each step); segment count is a signed `_ftol` + `fild` (port used uint32). |
| MATCH | Render, ResetAfterRender, SetRemap, DeleteRwObject, SetupLighting, RemoveLighting, KillPedsInVehicle, SetComponentAtomicAlpha, Set/ClearWindowOpenFlag, RemoveUpgrade, RemoveAllUpgrades, SetupRender, DestroyVehicleAndDriverAndPassengers, DoFixedMachineGuns. |

Also: `RandomInRangeExt` / `MagnitudeExt` and the `rnd` locals now return / use `x87d` instead of a bare `double`.

## Tests
StandaloneRelease gta_reversed.exe rebuilt (0 errors). Pre-commit grep clean. wave2_oracle_test (RW_EXE_ORACLE and GTA_EXE = gta_sa_compact.exe): 1142 functions, 0 mismatches PC24 and PC53. gameplay_regress combat (-b worktree StandaloneRelease): PASS=14. Not run: soak, PortableWin64.
