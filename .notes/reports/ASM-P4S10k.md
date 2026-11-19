# ASM-P4S10k

Part A: P4S10j fixes 09d7c3ddd (WalkAlongsidePed::ControlSubTask) and 10ccc4aba (KnockOffBike::AffectsPed) both CONFIRMED against the exe.
Part B: 11 more rows compared (ApplyForce/FrictionForce/AirResistance, ComputeDangerResponse, RoadRage::CreateNextSubTask, ComputeTargetDoor, PlayerWantsToAttack, Boat::IsVertexAffectedByWake, DestroyCar::CreateNextSubTask/CreateSubTask).

Changes:
- CPhysical::ApplyAirResistance (exe 0x544C40-0x544D4F): extra air-resistance multiplier gate: IsAutomobile() (type, +0x590) -> sub type (+0x594) == AUTOMOBILE or BIKE (monster trucks / quads no longer get it).
