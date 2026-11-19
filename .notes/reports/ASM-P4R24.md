# ASM-P4R24: vehicle-class functions with exe-address comments but no verdict

C:/gta-scratch/p4r22/cand.tsv holds 13 rows in source/game_sa/Entity/Vehicle; 11 already have P4R22 verdicts. The two left, CVehicle::DoBladeCollision and CBmx::LaunchBunnyHopCB, were compared instruction by instruction against exe 1.0: both MATCH (constants, jump table, x87 evaluation order, sector clamps and loop order, call argument order verified). 0 source changes, no build needed. Not examined: CVehicle::BladeColSectorList (template, not in the list).
