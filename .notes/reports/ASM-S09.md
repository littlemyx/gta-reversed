# ASM-S09 (Tasks/TaskTypes: WaitForBackup .. GoToVehicleAndLean) - 2026-10-28 - COMPLETE 40/40
Result: 40 rows: 31 MATCH, 9 FIXED in 8 commits (+2 in S08b). Table in S09.result.tsv. Tests: Release + RW builds OK, wave2_oracle_test 1142 functions PC24 0 / PC53 0 mismatches; combat regress PASS=14.
Fixes (run build, no FIX_BUGS guard needed):
- WaitForBackup::CreateNextSubTask (96291b85): the "backup close enough?" compare was inverted: the exe keeps aiming (achieve heading) while distSq >= waitDist^2 (NaN too) and finishes once the backup is closer.
- WalkRoundBuildingAttempt::ComputeRoute (d10f80b9): give-up at attempts>=20 after the increment (port +1 early); target = moveDir + m_pos (z-0.5), not the ped position; first LOS has no camera-ignore flag; "first attempt, all blocked => attempts=20" counts only iterations whose ped->target LOS was blocked (port counted every iteration, so it always fired); route cleared on entry.
- ComputeCrapRoute (f60a8448): both rand draws are unconditional (port early-outed when full), the [1,2) normal scale is drawn first; target = (normal*r1 + right*r2) + pedPos (association order); NaN-taking <0.35 test.
- WalkRoundBuildingAttempt::ControlSubTask (20f72e4d): entity give-up = |M*offset - targetPos|^2 > 16 (strict, z,y,x); port transformed (offset - targetPos) and used >=.
- Wander::ControlSubTask (7739a6f1): Say(GLOBAL_CHAT, 0, 0.1f) - port used probability 1.0 (chat every frame).
- WanderCop::ScanForStuff / LookForCriminals (8e02bdac): pursuit early-out tests the SUB-task type (port: own type, never true); criminal distance and forward-dot sums z,y,x with NaN skipping.
- GangHassleVehicle::ControlSubTask (8938a231): ProcessColModels uses CEntity::GetColModel (special col model); NaN-keeping heading test. GoToVehicleAndLean::ControlSubTask (7956f40b): moved-spot distance summed z,y,x.
MATCH: the other 31 (WalkRoundCar x4, WalkRoundObject x2, Wander CreateNext/First/CreateSubTask/GetWanderTaskByPedType, WanderCop::Control, WanderCriminal x2, WanderFlee::Control, WanderProstitute, WanderStandard x3, GangHasslePed x4, GangHassleVehicle dtor/CreateFirst, GoToVehicleAndLean x3, WaitFor* , WalkAlongside).
Notes: CVector::NormaliseAndMag already equals 0x59C970 (WalkRoundObject's private copy is redundant). Open range: none.
