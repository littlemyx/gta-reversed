# ASM-P4R2ab
Part A: P4R2aa fixes CPad::StartShake (8e4ff513b) and CPhysical::AttachEntityToEntity (a26e65c5f) re-read against the exe: both CONFIRMED (Pad offsets +0x12C/+0x130 unchanged).
Part B: 17 of the 29 open rows compared: 16 MATCH, 1 FIXED (CPhysical::ApplyTurnSpeed: unrounded cross products / COM transform). 12 rows NOT_REACHED (budget): ProcessBikeWheel (x87-annotated, spot-read), ProcessBuoyancy, CWeapon::Update, ApplyTurnForce, Reorthogonalise, ProcessAutoBusDoors, ScanForCrimes, FindSwatPosition, MarkTarget, GetAnimHierarchyFromSkinClump, 3 Fx rows.
See P4R2ab.result.tsv.
