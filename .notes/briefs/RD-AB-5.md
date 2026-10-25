# RD-AB-5 — orig-vs-port divergence at frame 19 (car-creation walks after ped group placement) and onward

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-ab-4 on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-AB-4.md: orig-port equal prefix 19/19/19 (3 reps x 1500 frames, pin 1023, intro); self-identity 1500/1500 on both sides
with an own proxy. Frame 19: rand 241 (orig) vs 303 (port). After ped group placement (per-group ped rand counts equal, 9 each) the port does
long GenerateCarCreationCoors2 walks (return-address pairs a6c28c/a6c29e) that the orig does not -> compare CCarCtrl::GenerateOneRandomCar /
GenerateCarCreationCoors2 / ChooseModel inputs at that frame (path areas, node flags, NumCars counters, the car budget) on both sides.
Tools: rcmp.py, RD_RSTACK, RD_BP/RD_BPFC (int3 probe in the original: registers/stack/FPU at an address), D-line gd=/np=/pcnt=/pn=/rw=,
DUMPPN/DUMPSCR/DUMPSTR. Use your own proxy: `export RD_PX=<own dir>` + `run.sh --build-proxy`.
Remember (RD-AB-4): the exe's own quirks (stack-slot reuse etc.) are the reference — mirror them, with the asm address in a comment.
## Task
One divergence at a time: frame / layer / call-site table / cause / fix (as the exe does, asm address, consumer audit, oracle row where
feasible). Push the orig-port prefix as far as the budget allows. Gate (v5): Release + touched targets + soak.
HARD budget 250k / 100 min — at 200k stop and land.
## Care
Work ONLY in your worktree. Original exe needs the foreground; never fight USER-PLAY. Never kill by image name. Trace data under
C:/gta-scratch/rd-ab-4/ — delete your own bulky traces before hand-back if they are no longer needed (no junction dirs: data dirs with
junctions are removed by the orchestrator).
