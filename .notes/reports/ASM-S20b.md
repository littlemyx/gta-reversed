# ASM-S20b - Ped.cpp leftovers (21 functions)

Rules v6; worktree C:/gta-wt/asm-s20b (removed at the end). Result table: `.notes/asm_sweep/S20b.result.tsv`. Stopped at the token budget.

| verdict | count |
|---|---|
| MATCH | 6 (Destructor, SpecialEntityPreCollisionStuff, SpecialEntityCalcCollisionSteps, SetMoveAnim, Save, Load) |
| FIXED | 2 |
| UNSURE | 0 |
| NOT_REACHED | 13 (Constructor, ShoulderBoneRotation, PlayFootSteps, DoFootLanded, MakeTyresMuddySectorList, KillPedWithCar, PreRenderAfterTest, ProcessBuoyancy, PositionPedOutOfCollision, PositionAnyPedOutOfCollision, SetPedPositionInCar, ProcessControl, ProcessEntityCollision) |

## Fixes
- CPed::DeadPedMakesTyresBloody 0x6B4200 (1a16c938): scans regular 120x120 sectors, `floor(c*0.02+60)` clamped to 0..119, not the 30x30 LOD lists (the dead ped never made tyres bloody outside a 200-unit-wrong window).
- CPed::Render 0x5E7833/0x5E77D6 (d1a547c1): held weapon skipped when `physicalFlags.bSubmergedInWater && swim task` (port tested bInVehicle); camera range early-out only when strictly beyond the range (ties/NaN render).

## Notes
- Consumer audit: both fixes are local to the function body.
- Gate: see hand-back (Release build, soak; no oracle target covers these functions).
