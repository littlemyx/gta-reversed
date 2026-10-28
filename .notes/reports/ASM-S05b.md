# ASM-S05b (Tasks/TaskTypes: GoToPointAiming .. KillCriminal) - 2026-10-28
Result: 12 of 36 compared (7 MATCH, 5 DEVIATION fixed in 5 commits, 0 UNSURE), 24 NOT_REACHED. Table in S05b.result.tsv.
Fixes (exe address):
- f8d56ba4 InvestigateDeadPed::ControlSubTask (0x697A00-0x697B41): squared delta summed (z2+y2)+x2.
- 53af3b25 InvestigateDisturbance::CreateNextSubTask (0x6749F0-0x674B64): no-entity fallback GoToPointAndStandStill radius 0.5 (port 1.5).
- 437093bf InvestigateDisturbance::CreateFirstSubTask (0x66C820-0x66CA30): direction is normalised before the dot, threshold 0.707f (port COS_45 on an un-normalised dot), duck unless dot < 0.707.
- 86e1c57c KillCriminal dtor (0x68BF30-0x68C04A): siren flag cleared only for the driver cop.
- 012d9abd KillCriminal::CreateNextSubTask (0x68E4F0-0x68E93F): criminal in a vehicle is chased by car (port fell through to FindNext), LEAVE_CAR tests ped->m_pVehicle (port bInVehicle), DRIVER-not-in-car sets m_CantGetInCar, 25m test sum order z,y,x.
MATCH: GoToPointAiming CreateNext/Control, GotoDoorAndOpen::CreateFirstSubTask, InvestigateDeadPed CreateNext/CreateFirst, KillCriminal CreateSubTask, KillCriminal::CreateFirstSubTask (decision logic and tail; inlined sub-task ctor args not re-read).
Open range (NOT_REACHED, in order): KillCriminal::ControlSubTask (0x68E950, x87-annotated, not started); KillAllThreats::CreateFirstSubTask; KillPedGroupOnFoot x3; HitPedWithCar::ComputeEvasiveStepMoveDir; GoToPointAndStandStillTimed x2; KillPedFromBoat::ControlSubTask; GoPickUpEntity::ControlSubTask + CreateFirstSubTask tail from 0x693881; GoToCarDoorAndStandStill x3; KillPedOnFoot x4; KillPedOnFootArmed x5; InAirAndLand::CreateNextSubTask tail from 0x67CFD8.
Helper for the next agent: C:/gta-scratch/asm-s05b/dis.sh ADDR [max] (annotated disasm with addresses and exe constants), tt.py (task id -> name), k.py (exe float constants).
Budget: ran out at the landing threshold; ~40% went into KillCriminal (the inlined CreateSubTask in CreateFirstSubTask makes it long).
