# ASM-P4R18f

Review of 68c3eafed (KillPedOnFootArmed::CreateFirstSubTask, 0x62C104-0x62C12A): CONFIRMED (range^2*0.25, const 0x858C84 = 0.25).

Compared 21 of the 26 NOT_REACHED rows of P4R18e: 20 MATCH, 1 FIXED, 5 NOT_REACHED (UseAttractor::ControlSubTask, WalkRoundBuildingAttempt::ComputeMoveDir, SimpleFight::BeHitWhileBlocking, InteriorShopKeeper::ControlSubTask, ExtinguishFireOnFoot::FindFireNearPed).

Fix: 0a9601278 CTaskComplexCarSlowBeDraggedOut::CreateNextSubTask - SET_PED_OUT now ends the task (exe byte table 0x64C89C), the port sent it to SET_PED_SLOW_DRAGGED_OUT.

Open: Melee::CreateFirstSubTask and Medic::CreateNextSubTask compare in float where the exe keeps the sum in extended precision (boundary-only difference, not changed).
Detail: .notes/asm_sweep/P4R18f.result.tsv
