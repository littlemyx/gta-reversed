# RD-AB-17 — next orig-vs-port divergences from frame 145

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-17 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-ENV.md: HEAD 76e12e2f gives 145/145/145 vs RD-AB-15's orig traces (o1..o3), orig-orig / port-port 300/300. Frame 145: rand
calls exe 6 / port 10, not analysed.
RULE (RD-ENV): every port exe used for A-B must have its gta_reversed.map (or <exe>.audio_rva) next to it; rd_port_run.sh now refuses
otherwise. Quote the runner's "audio_rva ranges: N" line (349 = good) with every prefix you report.
Open side items (harmless so far): ped init bHasACamera, m_nMoneyCount (exe 10, port 0), audio-entity init fields.
Tools: RD_BPN / RD_BPMEM / RD_BPMI / RD_BPARG (rd_orig.c), rcmp.py --seq, RD_RSTACK.
Tools: rcmp.py, RD_RSTACK, RD_BP/RD_BPFC, D-line probes, DUMPPN/DUMPSCR/DUMPSTR; own proxy (`export RD_PX=<own dir>` + `run.sh --build-proxy`).
Orig traces of RD-AB-4/5 can be reused while the orig side does not change.
## Task
One divergence at a time (frame / layer / call sites / cause / fix with asm address, consumer audit). Gate (v5): Release + touched targets
+ soak. HARD budget: at 200k tokens stop and land; never pass 250k (RD-AB-5 reached 322k).
## Care
Work ONLY in your worktree. Never fight USER-PLAY. Never kill by image name. Scratch under C:/gta-scratch/rd-ab-17/; never rm -rf a dir with
junctions (leave soakdata for the orchestrator).
