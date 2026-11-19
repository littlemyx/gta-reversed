# ASM-P4R23c: "audited in source" rows, ped/world zone (continued)

13 compared + 1 review: 12 MATCH, 1 FIXED, rest of the list NOT_REACHED (budget).
- Review of 6e9bf526e (CDarkel::DealWithWeaponChangeAtEndOfFrenzy): correct.
- FIXED CPedGroup::Teleport (exe 0x5F7AD0): the follower CTaskSimpleCarSetPedOut is created with m_bWarpingOutOfCar = true (exe `mov byte [task+0x11], 1` after the ctor); the port left it false.
- MATCH: UpdatePedCount, DoDriveByAutoAiming, UpdateStatsWhen{Sprinting,Running,Swimming}, IsAnyoneUsingCar, PlayerGaveCommand_Attack (speech tail not diffed), ChooseCivilianOccupationForVehicle, IsPedAppropriateForCurrentZone, UpdateIsGangArea, PickARandomGroupOfOtherPeds (random roll not diffed).
- Not reached: CWeapon::Update, SteerAICarBlockingPlayerForwardAndBack, TestCollisionBetween2MovingRects (AUDITED-IN-SOURCE rows), Garage / EntryExit / CarCtrl steering, FireInstantHit and the rest of C:/gta-scratch/p4r23c/cand.tsv (234 rows).
