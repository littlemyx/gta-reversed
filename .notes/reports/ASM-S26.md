# ASM-S26 - Physical.cpp asm sweep (slice S26, partial)
Method: exedis vs source, rules v5. Context cap reached after 19 of 28 functions; 9 marked NOT REACHED (ProcessCollision, ApplyCollisionAlt, ApplyFriction-self,
ProcessShiftSectorList (RD-AB-16 re-check), ApplyCollision-2, ApplySoftCollision-2, ProcessCollisionSectorList, _SimpleCar, PositionAttachedEntity). Details: S26.result.tsv.
Note: the standalone main thread runs x87 at PC=24 (PORT_BRIEF), so float-vs-extended sector index math (v*0.02f+60) is equal; CWorld::GetSectorX uses x/50+60 (UNSURE, 1 ulp edge only).

| function | exe addr | fix |
|---|---|---|
| ProcessShift | 0x54DB63..0x54DB92 | ped wall-hit speed damping: condition is (standingOn==null || !(standingOn->physFlags(+0x40)&4) || (physFlags&8)) || bSecondShift; port used CEntity flags and no damping for null standing |
| ProcessEntityCollision | 0x546D83..0x546D96 | SetHasHitWall also for entity with m_bIsStaticWaitingForCollision (0x40000) |
| ApplyCollision (3-arg) | 0x5436EC, 0x543776 | dot product order (ny*sy+nx*sx)+sz*nz; collision mass uses |r x n|^2 (port used |n|^2, cross product was unused) |
| ApplySoftCollision (3-arg) | 0x543A85 | same collision-mass bug (|r x n|^2) |

Consumer audit: no signatures/layouts changed. ProcessShift reads dword at +0x40 of m_standingOnEntity like the exe (for a CBuilding this is the next pool slot's data, as in the exe).
Gate: Release build of the worktree OK; soak.sh 3 min NOVSYNC RESULT: OK (0 errors); oracle targets not built (none of the four functions is covered by physics_oracle; the orchestrator gate runs the suite).
