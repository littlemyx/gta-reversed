# ASM-P4S4: vehicles + physics + entity (25 rows)

Rows: 16 compared (6 FIXED rows from 5 commits, 10 MATCH), 1 INTENTIONAL (CBoat::DebugCode), 8 NOT_REACHED (FlyingControl, FillBoatList, UpdateMovingCollision, ApplyFriction-other, DettachEntityFromEntity, UpdateTrailerLink, UpdateTractorLink, SpecialEntityPreCollisionStuff).
Table: .notes/asm_sweep/P4S4.result.tsv. Regress left to the gate.

TS (one line per fix):
- CEntity::GetIsBoundingBoxOnScreen: pick bbox max when the frustum-normal component has its sign bit set (-0.0 included), else min.
- CVehicle::ApplyTurnForceToOccupantOnEntry (AddPassenger x2): bike force = up*(mass*-0.02f), offset = forward*-0.1f; other = (0,0,mass*-0.2f) (multiplications, not divisions).
- CAutomobile::SetBumperDamage: bouncing panel angle = (rand/32767)*-0.3 - 0.2 (float chain), not -0.5 + r*0.3.
- CAutomobile::SetDoorDamage: boot branch gated on handling model-flag NoswingBoot (bit 7 of +0xCC), not handling-flag AltSteerOpt.
- CAutomobile::GetMovingCollisionOffset: Packer = angle * -0.0001.
