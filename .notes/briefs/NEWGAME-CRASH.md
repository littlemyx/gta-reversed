# NEWGAME-CRASH — HEAD crashes when a new game starts (regression since 2026-10-18 21:27)

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh newgame-crash. TOP PRIORITY.
## Evidence (.notes/reports/POSTFX-RASTER.md; logs C:/gta-scratch/postfx_raster/out_before*, out_mainref, soak_*.log)
HEAD 272acc53 and the 02:56 Release exe crash 3/3 on soak.sh's new-game route (menu > newgame_loading > cutscene_skip):
EXCEPTION_ACCESS_VIOLATION reading address 0, ECX = 0, EIP at a function's first instruction, right after the log line
`CTxdStore::AddTxdSlot("script") -> 3602` (before it: AnimManager `Reading block (DEALER)`), ~2 s after the key that ends the intro skip.
Last good soak: 2026-10-18 21:27 (/c/gta-soak-out/20261018-212754). Since then ~300 commits landed, gated only by run_all_tests (oracle
tests do not start a game). Guess (unverified): a script-command handler wrapper (DisplayText) getting null; or any of the many port
fixes (wave-3 oracle waves, ZONEINFO's script zone commands, LINK-CALLEES' InsertPlayerControlKeysInString rewrite, HOOK-ADDR moves...).
## Task
1. git bisect between the last-good commit (git log --before="2026-10-18 21:28" -1) and HEAD, testing each step with a fast new-game run
   (soak.sh route, NOVSYNC, windowed, silent, private USERDIR; a step = Release incremental build + one run, crash/no-crash within ~60 s).
   Script the step (`git bisect run`). Builds via build_locked.sh, runs via gpu_locked.sh.
2. At the first bad commit: find the exact faulting function (Release map / a RelWithDebInfo build / cdb), read it against the asm,
   fix it as the exe behaves (one commit, asm address in the subject). If the culprit is a wave-3 "fix", verify it against the asm again
   and say whether the oracle row was wrong (fixture artefact) or the fix broke a caller.
3. Prove: soak.sh 3-min route RESULT OK 3/3 on the fixed HEAD.
## Acceptance
First bad commit named; root cause with asm evidence; fix landed; 3/3 soaks OK; report ≤15 lines with start/end time.
HARD budget under 300k tokens.
