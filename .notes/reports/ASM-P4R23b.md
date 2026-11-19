# ASM-P4R23b
Verified 8 "audited in source" functions against the 1.0 exe: 7 MATCH, 1 FIXED (CDarkel::DealWithWeaponChangeAtEndOfFrenzy: MakeChangesForNewWeapon argument).
The orchestrator split the zone mid-run (traffic/weapon/path files moved to P4R25); the first four rows were already compared before that.
Rest of the list NOT_REACHED (budget): PedIK::PitchForSlope/RotateTorsoForArm/PointGunInDirection, Garage::ThrowCarsNearDoorOutOfGarage, Stats::UpdateFatAndMuscleStats/UpdateStatsWhenOnMotorBike, Pad::Update, PedGroup::Teleport, PedGroupIntelligence::Process, Population::ChooseCivilianOccupationForVehicle.
Result: .notes/asm_sweep/P4R23b.result.tsv
