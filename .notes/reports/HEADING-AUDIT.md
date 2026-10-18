# HEADING-AUDIT report

Branch `heading-audit` (worktree C:/gta-wt/heading-audit), base origin/reverse/interior_c d48e0973. Tool: `tools/standalone/heading_audit.py` (+ `heading_audit_explained.tsv`).

## Method
For every `RH_Scoped*Install` site with an exe address in `source/game_sa/Tasks`, `source/game_sa/Events`, `World.cpp` (FindPlayerHeading) and `Trailer.cpp` (SetTowLink, ProcessAI) the script disassembles the exe function (capstone, two end-of-function heuristics) and
counts its heading events: calls to LimitRadianAngle 0x53CB50, GetRadianAngleBetweenPoints 0x53CBE0 (GRABP), GetATanOfXY 0x53CC70, GetNodeHeadingFromVector 0x53CDC0, GetAngleBetweenPoints 0x53CEA0 (degrees), LimitAngle 0x53CB00,
CPlaceable::GetHeading 0x441DB0, FindPlayerHeading 0x56E450 and the inline `fpatan` (= `CVector::Heading()` / atan2). The same events are counted in the port function body (text scan, `Heading()` = fpatan, `Heading(true)` = fpatan + LIM).
A multiset difference is a candidate; each candidate (and a sample of equal ones) was then read in the asm. 90 hooked functions have events. Before the fixes: 53 equal, 34 different, 4 not found (templates / free function); after: 65 equal, 25 EXPLAINED (reason per row in the tsv), **0 unexplained**.
A second pass (one read-only helper, sonnet) checked ARGUMENTS / signs / stores of the 34 uncovered equal-multiset functions: 28 match, 6 deviations (2 fixed, 4 open, below).

## Port bugs fixed (one commit each, subject = function + asm address)
| exe addr | function | exe | port before | gameplay effect |
|---|---|---|---|---|
| 0x6973B0 | CTaskComplexWaitForBackup::CreateSubTask | LIM(GRABP(aggressor-ped,0,0)) | Heading() | wrong facing of cop waiting for backup |
| 0x697B70 | CTaskComplexReactToGunAimedAt::CreateSubTask | LIM(GRABP(...)) stored to +0x55C | Heading(), no store | wrong turn when aimed at |
| inlined 0x4BCE08, 0x4BD2B3, 0x4BD4D4, 0x4C1029 | CTaskSimpleAchieveHeading(ped, entity) ctor | LIM(GRABP(target, looker)) | Heading() | carjacked ped turned wrong way (EventHandler DraggedOutCar) |
| 0x4C0BD0 | CEventHandler::ComputeVehiclePotentialCollisionResponse (4 sites) | LIM(GRABP(veh, ped)) | Heading() | shake fist / hands up / face the car |
| 0x675890, 0x676640 | CTaskInteriorGoToInfo::CreateNextSubTask / CreateFirstSubTask | LIM(GRABP(Dir.x, Dir.y, 0, 0)) | Dir.Heading(true) | interior ped facing |
| 0x658AF0 | CTaskComplexTreatAccident::ComputeHeading | LIM(GRABP(injured, ped)) | Heading(true) | medic facing |
| 0x654C80 | CTaskComplexWalkRoundBuildingAttempt::CreateSubTask | Normalise, LIM(GRABP) | Heading() | |
| 0x67BE50 | CTaskComplexStuckInAir::ControlSubTask | LIM(GRABP(0,0,-x,y) - TheCamera.m_fOrientation) | GRABP only | player aim while stuck in air |
| 0x67E7B0 | CTaskSimpleJetPack::ProcessControlInput | pad: LIM(GRABP - cam orientation); mouse cam: fpatan of the (negated when not forward) front | atan2(x,y) without camera / Heading() - PI | jetpack facing |
| 0x649550 | CTaskSimpleWaitUntilPedIsOutCar::ProcessPed | GRABP (radians) | GetAngleBetweenPoints (DEGREES) | |
| 0x624B50 | CTaskSimpleFightingControl::CalcMoveCommand | wrap (heading - currentRotation) once, attack if \|d\| <= 15deg | wrapped the heading alone, `>=` | fight AI near +-pi |
| 0x62E540 | CTaskSimpleStealthKill::ProcessPed | (-d).Heading() | -(d.Heading()) | |
| 0x64C220 | CTaskSimpleCarSetPedSlowDraggedOut::ProcessPed | VEHICLE heading | ped's heading | |
| 0x620A20 | CTaskSimpleBeHit::ProcessPed | guard `!ped->IsPlayer()` | `!attacker->IsPlayer()` | |
| 0x62F520 | CTaskSimpleOnEscalator::ProcessPed | store to +0x55C | +0x558 | |
| 0x6CFDF0 | CTrailer::SetTowLink | ratio > -1000 -> 0; SetHeading(tow vehicle heading) | `clamp(ratio, 0, -1000)` (= -1000), own heading | baggage-trailer mode stuck on, trailer not aligned |
| 0x6CF590 | CTrailer::ProcessAI | unrounded headings, +-2pi applied to tow before the subtraction | float order | 1-ulp |
x87 rounding group (heading kept unrounded in ST0, `CPlaceable::GetHeadingExt()` / `FindPlayerHeadingExt()` new): CEventKnockOffBike::SetPedSafePosition 0x4B4AC0, CTaskSimpleCarJumpOut 0x64DD60, CTaskSimpleClimb (0x680DC0, 0x6803A0, 0x67A390, 0x67A5D0, 0x67FD30), CTaskSimpleHoldEntity 0x693C40, FindPlayerHeading 0x56E450 / DrawYouAreHere 0x5849B8.

