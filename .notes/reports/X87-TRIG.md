# X87-TRIG - do all exe calls to the CRT trig entry points map to the same math function in the port?

## Result
* 3 port bugs fixed in 3 commits (one real asin/acos swap, one PitchForSlope reconstruction, one UCRT-vs-x87 acos); everything else classified (0 unexplained rows).
* Tools (committed): `tools/standalone/x87_trig_audit.py` (+ `x87_trig_explained.tsv`) = site -> exe fn -> port fn table; `tools/standalone/x87_trig_prove.py` = unicorn proof of the entry points.

## 1. CRT math entry points of the exe (proved with unicorn, `x87_trig_prove.py`, 0 mismatches)
Found by scanning every call/jmp target of .text whose code (3 levels deep) contains fsin/fcos/fptan/fpatan/fsqrt/fyl2x/f2xm1/fprem.
| address | function | calls in the exe | proof |
|---|---|---|---|
| 0x821E70 | `_CIasin` (arg ST0) | 58 | f(0.5)=0.5236, f(1)=pi/2 |
| 0x822380 | `_CIacos` (arg ST0) | 19 | f(0.5)=1.0472, f(0)=pi/2 |
| 0x822394 / 0x82239D | cdecl `acos(double)` / its body (0x82239D is entered by 0x822380) | 3 (GetPresetViewParams x2, one wrapper) | f(0.5)=1.0472 |
| 0x822130 | `_CIpow` (x=ST1, y=ST0) | 120 | pow(2,3)=8, **pow(10,4)=10000.000000000007** |
| 0x8220F0 | cdecl `pow(double,double)` | 30 | same |
| 0x825DB8.. | fmod family (fprem) | 5, all RenderWare range | not evaluated |

Tiny wrappers (`fld [esp+4]; call _CIasin/_CIacos/_CIpow; ret`: 0x420794, 0x509750, 0x48DE40, 0x48DE50, 0x46FFF0, 0x470000) are followed: their callers are the sites.
**There is no `_CIsin/_CIcos/_CItan/_CIatan2/_CIsqrt/_CIlog` call in the game code**: sin/cos/tan/atan/atan2/sqrt/log/log10 are INLINE x87 instructions (fsin, fcos, fptan, fpatan, fsqrt, fyl2x), which the audit compares too.
The 0x748000+ range (RenderWare, replaced by librw) holds 44 of the 248 sites; not audited.

## 2. Method (x87_trig_audit.py)
For each of the 9443 hook-registry functions (RH_Scoped*Install with an exe address): the multiset of math events of the exe function (CRT calls, wrapper calls, inline x87 ops; log base from the
fldlg2/fldln2/fld1 before fyl2x; the CRT asin/acos body inlined as x87 asm is recognised) vs the same events in the built port function (map-file symbol; calls of `x87::asin/acos/...`, UCRT
`__CIpow/__CIlog10/_fmodf...`, inline asm), also through helpers the exe inlined (callee expansion, depth 3). Critical kinds (ASIN ACOS POW FMOD EXP LOG10 LN LOG2) must be equal or explained by helpers;
sin/cos/tan/atan must be present in the port's call tree. Verdicts: OK / OK(helper) / OK(sqrt differs: informational, both correctly rounded) / EXPLAINED(tag) / NOPORT.
Exe function boundaries are heuristic (ret after the farthest forward branch; known starts = registry + call targets + vtable entries + padding; tail jumps excluded).

## 3. Numbers (Release build of the fixed tree)
736 registry functions have math events: OK 365, OK(helper) 61, OK(sqrt differs) 271, EXPLAINED 33 (COUNT-ONLY 15, POW-SQR 5, DUP-BRANCH 3, LOG10-CRT 3, POW-COUNT 3, FMOD-CRT 2, LN-REFACTOR 2), NOPORT 6.
Exe CRT sites in the game range: 204 (ASIN 53, ACOS 14, POW 137; 7 with an unregistered owner, see 5). Before the fixes: MISMATCH on ASIN/ACOS in PreRender (3/0 vs 0/3) and PitchForSlope (2/6).

## 4. Port bugs found and fixed
| exe fn | exe | port before | fix commit |
|---|---|---|---|
| CAutomobile::PreRender 0x6AAB50 | `_CIasin` at 0x6AC5F4, 0x6AC644, 0x6AC9B2 (tow-bar angles of MISC_B, combine tilt) | `x87::acos` at all three | 5baf428c |
| CPedIK::PitchForSlope 0x5FE0E0 alive branch | 2 asin (thigh up.z, stored as float) reused for thigh/calf/foot; `push 2` = POSTCONCAT; null `GetActiveTask()` -> normal branch | 6 asin on thigh AND foot matrices, left foot negated, max() on the wrong factor, PRECONCAT, null deref | aaf36c1c |
| GetPresetViewParams 0x619910 | x87 CRT acos (0x822394) at 0x619992, 0x619A5D | UCRT `std::acos` (SSE2) | b4b6265a |

PitchForSlope: line-by-line review of the whole alive branch against the asm by a read-only helper: all 7 RtQuatRotate calls match (bone, formula, operand order, rounding stores, combine op, order).
Minor deviations noted by that review and NOT fixed (rounding only): spine angle `(float)((-pitch*57.29578)*0.75)` rounds once in the exe; the position adjust 0x5FE6E6..0x5FE73A keeps `sin(pitch)*0.75` unrounded and rounds pos.z once.
No oracle row: none of the three functions is oracle-testable (needs ped / vehicle / RW camera world state); the proof is call-target identity (unicorn) + asm review.

## 5. Rows that are not plain OK
* DUP-BRANCH (CFileLoader::LoadObjectInstance 0x538090 ACOS 2/1, CCam::Process_FollowPed_SA 0x522d40 ASIN 5/3, CAutomobile::UpdateWheelMatrix 0x6AA290 ASIN 6/1): checked in the asm, the compiler duplicated `asin(clamp(v))` per clamp branch
  (asin(1.0f), asin(-1.0f), asin(v)); same argument expression, the port has one call. CPlane::PreRender and CWeapon::DoBulletImpact (lambda inlined twice) are the same pattern (OK(helper)).
* COUNT-ONLY (15 rows): same kinds (sin/cos/atan/tan), only the number of calls differs (value computed once in the port / inlined helper). Kind identity is what the audit guarantees; the arguments are not individually verified.
* POW / LOG10 / FMOD "same function, other implementation": the exe uses the x87 CRT (`pow(10,4)` = 10000.000000000007) or inline fyl2x/fprem, the port the UCRT (SSE2). Function identity holds, last-ulp results differ.
  FOLLOW-UP proposal **X87-POW**: `x87::pow` (the 0x822130 algorithm) and `x87::log10` at the ~150 `std::pow` / `std::log10` sites.
  POW-SQR: the exe has no pow where the port calls `pow(x, 2)` (fmul chain / constant) - same value for squares; CGlass::CalcAlphaWithNormal 0x71ACF0 `pow(specular, 6)` is the exe's 5-multiplication chain in a refactored function (rounding differs).
* LN-REFACTOR (CPostEffects::ImmediateModeFilterStuffInitialise / SetupBackBufferVertex): the exe rounds the raster size up to 2^n via ln/pow, the port uses `std::bit_ceil`; may differ for exact power-of-two sizes - follow-up.
* NOPORT: DoRWStuffStartOfFrame(+_Horizon), FrontendIdle, Idle (fptan = `SCREEN_VIEW_WINDOW` macro -> `x87::tan`, OK); FireOneInstantHitRound (asin at Weapon.cpp:2911, 1:1);
  FindPlayerHeading (exe inline atan; port `GetHeading()` -> CVector::Heading = atan2(-x, y): **Heading-vs-GetRadianAngleBetweenPoints questions are left to HEADING-AUDIT**).
* Unregistered owners: g14 script fn 0x48EAA0 ACOS@0x48F318 (port `AcosCrt`, Group13_15.cpp:564), g20 0x472310 POW@0x4731A4 (TextCommands.cpp:100 `std::pow(10, precision)`), GetPresetViewParams 0x619910 (fixed),
  0x5FD930 ACOS x2 = dead code (no references in the exe), 0x747DB0 POW = RenderWare range.

## 6. Gates
Release (StandaloneRelease) build OK at HEAD of the worktree; run_all_tests / soak: see the hand-back and status line.

## 7. Full table
736 rows: exe fn | function | verdict | exe events | port events | registry file:line. Regenerate: `py -3 -X utf8 -I tools/standalone/x87_trig_audit.py --port build/StandaloneRelease/bin/gta_reversed.exe --md out.md` (events: `x2` = count, `*` = UCRT call instead of x87 inline).

