# ASM-S20c - Ped.cpp leftovers (13 functions)

Rules v6; worktree C:/gta-wt/asm-s20c (removed at the end). Result table: `.notes/asm_sweep/S20c.result.tsv`. Stopped at the token budget.

| verdict | count |
|---|---|
| MATCH | 2 (ShoulderBoneRotation, MakeTyresMuddySectorList) |
| FIXED | 1 |
| UNSURE | 0 |
| NOT_REACHED | 10 (Constructor, PlayFootSteps, DoFootLanded, KillPedWithCar [0x5F0360..0x5F0B2D read, not diffed past the heavy-hit start], PreRenderAfterTest, ProcessBuoyancy, PositionPedOutOfCollision, PositionAnyPedOutOfCollision, ProcessControl, ProcessEntityCollision) |

## Fix
- CPed::SetPedPositionInCar 0x5DFB96/0x5DFB03 (082f61e3): the van check compared `GetAnimGroupId()` (88 + group) with the raw 13, so it never fired (van rear passengers lost the side rotation/offset); the combine chassis offset must be `pos.z += chassisZ` on the rotated position, not an overwrite of the position.

## Notes
- Consumer audit: both changes are local to the function body.
- Gate: see hand-back.
