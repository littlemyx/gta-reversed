# RD-AB-16 — frame 98: GoToPoint arrival / ped 8 position after ManageTasks

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-16 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-AB-15.md: prefix 98/98/98 (300 x 3), self-identity 300/300. Frame 98 (fc 674): one extra port rand() because
CTaskComplexGangFollower::ControlSubTask runs for ped 8 twice (exe once): ped 8's leaf CTaskSimpleGoToPoint::ProcessPed (0x66D710) returns true
in the port, false in the exe. Ped 8's position differs from fc 674: the exe moved it in fc 673 after ManageTasks, the port did not. Next:
dump pos, m_vecMoveSpeed, m_vecAnimMovingShift at the end of CPed::ProcessControl on both sides (pin probes / RD_BPMEM), then the GoToPoint
arrival test (target radius, 2D distance, the exe's x87 compare order). Also open, harmless so far: ped init bHasACamera, m_nMoneyCount
(exe 10, port 0), audio-entity init fields — check the ped ctor / CPopulation::AddPed money setup vs the exe while there.
Probes: RD_BPGF (GangFollower::ControlSubTask), RD_BPPP (leaf ProcessPed result in ManageTasks).
Tools: RD_BPN / RD_BPMEM / RD_BPMI / RD_BPARG (rd_orig.c), rcmp.py --seq, RD_RSTACK.
Tools: rcmp.py, RD_RSTACK, RD_BP/RD_BPFC, D-line probes, DUMPPN/DUMPSCR/DUMPSTR; own proxy (`export RD_PX=<own dir>` + `run.sh --build-proxy`).
Orig traces of RD-AB-4/5 can be reused while the orig side does not change.
## Task
One divergence at a time (frame / layer / call sites / cause / fix with asm address, consumer audit). Gate (v5): Release + touched targets
+ soak. HARD budget: at 200k tokens stop and land; never pass 250k (RD-AB-5 reached 322k).
## Care
Work ONLY in your worktree. Never fight USER-PLAY. Never kill by image name. Scratch under C:/gta-scratch/rd-ab-16/; never rm -rf a dir with
junctions (leave soakdata for the orchestrator).
