# ASM-HC8d: third pass over hot MATCH functions in root source/game_sa
Taken 9 rows (6 fully, 2 partial): 8 RECONFIRMED, 0 FIXED. No earlier MATCH verdict found wrong in this pass (S30, P4R23e, S23b, P4R2z, S22d, S55 hold).
Only NOTE: CPopulation::ManageObject differs from the exe for NaN distances only (unreachable). Most remaining top rows are already x87d-traced in source (AddLight, FindNodesThisCarIsNearestTo, SlowCarDownForPeds...), so the yield of this pass is low; CRope::Update not reached.
Method: instruction-level compare, PC24 per-op float rounding (Base.h x87d) assumed.
