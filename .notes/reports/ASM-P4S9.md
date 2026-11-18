# ASM-P4S9
No source changes (all compared functions MATCH the 1.0 exe).
- CVehicle::FlyingControlModel268 (exe 0x6D986E-0x6DA8FF): MATCH -> CVehicle::FlyingControl is now fully compared (Model0, 1345, 268, Drag + prologue).
- CPhysical::DettachEntityFromEntity (0x5442F0): MATCH.
- CPed::CalculateNewVelocity (0x5E4C50): MATCH.
- CTaskSimpleHoldEntity::DropEntity (0x6930F0): MATCH.
- CTaskComplexTrackEntity::CalcMoveRatio (0x65F930): MATCH.
Not reached (budget): KillPedOnFootMelee::CreateNextSubTask, FollowNodeRoute::CalcGoToTaskType, FollowPedFootsteps::CreateSubTask, RoadRage::ControlSubTask, UpdateMovingCollision, SpecialEntityPreCollisionStuff, CEventHandler::Compute*Response, CTheScripts rows.
