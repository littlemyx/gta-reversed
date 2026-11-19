# ASM-P4R2z: level-1 callees of the entity ProcessControl family (184 rows, list from P4R2y)

Triage (no disassembly, heuristic on port body text): see P4R2z.result.tsv for per-row class.
Compared against the 1.0 compact exe: 12 functions, all MATCH, 0 fixes (no source changed, no build needed):
UpdatePassengerList, cTransmission::CalculateGearForSimpleCar, CVehicle::ActivateBomb, CPhysical::SkipPhysics,
AddCollisionRecord, GetVehicleAppearance, CTrain::FindMaximumSpeedToStopAtStations, CPhysical::GetLightingTotal,
CStats::GetPercentageProgress, CPed::SetLookTimer, CVehicle::ProcessWheelRotation, CPed::SetLookFlag(float).
Remaining NOT_REACHED rows are those still unreviewed (largest: CWanted::Update, CEventDamage::AffectsPed,
CPad getters, CPhysical::AttachEntityToEntity, CMatrix::RotateZ, CPopulation::UpdatePedCount).
