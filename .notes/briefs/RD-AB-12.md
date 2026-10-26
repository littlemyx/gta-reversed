# RD-AB-12 — frame 78 chain: the ped-collision event reaches the response 9 frames late in the port

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-12 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-AB-11.md: SetMoveAnim walk-speed fix landed (walkers bit-equal for 90 frames), prefix still 78/78/78, self-identity 300/300.
The exe calls CEventHandler::ComputePedCollisionWithPedResponse (0x4BDB80, victim = the standing model-106 ped) for the Grove follower
(ped slot 8) at fc 1048624..632; the port first at 1048633. The ped's last-collision record is bit-identical at fc 623/624, so the gap is
whether/when the CEventPedCollisionWithPed added in CollisionEventScanner.cpp (ped case, exe 0x604B56..0x604D3E) and CEventGroup::Add reaches
CEventHandler::HandleEvents / ComputeEventResponseTask (0x4C3870): event priority / TakesPriorityOver / IsValid / AffectsPed / the event-group
"has event" checks / temporary vs permanent event slots. In the exe the response does SetMaxEntityDist2D(dist+0.1), which swaps
FOLLOW_NODE_ROUTE for GO_TO_POINT_AND_STAND_STILL and turns the ped (= the frame-78 chain). Lead: orig BP on CEventGroup::Add and
CEventHandler::HandleEvents for that ped at fc 1048623..624. Also open (not urgent): ped slot 5 (model 106, likely a vehicle rider) position
64 ulp off from fc 1048612.
New probes (RD-AB-11): run.sh --pin bits 1024 (P ped lines), 2048 (A/Q), 4096 (C task chains), 8192 (K last collision); pdiff.py.
Tools: RD_BPN / RD_BPMEM / RD_BPMI / RD_BPARG (rd_orig.c), rcmp.py --seq, RD_RSTACK.
Tools: rcmp.py, RD_RSTACK, RD_BP/RD_BPFC, D-line probes, DUMPPN/DUMPSCR/DUMPSTR; own proxy (`export RD_PX=<own dir>` + `run.sh --build-proxy`).
Orig traces of RD-AB-4/5 can be reused while the orig side does not change.
## Task
One divergence at a time (frame / layer / call sites / cause / fix with asm address, consumer audit). Gate (v5): Release + touched targets
+ soak. HARD budget: at 200k tokens stop and land; never pass 250k (RD-AB-5 reached 322k).
## Care
Work ONLY in your worktree. Never fight USER-PLAY. Never kill by image name. Scratch under C:/gta-scratch/rd-ab-12/; never rm -rf a dir with
junctions (leave soakdata for the orchestrator).
