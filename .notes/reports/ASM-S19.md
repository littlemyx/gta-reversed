# ASM-S19 (Trailer.cpp, Train.cpp, Vehicle.cpp; 108 rows)

Partial. Compared 3 (3 MATCH), NOT_REACHED 105 (budget; slice consumed by S15d part 1 first). Result table: S19.result.tsv. Continue in S19b.

| Function | Verdict |
|---|---|
| CTrain::SkipToNextAllowedStation (0x6F72F0) | MATCH, all: engine walk, station loop (level as float, stat+1 < level), +-20 placement and +-0.1 speed, LoadScene, 2D distance*0.05+23 via ftol into PassTime |
| CTrain::IsInTunnel (0x6F6320) | MATCH, all |
| CTrain::RemoveRandomPassenger (0x6F6850) | MATCH, all (rating rand 0..12 -> 11, rand()&0xFFFF < 100 -> 25, LeaveCarAndWander(this,0,0,true) in primary slot 3) |

Open, in priority order for S19b: CTrain::ProcessControl (0x6F86A0, 1648 asm lines: split it), DoTrainGenerationAndRemoval (0x6F7900), TrainHitStuff (0x6F5CF0), CreateMissionTrain, then the CVehicle per-frame functions (PreRender, ProcessWeapons, InflictDamage, ReactToVehicleDamage, DoVehicleLights/DoHeadLight*, DoTailLightEffect, FirePlaneGuns, DoDriveByShootings), then the rest.
