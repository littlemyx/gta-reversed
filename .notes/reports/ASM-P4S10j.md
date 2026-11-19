# ASM-P4S10j

Part A (P4S10i fixes, independent re-read): ProcessOpenDoor caf3399ba CONFIRMED (anim-id jump table 0x6D5C38/0x6D5C60 checked); FindPedToAttack 3c3de413e CONFIRMED (0x47C34FF3).

Part B (queue rows not yet in any P4S10*.result.tsv, largest first): 9 functions (4 more than the named fix rows, 7 MATCH, 2 FIXED), see P4S10j.result.tsv.

Changes:
- CTaskComplexWalkAlongsidePed::ControlSubTask (exe 0x685170-0x68562C): one shared bool -> ped flag when its walk-anim branch ran, target flag only when the target also has a walk anim (09d7c3ddd)
- CEventKnockOffBike::AffectsPed (exe 0x4AFEE0-0x4AFF57): veto on any non-zero CantBeKnockedOffBike and !bHasBeenRendered -> veto only on CantBeKnockedOffBike == NEVER (1) && !forceKnockOff (10ccc4aba)
