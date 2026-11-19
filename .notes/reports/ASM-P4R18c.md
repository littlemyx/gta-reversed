# ASM-P4R18c

Review of 88928fe20 (CTaskSimpleSitIdle::MakeAbortable): CONFIRMED against exe 0x6311E0-0x631250.

Compared 4 further Tasks functions (KillCriminal::ControlSubTask 4288 B, CreateNextSubTask, PolicePursuit::PersistPursuit, FollowPointRoute::GetNextWaypoint): all MATCH, no source changes, no build needed.

NOT_REACHED (~47 of sel.tsv, budget): LineOfSightClearForAttack 0x621500, FindNextCriminalToKill 0x68C3C0, MedicTreatInjuredPed::ControlSubTask 0x65ABF0, WalkRoundBuildingAttempt::CreateSubTask 0x654C80, KillPedOnFootArmed::CreateFirstSubTask 0x62BF00 and the rest of the list after those.
