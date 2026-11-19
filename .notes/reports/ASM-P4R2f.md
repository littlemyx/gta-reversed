# ASM-P4R2f: bd9b0f322 review + story-path callees outside Scripts/

Part A: CTaskComplexKillPedOnFoot::Clone (0x623370) CONFIRMED against the exe (ctor, then copy of bits 0x10 and 0x20 of byte +0xC; bit 0x40 not copied).
Part B: 20 functions compared (LookAbout ctor/Clone/IsInterruptable, StandStill ctor/Clone/MakeAbortable, CarDrive ctors/Clone/SetUpCar, GoToPointAndStandStill + Timed ctors/Clone, CVehicle::CanBeDeleted, CMessages::Process, CCamera::Fade, CEventGroup::Remove/Flush, WaitUntilPedIsInCar::ProcessPed): all MATCH, 0 fixes.
Note: the name-matched story-path candidate list is mostly small functions; only ~45 AUTO_MATCH functions >= 0x80 bytes reachable by name from story handlers, remaining ones (task Clone/MakeAbortable rows, CScriptedBrainTaskStore::SetTask, SetMaximumWantedLevel, TakeControl, CloseAllDoors, RemoveAllTrains, FindMaxNumberOfGroupMembers, SetCurrentTime, RemoveBuildingsNotInArea) are NOT_REACHED.
