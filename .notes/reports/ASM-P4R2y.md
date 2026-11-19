# ASM-P4R2y: per-entity ProcessControl family, 2 levels

Walked: 47 level-0 roots (ProcessControl/Collision/Shift/Teleport/PreRender-logic of all entity classes, hooked; 44 already have verdicts, 3 Entity/AnimatedBuilding stubs without), plus CTaskManager/CEventGroup/CarAI/scanner helpers; level-1 callee list = 184 functions without a verdict (C:/gta-scratch/p4r2y/l1.txt).
Already-verdict at level 0: 44 of 47 roots (all except CEntity stubs 0x403E40-0x403E80 and CAnimatedBuilding 0x537A30/40, trivial).
Compared: 14 functions (see P4R2y.result.tsv): all MATCH, 0 fixes (CPhysical force/speed/skip/collision-record, CEventGroup helpers, CAutomobile::ScanForCrimes/BoostJumpControl, CTrain station speed).
NOT_REACHED: the remaining ~170 level-1 callees (pad getters, CVehicle::ProcessWheelRotation/ProcessCarAlarm/UpdatePassengerList, cTransmission, CPed::SetLookTimer, CEntity refs, level 2) and the CPedIntelligence/CTaskManager/CarCtrl 2-level walks; the physical helpers show x87 notes in source, i.e. were reviewed in earlier slices without a tsv row.
