# KILL-AUDIT — rules v5 item 1 across tools/: no kill by image name

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh kill-audit (no cmake/build needed: scripts only).
## Evidence
`grep -rnE "//IM|/IM |pkill -9 -f|Stop-Process -Name|taskkill" tools/` — known offenders:
tools/standalone/build_locked.sh:36 (kills EVERY cl.exe/link.exe on a 124 timeout: other agents' builds),
tools/standalone/gameplay_regress/orig_run.sh:32,41 (gta_sa.exe), tools/standalone/windows/fps_run.sh:19,22,25, plus anything else the grep finds.
Patterns already in the tree: run_all_tests.sh (taskkill //F //T //PID from /proc/<pid>/winpid), gameplay_regress.sh kill_game (PowerShell
Get-Process filtered by the private data-dir path), replay_diff/rd_port_run.sh + rd_orig_run.sh kill_ours (on branch land-rd / soon interior_c).
## Task
Replace each by a kill of the script's own process tree (winpid of the pid it started; for build_locked: the ninja it launched, /T) or by
the private-dir path filter. Keep behaviour otherwise identical. Test each changed script's kill path with a harmless dummy process
(e.g. a `sleep`-like exe copied into a temp dir) — never by killing a real game or build. Do not run the game, do not take gpu.lock.
Do NOT touch tools/standalone/replay_diff/ (being landed by the orchestrator).
Acceptance: grep shows no image-name kill left outside comments; bash -n on every changed script; a short test log per script.
HARD budget 120k tokens / 60 min.
