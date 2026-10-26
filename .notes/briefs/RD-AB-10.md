# RD-AB-10 — frame 78 (potential-collision event scan) and onward

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-10 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-AB-9.md: orig-port prefix 78/78/78 (3 reps x 1500, pin 1023), port-port 1500/1500. Frame 78: rand 58 (orig) vs 84 (port).
In frames 40..77 both sides draw 4 times per frame in CEventGroup::Add called from CEventScanner::ScanForPedPotentialCollisionEvents
(0x606580, return 0x606867); the orig stops after frame 77, the port keeps drawing through frame 100. Lead: orig breakpoint at 0x606580
around fc 1048653..1048654, then compare the scanner's candidate test (distance / heading / IsPedHeadingTowards / timers) with the port
line by line. An orig R-log trace covering frames 0..100 exists: C:/gta-scratch/rd-ab-9/o100/o1.trace (keep it; the RD-AB-4 ones cover 32).
Tools: RD_BPN / RD_BPMEM / RD_BPMI / RD_BPARG (rd_orig.c), rcmp.py --seq, RD_RSTACK.
Tools: rcmp.py, RD_RSTACK, RD_BP/RD_BPFC, D-line probes, DUMPPN/DUMPSCR/DUMPSTR; own proxy (`export RD_PX=<own dir>` + `run.sh --build-proxy`).
Orig traces of RD-AB-4/5 can be reused while the orig side does not change.
## Task
One divergence at a time (frame / layer / call sites / cause / fix with asm address, consumer audit). Gate (v5): Release + touched targets
+ soak. HARD budget: at 200k tokens stop and land; never pass 250k (RD-AB-5 reached 322k).
## Care
Work ONLY in your worktree. Never fight USER-PLAY. Never kill by image name. Scratch under C:/gta-scratch/rd-ab-10/; never rm -rf a dir with
junctions (leave soakdata for the orchestrator).
