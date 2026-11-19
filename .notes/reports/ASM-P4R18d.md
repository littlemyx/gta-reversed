# ASM-P4R18d: remaining "audited in source" Tasks functions

13 of 48 compared against exe 1.0 (budget-limited): 12 MATCH, 1 FIXED, 35 NOT_REACHED (see .notes/asm_sweep/P4R18d.result.tsv).

Fix: CTaskComplexKillCriminal::FindNextCriminalToKill (0x68C3C0): squared distances summed z,y,x in extended precision (x87d), reference stored as float (commit 8c4796dd6).

Verified MATCH: LineOfSightClearForAttack, MedicTreatInjuredPed::ControlSubTask, WalkRoundBuildingAttempt::CreateSubTask, KillPedOnFootArmed::CreateFirstSubTask (first 80%), HasTaskSecondary, SeekCoverUntilTargetDead::CreateNextSubTask, StealthKill CB, GoToBoatSteeringWheel::CreateNextSubTask, UseScriptedBrain::CreateFirstSubTask, SmartFleePoint::SetDefaultTaskWanderDir, PresentIdToCop::ComputeHeadingToCop, InvestigateDeadPed::ComputeTargetPos.
Open: the 35 NOT_REACHED rows (SitDown*, FollowNodeRoute, InvestigateDeadPed::CreateSubTask/ControlSubTask, StealCar, PassObject, SimpleFight::ChooseAttackAI, etc.). No game run / regress done (compare-only slice).
