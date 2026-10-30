# ASM-S71 report

Base e8b05a4c4 (origin/reverse/interior_c). Result table: `.notes/asm_sweep/S71.result.tsv`.

| row | verdict | commit |
|---|---|---|
| CTrafficLights::DisplayActualLight (0x49DAB0-0x49E5CC) | FIXED (camera dot y,z,x) | 60f6af4fe |
| WaterCreature_c::Init | FIXED (mirrored group offset, clamp NaN arm) | 84d8df5a6 |
| WaterCreature_c::Update | FIXED (int rolls, wrap tests, strictness, associations) | f027e9e94 |
| WaterCreatureManager_c::CanAddWaterCreatureAtPos | FIXED (2D, squared min dist) | 013d7aca2 |
| WaterCreatureManager_c::Update | FIXED (signed timer, stop on failed Init) | 5bf418364 |
| WaterCreatureManager_c::TryToFreeUp / TryToExitGroup | FIXED | ac44eb337 |
| C3dMarker::UpdateZCoordinate | FIXED (NaN arm) | d7c0e586f |
| C3dMarker::Render | FIXED (normalised tilt basis, tube scale) | 89048d86d |
| CMovingThings (5), CWaterCannon::Update_OncePerFrame, CWaterCannons::Update, WaterCreatureManager Init/Exit, WaterCreature_c::Exit | MATCH | - |
| CRope / CRopes (10), CWaterCannon::PushPeds/Render, CWaterCannons::Render, C3dMarker::AddMarker/DeleteMarkerObject | NOT_REACHED | - |

## Notes

- Traffic lights: `LightForCars*`/`LightForPeds` timing is outside this slice; DisplayActualLight was traced end to end (ped-light statics, all RegisterOne/RegisterCorona/AddLight/StoreStaticShadow args) and only the dot-product order differed.
- WaterCreature_c::Init: the exe stores `offset = pos - newPos` where `newPos = pos + dir*dist`, so followers swim to the mirror slot of where they spawned.
- WaterCreatureManager_c::CanAddWaterCreatureAtPos: the exe compares the 2D squared distance with minDist^2 (the port used a 3D squared distance against the unsquared value), which changes how close same-type leaders may spawn.
- C3dMarker::Render: `CMatrix::WithUp` (Core/Matrix.h) does NOT normalise right/forward, the exe does for markers. `tDirectionArrow::Render` (3dMarkers.cpp:727, another slice) also uses `WithUp`; check exe 0x7212A4 there — if it normalises too, WithUp itself should be fixed. Not touched here (shared helper).
- Open: ropes (crane/heli magnet segment physics, attach/detach) were not reached; WaterCannon::Render / PushPeds not re-traced.
