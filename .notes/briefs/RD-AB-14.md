# RD-AB-14 — frame 88: the exe drops ground contact of every ped standing on a building at fc 664

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-14 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-AB-13.md: prefix 88/88/88, self-identity 300/300; scan codes now equal through fc 664 (CBike ctor fix). At fc 664 the exe
drops the ground contact of ALL peds standing on a building (slots 0,1,3,4 on model 17613, slot 2 on 17880) and they re-land at fc 668
(z 13.34375); the port keeps contact. Model 17613's ref count drops 1 -> 0 at fc 664 on both sides (streaming removes/re-adds the building?
LOD swap? CWorld::Remove of the building entity -> CPed::m_pContactEntity / bIsStanding cleared?). Next step from the report: orig hardware
write watch (RD_WATCH, rd_orig.c) on ped0 m_pContactEntity / bIsStanding to get the writer's eip at fc 664, then the same watch on the port
(NOTSA_LOCKSTEP_WATCH); compare the code path. Small open items: ped flag bHasACamera (port set for peds 0/1, exe clear); BMX audio entity
init fields and one field at +0x3E0.
Tools: RD_BPN / RD_BPMEM / RD_BPMI / RD_BPARG (rd_orig.c), rcmp.py --seq, RD_RSTACK.
Tools: rcmp.py, RD_RSTACK, RD_BP/RD_BPFC, D-line probes, DUMPPN/DUMPSCR/DUMPSTR; own proxy (`export RD_PX=<own dir>` + `run.sh --build-proxy`).
Orig traces of RD-AB-4/5 can be reused while the orig side does not change.
## Task
One divergence at a time (frame / layer / call sites / cause / fix with asm address, consumer audit). Gate (v5): Release + touched targets
+ soak. HARD budget: at 200k tokens stop and land; never pass 250k (RD-AB-5 reached 322k).
## Care
Work ONLY in your worktree. Never fight USER-PLAY. Never kill by image name. Scratch under C:/gta-scratch/rd-ab-14/; never rm -rf a dir with
junctions (leave soakdata for the orchestrator).
