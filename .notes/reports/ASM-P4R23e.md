# ASM-P4R23e: ped/world rows with exe comments but no verdict

Checked 17 functions (zone: gameplay .cpp directly in source/game_sa/): 16 MATCH (some partially read, see TSV), 1 FIXED, 1 UNSURE (FindAngleToWeaveThroughTraffic, skimmed only). 12 larger rows (CarCtrl steering, CWeapon::Fire*, PitchForSlope, EntryExit transitions, PathFind) NOT_REACHED.
Fix: CPopulation::FindDistanceToNearestPedOfType (exe 0x6143E0): best kept as float of the extended sqrt, strict `<`; port kept min of float squares.
The exe-comment bodies read this slice were all accurately annotated; no logic bugs found, only a last-bit x87 difference.
Build: StandaloneRelease links. Tests/regress left to the gate.
