# ASM-P4R18e
- Review 925f1993f (KillCriminal::FindNextCriminalToKill): CONFIRMED (z,y,x sum in extended precision, reference stored as float, unrounded compare, ref==0 accepts; 5 slots).
- Fix 5f23ed2b6: CTaskComplexKillPedOnFootArmed::CreateFirstSubTask (exe 0x62C104-0x62C12A): the GUN_CTRL range threshold is range^2*0.25 (exe constant 0x858C84), the port used (range/4)^2. Rest of the final CreateSubTask selection matches.
- 9 more list rows compared at instruction level, all MATCH (SitDown x2, StealCar x2, PassObject, SimpleFight::ChooseAttackAI, InvestigateDeadPed x2, HitResponse::CreateFirstSubTask).
- 26 rows NOT_REACHED (budget). Build: StandaloneRelease link OK (changed .cpp + link). No game run.
