# RD-AB-9 — gang choice divergence (D line from frame 14) and the frame-73 rand divergence

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-9 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-AB-8.md: orig-port prefix 73/73/73 (3 reps x 1500, pin 1023), port-port 1500/1500. The frame-42 divergence was the
port audio-rand partition never being armed (rd_port_run.sh now derives the RVA ranges from the .map; audio_rva.py excludes shared inline
helpers) — keep using that runner.
1. From traced frame 14 the D line (not the F hash) shows the first gang ped is gang index 1 (Grove) in the exe but 8 in the port, with
   equal rand counts. Suspects: CPopulation::PickGangToCreateMembersOf (0x60F8D0), ChooseGangOccupation, the ped type read from the model
   info, gang index <-> ped type mapping. Fix as the exe.
2. Frame 73: rand 133 (orig) vs 42 (port), not analysed (may be the same cause).
Tools: RD_BPN / RD_BPMEM / RD_BPMI / RD_BPARG (rd_orig.c), rcmp.py --seq, RD_RSTACK.
Tools: rcmp.py, RD_RSTACK, RD_BP/RD_BPFC, D-line probes, DUMPPN/DUMPSCR/DUMPSTR; own proxy (`export RD_PX=<own dir>` + `run.sh --build-proxy`).
Orig traces of RD-AB-4/5 can be reused while the orig side does not change.
## Task
One divergence at a time (frame / layer / call sites / cause / fix with asm address, consumer audit). Gate (v5): Release + touched targets
+ soak. HARD budget: at 200k tokens stop and land; never pass 250k (RD-AB-5 reached 322k).
## Care
Work ONLY in your worktree. Never fight USER-PLAY. Never kill by image name. Scratch under C:/gta-scratch/rd-ab-9/; never rm -rf a dir with
junctions (leave soakdata for the orchestrator).
