# ASM-P4R2d: story-path callees, continued
22 functions compared against the exe: CCarCtrl SteerAICarWithPhysicsTryingToBlockTarget, SlowCarDownForOtherCar / ObjectsSectorList / CarsSectorList; CTaskComplexSequence (ctor, MakeAbortable, Flush, CreateNext/FirstSubTask); GoToPointAndStandStill(+Timed) CreateFirstSubTask/MakeAbortable; GetUpAndStandStill (3); EnterCarAsDriverTimed (ctor, MakeAbortable, ControlSubTask, CreateFirstSubTask); EnterCarAsPassengerTimed (ControlSubTask, CreateFirstSubTask); CTaskComplexEnterCar ctor.
All MATCH (x87 ordering already modelled). 0 fixes, no source changed, no build needed.
Not reached: FollowNodeRoute (carries ASM-P4S3b notes already), KillPedOnFoot / CarDriveMission ctors, remaining simple-task ctors.
