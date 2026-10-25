# RD-AB-8 — frame 42 (ChooseModel gang branch) + SetupSuspensionLines of the other vehicle classes

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-8 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-AB-7.md: orig-port prefix 42/42/42 (3 reps x 1500, port runs vs the RD-AB-4 orig traces), port-port 1500/1500.
1. Frame 42 (rand 81 orig vs 129 port): orig CCarCtrl::ChooseModel (0x424CE0) leaves the gang branch right after PickGangCar (0x611d35
   draw); the port continues into the gang idx loop (CarCtrl.cpp:457) and draws 50 times. Candidates: streaming state at frame 42
   (gang car models loaded or not), CPopulation::PickGangCar (0x614490), m_CarGroups[BALLAS] indexing. Compare and fix.
2. RD-AB-7 fixed CAutomobile::SetupSuspensionLines (0x6A681C..0x6A68B4: shared col bbox min.z lowered to the first line end, sphere radius
   raised unconditionally). Check CBike / CQuadBike / CTrailer / CMonsterTruck (and CBmx, CHeli/CPlane if they have one) SetupSuspensionLines
   against the exe the same way; fix deviations (consumer audit: shared model-info col model).
Tools: RD_BPN / RD_BPMEM / RD_BPMI / RD_BPARG (rd_orig.c), rcmp.py --seq, RD_RSTACK.
Tools: rcmp.py, RD_RSTACK, RD_BP/RD_BPFC, D-line probes, DUMPPN/DUMPSCR/DUMPSTR; own proxy (`export RD_PX=<own dir>` + `run.sh --build-proxy`).
Orig traces of RD-AB-4/5 can be reused while the orig side does not change.
## Task
One divergence at a time (frame / layer / call sites / cause / fix with asm address, consumer audit). Gate (v5): Release + touched targets
+ soak. HARD budget: at 200k tokens stop and land; never pass 250k (RD-AB-5 reached 322k).
## Care
Work ONLY in your worktree. Never fight USER-PLAY. Never kill by image name. Scratch under C:/gta-scratch/rd-ab-8/; never rm -rf a dir with
junctions (leave soakdata for the orchestrator).
