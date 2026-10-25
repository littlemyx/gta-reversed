# RD-AB-4 — orig-vs-port divergence at frame 10 (population) and onward

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-4 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-AB-3.md: orig-port equal prefix 10/10/10 (3 reps, pin 1023, intro scenario). Frame 10: rand calls 306 (orig) vs 184
(port), peds 4 vs 3 -> population spawn differs (CPopulation::AddToPopulation / GeneratePedsAtStartOfGame / ped creation coords /
CPopulation::Update timing). Tools: rcmp.py (per-frame rand-site tables), RD_RSTACK, D-line pn=/rw=, DUMPSCR/DUMPSTR.
IMPORTANT (RD-AB-3): use your own proxy: `export RD_PX=<own dir>` + `run.sh --build-proxy` (the shared C:/gta-scratch/rdd/px is stale and
does not partition the audio rand at 0x4D9C97).
Also open: CStreamingInfo TXD ids 20014.. loaded in different pairs on the two sides (RD-AB-3 DUMPSTR) — may be the same cause.
## Task
One divergence at a time: frame / layer / call-site table / cause / fix (as the exe does, asm address, consumer audit, oracle row where
feasible). Push the orig-port prefix as far as the budget allows. Gate (v5): Release + touched targets + soak.
HARD budget 250k / 100 min — at 200k stop and land.
## Care
Work ONLY in your worktree. Original exe needs the foreground; never fight USER-PLAY. Never kill by image name. Trace data under
C:/gta-scratch/rd-ab-4/ — delete your own bulky traces before hand-back if they are no longer needed (no junction dirs: data dirs with
junctions are removed by the orchestrator).
