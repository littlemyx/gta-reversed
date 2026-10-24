# RD-NOISE — slice 2 of .notes/REPLAY_DIFF_PLAN.md, plus the first A-B lead (frame 1: 74 vs 317 rand calls)

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh rd-noise, based on origin/rd-arm (50e686ca, not yet
on reverse/interior_c — the orchestrator lands it after its gate; at the end rebase onto whatever is there). Push your work to branch
rd-noise; land on reverse/interior_c only if rd-arm is already there and your gate passes.
## Evidence
.notes/reports/RD-ARM.md (tables, rlog per-call logs under C:/gta-scratch/rdd/rdarm*): arm frame 0 bit-identical orig vs port.
A-A equal prefix: orig 91, port 7-9. Orig frames 10-18 differ in population-spawn rand sequences (plate text, PickRandomCar,
GeneratePedCreationCoors) -> CdStream read completion vs frame (plan row 7); one audio rand; port frame 19 one extra
CGeneral::GetRandomNumber (General.cpp:203, caller unknown); pointer-derived srand at frame 1509 (Entity.cpp:1056).
A-B: frame 1 rand calls 74 (orig) vs 317 (port).
## Task
0. FIRST (cheap, high value): `run.sh --rlog` one orig and one port run; list frame-1 rand call sites on both sides (orig: return
   addresses -> symbols via the exe map/.notes/symbols.txt; port: file:line); explain the 243 extra port calls (a port deviation =
   fix it as the exe does, with an oracle row if possible). Report the site table.
1. Then the plan's slice 2 in the order it gives: CdStream tick barrier (reads complete at a frame boundary in both exes under a pin),
   audio RNG partition, pointer-seeded srand, Grain/OS_TimeMS/time(), threads.
Acceptance: frame-1 explanation + fix(es); A-A equal prefix improved (numbers, 3 reps each side). Port-side code inert without env vars;
gate before landing any game-code fix: Release, run_all_tests (do not kill a wrapper and leave the suite running — kill by Windows pid),
soak. HARD budget 250k tokens / 100 min.
## Care
Original exe needs the foreground; never fight USER-PLAY for gpu.lock. Data under C:/gta-scratch/rd*/. Never commit gta_sa_compact.exe.