## Table (after the fixes; port function -> events -> status; full list regenerated with `heading_audit.py --all`)
| exe addr | function | exe events | port events | status | n bodies | port file |
|---|---|---|---|---|---|---|
| 0x46F800 | CTaskComplexSeekEntityStandard::ControlSubTask | GRABP LIM | - | EXPLAINED(template) | 0 | TaskComplexSeekEntityStandard.cpp |
| 0x496760 | CTaskComplexSeekEntityRadiusAngleOffset::ControlSubTask | GRABP LIM | - | EXPLAINED(template) | 0 | TaskComplexSeekEntityRadiusAngleOffset.cpp |
| 0x4B4AC0 | CEventKnockOffBike::SetPedSafePosition | ATAN2x2 LIM | GETHDGx2 LIM | EXPLAINED(ext) | 1 | EventKnockOffBike.cpp |
| 0x4BC710 | CEventHandler::ComputeShotFiredResponse | GRABPx3 LIMx3 [strict -] | - | OK | 1 | EventHandler.cpp |
| 0x4BCC30 | CEventHandler::ComputeDraggedOutCarResponse | GRABPx3 LIMx3 | - | EXPLAINED(inlined-ctor) | 1 | EventHandler.cpp |
| 0x4C0BD0 | CEventHandler::ComputeVehiclePotentialCollisionResponse | GRABPx4 LIMx4 | GRABP LIM | EXPLAINED(lambda) | 1 | EventHandler.cpp |
| 0x56E450 | ::FindPlayerHeading | ATAN2x3 | - | EXPLAINED(ext) | 0 | World.cpp |
| 0x61D6F0 | CTaskSimpleFight::IsTargetInRange | ATAN2 | ATAN2 | OK | 1 | TaskSimpleFight.cpp |
| 0x61F9F0 | CTaskSimpleThrowControl::ProcessPed | ATAN2 | ATAN2 | OK | 1 | TaskSimpleThrowControl.cpp |
| 0x620490 | CTaskSimpleChoking::ProcessPed | ATAN2 | ATAN2 | OK | 1 | TaskSimpleChoking.cpp |
| 0x620A20 | CTaskSimpleBeHit::ProcessPed | ATAN2 | ATAN2 | OK | 1 | TaskSimpleBeHit.cpp |
| 0x621960 | CTaskSimpleGangDriveBy::ProcessPlayerPed | ATAN2x2 | ATAN2 | EXPLAINED(helper) | 1 | TaskSimpleGangDriveBy.cpp |
| 0x624710 | CTaskSimpleFight::ChooseAttackPlayer | ATAN2 | ATAN2 | OK | 1 | TaskSimpleFight.cpp |
| 0x624B50 | CTaskSimpleFightingControl::CalcMoveCommand | ATAN2 | ATAN2 | OK | 1 | TaskSimpleFightingControl.cpp |
| 0x624F30 | CTaskSimpleUseGun::StartAnim | ATAN2 [strict -] | - | OK | 1 | TaskSimpleUseGun.cpp |
| 0x625270 | CTaskSimpleGunControl::ProcessPed | ATAN2 | ATAN2 | OK | 1 | TaskSimpleGunControl.cpp |
| 0x627600 | CTaskSimpleGangDriveBy::ProcessAIPed | ATAN2x2 | ATAN2 | EXPLAINED(helper) | 1 | TaskSimpleGangDriveBy.cpp |
| 0x628350 | CTaskSimpleGangDriveBy::ProcessAimIK | ATAN2x2 GRABPx2 | GRABPx2 | EXPLAINED(helper) | 1 | TaskSimpleGangDriveBy.cpp |
| 0x6289F0 | CTaskComplexDestroyCarMelee::CalculateSearchPositionAndRanges | ATAN2 | ATAN2 | OK | 1 | TaskComplexDestroyCarMelee.cpp |
| 0x629920 | CTaskSimpleFight::ProcessPed | ATAN2x2 | ATAN2x2 | OK | 1 | TaskSimpleFight.cpp |
| 0x62A380 | CTaskSimpleUseGun::ProcessPed | ATAN2x2 | ATAN2x2 | OK | 1 | TaskSimpleUseGun.cpp |
| 0x62DB20 | CTaskComplexDestroyCarMelee::CreateFirstSubTask | GRABP | GRABP | OK | 1 | TaskComplexDestroyCarMelee.cpp |
| 0x62E0A0 | CTaskComplexDestroyCarArmed::CreateFirstSubTask | GRABP | GRABP | OK | 1 | TaskComplexDestroyCarArmed.cpp |
| 0x62E540 | CTaskSimpleStealthKill::ProcessPed | ATAN2x3 | ATAN2x2 | EXPLAINED(paths) | 1 | TaskSimpleStealthKill.cpp |
| 0x62F520 | CTaskSimpleOnEscalator::ProcessPed | GRABP LIM | GRABP LIM | OK | 1 | TaskSimpleOnEscalator.cpp |
| 0x63A380 | CTaskComplexEnterCar::GetCameraStickModifier | ATAN2x2 | ATAN2x2 | OK | 1 | TaskComplexEnterCar.cpp |
| 0x645CB0 | CTaskSimpleCarAlign::FixHeading | GRABP LIM | GRABP LIM | OK | 1 | TaskSimpleCarAlign.cpp |
| 0x6479B0 | CTaskSimpleCarSetPedOut::PositionPedOutOfCollision | ATAN2 | ATAN2 | OK | 1 | TaskSimpleCarSetPedOut.cpp |
| 0x649550 | CTaskSimpleWaitUntilPedIsOutCar::ProcessPed | GRABP LIM | GRABP LIM | OK | 1 | TaskSimpleWaitUntilPedIsOutCar.cpp |
| 0x64C220 | CTaskSimpleCarSetPedSlowDraggedOut::ProcessPed | ATAN2 | GETHDG | EXPLAINED(ext) | 1 | TaskSimpleCarSetPedSlowDraggedOut.cpp |
| 0x64DD60 | CTaskSimpleCarJumpOut::ProcessPed | GETHDGx2 LIM | GETHDG LIM | EXPLAINED(paths) | 1 | TaskSimpleCarJumpOut.cpp |
| 0x6513A0 | CTaskUtilityLineUpPedWithCar::ProcessPed | ATAN2x6 LIM [strict ATAN2x4 LIM] | ATAN2 LIM | EXPLAINED(helper) | 1 | TaskUtilityLineUpPedWithCar.cpp |
| 0x6533E0 | CTaskComplexEvasiveStep::CreateSubTask | GRABP | GRABP | OK | 1 | TaskComplexEvasiveStep.cpp |
| 0x653820 | CTaskComplexEvasiveDiveAndGetUp::CreateSubTask | GRABP | GRABP | OK | 1 | TaskComplexEvasiveDiveAndGetUp.cpp |
| 0x654C80 | CTaskComplexWalkRoundBuildingAttempt::CreateSubTask | GRABP LIM | GRABP LIM | OK | 1 | TaskComplexWalkRoundBuildingAttempt.cpp |
| 0x655570 | CTaskComplexEvasiveCower::CreateSubTask | GRABP | GRABP | OK | 1 | TaskComplexEvasiveCower.cpp |
| 0x656000 | CTaskComplexEvasiveDiveAndGetUp::CreateNextSubTask | LIM | LIM | OK | 1 | TaskComplexEvasiveDiveAndGetUp.cpp |
| 0x657380 | CTaskComplexWalkRoundObject::CreateFirstSubTask | GRABP LIM | GRABP LIM | OK | 1 | TaskComplexWalkRoundObject.cpp |
| 0x658200 | CTaskComplexWalkRoundCar::CreateFirstSubTask | GRABP LIM | GRABP LIM | OK | 1 | TaskComplexWalkRoundCar.cpp |
| 0x658AF0 | CTaskComplexTreatAccident::ComputeHeading | GRABP LIM | GRABP LIM | OK | 1 | TaskComplexTreatAccident.cpp |
| 0x6592A0 | CTaskComplexPresentIdToCop::ComputeHeadingToCop | GRABP LIM | GRABP LIM | OK | 1 | TaskComplexPresentIdToCop.cpp |
| 0x65BE40 | CTaskComplexSmartFleePoint::ComputeFleeDir | NODEH | NODEH | OK | 1 | TaskComplexSmartFleePoint.cpp |
| 0x65C140 | CTaskComplexSmartFleePoint::CreateFirstSubTask | NODEH | NODEH | OK | 1 | TaskComplexSmartFleePoint.cpp |
| 0x65FDD0 | CTaskGangHassleVehicle::GetTargetHeading | GRABP LIM | GRABP LIM | OK | 1 | TaskGangHassleVehicle.cpp |
| 0x660D80 | CTaskComplexSignalAtPed::ControlSubTask | GRABP LIM | GRABP LIM | OK | 1 | TaskComplexSignalAtPed.cpp |
| 0x661950 | CTaskComplexGangJoinRespond::ControlSubTask | GRABP LIM | GRABP LIM | OK | 1 | TaskComplexGangJoinRespond.cpp |
| 0x663540 | CTaskSimpleGoToPointFine::ProcessPed | GRABP LIM | GRABP LIM | OK | 1 | TaskSimpleGoToPointFine.cpp |
| 0x6637C0 | CTaskGangHassleVehicle::ControlSubTask | GETHDGx2 | GETHDGx2 | OK | 1 | TaskGangHassleVehicle.cpp |
| 0x664590 | CTaskGoToVehicleAndLean::CreateNextSubTask | GRABP LIM | GRABP LIM | OK | 1 | TaskGoToVehicleAndLean.cpp |
| 0x665760 | CTaskComplexSeekEntity::ControlSubTask | GRABP LIM | - | EXPLAINED(template) | 0 | TaskComplexSeekEntity.h |
| 0x668060 | CTaskSimpleAchieveHeading::ProcessPed | LIM | LIM | OK | 1 | TaskSimpleAchieveHeading.cpp |
| 0x66B9D0 | CTaskComplexTurnToFaceEntityOrCoord::ComputeTargetHeading | GRABP | GRABP | OK | 1 | TaskComplexTurnToFaceEntityOrCoord.cpp |
| 0x66C4E0 | CTaskSimpleSlideToCoord::ProcessPed | LIM | LIM | OK | 1 | TaskSimpleSlideToCoord.cpp |
| 0x66D710 | CTaskSimpleGoToPoint::ProcessPed | GRABP LIM | GRABP LIM | OK | 1 | TaskSimpleGoToPoint.cpp |
| 0x66F530 | CTaskComplexWander::ComputeTargetHeading | GRABP LIM | GRABP LIM | OK | 1 | TaskComplexWander.cpp |
| 0x675890 | CTaskInteriorGoToInfo::CreateNextSubTask | GRABP LIM | GRABP LIM | OK | 1 | TaskInteriorGoToInfo.cpp |
| 0x676640 | CTaskInteriorGoToInfo::CreateFirstSubTask | GRABP LIM | GRABP LIM | OK | 1 | TaskInteriorGoToInfo.cpp |
| 0x676D30 | CTaskInteriorSitOnChair::ProcessPed | GRABP LIMx2 | GRABP LIMx2 | OK | 1 | TaskInteriorSitOnChair.cpp |
| 0x6772E0 | CTaskInteriorLieInBed::ProcessPed | GRABP LIMx2 | GRABP LIMx2 | OK | 1 | TaskInteriorLieInBed.cpp |
| 0x677920 | CTaskInteriorSitAtDesk::ProcessPed | GRABP LIMx2 | GRABP LIMx2 | OK | 1 | TaskInteriorSitAtDesk.cpp |
| 0x67A390 | CTaskSimpleClimb::GetCameraTargetPos | ATAN2 | - | EXPLAINED(helper) | 1 | TaskSimpleClimb.cpp |
| 0x67A5D0 | CTaskSimpleClimb::GetCameraStickModifier | ATAN2x2 | GETHDG | EXPLAINED(helper) | 1 | TaskSimpleClimb.cpp |
| 0x67BE50 | CTaskComplexStuckInAir::ControlSubTask | GRABP LIM | GRABP LIM | OK | 1 | TaskComplexStuckInAir.cpp |
| 0x67DBE0 | CTaskSimpleClimb::StartAnim | ATAN2x2 [strict -] | - | OK | 1 | TaskSimpleClimb.cpp |
| 0x67E7B0 | CTaskSimpleJetPack::ProcessControlInput | ATAN2 GRABP LIM | ATAN2 GRABP LIMx2 | EXPLAINED(inline-wrap) | 1 | TaskSimpleJetPack.cpp |
| 0x67FD30 | CTaskSimpleClimb::ScanToGrab | ATAN2 | GETHDG | EXPLAINED(ext) | 1 | TaskSimpleClimb.cpp |
| 0x6803A0 | CTaskSimpleClimb::TestForClimb | ATAN2 | - | EXPLAINED(helper) | 1 | TaskSimpleClimb.cpp |
| 0x680DC0 | CTaskSimpleClimb::ProcessPed | ATAN2 | - | EXPLAINED(helper) | 1 | TaskSimpleClimb.cpp |
| 0x6834E0 | CTaskComplexWaitForPed::ControlSubTask | GRABP LIM | GRABP LIM | OK | 1 | TaskComplexWaitForPed.cpp |
| 0x683AD0 | CTaskComplexPartner::CreateNextSubTask | NODEH | NODEH | OK | 1 | TaskComplexPartner.cpp |
| 0x6859A0 | CTaskSimplePlayerOnFoot::ProcessPlayerWeapon | ATAN2 | ATAN2 | OK | 1 | TaskSimplePlayerOnFoot.cpp |
| 0x687530 | CTaskSimplePlayerOnFoot::PlayerControlFighter | GRABPx3 LIMx3 | GRABPx3 LIMx3 | OK | 1 | TaskSimplePlayerOnFoot.cpp |
| 0x687C20 | CTaskSimplePlayerOnFoot::PlayerControlZeldaWeapon | ATAN2 GRABP LIM | ATAN2 GRABP LIM | OK | 1 | TaskSimplePlayerOnFoot.cpp |
| 0x687F30 | CTaskSimplePlayerOnFoot::PlayerControlDucked | ATAN2 GRABPx2 LIMx2 | ATAN2 GRABPx2 LIMx2 | OK | 1 | TaskSimplePlayerOnFoot.cpp |
| 0x6883D0 | CTaskSimplePlayerOnFoot::PlayerControlZelda | GRABP LIM | GRABP LIM | OK | 1 | TaskSimplePlayerOnFoot.cpp |
| 0x688A90 | CTaskSimpleSwim::ProcessControlInput | ATAN2x3 GRABP LIM | ATAN2x3 GRABP LIM | OK | 1 | TaskSimpleSwim.cpp |
| 0x689640 | CTaskSimpleSwim::ProcessControlAI | ATAN2 | ATAN2 | OK | 1 | TaskSimpleSwim.cpp |
| 0x68AA70 | CTaskSimpleSwim::ProcessEffects | GAP LIMDEG | GAP LIMDEG | OK | 1 | TaskSimpleSwim.cpp |
| 0x68B8A0 | CTaskSimpleArrestPed::ProcessPed | GRABP | GRABP | OK | 1 | TaskSimpleArrestPed.cpp |
| 0x691AE0 | CTaskComplexGoPickUpEntity::CreateNextSubTask | ATAN2 | - | EXPLAINED(helper) | 1 | TaskComplexGoPickUpEntity.cpp |
| 0x691D50 | CTaskComplexGoPickUpEntity::ControlSubTask | ATAN2 GRABP | ATAN2 GRABP | OK | 1 | TaskComplexGoPickUpEntity.cpp |
| 0x693610 | CTaskComplexGoPickUpEntity::CreateFirstSubTask | ATAN2x3 | ATAN2 | EXPLAINED(helper) | 1 | TaskComplexGoPickUpEntity.cpp |
| 0x693C40 | CTaskSimpleHoldEntity::ProcessPed | ATAN2x2 | ATAN2 GETHDG | EXPLAINED(ext) | 1 | TaskSimpleHoldEntity.cpp |
| 0x6946F0 | CEntitySeekPosCalculatorRadiusAngleOffset::ComputeEntitySeekPos | GRABP LIMx2 | GRABP LIMx2 | OK | 1 | EntitySeekPosCalculatorRadiusAngleOffset.cpp |
| 0x6962A0 | CTaskComplexFollowLeaderInFormation::CreateSubTask | GRABP LIM | GRABP LIM | OK | 1 | TaskComplexFollowLeaderInFormation.cpp |
| 0x6973B0 | CTaskComplexWaitForBackup::CreateSubTask | GRABP LIM | GRABP LIM | OK | 1 | TaskComplexWaitForBackup.cpp |
| 0x697840 | CTaskComplexInvestigateDeadPed::CreateNextSubTask | GRABP | GRABP | OK | 1 | TaskComplexInvestigateDeadPed.cpp |
| 0x697B70 | CTaskComplexReactToGunAimedAt::CreateSubTask | GRABP LIM NODEH | GRABP LIM NODEH | OK | 1 | TaskComplexReactToGunAimedAt.cpp |
| 0x6CF590 | CTrailer::ProcessAI | ATAN2x2 | GETHDGx2 | EXPLAINED(ext) | 1 | Trailer.cpp |
| 0x6CFDF0 | CTrailer::SetTowLink | ATAN2 | GETHDG | EXPLAINED(ext) | 1 | Trailer.cpp |

