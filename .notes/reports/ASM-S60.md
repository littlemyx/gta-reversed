# ASM-S60 (+ S46b, shared helpers NaN) - 2026-10-30
Tables: S46b.result.tsv (3 CEventHandler rows + 2 shared-helper rows), S60.result.tsv (40 rows).
Result S46b: 2 MATCH + 1 FIXED (plus 2 shared-helper FIXED). Result S60: 40 rows = 18 MATCH + 5 FIXED + 17 NOT_REACHED (TaskAllocator* x14, CTask::operator new, CTaskComplex ?, CTaskManager dtor; budget).

Fixes (one commit each):
- CEventHandler::ComputeReviveResponse (0x4B97B0, d0f0d2582): the default wander task is stored in slot 4 (DEFAULT) - the port stored it in PRIMARY; the get-up and say tasks are allocated before the wander task.
- CVector2D::Normalise (0x44E480, a1c5d250b): the zero test is on the unrounded squared length, a NaN sum (jp) goes to the sqrt path and gives NaN components (port: NaN -> x = 1); reciprocal kept unrounded like CVector::Normalise. The hook is not installed (extra outMag parameter), no oracle row covers it; callers are all port code (122 sites), exe form is the reference.
- CPedIntelligence::GetPedFOVRange (0x4ABEA0, a02f56d87): hearing range only when strictly greater, otherwise the seeing range (equal / NaN differ from std::max).
- Chat allocator (0x5F8180, d3eed3109): the exe builds one sequence per member slot (go-to radius 2.0, repeat) and then runs one round PER MEMBER; in each round a single random member (picked first) chats and the others stand still, all for the same random time; the default task pair gets a clone only when no ped was passed or the pair belongs to it. The port did a single round with per-member partner/time and used SetDefaultTask with the (possibly null) ped.
- FollowAnyMeans (0x5F6B40, 35fd3a714) / FollowLimited (0x5F6C70, 839cb9452): slots 0..6 follow the leader, the leader slot (7) wanders; the port tested `ped == leader`, so a whole-group allocation made everybody wander.
- CTaskComplexSequence::Contains (0x41BF10, 63c17f8dc): null slots are skipped (the port dereferenced them).
- CStuckCarCheck::AddCarToCheck (0x465970, 23263dacd): a slot is free when its handle is < 0 (jl).

MATCH: RegisterKill, ComputeChatPartnerResponse, all 6 allocator GetType, Random / SitInLeaderCar / StandStill AllocateDefaultTasks, CPedGroupDefaultTaskAllocators::Get (jump table), CTaskComplexSequence Clone / CreateNextSubTask (both) / CreateFirstSubTask / AddTask(idx), CTaskComplexUseSequence CreateNextSubTask / CreateFirstSubTask / Clone.
Open: the 17 NOT_REACHED rows.
Tests: wave2_oracle_test (RW_EXE_ORACLE = GTA_EXE = compact exe, rebuilt after the last edit): 1142 functions, mismatches PC24 0 / 0, PC53 0 / 0. gameplay_regress combat: PASS=14. v6 pre-commit grep clean. PortableWin64: Vector2D, PedIntelligence, TaskComplexSequence, StuckCarCheck, PedGroupIntelligence (includes the three allocator headers), EventHandler compile with 0 errors (cl /Zs with the main tree's PortableWin64 compile commands pointed at the worktree).
