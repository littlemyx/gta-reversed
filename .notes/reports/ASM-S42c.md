# ASM-S42c (phase 2: CCarAI::UpdateCarAI tail + CarEnterExit) - 2026-10-29
Result: 19 rows = 5 FIXED, 5 MATCH, 9 NOT_REACHED (budget). Table: S42c.result.tsv.

Fixes (one commit per function):
- c08957acf CCarAI::UpdateCarAI tail (0x41FE13-0x420678) + FORWARDANDBACK (0x41ED1C): steering-to-hit block uses the sqrt'd, strictly ordered tests of the exe (veh speed < player speed, > 0.1, dist > 12 -> WAIT, < 20, dot > 0.8, speed > 0.6, then > 0.3 / dist < 45 / veh speed > 0.5); first same-direction test is the RAW forward y,x dot > 0; the "player heading to the vehicle" test is 0.5*|ps2D|*|v2D| < ps.v with v = vehicle - player (port tested an unrelated |ps|*0.5 < dist); the exe's extra test (player speed - 0.2) > vehicle speed was missing; forwards are normalised with CVector::Normalise (z=0). Speed multiplier snaps to the target when |diff| < step (port: kept the old value). Gang cruise speed = max(10 - n, 1), n counted with sqrt'd 3D distance < 10. FORWARDANDBACK: direction is player - vehicle (port: opposite sign), 2D length clamped up to 0.001, 1/len multiply, > 0.05.
  MATCH inside the range: horn rand 0x4203C1, water/land block 0x420445, steer-angle maths.
- 89a8e42b7 ComputeTargetDoorToExit: bus/coach test is the handling anim group (103/104) for any ped; only slots 0..2 compared.
- cd0e64d5c IsClearToDriveAway: probe distance size.y*0.5 + 1.5 (port: size.y).
- d1f1f3347 IsVehicleStealable: health > 600 ordered (NaN fails).
- 236d3aabf IsPathToDoorBlockedByVehicleCollisionModel: port returned the inverse (exe: true on a sphere hit); exe flattens line end.z and sphere z to the start z.
MATCH: MakeUndraggedPassengerPedsLeaveCar, MakeUndraggedDriverPedLeaveCar, RemoveCarSitAnim, RemoveGetInAnims, IsCarQuickJackPossible (always false).
NOT_REACHED: GetNearestCarDoor, GetNearestCarPassengerDoor, GetPositionToOpenCarDoor, IsPlayerToQuitCarEnter (port already looks closely transcribed, not compared), IsRoomForPedToLeaveCar, QuitEnteringCar, SetPedInCarDirect, AddInCarAnim, SetAnimOffsetForEnterOrExitVehicle.

Gate: Release build OK; wave2_oracle_test (1142 functions) PC24 0/0, PC53 0/0 (note: none of the touched functions is covered by the oracle); combat regress PASS=14; PortableWin64 compile of CarAI.cpp + CarEnterExit.cpp: 0 errors; v6 pre-commit grep clean.