rows with events: 90, {'EXPLAINED(template)': 3, 'EXPLAINED(ext)': 7, 'OK': 65, 'EXPLAINED(inlined-ctor)': 1, 'EXPLAINED(lambda)': 1, 'EXPLAINED(helper)': 10, 'EXPLAINED(paths)': 2, 'EXPLAINED(inline-wrap)': 1}

(`[strict ...]` = the exe scan with the exedis end-of-function rule when it differs from the loose scan.)

## OPEN (found by the argument-level pass, not fixed here - need own briefs)
1. CTaskSimplePlayerOnFoot::PlayerControlDucked 0x687F30: direction vector for IsPlayerAllowedToGoInThisDirection and the move vector - exe (-sin a, cos a, 0) with right/forward projections x = cos*r.y - sin*r.x; port TaskSimplePlayerOnFoot.cpp:1057/1070/1071 passes (0,-sin,cos) (sin/cos in the wrong slots).
2. CTaskSimpleSwim::ProcessControlInput 0x688A90: (a) 2-player direction vector same bug (TaskSimpleSwim.cpp:776); (b) mouse-3rd-person wrap: exe `fRotation += 2pi` (0x6891EF), port `360deg - aiming - current` (TaskSimpleSwim.cpp:880).
3. CTaskSimpleSwim::ProcessControlAI 0x689640: leader-sprint test uses the 2D magnitude vs 5.0 (0x689882), port `vecPosition.Magnitude()` is 3D (TaskSimpleSwim.cpp:711).
4. CEntitySeekPosCalculatorRadiusAngleOffset::ComputeEntitySeekPos 0x6946F0: for an entity without matrix the exe uses (-sin h, cos h) of the placement heading, port calls `GetForward()` on the null matrix.
(These four come from a sonnet helper's read; items 1-2 were not re-verified by me.)

## Rows
No exe-oracle rows were added: the fixed functions create tasks / read ped state (exe closure class C/D in ORACLE_COVERAGE), several are in `w2_skip.tsv` as "every case faults identically"; a cheap fixture does not exist.

## Gates
| gate | result |
|---|---|
| Release build (build/StandaloneRelease, worktree, all commits) | OK |
| run_all_tests build/StandaloneRW | 45 / 45 ok (26 oracle incl. wave2 1137 rows, 0 mismatches; 21 rw_* under the GPU lock; the 2 rw_skin oracle tests are in both groups) |
| soak 3 min, NOVSYNC, windowed, muted | RESULT: OK (0 errors, peak committed 547 MB) |
Unchanged by the fixes: the exe-oracle rows (the fixed functions are not oracle-able, see Rows). The user's gta_sa.set was restored from C:/gta-scratch/playtest1/gta_sa.set.
