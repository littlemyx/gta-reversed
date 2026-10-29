# ASM-S45 (phase 2: Entity/Bike/Bmx/Boat/EmergencyPed/EntityScanner/EntryExit, ...) - 2026-10-29
Result: 41 rows = 19 MATCH + 11 DEVIATION (fixed) + 1 UNSURE + 10 NOT_REACHED. Table: S45.result.tsv. Budget stop at ~110k: CEntryExitManager (9 functions) and CEntity::ProcessLightsForEntity not compared; CEntity::PreRender only the first part (0x535FA0-0x536174).
Task 0 (S44 follow-up): CDarkel::DrawMessages second Y = 148*s + 20*s (0a658d4e2).
Deviations fixed (CEntryExit, the interiors path):
- GetEntryExitToDisplayNameOf (53f67e2ca): an area-0 enex returns this (port nullptr); the stack branch returns stack[posn-2] (port stack[posn], out of bounds at posn 4).
- GenerateAmbientPeds (53f67e2ca): interior ped count is a signed byte.
- IsInArea (3e472ec48): rotated path centre (r-l)*0.5+l, local Z not offset, Z test on the original position.z, float rounding before the centre add-back.
- RequestAmbientPeds (1aeeb6420): port streamed the first NON-matching name (missing == 0) and returned; exe streams every matching name (bar1/strip2/LAstrip/MAFCAS/TRICAS).
- RequestObjectsInFrustum (1aeeb6420): request flags 0x20 (port 0).
- GetPositionRelativeToOutsideWorld (4499fd5a4): (centre + outPos) - exit with unrounded x87 sums.
- FindValidTeleportPoint (83764a4cb): unrounded angle, cos*r + base rounded once.
- WarpGangWithPlayer (8d65c5965): candidate = PLAYER pos + offset (port used the member pos), max 3 attempts per member then return, heading via GetRadianAngleBetweenPoints + LimitRadianAngle.
- TransitionStarted (1f00856d9): warp test is sqrt Magnitude > 10 (port squared); the no-door branch also takes over the fixed camera (port skipped it).
- TransitionFinished (52e61747c): vehicle spawn Z (z-1)+h single rounding.
MATCH: all scalar deleting destructors (Projectile, EmergencyPed, PlayerPed, Bike, Bmx, Boat, Train), Heli/Plane::Fix, Train door ratio, SetCompAlphaCB, SetPedAtomicVisibilityCB, EntityScanner ctor/dtor, Placeable Init/ShutdownMatrixArray, Entity dtor/UpdateAnim, ProcessStealableObjects.
Open range: CEntryExitManager::{SetAreaCodeForVisibleObjects 0x43ECF0, ResetAreaCodeForVisibleObjects 0x43ED80, GetEntryExitIndex 0x43EFD0, LinkEntryExit 0x43F050, PostEntryExitsCreation 0x43F0A0, GetPositionRelativeToOutsideWorld 0x43F150, FindNearestDoor 0x43F630, Init 0x43F880, AddOne 0x43FA00}, CEntity::PreRender tail 0x536174-0x536BAA, CEntity::ProcessLightsForEntity 0x6FC7A0.
Gate: Release build OK; wave2_oracle_test (rebuilt after the last edit, RW_EXE_ORACLE and GTA_EXE = the compact exe) 1142 functions PC24 0/0, PC53 0/0 (the target does not cover EntryExit); combat regress PASS=14; PortableWin64 compile of Darkel.cpp and EntryExit.cpp (subshell, x64 env): 0 errors; v6 pre-commit grep clean on every commit. Note: GetEntryExitToDisplayNameOf+GenerateAmbientPeds and RequestAmbientPeds+RequestObjectsInFrustum share a commit each.
