# ASM-P4R7: no-verdict sweep part 2 (Entity/** + gameplay root files)

Zone: 1113 hooked functions >= 0x30 bytes (estimated sizes, many inflated) without a verdict. Compared 9 (listed in .notes/asm_sweep/P4R7.result.tsv): all MATCH, 0 fixes, no source changes (no build needed).
Not reached: the rest (largest remaining: CPhysical::ApplyCollision, ApplySoftCollision, ProcessCollisionSectorList, PositionAttachedEntity, ApplyCollisionAlt, ProcessShiftSectorList, CVehicle::ProcessWheel, CPlaceable::SetMatrix, CEntity::CalculateBBProjection).
List: C:/gta-scratch/p4r/p4r7_zone.tsv.
