# ASM-S19d (Trailer.cpp, Train.cpp, Vehicle.cpp; 101 rows)

Partial. All 10 remaining CTrain rows compared (2 FIXED, 8 MATCH); the 91 CVehicle/CTrailer rows are NOT_REACHED (budget: the CTrain disassemblies were long). Result table: S19d.result.tsv. Continue in S19e from CVehicle::PreRender (0x6D6480), ProcessWeapons, InflictDamage (0x6D7C90, port already carries exe notes), ReactToVehicleDamage, DoVehicleLights/DoHeadLight*, DoTailLightEffect, FirePlaneGuns, DoDriveByShootings, then Trailer.

| Function | Verdict |
|---|---|
| CTrain::AddNearbyPedAsRandomPassenger 0x6F8170 | FIXED 59af6557: NaN polarity of the 625 range test (`dist > 625` skips) and of the side tests (`!(side < 0)` clockwise, `!(side > 0)` counter-clockwise); everything else matched (plane D z,y,x sum, 999999 start, task args, enter-car pick, nibble counters) |
| CTrain::Shutdown 0x6F58D0 | FIXED 4ae2f829: `for (auto node : pTrackNodes)` nulled a copy; the exe nulls the global slots, so InitTrains after Shutdown re-reads the track files instead of reusing freed nodes |
| SetModelIndex, Teleport, PreRender, OpenDoor, Constructor, InitTrains, ReadAndInterpretTrackFile | MATCH |
| MarkSurroundingEntitiesForCollisionWithTrain 0x6F6640 | MATCH (sub-ulp UNSURE: the exe compares the floored unrounded x-r sector for the clamp but returns the floored float-stored one) |

## Tests
StandaloneRelease gta_reversed.exe rebuilt (Train.cpp). wave2_oracle_test (RW_EXE_ORACLE and GTA_EXE = gta_sa_compact.exe): 1142 functions, 0 mismatches PC24 and PC53. gameplay_regress combat (-b worktree StandaloneRelease): PASS=14. Not run: soak, PortableWin64 (no new double/ftol spellings; pre-commit grep clean).
