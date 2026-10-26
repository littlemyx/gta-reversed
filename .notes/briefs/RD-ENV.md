# RD-ENV — the orig-port prefix fell from 98 to 42 for EVERY port exe after ~14:38 (2026-10-26): find the environment change

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-env on origin/reverse/interior_c.
## Evidence
.notes/reports/RD-AB-15.md: 98/98/98 measured 14:18-14:24 (orig-orig / port-port 300/300). .notes/reports/RD-AB-16.md: from ~14:38 every port
exe gives 42/42/42 — also the orchestrator's gate exe built 13:06 (C:/Users/littl/github/gta-reversed/build/StandaloneRelease/bin, before
RD-AB-15) — while the orig traces stay identical to RD-AB-15's (300/300). So something outside the port exe changed for the PORT leg between
14:24 and 14:38. At rcmp frame 43 the exe creates a vehicle; the port loops CCarCtrl::ChooseModel ~53 times without a loaded car model ->
loaded-vehicle streaming state differs (deterministic).
Candidates: what rd_port_run.sh reads besides the exe — its data dir (links to Steam; files written into it by the game: DebugModules.json,
stream.ini, logs, standalone*.log, gta_sa.set in the private USERDIR it seeds), env vars it inherits, the .map file it derives audio RVA
ranges from (built with the exe? stale map vs exe -> wrong audio partition -> rand shift!), the shared proxy, CdStream file order / img
timestamps. Check the .map hypothesis first: does run.sh use the .map next to the exe, and do the 13:06 exe and its map match?
## Task
Reproduce 98 with the RD-AB-15 port exe (C:/gta-scratch/rd-ab-15/ has its traces; rebuild bf7792b0 if its exe is gone) and find the exact
input that flips it to 42; fix the harness (or the port, if it is a real port dependence on that input) so the prefix is reproducible; then
measure bf7792b0 and HEAD (with RD-AB-16's ProcessShiftSectorList fix) 3 reps each. HARD budget: stop at 150k by your estimate.
## Care
Work ONLY in your worktree; never kill by image name; wait on gpu.lock; never two run.sh on the same --out; check the Documents signature.
