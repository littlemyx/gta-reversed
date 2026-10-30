# ASM-S67 (phase 2: 13 dangling UNSURE / NOT_REACHED rows) - 2026-10-30
Result: 13 rows = 2 MATCH (already covered by earlier slices, only bookkeeping) + 1 MATCH (ScanForCollisionEvents, new full diff) + 3 DEVIATION (fixed) + 7 NOT_REACHED (no exe callers). Table: S67.result.tsv.
Fixed:
- AddPedAtAttractor (3a2700d83): the CEventAttractor was built with the NEW ped as its entity and task id USE_ATTRACTOR; exe passes the `entity` argument (attractor owner) and sets task 0xE9 = TASK_COMPLEX_USE_EFFECT after the ctor (0x614368-0x614396).
- CTaskSimpleGoTo::SetUpIK (7438a21bd): look-at z = (2*dir.z + ped.z) + 0.61f summed in the FPU (port added ped.z + 0.61f first); forward dot z,y,x unrounded.
- CAutomobile::DoBurstAndSoftGroundRatios (c34269e76): forward-speed dot z,y,x; sand term offroad*(rem*adhesion) (port (offroad*rem)*adhesion); NaN arms (burst/sand stores keep NaN, strict `compression < 1` else rail branch, adhesion 0.4 floor keeps NaN); ftol truncated to 16 bits.
MATCH: ScanForCollisionEvents 0x604500-0x6052E0 whole function (73836c530 only respells 6 bare doubles as x87d).
Resolved by earlier work (no re-diff): CCam::Process (S31 whole function + S31b/S31e carried branch), CEntity::PreRender (S45 head + S45b tail).
NOT_REACHED (no exe callers, unreachable in the run build): Cover x3, LoadPlatecharsetDat (S43d), RestoreStuffFromMem (S52), PedGroupMembership::From (S54c), LaserScopeDot (S64b/c).
Open: none. Unknown: SoundQuiet balaclava position vec is zero in both exe and port.
