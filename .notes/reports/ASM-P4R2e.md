# ASM-P4R2e: story-path callees outside Scripts/ (continued)

20 functions compared (KillPedOnFoot ctor/MakeAbortable/Clone, CarDriveMission ctor/dtor/Clone, GoToPoint + GoToPointFine ctors/Clone/BaseRatio, KillPedOnFootArmed ctor/MakeAbortable, Stealth/Melee ctors, TrackEntity ctor/CreateFirst/CreateNext, CPed::SetAimFlag(entity), TaskSimplePause::ProcessPed): 19 MATCH, 1 FIXED. CTheScripts::StartNewScript rows skipped (Scripts/ belongs to P4R1).

| function | exe range | before -> after |
|---|---|---|
| CTaskComplexKillPedOnFoot::Clone | 0x623370-0x6233F8 | copied "waiting" (bit 0x40), dropped aimImmediate -> copies aimImmediate (0x10) + waitFor (0x20), waiting not copied |

Not reached: CTaskSimpleLookAbout / StandStill / CarDrive ctors, GoToPointAndStandStillTimed ctor/Clone, remaining AUTO_MATCH rows.
