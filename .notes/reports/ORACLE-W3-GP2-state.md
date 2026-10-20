# ORACLE-W3-GP2 state (partial landing)

Done and committed on branch oracle-w3-gp2: TrafficLights / Restart / Shopping / PedAttractor rows + 5 fixes (see ORACLE-W3-GP2.md). Target wave3_gp2_oracle_test (3 min 45 s, PC24 0).

Remaining: Vehicle.cpp A/B (9) and World.cpp A/B leftovers (12), lists in the report. Next steps:
1. World rows: either add game_sa/World.cpp, PathFind.cpp, Collision/Collision.cpp, Shadows.cpp, WaterLevel.cpp, RenderBuffer.cpp, TheZones.cpp, CullZones.cpp, TimeCycle.cpp, common.cpp, GridRef.cpp, Draw.cpp (the wave3_world_oracle_test list) to wave3_gp2_oracle_test (then drop the CTheZones::GetLevelFromPosition stub in wave3_oracle_gp2.inc, it would be defined twice), or put the rows into wave3_world_oracle_test (wave3_oracle_world*.inc has the fixtures).
2. Vehicle-pool World rows (SetAllCarsCanBeDamaged, StopAllLawEnforcersInTheirTracks, FindUnsuspectingTargetCar): rf::Enter / Reset (logic_oracle_refs.inc), rf::VehE(i) blobs, matrix pointer at +0x14, FuncCase-style Prep / Done with the kRf extension.
3. Vehicle.cpp rows go into wave3_vehicle_oracle_test (Vehicle.cpp is already linked there).
