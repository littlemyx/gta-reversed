# RD-ARM — slice 1 of .notes/REPLAY_DIFF_PLAN.md: arm point, pins, runner

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh rd-arm, based on origin/reverse/interior_c, then
bring in the port-side probe from branch origin/replay-diff-probe (0d2a0631: GameRand.h, Timer.cpp, WinPs.cpp, Lockstep.cpp) — review it
line by line first: everything must be inert unless a NOTSA_LOCKSTEP_* env var is set, and listed in standalone_deviations.tsv like the
other test switches.
## Task
Exactly section 8 slice 1 of the plan: arm-point re-seed + frame-counter normalisation (pin bit 8 + row 5) in rd_orig.c and Lockstep.cpp,
the S-line source tag in the port trace, `tools/standalone/replay_diff/run.sh` runner (both legs, private data dirs, gpu lock, the
original's Documents user files backed up and restored exactly as orig_run.sh does, gta_sa.set seeded identically).
Acceptance: orig-vs-orig and port-vs-port with all pins equal for >= 3000 frames on the intro scenario, 3 repetitions each (state the
numbers if not reached, and what the first remaining divergence is); orig-vs-port first-divergence line in the report.
Gate before landing the port-side code on reverse/interior_c: Release build, run_all_tests green, soak OK with the env vars UNSET (inert).
HARD budget 250k tokens / 100 min — at 200k stop and land what passes the gate; leave the rest on a branch + state file.
## Care
The original exe runs fullscreen and needs the foreground (inputdrv foreground guard). Abort a leg cleanly if focus is lost; never fight
the user for focus: if gpu.lock owner is USER-PLAY, wait. Data under C:/gta-scratch/rd*/; never rm -rf a dir with junctions.
Never commit gta_sa_compact.exe or any game file.
