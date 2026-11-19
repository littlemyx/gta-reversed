# ASM-Z2 (ped tasks + vehicles/physics, story-mission path) - 2026-11-19
Result: the ryder1 hitcount list had 221 rows with an exe address and no verdict; only ~35 fall into the Z2 zone (the zone files were already swept by P4R*/S* slices; the remainder are render/HUD/radar/font/VisibilityPlugins/Menu = SKIP, TheScripts = Z1). 34 compared in full, 33 MATCH, 1 FIXED, 1 SKIP (CSkidmarks::Update, visuals). Table: Z2.result.tsv.
Fix: CReferences::Init (exe 0x5719B0): last node m_ppEntity now cleared like the other 2999 (exe loop runs 3000 times). Stale pointer only on a re-Init, no known mission impact.
Notes: CGlass::Init sums pane 0 as v0+v1+v2 but panes 1-4 as v2+v1+v0 in the exe; verified identical float32 results for the static data, so left as is.
Not done: no row of the zone with count > 1000 was left without a verdict; deeper work (CTaskComplexEnterCar etc.) already has FIXED/MATCH verdicts from S03b/P4R. No game run, no regress.
