# ASM-P4R2: story-path callees outside Scripts/ (review + 22 functions)

Review of d57dcacc2 (CAutomobile::FixPanel/FixDoor): CONFIRMED against exe 0x6A3670 / 0x6A35A0 (CMatrix ctor over frame+0x10 with attach=0, SetTranslate with its own position, UpdateRW).

Path list: C:/gta-scratch/p4r/opcodes_story.tsv (939 opcodes; hot ones are mostly polling/arith; entity polls IsCharDead, IsCarDead, HasCharBeenDamagedByChar, LocateChar*, IsCharInCar, car recording).
Compared 22 functions (outside Scripts/, not in any earlier result tsv): 22 MATCH, 0 FIXED, 0 noise. No source changed (no build needed).
Most uncompared hooks in the zone (VehicleRecording, Physical, Vehicle, PedGroup*, PedIntelligence, CarEnterExit) turned out already ASM-clean; remaining uncompared: EventHandler Compute*Response, CarCtrl steering, Streaming::RequestModel/RemoveModel, World::Add/ClearCarsFromArea, Pickups, rest of VehicleRecording (Interpolate, SaveOrRetrieve, SkipForward).
