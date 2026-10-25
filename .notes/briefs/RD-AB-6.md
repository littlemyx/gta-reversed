# RD-AB-6 — frame 20 (gang follower task chain) + the same slot-enumeration bug class

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-6 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-AB-5.md: orig-port prefix 20/20/20 (3 reps x 1500, pin 1023, intro). Fixed there: CPedGroupIntelligence::Process syncs the
8 member SLOTS incl. nulls (0x5FC4B1..0x5FC604) — the port enumerated GetMembers() (skips nulls) and put the leader into follower slot 0.
1. SAME BUG CLASS, not yet touched: `rngv::enumerate(GetFollowers())` at PedGroup.cpp:161 and GroupEventHandler.cpp:293 (and grep for
   every other enumerate(GetMembers()/GetFollowers()) whose index is used as a slot number) — compare each with the exe's slot loop; fix.
2. Frame 20 (rand 87 orig vs 105 port): orig runs GangFollower control on all 3 followers + the leader's ScanForStuff (0x65E265); in the port
   the first follower gets its BeInGroup sub-task replaced (MonitorMainGroupTask 0x633010, replace path 0x63307D) and skips follower control.
   Orig breakpoints: that ped mismatches but is not aborted in frames 19/20; in the port its chain ends Follower -> StandStill (always
   abortable) — probably the seek chain finishes in one frame (CTaskComplexGangFollower::CreateNextSubTask). Compare with the exe and fix.
Tools: rcmp.py, RD_RSTACK, RD_BP/RD_BPFC, D-line probes, DUMPPN/DUMPSCR/DUMPSTR; own proxy (`export RD_PX=<own dir>` + `run.sh --build-proxy`).
Orig traces of RD-AB-4/5 can be reused while the orig side does not change.
## Task
One divergence at a time (frame / layer / call sites / cause / fix with asm address, consumer audit). Gate (v5): Release + touched targets
+ soak. HARD budget: at 200k tokens stop and land; never pass 250k (RD-AB-5 reached 322k).
## Care
Work ONLY in your worktree. Never fight USER-PLAY. Never kill by image name. Scratch under C:/gta-scratch/rd-ab-6/; never rm -rf a dir with
junctions (leave soakdata for the orchestrator).
