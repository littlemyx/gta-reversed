# ASM-P4R6c

Review of P4R6b: add7a166f CONFIRMED, d849f7919 CONFIRMED, 81015baa7 WRONG (the exe `fcomp 1.0; test ah,5; jp` skips on equal as well; fixed to `>=`, fe07ab82b).
CTaskComplexGoToCarDoorAndStandStill::CreateFirstSubTask finished: MATCH.
Compared 15 rows (see P4R6c.result.tsv): CTaskComplexGotoDoorAndOpen (3), CTaskComplexGoToPointAndStandStill (7), CTaskComplexLeaveCarAsPassengerWait::CreateSubTask, plus the review rows. 1 fix.
Not reached: KillPedOnFootArmed::ControlSubTask (0x62CCE0, already has ASM-S05e comments; first 0x180 bytes matched), event handler Compute* family.
