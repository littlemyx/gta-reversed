# ASM-P4S4b (continuation of P4S4)
5 of 8 compared: 4 FIXED, 1 MATCH, 3 NOT_REACHED (FlyingControl, ApplyFriction-other, UpdateTractorLink): budget. Table: .notes/asm_sweep/P4S4b.result.tsv.
Commits: 5707939da FillBoatList, d8d8cf605 UpdateMovingCollision, 8494ade2d DettachEntityFromEntity, 9c1063290 SpecialEntityPreCollisionStuff.

TS notes (per FIXED row):
- FillBoatList: normalise only XY of the camera forward (skip if len<=0); dot = dy*dirY+dx*dirX; compare squared dist > 6400 (not sqrt>80); replacement scan uses squared dists with initial min 999999.875.
- UpdateMovingCollision: angle<0 path: delta=int16(trunc(gun*ts*mult*(1/128))); sum=cur+delta; sum<0 -> 0; then >2500 -> 2500. Inverted (cement truck/ladder) path: delta = trunc(((gun*2)*ts*10)/128) (gun<-10), trunc(((gun+100)*ts*10)/128) (else), same op order.
- DettachEntityFromEntity: force direction = forward of (copy of attachedTo matrix) *= offset matrix (offset = RotateZ(y) then RotateX(x)), times z.
- SpecialEntityPreCollisionStuff: fallen lamp post object: set bCollisionDisabled and the object's ignored-collision, then return immediately.

Tests: run_all_tests with the exe oracle (see orchestrator note); regress left to the gate.
