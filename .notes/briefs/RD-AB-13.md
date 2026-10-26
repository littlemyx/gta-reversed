# RD-AB-13 — frame 88: world scan code one behind, peds keep ground contact in the port

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-13 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-AB-12.md: prefix 88/88/88 (300 frames x 3), self-identity 300/300. Frame 88 (fc 1048664): the exe loses ground contact
of ped slots 0,1,3,4 (player + Grove gang on building model 17613) for 4 frames (fc 664..667) and re-lands at z 13.34375; the port keeps
contact. CWorld::ms_nCurrentScanCode (0xB7CD78) runs 1 behind the exe from at least fc 662; from fc 665 the scans/frame diverge (exe +48,
port +27). Model 17613 colmodel identical; its ref count drops 1 -> 0 at fc 664 on both sides. Next: orig BP on CWorld::AdvanceCurrentScanCode
(0x4072E0) for fc 663..665 with return addresses, a non-inline probe on the port side; find the missing/extra scan and the code path.
Also: ped flag bHasACamera set in the port for peds 0 and 1, clear in the exe — check where it is set (exe vs port).
Tools: RD_BPN / RD_BPMEM / RD_BPMI / RD_BPARG (rd_orig.c), rcmp.py --seq, RD_RSTACK.
Tools: rcmp.py, RD_RSTACK, RD_BP/RD_BPFC, D-line probes, DUMPPN/DUMPSCR/DUMPSTR; own proxy (`export RD_PX=<own dir>` + `run.sh --build-proxy`).
Orig traces of RD-AB-4/5 can be reused while the orig side does not change.
## Task
One divergence at a time (frame / layer / call sites / cause / fix with asm address, consumer audit). Gate (v5): Release + touched targets
+ soak. HARD budget: at 200k tokens stop and land; never pass 250k (RD-AB-5 reached 322k).
## Care
Work ONLY in your worktree. Never fight USER-PLAY. Never kill by image name. Scratch under C:/gta-scratch/rd-ab-13/; never rm -rf a dir with
junctions (leave soakdata for the orchestrator).
