# ASM-P4R2c: review of P4R2b fixes + story-path callees (continued)

Part A: the three P4R2b fixes (32ff1022f, 6c5601514, 977b1f573) re-checked against the exe: all CONFIRMED, no change.
Part B: 13 more functions compared (CCarCtrl FindMaxSteerAngle / HeadingForTarget, CPickups::Update / GenerateNewOneRaw, CVehicleRecording Restore*/Interpolate, TurnToFaceEntityOrCoord x3, CPed::TurnBody, CVehicle::IsUpsideDown / CanPedEnterCar): all MATCH, x87 ordering noise only. 0 fixes, no source changed, no build needed (baseline StandaloneRelease built clean in the worktree).
Already covered by earlier slices (not repeated): CPickups DoPickUpEffects, VehicleRecording SaveOrRetrieve / SkipForward, CCarCtrl Join/PickNext*, KillPedOnFootArmed (S05d/e).
Not reached: SteerAICarWithPhysicsFollowPath (0x434900), TryingToBlockTarget (0x4335E0), SlowCarDownForOtherCar/ObjectsSectorList/CarsSectorList; remaining AUTO_MATCH task rows (P4T1_batch score <= 1.9) not hand-compared.
