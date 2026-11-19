# ASM-Z1: Scripts / Camera / Weapons zone, story-mission path

Zone rows of ryder1.tsv without a verdict (after de-duplicating against asm_sweep/*result*.tsv of tip 6a3d55a66) were only ~20; the rest of the zone (script command handlers, Locate*/InArea helpers, camera oracle waves) was already covered by earlier sweeps and script_oracle_g*_test.
Compared 38 functions: 36 MATCH, 1 UNSURE (CCamera::StartTransition, only head read), 4 SKIP (render). No deviations found, no code changed, no builds needed.
Notable full traces: CCam::FlyBySplineVec3/Float (x87 order, stale duration quirk, float stores), CCam::Init, DoDeathArrestCheck.
Open: CCamera::StartTransition (0x515200) remainder; 154 install rows have no asm_sweep verdict by address but mostly sit under wave3 camera/script oracle tests (not re-verified here).
