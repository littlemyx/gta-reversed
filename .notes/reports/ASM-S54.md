# ASM-S54 (+ S53b, + S53 task 0) (phase 2: PedGeometryAnalyser / PedGroup / PedClothesDesc; EventHandler FleeAnyMeans sites) - 2026-10-29
Result: S53b 25 rows = 15 MATCH + 9 DEVIATION (all fixed) + 1 NOT_REACHED (ComputeClearTarget 0x5F5D80). S54 40 rows = 40 NOT_REACHED (budget spent on S53b). Tables: S53b.result.tsv, S54.result.tsv.
Task 0 (8ae62259f): all 7 FleeAnyMeans sites in EventHandler.cpp (GunAimedAt 0x4C2B36/0x4C2CAE/0x4C2D55, PedThreat 0x4C19D3/0x4C20A8 + the cop arms) now go through one helper with the exe args (fleeTime/shoot/steal/period globals) and tolerance [0xC18CF0]. Extra find: ComputePedThreatResponse SMART_FLEE_ENTITY (0x4C1C4B) had the arms swapped (exe: wanted+cop -> FleeAnyMeans, else SmartFlee, walk only for EVENT_ACQUAINTANCE_PED_DISLIKE).
Fixes (one commit each):
- 8c058610a CPedGroup::Teleport (0x5F7AD0): early-out only when the CURRENT group event wraps EVENT_LEADER_ENTRY_EXIT (port returned when there was NO event, so followers never teleported, and tested the wrapper type).
- 7d43fad8a CPedGroup::IsAnyoneUsingCar (0x5F7DB0): enter-car driver (701) AND passenger (700) tasks are both compared to the vehicle.
- 783bf430b CPedGroup::PlayerGaveCommand_Attack (0x5F7CC0): the order shout is said by the commanding ped (mov ecx,esi), not the target.
- 01ff91827 CanPedTargetPoint (0x5F1B70): dist^2 sum order z,x,y. c96b6c270 CanPedTargetPed (0x5F1C40): (z+0.75)-1.0, two roundings.
- 65ae0997f CanPedJumpObstacle contacted (0x5F32D0): NaN arms of the slope tests. 05f20a5db IsWanderPathClear (0x5F2F70): z,y,x magnitude sum + water max-z NaN arm.
- f589cf7ec GetNearestPed (0x5F3590): z,x,y sum. 381d39832 GetIsLineOfSightClear(ped) (0x5F5A30): outDist z,y,x sum; classification/clip arms/return values verified.
MATCH: PedClothesDesc ctor + both SetTextureAndModel; CanPedJumpObstacle(simple), IsInAir, ComputeBuildingHitPoints, GetIsLineOfSightClear(v3d), the 3 BoundingBox wrappers (Corners / Planes / SegmentPlanes: 0x5F3660 = CornersUncached + PlanesUncached, same as the UncachedAll variant the port callers use), PedGroup ctor/dtor/Flush/Process/Gather.
Open: ComputeClearTarget (0x5F5D80), all 40 S54 rows.

## ASM-S54b (S54 + S53c) - 2026-10-30
Result: S54 40 rows = 20 MATCH + 7 DEVIATION (all fixed) + 1 UNSURE + 12 NOT_REACHED (budget). S53c ComputeClearTarget (0x5F5D80) = NOT_REACHED. Tables: S54.result.tsv, S53c.result.tsv.
Fixes (one commit each):
- db8d541e9 CPedGroupMembership::AddFollower (0x5F8020): free-slot search is slots 0..6 only (port could put a follower into an empty leader slot).
- 8b638a49a CPedIK::RotateTorso (0x5FDDB0): changeRoll -> PRECONCAT for the yaw rotation; end flag is bUpdateMatricesRequired (port set bDontAcceptIKLookAts).
- 68e2d8267 CPedIK::PointGunInDirection (0x5FDC00): NaN normalize -> non-normalized MoveLimb.
- f5ff3df82 CPedIK::RotateTorsoForArm (0x5FDF90): angle > 90deg clamps to 45deg (port returned), unrounded fpatan angle, exe constants, no-neck torso gets the full angle.
- 8e7bcc416 CPedIK::PitchForSlope (0x5FE0E0): (1-0.9)*{1,0.85} limit multiplier, exe float clamp bounds, 0.75*sin(pitch) scaled first.
- b8e698237 CPedList::ExtractPedsWithGuns (0x69A4C0): full destination list drops the ped (no array overflow).
- 135ed5db3 CPedGroups::Process (0x5FC800): decision-maker switch on mission start/end skips mission groups.
MATCH: PedIK ctor / GetWorldMatrix / PointGunAtPosition (a PC24-equal rewrite was dropped), PedGroupMembership::SetLeader / GetObjectForPedToHold, PedGroups AddGroup / Init / CleanUpForShutDown, PedList (6 more), PedPlacement (2), PedGroupIntelligence FlushTasks / SetDefaultTaskAllocator / ReportAllBarScriptTasksFinished / ProcessIgnorePlayerGroup.
UNSURE: PedGroupMembership::From (0x5F7FE0) - exe AddMember()s every slot (RegisterReference + DM type 0, crashes on a null slot), port default-copies; no live caller.
Open: PedGroupPlacer (4: PlaceFormationGroup / Chat / Random / PlaceGroup), PedGroupIntelligence (IsCurrentEventValid, ComputeScriptCommandTasks, SetEventResponseTask, SetScriptCommandTask, ReportFinishedTask, ComputeDefaultTasks, ComputeEventResponseTasks, Process), S53c ComputeClearTarget.
