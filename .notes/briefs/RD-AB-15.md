# RD-AB-15 — frame 88: why the port keeps IPL slots 69..92 requested at the cutscene LoadScene

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-15 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-AB-14.md: at fc 664 the intro cutscene starts (LoadCutSceneFile -> CStreaming::LoadScene 0x40EB70 -> CIplStore::LoadIpls
0x405170). In the exe IPL slots 69,71,80,81,82,83,85,92 have loadRequested=0 there and are unloaded (~1094 buildings, 917 dummies, incl.
the peds' contact building, model 17613, pool slot 9592); peds fall 4 frames until the IPLs reload at fc 667-669. In the port the same slots
still have loadRequested=1. Candidates: the three callers inside LoadIpls — SetIplsRequired(offset), SetIplsRequired(gvecIplsNeededAtPosn),
the mission-cleanup entity loop (0x405250..0x405349); also who SETS loadRequested before (CIplStore::RequestIpls / AddIplsNeededAtPosn /
the cutscene's own position, the player position used, IPL bounding boxes, radius) and who CLEARS it in the exe (ClearIplsNeededAtPosn).
Next step from the report: a Note after each candidate in the port and BPs at 0x40521F / 0x40523F / 0x405355 in the exe (RD_BPIPL /
RD_BPREQ / RD_BPIPLS probes exist), compare the per-slot loadRequested on both sides at each point.
Tools: RD_BPN / RD_BPMEM / RD_BPMI / RD_BPARG (rd_orig.c), rcmp.py --seq, RD_RSTACK.
Tools: rcmp.py, RD_RSTACK, RD_BP/RD_BPFC, D-line probes, DUMPPN/DUMPSCR/DUMPSTR; own proxy (`export RD_PX=<own dir>` + `run.sh --build-proxy`).
Orig traces of RD-AB-4/5 can be reused while the orig side does not change.
## Task
One divergence at a time (frame / layer / call sites / cause / fix with asm address, consumer audit). Gate (v5): Release + touched targets
+ soak. HARD budget: at 200k tokens stop and land; never pass 250k (RD-AB-5 reached 322k).
## Care
Work ONLY in your worktree. Never fight USER-PLAY. Never kill by image name. Scratch under C:/gta-scratch/rd-ab-15/; never rm -rf a dir with
junctions (leave soakdata for the orchestrator).
