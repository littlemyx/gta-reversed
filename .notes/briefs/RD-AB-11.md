# RD-AB-11 — frame 78: the gang follower's walk-target z (4 ulp) and the early route-point switch

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-11 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-AB-10.md: prefix still 78. ScanForPedPotentialCollisionEvents matches the exe; the 4 draws/frame in 40..77 come from one
Grove ped (model 107) following the player: BE_IN_GROUP > GANG_FOLLOWER > SEEK_ENTITY > FOLLOW_NODE_ROUTE > GO_TO_POINT_AND_STAND_STILL >
GO_TO_POINT on both sides. Its waypoint z: 0x41580457 (exe) vs 0x4158045b (port) — a float-rounding difference (x87 PC24 op order / a
double intermediate / int16 node coordinate scaling / CVector op order). At fc 1048630 the exe rewrites the target z to the ped's z and
keeps the same waypoint; at fc 1048629 the port switches to the next route point. Find the writer of the waypoint z (path node ->
route: CPathFind node position decode, CTaskComplexFollowNodeRoute::ComputeRoute / CreateNextSubTask 0x6718D0, CTaskComplexGangFollower::
ControlSubTask 0x662A10, CTaskSimpleGoToPoint::UpdatePoint 0x645700) and the route-advance test (distance vs radius, the exe's z rewrite);
orig BP at 0x66D710 / 0x645700 on that ped shows the caller. Fix rounding/order as the exe does (asm address; an oracle row for the
decode/advance function if feasible).
Tools: RD_BPN / RD_BPMEM / RD_BPMI / RD_BPARG (rd_orig.c), rcmp.py --seq, RD_RSTACK.
Tools: rcmp.py, RD_RSTACK, RD_BP/RD_BPFC, D-line probes, DUMPPN/DUMPSCR/DUMPSTR; own proxy (`export RD_PX=<own dir>` + `run.sh --build-proxy`).
Orig traces of RD-AB-4/5 can be reused while the orig side does not change.
## Task
One divergence at a time (frame / layer / call sites / cause / fix with asm address, consumer audit). Gate (v5): Release + touched targets
+ soak. HARD budget: at 200k tokens stop and land; never pass 250k (RD-AB-5 reached 322k).
## Care
Work ONLY in your worktree. Never fight USER-PLAY. Never kill by image name. Scratch under C:/gta-scratch/rd-ab-11/; never rm -rf a dir with
junctions (leave soakdata for the orchestrator).
