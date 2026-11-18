# ASM-P4S9b
No source changes (all compared functions MATCH the 1.0 exe).
- CTaskComplexKillPedOnFootMelee::CreateNextSubTask (0x62BC10-0x62BE28): MATCH.
- CTaskComplexFollowNodeRoute::CalcGoToTaskType (0x66EBE0-0x66EDAF): MATCH.
- CTaskComplexFollowPedFootsteps::CreateSubTask (0x695E40-0x696025): MATCH.
- CTaskComplexRoadRage::ControlSubTask (0x62E3F0-0x62E52B): MATCH.
- CAutomobile::UpdateMovingCollision (0x6A1460-0x6A1EAD): MATCH (exe uses uninitialised componentPos/matrix when the misc node is missing; port zero-inits; cosmetic).
- CVehicle::SpecialEntityPreCollisionStuff (0x6D6640-0x6D6A26): MATCH.
Not reached (budget): CEventHandler::Compute*Response rows, CTheScripts rows of P4S2.