| exe fn | function | verdict | exe events | port events | registry file:line |
|---|---|---|---|---|---|
| 0x406d50 | CVector::Magnitude2D | OK | SQRT | SQRT* | source/game_sa/Core/Vector.cpp:19 |
| 0x4082c0 | CVector::Magnitude | OK | SQRT | SQRT* | source/game_sa/Core/Vector.cpp:18 |
| 0x409760 | CStreaming::DeleteLeastUsedEntityRwObject | OK(sqrt differs) | SQRT | - | source/game_sa/Streaming.cpp:40 |
| 0x40b650 | CStreaming::StreamVehiclesAndPeds_Always | OK(sqrt differs) | SQRT | - | source/game_sa/Streaming.cpp:116 |
| 0x4100e0 | CColSphere::IntersectEdge | OK | SQRTx2 | SQRT*x2 | source/game_sa/Collision/ColSphere.cpp:11 |
| 0x412130 | CCollision::ProcessSphereBox | OK | SQRT | SQRT* | source/game_sa/Collision/Collision.cpp:3613 |
| 0x412970 | CCollision::DistToMathematicalLine | OK | SQRT | SQRT* | source/game_sa/Collision/Collision.cpp:3638 |
| 0x412a30 | CCollision::DistToMathematicalLine2D | OK | SQRT | SQRT* | source/game_sa/Collision/Collision.cpp:3639 |
| 0x412aa0 | CCollision::ProcessLineSphere | OK(sqrt differs) | SQRT | - | source/game_sa/Collision/Collision.cpp:3610 |
| 0x413960 | CCollision::ProcessDiscCollision | OK | SQRT | SQRT* | source/game_sa/Collision/Collision.cpp:3598 |
| 0x416450 | CCollision::ProcessSphereSphere | OK(sqrt differs) | SQRT | - | source/game_sa/Collision/Collision.cpp:3616 |
| 0x4165b0 | CCollision::TestSphereTriangle | OK(sqrt differs) | SQRTx4 | - | source/game_sa/Collision/Collision.cpp:3618 |
| 0x416ba0 | CCollision::ProcessSphereTriangle | OK(sqrt differs) | SQRTx7 | - | source/game_sa/Collision/Collision.cpp:3619 |
| 0x417470 | CCollision::TestLineSphere | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Collision/Collision.cpp:3609 |
| 0x417610 | CCollision::DistToLine | OK(sqrt differs) | SQRTx3 | SQRT* | source/game_sa/Collision/Collision.cpp:3654 |
| 0x417fd0 | CCollision::ClosestPointOnLine | OK | SQRT | SQRT* | source/game_sa/Collision/Collision.cpp:3656 |
| 0x4185c0 | CCollision::ProcessColModels | OK | SQRTx2 | SQRT*x2 | source/game_sa/Collision/Collision.cpp:3621 |
| 0x41ac40 | CCollision::BuildCacheOfCameraCollision | OK | SQRT | SQRT* | source/game_sa/Collision/Collision.cpp:3665 |
| 0x41cd00 | CCarAI::EntitiesGoHeadOn | OK | COSx2 SINx2 | COSx2 SINx2 | source/game_sa/CarAI.cpp:35 |
| 0x41ce30 | CCarAI::GetCarToGoToCoors | OK(sqrt differs) | SQRTx10 | SQRT* | source/game_sa/CarAI.cpp:42 |
| 0x41cfb0 | CCarAI::GetCarToGoToCoorsStraightLine | OK(sqrt differs) | SQRTx8 | SQRT* | source/game_sa/CarAI.cpp:45 |
| 0x41d0e0 | CCarAI::GetCarToGoToCoorsAccurate | OK(sqrt differs) | SQRTx7 | SQRT* | source/game_sa/CarAI.cpp:43 |
| 0x41d210 | CCarAI::GetCarToGoToCoorsRacing | OK(sqrt differs) | SQRTx6 | SQRT* | source/game_sa/CarAI.cpp:44 |
| 0x41d350 | CCarAI::GetCarToParkAtCoors | OK(sqrt differs) | SQRTx5 | SQRT* | source/game_sa/CarAI.cpp:46 |
| 0x41d3d0 | CCarAI::MellowOutChaseSpeed | OK(sqrt differs) | SQRTx4 | - | source/game_sa/CarAI.cpp:48 |
| 0x41d660 | CCarAI::MakeWayForCarWithSiren | OK(sqrt differs) | SQRTx3 | - | source/game_sa/CarAI.cpp:47 |
| 0x41da30 | CCarAI::UpdateCarAI | OK(sqrt differs) | SQRTx27 | - | source/game_sa/CarAI.cpp:54 |
| 0x420860 | CVector2D::Magnitude | OK | SQRT | SQRT* | source/game_sa/Core/Vector2D.cpp:16 |
| 0x420b30 | CPlaceable::GetRoll | OK | ATAN SQRT | ATAN SQRT | source/game_sa/Entity/Placeable.cpp:20 |
| 0x422090 | CCarCtrl::FindPathDirection | OK | SQRTx2 | SQRT*x2 | source/game_sa/CarCtrl.cpp:207 |
| 0x422370 | CCarCtrl::FindGhostRoadHeight | OK | SQRTx2 | SQRT*x2 | source/game_sa/CarCtrl.cpp:208 |
| 0x422620 | CCarCtrl::FindPercDependingOnDistToLink | OK(sqrt differs) | SQRT | - | source/game_sa/CarCtrl.cpp:211 |
| 0x423000 | CCarCtrl::FlyAIPlaneInCertainDirection | OK | ASIN ATAN SQRT | ASIN ATAN SQRT* | source/game_sa/CarCtrl.cpp:254 |
| 0x423940 | CCarCtrl::FlyAIHeliToTarget_FixedOrientation | OK | COS SIN | COS SIN | source/game_sa/CarCtrl.cpp:259 |
| 0x424210 | CCarCtrl::GenerateCarCreationCoors2 | OK | SQRTx7 | SQRT*x7 | source/game_sa/CarCtrl.cpp:220 |
| 0x424f80 | CCarCtrl::PossiblyRemoveVehicle | OK | SQRTx2 | SQRT*x2 | source/game_sa/CarCtrl.cpp:264 |
| 0x426350 | CCarCtrl::WeaveForOtherCar | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:285 |
| 0x426970 | CCarCtrl::WeaveForPed | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:214 |
| 0x426bc0 | CCarCtrl::WeaveForObject | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:284 |
| 0x426ef0 | CCarCtrl::PickNextNodeToChaseCar | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:215 |
| 0x427fe0 | CCarCtrl::FindMaxSteerAngle | OK(sqrt differs) | SQRT | - | source/game_sa/CarCtrl.cpp:245 |
| 0x428040 | CCarCtrl::DealWithBend_Racing | OK(sqrt differs) | SQRTx4 | SQRT*x2 | source/game_sa/CarCtrl.cpp:217 |
| 0x428990 | CCarCtrl::SteerAICarWithPhysicsTryingToBlockTarget_Stop | OK(sqrt differs) | SQRTx2 | SQRT* | source/game_sa/CarCtrl.cpp:266 |
| 0x428be0 | CCarCtrl::SteerAIBoatWithPhysicsHeadingForTarget | OK(sqrt differs) | SQRTx2 | - | source/game_sa/CarCtrl.cpp:267 |
| 0x428de0 | CCarCtrl::SteerAIBoatWithPhysicsAttackingPlayer | OK(sqrt differs) | SQRTx3 | SQRT* | source/game_sa/CarCtrl.cpp:268 |
| 0x429090 | CCarCtrl::SteerAIBoatWithPhysicsCirclingPlayer | OK(sqrt differs) | SQRTx2 | - | source/game_sa/CarCtrl.cpp:269 |
| 0x429300 | CCarCtrl::TriggerDogFightMoves | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:270 |
| 0x429520 | CCarCtrl::TestWhetherToFirePlaneGuns | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:271 |
| 0x429600 | CCarCtrl::PossiblyFireHSMissile | OK(sqrt differs) | SQRT | - | source/game_sa/CarCtrl.cpp:236 |
| 0x429780 | CCarCtrl::GetAIPlaneToAttackPlayer | OK(sqrt differs) | SQRT | - | source/game_sa/CarCtrl.cpp:272 |
| 0x429890 | CCarCtrl::GetAIPlaneToDoDogFight | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:273 |
| 0x429a70 | CCarCtrl::FlyAIHeliInCertainDirection | OK | COS SIN SQRTx5 | COS SIN SQRT*x5 | source/game_sa/CarCtrl.cpp:274 |
| 0x42a630 | CCarCtrl::SteerAIHeliTowardsTargetCoors | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:275 |
| 0x42a750 | CCarCtrl::SteerAIHeliToFollowEntity | OK | SQRTx2 | SQRT*x2 | source/game_sa/CarCtrl.cpp:277 |
| 0x42aad0 | CCarCtrl::SteerAIHeliAsPoliceHeli | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:278 |
| 0x42acb0 | CCarCtrl::SteerAIHeliFlyingAwayFromPlayer | OK(sqrt differs) | SQRTx3 | - | source/game_sa/CarCtrl.cpp:279 |
| 0x42ad30 | CCarCtrl::SteerAIHeliToLand | OK(sqrt differs) | SQRTx3 | SQRT*x2 | source/game_sa/CarCtrl.cpp:280 |
| 0x42aeb0 | CCarCtrl::SteerAIHeliToKeepEntityInView | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:281 |
| 0x42b270 | CCarCtrl::FireHeliRocketsAtTarget | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:282 |
| 0x42b7d0 | CCarCtrl::GenerateOneEmergencyServicesCar | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:222 |
| 0x42bd20 | CCarCtrl::FindNodesThisCarIsNearestTo | OK(sqrt differs) | SQRT | SQRT*x2 | source/game_sa/CarCtrl.cpp:286 |
| 0x42c2b0 | CCarCtrl::CreatePoliceChase | OK(sqrt differs) | SQRT | - | source/game_sa/CarCtrl.cpp:219 |
| 0x42c740 | CCarCtrl::CreateConvoy | OK(sqrt differs) | SQRT | - | source/game_sa/CarCtrl.cpp:218 |
| 0x42cd10 | CCarCtrl::RemoveDistantCars | OK(sqrt differs) | SQRT | - | source/game_sa/CarCtrl.cpp:239 |
| 0x42d0e0 | CCarCtrl::SlowCarDownForOtherCar | OK(sqrt differs) | SQRTx2 | - | source/game_sa/CarCtrl.cpp:288 |
| 0x42ec90 | CCarCtrl::DragCarToPoint | OK | ATAN COS SIN SQRT | ATAN COS SIN SQRT* | source/game_sa/CarCtrl.cpp:295 |
| 0x42f3c0 | CCarCtrl::GetAIHeliToAttackPlayer | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:297 |
| 0x42f5a0 | CCarCtrl::JoinCarWithRoadSystem | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:298 |
| 0x430050 | CCarCtrl::GenerateOneRandomCar | OK(sqrt differs) | SQRTx13 | SQRT* | source/game_sa/CarCtrl.cpp:247 |
| 0x4322b0 | CCarCtrl::RemoveCarsIfThePoolGetsFull | OK(sqrt differs) | SQRT | - | source/game_sa/CarCtrl.cpp:238 |
| 0x432420 | CCarCtrl::SlowCarDownForCarsSectorList | OK(sqrt differs) | SQRT | - | source/game_sa/CarCtrl.cpp:242 |
| 0x4325c0 | CCarCtrl::FindAngleToWeaveThroughTraffic | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:225 |
| 0x432dd0 | CCarCtrl::SteerAICarWithPhysicsFollowPreRecordedPath | OK(sqrt differs) | SQRTx5 | SQRT*x4 | source/game_sa/CarCtrl.cpp:301 |
| 0x433280 | CCarCtrl::SteerAICarWithPhysicsHeadingForTarget | OK(sqrt differs) | SQRTx3 | SQRT*x2 | source/game_sa/CarCtrl.cpp:302 |
| 0x4335e0 | CCarCtrl::SteerAICarWithPhysicsTryingToBlockTarget | OK(sqrt differs) | SQRTx5 | SQRT* | source/game_sa/CarCtrl.cpp:308 |
| 0x4336d0 | CCarCtrl::SteerAICarTowardsPointInEscort | OK(sqrt differs) | SQRTx4 | SQRT*x3 | source/game_sa/CarCtrl.cpp:309 |
| 0x433ba0 | CCarCtrl::SteerAICarParkParallel | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:310 |
| 0x433ea0 | CCarCtrl::SteerAICarParkPerpendicular | OK | SQRT | SQRT* | source/game_sa/CarCtrl.cpp:311 |
| 0x434900 | CCarCtrl::SteerAICarWithPhysicsFollowPath | OK | SQRTx4 | SQRT*x4 | source/game_sa/CarCtrl.cpp:304 |
| 0x435830 | CCarCtrl::SteerAICarWithPhysicsFollowPath_Racing | OK(sqrt differs) | SQRTx4 | SQRT*x3 | source/game_sa/CarCtrl.cpp:305 |
| 0x436a90 | CCarCtrl::SteerAICarWithPhysics_OnlyMission | OK(sqrt differs) | SQRTx4 | SQRT*x8 | source/game_sa/CarCtrl.cpp:306 |
| 0x43ac40 | CConversationForPed::IsPlayerInPositionForConversation | OK | SQRTx2 | SQRT*x2 | source/game_sa/Conversations/ConversationForPed.cpp:9 |
| 0x43b0f0 | CPedToPlayerConversations::Update | OK | SQRT | SQRT* | source/game_sa/Conversations/PedToPlayerConversations.cpp:33 |
| 0x43c660 | CalcSpeedVariationInBend | OK(sqrt differs) | SQRT | - | source/game_sa/Curves.cpp:10 |
| 0x43c710 | CalcSpeedScaleFactor | OK(sqrt differs) | SQRT | - | source/game_sa/Curves.cpp:11 |
| 0x43c880 | CalcCorrectedDist | OK | COS SIN | COS SIN | source/game_sa/Curves.cpp:12 |
| 0x43c900 | CalcCurvePoint | OK(sqrt differs) | SQRT | - | source/game_sa/Curves.cpp:13 |
| 0x43eaf0 | CEntryExit::FindValidTeleportPoint | EXPLAINED(COUNT-ONLY) | COSx2 SINx2 | COS SIN | source/game_sa/EntryExit.cpp:23 |
| 0x43f4b0 | CEntryExitManager::FindNearestEntryExit | OK | SQRT | SQRT* | source/game_sa/EntryExitManager.cpp:22 |
| 0x43f630 | CEntryExitManager::FindNearestDoor | OK(sqrt differs) | SQRT | - | source/game_sa/EntryExitManager.cpp:21 |
| 0x4404a0 | CEntryExit::TransitionFinished | OK(helper) | - | COS SIN | source/game_sa/EntryExit.cpp:27 |
| 0x4418e0 | CGameLogic::CalcDistanceToForbiddenTrainCrossing | OK(sqrt differs) | SQRTx3 | - | source/game_sa/GameLogic.cpp:12 |
| 0x441b70 | CGameLogic::FindCityClosestToPoint | OK(sqrt differs) | SQRTx3 | SQRT* | source/game_sa/GameLogic.cpp:15 |
| 0x441db0 | CPlaceable::GetHeading | OK | ATAN | ATAN | source/game_sa/Entity/Placeable.cpp:19 |
| 0x441e10 | CGameLogic::IsPlayerAllowedToGoInThisDirection | OK(sqrt differs) | SQRT | - | source/game_sa/GameLogic.cpp:19 |
| 0x442480 | CGameLogic::UpdateSkip | OK(sqrt differs) | SQRTx2 | SQRT* | source/game_sa/GameLogic.cpp:38 |
| 0x442980 | CGameLogic::ResetStuffUponResurrection | OK(helper) | ATAN | - | source/game_sa/GameLogic.cpp:28 |
| 0x442ad0 | CGameLogic::Update | OK(helper) | ATANx3 SQRTx2 | SQRT* | source/game_sa/GameLogic.cpp:37 |
| 0x443b00 | CGangWars::PickZoneToAttack | OK | SQRT | SQRT* | source/game_sa/GangWars.cpp:46 |
| 0x443d50 | CGangWars::TellStreamingWhichGangsAreNeeded | OK | SQRT | SQRT* | source/game_sa/GangWars.cpp:56 |
| 0x444300 | CGangWars::StartDefensiveGangWar | OK | SQRT | SQRT* | source/game_sa/GangWars.cpp:51 |
| 0x444810 | CGangWars::CreateAttackWave | OK(sqrt differs) | SQRT | - | source/game_sa/GangWars.cpp:34 |
| 0x4453d0 | CGangWars::CreateDefendingGroup | OK(sqrt differs) | COS SIN SQRTx2 | COS SIN SQRT* | source/game_sa/GangWars.cpp:35 |
| 0x445b30 | CGangWars::AttackWaveOvercome | OK | SQRT | SQRT* | source/game_sa/GangWars.cpp:28 |
| 0x446610 | CGangWars::Update | OK(sqrt differs) | SQRTx2 | - | source/game_sa/GangWars.cpp:57 |
| 0x4471e0 | CGarages::AddOne | OK | SQRTx2 | SQRT*x2 | source/game_sa/Garages.cpp:14 |
| 0x4479f0 | CGarage::BuildRotatedDoorMatrix | OK | COS SIN | COS SIN | source/game_sa/Garage.cpp:10 |
| 0x449ff0 | CGarage::FindDoorsWithGarage | OK | SQRT | SQRT* | source/game_sa/Garage.cpp:27 |
| 0x44a240 | CGarages::FindGarageForObject | OK | SQRT | SQRT* | source/game_sa/Garages.cpp:20 |
| 0x44a3c0 | CGarages::StoreCarInNearestImpoundingGarage | OK | SQRT | SQRT* | source/game_sa/Garages.cpp:29 |
| 0x44e480 | CVector2D::Normalise | OK | SQRT | SQRT* | source/game_sa/Core/Vector2D.cpp:17 |
| 0x44e790 | CPathFind::GeneratePedCreationCoors | OK | SQRTx3 | SQRT*x3 | source/game_sa/PathFind.cpp:96 |
| 0x44efc0 | CPathFind::CalcRoadDensity | OK | SQRT | SQRT* | source/game_sa/PathFind.cpp:123 |
| 0x44f190 | CPathFind::CalcDistToAnyConnectingLinks | OK(sqrt differs) | SQRT | - | source/game_sa/PathFind.cpp:122 |
| 0x44fce0 | CPathFind::FindNodeClosestToCoorsFavourDirection | OK | SQRTx2 | SQRT*x2 | source/game_sa/PathFind.cpp:98 |
| 0x44fee0 | CPathFind::FindNodePairClosestToCoors | OK | ATAN SQRT | ATAN SQRT* | source/game_sa/PathFind.cpp:97 |
| 0x450320 | CPathFind::FindNodeOrientationForCarPlacement | OK | ATAN | ATAN | source/game_sa/PathFind.cpp:94 |
| 0x450780 | CPathFind::FindNodeCoorsForScript | OK | ATAN | ATAN* | source/game_sa/PathFind.cpp:90 |
| 0x450de0 | CPathFind::IsWaterNodeNearby | OK | SQRT | SQRT* | source/game_sa/PathFind.cpp:92 |
| 0x451b70 | CPathFind::FindNextNodeWandering | OK | COS SIN SQRTx2 | COS SIN SQRT*x2 | source/game_sa/PathFind.cpp:115 |
| 0x454c70 | CPickup::IsVisible | OK | SQRT | SQRT* | source/game_sa/Pickup.cpp:18 |
| 0x454d20 | CPickup::PickUpShouldBeInvisible | OK(sqrt differs) | SQRTx2 | SQRT* | source/game_sa/Pickup.cpp:19 |
| 0x454e80 | CPickups::DoMoneyEffects | OK(helper) | COS SIN | - | source/game_sa/Pickups.cpp:37 |
| 0x4555a0 | CPickups::TryToMerge_WeaponType | OK(sqrt differs) | SQRT | - | source/game_sa/Pickups.cpp:63 |
| 0x455720 | CPickups::DoPickUpEffects | OK | COS SINx2 SQRT | COS SINx2 SQRT* | source/game_sa/Pickups.cpp:38 |
| 0x455e20 | CPickups::DoCollectableEffects | OK(helper) | COS SINx2 SQRT | SIN | source/game_sa/Pickups.cpp:35 |
| 0x4560e0 | CPickups::DoMineEffects | OK(helper) | COS SINx2 SQRT | SIN | source/game_sa/Pickups.cpp:36 |
| 0x4563a0 | CPickups::RemoveUnnecessaryPickups | OK(sqrt differs) | SQRT | - | source/game_sa/Pickups.cpp:60 |
| 0x456450 | CPickups::TestForPickupsInBubble | OK(sqrt differs) | SQRT | - | source/game_sa/Pickups.cpp:62 |
| 0x456a70 | CPickups::PictureTaken | OK(sqrt differs) | SQRT | - | source/game_sa/Pickups.cpp:54 |
| 0x457410 | CPickup::Update | OK(sqrt differs) | SQRT | SQRT*x2 | source/game_sa/Pickup.cpp:23 |
| 0x458970 | CPickups::CreateSomeMoney | OK | COS SIN | COS SIN | source/game_sa/Pickups.cpp:33 |
| 0x458a80 | CPickups::CreatePickupCoorsCloseToCoors | OK(sqrt differs) | COS SIN SQRTx3 | COS SIN SQRT* | source/game_sa/Pickups.cpp:32 |
| 0x459d10 | CVehicleRecording::SkipForwardInRecording | OK | SQRTx2 | SQRT*x2 | source/game_sa/VehicleRecording.cpp:476 |
| 0x45a1e0 | CVehicleRecording::SetRecordingToPointClosestToCoors | OK(sqrt differs) | SQRT | - | source/game_sa/VehicleRecording.cpp:473 |
| 0x45d760 | CReplay::ProcessLookAroundCam | OK(helper) | ATAN COSx2 SINx2 SQRT | ATAN COSx3 SINx2 | source/game_sa/Replay.cpp:35 |
| 0x460850 | CRestart::FindClosestHospitalRestartPoint | OK(sqrt differs) | SQRTx2 | SQRT* | source/game_sa/Restart.cpp:22 |
| 0x460a50 | CRestart::FindClosestPoliceRestartPoint | OK(sqrt differs) | SQRTx2 | SQRT* | source/game_sa/Restart.cpp:23 |
| 0x4619c0 | CRoadBlocks::CreateRoadBlockBetween2Points | OK | SQRT | SQRT* | source/game_sa/RoadBlocks.cpp:24 |
| 0x4629e0 | CRoadBlocks::GenerateRoadBlocks | OK(sqrt differs) | SQRTx2 | SQRT* | source/game_sa/RoadBlocks.cpp:26 |
| 0x464980 | CTheScripts::DrawScriptSpritesAndRectangles | OK | COS SIN | COS SIN | source/game_sa/Scripts/TheScripts.cpp:93 |
| 0x465680 | CStuckCarCheck::Process | OK(sqrt differs) | SQRT | - | source/game_sa/StuckCarCheck.cpp:15 |
| 0x46b3d0 | CScriptsForBrains::IsObjectWithinBrainActivationRange | OK(sqrt differs) | SQRT | - | source/game_sa/ScriptsForBrains.cpp:22 |
| 0x485c20 | CTheScripts::ScriptDebugCircle2D | OK | COSx2 SINx2 | COSx2 SINx2 | source/game_sa/Scripts/TheScripts.cpp:94 |
| 0x487f60 | CRunningScript::CharInAngledAreaCheckCommand | OK(sqrt differs) | COS SIN SQRTx3 | COS SIN SQRT*x2 | source/game_sa/Scripts/RunningScript.cpp:72 |
| 0x4883f0 | CRunningScript::ObjectInAngledAreaCheckCommand | OK(sqrt differs) | COS SIN SQRTx3 | COS SIN SQRT*x2 | source/game_sa/Scripts/RunningScript.cpp:74 |
| 0x488780 | CRunningScript::FlameInAngledAreaCheckCommand | OK(sqrt differs) | COS SIN SQRTx4 | COS SIN SQRT*x2 | source/game_sa/Scripts/RunningScript.cpp:73 |
| 0x4939f0 | CTheScripts::ProcessAllSearchLights | OK(helper) | ATANx2 SQRTx5 | - | source/game_sa/Scripts/TheScripts.cpp:85 |
| 0x49fb30 | Fx_c::AddWheelSpray | OK(sqrt differs) | SQRTx4 | SQRT*x3 | source/game_sa/Fx/Fx.cpp:244 |
| 0x4a0610 | Fx_c::AddWheelSand | OK(sqrt differs) | SQRTx3 | - | source/game_sa/Fx/Fx.cpp:248 |
| 0x4a09c0 | Fx_c::AddWheelDust | OK(sqrt differs) | SQRTx3 | - | source/game_sa/Fx/Fx.cpp:249 |
| 0x4a2580 | FxEmitter_c::CreateParticle | OK(sqrt differs) | SQRT | - | source/game_sa/Fx/FxEmitter.cpp:21 |
| 0x4a55e0 | FxInfoFriction_c::GetValue | OK | POW | POW* | source/game_sa/Fx/Blueprints/FxInfoFriction.cpp:11 |
| 0x4a9140 | FxManager_c::CalcFrustumInfo | OK | ATAN SINx2 SQRTx2 | ATAN SINx2 SQRT*x2 | source/game_sa/Fx/FxManager.cpp:36 |
| 0x4ac050 | CEvent::CalcSoundLevelIncrement | OK | LOG10 POWx2 | LOG10* POW*x2 | source/game_sa/Events/Event.cpp:10 |
| 0x4b2850 | CEvent::GetSoundLevel | OK(sqrt differs) | LOG10 SQRT | LOG10* | source/game_sa/Events/Event.cpp:11 |
| 0x4b4ac0 | CEventKnockOffBike::SetPedSafePosition | OK(helper) | ATANx2 | - | source/game_sa/Events/EventKnockOffBike.cpp:20 |
| 0x4bae30 | CEventHandler::ComputeWaterCannonResponse | OK(sqrt differs) | SQRT | - | source/game_sa/Events/EventHandler.cpp:315 |
| 0x4bd6a0 | CEventHandler::ComputeVehicleCollisionResponse | OK(sqrt differs) | SQRT | - | source/game_sa/Events/EventHandler.cpp:307 |
| 0x4d00e0 | CalcThetaFromQuats | OK | ACOS SIN | ACOS SIN SQRTx0 | source/game_sa/Animation/AnimBlendNode.cpp:22 |
| 0x4d6e60 | CAEAmbienceTrackManager::UpdateAmbienceTrackAndVolume | OK | LOG10 SQRT | LOG10* SQRT* | source/game_sa/Audio/Managers/AEAmbienceTrackManager.cpp:19 |
| 0x4d80b0 | CAEAudioEnvironment::GetPositionRelativeToCamera | OK | SQRT | SQRT* | source/game_sa/Audio/AEAudioEnvironment.cpp:53 |
| 0x4d8990 | CAEAudioHardware::RescaleChannelVolumes | OK | POW | POW* | source/game_sa/Audio/Hardware/AEAudioHardware.cpp:39 |
| 0x4d9e50 | CAEAudioUtility::AudioLog10 | OK | LOG10 | LOG10 | source/game_sa/Audio/AEAudioUtility.cpp:52 |
| 0x4da540 | CAECollisionAudioEntity::UpdateLoopingCollisionSound | OK(helper) | LOG10 SQRTx2 | - | source/game_sa/Audio/Entities/AECollisionAudioEntity.cpp:24 |
| 0x4dae40 | CAECollisionAudioEntity::ReportWaterSplash | OK(sqrt differs) | LOG10x2 SQRT | LOG10x2 SQRT*x2 | source/game_sa/Audio/Entities/AECollisionAudioEntity.cpp:35 |
| 0x4db150 | CAECollisionAudioEntity::PlayOneShotCollisionSound | OK(helper) | LOG10 | - | source/game_sa/Audio/Entities/AECollisionAudioEntity.cpp:27 |
| 0x4db450 | CAECollisionAudioEntity::PlayLoopingCollisionSound | OK | LOG10 SQRTx2 | LOG10 SQRT*x2 | source/game_sa/Audio/Entities/AECollisionAudioEntity.cpp:28 |
| 0x4dcbe0 | CAEExplosionAudioEntity::AddAudioEvent | OK(sqrt differs) | SQRT | - | source/game_sa/Audio/Entities/AEExplosionAudioEntity.cpp:89 |
| 0x4df210 | CAEGlobalWeaponAudioEntity::ServiceAmbientGunFire | OK | COS SIN | COS SIN | source/game_sa/Audio/Entities/AEGlobalWeaponAudioEntity.cpp:310 |
| 0x4e0ee0 | CAEPedAudioEntity::UpdateJetPack | OK | SIN | SIN | source/game_sa/Audio/Entities/AEPedAudioEntity.cpp:84 |
| 0x4e2ee0 | CAEPedAudioEntity::Service | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Audio/Entities/AEPedAudioEntity.cpp:96 |
| 0x4ea010 | CAERadioTrackManager::UpdateRadioVolumes | EXPLAINED(LOG10-CRT) | LOG10x2 | LOG10* | source/game_sa/Audio/Managers/AERadioTrackManager.cpp:654 |
| 0x4eed10 | CAESmoothFadeThread::Service | OK | LOG10 POW | LOG10 POW* | source/game_sa/Audio/AESmoothFadeThread.cpp:28 |
| 0x4eee90 | CAESmoothFadeThread::SmoothFadeProc | OK | LOG10 POW | LOG10 POW* | source/game_sa/Audio/AESmoothFadeThread.cpp:26 |
| 0x4ef880 | CAESound::SetPosition | OK(sqrt differs) | SQRTx3 | - | source/game_sa/Audio/AESound.cpp:60 |
| 0x4efa10 | CAESound::CalculateVolume | OK(sqrt differs) | SQRT | - | source/game_sa/Audio/AESound.cpp:61 |
| 0x4f8070 | CAEVehicleAudioEntity::GetFreqForPlayerEngineSound | OK | SIN | SIN | source/game_sa/Audio/Entities/AEVehicleAudioEntity.cpp:5004 |
| 0x4f8940 | CAEVehicleAudioEntity::ProcessVehicleFlatTyre | OK(sqrt differs) | SQRT | - | source/game_sa/Audio/Entities/AEVehicleAudioEntity.cpp:5032 |
| 0x4f8b00 | CAEVehicleAudioEntity::ProcessVehicleRoadNoise | OK(sqrt differs) | SQRT | - | source/game_sa/Audio/Entities/AEVehicleAudioEntity.cpp:5033 |
| 0x4fd290 | CAEVehicleAudioEntity::ProcessPlayerProp | OK(sqrt differs) | SQRT | - | source/game_sa/Audio/Entities/AEVehicleAudioEntity.cpp:5043 |
| 0x4fd8f0 | CAEVehicleAudioEntity::ProcessDummyProp | OK(sqrt differs) | SQRT | - | source/game_sa/Audio/Entities/AEVehicleAudioEntity.cpp:5044 |
| 0x4fdfd0 | CAEVehicleAudioEntity::ProcessAIProp | OK(sqrt differs) | SQRT | - | source/game_sa/Audio/Entities/AEVehicleAudioEntity.cpp:5045 |
| 0x4fe420 | CAEVehicleAudioEntity::ProcessPlayerHeli | OK(sqrt differs) | SQRT | - | source/game_sa/Audio/Entities/AEVehicleAudioEntity.cpp:5047 |
| 0x4fe940 | CAEVehicleAudioEntity::ProcessDummyHeli | OK(sqrt differs) | SQRT | - | source/game_sa/Audio/Entities/AEVehicleAudioEntity.cpp:5049 |
| 0x4fee20 | CAEVehicleAudioEntity::ProcessAIHeli | OK(sqrt differs) | SQRT | - | source/game_sa/Audio/Entities/AEVehicleAudioEntity.cpp:5048 |
| 0x4ff320 | CAEVehicleAudioEntity::ProcessPlayerSeaPlane | OK(sqrt differs) | SQRT | - | source/game_sa/Audio/Entities/AEVehicleAudioEntity.cpp:5051 |
| 0x4ffdc0 | CAEVehicleAudioEntity::ProcessDummyBicycle | OK | SIN | SIN | source/game_sa/Audio/Entities/AEVehicleAudioEntity.cpp:5062 |
| 0x500040 | CAEVehicleAudioEntity::ProcessPlayerBicycle | OK | SIN | SIN | source/game_sa/Audio/Entities/AEVehicleAudioEntity.cpp:5063 |
| 0x503ce0 | CAEWeaponAudioEntity::PlayGunSounds | OK(sqrt differs) | LOG10x2 SQRT | LOG10*x2 | source/game_sa/Audio/Entities/AEWeaponAudioEntity.cpp:35 |
| 0x5052f0 | CAEWeatherAudioEntity::Service | OK | LOG10 | LOG10* | source/game_sa/Audio/Entities/AEWeatherAudioEntity.cpp:460 |
| 0x505a00 | CAEWeatherAudioEntity::UpdateParameters | OK(sqrt differs) | COSx3 LOG10x5 POW SINx3 | COSx3 LOG10*x5 POW* SINx3 SQRT*x7 | source/game_sa/Audio/Entities/AEWeatherAudioEntity.cpp:459 |
| 0x506800 | CAEWeatherAudioEntity::AddAudioEvent | OK | LOG10 | LOG10 | source/game_sa/Audio/Entities/AEWeatherAudioEntity.cpp:458 |
| 0x5083c0 | CAudioZones::Update | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Audio/AudioZones.cpp:13 |
| 0x50a0a0 | LimitPrecision | OK(helper) | POWx2 | - | source/game_sa/Cam.cpp:542 |
| 0x50a4f0 | CCam::RotCamIfInFrontCar | OK | ATAN COS SIN SQRT | ATAN COS SIN SQRT* | source/game_sa/Cam.cpp:509 |
| 0x50a9f0 | CCamera::CamShake | OK | SQRTx2 | SQRT*x2 | source/game_sa/Camera.cpp:130 |
| 0x50ad40 | CCamera::Find3rdPersonQuickAimPitch | OK | ATAN TAN | ATAN TAN | source/game_sa/Camera.cpp:134 |
| 0x50c290 | CCamera::StoreValuesDuringInterPol | OK(sqrt differs) | SQRT | - | source/game_sa/Camera.cpp:112 |
| 0x50cb90 | CCamera::SetNearClipBasedOnPedCollision | OK | SQRT | SQRT* | source/game_sa/Camera.cpp:161 |
| 0x50d350 | CCamera::ProcessVectorTrackLinear | OK | SIN | SIN | source/game_sa/Camera.cpp:203 |
| 0x50d430 | CCamera::ProcessVectorMoveLinear | OK | SIN | SIN | source/game_sa/Camera.cpp:205 |
| 0x50d510 | CCamera::ProcessFOVLerp | OK | SIN | SIN | source/game_sa/Camera.cpp:207 |
| 0x50dd70 | CCam::Finalise_DW_CineyCams | OK | COSx2 SINx2 | COSx2 SINx2 | source/game_sa/Cam.cpp:500 |
| 0x50e760 | CIdleCam::FinaliseIdleCamera | OK(helper) | COSx3 SINx3 | COSx4 SINx3 | source/game_sa/IdleCam.cpp:20 |
| 0x50eb70 | CCam::Process_1rstPersonPedOnPC | OK | ATAN COSx2 SINx2 SQRT | ATAN COSx2 SINx2 SQRT* | source/game_sa/Cam.cpp:515 |
| 0x50f970 | CCam::Process_FollowPedWithMouse | OK(helper) | ATANx2 COSx4 POW SINx2 SQRTx3 TANx2 | ATANx2 COSx3 POW* SINx2 SQRT*x2 TANx2 | source/game_sa/Cam.cpp:534 |
| 0x5105c0 | CCam::Process_M16_1stPerson | OK(helper) | ASINx4 ATANx2 COSx7 POWx3 SINx6 | ASINx4 ATANx5 COSx7 POW*x3 SINx6 | source/game_sa/Cam.cpp:536 |
| 0x511b50 | CCam::Process_Rocket | OK(helper) | COSx2 SINx2 | COSx3 SINx2 | source/game_sa/Cam.cpp:537 |
| 0x512110 | CCam::Process_WheelCam | OK(sqrt differs) | COSx2 SIN SQRT | COSx2 SIN | source/game_sa/Cam.cpp:539 |
| 0x512b10 | CCam::Process_AttachedCam | OK(sqrt differs) | COS SIN SQRT | COS SIN | source/game_sa/Cam.cpp:518 |
| 0x513510 | CCam::Process_Cam_TwoPlayer_Separate_Cars | OK | COS SIN SQRT | COS SIN SQRT* | source/game_sa/Cam.cpp:521 |
| 0x513be0 | CCam::Process_Cam_TwoPlayer_Separate_Cars_TopDown | OK(helper) | COS SIN SQRT | COSx2 SINx3 | source/game_sa/Cam.cpp:522 |
| 0x513e40 | CCam::Get_TwoPlayer_AimVector | OK | TAN | TAN | source/game_sa/Cam.cpp:504 |
| 0x514030 | CCamera::AvoidTheGeometry | OK(helper) | COSx2 POW SINx2 SQRTx6 TAN | COSx3 POW* SINx2 SQRT*x6 TAN | source/game_sa/Cam.cpp:552 |
| 0x514970 | CCamera::Find3rdPersonCamTargetVector | OK | TAN | TAN | source/game_sa/Camera.cpp:187 |
| 0x514d60 | CCamera::CalculateFrustumPlanes | OK | COS SIN | COS SIN | source/game_sa/Camera.cpp:189 |
| 0x5150e0 | CCamera::CalculateDerivedValues | OK | ATAN SQRT | ATAN SQRT* | source/game_sa/Camera.cpp:190 |
| 0x515200 | CCamera::StartTransition | OK | ATAN | ATAN | source/game_sa/Camera.cpp:165 |
| 0x515d80 | GetArrestCamPosBesideCop | OK | SQRTx2 | SQRT*x2 | source/game_sa/Cam.cpp:545 |
| 0x5161a0 | CCam::GetLookFromLampPostPos | OK | SQRT | SQRT* | source/game_sa/Cam.cpp:502 |
| 0x516440 | CCamera::ProcessVectorTrackLinear | OK | SIN | SIN | source/game_sa/Camera.cpp:204 |
| 0x5164a0 | CCamera::ProcessVectorMoveLinear | OK | SIN | SIN | source/game_sa/Camera.cpp:206 |
| 0x516500 | CCamera::ProcessFOVLerp | OK | SIN | SIN | source/game_sa/Camera.cpp:208 |
| 0x516560 | CCamera::ProcessJiggle | OK | COS SIN | COS SIN | source/game_sa/Camera.cpp:209 |
| 0x516ae0 | CCamera::ProcessScriptedCommands | OK(helper) | SINx2 | SINx3 | source/game_sa/Camera.cpp:113 |
| 0x516b20 | CCamera::ImproveNearClip | OK | SIN SQRTx2 | SIN SQRT*x2 | source/game_sa/Camera.cpp:191 |
| 0x517130 | CCam::GetCoreDataForDWCineyCamMode | OK(sqrt differs) | COSx2 SINx2 SQRTx2 | COSx2 SINx2 | source/game_sa/Cam.cpp:501 |
| 0x517400 | CCam::IsTimeToExitThisDWCineyCamMode | OK | SQRT | SQRT* | source/game_sa/Cam.cpp:505 |
| 0x517500 | CCam::Process_SpecialFixedForSyphon | OK | ATAN | ATAN | source/game_sa/Cam.cpp:538 |
| 0x517770 | CIdleCam::IsTargetValid | OK | SQRT | SQRT* | source/game_sa/IdleCam.cpp:22 |
| 0x5179e0 | CIdleCam::ProcessSlerp | OK | SIN SQRTx2 | SIN SQRT*x2 | source/game_sa/IdleCam.cpp:24 |
| 0x517bf0 | CIdleCam::ProcessFOVZoom | OK | SIN SQRT | SIN SQRT* | source/game_sa/IdleCam.cpp:25 |
| 0x517ea0 | CCam::Process_1stPerson | OK(sqrt differs) | SQRT | - | source/game_sa/Cam.cpp:516 |
| 0x519250 | CCam::ProcessPedsDeadBaby | OK | COS SIN | COS SIN | source/game_sa/Cam.cpp:514 |
| 0x519810 | CCam::Process_Cam_TwoPlayer_InCarAndShooting | OK(helper) | ATANx4 COS POWx4 SINx2 SQRTx4 TANx2 | ATANx3 COS POW*x4 SINx2 SQRT*x4 TANx2 | source/game_sa/Cam.cpp:520 |
| 0x51a740 | CCam::Process_DW_HeliChaseCam | OK | SINx3 SQRTx2 | SINx3 SQRT*x2 | source/game_sa/Cam.cpp:525 |
| 0x51b120 | CCam::Process_DW_CamManCam | OK(sqrt differs) | SINx2 SQRTx3 | SINx2 SQRT*x2 | source/game_sa/Cam.cpp:524 |
| 0x51b850 | CCam::Process_DW_BirdyCam | OK(helper) | SINx3 SQRTx2 | SIN | source/game_sa/Cam.cpp:523 |
| 0x51c250 | CCam::Process_DW_PlaneSpotterCam | OK(sqrt differs) | SINx2 SQRTx2 | SINx2 SQRT* | source/game_sa/Cam.cpp:529 |
| 0x51c760 | CCam::Process_DW_PlaneCam1 | OK | SIN | SIN | source/game_sa/Cam.cpp:526 |
| 0x51cc30 | CCam::Process_DW_PlaneCam2 | OK | SIN | SIN | source/game_sa/Cam.cpp:527 |
| 0x51d470 | CCam::Process_Fixed | OK(sqrt differs) | SQRT | - | source/game_sa/Cam.cpp:531 |
| 0x51e560 | CCamera::TryToStartNewCamMode | OK(helper) | COSx4 SINx4 | - | source/game_sa/Camera.cpp:195 |
| 0x520190 | CCamera::CameraColDetAndReact | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Camera.cpp:196 |
| 0x520690 | CCam::LookBehind | OK | COS SIN | COS SIN | source/game_sa/Cam.cpp:507 |
| 0x520e40 | CCam::LookRight | OK | COS SIN | COS SIN | source/game_sa/Cam.cpp:508 |
| 0x521500 | CCam::Process_AimWeapon | OK(helper) | ACOS ASINx3 ATANx6 COSx5 POWx3 SINx2 SQRTx3 TAN | ACOS ASINx3 ATANx6 COSx4 POW*x3 SINx2 SQRT*x5 TAN | source/game_sa/Cam.cpp:517 |
| 0x522d40 | CCam::Process_FollowPed_SA | EXPLAINED(DUP-BRANCH) | ASINx5 ATANx3 COSx7 POWx3 SINx7 SQRT | ASINx3 ATANx3 COSx7 POW*x3 SINx7 SQRT*x3 | source/game_sa/Cam.cpp:535 |
| 0x5245b0 | CCam::Process_FollowCar_SA | OK(helper) | ASINx2 ATANx7 COSx10 POWx3 SINx7 SQRTx6 | ASINx2 ATANx7 COSx9 POW*x3 SINx7 SQRT*x6 | source/game_sa/Cam.cpp:533 |
| 0x525e50 | CCam::Process_Cam_TwoPlayer | OK | ATAN POW SQRT | ATAN POW* SQRT* | source/game_sa/Cam.cpp:519 |
| 0x526fc0 | CCam::Process | OK | POWx3 SQRTx3 | POW*x3 SQRT*x3 | source/game_sa/Cam.cpp:511 |
| 0x527fa0 | CCamera::CamControl | OK(helper) | ATAN COS SIN SQRTx2 | - | source/game_sa/Camera.cpp:197 |
| 0x52b730 | CCamera::Process | OK(helper) | COSx8 SINx4 SQRTx4 | COSx6 SINx2 SQRT*x2 | source/game_sa/Camera.cpp:198 |
| 0x533790 | CEntity::CreateEffects | EXPLAINED(COUNT-ONLY) | ATANx2 | - | source/game_sa/Entity/Entity.cpp:69 |
| 0x534e90 | CEntity::ModifyMatrixForTreeInWind | EXPLAINED(COUNT-ONLY) | SINx2 | SIN | source/game_sa/Entity/Entity.cpp:80 |
| 0x535040 | CEntity::ModifyMatrixForBannerInWind | EXPLAINED(POW-SQR) | SQRT | POW* SQRT* | source/game_sa/Entity/Entity.cpp:81 |
| 0x535340 | CEntity::CalculateBBProjection | OK(sqrt differs) | SQRTx3 | - | source/game_sa/Entity/Entity.cpp:83 |
| 0x535fa0 | CEntity::PreRender | OK(sqrt differs) | SQRTx3 | SQRT* | source/game_sa/Entity/Entity.cpp:50 |
| 0x538090 | CFileLoader::LoadObjectInstance | EXPLAINED(DUP-BRANCH) | ACOSx2 | ACOS | source/game_sa/FileLoader.cpp:53 |
| 0x5397f0 | CFireManager::GetNumFiresInRange | OK(sqrt differs) | SQRT | - | source/game_sa/FireManager.cpp:26 |
| 0x53a570 | CFire::ProcessFire | OK(sqrt differs) | SQRTx3 | - | source/game_sa/Fire.cpp:15 |
| 0x53af00 | CFireManager::Update | EXPLAINED(COUNT-ONLY) | COSx2 SINx2 SQRT | COS SIN | source/game_sa/FireManager.cpp:34 |
| 0x53cbe0 | GetRadianAngleBetweenPoints | OK | ATAN | ATAN | source/game_sa/General.cpp:25 |
| 0x53cc70 | GetATanOfXY | OK | ATANx8 | ATANx8 | source/game_sa/General.cpp:26 |
| 0x53ce30 | SolveQuadratic | OK | SQRT | SQRT* | source/game_sa/General.cpp:28 |
| 0x53d0b0 | CLoadMonitor::EndFrame | OK(sqrt differs) | SQRT | - | source/game_sa/LoadMonitor.cpp:13 |
| 0x53d690 | DoRWStuffStartOfFrame | NOPORT | TAN | - | source/app/app.cpp:23 |
| 0x53d7a0 | DoRWStuffStartOfFrame_Horizon | NOPORT | TAN | - | source/app/app.cpp:24 |
| 0x53e770 | FrontendIdle | NOPORT | TAN | - | source/app/app_game.cpp:46 |
| 0x53e920 | Idle | NOPORT | TAN | - | source/app/app_game.cpp:45 |
| 0x53ec10 | AppEventHandler | OK | TAN | TAN | source/app/app.cpp:28 |
| 0x53f9a0 | CPad::StartShake_Distance | OK(sqrt differs) | SQRT | - | source/game_sa/Pad.cpp:48 |
| 0x53fa70 | CPad::StartShake_Train | OK | SQRT | SQRT* | source/game_sa/Pad.cpp:49 |
| 0x544c40 | CPhysical::ApplyAirResistance | OK | POWx2 SQRT | POW*x2 SQRT* | source/game_sa/Entity/Physical.cpp:143 |
| 0x5454c0 | CPhysical::ApplyFriction | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Entity/Physical.cpp:145 |
| 0x545980 | CPhysical::ApplyFriction | OK(sqrt differs) | SQRTx8 | - | source/game_sa/Entity/Physical.cpp:146 |
| 0x546670 | CPhysical::ProcessShiftSectorList | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Physical.cpp:147 |
| 0x546ff0 | CPhysical::PositionAttachedEntity | OK(helper) | ATANx2 SQRT | SQRT* | source/game_sa/Entity/Physical.cpp:161 |
| 0x547b80 | CPhysical::ApplySpeed | OK(helper) | ATAN | - | source/game_sa/Entity/Physical.cpp:149 |
| 0x5483d0 | CPhysical::ApplyFriction | OK | POWx2 | POW*x2 | source/game_sa/Entity/Physical.cpp:151 |
| 0x54db10 | CPhysical::ProcessShift | OK(sqrt differs) | POWx2 SQRT | POW*x2 | source/game_sa/Entity/Physical.cpp:120 |
| 0x54dfb0 | CPhysical::ProcessCollision | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Physical.cpp:119 |
| 0x54f3b0 | CPlaceable::RemoveMatrix | OK | ATAN | ATAN | source/game_sa/Entity/Placeable.cpp:23 |
| 0x54f610 | CPlaceable::SetMatrix | OK | ATAN | ATAN | source/game_sa/Entity/Placeable.cpp:26 |
| 0x553aa0 | CRenderer::RenderEverythingBarRoads | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Renderer.cpp:41 |
| 0x554230 | CRenderer::SetupEntityVisibility | OK(sqrt differs) | SQRT | - | source/game_sa/Renderer.cpp:44 |
| 0x554650 | CRenderer::SetupBigBuildingVisibility | OK(sqrt differs) | SQRT | - | source/game_sa/Renderer.cpp:46 |
| 0x554840 | CRenderer::ScanSectorList | OK | ATAN | ATAN | source/game_sa/Renderer.cpp:50 |
| 0x554b10 | CRenderer::ScanBigBuildingList | OK | ATAN | ATAN | source/game_sa/Renderer.cpp:51 |
| 0x554eb0 | CRenderer::ShouldModelBeStreamed | OK(sqrt differs) | SQRT | - | source/game_sa/Renderer.cpp:52 |
| 0x5556e0 | CRenderer::ConstructRenderList | OK(helper) | ATAN | - | source/game_sa/Renderer.cpp:54 |
| 0x5561b0 | CRope::UpdateWeightInRope | OK(sqrt differs) | SQRTx7 | SQRT*x4 | source/game_sa/Rope.cpp:16 |
| 0x556800 | CRope::Render | OK(sqrt differs) | SQRT | - | source/game_sa/Rope.cpp:18 |
| 0x557530 | CRope::Update | OK | POW SQRTx5 | POW* SQRT*x5 | source/game_sa/Rope.cpp:20 |
| 0x55bf20 | CStats::UpdateSexAppealStat | OK | SQRT | SQRT* | source/game_sa/Stats.cpp:118 |
| 0x55cac0 | CStats::UpdateStatsWhenDriving | OK | SQRT | SQRT* | source/game_sa/Stats.cpp:122 |
| 0x55cc00 | CStats::UpdateStatsWhenFlying | OK | SQRT | SQRT* | source/game_sa/Stats.cpp:123 |
| 0x55cd60 | CStats::UpdateStatsWhenOnMotorBike | OK(sqrt differs) | SQRT | - | source/game_sa/Stats.cpp:111 |
| 0x55ffd0 | CTimeCycle::FindTimeCycleBox | OK | SQRT | SQRT* | source/game_sa/TimeCycle.cpp:24 |
| 0x5603d0 | CTimeCycle::CalcColoursForPoint | OK | COS SIN | COS SIN | source/game_sa/TimeCycle.cpp:22 |
| 0x5625f0 | CWanted::WorkOutPolicePresence | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Wanted.cpp:38 |
| 0x566c90 | CWorld::FindUnsuspectingTargetCar | OK | SQRT | SQRT* | source/game_sa/World.cpp:68 |
| 0x566da0 | CWorld::FindUnsuspectingTargetPed | OK | SQRT | SQRT* | source/game_sa/World.cpp:67 |
| 0x5684a0 | CWorld::Process | OK | POW | POW* | source/game_sa/World.cpp:50 |
| 0x56e450 | FindPlayerHeading | NOPORT | ATANx3 | - | source/game_sa/World.cpp:126 |
| 0x56ec80 | CPlayerInfo::ProcessCarGunCrosshair_Hook | EXPLAINED(COUNT-ONLY) | TANx2 | - | source/game_sa/PlayerInfo.cpp:55 |
| 0x56ef90 | CPlayerInfo::DrawCrosshair_Hook | EXPLAINED(COUNT-ONLY) | COS SINx2 | - | source/game_sa/PlayerInfo.cpp:56 |
| 0x56f8d0 | CPlayerInfo::Process | OK(sqrt differs) | SQRT | - | source/game_sa/PlayerInfo.cpp:33 |
| 0x5725b0 | Calc2DDistanceBetween2Zones | OK(sqrt differs) | SQRTx2 | SQRT* | source/game_sa/TheZones.cpp:34 |
| 0x5832f0 | CRadar::LimitRadarPoint | OK(sqrt differs) | SQRTx2 | SQRT | source/game_sa/Radar.cpp:108 |
| 0x583670 | CRadar::CalculateCachedSinCos | OK(helper) | ATANx4 COSx4 SINx4 SQRTx2 | ATANx2 COSx4 SINx4 | source/game_sa/Radar.cpp:115 |
| 0x584850 | CRadar::DrawRotatingRadarSprite | OK | COS SIN | COS SIN | source/game_sa/Radar.cpp:133 |
| 0x584960 | CRadar::DrawYouAreHereSprite | OK | COS SIN | COS SIN | source/game_sa/Radar.cpp:118 |
| 0x585700 | CRadar::DrawRadarMask | OK | COS SIN | COS SIN | source/game_sa/Radar.cpp:141 |
| 0x586650 | CRadar::DrawRadarGangOverlay | EXPLAINED(COUNT-ONLY) | SINx2 | SIN | source/game_sa/Radar.cpp:163 |
| 0x586880 | CRadar::DrawRadarMap | OK(helper) | ATAN COSx2 SINx4 SQRTx13 | ATAN COSx2 SINx3 | source/game_sa/Radar.cpp:128 |
| 0x586b00 | CRadar::DrawMap | OK(sqrt differs) | SQRT | - | source/game_sa/Radar.cpp:127 |
| 0x587000 | CRadar::DrawEntityBlip | OK | COS SIN SQRTx5 | COS SIN SQRT*x5 | source/game_sa/Radar.cpp:164 |
| 0x587d20 | CRadar::SetupAirstripBlips | OK | SQRT | SQRT* | source/game_sa/Radar.cpp:154 |
| 0x588050 | CRadar::DrawBlips | OK(helper) | SQRTx2 | COS SIN | source/game_sa/Radar.cpp:155 |
| 0x58a330 | CHud::DrawRadar | OK | ATAN COS SIN | ATAN COS SIN | source/game_sa/Hud.cpp:58 |
| 0x59a840 | CMatrix::ConvertToEulerAngles | OK | ATANx9 SQRTx2 | ATANx9 SQRTx2 | source/game_sa/Core/Matrix.cpp:63 |
| 0x59aa40 | CMatrix::ConvertFromEulerAngles | OK | COSx3 SINx3 | COSx3 SINx3 | source/game_sa/Core/Matrix.cpp:64 |
| 0x59ac90 | CMaths::InitMathsTables | EXPLAINED(COUNT-ONLY) | SIN | - | source/game_sa/Maths.cpp:58 |
| 0x59afa0 | CMatrix::SetRotateXOnly | OK | COS SIN | COS SIN | source/game_sa/Core/Matrix.cpp:47 |
| 0x59afe0 | CMatrix::SetRotateYOnly | OK | COS SIN | COS SIN | source/game_sa/Core/Matrix.cpp:48 |
| 0x59b020 | CMatrix::SetRotateZOnly | OK | COS SIN | COS SIN | source/game_sa/Core/Matrix.cpp:49 |
| 0x59b060 | CMatrix::SetRotateX | OK | COS SIN | COS SIN | source/game_sa/Core/Matrix.cpp:51 |
| 0x59b0a0 | CMatrix::SetRotateY | OK | COS SIN | COS SIN | source/game_sa/Core/Matrix.cpp:52 |
| 0x59b0e0 | CMatrix::SetRotateZ | OK | COS SIN | COS SIN | source/game_sa/Core/Matrix.cpp:53 |
| 0x59b120 | CMatrix::SetRotate | OK | COSx3 SINx3 | COSx3 SINx3 | source/game_sa/Core/Matrix.cpp:50 |
| 0x59b1e0 | CMatrix::RotateX | OK | COS SIN | COS SIN | source/game_sa/Core/Matrix.cpp:54 |
| 0x59b2c0 | CMatrix::RotateY | OK | COS SIN | COS SIN | source/game_sa/Core/Matrix.cpp:55 |
| 0x59b390 | CMatrix::RotateZ | OK | COS SIN | COS SIN | source/game_sa/Core/Matrix.cpp:56 |
| 0x59b460 | CMatrix::Rotate | OK | COSx3 SINx3 | COSx3 SINx3 | source/game_sa/Core/Matrix.cpp:57 |
| 0x59b6a0 | CMatrix::Reorthogonalise | OK | SQRTx2 | SQRT*x2 | source/game_sa/Core/Matrix.cpp:58 |
| 0x59c160 | CQuaternion::Get | OK | ATANx3 COS SIN | ATANx3 COS SIN | source/game_sa/Core/Quaternion.cpp:19 |
| 0x59c230 | CQuaternion::Get | OK | ACOS SIN | ACOS SIN | source/game_sa/Core/Quaternion.cpp:20 |
| 0x59c300 | CQuaternion::Slerp | OK | SINx4 | SINx4 | source/game_sa/Core/Quaternion.cpp:22 |
| 0x59c3e0 | CQuaternion::Set | OK | SQRTx4 | SQRT*x4 | source/game_sa/Core/Quaternion.cpp:23 |
| 0x59c530 | CQuaternion::Set | OK | COSx3 SINx3 | COSx3 SINx3 | source/game_sa/Core/Quaternion.cpp:24 |
| 0x59c600 | CQuaternion::Set | OK | COS SIN | COS SIN | source/game_sa/Core/Quaternion.cpp:25 |
| 0x59c630 | CQuaternion::Slerp | OK | ACOS SINx5 | ACOS SINx5 SQRTx0 | source/game_sa/Core/Quaternion.cpp:26 |
| 0x59c910 | CVector::Normalise | OK | SQRT | SQRT* | source/game_sa/Core/Vector.cpp:20 |
| 0x59c970 | CVector::NormaliseAndMag | OK | SQRT | SQRT* | source/game_sa/Core/Vector.cpp:21 |
| 0x59e220 | BreakObject_c::Update | OK | ACOS | ACOS | source/game_sa/BreakObject_c.cpp:17 |
| 0x59f200 | CObject::SetMatrixForTrainCrossing | OK | COS SIN | COS SIN | source/game_sa/Entity/Object/Object.cpp:65 |
| 0x5a02e0 | CObject::SpecialEntityCalcCollisionSteps | OK | SQRTx5 | SQRT*x5 | source/game_sa/Entity/Object/Object.cpp:35 |
| 0x5a0760 | CObject::SetIsStatic | OK | ATAN | ATAN | source/game_sa/Entity/Object/Object.cpp:26 |
| 0x5a07d0 | CObject::ProcessSamSiteBehaviour | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Object/Object.cpp:55 |
| 0x5a0b50 | CObject::ProcessTrainCrossingBehaviour | OK(sqrt differs) | ACOS SQRT | ACOS | source/game_sa/Entity/Object/Object.cpp:56 |
| 0x5a2130 | CObject::ProcessControl | OK(helper) | ATAN POWx2 | POW*x2 | source/game_sa/Entity/Object/Object.cpp:28 |
| 0x5a32d0 | ProcSurfaceInfo_c::AddObject | OK(sqrt differs) | COS SIN SQRT | COS SIN | source/game_sa/Plant/ProcSurfaceInfo.cpp:13 |
| 0x5a3850 | ProcSurfaceInfo_c::AddObjects | OK(sqrt differs) | SQRT | - | source/game_sa/Plant/ProcSurfaceInfo.cpp:14 |
| 0x5b25f0 | CCam::Process_FlyBy | OK | COSx2 SINx2 | COSx2 SINx2 | source/game_sa/Cam.cpp:532 |
| 0x5b97f0 | CAEAudioUtility::StaticInitialise | OK | LOG10 POW | LOG10* POW* | source/game_sa/Audio/AEAudioUtility.cpp:59 |
| 0x5bbac0 | CTimeCycle::Initialise | OK | COSx2 SINx2 | COSx2 SINx2 | source/game_sa/TimeCycle.cpp:15 |
| 0x5bd830 | cHandlingDataMgr::LoadHandlingData | EXPLAINED(POW-COUNT) | POWx18 | - | source/game_sa/cHandlingDataMgr.cpp:46 |
| 0x5d9900 | CCustomCarEnvMapPipeline::CustomPipeRenderCB | EXPLAINED(FMOD-CRT) | - | FMOD*x3 | source/game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline.cpp:20 |
| 0x5dad00 | CGrassRenderer::DrawTriPlants | OK(sqrt differs) | SQRTx2 | - | source/game_sa/GrassRenderer.cpp:16 |
| 0x5db3d0 | CPlantMgr::CalculateWindBending | EXPLAINED(COUNT-ONLY) | SINx2 | SIN | source/game_sa/PlantMgr.cpp:131 |
| 0x5dbe40 | CPlantMgr::_CalcDistanceSqrToEntity | OK(sqrt differs) | SQRT | - | source/game_sa/PlantMgr.cpp:144 |
| 0x5dc210 | GetPlantDensity | OK(sqrt differs) | SQRT | - | source/game_sa/Plant/PlantLocTri.cpp:21 |
| 0x5dc290 | CPlantLocTri::Add | OK(sqrt differs) | SQRT | - | source/game_sa/Plant/PlantLocTri.cpp:18 |
| 0x5def60 | CPed::GetLocalDirection | OK | ATAN | ATAN | source/game_sa/Entity/Ped/Ped.cpp:157 |
| 0x5df910 | CPed::SetPedPositionInCar | OK(helper) | ATANx5 COSx6 SINx5 | ATANx5 COSx5 SINx4 | source/game_sa/Entity/Ped/Ped.cpp:199 |
| 0x5dfdf0 | CPed::PositionAttachedPed | OK | ATAN | ATAN | source/game_sa/Entity/Ped/Ped.cpp:201 |
| 0x5e0820 | CPed::PositionPedOutOfCollision | OK | ATAN SQRTx2 | ATAN SQRT*x2 | source/game_sa/Entity/Ped/Ped.cpp:145 |
| 0x5e1660 | CPed::OurPedCanSeeThisEntity | OK | SQRT | SQRT | source/game_sa/Entity/Ped/Ped.cpp:75 |
| 0x5e17e0 | CPed::SortPeds | OK | SQRTx3 | SQRT*x3 | source/game_sa/Entity/Ped/Ped.cpp:144 |
| 0x5e1b10 | CPed::UpdatePosition | OK | SQRTx2 | SQRT*x2 | source/game_sa/Entity/Ped/Ped.cpp:105 |
| 0x5e1fa0 | CPed::ProcessBuoyancy | OK | POW | POW* | source/game_sa/Entity/Ped/Ped.cpp:143 |
| 0x5e2530 | CPed::ProcessEntityCollision | OK | ATAN SQRTx3 | ATAN SQRT*x3 | source/game_sa/Entity/Ped/Ped.cpp:215 |
| 0x5e3e90 | CPed::SpecialEntityCalcCollisionSteps | OK | SQRT | SQRT* | source/game_sa/Entity/Ped/Ped.cpp:209 |
| 0x5e4c50 | CPed::CalculateNewVelocity | EXPLAINED(POW-COUNT) | ASINx2 POWx2 SQRTx2 | ASINx2 POW* SQRT*x2 | source/game_sa/Entity/Ped/Ped.cpp:99 |
| 0x5e5380 | CPed::DoFootLanded | OK | SQRT | SQRT* | source/game_sa/Entity/Ped/Ped.cpp:96 |
| 0x5e57f0 | CPed::PlayFootSteps | EXPLAINED(LOG10-CRT) | LOG?x2 | LOG10*x2 | source/game_sa/Entity/Ped/Ped.cpp:95 |
| 0x5e65a0 | CPed::PreRenderAfterTest | OK(sqrt differs) | SIN | SIN SQRT* | source/game_sa/Entity/Ped/Ped.cpp:135 |
| 0x5e8cd0 | CPed::ProcessControl | OK | POW | POW* | source/game_sa/Entity/Ped/Ped.cpp:206 |
| 0x5ea390 | PlacePedAtEffect | OK(helper) | - | ATAN | source/game_sa/Attractors/PedAttractorPedPlacer.cpp:12 |
| 0x5ef420 | CPedShelterAttractor::GetDisplacement | OK | COS SIN | COS SIN | source/game_sa/Attractors/PedShelterAttractor.cpp:94 |
| 0x5f13f0 | CPedGeometryAnalyser::ComputePedShotSide | OK | ATAN | ATAN | source/game_sa/PedGeometryAnalyser.cpp:45 |
| 0x5f1500 | CPedGeometryAnalyser::ComputeEntityDirs | OK | COSx2 SINx2 | COSx2 SINx2 | source/game_sa/PedGeometryAnalyser.cpp:39 |
| 0x5f2c10 | CPedGeometryAnalyser::ComputeClosestSurfacePoint | OK(sqrt differs) | SQRT | - | source/game_sa/PedGeometryAnalyser.cpp:26 |
| 0x5f2f70 | CPedGeometryAnalyser::IsWanderPathClear | OK(sqrt differs) | SQRT | - | source/game_sa/PedGeometryAnalyser.cpp:54 |
| 0x5f32d0 | CPedGeometryAnalyser::CanPedJumpObstacle | OK(sqrt differs) | SQRT | - | source/game_sa/PedGeometryAnalyser.cpp:19 |
| 0x5f3970 | CPedGeometryAnalyser::IsEntityBlockingTarget | OK | SQRT | SQRT* | source/game_sa/PedGeometryAnalyser.cpp:52 |
| 0x5f3c20 | CPedGeometryAnalyser::ComputeEntityBoundingSphere | OK | SQRT | SQRT* | source/game_sa/PedGeometryAnalyser.cpp:37 |
| 0x5f3dd0 | CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox | OK(sqrt differs) | SQRTx3 | - | source/game_sa/PedGeometryAnalyser.cpp:47 |
| 0x5f5a30 | CPedGeometryAnalyser::GetIsLineOfSightClear | OK(sqrt differs) | SQRT | - | source/game_sa/PedGeometryAnalyser.cpp:49 |
| 0x5fb010 | CPedGroup::FindDistanceToFurthestMember | OK | SQRT | SQRT* | source/game_sa/PedGroup.cpp:181 |
| 0x5fb0a0 | CPedGroup::FindDistanceToNearestMember | OK | SQRT | SQRT* | source/game_sa/PedGroup.cpp:182 |
| 0x5fc9b0 | CPedGroupPlacer::PlaceFormationGroup | OK(sqrt differs) | SQRT | - | source/game_sa/PedGroupPlacer.cpp:17 |
| 0x5fce80 | CPedGroupPlacer::PlaceChatGroup | OK(sqrt differs) | COSx2 SIN SQRTx2 | COSx2 SIN SQRT* | source/game_sa/PedGroupPlacer.cpp:18 |
| 0x5fd330 | CPedGroupPlacer::PlaceRandomGroup | OK(sqrt differs) | COSx2 SIN SQRTx2 | COSx2 SIN SQRT* | source/game_sa/PedGroupPlacer.cpp:19 |
| 0x5fdc00 | CPedIK::PointGunInDirection | OK | ATAN COSx2 SINx2 | ATAN COSx2 SINx2 | source/game_sa/PedIK.cpp:35 |
| 0x5fde20 | CPedIK::PointGunAtPosition | OK | SQRT | SQRT* | source/game_sa/PedIK.cpp:34 |
| 0x5fdf90 | CPedIK::RotateTorsoForArm | OK | ATAN | ATAN | source/game_sa/PedIK.cpp:31 |
| 0x5fe0e0 | CPedIK::PitchForSlope | OK(helper) | ASINx2 ATANx2 COSx33 SINx33 | ASINx2 ATANx2 COSx6 COS*x20 SINx6 SIN*x20 | source/game_sa/PedIK.cpp:36 |
| 0x604500 | CCollisionEventScanner::ScanForCollisionEvents | OK | ATANx2 | ATANx2 | source/game_sa/Collision/CollisionEventScanner.cpp:66 |
| 0x605550 | CPedIntelligence::CanSeeEntityWithLights | OK(sqrt differs) | SQRT | - | source/game_sa/PedIntelligence.cpp:99 |
| 0x609f80 | CPlayerPed::CanIKReachThisTarget | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Ped/PlayerPed.cpp:84 |
| 0x60ba80 | CPlayerPed::DrawTriangleForMouseRecruitPed | OK(sqrt differs) | COS SIN SQRT | COS SIN SQRT*x2 | source/game_sa/Entity/Ped/PlayerPed.cpp:101 |
| 0x60c0c0 | CPlayerPed::DoesTargetHaveToBeBroken | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Ped/PlayerPed.cpp:102 |
| 0x60c520 | CPlayerPed::SetPlayerMoveBlendRatio | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Ped/PlayerPed.cpp:104 |
| 0x60c5f0 | CPlayerPed::FindPedToAttack | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Ped/PlayerPed.cpp:105 |
| 0x60d020 | CPlayerPed::EvaluateTarget | OK | SQRTx3 | SQRT*x3 | source/game_sa/Entity/Ped/PlayerPed.cpp:114 |
| 0x60d1c0 | CPlayerPed::EvaluateNeighbouringTarget | OK | SQRT | SQRT* | source/game_sa/Entity/Ped/PlayerPed.cpp:115 |
| 0x60dc50 | CPlayerPed::FindWeaponLockOnTarget | OK | ATANx2 SQRT | ATANx2 SQRT* | source/game_sa/Entity/Ped/PlayerPed.cpp:107 |
| 0x60ea90 | CPlayerPed::ProcessControl | OK(sqrt differs) | ATAN SQRTx2 | ATAN | source/game_sa/Entity/Ped/PlayerPed.cpp:121 |
| 0x610310 | GetCurrentPercOther_Peds | OK | SQRT | SQRT | source/game_sa/PopCycle.cpp:31 |
| 0x6116c0 | PedCreationDistMultiplier | OK(sqrt differs) | SQRT | - | source/game_sa/Population.cpp:108 |
| 0x611fc0 | ManagePed | OK(sqrt differs) | SQRT | - | source/game_sa/Population.cpp:56 |
| 0x612cd0 | AddDeadPedInFrontOfCar | OK(sqrt differs) | SQRT | - | source/game_sa/Population.cpp:62 |
| 0x6133f0 | CreateWaitingCoppers | OK | ATAN COS SIN | ATAN COS SIN | source/game_sa/Population.cpp:66 |
| 0x613d60 | PlaceCouple | OK | SQRT | SQRT* | source/game_sa/Population.cpp:69 |
| 0x6143e0 | FindDistanceToNearestPedOfType | OK(sqrt differs) | SQRT | SQRT*x2 | source/game_sa/Population.cpp:71 |
| 0x614720 | AddToPopulation | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Population.cpp:76 |
| 0x615970 | GeneratePedsAtAttractors | OK(sqrt differs) | SQRT | - | source/game_sa/Population.cpp:77 |
| 0x615dc0 | ManageObject | OK(sqrt differs) | SQRTx2 | SQRT* | source/game_sa/Population.cpp:79 |
| 0x616000 | ManageDummy | OK(sqrt differs) | SQRT | - | source/game_sa/Population.cpp:80 |
| 0x616300 | RemovePedsIfThePoolGetsFull | OK(sqrt differs) | SQRT | - | source/game_sa/Population.cpp:83 |
| 0x617080 | QuatToEuler | OK | ATANx5 SQRT | ATANx5 SQRT* | source/game_sa/Ragdoll/BoneNode.cpp:18 |
| 0x6171f0 | EulerToQuat | OK | COSx3 SINx3 | COSx3 SINx3 | source/game_sa/Ragdoll/BoneNode.cpp:17 |
| 0x6178b0 | IKChain_c::MoveBonesToTarget | OK | ACOS | ACOS | source/game_sa/Ragdoll/IKChain.cpp:15 |
| 0x619fa0 | RsSavePresetView | OK | ACOSx2 | ACOSx2 | source/app/platform/platform.cpp:42 |
| 0x61bf20 | CTaskSimpleRunNamedAnim::ProcessPed | OK(sqrt differs) | SQRT | - | source/game_sa/Tasks/TaskTypes/TaskSimpleRunNamedAnim.cpp:20 |
| 0x61c9b0 | CTaskSimpleFight::SetPlayerMoveAnim | OK | SQRT | SQRT* | source/game_sa/Tasks/TaskTypes/TaskSimpleFight.cpp:90 |
| 0x61d6f0 | CTaskSimpleFight::IsTargetInRange | OK | ATAN | ATAN | source/game_sa/Tasks/TaskTypes/TaskSimpleFight.cpp:85 |
| 0x61e200 | RequirePistolWhip | OK(sqrt differs) | SQRT | - | source/game_sa/Tasks/TaskTypes/TaskSimpleUseGun.cpp:20 |
| 0x61e3f0 | CTaskSimpleUseGun::SetMoveAnim | OK | SQRT | SQRT* | source/game_sa/Tasks/TaskTypes/TaskSimpleUseGun.cpp:32 |
| 0x61f9f0 | CTaskSimpleThrowControl::ProcessPed | OK | ATAN | ATAN | source/game_sa/Tasks/TaskTypes/TaskSimpleThrowControl.cpp:15 |
| 0x620490 | CTaskSimpleChoking::ProcessPed | OK | ATAN | ATAN | source/game_sa/Tasks/TaskTypes/TaskSimpleChoking.cpp:153 |
| 0x620a20 | CTaskSimpleBeHit::ProcessPed | OK | ATAN | ATAN | source/game_sa/Tasks/TaskTypes/TaskSimpleBeHit.cpp:17 |
| 0x621960 | CTaskSimpleGangDriveBy::ProcessPlayerPed | OK | ATANx2 | ATANx2 | source/game_sa/Tasks/TaskTypes/TaskSimpleGangDriveBy.cpp:34 |
| 0x624710 | CTaskSimpleFight::ChooseAttackPlayer | OK | ATAN | ATAN | source/game_sa/Tasks/TaskTypes/TaskSimpleFight.cpp:87 |
| 0x624b50 | CTaskSimpleFightingControl::CalcMoveCommand | OK(sqrt differs) | ATAN SQRT | ATAN | source/game_sa/Tasks/TaskTypes/TaskSimpleFightingControl.cpp:20 |
| 0x625270 | CTaskSimpleGunControl::ProcessPed | OK | ATAN | ATAN | source/game_sa/Tasks/TaskTypes/TaskSimpleGunControl.cpp:17 |
| 0x626260 | CTaskComplexKillPedOnFoot::ControlSubTask | OK | SQRTx3 | SQRT*x3 | source/game_sa/Tasks/TaskTypes/TaskComplexKillPedOnFoot.cpp:32 |
| 0x627600 | CTaskSimpleGangDriveBy::ProcessAIPed | OK | ATANx2 SQRT | ATANx2 SQRT* | source/game_sa/Tasks/TaskTypes/TaskSimpleGangDriveBy.cpp:35 |
| 0x627cc0 | CTaskSimpleGangDriveBy::FireGun | OK | SQRT | SQRT* | source/game_sa/Tasks/TaskTypes/TaskSimpleGangDriveBy.cpp:32 |
| 0x628350 | CTaskSimpleGangDriveBy::ProcessAimIK | OK | ASIN ATANx2 COS SIN SQRTx2 | ASIN ATANx2 COS SIN SQRT*x2 | source/game_sa/Tasks/TaskTypes/TaskSimpleGangDriveBy.cpp:36 |
| 0x6289f0 | CTaskComplexDestroyCarMelee::CalculateSearchPositionAndRanges | OK | ATAN | ATAN | source/game_sa/Tasks/TaskTypes/TaskComplexDestroyCarMelee.cpp:16 |
| 0x628c80 | CTaskComplexDestroyCarArmed::CalculateSearchPositionAndRanges | OK | SQRT | SQRT* | source/game_sa/Tasks/TaskTypes/TaskComplexDestroyCarArmed.cpp:18 |
| 0x629920 | CTaskSimpleFight::ProcessPed | OK(helper) | ATANx2 | ATAN | source/game_sa/Tasks/TaskTypes/TaskSimpleFight.cpp:98 |
| 0x62a380 | CTaskSimpleUseGun::ProcessPed | OK | ATANx2 POW | ATANx2 POW* | source/game_sa/Tasks/TaskTypes/TaskSimpleUseGun.cpp:36 |
| 0x62b150 | CTaskComplexKillPedOnFoot::CreateNextSubTask | OK | SQRT | SQRT* | source/game_sa/Tasks/TaskTypes/TaskComplexKillPedOnFoot.cpp:30 |
| 0x62b490 | CTaskComplexKillPedOnFoot::CreateFirstSubTask | OK | SQRTx2 | SQRT*x2 | source/game_sa/Tasks/TaskTypes/TaskComplexKillPedOnFoot.cpp:31 |
| 0x62bf00 | CTaskComplexKillPedOnFootArmed::CreateFirstSubTask | OK(sqrt differs) | SQRT | - | source/game_sa/Tasks/TaskTypes/TaskComplexKillPedOnFootArmed.cpp:32 |
| 0x62c190 | CTaskComplexKillPedOnFootArmed::CreateNextSubTask | OK | SQRTx3 | SQRT*x3 | source/game_sa/Tasks/TaskTypes/TaskComplexKillPedOnFootArmed.cpp:31 |
| 0x62ddb0 | CTaskComplexDestroyCarMelee::ControlSubTask | OK(sqrt differs) | SQRT | - | source/game_sa/Tasks/TaskTypes/TaskComplexDestroyCarMelee.cpp:23 |
| 0x62e540 | CTaskSimpleStealthKill::ProcessPed | EXPLAINED(COUNT-ONLY) | ATANx3 SQRT | ATANx2 | source/game_sa/Tasks/TaskTypes/TaskSimpleStealthKill.cpp:15 |
| 0x630600 | CTaskSimpleDead::ProcessPed | OK | ASINx2 | ASINx2 | source/game_sa/Tasks/TaskTypes/TaskSimpleDead.cpp:18 |
| 0x63a380 | CTaskComplexEnterCar::GetCameraStickModifier | OK | ATANx2 | ATANx2 | source/game_sa/Tasks/TaskTypes/TaskComplexEnterCar.cpp:54 |
| 0x63e990 | CTaskComplexEnterCar::CreateNextSubTask | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Tasks/TaskTypes/TaskComplexEnterCar.cpp:59 |
| 0x63f970 | CTaskComplexEnterCar::CreateNextSubTask_AfterSimpleCarAlign | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Tasks/TaskTypes/TaskComplexEnterCar.cpp:62 |
| 0x6428c0 | CTaskSimpleCarDrive::ProcessHeadBopping | OK | SIN | SIN | source/game_sa/Tasks/TaskTypes/TaskSimpleCarDrive.cpp:38 |
| 0x644470 | CTaskSimpleCarDrive::ProcessPed | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Tasks/TaskTypes/TaskSimpleCarDrive.cpp:46 |
| 0x645420 | CTaskComplexDriveToPoint::Drive | OK | SQRT | SQRT* | source/game_sa/Tasks/TaskTypes/TaskComplexDriveToPoint.cpp:10 |
| 0x6479b0 | CTaskSimpleCarSetPedOut::PositionPedOutOfCollision | OK | ATAN | ATAN | source/game_sa/Tasks/TaskTypes/TaskSimpleCarSetPedOut.cpp:17 |
| 0x64be40 | CTaskSimpleCarJumpOut::Constructor | OK(sqrt differs) | SQRT | - | source/game_sa/Tasks/TaskTypes/TaskSimpleCarJumpOut.cpp:10 |
| 0x64c220 | CTaskSimpleCarSetPedSlowDraggedOut::ProcessPed | OK(helper) | ATAN | - | source/game_sa/Tasks/TaskTypes/TaskSimpleCarSetPedSlowDraggedOut.cpp:18 |
| 0x64d2b0 | CTaskComplexGoToCarDoorAndStandStill::CreateNextSubTask | OK(sqrt differs) | SQRT | - | source/game_sa/Tasks/TaskTypes/TaskComplexGoToCarDoorAndStandStill.cpp:25 |
| 0x64dd60 | CTaskSimpleCarJumpOut::ProcessPed | OK | POW | POW* | source/game_sa/Tasks/TaskTypes/TaskSimpleCarJumpOut.cpp:21 |
| 0x64f240 | CCarEnterExit::IsPlayerToQuitCarEnter | OK(helper) | ATANx2 SQRT | SQRT* | source/game_sa/CarEnterExit.cpp:41 |
| 0x6504c0 | CCarEnterExit::IsRoomForPedToLeaveCar | OK | SQRT | SQRT* | source/game_sa/CarEnterExit.cpp:42 |
| 0x6513a0 | CTaskUtilityLineUpPedWithCar::ProcessPed | OK(helper) | ATANx4 COS SIN | ATANx3 COS SIN | source/game_sa/Tasks/TaskTypes/TaskUtilityLineUpPedWithCar.cpp:24 |
| 0x6528f0 | CCarEnterExit::GetNearestCarDoor | OK(helper) | ATANx2 | COS | source/game_sa/CarEnterExit.cpp:31 |
| 0x6564a0 | CTaskSimpleKillPedWithCar::ProcessPed | OK(sqrt differs) | SQRT | - | source/game_sa/Tasks/TaskTypes/TaskSimpleKillPedWithCar.cpp:16 |
| 0x65a990 | CTaskComplexMedicTreatInjuredPed::CreateNextSubTask | OK(sqrt differs) | SQRT | - | source/game_sa/Tasks/TaskTypes/TaskComplexMedicTreatInjuredPed.cpp:28 |
| 0x65b820 | CTaskComplexFleePoint::ComputeTargetPoint | OK | COS SIN | COS SIN | source/game_sa/Tasks/TaskTypes/TaskComplexFleePoint.cpp:20 |
| 0x65f930 | CTaskComplexTrackEntity::CalcMoveRatio | OK | SQRTx2 | SQRT*x2 | source/game_sa/Tasks/TaskTypes/TaskComplexTrackEntity.cpp:16 |
| 0x664770 | CTaskGoToVehicleAndLean::CalcTargetPos | OK | COS SIN | COS SIN | source/game_sa/Tasks/TaskTypes/TaskGoToVehicleAndLean.cpp:18 |
| 0x664e60 | CTaskGoToVehicleAndLean::ControlSubTask | OK(sqrt differs) | SQRT | - | source/game_sa/Tasks/TaskTypes/TaskGoToVehicleAndLean.cpp:26 |
| 0x667f20 | CTaskSimpleAchieveHeading::SetUpIK | OK | COS SIN | COS SIN | source/game_sa/Tasks/TaskTypes/TaskSimpleAchieveHeading.cpp:10 |
| 0x6689e0 | CTaskComplexGoToPointAiming::ControlSubTask | OK | SQRT | SQRT* | source/game_sa/Tasks/TaskTypes/TaskComplexGoToPointAiming.cpp:21 |
| 0x668ec0 | CTaskComplexFollowPointRoute::MakeAbortable | OK(sqrt differs) | SQRT | - | source/game_sa/Tasks/TaskTypes/TaskComplexFollowPointRoute.cpp:26 |
| 0x669520 | CTaskComplexFollowNodeRoute::MakeAbortable | OK(sqrt differs) | SQRT | - | source/game_sa/Tasks/TaskTypes/TaskComplexFollowNodeRoute.cpp:35 |
| 0x66a320 | CTaskComplexAvoidOtherPedWhileWandering::ComputeSphere | OK | SQRT | SQRT* | source/game_sa/Tasks/TaskTypes/TaskComplexAvoidOtherPedWhileWandering.cpp:18 |
| 0x66e320 | CTaskComplexFollowPointRoute::CreateFirstSubTask | OK(sqrt differs) | SQRT | - | source/game_sa/Tasks/TaskTypes/TaskComplexFollowPointRoute.cpp:28 |
| 0x66e710 | CTaskComplexFollowPointRoute::CalcBlendRatio | OK(helper) | COS | - | source/game_sa/Tasks/TaskTypes/TaskComplexFollowPointRoute.cpp:22 |
| 0x66edc0 | CTaskComplexFollowNodeRoute::CalcBlendRatio | OK | COS | COS | source/game_sa/Tasks/TaskTypes/TaskComplexFollowNodeRoute.cpp:28 |
| 0x66ef20 | CTaskComplexFollowNodeRoute::CanGoStraightThere | OK(sqrt differs) | SQRT | - | source/game_sa/Tasks/TaskTypes/TaskComplexFollowNodeRoute.cpp:29 |
| 0x66efa0 | CTaskComplexFollowNodeRoute::ComputePathNodes | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Tasks/TaskTypes/TaskComplexFollowNodeRoute.cpp:30 |
| 0x66f6c0 | CTaskComplexAvoidEntity::ComputeBoundingSphere | OK | SQRT | SQRT* | source/game_sa/Tasks/TaskTypes/TaskComplexAvoidEntity.cpp:41 |
| 0x670e20 | CTaskComplexFollowPatrolRoute::CreateFirstSubTask | OK(sqrt differs) | SQRT | SQRT*x2 | source/game_sa/Tasks/TaskTypes/TaskComplexFollowPatrolRoute.cpp:39 |
| 0x676d30 | CTaskInteriorSitOnChair::ProcessPed | OK | SQRT | SQRT* | source/game_sa/Tasks/TaskTypes/Interior/TaskInteriorSitOnChair.cpp:22 |
| 0x6772e0 | CTaskInteriorLieInBed::ProcessPed | OK | SQRT | SQRT* | source/game_sa/Tasks/TaskTypes/Interior/TaskInteriorLieInBed.cpp:22 |
| 0x677920 | CTaskInteriorSitAtDesk::ProcessPed | OK | SQRT | SQRT* | source/game_sa/Tasks/TaskTypes/Interior/TaskInteriorSitAtDesk.cpp:21 |
| 0x679b80 | CTaskSimpleJump::Launch | EXPLAINED(COUNT-ONLY) | COSx2 SIN | COS SIN | source/game_sa/Tasks/TaskTypes/TaskSimpleJump.cpp:12 |
| 0x67a390 | CTaskSimpleClimb::GetCameraTargetPos | OK(helper) | ATAN COS SIN | COS SIN | source/game_sa/Tasks/TaskTypes/TaskSimpleClimb.cpp:47 |
| 0x67a5d0 | CTaskSimpleClimb::GetCameraStickModifier | OK(helper) | ATANx2 COS SIN | ATANx3 COS SIN | source/game_sa/Tasks/TaskTypes/TaskSimpleClimb.cpp:46 |
| 0x67e7b0 | CTaskSimpleJetPack::ProcessControlInput | OK(helper) | ATAN POWx2 SQRTx2 | ATANx2 POW*x2 SQRT*x2 | source/game_sa/Tasks/TaskTypes/TaskSimpleJetPack.cpp:41 |
| 0x67ef20 | CTaskSimpleJetPack::ProcessThrust | OK(sqrt differs) | COS POWx2 SINx3 SQRTx2 | COS POW*x2 SINx3 SQRT* | source/game_sa/Tasks/TaskTypes/TaskSimpleJetPack.cpp:42 |
| 0x67f6a0 | CTaskSimpleJetPack::RenderJetPack | OK(helper) | COSx2 SINx2 | COSx2 COS* SINx2 SIN* | source/game_sa/Tasks/TaskTypes/TaskSimpleJetPack.cpp:34 |
| 0x67fd30 | CTaskSimpleClimb::ScanToGrab | OK(helper) | ATAN | - | source/game_sa/Tasks/TaskTypes/TaskSimpleClimb.cpp:36 |
| 0x6801f0 | CTaskSimpleJetPack::ProcessPed | OK | SIN | SIN | source/game_sa/Tasks/TaskTypes/TaskSimpleJetPack.cpp:47 |
| 0x6803a0 | CTaskSimpleClimb::TestForClimb | OK(helper) | ATAN COS SIN | COS SIN | source/game_sa/Tasks/TaskTypes/TaskSimpleClimb.cpp:40 |
| 0x6804d0 | CTaskSimpleClimb::TestForVault | OK | COS SIN | COS SIN | source/game_sa/Tasks/TaskTypes/TaskSimpleClimb.cpp:39 |
| 0x680570 | CTaskSimpleClimb::TestForStandUp | OK | COS SIN | COS SIN | source/game_sa/Tasks/TaskTypes/TaskSimpleClimb.cpp:38 |
| 0x680600 | CTaskSimpleInAir::ProcessPed | OK(sqrt differs) | SQRT | - | source/game_sa/Tasks/TaskTypes/TaskSimpleInAir.cpp:15 |
| 0x680dc0 | CTaskSimpleClimb::ProcessPed | OK(helper) | ATANx23 COSx8 SINx8 SQRTx11 | ATANx23 COSx7 SINx7 SQRT*x22 | source/game_sa/Tasks/TaskTypes/TaskSimpleClimb.cpp:48 |
| 0x681fe0 | CTaskComplexPartner::CalculateMeetingPoints | OK(sqrt differs) | SQRTx2 | SQRT*x3 | source/game_sa/Tasks/TaskTypes/TaskComplexPartner.cpp:42 |
| 0x683ad0 | CTaskComplexPartner::CreateNextSubTask | OK | SQRTx2 | SQRT*x2 | source/game_sa/Tasks/TaskTypes/TaskComplexPartner.cpp:43 |
| 0x6859a0 | CTaskSimplePlayerOnFoot::ProcessPlayerWeapon | OK | ATAN SIN | ATAN SIN | source/game_sa/Tasks/TaskTypes/TaskSimplePlayerOnFoot.cpp:33 |
| 0x687530 | CTaskSimplePlayerOnFoot::PlayerControlFighter | OK | COSx2 SINx2 SQRTx3 | COSx2 SINx2 SQRT*x3 | source/game_sa/Tasks/TaskTypes/TaskSimplePlayerOnFoot.cpp:35 |
| 0x687c20 | CTaskSimplePlayerOnFoot::PlayerControlZeldaWeapon | OK | ATAN COS SIN SQRTx2 | ATAN COS SIN SQRT*x2 | source/game_sa/Tasks/TaskTypes/TaskSimplePlayerOnFoot.cpp:30 |
| 0x687f30 | CTaskSimplePlayerOnFoot::PlayerControlDucked | EXPLAINED(COUNT-ONLY) | ATAN COSx2 SINx2 SQRT | ATAN COS SINx3 SQRT* | source/game_sa/Tasks/TaskTypes/TaskSimplePlayerOnFoot.cpp:31 |
| 0x6883d0 | CTaskSimplePlayerOnFoot::PlayerControlZelda | OK | COS SIN SQRT | COS SIN SQRT* | source/game_sa/Tasks/TaskTypes/TaskSimplePlayerOnFoot.cpp:36 |
| 0x688a90 | CTaskSimpleSwim::ProcessControlInput | OK(sqrt differs) | ASIN ATANx3 COS POWx3 SIN SQRT | ASIN ATANx3 COS POW*x3 SIN | source/game_sa/Tasks/TaskTypes/TaskSimpleSwim.cpp:18 |
| 0x689640 | CTaskSimpleSwim::ProcessControlAI | OK(sqrt differs) | ATAN SQRTx4 | ATAN | source/game_sa/Tasks/TaskTypes/TaskSimpleSwim.cpp:17 |
| 0x68a1d0 | CTaskSimpleSwim::ProcessSwimmingResistance | OK | COS POW SINx2 | COS POW* SINx2 | source/game_sa/Tasks/TaskTypes/TaskSimpleSwim.cpp:15 |
| 0x68aa70 | CTaskSimpleSwim::ProcessEffects | OK | COS SIN | COS SIN | source/game_sa/Tasks/TaskTypes/TaskSimpleSwim.cpp:16 |
| 0x68b1c0 | CTaskSimpleSwim::ProcessPed | OK | SQRT | SQRT* | source/game_sa/Tasks/TaskTypes/TaskSimpleSwim.cpp:11 |
| 0x691ae0 | CTaskComplexGoPickUpEntity::CreateNextSubTask | OK | ATAN | ATAN | source/game_sa/Tasks/TaskTypes/TaskComplexGoPickUpEntity.cpp:43 |
| 0x691d50 | CTaskComplexGoPickUpEntity::ControlSubTask | OK | ATAN SQRT | ATAN SQRT* | source/game_sa/Tasks/TaskTypes/TaskComplexGoPickUpEntity.cpp:45 |
| 0x693610 | CTaskComplexGoPickUpEntity::CreateFirstSubTask | OK | ATANx3 | ATANx3 | source/game_sa/Tasks/TaskTypes/TaskComplexGoPickUpEntity.cpp:44 |
| 0x693c40 | CTaskSimpleHoldEntity::ProcessPed | OK | ATANx3 COSx2 SINx2 | ATANx3 COSx2 SINx2 | source/game_sa/Tasks/TaskTypes/TaskSimpleHoldEntity.cpp:16 |
| 0x6946f0 | CEntitySeekPosCalculatorRadiusAngleOffset::ComputeEntitySeekPos | EXPLAINED(COUNT-ONLY) | COSx2 SINx2 | COS SIN | source/game_sa/Tasks/TaskTypes/SeekEntity/PosCalculators/EntitySeekPosCalculatorRadiusAngleOffset.cpp:10 |
| 0x695090 | CTaskComplexFollowPedFootsteps::ControlSubTask | OK | SQRT | SQRT* | source/game_sa/Tasks/TaskTypes/TaskComplexFollowPedFootsteps.cpp:20 |
| 0x6987f0 | CCover::CalculateHorizontalSize | OK(sqrt differs) | SQRT | - | source/game_sa/Cover.cpp:16 |
| 0x698d40 | CCover::FindDirFromVector | OK | ATAN | ATAN | source/game_sa/Cover.cpp:23 |
| 0x698d60 | CCover::FindVectorFromDir | OK | COS SIN | COS SIN | source/game_sa/Cover.cpp:24 |
| 0x698f30 | CCover::AddCoverPoint | OK(sqrt differs) | SQRTx2 | SQRT | source/game_sa/Cover.cpp:15 |
| 0x699120 | CCover::FindCoverPointsForThisBuilding | OK | ATAN | ATAN | source/game_sa/Cover.cpp:22 |
| 0x699230 | CCover::ShouldThisBuildingHaveItsCoverPointsCreated | OK(sqrt differs) | SQRT | - | source/game_sa/Cover.cpp:13 |
| 0x6992b0 | CCover::FindAndReserveCoverPoint | OK(helper) | COS SIN SQRTx3 | - | source/game_sa/Cover.cpp:20 |
| 0x699570 | CCover::FindCoordinatesCoverPoint | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Cover.cpp:21 |
| 0x6997e0 | CCover::Update | OK(sqrt differs) | SQRTx4 | - | source/game_sa/Cover.cpp:14 |
| 0x699ff0 | FindCoverPointsBehindBox | OK(sqrt differs) | SQRT | - | source/game_sa/Formation.cpp:346 |
| 0x69a620 | GenerateGatherDestinations | OK | COS SIN | COS SIN | source/game_sa/Formation.cpp:347 |
| 0x69b240 | DistributeDestinations | OK(sqrt differs) | SQRTx2 | SQRT* | source/game_sa/Formation.cpp:349 |
| 0x69b5b0 | DistributeDestinations_CoverPoints | OK | SQRTx3 | SQRT*x3 | source/game_sa/Formation.cpp:350 |
| 0x69b700 | DistributeDestinations_PedsToAttack | OK(sqrt differs) | SQRT | - | source/game_sa/Formation.cpp:351 |
| 0x69b860 | FindCoverPoints | OK(sqrt differs) | SQRTx2 | SQRT* | source/game_sa/Formation.cpp:352 |
| 0x6a07a0 | CAutomobile::HydraulicControl | OK | ATAN COS SIN SQRT | ATAN COS SIN SQRT* | source/game_sa/Entity/Vehicle/Automobile.cpp:57 |
| 0x6a40f0 | CAutomobile::TowTruckControl | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Automobile.cpp:109 |
| 0x6a4af0 | CAutomobile::DoSoftGroundResistance | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Automobile.cpp:62 |
| 0x6a4ec0 | CAutomobile::ProcessCarWheelPair | OK | COS POW SIN | COS POW* SIN | source/game_sa/Entity/Vehicle/Automobile.cpp:63 |
| 0x6a6010 | CAutomobile::GetCarRoll | OK | ATAN SQRT | ATAN SQRT* | source/game_sa/Entity/Vehicle/Automobile.cpp:92 |
| 0x6a6050 | CAutomobile::GetCarPitch | OK(sqrt differs) | ATAN SQRT | ATAN | source/game_sa/Entity/Vehicle/Automobile.cpp:93 |
| 0x6a65d0 | CAutomobile::SetupSuspensionLines | OK(sqrt differs) | SQRTx3 | - | source/game_sa/Entity/Vehicle/Automobile.cpp:153 |
| 0x6a6dc0 | CAutomobile::dmgDrawCarCollidingParticles | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Automobile.cpp:95 |
| 0x6a8500 | CAutomobile::ProcessFlyingCarStuff | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Automobile.cpp:143 |
| 0x6a8580 | CAutomobile::SpawnFlyingComponent | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Automobile.cpp:96 |
| 0x6a8c00 | CAutomobile::ProcessBuoyancy | OK | POW | POW* | source/game_sa/Entity/Vehicle/Automobile.cpp:59 |
| 0x6a9680 | CAutomobile::ProcessHarvester | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Automobile.cpp:60 |
| 0x6a9d70 | CAutomobile::ProcessSwingingDoor | OK | SIN | SIN | source/game_sa/Entity/Vehicle/Automobile.cpp:98 |
| 0x6aa290 | CAutomobile::UpdateWheelMatrix | EXPLAINED(DUP-BRANCH) | ASINx6 ATAN SIN | ASIN ATAN SIN | source/game_sa/Entity/Vehicle/Automobile.cpp:97 |
| 0x6aab50 | CAutomobile::PreRender | OK | ASINx3 ATANx2 COSx3 POW SINx3 SQRT | ASINx3 ATANx2 COSx3 POW* SINx3 SQRT* | source/game_sa/Entity/Vehicle/Automobile.cpp:113 |
| 0x6ad690 | CAutomobile::ProcessControlInputs | EXPLAINED(POW-COUNT) | POW SQRT | POW*x2 SQRT* | source/game_sa/Entity/Vehicle/Automobile.cpp:156 |
| 0x6ae850 | CAutomobile::TankControl | OK(sqrt differs) | ATANx2 COSx3 SINx3 SQRT | ATANx2 COSx3 SINx3 | source/game_sa/Entity/Vehicle/Automobile.cpp:61 |
| 0x6af110 | CAutomobile::BlowUpCarsInPath | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Automobile.cpp:103 |
| 0x6b1880 | CAutomobile::ProcessControl | OK(sqrt differs) | ASIN POWx3 SQRTx2 | ASIN POW*x3 SQRT* | source/game_sa/Entity/Vehicle/Automobile.cpp:139 |
| 0x6b4410 | CAutomobile::SetTowLink | OK | ATANx9 COS SIN SQRTx3 | ATANx9 COS SIN SQRT*x3 | source/game_sa/Entity/Vehicle/Automobile.cpp:149 |
| 0x6b4800 | CAutomobile::ProcessAI | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Automobile.cpp:141 |
| 0x6b5fb0 | CBike::ProcessBuoyancy | OK | POW | POW* | source/game_sa/Entity/Vehicle/Bike.cpp:101 |
| 0x6b7150 | CBike::CalculateLeanMatrix | OK | COS | COS | source/game_sa/Entity/Vehicle/Bike.cpp:107 |
| 0x6b7280 | CBike::ProcessRiderAnims | OK | POWx2 SQRT | POW*x2 SQRT* | source/game_sa/Entity/Vehicle/Bike.cpp:108 |
| 0x6b89b0 | CBike::SetupSuspensionLines | OK(sqrt differs) | SQRTx4 | SQRT*x3 | source/game_sa/Entity/Vehicle/Bike.cpp:123 |
| 0x6b9250 | CBike::ProcessControl | OK(sqrt differs) | ASINx4 COSx2 POWx7 SINx2 SQRTx6 | ASINx4 COSx2 POW*x7 SINx2 SQRT*x4 | source/game_sa/Entity/Vehicle/Bike.cpp:121 |
| 0x6bd090 | CBike::PreRender | OK | ASIN COSx3 SINx5 SQRT | ASIN COSx3 SINx5 SQRT* | source/game_sa/Entity/Vehicle/Bike.cpp:119 |
| 0x6be310 | CBike::ProcessControlInputs | OK | POWx2 SQRT | POW*x2 SQRT* | source/game_sa/Entity/Vehicle/Bike.cpp:116 |
| 0x6beb20 | CBike::BurstTyre | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Bike.cpp:115 |
| 0x6beeb0 | CBike::PlaceOnRoadProperly | OK | ATAN COS SIN | ATAN COS SIN | source/game_sa/Entity/Vehicle/Bike.cpp:110 |
| 0x6bf430 | CBike::Constructor | OK | TAN | TAN | source/game_sa/Entity/Vehicle/Bike.cpp:93 |
| 0x6bfa30 | CBmx::ProcessControl | OK | SINx2 | SINx2 | source/game_sa/Entity/Vehicle/Bmx.cpp:33 |
| 0x6bfb50 | CBmx::ProcessDrivingAnims | OK | POWx2 | POW*x2 | source/game_sa/Entity/Vehicle/Bmx.cpp:34 |
| 0x6c0390 | CBmx::LaunchBunnyHopCB | OK | ATAN | ATAN | source/game_sa/Entity/Vehicle/Bmx.cpp:38 |
| 0x6c0810 | CBmx::PreRender | OK | ASIN ATAN COSx3 SINx5 SQRT | ASIN ATAN COSx3 SINx5 SQRT* | source/game_sa/Entity/Vehicle/Bmx.cpp:35 |
| 0x6c2970 | cBuoyancy::SimpleSumBuoyancyData | EXPLAINED(POW-SQR) | - | POW* | source/game_sa/Buoyancy.cpp:18 |
| 0x6c34e0 | cBuoyancy::AddSplashParticles | OK(sqrt differs) | COS SIN SQRTx2 | COS SIN | source/game_sa/Buoyancy.cpp:16 |
| 0x6c4830 | CHeli::ProcessControlInputs | OK | POWx2 SQRT | POW*x2 SQRT* | source/game_sa/Entity/Vehicle/Heli.cpp:97 |
| 0x6c4e60 | CHeli::ProcessFlyingCarStuff | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Heli.cpp:98 |
| 0x6c58e0 | CHeli::SearchLightCone | OK(sqrt differs) | COS SIN SQRTx4 | COS SIN SQRT*x8 | source/game_sa/Entity/Vehicle/Heli.cpp:86 |
| 0x6c6520 | CHeli::GenerateHeli | OK | COSx3 SINx3 SQRT | COSx3 SINx3 SQRT* | source/game_sa/Entity/Vehicle/Heli.cpp:87 |
| 0x6c6890 | CHeli::TestSniperCollision | EXPLAINED(POW-SQR) | SQRT | POW* | source/game_sa/Entity/Vehicle/Heli.cpp:92 |
| 0x6c69c0 | CHeli::SendDownSwat | OK(sqrt differs) | SQRT | SQRT*x2 | source/game_sa/Entity/Vehicle/Heli.cpp:90 |
| 0x6c7050 | CHeli::ProcessControl | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Heli.cpp:101 |
| 0x6c79a0 | CHeli::UpdateHelis | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Heli.cpp:88 |
| 0x6c7fb0 | CMonsterTruck::SetupSuspensionLines | OK(sqrt differs) | SQRTx3 | - | source/game_sa/Entity/Vehicle/MonsterTruck.cpp:20 |
| 0x6c94a0 | CPlane::PreRender | OK(helper) | ASINx3 POW | ASIN POW* | source/game_sa/Entity/Vehicle/Plane.cpp:108 |
| 0x6cadd0 | CPlane::ProcessControlInputs | OK(sqrt differs) | POWx2 SQRTx2 | POW*x2 | source/game_sa/Entity/Vehicle/Plane.cpp:117 |
| 0x6cb7c0 | CPlane::ProcessFlyingCarStuff | OK(sqrt differs) | SIN SQRTx2 | SIN | source/game_sa/Entity/Vehicle/Plane.cpp:118 |
| 0x6cc4b0 | CPlane::VehicleDamage | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Plane.cpp:119 |
| 0x6ccaa0 | CPlane::AreWeInNoPlaneZone | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Plane.cpp:121 |
| 0x6ccbb0 | CPlane::AreWeInNoBigPlaneZone | OK | SQRTx2 | SQRT*x2 | source/game_sa/Entity/Vehicle/Plane.cpp:122 |
| 0x6cd090 | CPlane::FindPlaneCreationCoors | OK | COS SIN | COS SIN | source/game_sa/Entity/Vehicle/Plane.cpp:125 |
| 0x6cd2f0 | CPlane::DoPlaneGenerationAndRemoval | OK(sqrt differs) | COSx2 SINx2 | COSx2 SINx2 SQRT*x2 | source/game_sa/Entity/Vehicle/Plane.cpp:126 |
| 0x6cdcc0 | CQuadBike::ProcessControl | OK | POWx2 | POW*x2 | source/game_sa/Entity/Vehicle/QuadBike.cpp:50 |
| 0x6ce020 | CQuadBike::ProcessControlInputs | OK | POW | POW* | source/game_sa/Entity/Vehicle/QuadBike.cpp:51 |
| 0x6ce460 | CQuadBike::ProcessAI | OK | POW | POW* | source/game_sa/Entity/Vehicle/QuadBike.cpp:49 |
| 0x6cead0 | CQuadBike::PreRender | OK | ATANx2 | ATANx2 | source/game_sa/Entity/Vehicle/QuadBike.cpp:48 |
| 0x6cf030 | CTrailer::ScanForTowLink | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Trailer.cpp:24 |
| 0x6cf1a0 | CTrailer::SetupSuspensionLines | OK(sqrt differs) | SQRTx3 | - | source/game_sa/Entity/Vehicle/Trailer.cpp:26 |
| 0x6cf590 | CTrailer::ProcessAI | OK(helper) | ATANx2 | - | source/game_sa/Entity/Vehicle/Trailer.cpp:32 |
| 0x6cfac0 | CTrailer::PreRender | EXPLAINED(FMOD-CRT) | - | FMOD* | source/game_sa/Entity/Vehicle/Trailer.cpp:33 |
| 0x6cfdf0 | CTrailer::SetTowLink | OK(helper) | ATAN | - | source/game_sa/Entity/Vehicle/Trailer.cpp:27 |
| 0x6d0e90 | CVehicle::SpecialEntityCalcCollisionSteps | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Vehicle.cpp:95 |
| 0x6d1f30 | CVehicle::CanPedStepOutCar | OK | SQRTx2 | SQRT*x2 | source/game_sa/Entity/Vehicle/Vehicle.cpp:102 |
| 0x6d2030 | CVehicle::CanPedJumpOutCar | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Vehicle.cpp:103 |
| 0x6d2740 | CVehicle::ApplyBoatWaterResistance | OK | POWx3 | POW*x3 | source/game_sa/Entity/Vehicle/Vehicle.cpp:126 |
| 0x6d2bf0 | CVehicle::MakeDirty | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Entity/Vehicle/Vehicle.cpp:166 |
| 0x6d6c00 | CVehicle::ProcessWheel | OK | SQRTx3 | SQRT*x3 | source/game_sa/Entity/Vehicle/Vehicle.cpp:125 |
| 0x6d73b0 | CVehicle::ProcessBikeWheel | OK(sqrt differs) | SQRTx3 | SQRT*x4 | source/game_sa/Entity/Vehicle/Vehicle.cpp:208 |
| 0x6d85f0 | CVehicle::FlyingControl | OK(sqrt differs) | ACOS ASINx4 ATAN COSx4 POWx12 SINx5 SQRTx3 | ACOS ASINx4 ATAN COSx4 POW*x12 SINx5 SQRT*x10 | source/game_sa/Entity/Vehicle/Vehicle.cpp:214 |
| 0x6dbbb0 | CVehicle::SetTransmissionRotation | OK | ATANx2 | ATANx2 | source/game_sa/Entity/Vehicle/Vehicle.cpp:217 |
| 0x6dbce0 | CVehicle::ProcessBoatControl | OK(sqrt differs) | COS POWx3 SIN SQRTx2 | COS POW*x3 SIN | source/game_sa/Entity/Vehicle/Vehicle.cpp:127 |
| 0x6dd130 | CVehicle::DoBoatSplashes | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Vehicle.cpp:218 |
| 0x6dd6f0 | CVehicle::DoSunGlare | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Vehicle.cpp:219 |
| 0x6de880 | CVehicle::AddSingleWheelParticles | OK(sqrt differs) | SQRT | SQRT*x2 | source/game_sa/Entity/Vehicle/Vehicle.cpp:222 |
| 0x6df4a0 | CVehicle::ProcessDrivingAnims | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Vehicle.cpp:112 |
| 0x6dfc50 | CVehicle::UpdateTrailerLink | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Vehicle.cpp:226 |
| 0x6e0a50 | CVehicle::DoHeadLightEffect | OK | COS SIN SQRT | COS SIN SQRT* | source/game_sa/Entity/Vehicle/Vehicle.cpp:232 |
| 0x6e1440 | CVehicle::DoHeadLightReflectionSingle | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Entity/Vehicle/Vehicle.cpp:233 |
| 0x6e1600 | CVehicle::DoHeadLightReflectionTwin | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Vehicle.cpp:234 |
| 0x6e1780 | CVehicle::DoTailLightEffect | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Vehicle.cpp:236 |
| 0x6e1a60 | CVehicle::DoVehicleLights | OK(helper) | COSx2 SINx2 SQRTx2 | - | source/game_sa/Entity/Vehicle/Vehicle.cpp:237 |
| 0x6e3b00 | CVehicleAnimGroup::GetGroup | OK | POW | POW* | source/game_sa/VehicleAnimGroupData.cpp:11 |
| 0x6e4160 | WaterCreature_c::Init | OK | ATAN COSx3 SINx3 | ATAN COSx3 SINx3 | source/game_sa/WaterCreature_c.cpp:10 |
| 0x6e4670 | WaterCreature_c::Update | OK(sqrt differs) | ATANx4 COSx8 SINx8 SQRTx174 | ATANx4 COSx8 SINx8 SQRT*x24 | source/game_sa/WaterCreature_c.cpp:12 |
| 0x6e8780 | RenderHighDetailWaterTriangle_OneLayer | OK | SQRT | SQRT* | source/game_sa/WaterLevel.cpp:73 |
| 0x6e91d0 | RenderHighDetailWaterRectangle_OneLayer | OK | SQRT | SQRT* | source/game_sa/WaterLevel.cpp:70 |
| 0x6e9d70 | FindNearestWaterAndItsFlow | OK | SQRT | SQRT* | source/game_sa/WaterLevel.cpp:57 |
| 0x6ea260 | RenderWakeSegment | EXPLAINED(COUNT-ONLY) | SINx4 | SIN | source/game_sa/WaterLevel.cpp:53 |
| 0x6eb810 | RenderHighDetailWaterRectangle | OK | SQRT | SQRT* | source/game_sa/WaterLevel.cpp:69 |
| 0x6ed9a0 | RenderBoatWakes | OK(sqrt differs) | SQRTx2 | - | source/game_sa/WaterLevel.cpp:52 |
| 0x6ef650 | RenderWater | OK | COSx2 SINx2 SQRT | COSx2 SINx2 SQRT* | source/game_sa/WaterLevel.cpp:61 |
| 0x6f0a10 | CBoat::ProcessControlInputs | OK | POW | POW* | source/game_sa/Entity/Vehicle/Boat.cpp:42 |
| 0x6f0e80 | CBoat::IsSectorAffectedByWake | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Boat.cpp:54 |
| 0x6f0f50 | CBoat::IsVertexAffectedByWake | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Boat.cpp:55 |
| 0x6f1180 | CBoat::PreRender | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Entity/Vehicle/Boat.cpp:39 |
| 0x6f1770 | CBoat::ProcessControl | OK(helper) | ATANx2 SQRT | - | source/game_sa/Entity/Vehicle/Boat.cpp:37 |
| 0x6f2550 | CBoat::AddWakePoint | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Boat.cpp:48 |
| 0x6f2710 | CBoat::FillBoatList | OK(sqrt differs) | SQRT | SQRT*x2 | source/game_sa/Entity/Vehicle/Boat.cpp:53 |
| 0x6f2f40 | CCarGenerator::CheckIfWithinRangeOfAnyPlayers | OK(sqrt differs) | SQRT | - | source/game_sa/CarGenerator.cpp:16 |
| 0x6f4040 | CDoor::Process | OK | COS POW SIN | COS POW* SIN | source/game_sa/Door.cpp:32 |
| 0x6f49a0 | CBouncingPanel::ProcessPanel | OK | POW | POW* | source/game_sa/BouncingPanel.cpp:10 |
| 0x6f5240 | cHandlingDataMgr::ConvertBikeDataToWorldUnits | OK | ASINx3 | ASINx3 | source/game_sa/cHandlingDataMgr.cpp:54 |
| 0x6f5290 | cHandlingDataMgr::ConvertBikeDataToGameUnits | OK | SINx3 | SINx3 | source/game_sa/cHandlingDataMgr.cpp:55 |
| 0x6f55d0 | CTrain::ReadAndInterpretTrackFile | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Train.cpp:62 |
| 0x6f6bd0 | CTrain::FindClosestTrackNode | OK(sqrt differs) | SQRT | - | source/game_sa/Entity/Vehicle/Train.cpp:86 |
| 0x6f6cc0 | CTrain::FindPositionOnTrackFromCoors | OK | SQRTx3 | SQRT SQRT*x2 | source/game_sa/Entity/Vehicle/Train.cpp:87 |
| 0x6f7090 | CTrain::FindNearestTrain | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Train.cpp:88 |
| 0x6f72f0 | CTrain::SkipToNextAllowedStation | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Train.cpp:91 |
| 0x6f7900 | CTrain::DoTrainGenerationAndRemoval | OK(sqrt differs) | SQRTx8 | SQRT*x7 | source/game_sa/Entity/Vehicle/Train.cpp:93 |
| 0x6f86a0 | CTrain::ProcessControl | OK(helper) | ATANx2 POWx3 SQRT | POW*x3 | source/game_sa/Entity/Vehicle/Train.cpp:97 |
| 0x6fbaa0 | CCoronas::RenderSunReflection | OK | SIN | SIN | source/game_sa/Coronas.cpp:113 |
| 0x6fc180 | CCoronas::RegisterCorona | OK | SQRT | SQRT | source/game_sa/Coronas.cpp:114 |
| 0x6fc7a0 | CEntity::ProcessLightsForEntity | OK(sqrt differs) | SQRTx3 | SQRT*x2 | source/game_sa/Entity/Entity.cpp:91 |
| 0x6ff4f0 | CLines::RenderLineWithClipping | OK | SQRT | SQRT* | source/game_sa/Lines.cpp:10 |
| 0x6ffbb0 | CPointLights::GenerateLightsAffectingObject | OK | SQRT | SQRT | source/game_sa/PointLights.cpp:17 |
| 0x6ffe70 | CPointLights::GetLightMultiplier | OK | SQRT | SQRT | source/game_sa/PointLights.cpp:18 |
| 0x7000e0 | CPointLights::AddLight | OK | SQRT | SQRT | source/game_sa/PointLights.cpp:21 |
| 0x7002d0 | CPointLights::RenderFogEffect | OK(sqrt differs) | SQRTx4 | SQRT*x3 | source/game_sa/PointLights.cpp:22 |
| 0x7039c0 | CPostEffects::UnderWaterRipple | OK(helper) | SINx2 | SINx3 | source/game_sa/PostEffects.cpp:40 |
| 0x703cc0 | CPostEffects::ImmediateModeFilterStuffInitialize | EXPLAINED(LN-REFACTOR) | LNx3 POWx2 | - | source/game_sa/PostEffects.cpp:30 |
| 0x704150 | CPostEffects::Fog | OK(sqrt differs) | COSx2 SINx2 SQRT | COSx2 SINx2 | source/game_sa/PostEffects.cpp:51 |
| 0x7043d0 | CPostEffects::SetupBackBufferVertex | EXPLAINED(LN-REFACTOR) | LNx2 LOG? POWx2 | - | source/game_sa/PostEffects.cpp:25 |
| 0x7076c0 | CShadows::CalcPedShadowValues | OK | SQRT | SQRT* | source/game_sa/Shadows.cpp:30 |
| 0x707b40 | CShadows::StoreShadowForPedObject | OK | SQRT | SQRT* | source/game_sa/Shadows.cpp:32 |
| 0x707ca0 | CShadows::StoreRealTimeShadow | OK(sqrt differs) | ATANx2 SQRT | ATANx2 SQRT*x2 | source/game_sa/Shadows.cpp:33 |
| 0x70ba00 | CShadows::StoreStaticShadow | OK | SQRT | SQRT* | source/game_sa/Shadows.cpp:44 |
| 0x70bda0 | CShadows::StoreShadowForVehicle | OK | SQRTx3 | SQRT*x3 | source/game_sa/Shadows.cpp:45 |
| 0x70c500 | CShadows::StoreCarLightShadow | OK(sqrt differs) | SQRTx2 | SQRT* | source/game_sa/Shadows.cpp:46 |
| 0x70c950 | CShadows::UpdatePermanentShadows | OK | SQRTx2 | SQRT*x2 | source/game_sa/Shadows.cpp:23 |
| 0x70d490 | CSprite::RenderOneXLUSprite_Rotate_Aspect | OK | COS SIN | COS SIN | source/game_sa/Sprite.cpp:150 |
| 0x70e780 | CSprite::RenderBufferedOneXLUSprite_Rotate_Aspect | OK | COS SIN | COS SIN | source/game_sa/Sprite.cpp:153 |
| 0x70eab0 | CSprite::RenderBufferedOneXLUSprite_Rotate_Dimension | OK | COS SIN | COS SIN | source/game_sa/Sprite.cpp:154 |
| 0x70ede0 | CSprite::RenderBufferedOneXLUSprite_Rotate_2Colours | OK | COS SIN | COS SIN | source/game_sa/Sprite.cpp:155 |
| 0x711ef0 | CBirds::CreateNumberOfBirds | OK | ATAN | ATAN | source/game_sa/Birds.cpp:31 |
| 0x712330 | CBirds::Update | OK(sqrt differs) | ATANx2 COSx2 SINx2 | ATANx2 COSx2 SINx2 SQRT* | source/game_sa/Birds.cpp:30 |
| 0x712810 | CBirds::Render | OK(sqrt differs) | COSx3 SINx6 SQRT | COSx3 SINx6 | source/game_sa/Birds.cpp:32 |
| 0x712ff0 | CClouds::Update | OK | SIN | SIN | source/game_sa/Clouds.cpp:19 |
| 0x713950 | CClouds::Render | OK(sqrt differs) | ATAN COS SIN SQRTx13 | ATAN COS SIN SQRT | source/game_sa/Clouds.cpp:22 |
| 0x714650 | CClouds::RenderSkyPolys | OK | COS SIN | COS SIN | source/game_sa/Clouds.cpp:23 |
| 0x716380 | CClouds::VolumetricCloudsRender | OK(sqrt differs) | SQRT | - | source/game_sa/Clouds.cpp:42 |
| 0x716b10 | CClouds::MovingFog_Update | OK(sqrt differs) | SQRT | - | source/game_sa/Clouds.cpp:30 |
| 0x7174f0 | CPlaneTrails::Update | OK(sqrt differs) | COSx3 SINx3 | COSx3 SINx3 SQRTx3 | source/game_sa/PlaneTrails.cpp:12 |
| 0x717970 | CEscalator::AddThisOne | OK(sqrt differs) | SQRTx4 | - | source/game_sa/Escalator.cpp:13 |
| 0x717d30 | CEscalator::Update | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Escalator.cpp:14 |
| 0x71acf0 | CGlass::CalcAlphaWithNormal | EXPLAINED(POW-SQR) | - | POW* | source/game_sa/Glass.cpp:23 |
| 0x71b100 | CFallingGlassPane::Render | OK(sqrt differs) | SQRT | - | source/game_sa/FallingGlassPane.cpp:11 |
| 0x71b620 | CGlass::GeneratePanesForWindow | OK(sqrt differs) | SQRTx5 | - | source/game_sa/Glass.cpp:16 |
| 0x71bc40 | CGlass::WindowRespondsToCollision | OK(sqrt differs) | SQRTx2 | - | source/game_sa/Glass.cpp:15 |
| 0x71c1a0 | CGlass::WindowRespondsToExplosion | OK(sqrt differs) | SQRT | - | source/game_sa/Glass.cpp:28 |
| 0x71cf50 | CGlass::BreakGlassPhysically | OK | SQRTx2 | SQRT*x2 | source/game_sa/Glass.cpp:27 |
| 0x71e130 | COccluder::ProcessLineSegment | OK | SQRT | SQRT | source/game_sa/Occluder.cpp:12 |
| 0x71e5d0 | COccluder::ProcessOneOccluder | OK | SQRT | SQRT | source/game_sa/Occluder.cpp:11 |
| 0x71f960 | COccluder::NearCamera | OK | SQRT | SQRT | source/game_sa/Occluder.cpp:13 |
| 0x720930 | CSkidmarks::RegisterOne | OK(sqrt differs) | SQRTx3 | - | source/game_sa/Skidmarks.cpp:16 |
| 0x722970 | CCheckpoints::SetHeading | OK | COS SIN | COS SIN | source/game_sa/Checkpoints.cpp:16 |
| 0x722c40 | CCheckpoints::PlaceMarker | OK(sqrt differs) | SQRTx2 | SQRT* | source/game_sa/Checkpoints.cpp:14 |
| 0x723240 | C3dMarkers::User3dMarkersDraw | OK | SIN | SIN | source/game_sa/3dMarkers.cpp:33 |
| 0x723750 | CBulletTraces::AddTrace | EXPLAINED(LOG10-CRT) | LOG10x2 SQRT | LOG10* | source/game_sa/BulletTraces.cpp:10 |
| 0x723c10 | CBulletTraces::Render | OK(sqrt differs) | SQRT | - | source/game_sa/BulletTraces.cpp:12 |
| 0x7241c0 | CBrightLights::Render | OK | COS SIN | COS SIN | source/game_sa/BrightLights.cpp:11 |
| 0x724770 | CBrightLights::RegisterOne | OK(sqrt differs) | SQRT | - | source/game_sa/BrightLights.cpp:12 |
| 0x724b60 | CShinyTexts::RegisterOne | OK(sqrt differs) | SQRT | - | source/game_sa/ShinyTexts.cpp:13 |
| 0x725040 | C3dMarkers::Render | OK(sqrt differs) | SIN SQRTx2 | SIN | source/game_sa/3dMarkers.cpp:19 |
| 0x725120 | C3dMarkers::PlaceMarker | OK(sqrt differs) | SINx3 SQRT | SINx3 | source/game_sa/3dMarkers.cpp:25 |
| 0x725c00 | CCheckpoint::Render | OK(sqrt differs) | SQRT | - | source/game_sa/Checkpoint.cpp:10 |
| 0x726090 | CMirrors::RenderMirrorBuffer | OK(sqrt differs) | SQRT | - | source/game_sa/Mirrors.cpp:42 |
| 0x726af0 | CBulletTraces::AddTrace | OK(sqrt differs) | SQRTx2 | - | source/game_sa/BulletTraces.cpp:11 |
| 0x726d40 | C3dMarkers::PlaceMarkerCone | OK(sqrt differs) | SQRT | - | source/game_sa/3dMarkers.cpp:26 |
| 0x728da0 | CWaterCannon::Render | OK(sqrt differs) | SQRTx2 | - | source/game_sa/WaterCannon.cpp:14 |
| 0x729b60 | CAutomobile::FireTruckControl | OK(helper) | ATANx4 COSx2 SINx3 SQRT | ATANx3 COSx3 SINx3 | source/game_sa/Entity/Vehicle/Automobile.cpp:67 |
| 0x72a9a0 | CWeather::AddRain | OK | COS SIN | COS SIN | source/game_sa/Weather.cpp:26 |
| 0x72b630 | CWeather::UpdateInTunnelness | OK | COS SIN | COS SIN | source/game_sa/Weather.cpp:36 |
| 0x72b850 | CWeather::Update | OK | COSx2 | COSx2 | source/game_sa/Weather.cpp:35 |
| 0x72c8b0 | CWindModifiers::RegisterOne | OK(sqrt differs) | SQRT | - | source/game_sa/WindModifiers.cpp:9 |
| 0x72c950 | CWindModifiers::FindWindModifier | OK(sqrt differs) | SQRT | - | source/game_sa/WindModifiers.cpp:10 |
| 0x7328c0 | CVisibilityPlugins::SetRenderWareCamera | EXPLAINED(POW-SQR) | - | POW*x7 | source/game_sa/VisibilityPlugins.cpp:93 |
| 0x733160 | CVisibilityPlugins::SetupVehicleVariables | OK | ATAN SQRT | ATAN SQRT* | source/game_sa/VisibilityPlugins.cpp:95 |
| 0x7360d0 | CBulletInfo::Update | OK | ASIN | ASIN | source/game_sa/BulletInfo.cpp:18 |
| 0x737c80 | CProjectileInfo::AddProjectile | OK | ATANx3 COSx3 SINx3 | ATANx3 COSx3 SINx3 | source/game_sa/ProjectileInfo.cpp:29 |
| 0x738b20 | CProjectileInfo::Update | OK(sqrt differs) | POW SQRTx5 | POW* SQRT*x4 | source/game_sa/ProjectileInfo.cpp:32 |
| 0x739e60 | CShotInfo::Update | OK | POW | POW* | source/game_sa/ShotInfo.cpp:19 |
| 0x73af00 | FireOneInstantHitRound | NOPORT | ASIN | - | source/game_sa/Weapon.cpp:72 |
| 0x73b550 | CWeapon::DoBulletImpact | OK(helper) | ASINx2 | - | source/game_sa/Weapon.cpp:60 |
| 0x73c1f0 | CWeapon::TakePhotograph | OK | SQRTx2 | SQRTx2 | source/game_sa/Weapon.cpp:49 |
| 0x73c710 | CWeapon::SetUpPelletCol | OK(sqrt differs) | COS SIN SQRT | COS SIN | source/game_sa/Weapon.cpp:54 |
| 0x73cdc0 | CWeapon::DoDoomAiming | OK(sqrt differs) | SQRTx5 | - | source/game_sa/Weapon.cpp:50 |
| 0x73d1e0 | DoTankDoomAiming | OK(sqrt differs) | SQRTx6 | SQRT*x4 | source/game_sa/Weapon.cpp:64 |
| 0x73d720 | DoDriveByAutoAiming | OK(sqrt differs) | SQRTx6 TAN | TAN | source/game_sa/Weapon.cpp:65 |
| 0x73df00 | CVehicle::FireFixedMachineGuns | OK | SQRT | SQRT* | source/game_sa/Entity/Vehicle/Vehicle.cpp:245 |
| 0x73e240 | FindNearestTargetEntityWithScreenCoors | OK(sqrt differs) | SQRTx2 | SQRT* | source/game_sa/Weapon.cpp:66 |
| 0x73e560 | EvaluateTargetForHeatSeekingMissile | OK | SQRTx2 | SQRT*x2 | source/game_sa/Weapon.cpp:67 |
| 0x73e800 | CWeapon::FireAreaEffect | OK(helper) | ATAN COS SIN TAN | ATANx2 COS SIN TAN | source/game_sa/Weapon.cpp:55 |
| 0x73fb10 | CWeapon::FireInstantHit | OK(sqrt differs) | COSx2 SINx2 SQRTx3 | COSx2 SINx2 SQRT*x2 | source/game_sa/Weapon.cpp:58 |
| 0x741c00 | CWeapon::FireM16_1stPerson | OK | SQRT | SQRT* | source/game_sa/Weapon.cpp:62 |
| 0x742300 | CWeapon::Fire | OK(helper) | ATAN COS SIN | COS SIN | source/game_sa/Weapon.cpp:63 |
| 0x742cf0 | CWeaponEffects::Render | OK(sqrt differs) | ATANx2 COSx14 SINx15 SQRTx13 | ATANx2 COSx14 SINx15 SQRT*x2 | source/game_sa/WeaponEffects.cpp:23 |
| 0x746a10 | CPad::ProcessPad | EXPLAINED(COUNT-ONLY) | COS SIN | - | source/game_sa/Pad.cpp:41 |
| 0x747200 | CGamma::SetGamma | OK | POW | POW* | source/game_sa/Gamma.cpp:10 |
