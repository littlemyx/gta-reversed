# RD-AB-6 - orig-vs-port divergence at frames 20 (gang follower chain) and 30 + slot-class audit

Agent RD-AB-6, rules v5, 2026-10-25 12:44 - ~13:55 WEST. Worktree C:/gta-wt/rdab6 (branch rdab6 on 43f0319f). Data C:/gta-scratch/rdd/ab6 (r1..r4, o1..o4, soak). Own proxy RD_PX=C:/gta-scratch/rdd/px_ab6.
Orig side: RD-AB-4 traces C:/gta-scratch/rdd/ab4/r10/o1..o3.trace (orig-orig 1500/1500 equal). Documents user-files signature identical before/after every orig run.

## Divergences (0-based traced frames, pin 1023, intro scenario)
| frame | layer | cause | fix / exe address |
|---|---|---|---|
| 20 (a) | rng 87 vs 105 calls | `CTaskComplexSeekEntity<>::ControlSubTask` (exe 0x665760): the exe's result variable starts as `m_pSubTask` (eax=[esi+8] spilled to [esp+0x10] at 0x665790) and every sub-task type other than FOLLOW_NODE_ROUTE(906)/GO_TO_POINT_AND_STAND_STILL(903) falls through 0x665836 -> 0x665D12 and returns it unchanged. The port had `default: return nullptr`, and `CTaskManager::ParentsControlChildren` (0x681A80) treats a different result as "replace": the arrived-sequence (244 -> TurnToFace 920 -> AchieveHeading 902) was MakeAbortable'd (AchieveHeading::MakeAbortable sets tolerance 2pi) and dropped in the SAME frame, GangFollower::CreateNextSubTask fired (leader still: -> StandStill 203, always abortable) and BeInGroup replaced it => follower control skipped. Found with orig breakpoints: AchieveHeading::ProcessPed (0x668060) is NEVER called on that chain in frames 19/20 in the orig, MakeAbortable (0x667EB0) is (frames 19 and 20, same task), so the chain stays; port log showed GFNEXT sub=907 in frame 19. | TaskComplexSeekEntity.h: `default: return m_pSubTask` |
| 20 (b) | rng (4th draw of the car-drive/event block) | `CEventHandler::ComputeChatPartnerResponse` (0x4B98E0): `GetRandomNumberInRange(3, 7)` at 0x4B9934..0x4B993D, result DISCARDED (never reaches the CTaskComplexPartnerChat ctor) - the port omitted the call, so one RNG draw was missing per chat-partner event. Seen with the orig stack probe RD_RSTACK=407100-407200 (T lines: ret 0x4B993D, caller HandleEvents 0x4C39CF/0x4C4149). | EventHandler.cpp: draw added |
| 30 | rng 56 vs 58 calls | NOT analysed (out of budget) | - |

## Slot-class audit (brief item 1)
`GetFollowers()/GetMembers()` skip null slots, so an index taken from enumerate() is a compact rank, not the member slot (the exe loops over the 7 follower slots):
* `CPedGroup::Teleport` (0x5F7AD0, loop 0x5F7BE0..0x5F7C9B): formation offset index = member SLOT -> PedGroup.cpp now loops slots 0..6.
* `CGroupEventHandler::ComputeResponseLeaderExitedCar` (0x5F90A0, loop 0x5F90D8..0x5F9506, ebx += 500 per SLOT, null slot skipped): leave-car delay `slot * 500` (+ (slot+1)*500 in the burning-car case) -> GroupEventHandler.cpp loops slots 0..6.
* All other users of GetMembers()/GetFollowers() iterate without using the index (checked by grep: EntryExit, EventLeaderExitedCarAsDriver, GroupEventHandler other loops, allocators KillThreats*, SeekCoverUntilTargetDead). Not touched, needs its own asm check: `CPedGroupDefaultTaskAllocatorChat::AllocateDefaultTasks` (0x5F8180) is heavily refactored (vector copy of GetMembers(), SetDefaultTask inside the loop, RandomChoice order) - rand order/count vs the exe unverified; intro does not reach it.

## Equal prefixes (orig-port, pin 1023, intro; 1 rep x 1500 frames per step, port-only runs against the RD-AB-4 orig trace)
before (RD-AB-5 tree 43f0319f): 20 -> after SeekEntity ControlSubTask fix: 20 (first chain-end gone, rng still 87 vs 105) -> after ChatPartner draw: 30 (3 reps not repeated; port-port identity not re-measured this run).

## Gate (rules v5)
Release build (StandaloneRelease, this tree) ok. Soak (gpu_locked, NOVSYNC+MUTE, 3 min request): RESULT OK, errors 0 (one route gate timeout: until invehicle 8000 ms, route-side). Oracle targets: none touched (no oracle rows for these functions). Not run: run_all_tests (orchestrator).
Consumer audit: SeekEntity::ControlSubTask default now returns the running sub-task (as the exe) - affects every CTaskComplexSeekEntity<XYOffset> user (gang followers, group formation, etc.): previously a seek whose sub-task was the arrival sequence/turn/tired/stand-still was torn down every frame. The Standard/other instantiations share the template code (no hooks there; same source). ChatPartner draw: one rand per event, no other readers. Slot loops: formation offsets/leave delay only.
