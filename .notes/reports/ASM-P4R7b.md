# ASM-P4R7b

Zone 1112 functions (986 without a verdict in any result TSV). Big CPhysical functions were already covered by ASM-MIX1/S26b (ApplyCollision-2, ApplySoftCollision-2, ApplyCollisionAlt, PositionAttachedEntity, ProcessShiftSectorList).

Compared: CPlaceable::SetMatrix MATCH, CEntity::CalculateBBProjection MATCH, CVehicle::ProcessWheel MATCH (spot check), ProcessCollisionSectorList partial (0x54C9D0-0x54CCF2).

Fix: ProcessCollisionSectorList, boat friction branch skips the train/wrecked/abandoned block (0x54CB5B -> 0x54CC5D).

NOT_REACHED: ProcessCollisionSectorList rest of body; remaining ~980 zone functions.
