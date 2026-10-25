# RD-AB-7 — next orig-vs-port divergences from frame 30 on

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-6 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-AB-6.md: orig-port prefix 20 -> 30 (1 port run vs the RD-AB-4 orig trace). Frame 30: rand 56 (orig) vs 58 (port), not
analysed. Method that works (RD-AB-6): `--rlog 32` on both legs, `rcmp.py --seq`, then RD_RSTACK / RD_BP on the orig. First re-measure
port-port identity and the orig-port prefix with 3 reps (RD-AB-6 measured 1 rep only). Also unchecked: CPedGroupDefaultTaskAllocatorChat::
AllocateDefaultTasks (0x5F8180) is heavily refactored in the port — its rand order vs the exe.
Tools: rcmp.py, RD_RSTACK, RD_BP/RD_BPFC, D-line probes, DUMPPN/DUMPSCR/DUMPSTR; own proxy (`export RD_PX=<own dir>` + `run.sh --build-proxy`).
Orig traces of RD-AB-4/5 can be reused while the orig side does not change.
## Task
One divergence at a time (frame / layer / call sites / cause / fix with asm address, consumer audit). Gate (v5): Release + touched targets
+ soak. HARD budget: at 200k tokens stop and land; never pass 250k (RD-AB-5 reached 322k).
## Care
Work ONLY in your worktree. Never fight USER-PLAY. Never kill by image name. Scratch under C:/gta-scratch/rd-ab-6/; never rm -rf a dir with
junctions (leave soakdata for the orchestrator).
