# ASM-P4R6g
Reviewed P4R6f fixes: SetPursuit (0x68BBD0) and CEventSoundQuiet::AffectsPed (0x4B5240): both CONFIRMED against the exe.
Compared 17 more never-audited Tasks/Events functions (P4R6g.result.tsv): 2 fixes, 15 MATCH. Zone open: 89 of the 137 list.
- 989801828 CEventAcquaintancePedHateBadlyLit::AffectsPed: start-time difference compared signed (exe jl), port unsigned.
- 08b1e9940 CEventGunShotWhizzedBy::AffectsPed: projection was the squared component-wise product (not a dot product); closest-point distance had a stray z term.
Dead on PC: CEventBuildingCollision type-60 branch, PotentialWalkIntoObject avoid-entity branch. StandaloneRelease full link OK. Gameplay regress left to the gate.
